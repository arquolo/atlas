#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "al/image.h"

#ifndef VERSION_INFO
#define VERSION_INFO "dev"
#endif

using namespace al;

py::buffer
get_item(const _Image& self, std::tuple<py::slice, py::slice> slices) {
    const auto& [ys, xs] = slices;

    auto y_min = ys.attr("start");
    auto x_min = xs.attr("start");
    auto y_max = ys.attr("stop");
    auto x_max = xs.attr("stop");
    auto y_step_ = ys.attr("step");
    auto x_step_ = xs.attr("step");

    size_t y_step = (!y_step_.is_none()) ? y_step_.cast<size_t>() : 1;
    size_t x_step = (!x_step_.is_none()) ? x_step_.cast<size_t>() : 1;
    if (y_step != x_step)
        throw std::runtime_error{"Y and X steps must be equal"};

    auto [level, scale] = self.get_level(y_step);

    return self.read({
        {(!y_min.is_none() ? y_min.cast<size_t>() / scale : 0),
         (!x_min.is_none() ? x_min.cast<size_t>() / scale : 0)},
        {(!y_max.is_none() ? y_max.cast<size_t>() / scale : self.levels[level].shape[0]),
         (!x_max.is_none() ? x_max.cast<size_t>() / scale : self.levels[level].shape[1])},
        level
    });
}

PYBIND11_MODULE(atlas, m) {
    m.attr("__version__") = VERSION_INFO;

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
            [](const _Image& self) {
                return std::visit([](auto v) {
                    return py::dtype::of<decltype(v)>();
                }, self.dtype);
            },
            "Data type")
        .def_property_readonly(
            "shape",
            [](const _Image& self) { return al::as_tuple(self.levels.front().shape); },
            "Shape")
        .def_property_readonly("scales", &_Image::scales, "Scales")
        .def("__getitem__", &get_item, py::arg("slices"))
        ;
}
