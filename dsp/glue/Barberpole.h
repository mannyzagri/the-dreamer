// Barberpole -- Shepard/barberpole phaser (FEATURES.md 11.2): an endless
// up- or down-going phase sweep. A bank of N allpass "voices" have notch
// centers log-spaced across the spectrum, all rotating in the same direction
// at one rate; each voice is amplitude-windowed by a raised-cosine over the
// log-frequency span so a voice fades IN at one end exactly as another fades
// OUT at the other -- the auditory illusion of infinite motion. Cheap:
// allpass coefficients recomputed at CONTROL RATE (every 32 samples), only
// one-pole allpass math per sample.
//
// PARAMS extras (p0..p3): p0 = DIR (<=.5 down / >.5 up), p1 = STAGES
// (2..kMaxStages), p2 = FEEDBACK, p3 unused. RATE = slot RATE knob.
// New glue. C++17, JUCE-free, RT-safe after prepare().

#pragma once
#include <cmath>
#include <algorithm>

namespace dreamer {

class Barberpole {
public:
    static constexpr int kMaxVoices = 6;

    void prepare(double sampleRate) noexcept {
        fs_ = sampleRate;
        reset();
    }
    void reset() noexcept {
        for (int v = 0; v < kMaxVoices; ++v) {
            apL_[v] = apR_[v] = 0.0f;
            coef_[v] = 0.0f; amp_[v] = 0.0f;
        }
        fbL_ = fbR_ = 0.0f;
        dcL_ = dcR_ = dcX_L_ = dcX_R_ = 0.0f;
        phase_ = 0.0f;
        ctrl_ = 0;
        lastVoices_ = -1;
    }

    // rateHz from the slot RATE knob (mapped by the processor 0.02..2 Hz).
    void process(float& l, float& r, float rateHz, float p0_dir,
                 float p1_stages, float p2_feedback, float mix) noexcept {
        const int voices = std::min(kMaxVoices, std::max(2, 2 + (int)(p1_stages * (kMaxVoices - 2) + 0.5f)));
        const float dir = p0_dir > 0.5f ? 1.0f : -1.0f;
        const float fb  = std::clamp(p2_feedback, 0.0f, 0.95f) * 0.9f;   // hard cap (F1)

        // discrete STAGES change -> topology jump; fade the wet back in to
        // de-click (F7). fade_ ramps 0->1 over ~6 ms after any change.
        if (voices != lastVoices_) { lastVoices_ = voices; fade_ = 0.0f; }
        fade_ = std::min(1.0f, fade_ + 1.0f / (0.006f * (float)fs_));

        phase_ += dir * rateHz / (float)fs_;
        phase_ -= std::floor(phase_);

        if (ctrl_-- <= 0) {                              // control-rate coeff regen
            ctrl_ = 31;
            // log-frequency span 200 Hz .. 6 kHz; voices evenly spaced in the
            // rotating window, amplitude-windowed by raised cosine.
            const float loLn = std::log(200.0f), hiLn = std::log(6000.0f);
            for (int v = 0; v < voices; ++v) {
                float u = phase_ + (float)v / (float)voices;
                u -= std::floor(u);                      // 0..1 position in window
                const float fLn = loLn + (hiLn - loLn) * u;
                const float fc  = std::exp(fLn);
                // one-pole allpass coefficient for this center frequency
                const float t = std::tan(3.14159265f * std::min(fc / (float)fs_, 0.49f));
                coef_[v] = (t - 1.0f) / (t + 1.0f);
                amp_[v]  = 0.5f - 0.5f * std::cos(2.0f * 3.14159265f * u);   // window
            }
            for (int v = voices; v < kMaxVoices; ++v) amp_[v] = 0.0f;
        }

        float sumL = 0.0f, sumR = 0.0f;
        // feedback is soft-clipped + DC-blocked inside the loop so a rotating
        // notch through DC + near-unity FEEDBACK cannot self-oscillate (F1).
        float xL = l + fbL_ * fb;
        float xR = r + fbR_ * fb;
        for (int v = 0; v < voices; ++v) {
            const float c = coef_[v];
            const float yL = c * xL + apL_[v]; apL_[v] = xL - c * yL; xL = yL;
            const float yR = c * xR + apR_[v]; apR_[v] = xR - c * yR; xR = yR;
            sumL += yL * amp_[v];
            sumR += yR * amp_[v];
        }
        const float norm = 2.0f / (float)voices;
        sumL *= norm; sumR *= norm;
        // DC block + tanh limiter on the feedback path (not the dry output)
        float fbInL = std::tanh(sumL);
        float fbInR = std::tanh(sumR);
        dcL_ = fbInL - dcX_L_ + 0.995f * dcL_; dcX_L_ = fbInL; fbL_ = dcL_;
        dcR_ = fbInR - dcX_R_ + 0.995f * dcR_; dcX_R_ = fbInR; fbR_ = dcR_;

        const float m = std::clamp(mix, 0.0f, 1.0f) * fade_;
        l = l * (1.0f - m) + sumL * m;
        r = r * (1.0f - m) + sumR * m;
    }

private:
    double fs_ = 44100.0;
    float  apL_[kMaxVoices] = {}, apR_[kMaxVoices] = {};
    float  coef_[kMaxVoices] = {}, amp_[kMaxVoices] = {};
    float  fbL_ = 0.0f, fbR_ = 0.0f, phase_ = 0.0f;
    float  dcL_ = 0.0f, dcR_ = 0.0f, dcX_L_ = 0.0f, dcX_R_ = 0.0f;
    float  fade_ = 1.0f;
    int    ctrl_ = 0, lastVoices_ = -1;
};

} // namespace dreamer
