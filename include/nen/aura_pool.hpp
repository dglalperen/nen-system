#pragma once

namespace nen {

struct AuraPool {
    float current   = 120.0F;
    float max       = 2400.0F;
    float regenRate = 72.0F;   // per second base
    float control   = 1.0F;    // 0..1, scales output precision
};

} // namespace nen
