#include "views.h"

#include "imgui.h"

#include <algorithm>
#include <cstring>

namespace cheat::ui {

// RAII pair so every pane's Begin has a matching End even on early return.
namespace {
struct Window {
    bool open;
    Window(const char* name, ImGuiWindowFlags flags = 0) {
        open = ImGui::Begin(name, nullptr, flags);
    }
    ~Window() { ImGui::End(); }
    operator bool() const { return open; }
    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;
};

bool parseHex(const char* s, std::uint64_t& out) {
    if (!s) return false;
    while (*s == ' ') ++s;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    if (!*s) return false;
    char* end = nullptr;
    out = std::strtoull(s, &end, 16);
    return end != s;
}

const char* kTypeItems[] = {
    "int8", "uint8", "int16", "uint16", "int32", "uint32",
    "int64", "uint64", "float", "double", "bytes", "string", "wstring",
};

const char* kOpItems[] = {
    "unknown", "==", "!=", ">", ">=", "<", "<=", "between",
    "changed", "unchanged", "increased", "decreased",
};

ValueType typeFromIndex(int i) {
    switch (i) {
        case 0: return ValueType::I8;   case 1: return ValueType::U8;
        case 2: return ValueType::I16;  case 3: return ValueType::U16;
        case 4: return ValueType::I32;  case 5: return ValueType::U32;
        case 6: return ValueType::I64;  case 7: return ValueType::U64;
        case 8: return ValueType::F32;  case 9: return ValueType::F64;
        case 10: return ValueType::Bytes;
        case 11: return ValueType::AsciiString;
        case 12: return ValueType::WideString;
    }
    return ValueType::I32;
}

ScanOp opFromIndex(int i) {
    switch (i) {
        case 0: return ScanOp::Unknown;
        case 1: return ScanOp::Exact;    case 2: return ScanOp::NotEqual;
        case 3: return ScanOp::Greater;  case 4: return ScanOp::GreaterEq;
        case 5: return ScanOp::Less;     case 6: return ScanOp::LessEq;
        case 7: return ScanOp::Between;
        case 8: return ScanOp::Changed;  case 9: return ScanOp::Unchanged;
        case 10: return ScanOp::Increased; case 11: return ScanOp::Decreased;
    }
    return ScanOp::Exact;
}

bool opNeedsOperand(ScanOp op) {
    switch (op) {
        case ScanOp::Unknown:
        case ScanOp::Changed: case ScanOp::Unchanged:
        case ScanOp::Increased: case ScanOp::Decreased: return false;
        default: return true;
    }
}

bool opIsFirstScanValid(ScanOp op) {
    switch (op) {
        case ScanOp::Changed: case ScanOp::Unchanged:
        case ScanOp::Increased: case ScanOp::Decreased: return false;
        default: return true;
    }
}

} // anonymous

// ---------------------------------------------------------------------------
// control bar
// ---------------------------------------------------------------------------
void drawControlBar(UiState& ui, Engine& eng) {
    Window w("Control");
    if (!w) return;

    if (ImGui::Button("Attach...")) {
        ui.procs = enumerateProcesses();
        std::sort(ui.procs.begin(), ui.procs.end(),
                   [](auto& a, auto& b){ return a.name < b.name; });
        ui.showAttach = true;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!eng.isAttached());
    if (ImGui::Button("Detach")) eng.detach();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(12, 0)); ImGui::SameLine();

    ImGui::BeginDisabled(!eng.isAttached());
    if (eng.freezerRunning()) {
        if (ImGui::Button("Freezer: ON")) eng.freezerStop();
    } else {
        if (ImGui::Button("Freezer: off")) eng.freezerStart();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(12, 0)); ImGui::SameLine();

    ImGui::BeginDisabled(!eng.isAttached());
    if (ImGui::Button("Save .CT")) {
        // Very small inline save dialog -- no Win32 common dialog pulled.
        ImGui::OpenPopup("save_ct");
    }
    ImGui::SameLine();
    if (ImGui::Button("Load .CT")) ImGui::OpenPopup("load_ct");
    ImGui::EndDisabled();

    static char ctPath[260] = "cheat.ct";
    if (ImGui::BeginPopup("save_ct")) {
        ImGui::InputText("path", ctPath, sizeof(ctPath));
        if (ImGui::Button("Save")) { eng.saveTable(utf8_to_wide(ctPath)); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("load_ct")) {
        ImGui::InputText("path", ctPath, sizeof(ctPath));
        if (ImGui::Button("Load")) { eng.loadTable(utf8_to_wide(ctPath)); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(16, 0)); ImGui::SameLine();
    if (eng.isAttached()) {
        ImGui::TextDisabled("pid %lu   results %zu   bookmarks %zu",
                             eng.pid(), eng.resultCount(),
                             eng.bookmarks().size());
    } else {
        ImGui::TextDisabled("not attached");
    }
}

// ---------------------------------------------------------------------------
// scan panel
// ---------------------------------------------------------------------------
void drawScanPanel(UiState& ui, Engine& eng) {
    Window w("Scan");
    if (!w) return;

    ImGui::SetNextItemWidth(130);
    ImGui::Combo("type", &ui.scanType, kTypeItems, IM_ARRAYSIZE(kTypeItems));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110);
    ImGui::Combo("op",   &ui.scanOp,   kOpItems,   IM_ARRAYSIZE(kOpItems));

    const ScanOp     op = opFromIndex(ui.scanOp);
    const ValueType  vt = typeFromIndex(ui.scanType);

    ImGui::SetNextItemWidth(-1);
    if (opNeedsOperand(op)) {
        ImGui::InputText("##v1", ui.scanValue, sizeof(ui.scanValue));
        if (op == ScanOp::Between) {
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##v2", ui.scanValue2, sizeof(ui.scanValue2));
        }
    } else {
        ImGui::TextDisabled("(no operand needed)");
    }

    ImGui::Checkbox("fast scan (aligned)", &ui.fastScan); ImGui::SameLine();
    ImGui::Checkbox("writable only",       &ui.writableOnly);

    const bool scanning  = eng.scanning();
    const bool hasResult = eng.resultCount() > 0;
    const bool firstOk   = opIsFirstScanValid(op);

    ImGui::BeginDisabled(scanning || !eng.isAttached() || !firstOk);
    if (ImGui::Button(hasResult ? "Restart scan" : "First scan")) {
        ScanConfig cfg{};
        cfg.type          = vt;
        cfg.op            = op;
        cfg.fastScan      = ui.fastScan;
        cfg.writableOnly  = ui.writableOnly;
        if (opNeedsOperand(op)) {
            cfg.operand = TypedValue::parse(vt, ui.scanValue);
            if (op == ScanOp::Between) {
                cfg.operand2 = TypedValue::parse(vt, ui.scanValue2);
            }
        } else {
            cfg.operand = TypedValue::zero(vt);
        }
        eng.resetScan();
        eng.startFirstScan(cfg);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(scanning || !hasResult);
    if (ImGui::Button("Next scan")) {
        ScanConfig cfg{};
        cfg.type         = vt;
        cfg.op           = op;
        cfg.fastScan     = ui.fastScan;
        cfg.writableOnly = ui.writableOnly;
        if (opNeedsOperand(op)) {
            cfg.operand = TypedValue::parse(vt, ui.scanValue);
            if (op == ScanOp::Between) {
                cfg.operand2 = TypedValue::parse(vt, ui.scanValue2);
            }
        } else {
            cfg.operand = TypedValue::zero(vt);
        }
        eng.startNextScan(cfg);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!scanning);
    if (ImGui::Button("Cancel")) eng.cancelScan();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(scanning || !hasResult);
    if (ImGui::Button("Reset")) eng.resetScan();
    ImGui::EndDisabled();

    if (scanning) {
        ImGui::ProgressBar(eng.scanProgress(), ImVec2(-1, 0));
    }
}

// ---------------------------------------------------------------------------
// results
// ---------------------------------------------------------------------------
void drawResults(UiState& ui, Engine& eng) {
    Window w("Results");
    if (!w) return;

    std::size_t total = eng.resultCount();
    ImGui::TextDisabled("%zu results", total);
    if (total == 0) return;

    if (ui.resultOffset >= total) ui.resultOffset = 0;
    ui.resultCache = eng.resultsSlice(ui.resultOffset, ui.resultPage);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    int rp = static_cast<int>(ui.resultPage);
    if (ImGui::InputInt("per page", &rp, 50, 200)) {
        if (rp < 10) rp = 10; if (rp > 5000) rp = 5000;
        ui.resultPage = static_cast<std::size_t>(rp);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("<")) {
        if (ui.resultOffset >= ui.resultPage) ui.resultOffset -= ui.resultPage;
        else ui.resultOffset = 0;
    }
    ImGui::SameLine();
    ImGui::Text("%zu .. %zu", ui.resultOffset,
                 std::min(total, ui.resultOffset + ui.resultPage));
    ImGui::SameLine();
    if (ImGui::SmallButton(">")) {
        if (ui.resultOffset + ui.resultPage < total) ui.resultOffset += ui.resultPage;
    }

    ValueType vt = typeFromIndex(ui.scanType);
    if (ImGui::BeginTable("##res", 3,
                           ImGuiTableFlags_BordersInnerV |
                           ImGuiTableFlags_RowBg |
                           ImGuiTableFlags_ScrollY |
                           ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Address", 0, 160);
        ImGui::TableSetupColumn("Value",   ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("",        0, 100);
        ImGui::TableHeadersRow();

        for (const auto& r : ui.resultCache) {
            ImGui::PushID((int)r.address);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("0x%016llX", (unsigned long long)r.address);

            ImGui::TableSetColumnIndex(1);
            TypedValue cur;
            cur.type = vt;
            cur.bytes = r.value;
            ImGui::Text("%s", cur.toString().c_str());

            ImGui::TableSetColumnIndex(2);
            if (ImGui::SmallButton("+ Bookmark")) {
                Bookmark b{};
                b.name = sfmt("entry_%llX",
                                (unsigned long long)r.address);
                b.address = r.address;
                b.type    = vt;
                b.frozenValue = cur;
                eng.addBookmark(std::move(b));
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

// ---------------------------------------------------------------------------
// bookmarks
// ---------------------------------------------------------------------------
void drawBookmarks(UiState& ui, Engine& eng) {
    Window w("Bookmarks");
    if (!w) return;

    auto list = eng.bookmarks();
    if (list.empty()) {
        ImGui::TextDisabled("(no entries -- add some from the Results pane)");
        return;
    }

    if (ImGui::BeginTable("##bm", 6,
                           ImGuiTableFlags_BordersInnerV |
                           ImGuiTableFlags_RowBg |
                           ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Freeze",  0, 50);
        ImGui::TableSetupColumn("Name",    0, 160);
        ImGui::TableSetupColumn("Address", 0, 170);
        ImGui::TableSetupColumn("Type",    0, 70);
        ImGui::TableSetupColumn("Value",   ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("",        0, 70);
        ImGui::TableHeadersRow();

        for (auto& b : list) {
            ImGui::PushID((int)b.id);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            bool fr = b.frozen;
            if (ImGui::Checkbox("##f", &fr)) eng.setBookmarkFreeze(b.id, fr);

            ImGui::TableSetColumnIndex(1);
            char nameBuf[64]{};
            std::strncpy(nameBuf, b.name.c_str(), sizeof(nameBuf) - 1);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##n", nameBuf, sizeof(nameBuf),
                                   ImGuiInputTextFlags_EnterReturnsTrue)) {
                eng.setBookmarkName(b.id, nameBuf);
            }

            ImGui::TableSetColumnIndex(2);
            if (b.pointerOffsets.empty()) {
                ImGui::Text("0x%016llX", (unsigned long long)b.address);
            } else {
                ImGui::Text("[P] 0x%llX", (unsigned long long)b.pointerBase);
            }

            ImGui::TableSetColumnIndex(3);
            int ti = (int)b.type;
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##t", &ti, kTypeItems, IM_ARRAYSIZE(kTypeItems))) {
                eng.setBookmarkType(b.id, typeFromIndex(ti));
            }

            ImGui::TableSetColumnIndex(4);
            TypedValue live = eng.readBookmarkValue(b);
            auto& buf = ui.bmValueBuf[b.id];
            if (!b.frozen) {
                auto s = live.toString();
                std::strncpy(buf.data(), s.c_str(), buf.size() - 1);
                buf.back() = '\0';
            }
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##v", buf.data(), buf.size(),
                                   ImGuiInputTextFlags_EnterReturnsTrue)) {
                TypedValue nv = TypedValue::parse(b.type, buf.data());
                eng.setBookmarkValue(b.id, nv);
            }

            ImGui::TableSetColumnIndex(5);
            if (ImGui::SmallButton("Remove")) eng.removeBookmark(b.id);

            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

// ---------------------------------------------------------------------------
// memory
// ---------------------------------------------------------------------------
void drawMemory(UiState& ui, Engine& eng) {
    Window w("Memory");
    if (!w) return;

    ImGui::SetNextItemWidth(220);
    ImGui::InputText("Address", ui.memAddrBuf, sizeof(ui.memAddrBuf),
                      ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::InputInt("Rows", &ui.memRows);
    if (ui.memRows < 1)  ui.memRows = 1;
    if (ui.memRows > 64) ui.memRows = 64;
    ImGui::SameLine();
    if (ImGui::Button("Read")) {
        std::uint64_t addr = 0;
        if (parseHex(ui.memAddrBuf, addr)) {
            ui.memAddr = (Address)addr;
            ui.memBuf.assign(ui.memRows * 16, 0);
            eng.readMemory(ui.memAddr, ui.memBuf.data(), ui.memBuf.size());
        }
    }
    if (ui.memBuf.empty()) {
        ImGui::TextDisabled("(enter an address and Read)");
        return;
    }

    ImGui::BeginChild("##dump", ImVec2(0, 0), false);
    for (int row = 0; row < ui.memRows; ++row) {
        std::size_t off = row * 16;
        if (off >= ui.memBuf.size()) break;
        Address rowAddr = ui.memAddr + off;
        ImGui::TextColored(ImVec4(.7f,.7f,.75f,1), "%016llX:",
                           (unsigned long long)rowAddr);
        ImGui::SameLine();
        char hex[64]{}, ascii[20]{};
        int h = 0, a = 0;
        for (int i = 0; i < 16 && off + i < ui.memBuf.size(); ++i) {
            Byte b = ui.memBuf[off + i];
            h += std::snprintf(hex + h, sizeof(hex) - h, "%02X ", b);
            ascii[a++] = (b >= 0x20 && b < 0x7F) ? (char)b : '.';
        }
        ImGui::Text("%-48s |%s|", hex, ascii);
    }
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// pointer paths (results pane for the scanner)
// ---------------------------------------------------------------------------
void drawPointerPane(UiState& ui, Engine& eng) {
    Window w("Pointer paths");
    if (!w) return;

    if (ImGui::Button("New pointer scan...")) ui.showPtrScan = true;
    ImGui::SameLine();
    if (eng.pointerScanning()) {
        if (ImGui::Button("Cancel")) eng.cancelPointerScan();
        ImGui::SameLine();
        ImGui::ProgressBar(eng.pointerScanProgress(), ImVec2(-1, 0));
    }

    auto paths = eng.pointerPaths();
    if (paths.empty()) {
        ImGui::TextDisabled("(no paths yet)");
        return;
    }
    ImGui::TextDisabled("%zu paths", paths.size());

    if (ImGui::BeginTable("##pp", 3,
                           ImGuiTableFlags_BordersInnerV |
                           ImGuiTableFlags_RowBg |
                           ImGuiTableFlags_ScrollY |
                           ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Base",     0, 160);
        ImGui::TableSetupColumn("Offsets",  ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("",         0, 100);
        ImGui::TableHeadersRow();
        int idx = 0;
        for (const auto& p : paths) {
            ImGui::PushID(idx++);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("0x%016llX", (unsigned long long)p.base);

            ImGui::TableSetColumnIndex(1);
            std::string s;
            for (auto o : p.offsets) {
                if (!s.empty()) s += " -> ";
                s += sfmt("+0x%X", o);
            }
            ImGui::TextColored(ImVec4(.8f,.9f,.75f,1), "%s", s.c_str());

            ImGui::TableSetColumnIndex(2);
            if (ImGui::SmallButton("+ Bookmark")) {
                Bookmark b{};
                b.name = sfmt("ptr_%llX", (unsigned long long)p.base);
                b.type = ValueType::I32;
                b.pointerBase    = p.base;
                b.pointerOffsets = p.offsets;
                b.frozenValue    = TypedValue::zero(b.type);
                eng.addBookmark(std::move(b));
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

// ---------------------------------------------------------------------------
// log
// ---------------------------------------------------------------------------
void drawLog(UiState& ui) {
    Window w("Log");
    if (!w) return;
    ImGui::BeginChild("##lt", ImVec2(0, 0), false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& line : ui.log) ImGui::TextUnformatted(line.c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20)
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// attach dialog
// ---------------------------------------------------------------------------
void drawAttachDialog(UiState& ui, Engine& eng) {
    if (!ui.showAttach) return;
    ImGui::OpenPopup("Attach to process");
    ImGui::SetNextWindowSize(ImVec2(520, 440), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal("Attach to process", &ui.showAttach)) {
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##f", "filter...",
                                   ui.procFilter, sizeof(ui.procFilter));

        ImGui::BeginChild("##pl", ImVec2(0, -40), true);
        for (int i = 0; i < (int)ui.procs.size(); ++i) {
            const auto& e = ui.procs[i];
            std::string name = wide_to_utf8(e.name);
            if (ui.procFilter[0] && !std::strstr(name.c_str(), ui.procFilter)) continue;
            char lbl[256];
            std::snprintf(lbl, sizeof(lbl), "%-6lu  %s##%d", e.pid, name.c_str(), i);
            if (ImGui::Selectable(lbl, ui.procSelected == i,
                                    ImGuiSelectableFlags_AllowDoubleClick)) {
                ui.procSelected = i;
                if (ImGui::IsMouseDoubleClicked(0)) {
                    eng.attach(e.pid);
                    ui.showAttach = false;
                }
            }
        }
        ImGui::EndChild();

        ImGui::BeginDisabled(ui.procSelected < 0);
        if (ImGui::Button("Attach", ImVec2(120, 0))) {
            if (ui.procSelected >= 0 && ui.procSelected < (int)ui.procs.size()) {
                eng.attach(ui.procs[ui.procSelected].pid);
                ui.showAttach = false;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Refresh", ImVec2(120, 0))) {
            ui.procs = enumerateProcesses();
            std::sort(ui.procs.begin(), ui.procs.end(),
                       [](auto& a, auto& b){ return a.name < b.name; });
            ui.procSelected = -1;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) ui.showAttach = false;
        ImGui::EndPopup();
    }
}

// ---------------------------------------------------------------------------
// pointer-scan dialog
// ---------------------------------------------------------------------------
void drawPointerScanDialog(UiState& ui, Engine& eng) {
    if (!ui.showPtrScan) return;
    ImGui::OpenPopup("Pointer scan");
    ImGui::SetNextWindowSize(ImVec2(420, 200), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal("Pointer scan", &ui.showPtrScan,
                                 ImGuiWindowFlags_NoResize)) {
        ImGui::TextDisabled("Target address (hex) to find pointers for:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##t", ui.ptrTarget, sizeof(ui.ptrTarget),
                          ImGuiInputTextFlags_CharsHexadecimal);

        ImGui::SetNextItemWidth(120);
        ImGui::InputInt("Max depth",  &ui.ptrDepth);
        ImGui::SetNextItemWidth(120);
        ImGui::InputInt("Max offset", &ui.ptrMaxOff);

        if (ImGui::Button("Run", ImVec2(120, 0))) {
            std::uint64_t t = 0;
            if (parseHex(ui.ptrTarget, t)) {
                PointerScanConfig cfg{};
                cfg.maxDepth  = std::max(1, std::min(ui.ptrDepth, 6));
                cfg.maxOffset = std::max(0x10, ui.ptrMaxOff);
                eng.startPointerScan((Address)t, cfg);
                ui.showPtrScan = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) ui.showPtrScan = false;
        ImGui::EndPopup();
    }
}

} // namespace cheat::ui
