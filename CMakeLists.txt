cmake_minimum_required(VERSION 3.25)
project(Renderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)

add_executable(renderer
    src/main.cpp
    src/renderer.cpp
)

target_link_libraries(renderer PRIVATE
    glfw
    glm::glm
    Vulkan::Vulkan
    vk-bootstrap::vk-bootstrap
)