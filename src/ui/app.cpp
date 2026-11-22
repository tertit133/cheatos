#include "app.h"

#include "theme.h"
#include "views.h"

#include "imgui.h"
#include "imgui_internal.h"   // DockBuilder* lives here
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <dxgi.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace cheat::ui {

static App* g_app = nullptr;

LRESULT CALLBACK App::wndProcThunk(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (g_app) return g_app->handleMessage(h, m, w, l);
    return ::DefWindowProcW(h, m, w, l);
}

LRESULT App::handleMessage(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return 0;
    switch (m) {
        case WM_SIZE:
            if (m_dev && w != SIZE_MINIMIZED) {
                m_resizeW = LOWORD(l);
                m_resizeH = HIWORD(l);
                m_resizePending = true;
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((w & 0xFFF0) == SC_KEYMENU) return 0;
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProcW(h, m, w, l);
}

bool App::init(HINSTANCE hInst) {
    g_app = this;

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = wndProcThunk;
    wc.hInstance     = hInst;
    wc.hCursor       = ::LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW);
    wc.lpszClassName = L"CheatOsWindow";
    ::RegisterClassExW(&wc);

    m_hwnd = ::CreateWindowExW(
        0, wc.lpszClassName, L"CheatOS -- memory scanner / trainer",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1400, 900,
        nullptr, nullptr, hInst, nullptr);
    if (!m_hwnd) return false;

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount       = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow      = m_hwnd;
    sd.SampleDesc.Count  = 1;
    sd.Windowed          = TRUE;
    sd.SwapEffect        = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    D3D_FEATURE_LEVEL fl;
    HRESULT hr = ::D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &sd, &m_sc,
        &m_dev, &fl, &m_ctx);
    if (FAILED(hr)) return false;
    makeRenderTarget();

    ::ShowWindow(m_hwnd, SW_SHOW);
    ::UpdateWindow(m_hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard
                                 | ImGuiConfigFlags_DockingEnable;
    applyGrayTheme();
    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplDX11_Init(m_dev, m_ctx);
    return true;
}

void App::makeRenderTarget() {
    ID3D11Texture2D* bb = nullptr;
    m_sc->GetBuffer(0, IID_PPV_ARGS(&bb));
    if (bb) {
        m_dev->CreateRenderTargetView(bb, nullptr, &m_rtv);
        bb->Release();
    }
}

void App::dropRenderTarget() {
    if (m_rtv) { m_rtv->Release(); m_rtv = nullptr; }
}

void App::run() {
    MSG msg{};
    bool done = false;
    while (!done) {
        while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        if (m_resizePending) {
            dropRenderTarget();
            m_sc->ResizeBuffers(0, m_resizeW, m_resizeH, DXGI_FORMAT_UNKNOWN, 0);
            makeRenderTarget();
            m_resizePending = false;
        }
        frame();
        present();
    }
}

void App::frame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (m_eng) {
        auto lines = m_eng->drainLog();
        for (auto& ln : lines) m_ui.log.push_back(std::move(ln));
        if (m_ui.log.size() > m_ui.logLimit) {
            m_ui.log.erase(m_ui.log.begin(),
                            m_ui.log.begin() + (m_ui.log.size() - m_ui.logLimit));
        }
    }

    ImGuiID dockId = ImGui::DockSpaceOverViewport(
        ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    buildDefaultDockLayoutIfFirstRun(dockId);

    drawControlBar (m_ui, *m_eng);
    drawScanPanel  (m_ui, *m_eng);
    drawResults    (m_ui, *m_eng);
    drawBookmarks  (m_ui, *m_eng);
    drawMemory     (m_ui, *m_eng);
    drawPointerPane(m_ui, *m_eng);
    drawLog        (m_ui);

    drawAttachDialog   (m_ui, *m_eng);
    drawPointerScanDialog(m_ui, *m_eng);

    ImGui::Render();
}

void App::buildDefaultDockLayoutIfFirstRun(ImGuiID dockId) {
    if (m_dockLayoutBuilt) return;
    m_dockLayoutBuilt = true;

    if (ImGui::DockBuilderGetNode(dockId) &&
        ImGui::DockBuilderGetNode(dockId)->ChildNodes[0]) {
        return;
    }
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::DockBuilderRemoveNode(dockId);
    ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockId, vp->WorkSize);

    ImGuiID top    = ImGui::DockBuilderSplitNode(dockId, ImGuiDir_Up,   0.06f, nullptr, &dockId);
    ImGuiID left   = ImGui::DockBuilderSplitNode(dockId, ImGuiDir_Left, 0.35f, nullptr, &dockId);
    ImGuiID right  = dockId;
    ImGuiID bottom = ImGui::DockBuilderSplitNode(right,  ImGuiDir_Down, 0.30f, nullptr, &right);

    ImGui::DockBuilderDockWindow("Control",        top);
    ImGui::DockBuilderDockWindow("Scan",           left);
    ImGui::DockBuilderDockWindow("Results",        left);
    ImGui::DockBuilderDockWindow("Bookmarks",      right);
    ImGui::DockBuilderDockWindow("Memory",         right);
    ImGui::DockBuilderDockWindow("Pointer paths",  right);
    ImGui::DockBuilderDockWindow("Log",            bottom);

    ImGui::DockBuilderFinish(dockId);
}

void App::present() {
    const float clearColor[4]{ 0.08f, 0.08f, 0.09f, 1.0f };
    m_ctx->OMSetRenderTargets(1, &m_rtv, nullptr);
    m_ctx->ClearRenderTargetView(m_rtv, clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    m_sc->Present(1, 0);
}

void App::shutdown() {
    if (m_eng) m_eng->detach();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    dropRenderTarget();
    if (m_sc)  { m_sc->Release();  m_sc  = nullptr; }
    if (m_ctx) { m_ctx->Release(); m_ctx = nullptr; }
    if (m_dev) { m_dev->Release(); m_dev = nullptr; }
    if (m_hwnd) { ::DestroyWindow(m_hwnd); m_hwnd = nullptr; }
    g_app = nullptr;
}

} // namespace cheat::ui
