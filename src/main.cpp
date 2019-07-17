#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "al/image.h"

using namespace al;

namespace {
template <typename T>
struct numpy_dtype {
    static constexpr py::dtype visit() noexcept { return py::dtype::of<T>(); }
};
}

PYBIND11_MODULE(atlas, m) {
#ifdef VERSION_INFO
    m.attr("__version__") = VERSION_INFO;
#else
    m.attr("__version__") = "dev";
#endif

    py::enum_<Color>(m, "Color")
        .value("Invalid", Color::Invalid)
        .value("Indexed", Color::Indexed)
        .value("Monochrome", Color::Monochrome)
        .value("RGB", Color::RGB)
        .value("ARGB", Color::ARGB)
        ;

    py::enum_<Bitstream>(m, "Bitstream")
        .value("RAW", Bitstream::RAW)
        .value("LZW", Bitstream::LZW)
        .value("JPEG", Bitstream::JPEG)
        .value("JPEG2000", Bitstream::JPEG2000)
        ;

    py::enum_<Interpolation>(m, "Interpolation")
        .value("Nearest", Interpolation::Nearest)
        .value("Linear", Interpolation::Linear)
        ;

    py::class_<_Image>(m, "Image")
        .def(py::init(&_Image::make), py::arg("path"))
        .def_property_readonly(
            "dtype",
            [](const _Image& self){ return from_dtype<numpy_dtype>(self.dtype()); },
            "Data type")
        .def_property_readonly(
            "shape",
            [](const _Image& self) { return al::as_tuple(self.shape()); },
            "Shape")
        .def_property_readonly("scales", &_Image::scales, "Scales")
        .def("__getitem__", &_Image::read, py::arg("slices"))
        ;
}
