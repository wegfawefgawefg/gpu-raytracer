#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <SDL3/SDL_video.h>
#include <vulkan/vulkan.h>

#include "gpu_types.h"
#include "vulkan_helpers.h"

struct VulkanRenderer
{
    VulkanRenderer() = default;
    ~VulkanRenderer();

    void Initialize(SDL_Window* window, std::span<const GpuSphere> spheres);
    void Shutdown();

    void Resize(std::uint32_t windowWidth, std::uint32_t windowHeight, std::uint32_t renderWidth,
                std::uint32_t renderHeight);
    void Render(const GpuFrameParams& params);

    void CreateInstance();
    void CreateSurface();
    void PickPhysicalDevice();
    void CreateDevice();
    void CreateCommandObjects();
    void CreateSyncObjects();
    void CreateStaticBuffers(std::span<const GpuSphere> spheres);
    void CreateDescriptorObjects();
    void CreateComputePipeline();
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
    BufferResource m_sphereBuffer;
    ImageResource m_renderTarget;

    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkShaderModule m_shaderModule = VK_NULL_HANDLE;

    std::uint32_t m_renderWidth = 0;
    std::uint32_t m_renderHeight = 0;
    bool m_renderTargetPrimed = false;
};
