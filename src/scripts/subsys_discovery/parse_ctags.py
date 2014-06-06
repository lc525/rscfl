#!/usr/bin/env python2.7


def read_ctags_file(f):
    return f.readlines()


def parse_for_func_file(tags):
    parsed_tags = {}
    for line in tags:
        try:
            s = line.split('\t')
            func = s[0]
            fl = s[1]
            parsed_tags[func] = fl
        except IndexError:
            pass
    return parsed_tags


def get_subsys(filename):
    return filename.split('/')[0]

if __name__ == "__main__":
    tags_data = read_ctags_file("tags")
    func_file = parse_for_func_file(tags_data)
    for tag in func_file:
        if len(tag) > 1:
            print get_subsys(tag[1])
