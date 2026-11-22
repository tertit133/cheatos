#include "freezer.h"

namespace cheat {

Freezer::~Freezer() { stop(); }

void Freezer::start(HANDLE process, Provider provider) {
    stop();
    if (!process || !provider) return;
    m_process  = process;
    m_provider = std::move(provider);
    m_running  = true;
    m_thread   = std::thread(&Freezer::loop, this);
}

void Freezer::stop() {
    m_running = false;
    if (m_thread.joinable()) m_thread.join();
    m_process = nullptr;
}

void Freezer::loop() {
    while (m_running.load()) {
        const auto bms = m_provider ? m_provider() : std::vector<Bookmark>{};

        for (const auto& b : bms) {
            if (!b.frozen || b.frozenValue.bytes.empty()) continue;

            // Un-protect the target page just enough to write, then restore.
            // Write-only via VirtualProtectEx: if the page isn't writable
            // we can still succeed, otherwise WriteProcessMemory fails.
            DWORD oldProt = 0;
            ::VirtualProtectEx(m_process,
                                reinterpret_cast<LPVOID>(b.address),
                                b.frozenValue.bytes.size(),
                                PAGE_READWRITE, &oldProt);
            SIZE_T w = 0;
            ::WriteProcessMemory(m_process,
                                  reinterpret_cast<LPVOID>(b.address),
                                  b.frozenValue.bytes.data(),
                                  b.frozenValue.bytes.size(), &w);
            if (oldProt) {
                DWORD tmp = 0;
                ::VirtualProtectEx(m_process,
                                    reinterpret_cast<LPVOID>(b.address),
                                    b.frozenValue.bytes.size(),
                                    oldProt, &tmp);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(m_tickMs.load()));
    }
}

} // namespace cheat
