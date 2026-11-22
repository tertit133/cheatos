#include "scanner.h"

#include <future>

namespace cheat {

namespace {

std::size_t effectiveStride(const ScanConfig& cfg) {
    if (!cfg.fastScan) return 1;
    if (cfg.alignment > 0) return cfg.alignment;
    std::size_t sz = fixedSize(cfg.type);
    return sz ? sz : 1;
}

std::size_t itemSize(const ScanConfig& cfg) {
    std::size_t sz = fixedSize(cfg.type);
    if (sz) return sz;
    return cfg.operand.bytes.size();
}

// Scan one contiguous page / chunk in memory. Appends matches to `out`.
void scanChunk(const ScanConfig& cfg,
               Address base,
               const Byte* buf, std::size_t len,
               std::vector<ScanResult>& out) {
    const std::size_t stride = effectiveStride(cfg);
    const std::size_t sz     = itemSize(cfg);
    if (!sz || len < sz) return;

    const std::size_t limit = len - sz;
    for (std::size_t off = 0; off <= limit; off += stride) {
        const Byte* here = buf + off;
        if (cfg.operand.matches(cfg.op, here, len - off,
                                  /*prev*/ nullptr,
                                  cfg.op == ScanOp::Between ? &cfg.operand2 : nullptr)) {
            ScanResult r{};
            r.address = base + off;
            r.value.assign(here, here + sz);
            out.push_back(std::move(r));
        }
    }
}

} // anonymous

std::vector<ScanResult> Scanner::firstScan(HANDLE process,
                                             const ScanConfig& cfg,
                                             const std::atomic<bool>& cancel,
                                             ScanProgress prog) {
    std::vector<ScanResult> out;
    if (!process) return out;

    auto regions = enumerateRegions(process, cfg.writableOnly);
    if (regions.empty()) return out;

    // Chunk every region into <=4 MiB slices so we don't allocate huge
    // buffers and so we can honour cancellation promptly.
    constexpr std::size_t kChunk = 4 * 1024 * 1024;

    std::size_t totalPages = 0;
    for (const auto& r : regions) totalPages += (r.size + kChunk - 1) / kChunk;

    Bytes buf;
    buf.reserve(kChunk);

    std::size_t done = 0;
    for (const auto& reg : regions) {
        Address cursor = reg.base;
        std::size_t remain = reg.size;
        while (remain > 0) {
            if (cancel.load()) { if (prog) prog(done, totalPages); return out; }
            std::size_t take = std::min(remain, kChunk);
            buf.resize(take);
            SIZE_T got = 0;
            BOOL ok = ::ReadProcessMemory(process,
                                           reinterpret_cast<LPCVOID>(cursor),
                                           buf.data(), take, &got);
            if (ok && got >= itemSize(cfg)) {
                scanChunk(cfg, cursor, buf.data(), got, out);
            }
            cursor += take;
            remain -= take;
            ++done;
            if (prog && (done % 16 == 0)) prog(done, totalPages);
        }
    }
    if (prog) prog(totalPages, totalPages);
    return out;
}

std::vector<ScanResult> Scanner::nextScan(HANDLE process,
                                            const std::vector<ScanResult>& prev,
                                            const ScanConfig& cfg,
                                            const std::atomic<bool>& cancel,
                                            ScanProgress prog) {
    std::vector<ScanResult> out;
    if (!process) return out;

    out.reserve(prev.size() / 2);

    const std::size_t sz = itemSize(cfg);
    if (!sz) return out;

    std::size_t done = 0;
    const std::size_t total = prev.size();

    Byte here[32]{};  // enough for fixed scalars
    Bytes heapBuf;
    const std::size_t stackCap = sizeof(here);

    for (const auto& e : prev) {
        if (cancel.load()) { if (prog) prog(done, total); return out; }

        Byte* dst = (sz <= stackCap) ? here : (heapBuf.resize(sz), heapBuf.data());
        SIZE_T got = 0;
        BOOL ok = ::ReadProcessMemory(process,
                                       reinterpret_cast<LPCVOID>(e.address),
                                       dst, sz, &got);
        if (!ok || got != sz) {
            ++done;
            continue;
        }

        const Byte* prevBytes = e.value.size() == sz ? e.value.data() : nullptr;
        if (cfg.operand.matches(cfg.op, dst, sz, prevBytes,
                                  cfg.op == ScanOp::Between ? &cfg.operand2 : nullptr)) {
            ScanResult r{};
            r.address = e.address;
            r.value.assign(dst, dst + sz);
            out.push_back(std::move(r));
        }
        ++done;
        if (prog && (done % 1024 == 0)) prog(done, total);
    }
    if (prog) prog(total, total);
    return out;
}

} // namespace cheat
