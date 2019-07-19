#include "openslide.h"

#include "al/image_openslide.h"

#include <sstream>

namespace al {

namespace {

auto open(Path const& path) {
    ptr = openslide_open(path.c_str());

    if (!openslide_detect_vendor(path.c_str()))
        throw std::runtime_error{"Cant open without vendor"};

    if (const char* error = openslide_get_error(ptr))
        throw std::runtime_error{error};

    return ptr;
}

} // namespace

File::File(Path const& path) : ptr_{open(path), openslide_close} {}

OpenSlide::OpenSlide(Path const& path) : Codec<OpenSlide>{}, file_{path} {
    levels_ = openslide_get_level_count(file_);

    for (size_t i = 0; i < levels_; ++i) {
        int64_t y;
        int64_t x;
        openslide_get_level_dimensions(file_, i, &x, &y);
        level_dims_.emplace_back(
            static_cast<size_t>(y), static_cast<size_t>(x));
    }

    // std::stringstream ssm;
    // ssm << openslide_get_property_value(file_, OPENSLIDE_PROPERTY_NAME_MPP_Y);
    // if (ssm) {
    //     float tmp;
    //     ssm >> tmp;
    //     spacing_.push_back(tmp);
    //     ssm.clear();
    // }

    // ssm << openslide_get_property_value(slide_, OPENSLIDE_PROPERTY_NAME_MPP_X);
    // if (ssm) {
    //     float tmp;
    //     ssm >> tmp;
    //     spacing_.push_back(tmp);
    //     ssm.clear();
    // }

    // Get background color if present
    const char* bg_color_hex
        = openslide_get_property_value(slide_, "openslide.background-color");
    if (bg_color_hex) {
        uint32_t bg_color = std::stoi(bg_color_hex, 0, 16);
        bg_r_ = 0xFF & (bg_color >> 16);
        bg_g_ = 0xFF & (bg_color >> 8);
        bg_b_ = 0xFF & bg_color;
    }
}

// We are using OpenSlides caching system instead of our own.
void OpenSlide::cache_capacity(size_t capacity) {
#ifdef CUSTOM_OPENSLIDE
    if (slide_)
        openslide_set_cache_size(slide_, capacity);
#endif
}

std::string OpenSlide::get(const std::string& name) const {
    std::string value;
    if (value = openslide_get_property_value(file_, name.c_str()))
        return value;
    return {};
}

template <>
Array<uint8_t> OpenSlide::_read(const Box& box) {
    Array<uint8_t> result{{box.shape(0), box.shape(1), 4}};
    auto info = result.request();
    openslide_read_region(
        slide_, reinterpret_cast<uint32_t*>(info.ptr),
        x, y, level, box.width, box.height);

    auto r = result.mutable_unchecked<3>();

    for (size_t y = 0; y < box.height; ++y)
        for (size_t x = 0; x < box.width; ++x)
            const auto alpha = r(y, x, 3);
            if (alpha == 255) {
                r(y, x, 0) = r(y, x, 2);
                r(y, x, 1) = r(y, x, 1);
                r(y, x, 2) = r(y, x, 0);
            } else if (alpha == 0) {
                r(y, x, 0) = bg_r_;
                r(y, x, 1) = bg_g_;
                r(y, x, 2) = bg_b_;
            } else {
                r(y, x, 0) = 255.f * r(y, x, 2) / alpha;
                r(y, x, 1) = 255.f * r(y, x, 1) / alpha;
                r(y, x, 2) = 255.f * r(y, x, 0) / alpha;
            }
        }
    return result;
}

} // namespace al
