#include "renderer.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>

#include <cstdint>
#include <cstdio>

bool Renderer::init()
{
    if (!initWindow()) {
        return false;
    }
    if (!initVulkan()) {
        return false;
    }
    return true;
}

void Renderer::run()
{
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }
}

bool Renderer::initWindow()
{
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return false;
    }
    glfwInitialized = true;

    if (!glfwVulkanSupported()) {
        std::fprintf(stderr, "Vulkan loader not found\n");
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(1280, 720, "Renderer", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create window\n");
        return false;
    }

    return true;
}

bool Renderer::initVulkan()
{
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
        return false;
    }
    vkb::Instance vkbInstance = instanceResult.value();
    instance = vkbInstance.instance;
    debugMessenger = vkbInstance.debug_messenger;
    std::printf("Vulkan instance created\n");

    VkResult surfaceResult = glfwCreateWindowSurface(instance, window, nullptr, &surface);
    if (surfaceResult != VK_SUCCESS) {
        std::fprintf(stderr, "Failed to create window surface (VkResult %d)\n",
                     static_cast<int>(surfaceResult));
        return false;
    }
    std::printf("Window surface created\n");

    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    vkb::PhysicalDeviceSelector selector{vkbInstance};
    auto physicalDeviceResult = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_surface(surface)
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
        .select();
    if (!physicalDeviceResult) {
        std::fprintf(stderr, "Failed to select a GPU: %s\n",
                     physicalDeviceResult.error().message().c_str());
        return false;
    }
    vkb::PhysicalDevice vkbPhysicalDevice = physicalDeviceResult.value();
    physicalDevice = vkbPhysicalDevice.physical_device;
    std::printf("Selected GPU: %s\n", vkbPhysicalDevice.name.c_str());

    vkb::DeviceBuilder deviceBuilder{vkbPhysicalDevice};
    auto deviceResult = deviceBuilder.build();
    if (!deviceResult) {
        std::fprintf(stderr, "Failed to create logical device: %s\n",
                     deviceResult.error().message().c_str());
        return false;
    }
    vkb::Device vkbDevice = deviceResult.value();
    device = vkbDevice.device;
    std::printf("Logical device created\n");

    auto queueResult = vkbDevice.get_queue(vkb::QueueType::graphics);
    auto queueFamilyResult = vkbDevice.get_queue_index(vkb::QueueType::graphics);
    if (!queueResult || !queueFamilyResult) {
        std::fprintf(stderr, "Failed to get graphics queue\n");
        return false;
    }
    graphicsQueue = queueResult.value();
    graphicsQueueFamily = queueFamilyResult.value();
    std::printf("Graphics queue family: %u\n", graphicsQueueFamily);

    return true;
}
Renderer::~Renderer()
{
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
        vkDestroyDevice(device, nullptr);
    }
    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }
    if (debugMessenger != VK_NULL_HANDLE) {
        vkb::destroy_debug_utils_messenger(instance, debugMessenger);
    }
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
    }
    if (window != nullptr) {
        glfwDestroyWindow(window);
    }
    if (glfwInitialized) {
        glfwTerminate();
    }
}