// app.h -- Win32 window + DX11 swapchain + ImGui docking host.
#pragma once

#include "common.h"
#include "engine.h"

#include "imgui.h"

#include <d3d11.h>

namespace cheat::ui {

struct UiState {
    std::vector<std::string> log;
    std::size_t              logLimit{4096};

    // Attach dialog
    bool    showAttach{false};
    std::vector<ProcEntry> procs;
    char    procFilter[64]{};
    int     procSelected{-1};

    // Scan panel
    int     scanType{4};       // index into ValueType enum (I32 default)
    int     scanOp{1};         // Exact
    char    scanValue[128]  = "0";
    char    scanValue2[128] = "0";
    int     scanAlignment{0};  // 0 = type size default
    bool    fastScan{true};
    bool    writableOnly{true};

    // Results pagination
    std::size_t resultOffset{0};
    std::size_t resultPage{200};
    std::vector<ScanResult> resultCache;

    // Bookmark editor
    BookmarkId  selectedBookmark{};
    char        bookmarkName[64] = "new entry";
    // Per-bookmark edit buffers keyed by id, lazily populated
    std::map<BookmarkId, std::array<char, 128>> bmValueBuf;

    // Memory viewer
    char    memAddrBuf[32] = "0";
    Bytes   memBuf;
    Address memAddr{};
    int     memRows{16};

    // Pointer scanner dialog
    bool    showPtrScan{false};
    char    ptrTarget[32] = "0x";
    int     ptrDepth{3};
    int     ptrMaxOff{0x1000};
};

class App {
public:
    bool init(HINSTANCE hInst);
    void run();
    void shutdown();

    void bind(Engine* eng) { m_eng = eng; }

private:
    void frame();
    void present();
    void makeRenderTarget();
    void dropRenderTarget();
    void buildDefaultDockLayoutIfFirstRun(ImGuiID dockId);

    static LRESULT CALLBACK wndProcThunk(HWND, UINT, WPARAM, LPARAM);
    LRESULT handleMessage(HWND, UINT, WPARAM, LPARAM);

    HWND                    m_hwnd{};
    ID3D11Device*           m_dev{};
    ID3D11DeviceContext*    m_ctx{};
    IDXGISwapChain*         m_sc{};
    ID3D11RenderTargetView* m_rtv{};

    bool      m_resizePending{};
    UINT      m_resizeW{}, m_resizeH{};
    bool      m_dockLayoutBuilt{false};

    Engine*  m_eng{};
    UiState  m_ui;
};

} // namespace cheat::ui
