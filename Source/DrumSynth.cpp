#include "DrumSynth.h"
#include <cmath>

namespace kizami
{
namespace
{
constexpr double twoPi = juce::MathConstants<double>::twoPi;

/** Same shape as a WebAudio exponential attack/decay envelope. */
double expEnv (double t, double attack, double peak, double decay)
{
    constexpr double floor = 0.0001;
    peak = juce::jmax (peak, 0.0002);
    if (t < 0) return 0;
    if (t < attack) return floor * std::pow (peak / floor, t / attack);
    if (t < attack + decay) return peak * std::pow (floor / peak, (t - attack) / decay);
    return 0;
}

double expGlide (double t, double from, double to, double time)
{
    if (t >= time) return to;
    return from * std::pow (to / from, t / time);
}

struct Osc
{
    double phase = 0;
    double next (double freq, double sr) { const double p = phase; phase += freq / sr; phase -= std::floor (phase); return p; }
};

double sine (double p)     { return std::sin (twoPi * p); }
double square (double p)   { return p < 0.5 ? 1.0 : -1.0; }
double triangle (double p) { return 1.0 - 4.0 * std::abs (p - 0.5); }

struct Noise
{
    juce::Random rng { 0x4b495a41 };
    float next() { return rng.nextFloat() * 2.0f - 1.0f; }
};

juce::IIRFilter makeFilter (const juce::IIRCoefficients& c) { juce::IIRFilter f; f.setCoefficients (c); return f; }

double clampFreq (double f, double sr) { return juce::jlimit (20.0, sr * 0.45, f); }

juce::AudioBuffer<float> mono (double seconds, double sr)
{
    juce::AudioBuffer<float> b (1, juce::jmax (1, (int) std::ceil (seconds * sr)));
    b.clear();
    return b;
}

// ---- individual voices ------------------------------------------------------

juce::AudioBuffer<float> kick (double sr, double pm, double d)
{
    const double L = 0.42 * d;
    auto b = mono (L + 0.05, sr);
    auto* x = b.getWritePointer (0);
    Osc o; Noise n;
    auto hp = makeFilter (juce::IIRCoefficients::makeHighPass (sr, 2500.0));
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = i / sr;
        double s = sine (o.next (expGlide (t, 170 * pm, 50 * pm, 0.08), sr)) * expEnv (t, 0.002, 1.0, L);
        if (t < 0.03) s += hp.processSingleSampleRaw (n.next()) * expEnv (t, 0.0005, 0.35, 0.012);
        x[i] = (float) s;
    }
    return b;
}

juce::AudioBuffer<float> b808 (double sr, double pm, double d)
{
    const double L = 1.6 * d, base = 43.65 * pm;
    auto b = mono (L + 0.05, sr);
    auto* x = b.getWritePointer (0);
    Osc o;
    const double norm = std::tanh (2.6);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = i / sr;
        const double s = sine (o.next (expGlide (t, base * 2.2, base, 0.05), sr)) * expEnv (t, 0.003, 0.9, L);
        x[i] = (float) (std::tanh (2.6 * s) / norm * 0.85);
    }
    return b;
}

juce::AudioBuffer<float> snare (double sr, double pm, double d)
{
    const double L = 0.2 * d;
    auto b = mono (L + 0.05, sr);
    auto* x = b.getWritePointer (0);
    Osc o; Noise n;
    auto hp = makeFilter (juce::IIRCoefficients::makeHighPass (sr, clampFreq (1500 * pm, sr)));
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = i / sr;
        double s = triangle (o.next (expGlide (t, 200 * pm, 160 * pm, 0.04), sr)) * expEnv (t, 0.001, 0.55, 0.11 * d);
        s += hp.processSingleSampleRaw (n.next()) * expEnv (t, 0.001, 0.7, L);
        x[i] = (float) s;
    }
    return b;
}

juce::AudioBuffer<float> clap (double sr, double pm, double d)
{
    const double L = 0.25 * d;
    auto b = mono (0.04 + L + 0.02, sr);
    auto* x = b.getWritePointer (0);
    Noise n;
    auto bp = makeFilter (juce::IIRCoefficients::makeBandPass (sr, clampFreq (1150 * pm, sr), 1.3));
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = i / sr;
        double g;
        if (t < 0.033)
        {
            const double burst = std::fmod (t, 0.011);
            g = burst < 0.009 ? 0.9 * std::pow (0.12 / 0.9, burst / 0.009) : 0.12;
        }
        else
        {
            g = 0.8 * std::pow (0.0001 / 0.8, juce::jmin (1.0, (t - 0.033) / L));
        }
        x[i] = (float) (bp.processSingleSampleRaw (n.next()) * g);
    }
    return b;
}

juce::AudioBuffer<float> metalHat (double sr, double pm, double dec, double peak, double hpFreq)
{
    auto b = mono (dec + 0.03, sr);
    auto* x = b.getWritePointer (0);
    const double ratios[] = { 2, 3, 4.16, 5.43, 6.79, 8.21 };
    Osc osc[6]; Noise n;
    auto hp = makeFilter (juce::IIRCoefficients::makeHighPass (sr, clampFreq (hpFreq * pm, sr)));
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = i / sr;
        double m = 0;
        for (int k = 0; k < 6; ++k) m += square (osc[k].next (40 * ratios[k] * pm, sr));
        const double s = m * 0.12 + n.next() * 0.5;
        x[i] = (float) (hp.processSingleSampleRaw ((float) s) * expEnv (t, 0.0008, peak, dec));
    }
    return b;
}

juce::AudioBuffer<float> rim (double sr, double pm, double d)
{
    const double L = 0.035 * d;
    auto b = mono (L + 0.02, sr);
    auto* x = b.getWritePointer (0);
    Osc a, c;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = i / sr;
        x[i] = (float) ((sine (a.next (1750 * pm, sr)) + triangle (c.next (480 * pm, sr))) * expEnv (t, 0.0005, 0.6, L));
    }
    return b;
}

juce::AudioBuffer<float> tom (double sr, double pm, double d)
{
    const double L = 0.32 * d;
    auto b = mono (L + 0.05, sr);
    auto* x = b.getWritePointer (0);
    Osc o;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = i / sr;
        x[i] = (float) (sine (o.next (expGlide (t, 150 * pm, 95 * pm, 0.2), sr)) * expEnv (t, 0.002, 0.8, L));
    }
    return b;
}

juce::AudioBuffer<float> perc (double sr, double pm, double d)
{
    const double L = 0.13 * d;
    auto b = mono (L + 0.05, sr);
    auto* x = b.getWritePointer (0);
    Osc car, mod;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = i / sr;
        const double m = sine (mod.next (1400 * pm, sr)) * expEnv (t, 0.001, 900 * pm, 0.05);
        x[i] = (float) (sine (car.next (juce::jmax (1.0, 560 * pm + m), sr)) * expEnv (t, 0.001, 0.6, L));
    }
    return b;
}

juce::AudioBuffer<float> shaker (double sr, double pm, double d)
{
    const double L = 0.06 * d;
    auto b = mono (L + 0.04, sr);
    auto* x = b.getWritePointer (0);
    Noise n;
    auto bp = makeFilter (juce::IIRCoefficients::makeBandPass (sr, clampFreq (7500 * pm, sr), 0.9));
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = i / sr;
        x[i] = (float) (bp.processSingleSampleRaw (n.next()) * expEnv (t, 0.012, 0.35, L));
    }
    return b;
}

juce::AudioBuffer<float> cowbell (double sr, double pm, double d)
{
    const double L = 0.28 * d;
    auto b = mono (L + 0.05, sr);
    auto* x = b.getWritePointer (0);
    Osc a, c;
    auto bp = makeFilter (juce::IIRCoefficients::makeBandPass (sr, clampFreq (2640 * pm, sr), 1.6));
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = i / sr;
        const double s = square (a.next (545 * pm, sr)) + square (c.next (815 * pm, sr));
        x[i] = (float) (bp.processSingleSampleRaw ((float) s) * expEnv (t, 0.001, 0.35, L));
    }
    return b;
}

juce::AudioBuffer<float> userSample (double sr, double pm, double d, const SampleData* s)
{
    if (s == nullptr || s->audio.getNumSamples() == 0)
        return juce::AudioBuffer<float> (1, 1);

    const double ratio = (s->sampleRate / sr) * pm; // source samples per output sample
    const int srcLen = s->audio.getNumSamples();
    int outLen = (int) std::floor ((srcLen - 1) / ratio);
    if (d < 1.0) outLen = (int) (outLen * d);
    outLen = juce::jmax (1, outLen);

    const int chans = juce::jmin (2, s->audio.getNumChannels());
    juce::AudioBuffer<float> b (chans, outLen);
    for (int ch = 0; ch < chans; ++ch)
    {
        const float* src = s->audio.getReadPointer (ch);
        float* dst = b.getWritePointer (ch);
        for (int i = 0; i < outLen; ++i)
        {
            const double pos = i * ratio;
            const int i0 = (int) pos;
            const int i1 = juce::jmin (i0 + 1, srcLen - 1);
            const float frac = (float) (pos - i0);
            dst[i] = src[i0] + (src[i1] - src[i0]) * frac;
        }
    }

    // short fade-out so a trimmed sample never clicks
    const int fade = juce::jmin (outLen, (int) (0.02 * sr));
    if (d < 1.0 || pm > 1.0)
        for (int ch = 0; ch < chans; ++ch)
            b.applyGainRamp (ch, outLen - fade, fade, 1.0f, 0.0f);
    return b;
}

void trimSilence (juce::AudioBuffer<float>& b)
{
    int last = b.getNumSamples() - 1;
    while (last > 0)
    {
        bool silent = true;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            if (std::abs (b.getSample (ch, last)) > 1.0e-5f) { silent = false; break; }
        if (! silent) break;
        --last;
    }
    b.setSize (b.getNumChannels(), last + 1, true, true, true);
}
} // namespace

juce::AudioBuffer<float> renderSound (Sound sound, double sr, float pitchSemitones, float decay,
                                      const SampleData* sample)
{
    const double pm = std::pow (2.0, pitchSemitones / 12.0);
    const double d = juce::jlimit (0.1, 4.0, (double) decay);

    juce::AudioBuffer<float> b;
    switch (sound)
    {
        case Sound::kick:    b = kick (sr, pm, d); break;
        case Sound::b808:    b = b808 (sr, pm, d); break;
        case Sound::snare:   b = snare (sr, pm, d); break;
        case Sound::clap:    b = clap (sr, pm, d); break;
        case Sound::chh:     b = metalHat (sr, pm, 0.045 * d, 0.5, 7500); break;
        case Sound::ohh:     b = metalHat (sr, pm, 0.38 * d, 0.42, 7000); break;
        case Sound::crash:   b = metalHat (sr, pm, 1.5 * d, 0.38, 4800); break;
        case Sound::rim:     b = rim (sr, pm, d); break;
        case Sound::tom:     b = tom (sr, pm, d); break;
        case Sound::perc:    b = perc (sr, pm, d); break;
        case Sound::shaker:  b = shaker (sr, pm, d); break;
        case Sound::cowbell: b = cowbell (sr, pm, d); break;
        case Sound::sample:  return userSample (sr, pm, d, sample);
        case Sound::count:   break;
    }
    if (b.getNumSamples() == 0) b.setSize (1, 1, false, true);
    trimSilence (b);
    return b;
}

juce::AudioBuffer<float> renderClick (double sr, bool accent)
{
    auto b = mono (0.05, sr);
    auto* x = b.getWritePointer (0);
    Osc o;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = i / sr;
        x[i] = (float) (sine (o.next (accent ? 1600 : 1000, sr)) * expEnv (t, 0.001, accent ? 0.35 : 0.22, 0.03));
    }
    return b;
}

} // namespace kizami
