#pragma once

#include <map>

#include <pybind11/pytypes.h>

#include "core/box.h"
#include "core/factory.h"
#include "core/std.h"

namespace py = pybind11;
namespace ts {

// enum class Bitstream : int { RAW, LZW, JPEG, JPEG2000 };
// enum class Interpolation : int { Nearest, Linear };

struct ImageInfo {
    DType dtype;
    Size samples;
    std::map<Level, LevelInfo> levels;

    Size get_scale(LevelInfo const& info) const noexcept;
    std::vector<Size> scales() const noexcept;

    std::pair<Level, LevelInfo> get_level(Size scale) const noexcept;
};

struct Image : ImageInfo, Factory<Image> {
    template <class... Ts>
    Image(Ts&&... args) : ImageInfo{std::forward<Ts>(args)...} {}

    virtual py::buffer read_any(Box const& box) const = 0;
    virtual ~Image() noexcept;
};

} // namespace ts
