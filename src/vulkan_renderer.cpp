#include "vulkan_renderer.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>

#include <SDL3/SDL_vulkan.h>

namespace
{
constexpr std::uint32_t kWorkgroupSize = 16;
constexpr VkFormat kAccumulationFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
constexpr VkFormat kRenderTargetFormat = VK_FORMAT_R8G8B8A8_UNORM;
constexpr std::size_t kMaxSphereBytes = sizeof(GpuSphere) * 64;
constexpr std::size_t kMaxOverlayBytes = sizeof(std::uint32_t) * kOverlayPixelCount;
constexpr const char* kShaderPath = GPU_RAYTRACER_SHADER_PATH;
constexpr const char* kPresentShaderPath = GPU_PRESENT_SHADER_PATH;

void TransitionImage(
    VkCommandBuffer commandBuffer,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkAccessFlags srcAccessMask,
    VkAccessFlags dstAccessMask,
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask
)
{
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = srcAccessMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageMask,
        dstStageMask,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier
    );
}
} // namespace

VulkanRenderer::~VulkanRenderer()
{
    Shutdown();
}

void VulkanRenderer::Initialize(
    SDL_Window* window,
    std::span<const GpuSphere> spheres,
    InitProgressFn progress
)
{
    m_window = window;

    if (progress)
    {
        progress("Creating Vulkan instance...", 0.16f);
    }

    CreateInstance();
    if (progress)
    {
        progress("Creating presentation surface...", 0.28f);
    }
    CreateSurface();
    if (progress)
    {
        progress("Selecting GPU and queue family...", 0.40f);
    }
    PickPhysicalDevice();
    if (progress)
    {
        progress("Creating logical device...", 0.52f);
    }
    CreateDevice();
    if (progress)
    {
        progress("Allocating command and sync objects...", 0.64f);
    }
    CreateCommandObjects();
    CreateSyncObjects();
    if (progress)
    {
        progress("Uploading scene buffers...", 0.74f);
    }
    CreateStaticBuffers(spheres);
    if (progress)
    {
        progress("Building descriptor sets...", 0.82f);
    }
    CreateDescriptorObjects();
    CreatePresentDescriptorObjects();
    if (progress)
    {
        progress("Creating compute pipeline...", 0.90f);
    }
    CreateComputePipeline();
    CreatePresentPipeline();
}

void VulkanRenderer::Shutdown()
{
    if (m_device == VK_NULL_HANDLE)
    {
        return;
    }

    vkDeviceWaitIdle(m_device);

    DestroySwapchain();

    if (m_pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
    }
    if (m_pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    }
    if (m_shaderModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(m_device, m_shaderModule, nullptr);
    }
    if (m_presentPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_device, m_presentPipeline, nullptr);
    }
    if (m_presentPipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(m_device, m_presentPipelineLayout, nullptr);
    }
    if (m_presentShaderModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(m_device, m_presentShaderModule, nullptr);
    }
    if (m_descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    }
    if (m_presentDescriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(m_device, m_presentDescriptorPool, nullptr);
    }
    if (m_descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
    }
    if (m_presentDescriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(m_device, m_presentDescriptorSetLayout, nullptr);
    }

    DestroyBuffer(m_device, m_sphereBuffer);
    DestroyBuffer(m_device, m_overlayBuffer);
    DestroyBuffer(m_device, m_paramsBuffer);

    if (m_frameFence != VK_NULL_HANDLE)
    {
        vkDestroyFence(m_device, m_frameFence, nullptr);
    }
    if (m_renderFinishedSemaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(m_device, m_renderFinishedSemaphore, nullptr);
    }
    if (m_imageAvailableSemaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(m_device, m_imageAvailableSemaphore, nullptr);
    }
    if (m_commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    }

    vkDestroyDevice(m_device, nullptr);
    m_device = VK_NULL_HANDLE;

    if (m_surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::Resize(
    std::uint32_t windowWidth,
    std::uint32_t windowHeight,
    std::uint32_t renderWidth,
    std::uint32_t renderHeight
)
{
    if (windowWidth == 0 || windowHeight == 0 || renderWidth == 0 || renderHeight == 0)
    {
        return;
    }

    vkDeviceWaitIdle(m_device);
    DestroySwapchain();
    CreateSwapchain(windowWidth, windowHeight);
    CreateRenderTarget(renderWidth, renderHeight);
    UpdateDescriptorSet();
    ResetAccumulation();
}

void VulkanRenderer::Render(const GpuFrameParams& params, std::span<const std::uint32_t> overlayPixels)
{
    if (m_swapchain == VK_NULL_HANDLE || m_renderTarget.image == VK_NULL_HANDLE)
    {
        return;
    }

    if (overlayPixels.size_bytes() != kMaxOverlayBytes)
    {
        throw std::runtime_error("Overlay buffer size mismatch");
    }

    GpuFrameParams uploadParams = params;
    uploadParams.frameInfo.x = static_cast<float>(m_accumulatedFrames);
    uploadParams.frameInfo.y = static_cast<float>(m_accumulatedFrames + 1);
    std::memcpy(m_paramsBuffer.mapped, &uploadParams, sizeof(uploadParams));
    std::memcpy(m_overlayBuffer.mapped, overlayPixels.data(), kMaxOverlayBytes);

    CheckVk(vkWaitForFences(m_device, 1, &m_frameFence, VK_TRUE, UINT64_MAX), "vkWaitForFences");
    CheckVk(vkResetFences(m_device, 1, &m_frameFence), "vkResetFences");

    std::uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(
        m_device,
        m_swapchain,
        UINT64_MAX,
        m_imageAvailableSemaphore,
        VK_NULL_HANDLE,
        &imageIndex
    );
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return;
    }
    CheckVk(acquireResult, "vkAcquireNextImageKHR");

    CheckVk(vkResetCommandBuffer(m_commandBuffer, 0), "vkResetCommandBuffer");
    RecordCommandBuffer(imageIndex);

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &m_imageAvailableSemaphore,
        .pWaitDstStageMask = &waitStage,
        .commandBufferCount = 1,
        .pCommandBuffers = &m_commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &m_renderFinishedSemaphore,
    };
    CheckVk(vkQueueSubmit(m_queue, 1, &submitInfo, m_frameFence), "vkQueueSubmit");

    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &m_renderFinishedSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &m_swapchain,
        .pImageIndices = &imageIndex,
    };
    const VkResult presentResult = vkQueuePresentKHR(m_queue, &presentInfo);
    if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR &&
        presentResult != VK_ERROR_OUT_OF_DATE_KHR)
    {
        CheckVk(presentResult, "vkQueuePresentKHR");
    }

    ++m_accumulatedFrames;
}

void VulkanRenderer::ResetAccumulation()
{
    m_accumulatedFrames = 0;
    m_accumulationPrimed = false;
}

void VulkanRenderer::CreateInstance()
{
    Uint32 extensionCount = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (extensions == nullptr)
    {
        throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed");
    }

    VkApplicationInfo applicationInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "gpu-raytracer",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
        .pEngineName = "none",
        .engineVersion = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &applicationInfo,
        .enabledExtensionCount = extensionCount,
        .ppEnabledExtensionNames = extensions,
    };
    CheckVk(vkCreateInstance(&createInfo, nullptr, &m_instance), "vkCreateInstance");
}

void VulkanRenderer::CreateSurface()
{
    if (!SDL_Vulkan_CreateSurface(m_window, m_instance, nullptr, &m_surface))
    {
        throw std::runtime_error("SDL_Vulkan_CreateSurface failed");
    }
}

void VulkanRenderer::PickPhysicalDevice()
{
    std::uint32_t deviceCount = 0;
    CheckVk(
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr),
        "vkEnumeratePhysicalDevices"
    );

    std::vector<VkPhysicalDevice> devices(deviceCount);
    CheckVk(
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data()),
        "vkEnumeratePhysicalDevices"
    );

    for (VkPhysicalDevice device : devices)
    {
        m_physicalDevice = device;
        try
        {
            m_queueFamilyIndex = ChooseQueueFamily();
            return;
        }
        catch (const std::exception&)
        {
        }
    }

    throw std::runtime_error("Failed to find a Vulkan device with graphics, compute, and present");
}

void VulkanRenderer::CreateDevice()
{
    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = m_queueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };

    const std::array deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
    };
    CheckVk(
        vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device),
        "vkCreateDevice"
    );
    vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_queue);
}

void VulkanRenderer::CreateCommandObjects()
{
    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_queueFamilyIndex,
    };
    CheckVk(
        vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool),
        "vkCreateCommandPool"
    );

    VkCommandBufferAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m_commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    CheckVk(
        vkAllocateCommandBuffers(m_device, &allocateInfo, &m_commandBuffer),
        "vkAllocateCommandBuffers"
    );
}

void VulkanRenderer::CreateSyncObjects()
{
    VkSemaphoreCreateInfo semaphoreInfo = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    CheckVk(
        vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphore),
        "vkCreateSemaphore"
    );
    CheckVk(
        vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphore),
        "vkCreateSemaphore"
    );

    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    CheckVk(vkCreateFence(m_device, &fenceInfo, nullptr, &m_frameFence), "vkCreateFence");
}

void VulkanRenderer::CreateStaticBuffers(std::span<const GpuSphere> spheres)
{
    m_paramsBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        sizeof(GpuFrameParams),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );

    m_sphereBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        kMaxSphereBytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    std::memcpy(m_sphereBuffer.mapped, spheres.data(), spheres.size_bytes());

    m_overlayBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        kMaxOverlayBytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
}

void VulkanRenderer::CreateDescriptorObjects()
{
    const std::array bindings = {
        VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<std::uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };
    CheckVk(
        vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout),
        "vkCreateDescriptorSetLayout"
    );

    const std::array poolSizes = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
    };
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = static_cast<std::uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };
    CheckVk(
        vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool),
        "vkCreateDescriptorPool"
    );

    VkDescriptorSetAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_descriptorSetLayout,
    };
    CheckVk(
        vkAllocateDescriptorSets(m_device, &allocateInfo, &m_descriptorSet),
        "vkAllocateDescriptorSets"
    );
}

void VulkanRenderer::CreatePresentDescriptorObjects()
{
    const std::array bindings = {
        VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<std::uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };
    CheckVk(
        vkCreateDescriptorSetLayout(
            m_device,
            &layoutInfo,
            nullptr,
            &m_presentDescriptorSetLayout
        ),
        "vkCreateDescriptorSetLayout"
    );

    const std::array poolSizes = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
    };
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = static_cast<std::uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };
    CheckVk(
        vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_presentDescriptorPool),
        "vkCreateDescriptorPool"
    );

    VkDescriptorSetAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_presentDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_presentDescriptorSetLayout,
    };
    CheckVk(
        vkAllocateDescriptorSets(m_device, &allocateInfo, &m_presentDescriptorSet),
        "vkAllocateDescriptorSets"
    );
}

void VulkanRenderer::CreateComputePipeline()
{
    const std::vector<std::byte> shaderBytes = ReadBinaryFile(kShaderPath);

    VkShaderModuleCreateInfo moduleInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shaderBytes.size(),
        .pCode = reinterpret_cast<const std::uint32_t*>(shaderBytes.data()),
    };
    CheckVk(
        vkCreateShaderModule(m_device, &moduleInfo, nullptr, &m_shaderModule),
        "vkCreateShaderModule"
    );

    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &m_descriptorSetLayout,
    };
    CheckVk(
        vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_pipelineLayout),
        "vkCreatePipelineLayout"
    );

    VkPipelineShaderStageCreateInfo shaderStage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = m_shaderModule,
        .pName = "main",
    };
    VkComputePipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = shaderStage,
        .layout = m_pipelineLayout,
    };
    CheckVk(
        vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline),
        "vkCreateComputePipelines"
    );
}

void VulkanRenderer::CreatePresentPipeline()
{
    const std::vector<std::byte> shaderBytes = ReadBinaryFile(kPresentShaderPath);

    VkShaderModuleCreateInfo moduleInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shaderBytes.size(),
        .pCode = reinterpret_cast<const std::uint32_t*>(shaderBytes.data()),
    };
    CheckVk(
        vkCreateShaderModule(m_device, &moduleInfo, nullptr, &m_presentShaderModule),
        "vkCreateShaderModule"
    );

    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &m_presentDescriptorSetLayout,
    };
    CheckVk(
        vkCreatePipelineLayout(
            m_device,
            &layoutInfo,
            nullptr,
            &m_presentPipelineLayout
        ),
        "vkCreatePipelineLayout"
    );

    VkPipelineShaderStageCreateInfo shaderStage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = m_presentShaderModule,
        .pName = "main",
    };
    VkComputePipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = shaderStage,
        .layout = m_presentPipelineLayout,
    };
    CheckVk(
        vkCreateComputePipelines(
            m_device,
            VK_NULL_HANDLE,
            1,
            &pipelineInfo,
            nullptr,
            &m_presentPipeline
        ),
        "vkCreateComputePipelines"
    );
}

void VulkanRenderer::CreateSwapchain(std::uint32_t width, std::uint32_t height)
{
    VkSurfaceCapabilitiesKHR capabilities;
    CheckVk(
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities),
        "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"
    );

    std::uint32_t formatCount = 0;
    CheckVk(
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr),
        "vkGetPhysicalDeviceSurfaceFormatsKHR"
    );
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    CheckVk(
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            m_physicalDevice,
            m_surface,
            &formatCount,
            formats.data()
        ),
        "vkGetPhysicalDeviceSurfaceFormatsKHR"
    );

    std::uint32_t presentModeCount = 0;
    CheckVk(
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            m_physicalDevice,
            m_surface,
            &presentModeCount,
            nullptr
        ),
        "vkGetPhysicalDeviceSurfacePresentModesKHR"
    );
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    CheckVk(
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            m_physicalDevice,
            m_surface,
            &presentModeCount,
            presentModes.data()
        ),
        "vkGetPhysicalDeviceSurfacePresentModesKHR"
    );

    if ((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0)
    {
        throw std::runtime_error("Swapchain images do not support transfer-dst usage");
    }

    const VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(formats);
    const VkPresentModeKHR presentMode = ChoosePresentMode(presentModes);

    m_swapchainExtent.width =
        std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    m_swapchainExtent.height =
        std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    std::uint32_t imageCount = std::max(2u, capabilities.minImageCount);
    if (capabilities.maxImageCount > 0)
    {
        imageCount = std::min(imageCount, capabilities.maxImageCount);
    }

    VkSwapchainCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = m_surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = m_swapchainExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
    };
    CheckVk(
        vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain),
        "vkCreateSwapchainKHR"
    );
    m_swapchainFormat = surfaceFormat.format;

    std::uint32_t swapchainImageCount = 0;
    CheckVk(
        vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchainImageCount, nullptr),
        "vkGetSwapchainImagesKHR"
    );
    m_swapchainImages.resize(swapchainImageCount);
    CheckVk(
        vkGetSwapchainImagesKHR(
            m_device,
            m_swapchain,
            &swapchainImageCount,
            m_swapchainImages.data()
        ),
        "vkGetSwapchainImagesKHR"
    );
}

void VulkanRenderer::DestroySwapchain()
{
    DestroyImage(m_device, m_accumulationTarget);
    DestroyImage(m_device, m_presentTarget);
    DestroyImage(m_device, m_renderTarget);
    m_renderWidth = 0;
    m_renderHeight = 0;
    m_accumulatedFrames = 0;
    m_renderTargetPrimed = false;
    m_accumulationPrimed = false;

    if (m_swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
    m_swapchainImages.clear();
    m_swapchainFormat = VK_FORMAT_UNDEFINED;
    m_swapchainExtent = {};
}

void VulkanRenderer::CreateRenderTarget(std::uint32_t width, std::uint32_t height)
{
    m_accumulationTarget = CreateImage2D(
        m_physicalDevice,
        m_device,
        width,
        height,
        kAccumulationFormat,
        VK_IMAGE_USAGE_STORAGE_BIT
    );
    m_renderTarget = CreateImage2D(
        m_physicalDevice,
        m_device,
        width,
        height,
        kRenderTargetFormat,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
    );
    m_presentTarget = CreateImage2D(
        m_physicalDevice,
        m_device,
        m_swapchainExtent.width,
        m_swapchainExtent.height,
        kRenderTargetFormat,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
    );
    m_renderWidth = width;
    m_renderHeight = height;
    m_accumulatedFrames = 0;
    m_renderTargetPrimed = false;
    m_accumulationPrimed = false;
}

void VulkanRenderer::UpdateDescriptorSet()
{
    VkDescriptorImageInfo imageInfo = {
        .imageView = m_renderTarget.view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    VkDescriptorImageInfo accumulationInfo = {
        .imageView = m_accumulationTarget.view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    VkDescriptorBufferInfo paramsInfo = {
        .buffer = m_paramsBuffer.buffer,
        .offset = 0,
        .range = sizeof(GpuFrameParams),
    };
    VkDescriptorBufferInfo sphereInfo = {
        .buffer = m_sphereBuffer.buffer,
        .offset = 0,
        .range = kMaxSphereBytes,
    };
    VkDescriptorBufferInfo overlayInfo = {
        .buffer = m_overlayBuffer.buffer,
        .offset = 0,
        .range = kMaxOverlayBytes,
    };

    const std::array writes = {
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_descriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &imageInfo,
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_descriptorSet,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &accumulationInfo,
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_descriptorSet,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &paramsInfo,
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_descriptorSet,
            .dstBinding = 3,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &sphereInfo,
        },
    };

    vkUpdateDescriptorSets(
        m_device,
        static_cast<std::uint32_t>(writes.size()),
        writes.data(),
        0,
        nullptr
    );

    VkDescriptorImageInfo presentInputInfo = {
        .imageView = m_renderTarget.view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    VkDescriptorImageInfo presentOutputInfo = {
        .imageView = m_presentTarget.view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    const std::array presentWrites = {
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_presentDescriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &presentInputInfo,
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_presentDescriptorSet,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &presentOutputInfo,
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_presentDescriptorSet,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &paramsInfo,
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_presentDescriptorSet,
            .dstBinding = 3,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &overlayInfo,
        },
    };

    vkUpdateDescriptorSets(
        m_device,
        static_cast<std::uint32_t>(presentWrites.size()),
        presentWrites.data(),
        0,
        nullptr
    );
}

void VulkanRenderer::RecordCommandBuffer(std::uint32_t swapchainImageIndex)
{
    const std::uint32_t groupsX = (m_renderWidth + kWorkgroupSize - 1) / kWorkgroupSize;
    const std::uint32_t groupsY = (m_renderHeight + kWorkgroupSize - 1) / kWorkgroupSize;
    const std::uint32_t presentGroupsX =
        (m_swapchainExtent.width + kWorkgroupSize - 1) / kWorkgroupSize;
    const std::uint32_t presentGroupsY =
        (m_swapchainExtent.height + kWorkgroupSize - 1) / kWorkgroupSize;

    VkCommandBufferBeginInfo beginInfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    CheckVk(vkBeginCommandBuffer(m_commandBuffer, &beginInfo), "vkBeginCommandBuffer");

    const VkImageLayout renderTargetOldLayout =
        m_renderTargetPrimed ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;
    const VkImageLayout accumulationOldLayout =
        m_accumulationPrimed ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;
    const VkAccessFlags accumulationSrcAccess =
        m_accumulationPrimed ? VK_ACCESS_SHADER_WRITE_BIT : 0;
    const VkPipelineStageFlags accumulationSrcStage = m_accumulationPrimed
                                                          ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                                                          : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    TransitionImage(
        m_commandBuffer,
        m_renderTarget.image,
        renderTargetOldLayout,
        VK_IMAGE_LAYOUT_GENERAL,
        0,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
    );
    TransitionImage(
        m_commandBuffer,
        m_accumulationTarget.image,
        accumulationOldLayout,
        VK_IMAGE_LAYOUT_GENERAL,
        accumulationSrcAccess,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        accumulationSrcStage,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
    );
    TransitionImage(
        m_commandBuffer,
        m_presentTarget.image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        0,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
    );
    TransitionImage(
        m_commandBuffer,
        m_swapchainImages[swapchainImageIndex],
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT
    );

    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    vkCmdBindDescriptorSets(
        m_commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_pipelineLayout,
        0,
        1,
        &m_descriptorSet,
        0,
        nullptr
    );
    vkCmdDispatch(m_commandBuffer, groupsX, groupsY, 1);

    TransitionImage(
        m_commandBuffer,
        m_renderTarget.image,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
    );

    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_presentPipeline);
    vkCmdBindDescriptorSets(
        m_commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_presentPipelineLayout,
        0,
        1,
        &m_presentDescriptorSet,
        0,
        nullptr
    );
    vkCmdDispatch(m_commandBuffer, presentGroupsX, presentGroupsY, 1);

    TransitionImage(
        m_commandBuffer,
        m_presentTarget.image,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT
    );

    VkImageBlit blit = {};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.layerCount = 1;
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] =
        {static_cast<int32_t>(m_swapchainExtent.width), static_cast<int32_t>(m_swapchainExtent.height), 1};
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.layerCount = 1;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {
        static_cast<int32_t>(m_swapchainExtent.width),
        static_cast<int32_t>(m_swapchainExtent.height),
        1
    };
    vkCmdBlitImage(
        m_commandBuffer,
        m_presentTarget.image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        m_swapchainImages[swapchainImageIndex],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &blit,
        VK_FILTER_NEAREST
    );

    TransitionImage(
        m_commandBuffer,
        m_swapchainImages[swapchainImageIndex],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        0,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
    );

    CheckVk(vkEndCommandBuffer(m_commandBuffer), "vkEndCommandBuffer");
    m_renderTargetPrimed = true;
    m_accumulationPrimed = true;
}

VkSurfaceFormatKHR
VulkanRenderer::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const
{
    for (const VkSurfaceFormatKHR& format : formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM)
        {
            return format;
        }
    }

    return formats.front();
}

VkPresentModeKHR VulkanRenderer::ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes
) const
{
    for (const VkPresentModeKHR mode : presentModes)
    {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return mode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

std::uint32_t VulkanRenderer::ChooseQueueFamily() const
{
    std::uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, families.data());

    for (std::uint32_t index = 0; index < queueFamilyCount; ++index)
    {
        VkBool32 supportsPresent = VK_FALSE;
        CheckVk(
            vkGetPhysicalDeviceSurfaceSupportKHR(
                m_physicalDevice,
                index,
                m_surface,
                &supportsPresent
            ),
            "vkGetPhysicalDeviceSurfaceSupportKHR"
        );

        const VkQueueFlags flags = families[index].queueFlags;
        const bool supportsGraphics = (flags & VK_QUEUE_GRAPHICS_BIT) != 0;
        const bool supportsCompute = (flags & VK_QUEUE_COMPUTE_BIT) != 0;

        if (supportsGraphics && supportsCompute && supportsPresent == VK_TRUE)
        {
            return index;
        }
    }

    throw std::runtime_error("No compatible queue family found");
}
