#pragma once

#include <memory>

#include "al/image.h"

struct _openslide;
using openslide_t = _openslide;

namespace al {

class File final {
    std::unique_ptr<openslide_t, void (*)(openslide_t*)> ptr_;

public:
    File(Path const& path);
    operator openslide_t*() const noexcept { return ptr_.get(); }
    operator openslide_t*() noexcept { return ptr_.get(); }
};

class OpenSlide final : public Image<OpenSlide> {
    File file_;

    uint8_t bg_r_ = 255;
    uint8_t bg_g_ = 255;
    uint8_t bg_b_ = 255;

    Data dtype_ = Data::UInt8;
    Color ctype_ = Color::RGB;
    size_t samples_ = 3;

    std::string get(const std::string& name) const;
    void cache_capacity(size_t capacity);

public:
    static inline constexpr const char* extensions[] = {
        ".bif", ".ndpi", ".mrxs", ".scn", ".svs", ".svslide", ".tif", ".tiff", ".vms", ".vmu"};

    OpenSlide(const Path& path);

    template <typename T>
    Array<T> _read(const Box& box) const {
        throw std::runtime_error{"Not implemented"};
    }

    template <>
    Array<uint8_t> _read(const Box& box) const;
};

} // namespace al
