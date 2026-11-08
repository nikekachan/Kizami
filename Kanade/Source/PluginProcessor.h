#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Core.h"
#include <atomic>
#include <memory>
#include <mutex>

namespace kanade
{
inline juce::String jp (const char* s) { return juce::String::fromUTF8 (s); }

/** The vocal as transferred from the host: never changes after analysis. */
struct Session
{
    double sr = 44100;
    juce::int64 start = 0;                 // host sample position of audio[0]
    std::vector<float> audio;
    Analysis A;
    std::vector<Run> runs;
    std::vector<std::vector<int>> marks;
    juce::String name;
    double startSec() const { return (double) start / sr; }
    double endSec() const { return (double) (start + (juce::int64) audio.size()) / sr; }
};

/** What the audio thread plays: the corrected audio for one session. */
struct Playback
{
    std::shared_ptr<const Session> s;
    std::vector<float> out;
};

class KanadeProcessor : public juce::AudioProcessor, private juce::Timer, private juce::Thread
{
public:
    KanadeProcessor();
    ~KanadeProcessor() override;

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "KANADE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---------------- message thread: the edit
    std::shared_ptr<Session> session;      // null until something was transferred
    std::vector<Note> notes;
    int nextId = 1;
    float globalFormant = 0;
    int key = 0, scale = chromatic;
    bool snapOn = true;
    float kCenter = 1, kVariation = 0.5f, kTrans = 0.9f;
    int cutDiv = 16;
    std::atomic<int> generation { 0 };     // bumps when notes/session change from outside the editor

    void notesChanged (const std::vector<int>& noteIds, bool all = false);   // re-render (in the background)
    void pushUndo();
    bool undo();
    bool redo();
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }

    void armRecord (bool on);
    void clearSession();
    bool loadFile (const juce::File&, juce::String& err);
    bool writeWav (const juce::File&) const;
    juce::File dragFile() const;           // renders the corrected vocal to a temp WAV for drag & drop

    // ---------------- preview (plugin's own ▶ ■ while the host is stopped)
    void preview (double fromSec);
    void stopPreview() { previewOn = false; }
    std::atomic<bool> previewOn { false }, bypass { false };

    // ---------------- status for the UI
    std::atomic<bool> recArmed { false }, hostPlaying { false }, analysing { false }, rendering { false };
    std::atomic<float> progress { 0 };
    std::atomic<double> hostBpm { 120 }, hostSec { 0 }, previewSec { 0 }, capturedSec { 0 };
    std::atomic<float> peak { 0 };
    double sampleRate() const { return sr; }
    const Playback* livePlayback() const { return live.load(); }   // message thread: valid for drawing
    juce::String lastError;

private:
    void timerCallback() override;
    void run() override;
    void startAnalysis (std::vector<float> audio, juce::int64 start, double rate, juce::String name, std::vector<Note> restoreNotes = {});
    juce::String notesJson() const;
    void notesFromJson (const juce::String&);
    void publish (std::unique_ptr<Playback>);

    double sr = 44100;
    // audio-thread playback
    std::atomic<Playback*> live { nullptr };
    std::unique_ptr<Playback> current;
    std::vector<std::pair<std::unique_ptr<Playback>, juce::uint32>> retired;
    std::mutex retireLock;
    juce::int64 previewPos = 0;

    // capture
    std::vector<float> cap;
    std::atomic<juce::int64> capStart { -1 }, capMax { 0 };
    std::atomic<bool> capDone { false };
    bool wasPlaying = false;

    // background work
    std::mutex jobLock;
    struct AnalysisJob { std::vector<float> audio; juce::int64 start; double rate; juce::String name; std::vector<Note> restore; };
    std::unique_ptr<AnalysisJob> pendingAnalysis;
    bool renderAll = false;
    std::vector<int> renderRuns_;
    std::vector<Note> renderNotes; float renderFormant = 0; bool renderPending = false;
    std::atomic<bool> cancelAnalysis { false };
    juce::WaitableEvent wake;
    // results handed to the message thread
    std::mutex resultLock;
    std::shared_ptr<Session> readySession;
    std::vector<Note> readyNotes; bool readyNotesValid = false;

    std::vector<juce::String> undoStack, redoStack;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KanadeProcessor)
};
} // namespace kanade

using KanadeProcessor = kanade::KanadeProcessor;
