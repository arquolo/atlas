#include "al/io/tiff.h"

namespace al::io::tiff {

auto tiff_open(const Path& path, const std::string& flags) {
#ifdef _WIN32
    auto open_fn = TIFFOpenW;
#else
    auto open_fn = TIFFOpen;
#endif
    TIFFSetErrorHandler(nullptr);
    auto ptr_ = open_fn(path.c_str(), flags.c_str());
    if (ptr_)
        return std::unique_ptr<TIFF, void (*)(TIFF*)>{ptr_, TIFFClose};
    throw std::runtime_error{"Failed to open: " + path.generic_string()};
}

File::File(const Path& path, const std::string& flags)
  : ptr_{tiff_open(path, flags)} {}

uint32_t File::position(size_t iy, size_t ix) const noexcept {
    return static_cast<size_t>(TIFFComputeTile(
        *this, static_cast<uint32_t>(ix), static_cast<uint32_t>(iy), 0, 0));
}

uint32_t File::tiles() const noexcept { return TIFFNumberOfTiles(*this); }

// void File::write_directory () noexcept { TIFFWriteDirectory(*this); }

} // namespace al::io::tiff
