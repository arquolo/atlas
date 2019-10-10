#pragma once

// #include <iostream>
#include <memory>
#include <numeric>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

#include <pybind11/numpy.h>
#include <pybind11/pytypes.h>

namespace py = pybind11;

namespace ts {

// ---------------------------------- types ----------------------------------

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

template <typename T>
using Array = py::array_t<T, py::array::c_style | py::array::forcecast>;

// -------------------------------- functions --------------------------------

template <typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
constexpr T ceil(T num, T factor) noexcept;
template <typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
constexpr T floor(T num, T factor) noexcept;

template <
    typename Container,
    typename = std::enable_if_t<std::is_rvalue_reference_v<Container&&>>>
auto to_array(Container&& cont, const Shape& shape);

template <typename T>
auto to_array(std::shared_ptr<const Array<T>> sptr);

template <typename T, size_t N>
auto as_tuple(const std::array<T, N>& array);

template <typename Pred, typename... Ts>
auto make_variant_if(Pred pred, std::variant<Ts...> var) -> std::optional<decltype(var)>;

// -------------------------- function definitions --------------------------

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

namespace {
template <typename T, size_t N, size_t... Is>
auto _as_tuple(const std::array<T, N>& array, std::index_sequence<Is...>) {
    return std::make_tuple(array[Is]...);
}
} // namespace

template <typename T, size_t N>
auto as_tuple(const std::array<T, N>& array) {
    return _as_tuple(array, std::make_index_sequence<N>{});
}

template <typename Pred, typename... Ts>
auto make_variant_if(Pred pred, std::variant<Ts...> var) -> std::optional<decltype(var)> {
    if ((... || (pred(Ts{}) && ((var = Ts{}), true))))
        return std::move(var);
    return std::nullopt;
}

} // namespace ts

// template<typename ...Args>
// void print_(Args&&... args) {
//     std::cout << __FUNCSIG__ << '\n';
//     (std::cout << ... << std::forward<Args>(args)) << '\n';
// }
