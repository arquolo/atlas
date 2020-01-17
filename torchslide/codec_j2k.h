#pragma once

#include <cstddef>
#include <vector>

#include "tensor.h"

namespace ts::j2k {

using Bitstream = std::vector<char>;
using Error = std::string;

template <class T>
using Result = std::variant<T, Error>;

struct _RType {
    Bitstream data;
    Shape shape;
    DType dtype;
};

Result <_RType> decode(Bitstream const&);

template <typename T>
Tensor<T> unwrap(_RType const& storage) {
    Tensor<T> out{storage.shape};
    std::copy_n(
        storage.data.data(),
        static_cast<Bitstream::value_type*>(out.data()),
        storage.data.size(),
    );
    return out;
}

} // namespace ts::j2k
