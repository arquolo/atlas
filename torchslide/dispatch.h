#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include "tensor.h"
#include "image.h"

namespace py = pybind11;
namespace ts {

template <class Impl>
class Dispatch : public Image::Register<Impl> {
    auto* derived() const noexcept { return static_cast<Impl const*>(this); }

public:
    /// Virtual method to read tile of erased type.
    /// Gets access to implementation via `derived()`, then calls `read<T>` using T from `dtype`.
    /// So full stack is:
    ///   `Image::read_any` -[vtable]-> `Dispatch::read_any -> derived()::read<[dtype::visit]>`
    virtual py::buffer read_any(Box const& box) const final {
        py::gil_scoped_release no_gil;
        return std::visit(
            [this, &box](auto v) {
                return as_buffer(
                    this->derived()->template read<decltype(v)>(box)
                );
            },
            this->dtype);
    }

    template <typename T>
    Tensor<T> read(Box const& box) const;
};

template <typename T>
py::buffer as_buffer(Tensor<T>&& t) noexcept {
    auto ptr = new auto(t.storage());

    py::gil_scoped_acquire with_gil;
    return py::array_t<T, py::array::c_style | py::array::forcecast>{
        std::move(t.shape()),
        ptr->data(),
        py::capsule(ptr, [](void* p) {
            delete reinterpret_cast<decltype(ptr)>(p);
        }),
    };
}

template <typename T>
py::buffer as_buffer(Tensor<T> const&) noexcept {
    py::gil_scoped_acquire with_gil;
    return py::array_t<T, py::array::c_style | py::array::forcecast>{
        t.shape(), t.data()};
}

} // namespace ts
