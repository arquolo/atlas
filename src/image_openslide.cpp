#include "openslide.h"

#include "codecs/openslide.h"

#include <sstream>

namespace al::codec::openslide {

static const std::vector<std::string> extensions = {
    "svs", "tif", "tiff", "mrxs", "vms", "vmu", "ndpi", "scn",  "svslide", "bif"
};

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

OpenSlide::OpenSlide(Path const& path)
  : Codec<OpenSlide>{}
  , file_{path}
{
    levels_ = openslide_get_level_count(file_);

    for (size_t i = 0; i < levels_; ++i) {
        int64_t y;
        int64_t x;
        openslide_get_level_dimensions(file_, i, &x, &y);
        level_dims_.emplace_back(static_cast<size_t>(y), static_cast<size_t>(x));
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
    const char* bg_color_hex = openslide_get_property_value(slide_, "openslide.background-color");
    if (bg_color_hex) {
        unsigned int bg_color = std::stoi(bg_color_hex, 0, 16);
        bg_r_ = ((bg_color >> 16) & 0xff);
        bg_g_ = ((bg_color >> 8) & 0xff);
        bg_b_ = (bg_color & 0xff);
    }
}

// We are using OpenSlides caching system instead of our own.
void OpenSlide::cache_capacity(size_t capacity) {
#ifdef CUSTOM_OPENSLIDE
    if (slide_)
        openslide_set_cache_size(slide_, capacity);
#endif
}

std::string OpenSlide::get(std::string const& name) const {
    std::string value;
    if (value = openslide_get_property_value(file_, name.c_str()))
        return value;
    return {};
}

Array<uint8_t> OpenSlide::_read(Box const& box) {
    std::vector<uint8_t> bgra(height * width * 4);
    openslide_read_region(slide_, reinterpret_cast<uint32_t*>(bgra.data()),
                          x, y, level, width, height);

    std::vector<uint8_t> rgb(height * width * 3);
    for (size_t i = 0, j = 0; i < height * width * 4; i += 4, j += 3) {
        if (bgra[i + 3] == 255) {
            rgb[j] = bgra[i + 2];
            rgb[j + 1] = bgra[i + 1];
            rgb[j + 2] = bgra[i];
        } else if (bgra[i + 3] == 0) {
            rgb[j] = bg_r_;
            rgb[j + 1] = bg_g_;
            rgb[j + 2] = bg_b_;
        } else {
            rgb[j] = (255. * bgra[i + 2]) / bgra[i + 3];
            rgb[j + 1] = (255. * bgra[i + 1]) / bgra[i + 3];
            rgb[j + 2] = (255. * bgra[i]) / bgra[i + 3];
        }
  }
  return rgb;
}

} // namespace gs
