#!/usr/bin/env python2.7

import parse_ctags
import parse_ftrace
import sys
import argparse


def update_status(stat, tot):
    sys.stdout.write('\r%d of %d' % (stat, tot))
    sys.stdout.flush()


def write_funcs(funcs, filename):
    fd = open(filename, 'w')
    for line in funcs:
        tw = line[0] + ',' + line[1] + '-'
        fd.write(tw)
    fd.close()


def write_subsys(results, filename):
    fd = open(filename, 'w')
    for line in results:
        fd.write(line[0] + ':' + line[1] + ',')
    fd.close()


def get_subsystems(ftrace_d, ctags_d):
    total = len(ftrace_d)
    curr = 0
    ftfunc_file = []
    for func in ftrace_d:
        for tag in ctags_d:
            if len(tag) > 1:
                if tag[0] == func:
                    ftfunc_file.append([func, parse_ctags.get_subsys(tag[1])])
                    curr = curr + 1
                    update_status(curr, total)
                    break

    return ftfunc_file


def find_entry_exit(func_sys):
    prev_sys = func_sys[0][1]
    entry_points = []
    entry_points.append(prev_sys)

    total = len(func_sys)
    count = 1
    for entry in func_sys:
        if entry[1] != prev_sys:
            prev_sys = entry[1]
            entry_points.append([prev_sys, entry[0]])
        update_status(count, total)
        count = count + 1
    return entry_points


def parse_entries(filename):
    with open(filename) as f:
        lines = f.read()
    entries = []
    lines_s = lines.split(',')
    for entry in lines_s:
        if entry is not '':
            sys_fun = entry.split(':')
            entries.append([sys_fun[0], sys_fun[1]])
    return entries


def get_d_for_func(func):
    sig = "probe kernel.function(\"" + func + "\").call { \n"
    body = "\tif (should_acct()) {\n\t\tclear_acct_next(pid(), -1);\n\t\tfill_struct(get_cycles(), gettimeofday_us());\n\t\tupdate_relay();\n\t}\n"
    close = "}\n"
    probe = sig + ' ' + body + ' ' + close
    return probe

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="Find and output subsystems present in an FTrace trace")
    parser.add_argument('-t', dest='ftrace_fn', action='store',
                        type=argparse.FileType('r'), default='trace',
                        help='The FTrace trace file.')
    parser.add_argument('-c', dest='ctags_fn', action='store',
                        type=argparse.FileType('r'), default='tags',
                        help='The CTags file to use')
    parser.add_argument('--entries', dest='entry_fn', action='store',
                        type=argparse.FileType('w'), default='subsystems.out',
                        help='The file to output the final subsystem entries to')
    parser.add_argument('--efile', dest='entries_file',
                        action='store', help='An existing entries file')
    args = parser.parse_args()

    entry_points = []

    if not args.entries_file:
        print '.'
        ftrace_fn = args.ftrace_fn
        ctags_fn = args.ctags_fn

        ctags_d = parse_ctags.parse_for_func_file(
            parse_ctags.read_ctags_file(ctags_fn))
        ftrace_d = parse_ftrace.get_function_list(
            parse_ftrace.read_trace_file(ftrace_fn))

        print ' '
        print "Matching functions to subsystems"
        func_sys = get_subsystems(ftrace_d, ctags_d)
        write_funcs(func_sys, 'funcs.out')

        print ''
        print "Finding subsystem entry points"
        entry_points = find_entry_exit(func_sys)
        write_subsys(entry_points, 'entry_points.out')
    else:
        entry_points = parse_entries(args.entries_file)

    print ''
    print "Outputting D for each function"
    fd = open("probes.out", 'w')
    for entry in entry_points:
        probe = get_d_for_func(entry[1])
        fd.write(probe)
