# TorchSlide
- Works on Python-3.6+
- Compiles from sources
- Provides array-like interface for reading BigTIFF/SVS files

## Usage:

```python
import torchslide as ts

slide = ts.Image('test.svs')
shape: 'Tuple[int]' = slide.shape
scales: 'Tuple[int]' = slide.scales
image: np.ndarray = slide[:2048, :2048]  # get numpy.ndarray
```

## Installation

Currently `torchslide` is only supported under 64-bit Windows and Linux machines.
Compilation on other architectures should be relatively straightforward as no OS-specific libraries or headers are used.
The easiest way to install the software is to download package from `PyPI`.

## Compilation

To compile the code yourself, some prerequesites are required.
First, we use `setuptools >= 40` as our build system and Microsoft Build Tools or GCC as the compiler.
The software depends on numerous third-party libraries:

- libtiff (http://www.libtiff.org/)
- libjpeg (http://libjpeg.sourceforge.net/)
- DCMTK (http://dicom.offis.de/dcmtk.php.en)
- OpenSlide (http://openslide.org/)
- zlib (http://www.zlib.net/)

To help developers compile this software themselves we provide the necesarry binaries (Visual Studio 2017, 64-bit) for all third party libraries on Windows.
If you want to provide the packages yourself, there are no are no strict version requirements, except for libtiff (4.0.1 and higher).
On Linux all packages can be installed through the package manager on Ubuntu-derived systems (tested on Ubuntu and Kubuntu 16.04 LTS).

To compile the source code yourself, first make sure all third-party libraries are installed.
