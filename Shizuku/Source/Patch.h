#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include <memory>

// SHIZUKU patch model (mirrors the browser version) + Serum 2 preset reader.
namespace shz
{
struct LfoPt { float x = 0, y = 0, c = 0.5f; };

struct Osc
{
    bool on = false;
    bool sample = false;           // Serum "sample" oscillator
    juce::String tablePath, tableName { "Default Shapes" }, samplePath;
    float sampleRoot = 60;
    float oct = 0, semi = 0, fine = 0, vol = 0.5625f, pan = 0;
    int uni = 1;
    float det = 0.25f, blend = 75, stereo = 100, pos = 0;
    juce::String warp { "none" };
    float warpAmt = 0, phase = 0, rand = 100;
    std::shared_ptr<std::vector<float>> embedded;   // raw table samples (if any)
    std::shared_ptr<std::vector<float>> sampleData; float sampleRate = 44100;
};
struct Sub { bool on = false; juce::String shape { "sine" }; float oct = 0, vol = 0.5f, pan = 0; };
struct Noise { bool on = false; juce::String type { "White" }; float vol = 0.5f, color = 0.5f, pan = 0; juce::String samplePath; };
struct Route { int dest = 1; float bal = -100, bus1 = 0, bus2 = 0; }; // dest: 0 filter, 1 master(FX), 2 direct
struct Filter { bool on = false; juce::String type { "MgL12" }; float freq = 0.5f, reso = 10, drive = 0, vari = 0, key = 0; };
struct Env { float a = 0.0005f, h = 0, d = 1, s = 1, r = 0.015f, c1 = 50, c2 = 66.6f, c3 = 66.6f; };
struct Lfo
{
    juce::String mode { "Trig" }, type { "curve" };
    bool sync = true, mono = false, x10 = false;
    float rate = 2, smooth = 0;
    std::vector<LfoPt> pts { { 0, 0, 0.5f }, { 0.5f, 1, 0.5f }, { 1, 0, 0.5f } };
};
struct Mod { int src = 0, aux = 0; juce::String mod, p; int id = 0; float amt = 0; bool bip = false; };
struct Macro { juce::String name; float val = 0; };
struct FxUnit { int type = 0; juce::String name; bool on = true; juce::NamedValueSet p; };
struct Glob { float vol = 0.5f; bool mono = false, legato = false, portaAlways = false; float porta = 0, modwheel = 0, bend = 2, tuning = 440; };
struct Arp { bool on = false; juce::String mode { "up" }; float rate = 4; int oct = 1; float gate = 80; };

struct Patch
{
    juce::String name { "Init" }, author { "DJ KATSUNII" }, desc, source { "shizuku" };
    std::array<Osc, 3> osc;
    Sub sub; Noise noise;
    std::array<Route, 5> route;
    std::array<Filter, 2> filt;
    std::array<Env, 4> env;
    std::array<Lfo, 10> lfo;
    std::vector<Mod> mod;
    std::array<Macro, 8> macro;
    std::vector<FxUnit> fx, bus1, bus2;
    Glob glob; Arp arp;
    juce::StringArray missing;

    Patch();
    juce::var toVar (bool withTables) const;
    static Patch fromVar (const juce::var&);
};

juce::String srcName (int id);
const char* fxName (int type);
float fxParam (const FxUnit& f, const char* key, float def);

/** Reads a .SerumPreset file. Throws std::runtime_error (message in Japanese) on failure. */
Patch readSerumPreset (const juce::MemoryBlock& file);
std::vector<Patch> factoryPatches();
}
