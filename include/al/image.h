#include "al/core/factory.h"
#include "al/core/types.h"

namespace al {

struct LevelInfo {
    Shape shape;
    Shape tile_shape;
};

class _Image : public Factory<_Image> {
    virtual Buffer _read(const Box& box) const = 0;

protected:
    size_t samples_ = 0;
    std::vector<LevelInfo> levels_ = {};
    DType dtype_ = DType::Invalid;

public:
    _Image(_Key);  // for factory

    Shape const& shape(Level level = 0) const;

    size_t get_scale(const LevelInfo& info) const;
    std::vector<size_t> scales() const;

    std::pair<Level, size_t> get_level(size_t scale) const;

    DType dtype() const noexcept;
    Buffer read(std::tuple<py::slice, py::slice> slices) const;
};

template <class Impl>
class Image : public _Image::Register<Impl, Image<Impl>> {
    auto* derived() const noexcept { return static_cast<const Impl*>(this); }
    auto* derived() noexcept { return static_cast<Impl*>(this); }

    template <typename T>
    struct _read_visitor {
        static Buffer visit(const Impl* ptr, const Box& box) {
            return ptr->template _read_typed<T>(box);
        }
    };

    Buffer _read(const Box& box) const final {
        return from_dtype<_read_visitor>(this->dtype(), derived(), box);
    }

public:
    template <typename T>
    Array<T> _read_typed(const Box& box) const;
};

} // namespace al
