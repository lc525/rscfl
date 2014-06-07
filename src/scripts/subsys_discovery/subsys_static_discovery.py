#!/usr/bin/env python2.7

import os
import sys
import argparse
import re
import subprocess
from subprocess import PIPE, STDOUT
from sets import Set


KALLSYMS = "/proc/kallsyms"


def get_ext_link_fns():
    kern_syms = []
    p = re.compile("0000000000000000 T ([^\n\t]*)")
    kallsyms_fd = open(KALLSYMS)
    for line in kallsyms_fd:
        m = p.match(line)
        if m != None:
            kern_syms.append(m.group(1))
    return kern_syms


def read_cscope(p):
    lines = []
    line = p.stdout.readline()
    pat = re.compile(">> cscope: ([0-9]*) lines")
    m = pat.match(line)
    for i in range(int(m.group(1))):
        lines.append(p.stdout.readline())
    return lines


def find_caller_subsys(fns, linux):
    boundary_fns = []
    i = 0
    p = subprocess.Popen(["cscope -dl cscope.out"],
                         shell=True, stdout=PIPE, stdin=PIPE, stderr=PIPE,
                         cwd=linux)
    for fn in fns:
        # Find the subsytem that the function definition is in
        subsys = Set()
        p.stdin.write("1%s\n" % fn)
        for line in read_cscope(p):
            subsys.add(line.split('/')[0])

        # Find the subsystem that all function calls are in
        p.stdin.write("4%s\n" % fn)
        for line in read_cscope(p):
            callee_subsys = line.split('/')[0]
            if callee_subsys not in subsys:
                # Callee in a different subsystem to definition
                boundary_fns.append(fn)
                break
        print("%d of %d" % (i, len(fns)))
        i = i + 1
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
    ext_link_fns = get_ext_link_fns()
    entry_points = find_caller_subsys(ext_link_fns, args.linux_root)
    entry_points = ['kernel.function("%s").call,' % x for x in entry_points]
    # remove duplicates
    entry_points = list(set(entry_points))
    entry_points[-1] = entry_points[-1][0:-1]

    print("probe ")
    print("\n".join(entry_points))
    print("""
{
  if (should_acct()) {
    clear_acct_next(pid(), -1);
    fill_struct(get_cycles() - cycles, gettimeofday_us() - wall_clock_time);
    update_relay();
   }
}
          """)


if __name__ == '__main__':
    main()
