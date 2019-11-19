#include "image_tiff.h"

namespace ts::tiff {

auto tiff_open(const Path& path, const std::string& flags) {
#ifdef _WIN32
    auto open_fn = TIFFOpenW;
#else
    auto open_fn = TIFFOpen;
#endif
    TIFFSetErrorHandler(nullptr);
    auto ptr = open_fn(path.c_str(), flags.c_str());
    if (ptr)
        return std::unique_ptr<TIFF, void (*)(TIFF*)>{ptr, TIFFClose};
    throw std::runtime_error{"Failed to open: " + path.generic_string()};
}

File::File(const Path& path, const std::string& flags)
  : _ptr{tiff_open(path, flags)} {}

uint32_t File::position(uint32_t iy, uint32_t ix) const noexcept {
    return TIFFComputeTile(*this, ix, iy, 0, 0);
}

uint32_t File::tiles() const noexcept { return TIFFNumberOfTiles(*this); }

DType _get_dtype(const File& f) {
    auto dtype = f.try_get<uint16_t>(TIFFTAG_SAMPLEFORMAT).value_or(SAMPLEFORMAT_UINT);
    if (dtype != SAMPLEFORMAT_UINT && dtype != SAMPLEFORMAT_IEEEFP)
        throw std::runtime_error{"Unsupported data type"};

    auto bitdepth = f.get<uint16_t>(TIFFTAG_BITSPERSAMPLE);
    const auto is_compatible = [dtype, bitdepth](auto v) -> bool {
        if (bitdepth != sizeof(v) * 8)
            return false;
        if constexpr(std::is_unsigned_v<decltype(v)>)
            return dtype == SAMPLEFORMAT_UINT;
        else
            return dtype == SAMPLEFORMAT_IEEEFP;
    };
    auto opt = make_variant_if(is_compatible, DType{});
    if (opt)
        return opt.value();
    throw std::runtime_error{"Unsupported bitdepth " + std::to_string(bitdepth)};
}

Size _get_samples(const File& f) {
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

auto _read_pyramid(const File& f, Size samples) {
    TIFFSetDirectory(f, 0);
    Level level_count = TIFFNumberOfDirectories(f);
    if (level_count < 1)
        throw std::runtime_error{"Tiff have no levels"};

    // TODO: make std::map<Scale, std::pair<Level, LevelInfo>>
    std::map<Level, LevelInfo> levels;
    for (Level level = 0; level < level_count; ++level) {
        TIFFSetDirectory(f, level);
        if (!TIFFIsTiled(f))
            continue;
        levels[level] = LevelInfo{{f.get<uint32_t>(TIFFTAG_IMAGELENGTH),
                                   f.get<uint32_t>(TIFFTAG_IMAGEWIDTH),
                                   samples},
                                  {f.get<uint32_t>(TIFFTAG_TILELENGTH),
                                   f.get<uint32_t>(TIFFTAG_TILEWIDTH),
                                   samples}};
    }
    TIFFSetDirectory(f, 0);
    return levels;
}

TiffImage::TiffImage(const Path& path)
  : _file{path, "rm"}
  , _codec{_file.get<uint16_t>(TIFFTAG_COMPRESSION)}
{
    auto c_descr = _file.get_defaulted<const char*>(TIFFTAG_IMAGEDESCRIPTION);
    if (c_descr) {
        std::string descr{c_descr.value()};
        if (descr.find("DICOM") != std::string::npos
                || descr.find("xml") != std::string::npos
                || descr.find("XML") != std::string::npos)
            throw std::runtime_error{"Unsupported format: " + descr};
    }
    if (!TIFFIsTiled(_file))
        throw std::runtime_error{"Tiff is not tiled"};
    if (_file.get<uint16_t>(TIFFTAG_PLANARCONFIG) != PLANARCONFIG_CONTIG)
        throw std::runtime_error{"Tiff is not contiguous"};

    samples = _get_samples(_file);
    levels = _read_pyramid(_file, samples);
    dtype = _get_dtype(_file);
}

} // namespace ts::tiff
