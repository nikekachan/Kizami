#include "SampleData.h"
#include <cmath>

namespace nagare
{

static const std::vector<float>& sinTable()
{
    static const std::vector<float> t = [] {
        std::vector<float> v ((size_t) kTableSize);
        for (int i = 0; i < kTableSize; ++i)
            v[(size_t) i] = (float) std::sin (juce::MathConstants<double>::twoPi * i / kTableSize);
        return v;
    }();
    return t;
}

void Wavetable::build (const std::vector<float>& re, const std::vector<float>& im)
{
    const auto& S = sinTable();
    constexpr int N = kTableSize, mask = N - 1, quarter = N / 4;
    const int H = (int) juce::jmin (re.size(), im.size()) - 1;
    std::vector<double> acc ((size_t) N, 0.0);
    int done = 0;
    for (int L = 0; L < kMipLevels; ++L)
    {
        const int upto = juce::jmin (H, 1 << L);
        for (int h = done + 1; h <= upto; ++h)
        {
            const float a = re[(size_t) h], b = im[(size_t) h];
            if (a == 0.0f && b == 0.0f) continue;
            for (int i = 0; i < N; ++i)
            {
                const int k = (h * i) & mask;
                acc[(size_t) i] += a * S[(size_t) ((k + quarter) & mask)] + b * S[(size_t) k];
            }
        }
        done = juce::jmax (done, upto);
        auto& t = tables[(size_t) L];
        t.resize ((size_t) N + 1);
        for (int i = 0; i < N; ++i) t[(size_t) i] = (float) acc[(size_t) i];
        t[(size_t) N] = t[0];
    }
}

void Wavetable::normalisePeak()
{
    float peak = 0.0f;
    for (auto v : tables[kMipLevels - 1]) peak = juce::jmax (peak, std::abs (v));
    if (peak <= 0.0f) return;
    for (auto& t : tables)
        for (auto& v : t) v /= peak;
}

int Wavetable::levelFor (double freq, double sampleRate)
{
    const double maxH = sampleRate * 0.5 / juce::jmax (1.0, freq);
    if (maxH < 2.0) return 0;
    return juce::jlimit (0, kMipLevels - 1, (int) std::floor (std::log2 (maxH)));
}

static Wavetable makeBuiltin (Source s)
{
    constexpr int H = 1024;
    std::vector<float> re ((size_t) H + 1, 0.0f), im ((size_t) H + 1, 0.0f);
    switch (s)
    {
        case Source::sine:     im[1] = 1.0f; break;
        case Source::saw:      for (int h = 1; h <= H; ++h) im[(size_t) h] = 1.0f / (float) h; break;
        case Source::square:   for (int h = 1; h <= H; h += 2) im[(size_t) h] = 1.0f / (float) h; break;
        case Source::triangle: for (int h = 1; h <= H; h += 2) im[(size_t) h] = (((h - 1) / 2) % 2 == 0 ? 1.0f : -1.0f) / (float) (h * h); break;
        case Source::organ:
        {
            const float bars[] = { 0, 1.0f, 0.8f, 0.55f, 0.3f, 0, 0.38f, 0, 0.22f };
            for (int h = 1; h < 9; ++h) im[(size_t) h] = bars[h];
            break;
        }
        case Source::sample: case Source::supersaw: case Source::count: im[1] = 1.0f; break;
    }
    Wavetable w;
    w.build (re, im);
    w.normalisePeak();
    return w;
}

const Wavetable& builtinTable (Source s)
{
    static const std::array<Wavetable, 5> t = { makeBuiltin (Source::sine), makeBuiltin (Source::triangle),
                                                makeBuiltin (Source::saw), makeBuiltin (Source::square),
                                                makeBuiltin (Source::organ) };
    switch (s)
    {
        case Source::triangle: return t[1];
        case Source::saw: case Source::supersaw: return t[2];
        case Source::square: return t[3];
        case Source::organ: return t[4];
        default: return t[0];
    }
}

void warmUpTables() { (void) builtinTable (Source::sine); }

//==============================================================================
double detectPitch (const std::vector<float>& x, double sr, int start)
{
    const int W = 2048, n = (int) x.size();
    const int tauMin = (int) (sr / 1600.0), tauMax = juce::jmin ((int) (sr / 45.0), W);
    if (start + W + tauMax >= n) start = juce::jmax (0, n - W - tauMax - 1);
    if (n < W + tauMax + 1) return 0.0;

    std::vector<double> d ((size_t) tauMax + 1, 0.0), c ((size_t) tauMax + 1, 1.0);
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        double s = 0.0;
        for (int j = 0; j < W; ++j) { const double v = x[(size_t) (start + j)] - x[(size_t) (start + j + tau)]; s += v * v; }
        d[(size_t) tau] = s;
    }
    double run = 0.0;
    for (int tau = 1; tau <= tauMax; ++tau) { run += d[(size_t) tau]; c[(size_t) tau] = run > 0 ? d[(size_t) tau] * tau / run : 1.0; }

    int best = -1;
    for (int tau = tauMin; tau < tauMax; ++tau)
        if (c[(size_t) tau] < 0.15)
        {
            while (tau + 1 < tauMax && c[(size_t) tau + 1] < c[(size_t) tau]) ++tau;
            best = tau;
            break;
        }
    if (best < 0)
    {
        double m = 1e9;
        for (int tau = tauMin; tau < tauMax; ++tau) if (c[(size_t) tau] < m) { m = c[(size_t) tau]; best = tau; }
        if (m > 0.35 || best < 1) return 0.0;
    }
    const double a = c[(size_t) best - 1], b = c[(size_t) best], cc = c[(size_t) juce::jmin (best + 1, tauMax)];
    const double den = a - 2 * b + cc;
    const double shift = den != 0.0 ? 0.5 * (a - cc) / den : 0.0;
    return sr / (best + juce::jlimit (-1.0, 1.0, shift));
}

SourcePtr makeSource (std::vector<float> raw, double sr, const juce::String& name, const juce::MemoryBlock& bytes, bool demo)
{
    float peak = 0.0f;
    for (auto v : raw) peak = juce::jmax (peak, std::abs (v));
    if (peak < 1.0e-5f || raw.size() < 64) return {};

    size_t s0 = 0;
    while (s0 < raw.size() && std::abs (raw[s0]) < peak * 0.01f) ++s0;
    s0 = (size_t) juce::jmax (0, (int) s0 - (int) (sr * 0.002));
    const size_t maxLen = (size_t) (sr * 20.0);
    auto s = std::make_shared<SampleSource>();
    s->raw.assign (raw.begin() + (long) s0, raw.begin() + (long) juce::jmin (raw.size(), s0 + maxLen));
    int pi = 0;
    for (size_t i = 0; i < s->raw.size(); ++i)
    {
        s->raw[i] /= peak;
        if (std::abs (s->raw[i]) > std::abs (s->raw[(size_t) pi])) pi = (int) i;
    }
    s->name = name;
    s->demo = demo;
    s->fileBytes = bytes;
    s->sampleRate = sr;
    s->peakIndex = pi;
    const double f0 = detectPitch (s->raw, sr, juce::jmin ((int) s->raw.size() - 1, pi + (int) (sr * 0.04)));
    s->pitched = f0 > 40.0 && f0 < 1600.0;
    s->f0 = s->pitched ? f0 : 261.63;
    s->rootMidi = s->pitched ? 69.0 + 12.0 * std::log2 (f0 / 440.0) : 60.0;
    return s;
}

SourcePtr makeDemoSource()
{
    // A decaying one-shot pluck (A3): exactly the kind of sound that normally dies away while held.
    const double sr = 44100.0, dur = 1.6, f = 220.0;
    const int n = (int) (sr * dur);
    std::vector<float> x ((size_t) n);
    juce::Random rng (20261106);
    double ph[33];
    for (int h = 1; h <= 32; ++h) ph[h] = rng.nextDouble() * juce::MathConstants<double>::twoPi;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        double s = 0.0;
        for (int h = 1; h <= 32; ++h)
            s += std::exp (-t * (1.6 + 0.75 * h)) / h * std::sin (juce::MathConstants<double>::twoPi * h * f * t + ph[h]);
        if (t < 0.012) s += (rng.nextFloat() * 2.0f - 1.0f) * 0.25 * (1.0 - t / 0.012);
        x[(size_t) i] = (float) s;
    }
    return makeSource (std::move (x), sr, "demo", {}, true);
}

SourcePtr makeSourceFromBytes (const juce::MemoryBlock& bytes, const juce::String& name, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (std::make_unique<juce::MemoryInputStream> (bytes, false)));
    if (reader == nullptr) { error = juce::String::fromUTF8 ("この音声ファイルは読み込めませんでした。WAV / AIFF / MP3 で試してください。"); return {}; }
    const int len = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 60.0));
    const int chans = (int) juce::jmax (1u, reader->numChannels);
    juce::AudioBuffer<float> buf (chans, juce::jmax (1, len));
    reader->read (&buf, 0, len, 0, true, chans > 1);
    std::vector<float> mono ((size_t) len, 0.0f);
    for (int ch = 0; ch < chans; ++ch)
    {
        const float* d = buf.getReadPointer (ch);
        for (int i = 0; i < len; ++i) mono[(size_t) i] += d[i] / (float) chans;
    }
    auto s = makeSource (std::move (mono), reader->sampleRate, name, bytes, false);
    if (s == nullptr) error = juce::String::fromUTF8 ("無音か短すぎるファイルです。");
    return s;
}

void defaultLoopFor (const SampleSource& s, float& startFrac, float& endFrac)
{
    const double n = (double) juce::jmax ((size_t) 1, s.raw.size());
    startFrac = (float) juce::jmin (0.9, (s.peakIndex + 0.08 * s.sampleRate) / n);
    endFrac = (float) juce::jmin (0.98, startFrac + 0.09 * s.sampleRate / n);
}

//==============================================================================
SnapshotPtr buildSnapshot (const SampleSource& s, float loopStartFrac, float loopEndFrac)
{
    auto out = std::make_shared<LoopSnapshot>();
    const auto& x = s.raw;
    const int n = (int) x.size();
    const double sr = s.sampleRate, period = sr / s.f0;
    out->sampleRate = sr;
    out->f0 = s.f0;
    out->period = period;
    out->rootMidi = s.rootMidi;
    out->pitched = s.pitched;

    const int minLen = juce::jmax ((int) std::round (period), (int) std::round (0.008 * sr));
    int start = juce::jlimit (2, juce::jmax (2, n - minLen - 4), (int) std::round (loopStartFrac * n));
    for (int i = start; i < juce::jmin (n - minLen - 4, start + (int) std::ceil (period)); ++i)
        if (x[(size_t) i - 1] <= 0.0f && x[(size_t) i] > 0.0f) { start = i; break; }
    const int want = juce::jlimit (start + minLen, juce::jmax (start + minLen, n - 4), (int) std::round (loopEndFrac * n)) - start;
    int cycles = juce::jmax (1, (int) std::round (want / period)); // whole periods, so the loop is seamless
    while (cycles > 1 && start + (int) std::round (cycles * period) > n - 4) --cycles;
    const int loopLen = juce::jmax (1, juce::jmin (juce::jmax (minLen, (int) std::round (cycles * period)), n - 4 - start));
    const int X = juce::jmax (1, (int) std::round (juce::jmin (loopLen * 0.5, 0.025 * sr, (double) start - 1.0)));
    const int end = start + loopLen;
    const int flatLen = juce::jmin (n, end + loopLen + X + 8);

    std::vector<float> y (x.begin(), x.begin() + flatLen);

    // Flatten the level inside the loop so it does not pulse at the loop rate.
    const int w = juce::jmax (8, (int) std::round (juce::jmax (period * 2.0, 0.01 * sr)));
    const int r0 = juce::jmax (1, start - X);
    auto rms = [&] (int i) {
        double a = 0; int c = 0;
        for (int j = juce::jmax (0, i - w); j < juce::jmin (n, i + w); ++j) { a += (double) x[(size_t) j] * x[(size_t) j]; ++c; }
        return std::sqrt (a / juce::jmax (1, c));
    };
    double tgt = 0; int cnt = 0;
    for (int i = start; i < end; i += juce::jmax (1, w / 2)) { tgt += rms (i); ++cnt; }
    tgt /= juce::jmax (1, cnt);
    const int step = juce::jmax (1, w / 4);
    std::vector<double> gpts;
    for (int i = r0; i <= end; i += step) gpts.push_back (juce::jlimit (0.25, 4.0, tgt / juce::jmax (rms (i), 1.0e-4)));
    if (gpts.empty()) gpts.push_back (1.0);
    auto gAt = [&] (int i) {
        const double f = (double) (i - r0) / step;
        const int k = (int) std::floor (f);
        const double t = f - k;
        const double a = gpts[(size_t) juce::jlimit (0, (int) gpts.size() - 1, k)];
        const double b = gpts[(size_t) juce::jlimit (0, (int) gpts.size() - 1, k + 1)];
        return a + (b - a) * t;
    };
    const int ramp = juce::jmin (r0, X);
    const double g0 = gAt (r0);
    for (int i = r0 - ramp; i < r0; ++i) y[(size_t) i] *= (float) (1.0 + (g0 - 1.0) * (double) (i - (r0 - ramp)) / juce::jmax (1, ramp));
    for (int i = r0; i < flatLen; ++i) y[(size_t) i] *= (float) gAt (i);

    out->flat = std::move (y);
    out->loopStart = start;
    out->loopEnd = end;
    out->loopLen = loopLen;
    out->xfade = X;

    // One averaged cycle -> harmonics, for "波形化".
    if (s.pitched)
    {
        constexpr int M = kTableSize;
        const auto& S = sinTable();
        const int K = juce::jlimit (1, 6, (int) std::floor (loopLen / period));
        std::vector<float> cyc ((size_t) M);
        for (int j = 0; j < M; ++j)
        {
            double a = 0;
            for (int k = 0; k < K; ++k) a += out->readFlat (start + (k + (double) j / M) * period);
            cyc[(size_t) j] = (float) (a / K);
        }
        const int H = juce::jlimit (1, 160, (int) std::floor (sr / 2.0 / s.f0));
        std::vector<float> re ((size_t) H + 1, 0.0f), im ((size_t) H + 1, 0.0f);
        for (int h = 1; h <= H; ++h)
        {
            double a = 0, b = 0;
            for (int j = 0; j < M; ++j)
            {
                const int k = (h * j) & (M - 1);
                a += cyc[(size_t) j] * S[(size_t) ((k + M / 4) & (M - 1))];
                b += cyc[(size_t) j] * S[(size_t) k];
            }
            re[(size_t) h] = (float) (2.0 * a / M);
            im[(size_t) h] = (float) (2.0 * b / M);
        }
        out->wave.build (re, im);
        out->hasWave = true;
    }
    return out;
}

} // namespace nagare
