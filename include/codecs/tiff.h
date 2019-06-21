#pragma once

#include "image.h"

struct tiff;
using TIFF = tiff;

class JPEG2000Codec;

namespace gs {

class TIFFImage : public Image {
protected :
    TIFF* tiff_ = nullptr;
    std::vector<std::vector<unsigned int>> tile_sizes_per_level_;

    std::vector<double> min_vals_;
    std::vector<double> max_vals_;

    std::unique_ptr<JPEG2000Codec> jpeg2k_codec_ = nullptr;

    std::vector<float> read_data_from_image(
        size_t y, size_t x, size_t height, size_t width, size_t level);

    template <typename T>
    T* fill_requested_region_from_tiff(
        size_t y, size_t x, size_t height, size_t width, size_t level,
        size_t samples);

public:
    using Image::Image;
    ~TIFFImage();

    double min_(int channel = -1);
    double max_(int channel = -1);
    size_t encoded_tile_size(size_t y, size_t x, size_t level);
    uint8_t* read_encoded_data_from_image(size_t y, size_t x, size_t level);

};

} // namespace gs
