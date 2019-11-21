#include "tensor.h"

namespace ts::_detail {

Size _to_size(ShapeAny const& shape) noexcept {
    return std::accumulate(
        shape->begin(), shape->end(), Size{1}, std::multiplies{});
}

} // namespace ts::_detail
