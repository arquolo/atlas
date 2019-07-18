from dataclasses import dataclass
from functools import wraps
from pathlib import Path

import setuptools
from setuptools.command.build_ext import build_ext


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


@dataclass
class PyBindInclude:
    user: bool = False

    def __str__(self):
        import pybind11
        return pybind11.get_include(self.user)


class BuildExt(build_ext):
    """A custom build extension for adding compiler-specific options."""

    def build_extensions(self):
        options = {
            'unix': (
                [f'-DVERSION_INFO="{self.distribution.get_version()}"',
                 '-std=c++20',
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
    name='atlas',
    version='0.0.1',
    author='Paul Maevskikh',
    author_email='arquolo@gmail.com',
    url='https://github.com/arquolo/atlas',
    description='Library for read/write access to large image files',
    long_description='',
    packages=setuptools.find_packages(),
    ext_modules=[
        setuptools.Extension(
            'atlas',
            [
                'src/io/jpeg2000.cpp',
                'src/io/tiff.cpp',
                'src/image.cpp',
                'src/image_tiff.cpp',
                'src/main.cpp',
                # 'src/writer.cpp',
            ],
            # list(map(str, Path(__file__).parent.glob('src/*.cpp'))),
            include_dirs=[
                # PyBindInclude(),
                PyBindInclude(user=True),
                './include',
                './__dependencies__/include',
            ],
            library_dirs=['./__dependencies__/lib'],
            libraries=['openjp2', 'tiff'],
            language='c++'
        ),
    ],
    install_requires=['pybind11>=2.2'],
    cmdclass={'build_ext': BuildExt},
    zip_safe=False,
)
