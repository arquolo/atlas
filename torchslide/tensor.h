#pragma once

#include <array>
#include <algorithm>
#include <cassert>
#include <vector>
#include <utility>

#include <pybind11/numpy.h>

namespace py = pybind11;

namespace ts {

using Size = ssize_t;

template <typename T>
using PyArray = py::array_t<T, py::array::c_style | py::array::forcecast>;

template <Size N> using _Sized = std::array<Size, N>;
using _SizedAny = py::detail::any_container<Size>;

template <typename T, Size N>
struct _View {
    T* _data;
    _Sized<N> const _shape;
    _Sized<N> const _strides;

    T const& operator()(_Sized<N> indexes) const noexcept {
        auto offset = Size{0};
        for (auto i = Size{}; i != N; ++i) {
            assert(indexes[i] < this->_shape[i]);
            offset += indexes[i] * this->_strides[i];
        }
        return this->_data[offset];
    }
    T& operator()(_Sized<N> indexes) noexcept {
        return const_cast<T&>(std::as_const(*this)(std::move(indexes)));
    }
};

Size _to_size(_SizedAny const& shape) noexcept;

template <Size N>
_Sized<N> _to_strides(_SizedAny const& shape) noexcept {
    assert(shape->size() == N);
    _Sized<N> strides;
    if constexpr (N != 0)
        strides.back() = 1;
    std::partial_sum(
        shape->rbegin(), shape->rend() - 1, strides.rbegin() + 1,
        std::multiplies{}
    );
    return strides;
}

template <typename T>
class Tensor {
    using _Storage = std::vector<T>;

    _SizedAny _shape;
    _Storage _data = {};

public:
    Tensor(_SizedAny shape) : _shape{std::move(shape)} {
        _data.resize(_to_size(_shape));
    }
    Tensor(_SizedAny shape, _Storage data) : _shape{std::move(shape)}, _data{std::move(data)} {}

    T const* data() const noexcept { return this->_data.data(); }
    T* data() noexcept { return this->_data.data(); }

    template <Size N>
    _View<T const, N> view() const& noexcept {
        assert(size(this->_shape) == N);
        auto shape = _Sized<N>();
        std::copy_n(this->_shape->data(), N, shape.data());
        return {this->data(), std::move(shape), _to_strides<N>(this->_shape)};
    }
    template <Size N>
    _View<T, N> view() & noexcept {
        assert(size(this->_shape) == N);
        auto shape = _Sized<N>();
        std::copy_n(this->_shape->data(), N, shape.data());
        return {this->data(), std::move(shape), _to_strides<N>(this->_shape)};
    }

    operator PyArray<T>() && noexcept {
        auto ptr = new _Storage(this->_data);

        py::gil_scoped_acquire with_gil;
        return PyArray<T>{
            std::move(this->_shape),
            ptr->data(),
            py::capsule(ptr, [](void* p) {
                delete reinterpret_cast<decltype(ptr)>(p);
            }),
        };
    }
    operator PyArray<T>() const& noexcept {
        py::gil_scoped_acquire with_gil;
        return PyArray<T>{this->_shape, this->_data.data()};
    }
};

} // namespace ts
