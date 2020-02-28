#include <memory>
#include <sstream>

#include <openslide/openslide.h>

#include "dispatch.h"
#include "tensor.h"

namespace ts::os {

// ------------------------------ declarations ------------------------------

struct File {
    File(Path const& path);
    inline operator openslide_t*() const noexcept { return _ptr.get(); }
    inline operator openslide_t*() noexcept { return _ptr.get(); }

private:
    std::unique_ptr<openslide_t, void (*)(openslide_t*)> _ptr;
};

struct OpenSlide final : Dispatch<OpenSlide> {
    static inline constexpr int priority = 1;
    static inline constexpr char const* extensions[]
        = {".bif",
           ".ndpi",
           ".mrxs",
           ".scn",
           ".svs",
           ".svslide",
           ".tif",
           ".tiff",
           ".vms",
           ".vmu"};

    template <class... Ts>
    OpenSlide(
        File file, std::array<uint8_t, 3> bg_color, Ts&&... args) noexcept
      : Dispatch{std::forward<Ts>(args)...}
      , _file{std::move(file)}
      , _bg_color{std::move(bg_color)} { }

    static std::unique_ptr<Image> make_this(Path const& path);

    template <typename T>
    Tensor<T> read(Box const&) const {
        throw std::runtime_error{"Not implemented"};
    }

private:
    File _file;
    std::array<uint8_t, 3> _bg_color;
};

// ------------------------ non-template definitions ------------------------

auto os_open(Path const& path) {
    auto c_path = path.string();
    if (!openslide_detect_vendor(c_path.c_str()))
        throw std::runtime_error{"Cant open without vendor"};

    auto ptr = openslide_open(c_path.c_str());
    if (char const* error = openslide_get_error(ptr))
        throw std::runtime_error{error};

    return ptr;
}

File::File(Path const& path) : _ptr{os_open(path), openslide_close} { }

std::unique_ptr<Image> OpenSlide::make_this(Path const& path) {
    auto file = File{path};
    auto levels_num = openslide_get_level_count(file);

    std::map<Level, LevelInfo> levels;
    for (Level level = 0; level < levels_num; ++level) {
        int64_t y;
        int64_t x;
        openslide_get_level_dimensions(file, level, &x, &y);

        std::string tag_h
            = "openslide.level[" + std::to_string(level) + "].tile-height";
        std::string tag_w
            = "openslide.level[" + std::to_string(level) + "].tile-width";
        auto lh = openslide_get_property_value(file, tag_h.c_str());
        auto lw = openslide_get_property_value(file, tag_w.c_str());

        levels[level]
            = {{static_cast<uint32_t>(y), static_cast<uint32_t>(x), 3},
               {std::stoi(lh, 0, 10), std::stoi(lw, 0, 10), 3}};
    }

    Spacing spacing;
    int i = 0;
    for (auto tag :
         {OPENSLIDE_PROPERTY_NAME_MPP_Y, OPENSLIDE_PROPERTY_NAME_MPP_X}) {
        char const* ssm = openslide_get_property_value(file, tag);
        if (ssm)
            spacing[i++] = std::stof(ssm);
    }

    // Get background color if present
    std::array<uint8_t, 3> bg_color = {255, 255, 255};
    char const* bg_color_hex
        = openslide_get_property_value(file, "openslide.background-color");
    if (bg_color_hex) {
        uint32_t bg_color32 = std::stoi(bg_color_hex, 0, 16);
        bg_color[0] = 0xFF & (bg_color32 >> 16); // R
        bg_color[1] = 0xFF & (bg_color32 >> 8);  // G
        bg_color[2] = 0xFF & bg_color32;         // B
    }

    return std::make_unique<OpenSlide>(
        std::move(file),
        std::move(bg_color),
        uint8_t{},
        3,
        std::move(levels),
        std::move(spacing));
}

template <>
Tensor<uint8_t> OpenSlide::read(Box const& box) const {
    Tensor<uint8_t> buf{{box.shape(0), box.shape(1), Size{4}}};
    auto scale = this->scales()[box.level];
    openslide_read_region(
        _file,
        reinterpret_cast<uint32_t*>(buf.data()),
        box.min_[1] * scale,
        box.min_[0] * scale,
        box.level,
        box.shape(1),
        box.shape(0));

    Tensor<uint8_t> result{{box.shape(0), box.shape(1), Size{3}}};

    auto b = buf.template view<3>();
    auto r = result.template view<3>();

    for (Size y = 0; y < box.shape(0); ++y)
        for (Size x = 0; x < box.shape(1); ++x)
            std::reverse_copy(&b({y, x}), &b({y, x, 3}), &r({y, x, 3}));
    return result;
}

} // namespace ts::os
