#include "image.h"
#include <cmath>

namespace gs {

Image::Image(const std::string& imagepath) : ImageBase{}, file_path_ {imagepath}
{}

size_t Image::z_planes() const {
    return z_plane_index_;
}

void Image::z_plane_index(size_t index) {
    std::unique_lock lk(open_close_mutex_);
    z_plane_index_ = std::min(index, z_planes_ - 1);
}

size_t Image::z_plane_index() const {
  return z_plane_index_;
}

size_t Image::levels_count() const {
    return levels_count_;
}

const std::string& Image::file_type() const {
    return file_type_;
}

const std::vector<size_t>& Image::dims(size_t level) const {
    if (level >= levels_count())
        return {};
    return level_dims_[level];
}

double Image::scale(size_t level) const {
    if (level >= levels_count())
        return -1;
    return static_cast<float>(level_dims_[0][0]) / level_dims_[level][0];
}

size_t Image::find_level_for(double scale) const {
    if (scale < 1.0)
        return 0;

    for (int i = 1; i < level_dims_.size(); ++i) {
        double current = (double)level_dims_[0][0] / (double)level_dims_[i][0];
        double previous = (double)level_dims_[0][0] / (double)level_dims_[i - 1][0];
        if (scale < current) {
            if (std::abs(current - scale) > std::abs(previous - scale))
                return i - 1;
            else
                return i;
            return i - 1;
        }
    }
    return levels_count_ - 1;
}

Image::~Image() {
    std::unique_lock lk{open_close_mutex_};
    level_dims_.clear();
    spacing_.clear();
}

size_t Image::cache_capacity() {
    std::unique_lock lk{cache_mutex_};
    if (!cache_)
        return 0;

    switch (data_type_) {
    case enums::Data::UInt32:
        return (std::static_pointer_cast<TileCache<uint32_t>>(cache_))->capacity();
    case enums::Data::UInt16:
        return (std::static_pointer_cast<TileCache<uint16_t>>(cache_))->capacity();
    case enums::Data::UInt8:
        return (std::static_pointer_cast<TileCache<uint8_t>>(cache_))->capacity();
    case enums::Data::Float:
        return (std::static_pointer_cast<TileCache<float>>(cache_))->capacity();
    default:
        break;
    }
    return 0;
}

void Image::cache_capacity(size_t capacity) {
    std::unique_lock lk{cache_mutex_};
    if (!cache_)
        return;

    switch (data_type_) {
    case enums::Data::UInt32:
        (std::static_pointer_cast<TileCache<uint32_t>>(cache_))->resize(capacity);
        break;
    case enums::Data::UInt16:
        (std::static_pointer_cast<TileCache<uint16_t>>(cache_))->resize(capacity);
        break;
    case enums::Data::UInt8:
        (std::static_pointer_cast<TileCache<uint8_t>>(cache_))->resize(capacity);
        break;
    case enums::Data::Float:
        (std::static_pointer_cast<TileCache<float>>(cache_))->resize(capacity);
        break;
    default:
        break;
    }
}

} // namespace gs0
