// pointer_scan.h -- discover pointer-path chains that resolve to a given
// target address.
//
// Given a dynamic address T that moves between runs (e.g. the current
// slot for `playerHealth`), the scanner walks the process's readable
// memory to find chains of the form:
//
//     *(... *( STATIC + o1 ) ... + oN-1 ) + oN == T
//
// where STATIC is a pointer that lives inside a module image (MEM_IMAGE
// region), so its address survives process restarts. Depth and the
// offset window are configurable; results are capped to keep UI usable.
#pragma once

#include "common.h"
#include "mem_regions.h"

namespace cheat {

struct PointerPath {
    Address          base{};      // the "static" starting slot
    std::vector<int> offsets;     // applied in order, deepest last
};

struct PointerScanConfig {
    int         maxDepth{3};      // 1 .. 6 in practice
    int         maxOffset{0x1000};
    std::size_t maxResults{500};
};

using PointerProgress = std::function<void(std::size_t donePages,
                                             std::size_t totalPages)>;

class PointerScanner {
public:
    std::vector<PointerPath> find(HANDLE process, Address target,
                                    const PointerScanConfig& cfg,
                                    const std::atomic<bool>& cancel,
                                    PointerProgress prog = {});
};

} // namespace cheat
