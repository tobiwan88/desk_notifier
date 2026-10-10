// settings: NVS-backed configuration (Wi-Fi, token, hostname, saved style).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "eyes/core/style.hpp"

namespace settings {

void init() noexcept;  // nvs_flash_init (erases on version mismatch)
std::optional<std::string> get(const char* key) noexcept;
bool set(const char* key, std::string_view value) noexcept;
void erase_all() noexcept;

std::optional<eyes::EyeStyle> load_style() noexcept;
bool save_style(const eyes::EyeStyle&) noexcept;

}  // namespace settings
