#pragma once

#include "imgui.h"
#include <algorithm>
#include <cmath>

// The menu is drawn in a fixed logical canvas. These helpers fit that canvas to
// any screen and convert touch input and draw data between the two spaces.
namespace nl
{
    constexpr float kCanvasW = 750.0f;
    constexpr float kCanvasH = 756.0f;

    struct Layout
    {
        float scale = 1.0f;       // physical pixels per logical pixel
        ImVec2 origin{};          // physical position of logical (0, 0)
        int width = 0;            // physical surface size
        int height = 0;
    };

    inline Layout Fit(int width, int height, float fill = 0.97f, float user_scale = 1.0f)
    {
        Layout l;
        l.width = width;
        l.height = height;
        l.scale = std::min(width * fill / kCanvasW, height * fill / kCanvasH) * user_scale;
        if (l.scale < 0.1f) l.scale = 0.1f;
        l.origin = ImVec2(std::floor((width - kCanvasW * l.scale) * 0.5f),
                          std::floor((height - kCanvasH * l.scale) * 0.5f));
        return l;
    }

    inline ImVec2 ToLogical(const Layout& l, float px, float py)
    {
        return ImVec2((px - l.origin.x) / l.scale, (py - l.origin.y) / l.scale);
    }

    // Call after ImGui::Render(): moves every vertex and clip rect to physical pixels.
    inline void TransformDrawData(ImDrawData* dd, const Layout& l)
    {
        if (!dd) return;
        const float fw = (float)l.width, fh = (float)l.height;
        for (int n = 0; n < dd->CmdListsCount; ++n)
        {
            ImDrawList* list = dd->CmdLists[n];
            for (int i = 0; i < list->VtxBuffer.Size; ++i)
            {
                ImDrawVert& v = list->VtxBuffer[i];
                v.pos.x = v.pos.x * l.scale + l.origin.x;
                v.pos.y = v.pos.y * l.scale + l.origin.y;
            }
            for (int c = 0; c < list->CmdBuffer.Size; ++c)
            {
                ImVec4& r = list->CmdBuffer[c].ClipRect;
                r.x = std::max(0.0f, r.x * l.scale + l.origin.x);
                r.y = std::max(0.0f, r.y * l.scale + l.origin.y);
                r.z = std::min(fw,   r.z * l.scale + l.origin.x);
                r.w = std::min(fh,   r.w * l.scale + l.origin.y);
            }
        }
        dd->DisplayPos = ImVec2(0, 0);
        dd->DisplaySize = ImVec2(fw, fh);
        dd->FramebufferScale = ImVec2(1, 1);
    }
}
