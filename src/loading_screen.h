#pragma once

#include <deque>
#include <string>
#include <string_view>

#include <SDL3/SDL.h>

struct LoadingScreen
{
    void Attach(SDL_Window* window);
    void Update(std::string_view message, float progress);

    void DrawBackground(SDL_Surface* surface) const;
    void DrawPanel(SDL_Surface* surface, const SDL_Rect& rect, Uint32 color) const;
    void DrawProgressBar(SDL_Surface* surface, const SDL_Rect& rect, float progress) const;
    void
    DrawText(SDL_Surface* surface, int x, int y, std::string_view text, Uint32 color, int scale)
        const;

    SDL_Window* window = nullptr;
    std::deque<std::string> messages;
};
