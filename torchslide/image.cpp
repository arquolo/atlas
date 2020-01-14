#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "image.h"

#ifndef VERSION_INFO
#define VERSION_INFO "dev"
#endif

namespace py = pybind11;
using namespace ts;

Size ImageInfo::get_scale(LevelInfo const& info) const noexcept {
    return static_cast<Size>(
        std::round(static_cast<double>(this->levels.at(0).shape.front()) /
                   static_cast<double>(info.shape.front())));
}

std::vector<Size> ImageInfo::scales() const noexcept {
    std::vector<Size> scales_;
    for (auto const& [_, level_info] : this->levels)
        scales_.push_back(this->get_scale(level_info));
    return scales_;
}

std::pair<Level, LevelInfo> ImageInfo::get_level(Size scale) const noexcept {
    auto it = std::find_if(levels.begin(), levels.end(), [=, this](auto const& p) {
        return scale <= get_scale(p.second);
    });
    if (it == levels.end())
        --it;
    return *it;
}

Image::~Image() noexcept {}

py::buffer
get_item(Image const& self, std::tuple<py::slice, py::slice> slices) {
    auto const& [ys, xs] = slices;

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

    auto const& [level, info] = self.get_level(y_step);
    auto scale = self.get_scale(info);

    Box box{
        {(!y_min.is_none() ? y_min.cast<Size>() / scale : 0),
         (!x_min.is_none() ? x_min.cast<Size>() / scale : 0)},
        {(!y_max.is_none() ? y_max.cast<Size>() / scale : info.shape[0]),
         (!x_max.is_none() ? x_max.cast<Size>() / scale : info.shape[1])},
        level
    };
    return self.read_any(box);
}

PYBIND11_MODULE(torchslide, m) {
    m.attr("__version__") = VERSION_INFO;
    m.attr("__all__") = py::make_tuple("Image");

    py::class_<Image>(m, "Image")
        .def(py::init(&Image::make), py::arg("path"))
        .def_property_readonly(
            "dtype",
            [](Image const& self) {
                return std::visit([](auto v) {
                    return py::dtype::of<decltype(v)>();
                }, self.dtype);
            },
            "Data type")
        .def_property_readonly(
            "shape",
            [](Image const& self) { return ts::as_tuple(self.levels.at(0).shape); },
            "Shape")
        .def_property_readonly("scales", &Image::scales, "Scales")
        .def("__getitem__", &get_item, py::arg("slices"))
        ;
}
