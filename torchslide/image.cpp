#include "image.h"

namespace ts::abc {

Size Image::get_scale(const LevelInfo& info) const noexcept {
    return static_cast<Size>(
        std::round(static_cast<double>(this->levels.at(0).shape.front()) /
                   static_cast<double>(info.shape.front())));
}

std::vector<Size> Image::scales() const noexcept {
    std::vector<Size> scales_;
    for (const auto& [_, level_info] : this->levels)
        scales_.push_back(this->get_scale(level_info));
    return scales_;
}

std::pair<Level, LevelInfo> Image::get_level(Size scale) const noexcept {
    auto it = std::find_if(levels.begin(), levels.end(), [=](const auto& p) {
        return scale <= get_scale(p.second);
    });
    if (it == levels.end())
        --it;
    return *it;
}

Image::~Image() noexcept {}

} // namespace ts::abc
