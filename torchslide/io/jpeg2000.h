#pragma once

#include "core/types.h"

namespace ts::io::jp2k {

std::vector<uint8_t> decode(const std::vector<uint8_t>& buf);

std::vector<uint8_t> encode(py::buffer data, size_t rate);

} // namespace ts::io::jp2k
