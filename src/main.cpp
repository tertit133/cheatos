// main.cpp -- CheatOS WinMain
#include "common.h"
#include "engine.h"
#include "ui/app.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    cheat::Engine     eng;
    cheat::ui::App    app;

    if (!app.init(hInst)) {
        ::MessageBoxW(nullptr, L"Failed to initialise UI (D3D11?)",
                       L"CheatOS", MB_OK | MB_ICONERROR);
        return 1;
    }
    app.bind(&eng);
    app.run();
    app.shutdown();
    return 0;
}
