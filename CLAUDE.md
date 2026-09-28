# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

Pluma is a fully native Windows Markdown editor (C++20, Win32) designed to launch in under 100 ms and occupy under 3 MB on disk. It has zero embedded-browser dependency (no Chromium, no WebView2, no .NET) — the Markdown preview, including Mermaid diagrams, is rendered with Direct2D/DirectWrite.

Commit messages, code comments, and `docs/DECISIONS.md` are in Spanish; this file and code identifiers are in English/Spanish mixed with the existing codebase conventions — match the surrounding file's language when editing.

## Build commands

The project builds only on Windows with MSVC (Visual Studio 2022, C++20) via CMake presets (`CMakePresets.json`) and Ninja. There is no cross-platform build.

```powershell
# Release (optimized, static CRT /MT)
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure

# Debug (static CRT /MTd)
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure

# AddressSanitizer
cmake --preset asan
cmake --build --preset asan
ctest --preset asan --output-on-failure
```

The `pluma.exe` binary is produced at `build/<preset>/bin/pluma.exe`.

### Running a single test

Tests use GoogleTest (fetched via `FetchContent`) and are discovered with `gtest_discover_tests`. Filter by test name via ctest or run the test binary directly:

```powershell
ctest --preset release --output-on-failure -R MyTestName
# or, once built:
build\release\bin\unit_tests.exe --gtest_filter=Suite.CaseName
```

### Packaging / release

```powershell
.\scripts\package_release.ps1          # dist\: portable ZIP, Inno Setup installer, SHA256SUMS.txt
```

The installer step requires Inno Setup 6 (`winget install --id JRSoftware.InnoSetup -e`). Version is defined **only** in `project(VERSION)` in [CMakeLists.txt](CMakeLists.txt) (also mirrored in `res/pluma.manifest`); it is threaded through to code and `res/pluma.rc` via a generated `pluma_version.h`. Never publish a release tag that doesn't match `project(VERSION)` — the auto-updater compares against it. Full release automation (bump, test, package, notes, publish, verify) is documented in the `github-release` skill (`.claude/skills/github-release/SKILL.md`) — use it (or `/github-release`-style requests) instead of improvising release steps by hand.

### Benchmarks

```powershell
.\bench\startup.ps1 -ExePath "build\release\bin\pluma.exe" -FailOnHardLimit
```

CI (`.github/workflows/ci.yml`) enforces a hard binary-size limit of 4.0 MB (target ≤ 2.0 MB) and a cold-startup budget, in addition to running the Release and ASan test suites and building the installer.

## Code style

Enforced by `/W4 /WX /permissive-` (MSVC) — warnings are errors, so keep new code warning-clean. `.clang-format` governs formatting. Unicode-only Win32 APIs (`W` suffix); `/DUNICODE /D_UNICODE`; DPI-aware (Per-Monitor v2); no runtime dependencies beyond system DLLs (some are delay-loaded — see `/DELAYLOAD` flags in `CMakeLists.txt` — to keep the cold-start path light, NF-04).

## Architecture

Source is under `src/`, organized by concern; each module pairs a `.h`/`.cpp`. Mirrored by `tests/unit/test_*.cpp`, one file per major module, plus `tests/corpus/*.md` fixtures for parser/rendering tests.

**Document pipeline (edit → parse → render), roughly in dataflow order:**

1. **`src/editor/editor_view.*`** — wraps Scintilla via direct function pointers (`SCI_GETDIRECTFUNCTION`/`SCI_GETDIRECTPOINTER`), bypassing `SendMessage` overhead. Markdown lexing comes from Lexilla, both linked statically (no `LoadLibrary`).
2. **`src/markdown/`** — the Markdown pipeline:
   - `md4c_adapter.*` wraps md4c's SAX callbacks (CommonMark 0.31 + GFM) and builds `block_tree.*`, the AST (`BlockTree`): hierarchical blocks (headings, paragraphs, lists incl. GFM task lists, code blocks, tables, etc.) and inline spans, each carrying source line ranges (`startLine`/`endLine`) used for sync-scroll.
   - `parse_worker.*` runs parsing on a background `std::jthread` with a reactive 50 ms debounce (`SRWLOCK`/`CONDITION_VARIABLE`); stale in-flight parses are dropped without posting; the finished tree is handed to the UI thread via `PostMessageW(..., WM_USER_PARSE_COMPLETE, version, tree.release())`. Never parse synchronously on the UI thread.
   - `slug.*` / `emoji.*` are supporting utilities (heading anchors, `:shortcode:` emoji).
3. **`src/diagram/`** — a self-contained Mermaid engine (parser + Sugiyama-style layout in `graph_layout.*` + a backend-agnostic vector scene in `diagram_scene.*`). Supports `flowchart`/`graph`, `sequenceDiagram`, `stateDiagram(-v2)`, `pie`; unsupported diagram types degrade to a labeled code block. `mermaid_*.cpp` files split parsing by diagram kind; `mermaid_internal.h` is the shared internal API, `mermaid.h` the public one. The scene is rendered by three independent backends (Direct2D preview, inline SVG for HTML export, vector ops for PDF export) so layout is computed once and only the drawing/text-measurement step is backend-specific.
4. **`src/preview/`** — `PreviewLayout` (DirectWrite measurement/layout of the AST into DIP-space blocks, DPI-explicit) is decoupled from `PreviewRenderer` (draws a `LayoutEngine`'s output onto any `ID2D1RenderTarget`, window or offscreen WIC bitmap) and `PreviewView` (the Win32 child window: scrolling, hit-testing links via stored `D2D1_RECT_F` per `LinkTarget`, viewport culling). Keep layout and drawing separated when touching this code — it's what allows off-screen rendering for tests/export.
5. **`src/sync/sync_scroll.*`** — bidirectional editor↔preview scroll sync, proportional within AST blocks (`GetScrollYForLine`/`GetLineForScrollY` in the layout engine), with anti-echo tracking (`ScrollSource`, `lastEditorLine`/`lastPreviewLine`) to prevent feedback loops.
6. **`src/export/`** — `html_exporter.*` (self-contained HTML5 via md4c's `md_html`, CSS variables for light/dark, Mermaid diagrams inlined as SVG) and `pdf_exporter.*` (a from-scratch vector PDF 1.4 writer — Helvetica/Courier Type1 fonts with WinAnsi encoding, orphan-heading prevention, table auto-layout shared with the preview's table algorithm).
7. **`src/io/document_io.*`** — encoding-preserving round-trip I/O (UTF-8 with/without BOM, UTF-16 LE/BE, ANSI/Windows-1252 detection), memory-mapped reads for files > 1 MB, atomic saves (write to temp file in the same directory, `FlushFileBuffers`, `ReplaceFileW`/`MoveFileExW` fallback). `ReadDocument` throws on failure rather than returning an empty document (a prior bug silently truncated files on read failure — see Decision 010 in `docs/DECISIONS.md`).

**Supporting layers:**

- **`src/app/`** — `main.cpp` (message loop, `MainWindow`), `settings_dialog.*` (Preferences UI), `update_ui.*` (update flow dialogs).
- **`src/config/settings.*`** — hand-rolled INI parser/serializer; config lives at `%APPDATA%\Pluma\pluma.ini`, or `pluma.ini` next to the executable if present (portable mode). Out-of-range values are clamped, invalid ones keep defaults.
- **`src/outline/outline_panel.*`** — headings `TreeView` panel, fed from the same async-parsed AST (no extra parse pass).
- **`src/platform/`** — `dpi.*` (Per-Monitor DPI v2), `theme.*` (dark mode via DWM), `file_association.*` (HKCU-only, no-elevation `.md`/`.markdown`/`.mdown` registration).
- **`src/update/`** — self-update pipeline: `updater.*` orchestrates, `http_client.*` wraps WinHTTP, `release_info.*`/`json.*` parse the GitHub Releases API response, `sha256.*` verifies the downloaded installer (via CNG/`bcrypt`) against the published `SHA256SUMS.txt`, `version.*` does SemVer comparison. Checked once/day, 4s after startup (off the critical path), or on demand.

**Cross-cutting architectural rules** (see `docs/DECISIONS.md` for full rationale per decision):
- No WebView2/Chromium/.NET, no dynamic loading of first-party libraries (Scintilla/Lexilla/md4c are all statically linked).
- The AST (`BlockTree`) is the single source of truth consumed directly by the DirectWrite layout engine — no intermediate HTML/DOM generation for the native preview.
- Preview layout math is always in DIPs (device-independent pixels), never physical pixels — this was a past source of DPI-scaling bugs at 125–200%.
- Third-party dependencies are vendored at fixed versions under `third_party/` (Scintilla 5.5.3, Lexilla 5.4.3, md4c 0.5.2) — no vcpkg/conan.
