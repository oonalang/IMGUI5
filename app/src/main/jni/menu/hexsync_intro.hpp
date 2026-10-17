#pragma once

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui.h"

#include <cstdint>

// Fills size*size RGBA8 pixels (little endian 0xAABBGGRR) with the radial glow sprite.
void BuildHexSyncGlowPixels(std::uint32_t* pixels, int size);

// Returns true while the intro is still playing.
bool RenderHexSyncIntro(ImTextureID logo, ImTextureID glow, ImFont* wordmark_font);
