#include "renderer.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>

#include <cstdint>
#include <cstdio>

bool Renderer::init(){
    if (!initWindow()) {
        return false;
    }
    if (!initVulkan()) {
        return false;
    }

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);

    if (!createSwapchain(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height))) {
        return false;
    }

    if (!initFrames()) {
        return false;
    }

    return true;
}

void Renderer::run(){
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }
}

bool Renderer::initWindow(){
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

bool Renderer::initVulkan(){
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

bool Renderer::createSwapchain(std::uint32_t width, std::uint32_t height){
    vkb::SwapchainBuilder swapChainBuidler{physicalDevice,device,surface};

    auto swapChainResult = swapChainBuidler
        .set_desired_format(VkSurfaceFormatKHR{
            .format = VK_FORMAT_B8G8R8A8_UNORM,
            .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        })
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width,height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build();
    if(!swapChainResult){
        std::fprintf(stderr, "Failed to create swapchain: %s\n",
            swapChainResult.error().message().c_str());
        return false;
    }
    vkb::Swapchain vkbSwapchain = swapChainResult.value();
    swapchain = vkbSwapchain.swapchain;
    swapchainImageFormat = vkbSwapchain.image_format;
    swapchainExtent = vkbSwapchain.extent;

    auto imagesResult = vkbSwapchain.get_images();
    if (!imagesResult) {
        std::fprintf(stderr, "Failed to get swapchain images: %s\n",
                     imagesResult.error().message().c_str());
        return false;
    }
        swapchainImages = imagesResult.value();

    auto viewsResult = vkbSwapchain.get_image_views();
    if (!viewsResult) {
        std::fprintf(stderr, "Failed to create swapchain image views: %s\n",
                     viewsResult.error().message().c_str());
        return false;
    }
    swapchainImageViews = viewsResult.value();
    
    renderFinishedSemaphores.resize(swapchainImages.size(), VK_NULL_HANDLE);
    VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    for (VkSemaphore& semaphore : renderFinishedSemaphores) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) {
            std::fprintf(stderr, "Failed to create render finished semaphore\n");
            return false;
        }
    }

    std::printf("Swapchain created: %zu images, %ux%u\n",
                swapchainImages.size(), swapchainExtent.width, swapchainExtent.height);
    return true;


}
void Renderer::destroySwapchain(){
    for (VkSemaphore semaphore : renderFinishedSemaphores) {
        vkDestroySemaphore(device, semaphore, nullptr);
    }
    renderFinishedSemaphores.clear();

    for (VkImageView view : swapchainImageViews) {
        vkDestroyImageView(device, view, nullptr);
    }
    swapchainImageViews.clear();
    swapchainImages.clear();

    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
}

bool Renderer:: initFrames(){
    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphicsQueueFamily
    };

    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    for(FrameData& frame : frames){
        if (vkCreateCommandPool(device, &poolInfo, nullptr, &frame.commandPool) != VK_SUCCESS) {
            std::fprintf(stderr, "Failed to create command pool\n");
            return false;
        }

        VkCommandBufferAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = frame.commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        if (vkAllocateCommandBuffers(device, &allocInfo, &frame.commandBuffer) != VK_SUCCESS) {
            std::fprintf(stderr, "Failed to allocate command buffer\n");
            return false;
        }

        if (vkCreateFence(device, &fenceInfo, nullptr, &frame.renderFence) != VK_SUCCESS) {
            std::fprintf(stderr, "Failed to create fence\n");
            return false;
        }

        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frame.imageAvailableSemaphore) != VK_SUCCESS) {
            std::fprintf(stderr, "Failed to create semaphore\n");
            return false;
        }
    }
    std::printf("Frame Resources created\n");
    return true;
}
void Renderer::destroyFrames(){
        for (FrameData& frame : frames) {
        vkDestroySemaphore(device, frame.imageAvailableSemaphore, nullptr);
        vkDestroyFence(device, frame.renderFence, nullptr);
        vkDestroyCommandPool(device, frame.commandPool, nullptr);
        frame = FrameData{};
    }
}

Renderer::~Renderer(){
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
        destroyFrames();
        destroySwapchain();
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