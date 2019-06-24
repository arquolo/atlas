#include "utility.h"

namespace al {

bool ends_with(
    const Path& path,
    const std::initializer_list<std::string>& extensions
) noexcept {
    if (!extensions.size())
        return true;
    for (const auto& ext: extensions)
        if (ext == path.extension())
            return true;
    return false;
}

size_t to_samples(Color ctype) noexcept {
    switch (ctype) {
    case Color::Indexed:
    case Color::Monochrome:
        return 1;
    case Color::RGB:
        return 3;
    case Color::ARGB:
        return 4;
    default:
        return 0;
    }
}

} // namespace al
