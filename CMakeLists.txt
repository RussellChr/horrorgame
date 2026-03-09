cmake_minimum_required(VERSION 3.10)
project(HorrorGame C)

set(CMAKE_C_STANDARD 11)

# Allow finding SDL3 installed in /usr/local
list(APPEND CMAKE_PREFIX_PATH "/usr/local")

find_package(SDL3 REQUIRED CONFIG)

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

# Link math library on Linux
if(UNIX AND NOT APPLE)
    target_link_libraries(horrorgame m)
endif()
