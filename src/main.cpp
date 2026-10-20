#include <GLFW/glfw3.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>

int main()
{
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return EXIT_FAILURE;
    }

    if (!glfwVulkanSupported()) {
        std::fprintf(stderr, "Vulkan loader not found\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    std::uint32_t extensionCount = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
    std::printf("Required Vulkan instance extensions:\n");
    for (std::uint32_t i = 0; i < extensionCount; ++i) {
        std::printf("  %s\n", extensions[i]);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Renderer", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create window\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}