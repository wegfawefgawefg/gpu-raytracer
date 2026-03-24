#include "app.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <string_view>

namespace
{
constexpr std::string_view kWindowTitle = "gpu-raytracer";
constexpr int kInitialWindowWidth = 1440;
constexpr int kInitialWindowHeight = 900;
constexpr std::string_view kX11DialogWindowType = "_NET_WM_WINDOW_TYPE_DIALOG";

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

        Update(delta.count());

        const GpuFrameParams params =
            m_camera.BuildFrameParams(m_renderWidth, m_renderHeight, m_spheres.size());
        m_renderer.Render(params);
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

    CenterWindowOnPrimaryDisplay(m_window);
    m_loadingScreen.Attach(m_window);
    m_loadingScreen.Update("Booting SDL window...", 0.08f);
    m_renderer.Initialize(
        m_window,
        m_spheres,
        [this](std::string_view message, float progress)
        { m_loadingScreen.Update(message, progress); }
    );
    m_loadingScreen.Update("Sizing render targets...", 0.94f);
    SyncRendererSize();
    m_loadingScreen.Update("Startup complete.", 1.0f);
}

void App::Shutdown()
{
    m_renderer.Shutdown();

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

    m_camera.UpdateMovement(keys, deltaSeconds);
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
}
