#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "al/core/path.h"

namespace al {

template <class Base>
class Factory {
public:
    static std::unique_ptr<Base> make(const std::string& filename) {
        Path path{filename};
        const auto& factories = data();
        if (auto it = factories.find(path.extension().string());
                it != factories.end())
            return it->second(path);
        // throw py::type_error{"Unsupported extension"}
        throw std::runtime_error{"Unsupported extension"};
    }

    template <class Derived, class Friend>
    class Register : public Base {
        Register() : Base(_Key{}) { (void)registered; }
    public:
        friend Derived;
        friend Friend;

        static bool _register() {
            constexpr auto func = [](const Path& path) -> std::unique_ptr<Base> {
                return std::make_unique<Derived>(path);
            };
            for (const auto& ext : Derived::extensions)
                Factory::data()[ext] = func;
            return true;
        }
        static bool registered;
    };

    friend Base;
private:
    class _Key {
        _Key() {}
        template <class T, class Tf> friend class Register;
    };
    Factory() = default;

    static auto& data() {
        static std::unordered_map<
            std::string,
            std::unique_ptr<Base>(*)(const Path&)> factories;
        return factories;
    }
};

template <class Base>
template <class Derived, class Friend>
bool Factory<Base>::Register<Derived, Friend>::registered
    = Factory<Base>::Register<Derived, Friend>::_register();

} // namespace al
