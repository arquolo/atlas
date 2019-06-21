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

ImageWriter::ImageWriter(std::string const& filename) : filename_{filename} {
    TIFFSetWarningHandler(nullptr);
    tiff_ = TIFFOpen(filename_.c_str(), "w8");
    if (!tiff_)
        throw std::runtime_error{"Failed to open \"" + filename
                                 + "\" for writing"};
    pos_ = 0;
}

ImageWriter::~ImageWriter() {
    if (tiff_) {
        TIFFClose(tiff_);
        tiff_ = nullptr;
    }
}

void ImageWriter::spacing(std::vector<double>& spacing_) {
    if (tiff_) {
        TIFFSetField(tiff_, TIFFTAG_RESOLUTIONUNIT, RESUNIT_CENTIMETER);
        if (!spacing_.empty()) {
            double pix_per_cm_y = (1. / spacing_[0]) * 10000;
            double pix_per_cm_x = (1. / spacing_[1]) * 10000;
            TIFFSetField(tiff_, TIFFTAG_XRESOLUTION, pix_per_cm_x);
            TIFFSetField(tiff_, TIFFTAG_YRESOLUTION, pix_per_cm_y);
        }
    }
}

void ImageWriter::write_image(std::shared_ptr<Image> image) {
    color(image->color());
    if (image->color() == enums::Color::Indexed)
        indexed_colors(image->samples_per_pixel());
    dtype(image->dtype());

    auto dims = image->dims();
    auto spacing = override_spacing_.empty() ? image->spacing() : override_spacing_;

    this->spacing(spacing);
    this->size(dims[0], dims[1]);
    for (size_t y = 0; y < dims[0]; y += tile_size_)
        for (size_t x = 0; x < dims[1]; x += tile_size_)
            switch (dtype_) {
            case enums::Data::UInt32: {
                auto data = image->get_raw_region<uint32_t>(y, x, tile_size_, tile_size_);
                append(reinterpret_cast<void*>(data.data()));
                break;
            }
            case enums::Data::UInt16: {
                auto data = image->get_raw_region<uint16_t>(y, x, tile_size_, tile_size_);
                append(reinterpret_cast<void*>(data.data()));
                break;
            }
            case enums::Data::UInt8: {
                auto data = image->get_raw_region<uint8_t>(y, x, tile_size_, tile_size_);
                append(reinterpret_cast<void*>(data.data()));
                break;
            }
            default:
                break;
            }
    this->finish();
}

void ImageWriter::size(size_t size_y, size_t size_x) {
    if (tiff_)
        return;

    min_vals_.resize(cdepth_);
    max_vals_.resize(cdepth_);
    std::fill_n(min_vals_.begin(), cdepth_, std::numeric_limits<double>::max());
    std::fill_n(max_vals_.begin(), cdepth_, std::numeric_limits<double>::min());

    pyramid_tags(tiff_, size_y, size_x);
    size_t total_steps = (size_y / tile_size_) * (size_x / tile_size_);

    if (codec_ == enums::Codec::JPEG2000)
        jpeg2000_codec_ = std::make_unique<JPEG2000Codec>();
}

void ImageWriter::append(void* data) {
    c_put(data, pos_++);
}

void ImageWriter::put(void* data, size_t y, size_t x) {
    size_t pos = TIFFComputeTile(tiff_, x, y, 0, 0);
    c_put(data, pos);
}

template <typename T>
void ImageWriter::c_find_limits(T* data) {
    for (size_t i = 0; i < tile_size_ * tile_size_ * cdepth_; i += cdepth_)
        for (size_t j = 0; j < cdepth_; ++j) {
            double val = data[i + j];
            max_vals_[j] = std::max(max_vals_[j], val);
            min_vals_[j] = std::min(min_vals_[j], val);
        }
}

void ImageWriter::c_put(void* data, size_t pos) {
    size_t pixels = tile_size_ * tile_size_ * cdepth_;

    //Determine min/max of tile part
    switch (dtype_) {
    case enums::Data::UInt32:
        c_find_limits(reinterpret_cast<uint32_t*>(data));
        break;
    case enums::Data::UInt16:
        c_find_limits(reinterpret_cast<uint16_t*>(data));
        break;
    case enums::Data::UInt8:
        c_find_limits(reinterpret_cast<uint8_t*>(data));
        break;
    case enums::Data::Float:
        c_find_limits(reinterpret_cast<float*>(data));
        break;
    default:
        break;
    }

    if (codec_ == enums::Codec::JPEG2000) {
        size_t size = pixels * sizeof(uint8_t);
        if (dtype() == enums::Data::UInt32 || dtype() == enums::Data::Float)
            size = pixels * sizeof(float);
        else if (dtype() == enums::Data::UInt16)
            size = pixels * sizeof(uint16_t);

        float rate = jpeg_quality();
        jpeg2000_codec_->encode(
            reinterpret_cast<char*>(data), size, tile_size_, rate, cdepth_,
            dtype_, ctype_);
        TIFFWriteRawTile(tiff_, pos, data, size);
    } else
        switch (dtype_) {
        case enums::Data::Float:
            TIFFWriteEncodedTile(tiff_, pos, data, pixels * sizeof(float));
            break;
        case enums::Data::UInt32:
            TIFFWriteEncodedTile(tiff_, pos, data, pixels * sizeof(uint32_t));
            break;
        case enums::Data::UInt16:
            TIFFWriteEncodedTile(tiff_, pos, data, pixels * sizeof(uint16_t));
            break;
        case enums::Data::UInt8:
            TIFFWriteEncodedTile(tiff_, pos, data, pixels * sizeof(uint8_t));
            break;
        default:
            break;
        }
}

void ImageWriter::finish() {
    TIFFSetField(tiff_, TIFFTAG_PERSAMPLE, PERSAMPLE_MULTI);
    TIFFSetField(tiff_, TIFFTAG_SMINSAMPLEVALUE, min_vals_.data());
    TIFFSetField(tiff_, TIFFTAG_SMAXSAMPLEVALUE, max_vals_.data());
    /* Reset to default behavior, if needed. */
    TIFFSetField(tiff_, TIFFTAG_PERSAMPLE, PERSAMPLE_MERGED);
    if (dtype_ == enums::Data::UInt32) {
        write_pyramid_to_disk<uint32_t>();
        incorporate_pyramid<uint32_t>();
    } else if (dtype_ == enums::Data::UInt16) {
        write_pyramid_to_disk<uint16_t>();
        incorporate_pyramid<uint16_t>();
    } else if (dtype_ == enums::Data::UInt8) {
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

    TIFFClose(tiff_);
    tiff_ = nullptr;
}

template <typename T>
void ImageWriter::write_pyramid_to_disk() {

    //! First get the overall image width and height;
    unsigned long w = 0, h = 0, nrsamples = 0, nrbits = 0;
    // TIFF idiosyncracy, when setting resolution tags one uses doubles,
    // getting them requires floats
    float spacingX = 0, spacingY = 0;
    std::vector<double> spacing;
    TIFFGetField(_tiff, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(_tiff, TIFFTAG_IMAGELENGTH, &h);
    TIFFGetField(_tiff, TIFFTAG_SAMPLESPERPIXEL, &nrsamples);
    TIFFGetField(_tiff, TIFFTAG_BITSPERSAMPLE, &nrbits);
    if (TIFFGetField(_tiff, TIFFTAG_XRESOLUTION, &spacingX) == 1) {
        if (TIFFGetField(_tiff, TIFFTAG_YRESOLUTION, &spacingY) == 1) {
            spacing.push_back(1. / (spacingX / (10000.)));
            spacing.push_back(1. / (spacingY / (10000.)));
        }
    }
    // Determine the amount of pyramid levels
    unsigned int pyramidlevels = 1;
    unsigned int lowestwidth = w;
    while (lowestwidth > 1024) {
        lowestwidth /= 2;
        pyramidlevels += 1;
    }
    if (abs(1024. - lowestwidth) > abs(1024. - lowestwidth * 2)) {
        lowestwidth *= 2;
        pyramidlevels -= 1;
    }
    // Setup the image directory for the thumbnail
    unsigned int lowestheight
        = (unsigned int)(h / pow(2.0, (double)pyramidlevels));

    // Write temporary image to store previous level (LibTiff does not allow to go back and forth between
    // empty directories
#ifdef WIN32
    size_t found = _fileName.find_last_of("/\\");
#else
    size_t found = _fileName.find_last_of("/");
#endif
    string tmpPth = _fileName.substr(0, found + 1);
    string fileName = _fileName.substr(found + 1);
    size_t dotLoc = fileName.find_last_of(".");
    string baseName = fileName.substr(0, dotLoc);
    for (unsigned int level = 1; level <= pyramidlevels; ++level) {
        if (_monitor) {
            _monitor->setProgress(
                (_monitor->maximumProgress() / 2.)
                + (static_cast<float>(level)
                   / static_cast<float>(pyramidlevels))
                    * (_monitor->maximumProgress() / 4.));
        }
        TIFF* prevLevelTiff = _tiff;
        if (level != 1) {
            std::stringstream ssm;
            ssm << tmpPth << "temp" << baseName << "Level" << level - 1
                << ".tif";
            prevLevelTiff = TIFFOpen(ssm.str().c_str(), "r");
        }
        std::stringstream ssm;
        ssm << tmpPth << "temp" << baseName << "Level" << level << ".tif";
        TIFF* levelTiff = TIFFOpen(ssm.str().c_str(), "w8");
        _levelFiles.push_back(ssm.str());
        unsigned int levelw = (unsigned int)(w / pow(2., (double)level));
        unsigned int levelh = (unsigned int)(h / pow(2., (double)level));
        unsigned int prevLevelw
            = (unsigned int)(w / pow(2., (double)level - 1));
        unsigned int prevLevelh
            = (unsigned int)(h / pow(2., (double)level - 1));
        setTempPyramidTags(levelTiff, levelw, levelh);
        unsigned int nrTilesX = (unsigned int)ceil(float(levelw) / _tileSize);
        unsigned int nrTilesY = (unsigned int)ceil(float(levelh) / _tileSize);
        unsigned int levelTiles = nrTilesX * nrTilesY;
        unsigned int npixels = _tileSize * _tileSize * nrsamples;
        int rowOrg = -2, colOrg = 0;
        for (unsigned int i = 0; i < levelTiles; ++i) {
            if (i % nrTilesX == 0) {
                rowOrg += 2;
                colOrg = 0;
            }
            unsigned int xpos = _tileSize * colOrg;
            unsigned int ypos = _tileSize * rowOrg;
            T* tile1 = (T*)_TIFFmalloc(npixels * sizeof(T));
            T* tile2 = (T*)_TIFFmalloc(npixels * sizeof(T));
            T* tile3 = (T*)_TIFFmalloc(npixels * sizeof(T));
            T* tile4 = (T*)_TIFFmalloc(npixels * sizeof(T));
            T* outTile = (T*)_TIFFmalloc(npixels * sizeof(T));
            bool tile1Valid = false, tile2Valid = false, tile3Valid = false,
                 tile4Valid = false;
            unsigned int size = npixels * sizeof(T);
            if (level == 1 && (getCodec() == JPEG2000)) {
                int tileNr = TIFFComputeTile(prevLevelTiff, xpos, ypos, 0, 0);
                unsigned int outTileSize
                    = _tileSize * _tileSize * nrsamples * (nrbits / 8);
                int rawSize = TIFFReadRawTile(
                    prevLevelTiff, tileNr, tile1, outTileSize);
                if (rawSize > 0) {
                    tile1Valid = true;
                } else {
                    std::fill_n(tile1, npixels, 0);
                }
                if (xpos + _tileSize >= prevLevelw) {
                    std::fill_n(tile2, npixels, 0);
                } else {
                    tileNr = TIFFComputeTile(
                        prevLevelTiff, xpos + _tileSize, ypos, 0, 0);
                    int rawSize = TIFFReadRawTile(
                        prevLevelTiff, tileNr, tile2, outTileSize);
                    if (rawSize > 0) {
                        tile2Valid = true;
                    } else {
                        std::fill_n(tile2, npixels, 0);
                    }
                }
                if (ypos + _tileSize >= prevLevelh) {
                    std::fill_n(tile3, npixels, 0);
                } else {
                    tileNr = TIFFComputeTile(
                        prevLevelTiff, xpos, ypos + _tileSize, 0, 0);
                    int rawSize = TIFFReadRawTile(
                        prevLevelTiff, tileNr, tile3, outTileSize);
                    if (rawSize > 0) {
                        tile3Valid = true;
                    } else {
                        std::fill_n(tile3, npixels, 0);
                    }
                }
                if (xpos + _tileSize >= prevLevelw
                    || ypos + _tileSize >= prevLevelh) {
                    std::fill_n(tile4, npixels, 0);
                } else {
                    tileNr = TIFFComputeTile(
                        prevLevelTiff, xpos + _tileSize, ypos + _tileSize, 0,
                        0);
                    int rawSize = TIFFReadRawTile(
                        prevLevelTiff, tileNr, tile4, outTileSize);
                    if (rawSize > 0) {
                        tile4Valid = true;
                    } else {
                        std::fill_n(tile4, npixels, 0);
                    }
                }
            } else {
                if (TIFFReadTile(prevLevelTiff, tile1, xpos, ypos, 0, 0) < 0) {
                    std::fill_n(tile1, npixels, 0);
                } else {
                    tile1Valid = true;
                }
                if (xpos + _tileSize >= prevLevelw) {
                    std::fill_n(tile2, npixels, 0);
                } else {
                    if (TIFFReadTile(
                            prevLevelTiff, tile2, xpos + _tileSize, ypos, 0, 0)
                        < 0) {
                        std::fill_n(tile2, npixels, 0);
                    } else {
                        tile2Valid = true;
                    }
                }
                if (ypos + _tileSize >= prevLevelh) {
                    std::fill_n(tile3, npixels, 0);
                } else {
                    if (TIFFReadTile(
                            prevLevelTiff, tile3, xpos, ypos + _tileSize, 0, 0)
                        < 0) {
                        std::fill_n(tile3, npixels, 0);
                    } else {
                        tile3Valid = true;
                    }
                }
                if (xpos + _tileSize >= prevLevelw
                    || ypos + _tileSize >= prevLevelh) {
                    std::fill_n(tile4, npixels, 0);
                } else {
                    if (TIFFReadTile(
                            prevLevelTiff, tile4, xpos + _tileSize,
                            ypos + _tileSize, 0, 0)
                        < 0) {
                        std::fill_n(tile4, npixels, 0);
                    } else {
                        tile4Valid = true;
                    }
                }
            }
            if (tile1Valid || tile2Valid || tile3Valid || tile4Valid) {
                T* dsTile1 = downscaleTile(tile1, _tileSize, nrsamples);
                T* dsTile2 = downscaleTile(tile2, _tileSize, nrsamples);
                T* dsTile3 = downscaleTile(tile3, _tileSize, nrsamples);
                T* dsTile4 = downscaleTile(tile4, _tileSize, nrsamples);
                unsigned int dsSize = _tileSize / 2;
                for (unsigned int y = 0; y < _tileSize; ++y) {
                    for (unsigned int x = 0; x < _tileSize; ++x) {
                        for (unsigned int s = 0; s < nrsamples; ++s) {
                            unsigned int outIndex
                                = nrsamples * (y * _tileSize + x) + s;
                            T* usedTile = dsTile1;
                            unsigned int inIndex
                                = y * dsSize * nrsamples + x * nrsamples + s;
                            if (x >= dsSize && y < dsSize) {
                                usedTile = dsTile2;
                                inIndex = y * dsSize * nrsamples
                                    + ((x - dsSize) * nrsamples) + s;
                            } else if (x < dsSize && y >= dsSize) {
                                usedTile = dsTile3;
                                inIndex = (y - dsSize) * dsSize * nrsamples
                                    + x * nrsamples + s;
                            } else if (x >= dsSize && y >= dsSize) {
                                usedTile = dsTile4;
                                inIndex = (y - dsSize) * dsSize * nrsamples
                                    + (x - dsSize) * nrsamples + s;
                            }
                            T val = *(usedTile + inIndex);
                            *(outTile + outIndex) = val;
                        }
                    }
                }
                TIFFWriteEncodedTile(
                    levelTiff, i, outTile, npixels * sizeof(T));
                _TIFFfree(dsTile1);
                _TIFFfree(dsTile2);
                _TIFFfree(dsTile3);
                _TIFFfree(dsTile4);
            }
            _TIFFfree(tile1);
            _TIFFfree(tile2);
            _TIFFfree(tile3);
            _TIFFfree(tile4);
            _TIFFfree(outTile);
            colOrg += 2;
        }
        if (level != 1) {
            TIFFClose(prevLevelTiff);
        }
        TIFFSetField(_tiff, TIFFTAG_RESOLUTIONUNIT, RESUNIT_CENTIMETER);
        if (!spacing.empty()) {
            spacing[0] *= 2.;
            spacing[1] *= 2.;
            double pixPerCmX = (1. / spacing[0]) * 10000;
            double pixPerCmY = (1. / spacing[1]) * 10000;
            TIFFSetField(levelTiff, TIFFTAG_XRESOLUTION, pixPerCmX);
            TIFFSetField(levelTiff, TIFFTAG_YRESOLUTION, pixPerCmY);
        }
        TIFFClose(levelTiff);
    }
    //! Write base directory to disk
    TIFFWriteDirectory(_tiff);
    return 0;
}

template <typename T>
void ImageWriter::incorporate_pyramid() {
    size_t samples = 0;

    for (const auto& level_file: level_files_) {
        TIFF* level = TIFFOpen(level_file.c_str(), "rm");

        float spacing_y = 0;
        float spacing_x = 0;
        std::vector<double> spacing;
        if ((TIFFGetField(level, TIFFTAG_YRESOLUTION, &spacing_y) == 1)
            && (TIFFGetField(level, TIFFTAG_XRESOLUTION, &spacing_x) == 1))
                spacing.push_back(1. / (spacing_x / (10000.)));
                spacing.push_back(1. / (spacing_y / (10000.)));
        this->spacing(spacing);

        size_t levelh, levelw;
        TIFFGetField(level, TIFFTAG_IMAGELENGTH, &levelh);
        TIFFGetField(level, TIFFTAG_IMAGEWIDTH, &levelw);
        pyramid_tags(tiff_, levelh, levelw);
        TIFFSetField(tiff_, TIFFTAG_SUBFILETYPE, FILETYPE_REDUCEDIMAGE);
        TIFFGetField(level, TIFFTAG_SAMPLESPERPIXEL, &samples);

        write_pyramid_level<T>(level, levelh, levelw, nrsamples);

        TIFFWriteDirectory(tiff_);
        TIFFClose(level);
    }
    return 0;
}

template <>
void ImageWriter::incorporate_pyramid<uint32_t>();

void ImageWriter::base_tags(TIFF* levelTiff) {
    if (_cType == Monochrome || _cType == Indexed) {
        TIFFSetField(levelTiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    } else if (_cType == ARGB || _cType == RGB) {
        TIFFSetField(levelTiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    }

    if (_dType == UChar) {
        TIFFSetField(levelTiff, TIFFTAG_BITSPERSAMPLE, 8);
        TIFFSetField(levelTiff, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
    } else if (_dType == UInt32) {
        TIFFSetField(
            levelTiff, TIFFTAG_BITSPERSAMPLE, sizeof(unsigned int) * 8);
        TIFFSetField(levelTiff, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
    } else if (_dType == UInt16) {
        TIFFSetField(
            levelTiff, TIFFTAG_BITSPERSAMPLE, sizeof(unsigned short) * 8);
        TIFFSetField(levelTiff, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
    } else if (_dType == Float) {
        TIFFSetField(levelTiff, TIFFTAG_BITSPERSAMPLE, sizeof(float) * 8);
        TIFFSetField(levelTiff, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);
    }
    if (_cType == Monochrome) {
        TIFFSetField(levelTiff, TIFFTAG_SAMPLESPERPIXEL, 1);
    } else if (_cType == RGB) {
        TIFFSetField(levelTiff, TIFFTAG_SAMPLESPERPIXEL, 3);
    } else if (_cType == ARGB) {
        TIFFSetField(levelTiff, TIFFTAG_SAMPLESPERPIXEL, 4);
    } else if (_cType == Indexed) {
        TIFFSetField(
            levelTiff, TIFFTAG_SAMPLESPERPIXEL, _numberOfIndexedColors);
    }
    TIFFSetField(levelTiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(levelTiff, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
}

void ImageWriter::pyramid_tags(TIFF* level_tiff, size_t height, size_t width) {
    base_tags(level_tiff);

    switch (codec_) {
    case enums::Codec::LZW:
        TIFFSetField(level_tiff, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
        break;
    case enums::Codec::JPEG: {
        TIFFSetField(level_tiff, TIFFTAG_COMPRESSION, COMPRESSION_JPEG);
        TIFFSetField(level_tiff, TIFFTAG_JPEGQUALITY, static_cast<uint32_t>(quality_));
        break;
    }
    case enums::Codec::RAW:
        TIFFSetField(level_tiff, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
        break;
    case enums::Codec::JPEG2000:
        TIFFSetField(level_tiff, TIFFTAG_COMPRESSION, 33005);
        break;
    default:
        break;
    }

    TIFFSetField(level_tiff, TIFFTAG_TILEWIDTH, tile_size_);
    TIFFSetField(level_tiff, TIFFTAG_TILELENGTH, tile_size_);
    TIFFSetField(level_tiff, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(level_tiff, TIFFTAG_IMAGELENGTH, height);
}

void ImageWriter::temp_pyramid_tags(
    TIFF* level_tiff, size_t height, size_t width
) {
    base_tags(level_tiff);
    TIFFSetField(level_tiff, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
    TIFFSetField(level_tiff, TIFFTAG_TILEWIDTH, tile_size_);
    TIFFSetField(level_tiff, TIFFTAG_TILELENGTH, tile_size_);
    TIFFSetField(level_tiff, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(level_tiff, TIFFTAG_IMAGELENGTH, height);
}

template <typename T>
T* ImageWriter::downscale_tile(T* tile, size_t tile_size, size_t samples) {
    size_t out_size = tile_size / 2;
    size_t pixels = out_size * out_size * samples;
    T* out = (T*)_TIFFmalloc(out_size * out_size * samples * sizeof(T));

    size_t row = tile_size * samples;
    size_t col = samples;

    for (size_t y = 0; y < out_size; ++y)
        for (size_t x = 0; x < out_size; ++x)
            for (size_t s = 0; s < samples; ++s) {
                size_t index = (2 * y * tile_size * samples) + (2 * x * samples) + s;
                size_t out_index = (y * out_size * samples) + (x * samples) + s;
                if (interpolation_ == enums::Interpolation::Linear)
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
void ImageWriter::write_pyramid_level(
    TIFF* level_tiff,
    size_t level_height, size_t level_width, size_t samples
) {
    size_t pixels = tile_size_ * tile_size_ * samples;
    T* raster = (T*)_TIFFmalloc(pixels * sizeof(T));
    if (codec_ == enums::Codec::JPEG2000) {
        size_t depth = 8;
        size_t size = pixels * sizeof(uint8_t);
        if (dtype_ == enums::Data::UInt32 && ctype_ != enums::Color::ARGB) {
            depth = 32;
            size = pixels * sizeof(T);
        }
        for (size_t i = 0; i < TIFFNumberOfTiles(level_tiff); ++i)
            if (TIFFReadEncodedTile(level_tiff, i, raster, pixels * sizeof(T)) > 0)
                TIFFWriteRawTile(tiff_, i, raster, pixels * sizeof(T));
    } else
        for (size_t i = 0; i < TIFFNumberOfTiles(level_tiff); ++i)
            if (TIFFReadEncodedTile(level_tiff, i, raster, pixels * sizeof(T)) > 0)
                TIFFWriteEncodedTile(tiff_, i, raster, pixels * sizeof(T));
    _TIFFfree(raster);
}

} // namespace gs
