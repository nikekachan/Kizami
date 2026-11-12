#pragma once
#include "Patch.h"
#include "Tables.h"
#include <array>
#include <atomic>
#include <random>

namespace shz
{
// ---- DSP helpers ----
struct DelayLine
{
    std::vector<float> b; int n = 1, w = 0;
    void init (int len) { n = juce::jmax (2, len); b.assign ((size_t) n, 0.0f); w = 0; }
    void push (float x) { b[(size_t) w] = x; if (++w >= n) w = 0; }
    float read (double d) const
    {
        double r = w - d; while (r < 0) r += n;
        const int i = (int) r; const float t = (float) (r - i); const int j = (i + 1) % n;
        return b[(size_t) i] + (b[(size_t) j] - b[(size_t) i]) * t;
    }
};
struct Biquad
{
    double c[5] { 1, 0, 0, 0, 0 }, z1[2] {}, z2[2] {};
    void set (const char* type, double f, double q, double gainDb, double sr);
    float run (float x, int ch) { const double y = c[0] * x + z1[ch]; z1[ch] = c[1] * x - c[3] * y + z2[ch]; z2[ch] = c[2] * x - c[4] * y; return (float) y; }
};
struct Svf { double g = 0, k = 2, a1 = 0, a2 = 0, a3 = 0; void set (double fc, double sr, double k); };
enum class FMode { lp, hp, bp, notch, peak, ap, comb };
FMode classify (const juce::String& type);
int slopeOf (const juce::String& type);
double fnorm (double n);
double tnorm (double t);
double tfrom (double n);
double curveY (double t, double c);
double lfoCurveVal (const std::vector<LfoPt>& pts, double t);
double destSpan (const juce::String& mod, const juce::String& p);

// ---- FX ----
struct FxState
{
    std::array<DelayLine, 2> dl; std::vector<DelayLine> lines, diff; std::array<DelayLine, 2> pre;
    std::vector<int> base, dlen; std::array<double, 8> lp {}; double ph = 0, mph = 0; float fb[2] {};
    double ap[2][12] {}; double env[3] {}; float hold[2] {}; int cnt = 0;
    Biquad bq, b1, b2, hp, preF; std::array<Biquad, 4> xo; double fs[8] {}, fs2[8] {};
};
struct Fx
{
    int type = 0; bool on = true;
    std::vector<std::pair<juce::String, float>> p;  // numeric params
    juce::String mode;                              // kParamMode / kParamType string
    std::array<std::pair<juce::String, float>, 8> mods; int nmods = 0;
    std::unique_ptr<FxState> st;
    float P (const char* k, float d) const;
    bool has (const char* k) const;
    void addMod (const juce::String& k, float v);
};

// ---- compiled patch (built off the audio thread) ----
struct CMod { int src, aux; bool bip; float k; int slot; int fxId; juce::String fxP; bool hz, gvol; };
struct EnginePatch
{
    Patch p;
    std::vector<std::unique_ptr<Fx>> fx, bus1, bus2;
    std::vector<CMod> vmods, gmods;
    std::array<FMode, 2> fmode {}; std::array<bool, 2> ftwo {}, fmg {};
    std::array<int, 3> warpId {};
    double a4 = 440;
};
std::unique_ptr<EnginePatch> compile (const Patch& p, double sr);

struct OscCache { double inc[16] {}, gl[16] {}, gr[16] {}; };

struct Voice
{
    bool on = false, gate = false; int note = 60; float vel = 1; juce::uint64 age = 0; float rnd = 0;
    double ph[3][16] {}; double subPh = 0; double nz[7] {};
    struct E { int st = 0; double t = 0, v = 0, from = 0; } env[4];
    double lfoPh[10] {}, lfoHold[10] {};
    double fs[4][8] {};
    double src[41] {}; double dm[80] {};
    double pitch = 60, target = 60; double samplePos = 0; double ampPrev = -1;
};

class Engine
{
public:
    explicit Engine (double sampleRate);
    void setSampleRate (double sr);
    double sampleRate() const { return sr; }
    /** Swap in a new patch (audio thread, no allocation). Returns the old one for disposal off-thread. */
    std::unique_ptr<EnginePatch> swapPatch (std::unique_ptr<EnginePatch> np, bool full);
    void setTable (int i, TablePtr t) { tables[(size_t) i] = std::move (t); }
    TablePtr tableAt (int i) const { return tables[(size_t) i]; }
    void noteOn (int n, float vel);
    void noteOff (int n);
    void allOff (bool hard);
    void process (float* L, float* R, int n);
    const EnginePatch* patch() const { return P.get(); }

    double bpm = 120, bend = 0, modwheel = 0, outGain = 1;
    std::array<float, 8> macros {};    // 0..100 from host parameters
    float meter = 0;
    // visual feedback
    std::atomic<float> visLfoPh[10] {}, visEnv[4] {};

private:
    void trigger (int n, float vel);
    void release (Voice& v);
    void arpTick (int n);
    void arpStop();
    double envStep (Voice& v, int i, double dt);
    void globalLfos (double dt);
    void voiceSources (Voice& v, double dt);
    void voiceMods (Voice& v);
    void globalMods();
    void renderVoice (Voice& v, float* L, float* R, int s0, int len, double dt);
    void runFx (Fx& f, float* L, float* R, int n);

    double sr;
    std::unique_ptr<EnginePatch> P;
    std::array<TablePtr, 3> tables;
    std::array<Voice, 16> voices;
    Voice* last = nullptr;
    struct Held { int n; float vel; juce::uint64 t; };
    std::vector<Held> held;
    juce::uint64 noteCount = 0;
    double gLfoPh[10] {}, gLfoV[10] {}, gLfoHold[10] {}; double chaos[10][3] {};
    double arpT = 0, arpGate = -1; int arpI = 0, arpNote = -1;
    std::vector<float> dL, dR, b1L, b1R, b2L, b2R;
    bool useB1 = false, useB2 = false; double gVol = 0, lim = 1;
    std::array<OscCache, 3> oc;
    std::minstd_rand rng { 1234 };
    double rnd() { return std::uniform_real_distribution<double> (0, 1) (rng); }
};
}
