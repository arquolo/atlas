#include "image_writer.h"
#include "image.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <math.h>
#include <sstream>


extern "C" {
#include "tiffio.h"
};

#include "enums.h"
#include "image.h"
#include "codecs/jpeg2000.h"

namespace gs {

Writer::Writer(std::string const& filename)
  : filename_{filename}
  , tiff_{filename, "w8"} {
    TIFFSetWarningHandler(nullptr);
}

void Writer::spacing(std::vector<double>& spacing_) {
    tiff_.field(TIFFTAG_RESOLUTIONUNIT, RESUNIT_CENTIMETER);
    if (!spacing_.empty()) {
        tiff_.field(TIFFTAG_YRESOLUTION, 10000. / spacing_[0]);
        tiff_.field(TIFFTAG_XRESOLUTION, 10000. / spacing_[1]);
    }
}

void ImageWriter::write_image(const Image& image) {
    ctype(image.ctype());
    if (image.ctype() == Color::Indexed)
        indexed_colors(image.samples());
    dtype(image.dtype());

    auto spacing = spacing_.empty() ? image.spacing() : spacing_;
    this->spacing(spacing);

    auto dims = image.dims();
    this->size(dims[0], dims[1]);

    for (size_t y = 0; y < dims[0]; y += tile_size_)
        for (size_t x = 0; x < dims[1]; x += tile_size_)
            switch (dtype_) {
            case Data::UInt32: {
                auto data = image.template read<uint32_t>(y, x, tile_size_, tile_size_);
                this->append(data);
                break;
            }
            case Data::UInt16: {
                auto data = image.template read<uint16_t>(y, x, tile_size_, tile_size_);
                this->append(data);
                break;
            }
            case Data::UInt8: {
                auto data = image.template read<uint8_t>(y, x, tile_size_, tile_size_);
                this->append(data);
                break;
            }
            default:
                break;
            }
    this->close();
}

void ImageWriter::size(size_t size_y, size_t size_x) {
    min_vals_ = std::vector(cdepth_, std::numeric_limits<double>::max());
    max_vals_ = std::vector(cdepth_, std::numeric_limits<double>::min());

    pyramid_tags(tiff_, size_y, size_x);
    // size_t total_steps = (size_y / tile_size_) * (size_x / tile_size_);
}

void ImageWriter::close() {
    tiff_.field(TIFFTAG_PERSAMPLE, PERSAMPLE_MULTI);
    tiff_.field(TIFFTAG_PERSAMPLE, PERSAMPLE_MULTI);
    tiff_.field(TIFFTAG_SMINSAMPLEVALUE, min_vals_.data());
    tiff_.field(TIFFTAG_SMAXSAMPLEVALUE, max_vals_.data());

    /* Reset to default behavior, if needed. */
    tiff_.field(TIFFTAG_PERSAMPLE, PERSAMPLE_MERGED);
    if (dtype_ == Data::UInt32) {
        write_pyramid_to_disk<uint32_t>();
        incorporate_pyramid<uint32_t>();
    } else if (dtype_ == Data::UInt16) {
        write_pyramid_to_disk<uint16_t>();
        incorporate_pyramid<uint16_t>();
    } else if (dtype_ == Data::UInt8) {
        write_pyramid_to_disk<uint8_t>();
        incorporate_pyramid<uint8_t>();
    } else {
        write_pyramid_to_disk<float>();
        incorporate_pyramid<float>();
    }

    for (auto& it : level_files_)
        for (size_t i = 0; i < 5; ++i)
            if (remove(it.c_str()) == 0)
                break;
}

template <typename T>
void ImageWriter::write_pyramid_to_disk() {
    //! First get the overall image width and height;

    // TIFF idiosyncracy, when setting resolution tags one uses doubles,
    // getting them requires floats
    size_t h = tiff_.field(TIFFTAG_IMAGELENGTH);
    size_t w = tiff_.field(TIFFTAG_IMAGEWIDTH);

    size_t samples = tiff_.field(TIFFTAG_SAMPLESPERPIXEL);

    size_t bits = tiff_.field(TIFFTAG_BITSPERSAMPLE);

    std::vector<double> spacing{
        10000. / tiff_.template field<float>(TIFFTAG_YRESOLUTION),
        10000. / tiff_.template field<field>(TIFFTAG_XRESOLUTION),
    };
    // Determine the amount of pyramid levels
    size_t levels = 1;
    size_t lowestwidth = w;

    while (lowestwidth > 1024) {
        lowestwidth /= 2;
        ++levels;
    }
    if (abs(1024. - lowestwidth) > abs(1024. - lowestwidth * 2)) {
        lowestwidth *= 2;
        --levels;
    }
    // Setup the image directory for the thumbnail
    size_t lowestheight = (size_t)(h / pow(2.0, (double)levels));

    // Write temporary image to store previous level (LibTiff does not allow to go back and forth between
    // empty directories
#ifdef WIN32
    size_t found = filename_.find_last_of("/\\");
#else
    size_t found = filename_.find_last_of("/");
#endif
    std::string tmppath = filename_.substr(0, found + 1);
    std::string filename = filename_.substr(found + 1);
    std::string basename = filename.substr(0, filename.find_last_of("."));
    for (size_t level = 1; level <= levels; ++level) {
        TiffFile prev_level_tiff;
        if (level == 1)
            prev_level_tiff = tiff_;
        else {
            std::stringstream ssm;
            ssm << tmppath << "temp" << basename << "Level" << level - 1
                << ".tif";
            prev_level_tiff = tiff_open(ssm.str(), "r");
        }
        std::stringstream ssm;
        ssm << tmppath << "temp" << basename << "Level" << level << ".tif";
        auto level_tiff = tiff_open(ssm.str(), "w8");
        level_files_.push_back(ssm.str());

        size_t level_h = (size_t)(h / pow(2., (double)level));
        size_t level_w = (size_t)(w / pow(2., (double)level));
        size_t prev_level_h = (size_t)(h / pow(2., (double)level - 1));
        size_t prev_level_w = (size_t)(w / pow(2., (double)level - 1));

        temp_pyramid_tags(level_tiff, level_w, level_h);
        size_t tiles_y = (size_t)ceil(float{level_h} / tile_size_);
        size_t tiles_x = (size_t)ceil(float{level_w} / tile_size_);
        size_t level_tiles = tiles_y * tiles_x;
        size_t pixels = tile_size_ * tile_size_ * samples;
        int row = -2, col = 0;
        for (size_t i = 0; i < level_tiles; ++i) {
            if (i % tiles_x == 0) {
                row += 2;
                col = 0;
            }
            size_t ypos = tile_size_ * row;
            size_t xpos = tile_size_ * col;
            auto tile00 = tiff_new<T>(pixels);
            auto tile01 = tiff_new<T>(pixels);
            auto tile10 = tiff_new<T>(pixels);
            auto tile11 = tiff_new<T>(pixels);
            auto out_tile = tiff_new<T>(pixels);
            bool valid[2][2]{};
            size_t size = pixels * sizeof(T);
            if (level == 1 && codec_ == Codec::JPEG2000) {
                size_t out_tile_size = tile_size_ * tile_size_ * samples * (bits / 8);

                if (TIFFReadRawTile(
                        prev_level_tiff.get(),
                        prev_level_tiff.position(ypos, xpos),
                        tile00.get(),
                        out_tile_size) > 0)
                    valid[0][0] = true;
                else
                    std::fill_n(tile00, pixels, 0);

                if (xpos + tile_size_ >= prev_level_w)
                    std::fill_n(tile00, pixels, 0);
                else {
                    if (TIFFReadRawTile(
                            prev_level_tiff.get(),
                            prev_level_tiff.position(ypos, xpos + tile_size_),
                            tile01.get(),
                            out_tile_size) > 0)
                        valid[0][1] = true;
                    else
                        std::fill_n(tile00, pixels, 0);
                }
                if (ypos + _tileSize >= prevLevelh)
                    std::fill_n(tile3, npixels, 0);
                else {
                    tileNr = TIFFComputeTile(
                        prevLevelTiff, xpos, ypos + _tileSize, 0, 0);
                    int rawSize = TIFFReadRawTile(
                        prevLevelTiff, tileNr, tile3, outTileSize);
                    if (rawSize > 0)
                        valid[1][0] = true;
                    else {
                        std::fill_n(tile3, npixels, 0);
                    }
                }
                if (xpos + _tileSize >= prevLevelw || ypos + _tileSize >= prevLevelh)
                    std::fill_n(tile4, npixels, 0);
                else {
                    tileNr = TIFFComputeTile(prevLevelTiff, xpos + _tileSize, ypos + _tileSize, 0, 0);
                    int rawSize = TIFFReadRawTile(prevLevelTiff, tileNr, tile4, outTileSize);
                    if (rawSize > 0)
                        valid[1][1] = true;
                    else
                        std::fill_n(tile4, npixels, 0);
                }
            } else {
                if (TIFFReadTile(prevLevelTiff, tile1, xpos, ypos, 0, 0) < 0)
                    std::fill_n(tile00, pixels, 0);
                else
                    valid[0][0] = true;

                if (xpos + _tileSize >= prevLevelw)
                    std::fill_n(tile01, pixels, 0);
                else {
                    if (TIFFReadTile(prevLevelTiff, tile2, xpos + _tileSize, ypos, 0, 0) < 0)
                        std::fill_n(tile2, npixels, 0);
                    else
                        valid[0][1] = true;
                }
                if (ypos + _tileSize >= prevLevelh)
                    std::fill_n(tile3, npixels, 0);
                else {
                    if (TIFFReadTile(prevLevelTiff, tile3, xpos, ypos + _tileSize, 0, 0) < 0)
                        std::fill_n(tile3, npixels, 0);
                    else
                        valid[1][0] = true;
                }
                if (xpos + _tileSize >= prevLevelw || ypos + _tileSize >= prevLevelh)
                    std::fill_n(tile4, npixels, 0);
                else {
                    if (TIFFReadTile(prevLevelTiff, tile4, xpos + _tileSize, ypos + _tileSize, 0, 0) < 0)
                        std::fill_n(tile4, npixels, 0);
                    else
                        valid[1][1] = true;
                }
            }
            if (valid[0][0] || valid[0][1] || valid[1][0] || valid[1][1]) {
                auto dsTile00 = downscale_tile(tile00, tile_size_, samples);
                auto dsTile01 = downscale_tile(tile01, tile_size_, samples);
                auto dsTile10 = downscale_tile(tile10, tile_size_, samples);
                auto dsTile11 = downscale_tile(tile11, tile_size_, samples);
                unsigned int dsSize = _tileSize / 2;
                for (unsigned int y = 0; y < _tileSize; ++y)
                    for (unsigned int x = 0; x < _tileSize; ++x)
                        for (unsigned int s = 0; s < nrsamples; ++s) {
                            unsigned int outIndex = nrsamples * (y * _tileSize + x) + s;
                            T* usedTile = dsTile1;
                            unsigned int inIndex = y * dsSize * nrsamples + x * nrsamples + s;
                            if (x >= dsSize && y < dsSize) {
                                usedTile = dsTile2;
                                inIndex = y * dsSize * nrsamples + ((x - dsSize) * nrsamples) + s;
                            } else if (x < dsSize && y >= dsSize) {
                                usedTile = dsTile3;
                                inIndex = (y - dsSize) * dsSize * nrsamples + x * nrsamples + s;
                            } else if (x >= dsSize && y >= dsSize) {
                                usedTile = dsTile4;
                                inIndex = (y - dsSize) * dsSize * nrsamples + (x - dsSize) * nrsamples + s;
                            }
                            T val = *(usedTile + inIndex);
                            *(outTile + outIndex) = val;
                        }
                TIFFWriteEncodedTile(level_tiff.get(), i, out_tile, pixels * sizeof(T));
            }
            col += 2;
        }
        TIFFSetField(tiff_, TIFFTAG_RESOLUTIONUNIT, RESUNIT_CENTIMETER);
        if (!spacing.empty()) {
            spacing[0] *= 2.;
            spacing[1] *= 2.;
            TIFFSetField(levelTiff, TIFFTAG_YRESOLUTION, 10000. / spacing[0]);
            TIFFSetField(levelTiff, TIFFTAG_XRESOLUTION, 10000. / spacing[1]);
        }
    }
    //! Write base directory to disk
    TIFFWriteDirectory(tiff_);
}

void ImageWriter::base_tags(TiffFile& level_tiff) {
    if (ctype_ == Color::Monochrome || ctype_ == Color::Indexed)
        level_tiff.field(TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    else if (ctype_ == Color::ARGB || ctype_ == Color::RGB)
        level_tiff.field(TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);

    if (dtype_ == Data::Float) {
        level_tiff.field(TIFFTAG_BITSPERSAMPLE, sizeof(float) * 8);
        level_tiff.field(TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);
    } else {
        level_tiff.field(TIFFTAG_BITSPERSAMPLE, gs::bytes(dtype_) * 8);
        level_tiff.field(TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
    }

    level_tiff.field(TIFFTAG_SAMPLESPERPIXEL, gs::samples(ctype_));
    level_tiff.field(TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    level_tiff.field(TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
}

void ImageWriter::pyramid_tags(TiffFile& level_tiff, size_t height, size_t width) {
    base_tags(level_tiff);

    switch (codec_) {
    case Codec::LZW:
        level_tiff.field(TIFFTAG_COMPRESSION, COMPRESSION_LZW);
        break;
    case Codec::JPEG: {
        level_tiff.field(TIFFTAG_COMPRESSION, COMPRESSION_JPEG);
        level_tiff.field(TIFFTAG_JPEGQUALITY, static_cast<uint32_t>(quality_));
        break;
    }
    case Codec::RAW:
        level_tiff.field(TIFFTAG_COMPRESSION, COMPRESSION_NONE);
        break;
    case Codec::JPEG2000:
        level_tiff.field(TIFFTAG_COMPRESSION, 33005);
        break;
    default:
        break;
    }

    level_tiff.field(TIFFTAG_TILEWIDTH, tile_size_);
    level_tiff.field(TIFFTAG_TILELENGTH, tile_size_);
    level_tiff.field(TIFFTAG_IMAGEWIDTH, width);
    level_tiff.field(TIFFTAG_IMAGELENGTH, height);
}

void ImageWriter::temp_pyramid_tags(
    TiffFile& level_tiff, size_t height, size_t width
) {
    base_tags(level_tiff);
    level_tiff.field(TIFFTAG_COMPRESSION, COMPRESSION_LZW);
    level_tiff.field(TIFFTAG_TILEWIDTH, tile_size_);
    level_tiff.field(TIFFTAG_TILELENGTH, tile_size_);
    level_tiff.field(TIFFTAG_IMAGEWIDTH, width);
    level_tiff.field(TIFFTAG_IMAGELENGTH, height);
}

} // namespace gs
