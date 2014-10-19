#!/usr/bin/env python2.7

import argparse
import re
import subprocess

file_subsys_cache = {}
addr_line_cache = {}

def get_subsys(addr, addr2line, linux):
    # Use addr2line to convert addr to the filename that it is in.
    #
    # Args:
    #     addr: an address into the linux kernel.
    #     addr2line: an already-opened subprocess to addr2line, which we can
    #         use to map addr to a source code file.
    #     file_subsys_cache: a cache that maps files to subsystems that they're
    #         contained in. Used to prevent repeated calls to get_maintainer.pl.
    #     linux: string location of the Linux kernel.
    # Returns:
    #     A string representing the name of the subsystem that addr is located
    #     in.


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
            file_name = file_name.split("linux-stable/")[1]
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
        proc = subprocess.Popen(["%s/scripts/get_maintainer.pl" % linux,
                                 "--subsystem", "--noemail",
                                 "--no-remove-duplicates", "--no-rolestats",
                                 "-f", "%s" %  file_name], cwd=linux,
                                 stdout=subprocess.PIPE)
        (stdout, stderr) = proc.communicate()
        maintainers = stdout.strip().split("\n")
        subsys = ""
        # The most specific subsystem is listed straight after linux-kernel
        # mailing list.
        for i, line in enumerate(maintainers):
            if line.startswith("linux-kernel@vger.kernel.org"):
                subsys = maintainers[i+1]
                break
        file_subsys_cache[file_name] = subsys
        return subsys


def get_addresses_of_boundary_calls(linux):
    # Find the addresses of all call instructions that operate across a kernel
    # subsystem boundary.
    #
    # Args:
    #     linux: string representing the location of the linux kernel.
    #
    # Returns:
    #     a dictionary (indexed by subsystem name) where each element is a list
    #     of addresses that are callq instructions whose target is in the
    #     appropriate subsystem.
    boundary_fns = {}
    addr2line = subprocess.Popen(['addr2line', '-e' '%s/vmlinux' % linux],
                                 stdout=subprocess.PIPE,
                                 stdin=subprocess.PIPE)
    # Use objdump+grep to find all callq instructions.
    proc = subprocess.Popen('objdump -d vmlinux', shell=True,
                            stdout=subprocess.PIPE, cwd=linux)
    # regex to match callq instructions, creating groups from the caller, and
    # callee addresses.
    p_callq = re.compile("([0-9a-f]{16,16}).*callq.*([0-9a-f]{16,16}).*$")
    for line in proc.stdout:
        m = p_callq.match(line)
        if m:
            caller_addr = m.group(1)
            callee_addr = m.group(2)

            caller_subsys = get_subsys(caller_addr, addr2line, linux)
            callee_subsys = get_subsys(callee_addr, addr2line, linux)
            if not caller_subsys:
                # Address that we can't map to source file.
                continue
            if callee_subsys != caller_subsys:
                if callee_subsys not in boundary_fns:
                    boundary_fns[callee_subsys] = []
                boundary_fns[callee_subsys].append(caller_addr)
    return boundary_fns


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-l', dest='linux_root', action='store', default='.',
                        help="""location of the root of the
                        Linux source directory""")
    args = parser.parse_args()

    subsys_entries = get_addresses_of_boundary_calls(args.linux_root)

    for subsys in subsys_entries:
        entry_points = ['kprobe.statement(0x%s).absolute,' % x for x in
                        subsys_entries[subsys]]
        entry_points[-1] = entry_points[-1][0:-1]
        print("probe ")
        print("\n".join(entry_points))
        print("""
{
    print("Entered %s subsystem")
}
              """ % subsys)


if __name__ == '__main__':
    main()
