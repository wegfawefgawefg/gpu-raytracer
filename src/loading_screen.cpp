#include "loading_screen.h"

#include <algorithm>
#include <array>
#include <cstring>

#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"

namespace
{
struct FontVertex
{
    float x;
    float y;
    float z;
    unsigned char color[4];
};

float ClampProgress(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}
} // namespace

void LoadingScreen::Attach(SDL_Window* attachedWindow)
{
    window = attachedWindow;
}

void LoadingScreen::Update(std::string_view message, float progress)
{
    if (window == nullptr)
    {
        return;
    }

    messages.emplace_back(message);
    while (messages.size() > 6)
    {
        messages.pop_front();
    }

    SDL_Surface* surface = SDL_GetWindowSurface(window);
    if (surface == nullptr)
    {
        return;
    }

    if (!SDL_LockSurface(surface))
    {
        return;
    }

    DrawBackground(surface);

    const SDL_Rect panel = {64, 72, surface->w - 128, surface->h - 144};
    DrawPanel(surface, panel, SDL_MapSurfaceRGB(surface, 16, 20, 26));

    DrawText(
        surface,
        panel.x + 28,
        panel.y + 26,
        "GPU RAYTRACER STARTUP",
        SDL_MapSurfaceRGB(surface, 238, 243, 248),
        2
    );
    DrawText(
        surface,
        panel.x + 30,
        panel.y + 70,
        message,
        SDL_MapSurfaceRGB(surface, 255, 190, 92),
        2
    );

    const SDL_Rect barFrame = {panel.x + 28, panel.y + 112, panel.w - 56, 30};
    DrawPanel(surface, barFrame, SDL_MapSurfaceRGB(surface, 40, 48, 58));
    DrawProgressBar(
        surface,
        {barFrame.x + 4, barFrame.y + 4, barFrame.w - 8, barFrame.h - 8},
        ClampProgress(progress)
    );

    int lineY = panel.y + 176;
    for (const std::string& line : messages)
    {
        DrawText(surface, panel.x + 30, lineY, line, SDL_MapSurfaceRGB(surface, 148, 194, 255), 2);
        lineY += 28;
    }

    SDL_UnlockSurface(surface);
    SDL_UpdateWindowSurface(window);
    SDL_PumpEvents();
}

void LoadingScreen::DrawBackground(SDL_Surface* surface) const
{
    SDL_ClearSurface(surface, 0.03f, 0.035f, 0.05f, 1.0f);

    const Uint32 stripeColor = SDL_MapSurfaceRGB(surface, 14, 18, 24);
    for (int y = 0; y < surface->h; y += 12)
    {
        const SDL_Rect stripe = {0, y, surface->w, 1};
        SDL_FillSurfaceRect(surface, &stripe, stripeColor);
    }
}

void LoadingScreen::DrawPanel(SDL_Surface* surface, const SDL_Rect& rect, Uint32 color) const
{
    SDL_FillSurfaceRect(surface, &rect, color);
}

void LoadingScreen::DrawProgressBar(SDL_Surface* surface, const SDL_Rect& rect, float progress)
    const
{
    const SDL_Rect filled =
        {rect.x, rect.y, static_cast<int>(rect.w * ClampProgress(progress)), rect.h};

    const Uint32 fillColor = SDL_MapSurfaceRGB(surface, 236, 113, 64);
    const Uint32 shineColor = SDL_MapSurfaceRGB(surface, 255, 194, 92);
    SDL_FillSurfaceRect(surface, &filled, fillColor);

    const SDL_Rect shine = {rect.x, rect.y, filled.w, std::min(rect.h / 3, 8)};
    SDL_FillSurfaceRect(surface, &shine, shineColor);
}

void LoadingScreen::DrawText(
    SDL_Surface* surface,
    int x,
    int y,
    std::string_view text,
    Uint32 color,
    int scale
) const
{
    if (text.empty())
    {
        return;
    }

    const unsigned char rgba[4] = {
        static_cast<unsigned char>((color >> 16) & 0xFFu),
        static_cast<unsigned char>((color >> 8) & 0xFFu),
        static_cast<unsigned char>(color & 0xFFu),
        static_cast<unsigned char>((color >> 24) & 0xFFu),
    };

    char textBuffer[512] = {};
    const std::size_t length = std::min(text.size(), sizeof(textBuffer) - 1);
    std::memcpy(textBuffer, text.data(), length);

    std::array<char, 64 * 1024> vertexBytes = {};
    const int quadCount = stb_easy_font_print(
        static_cast<float>(x),
        static_cast<float>(y),
        textBuffer,
        const_cast<unsigned char*>(rgba),
        vertexBytes.data(),
        vertexBytes.size()
    );

    const FontVertex* vertices = reinterpret_cast<const FontVertex*>(vertexBytes.data());
    for (int quad = 0; quad < quadCount; ++quad)
    {
        const FontVertex& v0 = vertices[quad * 4 + 0];
        const FontVertex& v2 = vertices[quad * 4 + 2];

        const SDL_Rect rect = {
            static_cast<int>(v0.x) * scale,
            static_cast<int>(v0.y) * scale,
            std::max(1, static_cast<int>(v2.x - v0.x) * scale),
            std::max(1, static_cast<int>(v2.y - v0.y) * scale),
        };
        SDL_FillSurfaceRect(surface, &rect, color);
    }
}
