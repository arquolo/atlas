#include "tensor.h"

namespace ts {

Size _to_size(_SizedAny const& shape) noexcept {
    return std::accumulate(
        shape->begin(), shape->end(), Size{1}, std::multiplies{});
}

} // namespace ts
