#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>

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

    vkb::InstanceBuilder instanceBuilder;
    instanceBuilder
        .set_app_name("Renderer")
        .require_api_version(1, 3, 0)
        .request_validation_layers(true)
        .use_default_debug_messenger();

    for (std::uint32_t i = 0; i < extensionCount; ++i) {
        instanceBuilder.enable_extension(extensions[i]);
    }

    auto instanceResult = instanceBuilder.build();
    if (!instanceResult) {
        std::fprintf(stderr, "Failed to create Vulkan instance: %s\n",
                     instanceResult.error().message().c_str());
        glfwTerminate();
        return EXIT_FAILURE;
    }

    vkb::Instance instance = instanceResult.value();
    std::printf("Vulkan instance created\n");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Renderer", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create window\n");
        vkb::destroy_instance(instance);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult surfaceResult = glfwCreateWindowSurface(instance.instance, window, nullptr, &surface);
    if (surfaceResult != VK_SUCCESS) {
        std::fprintf(stderr, "Failed to create window surface (VkResult: %d)\n", static_cast<int>(surfaceResult));
        glfwDestroyWindow(window);
        vkb::destroy_instance(instance);
        glfwTerminate();
        return EXIT_FAILURE;
    }
    std::printf("Window surface created\n");

    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    vkb::PhysicalDeviceSelector selector(instance);
    auto physicalDeviceResult = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_surface(surface)
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
        .select();
    
    if (!physicalDeviceResult) {
        std::fprintf(stderr, "Failed to select GPU: %s\n", physicalDeviceResult.error().message().c_str());
        vkb::destroy_surface(instance, surface);
        glfwDestroyWindow(window);
        vkb::destroy_instance(instance);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    vkb :: PhysicalDevice physicalDevice = physicalDeviceResult.value();
    std::printf("Selected GPU: %s\n",physicalDevice.name.c_str());
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    vkb::destroy_surface(instance, surface);
    glfwDestroyWindow(window);
    vkb::destroy_instance(instance);
    glfwTerminate();
    return EXIT_SUCCESS;
}