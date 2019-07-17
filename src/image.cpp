// #include <pybind11/pybind11.h>

#include "al/image.h"

namespace al {

_Image::_Image(_Key) {}

DType _Image::dtype() const noexcept { return dtype_; }

Shape const& _Image::shape(Level level) const {
    if (level < levels_.size())
        return levels_[level].shape;
    throw py::index_error{"Not enough levels"};
}

size_t _Image::get_scale(const LevelInfo& info) const {
    return static_cast<size_t>(
        std::round(static_cast<double>(levels_.front().shape.front()) /
                   static_cast<double>(info.shape.front())));
}

std::vector<size_t> _Image::scales() const {
    std::vector<size_t> scales_;
    for (const auto& level_info : this->levels_)
        if (!level_info.shape.empty())
            scales_.push_back(this->get_scale(level_info));
    return scales_;
}

std::pair<Level, size_t> _Image::get_level(size_t scale) const {
    auto it = std::find_if(levels_.begin(), levels_.end(), [=](const LevelInfo& lv) {
        if (lv.shape.empty())
            return false;
        return scale < get_scale(lv);
    });
    if (it == levels_.begin())
        return {uint16_t{0}, size_t{1}};
    while (it == levels_.end() || it->shape.empty())
        --it;
    return {static_cast<uint16_t>(std::distance(levels_.begin(), it)),
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
        {(!y_max.is_none() ? y_max.cast<size_t>() / scale : shape(level)[0]),
         (!x_max.is_none() ? x_max.cast<size_t>() / scale : shape(level)[1])},
        level
    });
}

} // namespace al
