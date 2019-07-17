#pragma once

#include "al/io/tiff.h"
#include "al/io/jpeg2000.h"
#include "al/image.h"

namespace al {

class TiffImage final : public Image<TiffImage> {
    static inline constexpr const char* extensions[] = {".svs", ".tif", ".tiff"};

    io::tiff::File file_;
    Color ctype_ = Color::Invalid;
    uint16_t codec_ = 0;

    template <typename T>
    Array<T> _read_typed_at(Level level, size_t iy, size_t ix) const;

public:
    TiffImage(const Path& path);

    template <typename T>
    Array<T> _read_typed(const Box& box) const;
};

template <typename T>
Array<T> TiffImage::_read_typed_at(Level level, size_t iy, size_t ix) const {
    TIFFSetDirectory(this->file_, level);
    const auto& tshape = this->levels_[level].tile_shape;

    if (this->codec_ == 33005) { // Aperio SVS
        std::vector<uint8_t> buffer(TIFFTileSize(this->file_));
        buffer.resize(TIFFReadRawTile(
            this->file_,
            this->file_.position(iy, ix),
            buffer.data(),
            buffer.size()
        ));
        return io::jp2k::decode<T>(buffer, this->levels_[level].tile_shape);
    }
    if (this->ctype_ != Color::ARGB) {
        auto tile = Array<T>(tshape);
        auto info = tile.request();
        TIFFReadTile(this->file_, info.ptr, ix, iy, 0, 0);
        return tile;
    }

    std::vector<ssize_t> shape{
        (ssize_t)tshape[0], (ssize_t)tshape[1], (ssize_t)tshape[2]};
    std::vector<ssize_t> strides{
        -(ssize_t)tshape[1] * (ssize_t)tshape[2],
        +(ssize_t)tshape[2],
        1};
    py::array_t<T> tile{shape, strides};

    auto info = tile.request();
    TIFFReadRGBATile(this->file_, ix, iy, (uint32*)info.ptr);  // bgra
    return tile;
}

template <typename T>
Array<T> TiffImage::_read_typed(const Box& box) const {
    const auto& tshape = this->levels_[box.level].tile_shape;
    size_t min_[] = {
        floor(box.min_[0], tshape[0]),
        floor(box.min_[1], tshape[1])
    };
    const auto& shape = this->levels_[box.level].shape;
    size_t max_[] {
        ceil(std::min(box.max_[0], shape[0]), tshape[0]),
        ceil(std::min(box.max_[1], shape[1]), tshape[1])
    };
    Array<T> result{{box.shape(0), box.shape(1), this->samples_}};

    for (auto iy = min_[0]; iy < max_[0]; iy += tshape[0]) {
        for (auto ix = min_[1]; ix < max_[1]; ix += tshape[1]) {
            Array<T> tile = this->_read_typed_at<T>(box.level, iy, ix);

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

} // namespace al
