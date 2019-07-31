#include "al/image.h"

namespace al {

size_t _Image::get_scale(const LevelInfo& info) const noexcept {
    return static_cast<size_t>(
        std::round(static_cast<double>(this->levels.at(0).shape.front()) /
                   static_cast<double>(info.shape.front())));
}

std::vector<size_t> _Image::scales() const noexcept {
    std::vector<size_t> scales_;
    for (const auto& [_, level_info] : this->levels)
        scales_.push_back(this->get_scale(level_info));
    return scales_;
}

std::pair<Level, size_t> _Image::get_level(size_t scale) const noexcept {
    auto it = std::find_if(levels.begin(), levels.end(), [=](const auto& p) {
        return scale <= get_scale(p.second);
    });
    if (it == levels.end())
        --it;
    return {it->first, get_scale(it->second)};
}

} // namespace al
