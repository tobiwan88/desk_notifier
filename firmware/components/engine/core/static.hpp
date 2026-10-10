// Fixed-capacity containers, no heap (plan §3.0).
#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>

namespace engine {

template <class T, std::size_t N>
class StaticVec {
    static_assert(std::is_trivially_copyable_v<T>);
public:
    constexpr bool push_back(const T& v) noexcept {
        if (n_ >= N) return false;
        d_[n_++] = v;
        return true;
    }
    constexpr void clear() noexcept { n_ = 0; }
    constexpr std::size_t size() const noexcept { return n_; }
    constexpr bool empty() const noexcept { return n_ == 0; }
    static constexpr std::size_t capacity() noexcept { return N; }
    constexpr T& operator[](std::size_t i) noexcept { return d_[i]; }
    constexpr const T& operator[](std::size_t i) const noexcept { return d_[i]; }
    constexpr const T* begin() const noexcept { return d_.data(); }
    constexpr const T* end() const noexcept { return d_.data() + n_; }
    constexpr T* begin() noexcept { return d_.data(); }
    constexpr T* end() noexcept { return d_.data() + n_; }
private:
    std::array<T, N> d_{};
    std::size_t n_ = 0;
};

template <std::size_t N>
class StaticString {
public:
    constexpr StaticString() noexcept = default;
    constexpr explicit StaticString(std::string_view s) noexcept { assign(s); }
    constexpr bool assign(std::string_view s) noexcept {
        const bool fits = s.size() <= N;
        n_ = fits ? s.size() : N;
        for (std::size_t i = 0; i < n_; ++i) d_[i] = s[i];
        d_[n_] = '\0';
        return fits;
    }
    constexpr std::string_view view() const noexcept { return {d_.data(), n_}; }
    constexpr const char* c_str() const noexcept { return d_.data(); }
    constexpr bool empty() const noexcept { return n_ == 0; }
    constexpr std::size_t size() const noexcept { return n_; }
private:
    std::array<char, N + 1> d_{};
    std::size_t n_ = 0;
};

}  // namespace engine
