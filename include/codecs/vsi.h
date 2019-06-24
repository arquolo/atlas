#pragma once

#include <vector>

#include "image.h"

namespace gs {

class VSIImage : public Image {
	std::string vsi_filename_;
	std::string ets_file_;
	std::vector<size_t> tile_offsets_;
	std::vector<std::vector<size_t>> tile_coords_;
    size_t tile_size_y_;
    size_t tile_size_x_;
    size_t tile_count_y_;
    size_t tile_count_x_;

    size_t compression_type_;

    char* decode_tile(int no, int row, int col) const;
    size_t parse_ets_file(std::ifstream& ets);

protected :
    void cleanup();

    template <typename T>
    std::vector<T> read_impl(
        size_t y, size_t x, size_t height, size_t width, size_t level);

    double min_(int channel = -1) const { return 0.; }
    double max_(int channel = -1) const { return 255.; }

public:
    VSIImage(const std::string& filename);
    ~VSIImage();

};

} // namespace gs
