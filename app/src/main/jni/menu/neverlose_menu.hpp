#pragma once

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui.h"

// Draws the whole menu inside a fixed 750 x 756 logical canvas anchored at (0, 0).
// `avatar` may be null. `reveal_progress` runs 0..1 (open animation); input is
// blocked while it is below 1.
void RenderNeverloseMenu(ImTextureID avatar, float reveal_progress = 1.0f);
