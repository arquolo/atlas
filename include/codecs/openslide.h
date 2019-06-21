#pragma once

#include "Image.h"

struct _openslide;
using openslide_t = _openslide;

class OpenSlide : public Image {
  std::string error_state_;

  unsigned char bg_r_;
  unsigned char bg_g_;
  unsigned char bg_b_;

protected :
    void cleanup();

    void* read(size_t start_y, size_t start_x, size_t height, size_t width, size_t level);

    openslide_t* slide_;

public:
    OpenSlide(const std::string& path);
    ~OpenSlide();
    double min(int channel = -1) { return 0.; }
    double max(int channel = -1) { return 255.; }

    std::string property(const std::string& name);
    std::string error_state();

    void cache_capacity(size_t capacity);
};
