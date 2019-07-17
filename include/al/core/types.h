#pragma once

#include <iostream> // for print_
#include <memory>
#include <numeric>
#include <type_traits>
#include <vector>

#include <pybind11/numpy.h>
#include <pybind11/pytypes.h>

namespace py = pybind11;

namespace al {

// ---------------------------------- types ----------------------------------

enum class Color : int { Invalid, Indexed, Monochrome, RGB, ARGB };
enum class DType : int { Invalid, UInt8, UInt16, UInt32, Float };
enum class Bitstream : int { RAW, LZW, JPEG, JPEG2000 };
enum class Interpolation : int { Nearest, Linear };

using Level = uint16_t;
using Shape = std::array<size_t, 3>;

struct Box {
    size_t min_[2];
    size_t max_[2];
    Level level = 1;

    constexpr inline size_t shape(size_t dim) const noexcept {
        return max_[dim] - min_[dim];
    }
};

using Buffer = py::buffer;

template <typename T>
using Array = py::array_t<T, py::array::c_style | py::array::forcecast>;

// -------------------------------- functions --------------------------------

size_t to_samples(Color ctype) noexcept;

constexpr size_t bytes(DType dtype) noexcept;

template <typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
constexpr T ceil(T num, T factor) noexcept;
template <typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
constexpr T floor(T num, T factor) noexcept;

template <typename T>
constexpr DType to_dtype() noexcept;

template <template <typename> typename Tp, typename... Args>
constexpr auto from_dtype(DType dtype, Args&&... args);

template <
    typename Container,
    typename = std::enable_if_t<std::is_rvalue_reference_v<Container&&>>>
auto to_array(Container&& cont, const Shape& shape);

template <typename T>
auto to_array(std::shared_ptr<const Array<T>> sptr);

// -------------------------- function definitions --------------------------

template <typename T>
constexpr DType to_dtype() noexcept {
    if constexpr (std::is_same_v<T, uint8_t>)
        return DType::UInt8;
    else if constexpr (std::is_same_v<T, uint16_t>)
        return DType::UInt16;
    else if constexpr (std::is_same_v<T, uint32_t>)
        return DType::UInt32;
    else if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>)
        return DType::Float;
    else
        return DType::Invalid;
}

template <template <typename> typename Tp, typename... Args>
constexpr auto from_dtype(DType dtype, Args&&... args) {
    switch (dtype) {
    case DType::UInt8:
        return Tp<uint8_t>::visit(std::forward<Args>(args)...);
    case DType::UInt16:
        return Tp<uint16_t>::visit(std::forward<Args>(args)...);
    case DType::UInt32:
        return Tp<uint32_t>::visit(std::forward<Args>(args)...);
    case DType::Float:
        return Tp<float>::visit(std::forward<Args>(args)...);
    default:
        throw py::type_error{"Unknown data type"};
    }
}

namespace {
template <typename T>
struct sizeof_visitor {
    static constexpr size_t visit() noexcept { return sizeof(T); }
};
}

constexpr size_t bytes(DType dtype) noexcept {
    return from_dtype<sizeof_visitor>(dtype);
}

template <typename T, typename>
constexpr T floor(T num, T factor) noexcept { return num - num % factor; }

template <typename T, typename>
constexpr T ceil(T num, T factor) noexcept {
    num += factor - 1;
    return num - num % factor;
}

template <typename Container, typename>
auto to_array(Container&& cont, const Shape& shape) {
    if (std::accumulate(
            shape.begin(), shape.end(), size_t{1}, std::multiplies())
        != std::size(cont))
        throw std::runtime_error{"Size of container doesn't match shape"};

    auto ptr = new auto(std::move(cont));
    return Array<typename Container::value_type>(
        shape, ptr->data(), py::capsule(ptr, [](void* p) {
            delete reinterpret_cast<decltype(ptr)>(p);
        }));
}

template <typename T>
auto to_array(std::shared_ptr<const Array<T>> sptr) {
    auto info = sptr->request();
    return Array<T>(
        info.shape,
        info.ptr,
        py::capsule(new auto(std::move(sptr)), [](void* p) {
            delete reinterpret_cast<decltype(sptr)>(p);
        }));
}

template <typename T, size_t N, size_t... Is>
auto as_tuple(const std::array<T, N>& array, std::index_sequence<Is...>) {
    return std::make_tuple(array[Is]...);
}

template <typename T, size_t N>
auto as_tuple(const std::array<T, N>& array) {
    return as_tuple(array, std::make_index_sequence<N>{});
}

} // namespace al

template <typename Head, typename... Args>
inline void print_(Head&& head, Args&& ...args) {
    std::cout << std::forward<Head>(head) << " ";
    print_(std::forward<Args>(args)...);
}

inline void print_() {
    std::cout << "\n";
}
