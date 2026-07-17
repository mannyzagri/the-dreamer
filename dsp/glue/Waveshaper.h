// Waveshaper -- per-tone LUT waveshaping stage (FEATURES.md section 2, the
// Korg 01/W trick). Sits between PcmOscillator and the tone TVF.
//
//   y = lerp(x, table[drive(x)], depth)
//
// - depth 0..1 is the dry/shaped mix; the table INPUT index also scales with
//   an internal drive (1x..3x with depth) so the DEPTH knob sweeps feel alive.
// - One table read + 2 lerps per sample per tone (spec cost budget).
// - Authenticity: output re-quantized to the 12-bit ROM grain (low nibble
//   zero), truncating toward zero like the hardware DAC path -- the bank's
//   (sample & 0xF) == 0 invariant survives the shaper.
// - mode 0 = OFF (bit-exact passthrough), 1..5 = SOFT FOLD / HARD FOLD /
//   SINE FOLD / ASYM / DRIVE (table order in ShaperTables.h).
//
// C++17, header-only, JUCE-free, real-time safe, stateless.

#pragma once
#include <cstdint>
#include "dsp/glue/ShaperTables.h"

namespace dreamer {

struct Waveshaper {
    static float process(float x, int mode, float depth) noexcept {
        if (mode <= 0 || mode > 5 || depth <= 0.0f)
            return x;                               // OFF / depth 0: bit-exact dry
        const int16_t* t = shapers::kTables[mode - 1];

        // internal drive: index input scales 1x..3x with depth
        float ix = x * (1.0f + 2.0f * depth);
        if (ix > 1.0f) ix = 1.0f; else if (ix < -1.0f) ix = -1.0f;

        const float u  = (ix * 0.5f + 0.5f) * (float)(shapers::kTableLen - 1);
        const int   i0 = (int)u;
        const int   i1 = i0 < shapers::kTableLen - 1 ? i0 + 1 : i0;
        const float fr = u - (float)i0;
        const float shaped = ((float)t[i0] + (float)(t[i1] - t[i0]) * fr)
                             * (1.0f / 32768.0f);

        const float y = x + depth * (shaped - x);

        // re-quantize to 12-bit grain, truncate toward zero (integer div)
        int32_t q = (int32_t)(y * 32768.0f);
        if (q > 32767) q = 32767; else if (q < -32768) q = -32768;
        q = (q / 16) * 16;
        return (float)q * (1.0f / 32768.0f);
    }
};

} // namespace dreamer
