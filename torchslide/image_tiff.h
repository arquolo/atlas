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

namespace ts::tiff {

// ------------------------------ declarations ------------------------------

class File {
    std::unique_ptr<TIFF, void (*)(TIFF*)> ptr_;

public:
    File(const Path& path, const std::string& flags);

    /// for compatibility
    inline operator TIFF*() noexcept { return ptr_.get(); }
    inline operator TIFF*() const noexcept { return ptr_.get(); }

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

    const File file_;
    const uint16_t codec_ = 0;
    mutable std::mutex mutex_;

    template <typename T>
    Array<T> read_at(Level level, uint32_t iy, uint32_t ix) const;

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
Array<T> TiffImage::read_at(Level level, uint32_t iy, uint32_t ix) const {
    const auto& tshape = this->levels.at(level).tile_shape;

    if (this->codec_ == 33005) // Aperio SVS
        return ts::to_array(
            [&, this]() {
                py::gil_scoped_release no_gil;
                std::unique_lock lk{this->mutex_};

                TIFFSetDirectory(this->file_, level);
                std::vector<uint8_t> buffer(TIFFTileSize(this->file_));
                buffer.resize(TIFFReadRawTile(
                    this->file_,
                    this->file_.position(iy, ix),
                    buffer.data(),
                    buffer.size()
                ));
                return io::jp2k::decode(buffer);
            }(),
            this->levels.at(level).tile_shape
        );

    if (this->samples != 4) {
        auto tile = Array<T>(tshape);
        auto info = tile.request();
        {
            py::gil_scoped_release no_gil;
            std::unique_lock lk{this->mutex_};

            TIFFSetDirectory(this->file_, level);
            TIFFReadTile(this->file_, info.ptr, ix, iy, 0, 0);
        }
        return tile;
    }

    Size shape[] = {tshape[0], tshape[1], tshape[2]};
    Size strides[] = {-tshape[2] * tshape[1], tshape[2], 1};
    py::array_t<T> tile{shape, strides};

    auto info = tile.request();
    {
        py::gil_scoped_release no_gil;
        std::unique_lock lk{this->mutex_};

        TIFFSetDirectory(this->file_, level);
        TIFFReadRGBATile(this->file_, ix, iy, (uint32*)info.ptr);  // bgra
    }
    return tile;
}

template <typename T>
Array<T> TiffImage::read(const Box& box) const {
    Array<T> result{{box.shape(0), box.shape(1), this->samples}};

    const auto& shape = this->levels.at(box.level).shape;
    auto crop = box.fit_to(shape);
    if (!crop.area())
        return result;

    const auto& tshape = this->levels.at(box.level).tile_shape;
    Size min_[] = {
        floor(crop.min_[0], tshape[0]),
        floor(crop.min_[1], tshape[1]),
    };
    Size max_[] = {
        ceil(crop.max_[0], tshape[0]),
        ceil(crop.max_[1], tshape[1]),
    };

    for (auto iy = min_[0]; iy < max_[0]; iy += tshape[0]) {
        for (auto ix = min_[1]; ix < max_[1]; ix += tshape[1]) {
            Array<T> tile = this->read_at<T>(
                box.level,
                static_cast<uint32_t>(iy),
                static_cast<uint32_t>(ix)
            );

            auto t = tile.unchecked<3>();
            auto ty_begin = std::max(box.min_[0], iy);
            auto tx_begin = std::max(box.min_[1], ix);
            auto ty_end = std::min({box.max_[0], iy + tshape[0], shape[0]});
            auto tx_end = std::min({box.max_[1], ix + tshape[1], shape[1]});

            auto out = result.mutable_unchecked<3>();
            auto out_y = ty_begin - box.min_[0];
            auto out_x = tx_begin - box.min_[1];

            for (auto ty = ty_begin; ty < ty_end; ++ty, ++out_y)
                std::copy(
                    &t(ty - iy, tx_begin - ix, 0),
                    &t(ty - iy, tx_end - ix, 0),
                    &out(out_y, out_x, 0)
                );
        }
    }
    return result;
}

} // namespace ts::tiff
