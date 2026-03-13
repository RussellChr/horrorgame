cmake_minimum_required(VERSION 3.10)
project(HorrorGame C)

set(CMAKE_C_STANDARD 11)

# Allow finding SDL3 installed in /usr/local
list(APPEND CMAKE_PREFIX_PATH "/usr/local")
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/sdl3_image")

find_package(SDL3 REQUIRED CONFIG)


# SDL3_image is optional; enables PNG loading for the dialogue background
find_package(SDL3_image CONFIG)

include_directories(include)

add_executable(horrorgame
    src/main.c
    src/game.c
    src/ui.c
    src/player.c
    src/world.c
    src/dialogue.c
    src/story.c
    src/utils.c
    src/render.c
    src/animation.c
    src/camera.c
    src/collision.c
)

target_link_libraries(horrorgame SDL3::SDL3)

# If SDL3_image was found, enable PNG loading for the dialogue background
if(TARGET SDL3_image::SDL3_image)
    target_compile_definitions(horrorgame PRIVATE HAVE_SDL3_IMAGE=1)
    target_link_libraries(horrorgame SDL3_image::SDL3_image)
endif()

# Link math library on Linux
if(UNIX AND NOT APPLE)
    target_link_libraries(horrorgame m)
endif()
