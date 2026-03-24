cmake_minimum_required(VERSION 3.28)

project(gpu_raytracer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

include(FetchContent)
find_package(Vulkan REQUIRED)
find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_ALSA OFF CACHE BOOL "" FORCE)
set(SDL_JACK OFF CACHE BOOL "" FORCE)
set(SDL_PIPEWIRE OFF CACHE BOOL "" FORCE)
set(SDL_PULSEAUDIO OFF CACHE BOOL "" FORCE)
set(SDL_SNDIO OFF CACHE BOOL "" FORCE)
set(SDL_X11_XSCRNSAVER OFF CACHE BOOL "" FORCE)
set(SDL_X11_XTEST OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.4.2
  GIT_SHALLOW TRUE
)

FetchContent_Declare(
  SDL3_ttf
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
  GIT_TAG release-3.2.2
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(SDL3)
FetchContent_MakeAvailable(SDL3_ttf)

set(SHADER_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/raytrace.comp)
set(SHADER_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/raytrace.comp.spv)
set(PRESENT_SHADER_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/present.comp)
set(PRESENT_SHADER_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/present.comp.spv)

add_custom_command(
  OUTPUT ${SHADER_OUTPUT}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
  COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.3 -S comp -o ${SHADER_OUTPUT} ${SHADER_SOURCE}
  DEPENDS ${SHADER_SOURCE}
  VERBATIM
)

add_custom_command(
  OUTPUT ${PRESENT_SHADER_OUTPUT}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
  COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.3 -S comp -o ${PRESENT_SHADER_OUTPUT} ${PRESENT_SHADER_SOURCE}
  DEPENDS ${PRESENT_SHADER_SOURCE}
  VERBATIM
)

add_custom_target(gpu-raytracer-shaders DEPENDS ${SHADER_OUTPUT} ${PRESENT_SHADER_OUTPUT})

add_executable(gpu-raytracer
  src/app.cpp
  src/bvh_builder.cpp
  src/camera.cpp
  src/loading_screen.cpp
  src/main.cpp
  src/mesh_loader.cpp
  src/sample_scene.cpp
  src/texture_loader.cpp
  src/vulkan_helpers.cpp
  src/vulkan_renderer.cpp
)

target_compile_options(gpu-raytracer
  PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wno-missing-field-initializers
)

target_link_libraries(gpu-raytracer
  PRIVATE
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
    Vulkan::Vulkan
)

target_include_directories(gpu-raytracer
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(gpu-raytracer
  PRIVATE
    GPU_RAYTRACER_SHADER_PATH="${SHADER_OUTPUT}"
    GPU_PRESENT_SHADER_PATH="${PRESENT_SHADER_OUTPUT}"
    GPU_SAMPLE_MESH_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/viking_room.obj"
    GPU_SAMPLE_TEXTURE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/viking_room.png"
)

add_dependencies(gpu-raytracer gpu-raytracer-shaders)
