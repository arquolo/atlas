#pragma once

#define EXPORT

#include <vector>

class EXPORT Box {
    std::vector<size_t> start_ = {};
    std::vector<size_t> size_ = {};

public:
	Box intersection(const Box& r) const;
	bool intersects(const Box& r) const;

    Box() = default;
    Box(size_t y, size_t x, size_t height, size_t width);
    Box(size_t z, size_t y, size_t x, size_t depth, size_t height, size_t width);
    Box(const std::vector<size_t>& size);
    Box(const std::vector<size_t>& start, const std::vector<size_t>& size);

    const auto& start() const { return start_; }
    const auto& size() const { return size_; }

};
