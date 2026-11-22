#include "pointer_scan.h"

#include <algorithm>

namespace cheat {

namespace {

// Compact "a pointer value P was found at source slot S" record. Sorted
// by `dest` so a range lookup [T - off, T] is a single lower_bound.
struct PtrHit {
    Address dest{};     // the value we read at `src`
    Address src{};      // where that pointer lives in memory
    bool    srcIsImage{}; // src came from a MEM_IMAGE region (module)
};

// Build the hit table by walking every readable region and interpreting
// each aligned 8-byte slot as a pointer. Only slots that land inside
// another readable region are kept -- a pointer that points to nothing
// mapped is not useful for a chain.
std::vector<PtrHit> buildPtrMap(HANDLE process,
                                 const std::vector<Region>& regions,
                                 const std::atomic<bool>& cancel,
                                 PointerProgress prog) {
    std::vector<PtrHit> hits;
    hits.reserve(1 << 20);

    constexpr std::size_t kChunk = 4 * 1024 * 1024;

    // Precompute a sorted (base, end) list of valid destinations so a
    // pointer value can be checked with binary search.
    std::vector<std::pair<Address, Address>> readable;
    readable.reserve(regions.size());
    for (const auto& r : regions) {
        readable.emplace_back(r.base, r.base + r.size);
    }
    std::sort(readable.begin(), readable.end());

    auto isMapped = [&](Address a) {
        auto it = std::upper_bound(readable.begin(), readable.end(),
                                     std::make_pair(a, Address{0}));
        if (it == readable.begin()) return false;
        --it;
        return a >= it->first && a < it->second;
    };

    std::size_t totalPages = 0;
    for (const auto& r : regions) totalPages += (r.size + kChunk - 1) / kChunk;

    Bytes buf;
    buf.reserve(kChunk);

    std::size_t done = 0;
    for (const auto& reg : regions) {
        const bool srcIsImage = (reg.type == MEM_IMAGE);

        Address cursor = reg.base;
        std::size_t remain = reg.size;
        while (remain > 0) {
            if (cancel.load()) return hits;
            std::size_t take = std::min(remain, kChunk);
            buf.resize(take);
            SIZE_T got = 0;
            if (::ReadProcessMemory(process,
                                     reinterpret_cast<LPCVOID>(cursor),
                                     buf.data(), take, &got) && got >= 8) {
                const std::size_t aligned = got & ~std::size_t{7};
                for (std::size_t off = 0; off + 8 <= aligned; off += 8) {
                    Address val = 0;
                    std::memcpy(&val, buf.data() + off, 8);
                    if (val < 0x10000 || val > 0x7FFFFFFFFFFF) continue;
                    if (!isMapped(val)) continue;
                    PtrHit h{};
                    h.dest       = val;
                    h.src        = cursor + off;
                    h.srcIsImage = srcIsImage;
                    hits.push_back(h);
                }
            }
            cursor += take;
            remain -= take;
            ++done;
            if (prog && (done % 16 == 0)) prog(done, totalPages);
        }
    }
    if (prog) prog(totalPages, totalPages);
    std::sort(hits.begin(), hits.end(),
               [](const PtrHit& a, const PtrHit& b){ return a.dest < b.dest; });
    return hits;
}

// All hits whose `dest` lies in [lo, hi].
std::pair<std::size_t, std::size_t> hitsInRange(const std::vector<PtrHit>& hits,
                                                  Address lo, Address hi) {
    auto first = std::lower_bound(hits.begin(), hits.end(), lo,
                                    [](const PtrHit& h, Address v){ return h.dest < v; });
    auto last  = std::upper_bound(hits.begin(), hits.end(), hi,
                                    [](Address v, const PtrHit& h){ return v < h.dest; });
    return { static_cast<std::size_t>(first - hits.begin()),
             static_cast<std::size_t>(last  - hits.begin()) };
}

} // anonymous

std::vector<PointerPath> PointerScanner::find(HANDLE process, Address target,
                                                const PointerScanConfig& cfg,
                                                const std::atomic<bool>& cancel,
                                                PointerProgress prog) {
    std::vector<PointerPath> out;
    if (!process || !target) return out;

    // Build the pointer map once; subsequent BFS iterations are cheap.
    auto regions = enumerateRegions(process, /*writableOnly*/ false);
    auto hits    = buildPtrMap(process, regions, cancel, prog);
    if (hits.empty()) return out;

    struct QItem {
        Address          cur;
        int              depthLeft;
        std::vector<int> offsets;   // innermost first
    };

    std::vector<QItem> frontier;
    frontier.push_back({ target, cfg.maxDepth, {} });

    while (!frontier.empty()) {
        if (cancel.load() || out.size() >= cfg.maxResults) break;

        QItem q = std::move(frontier.back());
        frontier.pop_back();

        Address lo = (q.cur > (Address)cfg.maxOffset) ? q.cur - cfg.maxOffset : 0;
        Address hi = q.cur;
        auto [lb, ub] = hitsInRange(hits, lo, hi);

        for (std::size_t i = lb; i < ub; ++i) {
            if (out.size() >= cfg.maxResults) break;
            const auto& h = hits[i];
            int off = static_cast<int>(q.cur - h.dest);

            // Fresh path = [this off, then the deeper offsets we already had]
            std::vector<int> path;
            path.reserve(q.offsets.size() + 1);
            path.push_back(off);
            path.insert(path.end(), q.offsets.begin(), q.offsets.end());

            if (h.srcIsImage) {
                out.push_back({ h.src, std::move(path) });
            } else if (q.depthLeft > 1) {
                frontier.push_back({ h.src, q.depthLeft - 1, std::move(path) });
            }
        }
    }

    return out;
}

} // namespace cheat
