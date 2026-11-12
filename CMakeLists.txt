cmake_minimum_required(VERSION 3.20)
project(WallpaperFinder
    VERSION 1.0.0
    DESCRIPTION "Buscador y Gestor de Fondos de Pantalla para Linux Desktop (KDE/GNOME/etc.)"
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for LSP/clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Dependencies
find_package(Threads REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

# ImGui library
add_library(imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_glfw.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends
)

target_link_libraries(imgui
    PUBLIC
        glfw
        OpenGL::GL
)

# Core library
add_library(wallpaper_core STATIC
    src/app/config.cpp
    src/net/http_client.cpp
    src/net/wallhaven_provider.cpp
    src/net/reddit_provider.cpp
    src/net/unsplash_provider.cpp
    src/net/pexels_provider.cpp
    src/net/aggregate_provider.cpp
    src/core/texture_cache.cpp
    src/core/wallpaper_setter.cpp
)

target_include_directories(wallpaper_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
)

target_link_libraries(wallpaper_core
    PUBLIC
        Threads::Threads
        CURL::libcurl
        OpenGL::GL
        nlohmann_json::nlohmann_json
)

# Main Application executable
add_executable(wallpaper_finder
    src/app_main.cpp
    src/ui/ui_manager.cpp
    src/ui/preview_modal.cpp
    src/ui/grid_view.cpp
)

target_include_directories(wallpaper_finder
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
)

target_link_libraries(wallpaper_finder
    PRIVATE
        wallpaper_core
        imgui
        glfw
        OpenGL::GL
        Threads::Threads
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

# Test & verification executable
add_executable(wallpaper_finder_test
    src/main.cpp
)

target_include_directories(wallpaper_finder_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(wallpaper_finder_test
    PRIVATE
        wallpaper_core
        CURL::libcurl
        glfw
        OpenGL::GL
        Threads::Threads
        nlohmann_json::nlohmann_json
)
