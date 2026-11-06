// Drives YugamiProcessor like FL Studio (transport + MIDI) and measures what comes out.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>
#include <cstdlib>

using namespace yugami;

static int failures = 0;
#define CHECK(cond, msg) do { if (cond) std::cout << "  ok   " << msg << "\n"; else { std::cout << "  FAIL " << msg << "\n"; ++failures; } } while (0)

static constexpr double sr = 48000.0;
static constexpr int block = 512;
static constexpr double bpm = 120.0;          // 1 beat = 0.5 s, 1 bar = 2 s

struct Head : juce::AudioPlayHead
{
    bool playing = true; double ppq = 0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setIsPlaying (playing); p.setBpm (bpm); p.setPpqPosition (ppq);
        return p;
    }
};

struct Rig
{
    YugamiProcessor p;
    Head head;
    std::vector<float> in, out;
    double phase = 0;
    Rig() { p.setPlayHead (&head); p.prepareToPlay (sr, block); }
    void run (double seconds, double freq, std::function<void (juce::MidiBuffer&, int)> fn = {}, bool noise = false)
    {
        juce::AudioBuffer<float> buf (2, block);
        juce::Random rnd (7);
        const int blocks = (int) std::ceil (seconds * sr / block);
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < block; ++i)
            {
                const float x = noise ? rnd.nextFloat() - 0.5f : (float) (0.5 * std::sin (phase)); phase += juce::MathConstants<double>::twoPi * freq / sr;
                buf.setSample (0, i, x); buf.setSample (1, i, x); in.push_back (x);
            }
            juce::MidiBuffer m; if (fn) fn (m, b);
            p.processBlock (buf, m);
            for (int i = 0; i < block; ++i) out.push_back (buf.getSample (0, i));
            if (head.playing) head.ppq += block / (sr * 60.0 / bpm);
        }
    }
    double rms (double a, double b) const { double s = 0; size_t i0 = (size_t) (a * sr), i1 = juce::jmin (out.size(), (size_t) (b * sr)); for (size_t i = i0; i < i1; ++i) s += out[i] * out[i]; return std::sqrt (s / juce::jmax ((size_t) 1, i1 - i0)); }
    double freq (double a, double b) const { int c = 0; size_t i0 = (size_t) (a * sr), i1 = (size_t) (b * sr); for (size_t i = i0 + 1; i < i1; ++i) if (out[i - 1] < 0 && out[i] >= 0) ++c; return c / (b - a); }
    double maxDiffToInput() const { double m = 0; for (size_t i = 0; i < out.size(); ++i) m = juce::jmax (m, (double) std::abs (out[i] - in[i])); return m; }
    double maxStep (double a, double b) const { double m = 0; for (size_t i = (size_t) (a * sr) + 1; i < (size_t) (b * sr); ++i) m = juce::jmax (m, (double) std::abs (out[i] - out[i - 1])); return m; }
};

static int findSlot (YugamiProcessor& p, bool time, const char* name)
{
    for (int i = 0; i < kSlots; ++i) if (p.slot (time, i).name == jp (name)) return i;
    return -1;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    auto mine = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("yugami_test_mine.json");
    mine.deleteFile();
    setenv ("YUGAMI_MINE_FILE", mine.getFullPathName().toRawUTF8(), 1);

    std::cout << "[1] Presets and pages\n";
    {
        const auto& f = factory();
        CHECK (f.time.size() == 72 && f.vol.size() == 72, "factory library: " + juce::String ((int) f.time.size()) + " time + " + juce::String ((int) f.vol.size()) + " volume presets");
        auto pg = factoryPages();
        CHECK (pg.size() == 3 && pg[1].name == jp ("カクカク") && pg[2].time[0].isEmpty(), "3 pages: スタンダード / カクカク / マイページ");
        int bad = 0;
        for (auto* list : { &f.time, &f.vol })
            for (auto& p : *list)
            {
                std::vector<float> t (kTab); compile (p.toSlot(), t.data());
                for (float v : t) if (! std::isfinite (v) || v < -1e-4f || v > 1.0001f) { ++bad; break; }
            }
        CHECK (bad == 0, "all 144 presets compile to valid curves");
    }

    std::cout << "[2] Time: half speed on the live input\n";
    {
        Rig r; r.p.setParam ("tSlot", 2.0f); // ハーフ
        r.run (4.0, 220.0);
        const double f = r.freq (2.3, 3.7);
        CHECK (std::abs (f - 110.0) < 3.0, "ハーフ plays the incoming 220 Hz at ~110 Hz (" + juce::String (f, 1) + ")");
    }
    {
        Rig r; r.p.setParam ("tSlot", 1.0f); // なし
        r.run (2.0, 220.0);
        CHECK (r.maxDiffToInput() < 1e-3, "なし passes the sound through untouched");
    }
    {
        Rig r; r.head.playing = false; r.p.setParam ("tSlot", 2.0f);
        r.run (1.0, 220.0);
        CHECK (r.maxDiffToInput() < 1e-3, "transport stopped: sound passes through");
    }
    {
        Rig r; r.p.setPage (0); const int s = findSlot (r.p, true, "階段1/8"); r.p.setParam ("tSlot", (float) s + 1);
        r.run (4.0, 0, {}, true);
        // stop segments: every 1/8 beat the output freezes (repeats the same sample)
        int frozen = 0; for (size_t i = (size_t) (2.3 * sr); i < (size_t) (2.3 * sr) + 12000; ++i) if (std::abs (r.out[i] - r.out[i - 1]) < 0.03) ++frozen;
        CHECK (s >= 0 && frozen > 4500, "階段1/8 freezes the sound in steps (" + juce::String (frozen) + " held samples)");
    }

    std::cout << "[3] Volume\n";
    {
        Rig r; r.p.setParam ("tSlot", 1.0f);
        const int g = findSlot (r.p, false, "ゲート1/16"); r.p.setParam ("vSlot", (float) g + 1);
        r.run (3.0, 220.0);
        const double on = r.rms (2.0 + 0.003, 2.0 + 0.06), off = r.rms (2.0 + 0.085, 2.0 + 0.12);
        CHECK (g >= 0 && on > 0.3 && off < 0.02, "ゲート1/16 opens and closes on the grid (on " + juce::String (on, 3) + ", off " + juce::String (off, 4) + ")");
    }

    std::cout << "[4] MIDI and HOLD\n";
    {
        Rig r; r.p.setParam ("tSlot", 1.0f);
        int hotDuring = -9, hotAfter = -9;
        r.run (3.0, 220.0, [&] (juce::MidiBuffer& m, int b) {
            if (b == 100) m.addEvent (juce::MidiMessage::noteOn (1, 36 + 1, (juce::uint8) 100), 10);  // time slot 2 = ハーフ
            if (b == 180) { hotDuring = r.p.hotT.load(); m.addEvent (juce::MidiMessage::noteOff (1, 36 + 1), 0); }
            if (b == 200) hotAfter = r.p.hotT.load(); });
        CHECK (hotDuring == 1 && hotAfter == -1, "MIDI note 37 holds time slot 2 only while pressed");
        Rig q; q.p.setParam ("hold", 1.0f);
        int hot = -9;
        q.run (1.0, 220.0, [&] (juce::MidiBuffer& m, int b) {
            if (b == 10) m.addEvent (juce::MidiMessage::noteOn (1, 72 + 5, (juce::uint8) 100), 0);
            if (b == 20) m.addEvent (juce::MidiMessage::noteOff (1, 72 + 5), 0);
            if (b == 60) hot = q.p.hotV.load(); });
        CHECK (hot == 5, "HOLD keeps volume slot 6 after the key is released");
        Rig k; k.p.pushTrigger (true, 3, true); k.run (0.2, 220.0);
        CHECK (k.p.hotT.load() == 3, "editor keyboard trigger reaches the audio thread");
    }

    std::cout << "[5] Jumps are smooth\n";
    {
        Rig a; a.p.setPage (0); const int s = findSlot (a.p, true, "ロール1/16"); a.p.setParam ("tSlot", (float) s + 1);
        a.p.setParam ("smooth", 4.0f);
        a.run (4.0, 220.0);
        const double st = a.maxStep (2.0, 4.0);
        CHECK (st < 0.08, "ロール1/16 with スムーズ: no clicks (max step " + juce::String (st, 3) + ")");
        // song position jump (FL loop) handled too
        Rig b; b.p.setParam ("tSlot", 2.0f);
        b.run (1.3, 220.0); b.head.ppq = 0.0; b.run (1.0, 220.0);
        CHECK (b.maxStep (1.25, 1.4) < 0.08, "loop jump in the song position: no click");
    }

    std::cout << "[6] State, set files, my presets\n";
    {
        YugamiProcessor p; p.prepareToPlay (sr, block);
        p.addPage(); p.pages[(size_t) p.page].name = "Test"; p.slot (true, 4).name = "Custom"; p.slot (true, 4).nodes = { { 0, 0, 0 }, { 0.5f, 0.25f, 0.2f }, { 1, 0, 0 } };
        p.setParam ("tSlot", 5.0f); p.editIdx = 4;
        juce::MemoryBlock st; p.getStateInformation (st);
        YugamiProcessor q; q.setStateInformation (st.getData(), (int) st.getSize());
        CHECK (q.pages.size() == 4 && q.page == 1 && q.pages[1].name == "Test" && q.slot (true, 4).name == "Custom" && q.slot (true, 4).nodes.size() == 3
               && (int) q.param ("tSlot") == 5 && q.editIdx == 4, "project restore: pages, edited slot, selection (" + juce::String ((int) st.getSize()) + " bytes)");
        juce::String nm; p.saveMine (true, p.slot (true, 4), nm);
        YugamiProcessor m2;
        CHECK (m2.mineTime.size() == 1 && m2.mineTime[0].name == "Custom", "my preset saved to disk and seen by a new instance");
        auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("yugami_test_set.json");
        CHECK (p.exportSet (f), "set exported");
        YugamiProcessor r; juce::String err;
        CHECK (r.importSet (f, err) && r.pages.size() == 4 && r.pages[1].time[4].name == "Custom", "set imported " + err);
    }

    std::cout << "[7] Editor\n";
    {
        Rig r; r.p.setParam ("tSlot", 2.0f);
        r.run (1.2, 220.0);
        std::unique_ptr<juce::AudioProcessorEditor> ed (r.p.createEditor());
        ed->setSize (1240, 880);
        ed->setVisible (true);
        for (int i = 0; i < 6; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        juce::PNGImageFormat png;
        auto shot = [&] (const juce::String& name) { auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f); auto f = juce::File (argc > 1 ? argv[1] : "/tmp").getChildFile (name); f.deleteFile(); juce::FileOutputStream os (f); return png.writeImageToStream (img, os); };
        CHECK (shot ("yugami_editor.png"), "screenshot (time)");
        auto* e = dynamic_cast<YugamiEditor*> (ed.get());
        e->proc.setPage (1); e->selectSlot (false, 3); e->content.setLibraryOpen (true); e->refreshAll();
        for (int i = 0; i < 4; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        CHECK (shot ("yugami_editor_lib.png"), "screenshot (library)");
        e->content.setLibraryOpen (false); e->selectSlot (true, 0); e->refreshAll();
        ed->setSize (930, 660);
        for (int i = 0; i < 4; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        CHECK (shot ("yugami_editor_small.png"), "screenshot (page 2, small)");
        ed.reset();
    }

    std::cout << "[8] Load the built VST3 like a host\n";
    if (argc > 2)
    {
        juce::VST3PluginFormat vst3;
        juce::OwnedArray<juce::PluginDescription> types;
        vst3.findAllTypesForFile (types, argv[2]);
        CHECK (types.size() == 1 && ! types[0]->isInstrument, "VST3 scanned as an effect: " + (types.isEmpty() ? juce::String() : types[0]->name + " / " + types[0]->manufacturerName + " / " + types[0]->category));
        if (! types.isEmpty())
        {
            juce::String err;
            auto inst = vst3.createInstanceFromDescription (*types[0], sr, block, err);
            CHECK (inst != nullptr, "instance created " + err);
            if (inst)
            {
                inst->prepareToPlay (sr, block);
                juce::AudioBuffer<float> buf (2, block);
                juce::MidiBuffer m; double ph = 0; float peak = 0;
                for (int b = 0; b < 100; ++b)
                {
                    for (int i = 0; i < block; ++i) { const float x = (float) (0.5 * std::sin (ph)); ph += 0.03; buf.setSample (0, i, x); buf.setSample (1, i, x); }
                    inst->processBlock (buf, m);
                    if (b > 50) peak = juce::jmax (peak, buf.getMagnitude (0, block));
                }
                CHECK (peak > 0.2f, "audio comes through the VST3 (peak " + juce::String (peak, 3) + ")");
                std::unique_ptr<juce::AudioProcessorEditor> ed (inst->createEditorIfNeeded());
                CHECK (ed != nullptr, "VST3 editor opens");
                ed.reset();
            }
        }
    }
    mine.deleteFile();
    std::cout << (failures == 0 ? "\nALL CHECKS PASSED\n" : "\nFAILURES: " + std::to_string (failures) + "\n");
    return failures == 0 ? 0 : 1;
}
