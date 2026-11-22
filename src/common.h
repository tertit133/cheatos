// common.h -- shared types, Windows includes, utility helpers.
#pragma once

// Windows headers must come first so stdlib headers that pull in Win32
// internals (thread, mutex, ...) see a fully populated <windows.h> on
// first use. This ordering was the root cause of a nasty cascade of
// "DWORD undeclared" errors in a sibling project; keep it.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace cheat {

using Address = std::uintptr_t;
using Byte    = std::uint8_t;
using Bytes   = std::vector<Byte>;

template <typename... Args>
inline std::string sfmt(const char* fmt, Args... args) {
    char buf[512];
    int n = std::snprintf(buf, sizeof(buf), fmt, args...);
    if (n < 0) return {};
    return std::string(buf, static_cast<std::size_t>(
                             std::min<int>(n, sizeof(buf) - 1)));
}

inline std::wstring utf8_to_wide(const std::string& s) {
    if (s.empty()) return {};
    int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

inline std::string wide_to_utf8(const std::wstring& s) {
    if (s.empty()) return {};
    int n = ::WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(),
                                   nullptr, 0, nullptr, nullptr);
    std::string u(static_cast<std::size_t>(n), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(),
                           u.data(), n, nullptr, nullptr);
    return u;
}

} // namespace cheat
