import sys
import os
sys.path.insert(0, os.path.abspath("."))
micropy_version = os.getenv("MICROPY_VERSION") or "latest"
micropy_all_versions = (os.getenv("MICROPY_ALL_VERSIONS") or "latest").split(",")
url_pattern = "%s/en/%%s" % (os.getenv("MICROPY_URL_PREFIX") or "/",)
html_context = {
    "cur_version": micropy_version,
    "all_versions": [(ver, url_pattern % ver) for ver in micropy_all_versions],
    "downloads": [
        ("PDF", url_pattern % micropy_version + "/micropython-docs.pdf"),
    ],
    "is_release": micropy_version != "latest",
}
extensions = [
    "sphinx.ext.autodoc",
    "sphinx.ext.doctest",
    "sphinx.ext.intersphinx",
    "sphinx.ext.todo",
    "sphinx.ext.coverage",
    "sphinxcontrib.jquery",
]
templates_path = ["templates"]
source_suffix = ".rst"
master_doc = "index"
project = "MicroPython"
copyright = "- The MicroPython Documentation is Copyright © 2014-2024, Damien P. George, Paul Sokolovsky, and contributors"
version = release = micropy_version
exclude_patterns = ["build", ".venv"]
default_role = "any"
pygments_style = "sphinx"
rst_epilog = """
.. include:: /templates/replace.inc
"""
on_rtd = os.environ.get("READTHEDOCS", None) == "True"
if not on_rtd:  # only import and set the theme if we're building docs locally
    try:
        import sphinx_rtd_theme
        html_theme = "sphinx_rtd_theme"
        html_theme_path = [sphinx_rtd_theme.get_html_theme_path(), "."]
    except:
        html_theme = "default"
        html_theme_path = ["."]
else:
    html_theme_path = ["."]
html_favicon = "static/favicon.ico"
html_static_path = ["static"]
html_css_files = [
    "custom.css",
]
html_last_updated_fmt = "%d %b %Y"
html_additional_pages = {"index": "topindex.html"}
htmlhelp_basename = "MicroPythondoc"
latex_elements = {
    "preamble": r"\setcounter{tocdepth}{2}",
}
latex_documents = [
    (
        master_doc,
        "MicroPython.tex",
        "MicroPython Documentation",
        "Damien P. George, Paul Sokolovsky, and contributors",
        "manual",
    ),
]
latex_engine = "xelatex"
man_pages = [
    (
        "index",
        "micropython",
        "MicroPython Documentation",
        ["Damien P. George, Paul Sokolovsky, and contributors"],
        1,
    ),
]
texinfo_documents = [
    (
        master_doc,
        "MicroPython",
        "MicroPython Documentation",
        "Damien P. George, Paul Sokolovsky, and contributors",
        "MicroPython",
        "One line description of project.",
        "Miscellaneous",
    ),
]
intersphinx_mapping = {"python": ("https://docs.python.org/3.5", None)}
