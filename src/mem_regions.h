// mem_regions.h -- enumerate the debuggee's readable / writable memory
// pages. The scanner iterates these.
#pragma once

#include "common.h"

namespace cheat {

struct Region {
    Address base{};
    std::size_t size{};
    DWORD protect{};          // PAGE_* flags
    DWORD type{};             // MEM_PRIVATE / MEM_MAPPED / MEM_IMAGE
};

// Walk the process address space and return every committed region that
// is readable (and not a guard page). If `writableOnly` is true, skip
// read-only regions -- useful for scanning values the target owns,
// skipping modules' .text.
std::vector<Region> enumerateRegions(HANDLE process, bool writableOnly);

inline bool protectReadable(DWORD p) {
    const DWORD readableMask =
        PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
        PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (p & readableMask) != 0 && (p & PAGE_GUARD) == 0;
}

inline bool protectWritable(DWORD p) {
    const DWORD writableMask =
        PAGE_READWRITE | PAGE_WRITECOPY |
        PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (p & writableMask) != 0 && (p & PAGE_GUARD) == 0;
}

} // namespace cheat
