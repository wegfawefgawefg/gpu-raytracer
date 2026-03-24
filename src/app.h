#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "camera.h"
#include "gpu_types.h"
#include "loading_screen.h"
#include "mesh_loader.h"
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
    void ResetAccumulation();
    void UpdateOverlayText();

    SDL_Window* m_window = nullptr;
    TTF_Font* m_uiFont = nullptr;
    bool m_running = true;
    bool m_captureMouse = false;
    std::uint32_t m_renderDivisor = 2;
    std::uint32_t m_windowWidth = 0;
    std::uint32_t m_windowHeight = 0;
    std::uint32_t m_renderWidth = 0;
    std::uint32_t m_renderHeight = 0;
    std::uint32_t m_overlayWidth = 0;
    std::uint32_t m_overlayHeight = 0;
    float m_smoothedFps = 0.0f;
    float m_overlayRefreshSeconds = 0.0f;

    Camera m_camera;
    LoadingScreen m_loadingScreen;
    VulkanRenderer m_renderer;
    std::vector<GpuSphere> m_spheres;
    std::vector<GpuTriangle> m_triangles;
    std::array<std::uint32_t, kOverlayPixelCount> m_overlayPixels = {};
};
