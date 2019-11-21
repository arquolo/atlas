#pragma once

#include "core/std.h"

namespace ts::jp2k {

std::vector<uint8_t> decode(std::vector<uint8_t> const& buf);

// std::vector<uint8_t> encode(py::buffer data, size_t rate);

} // namespace ts::jp2k
