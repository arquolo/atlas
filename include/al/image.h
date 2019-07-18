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
public:
    size_t samples = 0;
    std::vector<LevelInfo> levels = {};
    DType dtype = DType::Invalid;

    size_t get_scale(const LevelInfo& info) const noexcept;
    std::vector<size_t> scales() const noexcept;

    std::pair<Level, size_t> get_level(size_t scale) const noexcept;

    Buffer read(std::tuple<py::slice, py::slice> slices) const;
};

template <class Impl>
class Image : public _Image::Register<Impl> {
    auto* derived() const noexcept { return static_cast<const Impl*>(this); }
    auto* derived() noexcept { return static_cast<Impl*>(this); }

    template <typename T>
    struct _read_visitor {
        static Buffer visit(const Impl* ptr, const Box& box) {
            return ptr->template _read_typed<T>(box);
        }
    };

    Buffer _read(const Box& box) const final {
        return from_dtype<_read_visitor>(this->dtype, derived(), box);
    }

public:
    template <typename T>
    Array<T> _read_typed(const Box& box) const;
};

} // namespace al
