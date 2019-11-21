#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

namespace ts {

using Path = std::filesystem::path;

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
    static std::unique_ptr<Base> make(std::string const& filename) {
        Path path{filename};
        if (auto it = data().find(path.extension().string());
                it != data().end())
            return it->second(path);
        throw std::runtime_error{"Unsupported extension"};
    }

    template <class Derived>
    class Register : public Base {
        static bool register_this() {
            static_assert(std::is_base_of_v<Register<Derived>, Derived>,
                          "Unregistered!!");
            constexpr auto func = [](Path const& path) -> std::unique_ptr<Base> {
                return std::make_unique<Derived>(path);
            };
            for (const auto& ext : Derived::extensions)
                Factory::data()[ext] = func;
            return true;
        }

        inline static bool is_registered = register_this();

    public:
        Register() { (void)is_registered; }
    };
};

} // namespace ts
