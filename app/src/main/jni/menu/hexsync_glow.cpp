#include "hexsync_intro.hpp"

#include <cmath>

void BuildHexSyncGlowPixels(std::uint32_t* pixels, int texture_size)
{
    for (int y = 0; y < texture_size; ++y)
    {
        for (int x = 0; x < texture_size; ++x)
        {
            const float nx = (x + 0.5f) / (texture_size * 0.5f) - 1.0f;
            const float ny = (y + 0.5f) / (texture_size * 0.5f) - 1.0f;
            const float distance = std::sqrt(nx * nx + ny * ny);
            const float alpha = std::pow(distance < 1.0f ? 1.0f - distance : 0.0f, 1.7f);
            pixels[y * texture_size + x] = 0x00FFFFFFu | (static_cast<std::uint32_t>(alpha * 255.0f) << 24);
        }
    }
}
