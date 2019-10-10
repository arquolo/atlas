import os
import sys
from functools import wraps
from pathlib import Path

import setuptools
from setuptools.command.build_ext import build_ext

PACKAGE = 'torchslide'


class BinaryDistribution(setuptools.Distribution):
    def has_ext_modules(self):
        return True


def patch_init(cls):
    def wrapper(self):
        initialize(self)
        self.compile_options = [
            '-nologo', '-DNDEBUG', '-W4',
            # '-O1',  # minimize size
            '-O2',  # maximize speed
            # '-MT',  # static linkage
            '-MD',  # dynamic linkage
            '-std:c++latest',
        ]
        self.ldflags_shared.clear()
        self.ldflags_shared.extend([
            '-nologo', '-INCREMENTAL:NO', '-DLL', '-MANIFEST:NO'])

    initialize = cls.initialize
    cls.initialize = wraps(initialize)(wrapper)


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
                 '-std=c++17',
                 '-fvisibility=hidden',
                 '-O3'],
                (lambda cls: None)),
            'msvc': (
                [f'-DVERSION_INFO=\\"{self.distribution.get_version()}\\"'],
                patch_init
            )
        }
        option = options.get(self.compiler.compiler_type)
        if option is None:
            raise RuntimeError(f'only {set(options)} compilers supported')

        args, patch = option
        for ext in self.extensions:
            ext.extra_compile_args = args
        patch(self.compiler.__class__)
        super().build_extensions()


setuptools.setup(
    name=f'{PACKAGE}-any',
    version='0.0.6',
    author='Paul Maevskikh',
    author_email='arquolo@gmail.com',
    url=f'https://github.com/arquolo/{PACKAGE}',
    description='Library for read/write access to large image files',
    long_description='',
    packages=setuptools.find_packages(),
    ext_package=PACKAGE,
    ext_modules=[
        setuptools.Extension(
            PACKAGE,
            [
                f'{PACKAGE}/io/jpeg2000.cpp',
                f'{PACKAGE}/io/tiff.cpp',
                f'{PACKAGE}/image.cpp',
                f'{PACKAGE}/image_tiff.cpp',
                f'{PACKAGE}/main.cpp',
                # f'{PACKAGE}/writer.cpp',
            ],
            # list(map(str, Path(__file__).parent.glob(f'{PACKAGE}/*.cpp'))),
            include_dirs=[
                # PyBindInclude(),
                PyBindInclude(user=True),
                f'./{PACKAGE}',
                './__dependencies__/include',
            ],
            library_dirs=['__dependencies__/lib'],
            libraries=['openjp2', 'tiff'],
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
