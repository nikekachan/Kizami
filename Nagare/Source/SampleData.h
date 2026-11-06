#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <memory>
#include <vector>

namespace nagare
{

constexpr int kTableSize = 2048;
constexpr int kMipLevels = 11; // level L holds harmonics 1 .. 2^L

/** Band-limited single-cycle wavetable: one table per octave of harmonic count. */
struct Wavetable
{
    std::array<std::vector<float>, kMipLevels> tables; // kTableSize + 1 (guard point)

    /** re[h], im[h] are the cosine / sine amplitudes of harmonic h (index 0 unused). */
    void build (const std::vector<float>& re, const std::vector<float>& im);
    void normalisePeak();
    static int levelFor (double freq, double sampleRate);
    float lookup (double phase, int level) const
    {
        const auto& t = tables[(size_t) level];
        const double p = phase * kTableSize;
        int i = (int) p;
        const float f = (float) (p - i);
        i &= (kTableSize - 1);
        return t[(size_t) i] + (t[(size_t) i + 1] - t[(size_t) i]) * f;
    }
};

enum class Source { sample = 0, sine, triangle, saw, square, supersaw, organ, count };
const Wavetable& builtinTable (Source s); // call once off the audio thread first (warmUpTables)
void warmUpTables();

/** A loaded sample after analysis (trimmed, normalised, pitch detected). Message thread only. */
struct SampleSource
{
    juce::String name;
    bool demo = false;
    juce::MemoryBlock fileBytes;   // original file, saved with presets / projects
    double sampleRate = 44100.0;
    std::vector<float> raw;        // mono, peak = 1
    int peakIndex = 0;
    double f0 = 261.63;
    bool pitched = false;
    double rootMidi = 60.0;

    double seconds() const { return raw.empty() ? 0.0 : (double) raw.size() / sampleRate; }
};
using SourcePtr = std::shared_ptr<const SampleSource>;

SourcePtr makeDemoSource();
SourcePtr makeSourceFromBytes (const juce::MemoryBlock& bytes, const juce::String& name, juce::String& error);
SourcePtr makeSource (std::vector<float> mono, double sampleRate, const juce::String& name,
                      const juce::MemoryBlock& bytes, bool demo);
void defaultLoopFor (const SampleSource& s, float& startFrac, float& endFrac);

/** What the voices play: the loop made seamless and level, plus the "波形化" wavetable. Read-only. */
struct LoopSnapshot
{
    double sampleRate = 44100.0;
    double f0 = 261.63, period = 168.6, rootMidi = 60.0;
    bool pitched = false;
    std::vector<float> flat;   // level-flattened sample, reaching past the loop end
    int loopStart = 0, loopEnd = 1, loopLen = 1, xfade = 1;
    bool hasWave = false;
    Wavetable wave;

    float readFlat (double p) const
    {
        if (p < 0.0) return 0.0f;
        const auto i = (size_t) p;
        if (i + 1 >= flat.size()) return 0.0f;
        const float fr = (float) (p - (double) i);
        return flat[i] + (flat[i + 1] - flat[i]) * fr;
    }
};
using SnapshotPtr = std::shared_ptr<const LoopSnapshot>;

SnapshotPtr buildSnapshot (const SampleSource& s, float loopStartFrac, float loopEndFrac);

double detectPitch (const std::vector<float>& x, double sr, int start);

} // namespace nagare
