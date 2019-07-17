#include "al/core/types.h"

namespace al {

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
