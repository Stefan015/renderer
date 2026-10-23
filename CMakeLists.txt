cmake_minimum_required(VERSION 3.25)
project(Renderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Vulkan REQUIRED COMPONENTS glslc)
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

set(SHADER_SOURCES
    shaders/triangle.vert
    shaders/triangle.frag
)

set(SHADER_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
set(SHADER_BINARIES)

foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SHADER_SPV ${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv)

    add_custom_command(
        OUTPUT ${SHADER_SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
        COMMAND Vulkan::glslc --target-env=vulkan1.3 ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER} -o ${SHADER_SPV}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}
        COMMENT "Compiling shader ${SHADER_NAME}"
        VERBATIM
    )

    list(APPEND SHADER_BINARIES ${SHADER_SPV})
endforeach()

add_custom_target(shaders DEPENDS ${SHADER_BINARIES})
add_dependencies(renderer shaders)

target_compile_definitions(renderer PRIVATE SHADER_DIR="${SHADER_OUTPUT_DIR}")