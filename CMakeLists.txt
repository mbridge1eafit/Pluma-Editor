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
# md4c (Vendorized Markdown parser)
# ==============================================================================
add_library(md4c STATIC
    third_party/md4c/src/md4c.c
    third_party/md4c/src/entity.c
)
target_include_directories(md4c PUBLIC
    third_party/md4c/src
)
if(MSVC)
    # md4c is third-party C code, suppress /WX on it to prevent upstream compiler warnings
    target_compile_options(md4c PRIVATE /W3 /wd4996)
endif()

# ==============================================================================
# Pluma Main Executable (Win32 GUI)
# ==============================================================================
set(PLUMA_SOURCES
    src/app/main.cpp
    src/platform/dpi.h
    src/platform/dpi.cpp
    src/platform/theme.h
    src/platform/theme.cpp
    res/pluma.rc
    res/resource.h
)

add_executable(pluma WIN32 ${PLUMA_SOURCES})

target_include_directories(pluma PRIVATE
    src
    res
    third_party/md4c/src
)

target_link_libraries(pluma PRIVATE
    md4c
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
