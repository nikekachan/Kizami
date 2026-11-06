#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"
#include <atomic>
#include <memory>

namespace yugami
{
class YugamiProcessor : public juce::AudioProcessor
{
public:
    YugamiProcessor();
    ~YugamiProcessor() override;

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "YUGAMI"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    float param (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    void setParam (const juce::String& id, float plain);

    // ---- bank (message thread only)
    std::vector<Page> pages;
    int page = 0;
    std::vector<Preset> mineTime, mineVol;
    bool editTime = true; int editIdx = 1;

    Slot& slot (bool time, int i) { return time ? pages[(size_t) page].time[(size_t) i] : pages[(size_t) page].vol[(size_t) i]; }
    void slotEdited();                      // recompile the current page for the audio thread
    void setPage (int p);
    bool addPage();
    bool deletePage();
    void resetFactory();
    bool exportSet (const juce::File&) const;
    bool importSet (const juce::File&, juce::String& err);
    void saveMine (bool time, const Slot& s, juce::String& savedName);
    void deleteMine (bool time, int idx);
    std::atomic<int> bankGeneration { 0 };  // bumps when pages/presets change outside the editor

    // ---- live triggers from the editor (computer keyboard / clicks)
    void pushTrigger (bool time, int slot, bool on);

    // ---- status for the editor
    std::atomic<float> phaseT { 0 }, phaseV { 0 }, delayBars { 0 }, peak { 0 }, level { 1 };
    std::atomic<int> hotT { -1 }, hotV { -1 };
    std::atomic<bool> running { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void rebuildTables();
    void handleTrigger (bool time, int slot, bool on);
    void loadMine();
    void writeMine() const;
    juce::var setToVar (bool withMine) const;

    Engine engine;
    std::atomic<Tables*> live { nullptr };
    std::vector<std::pair<std::unique_ptr<Tables>, juce::uint32>> retired;
    std::unique_ptr<Tables> current;

    juce::AbstractFifo fifo { 256 };
    std::array<std::array<int, 3>, 256> fifoData {};
    std::vector<int> heldT, heldV;
    double internalBeat = 0;
    bool lastLatch = false;
    double sampleRate = 48000.0;
    std::vector<float> scratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YugamiProcessor)
};
} // namespace yugami

using YugamiProcessor = yugami::YugamiProcessor;
