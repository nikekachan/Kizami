#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <memory>
#include <map>
#include <mutex>
#include <atomic>

namespace shz
{
using Frame = std::vector<float>; // 2048 samples

/** Band-limited wavetable: 11 mip levels, each frames*2049 samples (one guard sample per frame). */
struct Table
{
    int n = 0;
    std::vector<std::vector<float>> lv;
    std::vector<Frame> frames; // for display
};
using TablePtr = std::shared_ptr<const Table>;

struct Wav { std::vector<float> samples; double sr = 44100; int frameSize = 0; };
bool parseWav (const void* data, size_t size, Wav& out);
std::vector<Frame> framesFrom (const std::vector<float>& samples, int frameSize);
TablePtr buildTable (const std::vector<Frame>& frames, int maxFrames = 64);
const std::map<juce::String, std::vector<Frame>>& builtinTables();

/** Finds Serum 2's factory/user wavetables on disk (scanned once, in the background). */
class TableLibrary
{
public:
    static TableLibrary& get();
    static juce::Array<juce::File> defaultRoots();
    void setExtraRoot (const juce::File& f);
    juce::File extraRoot() const;
    void rescan();
    bool ready() const { return scanned.load(); }
    int count() const;
    /** Serum relative path (e.g. "Analog/Jno.wav" or "S2 Tables/Digital/JF Flute.wav") → file, or {} */
    juce::File find (const juce::String& relPath) const;
    juce::StringArray rootsFound() const;
private:
    TableLibrary();
    ~TableLibrary() { if (thread) thread->stopThread (3000); }
    void scan();
    mutable std::mutex m;
    std::map<juce::String, juce::File> byPath;   // lower-case path suffixes
    std::map<juce::String, juce::File> byName;   // lower-case file name
    juce::File extra;
    juce::StringArray roots;
    std::atomic<bool> scanned { false };
    std::unique_ptr<juce::Thread> thread;
};
}
