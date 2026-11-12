#include "Engine.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace shz
{
static constexpr double TAU = 6.283185307179586;
static constexpr int CTRL = 32;
template <typename T> static T clampv (T x, T a, T b) { return x < a ? a : x > b ? b : x; }
static double dbg (double db) { return std::pow (10.0, db / 20.0); }
static double lvl (double x) { x = std::max (0.0, x); return 4 * x * x; }
double fnorm (double n) { return 8.0 * std::pow (2750.0, clampv (n, 0.0, 1.0)); }
double tnorm (double t) { return std::pow (std::max (t, 0.0) / 32.0, 0.25); }
double tfrom (double n) { return 32.0 * std::pow (clampv (n, 0.0, 1.0), 4.0); }
double curveY (double t, double c)
{
    const double k = ((c - 50) / 50) * 6;
    if (std::abs (k) < 1e-3) return t;
    return (1 - std::exp (-k * t)) / (1 - std::exp (-k));
}
static double segShape (double f, double c)
{
    if (std::abs (c - 0.5) < 1e-3) return f;
    return std::pow (f, std::exp ((0.5 - c) * 5));
}
double lfoCurveVal (const std::vector<LfoPt>& pts, double t)
{
    if (pts.size() < 2) return 0;
    if (t <= pts[0].x) return pts[0].y;
    for (size_t i = 0; i + 1 < pts.size(); ++i)
    {
        const auto& a = pts[i]; const auto& b = pts[i + 1];
        if (t <= b.x)
        {
            const double w = b.x - a.x;
            if (w <= 1e-9) continue;
            return a.y + (b.y - a.y) * segShape ((t - a.x) / w, a.c);
        }
    }
    return pts.back().y;
}
FMode classify (const juce::String& t)
{
    auto starts = [&] (const char* s) { return t.startsWith (s); };
    if (t.containsIgnoreCase ("BandReject") || t.containsIgnoreCase ("Notch") || (t.startsWith ("N") && t.length() > 1 && juce::CharacterFunctions::isDigit (t[1]))) return FMode::notch;
    if (t.containsIgnoreCase ("Comb") || t.containsIgnoreCase ("Flange") || t.containsIgnoreCase ("Phase")) return FMode::comb;
    if (starts ("H") || starts ("MgH") || t.containsIgnoreCase ("High")) return FMode::hp;
    if (starts ("B") || starts ("MgB") || t.containsIgnoreCase ("Band")) return FMode::bp;
    if ((starts ("P") && t.length() > 1 && juce::CharacterFunctions::isDigit (t[1])) || t.containsIgnoreCase ("Peak")) return FMode::peak;
    if ((starts ("A") && t.length() > 1 && juce::CharacterFunctions::isDigit (t[1])) || t.containsIgnoreCase ("All")) return FMode::ap;
    return FMode::lp;
}
int slopeOf (const juce::String& t)
{
    const auto d = t.retainCharacters ("0123456789");
    const int s = d.isEmpty() ? 12 : d.getIntValue();
    return s >= 18 ? 2 : 1;
}
void Svf::set (double fc, double sr, double kk)
{
    g = std::tan (juce::MathConstants<double>::pi * std::min (fc, sr * 0.47) / sr); k = kk;
    a1 = 1 / (1 + g * (g + k)); a2 = g * a1; a3 = g * a2;
}
static inline double svfRun (double* st, double x, const Svf& c, FMode m)
{
    const double v3 = x - st[1];
    const double v1 = c.a1 * st[0] + c.a2 * v3;
    const double v2 = st[1] + c.a2 * st[0] + c.a3 * v3;
    st[0] = 2 * v1 - st[0]; st[1] = 2 * v2 - st[1];
    switch (m)
    {
        case FMode::lp: return v2;
        case FMode::hp: return x - c.k * v1 - v2;
        case FMode::bp: return v1;
        case FMode::notch: case FMode::comb: return x - c.k * v1;
        case FMode::peak: return 2 * v2 - x + c.k * v1;
        case FMode::ap: return x - 2 * c.k * v1;
    }
    return v2;
}
void Biquad::set (const char* type, double f, double q, double gainDb, double sr)
{
    const double A = std::pow (10.0, gainDb / 40), w = TAU * clampv (f, 10.0, sr * 0.45) / sr, cs = std::cos (w), sn = std::sin (w), al = sn / (2 * std::max (0.05, q));
    double b0, b1, b2, a0, a1, a2;
    auto is = [type] (const char* x) { return std::strcmp (type, x) == 0; };
    if (is ("peak")) { b0 = 1 + al * A; b1 = -2 * cs; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * cs; a2 = 1 - al / A; }
    else if (is ("ls")) { const double s = 2 * std::sqrt (A) * al; b0 = A * ((A + 1) - (A - 1) * cs + s); b1 = 2 * A * ((A - 1) - (A + 1) * cs); b2 = A * ((A + 1) - (A - 1) * cs - s); a0 = (A + 1) + (A - 1) * cs + s; a1 = -2 * ((A - 1) + (A + 1) * cs); a2 = (A + 1) + (A - 1) * cs - s; }
    else if (is ("hs")) { const double s = 2 * std::sqrt (A) * al; b0 = A * ((A + 1) + (A - 1) * cs + s); b1 = -2 * A * ((A - 1) + (A + 1) * cs); b2 = A * ((A + 1) + (A - 1) * cs - s); a0 = (A + 1) - (A - 1) * cs + s; a1 = 2 * ((A - 1) - (A + 1) * cs); a2 = (A + 1) - (A - 1) * cs - s; }
    else if (is ("hp")) { b0 = (1 + cs) / 2; b1 = -(1 + cs); b2 = (1 + cs) / 2; a0 = 1 + al; a1 = -2 * cs; a2 = 1 - al; }
    else if (is ("bp")) { b0 = al; b1 = 0; b2 = -al; a0 = 1 + al; a1 = -2 * cs; a2 = 1 - al; }
    else { b0 = (1 - cs) / 2; b1 = 1 - cs; b2 = (1 - cs) / 2; a0 = 1 + al; a1 = -2 * cs; a2 = 1 - al; }
    c[0] = b0 / a0; c[1] = b1 / a0; c[2] = b2 / a0; c[3] = a1 / a0; c[4] = a2 / a0;
}

double destSpan (const juce::String& mod, const juce::String& p)
{
    if (mod == "Global" && p == "kParamMasterTuning") return 100;
    if (p == "kParamCoarsePit") return 96;
    if (p == "kParamFine") return 100;
    if (p == "kParamTablePos") return 256;
    if (p == "kParamVolume" || p == "kParamDetune" || p == "kParamWarp" || p == "kParamMasterVolume" || p == "kParamLevelOut" || p == "kParamSustain") return 1;
    if (p == "kParamPan") return 2;
    if (p == "kParamFreq") return (mod == "VoiceFilter" || mod == "FXFilter" || mod == "FXDistortion") ? 1 : 0;
    if (p == "kParamAttack" || p == "kParamDecay" || p == "kParamRelease" || p == "kParamHold") return 1;
    return 100;
}

float Fx::P (const char* k, float d) const
{
    float v = d; bool found = false;
    for (auto& kv : p) if (kv.first == k) { v = kv.second; found = true; break; }
    juce::ignoreUnused (found);
    for (int i = 0; i < nmods; ++i) if (mods[(size_t) i].first == k) v += mods[(size_t) i].second;
    return v;
}
bool Fx::has (const char* k) const { for (auto& kv : p) if (kv.first == k) return true; return false; }
void Fx::addMod (const juce::String& k, float v)
{
    for (int i = 0; i < nmods; ++i) if (mods[(size_t) i].first == k) { mods[(size_t) i].second += v; return; }
    if (nmods < 8) mods[(size_t) nmods++] = { k, v };
}

static std::unique_ptr<FxState> makeState (int t, double sr)
{
    auto st = std::make_unique<FxState>();
    if (t == 1 || t == 2 || t == 3 || t == 9) { for (auto& d : st->dl) d.init ((int) std::ceil (sr * 0.1)); st->ph = 0.37; }
    if (t == 4) for (auto& d : st->dl) d.init ((int) std::ceil (sr * 4));
    if (t == 6)
    {
        const int base[8] = { 1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116 };
        for (int b : base) { const int n = (int) std::lround (b * sr / 44100); st->base.push_back (n); DelayLine d; d.init ((int) std::ceil (n * 2.2) + 8); st->lines.push_back (std::move (d)); }
        for (auto& d : st->pre) d.init ((int) std::ceil (sr * 0.5));
        const int df[4] = { 142, 107, 379, 277 }, dl2[4] = { 113, 162, 241, 341 };
        for (int k = 0; k < 4; ++k) { DelayLine d; d.init ((int) std::lround (df[k] * sr / 44100) + 2); st->diff.push_back (std::move (d)); st->dlen.push_back ((int) std::lround (dl2[k] * sr / 44100)); }
    }
    return st;
}

std::unique_ptr<EnginePatch> compile (const Patch& p, double sr)
{
    auto E = std::make_unique<EnginePatch>();
    E->p = p;
    E->a4 = (p.glob.tuning > 415 && p.glob.tuning < 466) ? p.glob.tuning : 440;
    auto rack = [&] (const std::vector<FxUnit>& src, std::vector<std::unique_ptr<Fx>>& dst)
    {
        for (auto& u : src)
        {
            auto f = std::make_unique<Fx>();
            f->type = u.type; f->on = u.on;
            for (auto& kv : u.p)
            {
                if (kv.value.isString()) { if (kv.name.toString() == "kParamMode" || kv.name.toString() == "kParamType") f->mode = kv.value.toString(); }
                else f->p.push_back ({ kv.name.toString(), (float) (double) kv.value });
            }
            f->st = makeState (u.type, sr);
            dst.push_back (std::move (f));
        }
    };
    rack (p.fx, E->fx); rack (p.bus1, E->bus1); rack (p.bus2, E->bus2);
    for (int j = 0; j < 2; ++j) { E->fmode[(size_t) j] = classify (p.filt[(size_t) j].type); E->ftwo[(size_t) j] = slopeOf (p.filt[(size_t) j].type) == 2; E->fmg[(size_t) j] = p.filt[(size_t) j].type.startsWith ("Mg"); }
    for (auto& m : p.mod)
    {
        if (m.mod.isEmpty() || m.p.isEmpty()) continue;
        const double span = destSpan (m.mod, m.p);
        CMod c { m.src, m.aux, m.bip, (float) (m.amt / 100.0 * (span != 0 ? span : 1)), -1, -1, m.p, span == 0, false };
        if (m.mod.startsWith ("FX")) { c.fxId = m.id; E->gmods.push_back (c); continue; }
        if (m.mod == "Global" && m.p == "kParamMasterVolume") { c.gvol = true; E->gmods.push_back (c); continue; }
        int slot = -1;
        if (m.mod == "Oscillator")
        {
            static const char* k[] = { "kParamVolume", "kParamPan", "kParamTablePos", "kParamDetune", "kParamWarp", "kParamCoarsePit", "kParamFine", "kParamUnisonStereo" };
            for (int i = 0; i < 8; ++i) if (m.p == k[i]) slot = m.id * 8 + i;
            if (m.p == "kParamPitch") slot = m.id * 8 + 5;
        }
        else if (m.mod == "VoiceFilter")
        {
            static const char* k[] = { "kParamFreq", "kParamReso", "kParamDrive", "kParamVar" };
            for (int i = 0; i < 4; ++i) if (m.p == k[i]) slot = 60 + m.id * 4 + i;
        }
        else if (m.mod == "Env")
        {
            static const char* k[] = { "kParamAttack", "kParamDecay", "kParamSustain", "kParamRelease" };
            for (int i = 0; i < 4; ++i) if (m.p == k[i]) slot = 40 + m.id * 4 + i;
        }
        else if (m.mod == "Global" && m.p == "kParamMasterTuning") slot = 70;
        if (slot < 0 || slot >= 80) continue;
        c.slot = slot; E->vmods.push_back (c);
    }
    return E;
}

//==============================================================================
Engine::Engine (double s) : sr (s) { setSampleRate (s); }
void Engine::setSampleRate (double s)
{
    sr = s;
    for (auto* v : { &dL, &dR, &b1L, &b1R, &b2L, &b2R }) v->assign (8192, 0.0f);
    if (P) P = compile (P->p, sr);
}

std::unique_ptr<EnginePatch> Engine::swapPatch (std::unique_ptr<EnginePatch> np, bool full)
{
    if (P && ! full)
    {
        auto carry = [] (std::vector<std::unique_ptr<Fx>>& a, std::vector<std::unique_ptr<Fx>>& b)
        { for (size_t i = 0; i < a.size() && i < b.size(); ++i) if (a[i]->type == b[i]->type) std::swap (a[i]->st, b[i]->st); };
        carry (np->fx, P->fx); carry (np->bus1, P->bus1); carry (np->bus2, P->bus2);
    }
    const bool monoChanged = ! P || P->p.glob.mono != np->p.glob.mono;
    std::swap (P, np);
    modwheel = P->p.glob.modwheel / 100.0;
    if (full || monoChanged) allOff (true);
    return np;
}

void Engine::allOff (bool hard)
{
    for (auto& v : voices) { if (hard) v.on = false; else release (v); }
    held.clear(); arpNote = -1; arpGate = -1;
}

void Engine::noteOn (int n, float vel)
{
    if (! P) return;
    held.erase (std::remove_if (held.begin(), held.end(), [n] (const Held& h) { return h.n == n; }), held.end());
    if (held.size() < 64) held.push_back ({ n, vel, noteCount++ });
    if (P->p.arp.on) return;
    trigger (n, vel);
}
void Engine::noteOff (int n)
{
    if (! P) return;
    held.erase (std::remove_if (held.begin(), held.end(), [n] (const Held& h) { return h.n == n; }), held.end());
    if (P->p.arp.on) { if (held.empty()) arpStop(); return; }
    if (P->p.glob.mono)
    {
        auto& v = voices[0];
        if (! held.empty()) { const auto& h = held.back(); v.target = h.n; v.note = h.n; if (P->p.glob.porta <= 0) v.pitch = h.n; }
        else release (v);
        return;
    }
    for (auto& v : voices) if (v.on && v.gate && v.note == n) release (v);
}
void Engine::trigger (int n, float vel)
{
    const auto& p = P->p;
    Voice* v = nullptr;
    if (p.glob.mono)
    {
        v = &voices[0];
        for (size_t i = 1; i < voices.size(); ++i) voices[i].on = false;
        const bool legato = v->on && v->gate;
        v->target = n;
        if (legato && p.glob.legato) { v->note = n; v->vel = vel; if (p.glob.porta <= 0) v->pitch = n; last = v; return; }
        if (! (v->on && (p.glob.portaAlways || legato)) || p.glob.porta <= 0) v->pitch = n;
    }
    else
    {
        for (auto& x : voices) if (! x.on) { v = &x; break; }
        if (! v) { v = &voices[0]; for (auto& x : voices) if (x.age < v->age) v = &x; }
        v->target = n;
        v->pitch = (p.glob.portaAlways && p.glob.porta > 0 && last && last->on) ? last->pitch : n;
    }
    const bool wasOn = v->on;
    v->on = true; v->gate = true; v->note = n; v->vel = vel; v->age = noteCount++; v->rnd = (float) rnd(); v->samplePos = 0;
    for (int o = 0; o < 3; ++o)
        for (int u = 0; u < 16; ++u) v->ph[o][u] = std::fmod (p.osc[(size_t) o].phase / 360.0 + p.osc[(size_t) o].rand / 100.0 * rnd(), 1.0);
    v->subPh = 0;
    for (auto& e : v->env) { e.from = wasOn ? e.v : 0; e.st = 1; e.t = 0; }
    for (int i = 0; i < 10; ++i) { v->lfoPh[i] = 0; v->lfoHold[i] = rnd(); }
    if (! wasOn) { std::fill (&v->fs[0][0], &v->fs[0][0] + 32, 0.0); v->ampPrev = -1; }
    last = v;
}
void Engine::release (Voice& v)
{
    if (! v.gate) return;
    v.gate = false;
    for (auto& e : v.env) { e.from = e.v; e.st = 4; e.t = 0; }
}
void Engine::arpStop()
{
    if (arpNote >= 0) for (auto& v : voices) if (v.on && v.note == arpNote) release (v);
    arpNote = -1; arpI = 0; arpT = 0; arpGate = -1;
}
void Engine::arpTick (int n)
{
    const auto& a = P->p.arp;
    if (! a.on || held.empty()) return;
    const double stepS = (60.0 / bpm) / std::max (0.25f, a.rate) * sr;
    for (int i = 0; i < n; ++i)
    {
        if (arpT <= 0)
        {
            int notes[64]; int cnt = 0;
            Held hs[64]; const int hn = (int) std::min<size_t> (64, held.size());
            std::copy (held.begin(), held.begin() + hn, hs);
            std::sort (hs, hs + hn, [&] (const Held& x, const Held& y) { return a.mode == "played" ? x.t < y.t : x.n < y.n; });
            int seq[256]; int sn = 0;
            for (int k = 0; k < hn; ++k) notes[cnt++] = hs[k].n;
            for (int o = 0; o < std::max (1, a.oct); ++o) for (int k = 0; k < cnt && sn < 256; ++k) seq[sn++] = notes[k] + 12 * o;
            if (a.mode == "down") std::reverse (seq, seq + sn);
            if (a.mode == "updown" && sn > 2) { const int base = sn; for (int k = base - 2; k >= 1 && sn < 256; --k) seq[sn++] = seq[k]; }
            const int nn = a.mode == "random" ? seq[(int) (rnd() * sn) % sn] : seq[arpI % sn];
            ++arpI;
            if (arpNote >= 0) for (auto& v : voices) if (v.on && v.gate && v.note == arpNote) release (v);
            trigger (nn, held.back().vel); arpNote = nn; arpT = stepS; arpGate = stepS * a.gate / 100.0;
        }
        arpT -= 1;
        if (arpGate >= 0) { arpGate -= 1; if (arpGate <= 0 && arpNote >= 0) { for (auto& v : voices) if (v.on && v.gate && v.note == arpNote) release (v); arpGate = -1; } }
    }
}

double Engine::envStep (Voice& v, int i, double dt)
{
    const auto& E = P->p.env[(size_t) i]; auto& e = v.env[i]; const double* dm = v.dm;
    const double a = tfrom (tnorm (E.a) + dm[40 + i * 4]), d = tfrom (tnorm (E.d) + dm[41 + i * 4]), s = clampv ((double) E.s + dm[42 + i * 4], 0.0, 1.0), r = tfrom (tnorm (E.r) + dm[43 + i * 4]);
    e.t += dt;
    switch (e.st)
    {
        case 1: if (e.t >= a) { e.st = E.h > 0 ? 2 : 3; e.t = 0; e.v = 1; } else e.v = e.from + (1 - e.from) * curveY (e.t / std::max (a, 1e-6), E.c1); break;
        case 2: e.v = 1; if (e.t >= E.h) { e.st = 3; e.t = 0; } break;
        case 3: if (e.t >= d) e.v = s; else e.v = 1 + (s - 1) * curveY (e.t / std::max (d, 1e-6), E.c2); break;
        case 4: if (e.t >= r) { e.v = 0; e.st = 0; } else e.v = e.from * (1 - curveY (e.t / std::max (r, 1e-6), E.c3)); break;
        default: e.v = 0;
    }
    return e.v;
}

void Engine::globalLfos (double dt)
{
    for (int i = 0; i < 10; ++i)
    {
        const auto& l = P->p.lfo[(size_t) i];
        const double hz = std::max (0.001, (double) l.rate * (l.x10 ? 10 : 1));
        if (l.type == "Lorenz" || l.type == "Rossler")
        {
            auto& c = chaos[i]; if (c[0] == 0 && c[1] == 0 && c[2] == 0) c[0] = 0.1;
            const double h = std::min (0.02, hz * dt * 2);
            for (int k = 0; k < 4; ++k)
            {
                double x = c[0], y = c[1], z = c[2];
                if (l.type == "Lorenz") { const double dx = 10 * (y - x), dy = x * (28 - z) - y, dz = x * y - (8.0 / 3) * z; c[0] = x + dx * h * 0.25; c[1] = y + dy * h * 0.25; c[2] = z + dz * h * 0.25; gLfoV[i] = clampv (0.5 + c[0] / 40, 0.0, 1.0); }
                else { const double dx = -y - z, dy = x + 0.2 * y, dz = 0.2 + z * (x - 5.7); c[0] = x + dx * h; c[1] = y + dy * h; c[2] = z + dz * h; gLfoV[i] = clampv (0.5 + c[0] / 22, 0.0, 1.0); }
            }
            continue;
        }
        const double prev = gLfoPh[i];
        gLfoPh[i] = std::fmod (prev + hz * dt, 1.0);
        if (l.type == "RandomSH")
        {
            if (gLfoPh[i] < prev) gLfoHold[i] = rnd();
            const double sm = l.smooth / 100.0;
            gLfoV[i] += (gLfoHold[i] - gLfoV[i]) * (sm > 0 ? std::min (1.0, dt * hz / (sm * 2)) : 1.0);
        }
        else gLfoV[i] = lfoCurveVal (l.pts, gLfoPh[i]);
    }
}

void Engine::voiceSources (Voice& v, double dt)
{
    const auto& p = P->p; double* S = v.src;
    for (int i = 0; i < 4; ++i) S[2 + i] = envStep (v, i, dt);
    for (int i = 0; i < 10; ++i)
    {
        const auto& l = p.lfo[(size_t) i];
        if (l.mode == "Free" || l.mono || l.type == "Lorenz" || l.type == "Rossler") { S[6 + i] = gLfoV[i]; continue; }
        const double hz = std::max (0.001, (double) l.rate * (l.x10 ? 10 : 1));
        double ph = v.lfoPh[i] + hz * dt;
        if (l.mode == "Envelope") { if (ph > 1) ph = 1; }
        else if (ph >= 1) { ph -= std::floor (ph); v.lfoHold[i] = rnd(); }
        v.lfoPh[i] = ph;
        S[6 + i] = l.type == "RandomSH" ? v.lfoHold[i] : lfoCurveVal (l.pts, ph);
    }
    S[1] = modwheel; S[16] = v.vel; S[17] = (v.note - 60) / 60.0 * 0.5 + 0.5; S[18] = 0; S[19] = bend * 0.5 + 0.5; S[20] = v.rnd;
    for (int i = 0; i < 8; ++i) S[25 + i] = macros[(size_t) i] / 100.0;
}
void Engine::voiceMods (Voice& v)
{
    double* dm = v.dm; const double* S = v.src;
    std::fill (dm, dm + 80, 0.0);
    for (auto& m : P->vmods)
    {
        double x = (m.src >= 0 && m.src < 41) ? S[m.src] : 0; if (m.bip) x = x * 2 - 1;
        if (m.aux) x *= (m.aux > 0 && m.aux < 41) ? S[m.aux] : 0;
        dm[m.slot] += x * m.k;
    }
}
void Engine::globalMods()
{
    Voice* v = last && last->on ? last : nullptr;
    const double* S = v ? v->src : nullptr;
    for (auto* list : { &P->fx, &P->bus1, &P->bus2 }) for (auto& f : *list) f->nmods = 0;
    gVol = 0;
    for (auto& m : P->gmods)
    {
        double x;
        if (m.src >= 25 && m.src <= 32) x = macros[(size_t) (m.src - 25)] / 100.0;
        else if (m.src >= 6 && m.src <= 15) x = S ? S[m.src] : gLfoV[m.src - 6];
        else if (m.src == 1) x = modwheel;
        else x = S && m.src < 41 ? S[m.src] : 0;
        if (m.bip) x = x * 2 - 1;
        if (m.aux) x *= (m.aux == 1 ? modwheel : (S && m.aux < 41 ? S[m.aux] : 0));
        if (m.gvol) { gVol += x * m.k; continue; }
        if (m.fxId < 0 || m.fxId >= (int) P->fx.size()) continue;
        auto& f = *P->fx[(size_t) m.fxId];
        if (m.hz) { const double base = f.has (m.fxP.toRawUTF8()) ? f.P (m.fxP.toRawUTF8(), 1000) : 1000; f.addMod (m.fxP, (float) (base * (std::pow (2.0, x * m.k * 0.08) - 1))); }
        else f.addMod (m.fxP, (float) (x * m.k));
    }
}

static inline float lookup (const float* lv, int n, double frame, double ph)
{
    const int f0 = (int) frame; const double ff = frame - f0; const int f1 = f0 + 1 < n ? f0 + 1 : f0;
    const double x = ph * 2048; const int i = std::min (2047, (int) x); const float t = (float) (x - i);
    const int b0 = f0 * 2049 + i, b1 = f1 * 2049 + i;
    const float s0 = lv[b0] + (lv[b0 + 1] - lv[b0]) * t;
    if (ff < 1e-4 || f1 == f0) return s0;
    const float s1 = lv[b1] + (lv[b1 + 1] - lv[b1]) * t;
    return s0 + (s1 - s0) * (float) ff;
}
enum Warp { wNone, wSync, wBendP, wBendN, wBendPN, wPWM, wAsymPN, wAsymN, wAsymP, wMirror, wQuant };
static int warpId (const juce::String& m)
{
    if (m.isEmpty() || m == "none") return wNone;
    if (m.containsIgnoreCase ("Sync")) return wSync;
    if (m.containsIgnoreCase ("BendPosNeg") || m.containsIgnoreCase ("BendPN")) return wBendPN;
    if (m.containsIgnoreCase ("BendPos")) return wBendP;
    if (m.containsIgnoreCase ("BendNeg")) return wBendN;
    if (m.containsIgnoreCase ("Bend")) return wBendPN;
    if (m.containsIgnoreCase ("PWM")) return wPWM;
    if (m.containsIgnoreCase ("ASYMPosNeg")) return wAsymPN;
    if (m.containsIgnoreCase ("ASYMNeg")) return wAsymN;
    if (m.containsIgnoreCase ("ASYM") || m.containsIgnoreCase ("PD")) return wAsymP;
    if (m.containsIgnoreCase ("Mirror")) return wMirror;
    if (m.containsIgnoreCase ("Quant")) return wQuant;
    return wNone;
}
static inline double warpPhase (double p, int m, double a)
{
    switch (m)
    {
        case wSync: { const double q = p * (1 + a * 15); return q - std::floor (q); }
        case wBendP: return std::pow (p, 1 + a * 3);
        case wBendN: return 1 - std::pow (1 - p, 1 + a * 3);
        case wBendPN: { const double e = 1 + a * 3; return p < 0.5 ? 0.5 * std::pow (2 * p, e) : 1 - 0.5 * std::pow (2 - 2 * p, e); }
        case wPWM: { const double w = 1 - a * 0.98; return p < w ? p / w : 0; }
        case wAsymPN: { const double d = 0.5 - 0.48 * a; if (p < 0.5) return p < d ? (p / d) * 0.25 : 0.25 + ((p - d) / (0.5 - d)) * 0.25; const double q = p - 0.5; return 0.5 + (q < d ? (q / d) * 0.25 : 0.25 + ((q - d) / (0.5 - d)) * 0.25); }
        case wAsymN: { const double d = 0.5 + 0.48 * a; return p < d ? (p / d) * 0.5 : 0.5 + ((p - d) / (1 - d)) * 0.5; }
        case wAsymP: { const double d = 0.5 - 0.48 * a; return p < d ? (p / d) * 0.5 : 0.5 + ((p - d) / (1 - d)) * 0.5; }
        case wMirror: { const double q = p < 0.5 ? p * 2 : 2 - p * 2; return p * (1 - a) + q * 0.5 * a; }
        case wQuant: { const double st = std::max (2.0, std::round (64 * (1 - a)) + 2); return std::floor (p * st) / st; }
        default: return p;
    }
}
static inline double polyblep (double t, double dt)
{
    if (dt <= 0) return 0;
    if (t < dt) { t /= dt; return t + t - t * t - 1; }
    if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
    return 0;
}

void Engine::renderVoice (Voice& v, float* outL, float* outR, int s0, int len, double dt)
{
    const auto& p = P->p;
    voiceSources (v, dt); voiceMods (v);
    const double* dm = v.dm; const double* S = v.src;
    if (v.pitch != v.target)
    {
        const double g = p.glob.porta > 0 ? std::min (1.0, dt / (p.glob.porta * 0.7)) : 1.0;
        v.pitch += (v.target - v.pitch) * g; if (std::abs (v.target - v.pitch) < 0.001) v.pitch = v.target;
    }
    const double amp = S[2];
    if (! v.gate && v.env[0].st == 0) { v.on = false; return; }
    const double baseNote = v.pitch + dm[70] + bend * p.glob.bend;
    const double nyq = sr / 2;
    double fw[5][2];
    for (int r = 0; r < 5; ++r) { const double b = p.route[(size_t) r].bal; fw[r][0] = (100 - b) / 200; fw[r][1] = (100 + b) / 200; }
    Svf fc[2]; bool fon[2] {}; double fdrive[2] { 1, 1 };
    for (int j = 0; j < 2; ++j)
    {
        const auto& F = p.filt[(size_t) j];
        if (! F.on) continue;
        fon[j] = true;
        double hz = fnorm (F.freq + dm[60 + j * 4]);
        if (F.key != 0) hz *= std::pow (2.0, (v.pitch / 12.0) * F.key);
        const double reso = clampv ((F.reso + dm[61 + j * 4]) / 100.0, 0.0, 1.0);
        const double k = P->fmode[(size_t) j] == FMode::peak ? 2 - 1.9 * reso : 2 - 1.97 * std::pow (reso, 0.8);
        fc[j].set (clampv (hz, 10.0, nyq * 0.94), sr, k);
        fdrive[j] = 1 + clampv ((F.drive + dm[62 + j * 4]) / 100.0, 0.0, 1.0) * (P->fmg[(size_t) j] ? 8 : 5);
    }
    bool oscOn[3] {}; const float* lvA[3] {}; int nA[3] {}; double frames[3] {}; int wm[3] {}; double wa[3] {}; int uniA[3] {}; double volA[3] {};
    for (int o = 0; o < 3; ++o)
    {
        const auto& O = p.osc[(size_t) o]; const auto& T = tables[(size_t) o]; auto& st = oc[(size_t) o];
        if (! O.on) continue;
        if (O.sample) { oscOn[o] = O.sampleData != nullptr && ! O.sampleData->empty(); volA[o] = clampv (O.vol + dm[o * 8], 0.0, 2.0); continue; }
        if (! T) continue;
        oscOn[o] = true;
        const double pitch = baseNote + O.oct * 12 + O.semi + (O.fine + dm[o * 8 + 6]) / 100.0 + dm[o * 8 + 5];
        const double f0 = P->a4 * std::pow (2.0, (pitch - 69) / 12.0);
        const int uni = clampv (O.uni, 1, 16);
        const double det = clampv (O.det + dm[o * 8 + 3], 0.0, 1.0);
        const double stereo = clampv ((O.stereo + dm[o * 8 + 7]) / 100.0, 0.0, 1.0);
        const double pan = clampv (O.pan / 100.0 + dm[o * 8 + 1] / 2, -1.0, 1.0);
        const double vol = clampv (O.vol + dm[o * 8], 0.0, 2.0);
        const double blend = O.blend / 100.0;
        double wsum = 0;
        for (int u = 0; u < uni; ++u)
        {
            const double sp = uni == 1 ? 0 : (double) u / (uni - 1) * 2 - 1;
            st.inc[u] = f0 * std::pow (2.0, sp * det * 100 / 1200) / sr;
            const double pu = clampv (pan + sp * stereo, -1.0, 1.0);
            const double w = uni == 1 ? 1 : (std::abs (sp) < 0.34 ? 1 : 0.4 + blend * 0.9);
            st.gl[u] = std::cos ((pu + 1) * juce::MathConstants<double>::pi / 4) * w; st.gr[u] = std::sin ((pu + 1) * juce::MathConstants<double>::pi / 4) * w; wsum += w * w;
        }
        const double norm = vol / std::sqrt (std::max (1.0, wsum)) * 1.4142;
        for (int u = 0; u < uni; ++u) { st.gl[u] *= norm; st.gr[u] *= norm; }
        const double maxH = nyq / std::max (f0 * (1 + det * 0.06), 1.0);
        wm[o] = warpId (O.warp); wa[o] = clampv (O.warpAmt + dm[o * 8 + 4], 0.0, 1.0);
        int level = (int) std::ceil (std::log2 (1024.0 / std::max (maxH, 1.0)));
        if (wm[o] != wNone && wa[o] > 0) level += 1;
        level = clampv (level, 0, 10);
        lvA[o] = T->lv[(size_t) level].data(); nA[o] = T->n;
        frames[o] = clampv ((O.pos + dm[o * 8 + 2]) / 256.0, 0.0, 1.0) * (T->n - 1);
        uniA[o] = uni;
    }
    const auto& sub = p.sub; const auto& noise = p.noise;
    const double subInc = sub.on ? P->a4 * std::pow (2.0, (baseNote + sub.oct * 12 - 69) / 12.0) / sr : 0;
    const double subPan = clampv (sub.pan / 100.0 + dm[33] / 2, -1.0, 1.0), subVol = clampv (sub.vol + dm[32], 0.0, 2.0);
    const double nzVol = clampv (noise.vol + dm[24], 0.0, 2.0), nzPan = clampv (noise.pan / 100.0 + dm[25] / 2, -1.0, 1.0);
    const double sgl = std::cos ((subPan + 1) * juce::MathConstants<double>::pi / 4) * 1.414, sgr = std::sin ((subPan + 1) * juce::MathConstants<double>::pi / 4) * 1.414;
    const double ngl = std::cos ((nzPan + 1) * juce::MathConstants<double>::pi / 4) * 1.414, ngr = std::sin ((nzPan + 1) * juce::MathConstants<double>::pi / 4) * 1.414;
    const int nType = noise.type == "Brown" ? 1 : (noise.type == "Pink" || noise.type == "Sample") ? 2 : 0;
    bool sendB1 = false, sendB2 = false;
    for (auto& r : p.route) { if (r.bus1 > 0) sendB1 = true; if (r.bus2 > 0) sendB2 = true; }
    sendB1 = sendB1 && ! P->bus1.empty(); sendB2 = sendB2 && ! P->bus2.empty();
    if (sendB1) useB1 = true; if (sendB2) useB2 = true;
    const double ampStart = v.ampPrev < 0 ? amp : v.ampPrev; v.ampPrev = amp;
    double srcL[5], srcR[5];
    for (int i = 0; i < len; ++i)
    {
        const double ai = ampStart + (amp - ampStart) * ((double) i / len);
        for (int o = 0; o < 3; ++o)
        {
            srcL[o] = srcR[o] = 0;
            if (! oscOn[o]) continue;
            const auto& O = p.osc[(size_t) o];
            if (O.sample)
            {
                const auto& sd = *O.sampleData;
                const double rate = (O.sampleRate / sr) * std::pow (2.0, (baseNote + O.oct * 12 + O.semi - O.sampleRoot) / 12.0);
                const int j = (int) v.samplePos; const double t = v.samplePos - j;
                const double x = j + 1 < (int) sd.size() ? sd[(size_t) j] + (sd[(size_t) j + 1] - sd[(size_t) j]) * t : 0;
                v.samplePos += rate;
                srcL[o] = srcR[o] = x * volA[o];
                continue;
            }
            const auto& st = oc[(size_t) o]; double* ph = v.ph[o];
            double l = 0, r = 0;
            for (int u = 0; u < uniA[o]; ++u)
            {
                double q = ph[u] + st.inc[u]; if (q >= 1) q -= 1; ph[u] = q;
                const float x = lookup (lvA[o], nA[o], frames[o], wa[o] > 0 ? warpPhase (q, wm[o], wa[o]) : q);
                l += x * st.gl[u]; r += x * st.gr[u];
            }
            srcL[o] = l; srcR[o] = r;
        }
        if (noise.on)
        {
            const double w = rnd() * 2 - 1; double x = w; double* z = v.nz;
            if (nType == 1) { z[0] = (z[0] + w * 0.05) * 0.995; x = z[0] * 3; }
            else if (nType == 2) { z[1] = 0.99765 * z[1] + w * 0.099; z[2] = 0.963 * z[2] + w * 0.2965; z[3] = 0.57 * z[3] + w * 1.0527; x = (z[1] + z[2] + z[3] + w * 0.1848) * 0.25; }
            else if (noise.color != 0.5f) { const double c = noise.color; z[4] += (w - z[4]) * (0.05 + c * 0.95); x = c < 0.5 ? z[4] : w - z[4] * (1 - c); }
            srcL[3] = x * nzVol * ngl * 0.5; srcR[3] = x * nzVol * ngr * 0.5;
        }
        else srcL[3] = srcR[3] = 0;
        if (sub.on)
        {
            v.subPh += subInc; if (v.subPh >= 1) v.subPh -= 1;
            const double q = v.subPh; double x;
            if (sub.shape == "sine") x = std::sin (TAU * q);
            else if (sub.shape == "tri") x = q < 0.25 ? 4 * q : q < 0.75 ? 2 - 4 * q : 4 * q - 4;
            else if (sub.shape == "saw") { x = 2 * q - 1; x -= polyblep (q, subInc); }
            else { x = q < 0.5 ? 1 : -1; x += polyblep (q, subInc); x -= polyblep (std::fmod (q + 0.5, 1.0), subInc); }
            srcL[4] = x * subVol * sgl * 0.7; srcR[4] = x * subVol * sgr * 0.7;
        }
        else srcL[4] = srcR[4] = 0;
        double f1l = 0, f1r = 0, f2l = 0, f2r = 0, ml = 0, mr = 0, dl = 0, dr = 0, b1l = 0, b1r = 0, b2l = 0, b2r = 0;
        for (int k = 0; k < 5; ++k)
        {
            const double l = srcL[k], r = srcR[k]; if (l == 0 && r == 0) continue;
            const auto& R0 = p.route[(size_t) k];
            if (R0.dest == 0) { f1l += l * fw[k][0]; f1r += r * fw[k][0]; f2l += l * fw[k][1]; f2r += r * fw[k][1]; }
            else if (R0.dest == 2) { dl += l; dr += r; }
            else { ml += l; mr += r; }
            if (R0.bus1 > 0) { b1l += l * R0.bus1 / 100; b1r += r * R0.bus1 / 100; }
            if (R0.bus2 > 0) { b2l += l * R0.bus2 / 100; b2r += r * R0.bus2 / 100; }
        }
        for (int j = 0; j < 2; ++j)
        {
            double xl = j ? f2l : f1l, xr = j ? f2r : f1r;
            if (fon[j])
            {
                double* st = v.fs[j]; double* st2 = v.fs[j + 2]; const auto md = P->fmode[(size_t) j]; const double drv = fdrive[j];
                if (drv > 1.01) { xl = std::tanh (xl * drv) / std::sqrt (drv); xr = std::tanh (xr * drv) / std::sqrt (drv); }
                xl = svfRun (st, xl, fc[j], md); xr = svfRun (st + 4, xr, fc[j], md);
                if (P->ftwo[(size_t) j] && md != FMode::comb) { xl = svfRun (st2, xl, fc[j], md); xr = svfRun (st2 + 4, xr, fc[j], md); }
            }
            ml += xl; mr += xr;
        }
        const int idx = s0 + i; const double g = ai * 0.5;
        outL[idx] += (float) (ml * g); outR[idx] += (float) (mr * g);
        dL[(size_t) idx] += (float) (dl * g); dR[(size_t) idx] += (float) (dr * g);
        if (sendB1) { b1L[(size_t) idx] += (float) (b1l * g); b1R[(size_t) idx] += (float) (b1r * g); }
        if (sendB2) { b2L[(size_t) idx] += (float) (b2l * g); b2R[(size_t) idx] += (float) (b2r * g); }
    }
}

void Engine::process (float* L, float* R, int n)
{
    std::fill (L, L + n, 0.0f); std::fill (R, R + n, 0.0f);
    if (! P) return;
    if ((int) dL.size() < n) return; // processor guarantees blocks <= 8192
    const auto& p = P->p;
    if (p.arp.on) arpTick (n);
    for (int s0 = 0; s0 < n; s0 += CTRL)
    {
        const int len = std::min (CTRL, n - s0); const double dt = len / sr;
        globalLfos (dt);
        for (auto& v : voices) if (v.on) renderVoice (v, L, R, s0, len, dt);
    }
    globalMods();
    for (auto& f : P->fx) runFx (*f, L, R, n);
    if (useB1) { for (auto& f : P->bus1) runFx (*f, b1L.data(), b1R.data(), n); for (int i = 0; i < n; ++i) { L[i] += b1L[(size_t) i]; R[i] += b1R[(size_t) i]; } }
    if (useB2) { for (auto& f : P->bus2) runFx (*f, b2L.data(), b2R.data(), n); for (int i = 0; i < n; ++i) { L[i] += b2L[(size_t) i]; R[i] += b2R[(size_t) i]; } }
    const double mv = std::max (0.0, p.glob.vol + gVol) * 2 * outGain;
    float pk = 0;
    for (int i = 0; i < n; ++i)
    {
        double l = (L[i] + dL[(size_t) i]) * mv, r = (R[i] + dR[(size_t) i]) * mv;
        const double a = std::max (std::abs (l), std::abs (r));
        const double tgt = a > 0.98 ? 0.98 / a : 1;
        lim = tgt < lim ? tgt : lim + (1 - lim) * 0.0003;
        l *= lim; r *= lim;
        if (! std::isfinite (l)) l = 0; if (! std::isfinite (r)) r = 0;
        L[i] = (float) l; R[i] = (float) r; if (a > pk) pk = (float) a;
    }
    meter = std::max (pk, meter * 0.9f);
    std::fill (dL.begin(), dL.begin() + n, 0.0f); std::fill (dR.begin(), dR.begin() + n, 0.0f);
    std::fill (b1L.begin(), b1L.begin() + n, 0.0f); std::fill (b1R.begin(), b1R.begin() + n, 0.0f);
    std::fill (b2L.begin(), b2L.begin() + n, 0.0f); std::fill (b2R.begin(), b2R.begin() + n, 0.0f);
    useB1 = useB2 = false;
    // visuals
    for (int i = 0; i < 10; ++i) visLfoPh[i].store ((float) ((last && last->on && p.lfo[(size_t) i].mode != "Free") ? last->lfoPh[i] : gLfoPh[i]));
    for (int i = 0; i < 4; ++i) visEnv[i].store ((float) (last && last->on ? last->src[2 + i] : 0));
}

void Engine::runFx (Fx& fx, float* L, float* R, int n)
{
    if (! fx.on || ! fx.st) return;
    auto& st = *fx.st; const int t = fx.type;
    auto P_ = [&] (const char* k, double d) { return (double) fx.P (k, (float) d); };
    if (t == 0)
    {
        const juce::String mode = fx.mode.isEmpty() ? "kTube" : fx.mode;
        const double drive = clampv (P_ ("kParamDrive", 25), 0.0, 100.0) / 100, wet = clampv (P_ ("kParamWet", 100), 0.0, 100.0) / 100, out = lvl (P_ ("kParamLevelOut", 0.5));
        const double g = dbg (drive * 36);
        const bool useF = fx.has ("kParamLPHP") || fx.has ("kParamFreq");
        const double lphp = P_ ("kParamLPHP", 0);
        if (useF) st.preF.set (lphp > 50 ? "hp" : "lp", fnorm (P_ ("kParamFreq", 0.5)) * (lphp > 50 ? 0.25 : 1), 0.707, 0, sr);
        const int hold = std::max (1, (int) std::lround (1 + drive * 40));
        int m = 0;
        if (mode == "kHardClip") m = 1; else if (mode == "kSoftClip") m = 2; else if (mode == "kAsym") m = 3; else if (mode == "kSine") m = 4; else if (mode == "kLinFold") m = 5;
        else if (mode == "kZeroSquare") m = 6; else if (mode == "kRectify") m = 7; else if (mode == "kDownsample") m = 8; else if (mode == "kQuant") m = 9;
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < 2; ++c)
            {
                float* A = c ? R : L; double x = A[i], y;
                if (useF && lphp < 50) x = st.preF.run ((float) x, c);
                const double xi = x * g;
                switch (m)
                {
                    case 1: y = clampv (xi, -1.0, 1.0); break;
                    case 2: y = xi / (1 + std::abs (xi)); break;
                    case 3: y = std::tanh (xi + 0.3) - std::tanh (0.3); break;
                    case 4: y = std::sin (xi * 1.5); break;
                    case 5: { double v = (xi + 1) * 0.25; v -= std::floor (v); y = std::abs (v * 4 - 2) - 1; break; }
                    case 6: y = std::tanh (xi * std::abs (xi) + xi * 0.2); break;
                    case 7: y = std::abs (std::tanh (xi)) * 2 - 0.5; break;
                    case 8: if (c == 0) st.cnt++; if (st.cnt % hold == 0) st.hold[c] = (float) x; y = st.hold[c]; break;
                    case 9: { const double q = std::pow (2.0, 16 - drive * 14); y = std::round (x * q) / q; break; }
                    default: y = std::tanh (xi);
                }
                if (useF && lphp >= 50) y = st.preF.run ((float) y, c);
                A[i] = (float) ((x * (1 - wet) + y * wet) * out);
            }
    }
    else if (t == 1 || t == 3 || t == 9)
    {
        double rate, depth, base, fb, wet; int voicesN = 2;
        if (t == 1) { rate = P_ ("kParamRate", 0.3); depth = clampv (P_ ("kParamWidth", 100), 0.0, 400.0) / 400 * 0.004 * sr; base = 0.0008 * sr; fb = clampv (P_ ("kParamFeedback", 50), 0.0, 98.0) / 100; wet = clampv (P_ ("kParamWet", 100), 0.0, 100.0) / 100; }
        else if (t == 3) { rate = P_ ("kParamRate", 0.8); depth = clampv (P_ ("kParamDepth", 26), 0.0, 100.0) / 100 * 0.006 * sr; base = clampv (P_ ("kParamDelay", 8), 1.0, 40.0) / 1000 * sr; fb = clampv (P_ ("kParamFeedback", 0), 0.0, 95.0) / 100; wet = clampv (P_ ("kParamWet", 50), 0.0, 100.0) / 100; }
        else { voicesN = clampv ((int) std::lround (P_ ("kParamUnison", 4)), 1, 7); rate = 0.15 + clampv (P_ ("kParamRate", 30), 0.0, 100.0) / 100 * 2.5; depth = clampv (P_ ("kParamDetune", 25), 0.0, 100.0) / 100 * 0.004 * sr; base = 0.012 * sr; fb = 0; wet = clampv (P_ ("kParamWet", 50), 0.0, 100.0) / 100; const double dw = clampv (P_ ("kParamDimEWet", 0), 0.0, 100.0) / 100; wet = std::min (1.0, wet + dw * 0.5); }
        const double inc = rate / sr;
        for (int i = 0; i < n; ++i)
        {
            st.ph += inc; if (st.ph >= 1) st.ph -= 1;
            const float l = L[i], r = R[i];
            st.dl[0].push ((float) (l + st.fb[0] * fb)); st.dl[1].push ((float) (r + st.fb[1] * fb));
            double ol = 0, orr = 0;
            for (int v = 0; v < voicesN; ++v)
            {
                const double o = (double) v / voicesN;
                ol += st.dl[0].read (base + depth * (0.5 + 0.5 * std::sin (TAU * (st.ph + o))));
                orr += st.dl[1].read (base + depth * (0.5 + 0.5 * std::sin (TAU * (st.ph + o + 0.25))));
            }
            ol /= voicesN; orr /= voicesN;
            if (t == 9) { const double s = ol; ol = ol * 0.8 + orr * 0.2; orr = orr * 0.8 + s * 0.2; }
            st.fb[0] = (float) ol; st.fb[1] = (float) orr;
            if (t == 1) { L[i] = (float) (l * (1 - wet) + ol * wet); R[i] = (float) (r * (1 - wet) + orr * wet); }
            else { L[i] = (float) (l * (1 - wet * 0.5) + ol * wet); R[i] = (float) (r * (1 - wet * 0.5) + orr * wet); }
        }
    }
    else if (t == 2)
    {
        const double f0 = clampv (P_ ("kParamFreq", 800), 20.0, 18000.0), rate = P_ ("kParamRate", 0.3), depth = clampv (P_ ("kParamDepth", 50), 0.0, 100.0) / 100;
        const double fb = clampv (P_ ("kParamFeedback", 40), 0.0, 95.0) / 100, wet = clampv (P_ ("kParamWet", 50), 0.0, 100.0) / 100, inc = rate / sr;
        for (int i = 0; i < n; ++i)
        {
            st.ph += inc; if (st.ph >= 1) st.ph -= 1;
            for (int c = 0; c < 2; ++c)
            {
                float* A = c ? R : L; const double x = A[i];
                const double f = f0 * std::pow (2.0, depth * 3 * std::sin (TAU * (st.ph + c * 0.25)));
                const double tn = std::tan (juce::MathConstants<double>::pi * std::min (f, sr * 0.45) / sr), a = (tn - 1) / (tn + 1);
                double y = x + st.fb[c] * fb; double* z = st.ap[c];
                for (int s = 0; s < 6; ++s) { const double o = a * y + z[s]; z[s] = y - a * o; y = o; }
                st.fb[c] = (float) y;
                A[i] = (float) (x * (1 - wet * 0.5) + y * wet * 0.7);
            }
        }
    }
    else if (t == 4)
    {
        const double tl = clampv (P_ ("kParamTimeL", 0.25), 0.001, 3.9), tr = clampv (P_ ("kParamTimeR", fx.has ("kParamTimeL") ? P_ ("kParamTimeL", 0.25) : 0.25), 0.001, 3.9);
        const double fb = clampv (P_ ("kParamFeedback", 40), 0.0, 98.0) / 100, wet = clampv (P_ ("kParamWet", 30), 0.0, 100.0) / 100;
        const int mode = (int) P_ ("kParamMode", fx.mode.getIntValue());
        const double fq = clampv (P_ ("kParamFreq", 2000), 40.0, 18000.0), bw = clampv (P_ ("kParamBW", 4), 0.3, 10.0);
        st.bq.set ("bp", fq, 1.4 / bw * 2, 0, sr);
        const double dl = std::min (tl * sr, (double) st.dl[0].n - 2), dr = std::min (tr * sr, (double) st.dl[1].n - 2);
        for (int i = 0; i < n; ++i)
        {
            const float l = L[i], r = R[i];
            const float yl = st.dl[0].read (dl), yr = st.dl[1].read (dr);
            const float fl = st.bq.run (yl, 0) * 0.6f + yl * 0.4f, fr = st.bq.run (yr, 1) * 0.6f + yr * 0.4f;
            if (mode == 1) { st.dl[0].push ((float) ((l + r) * 0.5 + fr * fb)); st.dl[1].push ((float) (fl * fb)); }
            else { st.dl[0].push ((float) (l + fl * fb)); st.dl[1].push ((float) (r + fr * fb)); }
            L[i] = (float) (l + yl * wet); R[i] = (float) (r + yr * wet);
        }
    }
    else if (t == 5)
    {
        const double thr = (clampv (P_ ("kParamThresh", 0.5), 0.0, 1.0) - 1) * 48;
        const double ratio = std::max (1.0, P_ ("kParamRatio", 4)), up = clampv (P_ ("kParamRatioBelow", 0.66), 0.0, 1.0);
        const double atk = std::exp (-1 / (std::max (0.1, P_ ("kParamAttack", 3)) * 0.001 * sr)), rel = std::exp (-1 / (std::max (1.0, P_ ("kParamRelease", 80)) * 0.001 * sr));
        const double mk = dbg (P_ ("kParamMakeup", 0)), wet = clampv (P_ ("kParamWet", 100), 0.0, 100.0) / 100, out = lvl (P_ ("kParamLevelOut", 0.5));
        const bool mb = P_ ("kParamMultiband", 0) != 0;
        const double gains[3] = { dbg (P_ ("kParamGain1", 0)), dbg (P_ ("kParamGain2", 0)), dbg (P_ ("kParamGain3", 0)) };
        if (mb) { st.xo[0].set ("lp", 120, 0.707, 0, sr); st.xo[1].set ("hp", 120, 0.707, 0, sr); st.xo[2].set ("lp", 2500, 0.707, 0, sr); st.xo[3].set ("hp", 2500, 0.707, 0, sr); }
        auto comp = [&] (int b, double xin)
        {
            const double lv = std::abs (xin) + 1e-9;
            const double c = lv > st.env[b] ? atk : rel; st.env[b] = c * st.env[b] + (1 - c) * lv;
            const double e = 20 * std::log10 (st.env[b] + 1e-9);
            double gdb = 0;
            if (e > thr) gdb = (thr + (e - thr) / ratio) - e;
            else if (mb) gdb = std::min (18.0, (thr - e) * up * 0.5);
            return dbg (gdb);
        };
        for (int i = 0; i < n; ++i)
        {
            const float l = L[i], r = R[i];
            if (! mb) { const double g = comp (0, std::max (std::abs (l), std::abs (r))) * mk; L[i] = (float) ((l * (1 - wet) + l * g * wet) * out); R[i] = (float) ((r * (1 - wet) + r * g * wet) * out); continue; }
            const float lo0 = st.xo[0].run (l, 0), lo1 = st.xo[0].run (r, 1), re0 = st.xo[1].run (l, 0), re1 = st.xo[1].run (r, 1);
            const float mi0 = st.xo[2].run (re0, 0), mi1 = st.xo[2].run (re1, 1), hi0 = st.xo[3].run (re0, 0), hi1 = st.xo[3].run (re1, 1);
            const double g0 = comp (0, std::max (std::abs (lo0), std::abs (lo1))) * gains[0];
            const double g1 = comp (1, std::max (std::abs (mi0), std::abs (mi1))) * gains[1];
            const double g2 = comp (2, std::max (std::abs (hi0), std::abs (hi1))) * gains[2];
            const double yl = (lo0 * g0 + mi0 * g1 + hi0 * g2) * mk, yr = (lo1 * g0 + mi1 * g1 + hi1 * g2) * mk;
            L[i] = (float) ((l * (1 - wet) + yl * wet) * out); R[i] = (float) ((r * (1 - wet) + yr * wet) * out);
        }
    }
    else if (t == 6)
    {
        const juce::String type = fx.mode.isEmpty() ? "kHall" : fx.mode;
        const double size = clampv (P_ ("kParamSize", 50), 0.0, 100.0) / 100, decay = clampv (P_ ("kParamDelay", 50), 0.0, 100.0) / 100;
        const double damp = clampv (P_ ("kParamFreq", 40), 0.0, 100.0) / 100, lowcut = clampv (P_ ("kParamFreqB", 0), 0.0, 100.0) / 100;
        const double wet = clampv (P_ ("kParamWet", 20), 0.0, 100.0) / 100, width = clampv (P_ ("kParamWidth", 100), 0.0, 100.0) / 100, out = lvl (P_ ("kParamLevelOut", 0.5));
        const double pre = clampv (P_ ("kParamPreDelay", 0), 0.0, 0.45) * sr;
        double rt = 0.4 + decay * decay * 9; if (type.contains ("Abyss")) rt *= 2.5; if (type.contains ("Plate")) rt *= 0.8;
        const double sc = 0.5 + size * 1.6;
        double dlen[8], g[8];
        for (int k = 0; k < 8; ++k) { dlen[k] = std::min (st.base[(size_t) k] * sc, (double) st.lines[(size_t) k].n - 10); g[k] = std::pow (10.0, (-3 * dlen[k]) / (rt * sr)); }
        const double dc = 0.05 + damp * 0.9;
        st.hp.set ("hp", 20 + lowcut * lowcut * 900, 0.7, 0, sr);
        double tmp[8];
        for (int i = 0; i < n; ++i)
        {
            const float l = L[i], r = R[i];
            st.pre[0].push (l); st.pre[1].push (r);
            const double xl = pre > 1 ? st.pre[0].read (pre) : l, xr = pre > 1 ? st.pre[1].read (pre) : r;
            double x = (xl + xr) * 0.5;
            for (int k = 0; k < 4; ++k) { auto& d = st.diff[(size_t) k]; const double y = d.read (std::min ((double) st.dlen[(size_t) k], (double) d.n - 2)); const double v = x + y * 0.6; d.push ((float) v); x = y - v * 0.6; }
            st.mph += 0.6 / sr;
            for (int k = 0; k < 8; ++k) tmp[k] = st.lines[(size_t) k].read (dlen[k] + ((k & 1) ? 1 : -1) * 6 * std::sin (TAU * (st.mph + k * 0.13)));
            const double s = (tmp[0] + tmp[1] + tmp[2] + tmp[3] + tmp[4] + tmp[5] + tmp[6] + tmp[7]) * 0.25;
            double ol = 0, orr = 0;
            for (int k = 0; k < 8; ++k)
            {
                double y = (tmp[k] - s) * g[k];
                st.lp[(size_t) k] += (y - st.lp[(size_t) k]) * (1 - dc); y = st.lp[(size_t) k];
                st.lines[(size_t) k].push ((float) (x * 0.35 + y));
                if (k & 1) orr += tmp[k]; else ol += tmp[k];
            }
            ol = st.hp.run ((float) (ol * 0.3), 0); orr = st.hp.run ((float) (orr * 0.3), 1);
            const double m = (ol + orr) * 0.5, sd = (ol - orr) * 0.5 * width;
            L[i] = (float) ((l * (1 - wet * 0.3) + (m + sd) * wet * 1.6) * out); R[i] = (float) ((r * (1 - wet * 0.3) + (m - sd) * wet * 1.6) * out);
        }
    }
    else if (t == 7)
    {
        auto ty = [&] (const char* key, int band) { const int v = (int) P_ (key, 0); return v == 1 ? "peak" : v == 2 ? (band == 1 ? "hp" : "lp") : (band == 1 ? "ls" : "hs"); };
        st.b1.set (ty ("kParamType1", 1), P_ ("kParamFreq1", 200), 0.4 + clampv (P_ ("kParamReso1", 30), 0.0, 100.0) / 100 * 6, P_ ("kParamGain1", 0), sr);
        st.b2.set (ty ("kParamType2", 2), P_ ("kParamFreq2", 3000), 0.4 + clampv (P_ ("kParamReso2", 30), 0.0, 100.0) / 100 * 6, P_ ("kParamGain2", 0), sr);
        const double out = lvl (P_ ("kParamLevelOut", 0.5));
        for (int i = 0; i < n; ++i) { L[i] = (float) (st.b2.run (st.b1.run (L[i], 0), 0) * out); R[i] = (float) (st.b2.run (st.b1.run (R[i], 1), 1) * out); }
    }
    else if (t == 8)
    {
        const juce::String ft = fx.mode.isEmpty() ? "L12" : fx.mode;
        const auto mode = classify (ft); const bool two = slopeOf (ft) == 2;
        const double k = 2 - 1.96 * clampv (P_ ("kParamReso", 10), 0.0, 100.0) / 100;
        const double drive = 1 + clampv (P_ ("kParamDrive", 0), 0.0, 100.0) / 100 * 6, wet = clampv (P_ ("kParamWet", 100), 0.0, 100.0) / 100, out = lvl (P_ ("kParamLevelOut", 0.5));
        Svf c; c.set (fnorm (P_ ("kParamFreq", 0.5)), sr, k);
        for (int i = 0; i < n; ++i)
            for (int ch = 0; ch < 2; ++ch)
            {
                float* A = ch ? R : L; const double x = A[i]; double y;
                if (mode == FMode::comb) y = svfRun (st.fs + ch * 4, x, c, FMode::notch);
                else { const double xi = drive > 1.01 ? std::tanh (x * drive) / std::sqrt (drive) : x; y = svfRun (st.fs + ch * 4, xi, c, mode); if (two) y = svfRun (st.fs2 + ch * 4, y, c, mode); }
                A[i] = (float) ((x * (1 - wet) + y * wet) * out);
            }
    }
    else if (t == 12)
    {
        const double hp = P_ ("kParamHPF", 0), g = lvl (P_ ("kParamLevelOut", 0.5));
        if (hp > 15) st.hp.set ("hp", hp, 0.707, 0, sr);
        for (int i = 0; i < n; ++i) { float l = L[i], r = R[i]; if (hp > 15) { l = st.hp.run (l, 0); r = st.hp.run (r, 1); } L[i] = (float) (l * g); R[i] = (float) (r * g); }
    }
}
}
