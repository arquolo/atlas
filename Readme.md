# ATLAS - library for read/write access to large image files.

ATLAS is an open source platform for visualizing, annotating and automatically analyzing whole-slide images.
It consists of several key-components (slide input/output, image processing, viewer) which can be used seperately.
It is built on top of several well-developed open source packages like OpenSlide, but also tries to extend them in several meaningful ways.

#### Features

- Reading of scanned whole-slide images from several different vendors (Aperio, Ventana, Hamamatsu, Olympus, support for fluorescence images in Leica LIF format)
- Writing of generic multi-resolution tiled TIFF files for ARGB, RGB, Indexed and monochrome images (including support for different data types like float)
- Python wrapping of the IO library for access to multi-resolution images through Numpy array's
- Viewer and reading library can easily be extended by implementing plugins using one of the four interface (tools, filters, extensions, fileformats)

#### Installation

Currently `atlas` is only supported under 64-bit Windows and Linux machines.
Compilation on other architectures should be relatively straightforward as no OS-specific libraries or headers are used.
The easiest way to install the software is to download package from `PyPI`.

#### Compilation

To compile the code yourself, some prerequesites are required.
First, we use `setuptools >= 40` as our build system and Microsoft Build Tools or GCC as the compiler.
The software depends on numerous third-party libraries:

- libtiff (http://www.libtiff.org/)
- libjpeg (http://libjpeg.sourceforge.net/)
- JasPer (http://www.ece.uvic.ca/~frodo/jasper/)
- DCMTK (http://dicom.offis.de/dcmtk.php.en)
- OpenSlide (http://openslide.org/)
- PugiXML (http://pugixml.org/)
- zlib (http://www.zlib.net/)

To help developers compile this software themselves we provide the necesarry binaries (Visual Studio 2017, 64-bit) for all third party libraries on Windows.
If you want to provide the packages yourself, there are no are no strict version requirements, except for libtiff (4.0.1 and higher).
On Linux all packages can be installed through the package manager on Ubuntu-derived systems (tested on Ubuntu and Kubuntu 16.04 LTS).

To compile the source code yourself, first make sure all third-party libraries are installed.
