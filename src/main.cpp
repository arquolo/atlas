#include <pybind11/pybind11.h>

#include <image_base.h>

namespace py = pybind11;

PYBIND11_MODULE(gigaslide, m) {
    py::enum_<enums::Color>(m, "Color")
        .value("Invalid", enums::Color::Invalid)
        .value("Monochrome", enums::Color::Monochrome)
        .value("RGB", enums::Color::RGB)
        .value("ARGB", enums::Color::ARGB)
        .value("Indexed", enums::Color::Indexed);

    py::enum_<enums::Data>(m, "Data")
        .value("Invalid", enums::Data::Invalid)
        .value("UInt8", enums::Data::UInt8)
        .value("UInt16", enums::Data::UInt16)
        .value("UInt32", enums::Data::UInt32)
        .value("Float", enums::Data::Float);

    py::enum_<enums::Codec>(m, "Codec")
        .value("RAW", enums::Codec::RAW)
        .value("JPEG", enums::Codec::JPEG)
        .value("LZW", enums::Codec::LZW)
        .value("JPEG2000", enums::Codec::JPEG2000);

    py::enum_<enums::Interpolation>(m, "Interpolation")
        .value("Nearest", enums::Interpolation::Nearest)
        .value("Linear", enums::Interpolation::Linear);

    py::class_<gs::ImageBase>(m, "ImageBase")
        .def(py::init<>())
        .def_property_readonly(
            "samples_per_pixel", &gs::ImageBase::samples_per_pixel)
        .def_property_readonly(
            "spacing", &gs::ImageBase::spacing)
        .def_property_readonly(
            "color", &gs::ImageBase::color)
        .def_property_readonly(
            "dtype", &gs::ImageBase::dtype);
}
