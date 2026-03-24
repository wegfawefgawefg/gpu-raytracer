#include "app.h"

#include <algorithm>
#include <cstdio>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace
{
constexpr std::string_view kWindowTitle = "gpu-raytracer";
constexpr std::string_view kUiFontPath = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf";
constexpr std::string_view kSampleMeshPath = GPU_SAMPLE_MESH_PATH;
constexpr std::string_view kSampleTexturePath = GPU_SAMPLE_TEXTURE_PATH;
constexpr int kInitialWindowWidth = 1440;
constexpr int kInitialWindowHeight = 900;
constexpr std::string_view kX11DialogWindowType = "_NET_WM_WINDOW_TYPE_DIALOG";
constexpr float kOverlayRefreshPeriod = 0.12f;

void LogSdlError(std::string_view context)
{
    SDL_Log("%s: %s", context.data(), SDL_GetError());
}

void CenterWindowOnPrimaryDisplay(SDL_Window* window)
{
    const SDL_DisplayID primaryDisplay = SDL_GetPrimaryDisplay();
    if (primaryDisplay == 0)
    {
        return;
    }

    SDL_Rect bounds;
    if (!SDL_GetDisplayBounds(primaryDisplay, &bounds))
    {
        return;
    }

    const int centeredX = bounds.x + (bounds.w - kInitialWindowWidth) / 2;
    const int centeredY = bounds.y + (bounds.h - kInitialWindowHeight) / 2;
    SDL_SetWindowPosition(window, centeredX, centeredY);
}
} // namespace

App::~App()
{
    Shutdown();
}

void App::Run()
{
    Initialize();

    auto previousTime = std::chrono::steady_clock::now();
    while (m_running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            HandleEvent(event);
        }

        const auto currentTime = std::chrono::steady_clock::now();
        const std::chrono::duration<float> delta = currentTime - previousTime;
        previousTime = currentTime;

        const float deltaSeconds = delta.count();
        Update(deltaSeconds);

        if (deltaSeconds > 0.0f)
        {
            const float instantFps = 1.0f / deltaSeconds;
            if (m_smoothedFps <= 0.0f)
            {
                m_smoothedFps = instantFps;
            }
            else
            {
                m_smoothedFps = m_smoothedFps * 0.985f + instantFps * 0.015f;
            }
        }

        m_overlayRefreshSeconds -= deltaSeconds;
        if (m_overlayRefreshSeconds <= 0.0f)
        {
            UpdateOverlayText();
            m_overlayRefreshSeconds = kOverlayRefreshPeriod;
        }

        GpuFrameParams params = m_camera.BuildFrameParams(
            m_renderWidth,
            m_renderHeight,
            m_scene.spheres.size(),
            m_scene.triangles.size()
        );
        params.renderInfo.z = static_cast<float>(m_windowWidth);
        params.frameInfo.z = m_accumulationEnabled ? 1.0f : 0.0f;
        params.frameInfo.w = static_cast<float>(m_windowHeight);
        params.overlayInfo =
            {
                static_cast<float>(m_overlayWidth),
                static_cast<float>(m_overlayHeight),
                16.0f,
                16.0f,
            };
        params.sceneInfo =
            {
                static_cast<float>(m_scene.triangles.size()),
                static_cast<float>(m_scene.bvhNodes.size()),
                static_cast<float>(m_scene.materials.size()),
                static_cast<float>(m_scene.textures.size()),
            };
        m_renderer.Render(params, m_overlayPixels);
    }
}

void App::Initialize()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        throw std::runtime_error("SDL_Init failed");
    }

    SDL_SetHint(SDL_HINT_X11_WINDOW_TYPE, kX11DialogWindowType.data());

    m_window = SDL_CreateWindow(
        kWindowTitle.data(),
        kInitialWindowWidth,
        kInitialWindowHeight,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );
    if (m_window == nullptr)
    {
        throw std::runtime_error("SDL_CreateWindow failed");
    }

    if (!TTF_Init())
    {
        throw std::runtime_error("TTF_Init failed");
    }

    m_uiFont = TTF_OpenFont(kUiFontPath.data(), 22.0f);
    if (m_uiFont == nullptr)
    {
        throw std::runtime_error("TTF_OpenFont failed");
    }

    m_scene = BuildSampleScene(kSampleMeshPath, kSampleTexturePath);

    CenterWindowOnPrimaryDisplay(m_window);
    m_loadingScreen.Attach(m_window);
    m_loadingScreen.Update("Booting SDL window...", 0.08f);
    m_renderer.Initialize(
        m_window,
        m_scene,
        [this](std::string_view message, float progress)
        { m_loadingScreen.Update(message, progress); }
    );
    m_loadingScreen.Update("Sizing render targets...", 0.94f);
    SyncRendererSize();
    m_loadingScreen.Update("Startup complete.", 1.0f);
    UpdateOverlayText();
}

void App::Shutdown()
{
    m_renderer.Shutdown();

    if (m_uiFont != nullptr)
    {
        TTF_CloseFont(m_uiFont);
        m_uiFont = nullptr;
    }

    TTF_Quit();

    if (m_window != nullptr)
    {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    SDL_Quit();
}

void App::HandleEvent(const SDL_Event& event)
{
    switch (event.type)
    {
    case SDL_EVENT_QUIT:
        m_running = false;
        break;

    case SDL_EVENT_KEY_DOWN:
        if (event.key.key == SDLK_ESCAPE)
        {
            m_running = false;
        }
        if (event.key.key == SDLK_F1)
        {
            m_renderDivisor = 1;
            SyncRendererSize();
        }
        if (event.key.key == SDLK_F2)
        {
            m_renderDivisor = 2;
            SyncRendererSize();
        }
        if (event.key.key == SDLK_F3)
        {
            m_renderDivisor = 4;
            SyncRendererSize();
        }
        if (event.key.key == SDLK_F4)
        {
            m_accumulationEnabled = !m_accumulationEnabled;
            ResetAccumulation();
            UpdateOverlayText();
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event.button.button == SDL_BUTTON_RIGHT)
        {
            m_captureMouse = true;
            SDL_SetWindowRelativeMouseMode(m_window, true);
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.button == SDL_BUTTON_RIGHT)
        {
            m_captureMouse = false;
            SDL_SetWindowRelativeMouseMode(m_window, false);
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        if (m_captureMouse)
        {
            m_camera.UpdateLook(event.motion.xrel, event.motion.yrel);
            ResetAccumulation();
        }
        break;

    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        SyncRendererSize();
        break;

    default:
        break;
    }
}

void App::Update(float deltaSeconds)
{
    const bool* keys = SDL_GetKeyboardState(nullptr);
    if (keys == nullptr)
    {
        LogSdlError("SDL_GetKeyboardState failed");
        return;
    }

    if (m_camera.UpdateMovement(keys, deltaSeconds))
    {
        ResetAccumulation();
    }
}

void App::SyncRendererSize()
{
    int pixelWidth = 0;
    int pixelHeight = 0;
    if (!SDL_GetWindowSizeInPixels(m_window, &pixelWidth, &pixelHeight))
    {
        LogSdlError("SDL_GetWindowSizeInPixels failed");
        return;
    }

    m_windowWidth = static_cast<std::uint32_t>(std::max(pixelWidth, 1));
    m_windowHeight = static_cast<std::uint32_t>(std::max(pixelHeight, 1));
    m_renderWidth = std::max(1u, m_windowWidth / m_renderDivisor);
    m_renderHeight = std::max(1u, m_windowHeight / m_renderDivisor);
    m_renderer.Resize(m_windowWidth, m_windowHeight, m_renderWidth, m_renderHeight);
    ResetAccumulation();
}

void App::ResetAccumulation()
{
    m_renderer.ResetAccumulation();
}

void App::UpdateOverlayText()
{
    m_overlayPixels.fill(0);
    m_overlayWidth = 0;
    m_overlayHeight = 0;

    if (m_uiFont == nullptr)
    {
        return;
    }

    const float msPerFrame = m_smoothedFps > 0.0f ? 1000.0f / m_smoothedFps : 0.0f;
    char text[128] = {};
    std::snprintf(
        text,
        sizeof(text),
        "%.2f ms   %.0f fps   %ux%u",
        msPerFrame,
        m_smoothedFps,
        m_renderWidth,
        m_renderHeight
    );

    char decoratedText[160] = {};
    std::snprintf(
        decoratedText,
        sizeof(decoratedText),
        "%s   %s",
        text,
        m_accumulationEnabled ? "accum" : "classic"
    );

    const SDL_Color color = {255, 235, 210, 255};
    SDL_Surface* surface = TTF_RenderText_Blended(
        m_uiFont,
        decoratedText,
        std::strlen(decoratedText),
        color
    );
    if (surface == nullptr)
    {
        return;
    }

    SDL_Surface* rgbaSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA8888);
    SDL_DestroySurface(surface);
    if (rgbaSurface == nullptr)
    {
        return;
    }

    m_overlayWidth =
        std::min(static_cast<std::uint32_t>(rgbaSurface->w), kOverlayBufferWidth);
    m_overlayHeight =
        std::min(static_cast<std::uint32_t>(rgbaSurface->h), kOverlayBufferHeight);

    if (m_overlayWidth > 0 && m_overlayHeight > 0 && SDL_LockSurface(rgbaSurface))
    {
        const auto* sourceRows = static_cast<const std::uint8_t*>(rgbaSurface->pixels);
        for (std::uint32_t row = 0; row < m_overlayHeight; ++row)
        {
            const auto* source = sourceRows + row * rgbaSurface->pitch;
            auto* destination =
                reinterpret_cast<std::uint8_t*>(m_overlayPixels.data() + row * kOverlayBufferWidth);
            std::memcpy(destination, source, m_overlayWidth * sizeof(std::uint32_t));
        }
        SDL_UnlockSurface(rgbaSurface);
    }

    SDL_DestroySurface(rgbaSurface);
}
