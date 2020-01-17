#pragma once

#include <array>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

#include <pybind11/detail/common.h>

namespace ts {

using Level = uint16_t;
using Size = int64_t;

template <size_t N>
using Shape_ = std::array<Size, N>;

using Shape = Shape_<3>;
using ShapeAny = pybind11::detail::any_container<Size>;

using DType = std::variant<uint8_t, uint16_t, uint32_t, float>;

// ------------------------------- some math -------------------------------

template <typename T>
constexpr T floor(T num, T factor) noexcept { return num - num % factor; }

template <typename T>
constexpr T ceil(T num, T factor) noexcept {
    num += factor - 1;
    return num - num % factor;
}

// ------------------------- container conversions -------------------------

template <size_t N, typename T>
std::array<T, N> to_array(std::vector<T> const& vec) noexcept {
    assert(vec.size() >= N);
    std::array<T, N> arr;
    std::copy_n(vec.data(), N, arr.data());
    return arr;
}

template <size_t N, typename T>
std::vector<T> to_vector(std::array<T, N> const& arr) noexcept {
    return std::vector<T>(arr.begin(), arr.end());
}

namespace {
template <typename T, size_t N, size_t... Is>
auto _as_tuple(std::array<T, N> const& array, std::index_sequence<Is...>) {
    return std::make_tuple(array[Is]...);
}
} // namespace

template <typename T, size_t N>
auto as_tuple(std::array<T, N> const& array) {
    return _as_tuple(array, std::make_index_sequence<N>{});
}

template <typename Pred, typename... Ts>
auto make_variant_if(Pred pred, std::variant<Ts...> var) -> std::optional<decltype(var)> {
    if ((... || (pred(Ts{}) && ((var = Ts{}), true))))
        return std::move(var);
    return std::nullopt;
}

} // namespace ts
