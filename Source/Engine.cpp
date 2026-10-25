#include "Engine.h"
#include "DrumSynth.h"
#include <cmath>

namespace kizami
{

//==============================================================================
static const SoundInfo soundTable[] = {
    { "kick",    "キック",            36 },
    { "b808",    "808ベース",         35 },
    { "snare",   "スネア",            38 },
    { "clap",    "クラップ",          39 },
    { "chh",     "ハット",            42 },
    { "ohh",     "オープンハット",    46 },
    { "rim",     "リム",              37 },
    { "tom",     "タム",              45 },
    { "perc",    "パーカッション",    63 },
    { "shaker",  "シェイカー",        70 },
    { "cowbell", "カウベル",          56 },
    { "crash",   "クラッシュ",        49 },
    { "sample",  "サンプル",          60 },
};

const SoundInfo& soundInfo (Sound s)
{
    return soundTable[juce::jlimit (0, (int) Sound::count - 1, (int) s)];
}

Sound soundFromId (const juce::String& id)
{
    for (int i = 0; i < (int) Sound::count; ++i)
        if (id == soundTable[i].id)
            return (Sound) i;
    return Sound::kick;
}

juce::String soundLabel (Sound s) { return juce::String::fromUTF8 (soundInfo (s).label); }

static long long floorMod (long long a, long long b) { const long long m = a % b; return m < 0 ? m + b : m; }

static float softClip (float x)
{
    const float a = std::abs (x);
    if (a < 0.8f) return x;
    const float y = 0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f);
    return x < 0 ? -y : y;
}

//==============================================================================
Track::Track()
{
    for (auto& v : vel) v.store (0.0f);
    for (auto& r : roll) r.store (1);
    for (auto& s : skipOnce) s.store (false);
}

int Track::effectiveNote() const
{
    const int n = note.load();
    return n >= 0 ? n : soundInfo (sound).defaultNote;
}

//==============================================================================
Engine::Engine()
{
    clickHi = std::make_shared<juce::AudioBuffer<float>> (renderClick (sampleRate, true));
    clickLo = std::make_shared<juce::AudioBuffer<float>> (renderClick (sampleRate, false));
    resetToDefault();
}

void Engine::prepare (double newRate)
{
    const bool changed = std::abs (newRate - sampleRate) > 0.5;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        for (auto& v : voices) { v.active = false; v.buffer.reset(); }
        lastPpqEnd = -1.0e9;
    }
    if (! changed) return;

    sampleRate = newRate;
    clickHi = std::make_shared<juce::AudioBuffer<float>> (renderClick (sampleRate, true));
    clickLo = std::make_shared<juce::AudioBuffer<float>> (renderClick (sampleRate, false));
    for (auto& t : getTracks())
        rerender (*t);
}

std::vector<TrackPtr> Engine::getTracks() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return tracks;
}

TrackPtr Engine::findTrack (int uid) const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    for (auto& t : tracks)
        if (t->uid == uid) return t;
    return {};
}

void Engine::rerender (Track& t)
{
    auto buf = std::make_shared<juce::AudioBuffer<float>> (
        renderSound (t.sound, sampleRate, t.pitch, t.decay, t.sample.get()));
    const juce::SpinLock::ScopedLockType sl (lock);
    if (t.rendered != nullptr) graveyard.push_back (t.rendered);
    t.rendered = std::move (buf);
}

void Engine::collectGarbage()
{
    const juce::SpinLock::ScopedLockType sl (lock);
    graveyard.erase (std::remove_if (graveyard.begin(), graveyard.end(),
                                     [] (const SharedBuffer& b) { return b.use_count() <= 1; }),
                     graveyard.end());
}

TrackPtr Engine::addTrack (Sound s)
{
    auto t = std::make_shared<Track>();
    t->sound = s;
    t->name = soundLabel (s);
    rerender (*t);
    const juce::SpinLock::ScopedLockType sl (lock);
    t->uid = nextUid++;
    tracks.push_back (t);
    return t;
}

TrackPtr Engine::duplicateTrack (int uid)
{
    auto src = findTrack (uid);
    if (src == nullptr) return {};
    auto t = std::make_shared<Track>();
    t->sound = src->sound;
    t->name = src->name + " 2";
    t->volume = src->volume.load();
    t->pan = src->pan.load();
    t->mute = src->mute.load();
    t->note = -1;
    t->pitch = src->pitch;
    t->decay = src->decay;
    t->sample = src->sample;
    for (int i = 0; i < kMaxSteps; ++i) { t->vel[i] = src->vel[i].load(); t->roll[i] = src->roll[i].load(); }
    rerender (*t);
    const juce::SpinLock::ScopedLockType sl (lock);
    t->uid = nextUid++;
    auto it = std::find (tracks.begin(), tracks.end(), src);
    tracks.insert (it == tracks.end() ? tracks.end() : it + 1, t);
    return t;
}

void Engine::removeTrack (int uid)
{
    const juce::SpinLock::ScopedLockType sl (lock);
    for (auto it = tracks.begin(); it != tracks.end(); ++it)
        if ((*it)->uid == uid)
        {
            if ((*it)->rendered != nullptr) graveyard.push_back ((*it)->rendered);
            tracks.erase (it);
            return;
        }
}

void Engine::moveTrack (int uid, int delta)
{
    const juce::SpinLock::ScopedLockType sl (lock);
    for (int i = 0; i < (int) tracks.size(); ++i)
        if (tracks[(size_t) i]->uid == uid)
        {
            const int j = juce::jlimit (0, (int) tracks.size() - 1, i + delta);
            std::swap (tracks[(size_t) i], tracks[(size_t) j]);
            return;
        }
}

void Engine::setSound (int uid, Sound s)
{
    if (auto t = findTrack (uid))
    {
        if (t->name == soundLabel (t->sound)) t->name = soundLabel (s);
        t->sound = s;
        t->note = -1;
        rerender (*t);
    }
}

void Engine::setPitchDecay (int uid, float pitch, float decay)
{
    if (auto t = findTrack (uid))
    {
        t->pitch = pitch;
        t->decay = decay;
        rerender (*t);
    }
}

bool Engine::loadSampleBytes (int uid, const juce::MemoryBlock& bytes, const juce::String& name)
{
    auto t = findTrack (uid);
    if (t == nullptr) return false;

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (
        fm.createReaderFor (std::make_unique<juce::MemoryInputStream> (bytes, false)));
    if (reader == nullptr) return false;

    const auto maxLen = (juce::int64) (reader->sampleRate * 60.0); // one minute is plenty for a one-shot
    const int len = (int) juce::jmin (reader->lengthInSamples, maxLen);
    if (len <= 0) return false;

    auto data = std::make_shared<SampleData>();
    const int chans = (int) juce::jmin (2u, reader->numChannels);
    data->audio.setSize (chans, len);
    reader->read (&data->audio, 0, len, 0, true, chans > 1);
    data->sampleRate = reader->sampleRate;
    data->fileBytes = bytes;
    data->fileName = name;

    const bool wasDefaultName = t->name == soundLabel (t->sound) || t->sample != nullptr;
    t->sample = data;
    t->sound = Sound::sample;
    if (wasDefaultName) t->name = juce::File::createFileWithoutCheckingPath (name).getFileNameWithoutExtension().substring (0, 24);
    rerender (*t);
    return true;
}

bool Engine::loadSampleFile (int uid, const juce::File& file, juce::String& error)
{
    if (! file.existsAsFile()) { error = juce::String::fromUTF8 ("ファイルが見つかりません。"); return false; }
    if (file.getSize() > 50 * 1024 * 1024) { error = juce::String::fromUTF8 ("ファイルが大きすぎます(50MBまで)。"); return false; }
    juce::MemoryBlock mb;
    if (! file.loadFileAsData (mb) || ! loadSampleBytes (uid, mb, file.getFileName()))
    {
        error = juce::String::fromUTF8 ("この音声ファイルは読み込めませんでした。WAV / AIFF / MP3 で試してください。");
        return false;
    }
    return true;
}

void Engine::setBars (int newBars, bool copyLastBar)
{
    newBars = juce::jlimit (1, kMaxBars, newBars);
    const int old = bars.load();
    if (newBars == old) return;
    for (auto& t : getTracks())
    {
        if (newBars > old)
        {
            for (int i = old * kStepsPerBar; i < newBars * kStepsPerBar; ++i)
            {
                const int src = (old - 1) * kStepsPerBar + i % kStepsPerBar;
                t->vel[i] = copyLastBar ? t->vel[src].load() : 0.0f;
                t->roll[i] = copyLastBar ? t->roll[src].load() : (uint8_t) 1;
            }
        }
        else
        {
            for (int i = newBars * kStepsPerBar; i < old * kStepsPerBar; ++i) { t->vel[i] = 0.0f; t->roll[i] = 1; }
        }
    }
    bars.store (newBars);
}

void Engine::pushPadEvent (int trackUid, float velocity, bool record)
{
    const auto scope = padFifo.write (1);
    if (scope.blockSize1 > 0) padEvents[(size_t) scope.startIndex1] = { trackUid, velocity, record };
    else if (scope.blockSize2 > 0) padEvents[(size_t) scope.startIndex2] = { trackUid, velocity, record };
}

//==============================================================================
bool Engine::isAudible (const Track& t, bool anySolo)
{
    return anySolo ? t.solo.load() : ! t.mute.load();
}

double Engine::swingOffsetPpq (long long globalStep, float swingAmount)
{
    return (floorMod (globalStep, 2) == 1) ? swingAmount * 0.33 * kPpqPerStep : 0.0;
}

Voice* Engine::allocateVoice (Voice* vs)
{
    Voice* oldest = &vs[0];
    for (int i = 0; i < kMaxVoices; ++i)
    {
        if (! vs[i].active) return &vs[i];
        if (vs[i].position > oldest->position) oldest = &vs[i];
    }
    return oldest;
}

void Engine::triggerVoice (Voice* vs, Track& t, int offset, float velocity, bool live)
{
    if (t.rendered == nullptr) return;

    // 808s are mono (a new note cuts the old one); a closed hat cuts open hats.
    for (int i = 0; i < kMaxVoices; ++i)
    {
        auto& v = vs[i];
        if (! v.active) continue;
        const bool choke = (t.sound == Sound::b808 && v.trackUid == t.uid)
                        || (t.sound == Sound::chh && v.sound == Sound::ohh);
        if (choke && (v.chokeDelay < 0 || v.chokeDelay > offset)) v.chokeDelay = offset;
    }

    auto* v = allocateVoice (vs);
    const float vol = t.volume.load() * velocity;
    const float angle = (t.pan.load() + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    v->buffer = t.rendered;
    v->position = 0;
    v->delay = offset;
    v->chokeDelay = -1;
    v->fade = 1.0f;
    v->fadeStep = 0.0f;
    v->gainL = vol * std::cos (angle) * juce::MathConstants<float>::sqrt2;
    v->gainR = vol * std::sin (angle) * juce::MathConstants<float>::sqrt2;
    v->trackUid = t.uid;
    v->sound = t.sound;
    v->active = true;
    if (live) t.hitCounter.fetch_add (1);
}

void Engine::triggerClick (Voice* vs, bool accent, int offset)
{
    auto* v = allocateVoice (vs);
    v->buffer = accent ? clickHi : clickLo;
    v->position = 0; v->delay = offset; v->chokeDelay = -1;
    v->fade = 1.0f; v->fadeStep = 0.0f;
    v->gainL = v->gainR = 1.0f;
    v->trackUid = 0; v->sound = Sound::count; v->active = true;
}

void Engine::renderVoices (Voice* vs, juce::AudioBuffer<float>& out, int start, int num)
{
    float* L = out.getWritePointer (0);
    float* R = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
    const float chokeStep = 1.0f / (float) (0.005 * sampleRate);

    for (int vi = 0; vi < kMaxVoices; ++vi)
    {
        auto& v = vs[vi];
        if (! v.active) continue;
        const auto& buf = *v.buffer;
        const int len = buf.getNumSamples();
        const float* s0 = buf.getReadPointer (0);
        const float* s1 = buf.getNumChannels() > 1 ? buf.getReadPointer (1) : s0;

        for (int i = start; i < start + num; ++i)
        {
            if (v.chokeDelay == 0) { v.fadeStep = chokeStep; v.chokeDelay = -1; }
            else if (v.chokeDelay > 0) --v.chokeDelay;

            if (v.delay > 0) { --v.delay; continue; }
            if (v.position >= len || v.fade <= 0.0f) { v.active = false; break; }

            const float a = s0[v.position] * v.fade, b = s1[v.position] * v.fade;
            L[i] += a * v.gainL;
            if (R != nullptr) R[i] += b * v.gainR;
            v.fade -= v.fadeStep;
            ++v.position;
        }
        if (! v.active) v.buffer.reset();
    }
}

void Engine::sequence (Voice* vs, const std::vector<TrackPtr>& list, double from, double to,
                       double blockStart, double samplesPerPpq, int numSamples, bool live, int onlyUid)
{
    if (to <= from) return;
    const int total = getTotalSteps();
    const float sw = swing.load();
    bool anySolo = false;
    for (auto& t : list) anySolo = anySolo || t->solo.load();

    auto offsetOf = [&] (double ppq) {
        return juce::jlimit (0, numSamples - 1, (int) std::floor ((ppq - blockStart) * samplesPerPpq + 0.5));
    };

    const long long k0 = (long long) std::floor (from / kPpqPerStep) - 1;
    const long long k1 = (long long) std::floor (to / kPpqPerStep);
    for (long long k = k0; k <= k1; ++k)
    {
        const int step = (int) floorMod (k, total);
        const double base = (double) k * kPpqPerStep + swingOffsetPpq (k, sw);

        if (live && metronome.load() && floorMod (k, 4) == 0 && base >= from && base < to)
            triggerClick (vs, floorMod (k, 16) == 0, offsetOf (base));

        for (auto& tp : list)
        {
            auto& t = *tp;
            if (onlyUid != 0 && t.uid != onlyUid) continue;
            if (! isAudible (t, anySolo)) continue;
            const float v = t.vel[step].load();
            if (v <= 0.0f) continue;
            const int r = juce::jlimit (1, 4, (int) t.roll[step].load());
            for (int j = 0; j < r; ++j)
            {
                const double tt = base + j * kPpqPerStep / r;
                if (tt < from || tt >= to) continue;
                if (j == 0 && live && t.skipOnce[step].exchange (false)) break; // just recorded live
                triggerVoice (vs, t, offsetOf (tt), v * (j > 0 ? 0.82f : 1.0f), live);
            }
        }
    }
}

void Engine::handlePad (const PadEvent& e, int offset, bool playing, double ppqAt, double blockEndPpq)
{
    Track* t = nullptr;
    for (auto& tp : tracks)
        if (tp->uid == e.trackUid) { t = tp.get(); break; }
    if (t == nullptr) return;

    triggerVoice (voices.data(), *t, offset, e.velocity, true);

    if (! (e.record && playing && recording.load())) return;

    const int total = getTotalSteps();
    const float sw = swing.load();
    const long long kf = (long long) std::floor (ppqAt / kPpqPerStep);
    long long best = kf;
    double bestDist = 1.0e9;
    for (long long k = kf - 1; k <= kf + 1; ++k)
    {
        const double d = std::abs (ppqAt - ((double) k * kPpqPerStep + swingOffsetPpq (k, sw)));
        if (d < bestDist) { bestDist = d; best = k; }
    }
    const int step = (int) floorMod (best, total);
    t->vel[step] = juce::jlimit (0.4f, 1.0f, e.velocity);
    t->roll[step] = 1;
    // The step lies in a block we haven't sequenced yet: don't play this hit a second time.
    if ((double) best * kPpqPerStep + swingOffsetPpq (best, sw) >= blockEndPpq)
        t->skipOnce[step] = true;
}

void Engine::process (juce::AudioBuffer<float>& out, juce::MidiBuffer& midi, juce::AudioPlayHead* playHead)
{
    const int n = out.getNumSamples();
    out.clear();
    if (n == 0) return;

    bool hostPlay = false;
    double ppq = 0.0, bpm = lastBpm;
    if (playHead != nullptr)
        if (auto pos = playHead->getPosition())
        {
            if (auto b = pos->getBpm()) if (*b > 1.0) bpm = *b;
            if (auto p = pos->getPpqPosition()) { ppq = *p; hostPlay = pos->getIsPlaying(); }
            else if (auto secs = pos->getTimeInSeconds()) { ppq = *secs * bpm / 60.0; hostPlay = pos->getIsPlaying(); }
        }
    lastBpm = bpm;
    hostBpm.store (bpm);
    hostPlaying.store (hostPlay);

    const double ppqPerSample = bpm / 60.0 / sampleRate;
    bool playing = false;
    double start = 0.0;
    if (hostPlay)
    {
        internalPlay.store (false);
        wasInternal = false;
        playing = true;
        start = ppq;
    }
    else if (internalPlay.load())
    {
        if (! wasInternal) { internalPpq = 0.0; lastPpqEnd = -1.0e9; }
        wasInternal = true;
        playing = true;
        start = internalPpq;
        internalPpq += n * ppqPerSample;
    }
    else
    {
        wasInternal = false;
    }

    const double end = start + n * ppqPerSample;
    const double from = (playing && std::abs (start - lastPpqEnd) < ppqPerSample * 4.0) ? lastPpqEnd : start;
    lastPpqEnd = playing ? end : -1.0e9;
    currentStep.store (playing ? (int) floorMod ((long long) std::floor (start / kPpqPerStep + 1.0e-9), getTotalSteps()) : -1);

    {
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (sl.isLocked())
        {
            if (playing)
                sequence (voices.data(), tracks, from, end, start, 1.0 / ppqPerSample, n, true, 0);

            for (const auto meta : midi)
            {
                const auto msg = meta.getMessage();
                if (! msg.isNoteOn()) continue;
                const int offset = juce::jlimit (0, n - 1, meta.samplePosition);
                for (auto& tp : tracks)
                    if (tp->effectiveNote() == msg.getNoteNumber())
                        handlePad ({ tp->uid, msg.getFloatVelocity(), true }, offset, playing,
                                   start + offset * ppqPerSample, end);
            }

            const auto scope = padFifo.read (padFifo.getNumReady());
            for (int i = 0; i < scope.blockSize1; ++i) handlePad (padEvents[(size_t) (scope.startIndex1 + i)], 0, playing, start, end);
            for (int i = 0; i < scope.blockSize2; ++i) handlePad (padEvents[(size_t) (scope.startIndex2 + i)], 0, playing, start, end);
        }
    }

    renderVoices (voices.data(), out, 0, n);

    const float g = master.load();
    for (int ch = 0; ch < out.getNumChannels(); ++ch)
    {
        auto* x = out.getWritePointer (ch);
        for (int i = 0; i < n; ++i) x[i] = softClip (x[i] * g);
    }
}

//==============================================================================
juce::AudioBuffer<float> Engine::renderOffline (int loops, double bpm, double /*sr*/, int trackUid)
{
    const auto list = getTracks();
    const int total = getTotalSteps();
    const double samplesPerPpq = sampleRate * 60.0 / bpm;
    const double loopPpq = total * kPpqPerStep;
    const int loopLen = (int) std::round (loopPpq * samplesPerPpq);
    const int renderLen = loopLen * (loops + 1); // one extra loop first, so tails wrap round seamlessly

    juce::AudioBuffer<float> tmp (2, renderLen);
    tmp.clear();
    std::vector<Voice> vs ((size_t) kMaxVoices);

    constexpr int block = 512;
    for (int pos = 0; pos < renderLen; pos += block)
    {
        const int num = juce::jmin (block, renderLen - pos);
        const double a = pos / samplesPerPpq, b = (pos + num) / samplesPerPpq;
        juce::AudioBuffer<float> view (tmp.getArrayOfWritePointers(), 2, pos, num);
        sequence (vs.data(), list, a, b, a, samplesPerPpq, num, false, trackUid);
        renderVoices (vs.data(), view, 0, num);
    }

    juce::AudioBuffer<float> result (2, loopLen * loops);
    const float g = master.load();
    for (int ch = 0; ch < 2; ++ch)
    {
        const float* src = tmp.getReadPointer (ch, loopLen);
        float* dst = result.getWritePointer (ch);
        for (int i = 0; i < result.getNumSamples(); ++i) dst[i] = softClip (src[i] * g);
    }
    return result;
}

//==============================================================================
static juce::String encodeSteps (const Track& t, int total)
{
    juce::String s;
    for (int i = 0; i < total; ++i)
    {
        const float v = t.vel[i].load();
        if (v <= 0.0f) continue;
        s << i << ':' << juce::String (v, 2) << ':' << (int) t.roll[i].load() << ';';
    }
    return s;
}

static void decodeSteps (Track& t, const juce::String& s)
{
    for (auto& tok : juce::StringArray::fromTokens (s, ";", ""))
    {
        auto parts = juce::StringArray::fromTokens (tok, ":", "");
        if (parts.size() < 2) continue;
        const int i = parts[0].getIntValue();
        if (i < 0 || i >= kMaxSteps) continue;
        t.vel[i] = juce::jlimit (0.0f, 1.0f, parts[1].getFloatValue());
        t.roll[i] = (uint8_t) juce::jlimit (1, 4, parts.size() > 2 ? parts[2].getIntValue() : 1);
    }
}

juce::ValueTree Engine::toValueTree (bool includeSamples) const
{
    juce::ValueTree v ("KIZAMI");
    v.setProperty ("version", 1, nullptr);
    v.setProperty ("swing", swing.load(), nullptr);
    v.setProperty ("master", master.load(), nullptr);
    v.setProperty ("bars", bars.load(), nullptr);
    v.setProperty ("metronome", metronome.load(), nullptr);
    const int total = getTotalSteps();
    for (auto& t : getTracks())
    {
        juce::ValueTree tr ("TRACK");
        tr.setProperty ("name", t->name, nullptr);
        tr.setProperty ("sound", soundInfo (t->sound).id, nullptr);
        tr.setProperty ("volume", t->volume.load(), nullptr);
        tr.setProperty ("pan", t->pan.load(), nullptr);
        tr.setProperty ("pitch", t->pitch, nullptr);
        tr.setProperty ("decay", t->decay, nullptr);
        tr.setProperty ("mute", t->mute.load(), nullptr);
        tr.setProperty ("solo", t->solo.load(), nullptr);
        tr.setProperty ("note", t->note.load(), nullptr);
        tr.setProperty ("steps", encodeSteps (*t, total), nullptr);
        if (t->sample != nullptr)
        {
            tr.setProperty ("sampleName", t->sample->fileName, nullptr);
            if (includeSamples) tr.setProperty ("sampleData", t->sample->fileBytes.toBase64Encoding(), nullptr);
        }
        v.appendChild (tr, nullptr);
    }
    return v;
}

void Engine::fromValueTree (const juce::ValueTree& v)
{
    if (! v.hasType ("KIZAMI")) return;

    std::vector<TrackPtr> fresh;
    juce::StringArray missing;
    int uid = 1;
    for (const auto& tr : v)
    {
        if (! tr.hasType ("TRACK")) continue;
        auto t = std::make_shared<Track>();
        t->uid = uid++;
        t->sound = soundFromId (tr.getProperty ("sound", "kick").toString());
        t->name = tr.getProperty ("name", soundLabel (t->sound)).toString();
        t->volume = (float) tr.getProperty ("volume", 0.8f);
        t->pan = (float) tr.getProperty ("pan", 0.0f);
        t->pitch = (float) tr.getProperty ("pitch", 0.0f);
        t->decay = (float) tr.getProperty ("decay", 1.0f);
        t->mute = (bool) tr.getProperty ("mute", false);
        t->solo = (bool) tr.getProperty ("solo", false);
        t->note = (int) tr.getProperty ("note", -1);
        decodeSteps (*t, tr.getProperty ("steps").toString());

        if (tr.hasProperty ("sampleData"))
        {
            juce::MemoryBlock mb;
            if (mb.fromBase64Encoding (tr.getProperty ("sampleData").toString()))
            {
                juce::AudioFormatManager fm;
                fm.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> reader (
                    fm.createReaderFor (std::make_unique<juce::MemoryInputStream> (mb, false)));
                if (reader != nullptr)
                {
                    auto data = std::make_shared<SampleData>();
                    const int len = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 60.0));
                    const int chans = (int) juce::jmin (2u, reader->numChannels);
                    data->audio.setSize (chans, juce::jmax (1, len));
                    reader->read (&data->audio, 0, len, 0, true, chans > 1);
                    data->sampleRate = reader->sampleRate;
                    data->fileBytes = mb;
                    data->fileName = tr.getProperty ("sampleName").toString();
                    t->sample = data;
                }
            }
        }
        t->rendered = std::make_shared<juce::AudioBuffer<float>> (
            renderSound (t->sound, sampleRate, t->pitch, t->decay, t->sample.get()));
        fresh.push_back (t);
    }

    const juce::SpinLock::ScopedLockType sl (lock);
    for (auto& t : tracks)
        if (t->rendered != nullptr) graveyard.push_back (t->rendered);
    tracks = std::move (fresh);
    nextUid = uid;
    swing.store ((float) v.getProperty ("swing", 0.0f));
    master.store ((float) v.getProperty ("master", 0.8f));
    bars.store (juce::jlimit (1, kMaxBars, (int) v.getProperty ("bars", 1)));
    metronome.store ((bool) v.getProperty ("metronome", false));
}

//==============================================================================
namespace
{
struct PresetDef
{
    const char* name;
    float swing;
    struct T { const char* sound; const char* steps; float vol; float decay; } tracks[5];
};

// steps: "index:velocity:roll;" — same format as saved files
const PresetDef presets[] = {
    { "トラップ 140", 0.0f, {
        { "kick",  "0:1:1;6:1:1;10:1:1;", 0.85f, 1.0f },
        { "snare", "8:1:1;", 0.7f, 1.0f },
        { "clap",  "8:1:1;", 0.55f, 1.0f },
        { "chh",   "0:1:1;2:0.7:1;4:1:1;6:0.7:2;8:1:1;10:0.7:1;12:1:3;13:0.7:1;14:1:2;", 0.55f, 1.0f },
        { nullptr, nullptr, 0, 0 } } },
    { "ブーンバップ 90", 0.35f, {
        { "kick",  "0:1:1;7:1:1;10:1:1;", 0.85f, 1.2f },
        { "snare", "4:1:1;12:1:1;", 0.75f, 1.3f },
        { "clap",  "12:0.6:1;", 0.4f, 1.0f },
        { "chh",   "0:1:1;2:0.6:1;4:1:1;6:0.6:1;8:1:1;10:0.6:1;12:1:1;14:0.6:1;", 0.5f, 1.4f },
        { nullptr, nullptr, 0, 0 } } },
    { "ハウス 124", 0.1f, {
        { "kick",  "0:1:1;4:1:1;8:1:1;12:1:1;", 0.85f, 0.9f },
        { "snare", "", 0.6f, 1.0f },
        { "clap",  "4:1:1;12:1:1;", 0.6f, 1.0f },
        { "chh",   "1:0.4:1;2:1:1;3:0.4:1;5:0.4:1;6:1:1;7:0.4:1;9:0.4:1;10:1:1;11:0.4:1;13:0.4:1;14:1:1;15:0.4:1;", 0.5f, 1.0f },
        { "ohh",   "2:0.6:1;6:0.6:1;10:0.6:1;14:0.6:1;", 0.35f, 0.8f } } },
};
} // namespace

int Engine::builtInPresetCount() { return (int) std::size (presets); }
juce::String Engine::builtInPresetName (int i) { return juce::String::fromUTF8 (presets[i].name); }

juce::ValueTree Engine::builtInPreset (int index)
{
    const auto& p = presets[juce::jlimit (0, builtInPresetCount() - 1, index)];
    juce::ValueTree v ("KIZAMI");
    v.setProperty ("version", 1, nullptr);
    v.setProperty ("swing", p.swing, nullptr);
    v.setProperty ("master", 0.8f, nullptr);
    v.setProperty ("bars", 1, nullptr);
    for (const auto& t : p.tracks)
    {
        if (t.sound == nullptr) continue;
        juce::ValueTree tr ("TRACK");
        tr.setProperty ("sound", t.sound, nullptr);
        tr.setProperty ("name", soundLabel (soundFromId (t.sound)), nullptr);
        tr.setProperty ("volume", t.vol, nullptr);
        tr.setProperty ("decay", t.decay, nullptr);
        tr.setProperty ("steps", juce::String (t.steps), nullptr);
        v.appendChild (tr, nullptr);
    }
    return v;
}

void Engine::resetToDefault()
{
    fromValueTree (builtInPreset (0));
}

} // namespace kizami
