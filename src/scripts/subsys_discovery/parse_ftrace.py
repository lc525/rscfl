#!/usr/bin/env python2.7

def read_trace_file(f):
        return f.readlines()[11:-1]


def get_function_list(trace):
    functions = []
    for line in trace:
        line = line.strip()
        parts = line.split(' ')
        func = parts[-1]
        functions.append(func.replace('<-', ''))
    return functions

if __name__ == '__main__':
    filename = "trace"
    trace = read_trace_file(filename)
    functions = get_function_list(trace)
    print functions
