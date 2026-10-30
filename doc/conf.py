# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'OOFEM'
copyright = '2025, Bořek Patzák &al.'
author = 'Bořek Patzák &al.'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = ['myst_parser']
myst_enable_extensions=['attrs_inline','linkify','colon_fence']

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

#breathe_projects={'oofem':'refman/xml/'}
#breathe_default_project='oofem'

# Shared settings, among them the alabaster theme used by every manual.  This
# directory holds doc/_static itself, hence the different html_static_path.
import os, sys
sys.path.insert(0, os.path.abspath('.'))
from global_conf import html_theme, html_theme_options, html_sidebars, html_css_files

html_static_path = ['_static']
html_theme_options = dict(
    html_theme_options,
    # Shown under the table of contents in the sidebar.
    extra_nav_links={
        'Element Library': 'http://www.oofem.org/resources/doc/elementlibmanual/html/elementlibmanual.html',
        'Material Library': 'http://www.oofem.org/resources/doc/matlibmanual/html/matlibmanual.html',
        'Extractor': 'http://www.oofem.org/resources/doc/extractorInput/html/index.html',
        'C++ reference': 'http://www.oofem.org/resources/doc/oofemrefman/index.html',
        'GitHub': 'https://github.com/oofem/oofem',
        'Homepage': 'https://oofem.org',
    },
)
