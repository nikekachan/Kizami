#include "Synth.h"
#include <cmath>

namespace nagare
{

void Synth::prepare (double sampleRate)
{
    sr = sampleRate;
    allNotesOff (false);
    for (auto& v : voices) { v.active = false; v.snap.reset(); }
}

void Synth::setSnapshot (SnapshotPtr s)
{
    const juce::SpinLock::ScopedLockType sl (lock);
    if (current != nullptr) graveyard.push_back (current);
    current = std::move (s);
}

void Synth::collectGarbage()
{
    const juce::SpinLock::ScopedLockType sl (lock);
    graveyard.erase (std::remove_if (graveyard.begin(), graveyard.end(),
                                     [] (const SnapshotPtr& p) { return p.use_count() <= 1; }),
                     graveyard.end());
}

void Synth::pullSnapshot()
{
    const juce::SpinLock::ScopedTryLockType sl (lock);
    if (sl.isLocked() && audioSnap != current) audioSnap = current; // old one is kept alive by the graveyard
}

int Synth::activeVoiceCount() const
{
    int n = 0;
    for (auto& v : voices) n += v.active ? 1 : 0;
    return n;
}

//==============================================================================
Voice* Synth::startVoice (int note, float velocity, const BlockParams& p)
{
    int sounding = 0;
    Voice* oldest = nullptr;
    for (auto& v : voices)
        if (v.active && ! v.released)
        {
            ++sounding;
            if (oldest == nullptr || v.age < oldest->age) oldest = &v;
        }
    if (sounding >= kMaxSounding && oldest != nullptr) releaseVoice (*oldest, p, true);

    Voice* v = nullptr;
    for (auto& c : voices) if (! c.active) { v = &c; break; }
    if (v == nullptr)
    {
        for (auto& c : voices) if (c.released && (v == nullptr || c.level < v->level)) v = &c;
        if (v == nullptr) v = oldest != nullptr ? oldest : &voices[0];
        if (monoVoice == v) monoVoice = nullptr;
        for (auto& m : polyMap) if (m == v) m = nullptr;
    }

    *v = Voice();
    v->active = true;
    v->note = note;
    v->velocity = velocity;
    v->age = ++counter;
    v->semi = v->target = note;
    v->source = p.source;

    // envelope (attack never below 0.3 ms, to avoid a click)
    const double A = juce::jmax (0.0003, p.attackMs / 1000.0);
    const double D = juce::jmax (0.001, p.decayMs / 1000.0);
    const bool useSample = p.source == Source::sample && audioSnap != nullptr;
    v->peak = velocity * (useSample ? 0.32 : 0.3);
    v->attackInc = v->peak / (A * sr);
    v->decayCoef = std::exp (-1.0 / ((D / 4.0) * sr));
    v->sustainLevel = v->peak * p.sustain;

    if (useSample)
    {
        v->snap = audioSnap;
        v->sampleMode = true;
        v->timeKeep = p.keepLength && v->snap->pitched;
        v->waveMode = p.mode == Mode::wave && v->snap->hasWave;
        v->ga = 0.0;
        v->gb = v->snap->period / std::pow (2.0, (note - v->snap->rootMidi + p.pitchAdjust) / 12.0);
        v->oscStep = 1.0 / (0.045 * sr);
        v->wavePhase = 0.0;
    }
    else
    {
        static const double ss[7] = { -24, -15, -7, 0, 7, 15, 24 };
        juce::Random rng ((juce::int64) counter * 7919);
        const Source s = p.source == Source::sample ? Source::sine : p.source;
        v->source = s;
        v->table = &builtinTable (s);
        if (s == Source::supersaw)
        {
            v->numOsc = 7;
            for (int i = 0; i < 7; ++i)
            {
                v->detune[i] = ss[i] + (rng.nextDouble() - 0.5) * 3.0;
                v->gain[i] = i == 3 ? 0.24f : 0.17f;
                v->phase[i] = rng.nextDouble();
            }
        }
        else
        {
            v->numOsc = 1;
            v->detune[0] = 0;
            v->phase[0] = 0;
            v->gain[0] = s == Source::sine ? 0.85f : s == Source::triangle ? 0.85f : s == Source::saw ? 0.5f
                       : s == Source::square ? 0.38f : 0.5f;
        }
    }
    updateVoicePitch (*v, p);
    return v;
}

void Synth::updateVoicePitch (Voice& v, const BlockParams&)
{
    if (v.sampleMode)
    {
        const double ratio = std::pow (2.0, (v.semi - v.snap->rootMidi) / 12.0); // pitch adjust added at render
        v.waveLevel = Wavetable::levelFor (v.snap->f0 * ratio, sr);
        return;
    }
    const double f = 440.0 * std::pow (2.0, (v.semi - 69.0) / 12.0);
    for (int i = 0; i < v.numOsc; ++i)
        v.level7[i] = Wavetable::levelFor (f * std::pow (2.0, v.detune[i] / 1200.0), sr);
}

void Synth::releaseVoice (Voice& v, const BlockParams& p, bool fast)
{
    if (! v.active || v.released) return;
    v.released = true;
    v.stage = 2;
    const double R = fast ? 0.012 : juce::jmax (0.005, p.releaseMs / 1000.0);
    v.releaseCoef = std::exp (-1.0 / ((R / 4.0) * sr));
}

void Synth::setTarget (Voice& v, int note, const BlockParams& p)
{
    v.note = note;
    v.target = note;
    const double tc = juce::jmax (0.003, p.glideMs / 3000.0);
    v.glideCoef = p.glideMs < 1.0f ? 0.0 : std::exp (-1.0 / (tc * sr));
    if (v.glideCoef == 0.0) { v.semi = note; updateVoicePitch (v, p); }
}

void Synth::noteOn (int note, float velocity, const BlockParams& p)
{
    note = juce::jlimit (0, 127, note);
    held[(size_t) note] = true;
    sustained[(size_t) note] = false;
    if (p.mono)
    {
        int k = 0;
        for (int i = 0; i < monoCount; ++i) if (monoStack[(size_t) i] != note) monoStack[(size_t) k++] = monoStack[(size_t) i];
        monoCount = k;
        if (monoCount < 128) monoStack[(size_t) monoCount++] = note;
        if (monoVoice != nullptr && monoVoice->active && ! monoVoice->released) setTarget (*monoVoice, note, p);
        else monoVoice = startVoice (note, velocity, p);
    }
    else
    {
        if (auto* old = polyMap[(size_t) note]) if (old->active && ! old->released && old->note == note) releaseVoice (*old, p, true);
        polyMap[(size_t) note] = startVoice (note, velocity, p);
    }
}

void Synth::noteOff (int note, const BlockParams& p)
{
    note = juce::jlimit (0, 127, note);
    held[(size_t) note] = false;
    if (pedal) { sustained[(size_t) note] = true; return; }
    doOff (note, p);
}

void Synth::doOff (int note, const BlockParams& p)
{
    if (p.mono)
    {
        int k = 0;
        for (int i = 0; i < monoCount; ++i) if (monoStack[(size_t) i] != note) monoStack[(size_t) k++] = monoStack[(size_t) i];
        monoCount = k;
        if (monoVoice == nullptr || ! monoVoice->active) return;
        if (monoCount > 0) { if (monoVoice->note != monoStack[(size_t) monoCount - 1]) setTarget (*monoVoice, monoStack[(size_t) monoCount - 1], p); }
        else { releaseVoice (*monoVoice, p, false); monoVoice = nullptr; }
    }
    else
    {
        auto* v = polyMap[(size_t) note];
        if (v != nullptr && v->active && ! v->released && v->note == note) releaseVoice (*v, p, false);
        polyMap[(size_t) note] = nullptr;
    }
}

void Synth::setPedal (bool down, const BlockParams& p)
{
    if (down == pedal) return;
    pedal = down;
    if (! down)
        for (int n = 0; n < 128; ++n)
            if (sustained[(size_t) n] && ! held[(size_t) n]) { sustained[(size_t) n] = false; doOff (n, p); }
}

void Synth::allNotesOff (bool fast)
{
    BlockParams p;
    for (auto& v : voices)
        if (v.active)
        {
            if (fast) releaseVoice (v, p, true);
            else { v.active = false; v.snap.reset(); }
        }
    monoVoice = nullptr;
    monoCount = 0;
    polyMap.fill (nullptr);
    held.fill (false);
    sustained.fill (false);
}

//==============================================================================
float Synth::sampleValue (Voice& v, double ratio)
{
    const auto& s = *v.snap;
    const double srcPerHost = s.sampleRate / sr;
    const double end = s.loopEnd, X = s.xfade, L = s.loopLen;
    float head = 0.0f;

    if (v.oscMix < 1.0)
    {
        if (! v.timeKeep)
        {
            // classic: play faster for higher notes
            double p = v.pos;
            head = s.readFlat (p);
            if (p >= end - X) { const float q = (float) ((p - (end - X)) / X); head = head * (1.0f - q) + s.readFlat (p - L) * q; }
            v.pos += ratio * srcPerHost;
            if (v.pos >= end) v.pos -= L;
        }
        else
        {
            // keep the original timing: one grain per new period, each grain a resampled original period
            const double T1 = s.period / ratio;
            while (v.t >= v.gb) { v.ga = v.gb; v.gb = v.ga + T1; }
            const double span = juce::jmax (1.0e-9, v.gb - v.ga);
            const double u = (v.t - v.ga) / span;
            const double wa = 0.5 * (1.0 + std::cos (juce::MathConstants<double>::pi * u));
            auto grain = [&] (double c) {
                const double tc = c < end ? c : s.loopStart + std::fmod (c - s.loopStart, L);
                const double m = std::round (tc / s.period) * s.period;
                const double off = (v.t - c) * ratio;
                float val = s.readFlat (m + off);
                if (tc >= end - X && tc < end)
                {
                    const float q = (float) ((tc - (end - X)) / X);
                    val = val * (1.0f - q) + s.readFlat (m - L + off) * q;
                }
                return val;
            };
            head = (float) (wa * grain (v.ga) + (1.0 - wa) * grain (v.gb));
            v.t += srcPerHost;
        }
    }

    if (! v.waveMode) return head;

    const double where = v.timeKeep ? v.t : v.pos;
    if (v.oscMix <= 0.0 && where < s.loopStart) return head;
    v.oscMix = juce::jmin (1.0, v.oscMix + v.oscStep);
    const float w = s.wave.lookup (v.wavePhase, v.waveLevel);
    v.wavePhase += s.f0 * ratio / sr;
    v.wavePhase -= std::floor (v.wavePhase);
    return (float) ((1.0 - v.oscMix) * head + v.oscMix * w);
}

void Synth::render (float* out, int start, int num, const BlockParams& p)
{
    for (auto& v : voices)
    {
        if (! v.active) continue;
        double ratio = v.sampleMode ? std::pow (2.0, (v.semi - v.snap->rootMidi + p.pitchAdjust) / 12.0) : 1.0;
        double incs[7];
        auto oscIncs = [&] {
            const double f = 440.0 * std::pow (2.0, (v.semi - 69.0) / 12.0);
            for (int i = 0; i < v.numOsc; ++i) incs[i] = f * std::pow (2.0, v.detune[i] / 1200.0) / sr;
        };
        if (! v.sampleMode) oscIncs();

        for (int i = start; i < start + num; ++i)
        {
            if (v.glideCoef > 0.0 && std::abs (v.semi - v.target) > 1.0e-4)
            {
                v.semi = v.target + (v.semi - v.target) * v.glideCoef;
                if ((i & 31) == 0 || std::abs (v.semi - v.target) <= 1.0e-4)
                {
                    if (std::abs (v.semi - v.target) <= 1.0e-4) v.semi = v.target;
                    updateVoicePitch (v, p);
                    if (v.sampleMode) ratio = std::pow (2.0, (v.semi - v.snap->rootMidi + p.pitchAdjust) / 12.0);
                    else oscIncs();
                }
            }

            // envelope
            switch (v.stage)
            {
                case 0: v.level += v.attackInc; if (v.level >= v.peak) { v.level = v.peak; v.stage = 1; } break;
                case 1: v.level = v.sustainLevel + (v.level - v.sustainLevel) * v.decayCoef; break;
                default: v.level *= v.releaseCoef; break;
            }
            if (v.stage == 2 && v.level < 1.0e-5)
            {
                v.active = false;
                v.snap.reset(); // the graveyard / synth still holds it: no free on this thread
                if (monoVoice == &v) monoVoice = nullptr;
                break;
            }

            float s = 0.0f;
            if (v.sampleMode) s = sampleValue (v, ratio);
            else
                for (int k = 0; k < v.numOsc; ++k)
                {
                    s += v.gain[k] * v.table->lookup (v.phase[k], v.level7[k]);
                    v.phase[k] += incs[k];
                    v.phase[k] -= std::floor (v.phase[k]);
                }
            out[i] += (float) (s * v.level);
        }
    }
}

//==============================================================================
void Effects::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    z1 = z2 = 0;
    lastCutoff = lastQ = -1;
    for (auto* s : { &cutoffS, &delayS, &reverbS, &volS }) s->reset (sampleRate, 0.03);
    cutoffS.setCurrentAndTargetValue (20000.0f);
    dlen = (int) (sampleRate * 0.32);
    dline.assign ((size_t) dlen, 0.0f);
    dpos = 0;
    dlp = 0;
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.8f; rp.damping = 0.45f; rp.width = 1.0f; rp.wetLevel = 1.0f; rp.dryLevel = 0.0f;
    reverb.setParameters (rp);
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    rvL.assign ((size_t) maxBlock, 0.0f);
    rvR.assign ((size_t) maxBlock, 0.0f);
    filt.assign ((size_t) maxBlock, 0.0f);
}

static float softClip (float x)
{
    const float a = std::abs (x);
    if (a < 0.8f) return x;
    const float y = 0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f);
    return x < 0 ? -y : y;
}

void Effects::process (const float* in, float* L, float* R, int num, float cutoff, float q, float dsend, float rsend, float volDb)
{
    if ((int) filt.size() < num) { filt.resize ((size_t) num); rvL.resize ((size_t) num); rvR.resize ((size_t) num); }
    cutoffS.setTargetValue (juce::jlimit (20.0f, (float) (sr * 0.45), cutoff));
    delayS.setTargetValue (dsend * 0.8f);
    reverbS.setTargetValue (rsend * 0.6f);
    volS.setTargetValue (juce::Decibels::decibelsToGain (volDb));
    const double fbLp = 1.0 - std::exp (-juce::MathConstants<double>::twoPi * 4500.0 / sr);

    for (int i = 0; i < num; ++i)
    {
        const float fc = cutoffS.getNextValue();
        if ((i & 15) == 0 && (std::abs (fc - lastCutoff) > 0.5f || q != lastQ))
        {
            lastCutoff = fc; lastQ = q;
            const double w0 = juce::MathConstants<double>::twoPi * fc / sr, cw = std::cos (w0), al = std::sin (w0) / (2.0 * juce::jmax (0.3f, q));
            const double a0 = 1.0 + al;
            b0 = (1.0 - cw) / 2.0 / a0; b1 = (1.0 - cw) / a0; b2 = b0;
            a1 = -2.0 * cw / a0; a2 = (1.0 - al) / a0;
        }
        const double x = in[i];
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        filt[(size_t) i] = (float) y;
    }

    for (int i = 0; i < num; ++i)
    {
        const float x = filt[(size_t) i];
        // delay with a darkened feedback path
        const float d = dline[(size_t) dpos];
        dlp += (float) ((d - dlp) * fbLp);
        dline[(size_t) dpos] = x * delayS.getNextValue() + dlp * 0.38f;
        dpos = (dpos + 1) % dlen;
        const float rs = reverbS.getNextValue();
        rvL[(size_t) i] = rvR[(size_t) i] = x * rs;
        L[i] = x + dlp;
        R[i] = x + dlp;
    }
    reverb.processStereo (rvL.data(), rvR.data(), num);
    for (int i = 0; i < num; ++i)
    {
        const float g = volS.getNextValue();
        L[i] = softClip ((L[i] + rvL[(size_t) i]) * g);
        R[i] = softClip ((R[i] + rvR[(size_t) i]) * g);
    }
}

} // namespace nagare
