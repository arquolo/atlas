#include "al/image.h"

namespace al {

size_t _Image::get_scale(const LevelInfo& info) const noexcept {
    return static_cast<size_t>(
        std::round(static_cast<double>(this->levels.front().shape.front()) /
                   static_cast<double>(info.shape.front())));
}

std::vector<size_t> _Image::scales() const noexcept {
    std::vector<size_t> scales_;
    for (const auto& level_info : this->levels)
        if (!level_info.shape.empty())
            scales_.push_back(this->get_scale(level_info));
    return scales_;
}

std::pair<Level, size_t> _Image::get_level(size_t scale) const noexcept {
    auto it = std::find_if(levels.begin(), levels.end(), [=](const LevelInfo& lv) {
        if (lv.shape.empty())
            return false;
        return scale < get_scale(lv);
    });
    if (it == levels.begin())
        return {Level{0}, size_t{1}};
    while (it == levels.end() || it->shape.empty())
        --it;
    return {static_cast<Level>(std::distance(levels.begin(), it)),
            get_scale(*it)};
}

} // namespace al
