# coding: utf-8
"""
    autodoc_c.parser
    ~~~~~~~~~~~~~~~~

    :copyright: 2014 by Daniel Neuhäuser
    :license: BSD, see LICENSE.rst for details
"""
import textwrap
from itertools import chain

from autodoc_c.clang.cindex import (
    Index, TranslationUnit, TokenKind, CursorKind
)

def process_doc(app, objtype, fullname, s_object, docstrings):
    """Let the user process the docstrings before adding them."""
    options = {}
    for docstringlines in docstrings:
        if app:
            # let extensions preprocess docstrings
            app.emit('autodoc-process-docstring',
                     objtype, fullname, s_object,
                     options, docstringlines)
        for line in docstringlines:
            yield line


def get_commented_nodes(path, app):
    """
    Yields tuples containing an AST node, the corresponding comment as string
    and two tuples corresponding to the start and end location of the comment
    respectively, containing the line and column number.
    """
    index = Index.create()
    translation_unit = index.parse(
        path,
        options=(
            TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD |
            TranslationUnit.PARSE_SKIP_FUNCTION_BODIES
        )
    )
    root = translation_unit.cursor
    # Clang is capable of associating comments with AST nodes, however the
    # approach it takes is fairly simple and produces weird results sometimes,
    # so we do that ourselves by inspecting the tokens.
    #
    # For our purposes we redefine "comment" to mean any sequence of comments
    # not separated by a blank line. So this:
    #
    #   // first line
    #   // second line
    #
    # is considered to be a single comment whereas:
    #
    #   // first line
    #
    #   // second line
    #
    # are considered to be two comments.
    comments = {}
    grouped = chain(_grouped_comments(root.get_tokens()), [None])
    previous = current = None
    following = next(grouped)
    for new_following in grouped:
        previous, current, following = current, following, new_following
        if (
            not isinstance(current, list) or # not a comment
            not is_documentation_comment(current[0].spelling)
        ):
            continue
        if (
            previous is None and (
                isinstance(following, list) or
                current[-1].extent.end.line + 1 != following.location.line
            )
        ):
            # This comment is the first comment in the file and no non-comment
            # token follows it. We assume this to mean that it covers the
            # entire file and therefore assign it the location (0, 0) which
            # corresponds with the location of the translation unit.
            comments[0, 0] = current
        elif (
            not isinstance(following, list) and
            current[-1].extent.end.line + 1 == following.location.line
        ):
            # This comment is followed by a non-comment token without a blank
            # line inbetween.
            comments[following.location.line, following.location.column] = current
        elif (
            not isinstance(previous, list) and
            previous.extent.end.line == current[0].location.line
        ):
            # This comment follows another token on the same line, so it has to
            # be an inline comment. We associate this comment with that one.
            comments[previous.extent.end.line, previous.extent.end.column] = current
        elif (
            not isinstance(previous, list) and
            current[0].extent.start.line - 1 == previous.extent.end.line
        ):
            # This comment is preceded by a non-comment token without a blank
            # line inbetween.
            comments[previous.location.line, previous.location.column] = current
        else:
            # Other comments can't be sensibly associated with anything else so we
            # ignore these.
            pass

    def find_nodes(node, app):
        if node is root:
            if (node.location.line, node.location.column) in comments:
                comment = comments.pop((node.location.line, node.location.column))
                yield (
                    node,
                    get_name_path(node),
                    ' '.join(
                        strip_comment_syntax(token.spelling) for token in comment
                    ),
                    (comment[0].extent.start.line, comment[0].extent.start.column),
                    (comment[-1].extent.end.line, comment[-1].extent.end.column)
                )
        elif (
            node.location.file is not None and
            node.location.file.name == path and
            (node.spelling or node.displayname)
        ):
            # The ast might include parts of included files, so we have to
            # filter by node location.
            #print(node.spelling)
            tokens = list(node.get_tokens())
            if len(tokens) == 0:
                return
            first_location = (
                tokens[0].extent.start.line,
                tokens[0].extent.start.column
            )
            last_location = (
                tokens[-1].extent.end.line,
                tokens[-1].extent.end.column
            )
            if node.kind is CursorKind.MACRO_DEFINITION:
                # In the case of a macro definition, the first token is the
                # identifier and not the #define part, which means that the
                # first column is len("#define ") characters too great.
                first_location = (
                    first_location[0],
                    first_location[1] - len("#define ")
                )
            if first_location in comments:
                comment = comments.pop(first_location)
            elif last_location in comments:
                comment = comments.pop(last_location)
            else:
                comment = None
                #print("No comment")
            if comment:
                cmt_text = []
                cmt = []
                for token in comment:
                    cmt_text.append(strip_comment_syntax(token.spelling))
                print(cmt_text)
                for i, line in enumerate(process_doc(app, 'function', node.displayname, node, cmt_text)):
                    cmt.append(line+"\n")
                #TODO(lc525): figure out how to parse function signatures from node cursor
                result = app.emit_firstresult(
                        'autodoc-process-signature', 'function', node.displayname,
                        node, object(), [], 'int')
                print(cmt)
                yield (
                    node,
                    get_name_path(node),
                    cmt,
                    (comment[0].extent.start.line, comment[0].extent.start.column),
                    (comment[-1].extent.end.line, comment[-1].extent.end.column)
                )

        for child in node.get_children():
            for yielded in find_nodes(child, app):
                yield yielded

    return find_nodes(root, app)


def _grouped_comments(tokens):
    """
    Groups tokens that aren't separated by a blank line in a list otherwise
    yields tokens as given.
    """
    tokens = list(tokens)
    i = 0
    while i < len(tokens):
        token = tokens[i]
        if token.kind is TokenKind.COMMENT:
            comment = [token]
            while i + 1 < len(tokens) and tokens[i + 1].kind is TokenKind.COMMENT:
                possible_next_part = tokens[i + 1]
                last_comment_line = comment[-1].extent.end.line
                if possible_next_part.location.line == last_comment_line + 1:
                    comment.append(possible_next_part)
                    i += 1
                else:
                    break
            yield comment
        else:
            yield token
        i += 1


def is_documentation_comment(comment):
    return comment.startswith('///') or comment.startswith('/**')


def strip_comment_syntax(comment):
    if comment.startswith('///'):
        return comment[3:].lstrip()
    lines = comment.splitlines()
    lines[0] = lines[0][3:]
    for i, line in enumerate(lines[1:-1], 1):
        if line.lstrip().startswith('* '):
            lines[i] = line.lstrip()[2:]
        elif line.lstrip().startswith('*'):
            lines[i] = line.lstrip()[1:]
    lines[-1] = lines[-1][:-2].rstrip()
    #return textwrap.dedent('\n'.join(lines))
    return lines


def get_name_path(node):
    if node.kind is CursorKind.TRANSLATION_UNIT:
        return []
    elif node.kind is CursorKind.STRUCT_DECL and not (node.spelling or node.displayname):
        return []
    elif node.kind is CursorKind.FIELD_DECL:
        return get_name_path(node.semantic_parent) + [node.spelling or node.displayname]
    return [node.spelling or node.displayname]
