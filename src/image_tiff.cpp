#include <optional>

#include "tiffio.h"

#include "al/image_tiff.h"
#include "al/io/jpeg2000.h"

// TIFFImage::TIFFImage(std::filesystem::path const& path): Image{path} {
//     TIFFSetDirectory(tiff_, 0);
//     float spacing_y;
//     float spacing_x;
//     if (TIFFGetField(tiff_, TIFFTAG_YRESOLUTION, &spacing_y) == 1)
//         spacing_.push_back(10000. / spacing_y);
//     if (TIFFGetField(tiff_, TIFFTAG_XRESOLUTION, &spacing_x) == 1)
//         spacing_.push_back(10000. / spacing_x);
// }

namespace al {
namespace detail {

DType get_dtype(const io::tiff::File& f) {
    auto bitdepth = f.get<uint16_t>(TIFFTAG_BITSPERSAMPLE);
    auto dtype = f.try_get<uint16_t>(TIFFTAG_SAMPLEFORMAT).value_or(1);

    switch (dtype) {
    case SAMPLEFORMAT_UINT:
        switch (bitdepth) {
        case 8:
            return DType::UInt8;
        case 16:
            return DType::UInt16;
        case 32:
            return DType::UInt32;
        default:
            throw std::runtime_error{"Unsupported bitdepth"
                                     + std::to_string(bitdepth)};
        }
    case SAMPLEFORMAT_IEEEFP:
        if (bitdepth == 32)
            return DType::Float;
        throw std::runtime_error{"Unsupported bitdepth" + std::to_string(bitdepth)};
    default:
        throw std::runtime_error{"Unsupported dtype"};
    }
}

size_t get_samples(const io::tiff::File& f) {
    auto ctype = f.get<uint16_t>(TIFFTAG_PHOTOMETRIC);
    switch (ctype) {
    case PHOTOMETRIC_MINISBLACK: {
        auto samples = f.get<uint16_t>(TIFFTAG_SAMPLESPERPIXEL);
        if (samples == 1)
            return samples;
        throw std::runtime_error{"Indexed color is not supported"};
    }
    case PHOTOMETRIC_RGB: {
        auto samples = f.get<uint16_t>(TIFFTAG_SAMPLESPERPIXEL);
        if (samples == 3 || samples == 4)
            return samples;
        throw std::runtime_error{"Unsupported sample count: " + std::to_string(samples)};
    }
    case PHOTOMETRIC_YCBCR:
        return 4;
    default:
        throw std::runtime_error{"Unsupported color type"};
    }
}

auto read_pyramid(const io::tiff::File& f, size_t samples) {
    TIFFSetDirectory(f, 0);
    Level level_count = TIFFNumberOfDirectories(f);
    if (level_count < 1)
        throw std::runtime_error{"Tiff have no levels"};

    std::vector<LevelInfo> levels;
    for (Level level = 0; level < level_count; ++level) {
        TIFFSetDirectory(f, level);
        if (TIFFIsTiled(f)) {
            LevelInfo lv{Shape{f.get<uint32_t>(TIFFTAG_IMAGELENGTH),
                               f.get<uint32_t>(TIFFTAG_IMAGEWIDTH),
                               samples},
                         Shape{f.get<uint32_t>(TIFFTAG_TILELENGTH),
                               f.get<uint32_t>(TIFFTAG_TILEWIDTH),
                               samples}};
            levels.emplace_back(std::move(lv));
        } else
            levels.emplace_back();
    }
    TIFFSetDirectory(f, 0);
    return levels;
}

} // namespace detail

TiffImage::TiffImage(const Path& path)
  : file_{path, "rm"}
  , codec_{file_.get<uint16_t>(TIFFTAG_COMPRESSION)}
{
    auto c_descr = file_.get_defaulted<const char*>(TIFFTAG_IMAGEDESCRIPTION);
    if (c_descr) {
        std::string descr{c_descr.value()};
        if (descr.find("DICOM") != std::string::npos
                || descr.find("xml") != std::string::npos
                || descr.find("XML") != std::string::npos)
            throw std::runtime_error{"Unsupported format: " + descr};
    }
    if (!TIFFIsTiled(file_))
        throw std::runtime_error{"Tiff is not tiled"};
    if (file_.get<uint16_t>(TIFFTAG_PLANARCONFIG) != PLANARCONFIG_CONTIG)
        throw std::runtime_error{"Tiff is not contiguous"};

    samples = detail::get_samples(file_);
    levels = detail::read_pyramid(file_, samples);
    dtype = detail::get_dtype(file_);
}

} // namespace al
