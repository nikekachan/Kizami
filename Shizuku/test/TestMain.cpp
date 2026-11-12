// Renders every Serum preset in test/presets through the engine and drives the plugin + editor headless.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <iostream>
#include <chrono>

using namespace shz;
static int failures = 0;
#define CHECK(cond, msg) do { if (cond) std::cout << "  ok   " << msg << "\n"; else { std::cout << "  FAIL " << msg << "\n"; ++failures; } } while (0)
static void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

struct Stats { float peak = 0; double rms = 0; int nan = 0; double cpu = 0; std::vector<float> env; };
static Stats render (const Patch& p, double sr = 44100)
{
    Engine e (sr);
    e.swapPatch (compile (p, sr), true);
    for (int i = 0; i < 3; ++i)
    {
        std::vector<Frame> fr;
        if (p.osc[(size_t) i].embedded) fr = framesFrom (*p.osc[(size_t) i].embedded, 2048);
        else fr = builtinTables().at (p.osc[(size_t) i].tablePath.containsIgnoreCase ("Basic Shapes") ? "Basic Shapes" : "Default Shapes");
        e.setTable (i, buildTable (fr));
    }
    for (int i = 0; i < 8; ++i) e.macros[(size_t) i] = p.macro[(size_t) i].val;
    const int N = (int) (sr * 3), bl = 512; std::vector<float> L (bl), R (bl);
    Stats s; double sum = 0; int count = 0; double wsum = 0; int wn = 0;
    const int notes[] = { 48, 55, 60, 64 }; const bool mono = p.glob.mono;
    auto t0 = std::chrono::steady_clock::now();
    for (int pos = 0; pos < N; pos += bl)
    {
        if (pos == 0) { if (mono) e.noteOn (60, 0.8f); else for (int n : notes) e.noteOn (n, 0.8f); }
        if (pos == (int) (sr * 1.8) / bl * bl) { if (mono) e.noteOff (60); else for (int n : notes) e.noteOff (n); }
        e.process (L.data(), R.data(), bl);
        for (int i = 0; i < bl; ++i) { const float x = L[(size_t) i]; if (! std::isfinite (x)) ++s.nan; s.peak = std::max (s.peak, std::abs (x)); sum += x * x; ++count; wsum += x * x; ++wn; if (wn == 8820) { s.env.push_back ((float) (10 * std::log10 (wsum / wn + 1e-12))); wsum = 0; wn = 0; } }
    }
    s.cpu = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / 3.0 * 100;
    s.rms = std::sqrt (sum / count);
    return s;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto dir = juce::File (__FILE__).getParentDirectory().getChildFile ("presets");
    std::cout << "== Serum presets ==\n";
    for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.serumpreset"))
    {
        juce::MemoryBlock mb; f.loadFileAsData (mb);
        try
        {
            auto p = readSerumPreset (mb);
            auto s = render (p);
            juce::String env; for (size_t i = 0; i < s.env.size(); i += 2) env << juce::String ((int) s.env[i]) << " ";
            std::cout << "  " << p.name.paddedRight (' ', 42) << " peak " << juce::String (s.peak, 2) << " rms " << juce::String (s.rms, 3) << " cpu " << juce::String (s.cpu, 1) << "%  env " << env << "\n";
            CHECK (s.nan == 0 && s.peak > 0.001f && s.peak <= 1.0f, p.name + " renders sound");
            // state round trip
            auto p2 = Patch::fromVar (juce::JSON::parse (juce::JSON::toString (p.toVar (true))));
            CHECK (p2.mod.size() == p.mod.size() && p2.fx.size() == p.fx.size() && p2.name == p.name && (bool) p2.osc[0].embedded == (bool) p.osc[0].embedded, p.name + " survives save/load");
        }
        catch (const std::exception& ex) { std::cout << "  FAIL " << f.getFileName() << ": " << ex.what() << "\n"; ++failures; }
    }
    std::cout << "== factory ==\n";
    for (auto& p : factoryPatches()) { auto s = render (p); CHECK (s.nan == 0 && s.peak > 0.01f, p.name + " (peak " + juce::String (s.peak, 2) + ")"); }

    std::cout << "== plugin ==\n";
    {
        ShizukuProcessor proc;
        proc.setPlayConfigDetails (0, 2, 48000, 512);
        proc.prepareToPlay (48000, 512);
        juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi;
        auto files = dir.findChildFiles (juce::File::findFiles, false, "*.serumpreset");
        juce::String err;
        CHECK (files.isEmpty() || proc.loadFile (files[0], err), "loadFile " + err);
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 100);
        float pk = 0;
        for (int b = 0; b < 60; ++b) { buf.clear(); proc.processBlock (buf, midi); midi.clear(); pk = std::max (pk, buf.getMagnitude (0, 512)); if (b == 40) midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0); }
        CHECK (pk > 0.001f, "plugin makes sound via MIDI (" + juce::String (pk, 3) + ")");
        juce::MemoryBlock st; proc.getStateInformation (st);
        ShizukuProcessor proc2; proc2.setStateInformation (st.getData(), (int) st.getSize());
        CHECK (proc2.patch.name == proc.patch.name && proc2.patch.mod.size() == proc.patch.mod.size(), "state restores (" + juce::String ((int) st.getSize()) + " bytes)");
        // editor
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setVisible (true); pump (300);
        auto* e = dynamic_cast<ui::ShizukuEditor*> (ed.get());
        CHECK (e != nullptr, "editor opens");
        for (int t = 0; t < 4; ++t) { e->showTab (t); pump (80); }
        for (auto k : { "lfo0", "lfo3", "env1", "vel", "env0" }) { e->selectSource (k); pump (80); }
        proc.patch.fx.push_back ({ 6, "Reverb", true, {} }); e->edited (true); e->rebuild(); pump (60);
        Mod m; m.src = 6; m.mod = "VoiceFilter"; m.id = 0; m.p = "kParamFreq"; m.amt = 40; proc.patch.mod.push_back (m); e->edited (true); e->showTab (2); pump (150);
        juce::Image img (juce::Image::RGB, 1320, 1010, true);
        { juce::Graphics g (img); ed->paintEntireComponent (g, true); }
        if (auto png = juce::File::getCurrentWorkingDirectory().getChildFile ("shizuku_editor.png"); true)
        { png.deleteFile(); juce::FileOutputStream os (png); juce::PNGImageFormat().writeImageToStream (img, os); }
        e->showTab (0); pump (100);
        { juce::Image i2 (juce::Image::RGB, 1056, 808, true); juce::Graphics g (i2); ed->paintEntireComponent (g, true); auto png = juce::File::getCurrentWorkingDirectory().getChildFile ("shizuku_osc.png"); png.deleteFile(); juce::FileOutputStream os (png); juce::PNGImageFormat().writeImageToStream (i2, os); }
        for (int i = 0; i < 6 && ! proc.browserReady; ++i) pump (200);
        CHECK (proc.browserReady.load() && proc.browser.size() >= 5, "preset browser lists factory presets (" + juce::String ((int) proc.browser.size()) + ")");
        ed.reset(); pump (50);
    }
    std::cout << (failures ? "FAILED: " + std::to_string (failures) : std::string ("ALL PASSED")) << "\n";
    return failures ? 1 : 0;
}
