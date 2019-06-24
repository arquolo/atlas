#include "codec.h"

namespace al {

Shape const& _Codec::shape(uint16_t level) const {
    if (level >= levels.size())
        throw py::index_error{"Not enough levels"};
    return levels[level].shape;
}

size_t _Codec::get_scale(const Level& level) const {
    return static_cast<size_t>(
        std::round(static_cast<double>(levels.front().shape.front()) /
                   static_cast<double>(level.shape.front())));
}

std::pair<uint16_t, size_t> _Codec::level_for(size_t scale) const {
    const auto& shape_0 = levels.front().shape;
    auto it = std::find_if(levels.begin(), levels.end(), [=](const Level& lv) {
        return scale < get_scale(lv);
    });
    if (it == levels.begin())
        return {0, 1};
    --it;
    return {static_cast<uint16_t>(std::distance(levels.begin(), it)),
            get_scale(*it)};
}

} // namespace al
