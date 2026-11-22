cmake_minimum_required(VERSION 3.20)
project(cheatos LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "CheatOS is Windows-only (uses the Win32 process APIs).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /EHsc /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

include(FetchContent)

# ---- Dear ImGui (docking branch) -------------------------------------------
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.5-docking
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)

# ---- pugixml (CE-table XML read/write) -------------------------------------
FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG        v1.14
)
FetchContent_MakeAvailable(pugixml)

# ---- cheatos executable ----------------------------------------------------
add_executable(cheatos WIN32
    src/main.cpp
    src/value_types.cpp
    src/proc_attach.cpp
    src/mem_regions.cpp
    src/scanner.cpp
    src/bookmarks.cpp
    src/freezer.cpp
    src/pointer_scan.cpp
    src/ce_table.cpp
    src/engine.cpp
    src/ui/theme.cpp
    src/ui/app.cpp
    src/ui/views.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(cheatos PRIVATE
    src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(cheatos PRIVATE
    pugixml::pugixml
    d3d11 dxgi dxguid dwmapi psapi
)

set_target_properties(cheatos PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
