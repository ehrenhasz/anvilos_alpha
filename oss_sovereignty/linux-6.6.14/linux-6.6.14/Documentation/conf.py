import sys
import os
import sphinx
import shutil
def have_command(cmd):
    """Search ``cmd`` in the ``PATH`` environment.
    If found, return True.
    If not found, return False.
    """
    return shutil.which(cmd) is not None
major, minor, patch = sphinx.version_info[:3]
if (major < 2) or (major == 2 and minor < 4):
    print('WARNING: support for Sphinx < 2.4 will be removed soon.')
sys.path.insert(0, os.path.abspath('sphinx'))
from load_config import loadConfig
needs_sphinx = '1.7'
extensions = ['kerneldoc', 'rstFlatTable', 'kernel_include',
              'kfigure', 'sphinx.ext.ifconfig', 'automarkup',
              'maintainers_include', 'sphinx.ext.autosectionlabel',
              'kernel_abi', 'kernel_feat']
if major >= 3:
    if (major > 3) or (minor > 0 or patch >= 2):
        c_id_attributes = [
            "__restrict__",
            "__iomem",
            "__kernel",
            "noinstr",
            "notrace",
            "__percpu",
            "__rcu",
            "__user",
            "__force",
            "__alias",
            "__aligned",
            "__aligned_largest",
            "__always_inline",
            "__assume_aligned",
            "__cold",
            "__attribute_const__",
            "__copy",
            "__pure",
            "__designated_init",
            "__visible",
            "__printf",
            "__scanf",
            "__gnu_inline",
            "__malloc",
            "__mode",
            "__no_caller_saved_registers",
            "__noclone",
            "__nonstring",
            "__noreturn",
            "__packed",
            "__pure",
            "__section",
            "__always_unused",
            "__maybe_unused",
            "__used",
            "__weak",
            "noinline",
            "__fix_address",
            "__init_memblock",
            "__meminit",
            "__init",
            "__ref",
            "asmlinkage",
            "__bpf_kfunc",
        ]
else:
    extensions.append('cdomain')
autosectionlabel_prefix_document = True
autosectionlabel_maxdepth = 2
have_latex =  have_command('latex')
have_dvipng = have_command('dvipng')
load_imgmath = have_latex and have_dvipng
if 'SPHINX_IMGMATH' in os.environ:
    env_sphinx_imgmath = os.environ['SPHINX_IMGMATH']
    if 'yes' in env_sphinx_imgmath:
        load_imgmath = True
    elif 'no' in env_sphinx_imgmath:
        load_imgmath = False
    else:
        sys.stderr.write("Unknown env SPHINX_IMGMATH=%s ignored.\n" % env_sphinx_imgmath)
load_imgmath = (load_imgmath or (major == 1 and minor < 8)
                or 'epub' in sys.argv)
if load_imgmath:
    extensions.append("sphinx.ext.imgmath")
    math_renderer = 'imgmath'
else:
    math_renderer = 'mathjax'
templates_path = ['sphinx/templates']
source_suffix = '.rst'
master_doc = 'index'
project = 'The Linux Kernel'
copyright = 'The kernel development community'
author = 'The kernel development community'
try:
    makefile_version = None
    makefile_patchlevel = None
    for line in open('../Makefile'):
        key, val = [x.strip() for x in line.split('=', 2)]
        if key == 'VERSION':
            makefile_version = val
        elif key == 'PATCHLEVEL':
            makefile_patchlevel = val
        if makefile_version and makefile_patchlevel:
            break
except:
    pass
finally:
    if makefile_version and makefile_patchlevel:
        version = release = makefile_version + '.' + makefile_patchlevel
    else:
        version = release = "unknown version"
def get_cline_version():
    c_version = c_release = ''
    for arg in sys.argv:
        if arg.startswith('version='):
            c_version = arg[8:]
        elif arg.startswith('release='):
            c_release = arg[8:]
    if c_version:
        if c_release:
            return c_version + '-' + c_release
        return c_version
    return version 
language = 'en'
exclude_patterns = ['output']
pygments_style = 'sphinx'
todo_include_todos = False
primary_domain = 'c'
highlight_language = 'none'
html_theme = 'alabaster'
html_css_files = []
if "DOCS_THEME" in os.environ:
    html_theme = os.environ["DOCS_THEME"]
if html_theme == 'sphinx_rtd_theme' or html_theme == 'sphinx_rtd_dark_mode':
    try:
        import sphinx_rtd_theme
        html_theme_path = [sphinx_rtd_theme.get_html_theme_path()]
        html_css_files = [
            'theme_overrides.css',
        ]
        if html_theme == 'sphinx_rtd_dark_mode':
            try:
                import sphinx_rtd_dark_mode
                extensions.append('sphinx_rtd_dark_mode')
            except ImportError:
                html_theme == 'sphinx_rtd_theme'
        if html_theme == 'sphinx_rtd_theme':
                html_css_files.append('theme_rtd_colors.css')
        html_theme_options = {
            'navigation_depth': -1,
        }
    except ImportError:
        html_theme = 'alabaster'
if "DOCS_CSS" in os.environ:
    css = os.environ["DOCS_CSS"].split(" ")
    for l in css:
        html_css_files.append(l)
if major <= 1 and minor < 8:
    html_context = {
        'css_files': [],
    }
    for l in html_css_files:
        html_context['css_files'].append('_static/' + l)
if  html_theme == 'alabaster':
    html_theme_options = {
        'description': get_cline_version(),
        'page_width': '65em',
        'sidebar_width': '15em',
        'fixed_sidebar': 'true',
        'font_size': 'inherit',
        'font_family': 'serif',
    }
sys.stderr.write("Using %s theme\n" % html_theme)
html_static_path = ['sphinx-static']
smartquotes = False
html_sidebars = { '**': ['searchbox.html', 'kernel-toc.html', 'sourcelink.html']}
if html_theme == 'alabaster':
    html_sidebars['**'].insert(0, 'about.html')
htmlhelp_basename = 'TheLinuxKerneldoc'
latex_elements = {
    'papersize': 'a4paper',
    'pointsize': '11pt',
    'inputenc': '',
    'utf8extra': '',
    'sphinxsetup': '''
        hmargin=0.5in, vmargin=1in,
        parsedliteralwraps=true,
        verbatimhintsturnover=false,
    ''',
    'extrapackages': r'\usepackage{setspace}',
    'preamble': '''
        % Use some font with UTF-8 support with XeLaTeX
        \\usepackage{fontspec}
        \\setsansfont{DejaVu Sans}
        \\setromanfont{DejaVu Serif}
        \\setmonofont{DejaVu Sans Mono}
    ''',
}
if major == 1:
    latex_elements['preamble']  += '\\renewcommand*{\\DUrole}[2]{ 
latex_elements['preamble'] += '''
        % Load kerneldoc specific LaTeX settings
	\\input{kerneldoc-preamble.sty}
'''
latex_documents = [
]
for fn in os.listdir('.'):
    doc = os.path.join(fn, "index")
    if os.path.exists(doc + ".rst"):
        has = False
        for l in latex_documents:
            if l[0] == doc:
                has = True
                break
        if not has:
            latex_documents.append((doc, fn + '.tex',
                                    'Linux %s Documentation' % fn.capitalize(),
                                    'The kernel development community',
                                    'manual'))
latex_additional_files = [
    'sphinx/kerneldoc-preamble.sty',
]
man_pages = [
    (master_doc, 'thelinuxkernel', 'The Linux Kernel Documentation',
     [author], 1)
]
texinfo_documents = [
    (master_doc, 'TheLinuxKernel', 'The Linux Kernel Documentation',
     author, 'TheLinuxKernel', 'One line description of project.',
     'Miscellaneous'),
]
epub_title = project
epub_author = author
epub_publisher = author
epub_copyright = copyright
epub_exclude_files = ['search.html']
pdf_documents = [
    ('kernel-documentation', u'Kernel', u'Kernel', u'J. Random Bozo'),
]
kerneldoc_bin = '../scripts/kernel-doc'
kerneldoc_srctree = '..'
loadConfig(globals())
