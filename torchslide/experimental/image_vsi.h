#pragma once

#include <string>
#include <vector>

#include "image.h"

namespace ts {

class VSIImage final : public Image<VSIImage> {
    static inline constexpr const char* extensions[] = {".vsi",};

	std::string vsi_filename_ = "";
	std::string ets_file_ = "";
	std::vector<size_t> tile_offsets_ = {};
	std::vector<std::vector<size_t>> tile_coords_ = {};
    size_t tile_size_y_ = 0;
    size_t tile_size_x_ = 0;
    size_t tile_count_y_ = 0;
    size_t tile_count_x_ = 0;

    const size_t compression_type_ = 0;

    char* decode_tile(int no, int row, int col) const;
    size_t parse_ets_file(std::ifstream& ets);

public:
    VSIImage(const Path& path);

    template <typename T>
    Array<T> _read(const Box& box) const {
        throw std::runtime_error{"Not implemented"};
    }

    template <>
    Array<uint8_t> _read(const Box& box) const;
};

} // namespace ts
