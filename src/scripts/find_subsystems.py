#!/usr/bin/env python2.7

import argparse
import binascii
from collections import OrderedDict
import json
import os
import re
import sets
import sys
import subprocess

import jinja2
from elftools.elf.elffile import ELFFile

file_subsys_cache = {}
addr_line_cache = {}
fn_blacklist = set()
args = {}

# syscall_types
ADDR_INVALID        = 0
ADDR_CALLQ          = 1
ADDR_USER_SYSCALL   = 2
ADDR_KERNEL_SYSCALL = 3

#
# Progress bar data
progress = 0
# estimated number of probes without function pointers
est_probes_nofp = 87000
# estimated number of probes including function pointers
est_probes_fp = 141400
p_addr_delta = 1.0 / est_probes_fp
stage = ""

# Code to be included at the top of the subsystems header.
RSCFL_SUBSYS_HEADER_TEMPLATE = """
#ifndef _RSCFL_SUBSYS_LIST_H_
#define _RSCFL_SUBSYS_LIST_H_

#include <rscfl/macros.h>

#define SUBSYS_TABLE(_) \\
{%- for _, subsystem in subsystems %}
_({{ subsystem.id }}, {{ subsystem.short_name }}, \"{{ subsystem.long_name }}\") \\
{%- endfor %}

#define SUBSYS_AS_ENUM(a, b, c) b = a,

typedef enum {
    SUBSYS_TABLE(SUBSYS_AS_ENUM)
    NUM_SUBSYSTEMS
} rscfl_subsys;

typedef enum {
    ADDR_INVALID        = {{ ADDR_INVALID }},
    ADDR_CALLQ          = {{ ADDR_CALLQ }},
    ADDR_USER_SYSCALL   = {{ ADDR_USER_SYSCALL }},
    ADDR_KERNEL_SYSCALL = {{ ADDR_KERNEL_SYSCALL }}
} sys_type;

#endif /* _RSCFL_SUBSYS_H_ */
"""


RSCFL_SUBSYS_ADDR_TEMPLATE = """
#ifndef _RSCFL_SUBSYS_ADDR_H
#define _RSCFL_SUBSYS_ADDR_H

#include <linux/types.h>

#include "rscfl/kernel/subsys.h"
#include "rscfl/{{ subsys_list_header }}"

#define RSCFL_NUM_PROBES {{ NUM_PROBES }}

{% for subsystem in subsystems %}
static u8 *{{ subsystem }}_ADDRS[] = {{ '{' }}
{% for (addr, name, type) in subsystems[subsystem] %}
  (u8 *)(0x{{ addr }}), {%- if name != "" %}   // {{ name }}{%- endif %}
{% endfor %}
  0
{{ '};' }}

static char {{ subsystem }}_INTERNAL_SYSCALL[] = {{ '{' }}
{% for (addr, name, type) in subsystems[subsystem] -%}
 {{ type }},
{%- endfor %}
  0
{{ '};' }}

int rscfl_pre_handler_{{ subsystem }}(void)
{{ '{' }}
  return rscfl_subsys_entry({{ subsystem }});
{{ '}' }}

void rscfl_rtn_handler_{{ subsystem }}(void)
{{ '{' }}
  asm("push %rax;");
  asm("push %rdx;");
  rscfl_subsys_exit({{ subsystem }});
  asm("pop %rdx;");
  asm("pop %rax;");
{{ '}' }}
{% endfor %}

static u8 **UNUSED(probe_addrs[]) = {{ '{'  }}
{% for subsys in subsystems %}
  {{ subsys }}_ADDRS,
{% endfor %}
{{ '};' }}

int (*rscfl_pre_handlers[]) (void) = {{ '{' }}
{% for subsys in subsystems %}
  rscfl_pre_handler_{{ subsys }},
{% endfor %}
{{ '};' }}

void (*rscfl_post_handlers[]) (void) = {{ '{' }}
{% for subsys in subsystems %}
  rscfl_rtn_handler_{{ subsys }},
{% endfor %}
{{ '};' }}

static char* UNUSED(syscall_type[]) = {{ '{'  }}
{% for subsys in subsystems %}
  {{ subsys }}_INTERNAL_SYSCALL,
{% endfor %}
{{ '};' }}


#endif
"""

def update_progress(delta, inc=True):
    """
    Args:
        delta: value between 0 (0%) and 1 (100% percent complete)
        inc: boolean that controls whether delta is an incremental value
            (will be added to the previous progress - e.g increment progress by
            2%) or an absolute value (the overall progress is 2%)

    """
    global progress
    if sys.__stdin__.isatty():
        if inc:
            progress = progress + delta
        else:
            progress = delta
        sys.stderr.write("\r[{1:3.0f}%] [{0:25s}] subsys_gen: {2:<32}"
                .format('#' * int(progress * 25), progress * 100, stage))

def to_upper_alpha(str):
    """
    Args:
        str: a string.

    Returns: str, with all non [A-Za-z] characters removed. Then in
        uppercase.
    """
    return re.sub(r'\W+', '', str).upper()

def get_subsys(addr, addr2line, linux, build_dir):
    """
    Use addr2line to convert addr to the filename that it is in.

    Args:
        addr: an address into the linux kernel.
        addr2line: an already-opened subprocess to addr2line, which we can
            use to map addr to a source code file.
        linux: string location of the Linux kernel.
        build_dir: the directory that Linux was built in. This may not
            actually exist on the current filesystem. To find this directory,
            run addr2line on vmlinux, and find the first bit.
    Returns:
        A string representing the name of the subsystem that addr is located
        in.
    """

    # Have we already mapped addr to a file name? If so use the cached value,
    # to save expensive calls out to addr2line.
    if addr in addr_line_cache:
        file_name = addr_line_cache[addr]
    else:
        addr2line.stdin.write("%s\n" % addr)
        file_line = addr2line.stdout.readline()
        if file_line.startswith("??:"):
            # addr2line can't map this address.
            return None
        # Convert file:line to file
        file_name = file_line.split(":")[0]

        # Make filename relative
        try:
            file_name = file_name.split("%s/" % build_dir)[1]
        except IndexError:
            # Generated filenames are already relative.
            pass
        addr_line_cache[addr] = file_name

    # Check the cache of files we've already found a subsystem for.
    if file_name in file_subsys_cache:
        return file_subsys_cache[file_name]
    else:
        # Use the get_maintainer.pl script to check the MAINTAINERS file,
        # to get the subsystem.
        proc = subprocess.Popen(["scripts/get_maintainer.pl",
                                 "--subsystem", "--noemail",
                                 "--no-remove-duplicates", "--no-rolestats",
                                 "-f", file_name], cwd=linux,
                                stdout=subprocess.PIPE)
        (stdout, stderr) = proc.communicate()
        maintainers = stdout.strip().split("\n")
        subsys = "ERROR_DECODING_SUBSYS"
        # The most specific subsystem is listed straight after linux-kernel
        # mailing list.
        for i, line in enumerate(maintainers):
            if line.startswith("linux-kernel@vger.kernel.org"):
                subsys = maintainers[i+1]
                break
        file_subsys_cache[file_name] = subsys
        return subsys

def twiddle_endianness(word):
    """Transform words of the form abcdefgh into ghefcdab."""
    return re.sub(r'([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})',
                  r'\4\3\2\1', word)


def get_function_pointers(vmlinux_path):
    """Find the targets of all function pointers in vmlinux.

    We scan the elf binary and look for entries in rodata that are also
    function addresses. We then assume that that these occur on the entry into
    a subsystem.

    The standard way of implementing modularity throughout the kernel is
    creating a struct of function pointers. e.g. struct file_operations.
    These structs are static (const) structs, so live in .rodata of
    the elf file.  We assume that every function we find of this form
    is on the entry into a subsystem.
    """
    syms = []
    seen_once = sets.Set()
    fn_ptrs = sets.Set()
    with open(vmlinux_path, 'rb') as f:
        elf_file = ELFFile(f)
        # Find the addresses of all functions in the kernel by looking in the
        # symbol table.
        symtab = elf_file.get_section_by_name(b'.symtab')
        for sym in symtab.iter_symbols():
            if sym.entry['st_info']['type'] == "STT_FUNC":
                syms.append(hex(sym.entry['st_value']))
        # Scan the rodata section, looking for anything that points to an
        # address of a function.
        rodata = elf_file.get_section_by_name(b'.rodata')
        length = len(rodata.data())
        boundary = 2000
        pleft = 1 - progress + 0.1
        pbase = progress
        for i in xrange(0, len(rodata.data()), 4):
            word = binascii.hexlify(rodata.data()[i:i+4])
            # Fix endianness
            word = twiddle_endianness(word)
            if "0xffffffff%sL" % word in syms:
                if word in seen_once:
                    fn_ptrs.add("ffffffff%s" % word)
                else:
                    seen_once.add(word)
            if i > boundary:
                boundary += 2000
                p_val = i*pleft/length
                update_progress(pbase + p_val, False)
    return fn_ptrs

def add_address_to_subsys(boundary_fns, subsys, fn_addr, fn_name,
        syscall_type, upd_progress=True):
    """Add an address to the list of addresses on a subsystem boundary.

    Check to see if the address is already in the subsystem, and if not then
    add it.

    Args:
        boundary_fns: a dict, with keys being names of subsystems and values
            being lists of addresses on the boundary to that subsystem.
        subsys: name of the subsystem to enter a boundary address for.
        fn_addr: address of the function on the boundary.
        fn_name: name of the function on the boundary.
        upd_progress: whether to update progress bar or not
    """
    if subsys not in boundary_fns:
        boundary_fns[subsys] = []
    if ((fn_addr, fn_name, syscall_type)) not in boundary_fns[subsys]:
        boundary_fns[subsys].append((fn_addr, fn_name, syscall_type))
        if upd_progress:
            update_progress(p_addr_delta)


def get_addresses_of_boundary_calls(linux, build_dir, vmlinux_path):
    """
    Find the addresses of all call instructions that operate across a kernel
    subsystem boundary.

    Args:
        linux: string representing the location of the linux kernel.
        build_dir: the directory that linux was built in. Found by looking
            at the prefix of the results returned by addr2line.
        vmlinux_path: string representing the path to vm_linux.

    Returns:
        a dictionary (indexed by subsystem name) where each element is a list
        of addresses that are callq instructions whose target is in the
        appropriate subsystem.
    """
    global stage
    boundary_fns = {}
    boundary_fns["USERSPACE_XEN"] = []
    fn_addr_name_map = {}
    skip_callqs = 0
    stage = "kernel subsystem boundaries [callq]"
    addr2line = subprocess.Popen(['addr2line', '-e', vmlinux_path],
                                 stdout=subprocess.PIPE,
                                 stdin=subprocess.PIPE)
    # Use objdump+grep to find all callq instructions.
    proc = subprocess.Popen('objdump -d %s' % vmlinux_path, shell=True,
                            stdout=subprocess.PIPE)

    # Regex to match the entry point into every function, so we can see if it's
    # a syscall.
    p_fn_entry = re.compile("([0-9a-f]{16,16}) <(.*)>:$")

    # regex to match callq instructions, creating groups from the caller, and
    # callee addresses.
    p_callq = re.compile("([0-9a-f]{16,16}).*callq.*([0-9a-f]{16,16}) <(.*)>$")
    for line in proc.stdout:

        # We are entering a function.
        m = p_fn_entry.match(line)
        if m:
            skip_callqs = 0
            fn_addr = m.group(1)
            fn_name = m.group(2)
            if fn_name.strip() in fn_blacklist:
                # skip all callqs until the next function
                skip_callqs = 1
                continue
            fn_addr_name_map[fn_addr] = fn_name
            # Syscalls all have SyS_ in their name. Sometimes they may be
            # prepended. e.g. compat_SyS_sendfile.
            caller_subsys = get_subsys(fn_addr, addr2line, linux, build_dir)

            if "SyS_" in fn_name:
                fn_subsys = get_subsys(fn_addr, addr2line, linux, build_dir)
                add_address_to_subsys(boundary_fns, fn_subsys, fn_addr,
                                      fn_name, ADDR_USER_SYSCALL)

        if skip_callqs == 1:
            continue

        # If this is a function call look to see if we're crosssing a boundary.
        m = p_callq.match(line)
        if m:
            caller_addr = m.group(1)
            callee_addr = m.group(2)
            callee_name = m.group(3)

            if callee_name == "__fentry__":
                continue

            if (callee_name == "get_evtchn_to_irq") and (
                    fn_name == "irq_from_evtchn"):
                add_address_to_subsys(boundary_fns, "XENINTERRUPTS",
                                      caller_addr, callee_name, ADDR_CALLQ)

            if callee_name.strip() in fn_blacklist:
                continue

            callee_subsys = get_subsys(callee_addr, addr2line, linux, build_dir)
            if callee_subsys != caller_subsys and callee_subsys is not None:
                syscall_type = ADDR_CALLQ
                if "SyS_" in callee_name:
                    syscall_type = ADDR_KERNEL_SYSCALL
                add_address_to_subsys(boundary_fns, callee_subsys, caller_addr,
                                      callee_name, syscall_type)
    if not args.no_fp:
        stage = "kernel subsystem boundaries [f_ptr]"
        update_progress(0)
        fn_ptr_targets = get_function_pointers(vmlinux_path)
        for target in fn_ptr_targets:
            subsys = get_subsys(target, addr2line, linux, build_dir)
            if not subsys:
                sys.stderr.write("Error %s\n" % target)
            else:
                add_address_to_subsys(boundary_fns, subsys, target,
                                      fn_addr_name_map[target], 0, False)
    return boundary_fns


def append_to_json_file(json_fname, subsys_names):
    """
    Add new subsystems to a JSON file.

    Parses json_fname, and adds any subsystems in subsys_entries to
    the file.

    Args:
        json_fname: the JSON file name. If the file contains a valid JSON
            structure, new subsystems will be appended. Otherwise, all
            subsystems will be dumped to the file.
        subsys_names: a list of string names of Linux subsystems.
    """
    try:
        json_file = open(json_fname, 'r+')
        json_entries = json.load(json_file,
                                 object_pairs_hook=OrderedDict)
        json_file.seek(0)
        json_file.truncate()
    except IOError:
        # File does not exist
        json_file = open(json_fname, 'w')
        json_entries = OrderedDict()
    except ValueError:
        # No valid JSON in the file.
        json_file.close()
        json_file = open(json_fname, 'w')
        json_entries = OrderedDict()

    subsys_names.sort()
    for subsys in subsys_names:
        if to_upper_alpha(subsys) not in json_entries:
            # Remove various bits of punctuation so we can index using the name.
            clean_subsys_name = re.sub(r'\W+', '', subsys).upper()
            json_entries[clean_subsys_name] = {}
            json_entries[clean_subsys_name]['id'] = len(json_entries) - 1
            # long_name is used for human output. Its value can be changed to be
            # more human-friendly
            json_entries[clean_subsys_name]['long_name'] = subsys.capitalize()
            # short_name is used as a key in enums. Its value can be modified to
            # be more code-friendly.
            json_entries[clean_subsys_name]['short_name'] = clean_subsys_name

    json.dump(json_entries, json_file, indent=2)
    json_file.close()


def generate_rscfl_subsystems_header(json_fname, header_file):
    """Using the JSON list of subsystems, generate a header file that creates
    a enum of subsystems.

    Save this header file to $header_file

    Args:
        json_file: File object with a JSON list of subsystems.
        header_file: File to write a C header file containing an enum of
            possible subsystems.
    """
    json_file = open(json_fname, 'r')
    subsys_json = json.load(json_file)
    subsystems = sorted(subsys_json.items(), key=lambda x: x[1]['id'])
    template = jinja2.Template(RSCFL_SUBSYS_HEADER_TEMPLATE)
    args = {}
    args['subsystems'] = subsystems
    args['ADDR_INVALID'] = ADDR_INVALID
    args['ADDR_CALLQ'] = ADDR_CALLQ
    args['ADDR_USER_SYSCALL'] = ADDR_USER_SYSCALL
    args['ADDR_KERNEL_SYSCALL'] = ADDR_KERNEL_SYSCALL
    header_file.write(template.render(args))

    json_file.close()


def read_blacklist(blacklist_file):
    global fn_blacklist
    for fn_name in blacklist_file:
        fn_blacklist.add(fn_name.strip())

def main():
    """
    Main.
    """
    global args
    global p_addr_delta
    global stage
    parser = argparse.ArgumentParser()
    parser.add_argument('-l', dest='linux_root', action='store',
                        help="""location of the root of the
                        Linux source directory.""")
    parser.add_argument('-v', dest="vmlinux_path", action='store', help="""Path
                        to vmlinux.""")
    parser.add_argument('--build_dir', help="""Location that vmlinux was
                        built in. This might not exist on the filesystem. It
                        can be found by running addr2line on vmlinux, with
                        a symbol address, and finding the first bit.""")
    parser.add_argument('--find_subsystems', action='store_true',
                        help="""Scan vmlinux, looking for all addresses that
                        cross the boundary of subsystems. These are exported
                        as a .h file of addresses.""")
    parser.add_argument('--no-fp', dest='no_fp', action='store_true',
                        default = False,
                        help="""Don't scan kernel binary for function
                        pointers""")
    parser.add_argument('-J', dest='subsys_json_fname', help="""JSON file to
                        write subsystems to.""")
    parser.add_argument('--update_json', action='store_true', help="""Append
                        any new subsystems to the JSON file.""")
    parser.add_argument('--gen_shared_header', type=argparse.FileType('w'),
                        help="""Generate a .h file that should be shared
                        across the user-kernel boundary that defines
                        an enum that maps subsystems to array indices.""")
    parser.add_argument('--fn_blacklist', type=argparse.FileType('r'),
                        help="""Read file containing a list of blacklisted
                        addresses. Those will not be included in any output""")

    args = parser.parse_args()

    if args.fn_blacklist:
        read_blacklist(args.fn_blacklist)

    if args.no_fp:
        p_addr_delta = 1.0 / est_probes_nofp

    # If we havent' been given an explicit build directory, it is fair to
    # assume that the kernel was built in the source directory.
    if args.build_dir:
        build_dir = args.build_dir
    else:
        build_dir = args.linux_root
    if args.update_json or args.find_subsystems:
        stage = "getting subsystem boundaries"
        update_progress(0)
        subsys_entries = get_addresses_of_boundary_calls(args.linux_root,
                                                         build_dir,
                                                         args.vmlinux_path)

    stage = "writing files (subsys/addr data)    "
    update_progress(0.99, False)
    if args.update_json:
        append_to_json_file(args.subsys_json_fname,
                            subsys_entries.keys())

    if args.gen_shared_header:
        generate_rscfl_subsystems_header(args.subsys_json_fname,
                                         args.gen_shared_header)

    if args.find_subsystems:
        sharedh_fname = args.gen_shared_header.name
        template = jinja2.Template(RSCFL_SUBSYS_ADDR_TEMPLATE)
        targs = {}
        targs['NUM_PROBES'] = sum(len(value) for (key, value) in
                                  subsys_entries.items())
        targs['subsys_list_header'] = os.path.basename(sharedh_fname)
        targs['subsystems'] = dict((to_upper_alpha(key), value) for (key, value)
                                  in subsys_entries.items())
        print template.render(targs)

    stage = "Done                               \n"
    update_progress(1.0, False)

if __name__ == '__main__':
    main()
