#include "engine.h"

namespace cheat {

Engine::Engine() {
    m_worker = std::thread(&Engine::workerLoop, this);
}

Engine::~Engine() {
    m_workerQuit = true;
    m_taskCv.notify_all();
    if (m_worker.joinable()) m_worker.join();
    detach();
}

// ---------------------------------------------------------------------------
// Process lifecycle
// ---------------------------------------------------------------------------
bool Engine::attach(DWORD pid) {
    detach();
    HANDLE h = openForFullAccess(pid);
    if (!h) { pushLog(sfmt("[!] OpenProcess failed (pid=%lu)", pid)); return false; }
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_process = h;
        m_pid     = pid;
    }
    pushLog(sfmt("[+] attached to pid=%lu", pid));
    return true;
}

void Engine::detach() {
    freezerStop();
    cancelScan();

    HANDLE h = nullptr;
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        h = m_process;
        m_process = nullptr;
        m_pid = 0;
        m_results.clear();
        m_ptrPaths.clear();
    }
    if (h) ::CloseHandle(h);
}

std::wstring Engine::imagePath() {
    std::lock_guard<std::mutex> lk(m_mtx);
    return m_process ? processImagePath(m_process) : std::wstring{};
}

// ---------------------------------------------------------------------------
// Scanning
// ---------------------------------------------------------------------------
void Engine::startFirstScan(ScanConfig cfg) {
    if (!m_process || m_scanning.load()) return;
    m_scanCancel = false;
    m_scanning   = true;
    m_scanProgress = 0;

    enqueue([this, cfg = std::move(cfg)]() {
        auto results = m_scanner.firstScan(
            m_process, cfg, m_scanCancel,
            [this](std::size_t d, std::size_t t) {
                m_scanProgress = t ? float(d) / float(t) : 1.0f;
            });
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            m_results = std::move(results);
        }
        pushLog(sfmt("[*] first scan complete: %zu hits",
                       m_results.size()));
        m_scanning = false;
    });
}

void Engine::startNextScan(ScanConfig cfg) {
    if (!m_process || m_scanning.load()) return;
    m_scanCancel = false;
    m_scanning   = true;
    m_scanProgress = 0;

    enqueue([this, cfg = std::move(cfg)]() {
        std::vector<ScanResult> prev;
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            prev = m_results;
        }
        auto results = m_scanner.nextScan(
            m_process, prev, cfg, m_scanCancel,
            [this](std::size_t d, std::size_t t) {
                m_scanProgress = t ? float(d) / float(t) : 1.0f;
            });
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            m_results = std::move(results);
        }
        pushLog(sfmt("[*] next scan: %zu -> %zu",
                       prev.size(), m_results.size()));
        m_scanning = false;
    });
}

void Engine::cancelScan() {
    m_scanCancel = true;
}

void Engine::resetScan() {
    cancelScan();
    std::lock_guard<std::mutex> lk(m_mtx);
    m_results.clear();
}

std::size_t Engine::resultCount() {
    std::lock_guard<std::mutex> lk(m_mtx);
    return m_results.size();
}

std::vector<ScanResult> Engine::resultsSlice(std::size_t offset,
                                               std::size_t count) {
    std::lock_guard<std::mutex> lk(m_mtx);
    if (offset >= m_results.size()) return {};
    const std::size_t end = std::min(m_results.size(), offset + count);
    return { m_results.begin() + offset, m_results.begin() + end };
}

// ---------------------------------------------------------------------------
// Bookmarks
// ---------------------------------------------------------------------------
BookmarkId Engine::addBookmark(Bookmark b) {
    std::lock_guard<std::mutex> lk(m_mtx);
    return m_bookmarks.add(std::move(b));
}

bool Engine::removeBookmark(BookmarkId id) {
    std::lock_guard<std::mutex> lk(m_mtx);
    return m_bookmarks.remove(id);
}

bool Engine::setBookmarkFreeze(BookmarkId id, bool frozen) {
    std::lock_guard<std::mutex> lk(m_mtx);
    auto* b = m_bookmarks.find(id);
    if (!b) return false;
    b->frozen = frozen;
    if (frozen && b->frozenValue.bytes.empty()) {
        b->frozenValue = TypedValue::zero(b->type);
    }
    return true;
}

bool Engine::setBookmarkValue(BookmarkId id, const TypedValue& v) {
    std::lock_guard<std::mutex> lk(m_mtx);
    auto* b = m_bookmarks.find(id);
    if (!b) return false;
    b->frozenValue = v;
    // Also push the value into the target now so the change is visible
    // immediately; the freezer takes over on the next tick.
    if (m_process && b->address) {
        SIZE_T w = 0;
        ::WriteProcessMemory(m_process,
                              reinterpret_cast<LPVOID>(b->address),
                              v.bytes.data(), v.bytes.size(), &w);
    }
    return true;
}

bool Engine::setBookmarkName(BookmarkId id, std::string name) {
    std::lock_guard<std::mutex> lk(m_mtx);
    auto* b = m_bookmarks.find(id);
    if (!b) return false;
    b->name = std::move(name);
    return true;
}

bool Engine::setBookmarkType(BookmarkId id, ValueType t) {
    std::lock_guard<std::mutex> lk(m_mtx);
    auto* b = m_bookmarks.find(id);
    if (!b) return false;
    b->type = t;
    b->frozenValue = TypedValue::zero(t);
    return true;
}

std::vector<Bookmark> Engine::bookmarks() {
    std::lock_guard<std::mutex> lk(m_mtx);
    return m_bookmarks.list();
}

TypedValue Engine::readBookmarkValue(const Bookmark& b) {
    TypedValue v = TypedValue::zero(b.type);
    if (!m_process) return v;

    Address addr = b.address;
    if (!b.pointerOffsets.empty()) {
        addr = resolvePointerPath(b);
        if (!addr) return v;
    }

    std::size_t sz = fixedSize(b.type);
    if (!sz) return v;
    v.bytes.assign(sz, 0);
    SIZE_T got = 0;
    ::ReadProcessMemory(m_process, reinterpret_cast<LPCVOID>(addr),
                         v.bytes.data(), sz, &got);
    return v;
}

Address Engine::resolvePointerPath(const Bookmark& b) {
    Address cur = b.pointerBase;
    for (std::size_t i = 0; i < b.pointerOffsets.size(); ++i) {
        Address next = 0;
        SIZE_T got = 0;
        if (!::ReadProcessMemory(m_process,
                                   reinterpret_cast<LPCVOID>(cur),
                                   &next, sizeof(next), &got) || got != sizeof(next)) {
            return 0;
        }
        cur = next + b.pointerOffsets[i];
    }
    return cur;
}

// ---------------------------------------------------------------------------
// Memory
// ---------------------------------------------------------------------------
bool Engine::readMemory(Address a, void* buf, std::size_t n) {
    HANDLE p;
    { std::lock_guard<std::mutex> lk(m_mtx); p = m_process; }
    if (!p) return false;
    SIZE_T got = 0;
    return ::ReadProcessMemory(p, reinterpret_cast<LPCVOID>(a), buf, n, &got)
           && got == n;
}

bool Engine::writeMemory(Address a, const void* buf, std::size_t n) {
    HANDLE p;
    { std::lock_guard<std::mutex> lk(m_mtx); p = m_process; }
    if (!p) return false;
    DWORD oldProt = 0;
    ::VirtualProtectEx(p, reinterpret_cast<LPVOID>(a), n,
                        PAGE_READWRITE, &oldProt);
    SIZE_T w = 0;
    BOOL ok = ::WriteProcessMemory(p, reinterpret_cast<LPVOID>(a), buf, n, &w);
    if (oldProt) {
        DWORD tmp = 0;
        ::VirtualProtectEx(p, reinterpret_cast<LPVOID>(a), n, oldProt, &tmp);
    }
    return ok && w == n;
}

// ---------------------------------------------------------------------------
// Freezer
// ---------------------------------------------------------------------------
void Engine::freezerStart() {
    if (!m_process) return;
    m_freezer.start(m_process, [this]{ return bookmarks(); });
    pushLog("[*] freezer started");
}

void Engine::freezerStop() {
    m_freezer.stop();
}

// ---------------------------------------------------------------------------
// Pointer scanner
// ---------------------------------------------------------------------------
void Engine::startPointerScan(Address target, PointerScanConfig cfg) {
    if (!m_process || m_ptrScanning.load()) return;
    m_scanCancel   = false;
    m_ptrScanning  = true;
    m_ptrProgress  = 0;

    enqueue([this, target, cfg]() {
        auto paths = m_ptrScanner.find(
            m_process, target, cfg, m_scanCancel,
            [this](std::size_t d, std::size_t t) {
                m_ptrProgress = t ? float(d) / float(t) : 1.0f;
            });
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            m_ptrPaths = std::move(paths);
        }
        pushLog(sfmt("[*] pointer scan: %zu paths", m_ptrPaths.size()));
        m_ptrScanning = false;
    });
}

std::vector<PointerPath> Engine::pointerPaths() {
    std::lock_guard<std::mutex> lk(m_mtx);
    return m_ptrPaths;
}

// ---------------------------------------------------------------------------
// CE table I/O
// ---------------------------------------------------------------------------
bool Engine::loadTable(const std::wstring& path) {
    auto items = loadCheatTable(path);
    if (!items) { pushLog("[!] CE table load failed"); return false; }
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_bookmarks.clear();
        for (auto& b : *items) m_bookmarks.add(std::move(b));
    }
    pushLog(sfmt("[+] loaded %zu entries from CT", items->size()));
    return true;
}

bool Engine::saveTable(const std::wstring& path) {
    auto bms = bookmarks();
    if (!saveCheatTable(path, bms)) { pushLog("[!] CE table save failed"); return false; }
    pushLog(sfmt("[+] saved %zu entries to CT", bms.size()));
    return true;
}

// ---------------------------------------------------------------------------
// Log + task queue
// ---------------------------------------------------------------------------
std::vector<std::string> Engine::drainLog() {
    std::lock_guard<std::mutex> lk(m_logMtx);
    auto v = std::move(m_log);
    m_log.clear();
    return v;
}

void Engine::pushLog(std::string s) {
    std::lock_guard<std::mutex> lk(m_logMtx);
    m_log.push_back(std::move(s));
}

void Engine::enqueue(std::function<void()> fn) {
    {
        std::lock_guard<std::mutex> lk(m_taskMtx);
        m_tasks.push(std::move(fn));
    }
    m_taskCv.notify_one();
}

void Engine::workerLoop() {
    while (!m_workerQuit.load()) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lk(m_taskMtx);
            m_taskCv.wait(lk, [this]{ return !m_tasks.empty() || m_workerQuit.load(); });
            if (m_workerQuit.load() && m_tasks.empty()) break;
            task = std::move(m_tasks.front());
            m_tasks.pop();
        }
        try { task(); } catch (...) { pushLog("[!] task threw"); }
    }
}

} // namespace cheat
