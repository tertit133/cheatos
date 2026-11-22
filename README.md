<div align="center">

# CheatOS

**Memory scanner and trainer framework for Windows x64.**

[![build](https://github.com/tertit133/cheatos/actions/workflows/build.yml/badge.svg)](https://github.com/tertit133/cheatos/actions/workflows/build.yml)
[![license](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-20-00599C.svg?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![platform](https://img.shields.io/badge/platform-Windows%20x64-0078D6.svg?logo=windows)](https://learn.microsoft.com/en-us/windows/win32/memory/)

ImGui + DX11 UI (docking) * pugixml for CheatEngine .CT compatibility

</div>

> A compact Cheat-Engine-style trainer: attach to any process, scan
> memory for values, filter down across scans, freeze what you want,
> resolve pointer chains, and load/save CheatEngine tables. ~2.5k LOC
> of straight C++20 with a small set of focused modules.

## Features

- **Attach to any PID** with full RW access; process list with live filter.
- **Typed scanner** -- int8/16/32/64 signed & unsigned, float/double,
  byte patterns (`DE AD ?? EF`), ASCII and wide strings.
- **Scan ops** -- `exact`, `!=`, `>`, `>=`, `<`, `<=`, `between`,
  and the next-scan filters `changed` / `unchanged` / `increased` /
  `decreased`. Fast aligned scans or every-byte mode.
- **Freezer thread** -- rewrites pinned bookmark values ~60 Hz so game
  logic can't decrement them.
- **Pointer scanner** -- BFS over the process's pointer map, finds
  chains `*(static + o1) ... + oN` that land on a target, up to depth 6.
  Keeps only chains rooted in a module image so they survive restarts.
- **Bookmarks** -- live value column, inline rename/type-change, freeze
  toggle, pointer-path aware.
- **CheatEngine table I/O** -- load/save `.CT` XML via pugixml; works
  with existing tables for common entries.
- **ImGui docking layout** -- drag any pane, split, float; layout
  persists in `imgui.ini` next to the exe.

## Build

Prerequisites:

- Windows 10/11 x64
- Visual Studio 2022 (MSVC, x64 Native Tools) or CMake + Ninja with MSVC
- CMake 3.20+
- Git (FetchContent pulls ImGui + pugixml)

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
build\bin\Release\cheatos.exe
```

First configure pulls ~30 MB of dependencies.

## Architecture

```
src/
+-- common.h            types, Windows includes (windows.h FIRST, lesson learned)
+-- value_types.{h,cpp} typed values, parse/compare/format, byte patterns
+-- proc_attach.{h,cpp} enumerate processes, OpenProcess for VM RW
+-- mem_regions.{h,cpp} VirtualQueryEx walker (committed + readable/writable)
+-- scanner.{h,cpp}     first-scan and next-scan, chunked RPM, cancellable
+-- bookmarks.{h,cpp}   saved address list (name, type, frozen value, pointer path)
+-- freezer.{h,cpp}     ~60 Hz WPM thread that holds frozen values
+-- pointer_scan.{h,cpp} BFS pointer-path finder, rooted in MEM_IMAGE
+-- ce_table.{h,cpp}    CheatEngine .CT XML read/write (pugixml)
+-- engine.{h,cpp}      orchestrator; UI thread talks only to this class
+-- main.cpp            wWinMain
+-- ui/
    +-- theme.{h,cpp}  gray ImGui style
    +-- app.{h,cpp}    Win32 + DX11 + ImGui docking bootstrap
    +-- views.{h,cpp}  Control / Scan / Results / Bookmarks / Memory / Pointer / Log
```

The `Engine` owns a single worker thread that scans/pointer-scans run
on, so the UI thread never blocks. State (results list, pointer paths,
bookmarks) is read via thread-safe snapshot calls. The freezer is its
own thread that pulls bookmarks through a provider callback so it does
not need to co-own the list.

## Scan engine

- `enumerateRegions` walks `VirtualQueryEx` from `lpMinimumApplicationAddress`
  to `lpMaximumApplicationAddress`, keeping committed pages that match
  the access mask (`PAGE_READWRITE`, `PAGE_WRITECOPY`, ...).
- `firstScan` splits every region into 4 MiB chunks, `ReadProcessMemory`
  into a reusable buffer, then walks aligned slots applying the typed
  comparator. Cancellation is checked between chunks.
- `nextScan` filters the stored previous results in place -- one RPM per
  entry, compare against the user operand (plus the previous value for
  `changed`/`increased`/... ops).
- Match captures the current bytes so the next scan can compare against
  them without re-reading. Byte pattern + wildcard mask (`DE AD ?? EF`)
  supported via a parallel mask vector.

## Pointer scan

1. Walk every readable region, interpret each aligned 8-byte slot as a
   pointer value, keep only those pointing inside another readable
   region -- stored sorted by `dest`.
2. Starting from the target address, binary-search the pointer-map for
   `dest` in `[T - maxOff, T]`. Each hit is a one-level resolve with
   offset `T - dest`.
3. If the hit source lives in a `MEM_IMAGE` region (module), accept it
   as a final chain. Otherwise enqueue it as a new target and recurse
   up to `maxDepth`.
4. Keeps up to `maxResults` paths so huge targets don't blow the UI.

## CheatEngine .CT compatibility

`loadCheatTable` / `saveCheatTable` handle the subset that actually
matters for trainer entries:

- `<Description>`
- `<VariableType>` -- "Byte", "2 Bytes", "4 Bytes", "8 Bytes", "Float",
  "Double", "String", "Array of byte"
- `<Address>` in hex (base when there's a pointer chain)
- `<Offsets><Offset>...</Offset></Offsets>`

Scripts, groupings, and anti-cheat sections are ignored on read and
omitted on write. Round-trips are stable for every entry in that set.

## License

MIT. See `LICENSE`.
