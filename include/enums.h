#pragma once

namespace enums {

enum class Color : int {
    Invalid,
    Monochrome,
    RGB,
    ARGB,
    Indexed,
};

enum class Data : int {
    Invalid,
    UInt8,
    UInt16,
    UInt32,
    Float,
};

enum class Codec : int {
    RAW,
    JPEG,
    LZW,
    JPEG2000,
};

enum class Interpolation : int {
    Nearest,
    Linear,
};

} // namespace enums
