#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

#include <SDL3/SDL_video.h>
#include <vulkan/vulkan.h>

#include "gpu_types.h"
#include "vulkan_helpers.h"

struct VulkanRenderer
{
    using InitProgressFn = std::function<void(std::string_view, float)>;

    VulkanRenderer() = default;
    ~VulkanRenderer();

    void Initialize(
        SDL_Window* window,
        std::span<const GpuSphere> spheres,
        std::span<const GpuTriangle> triangles,
        InitProgressFn progress = {}
    );
    void Shutdown();

    void Resize(
        std::uint32_t windowWidth,
        std::uint32_t windowHeight,
        std::uint32_t renderWidth,
        std::uint32_t renderHeight
    );
    void Render(const GpuFrameParams& params, std::span<const std::uint32_t> overlayPixels);
    void ResetAccumulation();

    void CreateInstance();
    void CreateSurface();
    void PickPhysicalDevice();
    void CreateDevice();
    void CreateCommandObjects();
    void CreateSyncObjects();
    void CreateStaticBuffers(
        std::span<const GpuSphere> spheres,
        std::span<const GpuTriangle> triangles
    );
    void CreateDescriptorObjects();
    void CreatePresentDescriptorObjects();
    void CreateComputePipeline();
    void CreatePresentPipeline();
    void CreateSwapchain(std::uint32_t width, std::uint32_t height);
    void DestroySwapchain();
    void CreateRenderTarget(std::uint32_t width, std::uint32_t height);
    void UpdateDescriptorSet();
    void RecordCommandBuffer(std::uint32_t swapchainImageIndex);

    VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes) const;
    std::uint32_t ChooseQueueFamily() const;

    SDL_Window* m_window = nullptr;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    std::uint32_t m_queueFamilyIndex = 0;
    VkQueue m_queue = VK_NULL_HANDLE;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_swapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_swapchainExtent = {};
    std::vector<VkImage> m_swapchainImages;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
    VkSemaphore m_imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore m_renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence m_frameFence = VK_NULL_HANDLE;

    BufferResource m_paramsBuffer;
    BufferResource m_overlayBuffer;
    BufferResource m_sphereBuffer;
    BufferResource m_triangleBuffer;
    ImageResource m_accumulationTarget;
    ImageResource m_presentTarget;
    ImageResource m_renderTarget;

    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkShaderModule m_shaderModule = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_presentDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_presentDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_presentDescriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout m_presentPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_presentPipeline = VK_NULL_HANDLE;
    VkShaderModule m_presentShaderModule = VK_NULL_HANDLE;

    std::uint32_t m_renderWidth = 0;
    std::uint32_t m_renderHeight = 0;
    std::uint32_t m_accumulatedFrames = 0;
    std::uint32_t m_sphereCount = 0;
    std::uint32_t m_triangleCount = 0;
    VkDeviceSize m_sphereBufferBytes = 0;
    VkDeviceSize m_triangleBufferBytes = 0;
    bool m_renderTargetPrimed = false;
    bool m_accumulationPrimed = false;
};
