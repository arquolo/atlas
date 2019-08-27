#pragma once

#include "core/types.h"

namespace ts::io::jp2k {

std::vector<uint8_t> _decode(const std::vector<uint8_t>& buf);

template <typename T>
Array<T> decode(const std::vector<uint8_t>& buf, Shape shape) {
    return ts::to_array(_decode(buf), shape);
}

std::vector<uint8_t> encode(py::buffer data, size_t rate);

} // namespace ts::io::jp2k
