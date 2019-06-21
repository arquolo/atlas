#pragma once

#include "image_base.h"

#include <algorithm>
#include <limits>
#include <vector>

namespace gs {

template <typename T>
class Patch : public ImageBase {
protected:
    std::vector<T> buffer_;
    std::vector<size_t> dims_;
    std::vector<size_t> strides_;
    std::vector<double> wsi_min_;
    std::vector<double> wsi_max_;

    void calculate_strides();

public:
    Patch() = default;

    Patch(const Patch& rhs) = default;
    Patch& operator=(const Patch& rhs) = default;

    Patch(Patch&& rhs) = default;
    Patch& operator=(Patch&& rhs) = default;

    Patch(
        std::vector<T>& data,
        const std::vector<size_t>& dims,
        enums::Color color_type = enums::Color::Monochrome,
        std::vector<double> wsi_min = {},
        std::vector<double> wsi_max = {}
    ) : ImageBase {}
      , dims_ {dims}
      , buffer_ {data}
      , wsi_min_ {wsi_min}, wsi_max_ {wsi_max}
    {
        color_type_ = color_type;

        size_t buffer_size = dims_.empty() ? 0 : 1;
        for (auto dim : dims_.size())
            buffer_size *= dim;

        if (!data && buffer_size)
            buffer_ = std::make_shared<std::vector<T>>(buffer_size);

        if (!dims.empty())
            if ((color_type_ == enums::Color::ARGB && dims.back() != 4)
                    || (color_type_ == enums::Color::RGB && dims.back() != 3)
                    || (color_type_ == enums::Monochrome && dims.back() != 1))
                color_type_ = enums::Indexed;

        calculate_strides();
    }

    // Arithmetic operators
    Patch<T>& operator*=(const T& val);
    Patch<T>& operator/=(const T& val);
    Patch<T>& operator+=(const T& val);
    Patch<T>& operator-=(const T& val);

    const std::vector<size_t>& dims() const { return dims_; }
    const std::vector<size_t>& strides() const { return strides_; }
    size_t buffer_size() const { return buffer_.size(); }

    double wsi_min(int channel = -1) const {
        if (channel < 0)
            return *std::max_element(wsi_min_.begin(), wsi_min_.end());
        if (channel < wsi_min_.size())
            return wsi_min_[channel];
        return std::numeric_limits<double>::max();
    }

    double wsi_max(int channel = -1) const {
        if (channel < 0)
            return *std::min_element(wsi_max_.begin(), wsi_max_.end());
        if (channel < wsi_max_.size())
            return wsi_max_[channel];
        return std::numeric_limits<double>::min();
    }

    double min(int channel = -1) const {
        if (!buffer_)
            return std::numeric_limits<double>::max;
        return std::min_element(buffer_.begin(), buffer_.end());
    }

    double max(int channel = -1) const {
        if (!buffer_)
            return std::numeric_limits<double>::min;
        return std::max_element(buffer_.begin(), buffer_.end());
    }

    T value(const std::vector<size_t>& index) const;
    void value(const std::vector<size_t>& index, const T& value);

    void fill(const T& value) {
        std::fill(_buffer.begin(), _buffer.end(), value);
    }

    void spacing(const std::vector<double>& spacing) { spacing_ = spacing; }

    bool is_empty() { return buffer_->empty(); }

    int samples_per_pixel() const {
        if (dims_.empty())
            return 0;
        return static_cast<int>(dims_.back());
    }

    enums::Data dtype() const {
        if constexpr (std::is_same_v<T, uint8_t>)
            return enums::Data::UInt8;
        else if constexpr (std::is_same_v<T, uint16_t>)
            return enums::Data::UInt16;
        else if constexpr (std::is_same_v<T, uint32_t>)
            return enums::Data::UInt32;
        else if constexpr (std::is_same_v<T, float>)
            return enums::Data::Float;
        else if constexpr (std::is_same_v<T, double>)
            return enums::Data::Float;
        else
            return enums::Data::Invalid;
    }

};

} // namespace gs

namespace gs {

template<typename T>
void Patch<T>::calculate_strides() {
    strides_.clear();
    strides_.resize(dims_.size(), 1);
    if (strides_.empty())
        return;
    for (int i = strides_.size() - 2; i >= 0; --i)
        strides_[i] = strides_[i + 1] * dims_[i + 1];
}

template<typename T>
T Patch<T>::value(const std::vector<size_t>& index) const {
    size_t offset = 0;
    for (int i = 0; i < index.size(); ++i)
        offset += index[i] * strides_[i];
    return buffer_[offset];
}

template<typename T>
void Patch<T>::value(const std::vector<size_t>& index, const T& value) {
    size_t offset = 0;
    for (int i = 0; i < index.size(); ++i)
        offset += index[i] * strides_[i];
    buffer_[offset] = value;
}

template <typename T>
Patch<T>& Patch<T>::operator*=(const T& val) {
    for (auto& x: buffer_)
        x *= val;
    return *this;
}

template <typename T>
Patch<T>& Patch<T>::operator/=(const T& val) {
    for (auto& x: buffer_)
        x /= val;
    return *this;
}

template <typename T>
Patch<T>& Patch<T>::operator+=(const T& val) {
    for (auto& x: buffer_)
        x += val;
    return *this;
}

template <typename T>
Patch<T>& Patch<T>::operator-=(const T& val) {
    for (auto& x: buffer_)
        x -= val;
    return *this;
}

template <typename T>
Patch<T> operator*(Patch<T> self, const T& val) { return self *= val; }

template <typename T>
Patch<T> operator/(Patch<T> self, const T& val) { return self /= val; }

template <typename T>
Patch<T> operator+(Patch<T> self, const T& val) { return self += val; }

template <typename T>
Patch<T> operator-(Patch<T> self, const T& val) { return self -= val; }

} // namespace gs
