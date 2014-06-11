#!/usr/bin/env python2.7

import os
import sys
import argparse
import re
import shlex
import subprocess
from subprocess import PIPE, STDOUT
from sets import Set

def get_subsys(fn, p):
    subsys = []
    p.stdin.write("1%s\n" % fn)
    for line in read_cscope(p):
        subsys.append(line.split('/')[0])
    return subsys


def get_fn_calls(linux):
    i = 0
    subsys_cache = {}
    boundary_fns = {}
    cscope = subprocess.Popen(["cscope -dl cscope.out"],
                         shell=True, stdout=PIPE, stdin=PIPE, stderr=PIPE,
                         cwd="/home/oc243/linux-stable/")
    probe_addrs = []
    proc1 = subprocess.Popen('objdump -d vmlinux', shell=True,
                             stdout=subprocess.PIPE)
    proc2 = subprocess.Popen(
        ['grep', '-E', 'callq|>:'], stdin=proc1.stdout, stdout=PIPE)
    out, err = proc2.communicate()
    p = re.compile("([0-9a-f]{16,16}) <(.*)>:")
    p_callq = re.compile("([0-9a-f]{16,16}).*callq.*<(.*)>")
    for line in out.split('\n'):
        m = p.match(line)
        if m:
            fn = m.group(2)
            addr = m.group(1)
            subsys = get_subsys(fn, cscope)
        else:
            m = p_callq.match(line)
            if m:
                callee_fn = m.group(2)
                callee_addr = m.group(1)
                try:
                    callee_subsys = subsys_cache[callee_fn]
                except KeyError:
                    callee_subsys = get_subsys(callee_fn, cscope)
                    subsys_cache[callee_fn] = callee_subsys
                if (get_subsys(callee_fn, cscope) != subsys):
                    for x in callee_subsys:
                        try:
                            boundary_fns[x]
                        except KeyError:
                            boundary_fns[x] = []
                        boundary_fns[x].append(callee_addr)
    return boundary_fns


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-l', dest='linux_root', action='store', default='.',
                        help="""location of the root of the
                        Linux source directory""")
    args = parser.parse_args()

    if not os.path.exists("%s/cscope.out" % args.linux_root):
        sys.stderr.write("Cannot find cscope.out\n")
        exit(-1)
    subsys_entries = get_fn_calls(args.linux_root)

    for subsys in subsys_entries:
        entry_points = ['kernel.statement("%s"),' % x for x in
                        subsys_entries[subsys]]
        # remove duplicates
        entry_points = list(set(entry_points))
        entry_points[-1] = entry_points[-1][0:-1]

        print("probe ")
        print("\n".join(entry_points))
        print("""
{
  if (should_acct()) {
    print("Entered %s subsystem")
    clear_acct_next(pid(), -1);
    fill_struct(get_cycles() - cycles, gettimeofday_us() - wall_clock_time);
    update_relay();
   }
}
              """ % subsys)


if __name__ == '__main__':
    main()
