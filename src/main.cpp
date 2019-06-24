#include <pybind11/pybind11.h>

#include "codec.h"
#include "codec_tiff.h"
#include "utility.h"

using namespace al;
namespace py = pybind11;

struct Image {
    std::unique_ptr<_Codec> codec;

    Image(const std::string& path): codec{
        [path = Path{path}]() {
            auto ext = path.extension();
            if (ext == ".tif" || ext == ".tiff")
                return std::make_unique<codec::tiff::Tiff>(path);
            throw py::type_error{"Unsupported extension"};
        }()
    } {}

    Buffer getitem(std::tuple<py::slice, py::slice> slices) const;

    auto shape() const {
        const auto& s = codec->shape();
        return std::make_tuple(s[0], s[1], codec->samples);
    }
};

Buffer Image::getitem(std::tuple<py::slice, py::slice> slices) const {
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

    auto [level, scale] = codec->level_for(y_step);

    return codec->read({
        {(!y_min.is_none() ? y_min.cast<size_t>() / scale : 0),
         (!x_min.is_none() ? x_min.cast<size_t>() / scale : 0)},
        {(!y_max.is_none() ? y_max.cast<size_t>() / scale : codec->shape(level)[0]),
         (!x_max.is_none() ? x_max.cast<size_t>() / scale : codec->shape(level)[1])},
        level
    });
}

namespace {
template <typename T>
struct numpy_dtype {
    static constexpr py::dtype visit() noexcept { return py::dtype::of<T>(); }
};
}

PYBIND11_MODULE(atlas, m) {
    py::enum_<Color>(m, "Color")
        .value("Invalid", Color::Invalid)
        .value("Indexed", Color::Indexed)
        .value("Monochrome", Color::Monochrome)
        .value("RGB", Color::RGB)
        .value("ARGB", Color::ARGB);

    py::enum_<Bitstream>(m, "Bitstream")
        .value("RAW", Bitstream::RAW)
        .value("LZW", Bitstream::LZW)
        .value("JPEG", Bitstream::JPEG)
        .value("JPEG2000", Bitstream::JPEG2000);

    py::enum_<Interpolation>(m, "Interpolation")
        .value("Nearest", Interpolation::Nearest)
        .value("Linear", Interpolation::Linear);

    py::class_<Image>(m, "Image")
        .def(py::init<const std::string&>(), py::arg("path"))
        .def("__getitem__", &Image::getitem, py::arg("slices"))
        .def_property_readonly("shape", &Image::shape, "Shape")
        .def_property_readonly(
            "dtype",
            [](const Image& self) { return typed<numpy_dtype>(self.codec->dtype()); },
            "Data type");
}
