#!/usr/bin/python3

import os
from io import StringIO
from concurrent.futures import ThreadPoolExecutor
from contextlib import redirect_stdout
from pathlib import Path

import setuptools
from setuptools.command.build_ext import build_ext

PACKAGE = 'torchslide'
LIBRARIES = ['tiff', ('lib' if os.name == 'nt' else '') + 'openslide']


class BinaryDistribution(setuptools.Distribution):
    def has_ext_modules(self):
        return True


def patch(cls, target, wrapper):
    wrapped = getattr(cls, target)

    def wrapper_(*args, **kwargs):
        return wrapper(*args, wrapped=wrapped, **kwargs)

    setattr(cls, target, wrapper_)


def initialize(self, *, wrapped=None):
    wrapped(self)
    self.compile_options = [
        '-nologo', '-DNDEBUG', '-W4', '-MD', '-std:c++latest',
        # '-O1',  # minimize size
        '-O2',  # maximize speed
    ]
    self.ldflags_shared.clear()
    self.ldflags_shared.extend([
        '-nologo', '-INCREMENTAL:NO', '-DLL', '-MANIFEST:NO'])


def compile_(self, sources, wrapped=None, **kwargs):
    def worker(src):
        return wrapped(self, [src], **kwargs)

    with ThreadPoolExecutor(os.cpu_count()) as pool:
        with redirect_stdout(StringIO()):
            return [obj for obj, in pool.map(worker, sources)]


class PyBindInclude:
    def __init__(self, user=False):
        self.user = user

    def __str__(self):
        import pybind11
        return pybind11.get_include(self.user)


class BuildExt(build_ext):
    """A custom build extension for adding compiler-specific options."""

    def build_extensions(self):
        options = {
            'unix': (
                [f'-DVERSION_INFO="{self.distribution.get_version()}"',
                 '-std=c++2a',
                 '-fvisibility=hidden',
                 '-O3'],
                ['-Wl,-strip-all'],
                {'compile': compile_}),
            'msvc': (
                [f'-DVERSION_INFO=\\"{self.distribution.get_version()}\\"'],
                [],
                {'compile': compile_, 'initialize': initialize}
            )
        }
        option = options.get(self.compiler.compiler_type)
        if option is None:
            raise RuntimeError(f'only {set(options)} compilers supported')

        cargs, largs, patches = option
        for ext in self.extensions:
            ext.extra_compile_args = cargs
            ext.extra_link_args = largs

        for target, wrapper in patches.items():
            patch(self.compiler.__class__, target, wrapper)
        super().build_extensions()


setuptools.setup(
    name=f'{PACKAGE}-any',
    version='0.3.0',
    author='Paul Maevskikh',
    author_email='arquolo@gmail.com',
    url=f'https://github.com/arquolo/{PACKAGE}',
    description=f'{PACKAGE}-any - source version of {PACKAGE} for Python 3.6+',
    long_description=Path('README.md').read_text(),
    long_description_content_type='text/markdown',
    packages=setuptools.find_packages(),
    ext_package=PACKAGE,
    ext_modules=[
        setuptools.Extension(
            PACKAGE,
            list(map(str, Path(__file__).parent.glob(f'{PACKAGE}/*.cpp'))),
            include_dirs=[
                PyBindInclude(user=True),
                f'./{PACKAGE}',
                './__dependencies__/include',
            ],
            library_dirs=['__dependencies__/lib'],
            libraries=LIBRARIES,
            language='c++'
        ),
    ],
    package_data={
        '': ['*.dll', '*.pyd'] if os.name == 'nt' else ['*.so'],
    },
    install_requires=['pybind11>=2.2'],
    classifiers=[
        'Development Status :: 3 - Alpha',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.7',
        'License :: OSI Approved :: MIT License',
        'Operating System :: Microsoft :: Windows',
        'Operating System :: POSIX :: Linux',
    ],
    cmdclass={'build_ext': BuildExt},
    distclass=BinaryDistribution,
)
