#pragma once
#include <vector>
#include <atomic>
#include <cmath>
#include <utility>

namespace kanade
{
constexpr int HOP = 256;

struct Note
{
    int id = 0, a = 0, b = 0;              // frames [a, b)
    float center = 0, pitch = 0;           // MIDI (float): analysed centre, target
    float drift = 0;                       // 1 - variation (0 = natural, 1 = flat, -1 = doubled)
    float inMs = 30, outMs = 30, gain = 0, formant = 0;
    float fadeIn = 0, fadeOut = 0, varIn = 0, varOut = 0;
};

struct Analysis
{
    double sr = 44100;
    int n = 0;                             // frames
    std::vector<float> f0;                 // MIDI per frame, NaN = unvoiced
    std::vector<float> rms;
    double frameSec() const { return HOP / sr; }
};

struct Curves { std::vector<float> shift, form, gain; };
using Run = std::pair<int, int>;

/** progress: 0..1 written while running; cancel: checked between frames (may be null) */
Analysis analyze (const float* x, int len, double sr, std::atomic<float>* progress = nullptr, std::atomic<bool>* cancel = nullptr);
std::vector<Note> segment (const Analysis&, int& nextId);
Note makeNote (int a, int b, const Analysis&, int id);
void recenter (Note&, const Analysis&);
float varWeight (const Note&, int i, double fr);
Curves buildCurves (const std::vector<Note>&, const Analysis&, float globalFormant);
std::vector<Run> voicedRuns (const Analysis&);
std::vector<int> pitchMarks (const float* x, int len, const Analysis&, Run);
/** Re-renders the listed runs of `out` (TD-PSOLA; grains keep the spectral envelope, formant shift = grain resampling). */
void renderRuns (const float* x, int len, const Analysis&, const Curves&, const std::vector<Run>&,
                 const std::vector<std::vector<int>>& marks, float* out, const std::vector<int>& which);

enum Scale { chromatic = 0, major = 1, minor = 2 };
float snapToScale (float m, int key, int scale);
std::pair<int, int> detectKey (const std::vector<Note>&);   // key, scale (major/minor)

inline float ss (float t) { return t <= 0 ? 0.0f : t >= 1 ? 1.0f : t * t * (3 - 2 * t); }
inline bool voiced (float v) { return ! std::isnan (v); }
}
