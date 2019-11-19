#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <vector>

#include <tiffio.h>

#include "core/path.h"
#include "image.h"
#include "io/jpeg2000.h"
#include "tensor.h"

namespace ts::tiff {

// ------------------------------ declarations ------------------------------

class File {
    std::unique_ptr<TIFF, void (*)(TIFF*)> _ptr;

public:
    File(const Path& path, const std::string& flags);

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

class TiffImage final : public Image<TiffImage> {
    const File _file;
    const uint16_t _codec = 0;
    mutable std::mutex _mutex;

    template <typename T>
    Tensor<T> _read_at(Level level, uint32_t iy, uint32_t ix) const;

public:
    static inline constexpr const char* extensions[] = {".svs", ".tif", ".tiff"};

    TiffImage(const Path& path);

    template <typename T>
    Array<T> read(const Box& box) const;
};

// ------------------------------ definitions ------------------------------

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
    const auto& shape = this->levels.at(level).tile_shape;

    if (this->_codec == 33005) {  // Aperio SVS
        auto buffer = [&, this]() {
            std::unique_lock lk{this->_mutex};
            TIFFSetDirectory(this->_file, level);
            std::vector<uint8_t> buf(TIFFTileSize(this->_file));
            buf.resize(TIFFReadRawTile(
                this->_file,
                this->_file.position(iy, ix),
                buf.data(),
                buf.size()
            ));
            return buf;
        }();

        if constexpr(std::is_same_v<std::decay_t<T>, uint8_t>)
            return Tensor<T>{shape, io::jp2k::decode(buffer)};
        else {
            auto tile = Tensor<T>{shape};
            auto data = io::jp2k::decode(buffer);
            std::copy(data.begin(), data.end(), tile.data());
            return tile;
        }
    }

    auto tile = Tensor<T>{shape};
    {
        std::unique_lock lk{this->_mutex};
        TIFFSetDirectory(this->_file, level);
        if (this->samples != 4)
            TIFFReadTile(this->_file, tile.data(), ix, iy, 0, 0);
        else
            // tile._strides = {-tshape[2] * tshape[1], tshape[2], 1};
            // bgra
            TIFFReadRGBATile(this->_file, ix, iy, (uint32*)tile.data());
    }
    return tile;
}

template <typename T>
Array<T> TiffImage::read(const Box& box) const {
    py::gil_scoped_release no_gil;
    Tensor<T> result{{box.shape(0), box.shape(1), this->samples}};

    const auto& shape = this->levels.at(box.level).shape;
    auto crop = box.fit_to(shape);
    if (!crop.area())
        return std::move(result);

    const auto& tshape = this->levels.at(box.level).tile_shape;
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

            auto t = tile.view<3>();
            auto ty_begin = std::max(box.min_[0], iy);
            auto tx_begin = std::max(box.min_[1], ix);
            auto ty_end = std::min({box.max_[0], iy + tshape[0], shape[0]});
            auto tx_end = std::min({box.max_[1], ix + tshape[1], shape[1]});

            auto out = result.view<3>();
            auto out_y = ty_begin - box.min_[0];
            auto out_x = tx_begin - box.min_[1];

            for (auto ty = ty_begin; ty < ty_end; ++ty, ++out_y)
                std::copy(
                    &t({ty - iy, tx_begin - ix}),
                    &t({ty - iy, tx_end - ix}),
                    &out({out_y, out_x})
                );
        }
    return std::move(result);
}

} // namespace ts::tiff
