#pragma once

#include <string_view>

namespace ts {

// ---------------------------------- types ----------------------------------

// -------------------------------- functions --------------------------------

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
