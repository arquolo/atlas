#pragma once

#include "core/std.h"

namespace ts {

template <typename T, size_t N>
struct _View {
    T* data;
    Shape_<N> const shape;
    Shape_<N> const strides;

    T const& operator()(Shape_<N> const& indexes) const noexcept {
        auto offset = Size{0};
        for (auto i = Size{}; i != N; ++i) {
            assert(indexes[i] < this->shape[i]);
            offset += indexes[i] * this->strides[i];
        }
        return this->data[offset];
    }

    T& operator()(Shape_<N> const& indexes) noexcept {
        return const_cast<T&>(std::as_const(*this)(indexes));
    }
};

} // namespace ts
