#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace plant {

template <std::size_t Capacity>
class FixedBuffer {
public:
    [[nodiscard]] constexpr std::size_t capacity() const noexcept { return Capacity; }
    [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] constexpr const std::uint8_t* data() const noexcept { return bytes_.data(); }
    [[nodiscard]] constexpr std::uint8_t* data() noexcept { return bytes_.data(); }

    constexpr void clear() noexcept { size_ = 0; }

    constexpr bool resize(std::size_t size) noexcept {
        if (size > Capacity) {
            return false;
        }
        size_ = size;
        return true;
    }

    constexpr bool append(const std::uint8_t* source, std::size_t size) noexcept {
        if (source == nullptr || size > Capacity - size_) {
            return false;
        }
        for (std::size_t index = 0; index < size; ++index) {
            bytes_[size_ + index] = source[index];
        }
        size_ += size;
        return true;
    }

    constexpr bool push_back(std::uint8_t value) noexcept {
        if (size_ == Capacity) {
            return false;
        }
        bytes_[size_++] = value;
        return true;
    }

    [[nodiscard]] constexpr std::uint8_t operator[](std::size_t index) const noexcept {
        return bytes_[index];
    }

private:
    std::array<std::uint8_t, Capacity> bytes_{};
    std::size_t size_{0};
};

}  // namespace plant
