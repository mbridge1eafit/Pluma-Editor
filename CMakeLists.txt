cmake_minimum_required(VERSION 3.28)
project(pluma VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Place all binaries in bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Static CRT configuration (/MT for Release, /MTd for Debug)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    # Strict warnings and standard conformance
    add_compile_options(
        /W4
        /WX
        /permissive-
        /Zc:__cplusplus
        /utf-8
        /D_UNICODE
        /DUNICODE
        /DWIN32_LEAN_AND_MEAN
        /DNOMINMAX
    )

    # Release optimizations
    add_compile_options("$<$<CONFIG:Release>:/O2;/Oi;/GL>")
    add_link_options("$<$<CONFIG:Release>:/LTCG>")

    # AddressSanitizer support
    if(ENABLE_ASAN)
        add_compile_options(/fsanitize=address)
        add_link_options(/fsanitize=address)
    endif()
endif()

# ==============================================================================
# Third-party Dependencies (md4c, scintilla, lexilla)
# ==============================================================================
add_subdirectory(third_party)

# ==============================================================================
# Pluma Main Executable (Win32 GUI)
# ==============================================================================
set(PLUMA_SOURCES
    src/app/main.cpp
    src/app/settings_dialog.h
    src/app/settings_dialog.cpp
    src/config/settings.h
    src/config/settings.cpp
    src/outline/outline_panel.h
    src/outline/outline_panel.cpp
    src/platform/file_association.h
    src/platform/file_association.cpp
    src/platform/dpi.h
    src/platform/dpi.cpp
    src/platform/theme.h
    src/platform/theme.cpp
    src/io/document_io.h
    src/io/document_io.cpp
    src/editor/editor_view.h
    src/editor/editor_view.cpp
    src/markdown/block_tree.h
    src/markdown/block_tree.cpp
    src/markdown/slug.h
    src/markdown/slug.cpp
    src/markdown/emoji.h
    src/markdown/emoji.cpp
    src/markdown/md4c_adapter.h
    src/markdown/md4c_adapter.cpp
    src/markdown/parse_worker.h
    src/markdown/parse_worker.cpp
    src/diagram/diagram_scene.h
    src/diagram/diagram_scene.cpp
    src/diagram/graph_layout.h
    src/diagram/graph_layout.cpp
    src/diagram/mermaid.h
    src/diagram/mermaid.cpp
    src/diagram/mermaid_internal.h
    src/diagram/mermaid_common.cpp
    src/diagram/mermaid_flowchart.cpp
    src/diagram/mermaid_sequence.cpp
    src/diagram/mermaid_pie.cpp
    src/diagram/svg_writer.h
    src/diagram/svg_writer.cpp
    src/preview/preview_layout.h
    src/preview/preview_layout.cpp
    src/preview/preview_renderer.h
    src/preview/preview_renderer.cpp
    src/preview/preview_view.h
    src/preview/preview_view.cpp
    src/sync/sync_scroll.h
    src/sync/sync_scroll.cpp
    src/export/html_exporter.h
    src/export/html_exporter.cpp
    src/export/pdf_exporter.h
    src/export/pdf_exporter.cpp
    src/update/version.h
    src/update/version.cpp
    src/update/json.h
    src/update/json.cpp
    src/update/release_info.h
    src/update/release_info.cpp
    src/update/sha256.h
    src/update/sha256.cpp
    src/update/http_client.h
    src/update/http_client.cpp
    src/update/updater.h
    src/update/updater.cpp
    src/app/update_ui.h
    src/app/update_ui.cpp
    res/pluma.rc
    res/resource.h
    res/pluma.manifest
    res/icons/app.ico
    res/icons/doc_md.ico
)

# Single source of the version for the code and the resources (res/pluma.rc)
configure_file(res/version.h.in ${CMAKE_BINARY_DIR}/generated/pluma_version.h @ONLY)
# Ninja's header scanning of rc.exe output (cmcldeps) misses includes with localized MSVC, which kept a
# stale version resource after a version bump: declare the dependencies explicitly.
set_source_files_properties(res/pluma.rc PROPERTIES OBJECT_DEPENDS
    "${CMAKE_BINARY_DIR}/generated/pluma_version.h;${CMAKE_SOURCE_DIR}/res/resource.h")

add_executable(pluma WIN32 ${PLUMA_SOURCES})

target_include_directories(pluma PRIVATE
    src
    res
    ${CMAKE_BINARY_DIR}/generated
    third_party/md4c/src
    third_party/scintilla/include
    third_party/lexilla/include
    third_party/lexilla/lexlib
)

target_link_libraries(pluma PRIVATE
    md4c
    scintilla
    lexilla
    user32
    gdi32
    comctl32
    uxtheme
    d2d1
    dwrite
    windowscodecs
    dwmapi
    shell32
    shlwapi
    comdlg32
    ole32
    imm32
    winhttp
    bcrypt
    delayimp
)

if(MSVC)
    # Delay load libraries not needed at initial startup frame (NF-04)
    target_link_options(pluma PRIVATE
        "/DELAYLOAD:windowscodecs.dll"
        "/DELAYLOAD:comdlg32.dll"
        "/DELAYLOAD:winhttp.dll"
        "/DELAYLOAD:bcrypt.dll"
    )
endif()

# ==============================================================================
# Unit Tests & Testing
# ==============================================================================
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
