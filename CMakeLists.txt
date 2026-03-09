cmake_minimum_required(VERSION 3.16)
project(paper_lily C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── Compiler warnings ───────────────────────────────────────────────────
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ── Sources ─────────────────────────────────────────────────────────────
set(SOURCES
    src/main.c
    src/game.c
    src/player.c
    src/world.c
    src/dialogue.c
    src/story.c
    src/ui.c
    src/utils.c
)

# ── Executable ──────────────────────────────────────────────────────────
add_executable(paper_lily ${SOURCES})

target_include_directories(paper_lily PRIVATE include)

# ── Platform-specific libraries ─────────────────────────────────────────
if(WIN32)
    # nothing extra required
elseif(UNIX)
    target_link_libraries(paper_lily PRIVATE m)
endif()

# ── Install rules ────────────────────────────────────────────────────────
install(TARGETS paper_lily DESTINATION bin)
install(DIRECTORY assets/ DESTINATION share/paper_lily/assets)
install(FILES docs/DESIGN.md DESTINATION share/doc/paper_lily)

# ── Optional: enable AddressSanitizer in Debug builds ────────────────────
option(ENABLE_ASAN "Enable AddressSanitizer (Debug only)" OFF)
if(ENABLE_ASAN AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(NOT MSVC)
        target_compile_options(paper_lily PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(paper_lily PRIVATE -fsanitize=address)
    endif()
endif()
