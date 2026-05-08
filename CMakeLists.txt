# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(axylith VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── Find dependencies ────────────────────────────────
find_package(Vulkan REQUIRED)
find_package(X11 REQUIRED)

# ─── Main binary ──────────────────────────────────────
add_executable(axylith
    src/main.cpp
    src/window.cpp
    src/vulkan_init.cpp
    src/swapchain.cpp
    src/monitor.h
    src/renderer.cpp
)

target_include_directories(axylith PRIVATE src)

target_link_libraries(axylith PRIVATE
    Vulkan::Vulkan
    X11::X11
)

# ─── Warnings (strict but not -Werror yet) ────────────
target_compile_options(axylith PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion
)

# ─── Release optimizations ────────────────────────────
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(axylith PRIVATE -Os -ffunction-sections -fdata-sections -fno-rtti -fno-exceptions)
    target_link_options(axylith PRIVATE -s -Wl,--gc-sections)
endif()

# ─── Tests ────────────────────────────────────────────
add_executable(axl_test tests/axl_test.cpp)
target_include_directories(axl_test PRIVATE src)    # ← add this line
target_compile_options(axl_test PRIVATE -Wall -Wextra)

enable_testing()
add_test(NAME axl_test COMMAND axl_test)