#include "Core.h"
#include <algorithm>
#include <cstdint>
#include <limits>

namespace kanade
{
static const float NaN = std::numeric_limits<float>::quiet_NaN();

static float median (std::vector<float> w)
{
    if (w.empty()) return NaN;
    std::nth_element (w.begin(), w.begin() + (long) (w.size() / 2), w.end());
    return w[w.size() / 2];
}

static void cleanPitch (std::vector<float>& f0)
{
    const int n = (int) f0.size();
    std::vector<float> g (f0), w;
    w.reserve (40);
    // octave errors against a wide median
    for (int i = 0; i < n; ++i)
    {
        if (! voiced (f0[(size_t) i])) continue;
        w.clear();
        for (int j = std::max (0, i - 12); j <= std::min (n - 1, i + 12); ++j) if (voiced (f0[(size_t) j])) w.push_back (f0[(size_t) j]);
        const float d = f0[(size_t) i] - median (w);
        if (std::abs (d - 12) < 1.2f) g[(size_t) i] = f0[(size_t) i] - 12; else if (std::abs (d + 12) < 1.2f) g[(size_t) i] = f0[(size_t) i] + 12;
    }
    // outliers (noise, breaths, sibilants) far from their surroundings are unvoiced
    {
        std::vector<float> h (g);
        for (int i = 0; i < n; ++i)
        {
            if (! voiced (h[(size_t) i])) continue;
            w.clear();
            for (int j = std::max (0, i - 15); j <= std::min (n - 1, i + 15); ++j) if (voiced (h[(size_t) j])) w.push_back (h[(size_t) j]);
            if (w.size() < 8 || std::abs (h[(size_t) i] - median (w)) > 4) g[(size_t) i] = NaN;
        }
    }
    // 5-point median
    for (int i = 0; i < n; ++i)
    {
        if (! voiced (g[(size_t) i])) { f0[(size_t) i] = NaN; continue; }
        w.clear();
        for (int j = i - 2; j <= i + 2; ++j) if (j >= 0 && j < n && voiced (g[(size_t) j])) w.push_back (g[(size_t) j]);
        f0[(size_t) i] = median (w);
    }
    // fill gaps up to 3 frames, drop runs shorter than 6 frames
    for (int i = 1; i < n; ++i)
        if (voiced (f0[(size_t) i - 1]) && ! voiced (f0[(size_t) i]))
        {
            int j = i; while (j < n && ! voiced (f0[(size_t) j]) && j - i < 4) ++j;
            if (j < n && j - i <= 3 && voiced (f0[(size_t) j]) && std::abs (f0[(size_t) j] - f0[(size_t) i - 1]) < 2)
                for (int k = i; k < j; ++k) f0[(size_t) k] = f0[(size_t) i - 1] + (f0[(size_t) j] - f0[(size_t) i - 1]) * (float) (k - i + 1) / (float) (j - i + 1);
        }
    for (int i = 0; i < n;)
    {
        if (! voiced (f0[(size_t) i])) { ++i; continue; }
        int j = i; while (j < n && voiced (f0[(size_t) j])) ++j;
        if (j - i < 6) for (int k = i; k < j; ++k) f0[(size_t) k] = NaN;
        i = j;
    }
}

Analysis analyze (const float* x, int len, double sr, std::atomic<float>* progress, std::atomic<bool>* cancel)
{
    Analysis A; A.sr = sr;
    const int n = len / HOP + 1;
    A.n = n; A.f0.assign ((size_t) n, NaN); A.rms.assign ((size_t) n, 0.0f);
    const int DS = std::max (1, (int) std::lround (sr / 11025.0));
    const double sd = sr / DS;
    // decimate: two one-pole lowpasses at ~2.4 kHz then take every DS-th
    std::vector<float> xd ((size_t) (len / DS + 1));
    {
        const float a = 1.0f - (float) std::exp (-2 * M_PI * 2400 / sr);
        float p = 0, q = 0;
        for (int i = 0; i < len; ++i) { p += (x[i] - p) * a; q += (p - q) * a; if (i % DS == 0) xd[(size_t) (i / DS)] = q; }
    }
    const int xdLen = (int) xd.size();
    const int W = (int) std::lround (0.029 * sd), TMAX = (int) (sd / 55.0), TMIN = std::max (2, (int) (sd / 1000.0));
    const int WF = (int) std::lround (1024 * sr / 44100.0), W2 = WF;
    std::vector<float> d ((size_t) TMAX + 2);
    for (int i = 0; i < n; ++i)
    {
        if ((i & 255) == 0)
        {
            if (progress) progress->store ((float) i / (float) n);
            if (cancel && cancel->load()) return A;
        }
        const int c = i * HOP;
        {
            double s = 0; int k = 0;
            for (int j = std::max (0, c - WF / 2); j < std::min (len, c + WF / 2); ++j) { s += (double) x[j] * x[j]; ++k; }
            A.rms[(size_t) i] = (float) std::sqrt (s / std::max (1, k));
        }
        if (A.rms[(size_t) i] < 0.006f) continue;
        const int st = c / DS - W / 2;
        if (st < 0 || st + W + TMAX >= xdLen) continue;
        d[0] = 1; double run = 0;
        for (int t = 1; t <= TMAX; ++t)
        {
            double s = 0;
            const float* p0 = xd.data() + st; const float* p1 = p0 + t;
            for (int j = 0; j < W; ++j) { const float df = p0[j] - p1[j]; s += df * df; }
            run += s; d[(size_t) t] = run > 0 ? (float) (s * t / run) : 1.0f;
        }
        int tau = -1;
        for (int t = TMIN; t < TMAX; ++t) if (d[(size_t) t] < 0.15f) { while (t + 1 < TMAX && d[(size_t) t + 1] < d[(size_t) t]) ++t; tau = t; break; }
        if (tau < 0)
        {
            float mn = 1e9f;
            for (int t = TMIN; t < TMAX; ++t) if (d[(size_t) t] < mn) { mn = d[(size_t) t]; tau = t; }
            if (mn > 0.3f) continue;
        }
        if (tau < 1 || tau + 1 > TMAX) continue;
        const float a = d[(size_t) tau - 1], b = d[(size_t) tau], cc = d[(size_t) tau + 1], den = a - 2 * b + cc;
        const double tf = (den > 0 ? tau + 0.5 * (a - cc) / den : tau) * DS;
        // refine at full rate
        const int s2 = c - W2 / 2;
        if (s2 < 0 || s2 + W2 + (int) tf + 10 >= len) { A.f0[(size_t) i] = (float) (69 + 12 * std::log2 (sr / tf / 440)); continue; }
        int best = -1; double bv = 1e300; double vals[16] = {}; const int t0 = std::max (2, (int) std::lround (tf) - 5);
        for (int t = t0; t <= t0 + 10; ++t)
        {
            double s = 0;
            const float* p0 = x + s2; const float* p1 = p0 + t;
            for (int j = 0; j < W2; ++j) { const float df = p0[j] - p1[j]; s += df * df; }
            vals[t - t0] = s; if (s < bv) { bv = s; best = t; }
        }
        double tt = best;
        if (best > t0 && best < t0 + 10)
        {
            const double A0 = vals[best - 1 - t0], B = vals[best - t0], C = vals[best + 1 - t0], dn = A0 - 2 * B + C;
            if (dn > 0) tt = best + 0.5 * (A0 - C) / dn;
        }
        A.f0[(size_t) i] = (float) (69 + 12 * std::log2 (sr / tt / 440));
    }
    if (progress) progress->store (1.0f);
    cleanPitch (A.f0);
    return A;
}

void recenter (Note& nt, const Analysis& A)
{
    const int L = nt.b - nt.a;
    std::vector<float> w;
    for (int i = nt.a + L / 4; i < nt.b - L / 10; ++i) if (voiced (A.f0[(size_t) i])) w.push_back (A.f0[(size_t) i]);
    if (w.empty()) for (int i = nt.a; i < nt.b; ++i) if (voiced (A.f0[(size_t) i])) w.push_back (A.f0[(size_t) i]);
    const float m = median (w);
    nt.center = voiced (m) ? m : 60.0f;
}

Note makeNote (int a, int b, const Analysis& A, int id)
{
    Note nt; nt.id = id; nt.a = a; nt.b = b;
    recenter (nt, A); nt.pitch = nt.center;
    return nt;
}

std::vector<Note> segment (const Analysis& A, int& nextId)
{
    const int n = A.n;
    const auto& f0 = A.f0; const auto& rms = A.rms;
    std::vector<Note> notes;
    std::vector<float> sm ((size_t) n, NaN), w;
    for (int i = 0; i < n; ++i)
    {
        if (! voiced (f0[(size_t) i])) continue;
        w.clear();
        for (int j = i - 4; j <= i + 4; ++j) if (j >= 0 && j < n && voiced (f0[(size_t) j])) w.push_back (f0[(size_t) j]);
        sm[(size_t) i] = median (w);
    }
    for (int i = 0; i < n;)
    {
        if (! voiced (f0[(size_t) i])) { ++i; continue; }
        int j = i; while (j < n && voiced (f0[(size_t) j])) ++j;
        std::vector<int> cuts { i };
        float ref = sm[(size_t) (i + std::min (4, j - i - 1))]; int cnt = 0, refStart = i;
        for (int k = i + 1; k < j; ++k)
        {
            const int lo = std::max (i, k - 12), hi = std::min (j - 1, k + 12);
            float mL = 0, mR = 0;
            for (int q = lo; q < k; ++q) mL = std::max (mL, rms[(size_t) q]);
            for (int q = k + 1; q <= hi; ++q) mR = std::max (mR, rms[(size_t) q]);
            const bool dip = rms[(size_t) k] < 0.4f * std::min (mL, mR) && rms[(size_t) k] <= rms[(size_t) k - 1] && rms[(size_t) k] <= rms[(size_t) (k + 1 < j ? k + 1 : k)];
            if (std::abs (sm[(size_t) k] - ref) > 0.7f) ++cnt;
            else { cnt = 0; w.clear(); for (int q = std::max (refStart, k - 20); q <= k; ++q) w.push_back (sm[(size_t) q]); ref = median (w); }
            if (dip && k - cuts.back() >= 10) { cuts.push_back (k); ref = sm[(size_t) std::min (j - 1, k + 4)]; refStart = k; cnt = 0; }
            else if (cnt >= 8)
            {
                const int at = k - 7;
                if (at - cuts.back() >= 10) { cuts.push_back (at); refStart = std::min (j - 1, k + 8); ref = sm[(size_t) refStart]; }
                cnt = 0;
            }
        }
        cuts.push_back (j);
        for (size_t c = 0; c + 1 < cuts.size(); ++c)
        {
            const int a = cuts[c], b = cuts[c + 1];
            if (b - a < 10 && ! notes.empty() && notes.back().b == a) { notes.back().b = b; continue; }
            notes.push_back (makeNote (a, b, A, nextId++));
        }
        i = j;
    }
    // very short pieces are glides (scoops) into the next note, or tails of the previous one
    for (int k = 0; k < (int) notes.size(); ++k)
    {
        auto& nt = notes[(size_t) k];
        if (nt.b - nt.a >= 22) continue;
        if (k + 1 < (int) notes.size() && notes[(size_t) k + 1].a == nt.b) { notes[(size_t) k + 1].a = nt.a; notes.erase (notes.begin() + k); --k; }
        else if (k > 0 && notes[(size_t) k - 1].b == nt.a) { notes[(size_t) k - 1].b = nt.b; notes.erase (notes.begin() + k); --k; }
    }
    for (auto& nt : notes) { recenter (nt, A); nt.pitch = nt.center; }
    return notes;
}

float varWeight (const Note& nt, int i, double fr)
{
    const float t = (float) ((i - nt.a) * fr * 1000), te = (float) ((nt.b - 1 - i) * fr * 1000);
    float w = 1;
    if (nt.varIn > 0) w *= ss (t / nt.varIn);
    if (nt.varOut > 0) w *= ss (te / nt.varOut);
    return w;
}

static float coreShift (const Note& nt, int i, const Analysis& A)
{
    const float v = (i >= 0 && i < A.n) ? A.f0[(size_t) i] : NaN;
    const float d = voiced (v) ? v - nt.center : 0.0f;
    return nt.pitch - nt.center - d * nt.drift * varWeight (nt, i, A.frameSec());
}

Curves buildCurves (const std::vector<Note>& notesIn, const Analysis& A, float globalFormant)
{
    const int n = A.n;
    const double FR = A.frameSec();
    Curves c;
    c.shift.assign ((size_t) n, 0.0f); c.form.assign ((size_t) n, globalFormant); c.gain.assign ((size_t) n, 1.0f);
    std::vector<Note> s (notesIn);
    std::sort (s.begin(), s.end(), [] (const Note& p, const Note& q) { return p.a < q.a; });
    for (auto& nt : s)
    {
        const float g = std::pow (10.0f, nt.gain / 20.0f);
        for (int i = std::max (0, nt.a); i < std::min (n, nt.b); ++i)
        {
            c.shift[(size_t) i] = coreShift (nt, i, A);
            c.form[(size_t) i] = globalFormant + nt.formant;
            const float t = (float) ((i - nt.a + 0.5) * FR * 1000), te = (float) ((nt.b - i - 0.5) * FR * 1000);
            float f = 1;
            if (nt.fadeIn > 0) f *= ss (t / nt.fadeIn);
            if (nt.fadeOut > 0) f *= ss (te / nt.fadeOut);
            c.gain[(size_t) i] = g * f;
        }
    }
    auto frames = [FR] (float ms) { return std::max (0, (int) std::lround (ms / 1000.0 / FR)); };
    for (size_t k = 0; k < s.size(); ++k)
    {
        const auto& nt = s[k];
        const Note* prev = k > 0 ? &s[k - 1] : nullptr;
        const Note* next = k + 1 < s.size() ? &s[k + 1] : nullptr;
        const int inF = frames (nt.inMs), outF = frames (nt.outMs);
        if (prev && nt.a - prev->b <= 3)
        {
            const int pOut = frames (prev->outMs), L = pOut + inF;
            if (L > 0)
            {
                const int s0 = nt.a - pOut;
                for (int i = std::max (prev->a, s0); i < std::min (nt.b, nt.a + inF); ++i)
                {
                    if (i < 0 || i >= n) continue;
                    const float t = (float) (i - s0 + 0.5) / (float) L;
                    const float v0 = coreShift (*prev, i, A), v1 = coreShift (nt, i, A);
                    c.shift[(size_t) i] = v0 + (v1 - v0) * ss (t);
                }
            }
        }
        else if (inF > 0)
            for (int i = nt.a; i < std::min (nt.b, nt.a + inF); ++i) if (i >= 0 && i < n) c.shift[(size_t) i] *= ss ((float) (i - nt.a + 0.5) / (float) inF);
        if (! (next && next->a - nt.b <= 3) && outF > 0)
            for (int i = std::max (nt.a, nt.b - outF); i < nt.b; ++i) if (i >= 0 && i < n) c.shift[(size_t) i] *= ss ((float) (nt.b - i - 0.5) / (float) outF);
    }
    return c;
}

std::vector<Run> voicedRuns (const Analysis& A)
{
    std::vector<Run> r;
    for (int i = 0; i < A.n;)
    {
        if (! voiced (A.f0[(size_t) i])) { ++i; continue; }
        int j = i; while (j < A.n && voiced (A.f0[(size_t) j])) ++j;
        r.push_back ({ i, j }); i = j;
    }
    return r;
}

static double periodAt (const Analysis& A, double s)
{
    const double fi = s / HOP;
    const int i = std::clamp ((int) std::floor (fi), 0, A.n - 1), j = std::min (A.n - 1, i + 1);
    float v = A.f0[(size_t) i], w = A.f0[(size_t) j];
    if (! voiced (v)) v = w; if (! voiced (w)) w = v;
    if (! voiced (v)) return A.sr / 150.0;
    const double m = v + (w - v) * (fi - i);
    return A.sr / (440.0 * std::pow (2.0, (m - 69) / 12.0));
}

std::vector<int> pitchMarks (const float* x, int len, const Analysis& A, Run run)
{
    const int s0 = run.first * HOP, s1 = std::min (len - 1, run.second * HOP);
    std::vector<int> marks;
    double T = periodAt (A, s0);
    int best = s0; float bv = -1e9f;
    for (int s = s0; s < std::min ((double) s1, s0 + T); ++s) if (x[s] > bv) { bv = x[s]; best = s; }
    int m = best;
    while (m < s1)
    {
        marks.push_back (m);
        T = periodAt (A, m);
        const int lo = (int) std::floor (m + T * 0.8), hi = (int) std::ceil (m + T * 1.2);
        int b = -1; float bv2 = -1e9f;
        for (int s = lo; s <= hi && s < len; ++s) if (x[s] > bv2) { bv2 = x[s]; b = s; }
        if (b < 0 || b <= m) break;
        m = b;
    }
    return marks;
}

static float interp (const std::vector<float>& arr, double s)
{
    const double fi = s / HOP;
    const int n = (int) arr.size();
    const int i = std::clamp ((int) std::floor (fi), 0, n - 1), j = std::min (n - 1, i + 1);
    const float u = (float) std::clamp (fi - i, 0.0, 1.0);
    return arr[(size_t) i] + (arr[(size_t) j] - arr[(size_t) i]) * u;
}

void renderRuns (const float* x, int len, const Analysis& A, const Curves& C, const std::vector<Run>& runs,
                 const std::vector<std::vector<int>>& marksList, float* out, const std::vector<int>& which)
{
    std::vector<float> acc, wFrame, sm;
    for (int ri : which)
    {
        if (ri < 0 || ri >= (int) runs.size()) continue;
        const auto& run = runs[(size_t) ri];
        const auto& marks = marksList[(size_t) ri];
        if (marks.size() < 3) continue;
        const int a = run.first, b = run.second;
        bool active = false;
        for (int i = a; i < b; ++i) if (std::abs (C.shift[(size_t) i]) > 0.003f || C.form[(size_t) i] != 0) { active = true; break; }
        const int s0 = std::max (0, a * HOP - HOP), s1 = std::min (len, b * HOP + HOP);
        if (! active) { for (int s = s0; s < s1; ++s) out[s] = x[s] * interp (C.gain, s); continue; }
        acc.assign ((size_t) (s1 - s0 + 8192), 0.0f);
        const int off = s0 - 4096;
        size_t k = 0;
        double ts = marks[0];
        while (ts < marks.back())
        {
            while (k + 1 < marks.size() && std::abs (marks[k + 1] - ts) < std::abs (marks[k] - ts)) ++k;
            const int m = marks[k];
            const double Ta = k + 1 < marks.size() ? (k > 0 ? (marks[k + 1] - marks[k - 1]) / 2.0 : (double) (marks[k + 1] - marks[k])) : (double) (marks[k] - marks[k - 1]);
            const double sh = interp (C.shift, ts), al = std::pow (2.0, interp (C.form, ts) / 12.0);
            const double Ts = periodAt (A, ts) / std::pow (2.0, sh / 12.0);
            const double half = Ta / al, g = std::min (1.6, Ts / half);
            const int jl = (int) std::ceil (half), tsr = (int) std::lround (ts);
            for (int j = -jl; j <= jl; ++j)
            {
                if (std::abs (j) > half) continue;
                const double p = m + j * al;
                const int ip = (int) std::floor (p);
                if (ip < 0 || ip + 1 >= len) continue;
                const float fr = (float) (p - ip);
                const float wv = 0.5f + 0.5f * (float) std::cos (M_PI * j / half);
                const int o = tsr + j - off;
                if (o >= 0 && o < (int) acc.size()) acc[(size_t) o] += (x[ip] + (x[ip + 1] - x[ip]) * fr) * wv * (float) g;
            }
            ts += std::max (16.0, Ts);
        }
        // original where nothing changes, PSOLA where something does (short fades at the run edges)
        wFrame.assign ((size_t) (b - a + 2), 0.0f);
        for (int i = a; i < b; ++i) wFrame[(size_t) (i - a)] = (std::abs (C.shift[(size_t) i]) > 0.003f || C.form[(size_t) i] != 0) ? 1.0f : 0.0f;
        sm.assign (wFrame.size(), 0.0f);
        for (int i = 0; i < (int) wFrame.size(); ++i)
        {
            float s = 0; int c = 0;
            for (int q = i - 3; q <= i + 3; ++q) if (q >= 0 && q < (int) wFrame.size()) { s += wFrame[(size_t) q]; ++c; }
            sm[(size_t) i] = s / (float) c;
        }
        const double edge = std::min (A.sr * 0.01, (s1 - s0) / 4.0);
        for (int s = s0; s < s1; ++s)
        {
            float w = sm[(size_t) std::clamp (s / HOP - a, 0, (int) sm.size() - 1)];
            const double fromStart = s - marks.front(), toEnd = marks.back() - s;
            w *= (float) std::clamp (std::min (fromStart, toEnd) / edge, 0.0, 1.0);
            const int ai = s - off;
            const float ps = (ai >= 0 && ai < (int) acc.size()) ? acc[(size_t) ai] : 0.0f;
            out[s] = (x[s] * (1 - w) + ps * w) * interp (C.gain, s);
        }
    }
}

float snapToScale (float m, int key, int scale)
{
    static const int MAJ[] = { 0, 2, 4, 5, 7, 9, 11 }, MIN[] = { 0, 2, 3, 5, 7, 8, 10 };
    if (scale == chromatic) return std::round (m);
    const int* sc = scale == major ? MAJ : MIN;
    float best = std::round (m), bd = 1e9f;
    for (int c = (int) std::floor (m) - 2; c <= (int) std::ceil (m) + 2; ++c)
    {
        const int pc = ((c - key) % 12 + 12) % 12;
        if (std::find (sc, sc + 7, pc) == sc + 7) continue;
        const float dd = std::abs ((float) c - m);
        if (dd < bd) { bd = dd; best = (float) c; }
    }
    return best;
}

std::pair<int, int> detectKey (const std::vector<Note>& notes)
{
    const double maj[] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    const double min[] = { 6.33, 2.68, 3.52, 5.38, 2.6, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
    double h[12] = {};
    for (auto& n : notes) h[((int) std::lround (n.center) % 12 + 12) % 12] += n.b - n.a;
    auto corr = [&] (const double* p, int k) {
        double mh = 0, mp = 0; for (int i = 0; i < 12; ++i) { mh += h[i] / 12; mp += p[i] / 12; }
        double a = 0, b = 0, c = 0;
        for (int i = 0; i < 12; ++i) { const double xx = h[(i + k) % 12] - mh, yy = p[i] - mp; a += xx * yy; b += xx * xx; c += yy * yy; }
        return a / std::sqrt (b * c > 0 ? b * c : 1); };
    double br = -2; int key = 0, sc = major;
    for (int k = 0; k < 12; ++k)
    {
        const double r1 = corr (maj, k), r2 = corr (min, k);
        if (r1 > br) { br = r1; key = k; sc = major; }
        if (r2 > br) { br = r2; key = k; sc = minor; }
    }
    return { key, sc };
}
}
