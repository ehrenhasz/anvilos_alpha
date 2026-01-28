import sys
import os
sys.path.insert(0, os.path.abspath('../..'))
extensions = [
    'sphinx.ext.autodoc',
]
templates_path = ['_templates']
source_suffix = '.rst'
master_doc = 'index'
project = u'pyzfs'
copyright = u'2015, ClusterHQ'
author = u'ClusterHQ'
version = '0.2.3'
release = '0.2.3'
language = None
exclude_patterns = []
pygments_style = 'sphinx'
todo_include_todos = False
html_theme = 'classic'
html_static_path = ['_static']
htmlhelp_basename = 'pyzfsdoc'
latex_elements = {
}
latex_documents = [
  (master_doc, 'pyzfs.tex', u'pyzfs Documentation',
   u'ClusterHQ', 'manual'),
]
man_pages = [
    (master_doc, 'pyzfs', u'pyzfs Documentation',
     [author], 1)
]
texinfo_documents = [
  (master_doc, 'pyzfs', u'pyzfs Documentation',
   author, 'pyzfs', 'One line description of project.',
   'Miscellaneous'),
]
autodoc_member_order = 'bysource'
import functools
def no_op_wraps(func):
    def wrapper(decorator):
        return func
    return wrapper
functools.wraps = no_op_wraps
