cmake_minimum_required(VERSION 3.16)
project(scribbolyth VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

# Create executable
add_executable(scribbolyth
    src/main.cpp
    src/editor/editor.cpp
    src/treeview/treeview.cpp
    src/keyboard/keymap.cpp
    src/keyboard/default_keymaps.cpp
)

# FTXUI is terminal-only, no extra platform libs needed

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(scribbolyth PRIVATE -Wall -Wextra -O2)
endif()

# Set output directory
set_target_properties(scribbolyth PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
target_link_libraries(scribbolyth PRIVATE ftxui::screen)
target_link_libraries(scribbolyth PRIVATE ftxui::dom)
target_link_libraries(scribbolyth PRIVATE ftxui::component)
