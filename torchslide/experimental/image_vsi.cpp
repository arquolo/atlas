#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

// Include DCMTK LIBJPEG for lossy and lossless JPEG compression
// extern "C" {
// #define boolean ijg_boolean
// #include "dcmjpeg/libijg8/jpeglib8.h"
// #include "jpeg_mem_src.h"
// #undef boolean
// }

#include "io/jpeg2000.h"
#include "image_vsi.h"

namespace ts {

VSIImage::VSIImage(const Path& path) : vsi_filename_{path.string()} {
    // auto pth = extract_filepath(path);
    // auto filename = extract_filename(path);
    // auto basename = extract_basename(path);
    // auto ets_files = get_files(
    //     complete_path("_" + basename + "_", path),
    //     "*.ets",
    //     true);
    // if (ets_files.empty())
    //     ets_files = get_files(
    //         complete_path(basename, pth), "*.ets", true);
    // if (ets_files.empty())
    //     ets_files = get_files(
    //         "_" + complete_path(basename, pth), "*.ets", true);
    // if (ets_files.empty()) {
    //     ets_files = get_files(
    //         complete_path(basename, pth) + "_", "*.ets", true);
    // if (ets_files.empty())
    //     throw std::runtime_error{
    //         "Could not find the .ets files belonging to " +
    //         basename +
    //         ".vsi, is the VSI-folder present and correctly named?"};

    auto filename = path.filename();
    auto basename = path.parent_path();
    std::vector<std::string> ets_files;  //? WTF

    auto it = std::max_element(ets_files.begin(), ets_files.end(), [](auto&& ets_file){
        std::ifstream ets{ets_file.c_str(), std::ios::in | std::ios::binary};
        if (!ets.good())
            return 0;
        return parse_ets_file(ets);
    });
    if (it == ets_files.end())
        throw std::runtime_error{"No ets files found"};

    std::ifstream ets{it->c_str(), std::ios::in | std::ios::binary};
    parse_ets_file(ets);
}

template <typename T>
T read_typed(std::ifstream& f) {
    T value;
    f.read(reinterpret_cast<char*>(&value), sizeof(T));
    return value;
}

size_t VSIImage::parse_ets_file(std::ifstream& ets) {
    // Read general file info
    ets.seekg(4, std::ios::cur);

    auto header_size = read_typed<int32_t>(ets);
    auto version = read_typed<int32_t>(ets);
    auto dims = read_typed<int32_t>(ets);
    auto additional_header_offset = read_typed<int64_t>(ets);
    auto additional_header_size = read_typed<int32_t>(ets);

    ets.seekg(4, std::ios::cur);

    auto used_chunk_offset = read_typed<uint64_t>(ets);
    auto used_chunks = read_typed<int32_t>(ets);

    ets.seekg(additional_header_offset + 8, std::ios::beg);

    auto pixel_type = read_typed<int32_t>(ets);
    auto colors = read_typed<int32_t>(ets);
    auto color_space = read_typed<int32_t>(ets);
    auto compression_type = read_typed<int32_t>(ets);
    auto compression_quality = read_typed<int32_t>(ets);

    tile_size_x_ = read_typed<int32_t>(ets);
    tile_size_y_ = read_typed<int32_t>(ets);
    auto tile_depth = read_typed<int32_t>(ets);

    bool is_rgb = colors > 1;

    // Read locations of tiles and file offsets
    ets.seekg(used_chunk_offset, std::ios::beg);
    for (int tile = 0; tile < used_chunks; ++tile) {
        ets.seekg(4, std::ios::cur);
        std::vector<size_t> cur_tile_coords;
        for (int i = 0; i < dims; ++i)
            cur_tile_coords.push_back(read_typed<int32_t>(ets));

        tile_offsets_.push_back(read_typed<uint64_t>(ets));
        tile_coords_.push_back(cur_tile_coords);
        ets.seekg(8, std::ios::cur);
    }

    int max_x = 0;
    int max_y = 0;
    for (const auto& t : tile_coords_) {
        if (t[0] > max_x)
            max_x = t[0];
        if (t[1] > max_y)
            max_y = t[1];
    }

    image_size_y_ = (max_y > 1) ? (tile_size_y_ * (max_y + 1)) : tile_size_y_;
    image_size_x_ = (max_x > 1) ? (tile_size_x_ * (max_x + 1)) : tile_size_x_;
    nr_tiles_y_ = (max_y > 1) ? (max_y + 1) : 1;
    nr_tiles_x_ = (max_x > 1) ? (max_x + 1) : 1;

    return _imageSizeX * _imageSizeY;
};

VSIImageReader::Region::Region(
    unsigned int x, unsigned int y, unsigned int w, unsigned int h)
  : x(x), y(y), w(w), h(h) {}

bool VSIImageReader::Region::intersects(const VSIImageReader::Region& r) const {
    unsigned int tw = this->w;
    unsigned int th = this->h;
    unsigned int rw = r.w;
    unsigned int rh = r.h;
    if (rw <= 0 || rh <= 0 || tw <= 0 || th <= 0) {
        return false;
    }
    unsigned int tx = this->x;
    unsigned int ty = this->y;
    unsigned int rx = r.x;
    unsigned int ry = r.y;
    rw += rx;
    rh += ry;
    tw += tx;
    th += ty;
    bool rtn
        = ((rw < rx || rw > tx) && (rh < ry || rh > ty) && (tw < tx || tw > rx)
           && (th < ty || th > ry));
    return rtn;
}

VSIImageReader::Region
VSIImageReader::Region::intersection(const VSIImageReader::Region& r) const {
    int x = max(this->x, r.x);
    int y = max(this->y, r.y);
    int w = min(this->x + this->w, r.x + r.w) - x;
    int h = min(this->y + this->h, r.y + r.h) - y;

    if (w < 0)
        w = 0;
    if (h < 0)
        h = 0;

    return Region(x, y, w, h);
}

char* VSIImageReader::decodeTile(int no, int row, int col) const {
    int size = _tileSizeX * _tileSizeY * 3;
    char* buf = new char[size];
    if (no == _tileCoords.size()) {
        std::fill(buf, buf + (3 * _tileSizeX * _tileSizeY), 0);
        return buf;
    }
    ifstream ets;
    ets.open(_etsFile.c_str(), ios::in | ios::binary);
    ets.seekg(_tileOffsets[no]);
    ets.read(buf, size);
    if (_compressionType == 0)
        return buf;

    if (_compressionType == 3) {
        JPEG2000Codec cod;
        cod.decode(buf, size);
        return buf;
    }
    if (_compressionType != 2 && _compressionType != 5)
        return buf;

    jpeg_decompress_struct cinfo;
    jpeg_error_mgr jerr; //error handling
    jpeg_source_mgr src_mem;
    jpeg_create_decompress(&cinfo);
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_mem_src(&cinfo, &src_mem, (void*)buf, size);
    jpeg_read_header(&cinfo, true);
    if (_compressionType == 2) {
        cinfo.jpeg_color_space = JCS_YCbCr;
    } else {
        cinfo.jpeg_color_space = JCS_RGB;
    }
    jpeg_start_decompress(&cinfo);
    unsigned char* outBuf = new unsigned char[size];
    unsigned char* line = outBuf;
    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, &line, 1);
        line += 3 * cinfo.output_width;
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    delete[] buf;
    buf = (char*)outBuf;
    return buf;
}

void VSIImageReader::readRegion(
    const unsigned long long& startX,
    const unsigned long long& startY,
    const unsigned long long& width,
    const unsigned long long& height,
    unsigned char* data) const {

    int tileRows = _nrTilesY;
    int tileCols = _nrTilesX;

    VSIImageReader::Region image = Region(startX, startY, width, height);
    int outputRow = 0, outputCol = 0;
    int outputRowLen = width * 3;

    Region intersection(0, 0, 0, 0);
    for (int row = 0; row < tileRows; row++) {
        for (int col = 0; col < tileCols; col++) {
            int width = _tileSizeX;
            int height = _tileSizeY;
            Region tile = Region(col * width, row * height, width, height);
            if (!tile.intersects(image))
                continue;

            intersection = tile.intersection(image);
            int intersectionX = 0;

            if (tile.x < image.x)
                intersectionX = image.x - tile.x;

            int no = 0;
            for (const auto&& coords : tile_coords_) {
                if ((coords[0] == col) && (coords[1] == row) && (coords[3] == 0))
                    break;
                ++no;
            }
            char* tileBuf = decodeTile(no, row, col);
            int rowLen = 3 * std::min((int)intersection.w, width);

            int outputOffset = outputRow * outputRowLen + outputCol;
            for (int trow = 0; trow < intersection.h; trow++) {
                int realRow = trow + intersection.y - tile.y;
                int inputOffset = 3 * (realRow * width + intersectionX);
                memcpy(data + outputOffset, tileBuf + inputOffset, rowLen);
                outputOffset += outputRowLen;
            }

            outputCol += rowLen;
            delete[] tileBuf;
        }

        if (intersection.w > 0 && intersection.h > 0) {
            outputRow += intersection.h;
            outputCol = 0;
        }
    }
}


char* VSIImage::decodeTile(int no, int row, int col) const {
    int size = _tileSizeX * _tileSizeY * 3;
    char* buf = new char[size];
    std::fill(buf, buf + size, 255);
    if (no != _tileCoords.size()) {
        ifstream ets;
        ets.open(_etsFile.c_str(), ios::in | ios::binary);
        ets.seekg(_tileOffsets[no]);
        ets.read(buf, size);
        if (_compressionType == 0) {
            return buf;
        } else if (_compressionType == 3) {
            JPEG2000Codec cod;
            cod.decode((unsigned char*)buf, size, size);
        } else if (_compressionType == 2 || _compressionType == 5) {
            jpeg_decompress_struct cinfo;
            jpeg_error_mgr jerr; //error handling
            jpeg_source_mgr src_mem;
            jpeg_create_decompress(&cinfo);
            cinfo.err = jpeg_std_error(&jerr);
            jpeg_mem_src(&cinfo, &src_mem, (void*)buf, size);
            jpeg_read_header(&cinfo, true);
            if (_compressionType == 2) {
                cinfo.jpeg_color_space = JCS_YCbCr;
            } else {
                cinfo.jpeg_color_space = JCS_RGB;
            }
            jpeg_start_decompress(&cinfo);
            unsigned char* outBuf = new unsigned char[size];
            unsigned char* line = outBuf;
            while (cinfo.output_scanline < cinfo.output_height) {
                jpeg_read_scanlines(&cinfo, &line, 1);
                line += 3 * cinfo.output_width;
            }
            jpeg_finish_decompress(&cinfo);
            jpeg_destroy_decompress(&cinfo);
            delete[] buf;
            buf = (char*)outBuf;
        }
    }
    return buf;
}


unsigned long long VSIImage::parseETSFile(std::ifstream& ets) {
    // Read general file info
    char* memblock = new char[4];
    char* memblockLong = new char[8];
    ets.read(memblock, 4);
    ets.read(memblock, 4);
    int headerSize = *reinterpret_cast<int*>(memblock);
    ets.read(memblock, 4);
    int version = *reinterpret_cast<int*>(memblock);
    ets.read(memblock, 4);
    int nDims = *reinterpret_cast<int*>(memblock);
    ets.read(memblockLong, 8);
    long additionalHeaderOffset = *reinterpret_cast<long*>(memblockLong);
    ets.read(memblock, 4);
    int additionalHeaderSize = *reinterpret_cast<int*>(memblock);
    ets.seekg(4, ios::cur);
    ets.read(memblockLong, 8);
    unsigned long long usedChunkOffset
        = *reinterpret_cast<unsigned long long*>(memblockLong);
    ets.read(memblock, 4);
    int nUsedChunks = *reinterpret_cast<int*>(memblock);
    ets.seekg(additionalHeaderOffset);
    ets.read(memblock, 4);
    ets.seekg(4, ios::cur);
    ets.read(memblock, 4);
    int pixelType = *reinterpret_cast<int*>(memblock);
    ets.read(memblock, 4);
    int nrColors = *reinterpret_cast<int*>(memblock);
    ets.read(memblock, 4);
    int colorSpace = *reinterpret_cast<int*>(memblock);
    ets.read(memblock, 4);
    _compressionType = *reinterpret_cast<int*>(memblock);
    ets.read(memblock, 4);
    int compressionQuality = *reinterpret_cast<int*>(memblock);
    ets.read(memblock, 4);
    _tileSizeX = *reinterpret_cast<int*>(memblock);
    ets.read(memblock, 4);
    _tileSizeY = *reinterpret_cast<int*>(memblock);
    ets.read(memblock, 4);
    int tileDepth = *reinterpret_cast<int*>(memblock);
    bool isRGB = nrColors > 1;

    // Read locations of tiles and file offsets
    ets.seekg(usedChunkOffset);
    for (int tile = 0; tile < nUsedChunks; ++tile) {
        ets.seekg(4, ios::cur);
        vector<int> curTileCoords;
        for (int i = 0; i < nDims; i++) {
            ets.read(memblock, 4);
            curTileCoords.push_back(*reinterpret_cast<int*>(memblock));
        }
        ets.read(memblockLong, 8);
        _tileOffsets.push_back(
            *reinterpret_cast<unsigned long long*>(memblockLong));
        ets.read(memblock, 4);
        int nrBytes = *reinterpret_cast<int*>(memblock);
        ets.seekg(4, ios::cur);
        _tileCoords.push_back(curTileCoords);
    }
    int maxX = 0;
    int maxY = 0;

    for (vector<vector<int>>::iterator t = _tileCoords.begin();
         t != _tileCoords.end();
         ++t) {
        if ((*t)[0] > maxX) {
            maxX = (*t)[0];
        }
        if ((*t)[1] > maxY) {
            maxY = (*t)[1];
        }
    }
    std::vector<unsigned long long> L0Dims(2, 0);
    if (maxX > 1) {
        L0Dims[0] = _tileSizeX * (maxX + 1);
    } else {
        L0Dims[0] = _tileSizeX;
    }
    if (maxY > 1) {
        L0Dims[1] = _tileSizeY * (maxY + 1);
    } else {
        L0Dims[1] = _tileSizeY;
    }
    _levelDimensions.push_back(L0Dims);

    if (maxY > 1) {
        _nrTilesY = maxY + 1;
    } else {
        _nrTilesY = 1;
    }
    if (maxX > 1) {
        _nrTilesX = maxX + 1;
    } else {
        _nrTilesX = 1;
    }

    delete[] memblock;
    delete[] memblockLong;

    // Set some defaults for VSI
    _numberOfLevels = 1;
    _samplesPerPixel = 3;
    _colorType = RGB;
    _dataType = UChar;
    if (L0Dims[0] * L0Dims[1] > 0) {
        _isValid = true;
    } else {
        _isValid = false;
    }
    return L0Dims[0] * L0Dims[1];
}

void* VSIImage::readDataFromImage(
    const long long& startX,
    const long long& startY,
    const unsigned long long& width,
    const unsigned long long& height,
    const unsigned int& level) {
    if (level != 0)
        return nullptr;

    int data_size = height * width * this->samples;
    std::vector<unsigned char> data(data_size, 255)

    int tileRows = tile_count_y_;
    int tileCols = tile_count_x_;

    Box image = Box(startX, startY, width, height);
    int outputRow = 0, outputCol = 0;
    int outputRowLen = width * 3;

    Box intersection(0, 0, 0, 0);
    for (int row = 0; row < tileRows; row++) {
        for (int col = 0; col < tileCols; col++) {
            int width = _tileSizeX;
            int height = _tileSizeY;
            Box tile = Box(col * width, row * height, width, height);
            if (!tile.intersects(image)) {
                continue;
            }

            intersection = tile.intersection(image);
            int intersectionX = 0;

            if (tile.getStart()[0] < image.getStart()[0]) {
                intersectionX = image.getStart()[0] - tile.getStart()[0];
            }

            int no = 0;
            for (vector<vector<int>>::const_iterator it = _tileCoords.begin();
                 it != _tileCoords.end();
                 ++it) {
                if (((*it)[0] == col)
                    && ((*it)[1] == row && ((*it)[3] == 0))) {
                    break;
                }
                no++;
            }
            char* tileBuf = decodeTile(no, row, col);
            int rowLen = 3
                         * (intersection.getSize()[0] < width
                                ? intersection.getSize()[0]
                                : width);
            int outputOffset = outputRow * outputRowLen + outputCol;
            for (int trow = 0; trow < intersection.getSize()[1]; trow++) {
                int realRow
                    = trow + intersection.getStart()[1] - tile.getStart()[1];
                int inputOffset = 3 * (realRow * width + intersectionX);
                std::memcpy(
                    data + outputOffset, tileBuf + inputOffset, rowLen);
                outputOffset += outputRowLen;
            }

            outputCol += rowLen;
            delete[] tileBuf;
        }

        if (intersection.getSize()[0] > 0 && intersection.getSize()[1] > 0) {
            outputRow += intersection.getSize()[1];
            outputCol = 0;
        }
    }
    return data;
}

} // namespace ts
