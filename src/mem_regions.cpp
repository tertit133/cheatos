#include "mem_regions.h"

namespace cheat {

std::vector<Region> enumerateRegions(HANDLE process, bool writableOnly) {
    std::vector<Region> out;
    if (!process) return out;

    SYSTEM_INFO si{};
    ::GetSystemInfo(&si);

    auto addr    = reinterpret_cast<Byte*>(si.lpMinimumApplicationAddress);
    auto maxAddr = reinterpret_cast<Byte*>(si.lpMaximumApplicationAddress);

    MEMORY_BASIC_INFORMATION mbi{};
    while (addr < maxAddr) {
        if (::VirtualQueryEx(process, addr, &mbi, sizeof(mbi)) != sizeof(mbi)) {
            break;
        }

        const bool committed = (mbi.State == MEM_COMMIT);
        const bool wantable  = writableOnly ? protectWritable(mbi.Protect)
                                             : protectReadable(mbi.Protect);
        if (committed && wantable) {
            Region r{};
            r.base    = reinterpret_cast<Address>(mbi.BaseAddress);
            r.size    = mbi.RegionSize;
            r.protect = mbi.Protect;
            r.type    = mbi.Type;
            out.push_back(r);
        }

        addr += mbi.RegionSize;
    }
    return out;
}

} // namespace cheat
