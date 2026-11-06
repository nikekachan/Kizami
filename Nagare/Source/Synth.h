#pragma once

#include "SampleData.h"
#include <juce_dsp/juce_dsp.h>

namespace nagare
{

enum class Mode { loop = 0, wave };

/** Parameter values for one block, read once on the audio thread. */
struct BlockParams
{
    Source source = Source::sample;
    Mode mode = Mode::loop;
    bool mono = false;
    bool keepLength = true;
    float pitchAdjust = 0.0f;         // semitones
    float attackMs = 5, decayMs = 350, sustain = 0.75f, releaseMs = 350;
    float glideMs = 60;
};

struct Voice
{
    bool active = false, released = false;
    int note = 60;
    float velocity = 1.0f;
    juce::uint64 age = 0;

    // pitch (semitones, glides in mono mode)
    double semi = 60, target = 60, glideCoef = 0;

    // envelope
    int stage = 0; // 0 attack, 1 decay/sustain, 2 release
    double level = 0, peak = 0, attackInc = 0, decayCoef = 0, sustainLevel = 0, releaseCoef = 0;

    // sample playback
    SnapshotPtr snap;
    bool sampleMode = false, timeKeep = false, waveMode = false;
    double pos = 0;            // resample mode read position (source samples)
    double t = 0, ga = 0, gb = 0; // time-keeping mode: source time and the two grain centres
    double oscMix = 0, oscStep = 0, wavePhase = 0;
    int waveLevel = 0;

    // built-in oscillators
    Source source = Source::sine;
    int numOsc = 1;
    double phase[7] {}, detune[7] {};
    float gain[7] {};
    int level7[7] {};
    const Wavetable* table = nullptr;
};

class Synth
{
public:
    static constexpr int kMaxVoices = 24;
    static constexpr int kMaxSounding = 14;

    void prepare (double sampleRate);
    void setSnapshot (SnapshotPtr s);      // message thread
    void collectGarbage();                 // message thread
    void pullSnapshot();                   // audio thread, once per block

    void noteOn (int note, float velocity, const BlockParams& p);
    void noteOff (int note, const BlockParams& p);
    void setPedal (bool down, const BlockParams& p);
    void allNotesOff (bool fast);
    bool pedalDown() const { return pedal; }

    /** Adds the mono voice mix into out[start .. start+num). */
    void render (float* out, int start, int num, const BlockParams& p);

    int activeVoiceCount() const;

private:
    Voice* startVoice (int note, float velocity, const BlockParams& p);
    void releaseVoice (Voice& v, const BlockParams& p, bool fast);
    void doOff (int note, const BlockParams& p);
    void setTarget (Voice& v, int note, const BlockParams& p);
    void updateVoicePitch (Voice& v, const BlockParams& p);
    float sampleValue (Voice& v, double ratio);

    double sr = 44100.0;
    std::array<Voice, kMaxVoices> voices;
    juce::uint64 counter = 0;

    Voice* monoVoice = nullptr;
    std::array<int, 128> monoStack {};
    int monoCount = 0;
    std::array<Voice*, 128> polyMap {};
    std::array<bool, 128> held {}, sustained {};
    bool pedal = false;

    juce::SpinLock lock;
    SnapshotPtr current, audioSnap;
    std::vector<SnapshotPtr> graveyard;
};

/** Low-pass filter, delay and reverb on the mixed voices, then volume and a soft limiter. */
class Effects
{
public:
    void prepare (double sampleRate, int maxBlock);
    void process (const float* monoIn, float* L, float* R, int num,
                  float cutoff, float resonance, float delaySend, float reverbSend, float volumeDb);

private:
    double sr = 44100.0;
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    float lastCutoff = -1, lastQ = -1;
    juce::SmoothedValue<float> cutoffS, delayS, reverbS, volS;
    std::vector<float> dline;
    int dpos = 0, dlen = 1;
    float dlp = 0;
    juce::Reverb reverb;
    std::vector<float> rvL, rvR, filt;
};

} // namespace nagare
