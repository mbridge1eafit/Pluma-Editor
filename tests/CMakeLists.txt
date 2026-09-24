include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# Unit test executable
set(UNIT_TEST_SOURCES
    unit/test_platform.cpp
    unit/test_document_io.cpp
    unit/test_editor_view.cpp
    unit/test_markdown_parser.cpp
    unit/test_preview_layout.cpp
    unit/test_sync_scroll.cpp
    unit/test_html_exporter.cpp
    unit/test_pdf_exporter.cpp
    ../src/platform/dpi.cpp
    ../src/platform/dpi.h
    ../src/platform/theme.cpp
    ../src/platform/theme.h
    ../src/io/document_io.cpp
    ../src/io/document_io.h
    ../src/editor/editor_view.cpp
    ../src/editor/editor_view.h
    ../src/markdown/block_tree.cpp
    ../src/markdown/slug.cpp
    ../src/markdown/slug.h
    ../src/markdown/emoji.cpp
    ../src/markdown/emoji.h
    ../src/markdown/block_tree.h
    ../src/markdown/md4c_adapter.cpp
    ../src/markdown/md4c_adapter.h
    ../src/markdown/parse_worker.cpp
    ../src/markdown/parse_worker.h
    ../src/preview/preview_layout.cpp
    ../src/preview/preview_renderer.cpp
    ../src/preview/preview_renderer.h
    ../src/preview/preview_layout.h
    ../src/preview/preview_view.cpp
    ../src/preview/preview_view.h
    ../src/sync/sync_scroll.cpp
    ../src/sync/sync_scroll.h
    ../src/export/html_exporter.cpp
    ../src/export/html_exporter.h
    ../src/export/pdf_exporter.cpp
    ../src/export/pdf_exporter.h
)

add_executable(unit_tests ${UNIT_TEST_SOURCES})

target_include_directories(unit_tests PRIVATE
    ../src
    ../third_party/md4c/src
    ../third_party/scintilla/include
    ../third_party/lexilla/include
    ../third_party/lexilla/lexlib
)

target_link_libraries(unit_tests PRIVATE
    GTest::gtest_main
    md4c
    scintilla
    lexilla
    dwmapi
    uxtheme
    d2d1
    dwrite
    user32
    gdi32
    imm32
    ole32
    uuid
)

include(GoogleTest)
gtest_discover_tests(unit_tests)
