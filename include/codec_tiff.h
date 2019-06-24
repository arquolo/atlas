#pragma once

#include <vector>

#include "codec.h"
#include "codec_tiff_tools.h"
#include "codec_jpeg2000.h"

namespace al::codec::tiff {

class Tiff final : public Codec<Tiff> {
    File file_;
    DType dtype_ = DType::Invalid;
    Color ctype_ = Color::Invalid;
    uint16_t codec_ = 0;

    template <typename T>
    Array<T> _read_atom(uint16_t level, size_t iy, size_t ix) const;

public:
    Tiff(Path const& path);

    DType dtype() const noexcept;
    const Shape& shape() const noexcept;

    template <typename T>
    Array<T> _read(const Box& box) const;
};

template <typename T>
Array<T> Tiff::_read_atom(uint16_t level, size_t iy, size_t ix) const {
    TIFFSetDirectory(this->file_, level);
    const auto& tshape = this->levels[level].tile_shape;

    if (this->codec_ == 33005) { // Aperio SVS
        std::vector<uint8_t> buffer(TIFFTileSize(this->file_));
        buffer.resize(TIFFReadRawTile(
            file_,
            file_.position(iy, ix),
            buffer.data(),
            buffer.size()
        ));
        return jp2k::decode<T>(buffer, levels[level].tile_shape, this->samples);
    }
    if (this->ctype_ != Color::ARGB) {
        auto tile = Array<T>({tshape[0], tshape[1], this->samples});
        auto info = tile.request();
        TIFFReadTile(this->file_, info.ptr, ix, iy, 0, 0);
        return tile;
    }

    std::vector<ssize_t> shape{
        (ssize_t)tshape[0], (ssize_t)tshape[1], (ssize_t)this->samples};
    std::vector<ssize_t> strides{
        -(ssize_t)tshape[1] * (ssize_t)this->samples, (ssize_t)this->samples, 1};
    py::array_t<T> tile{shape, strides};

    auto info = tile.request();
    TIFFReadRGBATile(this->file_, ix, iy, (uint32*)info.ptr);  // bgra
    return tile;
}

template <typename T>
Array<T> Tiff::_read(const Box& box) const {
    const auto& tshape = this->levels[box.level].tile_shape;
    size_t min_[] = {
        floor(box.min_[0], tshape[0]),
        floor(box.min_[1], tshape[1])
    };
    const auto& shape = this->levels[box.level].shape;
    size_t max_[] {
        ceil(std::min(box.max_[0], shape[0]), tshape[0]),
        ceil(std::min(box.max_[1], shape[1]), tshape[1])
    };
    Array<T> result{{box.shape(0), box.shape(1), this->samples}};

    for (auto iy = min_[0]; iy < max_[0]; iy += tshape[0]) {
        for (auto ix = min_[1]; ix < max_[1]; ix += tshape[1]) {
            Array<T> tile = this->_read_atom<T>(box.level, iy, ix);

            auto t = tile.unchecked<3>();
            auto tys = std::max(box.min_[0], iy);
            auto txs = std::max(box.min_[1], ix);
            auto tye = std::min(box.max_[0], iy + tshape[0]);
            auto txe = std::min(box.max_[1], ix + tshape[1]);

            auto r = result.mutable_unchecked<3>();
            auto ry = tys - box.min_[0];
            auto rx = txs - box.min_[1];

            for (auto ty = tys; ty < tye; ++ty, ++ry)
                std::copy(
                    &t(ty - iy, txs - ix, 0),
                    &t(ty - iy, txe - ix, 0),
                    &r(ry, rx, 0)
                );
        }
    }
    return result;
}

} // namespace al::codec::tiff
