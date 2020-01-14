#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <vector>

#include <tiffio.h>

#include "tensor.h"
#include "dispatch.h"

#define _TIFF_JPEG2K_YUV 33003
#define _TIFF_JPEG2K_RGB 33005

namespace ts::tiff {

// ------------------------------ declarations ------------------------------

struct File {
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

private:
    std::unique_ptr<TIFF, void (*)(TIFF*)> _ptr;
};

struct TiffImage final : Dispatch<TiffImage> {
    static inline constexpr int priority = 0;
    static inline constexpr char const* extensions[] = {".svs", ".tif", ".tiff"};

    template <class... Ts>
    TiffImage(File file, uint16_t codec, Ts&&... args) noexcept
      : Dispatch{std::forward<Ts>(args)...}
      , _file{std::move(file)}
      , _codec{codec}
    {}

    static std::unique_ptr<Image> make_this(Path const& path);

    template <typename T>
    Tensor<T> read(Box const& box) const;

private:
    File const _file;
    uint16_t const _codec = 0;
    std::mutex mutable _mutex;

    template <typename T>
    Tensor<T> _read_at(Level level, uint32_t iy, uint32_t ix) const;
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

    auto tile = Tensor<T>{shape};
    if (this->samples == 4) {
        Tensor<T> buf{shape};
        TIFFReadRGBATile(this->_file, ix, iy, (uint32*)buf.data());
        auto b = buf.template view<3>();
        auto t = tile.template view<3>();
        for (Size y = 0; y < shape[0]; ++y)
            std::copy(&b({y}), &b({y + 1}), &t({shape[0] - y - 1}));
    } else
        TIFFReadTile(this->_file, tile.data(), ix, iy, 0, 0);
    return tile;
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

std::unique_ptr<Image> TiffImage::make_this(Path const& path) {
    auto file = File{path, "rm"};
    auto codec = file.get<uint16_t>(TIFFTAG_COMPRESSION);

    if (codec == _TIFF_JPEG2K_YUV || codec == _TIFF_JPEG2K_RGB)
        throw std::runtime_error{"JPEG2000 encoded tile is not yet supported"};

    auto c_descr = file.get_defaulted<char const*>(TIFFTAG_IMAGEDESCRIPTION);
    if (c_descr) {
        std::string descr{c_descr.value()};
        if (descr.find("DICOM") != std::string::npos
                || descr.find("xml") != std::string::npos
                || descr.find("XML") != std::string::npos)
            throw std::runtime_error{"Unsupported format: " + descr};
    }
    if (!TIFFIsTiled(file))
        throw std::runtime_error{"Tiff is not tiled"};
    if (file.get<uint16_t>(TIFFTAG_PLANARCONFIG) != PLANARCONFIG_CONTIG)
        throw std::runtime_error{"Tiff is not contiguous"};

    auto dtype = _get_dtype(file);
    auto samples = _get_samples(file);
    auto levels = _read_pyramid(file, samples);
    return std::make_unique<TiffImage>(
        std::move(file), codec,
        std::move(dtype), std::move(samples), std::move(levels)
    );
}

} // namespace ts::tiff
