#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "image.h"

#ifndef VERSION_INFO
#define VERSION_INFO "dev"
#endif

using namespace ts;

py::buffer
get_item(const abc::Image& self, std::tuple<py::slice, py::slice> slices) {
    const auto& [ys, xs] = slices;

    auto y_min = ys.attr("start");
    auto x_min = xs.attr("start");
    auto y_max = ys.attr("stop");
    auto x_max = xs.attr("stop");
    auto y_step_ = ys.attr("step");
    auto x_step_ = xs.attr("step");

    size_t y_step = (!y_step_.is_none()) ? y_step_.cast<Size>() : 1;
    size_t x_step = (!x_step_.is_none()) ? x_step_.cast<Size>() : 1;
    if (y_step != x_step)
        throw std::runtime_error{"Y and X steps must be equal"};

    const auto& [level, info] = self.get_level(y_step);
    auto scale = self.get_scale(info);

    return self.read_any({
        {(!y_min.is_none() ? y_min.cast<Size>() / scale : 0),
         (!x_min.is_none() ? x_min.cast<Size>() / scale : 0)},
        {(!y_max.is_none() ? y_max.cast<Size>() / scale : info.shape[0]),
         (!x_max.is_none() ? x_max.cast<Size>() / scale : info.shape[1])},
        level
    });
}

PYBIND11_MODULE(torchslide, m) {
    m.attr("__version__") = VERSION_INFO;
    m.attr("__all__") = py::make_tuple("Bitstream", "Interpolation", "Image");

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

    py::class_<abc::Image>(m, "Image")
        .def(py::init(&abc::Image::make), py::arg("path"))
        .def_property_readonly(
            "dtype",
            [](const abc::Image& self) {
                return std::visit([](auto v) {
                    return py::dtype::of<decltype(v)>();
                }, self.dtype);
            },
            "Data type")
        .def_property_readonly(
            "shape",
            [](const abc::Image& self) { return ts::as_tuple(self.levels.at(0).shape); },
            "Shape")
        .def_property_readonly("scales", &abc::Image::scales, "Scales")
        .def("__getitem__", &get_item, py::arg("slices"))
        ;
}
