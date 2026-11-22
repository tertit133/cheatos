#include "proc_attach.h"

#include <tlhelp32.h>

namespace cheat {

std::vector<ProcEntry> enumerateProcesses() {
    std::vector<ProcEntry> out;
    HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return out;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (::Process32FirstW(snap, &pe)) {
        do {
            out.push_back({ pe.th32ProcessID, pe.szExeFile });
        } while (::Process32NextW(snap, &pe));
    }
    ::CloseHandle(snap);
    return out;
}

HANDLE openForFullAccess(DWORD pid) {
    // Minimum set for scan + write + VirtualProtect + VirtualQuery.
    constexpr DWORD kAccess =
        PROCESS_VM_READ |
        PROCESS_VM_WRITE |
        PROCESS_VM_OPERATION |
        PROCESS_QUERY_INFORMATION;
    return ::OpenProcess(kAccess, FALSE, pid);
}

std::wstring processImagePath(HANDLE process) {
    wchar_t buf[MAX_PATH]{};
    DWORD n = MAX_PATH;
    if (!::QueryFullProcessImageNameW(process, 0, buf, &n)) return {};
    return std::wstring(buf, n);
}

} // namespace cheat
