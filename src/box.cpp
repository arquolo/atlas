#include "box.h"

#include <algorithm>


Box::Box(size_t y, size_t x, size_t height, size_t width)
  : start_ {y, x}
  , size_ {height, width} {}

Box::Box(size_t z, size_t y, size_t x, size_t depth, size_t height, size_t width)
  : start_ {z, y, x}
  , size_ {depth, height, width} {}

Box::Box(const std::vector<size_t>& size)
  : start_(size.size(), 0)
  , size_ {size} {}

Box::Box(const std::vector<size_t>& start, const std::vector<size_t>& size)
  : start_ {start}
  , size_ {size} {
    if (start_.size() != size_.size()) {
        start_.clear();
        size_.clear();
    }
}

bool Box::intersects(const Box &b) const {
    if (b.size().size() != size_.size() || b.start().size() != start_.size())
        return false;

    for (auto s: size_)
        if (s <= 0)
            return false;

    for (auto s: b.size())
        if (s <= 0)
            return false;

    for (int i = 0; i < start_.size(); ++i)
        if (start_[i] + size_[i] <= b.start()[i]
                || start_[i] >= b.start()[i] + b.size()[i])
            return false;

    return true;
}

Box Box::intersection(const Box& b) const {
    if (!intersects(b))
        return {};

    std::vector<size_t> start (start_.size());
    std::vector<size_t> size (start_.size());
    for (int i = 0; i < start_.size(); ++i) {
        start[i] = std::max(start_[i], b.start()[i]);
        size_t end = std::min(start_[i] + size_[i], b.start()[i] + b.size()[i]);
        size[i] = std::max(end - start[i], size_t{});
    }
    return Box{start, size};
}
