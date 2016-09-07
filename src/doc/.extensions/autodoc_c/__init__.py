# coding: utf-8
"""
    autodoc_c
    ~~~~~~~~~

    :copyright: 2014 by Daniel Neuhäuser
    :license: BSD, see LICENSE.rst for details
"""
import os
import sys
from weakref import WeakKeyDictionary

from docutils import nodes
from docutils.statemachine import ViewList

from sphinx import addnodes
from sphinx.directives import ObjectDescription
from sphinx.domains.c import CDomain, CObject

from autodoc_c.parser import get_commented_nodes
from autodoc_c.clang.cindex import (
    Config as ClangConfig, TranslationUnitLoadError, CursorKind, TokenKind
)


if sys.platform == 'darwin':
    ClangConfig.set_library_path(
        '/Library/Developer/CommandLineTools/usr/lib/'
    )


_APP_CACHES = WeakKeyDictionary()


def parse_headers(app):
    cache = _APP_CACHES[app] = {}
    for header in app.config.autodoc_c_headers:
        header = os.path.join(app.srcdir, header)
        try:
            commented = get_commented_nodes(header, app)
        except TranslationUnitLoadError as error:
            app.warn('Could not load header: %s\n%s' % (header, str(error)))
            continue
        for node, name, comment, start, end in commented:
            print("Add to cache key:"+str(node.kind)+str(tuple(name)))
            cache[node.kind, tuple(name)] = node, comment, start, end, {}
        for kind, name in cache:
            if kind is CursorKind.STRUCT_DECL:
                struct_name = name[0]
                members = cache[kind, name][4]
                for (kind, name), value in cache.items():
                    if kind is CursorKind.FIELD_DECL and name[0] == struct_name:
                        members[kind, name] = value

class AutoMacro(ObjectDescription):
    def handle_signature(self, sig, signode):
        cache = _APP_CACHES.get(self.env.app, {})
        key = CursorKind.MACRO_DEFINITION, (sig, )
        if key in cache:
            node, comment, start, end, _ = cache[key]
            signode += addnodes.desc_name(node.displayname, node.displayname)

            # There is unfortunately no API to get the parameters of a macro,
            # so we identify them by looking at the tokens.
            tokens = list(node.get_tokens())
            if (
                tokens[1].kind is TokenKind.PUNCTUATION and
                tokens[1].spelling == '('
            ):
                paramlist = addnodes.desc_parameterlist()
                for token in tokens[2:]:
                    if (
                        token.kind is TokenKind.PUNCTUATION and
                        token.spelling == ')'
                    ):
                        break
                    elif token.kind is TokenKind.IDENTIFIER:
                        paramlist += addnodes.desc_parameter(token.spelling, token.spelling)
                signode += paramlist

            self.content = ViewList()
            for lineno, line in enumerate(comment.splitlines(), start[0]):
                self.content.append(line, '<unknown>', lineno)
        return sig

    def get_index_text(self, name):
        return '%s (C macro)' % name

    def add_target_and_index(self, name, sig, signode):
        CObject.add_target_and_index(self, name, sig, signode)


class AutoVar(ObjectDescription):
    objtype = 'data'

    def handle_signature(self, sig, signode):
        cache = _APP_CACHES.get(self.env.app, {})
        key = CursorKind.VAR_DECL, (sig, )
        if key in cache:
            node, comment, start, end, _ = cache[key]
            signode += addnodes.desc_type(node.type.spelling, node.type.spelling + ' ')
            signode += addnodes.desc_name(node.spelling, node.spelling)

            self.content = ViewList()
            for lineno, line in enumerate(comment.splitlines(), start[0]):
                self.content.append(line, '<unknown>', lineno)
        return sig

    def get_index_text(self, name):
        return '%s (C variable)' % name

    def add_target_and_index(self, name, sig, signode):
        CObject.add_target_and_index(self, name, sig, signode)


class AutoFunction(ObjectDescription):

    def handle_signature(self, sig, signode):
        print("looking for: " + sig)
        cache = _APP_CACHES.get(self.env.app, {})
        key = CursorKind.FUNCTION_DECL, (sig, )
        if key in cache:
            print("KEY FOunD!")
            node, comment, start, end, _ = cache[key]

            result_type = node.type.get_result()
            signode += addnodes.desc_type(result_type.spelling, result_type.spelling + ' ')
            signode += addnodes.desc_name(node.spelling, node.spelling)
            paramlist = addnodes.desc_parameterlist()
            for argument in node.get_arguments():
                parameter = addnodes.desc_parameter()
                parameter += addnodes.desc_type(argument.type.spelling, argument.type.spelling + ' ')
                parameter += nodes.Text(argument.spelling, argument.spelling)
                paramlist += parameter
            signode += paramlist

            self.content = ViewList()
            comment = ''.join(comment)
            for lineno, line in enumerate(comment.splitlines(), start[0]):
                self.content.append(line, '<unknown>', lineno)
        return sig

    def get_index_text(self, name):
        return '%s (C function)' % name

    def add_target_and_index(self, name, sig, signode):
        #print("Sig!:" + ''.join(sig)+"->"+''.join(signode))
        CObject.add_target_and_index(self, name, sig, signode)


class AutoType(ObjectDescription):
    def handle_signature(self, sig, signode):
        try:
            tag, name = sig.split()
        except ValueError:
            tag, name = None, sig
        cache = _APP_CACHES.get(self.env.app, {})
        key = {'struct': CursorKind.STRUCT_DECL}[tag], (name, )
        if key in cache:
            node, comment, start, end, members = cache[key]
            signode += addnodes.desc_type(tag, tag + ' ')
            signode += addnodes.desc_name(node.spelling, node.spelling)

            self.content = ViewList()
            for line in comment.splitlines():
                self.content.append(line, '<unknown>')

            self.content.append('', '<unknown>')

            for (_, member_name), value in members.items():
                member_node, member_comment, _, _, _ = value
                self.content.append(
                    '.. c:member:: %s %s' % (member_node.type.spelling, member_node.spelling),
                    '<unknown>'
                )
                self.content.append('', '<unknown>')
                for line in member_comment.splitlines():
                    self.content.append('   ' + line, '<unknown>')
                self.content.append('', '<unknown>')


        return sig

    def get_index_text(self, name):
        return '%s (C type)' % name

    def add_target_and_index(self, name, sig, signode):
        CObject.add_target_and_index(self, name, sig, signode)



def setup(app):
    app.connect('builder-inited', parse_headers)
    app.add_config_value('autodoc_c_headers', [], 'env')
    app.add_directive_to_domain(CDomain.name, 'automacro', AutoMacro)
    app.add_directive_to_domain(CDomain.name, 'autovar', AutoVar)
    app.add_directive_to_domain(CDomain.name, 'autofunction', AutoFunction)
    app.add_directive_to_domain(CDomain.name, 'autotype', AutoType)
    app.add_event('autodoc-process-docstring')
    app.add_event('autodoc-process-signature')
    app.add_event('autodoc-skip-member')
