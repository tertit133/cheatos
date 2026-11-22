// proc_attach.h -- process enumeration + open-for-full-RW.
#pragma once

#include "common.h"

namespace cheat {

struct ProcEntry {
    DWORD        pid{};
    std::wstring name;
};

std::vector<ProcEntry> enumerateProcesses();

// OpenProcess with the access rights the scanner / writer need. Returns
// nullptr on failure. Caller owns the handle.
HANDLE openForFullAccess(DWORD pid);

// Full path to the executable file on disk, derived from the process
// handle via QueryFullProcessImageNameW. Empty on failure.
std::wstring processImagePath(HANDLE process);

} // namespace cheat
