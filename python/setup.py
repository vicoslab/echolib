import sys
import os
import glob
from typing import Callable
import setuptools

from setuptools import setup

__version__ = "0.1.0"

from distutils.core import setup, Extension
from distutils.command.build_ext import build_ext

class get_pybind_include(object):
    def __str__(self):
        import pybind11
        return pybind11.get_include()

class get_numpy_include(object):
    def __str__(self):
        import numpy
        return numpy.get_include()

root = os.path.abspath(os.path.dirname(__file__))
platform = os.getenv("ECHOLIB_PYTHON_PLATFORM", sys.platform)

if platform.startswith('linux'):
    library_suffix = '.so'
elif platform in ['darwin']:
    library_suffix = '.dylib'
elif platform.startswith('win'):
    library_suffix = '.dll'

class build_ext_ctypes(build_ext):

    def build_extension(self, ext):
        self._ctypes = isinstance(ext, CTypes)

        def _realize(c):
            if isinstance(c, Callable):
                return c()
            else:
                return c

        ct = self.compiler.compiler_type
        opts = self.c_opts.get(ct, [])
        #link_opts = self.l_opts.get(ct, [])
        if ct == 'unix':
            opts.append("-std=c++11")

        ext.include_dirs = [_realize(x) for x in ext.include_dirs]

        ext.extra_compile_args = opts

        return super().build_extension(ext)

    def get_export_symbols(self, ext):
        if self._ctypes:
            return ext.export_symbols
        return super().get_export_symbols(ext)

    def get_ext_filename(self, ext_name):
        if self._ctypes:
            return ext_name + library_suffix
        return super().get_ext_filename(ext_name)

class CTypes(Extension): 
    pass

try:
    from wheel.bdist_wheel import bdist_wheel as _bdist_wheel

    class bdist_wheel(_bdist_wheel):

        def finalize_options(self):
            _bdist_wheel.finalize_options(self)
            # Mark us as not a pure python package
            self.root_is_pure = False

        def get_tag(self):
            python, abi, plat = _bdist_wheel.get_tag(self)
            # We don't contain any python source
            python, abi = 'py2.py3', 'none'
            return python, abi, plat
except ImportError:
    bdist_wheel = None

varargs = dict()

if os.path.isfile(os.path.join("echolib", "pyecho" + library_suffix)):
    varargs["package_data"] = {"echolib" : ["pyecho" + library_suffix]}
    varargs["cmdclass"] = {'bdist_wheel': bdist_wheel}
    varargs["setup_requires"] = ['wheel']
elif os.path.isfile(os.path.join("trax", "trax.c")):
    sources = glob.glob("trax/*.c") + glob.glob("trax/*.cpp")
    varargs["ext_modules"] = [CTypes("echolib.pyecho", sources=sources, define_macros=[("ECHOLIB_EXPORTS", "1")])]
    varargs["cmdclass"] = {'build_ext': build_ext_ctypes}
    varargs["setup_requires"] = ["pybind11>=2.5.0", "numpy>=1.16"]

varargs["package_data"]['echolib.messages.library'] = ['*.msg']
varargs["package_data"]['echolib.messages.templates'] = ['*.tpl']

setup(
    name='echolib',
    version=__version__,
    author='Luka Cehovin Zajc',
    author_email='luka.cehovin@gmail.com',
    url='https://github.com/vicoslab/echolib',
    description='Python bindings for echolib',
    long_description='',
    packages=setuptools.find_packages(),
    include_package_data=True,
    python_requires='>=3.6',
    install_requires=["numpy>=1.16", "jinja2", "pyparsing"],
    zip_safe=False,
    entry_points={
        'console_scripts': [
            'echodaemon = echolib.__main__:main',
            'echorouter = echolib.__main__:main',
            'echogenerate = echolib.messages.cli:main',
        ],
    },
    **varargs
)