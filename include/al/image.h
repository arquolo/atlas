#include <map>

#include "al/core/factory.h"
#include "al/core/types.h"

namespace al {

using DType = std::variant<uint8_t, uint16_t, uint32_t, float>;

struct LevelInfo {
    Shape shape;
    Shape tile_shape;
};

class _Image : public Factory<_Image> {
public:
    DType dtype;
    size_t samples = 0;
    std::map<Level, LevelInfo> levels = {};

    size_t get_scale(const LevelInfo& info) const noexcept;
    std::vector<size_t> scales() const noexcept;

    std::pair<Level, size_t> get_level(size_t scale) const noexcept;

    virtual py::buffer read(const Box& box) const = 0;
};

template <class Impl>
class Image : public _Image::Register<Impl> {
    auto* derived() const noexcept { return static_cast<const Impl*>(this); }

public:
    py::buffer read(const Box& box) const final {
        return std::visit(
            [this, &box](auto v) -> py::buffer {
                return this->derived()->template _read<decltype(v)>(box);
            },
            this->dtype);
    }

    template <typename T>
    Array<T> _read(const Box& box) const;
};

} // namespace al
