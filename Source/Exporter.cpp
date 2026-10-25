#include "Exporter.h"

namespace kizami
{

juce::String safeFileName (const juce::String& s)
{
    auto r = juce::File::createLegalFileName (s).trim();
    return r.isEmpty() ? juce::String ("track") : r;
}

bool writeMidiFile (Engine& engine, int loops, double bpm, const juce::File& file)
{
    constexpr int ppq = 96, stepTicks = ppq / 4;
    const int total = engine.getTotalSteps();
    const float sw = engine.swing.load();
    const auto tracks = engine.getTracks();
    bool anySolo = false;
    for (auto& t : tracks) anySolo = anySolo || t->solo.load();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ppq);

    juce::MidiMessageSequence tempo;
    tempo.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / bpm)), 0);
    tempo.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
    tempo.addEvent (juce::MidiMessage::endOfTrack(), 0);
    mf.addTrack (tempo);

    for (auto& t : tracks)
    {
        if (! Engine::isAudible (*t, anySolo)) continue;
        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::textMetaEvent (3, t->name), 0);
        const int note = juce::jlimit (0, 127, t->effectiveNote());
        bool any = false;
        for (int loop = 0; loop < loops; ++loop)
            for (int i = 0; i < total; ++i)
            {
                const float v = t->vel[i].load();
                if (v <= 0.0f) continue;
                const int r = juce::jlimit (1, 4, (int) t->roll[i].load());
                const long long k = (long long) loop * total + i;
                const double base = (double) k * stepTicks + Engine::swingOffsetPpq (k, sw) * ppq;
                for (int j = 0; j < r; ++j)
                {
                    const double tick = base + j * (double) stepTicks / r;
                    const auto vel = (juce::uint8) juce::jlimit (1, 127, (int) std::round (127.0f * v * (j > 0 ? 0.82f : 1.0f)));
                    seq.addEvent (juce::MidiMessage::noteOn (10, note, vel), std::round (tick));
                    seq.addEvent (juce::MidiMessage::noteOff (10, note), std::round (tick + juce::jmax (1.0, stepTicks / (2.0 * r))));
                    any = true;
                }
            }
        if (! any) continue;
        seq.updateMatchedPairs();
        seq.addEvent (juce::MidiMessage::endOfTrack(), (double) loops * total * stepTicks);
        mf.addTrack (seq);
    }

    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk()) return false;
    return mf.writeTo (out, 1);
}

bool writeWavFile (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::File& file)
{
    file.deleteFile();
    auto stream = std::make_unique<juce::FileOutputStream> (file);
    if (! stream->openedOk()) return false;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav.createWriterFor (stream.get(), sampleRate, (unsigned int) audio.getNumChannels(), 24, {}, 0));
    if (writer == nullptr) return false;
    stream.release(); // the writer owns it now
    return writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
}

int exportAll (Engine& engine, int loops, double bpm, const juce::File& folder, juce::String& error)
{
    if (! folder.createDirectory()) { error = juce::String::fromUTF8 ("フォルダを作成できませんでした。"); return 0; }
    const juce::String base = "Kizami_" + juce::String (juce::roundToInt (bpm)) + "bpm";
    int count = 0;
    const double sr = engine.getSampleRate();

    if (writeWavFile (engine.renderOffline (loops, bpm, sr, 0), sr, folder.getChildFile (base + "_mix.wav"))) ++count;

    const auto tracks = engine.getTracks();
    bool anySolo = false;
    for (auto& t : tracks) anySolo = anySolo || t->solo.load();
    int index = 1;
    for (auto& t : tracks)
    {
        if (! Engine::isAudible (*t, anySolo)) continue;
        bool hasHits = false;
        for (int i = 0; i < engine.getTotalSteps(); ++i) hasHits = hasHits || t->vel[i].load() > 0.0f;
        if (! hasHits) continue;
        const auto name = juce::String (index++).paddedLeft ('0', 2) + "_" + safeFileName (t->name) + ".wav";
        if (writeWavFile (engine.renderOffline (loops, bpm, sr, t->uid), sr, folder.getChildFile (name))) ++count;
    }

    if (writeMidiFile (engine, loops, bpm, folder.getChildFile (base + ".mid"))) ++count;
    if (count == 0) error = juce::String::fromUTF8 ("ファイルを書き出せませんでした。");
    return count;
}

} // namespace kizami
