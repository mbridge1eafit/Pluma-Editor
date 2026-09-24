cmake_minimum_required(VERSION 3.28)
project(pluma VERSION 0.1.0 LANGUAGES C CXX)

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
    src/markdown/md4c_adapter.h
    src/markdown/md4c_adapter.cpp
    src/markdown/parse_worker.h
    src/markdown/parse_worker.cpp
    src/preview/preview_layout.h
    src/preview/preview_layout.cpp
    src/preview/preview_view.h
    src/preview/preview_view.cpp
    src/sync/sync_scroll.h
    src/sync/sync_scroll.cpp
    src/export/html_exporter.h
    src/export/html_exporter.cpp
    src/export/pdf_exporter.h
    src/export/pdf_exporter.cpp
    res/pluma.rc
    res/resource.h
    res/pluma.manifest
    res/icons/app.ico
    res/icons/doc_md.ico
)

add_executable(pluma WIN32 ${PLUMA_SOURCES})

target_include_directories(pluma PRIVATE
    src
    res
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
    d2d1
    dwrite
    windowscodecs
    dwmapi
    shell32
    comdlg32
    ole32
    imm32
    delayimp
)

if(MSVC)
    # Delay load libraries not needed at initial startup frame (NF-04)
    target_link_options(pluma PRIVATE
        "/DELAYLOAD:windowscodecs.dll"
        "/DELAYLOAD:comdlg32.dll"
    )
endif()

# ==============================================================================
# Unit Tests & Testing
# ==============================================================================
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
