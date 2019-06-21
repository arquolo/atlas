#pragma once

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>

#include "enums.h"
#include "image_base.h"
#include "patch.h"
#include "tile_cache.h"

namespace gs {

class Image : public ImageBase {
protected:

    //! To make Image thread-safe
    std::shared_mutex open_close_mutex_ {};
    std::mutex cache_mutex_ {};
    std::shared_ptr<void> cache_ = nullptr;

    // Aditional properties of a multi-resolution image
    std::vector<std::vector<size_t>> level_dims_ = {};
    size_t levels_count_ = 0;
    size_t z_planes_ = 1;
    size_t z_plane_index_ = 0;

    // Properties of the loaded slide
    size_t cache_capacity_ = 0;
    std::string file_type_ = {};
    std::string file_path_;

    // Reads the actual data from the image
    template <typename T>
    /* virtual */
    std::vector<T> read_data_from_image(
        size_t y, size_t x, size_t height, size_t width, size_t level); // = 0;

    template <typename T>
    void create_cache() {
        cache_.reset(std::make_shared<TileCache>(cache_capacity_));
    }

public :
    Image(const std::string& imagepath);
    /* virtual */
    ~Image();

    //! Support for slides with multiple z-planes
    size_t z_planes() const;
    void z_plane_index(size_t index);
    size_t z_plane_index() const;

    //! Get a stored data property (e.g. objective magnification")
    /* virtual */
    std::string property(const std::string& name) { return std::string{}; };

    //! Gets/Sets the maximum size of the cache
    /* virtual */
    size_t cache_capacity();
    /* virtual */
    void cache_capacity(size_t capacity);

    //! Gets the number of levels in the slide pyramid
    /* virtual */
    size_t levels_count() const;

    //! Gets the dimensions of the level 0
    //! Gets the dimensions of the specified level of the pyramid
    /* virtual */
    const std::vector<size_t>& dims(size_t level = 0) const;

    //! Get the downsampling factor of the given level relative to the base level
    /* virtual */
    double scale(size_t level) const;

    //! Gets the level corresponding to the closest downsample factor given the requested downsample factor
    /* virtual */
    size_t find_level_for(double scale) const;

    //! Gets the minimum value for a channel. If no channel is specified, default to the first channel
    /* virtual */
    double min_(int channel = -1); // = 0;

    //! Gets the maximum value for a channel. If no channel is specified, default to the first channel
    /* virtual */
    double max_(int channel = -1); // = 0;

    //! Get the file type of the opened image
    const std::string& file_type() const;

    //! Obtains data as a patch,
    //! which is a basic image class containing all relevant information for further processing,
    //! like data and colortype
    template <typename T>
    Patch<T> patch(
        size_t y, size_t x, size_t height, size_t width, size_t level
    ) {
        std::vector<size_t> dims = {height, width, samples_per_pixel_};

        auto data = get_raw_region<T>(y, x, height, width, level);

        std::vector<double> patch_spacing(spacing_.size(), 1.0);
        double scale = this->scale(level);
        for (size_t i = 0; i < spacing_.size(); ++i)
            patch_spacing[i] = spacing_[i] * scale;

        std::vector<double> min_vals;
        std::vector<double> max_vals;
        for (size_t i = 0; i < this->samples_per_pixel(); ++i) {
            min_vals.push_back(this->min_(i));
            max_vals.push_back(this->max_(i));
        }
        Patch<T> patch = Patch<T>{
            std::move(data),
            dims,
            this->color(),
            min_vals,
            max_vals
        };
        patch.spacing(patch_spacing);
        return patch;
    }

    //! Obtains pixel data for a requested region.
    //! Please note that in case of int32 ARGB data, like in OpenSlide,
    //! the order of the colors depends on the endianness of your machine
    //! (Windows typically BGRA)
    template <typename T>
    std::vector<T> get_raw_region(
        size_t y, size_t x, size_t height, size_t width, size_t level = 0
    ) {
        if (level >= levels_count())
            return {};

        switch (dtype()) {
        case enums::Data::Float:
            return read<T, float>(y, x, height, width, level);
        case enums::Data::UInt8:
            return read<T, uint8_t>(y, x, height, width, level);
        case enums::Data::UInt16:
            return read<T, uint16_t>(y, x, height, width, level);
        case enums::Data::UInt32:
            return read<T, uint32_t>(y, x, height, width, level);
        default:
            return {};
        }
    }

    template <typename T, typename Ts>
    std::vector<T> read(
        size_t y, size_t x, size_t height, size_t width, size_t level
    ) {
        auto buf = read_data_from_image<Ts>(y, x, height, width, level);
        if constexpr (std::is_same_v<T, Ts>)
            return buf;
        else
            return std::vector<T>(buf.begin(), buf.end());
    }

};

template <>
std::vector<uint8_t> Image::get_raw_region(
    size_t y, size_t x, size_t height, size_t width, size_t level);

template <>
std::vector<uint16_t> Image::get_raw_region(
    size_t y, size_t x, size_t height, size_t width, size_t level);

template <>
std::vector<uint32_t> Image::get_raw_region(
    size_t y, size_t x, size_t height, size_t width, size_t level);

template <>
std::vector<float> Image::get_raw_region(
    size_t y, size_t x, size_t height, size_t width, size_t level);

} // namespace gs
