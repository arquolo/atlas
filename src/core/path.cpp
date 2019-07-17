#include "al/core/path.h"

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

} // namespace al
