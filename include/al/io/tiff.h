#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include "tiffio.h"

#include "al/core/path.h"

namespace al::io::tiff {

template <typename T>
struct Allocator {
    using value_type = T;

    Allocator() = default;

    template <typename U>
    constexpr Allocator(const Allocator<U>&) noexcept {}

    [[nodiscard]] T* allocate(size_t n) {
        if (n > size_t{-1} / sizeof(T))
            throw std::bad_alloc{};
        if (auto p = static_cast<T*>(_TIFFmalloc(n * sizeof(T))))
            return p;
        throw std::bad_alloc{};
    }

    void deallocate(T* p, size_t) noexcept { _TIFFfree(p); }
};

template <typename T, typename U>
bool operator==(const Allocator<T>&, const Allocator<U>&) { return true; }

template <typename T, typename U>
bool operator!=(const Allocator<T>&, const Allocator<U>&) { return false; }

class File {
    std::unique_ptr<TIFF, void (*)(TIFF*)> ptr_;

public:
    File(const Path& path, const std::string& flags);

    /// for compatibility
    inline operator TIFF*() noexcept { return ptr_.get(); }
    inline operator TIFF*() const noexcept { return ptr_.get(); }

    uint32_t position(size_t iy, size_t ix) const noexcept;
    uint32_t tiles() const noexcept;

    template <typename T>
    T get(uint32 tag) const {
        T value = {};
        TIFFGetField(*this, tag, &value);
        return value;
    }

    template <typename T>
    std::optional<T> try_get(uint32 tag) const {
        T value = {};
        if (TIFFGetField(*this, tag, &value))
            return value;
        return {};
    }

    template <typename T>
    std::optional<T> get_defaulted(uint32 tag) const {
        T value = {};
        if (TIFFGetFieldDefaulted(*this, tag, &value))
            return value;
        return {};
    }

    template <typename T>
    void set_field(uint32_t tag, T const& value) noexcept {
        TIFFSetField(ptr_, tag, value);
    }

    // template <typename T>
    // void write_raw(size_t pos, const std::vector<T>& tile) noexcept {
    //     TIFFWriteRawTile(
    //         ptr_, static_cast<uint32_t>(pos), static_cast<void*>(tile.data()),
    //         tile.size() * sizeof(T));
    // }

    // template <typename T>
    // void write_encoded(size_t pos, const std::vector<T>& tile) noexcept {
    //     TIFFWriteEncodedTile(
    //         ptr_, static_cast<uint32_t>(pos), static_cast<void*>(tile.data()),
    //         tile.size() * sizeof(T));
    // }

    // template <typename T>
    // std::vector<T> read_encoded(size_t pos, size_t pixels) noexcept {
    //     std::vector<T> raster(pixels);
    //     auto size = TIFFReadEncodedTile(
    //         ptr_, static_cast<uint32_t>(pos), static_cast<void*>(raster.data()),
    //         pixels * sizeof(T));
    //     if (size > 0)
    //         return raster;
    //     return {}
    // }

    // void write_directory() noexcept;
};

} // namespace al::io::tiff
