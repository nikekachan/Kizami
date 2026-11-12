#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace shz
{
ShizukuProcessor::ShizukuProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SHIZUKU", layout())
{
    for (int i = 0; i < 8; ++i) macroParams[(size_t) i] = apvts.getRawParameterValue ("m" + juce::String (i + 1));
    outParam = apvts.getRawParameterValue ("out");
    factory = factoryPatches();
    setPatch (factory[0]);
    TableLibrary::get();
    rescanBrowser();
    startTimerHz (10);
}
ShizukuProcessor::~ShizukuProcessor()
{
    stopTimer();
    if (scanThread) scanThread->stopThread (3000);
}

juce::AudioProcessorValueTreeState::ParameterLayout ShizukuProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (int i = 1; i <= 8; ++i)
        l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "m" + juce::String (i), 1 }, "Macro " + juce::String (i), juce::NormalisableRange<float> (0, 100), 0.0f));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "out", 1 }, "Output", juce::NormalisableRange<float> (-24, 12), 0.0f,
                                                        juce::AudioParameterFloatAttributes().withLabel ("dB")));
    return l;
}

bool ShizukuProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() || l.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void ShizukuProcessor::prepareToPlay (double sampleRate, int)
{
    if (std::abs (sampleRate - sr) > 0.5 || engine.patch() == nullptr)
    {
        sr = sampleRate;
        juce::SpinLock::ScopedLockType sl (lock);
        engine.setSampleRate (sr);
        pending = compile (patch, sr); pendingFull = true;
    }
    keyboard.reset();
}

void ShizukuProcessor::push (bool full)
{
    auto np = compile (patch, sr);
    juce::SpinLock::ScopedLockType sl (lock);
    pending = std::move (np);
    pendingFull = pendingFull || full;
}

void ShizukuProcessor::setPatch (const Patch& p)
{
    patch = p;
    for (int i = 0; i < 8; ++i)
        if (auto* prm = apvts.getParameter ("m" + juce::String (i + 1)))
            prm->setValueNotifyingHost (prm->convertTo0to1 (patch.macro[(size_t) i].val));
    push (true);
    resolveTables();
}
void ShizukuProcessor::patchEdited() { push (false); }
void ShizukuProcessor::structureEdited() { push (false); }

void ShizukuProcessor::resolveTables()
{
    std::array<TablePtr, 3> t;
    auto& lib = TableLibrary::get();
    for (int i = 0; i < 3; ++i)
    {
        auto& o = patch.osc[(size_t) i];
        TableStatus status = TableStatus::builtin;
        std::vector<Frame> frames;
        if (o.sample)
        {
            if (! o.sampleData && o.samplePath.isNotEmpty())
            {
                juce::File f (o.samplePath);
                if (! f.existsAsFile() && lib.ready()) f = lib.find (o.samplePath.fromLastOccurrenceOf ("/", false, false).fromLastOccurrenceOf ("\\", false, false));
                juce::MemoryBlock mb;
                Wav w;
                if (f.existsAsFile() && f.loadFileAsData (mb) && parseWav (mb.getData(), mb.getSize(), w))
                { o.sampleData = std::make_shared<std::vector<float>> (std::move (w.samples)); o.sampleRate = (float) w.sr; }
            }
            status = o.sampleData ? TableStatus::sampleOk : TableStatus::sampleMissing;
            frames = builtinTables().at ("Default Shapes");
        }
        else if (o.embedded) { frames = framesFrom (*o.embedded, 2048); status = TableStatus::embedded; }
        else if (o.tablePath.isNotEmpty())
        {
            const auto f = lib.find (o.tablePath);
            juce::MemoryBlock mb; Wav w;
            if (f.existsAsFile() && f.loadFileAsData (mb) && parseWav (mb.getData(), mb.getSize(), w))
            { frames = framesFrom (w.samples, w.frameSize > 0 ? w.frameSize : 2048); status = TableStatus::lib; }
            else
            {
                const auto low = o.tablePath.toLowerCase();
                const char* key = low.contains ("default shapes") ? "Default Shapes" : low.contains ("basic shapes") ? "Basic Shapes" : nullptr;
                frames = builtinTables().at (key ? key : "Default Shapes");
                status = key ? TableStatus::approx : TableStatus::missing;
            }
        }
        else frames = builtinTables().at ("Default Shapes");
        t[(size_t) i] = buildTable (frames);
        tableStatus[(size_t) i] = status;
    }
    shownTables = t;
    {
        juce::SpinLock::ScopedLockType sl (lock);
        pendingTables = t; tablesPending = true;
    }
    ++tablesGeneration;
    // sample data may have been attached above → engine needs the patch again
    push (false);
}

void ShizukuProcessor::timerCallback()
{
    std::vector<std::unique_ptr<EnginePatch>> rp; std::vector<TablePtr> rt;
    {
        juce::SpinLock::ScopedLockType sl (lock);
        rp.swap (retiredPatches); rt.swap (retiredTables);
    }
    rp.clear(); rt.clear();
    meter.store (engine.meter);
    const bool ready = TableLibrary::get().ready();
    if (ready && ! libWasReady)
    {
        bool need = false;
        for (int i = 0; i < 3; ++i) if (tableStatus[(size_t) i] == TableStatus::missing || tableStatus[(size_t) i] == TableStatus::approx || tableStatus[(size_t) i] == TableStatus::sampleMissing) need = true;
        if (need) resolveTables();
    }
    libWasReady = ready;
}

void ShizukuProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    keyboard.processNextMidiBuffer (midi, 0, n, true);
    {
        juce::SpinLock::ScopedTryLockType sl (lock);
        if (sl.isLocked())
        {
            if (pending) { auto old = engine.swapPatch (std::move (pending), pendingFull); pendingFull = false; if (old) retiredPatches.push_back (std::move (old)); }
            if (tablesPending)
            {
                for (int i = 0; i < 3; ++i) { retiredTables.push_back (engine.tableAt (i)); engine.setTable (i, std::move (pendingTables[(size_t) i])); }
                tablesPending = false;
            }
        }
    }
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) { engine.bpm = *b; hostBpm.store ((float) *b); }
    for (int i = 0; i < 8; ++i) engine.macros[(size_t) i] = macroParams[(size_t) i]->load();
    engine.outGain = juce::Decibels::decibelsToGain (outParam->load());
    const double ub = uiBend.exchange (1e9); if (ub < 1e8) engine.bend = ub;
    const double um = uiModWheel.exchange (-1); if (um >= 0) engine.modwheel = um;

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    float monoR[8192];
    int pos = 0;
    auto render = [&] (int upto)
    {
        while (pos < upto)
        {
            const int len = std::min (upto - pos, 4096);
            engine.process (L + pos, R ? R + pos : monoR, len);
            if (! R) for (int i = 0; i < len; ++i) L[pos + i] = 0.5f * (L[pos + i] + monoR[i]);
            pos += len;
        }
    };
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        render (juce::jlimit (0, n, meta.samplePosition));
        if (m.isNoteOn()) engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) engine.noteOff (m.getNoteNumber());
        else if (m.isPitchWheel()) engine.bend = (m.getPitchWheelValue() - 8192) / 8192.0;
        else if (m.isController() && m.getControllerNumber() == 1) engine.modwheel = m.getControllerValue() / 127.0;
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allOff (m.isAllSoundOff());
    }
    render (n);
    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
}

//==============================================================================
static Patch withMacrosFromParams (Patch p, juce::AudioProcessorValueTreeState& s)
{
    for (int i = 0; i < 8; ++i) p.macro[(size_t) i].val = s.getRawParameterValue ("m" + juce::String (i + 1))->load();
    return p;
}

void ShizukuProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty ("patch", withMacrosFromParams (patch, apvts).toVar (true));
    o->setProperty ("out", (double) outParam->load());
    const auto s = juce::JSON::toString (juce::var (o.get()), true);
    juce::MemoryOutputStream mo (dest, false);
    mo.writeString ("SHIZUKU1");
    juce::GZIPCompressorOutputStream gz (mo, 6);
    gz.writeString (s);
}

void ShizukuProcessor::setStateInformation (const void* data, int size)
{
    juce::MemoryInputStream mi (data, (size_t) size, false);
    if (mi.readString() != "SHIZUKU1") return;
    juce::GZIPDecompressorInputStream gz (mi);
    const auto v = juce::JSON::parse (gz.readString());
    if (! v.isObject()) return;
    Patch p = Patch::fromVar (v["patch"]);
    setPatch (p);
    if (auto* prm = apvts.getParameter ("out")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (double) v["out"]));
}

//==============================================================================
bool ShizukuProcessor::loadFile (const juce::File& f, juce::String& error)
{
    juce::MemoryBlock mb;
    if (! f.loadFileAsData (mb)) { error = jp ("ファイルを開けませんでした"); return false; }
    try
    {
        if (f.hasFileExtension ("shizuku") || f.hasFileExtension ("json"))
        {
            const auto v = juce::JSON::parse (mb.toString());
            if (! v.isObject() || ! v.hasProperty ("shizuku")) { error = jp ("SHIZUKUの保存ファイルではありません"); return false; }
            setPatch (Patch::fromVar (v));
        }
        else setPatch (readSerumPreset (mb));
        return true;
    }
    catch (const std::exception& e) { error = juce::String::fromUTF8 (e.what()); }
    return false;
}

bool ShizukuProcessor::savePatch (const juce::File& f)
{
    const auto p = withMacrosFromParams (patch, apvts);
    return f.replaceWithText (juce::JSON::toString (p.toVar (true), false));
}

void ShizukuProcessor::loadOscWav (int i, const juce::File& f, juce::String& error)
{
    juce::MemoryBlock mb; Wav w;
    if (! f.loadFileAsData (mb) || ! parseWav (mb.getData(), mb.getSize(), w)) { error = jp ("WAVを読み込めませんでした"); return; }
    auto& o = patch.osc[(size_t) i];
    if (o.sample) { o.sampleData = std::make_shared<std::vector<float>> (std::move (w.samples)); o.sampleRate = (float) w.sr; }
    else
    {
        if (w.frameSize > 0 && w.frameSize != 2048) { auto fr = framesFrom (w.samples, w.frameSize); auto e = std::make_shared<std::vector<float>>(); for (auto& x : fr) e->insert (e->end(), x.begin(), x.end()); o.embedded = e; }
        else o.embedded = std::make_shared<std::vector<float>> (std::move (w.samples));
        o.tablePath = {}; o.tableName = f.getFileNameWithoutExtension();
    }
    resolveTables();
}

//==============================================================================
juce::File ShizukuProcessor::userPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Library/Audio/Presets/DJ KATSUNII/SHIZUKU");
}

void ShizukuProcessor::rescanBrowser()
{
    if (scanThread) scanThread->stopThread (3000);
    browserReady = false;
    struct T : juce::Thread
    {
        ShizukuProcessor& o; explicit T (ShizukuProcessor& x) : juce::Thread ("SHIZUKU presets"), o (x) {}
        void run() override
        {
            std::vector<BrowserEntry> list;
            for (int i = 0; i < (int) o.factory.size(); ++i) list.push_back ({ o.factory[(size_t) i].name, "SHIZUKU", {}, i });
            auto addDir = [&] (const juce::File& root, const juce::String& pattern, const juce::String& label)
            {
                if (! root.isDirectory()) return;
                std::vector<BrowserEntry> part;
                for (const auto& e : juce::RangedDirectoryIterator (root, true, pattern, juce::File::findFiles))
                {
                    if (threadShouldExit()) return;
                    const auto f = e.getFile();
                    auto folder = f.getParentDirectory().getRelativePathFrom (root).replaceCharacter ('\\', '/');
                    if (folder == ".") folder = {};
                    part.push_back ({ f.getFileNameWithoutExtension(), label + (folder.isNotEmpty() ? "/" + folder : juce::String()), f, -1 });
                    if (part.size() > 20000) break;
                }
                std::sort (part.begin(), part.end(), [] (const BrowserEntry& a, const BrowserEntry& b) { return a.folder == b.folder ? a.name.compareNatural (b.name) < 0 : a.folder.compareNatural (b.folder) < 0; });
                list.insert (list.end(), part.begin(), part.end());
            };
            addDir (userPresetFolder(), "*.shizuku", "My SHIZUKU");
            juce::StringArray seen;
            for (auto& tablesRoot : TableLibrary::defaultRoots())
            {
                const auto presets = tablesRoot.getParentDirectory().getChildFile ("Presets");
                if (! presets.isDirectory() || seen.contains (presets.getFullPathName())) continue;
                seen.add (presets.getFullPathName());
                addDir (presets, "*.SerumPreset", "Serum 2");
            }
            const juce::ScopedLock sl (o.browserLock);
            o.browser = std::move (list);
            o.browserReady = true;
        }
    };
    scanThread = std::make_unique<T> (*this);
    scanThread->startThread (juce::Thread::Priority::low);
}

bool ShizukuProcessor::loadBrowser (int index, juce::String& error)
{
    BrowserEntry e;
    {
        const juce::ScopedLock sl (browserLock);
        if (browser.empty()) return false;
        index = (index % (int) browser.size() + (int) browser.size()) % (int) browser.size();
        e = browser[(size_t) index];
    }
    browserIndex = index;
    if (e.factoryIndex >= 0) { setPatch (factory[(size_t) e.factoryIndex]); return true; }
    return loadFile (e.file, error);
}

juce::AudioProcessorEditor* ShizukuProcessor::createEditor() { return new ui::ShizukuEditor (*this); }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new shz::ShizukuProcessor(); }
