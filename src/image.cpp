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

Buffer _Image::read(std::tuple<py::slice, py::slice> slices) const {
    const auto& [ys, xs] = slices;

    auto y_min = ys.attr("start");
    auto x_min = xs.attr("start");
    auto y_max = ys.attr("stop");
    auto x_max = xs.attr("stop");
    auto y_step_ = ys.attr("step");
    auto x_step_ = xs.attr("step");

    size_t y_step = (!y_step_.is_none()) ? y_step_.cast<size_t>() : 1;
    size_t x_step = (!x_step_.is_none()) ? x_step_.cast<size_t>() : 1;
    if (y_step != x_step)
        throw std::runtime_error{"Y and X steps must be equal"};

    auto [level, scale] = get_level(y_step);

    return this->_read({
        {(!y_min.is_none() ? y_min.cast<size_t>() / scale : 0),
         (!x_min.is_none() ? x_min.cast<size_t>() / scale : 0)},
        {(!y_max.is_none() ? y_max.cast<size_t>() / scale : this->levels[level].shape[0]),
         (!x_max.is_none() ? x_max.cast<size_t>() / scale : this->levels[level].shape[1])},
        level
    });
}

} // namespace al
