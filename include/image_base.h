#pragma once

#include <string>
#include <vector>

#include "enums.h"

namespace gs {

class ImageBase {
protected:
    // Properties of an image
    std::vector<double> spacing_ = {};
    size_t samples_per_pixel_ = 0;
    enums::Color color_type_ = enums::Color::Invalid;
    enums::Data data_type_ = enums::Data::Invalid;

public:
    ImageBase() = default;

    //! Gets the dimensions of the base level of the pyramid
    /* virtual */
    const std::vector<size_t>& dims() const {} // = 0;

    //! Returns the color type
    enums::Color color() const;

    //! Returns the data type
    enums::Data dtype() const;

    //! Returns the number of samples per pixel
    size_t samples_per_pixel() const;

    //! Returns the pixel spacing meta-data (um per pixel)
    std::vector<double> spacing() const;

    //! Gets the minimum value for a channel. If no channel is specified, default to overall minimum
    /* virtual */
    double min_(int channel = -1) {} // = 0;

    //! Gets the maximum value for a channel. If no channel is specified, default to overall maximum
    /* virtual */
    double max_(int channel = -1) {} // = 0;

};

} // namespace gs
