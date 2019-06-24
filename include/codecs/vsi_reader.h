#pragma once

#include <string>
#include <vector>
#include <map>

#include "enums.h"

namespace gs {

class VSIImageReader {
public:
    VSIImageReader(const std::string& filename);
    ~VSIImageReader();

    //! Gets the dimensions of the base level of the pyramid
    const std::vector<size_t>& dims() const;

    //! Gets and sets the pixel size
    float pixel_size_x() { return pixel_size_x_; }
    float pixel_size_y() { return pixel_size_y_; }
    void pixel_size_x(float value) { pixel_size_x_ = value; }
    void pixel_size_y(float value) { pixel_size_y_ = value; }

    //! Obtains pixel data for a requested region as int8 array
    std::vector<uint8_t> read_impl(
        size_t y, size_t x, size_t height, size_t width, size_t level);

};

} // namespace gs
