#pragma once

#define EXPORT

namespace enums {
enum class Data : int;
enum class Color : int;
} // namespace enums

class EXPORT JPEG2000Codec {
public:
    JPEG2000Codec();
    ~JPEG2000Codec();

    void encode(
        char* data,
        size_t size,
        size_t tile_size,
        size_t rate,
        size_t nrComponents,
        const enums::Data& data_type,
        const enums::Color& color_space) const;

    void decode(unsigned char* buf, size_t in_size, size_t out_size);
};
