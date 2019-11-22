#pragma once

#include <string_view>

namespace ts {

template <typename T>
constexpr std::string_view type_name() {
    #if defined(__clang__) || defined(__GNUC__)
        constexpr std::string_view fn = __PRETTY_FUNCTION__;
    #elif defined(_MSC_VER)
        constexpr std::string_view fn = __FUNCSIG__;
    #endif
    constexpr auto _begin = fn.find("type_name<") + 10;
    constexpr auto _end = fn.find(">(", _begin);
    return fn.substr(_begin, _end - _begin);
}

} // namespace ts

// template<typename ...Args>
// void print_(Args&&... args) {
//     std::cout << __FUNCSIG__ << '\n';
//     (std::cout << ... << std::forward<Args>(args)) << '\n';
// }
