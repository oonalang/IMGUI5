#pragma once

#include "imgui.h"

// The Windows build blurs the desktop wallpaper behind the shell. There is no
// wallpaper on Android, so this is a no-op; the translucent shell fill still
// gives the glass look over the gradient background.
void draw_blur_region(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max, float rounding);

// Called by the ImGui core (imgui.cpp, per window); nothing to do on Android.
void draw_blur(ImDrawList* draw_list);
