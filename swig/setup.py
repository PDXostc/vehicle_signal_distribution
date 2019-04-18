"""
setup.py file for VSD SWIG
"""

import setuptools
import os
from distutils.core import setup, Extension

setup (name = 'vsd',
       version=os.environ['VERSION'],
       author      = "SWIG Docs",
       description = """SWIG wrapper for VSD.""",
       py_modules = [ 'vsd', 'vsd_swig' ],
       ext_modules = [
           Extension('_vsd_swig', sources=['vsd_swig_wrap.c',],libraries=['vsd', 'dstc', 'rmc'])
       ],
       )
