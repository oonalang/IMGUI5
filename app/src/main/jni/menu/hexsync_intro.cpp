#define IMGUI_DEFINE_MATH_OPERATORS
#include "hexsync_intro.hpp"

#include "imgui.h"
#include "imgui_internal.h"

#include <cmath>

namespace
{
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kDuration = 3.4f;
    constexpr float kFontSize = 40.0f;
    constexpr ImU32 kAccent = IM_COL32(67, 184, 236, 255);
    constexpr ImU32 kAccentDeep = IM_COL32(31, 116, 212, 255);
    constexpr ImU32 kForeground = IM_COL32(242, 245, 251, 255);

    double start_time = -1.0;

    float Clamp01(float value)
    {
        return ImClamp(value, 0.0f, 1.0f);
    }

    float Segment(float time, float delay, float duration)
    {
        return Clamp01((time - delay) / duration);
    }

    float CubicBezier(float x, float x1, float y1, float x2, float y2)
    {
        const auto sample = [](float value, float a, float b)
        {
            const float c = 3.0f * a;
            const float d = 3.0f * b - 6.0f * a;
            const float e = 1.0f - 3.0f * b + 3.0f * a;
            return ((e * value + d) * value + c) * value;
        };
        const auto slope = [](float value, float a, float b)
        {
            const float c = 3.0f * a;
            const float d = 3.0f * b - 6.0f * a;
            const float e = 1.0f - 3.0f * b + 3.0f * a;
            return 3.0f * e * value * value + 2.0f * d * value + c;
        };

        float guess = Clamp01(x);
        for (int iteration = 0; iteration < 6; ++iteration)
        {
            const float derivative = slope(guess, x1, x2);
            if (std::fabs(derivative) < 0.000001f)
                break;
            guess = Clamp01(guess - (sample(guess, x1, x2) - x) / derivative);
        }
        return sample(guess, y1, y2);
    }

    float EaseOut(float value)
    {
        return CubicBezier(Clamp01(value), 0.16f, 1.0f, 0.3f, 1.0f);
    }

    ImU32 WithAlpha(ImU32 color, float alpha)
    {
        return (color & 0x00FFFFFFu) | (static_cast<ImU32>(255.0f * Clamp01(alpha)) << IM_COL32_A_SHIFT);
    }

    ImU32 LerpColor(ImU32 from, ImU32 to, float amount, float alpha)
    {
        const ImVec4 a = ImGui::ColorConvertU32ToFloat4(from);
        const ImVec4 b = ImGui::ColorConvertU32ToFloat4(to);
        ImVec4 result = ImLerp(a, b, Clamp01(amount));
        result.w = Clamp01(alpha);
        return ImGui::ColorConvertFloat4ToU32(result);
    }

    ImVec2 RotatePoint(const ImVec2& point, float angle)
    {
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        return ImVec2(point.x * cosine - point.y * sine, point.x * sine + point.y * cosine);
    }

    void AddRadialGlow(ImDrawList* draw, ImTextureID glow, const ImVec2& center,
        float radius, ImU32 color, float alpha)
    {
        if (!glow)
            return;
        draw->AddImage(glow, center - ImVec2(radius, radius), center + ImVec2(radius, radius),
            ImVec2(0, 0), ImVec2(1, 1), WithAlpha(color, alpha));
    }

    void Hexagon(ImVec2* points, const ImVec2& center, float radius, float rotation)
    {
        for (int index = 0; index < 6; ++index)
        {
            const float angle = -kPi * 0.5f + index * kPi / 3.0f + rotation;
            points[index] = center + ImVec2(std::cos(angle), std::sin(angle)) * radius;
        }
    }

    void AddPartialHexagon(ImDrawList* draw, const ImVec2& center, float radius, float rotation,
        float progress, float alpha, float thickness)
    {
        constexpr int subdivisions = 12;
        constexpr int segment_count = 6 * subdivisions;
        ImVec2 points[6];
        Hexagon(points, center, radius, rotation);
        const float visible = Clamp01(progress) * segment_count;
        for (int segment = 0; segment < segment_count && segment < visible; ++segment)
        {
            const int edge = segment / subdivisions;
            const float local = static_cast<float>(segment % subdivisions) / subdivisions;
            const float next = ImMin(local + 1.0f / subdivisions, 1.0f);
            const ImVec2 from = ImLerp(points[edge], points[(edge + 1) % 6], local);
            const ImVec2 to = ImLerp(points[edge], points[(edge + 1) % 6], next);
            draw->AddLine(from, to, LerpColor(kAccent, kAccentDeep,
                static_cast<float>(segment) / (segment_count - 1), alpha), thickness);
        }
    }

    void AddRotatedImage(ImDrawList* draw, ImTextureID texture, const ImVec2& center,
        float size, float angle, float alpha, float blur)
    {
        if (!texture)
            return;

        const float half = size * 0.5f;
        const ImVec2 corners[] = {
            RotatePoint(ImVec2(-half, -half), angle) + center,
            RotatePoint(ImVec2( half, -half), angle) + center,
            RotatePoint(ImVec2( half,  half), angle) + center,
            RotatePoint(ImVec2(-half,  half), angle) + center
        };
        for (int tap = 0; tap < 8 && blur > 0.1f; ++tap)
        {
            const float tap_angle = tap * kPi * 0.25f;
            const ImVec2 offset(std::cos(tap_angle) * blur, std::sin(tap_angle) * blur);
            draw->AddImageQuad(texture, corners[0] + offset, corners[1] + offset, corners[2] + offset,
                corners[3] + offset, ImVec2(0, 0), ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1),
                WithAlpha(IM_COL32_WHITE, alpha * 0.055f));
        }
        draw->AddImageQuad(texture, corners[0], corners[1], corners[2], corners[3],
            ImVec2(0, 0), ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1), WithAlpha(IM_COL32_WHITE, alpha));
    }

    void AddNode(ImDrawList* draw, ImTextureID glow, const ImVec2& center,
        float time, float delay, float overlay_alpha)
    {
        const float progress = Segment(time, delay, 1.1f);
        if (progress <= 0.0f || progress >= 1.0f)
            return;
        const float alpha = progress < 0.35f
            ? ImLerp(0.0f, 0.95f, progress / 0.35f)
            : ImLerp(0.95f, 0.0f, (progress - 0.35f) / 0.65f);
        AddRadialGlow(draw, glow, center, ImLerp(3.3f, 26.0f, progress), kAccent, alpha * overlay_alpha);
        draw->AddCircleFilled(center, ImLerp(1.0f, 3.0f, progress), WithAlpha(IM_COL32_WHITE, alpha * overlay_alpha), 20);
    }

    void AddWordmark(ImDrawList* draw, ImFont* font, const ImVec2& position, float time, float overlay_alpha)
    {
        const char* text = "HexSync";
        const float sync_start = position.x + font->CalcTextSizeA(kFontSize, FLT_MAX, 0.0f, "Hex").x - 1.8f;
        const float sync_end = position.x + font->CalcTextSizeA(kFontSize, FLT_MAX, 0.0f, text).x - 3.6f;
        float x = position.x;
        for (int index = 0; text[index] != '\0'; ++index)
        {
            const char glyph[] = { text[index], '\0' };
            const float progress = Segment(time, 2.1f + index * 0.05f, 0.7f);
            const float eased = EaseOut(progress);
            const float alpha = Clamp01(progress / 0.5f) * overlay_alpha;
            if (alpha > 0.001f)
            {
                const ImVec2 glyph_position(x, position.y + (1.0f - eased) * 0.55f * kFontSize);
                draw->AddText(font, kFontSize, glyph_position + ImVec2(0.0f, 1.5f),
                    WithAlpha(IM_COL32_BLACK, alpha * 0.55f), glyph);
                const int first_vertex = draw->VtxBuffer.Size;
                draw->AddText(font, kFontSize, glyph_position, WithAlpha(kForeground, alpha), glyph);
                if (index >= 3)
                    ImGui::ShadeVertsLinearColorGradientKeepAlpha(draw, first_vertex, draw->VtxBuffer.Size,
                        ImVec2(sync_start, 0), ImVec2(sync_end, 0), kAccent, kAccentDeep);
            }
            x += font->CalcTextSizeA(kFontSize, FLT_MAX, 0.0f, glyph).x - 0.6f;
        }
    }
}

bool RenderHexSyncIntro(ImTextureID logo, ImTextureID glow, ImFont* wordmark_font)
{
    const double now = ImGui::GetTime();
    if (start_time < 0.0 || ImGui::IsKeyPressed(ImGuiKey_F5, false))
        start_time = now;

    float time = static_cast<float>(now - start_time);
    if (time < kDuration && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        start_time = now - kDuration;
        time = kDuration;
    }
    if (time >= kDuration)
        return false;

    ImGuiIO& io = ImGui::GetIO();
    ImFont* font = wordmark_font ? wordmark_font : ImGui::GetFont();
    const float overlay_alpha = 1.0f - EaseOut(Segment(time, 3.12f, kDuration - 3.12f));

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNav;

    if (ImGui::Begin("##hexsync_intro", nullptr, flags))
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 origin = ImGui::GetWindowPos();
        const ImVec2 center = origin + io.DisplaySize * ImVec2(0.5f, 0.5f);

        ImGui::SetCursorScreenPos(origin);
        ImGui::InvisibleButton("##hexsync_intro_block", io.DisplaySize);

        const float word_width = font->CalcTextSizeA(kFontSize, FLT_MAX, 0.0f, "HexSync").x - 3.6f;
        const float icon_size = 2.35f * kFontSize;
        const float gap = 0.42f * kFontSize;
        const float total_width = icon_size + gap + word_width;
        const float base_left = center.x - total_width * 0.5f;
        const float shift = (gap + word_width) * 0.5f;
        const float lock = EaseOut(Segment(time, 1.95f, 0.95f));
        const float lock_translation = shift * (1.0f - lock);
        const float icon_settle = ImLerp(1.7f, 1.0f, lock);
        const float breathe = time > 3.0f ? 1.0f + 0.03f * std::sin((time - 3.0f) / 2.3f * kPi) : 1.0f;
        const float icon_scale = icon_settle * breathe;
        const ImVec2 icon_center(base_left + icon_size * 0.5f + lock_translation, center.y);
        const float radius = icon_size * 0.5f;

        float halo_alpha = 0.55f * CubicBezier(Segment(time, 0.6f, 1.5f), 0.0f, 0.0f, 0.58f, 1.0f);
        if (time > 3.0f)
            halo_alpha *= 1.0f + 0.12f * std::sin((time - 3.0f) / 2.3f * kPi);
        AddRadialGlow(draw, glow, icon_center, radius * 1.5f * icon_scale, kAccent, halo_alpha * overlay_alpha);

        const float ghost_alpha = 0.14f * Segment(time, 2.5f, 1.0f) * overlay_alpha;
        if (ghost_alpha > 0.001f)
        {
            ImVec2 ghost[6];
            Hexagon(ghost, icon_center, radius * icon_scale, -time * 0.2f);
            draw->AddPolyline(ghost, 6, WithAlpha(kForeground, ghost_alpha), ImDrawFlags_Closed, 0.8f);
        }

        const float ring_progress = CubicBezier(Segment(time, 0.2f, 0.9f), 0.65f, 0.0f, 0.35f, 1.0f);
        const float ring_rotation = ImLerp(kPi / 3.0f, 0.0f, EaseOut(Segment(time, 0.2f, 1.9f)));
        const float ring_alpha = 0.85f * Segment(time, 0.2f, 0.45f) * overlay_alpha;
        AddPartialHexagon(draw, icon_center, radius * 0.92f * icon_scale, ring_rotation,
            ring_progress, ring_alpha, 1.8f);

        const float logo_progress = Segment(time, 0.2f, 1.5f);
        const float logo_eased = EaseOut(logo_progress);
        AddRotatedImage(draw, logo, icon_center, icon_size * 0.88f * ImLerp(0.38f, 1.0f, logo_eased) * icon_scale,
            ImLerp(-2.0f * kPi / 3.0f, 0.0f, logo_eased), Clamp01(logo_progress / 0.45f) * overlay_alpha,
            (1.0f - logo_eased) * 6.0f);

        const float flash = Segment(time, 1.6f, 0.7f);
        if (flash > 0.0f && flash < 1.0f)
        {
            const float flash_alpha = flash < 0.4f ? ImLerp(0.0f, 0.9f, flash / 0.4f)
                : ImLerp(0.9f, 0.0f, (flash - 0.4f) / 0.6f);
            AddRadialGlow(draw, glow, icon_center, radius * 1.4f * ImLerp(0.5f, 1.5f, flash) * icon_scale,
                IM_COL32_WHITE, flash_alpha * overlay_alpha);
        }

        AddNode(draw, glow, icon_center + ImVec2(-icon_size * 0.46f * icon_scale, 0), time, 1.65f, overlay_alpha);
        AddNode(draw, glow, icon_center + ImVec2( icon_size * 0.46f * icon_scale, 0), time, 1.75f, overlay_alpha);
        AddWordmark(draw, font, ImVec2(base_left + icon_size + gap + lock_translation,
            center.y - kFontSize * 0.655f), time, overlay_alpha);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    return true;
}
