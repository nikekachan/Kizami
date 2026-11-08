// Drives KanadeProcessor like FL Studio: transfer the vocal while playing, stop, edit, play back.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>

using namespace kanade;
static int failures = 0;
#define CHECK(cond, msg) do { if (cond) std::cout << "  ok   " << msg << "\n"; else { std::cout << "  FAIL " << msg << "\n"; ++failures; } } while (0)
static constexpr double sr = 44100.0;
static constexpr int block = 512;

struct Head : juce::AudioPlayHead
{
    bool playing = true; juce::int64 pos = 0; double bpm = 100;
    juce::Optional<PositionInfo> getPosition() const override
    { PositionInfo p; p.setIsPlaying (playing); p.setBpm (bpm); p.setTimeInSamples (pos); p.setTimeInSeconds ((double) pos / sr); p.setPpqPosition ((double) pos / sr * bpm / 60); return p; }
};

static void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }
static bool waitFor (std::function<bool()> f, int ms) { for (int t = 0; t < ms; t += 20) { if (f()) return true; pump (20); } return f(); }

// play `in` (placed at song position `offset`) through the processor; returns the output
static std::vector<float> runThrough (KanadeProcessor& p, Head& h, const std::vector<float>& in, juce::int64 offset, juce::int64 total)
{
    std::vector<float> out;
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer m;
    for (h.pos = 0; h.pos < total; h.pos += block)
    {
        for (int i = 0; i < block; ++i) { const juce::int64 k = h.pos + i - offset; const float x = (k >= 0 && k < (juce::int64) in.size()) ? in[(size_t) k] : 0.0f; buf.setSample (0, i, x); buf.setSample (1, i, x); }
        p.processBlock (buf, m);
        for (int i = 0; i < block; ++i) out.push_back (buf.getSample (0, i));
    }
    return out;
}

static std::vector<float> centsAfter (const std::vector<float>& y)
{
    auto A = analyze (y.data(), (int) y.size(), sr);
    int id = 1; auto ns = segment (A, id);
    std::vector<float> c; for (auto& n : ns) c.push_back ((n.center - std::round (n.center)) * 100);
    return c;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String outDir = argc > 1 ? argv[1] : "/tmp";
    std::vector<float> demo;
    {
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile ("Kanade/test/demo44k.f32"));
        if (! f.existsAsFile()) f = juce::File (juce::String (argc > 3 ? argv[3] : ""));
        juce::MemoryBlock mb; f.loadFileAsData (mb);
        demo.assign ((const float*) mb.getData(), (const float*) mb.getData() + mb.getSize() / 4);
    }
    CHECK (demo.size() > 100000, "demo vocal loaded (" + juce::String (demo.size() / sr, 1) + " s)");
    const juce::int64 offset = (juce::int64) (sr * 2.0);   // the vocal starts at 2.0 s in the song
    const juce::int64 total = offset + (juce::int64) demo.size() + (juce::int64) sr;

    std::cout << "[1] Transfer from the host\n";
    KanadeProcessor p; Head h; p.setPlayHead (&h); p.prepareToPlay (sr, block);
    p.armRecord (true);
    auto pass = runThrough (p, h, demo, offset, total);
    h.playing = false; { juce::AudioBuffer<float> b (2, block); b.clear(); juce::MidiBuffer m; p.processBlock (b, m); }
    double maxd = 0; for (juce::int64 i = offset; i < offset + (juce::int64) demo.size(); ++i) maxd = std::max (maxd, (double) std::abs (pass[(size_t) i] - demo[(size_t) (i - offset)]));
    CHECK (maxd < 1e-6, "while recording, the vocal passes through untouched");
    CHECK (waitFor ([&] { return p.session != nullptr && ! p.analysing.load(); }, 20000), "stopping the host starts the analysis, which finishes");
    CHECK (p.session && p.session->startSec() <= 2.0 && p.session->endSec() >= 2.0 + demo.size() / sr, "the transferred vocal keeps its song position (from " + juce::String (p.session ? p.session->startSec() : -1, 3) + " s)");
    CHECK (p.notes.size() == 17, "17 notes found (" + juce::String ((int) p.notes.size()) + ")");
    const auto k = detectKey (p.notes);
    CHECK (k.first == 0 && k.second == major, "key detected: C major");

    std::cout << "[2] Correct and play back in place\n";
    h.playing = true;
    auto before = runThrough (p, h, demo, offset, total);
    double d0 = 0; for (size_t i = 0; i < before.size(); ++i) { const juce::int64 kk = (juce::int64) i - offset; const float x = (kk >= 0 && kk < (juce::int64) demo.size()) ? demo[(size_t) kk] : 0.0f; d0 = std::max (d0, (double) std::abs (before[i] - x)); }
    CHECK (d0 < 1e-5, "before any edit the output equals the vocal");
    p.key = 0; p.scale = major;
    for (auto& n : p.notes) n.pitch = snapToScale (n.center, 0, major);
    p.notesChanged ({}, true);
    pump (100);
    CHECK (waitFor ([&] { return ! p.rendering.load(); }, 10000), "background render finished");
    pump (200);
    auto after = runThrough (p, h, demo, offset, total);
    std::vector<float> seg (after.begin() + offset, after.begin() + offset + (long) demo.size());
    auto c = centsAfter (seg);
    float worst = 0; for (float v : c) worst = std::max (worst, std::abs (v));
    CHECK (c.size() == 17 && worst < 4, "after correction every note is in tune (worst " + juce::String (worst, 1) + " cents)");
    p.bypass = true;
    auto byp = runThrough (p, h, demo, offset, total);
    double db = 0; for (juce::int64 i = offset; i < offset + (juce::int64) demo.size(); ++i) db = std::max (db, (double) std::abs (byp[(size_t) i] - demo[(size_t) (i - offset)]));
    CHECK (db < 1e-5, "補正前 (A/B) plays the original");
    p.bypass = false;

    std::cout << "[3] Preview while the host is stopped\n";
    h.playing = false; p.preview (5.0);
    { juce::AudioBuffer<float> b (2, block); juce::MidiBuffer m; float pk = 0; for (int i = 0; i < 40; ++i) { b.clear(); p.processBlock (b, m); pk = std::max (pk, b.getMagnitude (0, 0, block)); }
      CHECK (pk > 0.05f && p.previewOn.load(), "▶ plays the corrected vocal from the clicked position (peak " + juce::String (pk, 2) + ")"); }
    p.stopPreview();

    std::cout << "[4] Save in the project and restore\n";
    p.notes[3].gain = -6; p.notes[3].inMs = 120;
    juce::MemoryBlock st; p.getStateInformation (st);
    CHECK (st.getSize() > 100000 && st.getSize() < demo.size() * 4, "state holds the vocal compressed (" + juce::String ((int) st.getSize() / 1024) + " KB)");
    KanadeProcessor q; Head h2; q.setPlayHead (&h2); q.prepareToPlay (sr, block);
    q.setStateInformation (st.getData(), (int) st.getSize());
    CHECK (waitFor ([&] { return q.session != nullptr && ! q.analysing.load(); }, 20000), "restored project re-analyses");
    CHECK (q.notes.size() == 17 && std::abs (q.notes[3].gain + 6) < 1e-4 && std::abs (q.notes[3].pitch - p.notes[3].pitch) < 1e-4 && q.scale == major, "notes and edits restored");
    pump (200);
    h2.playing = true;
    auto rq = runThrough (q, h2, demo, offset, total);
    std::vector<float> seg2 (rq.begin() + offset, rq.begin() + offset + (long) demo.size());
    auto c2 = centsAfter (seg2); float w2 = 0; for (float v : c2) w2 = std::max (w2, std::abs (v));
    CHECK (w2 < 4, "restored project plays corrected (worst " + juce::String (w2, 1) + " cents)");

    std::cout << "[5] Files\n";
    auto wf = juce::File (outDir).getChildFile ("kanade_test.wav");
    CHECK (p.writeWav (wf) && wf.getSize() > 1000000, "WAV written (" + juce::String ((int) wf.getSize() / 1024) + " KB)");
    auto df = p.dragFile();
    CHECK (df.existsAsFile(), "drag file prepared: " + df.getFileName());
    {
        KanadeProcessor r; r.prepareToPlay (48000, block); juce::String err;
        CHECK (r.loadFile (wf, err), "ファイルを開く accepts a WAV " + err);
        CHECK (waitFor ([&] { return r.session != nullptr && ! r.analysing.load(); }, 20000) && r.notes.size() == 17, "loaded file (resampled to 48 kHz) gives 17 notes (" + juce::String ((int) r.notes.size()) + ")");
    }

    std::cout << "[6] Editor\n";
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (1240, 900); ed->setVisible (true);
        for (int i = 0; i < 8; ++i) pump (40);
        juce::PNGImageFormat png;
        auto shot = [&] (const juce::String& name) { auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f); auto f = juce::File (outDir).getChildFile (name); f.deleteFile(); juce::FileOutputStream os (f); return png.writeImageToStream (img, os); };
        CHECK (shot ("kanade_editor.png"), "screenshot");
        auto* e = dynamic_cast<KanadeEditor*> (ed.get());
        e->content.roll.zoomH (2.0f); e->content.roll.openAdv (p.notes[7].id);
        for (int i = 0; i < 4; ++i) pump (40);
        CHECK (shot ("kanade_editor_adv.png"), "screenshot (detailed edit)");
        e->content.logo.dancing = true; for (int i = 0; i < 6; ++i) pump (40);
        CHECK (shot ("kanade_editor_dance.png"), "screenshot (dancing)");
        CHECK (e->content.logo.dancing, "logo dances on click");
        ed.reset();
    }

    std::cout << "[7] Load the built VST3 like a host\n";
    if (argc > 2 && juce::String (argv[2]).isNotEmpty())
    {
        juce::VST3PluginFormat vst3; juce::OwnedArray<juce::PluginDescription> types;
        vst3.findAllTypesForFile (types, argv[2]);
        CHECK (types.size() == 1 && ! types[0]->isInstrument, "VST3 scanned as an effect: " + (types.isEmpty() ? juce::String() : types[0]->name + " / " + types[0]->manufacturerName + " / " + types[0]->category));
        if (! types.isEmpty())
        {
            juce::String err; auto inst = vst3.createInstanceFromDescription (*types[0], sr, block, err);
            CHECK (inst != nullptr, "instance created " + err);
            if (inst)
            {
                inst->prepareToPlay (sr, block);
                juce::AudioBuffer<float> b (2, block); juce::MidiBuffer m; float pk = 0; double ph = 0;
                for (int i = 0; i < 40; ++i) { for (int s = 0; s < block; ++s) { const float x = (float) (0.5 * std::sin (ph)); ph += 0.03; b.setSample (0, s, x); b.setSample (1, s, x); } inst->processBlock (b, m); pk = std::max (pk, b.getMagnitude (0, block)); }
                CHECK (pk > 0.2f, "audio passes through the VST3");
                std::unique_ptr<juce::AudioProcessorEditor> ed (inst->createEditorIfNeeded());
                CHECK (ed != nullptr, "VST3 editor opens");
            }
        }
    }
    std::cout << (failures == 0 ? "\nALL CHECKS PASSED\n" : "\nFAILURES: " + std::to_string (failures) + "\n");
    return failures == 0 ? 0 : 1;
}
