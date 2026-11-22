// views.h -- panel entry points. Each is a top-level ImGui window
// (docking branch), so the user can drag / split / float them.
#pragma once

#include "app.h"

namespace cheat::ui {

void drawControlBar  (UiState& ui, Engine& eng);
void drawScanPanel   (UiState& ui, Engine& eng);
void drawResults     (UiState& ui, Engine& eng);
void drawBookmarks   (UiState& ui, Engine& eng);
void drawMemory      (UiState& ui, Engine& eng);
void drawPointerPane (UiState& ui, Engine& eng);
void drawLog         (UiState& ui);

void drawAttachDialog       (UiState& ui, Engine& eng);
void drawPointerScanDialog  (UiState& ui, Engine& eng);

} // namespace cheat::ui
