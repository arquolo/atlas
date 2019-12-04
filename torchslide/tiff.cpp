#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <vector>

#include <tiffio.h>

#include "tensor.h"
#include "dispatch.h"
#include "tiff_j2k.h"

#define _TIFF_JPEG2K 33005

namespace ts::tiff {

// ------------------------------ declarations ------------------------------

class File {
    std::unique_ptr<TIFF, void (*)(TIFF*)> _ptr;

public:
    File(Path const& path, std::string const& flags);

    /// for compatibility
    inline operator TIFF*() noexcept { return _ptr.get(); }
    inline operator TIFF*() const noexcept { return _ptr.get(); }

    uint32_t position(uint32_t iy, uint32_t ix) const noexcept;
    uint32_t tiles() const noexcept;

    template <typename T>
    T get(uint32 tag) const;

    template <typename T>
    std::optional<T> try_get(uint32 tag) const noexcept;

    template <typename T>
    std::optional<T> get_defaulted(uint32 tag) const noexcept;
};

class TiffImage final : public Dispatch<TiffImage> {
    File const _file;
    uint16_t const _codec = 0;
    std::mutex mutable _mutex;

    template <typename T>
    Tensor<T> _read_at(Level level, uint32_t iy, uint32_t ix) const;

public:
    static inline constexpr char const* extensions[] = {".svs", ".tif", ".tiff"};

    TiffImage(Path const& path);

    template <typename T>
    Tensor<T> read(Box const& box) const;
};

// -------------------------- template definitions --------------------------

template <typename T>
T File::get(uint32 tag) const {
    T value = {};
    TIFFGetField(*this, tag, &value);
    return value;
}

template <typename T>
std::optional<T> File::try_get(uint32 tag) const noexcept {
    T value = {};
    if (TIFFGetField(*this, tag, &value))
        return value;
    return {};
}

template <typename T>
std::optional<T> File::get_defaulted(uint32 tag) const noexcept {
    T value = {};
    if (TIFFGetFieldDefaulted(*this, tag, &value))
        return value;
    return {};
}

template <typename T>
Tensor<T> TiffImage::_read_at(Level level, uint32_t iy, uint32_t ix) const {
    auto const& shape = this->levels.at(level).tile_shape;

    std::unique_lock lk{this->_mutex};
    TIFFSetDirectory(this->_file, level);

    // not Aperio SVS
    // if (this->_codec != _TIFF_JPEG2K) {
        auto tile = Tensor<T>{shape};
        if (this->samples == 4)
            // tile._strides = {-tshape[2] * tshape[1], tshape[2], 1};
            TIFFReadRGBATile(this->_file, ix, iy, (uint32*)tile.data());
        else
            TIFFReadTile(this->_file, tile.data(), ix, iy, 0, 0);
        return tile;
    // }

    // Aperio SVS
    /*
    std::vector<uint8_t> buffer;
    buffer.resize(TIFFTileSize(this->_file));

    auto real_size = TIFFReadRawTile(
        this->_file,
        this->_file.position(iy, ix),
        buffer.data(),
        buffer.size());
    buffer.resize(real_size);

    if constexpr (std::is_same_v<std::decay_t<T>, uint8_t>)
        return Tensor<T>{shape, jp2k::decode(buffer)};
    else {
        auto tile = Tensor<T>{shape};
        auto data = jp2k::decode(buffer);
        std::copy(data.begin(), data.end(), tile.data());
        return tile;
    }
    */
}

template <typename T>
Tensor<T> TiffImage::read(Box const& box) const {
    Tensor<T> result{{box.shape(0), box.shape(1), this->samples}};

    auto const& shape = this->levels.at(box.level).shape;
    auto crop = box.fit_to(shape);
    if (!crop.area())
        return result;

    auto const& tshape = this->levels.at(box.level).tile_shape;
    Size min_[] = {
        floor(crop.min_[0], tshape[0]),
        floor(crop.min_[1], tshape[1]),
    };
    Size max_[] = {
        ceil(crop.max_[0], tshape[0]),
        ceil(crop.max_[1], tshape[1]),
    };

    for (auto iy = min_[0]; iy < max_[0]; iy += tshape[0])
        for (auto ix = min_[1]; ix < max_[1]; ix += tshape[1]) {
            auto const tile = this->_read_at<T>(
                box.level,
                static_cast<uint32_t>(iy),
                static_cast<uint32_t>(ix)
            );

            auto t = tile.template view<3>();
            auto ty_begin = std::max(box.min_[0], iy);
            auto tx_begin = std::max(box.min_[1], ix);
            auto ty_end = std::min({box.max_[0], iy + tshape[0], shape[0]});
            auto tx_end = std::min({box.max_[1], ix + tshape[1], shape[1]});

            auto out = result.template view<3>();
            auto out_y = ty_begin - box.min_[0];
            auto out_x = tx_begin - box.min_[1];

            for (auto ty = ty_begin; ty < ty_end; ++ty, ++out_y)
                std::copy(
                    &t({ty - iy, tx_begin - ix}),
                    &t({ty - iy, tx_end - ix}),
                    &out({out_y, out_x})
                );
        }
    return result;
}

// ------------------------ non-template definitions ------------------------

auto tiff_open(Path const& path, std::string const& flags) {
    TIFFSetErrorHandler(nullptr);
#ifdef _WIN32
    auto ptr = TIFFOpenW(path.c_str(), flags.c_str());
#else
    auto ptr = TIFFOpen(path.c_str(), flags.c_str());
#endif
    if (ptr)
        return std::unique_ptr<TIFF, void (*)(TIFF*)>{ptr, TIFFClose};
    throw std::runtime_error{"Failed to open: " + path.generic_string()};
}

File::File(Path const& path, std::string const& flags) : _ptr{tiff_open(path, flags)} {}

uint32_t File::position(uint32_t iy, uint32_t ix) const noexcept {
    return TIFFComputeTile(*this, ix, iy, 0, 0);
}

uint32_t File::tiles() const noexcept { return TIFFNumberOfTiles(*this); }

DType _get_dtype(File const& f) {
    auto dtype = f.try_get<uint16_t>(TIFFTAG_SAMPLEFORMAT).value_or(SAMPLEFORMAT_UINT);
    if (dtype != SAMPLEFORMAT_UINT && dtype != SAMPLEFORMAT_IEEEFP)
        throw std::runtime_error{"Unsupported data type"};

    auto bitdepth = f.get<uint16_t>(TIFFTAG_BITSPERSAMPLE);
    auto const is_compatible = [dtype, bitdepth](auto v) -> bool {
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

Size _get_samples(File const& f) {
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

auto _read_pyramid(File const& f, Size samples) {
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
        levels[level] = {
            {f.get<uint32_t>(TIFFTAG_IMAGELENGTH),
             f.get<uint32_t>(TIFFTAG_IMAGEWIDTH),
             samples},
            {f.get<uint32_t>(TIFFTAG_TILELENGTH),
             f.get<uint32_t>(TIFFTAG_TILEWIDTH),
             samples}
        };
    }
    TIFFSetDirectory(f, 0);
    return levels;
}

TiffImage::TiffImage(Path const& path)
  : _file{path, "rm"}
  , _codec{_file.get<uint16_t>(TIFFTAG_COMPRESSION)}
{
    if (_codec == _TIFF_JPEG2K)
        throw std::runtime_error{"JPEG2000 encoded tile is not yet supported"};

    auto c_descr = _file.get_defaulted<char const*>(TIFFTAG_IMAGEDESCRIPTION);
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
