#pragma once

#include "core/std.h"

namespace ts {

struct Box {
    Size min_[2];
    Size max_[2];
    Level level = 1;

    constexpr Size shape(size_t dim) const noexcept {
        return static_cast<Size>(std::max(max_[dim] - min_[dim], Size{}));
    }
    constexpr Size area() const noexcept { return shape(0) * shape(1); }

    constexpr Box fit_to(Shape const& shape) const noexcept {
        return {
            {
                std::clamp(min_[0], Size{}, shape[0]),
                std::clamp(min_[1], Size{}, shape[1])
            }, {
                std::clamp(max_[0], Size{}, shape[0]),
                std::clamp(max_[1], Size{}, shape[1])
            },
            level,
        };
    };
};

} // namespace ts
