#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "al/core/path.h"

namespace al {

template <class Base>
class Factory {
    friend Base;
    Factory() = default;

    static auto& data() {
        static std::unordered_map<
            std::string,
            std::unique_ptr<Base>(*)(const Path&)> factories;
        return factories;
    }

public:
    static std::unique_ptr<Base> make(const std::string& filename) {
        Path path{filename};
        if (auto it = data().find(path.extension().string());
                it != data().end())
            return it->second(path);
        throw std::runtime_error{"Unsupported extension"};
    }

    template <class Derived>
    class Register : public Base {
        static bool is_registered;

        static bool register_this() {
            static_assert(std::is_base_of_v<Register<Derived>, Derived>,
                          "Unregistered!!");
            constexpr auto func = [](const Path& path) -> std::unique_ptr<Base> {
                return std::make_unique<Derived>(path);
            };
            for (const auto& ext : Derived::extensions)
                Factory::data()[ext] = func;
            return true;
        }

    public:
        Register() { (void)is_registered; }
    };
};

template <class Base>
template <class Derived>
bool Factory<Base>::Register<Derived>::is_registered
    = Factory<Base>::Register<Derived>::register_this();

} // namespace al
