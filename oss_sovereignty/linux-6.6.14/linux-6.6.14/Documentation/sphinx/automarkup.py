from docutils import nodes
import sphinx
from sphinx import addnodes
if sphinx.version_info[0] < 2 or \
   sphinx.version_info[0] == 2 and sphinx.version_info[1] < 1:
    from sphinx.environment import NoUri
else:
    from sphinx.errors import NoUri
import re
from itertools import chain
try:
    ascii_p3 = re.ASCII
except AttributeError:
    ascii_p3 = 0
RE_function = re.compile(r'\b(([a-zA-Z_]\w+)\(\))', flags=ascii_p3)
RE_generic_type = re.compile(r'\b(struct|union|enum|typedef)\s+([a-zA-Z_]\w+)',
                             flags=ascii_p3)
RE_struct = re.compile(r'\b(struct)\s+([a-zA-Z_]\w+)', flags=ascii_p3)
RE_union = re.compile(r'\b(union)\s+([a-zA-Z_]\w+)', flags=ascii_p3)
RE_enum = re.compile(r'\b(enum)\s+([a-zA-Z_]\w+)', flags=ascii_p3)
RE_typedef = re.compile(r'\b(typedef)\s+([a-zA-Z_]\w+)', flags=ascii_p3)
RE_doc = re.compile(r'(\bDocumentation/)?((\.\./)*[\w\-/]+)\.(rst|txt)')
RE_namespace = re.compile(r'^\s*..\s*c:namespace::\s*(\S+)\s*$')
Skipnames = [ 'for', 'if', 'register', 'sizeof', 'struct', 'unsigned' ]
Skipfuncs = [ 'open', 'close', 'read', 'write', 'fcntl', 'mmap',
              'select', 'poll', 'fork', 'execve', 'clone', 'ioctl',
              'socket' ]
c_namespace = ''
def markup_refs(docname, app, node):
    t = node.astext()
    done = 0
    repl = [ ]
    markup_func_sphinx2 = {RE_doc: markup_doc_ref,
                           RE_function: markup_c_ref,
                           RE_generic_type: markup_c_ref}
    markup_func_sphinx3 = {RE_doc: markup_doc_ref,
                           RE_function: markup_func_ref_sphinx3,
                           RE_struct: markup_c_ref,
                           RE_union: markup_c_ref,
                           RE_enum: markup_c_ref,
                           RE_typedef: markup_c_ref}
    if sphinx.version_info[0] >= 3:
        markup_func = markup_func_sphinx3
    else:
        markup_func = markup_func_sphinx2
    match_iterators = [regex.finditer(t) for regex in markup_func]
    sorted_matches = sorted(chain(*match_iterators), key=lambda m: m.start())
    for m in sorted_matches:
        if m.start() > done:
            repl.append(nodes.Text(t[done:m.start()]))
        repl.append(markup_func[m.re](docname, app, m))
        done = m.end()
    if done < len(t):
        repl.append(nodes.Text(t[done:]))
    return repl
failed_lookups = { }
def failure_seen(target):
    return (target) in failed_lookups
def note_failure(target):
    failed_lookups[target] = True
def markup_func_ref_sphinx3(docname, app, match):
    cdom = app.env.domains['c']
    base_target = match.group(2)
    target_text = nodes.Text(match.group(0))
    xref = None
    possible_targets = [base_target]
    if c_namespace:
        possible_targets.insert(0, c_namespace + "." + base_target)
    if base_target not in Skipnames:
        for target in possible_targets:
            if (target not in Skipfuncs) and not failure_seen(target):
                lit_text = nodes.literal(classes=['xref', 'c', 'c-func'])
                lit_text += target_text
                pxref = addnodes.pending_xref('', refdomain = 'c',
                                              reftype = 'function',
                                              reftarget = target,
                                              modname = None,
                                              classname = None)
                try:
                    xref = cdom.resolve_xref(app.env, docname, app.builder,
                                             'function', target, pxref,
                                             lit_text)
                except NoUri:
                    xref = None
                if xref:
                    return xref
                note_failure(target)
    return target_text
def markup_c_ref(docname, app, match):
    class_str = {
                 RE_function: 'c-func',
                 RE_generic_type: 'c-type',
                 RE_struct: 'c-struct',
                 RE_union: 'c-union',
                 RE_enum: 'c-enum',
                 RE_typedef: 'c-type',
                 }
    reftype_str = {
                   RE_function: 'function',
                   RE_generic_type: 'type',
                   RE_struct: 'struct',
                   RE_union: 'union',
                   RE_enum: 'enum',
                   RE_typedef: 'type',
                   }
    cdom = app.env.domains['c']
    base_target = match.group(2)
    target_text = nodes.Text(match.group(0))
    xref = None
    possible_targets = [base_target]
    if c_namespace:
        possible_targets.insert(0, c_namespace + "." + base_target)
    if base_target not in Skipnames:
        for target in possible_targets:
            if not (match.re == RE_function and target in Skipfuncs):
                lit_text = nodes.literal(classes=['xref', 'c', class_str[match.re]])
                lit_text += target_text
                pxref = addnodes.pending_xref('', refdomain = 'c',
                                              reftype = reftype_str[match.re],
                                              reftarget = target, modname = None,
                                              classname = None)
                try:
                    xref = cdom.resolve_xref(app.env, docname, app.builder,
                                             reftype_str[match.re], target, pxref,
                                             lit_text)
                except NoUri:
                    xref = None
                if xref:
                    return xref
    return target_text
def markup_doc_ref(docname, app, match):
    stddom = app.env.domains['std']
    absolute = match.group(1)
    target = match.group(2)
    if absolute:
       target = "/" + target
    xref = None
    pxref = addnodes.pending_xref('', refdomain = 'std', reftype = 'doc',
                                  reftarget = target, modname = None,
                                  classname = None, refexplicit = False)
    try:
        xref = stddom.resolve_xref(app.env, docname, app.builder, 'doc',
                                   target, pxref, None)
    except NoUri:
        xref = None
    if xref:
        return xref
    else:
        return nodes.Text(match.group(0))
def get_c_namespace(app, docname):
    source = app.env.doc2path(docname)
    with open(source) as f:
        for l in f:
            match = RE_namespace.search(l)
            if match:
                return match.group(1)
    return ''
def auto_markup(app, doctree, name):
    global c_namespace
    c_namespace = get_c_namespace(app, name)
    def text_but_not_a_reference(node):
        if not isinstance(node, nodes.Text) or isinstance(node.parent, nodes.literal):
            return False
        child_of_reference = False
        parent = node.parent
        while parent:
            if isinstance(parent, nodes.Referential):
                child_of_reference = True
                break
            parent = parent.parent
        return not child_of_reference
    for para in doctree.traverse(nodes.paragraph):
        for node in para.traverse(condition=text_but_not_a_reference):
            node.parent.replace(node, markup_refs(name, app, node))
def setup(app):
    app.connect('doctree-resolved', auto_markup)
    return {
        'parallel_read_safe': True,
        'parallel_write_safe': True,
        }
