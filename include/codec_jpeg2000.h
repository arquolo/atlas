#pragma once

#include <vector>

#include "utility.h"

namespace al::codec::jp2k {
namespace detail {

std::vector<uint8_t> decode(const std::vector<uint8_t>& buf);

} // namespace detail

std::vector<uint8_t> encode(Buffer data, size_t rate, Color ctype);

template <typename T>
Array<T> decode(const std::vector<uint8_t>& buf, Shape shape, size_t samples);

template <typename T>
Array<T> decode(const std::vector<uint8_t>& buf, Shape shape, size_t samples) {
    shape.push_back(samples);
    return al::to_array(detail::decode(buf), shape);
}

} // namespace al::codec::jp2k
