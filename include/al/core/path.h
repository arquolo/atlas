#pragma once

#include <initializer_list>
#include <filesystem>
#include <string>

namespace al {

using Path = std::filesystem::path;

bool ends_with(
    const Path& path,
    const std::initializer_list<std::string>& extension) noexcept;

} // namespace al
