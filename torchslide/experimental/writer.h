#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "enums.h"
#include "codecs/tiff_tools.h"
#include "image.h"

class JPEG2000Codec;

//! This class can be used to write images to disk in a multi-resolution pyramid fashion.
//! It supports writing the image in parts, to facilitate processing pipelines or in one go,
//! in the first setting one should first open the file (openFile), then write the image
//! information (writeImageInformation), write the base parts (writeBaseParts) and then finish
//! the pyramid (finishImage). The class also contains a convenience function (writeImage),
//! which writes an entire Image to disk using the image properties (color, data)
//! and the specified codec.

namespace gs {

class Writer: Image {
protected:
    size_t quality_ = 30;  // JPEG compression quality

    Codec codec_ = Codec::LZW;
    Interpolation interpolation_ = Interpolation::Linear;
    size_t samples_ = 1;
    size_t bitdepth_ = 8;

    /// Min and max values of the image that is written to disk
    std::vector<double> min_vals_ = {};
    std::vector<double> max_vals_ = {};

    /// Positions in currently opened file
    size_t pos_ = 0;

    /// Temporary storage for the levelFiles
    std::vector<std::filesystem::path> level_files_ = {};
    TiffFile tiff_ = {};

    void base_tags(TiffFile& level_tiff);
    void pyramid_tags(TiffFile& level_tiff, size_t height, size_t width);
    void temp_pyramid_tags(TiffFile& level_tiff, size_t height, size_t width);

    template <typename T>
    void write_pyramid_level(
        TiffFile& level_tiff, size_t level_height, size_t level_width,
        size_t samples);

    template <typename T>
    std::shared_ptr<T> downscale_tile(std::shared_ptr<T> tile, size_t tile_size, size_t samples);

    template <typename T>
    void write_pyramid_to_disk();

    template <typename T>
    void incorporate_pyramid();

    /// Determine min/max of tile part
    template <typename T>
    void update_limits(std::vector<T> const& tile);

    template <typename T>
    void put_impl(std::vector<T> const& tile, size_t pos);

public:
    ImageWriter(const std::string& filename);

    const std::string& filename() const { return filename_; }

    void size(size_t size_y, size_t size_x);

    template <typename T>
    void append(std::vector<T> const& data);

    template<typename T>
    void put(std::vector<T> const& data, size_t y, size_t x);

    /// Will close the base image and finish writing the image pyramid and
    /// optionally the thumbnail image.
    /// Subsequently the image will be closed.
    void close();

    /// Get/set the compression
    void compression(Codec codec) { codec_ = codec; }
    Codec compression() const { return codec_; }

    /// Get/set the interpolation
    void interpolation(Interpolation inter) { interpolation_ = inter; }
    Interpolation interpolation() const { return interpolation_; }

    /// Get/Set the datatype
    void dtype(Data dtype) { dtype_ = dtype; }

    /// Sets the color type
    void ctype(Color ctype) { ctype_ = ctype; }
    void indexed_colors(size_t count) { cdepth_ = count; }

    /// Get/set the compression
    void tile_size(size_t tile_size) { tile_size_ = tile_size; }
    size_t tile_size() const { return tile_size_; }

    /// Set spacing
    void spacing(std::vector<double>& spacing);

    /// Get/set the jpeg quality
    void jpeg_quality(size_t quality) {
        if ((quality == 0) || (quality > 100))
            throw std::runtime_error{"JPEG quality should be in [1..100] range"};
        quality_ = quality;
    }

    void write_image(const Image& image);

};

template <typename T>
void ImageWriter::update_limits(std::vector<T> const& tile) {
    for (size_t i = 0; i < tile_size_ * tile_size_ * cdepth_; i += cdepth_)
        for (size_t j = 0; j < cdepth_; ++j) {
            double val = data[i + j];
            max_vals_[j] = std::max(max_vals_[j], val);
            min_vals_[j] = std::min(min_vals_[j], val);
        }
}

template <typename T>
void ImageWriter::append(std::vector<T> const& data) {
    put_impl(data, pos_++);
}

template <typename T>
void ImageWriter::put(std::vector<T> const& data, size_t y, size_t x) {
    put_impl(data, position(y, x));
}

template <typename T>
void ImageWriter::put_impl(std::vector<T> const& tile, size_t pos) {
    size_t pixels = tile_size_ * tile_size_ * cdepth_;

    update_limits(tile);

    size_t size = pixels * gs::bytes(dtype_);
    if (codec_ == Codec::JPEG2000) {
        auto compressed = jp2k::encode(
            reinterpret_cast<char*>(data), size, tile_size_, quality_, cdepth_,
            dtype_, ctype_);
        tiff_.write_raw(pos, compressed);
        // TIFFWriteRawTile(tiff_, static_cast<uint32_t>(pos), data, size);
    } else
        tiff_.write_encoded(pos, tile);
        // TIFFWriteEncodedTile(
        //     tiff_, static_cast<uint32_t>(pos), data, size);
}

template <typename T>
void ImageWriter::write_pyramid_level(
    TiffFile& level_tiff,
    size_t level_height, size_t level_width, size_t samples
) {
    size_t pixels = tile_size_ * tile_size_ * samples;
    if (codec_ == Codec::JPEG2000)
        for (size_t pos = 0; pos < level_tiff.tiles(); ++i)
            tiff_.write_raw(pos, level_tiff.read_encoded<T>(pos, pixels));
    else
        for (size_t pos = 0; pos < level_tiff.tiles(); ++pos)
            tiff_.write_encoded(pos, level_tiff.read_encoded<T>(pos, pixels));
}

template <typename T>
std::shared_ptr<T> ImageWriter::downscale_tile(
    std::shared_ptr<T> tile, size_t tile_size, size_t samples
) {
    size_t out_size = tile_size / 2;
    size_t pixels = out_size * out_size * samples;
    auto out = gs::tiff_new<T>(out_size * out_size * samples);

    size_t row = tile_size * samples;
    size_t col = samples;

    auto out_tile = out.get();
    for (size_t y = 0; y < out_size; ++y)
        for (size_t x = 0; x < out_size; ++x)
            for (size_t s = 0; s < samples; ++s) {
                size_t index = (2 * y * tile_size * samples) + (2 * x * samples) + s;
                size_t out_index = (y * out_size * samples) + (x * samples) + s;
                if (interpolation_ == Interpolation::Linear)
                    out_tile[out_index] = static_cast<T>(
                        tile[index] / 4. +
                        tile[index + row] / 4. +
                        tile[index + col] / 4. +
                        tile[index + row + col] / 4.
                    );
                else
                    out_tile[out_index] = tile[index];
            }
    return out;
}

template <typename T>
void ImageWriter::incorporate_pyramid() {
    for (const auto& level_file: level_files_) {
        auto level = gs::tiff_open(level_file, "rm");

        float spacing_y = 0;
        float spacing_x = 0;
        if ((TIFFGetField(level.get(), TIFFTAG_YRESOLUTION, &spacing_y) == 1)
            && (TIFFGetField(level.get(), TIFFTAG_XRESOLUTION, &spacing_x) == 1))
            this->spacing({10000. / spacing_y, 10000. / spacing_x});

        size_t level_h = tiff_.field(TIFFTAG_IMAGELENGTH);
        size_t level_w = tiff_.field(TIFFTAG_IMAGEWIDTH);

        pyramid_tags(tiff_, level_h, level_w);

        tiff_.field(TIFFTAG_SUBFILETYPE, FILETYPE_REDUCEDIMAGE);
        size_t samples = tiff_.field(TIFFTAG_SAMPLESPERPIXEL);

        write_pyramid_level<T>(level, level_h, level_w, samples);
        tiff_.write_directory();
    }
    return 0;
}

} // namespace gs
