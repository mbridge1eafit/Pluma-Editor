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
    ../src/platform/dpi.cpp
    ../src/platform/dpi.h
    ../src/platform/theme.cpp
    ../src/platform/theme.h
)

add_executable(unit_tests ${UNIT_TEST_SOURCES})

target_include_directories(unit_tests PRIVATE
    ../src
)

target_link_libraries(unit_tests PRIVATE
    GTest::gtest_main
    dwmapi
    user32
    gdi32
)

include(GoogleTest)
gtest_discover_tests(unit_tests)
