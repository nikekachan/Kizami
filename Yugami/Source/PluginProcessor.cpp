#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace yugami
{
static juce::File mineFile()
{
    if (auto env = juce::SystemStats::getEnvironmentVariable ("YUGAMI_MINE_FILE", {}); env.isNotEmpty()) return juce::File (env);
    auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    d = d.getChildFile ("Application Support");
   #endif
    return d.getChildFile ("DJ KATSUNII").getChildFile ("YUGAMI").getChildFile ("my-presets.json");
}

juce::AudioProcessorValueTreeState::ParameterLayout YugamiProcessor::layout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; });
    auto ms = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 0.05f ? String ("0 ms") : v < 10 ? String (v, 1) + " ms" : String (roundToInt (v)) + " ms"; });
    auto db = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; });
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "tSlot", 1 }, jp ("タイムのスロット"), 1, kSlots, 2));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "vSlot", 1 }, jp ("ボリュームのスロット"), 1, kSlots, 1));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "tMix", 1 }, jp ("タイムの量"), NormalisableRange<float> (0, 100), 100.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "vMix", 1 }, jp ("ボリュームの量"), NormalisableRange<float> (0, 100), 100.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "attack", 1 }, jp ("アタック"), NormalisableRange<float> (0, 100, 0, 0.3f), 2.0f, ms));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, jp ("リリース"), NormalisableRange<float> (0, 200, 0, 0.3f), 4.0f, ms));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "smooth", 1 }, jp ("スムーズ"), NormalisableRange<float> (0, 50, 0, 0.35f), 4.0f, ms));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, jp ("出力"), NormalisableRange<float> (-24, 6), 0.0f, db));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "limiter", 1 }, jp ("リミッター"), true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "hold", 1 }, "HOLD", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "trigger", 1 }, jp ("キーで呼んだとき"),
                                                   StringArray { jp ("曲の位置に合わせる"), jp ("押した瞬間から") }, 0));
    return l;
}

YugamiProcessor::YugamiProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "YUGAMI", layout())
{
    pages = factoryPages();
    loadMine();
    heldT.reserve (64); heldV.reserve (64);
    rebuildTables();
}

YugamiProcessor::~YugamiProcessor() = default;

void YugamiProcessor::setParam (const juce::String& id, float plain)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (plain));
}

bool YugamiProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono());
}

void YugamiProcessor::prepareToPlay (double sr, int maxBlock)
{
    engine.prepare (sr);
    sampleRate = sr > 0 ? sr : 48000.0;
    scratch.assign ((size_t) juce::jmax (512, maxBlock * 2), 0.0f);
    heldT.clear(); heldV.clear();
    internalBeat = 0;
}

//==============================================================================
void YugamiProcessor::rebuildTables()
{
    auto t = std::make_unique<Tables>();
    const auto& pg = pages[(size_t) juce::jlimit (0, (int) pages.size() - 1, page)];
    for (int i = 0; i < kSlots; ++i)
    {
        compile (pg.time[(size_t) i], t->t[i]); t->tLen[i] = pg.time[(size_t) i].len;
        compile (pg.vol[(size_t) i], t->v[i]);  t->vLen[i] = pg.vol[(size_t) i].len;
    }
    live.store (t.get(), std::memory_order_release);
    const auto now = juce::Time::getMillisecondCounter();
    if (current) retired.emplace_back (std::move (current), now);
    current = std::move (t);
    // an audio block is far shorter than this; old tables are no longer referenced
    retired.erase (std::remove_if (retired.begin(), retired.end(), [now] (auto& r) { return now - r.second > 2000; }), retired.end());
}

void YugamiProcessor::slotEdited() { rebuildTables(); }

void YugamiProcessor::setPage (int p)
{
    page = juce::jlimit (0, (int) pages.size() - 1, p);
    rebuildTables();
}

bool YugamiProcessor::addPage()
{
    if ((int) pages.size() >= kMaxPages) return false;
    pages.insert (pages.begin() + page + 1, emptyPage (jp ("ページ ") + juce::String ((int) pages.size() + 1)));
    setPage (page + 1);
    return true;
}

bool YugamiProcessor::deletePage()
{
    if (pages.size() < 2) return false;
    pages.erase (pages.begin() + page);
    setPage (juce::jmin (page, (int) pages.size() - 1));
    return true;
}

void YugamiProcessor::resetFactory()
{
    pages = factoryPages();
    page = 0; editTime = true; editIdx = 1;
    setParam ("tSlot", 2); setParam ("vSlot", 1);
    rebuildTables();
    ++bankGeneration;
}

juce::var YugamiProcessor::setToVar (bool withMine) const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("yugami", 1);
    juce::Array<juce::var> ps;
    for (auto& p : pages) ps.add (toVar (p));
    o->setProperty ("pages", ps);
    o->setProperty ("page", page);
    if (withMine)
    {
        juce::Array<juce::var> t, v;
        for (auto& p : mineTime) t.add (toVar (p));
        for (auto& p : mineVol) v.add (toVar (p));
        auto* m = new juce::DynamicObject(); m->setProperty ("t", t); m->setProperty ("v", v);
        o->setProperty ("mine", m);
    }
    return o;
}

bool YugamiProcessor::exportSet (const juce::File& f) const
{
    return f.replaceWithText (juce::JSON::toString (setToVar (true), true));
}

bool YugamiProcessor::importSet (const juce::File& f, juce::String& err)
{
    auto v = juce::JSON::parse (f.loadFileAsString());
    std::vector<Page> ps;
    if (auto* a = v["pages"].getArray())
        for (auto& x : *a) { Page p; if (fromVar (x, p)) ps.push_back (std::move (p)); }
    // TOKI / browser prototype files: {bank:{pages:[...]}, mine:{t,v}} with arrays as [x,y,b] and {name,len,nodes}
    if (ps.empty())
        if (auto* a = v["bank"]["pages"].getArray())
            for (auto& x : *a)
            {
                Page p = emptyPage (x["name"].toString());
                auto conv = [] (const juce::var& s, Slot& out) {
                    auto* o = new juce::DynamicObject(); o->setProperty ("n", s["name"]); o->setProperty ("l", s["len"]); o->setProperty ("k", s["nodes"]);
                    return fromVar (juce::var (o), out); };
                auto* t = x["time"].getArray(); auto* vo = x["vol"].getArray();
                if (t == nullptr || vo == nullptr || t->size() != kSlots || vo->size() != kSlots) continue;
                for (int i = 0; i < kSlots; ++i) { conv ((*t)[i], p.time[(size_t) i]); conv ((*vo)[i], p.vol[(size_t) i]); }
                ps.push_back (std::move (p));
            }
    if (ps.empty()) { err = jp ("YUGAMIのセットファイルではありませんでした。"); return false; }
    if (ps.size() > (size_t) kMaxPages) ps.resize ((size_t) kMaxPages);
    pages = std::move (ps);
    auto mine = v["mine"];
    auto addMine = [] (const juce::var& arr, std::vector<Preset>& out) {
        if (auto* a = arr.getArray())
            for (auto& x : *a)
            {
                Preset p;
                if (! fromVar (x, p))
                {
                    auto* o = new juce::DynamicObject(); o->setProperty ("n", x["name"]); o->setProperty ("l", x["len"]); o->setProperty ("k", x["nodes"]);
                    if (! fromVar (juce::var (o), p)) continue;
                }
                p.cat = jp ("マイプリセット");
                const bool dup = std::any_of (out.begin(), out.end(), [&] (const Preset& q) { return q.name == p.name; });
                if (! dup) out.push_back (p);
            } };
    addMine (mine["t"], mineTime); addMine (mine["v"], mineVol);
    writeMine();
    setPage (juce::jlimit (0, (int) pages.size() - 1, (int) v["page"]));
    ++bankGeneration;
    return true;
}

void YugamiProcessor::saveMine (bool time, const Slot& s, juce::String& savedName)
{
    auto& list = time ? mineTime : mineVol;
    juce::String base = s.isEmpty() || s.name.startsWith (jp ("空き")) ? jp ("マイ ") + juce::String ((int) list.size() + 1) : s.name;
    juce::String n = base; int c = 2;
    while (std::any_of (list.begin(), list.end(), [&] (const Preset& q) { return q.name == n; })) n = base.substring (0, 9) + " " + juce::String (c++);
    list.push_back ({ n, jp ("マイプリセット"), s.len, s.nodes });
    savedName = n;
    writeMine();
}

void YugamiProcessor::deleteMine (bool time, int idx)
{
    auto& list = time ? mineTime : mineVol;
    if (idx >= 0 && idx < (int) list.size()) list.erase (list.begin() + idx);
    writeMine();
}

void YugamiProcessor::loadMine()
{
    auto v = juce::JSON::parse (mineFile().loadFileAsString());
    mineTime.clear(); mineVol.clear();
    auto load = [] (const juce::var& arr, std::vector<Preset>& out) {
        if (auto* a = arr.getArray()) for (auto& x : *a) { Preset p; if (fromVar (x, p)) { p.cat = jp ("マイプリセット"); out.push_back (p); } } };
    load (v["t"], mineTime); load (v["v"], mineVol);
}

void YugamiProcessor::writeMine() const
{
    auto f = mineFile();
    f.getParentDirectory().createDirectory();
    juce::Array<juce::var> t, v;
    for (auto& p : mineTime) t.add (toVar (p));
    for (auto& p : mineVol) v.add (toVar (p));
    auto* o = new juce::DynamicObject(); o->setProperty ("t", t); o->setProperty ("v", v);
    f.replaceWithText (juce::JSON::toString (juce::var (o), true));
}

//==============================================================================
void YugamiProcessor::pushTrigger (bool time, int slot, bool on)
{
    int s1, n1, s2, n2;
    fifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 > 0) { fifoData[(size_t) s1] = { time ? 1 : 0, slot, on ? 1 : 0 }; fifo.finishedWrite (1); }
}

void YugamiProcessor::handleTrigger (bool time, int slot, bool on)
{
    if (slot < 0 || slot >= kSlots) return;
    auto& held = time ? heldT : heldV;
    const bool latch = param ("hold") > 0.5f;
    if (on)
    {
        if (latch) held.clear();
        held.erase (std::remove (held.begin(), held.end(), slot), held.end());
        if (held.size() < 60) held.push_back (slot);
    }
    else
    {
        if (latch) return;   // HOLD: the slot stays until another one is called
        held.erase (std::remove (held.begin(), held.end(), slot), held.end());
    }
    const int top = held.empty() ? -1 : held.back();
    engine.setHold (time, top, (int) param ("trigger") == 1);
}

void YugamiProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);

    // HOLD switched off: let go of latched slots
    const bool latch = param ("hold") > 0.5f;
    if (lastLatch && ! latch) { heldT.clear(); heldV.clear(); engine.setHold (true, -1, false); engine.setHold (false, -1, false); }
    lastLatch = latch;

    {
        int s1, n1, s2, n2;
        fifo.prepareToRead (fifo.getNumReady(), s1, n1, s2, n2);
        for (int k = 0; k < n1; ++k) { auto& e = fifoData[(size_t) (s1 + k)]; handleTrigger (e[0] == 1, e[1], e[2] == 1); }
        for (int k = 0; k < n2; ++k) { auto& e = fifoData[(size_t) (s2 + k)]; handleTrigger (e[0] == 1, e[1], e[2] == 1); }
        fifo.finishedRead (n1 + n2);
    }

    bool playing = true, hasPpq = false; double ppq = 0, bpm = 120;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) { ppq = *q; hasPpq = true; }
        }
    }
    if (! hasPpq) ppq = internalBeat;

    const Tables* T = live.load (std::memory_order_acquire);
    Engine::Settings s;
    s.baseT = (int) param ("tSlot") - 1; s.baseV = (int) param ("vSlot") - 1;
    s.tMix = param ("tMix") / 100.0f; s.vMix = param ("vMix") / 100.0f;
    s.attack = param ("attack") / 1000.0f; s.release = param ("release") / 1000.0f; s.smooth = param ("smooth") / 1000.0f;
    s.out = juce::Decibels::decibelsToGain (param ("output"));
    s.limiter = param ("limiter") > 0.5f;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    if (R == nullptr)
    {
        if ((int) scratch.size() < n) scratch.resize ((size_t) n);
        std::copy (L, L + n, scratch.data()); R = scratch.data();
    }

    const double spb = sampleRate * 60.0 / juce::jlimit (30.0, 300.0, bpm);
    int pos = 0;
    auto run = [&] (int upTo) {
        if (upTo <= pos || T == nullptr) return;
        engine.process (L + pos, R + pos, upTo - pos, *T, s, playing, true, ppq + (double) pos / spb, bpm);
        pos = upTo;
    };
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int at = juce::jlimit (0, n, meta.samplePosition);
        const int note = m.getNoteNumber();
        if (! (m.isNoteOn() || m.isNoteOff())) continue;
        const bool time = note >= 36 && note <= 71, vol = note >= 72 && note <= 107;
        if (! time && ! vol) continue;
        run (at);
        handleTrigger (time, time ? note - 36 : note - 72, m.isNoteOn());
    }
    run (n);
    if (playing) internalBeat = (hasPpq ? ppq : internalBeat) + n / spb;

    phaseT = engine.phaseT; phaseV = engine.phaseV; delayBars = engine.delayBars; peak = engine.peak; level = engine.level;
    hotT = engine.holdT; hotV = engine.holdV; running = playing;
}

//==============================================================================
void YugamiProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    juce::ValueTree bank ("BANK");
    bank.setProperty ("json", juce::JSON::toString (setToVar (false), true), nullptr);
    bank.setProperty ("editTime", editTime, nullptr);
    bank.setProperty ("editIdx", editIdx, nullptr);
    st.removeChild (st.getChildWithName ("BANK"), nullptr);
    st.appendChild (bank, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void YugamiProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto st = juce::ValueTree::fromXml (*xml);
    if (! st.isValid()) return;
    auto bank = st.getChildWithName ("BANK");
    if (bank.isValid())
    {
        auto v = juce::JSON::parse (bank["json"].toString());
        std::vector<Page> ps;
        if (auto* a = v["pages"].getArray()) for (auto& x : *a) { Page p; if (fromVar (x, p)) ps.push_back (std::move (p)); }
        if (! ps.empty())
        {
            if (ps.size() > (size_t) kMaxPages) ps.resize ((size_t) kMaxPages);
            pages = std::move (ps);
            page = juce::jlimit (0, (int) pages.size() - 1, (int) v["page"]);
        }
        editTime = (bool) bank.getProperty ("editTime", true);
        editIdx = juce::jlimit (0, kSlots - 1, (int) bank.getProperty ("editIdx", 1));
        st.removeChild (bank, nullptr);
    }
    apvts.replaceState (st);
    rebuildTables();
    ++bankGeneration;
}
} // namespace yugami

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new yugami::YugamiProcessor(); }

juce::AudioProcessorEditor* yugami::YugamiProcessor::createEditor() { return new YugamiEditor (*this); }
