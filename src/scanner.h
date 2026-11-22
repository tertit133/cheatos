// scanner.h -- first-scan + next-scan memory search.
//
// A first-scan walks every committed writable region, reading in large
// chunks and applying the comparator at every aligned position. A
// next-scan filters the previous result set in place. Both report progress
// through a callback and honour an atomic cancel flag so the UI can stop
// a runaway scan.
#pragma once

#include "common.h"
#include "mem_regions.h"
#include "value_types.h"

namespace cheat {

struct ScanConfig {
    ValueType  type{ValueType::I32};
    ScanOp     op{ScanOp::Exact};
    TypedValue operand;
    TypedValue operand2;        // Between upper bound (optional)
    std::size_t alignment{0};   // 0 = default (type size, min 1)
    bool       fastScan{true};  // honour alignment vs. scan every byte
    bool       writableOnly{true};
};

struct ScanResult {
    Address address{};
    Bytes   value;              // captured so next-scan can compare
};

using ScanProgress = std::function<void(std::size_t donePages,
                                          std::size_t totalPages)>;

class Scanner {
public:
    std::vector<ScanResult> firstScan(HANDLE process,
                                        const ScanConfig& cfg,
                                        const std::atomic<bool>& cancel,
                                        ScanProgress prog = {});

    std::vector<ScanResult> nextScan(HANDLE process,
                                       const std::vector<ScanResult>& prev,
                                       const ScanConfig& cfg,
                                       const std::atomic<bool>& cancel,
                                       ScanProgress prog = {});
};

} // namespace cheat
