#pragma once

#include <map>

#include "core/factory.h"
#include "core/types.h"

namespace ts {

using DType = std::variant<uint8_t, uint16_t, uint32_t, float>;

struct LevelInfo {
    Shape shape;
    Shape tile_shape;
};

namespace abc {

class Image : public Factory<Image> {
public:
    DType dtype;
    Size samples = 0;
    std::map<Level, LevelInfo> levels = {};

    Size get_scale(const LevelInfo& info) const noexcept;
    std::vector<Size> scales() const noexcept;

    std::pair<Level, LevelInfo> get_level(Size scale) const noexcept;

    virtual py::buffer read_any(const Box& box) const = 0;
    virtual ~Image() noexcept;
};

} // namespace abc

template <class Impl>
class Image : public abc::Image::Register<Impl> {
    auto* derived() const noexcept { return static_cast<const Impl*>(this); }

public:
    py::buffer read_any(const Box& box) const final {
        return std::visit(
            [this, &box](auto v) -> py::buffer {
                return this->derived()->template read<decltype(v)>(box);
            },
            this->dtype);
    }

    template <typename T>
    Array<T> read(const Box& box) const;
};

} // namespace ts
