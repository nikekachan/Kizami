#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace kizami
{

constexpr int kStepsPerBar = 16;
constexpr int kMaxBars     = 16;
constexpr int kMaxSteps    = kStepsPerBar * kMaxBars;
constexpr int kMaxVoices   = 96;
constexpr double kPpqPerStep = 0.25; // 16th notes

enum class Sound
{
    kick, b808, snare, clap, chh, ohh, rim, tom, perc, shaker, cowbell, crash, sample, count
};

struct SoundInfo
{
    const char* id;     // stable id used in saved files
    const char* label;  // Japanese display name (UTF-8)
    int defaultNote;    // General MIDI drum note
};

const SoundInfo& soundInfo (Sound s);
Sound soundFromId (const juce::String& id);
juce::String soundLabel (Sound s);

using SharedBuffer = std::shared_ptr<const juce::AudioBuffer<float>>;

struct SampleData
{
    juce::AudioBuffer<float> audio;
    double sampleRate = 44100.0;
    juce::MemoryBlock fileBytes; // original file, embedded in presets/projects
    juce::String fileName;
};

struct Track
{
    Track();

    int uid = 0;
    juce::String name;
    Sound sound = Sound::kick;

    // Live-editable from the UI while audio runs
    std::atomic<float> volume { 0.8f };
    std::atomic<float> pan { 0.0f };
    std::atomic<bool>  mute { false };
    std::atomic<bool>  solo { false };
    std::atomic<int>   note { -1 };     // -1 = default note of the sound
    std::array<std::atomic<float>, kMaxSteps> vel;   // 0 = off, else 0.1..1
    std::array<std::atomic<uint8_t>, kMaxSteps> roll; // 1..4 hits per step
    std::array<std::atomic<bool>, kMaxSteps> skipOnce;
    std::atomic<int> hitCounter { 0 };  // bumps on every trigger, the UI flashes pads with it

    // Only touched on the message thread
    float pitch = 0.0f;   // semitones
    float decay = 1.0f;   // length multiplier
    std::shared_ptr<SampleData> sample;

    // Swapped under the engine lock
    SharedBuffer rendered;

    int effectiveNote() const;
};

using TrackPtr = std::shared_ptr<Track>;

/** One playing hit. */
struct Voice
{
    SharedBuffer buffer;
    int position = 0;
    int delay = 0;          // samples to wait before starting (offset inside block)
    int chokeDelay = -1;    // samples until a choke fade starts (-1 = none)
    float gainL = 0, gainR = 0;
    float fade = 1.0f;      // for choking
    float fadeStep = 0.0f;
    int trackUid = 0;
    Sound sound = Sound::kick;
    bool active = false;
};

/** A hit from the UI pads or from MIDI, waiting for the audio thread. */
struct PadEvent
{
    int trackUid = 0;
    float velocity = 1.0f;
    bool record = true;
};

class Engine
{
public:
    Engine();

    //==== message thread ======================================================
    void prepare (double sampleRate);
    double getSampleRate() const { return sampleRate; }

    std::vector<TrackPtr> getTracks() const;     // snapshot copy
    TrackPtr findTrack (int uid) const;
    TrackPtr addTrack (Sound s);
    TrackPtr duplicateTrack (int uid);
    void removeTrack (int uid);
    void moveTrack (int uid, int delta);
    void setSound (int uid, Sound s);
    void setPitchDecay (int uid, float pitch, float decay);
    bool loadSampleFile (int uid, const juce::File& file, juce::String& error);
    bool loadSampleBytes (int uid, const juce::MemoryBlock& bytes, const juce::String& name);
    void rerender (Track& t);
    void collectGarbage();

    int  getBars() const { return bars.load(); }
    void setBars (int newBars, bool copyLastBar);
    int  getTotalSteps() const { return bars.load() * kStepsPerBar; }

    std::atomic<float> swing { 0.0f };   // 0..1
    std::atomic<float> master { 0.8f };
    std::atomic<bool>  metronome { false };
    std::atomic<bool>  recording { false };
    std::atomic<bool>  internalPlay { false };

    // Status for the UI
    std::atomic<int>    currentStep { -1 };
    std::atomic<double> hostBpm { 140.0 };
    std::atomic<bool>   hostPlaying { false };

    void pushPadEvent (int trackUid, float velocity, bool record);

    // Serialisation
    juce::ValueTree toValueTree (bool includeSamples = true) const;
    void fromValueTree (const juce::ValueTree& v);
    void resetToDefault();

    //==== audio thread ========================================================
    void process (juce::AudioBuffer<float>& out, juce::MidiBuffer& midi,
                  juce::AudioPlayHead* playHead);

    //==== offline rendering (message thread) ==================================
    /** Renders `loops` repetitions of the pattern. trackUid = 0 renders the full mix. */
    juce::AudioBuffer<float> renderOffline (int loops, double bpm, double sr, int trackUid);

    static bool isAudible (const Track& t, bool anySolo);
    static int builtInPresetCount();
    static juce::String builtInPresetName (int index);
    static juce::ValueTree builtInPreset (int index);
    static double swingOffsetPpq (long long globalStep, float swingAmount);

private:
    void triggerVoice (Voice* voices, Track& t, int offset, float velocity, bool live);
    void triggerClick (Voice* voices, bool accent, int offset);
    void renderVoices (Voice* voices, juce::AudioBuffer<float>& out, int start, int num);
    void sequence (Voice* voices, const std::vector<TrackPtr>& list, double fromPpq, double toPpq,
                   double blockStartPpq, double samplesPerPpq, int numSamples, bool live, int onlyUid);
    void handlePad (const PadEvent& e, int offset, bool playing, double ppqAt, double blockEndPpq);
    Voice* allocateVoice (Voice* voices);

    mutable juce::SpinLock lock;
    std::vector<TrackPtr> tracks;
    std::vector<SharedBuffer> graveyard;
    int nextUid = 1;
    std::atomic<int> bars { 1 };
    double sampleRate = 44100.0;

    SharedBuffer clickHi, clickLo;
    std::array<Voice, kMaxVoices> voices;

    juce::AbstractFifo padFifo { 256 };
    std::array<PadEvent, 256> padEvents;

    double lastPpqEnd = -1.0e9;
    double internalPpq = 0.0;
    bool   wasInternal = false;
    double lastBpm = 140.0;
};

} // namespace kizami
