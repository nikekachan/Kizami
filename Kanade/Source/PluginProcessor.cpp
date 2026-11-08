#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace kanade
{
KanadeProcessor::KanadeProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      juce::Thread ("KANADE worker")
{
    startThread (juce::Thread::Priority::low);
    startTimerHz (15);
}

KanadeProcessor::~KanadeProcessor()
{
    stopTimer();
    cancelAnalysis = true;
    signalThreadShouldExit();
    wake.signal();
    stopThread (4000);
    live.store (nullptr);
}

bool KanadeProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono());
}

void KanadeProcessor::prepareToPlay (double rate, int)
{
    sr = rate > 0 ? rate : 44100.0;
}

//==============================================================================
void KanadeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples(), nch = buffer.getNumChannels();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (nch == 0) return;

    bool playing = false, hasPos = false; juce::int64 pos = 0; double bpm = hostBpm.load();
    if (auto* ph = getPlayHead())
        if (auto p = ph->getPosition())
        {
            playing = p->getIsPlaying();
            if (auto b = p->getBpm()) bpm = *b;
            if (auto t = p->getTimeInSamples()) { pos = *t; hasPos = true; }
        }
    hostBpm = bpm; hostPlaying = playing;
    if (hasPos) hostSec = (double) pos / sr;

    float* L = buffer.getWritePointer (0);
    float* R = nch > 1 ? buffer.getWritePointer (1) : nullptr;

    // ---- transfer: record what the host plays into the capture buffer, at its song position
    if (recArmed && playing && hasPos && ! cap.empty())
    {
        if (capStart.load() < 0) capStart = pos;
        const juce::int64 cs = capStart.load();
        juce::int64 mx = capMax.load();
        for (int i = 0; i < n; ++i)
        {
            const juce::int64 idx = pos + i - cs;
            if (idx < 0 || idx >= (juce::int64) cap.size()) continue;
            cap[(size_t) idx] = R ? 0.5f * (L[i] + R[i]) : L[i];
            mx = std::max (mx, idx + 1);
        }
        capMax = mx;
        capturedSec = (double) mx / sr;
    }
    if (recArmed && ! playing && wasPlaying && capMax.load() > 0) capDone = true;
    wasPlaying = playing;
    if (playing) previewOn = false;

    const Playback* P = live.load (std::memory_order_acquire);
    const bool byp = bypass.load();
    auto sampleAt = [&] (juce::int64 p, float& v) -> bool {
        if (P == nullptr || P->s == nullptr) return false;
        const juce::int64 idx = p - P->s->start;
        if (idx < 0 || idx >= (juce::int64) P->out.size()) return false;
        v = byp ? P->s->audio[(size_t) idx] : P->out[(size_t) idx];
        return true; };

    if (! recArmed && playing && hasPos && P != nullptr)
    {
        for (int i = 0; i < n; ++i)
        {
            float v;
            if (sampleAt (pos + i, v)) { L[i] = v; if (R) R[i] = v; }
        }
    }
    else if (! playing && previewOn && P != nullptr)
    {
        const juce::int64 end = P->s->start + (juce::int64) P->out.size();
        for (int i = 0; i < n; ++i)
        {
            float v = 0;
            sampleAt (previewPos, v);
            L[i] = v; if (R) R[i] = v;
            if (++previewPos >= end) { previewOn = false; for (int k = i + 1; k < n; ++k) { L[k] = 0; if (R) R[k] = 0; } break; }
        }
        previewSec = (double) previewPos / sr;
    }
    peak = std::max (buffer.getMagnitude (0, 0, n), peak.load() * 0.8f);
}

void KanadeProcessor::preview (double fromSec)
{
    previewPos = (juce::int64) std::llround (fromSec * sr);
    previewSec = fromSec;
    previewOn = true;
}

//==============================================================================
void KanadeProcessor::publish (std::unique_ptr<Playback> pb)
{
    std::lock_guard<std::mutex> g (retireLock);
    live.store (pb.get(), std::memory_order_release);
    if (current) retired.emplace_back (std::move (current), juce::Time::getMillisecondCounter());
    current = std::move (pb);
}

void KanadeProcessor::startAnalysis (std::vector<float> audio, juce::int64 start, double rate, juce::String name, std::vector<Note> restore)
{
    cancelAnalysis = true;    // a running analysis gives way
    {
        std::lock_guard<std::mutex> g (jobLock);
        pendingAnalysis = std::make_unique<AnalysisJob> (AnalysisJob { std::move (audio), start, rate, std::move (name), std::move (restore) });
    }
    analysing = true; progress = 0;
    wake.signal();
}

void KanadeProcessor::run()
{
    while (! threadShouldExit())
    {
        wake.wait (250);
        if (threadShouldExit()) break;

        std::unique_ptr<AnalysisJob> job;
        { std::lock_guard<std::mutex> g (jobLock); job = std::move (pendingAnalysis); }
        if (job)
        {
            cancelAnalysis = false;
            auto s = std::make_shared<Session>();
            s->sr = job->rate; s->start = job->start; s->audio = std::move (job->audio); s->name = job->name;
            s->A = analyze (s->audio.data(), (int) s->audio.size(), s->sr, &progress, &cancelAnalysis);
            if (cancelAnalysis.load() || threadShouldExit()) continue;
            std::vector<Note> ns;
            int id = 1;
            if (! job->restore.empty())
            {
                for (auto& nt : job->restore) if (nt.a >= 0 && nt.b <= s->A.n && nt.b > nt.a) ns.push_back (nt);
                for (auto& nt : ns) id = std::max (id, nt.id + 1);
            }
            if (ns.empty()) ns = segment (s->A, id);
            s->runs = voicedRuns (s->A);
            for (auto r : s->runs) s->marks.push_back (pitchMarks (s->audio.data(), (int) s->audio.size(), s->A, r));
            auto pb = std::make_unique<Playback>();
            pb->out = s->audio;
            if (! job->restore.empty())
            {
                float form; { std::lock_guard<std::mutex> g (jobLock); form = renderFormant; }
                const auto C = buildCurves (ns, s->A, form);
                std::vector<int> all; for (int i = 0; i < (int) s->runs.size(); ++i) all.push_back (i);
                renderRuns (s->audio.data(), (int) s->audio.size(), s->A, C, s->runs, s->marks, pb->out.data(), all);
            }
            pb->s = s;
            {
                std::lock_guard<std::mutex> g (resultLock);
                readySession = s; readyNotes = ns; readyNotesValid = true;
            }
            publish (std::move (pb));
            analysing = false;
            continue;
        }

        std::vector<Note> ns; float form = 0; bool all = false; std::vector<int> runsToDo; bool doRender = false;
        {
            std::lock_guard<std::mutex> g (jobLock);
            if (renderPending) { ns = renderNotes; form = renderFormant; all = renderAll; runsToDo = renderRuns_; renderPending = false; renderAll = false; renderRuns_.clear(); doRender = true; }
        }
        if (doRender)
        {
            const Playback* P = live.load();
            if (P == nullptr || P->s == nullptr) continue;
            const auto& s = *P->s;
            bool fits = true;
            for (auto& nt : ns) if (nt.b > s.A.n || nt.a < 0) { fits = false; break; }
            if (! fits) continue;
            rendering = true;
            auto pb = std::make_unique<Playback>();
            pb->s = P->s; pb->out = P->out;
            const auto C = buildCurves (ns, s.A, form);
            if (all) { runsToDo.clear(); for (int i = 0; i < (int) s.runs.size(); ++i) runsToDo.push_back (i); }
            renderRuns (s.audio.data(), (int) s.audio.size(), s.A, C, s.runs, s.marks, pb->out.data(), runsToDo);
            publish (std::move (pb));
            rendering = false;
        }
    }
}

void KanadeProcessor::notesChanged (const std::vector<int>& ids, bool all)
{
    if (! session) return;
    std::vector<int> runsHit;
    if (! all)
        for (auto& nt : notes)
        {
            if (std::find (ids.begin(), ids.end(), nt.id) == ids.end()) continue;
            for (int r = 0; r < (int) session->runs.size(); ++r)
            {
                const auto& run = session->runs[(size_t) r];
                if (run.second >= nt.a - 80 && run.first <= nt.b + 80) runsHit.push_back (r);
            }
        }
    {
        std::lock_guard<std::mutex> g (jobLock);
        renderNotes = notes; renderFormant = globalFormant;
        if (all) renderAll = true;
        for (int r : runsHit) if (std::find (renderRuns_.begin(), renderRuns_.end(), r) == renderRuns_.end()) renderRuns_.push_back (r);
        renderPending = true;
    }
    wake.signal();
}

void KanadeProcessor::timerCallback()
{
    if (capDone.load())
    {
        capDone = false;
        recArmed = false;
        const auto mx = capMax.load();
        std::vector<float> a (cap.begin(), cap.begin() + (long) std::min<juce::int64> (mx, (juce::int64) cap.size()));
        std::vector<float>().swap (cap);
        // trim leading/trailing silence is not needed: the timeline keeps its position
        startAnalysis (std::move (a), capStart.load(), sr, "vocal");
    }
    {
        std::lock_guard<std::mutex> g (resultLock);
        if (readyNotesValid)
        {
            session = readySession; notes = readyNotes; readyNotesValid = false; readySession.reset();
            for (auto& nt : notes) nextId = std::max (nextId, nt.id + 1);
            undoStack.clear(); redoStack.clear();
            ++generation;
        }
    }
    {
        std::lock_guard<std::mutex> g (retireLock);
        const auto now = juce::Time::getMillisecondCounter();
        retired.erase (std::remove_if (retired.begin(), retired.end(), [now] (auto& r) { return now - r.second > 2000; }), retired.end());
    }
}

void KanadeProcessor::armRecord (bool on)
{
    if (on)
    {
        cap.assign ((size_t) (sr * 60.0 * 8.0), 0.0f);   // up to 8 minutes
        capStart = -1; capMax = 0; capDone = false; capturedSec = 0;
        recArmed = true;
    }
    else
    {
        recArmed = false;
        if (capMax.load() > 0) capDone = true;
        else std::vector<float>().swap (cap);
    }
}

void KanadeProcessor::clearSession()
{
    cancelAnalysis = true;
    live.store (nullptr);
    { std::lock_guard<std::mutex> g (retireLock); if (current) retired.emplace_back (std::move (current), juce::Time::getMillisecondCounter()); }
    session.reset(); notes.clear(); undoStack.clear(); redoStack.clear();
    analysing = false;
    ++generation;
}

bool KanadeProcessor::loadFile (const juce::File& f, juce::String& err)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr) { err = jp ("この音声ファイルは読み込めませんでした(WAV / AIFF / FLAC / MP3 を試してください)。"); return false; }
    const juce::int64 maxLen = (juce::int64) (r->sampleRate * 60 * 8);
    const int len = (int) std::min (r->lengthInSamples, maxLen);
    juce::AudioBuffer<float> b ((int) r->numChannels, len);
    r->read (&b, 0, len, 0, true, true);
    std::vector<float> mono ((size_t) len, 0.0f);
    for (int c = 0; c < b.getNumChannels(); ++c) { auto* d = b.getReadPointer (c); for (int i = 0; i < len; ++i) mono[(size_t) i] += d[i] / (float) b.getNumChannels(); }
    if (std::abs (r->sampleRate - sr) > 1)
    {
        const double ratio = r->sampleRate / sr;
        std::vector<float> rs ((size_t) (len / ratio));
        for (size_t i = 0; i < rs.size(); ++i) { const double p = (double) i * ratio; const size_t a = (size_t) p; const float fr = (float) (p - (double) a);
            rs[i] = mono[a] + (mono[std::min ((size_t) len - 1, a + 1)] - mono[a]) * fr; }
        mono.swap (rs);
    }
    startAnalysis (std::move (mono), 0, sr, f.getFileNameWithoutExtension());
    return true;
}

bool KanadeProcessor::writeWav (const juce::File& f) const
{
    std::vector<float> data; double rate;
    {
        std::lock_guard<std::mutex> g (const_cast<std::mutex&> (retireLock));
        if (! current || ! current->s) return false;
        data = current->out; rate = current->s->sr;
    }
    f.deleteFile();
    std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
    if (os == nullptr) return false;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), rate, 1, 24, {}, 0));
    if (w == nullptr) return false;
    os.release();
    const float* ch[] = { data.data() };
    return w->writeFromFloatArrays (ch, 1, (int) data.size());
}

juce::File KanadeProcessor::dragFile() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("KANADE");
    dir.createDirectory();
    juce::String base = session ? session->name : juce::String ("vocal");
    base = juce::File::createLegalFileName (base.isEmpty() ? juce::String ("vocal") : base);
    auto f = dir.getChildFile (base + "_KANADE.wav");
    return writeWav (f) ? f : juce::File();
}

//==============================================================================
juce::String KanadeProcessor::notesJson() const
{
    juce::Array<juce::var> a;
    for (auto& n : notes)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("id", n.id); o->setProperty ("a", n.a); o->setProperty ("b", n.b);
        o->setProperty ("c", n.center); o->setProperty ("p", n.pitch); o->setProperty ("d", n.drift);
        o->setProperty ("i", n.inMs); o->setProperty ("o", n.outMs); o->setProperty ("g", n.gain); o->setProperty ("f", n.formant);
        o->setProperty ("fi", n.fadeIn); o->setProperty ("fo", n.fadeOut); o->setProperty ("vi", n.varIn); o->setProperty ("vo", n.varOut);
        a.add (o);
    }
    auto* root = new juce::DynamicObject();
    root->setProperty ("notes", a); root->setProperty ("formant", globalFormant);
    return juce::JSON::toString (juce::var (root), true);
}

static std::vector<Note> parseNotes (const juce::var& v)
{
    std::vector<Note> out;
    if (auto* a = v.getArray())
        for (auto& x : *a)
        {
            Note n;
            n.id = x["id"]; n.a = x["a"]; n.b = x["b"];
            n.center = (float) (double) x["c"]; n.pitch = (float) (double) x["p"]; n.drift = (float) (double) x["d"];
            n.inMs = (float) (double) x["i"]; n.outMs = (float) (double) x["o"]; n.gain = (float) (double) x["g"]; n.formant = (float) (double) x["f"];
            n.fadeIn = (float) (double) x["fi"]; n.fadeOut = (float) (double) x["fo"]; n.varIn = (float) (double) x["vi"]; n.varOut = (float) (double) x["vo"];
            if (n.b > n.a && n.a >= 0) out.push_back (n);
        }
    return out;
}

void KanadeProcessor::notesFromJson (const juce::String& s)
{
    auto v = juce::JSON::parse (s);
    notes = parseNotes (v["notes"]);
    globalFormant = (float) (double) v["formant"];
}

void KanadeProcessor::pushUndo()
{
    undoStack.push_back (notesJson());
    if (undoStack.size() > 150) undoStack.erase (undoStack.begin());
    redoStack.clear();
}
bool KanadeProcessor::undo()
{
    if (undoStack.empty()) return false;
    redoStack.push_back (notesJson());
    notesFromJson (undoStack.back()); undoStack.pop_back();
    notesChanged ({}, true);
    return true;
}
bool KanadeProcessor::redo()
{
    if (redoStack.empty()) return false;
    undoStack.push_back (notesJson());
    notesFromJson (redoStack.back()); redoStack.pop_back();
    notesChanged ({}, true);
    return true;
}

//==============================================================================
void KanadeProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree t ("KANADE");
    t.setProperty ("version", 1, nullptr);
    t.setProperty ("notes", notesJson(), nullptr);
    t.setProperty ("key", key, nullptr); t.setProperty ("scale", scale, nullptr); t.setProperty ("snap", snapOn, nullptr);
    t.setProperty ("kc", kCenter, nullptr); t.setProperty ("kv", kVariation, nullptr); t.setProperty ("kt", kTrans, nullptr);
    t.setProperty ("cut", cutDiv, nullptr);
    if (session)
    {
        t.setProperty ("name", session->name, nullptr);
        t.setProperty ("start", (juce::int64) session->start, nullptr);
        t.setProperty ("rate", session->sr, nullptr);
        juce::MemoryBlock flac;
        {
            juce::FlacAudioFormat fmt;
            auto* mos = new juce::MemoryOutputStream (flac, false);
            std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (mos, session->sr, 1, 24, {}, 5));
            if (w == nullptr) delete mos;
            else { const float* ch[] = { session->audio.data() }; w->writeFromFloatArrays (ch, 1, (int) session->audio.size()); }
        }
        t.setProperty ("audio", juce::var (flac), nullptr);
    }
    juce::MemoryOutputStream os (dest, false);
    t.writeToStream (os);
}

void KanadeProcessor::setStateInformation (const void* data, int size)
{
    auto t = juce::ValueTree::readFromData (data, (size_t) size);
    if (! t.isValid() || ! t.hasType ("KANADE")) return;
    key = t.getProperty ("key", 0); scale = t.getProperty ("scale", 0); snapOn = t.getProperty ("snap", true);
    kCenter = t.getProperty ("kc", 1.0f); kVariation = t.getProperty ("kv", 0.5f); kTrans = t.getProperty ("kt", 0.9f);
    cutDiv = t.getProperty ("cut", 16);
    const auto v = juce::JSON::parse (t["notes"].toString());
    auto restore = parseNotes (v["notes"]);
    globalFormant = (float) (double) v["formant"];
    { std::lock_guard<std::mutex> g (jobLock); renderFormant = globalFormant; }
    if (auto* mb = t["audio"].getBinaryData())
    {
        juce::FlacAudioFormat fmt;
        std::unique_ptr<juce::AudioFormatReader> r (fmt.createReaderFor (new juce::MemoryInputStream (*mb, false), true));
        if (r != nullptr)
        {
            juce::AudioBuffer<float> b (1, (int) r->lengthInSamples);
            r->read (&b, 0, (int) r->lengthInSamples, 0, true, false);
            std::vector<float> a (b.getReadPointer (0), b.getReadPointer (0) + b.getNumSamples());
            startAnalysis (std::move (a), (juce::int64) t["start"], (double) t.getProperty ("rate", 44100.0), t["name"].toString(), restore);
        }
    }
    ++generation;
}
} // namespace kanade

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new kanade::KanadeProcessor(); }
juce::AudioProcessorEditor* kanade::KanadeProcessor::createEditor() { return new KanadeEditor (*this); }
