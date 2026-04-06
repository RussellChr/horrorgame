cmake_minimum_required(VERSION 3.10)
project(HorrorGame C)

set(CMAKE_C_STANDARD 11)

# Allow finding SDL3 installed in /usr/local (macOS native builds)
list(APPEND CMAKE_PREFIX_PATH "/usr/local")
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/sdl3_image")

# Windows cross-compilation via MinGW (used when mingw.cmake toolchain is active)
if(MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Support both Intel (/usr/local) and Apple Silicon (/opt/homebrew) Homebrew prefixes
    list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/mingw-w64/toolchain-x86_64/x86_64-w64-mingw32")
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/mingw-w64/toolchain-x86_64/x86_64-w64-mingw32")
    # Produce a .exe output
    set(CMAKE_EXECUTABLE_SUFFIX ".exe")
endif()

find_package(SDL3 REQUIRED CONFIG)

# SDL3_image is optional; enables PNG loading for the dialogue background
find_package(SDL3_image CONFIG)

# Prefer static SDL3 target to avoid missing DLL at runtime
if(TARGET SDL3::SDL3-static)
    set(SDL3_LINK_TARGET SDL3::SDL3-static)
else()
    set(SDL3_LINK_TARGET SDL3::SDL3)
endif()

# Prefer static SDL3_image target
if(TARGET SDL3_image::SDL3_image-static)
    set(SDL3_IMAGE_LINK_TARGET SDL3_image::SDL3_image-static)
elseif(TARGET SDL3_image::SDL3_image)
    set(SDL3_IMAGE_LINK_TARGET SDL3_image::SDL3_image)
else()
    set(SDL3_IMAGE_LINK_TARGET "")
endif()

include_directories(include)

add_executable(horrorgame
    src/main.c
    src/game.c
    src/game_render.c
    src/effects.c
    src/interactions.c
    src/ui.c
    src/player.c
    src/world.c
    src/map.c
    src/dialogue.c
    src/monologue.c
    src/story.c
    src/utils.c
    src/render.c
    src/animation.c
    src/camera.c
    src/collision.c
    src/npc.c
    src/enemy.c
)

target_link_libraries(horrorgame ${SDL3_LINK_TARGET})

# If SDL3_image was found, enable PNG loading for the dialogue background
if(SDL3_IMAGE_LINK_TARGET)
    target_compile_definitions(horrorgame PRIVATE HAVE_SDL3_IMAGE=1)
    target_link_libraries(horrorgame ${SDL3_IMAGE_LINK_TARGET})
endif()

# Link math library on Linux
if(UNIX AND NOT APPLE)
    target_link_libraries(horrorgame m)
endif()

# On MinGW, statically link the C/C++ runtime to avoid missing
# libgcc_s_seh-1.dll / libwinpthread-1.dll at runtime.
if(MINGW)
    target_link_options(horrorgame PRIVATE -static-libgcc -static-libstdc++)
endif()

# Copy assets folder to the build output directory after each build
add_custom_command(TARGET horrorgame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:horrorgame>/assets"
    COMMENT "Copying assets to build directory"
)

# Copy maps folder to the build output directory after each build
add_custom_command(TARGET horrorgame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/maps"
        "$<TARGET_FILE_DIR:horrorgame>/maps"
    COMMENT "Copying maps to build directory"
)

# Install the executable and assets
install(TARGETS horrorgame DESTINATION .)
install(DIRECTORY assets DESTINATION .)
install(DIRECTORY maps DESTINATION .)
