#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"

class KizamiProcessor : public juce::AudioProcessor,
                        private juce::Timer
{
public:
    KizamiProcessor();
    ~KizamiProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Kizami"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    kizami::Engine engine;

    /** Bumped whenever the whole state is replaced (project load), so the editor rebuilds. */
    std::atomic<int> stateGeneration { 0 };

    // Editor settings that are saved with the project
    int exportLoops = 4;

    static juce::File presetFolder();

private:
    void timerCallback() override { engine.collectGarbage(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KizamiProcessor)
};
