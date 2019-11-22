#pragma once

#include <cassert>
#include <numeric>
#include <utility>

#include "core/view.h"
#include "core/std.h"

namespace ts {

namespace _detail {

Size _to_size(ShapeAny const& shape) noexcept;

template <size_t N>
Shape_<N> _to_strides(ShapeAny const& shape) noexcept {
    assert(shape->size() == N);
    Shape_<N> strides;
    if constexpr (N != 0)
        strides.back() = 1;
    std::partial_sum(
        shape->rbegin(), shape->rend() - 1, strides.rbegin() + 1,
        std::multiplies{}
    );
    return strides;
}

} // namespace _detail

template <typename T>
class Tensor {
    using _Storage = std::vector<T>;

    ShapeAny _shape;
    _Storage _data = {};

public:
    Tensor(ShapeAny shape) : _shape{std::move(shape)} {
        _data.resize(_detail::_to_size(_shape));
    }
    Tensor(ShapeAny shape, _Storage data)
        : _shape{std::move(shape)}, _data{std::move(data)}
    {}

    auto const& shape() const& noexcept { return this->_shape; }
    auto&& shape() && noexcept { return this->_shape; }

    auto const& storage() const& noexcept { return this->_data; }
    auto&& storage() && noexcept { return this->_data; }

    T const* data() const noexcept { return this->_data.data(); }
    T* data() noexcept { return this->_data.data(); }

    template <size_t N>
    _View<T const, N> view() const& noexcept {
        return {
            this->data(),
            to_array<N>(*this->_shape),
            _detail::_to_strides<N>(this->_shape)
        };
    }

    template <size_t N>
    _View<T, N> view() & noexcept {
        return {
            this->data(),
            to_array<N>(*this->_shape),
            _detail::_to_strides<N>(this->_shape)
        };
    }
};

} // namespace ts
