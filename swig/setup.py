"""
setup.py file for VSD SWIG
"""

from distutils.core import setup, Extension

setup (name = 'vsd',
       version = '0.1',
       author      = "SWIG Docs",
       description = """SWIG wrapper for VSD.""",
       py_modules = [ 'vsd', 'vsd_swig' ],
       ext_modules = [
           Extension('_vsd_swig', sources=['vsd_swig_wrap.c',],libraries=['vsd', 'dstc', 'rmc'])
       ],
       )
