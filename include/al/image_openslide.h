#pragma once

#include <memory>

#include "codec.h"

struct _openslide;
using openslide_t = _openslide;

namespace al::codec::openslide {

class File final {
    std::unique_ptr<openslide_t, void (*)(openslide_t*)> ptr_;

public:
    File(Path const& path);
    operator openslide_t* () const noexcept { return ptr_.get(); }
    operator openslide_t* () noexcept { return ptr_.get(); }
};

class OpenSlide final : public Codec<OpenSlide> {
    File file_;

    uint8_t bg_r_ = 255;
    uint8_t bg_g_ = 255;
    uint8_t bg_b_ = 255;

    Data dtype_ = Data::UInt8;
    Color ctype_ = Color::RGB;
    size_t samples_ = 3;

    std::string get(std::string const& name) const;
    void cache_capacity(size_t capacity);

public:
    OpenSlide(Path const& path);

    template <typename T>
    Array<T> _read(Box const& box) const;

};

template <typename T>
Array<T> OpenSlide::_read(Box const& box) const {
    throw std::runtime_error{"Not implemented"};
}

template <>
Array<uint8_t> OpenSlide::_read(Box const& box) const;

} //namespace al::codec::openslide
