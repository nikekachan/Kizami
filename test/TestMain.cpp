// Verification harness: drives KizamiProcessor like a DAW would and checks the results.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/Exporter.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>

using namespace kizami;

static int failures = 0;
#define CHECK(cond, msg) do { if (cond) std::cout << "  ok   " << msg << "\n"; else { std::cout << "  FAIL " << msg << "\n"; ++failures; } } while (0)

struct FakeHead : juce::AudioPlayHead
{
    double bpm = 140.0, ppq = 0.0;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        p.setIsPlaying (playing);
        return p;
    }
};

static constexpr double sr = 48000.0;
static constexpr int block = 512;

/** Runs `seconds` of audio, returns the left channel. */
static std::vector<float> run (KizamiProcessor& p, FakeHead& head, double seconds,
                               std::function<void (juce::MidiBuffer&, double ppqStart)> midiFn = {})
{
    std::vector<float> out;
    juce::AudioBuffer<float> buf (2, block);
    const int blocks = (int) std::ceil (seconds * sr / block);
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (midiFn) midiFn (midi, head.ppq);
        p.processBlock (buf, midi);
        for (int i = 0; i < block; ++i) out.push_back (buf.getSample (0, i));
        if (head.playing) head.ppq += block * head.bpm / 60.0 / sr;
    }
    return out;
}

static std::vector<int> onsets (const std::vector<float>& x)
{
    std::vector<int> r;
    int quiet = 1 << 30;
    for (int i = 0; i < (int) x.size(); ++i)
    {
        if (std::abs (x[(size_t) i]) > 0.02f) { if (quiet > 2000) r.push_back (i); quiet = 0; }
        else ++quiet;
    }
    return r;
}

static void soloOnly (Engine& e, Sound s)
{
    for (auto& t : e.getTracks()) { t->solo = (t->sound == s); t->mute = false; }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double samplesPerStep = 0.25 * 60.0 / 140.0 * sr;

    std::cout << "[1] Sequencer timing (host at 140 BPM)\n";
    {
        KizamiProcessor p; FakeHead head; p.setPlayHead (&head);
        p.prepareToPlay (sr, block);
        soloOnly (p.engine, Sound::kick);
        auto x = run (p, head, (16 * samplesPerStep - block) / sr);
        auto o = onsets (x);
        std::vector<int> expect = { 0, 6, 10 };
        bool ok = o.size() >= 3;
        const int lat = ok ? o[0] : 0; // detector latency (envelope attack), same for every hit
        for (size_t i = 0; ok && i < 3; ++i) ok = std::abs (o[i] - lat - (int) std::round (expect[i] * samplesPerStep)) <= 2;
        ok = ok && lat < 64;
        std::cout << "       kick onsets:"; for (auto v : o) std::cout << " " << v; std::cout << "\n";
        CHECK (ok, "kick lands exactly on steps 1, 7 and 11 (within 2 samples)");
    }

    std::cout << "[2] Hit counts incl. hat rolls, one bar\n";
    {
        KizamiProcessor p; FakeHead head; p.setPlayHead (&head);
        p.prepareToPlay (sr, block);
        run (p, head, (16 * samplesPerStep - block) / sr);
        int counts[4] = {};
        auto ts = p.engine.getTracks();
        for (int i = 0; i < 4; ++i) counts[i] = ts[(size_t) i]->hitCounter.load();
        CHECK (counts[0] == 3 && counts[1] == 1 && counts[2] == 1 && counts[3] == 13,
               "kick 3 / snare 1 / clap 1 / hat 13 hits (got " + juce::String (counts[0]) + "/" + juce::String (counts[1]) + "/"
               + juce::String (counts[2]) + "/" + juce::String (counts[3]) + ")");
    }

    std::cout << "[3] FL loop jump back to bar start retriggers step 1\n";
    {
        KizamiProcessor p; FakeHead head; p.setPlayHead (&head);
        p.prepareToPlay (sr, block);
        soloOnly (p.engine, Sound::kick);
        head.ppq = 3.9;
        run (p, head, 0.04);
        const int before = p.engine.getTracks()[0]->hitCounter.load();
        head.ppq = 0.0; // loop
        run (p, head, 0.02);
        CHECK (p.engine.getTracks()[0]->hitCounter.load() == before + 1, "kick fires again after the loop jump");
    }

    std::cout << "[4] Swing delays odd steps only\n";
    {
        KizamiProcessor p; FakeHead head; p.setPlayHead (&head);
        p.prepareToPlay (sr, block);
        p.engine.swing = 1.0f;
        auto snare = p.engine.getTracks()[1];
        for (int i = 0; i < kMaxSteps; ++i) snare->vel[(size_t) i] = 0.0f;
        snare->vel[3] = 1.0f; snare->vel[8] = 1.0f;
        soloOnly (p.engine, Sound::snare);
        auto o = onsets (run (p, head, (16 * samplesPerStep - block) / sr));
        std::cout << "       snare onsets:"; for (auto v : o) std::cout << " " << v; std::cout << "\n";
        const int lat = 0; juce::ignoreUnused (lat);
        const int exp3 = (int) std::round ((3 + 0.33) * samplesPerStep), exp8 = (int) std::round (8 * samplesPerStep);
        CHECK (o.size() == 2 && std::abs ((o[1] - o[0]) - (exp8 - exp3)) <= 2 && o[0] >= exp3 && o[0] - exp3 < 64, "step 4 swung by 33%, step 9 on the grid");
    }

    std::cout << "[5] Recording from MIDI while FL plays\n";
    {
        KizamiProcessor p; FakeHead head; p.setPlayHead (&head);
        p.prepareToPlay (sr, block);
        p.engine.recording = true;
        auto snare = p.engine.getTracks()[1];
        bool sent = false;
        // play a snare (note 38) a little before step 4 (ppq 0.75)
        run (p, head, 0.8, [&] (juce::MidiBuffer& m, double ppqStart) {
            const double target = 0.72;
            const double ppqEnd = ppqStart + block * head.bpm / 60.0 / sr;
            if (! sent && target >= ppqStart && target < ppqEnd)
            {
                m.addEvent (juce::MidiMessage::noteOn (10, 38, (juce::uint8) 110), (int) ((target - ppqStart) / (head.bpm / 60.0 / sr)));
                sent = true;
            }
        });
        CHECK (snare->vel[3].load() > 0.8f, "snare written to step 4 (velocity " + juce::String (snare->vel[3].load(), 2) + ")");
        const int hits = snare->hitCounter.load();
        CHECK (hits == 1, "live hit not doubled by the sequencer (" + juce::String (hits) + " trigger)");
        run (p, head, 16 * samplesPerStep / sr); // next loop: plays from the pattern
        CHECK (snare->hitCounter.load() >= 3, "recorded step plays back on the next loop");
    }

    std::cout << "[6] Preview play when FL is stopped, and host takes over\n";
    {
        KizamiProcessor p; FakeHead head; head.playing = false; p.setPlayHead (&head);
        p.prepareToPlay (sr, block);
        run (p, head, 0.2);
        CHECK (p.engine.currentStep.load() == -1, "silent while nothing plays");
        p.engine.internalPlay = true;
        run (p, head, 0.5);
        CHECK (p.engine.currentStep.load() > 0 && p.engine.getTracks()[0]->hitCounter.load() >= 1, "internal preview runs the pattern");
        head.playing = true; head.ppq = 0;
        run (p, head, 0.05);
        CHECK (! p.engine.internalPlay.load(), "preview stops when FL starts playing");
    }

    std::cout << "[7] Bars, state save/restore (FL project)\n";
    {
        KizamiProcessor p; p.prepareToPlay (sr, block);
        p.engine.setBars (2, true);
        auto kick = p.engine.getTracks()[0];
        CHECK (p.engine.getTotalSteps() == 32 && kick->vel[16 + 6].load() > 0.0f, "+copy duplicates bar 1 into bar 2");
        p.engine.setBars (3, false);
        CHECK (kick->vel[32].load() == 0.0f, "+empty adds an empty bar");
        p.engine.swing = 0.4f;
        kick->note = 48; kick->pitch = 3.0f;

        // a user sample
        auto wavFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("kizami_test_tone.wav");
        juce::AudioBuffer<float> tone (1, 4800);
        for (int i = 0; i < 4800; ++i) tone.setSample (0, i, 0.5f * std::sin (i * 0.05f));
        writeWavFile (tone, 48000.0, wavFile);
        auto st = p.engine.addTrack (Sound::sample);
        juce::String err;
        CHECK (p.engine.loadSampleFile (st->uid, wavFile, err) && st->rendered->getNumSamples() > 4000, "WAV sample loads into a new track");

        juce::MemoryBlock state;
        p.getStateInformation (state);
        KizamiProcessor q; q.prepareToPlay (sr, block);
        q.setStateInformation (state.getData(), (int) state.getSize());
        const auto a = p.engine.toValueTree().toXmlString(), b = q.engine.toValueTree().toXmlString();
        CHECK (a == b, "restored project is identical (" + juce::String (state.getSize()) + " bytes, sample embedded)");
        CHECK (q.engine.getTracks().back()->rendered->getNumSamples() > 4000, "embedded sample plays after reload");
    }

    std::cout << "[8] Export: WAV mix, stems, MIDI\n";
    {
        KizamiProcessor p; p.prepareToPlay (sr, block);
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("kizami_export_test");
        dir.deleteRecursively();
        juce::String err;
        const int n = exportAll (p.engine, 2, 140.0, dir, err);
        CHECK (n == 6, "6 files written (mix + 4 stems + MIDI), got " + juce::String (n));
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (dir.getChildFile ("Kizami_140bpm_mix.wav")));
        const auto expectLen = (juce::int64) std::round (2 * 16 * samplesPerStep);
        CHECK (r != nullptr && std::abs (r->lengthInSamples - expectLen) <= 2, "mix is exactly 2 loops long (" + juce::String (r ? r->lengthInSamples : 0) + " samples)");
        juce::MidiFile mf;
        juce::FileInputStream in (dir.getChildFile ("Kizami_140bpm.mid"));
        const bool readOk = mf.readFrom (in);
        int notes = 0;
        for (int t = 0; t < mf.getNumTracks(); ++t)
            for (auto* ev : *mf.getTrack (t)) notes += ev->message.isNoteOn() ? 1 : 0;
        CHECK (readOk && mf.getNumTracks() == 5 && notes == 2 * (3 + 1 + 1 + 13), "MIDI has tempo + 4 drum tracks, 36 notes (got " + juce::String (notes) + ")");
        std::cout << "       files:"; for (auto& f : dir.findChildFiles (juce::File::findFiles, false)) std::cout << " " << f.getFileName(); std::cout << "\n";
    }

    std::cout << "[9] Editor opens and draws\n";
    {
        KizamiProcessor p; FakeHead head; p.setPlayHead (&head);
        p.prepareToPlay (sr, block);
        p.engine.setBars (2, true);
        p.engine.recording = true;
        run (p, head, 0.4);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (1160, 760);
        ed->setVisible (true);
        for (int i = 0; i < 5; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto out = juce::File (argc > 1 ? argv[1] : "/tmp/kizami_editor.png");
        out.deleteFile();
        juce::FileOutputStream os (out);
        juce::PNGImageFormat png;
        CHECK (png.writeImageToStream (img, os), "screenshot written to " + out.getFullPathName());
        ed->setSize (1080, 600);
        for (int i = 0; i < 3; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
        auto img2 = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        out.getSiblingFile ("kizami_editor_small.png").deleteFile();
        juce::FileOutputStream os2 (out.getSiblingFile ("kizami_editor_small.png"));
        png.writeImageToStream (img2, os2);
        {
            kizami::ui::TrackDetail d (*static_cast<KizamiEditor*> (ed.get()), p.engine.getTracks()[0]);
            auto im = d.createComponentSnapshot (d.getLocalBounds(), true, 1.0f);
            auto f = out.getSiblingFile ("kizami_detail.png"); f.deleteFile();
            juce::FileOutputStream os3 (f); png.writeImageToStream (im, os3);
        }
        ed.reset();
    }

    std::cout << "[10] Load the built VST3 like a host\n";
    if (argc > 2)
    {
        juce::VST3PluginFormat vst3;
        juce::OwnedArray<juce::PluginDescription> types;
        vst3.findAllTypesForFile (types, argv[2]);
        CHECK (types.size() == 1 && types[0]->isInstrument, "VST3 scanned as an instrument: " + (types.isEmpty() ? juce::String() : types[0]->name + " / " + types[0]->category));
        if (! types.isEmpty())
        {
            juce::String err;
            auto inst = vst3.createInstanceFromDescription (*types[0], sr, block, err);
            CHECK (inst != nullptr, "VST3 instance created " + err);
            if (inst)
            {
                inst->prepareToPlay (sr, block);
                juce::AudioBuffer<float> buf (2, block);
                juce::MidiBuffer m;
                m.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 127), 0);
                float peak = 0;
                for (int b = 0; b < 20; ++b) { inst->processBlock (buf, m); m.clear(); peak = juce::jmax (peak, buf.getMagnitude (0, block)); }
                CHECK (peak > 0.1f, "MIDI C3 (note 36) plays the kick through the VST3 (peak " + juce::String (peak, 2) + ")");
                std::unique_ptr<juce::AudioProcessorEditor> ed (inst->createEditorIfNeeded());
                CHECK (ed != nullptr, "VST3 editor opens");
                ed.reset();
                inst->releaseResources();
            }
        }
    }

    std::cout << (failures == 0 ? "\nALL CHECKS PASSED\n" : "\nFAILURES: " + std::to_string (failures) + "\n");
    return failures == 0 ? 0 : 1;
}
