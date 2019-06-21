#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "enums.h"
#include "image.h"

struct tiff;
using TIFF = tiff;

class JPEG2000Codec;

//! This class can be used to write images to disk in a multi-resolution pyramid fashion.
//! It supports writing the image in parts, to facilitate processing pipelines or in one go,
//! in the first setting one should first open the file (openFile), then write the image
//! information (writeImageInformation), write the base parts (writeBaseParts) and then finish
//! the pyramid (finishImage). The class also contains a convenience function (writeImage),
//! which writes an entire Image to disk using the image properties (color, data)
//! and the specified codec.

namespace gs {

class ImageWriter {
protected:
    size_t tile_size_ = 512;
    float quality_ = 30;  //! JPEG compression quality

    enums::Codec codec_ = enums::Codec::LZW;
    enums::Interpolation interpolation_ = enums::Interpolation::Linear;
    enums::Data dtype_ = enums::Data::Invalid;
    enums::Color ctype_ = enums::Color::Invalid;
    size_t cdepth_ = 1;
    size_t bits_ = 8;

    //! Pixel spacing
    //! (normally taken from original image, but can be overwritten or provided when not specified)
    std::vector<double> override_spacing_ = {};

    //! Min and max values of the image that is written to disk
    std::vector<double> min_vals_ = {};
    std::vector<double> max_vals_ = {};

    //! Positions in currently opened file
    size_t pos_ = 0;

    //! Currently opened file path
    std::string filename_;

    //! Temporary storage for the levelFiles
    std::vector<std::string> level_files_ = {};
    TIFF* tiff_ = nullptr;
    std::unique_ptr<JPEG2000Codec> jpeg2000_codec_ = nullptr;

    void base_tags(TIFF* level_tiff);
    void pyramid_tags(TIFF* level_tiff, size_t height, size_t width);
    void temp_pyramid_tags(TIFF* level_tiff, size_t height, size_t width);

    template <typename T>
    void write_pyramid_level(
        TIFF* level_tiff, size_t level_height, size_t level_width,
        size_t samples);

    template <typename T>
    T* downscale_tile(T* tile, size_t tile_size, size_t samples);

    template <typename T>
    void write_pyramid_to_disk();

    template <typename T>
    void incorporate_pyramid();

    void c_put(void* data, size_t pos);

    template <typename T>
    void c_find_limits(T* data);

public:
    ImageWriter(const std::string& filename);
    virtual ~ImageWriter();

    const std::string& filename() const { return filename_; }

    //! Writes the image information like image size, tile size, color and data types
    void size(size_t size_y, size_t size_x);

    //! Write image functions for different data types.
    //! This function provides functionality to write parts of the input image,
    //! so it does not have to be loaded in memory fully,
    //! can be useful for testing or large processing pipelines.
    void append(void* data);
    void put(void* data, size_t y, size_t x);

    //! Will close the base image and finish writing the image pyramid and
    //! optionally the thumbnail image.
    //! Subsequently the image will be closed.
    virtual void finish();

    //! Get/set the compression
    void compression(enums::Codec codec) { codec_ = codec; }
    enums::Codec compression() const { return codec_; }

    //! Get/set the interpolation
    void interpolation(enums::Interpolation inter) { interpolation_ = inter; }
    enums::Interpolation interpolation() const { return interpolation_; }

    //! Get/Set the datatype
    void dtype(enums::Data dtype) {
        dtype_ = dtype;
        bits_ = [this]() -> size_t {
            switch (dtype_) {
            case enums::Data::UInt32:
            case enums::Data::Float:
                return 32;
            case enums::Data::UInt16:
                return 16;
            default:
                return 8;
            }
        }();
    }
    enums::Data dtype() const { return dtype_; }

    //! Sets the color type
    void color(enums::Color ctype) {
        ctype_ = ctype;
        cdepth_ = [this]() -> size_t {
            switch (ctype_) {
            case enums::Color::Indexed:
                return cdepth_;
            case enums::Color::ARGB:
                return 4;
            case enums::Color::RGB:
                return 3;
            default:
                return 1;
            }
        }();
    }
    void indexed_colors(size_t count) { cdepth_ = count; }

    //! Get/set the compression
    void tile_size(size_t tile_size) { tile_size_ = tile_size; }
    size_t tile_size() const { return tile_size_; }

    //! Set spacing
    void spacing(std::vector<double>& spacing);

    //! Get/set the override spacing
    void override_spacing(std::vector<double> const& spacing) { override_spacing_ = spacing; }
    const std::vector<double>& override_spacing() const { return override_spacing_; }

    //! Get/set the jpeg quality
    void jpeg_quality(float quality) {
        if ((quality <= 0) || (quality > 100))
            throw std::runtime_error{"JPEG quality should be in (0..100] range"};
        quality_ = quality;
    }
    float jpeg_quality() const { return quality_; }

    void write_image(std::shared_ptr<Image> image);

};

} // namespace gs
