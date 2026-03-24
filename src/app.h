#pragma once

#include <array>
#include <cstdint>

#include <SDL3/SDL.h>

#include "camera.h"
#include "gpu_types.h"
#include "vulkan_renderer.h"

struct App
{
    ~App();

    void Run();

    void Initialize();
    void Shutdown();
    void HandleEvent(const SDL_Event& event);
    void Update(float deltaSeconds);
    void SyncRendererSize();

    SDL_Window* m_window = nullptr;
    bool m_running = true;
    bool m_captureMouse = false;
    std::uint32_t m_renderDivisor = 2;
    std::uint32_t m_windowWidth = 0;
    std::uint32_t m_windowHeight = 0;
    std::uint32_t m_renderWidth = 0;
    std::uint32_t m_renderHeight = 0;

    Camera m_camera;
    VulkanRenderer m_renderer;
    std::array<GpuSphere, 3> m_spheres = BuildDefaultScene();
};
