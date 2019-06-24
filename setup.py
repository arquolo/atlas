from pathlib import Path

import setuptools
from setuptools.command.build_ext import build_ext


class PyBindInclude:
    """
    Helper class to determine the pybind11 include path
    The purpose of this class is to postpone importing pybind11
    until it is actually installed, so that the ``get_include()``
    method can be invoked. """

    def __init__(self, user=False):
        self.user = user

    def __str__(self):
        import pybind11
        include_path = pybind11.get_include(self.user)
        print(include_path)
        return include_path


class BuildExt(build_ext):
    """A custom build extension for adding compiler-specific options."""

    def build_extensions(self):
        opts_mapping = {
            'unix': [
                f'-DVERSION_INFO="{self.distribution.get_version()}"',
                '-std=c++20',
                '-fvisibility=hidden',
                '-Oz',
            ],
            'msvc': [
                f'/DVERSION_INFO=\\"{self.distribution.get_version()}\\"',
                '/std:c++latest',
                '/Os',
            ]
        }
        opts = opts_mapping.get(self.compiler.compiler_type)
        if opts is None:
            raise RuntimeError(f'only {set(opts_mapping)} compilers supported')

        for ext in self.extensions:
            ext.extra_compile_args = opts
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
                'src/utility.cpp',
                'src/codec.cpp',
                'src/codec_jpeg2000.cpp',
                'src/codec_tiff_tools.cpp',
                'src/codec_tiff.cpp',
                'src/main.cpp',
                # 'src/image_writer.cpp',
            ],
            # [str(s) for s in (Path(__file__).parent / 'src').rglob('*.cpp')],
            include_dirs=[
                # PyBindInclude(),
                PyBindInclude(user=True),
                './include',
                'C:/Users/pmaevskikh/Sources/deps/include/tiff/',
                'C:/Users/pmaevskikh/Sources/deps/include/openjpeg/',
            ],
            libraries=[
                'openjp2', 'tiff'
            ],
            library_dirs=[
                'C:/Users/pmaevskikh/Sources/deps/lib/',
            ],
            language='c++'
        ),
    ],
    install_requires=['pybind11>=2.2'],
    cmdclass={'build_ext': BuildExt},
    zip_safe=False,
)
