#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Synth.h"

namespace nagare
{
juce::String jp (const char* utf8);
juce::String noteName (double midi); // FL Studio naming: 60 = C5
juce::String fmtMs (float ms);
juce::String fmtDb (float gain);

struct BuiltinPreset
{
    const char* name;
    Source source; Mode mode; bool mono; bool demo;
    float loopStart, loopEnd, pitch, attack, decay, sustain, release, volume, cutoff, reso, glide, delay, reverb;
};
int builtinPresetCount();
const BuiltinPreset& builtinPreset (int i);
}

class NagareProcessor : public juce::AudioProcessor,
                        private juce::Timer
{
public:
    NagareProcessor();
    ~NagareProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Nagare"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout (NagareProcessor& self);

    // ---- used by the editor (message thread) ----
    enum class EventType { on, off, pedalDown, pedalUp, allOff };
    void pushEvent (EventType type, int note = 0, float velocity = 0.0f);

    bool loadSampleFile (const juce::File& f, juce::String& error);
    nagare::SourcePtr getSource() const { return source; }
    nagare::SnapshotPtr getSnapshot() const { return snapshot; }
    void applyBuiltin (int index);
    bool savePreset (const juce::File& f);
    bool loadPreset (const juce::File& f, juce::String& error);
    static juce::File presetFolder();

    void setParam (const juce::String& id, float value);
    float param (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }

    std::atomic<int> octave { 0 }, transpose { 0 };
    std::atomic<int> sourceGeneration { 0 };   // bumps when the sample changes
    std::atomic<double> sampleSeconds { 1.6 };
    bool isPhysicalHeld (int note) const { return (heldBits[(size_t) (note >> 5)].load() >> (note & 31)) & 1u; }

private:
    void timerCallback() override;
    void setSource (nagare::SourcePtr s, bool freshLoop);
    void rebuildSnapshot();
    nagare::BlockParams readParams() const;
    void physOn (int phys, float vel, std::array<int, 128>& map);
    void physOff (int phys, std::array<int, 128>& map);
    void setHeldBit (int note, bool on);
    juce::ValueTree sampleToTree (bool includeData) const;

    nagare::Synth synth;
    nagare::Effects fx;
    std::vector<float> monoBuf;

    nagare::SourcePtr source;
    nagare::SnapshotPtr snapshot;
    float builtStart = -1, builtEnd = -1;

    struct Event { EventType type; int note; float velocity; };
    juce::AbstractFifo fifo { 512 };
    std::array<Event, 512> events;

    std::array<int, 128> hostMap {}, uiMap {}, playedRefs {};
    std::array<std::atomic<juce::uint32>, 4> heldBits {};
    bool uiPedal = false, midiPedal = false, lastHold = false;
    int lastSource = -1, lastMode = -1, lastVoice = -1, lastKeep = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NagareProcessor)
};
