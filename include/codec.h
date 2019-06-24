#pragma once

#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

// #include "cache.h"
#include "utility.h"

namespace al {

struct Level {
    Shape shape;
    Shape tile_shape;
};

class _Codec {
public:
    std::vector<Level> levels = {};
    size_t samples = {};

    _Codec() = default;

    template <typename Container>
    _Codec(Container&& levels) : levels{std::forward<Container>(levels)} {}

    const Shape& shape(uint16_t level = 0) const;

    size_t get_scale(const Level& level) const;
    std::pair<uint16_t, size_t> level_for(size_t scale) const;

    virtual ~_Codec() = default;
    virtual DType dtype() const noexcept = 0;
    virtual Buffer read(const Box& box) const = 0;
};

template <typename Impl>
class Codec : public _Codec {
    auto* derived() const noexcept { return static_cast<const Impl*>(this); }
    auto* derived() noexcept { return static_cast<Impl*>(this); }

    template <typename T>
    struct _read_visitor {
        static Buffer visit(Impl const* ptr, const Box& box) {
            return ptr->template _read<T>(box);
        }
    };

public:
    template <typename T>
    Array<T> _read(const Box& box) const;

    Buffer read(const Box& box) const final {
        return typed<_read_visitor>(derived()->dtype(), derived(), box);
    }
};

} // namespace al
