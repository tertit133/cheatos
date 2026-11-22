// freezer.h -- background thread that rewrites "frozen" bookmark values.
//
// The freezer wakes up every ~16 ms (a video-frame-ish cadence), walks
// the current bookmark list under a shared mutex, and for each frozen
// entry writes its pinned bytes back into the target process. That rate
// is fast enough that normal in-game logic that tries to decrement a
// value (hp, ammo, cooldown) can't win against it.
#pragma once

#include "bookmarks.h"
#include "common.h"

namespace cheat {

class Freezer {
public:
    Freezer() = default;
    ~Freezer();

    Freezer(const Freezer&) = delete;
    Freezer& operator=(const Freezer&) = delete;

    // The freezer reads bookmarks through a provider callback so it does
    // not assume ownership of the list. The callback must be thread-safe
    // (typically it locks the Engine's state mutex and copies the list).
    using Provider = std::function<std::vector<Bookmark>()>;

    void start(HANDLE process, Provider provider);
    void stop();

    bool running() const { return m_running.load(); }

    // Tick period in milliseconds. Default 16 ms (about 60 Hz).
    void setTickMs(unsigned ms) { m_tickMs.store(ms ? ms : 1); }

private:
    void loop();

    HANDLE                m_process{};
    Provider              m_provider;
    std::thread           m_thread;
    std::atomic<bool>     m_running{false};
    std::atomic<unsigned> m_tickMs{16};
};

} // namespace cheat
