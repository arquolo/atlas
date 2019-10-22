#pragma once

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

using Size = ssize_t;
using Level = uint16_t;
using Shape = std::array<ssize_t, 3>;

struct Box {
    Size min_[2];
    Size max_[2];
    Level level = 1;

    constexpr Size shape(size_t dim) const noexcept {
        return static_cast<Size>(max_[dim] - min_[dim]);
    }
    constexpr Size area() const noexcept { return shape(0) * shape(1); }
};

template <typename T>
using Array = py::array_t<T, py::array::c_style | py::array::forcecast>;

// -------------------------------- functions --------------------------------

template <typename T>
constexpr T floor(T num, T factor) noexcept { return num - num % factor; }

template <typename T>
constexpr T ceil(T num, T factor) noexcept {
    num += factor - 1;
    return num - num % factor;
}

template <
    typename Container,
    std::enable_if_t<std::is_rvalue_reference_v<Container&&>, bool> = true>
auto to_array(Container&& cont, const Shape& shape) -> Array<typename Container::value_type> {
    if (std::accumulate(
            shape.begin(), shape.end(), Size{1}, std::multiplies())
        != static_cast<Size>(std::size(cont)))
        throw std::runtime_error{"Size of container doesn't match shape"};

    auto ptr = new auto(std::move(cont));
    return Array<typename Container::value_type>(
        shape,
        ptr->data(),
        py::capsule(ptr, [](void* p) {
            delete reinterpret_cast<decltype(ptr)>(p);
        })
    );
}

template <typename T>
auto to_array(std::shared_ptr<const Array<T>> sptr) -> Array<T> {
    auto info = sptr->request();
    return {
        info.shape,
        info.ptr,
        py::capsule(new auto(std::move(sptr)), [](void* p) {
            delete reinterpret_cast<decltype(sptr)>(p);
        }),
    };
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

template <typename T>
constexpr std::string_view type_name() {
    #ifdef __clang__
        std::string_view p = __PRETTY_FUNCTION__;
        return std::string_view(p.data() + 34, p.size() - 34 - 1);
    #elif defined(__GNUC__)
        std::string_view p = __PRETTY_FUNCTION__;
        return std::string_view(p.data() + 49, p.find(';', 49) - 49);
    #elif defined(_MSC_VER)
        std::string_view p = __FUNCSIG__;
        return std::string_view(p.data() + 84, p.size() - 84 - 7);
    #endif
}

} // namespace ts

// template<typename ...Args>
// void print_(Args&&... args) {
//     std::cout << __FUNCSIG__ << '\n';
//     (std::cout << ... << std::forward<Args>(args)) << '\n';
// }
