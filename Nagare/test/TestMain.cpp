// Drives NagareProcessor like a DAW and measures the sound it makes.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>

using namespace nagare;

static int failures = 0;
#define CHECK(cond, msg) do { if (cond) std::cout << "  ok   " << msg << "\n"; else { std::cout << "  FAIL " << msg << "\n"; ++failures; } } while (0)

static constexpr double sr = 48000.0;
static constexpr int block = 512;

struct Rig
{
    NagareProcessor p;
    std::vector<float> out;
    Rig()
    {
        p.prepareToPlay (sr, block);
        p.setParam ("delay", 0.0f);
        p.setParam ("reverb", 0.0f);
    }
    void run (double seconds, std::function<void (juce::MidiBuffer&, int blockIndex)> fn = {})
    {
        juce::AudioBuffer<float> buf (2, block);
        const int blocks = (int) std::ceil (seconds * sr / block);
        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer m;
            if (fn) fn (m, b);
            p.processBlock (buf, m);
            for (int i = 0; i < block; ++i) out.push_back (buf.getSample (0, i));
        }
    }
    void note (int n, double holdSeconds, double tail = 0.0, int vel = 110)
    {
        const int offBlock = (int) (holdSeconds * sr / block);
        run (holdSeconds + tail, [&] (juce::MidiBuffer& m, int b) {
            if (b == 0) m.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) vel), 0);
            if (b == offBlock) m.addEvent (juce::MidiMessage::noteOff (1, n), 0);
        });
    }
    double rms (double from, double to) const
    {
        const size_t a = (size_t) (from * sr), b = juce::jmin (out.size(), (size_t) (to * sr));
        double s = 0; for (size_t i = a; i < b; ++i) s += out[i] * out[i];
        return std::sqrt (s / juce::jmax ((size_t) 1, b - a));
    }
    double pitchAt (double t) const
    {
        std::vector<float> seg (out.begin() + (long) (t * sr), out.begin() + (long) ((t + 0.15) * sr));
        return detectPitch (seg, sr, 0);
    }
    /** seconds until the level first drops below `frac` of its peak (20 ms windows). */
    double fallTime (double frac) const
    {
        std::vector<double> lv;
        for (double t = 0; t + 0.02 < out.size() / sr; t += 0.02) lv.push_back (rms (t, t + 0.02));
        double pk = 0; size_t pi = 0;
        for (size_t i = 0; i < lv.size(); ++i) if (lv[i] > pk) { pk = lv[i]; pi = i; }
        for (size_t i = pi; i < lv.size(); ++i) if (lv[i] < pk * frac) return i * 0.02;
        return 1e9;
    }
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    std::cout << "[1] Demo sample analysis\n";
    {
        auto s = makeDemoSource();
        CHECK (s && s->pitched && std::abs (s->f0 - 220.0) < 0.5, "demo pluck detected as " + juce::String (s->f0, 2) + " Hz (" + noteName (s->rootMidi) + ")");
        auto snap = buildSnapshot (*s, 0.28f, 0.34f);
        const double periods = snap->loopLen / snap->period;
        CHECK (std::abs (periods - std::round (periods)) < 0.02, "loop is a whole number of periods (" + juce::String (periods, 3) + ")");
        CHECK (snap->hasWave, "waveform for 波形化 built");
    }

    std::cout << "[2] Holding a note: it keeps sounding (no decay)\n";
    {
        Rig r; r.note (57, 4.0, 0.5);
        const double a = r.rms (1.0, 1.2), b = r.rms (3.6, 3.8);
        CHECK (a > 0.01 && std::abs (b / a - 1.0) < 0.08, "level at 1.0 s " + juce::String (a, 4) + " vs 3.6 s " + juce::String (b, 4));
        CHECK (r.rms (4.45, 4.5) < a * 0.2, "after release it fades out (" + juce::String (r.rms (4.45, 4.5), 4) + ")");
        CHECK (std::abs (r.pitchAt (2.0) - 220.0) < 1.5, "pitch at root " + juce::String (r.pitchAt (2.0), 1) + " Hz");
    }

    std::cout << "[3] High notes keep the same length (長さを保つ ON)\n";
    {
        Rig base; base.note (57, 3.0); const double t0 = base.fallTime (0.5);
        Rig hi;   hi.note (81, 3.0);   const double t2 = hi.fallTime (0.5);
        CHECK (std::abs (t2 - t0) < 0.06, "fall to 50%: root " + juce::String (t0, 2) + " s, +24 st " + juce::String (t2, 2) + " s");
        CHECK (std::abs (hi.pitchAt (2.0) - 880.0) < 6.0, "+24 st pitch " + juce::String (hi.pitchAt (2.0), 1) + " Hz (want 880)");
        Rig oct; oct.note (69, 3.0);
        CHECK (std::abs (oct.pitchAt (2.0) - 440.0) < 3.0, "+12 st pitch " + juce::String (oct.pitchAt (2.0), 1) + " Hz (want 440)");
        const double lv0 = base.rms (2.0, 2.4), lv2 = hi.rms (2.0, 2.4);
        CHECK (lv2 / lv0 > 0.75 && lv2 / lv0 < 1.33, "sustain level root " + juce::String (lv0, 4) + " vs +24 " + juce::String (lv2, 4));

        Rig old; old.p.setParam ("keepLen", 0.0f); old.note (81, 3.0); const double tOld = old.fallTime (0.5);
        CHECK (tOld < t0 * 0.6, "with the switch OFF it shortens like before (" + juce::String (tOld, 2) + " s)");
    }

    std::cout << "[4] 波形化 mode\n";
    {
        Rig r; r.p.setParam ("mode", 1.0f); r.note (69, 3.0);
        const double a = r.rms (1.0, 1.2), b = r.rms (2.6, 2.8);
        CHECK (std::abs (r.pitchAt (2.0) - 440.0) < 3.0 && std::abs (b / a - 1.0) < 0.05, "pitch " + juce::String (r.pitchAt (2.0), 1) + " Hz, level steady " + juce::String (a, 4) + " / " + juce::String (b, 4));
    }

    std::cout << "[5] Built-in waves, transpose, mono glide, pedal\n";
    {
        Rig r; r.p.setParam ("source", 3.0f); r.p.setParam ("cutoff", 20000.0f); r.note (69, 1.0);
        CHECK (std::abs (r.pitchAt (0.5) - 440.0) < 3.0, "saw A4 = " + juce::String (r.pitchAt (0.5), 1) + " Hz");
        bool finite = true; for (auto v : r.out) finite = finite && std::isfinite (v);
        CHECK (finite, "no NaN / inf in output");

        Rig t; t.p.setParam ("source", 1.0f); t.p.transpose.store (2); t.note (60, 1.0);
        CHECK (std::abs (t.pitchAt (0.5) - 293.66) < 2.5, "transpose +2: MIDI C5 plays " + juce::String (t.pitchAt (0.5), 1) + " Hz (D, 293.7)");

        Rig m; m.p.setParam ("source", 1.0f); m.p.setParam ("voice", 1.0f); m.p.setParam ("glide", 80.0f);
        m.run (1.2, [] (juce::MidiBuffer& mb, int b) {
            if (b == 0) mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            if (b == 40) mb.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
        });
        CHECK (std::abs (m.pitchAt (0.2) - 261.6) < 2.5 && std::abs (m.pitchAt (0.9) - 329.6) < 2.5,
               "mono legato glides 261.6 → " + juce::String (m.pitchAt (0.9), 1) + " Hz without retriggering");

        Rig pd; pd.p.setParam ("source", 1.0f); pd.p.setParam ("release", 100.0f);
        pd.run (2.5, [] (juce::MidiBuffer& mb, int b) {
            if (b == 0) { mb.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0); mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 1); }
            if (b == 10) mb.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            if (b == 150) mb.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
        });
        CHECK (pd.rms (1.0, 1.2) > 0.02 && pd.rms (2.2, 2.4) < 0.001, "sustain pedal holds after key-up (" + juce::String (pd.rms (1.0, 1.2), 3) + "), releases when lifted");
    }

    std::cout << "[6] Loading a WAV, saving and restoring the FL project\n";
    {
        Rig r;
        auto wav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("nagare_test_pluck.wav");
        {
            juce::AudioBuffer<float> b (1, 48000);
            for (int i = 0; i < 48000; ++i) b.setSample (0, i, (float) (std::exp (-i / 9000.0) * (std::sin (i * 2 * juce::MathConstants<double>::pi * 330.0 / 48000.0) + 0.3 * std::sin (i * 2 * juce::MathConstants<double>::pi * 660.0 / 48000.0))));
            wav.deleteFile();
            juce::WavAudioFormat fmt;
            std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (wav), 48000.0, 1, 24, {}, 0));
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        }
        juce::String err;
        CHECK (r.p.loadSampleFile (wav, err), "WAV loaded " + err);
        auto s = r.p.getSource();
        CHECK (s && std::abs (s->f0 - 330.0) < 1.0, "pitch detected " + juce::String (s ? s->f0 : 0, 1) + " Hz (" + noteName (s ? s->rootMidi : 0) + ")");
        r.p.setParam ("loopStart", 0.3f); r.p.setParam ("loopEnd", 0.45f); r.p.setParam ("attack", 0.0f); r.p.octave.store (1); r.p.transpose.store (-3);
        juce::MemoryBlock state; r.p.getStateInformation (state);
        NagareProcessor q; q.prepareToPlay (sr, block); q.setStateInformation (state.getData(), (int) state.getSize());
        auto qs = q.getSource();
        CHECK (qs && qs->name == "nagare_test_pluck" && std::abs (qs->f0 - 330.0) < 1.0 && std::abs (q.param ("loopStart") - 0.3f) < 1e-4
               && q.param ("attack") == 0.0f && q.octave.load() == 1 && q.transpose.load() == -3,
               "project restore: sample, loop points, attack 0, octave, transpose (" + juce::String ((int) state.getSize()) + " bytes)");
        auto pf = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("nagare_test.nagare");
        CHECK (r.p.savePreset (pf), "preset saved");
        NagareProcessor q2; q2.prepareToPlay (sr, block);
        CHECK (q2.loadPreset (pf, err) && q2.getSource() && q2.getSource()->name == "nagare_test_pluck" && std::abs (q2.param ("loopEnd") - 0.45f) < 1e-4, "preset loads with its sample");
    }

    std::cout << "[7] Editor\n";
    {
        Rig r;
        r.run (0.3, [] (juce::MidiBuffer& m, int b) { if (b == 0) m.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0); });
        std::unique_ptr<juce::AudioProcessorEditor> ed (r.p.createEditor());
        ed->setSize (1100, 1044);
        ed->setVisible (true);
        for (int i = 0; i < 6; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        auto out = juce::File (argc > 1 ? argv[1] : "/tmp/nagare_editor.png");
        juce::PNGImageFormat png;
        auto shot = [&] (const juce::File& f) { auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f); f.deleteFile(); juce::FileOutputStream os (f); return png.writeImageToStream (img, os); };
        CHECK (shot (out), "screenshot " + out.getFileName());
        r.p.setParam ("source", 5.0f);
        ed->setSize (902, 856);
        for (int i = 0; i < 4; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        shot (out.getSiblingFile ("nagare_editor_small.png"));
        ed.reset();
    }

    std::cout << "[8] Load the built VST3 like a host\n";
    if (argc > 2)
    {
        juce::VST3PluginFormat vst3;
        juce::OwnedArray<juce::PluginDescription> types;
        vst3.findAllTypesForFile (types, argv[2]);
        CHECK (types.size() == 1 && types[0]->isInstrument, "VST3 scanned as instrument: " + (types.isEmpty() ? juce::String() : types[0]->name + " / " + types[0]->manufacturerName));
        if (! types.isEmpty())
        {
            juce::String err;
            auto inst = vst3.createInstanceFromDescription (*types[0], sr, block, err);
            CHECK (inst != nullptr, "instance created " + err);
            if (inst)
            {
                inst->prepareToPlay (sr, block);
                juce::AudioBuffer<float> buf (2, block);
                juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 110), 0);
                float peak = 0;
                for (int b = 0; b < 200; ++b) { inst->processBlock (buf, m); m.clear(); if (b > 150) peak = juce::jmax (peak, buf.getMagnitude (0, block)); }
                CHECK (peak > 0.02f, "held note still sounding after 2 s through the VST3 (peak " + juce::String (peak, 3) + ")");
                std::unique_ptr<juce::AudioProcessorEditor> ed (inst->createEditorIfNeeded());
                CHECK (ed != nullptr, "VST3 editor opens");
                ed.reset();
            }
        }
    }

    std::cout << (failures == 0 ? "\nALL CHECKS PASSED\n" : "\nFAILURES: " + std::to_string (failures) + "\n");
    return failures == 0 ? 0 : 1;
}
