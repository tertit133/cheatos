// engine.h -- the CheatOS orchestrator: process handle, scanner,
// bookmark list, freezer, pointer scanner. Mirrors the shape of
// MiniDbg's Debugger so the UI thread never touches a Win32 API
// directly; everything flows through this class.
#pragma once

#include "bookmarks.h"
#include "ce_table.h"
#include "common.h"
#include "freezer.h"
#include "pointer_scan.h"
#include "proc_attach.h"
#include "scanner.h"
#include "value_types.h"

namespace cheat {

class Engine {
public:
    Engine();
    ~Engine();

    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

    // ---- process lifecycle ------------------------------------------------
    bool         attach(DWORD pid);
    void         detach();
    bool         isAttached() const { return m_process != nullptr; }
    DWORD        pid() const        { return m_pid; }
    std::wstring imagePath();

    // ---- scanning ---------------------------------------------------------
    void        startFirstScan(ScanConfig cfg);
    void        startNextScan(ScanConfig cfg);
    void        cancelScan();
    void        resetScan();
    bool        scanning() const      { return m_scanning.load(); }
    float       scanProgress() const  { return m_scanProgress.load(); }
    std::size_t resultCount();
    std::vector<ScanResult> resultsSlice(std::size_t offset, std::size_t count);

    // ---- bookmarks --------------------------------------------------------
    BookmarkId  addBookmark(Bookmark b);
    bool        removeBookmark(BookmarkId id);
    bool        setBookmarkFreeze(BookmarkId id, bool frozen);
    bool        setBookmarkValue(BookmarkId id, const TypedValue& v);
    bool        setBookmarkName(BookmarkId id, std::string name);
    bool        setBookmarkType(BookmarkId id, ValueType t);
    std::vector<Bookmark> bookmarks();

    // Live read of a bookmark's value (resolves pointer chain if present).
    TypedValue  readBookmarkValue(const Bookmark& b);

    // ---- memory -----------------------------------------------------------
    bool readMemory (Address a, void* buf, std::size_t n);
    bool writeMemory(Address a, const void* buf, std::size_t n);

    // ---- freezer ----------------------------------------------------------
    void freezerStart();
    void freezerStop();
    bool freezerRunning() const { return m_freezer.running(); }

    // ---- pointer scanner --------------------------------------------------
    void startPointerScan(Address target, PointerScanConfig cfg);
    void cancelPointerScan() { m_scanCancel = true; }
    bool pointerScanning() const    { return m_ptrScanning.load(); }
    float pointerScanProgress() const { return m_ptrProgress.load(); }
    std::vector<PointerPath> pointerPaths();

    // ---- CE table I/O -----------------------------------------------------
    bool loadTable(const std::wstring& path);
    bool saveTable(const std::wstring& path);

    // ---- log --------------------------------------------------------------
    std::vector<std::string> drainLog();

private:
    void workerLoop();
    void enqueue(std::function<void()> fn);
    void pushLog(std::string s);

    Address resolvePointerPath(const Bookmark& b);

private:
    HANDLE  m_process{};
    DWORD   m_pid{};

    mutable std::mutex       m_mtx;
    Scanner                  m_scanner;
    PointerScanner           m_ptrScanner;
    std::vector<ScanResult>  m_results;
    std::vector<PointerPath> m_ptrPaths;
    BookmarkList             m_bookmarks;
    Freezer                  m_freezer;

    std::atomic<bool>  m_scanning{false};
    std::atomic<bool>  m_ptrScanning{false};
    std::atomic<bool>  m_scanCancel{false};
    std::atomic<float> m_scanProgress{0.0f};
    std::atomic<float> m_ptrProgress{0.0f};

    std::thread                      m_worker;
    std::mutex                       m_taskMtx;
    std::condition_variable          m_taskCv;
    std::queue<std::function<void()>> m_tasks;
    std::atomic<bool>                m_workerQuit{false};

    std::mutex               m_logMtx;
    std::vector<std::string> m_log;
};

} // namespace cheat
