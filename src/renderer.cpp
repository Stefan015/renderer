#include "renderer.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>

static void transitionImage(VkCommandBuffer cmd, VkImage image,
                            VkImageLayout oldLayout, VkImageLayout newLayout){
    VkImageMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        },
    };

    VkDependencyInfo dependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };

    vkCmdPipelineBarrier2(cmd, &dependencyInfo);
}

static bool loadShaderModule(VkDevice device, const char* fileName, VkShaderModule& outModule){
    std::string path = std::string(SHADER_DIR) + "/" + fileName;

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::fprintf(stderr, "Failed to open shader file: %s\n", path.c_str());
        return false;
    }

    std::size_t fileSize = static_cast<std::size_t>(file.tellg());
    if (fileSize == 0 || fileSize % sizeof(std::uint32_t) != 0) {
        std::fprintf(stderr, "Invalid SPIR-V file size: %s\n", path.c_str());
        return false;
    }

    std::vector<std::uint32_t> code(fileSize / sizeof(std::uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(fileSize));

    VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fileSize,
        .pCode = code.data(),
    };
    if (vkCreateShaderModule(device, &createInfo, nullptr, &outModule) != VK_SUCCESS) {
        std::fprintf(stderr, "Failed to create shader module: %s\n", path.c_str());
        return false;
    }

    return true;
}

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
    if (!initPipeline()) {
        return false;
    }

    return true;
}

void Renderer::run(){
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        drawFrame();
    }
}
void Renderer::drawFrame()
{
    FrameData& frame = frames[frameNumber % FRAMES_IN_FLIGHT];

    vkWaitForFences(device, 1, &frame.renderFence, VK_TRUE, UINT64_MAX);

    std::uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                                   frame.imageAvailableSemaphore,
                                                   VK_NULL_HANDLE, &imageIndex);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }

    vkResetFences(device, 1, &frame.renderFence);

    VkCommandBuffer cmd = frame.commandBuffer;
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkImage image = swapchainImages[imageIndex];

    transitionImage(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    float blue = std::abs(std::sin(static_cast<float>(frameNumber) / 120.0f));

    VkRenderingAttachmentInfo colorAttachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = swapchainImageViews[imageIndex],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {
            .color = {
                .float32 = {0.0f, 0.0f, blue * 0.3f, 1.0f},
            },
        },
    };

    VkRenderingInfo renderingInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .offset = {0, 0},
            .extent = swapchainExtent,
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment,
    };

    vkCmdBeginRendering(cmd, &renderingInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);

    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(swapchainExtent.width),
        .height = static_cast<float>(swapchainExtent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{
        .offset = {0, 0},
        .extent = swapchainExtent,
    };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRendering(cmd);

    transitionImage(cmd, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    vkEndCommandBuffer(cmd);

    VkCommandBufferSubmitInfo commandBufferInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmd,
    };
    VkSemaphoreSubmitInfo waitInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = frame.imageAvailableSemaphore,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    };
    VkSemaphoreSubmitInfo signalInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = renderFinishedSemaphores[imageIndex],
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    };
    VkSubmitInfo2 submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &waitInfo,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &commandBufferInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signalInfo,
    };
    vkQueueSubmit2(graphicsQueue, 1, &submitInfo, frame.renderFence);

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinishedSemaphores[imageIndex],
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &imageIndex,
    };

    VkResult presentResult = vkQueuePresentKHR(graphicsQueue, &presentInfo);

    ++frameNumber;

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapchain();
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

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

    if (!window) {
        std::fprintf(stderr, "Failed to create window\n");
        return false;
    }

    return true;
}
void Renderer::framebufferResizeCallback(GLFWwindow* window, int, int)
{
    auto* renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(window));
    renderer->framebufferResized = true;
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
void Renderer::recreateSwapchain(){
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        if (glfwWindowShouldClose(window)) {
            return;
        }
        glfwWaitEvents();
        glfwGetFramebufferSize(window, &width, &height);
    }

    vkDeviceWaitIdle(device);
    destroySwapchain();
    if (!createSwapchain(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height))) {
        std::fprintf(stderr, "Failed to recreate swapchain\n");
        glfwSetWindowShouldClose(window, GLFW_TRUE);
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

bool Renderer::initPipeline()
{
    VkShaderModule vertShader = VK_NULL_HANDLE;
    VkShaderModule fragShader = VK_NULL_HANDLE;

    if (!loadShaderModule(device, "triangle.vert.spv", vertShader)) {
        return false;
    }
    if (!loadShaderModule(device, "triangle.frag.spv", fragShader)) {
        vkDestroyShaderModule(device, vertShader, nullptr);
        return false;
    }

    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };
    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        std::fprintf(stderr, "Failed to create pipeline layout\n");
        vkDestroyShaderModule(device, fragShader, nullptr);
        vkDestroyShaderModule(device, vertShader, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertShader,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragShader,
            .pName = "main",
        },
    };

    VkPipelineVertexInputStateCreateInfo vertexInput{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisampling{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo colorBlending{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
    };

    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamicState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamicStates,
    };

    VkPipelineRenderingCreateInfo renderingInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapchainImageFormat,
    };

    VkGraphicsPipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderingInfo,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = pipelineLayout,
    };

    VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                                nullptr, &trianglePipeline);

    vkDestroyShaderModule(device, fragShader, nullptr);
    vkDestroyShaderModule(device, vertShader, nullptr);

    if (result != VK_SUCCESS) {
        std::fprintf(stderr, "Failed to create graphics pipeline\n");
        return false;
    }

    std::printf("Graphics pipeline created\n");
    return true;
}

Renderer::~Renderer(){
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
        vkDestroyPipeline(device, trianglePipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        destroyFrames();
        destroySwapchain();
        vkDestroyDevice(device, nullptr);
    }  vkDestroyDevice(device, nullptr);
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