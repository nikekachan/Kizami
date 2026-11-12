#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Engine.h"

inline juce::String jp (const char* s) { return juce::String::fromUTF8 (s); }

namespace shz
{
enum class TableStatus { none, builtin, approx, lib, embedded, missing, sampleOk, sampleMissing };

struct BrowserEntry { juce::String name, folder; juce::File file; int factoryIndex = -1; };

class ShizukuProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    ShizukuProcessor();
    ~ShizukuProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "SHIZUKU"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return patch.name; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- message thread API ----
    Patch patch;                                  // the editable patch
    void setPatch (const Patch& p);               // load a whole new sound
    void patchEdited();                           // a knob changed
    void structureEdited();                       // FX added/removed/reordered, mono toggled…
    bool loadFile (const juce::File& f, juce::String& error);
    bool savePatch (const juce::File& f);
    void loadOscWav (int osc, const juce::File& f, juce::String& error);
    void resolveTables();
    std::array<TableStatus, 3> tableStatus {};
    std::array<TablePtr, 3> shownTables;          // for the editor
    std::atomic<int> tablesGeneration { 0 };

    // preset browser (factory + Serum 2 folder + SHIZUKU folder)
    std::vector<Patch> factory;
    std::vector<BrowserEntry> browser;
    std::atomic<bool> browserReady { false };
    int browserIndex = 0;
    void rescanBrowser();
    bool loadBrowser (int index, juce::String& error);
    static juce::File userPresetFolder();

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboard;
    std::atomic<float> meter { 0 }, hostBpm { 120 };
    std::atomic<double> uiBend { 0 }, uiModWheel { -1 };
    Engine& eng() { return engine; }
    juce::String lastMessage;

private:
    void timerCallback() override;
    void push (bool full);
    juce::AudioProcessorValueTreeState::ParameterLayout layout();

    Engine engine { 44100 };
    double sr = 44100;
    juce::SpinLock lock;
    std::unique_ptr<EnginePatch> pending; bool pendingFull = false;
    std::array<TablePtr, 3> pendingTables; bool tablesPending = false;
    std::vector<std::unique_ptr<EnginePatch>> retiredPatches;
    std::vector<TablePtr> retiredTables;
    std::array<std::atomic<float>*, 8> macroParams {};
    std::atomic<float>* outParam = nullptr;
    bool libWasReady = false;
    std::unique_ptr<juce::Thread> scanThread;
    juce::CriticalSection browserLock;
};
}
