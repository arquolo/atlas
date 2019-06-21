#include "image_base.h"

namespace gs {

size_t ImageBase::samples_per_pixel() const {
    return samples_per_pixel_;
}

enums::Color ImageBase::color() const {
    return color_type_;
}

std::vector<double> ImageBase::spacing() const {
    return spacing_;
}

enums::Data ImageBase::dtype() const {
    return data_type_;
}

} // namespace gs
