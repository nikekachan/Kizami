#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace nagare;

namespace nagare
{
juce::String jp (const char* utf8) { return juce::String::fromUTF8 (utf8); }

juce::String noteName (double midi)
{
    static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int m = (int) std::round (midi);
    return juce::String (n[((m % 12) + 12) % 12]) + juce::String ((int) std::floor (m / 12.0));
}

juce::String fmtMs (float v)
{
    if (v < 0.05f) return "0 ms";
    if (v < 10.0f) return juce::String (v, 1) + " ms";
    if (v < 1000.0f) return juce::String (juce::roundToInt (v)) + " ms";
    return juce::String (v / 1000.0f, 2) + " s";
}

juce::String fmtDb (float g)
{
    if (g <= 0.0005f) return juce::String::fromUTF8 ("-∞ dB");
    return juce::String (20.0f * std::log10 (g), 1) + " dB";
}

static const BuiltinPreset presets[] = {
    // name                                   source            mode        mono   demo  lS    lE    pit  att  dec  sus   rel  vol cut     res  gli del   rev
    { "サンプル例:減衰するプラックを持続", Source::sample,   Mode::loop, false, true, 0.28f,0.34f, 0,   3,   600, 0.6f, 500, 0,  20000,  0.7f, 60, 0.18f,0.25f },
    { "やわらかリード",                     Source::triangle, Mode::loop, true,  false,0.28f,0.34f, 0,   14,  400, 0.8f, 320, 0,  5200,   1.6f, 90, 0.22f,0.22f },
    { "スーパーソー・リード",               Source::supersaw, Mode::loop, true,  false,0.28f,0.34f, 0,   8,   500, 0.75f,450, -3, 9000,   1.0f, 50, 0.26f,0.3f  },
    { "8bitリード",                         Source::square,   Mode::loop, true,  false,0.28f,0.34f, 0,   1,   120, 0.7f, 60,  -4, 20000,  0.7f, 0,  0.12f,0.08f },
    { "オルガン",                           Source::organ,    Mode::loop, false, false,0.28f,0.34f, 0,   8,   200, 1.0f, 120, -2, 12000,  0.7f, 0,  0.05f,0.25f },
};
int builtinPresetCount() { return (int) std::size (presets); }
const BuiltinPreset& builtinPreset (int i) { return presets[juce::jlimit (0, builtinPresetCount() - 1, i)]; }
}

//==============================================================================
static juce::NormalisableRange<float> expRange (float lo, float hi)
{
    return { lo, hi, [] (float a, float b, float n) { return a * std::pow (b / a, n); },
                     [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); } };
}
static juce::NormalisableRange<float> cubeRange (float lo, float hi)
{
    return { lo, hi, [] (float a, float b, float n) { return a + n * n * n * (b - a); },
                     [] (float a, float b, float v) { return std::cbrt ((v - a) / (b - a)); } };
}
static juce::NormalisableRange<float> sqRange (float lo, float hi)
{
    return { lo, hi, [] (float a, float b, float n) { return a + n * n * (b - a); },
                     [] (float a, float b, float v) { return std::sqrt ((v - a) / (b - a)); } };
}

juce::AudioProcessorValueTreeState::ParameterLayout NagareProcessor::createLayout (NagareProcessor& self)
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto f = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, std::function<String (float)> txt) {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, jp (name), r, def,
                   AudioParameterFloatAttributes().withStringFromValueFunction ([txt] (float v, int) { return txt (v); })));
    };
    auto pos = [&self] (float v) {
        const double s = v * self.sampleSeconds.load();
        return s < 1.0 ? String (roundToInt (s * 1000.0)) + " ms" : String (s, 2) + " s";
    };
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "source", 1 }, jp ("音源"),
        StringArray { jp ("サンプル"), jp ("サイン"), jp ("三角"), jp ("ノコギリ"), jp ("矩形"), jp ("スーパーソー"), jp ("オルガン") }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, jp ("持続のさせ方"), StringArray { jp ("ループ"), jp ("波形化") }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "voice", 1 }, jp ("発音"), StringArray { jp ("ポリ"), jp ("モノ") }, 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "keepLen", 1 }, jp ("高い音でも長さを保つ"), true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "hold", 1 }, jp ("ホールド"), false));
    f ("loopStart", "ループ開始", { 0.0f, 1.0f }, 0.28f, pos);
    f ("loopEnd", "ループ終了", { 0.0f, 1.0f }, 0.34f, pos);
    f ("pitch", "ピッチ補正", { -12.0f, 12.0f }, 0.0f, [] (float v) { return (v > 0 ? "+" : "") + String (v, 2); });
    f ("attack", "アタック", cubeRange (0.0f, 5000.0f), 5.0f, fmtMs);
    f ("decay", "ディケイ", expRange (5.0f, 5000.0f), 350.0f, fmtMs);
    f ("sustain", "サステイン", { 0.0f, 1.0f }, 0.75f, fmtDb);
    f ("release", "リリース", expRange (5.0f, 8000.0f), 350.0f, fmtMs);
    f ("volume", "音量", { -30.0f, 6.0f }, 0.0f, [] (float v) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; });
    f ("cutoff", "カットオフ", expRange (60.0f, 20000.0f), 20000.0f, [] (float v) {
        return v >= 19500 ? jp ("全開") : v >= 1000 ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; });
    f ("reso", "レゾナンス", { 0.5f, 18.0f }, 0.7f, [] (float v) { return String (v, 1); });
    f ("glide", "グライド", sqRange (0.0f, 1000.0f), 60.0f, [] (float v) { return v < 1 ? jp ("オフ") : String (roundToInt (v)) + " ms"; });
    f ("delay", "ディレイ", { 0.0f, 1.0f }, 0.15f, [] (float v) { return String (roundToInt (v * 100)) + "%"; });
    f ("reverb", "リバーブ", { 0.0f, 1.0f }, 0.2f, [] (float v) { return String (roundToInt (v * 100)) + "%"; });
    return l;
}

NagareProcessor::NagareProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NAGARE", createLayout (*this))
{
    warmUpTables();
    hostMap.fill (-1);
    uiMap.fill (-1);
    for (auto& b : heldBits) b.store (0);
    applyBuiltin (0);
    startTimerHz (30);
}

NagareProcessor::~NagareProcessor() { stopTimer(); }

bool NagareProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void NagareProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.prepare (sampleRate);
    fx.prepare (sampleRate, juce::jmax (512, samplesPerBlock));
    monoBuf.assign ((size_t) juce::jmax (512, samplesPerBlock), 0.0f);
    playedRefs.fill (0);
    hostMap.fill (-1);
    uiMap.fill (-1);
}

nagare::BlockParams NagareProcessor::readParams() const
{
    BlockParams p;
    p.source = (Source) juce::jlimit (0, 6, (int) param ("source"));
    p.mode = (Mode) juce::jlimit (0, 1, (int) param ("mode"));
    p.mono = param ("voice") > 0.5f;
    p.keepLength = param ("keepLen") > 0.5f;
    p.pitchAdjust = param ("pitch");
    p.attackMs = param ("attack");
    p.decayMs = param ("decay");
    p.sustain = param ("sustain");
    p.releaseMs = param ("release");
    p.glideMs = param ("glide");
    return p;
}

void NagareProcessor::setHeldBit (int note, bool on)
{
    auto& w = heldBits[(size_t) (note >> 5)];
    const juce::uint32 bit = 1u << (note & 31);
    if (on) w.fetch_or (bit); else w.fetch_and (~bit);
}

void NagareProcessor::physOn (int phys, float vel, std::array<int, 128>& map)
{
    phys = juce::jlimit (0, 127, phys);
    const auto p = readParams();
    if (map[(size_t) phys] >= 0) physOff (phys, map);
    const int played = juce::jlimit (0, 127, phys + transpose.load());
    map[(size_t) phys] = played;
    playedRefs[(size_t) played]++;
    setHeldBit (phys, true);
    synth.noteOn (played, vel, p);
}

void NagareProcessor::physOff (int phys, std::array<int, 128>& map)
{
    phys = juce::jlimit (0, 127, phys);
    const int played = map[(size_t) phys];
    if (played < 0) return;
    map[(size_t) phys] = -1;
    if (hostMap[(size_t) phys] < 0 && uiMap[(size_t) phys] < 0) setHeldBit (phys, false);
    if (--playedRefs[(size_t) played] <= 0) { playedRefs[(size_t) played] = 0; synth.noteOff (played, readParams()); }
}

void NagareProcessor::pushEvent (EventType type, int note, float velocity)
{
    const auto scope = fifo.write (1);
    if (scope.blockSize1 > 0) events[(size_t) scope.startIndex1] = { type, note, velocity };
    else if (scope.blockSize2 > 0) events[(size_t) scope.startIndex2] = { type, note, velocity };
}

void NagareProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n == 0) return;
    if ((int) monoBuf.size() < n) monoBuf.resize ((size_t) n);
    std::fill (monoBuf.begin(), monoBuf.begin() + n, 0.0f);

    synth.pullSnapshot();
    auto p = readParams();

    // changing the sound type or voice mode starts clean
    const int src = (int) p.source, mode = (int) p.mode, voice = p.mono ? 1 : 0, keep = p.keepLength ? 1 : 0;
    if (src != lastSource || mode != lastMode || voice != lastVoice || keep != lastKeep)
    {
        if (lastSource >= 0) synth.allNotesOff (true);
        playedRefs.fill (0); hostMap.fill (-1); uiMap.fill (-1);
        for (auto& b : heldBits) b.store (0);
        lastSource = src; lastMode = mode; lastVoice = voice; lastKeep = keep;
    }
    const bool hold = param ("hold") > 0.5f;
    if (hold != lastHold) lastHold = hold;
    auto applyPedal = [&] { synth.setPedal (uiPedal || midiPedal || hold, p); };
    applyPedal();

    // the plugin's own keyboard
    {
        const auto scope = fifo.read (fifo.getNumReady());
        auto handle = [&] (const Event& e) {
            switch (e.type)
            {
                case EventType::on: physOn (e.note, e.velocity, uiMap); break;
                case EventType::off: physOff (e.note, uiMap); break;
                case EventType::pedalDown: uiPedal = true; applyPedal(); break;
                case EventType::pedalUp: uiPedal = false; applyPedal(); break;
                case EventType::allOff:
                    synth.allNotesOff (true); playedRefs.fill (0); hostMap.fill (-1); uiMap.fill (-1);
                    for (auto& b : heldBits) b.store (0);
                    break;
            }
        };
        for (int i = 0; i < scope.blockSize1; ++i) handle (events[(size_t) (scope.startIndex1 + i)]);
        for (int i = 0; i < scope.blockSize2; ++i) handle (events[(size_t) (scope.startIndex2 + i)]);
    }

    // host MIDI, sample-accurate
    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, n, meta.samplePosition);
        if (at > pos) { synth.render (monoBuf.data(), pos, at - pos, p); pos = at; }
        const auto m = meta.getMessage();
        if (m.isNoteOn()) physOn (m.getNoteNumber(), juce::jlimit (0.05f, 1.0f, std::pow (m.getFloatVelocity(), 0.8f)), hostMap);
        else if (m.isNoteOff()) physOff (m.getNoteNumber(), hostMap);
        else if (m.isSustainPedalOn()) { midiPedal = true; applyPedal(); }
        else if (m.isSustainPedalOff()) { midiPedal = false; applyPedal(); }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            synth.allNotesOff (true); playedRefs.fill (0); hostMap.fill (-1); uiMap.fill (-1);
            for (auto& b : heldBits) b.store (0);
        }
    }
    if (pos < n) synth.render (monoBuf.data(), pos, n - pos, p);

    fx.process (monoBuf.data(), buffer.getWritePointer (0), buffer.getWritePointer (1), n,
                param ("cutoff"), param ("reso"), param ("delay"), param ("reverb"), param ("volume"));
}

//==============================================================================
void NagareProcessor::timerCallback()
{
    synth.collectGarbage();
    const float s = param ("loopStart"), e = param ("loopEnd");
    if (source != nullptr && (s != builtStart || e != builtEnd)) rebuildSnapshot();
}

void NagareProcessor::rebuildSnapshot()
{
    if (source == nullptr) return;
    builtStart = param ("loopStart");
    builtEnd = param ("loopEnd");
    float a = builtStart, b = builtEnd;
    if (b < a + 0.0015f) b = juce::jmin (1.0f, a + 0.0015f);
    snapshot = buildSnapshot (*source, a, b);
    synth.setSnapshot (snapshot);
}

void NagareProcessor::setSource (SourcePtr s, bool freshLoop)
{
    if (s == nullptr) return;
    source = std::move (s);
    sampleSeconds.store (source->seconds());
    if (freshLoop)
    {
        float a, b;
        defaultLoopFor (*source, a, b);
        setParam ("loopStart", a);
        setParam ("loopEnd", b);
    }
    if (! source->pitched) setParam ("mode", 0.0f);
    rebuildSnapshot();
    sourceGeneration.fetch_add (1);
}

void NagareProcessor::setParam (const juce::String& id, float value)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        prm->endChangeGesture();
    }
}

bool NagareProcessor::loadSampleFile (const juce::File& f, juce::String& error)
{
    if (! f.existsAsFile()) { error = jp ("ファイルが見つかりません。"); return false; }
    if (f.getSize() > 60 * 1024 * 1024) { error = jp ("ファイルが大きすぎます(60MBまで)。"); return false; }
    juce::MemoryBlock mb;
    if (! f.loadFileAsData (mb)) { error = jp ("ファイルを読めませんでした。"); return false; }
    auto s = makeSourceFromBytes (mb, f.getFileNameWithoutExtension(), error);
    if (s == nullptr) return false;
    pushEvent (EventType::allOff);
    setParam ("source", 0.0f);
    setSource (s, true);
    return true;
}

void NagareProcessor::applyBuiltin (int index)
{
    const auto& b = builtinPreset (index);
    pushEvent (EventType::allOff);
    setParam ("source", (float) (int) b.source);
    setParam ("mode", (float) (int) b.mode);
    setParam ("voice", b.mono ? 1.0f : 0.0f);
    setParam ("keepLen", 1.0f);
    setParam ("pitch", b.pitch);
    setParam ("attack", b.attack); setParam ("decay", b.decay); setParam ("sustain", b.sustain); setParam ("release", b.release);
    setParam ("volume", b.volume); setParam ("cutoff", b.cutoff); setParam ("reso", b.reso); setParam ("glide", b.glide);
    setParam ("delay", b.delay); setParam ("reverb", b.reverb);
    if (b.demo || source == nullptr)
    {
        setParam ("loopStart", b.loopStart);
        setParam ("loopEnd", b.loopEnd);
        setSource (makeDemoSource(), false);
    }
}

juce::ValueTree NagareProcessor::sampleToTree (bool includeData) const
{
    juce::ValueTree t ("SAMPLE");
    if (source == nullptr) return t;
    t.setProperty ("name", source->name, nullptr);
    t.setProperty ("demo", source->demo, nullptr);
    if (includeData && ! source->demo && source->fileBytes.getSize() > 0)
        t.setProperty ("data", source->fileBytes.toBase64Encoding(), nullptr);
    return t;
}

void NagareProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("SAMPLE"), nullptr);
    state.appendChild (sampleToTree (true), nullptr);
    state.setProperty ("octave", octave.load(), nullptr);
    state.setProperty ("transpose", transpose.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void NagareProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType (apvts.state.getType())) return;
    const auto smp = state.getChildWithName ("SAMPLE").createCopy();
    octave.store (juce::jlimit (-4, 3, (int) state.getProperty ("octave", 0)));
    transpose.store (juce::jlimit (-24, 24, (int) state.getProperty ("transpose", 0)));
    state.removeChild (state.getChildWithName ("SAMPLE"), nullptr);
    apvts.replaceState (state);
    pushEvent (EventType::allOff);

    SourcePtr s;
    if (smp.isValid() && smp.hasProperty ("data"))
    {
        juce::MemoryBlock mb;
        juce::String err;
        if (mb.fromBase64Encoding (smp.getProperty ("data").toString()))
            s = makeSourceFromBytes (mb, smp.getProperty ("name").toString(), err);
    }
    setSource (s != nullptr ? s : makeDemoSource(), false);
}

juce::File NagareProcessor::presetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory)
        .getChildFile ("DJ KATSUNII").getChildFile ("Nagare").getChildFile ("Presets");
}

bool NagareProcessor::savePreset (const juce::File& f)
{
    juce::ValueTree t ("NAGAREPRESET");
    for (auto* prm : getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (p->getParameterID() != "hold")
                t.setProperty (p->getParameterID(), p->convertFrom0to1 (p->getValue()), nullptr);
    t.appendChild (sampleToTree (true), nullptr);
    auto xml = t.createXml();
    return xml != nullptr && f.getParentDirectory().createDirectory() && xml->writeTo (f);
}

bool NagareProcessor::loadPreset (const juce::File& f, juce::String& error)
{
    auto xml = juce::parseXML (f);
    if (xml == nullptr || ! xml->hasTagName ("NAGAREPRESET")) { error = jp ("プリセットを読み込めませんでした。"); return false; }
    const auto t = juce::ValueTree::fromXml (*xml);
    pushEvent (EventType::allOff);
    const auto smp = t.getChildWithName ("SAMPLE");
    if (smp.isValid() && (bool) smp.getProperty ("demo", false)) setSource (makeDemoSource(), false);
    else if (smp.isValid() && smp.hasProperty ("data"))
    {
        juce::MemoryBlock mb;
        if (mb.fromBase64Encoding (smp.getProperty ("data").toString()))
            if (auto s = makeSourceFromBytes (mb, smp.getProperty ("name").toString(), error)) setSource (s, false);
    }
    for (int i = 0; i < t.getNumProperties(); ++i)
    {
        const auto id = t.getPropertyName (i).toString();
        setParam (id, (float) t.getProperty (id));
    }
    rebuildSnapshot();
    return true;
}

juce::AudioProcessorEditor* NagareProcessor::createEditor() { return new NagareEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NagareProcessor(); }
