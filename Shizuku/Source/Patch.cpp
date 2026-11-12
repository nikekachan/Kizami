#include "Patch.h"
#include <stdexcept>
#include <cstring>
#include <cmath>

extern "C" {
size_t ZSTD_decompress (void* dst, size_t dstCapacity, const void* src, size_t compressedSize);
unsigned ZSTD_isError (size_t code);
unsigned long long ZSTD_getFrameContentSize (const void* src, size_t srcSize);
}

namespace shz
{
static juce::String U (const char* s) { return juce::String::fromUTF8 (s); }

Patch::Patch()
{
    osc[0].on = true;
    for (int i = 0; i < 3; ++i) osc[(size_t) i].tablePath = "S2 Tables/Default Shapes.wav";
    route[0].dest = 0;
    for (int i = 0; i < 8; ++i) macro[(size_t) i].name = "Macro " + juce::String (i + 1);
}

juce::String srcName (int id)
{
    switch (id)
    {
        case 0: return U ("なし");
        case 1: return "Mod Wheel";
        case 16: return "Velocity";
        case 17: return "Note";
        case 18: return "Aftertouch";
        case 19: return "Pitch Bend";
        case 20: return "Random";
        default: break;
    }
    if (id >= 2 && id <= 5) return "ENV " + juce::String (id - 1);
    if (id >= 6 && id <= 15) return "LFO " + juce::String (id - 5);
    if (id >= 25 && id <= 32) return "Macro " + juce::String (id - 24);
    return "#" + juce::String (id);
}

const char* fxName (int t)
{
    static const char* n[] = { "Distortion", "Flanger", "Phaser", "Chorus", "Delay", "Compressor", "Reverb", "EQ", "Filter", "Hyper/Dimension", "Bode", "Convolve", "Utility" };
    return (t >= 0 && t <= 12) ? n[t] : "?";
}

float fxParam (const FxUnit& f, const char* key, float def)
{
    if (auto* v = f.p.getVarPointer (juce::Identifier (key)))
        if (v->isDouble() || v->isInt() || v->isInt64() || v->isBool()) return (float) (double) *v;
    return def;
}

//==============================================================================
// Minimal CBOR → juce::var. Long float arrays become a MemoryBlock of float32.
namespace
{
struct Cbor
{
    const uint8_t* b; size_t n, p = 0;
    uint8_t byte() { if (p >= n) throw std::runtime_error ("cbor eof"); return b[p++]; }
    uint64_t be (int k) { uint64_t v = 0; for (int i = 0; i < k; ++i) v = (v << 8) | byte(); return v; }
    int64_t len (int ai)
    {
        if (ai < 24) return ai;
        if (ai == 24) return (int64_t) be (1);
        if (ai == 25) return (int64_t) be (2);
        if (ai == 26) return (int64_t) be (4);
        if (ai == 27) return (int64_t) be (8);
        if (ai == 31) return -1;
        throw std::runtime_error ("cbor ai");
    }
    static double half (uint16_t h)
    {
        const int s = h >> 15 ? -1 : 1, e = (h >> 10) & 31, f = h & 1023;
        if (e == 0) return s * f * std::pow (2.0, -24);
        if (e == 31) return f ? std::nan ("") : s * INFINITY;
        return s * (1 + f / 1024.0) * std::pow (2.0, e - 15);
    }
    bool numericAt() const
    {
        if (p >= n) return false;
        const uint8_t c = b[p];
        return c == 0xfa || c == 0xfb || c == 0xf9 || c <= 0x17 || (c >= 0x20 && c <= 0x37);
    }
    juce::var item()
    {
        const uint8_t c = byte(); const int mt = c >> 5, ai = c & 31;
        switch (mt)
        {
            case 0: return juce::var ((juce::int64) len (ai));
            case 1: return juce::var ((juce::int64) (-1 - len (ai)));
            case 2: { auto k = (size_t) len (ai); if (p + k > n) throw std::runtime_error ("cbor"); juce::MemoryBlock m (b + p, k); p += k; return juce::var (m); }
            case 3: { auto k = (size_t) len (ai); if (p + k > n) throw std::runtime_error ("cbor"); auto s = juce::String::fromUTF8 ((const char*) b + p, (int) k); p += k; return s; }
            case 4:
            {
                const int64_t k = len (ai);
                if (k > 64 && numericAt())
                {
                    juce::MemoryBlock m ((size_t) k * sizeof (float));
                    auto* f = (float*) m.getData();
                    for (int64_t i = 0; i < k; ++i) f[i] = (float) (double) item();
                    juce::DynamicObject::Ptr o = new juce::DynamicObject();
                    o->setProperty ("__floats", juce::var (m));
                    return juce::var (o.get());
                }
                juce::Array<juce::var> a;
                if (k < 0) { while (p < n && b[p] != 0xff) a.add (item()); ++p; }
                else for (int64_t i = 0; i < k; ++i) a.add (item());
                return a;
            }
            case 5:
            {
                const int64_t k = len (ai);
                juce::DynamicObject::Ptr o = new juce::DynamicObject();
                auto one = [&] { auto key = item().toString(); o->setProperty (key, item()); };
                if (k < 0) { while (p < n && b[p] != 0xff) one(); ++p; }
                else for (int64_t i = 0; i < k; ++i) one();
                return juce::var (o.get());
            }
            case 6: len (ai); return item();
            case 7:
                if (ai == 20) return false;
                if (ai == 21) return true;
                if (ai == 22 || ai == 23) return {};
                if (ai == 25) return half ((uint16_t) be (2));
                if (ai == 26) { uint32_t u = (uint32_t) be (4); float f; std::memcpy (&f, &u, 4); return (double) f; }
                if (ai == 27) { uint64_t u = be (8); double d; std::memcpy (&d, &u, 8); return d; }
                return {};
        }
        return {};
    }
};

const juce::var& get (const juce::var& o, const char* k) { static juce::var none; if (auto* d = o.getDynamicObject()) if (auto* v = d->getProperties().getVarPointer (juce::Identifier (k))) return *v; return none; }
juce::var pp (const juce::var& o) { return get (o, "plainParams"); }
float num (const juce::var& o, const char* k, float d) { const auto& v = get (o, k); if (v.isDouble() || v.isInt() || v.isInt64() || v.isBool()) { const double x = v; return std::isfinite (x) ? (float) x : d; } return d; }
juce::String str (const juce::var& o, const char* k, const juce::String& d = {}) { const auto& v = get (o, k); return v.isString() ? v.toString() : d; }
std::shared_ptr<std::vector<float>> floats (const juce::var& v)
{
    if (auto* m = get (v, "__floats").getBinaryData())
    {
        auto out = std::make_shared<std::vector<float>> (m->getSize() / sizeof (float));
        std::memcpy (out->data(), m->getData(), out->size() * sizeof (float));
        return out;
    }
    if (auto* a = v.getArray())
    {
        auto out = std::make_shared<std::vector<float>>();
        for (auto& x : *a) out->push_back ((float) (double) x);
        return out;
    }
    return {};
}

std::vector<FxUnit> rack (const juce::var& r)
{
    std::vector<FxUnit> out;
    if (auto* a = get (r, "FX").getArray())
        for (auto& fx : *a)
        {
            FxUnit u; u.type = (int) num (fx, "type", 0); u.name = fxName (u.type);
            juce::var body;
            if (auto* d = fx.getDynamicObject())
                for (auto& kv : d->getProperties())
                    if (kv.name.toString().startsWith ("FX")) body = kv.value;
            if (auto* d = pp (body).getDynamicObject()) u.p = d->getProperties();
            u.on = fxParam (u, "kParamEnable", 1) != 0;
            out.push_back (std::move (u));
        }
    return out;
}
}

Patch readSerumPreset (const juce::MemoryBlock& file)
{
    const auto* u8 = (const uint8_t*) file.getData();
    const size_t n = file.getSize();
    if (n < 30 || std::memcmp (u8, "XferJson", 8) != 0) throw std::runtime_error ("Serum 2のプリセット(.SerumPreset)ではありません");
    uint64_t jl = 0; for (int i = 0; i < 8; ++i) jl |= (uint64_t) u8[9 + i] << (8 * i);
    size_t p = 17;
    if (p + jl + 8 > n) throw std::runtime_error ("ファイルが壊れています");
    const auto meta = juce::JSON::parse (juce::String::fromUTF8 ((const char*) u8 + p, (int) jl));
    p += (size_t) jl;
    uint32_t raw = 0; for (int i = 0; i < 4; ++i) raw |= (uint32_t) u8[p + (size_t) i] << (8 * i);
    p += 8;
    const auto fcs = ZSTD_getFrameContentSize (u8 + p, n - p);
    const size_t cap = (fcs != 0ULL && fcs < (1ULL << 31) && fcs < ~0ULL - 1) ? (size_t) fcs : (size_t) raw;
    std::vector<uint8_t> out (cap);
    const size_t got = ZSTD_decompress (out.data(), out.size(), u8 + p, n - p);
    if (ZSTD_isError (got)) throw std::runtime_error ("圧縮データを展開できません");
    Cbor c { out.data(), got };
    const juce::var d = c.item();

    Patch P;
    P.source = "serum";
    P.name = str (meta, "presetName", str (d, "presetName", "Serum preset"));
    P.author = str (meta, "presetAuthor");
    P.desc = str (meta, "presetDescription");
    for (int i = 0; i < 3; ++i)
    {
        const auto& O = get (d, ("Oscillator" + std::to_string (i)).c_str());
        const auto q = pp (O);
        const auto& W = get (O, ("WTOsc" + std::to_string (i)).c_str());
        const auto w = pp (W);
        auto& o = P.osc[(size_t) i];
        o.on = num (q, "kParamEnable", i == 0 ? 1.0f : 0.0f) != 0;
        o.oct = num (q, "kParamOctave", 0); o.semi = num (q, "kParamPitch", 0); o.fine = num (q, "kParamFine", 0);
        o.vol = num (q, "kParamVolume", 0.5625f); o.pan = num (q, "kParamPan", 0);
        o.uni = juce::jmax (1, (int) std::lround (num (q, "kParamUnison", 1)));
        o.det = num (q, "kParamDetune", 0.25f); o.blend = num (q, "kParamDetuneWid", 75); o.stereo = num (q, "kParamUnisonStereo", 100);
        o.pos = num (w, "kParamTablePos", 0); o.warp = str (w, "kParamWarpMenu", "none"); o.warpAmt = num (w, "kParamWarp", 0);
        o.phase = num (w, "kParamInitialPhase", 0); o.rand = num (w, "kParamRandomPhase", 100);
        o.tablePath = str (W, "relativePathToWT");
        o.tableName = str (W, "tableDisplayName");
        if (o.tableName.isEmpty()) o.tableName = o.tablePath.isNotEmpty() ? o.tablePath.fromLastOccurrenceOf ("/", false, false).upToLastOccurrenceOf (".", false, false) : juce::String ("Custom");
        if (auto e = floats (get (W, "embeddedWTData")); e && e->size() >= 256) o.embedded = e;
        const auto type = str (q, "kParamType");
        if (type == "kOsc_Sample")
        {
            const auto& S = get (O, ("SampleOsc" + std::to_string (i)).c_str());
            o.sample = true; o.samplePath = str (S, "samplePath"); o.sampleRoot = num (S, "baseNote", 60);
            o.tableName = o.samplePath.fromLastOccurrenceOf ("/", false, false).fromLastOccurrenceOf ("\\", false, false);
        }
        else if (type.isNotEmpty() && type != "kOsc_WT")
            P.missing.add ("OSC " + juce::String::charToString ((juce::juce_wchar) ('A' + i)) + ": " + type.replace ("kOsc_", "") + U (" モードはWavetableで代用"));
    }
    const auto& O3 = get (d, "Oscillator3"); const auto& N = get (O3, "NoiseOsc3");
    P.noise.on = num (pp (O3), "kParamEnable", 0) != 0; P.noise.vol = num (pp (O3), "kParamVolume", 0.5f); P.noise.pan = num (pp (O3), "kParamPan", 0);
    P.noise.samplePath = str (N, "relativePathToNoiseSample");
    P.noise.type = str (pp (N), "kParamNoiseType", P.noise.samplePath.isNotEmpty() ? "Sample" : "White");
    P.noise.color = num (pp (N), "kParamColor", 0.5f);
    const auto& O4 = get (d, "Oscillator4"); const auto sq = pp (O4), ss = pp (get (O4, "SubOsc4"));
    P.sub.on = num (sq, "kParamEnable", 0) != 0; P.sub.oct = num (sq, "kParamOctave", 0); P.sub.vol = num (sq, "kParamVolume", 0.5f); P.sub.pan = num (sq, "kParamPan", 0);
    const auto shape = str (ss, "kParamShape");
    P.sub.shape = shape == "kSquare" || shape == "kRoundRect" ? "square" : shape == "kSaw" ? "saw" : shape == "kTriangle" ? "tri" : "sine";

    for (int i = 0; i < 5; ++i)
    {
        const auto r = pp (get (d, ("RoutingSlot" + std::to_string (i)).c_str()));
        auto& R = P.route[(size_t) i];
        const auto dest = str (r, "kParamRoutingDest");
        if (dest.isNotEmpty()) R.dest = dest == "kRoutingDestFilter" ? 0 : dest == "kRoutingDestDirect" ? 2 : 1;
        R.bal = num (r, "kParamFilterBalance", -100); R.bus1 = num (r, "kParamFXBus1Level", 0); R.bus2 = num (r, "kParamFXBus2Level", 0);
    }
    for (int i = 0; i < 2; ++i)
    {
        const auto f = pp (get (d, ("VoiceFilter" + std::to_string (i)).c_str()));
        auto& F = P.filt[(size_t) i];
        F.on = num (f, "kParamEnable", 0) != 0; F.type = str (f, "kParamType", "MgL12"); F.freq = num (f, "kParamFreq", 0.5f);
        F.reso = num (f, "kParamReso", 10); F.drive = num (f, "kParamDrive", 0); F.vari = num (f, "kParamVar", 0); F.key = num (f, "kParamKeyTrack", 0);
    }
    for (int i = 0; i < 4; ++i)
    {
        const auto e = pp (get (d, ("Env" + std::to_string (i)).c_str()));
        auto& E = P.env[(size_t) i];
        E.a = num (e, "kParamAttack", 0.0005f); E.h = num (e, "kParamHold", 0); E.d = num (e, "kParamDecay", 1); E.s = num (e, "kParamSustain", 1);
        E.r = num (e, "kParamRelease", 0.015f); E.c1 = num (e, "kParamCurve1", 50); E.c2 = num (e, "kParamCurve2", 66.6f); E.c3 = num (e, "kParamCurve3", 66.6f);
    }
    for (int i = 0; i < 10; ++i)
    {
        const auto& L = get (d, ("LFO" + std::to_string (i)).c_str());
        const auto l = pp (L);
        auto& o = P.lfo[(size_t) i];
        o.mode = str (l, "kParamMode", "Trig"); o.sync = num (l, "kParamBeatSync", 1) != 0; o.rate = num (l, "kParamRate", 2);
        o.type = str (l, "kParamType", "curve"); o.smooth = num (l, "kParamSmooth", 0); o.mono = num (l, "kParamMono", 0) != 0; o.x10 = num (l, "kParamRate10x", 0) != 0;
        const auto& cd = get (L, "curveData");
        auto xs = floats (get (cd, "xVals")), ys = floats (get (cd, "yVals")), cs = floats (get (cd, "curveVals"));
        if (xs && xs->size() >= 2)
        {
            o.pts.clear();
            for (size_t k = 0; k < xs->size(); ++k)
                o.pts.push_back ({ (*xs)[k], 1.0f - (ys && k < ys->size() ? (*ys)[k] : 0.0f), cs && k < cs->size() ? (*cs)[k] : 0.5f });
        }
    }
    for (int i = 0; i < 64; ++i)
    {
        const auto& m = get (d, ("ModSlot" + std::to_string (i)).c_str());
        const auto& s = get (m, "source");
        auto src = floats (s);
        if (! src || src->empty()) continue;
        const auto q = pp (m);
        Mod M;
        M.src = (int) (*src)[0]; M.aux = src->size() > 1 ? (int) (*src)[1] : 0;
        M.mod = str (m, "destModuleTypeString"); M.id = (int) num (m, "destModuleID", 0); M.p = str (m, "destModuleParamName");
        M.amt = num (q, "kParamAmount", 0); M.bip = num (q, "kParamBipolar", 0) != 0;
        if (M.mod.isNotEmpty() && M.p.isNotEmpty()) P.mod.push_back (M);
        if (srcName (M.src).startsWith ("#")) P.missing.add (U ("モジュレーション元 ") + srcName (M.src) + U (" は未対応(無視)"));
    }
    for (int i = 0; i < 8; ++i)
    {
        const auto& M = get (d, ("Macro" + std::to_string (i)).c_str());
        if (str (M, "name").isNotEmpty()) P.macro[(size_t) i].name = str (M, "name");
        P.macro[(size_t) i].val = num (pp (M), "kParamValue", 0);
    }
    P.fx = rack (get (d, "FXRack0")); P.bus1 = rack (get (d, "FXRack1")); P.bus2 = rack (get (d, "FXRack2"));
    for (auto* r : { &P.fx, &P.bus1, &P.bus2 }) for (auto& f : *r) if (f.type == 10 || f.type == 11) P.missing.add (f.name + U (" FXは未対応"));
    const auto g = pp (get (d, "Global0"));
    P.glob.vol = num (g, "kParamMasterVolume", 0.5f); P.glob.mono = num (g, "kParamMonoToggle", 0) != 0; P.glob.legato = num (g, "kParamLegato", 0) != 0;
    P.glob.porta = num (g, "kParamPortamentoTime", 0); P.glob.portaAlways = num (g, "kParamPortaAlways", 0) != 0; P.glob.modwheel = num (g, "kParamModWheel", 0);
    const float tun = num (g, "kParamGlobalTuning", 440); P.glob.tuning = (tun > 415 && tun < 466) ? tun : 440.0f;
    return P;
}

//==============================================================================
// Serialisation (plugin state / .shizuku files)
namespace
{
juce::var floatsVar (const std::shared_ptr<std::vector<float>>& v)
{
    if (! v) return {};
    juce::MemoryBlock m (v->data(), v->size() * sizeof (float));
    return m.toBase64Encoding();
}
std::shared_ptr<std::vector<float>> floatsFrom (const juce::var& v)
{
    if (! v.isString()) return {};
    juce::MemoryBlock m; if (! m.fromBase64Encoding (v.toString())) return {};
    auto out = std::make_shared<std::vector<float>> (m.getSize() / sizeof (float));
    std::memcpy (out->data(), m.getData(), out->size() * sizeof (float));
    return out;
}
juce::var fxList (const std::vector<FxUnit>& l)
{
    juce::Array<juce::var> a;
    for (auto& f : l)
    {
        juce::DynamicObject::Ptr o = new juce::DynamicObject(), p = new juce::DynamicObject();
        o->setProperty ("type", f.type); o->setProperty ("on", f.on);
        for (auto& kv : f.p) p->setProperty (kv.name, kv.value);
        o->setProperty ("p", juce::var (p.get()));
        a.add (juce::var (o.get()));
    }
    return a;
}
std::vector<FxUnit> fxFrom (const juce::var& v)
{
    std::vector<FxUnit> out;
    if (auto* a = v.getArray())
        for (auto& x : *a)
        {
            FxUnit f; f.type = (int) num (x, "type", 0); f.name = fxName (f.type); f.on = (bool) get (x, "on");
            if (auto* d = get (x, "p").getDynamicObject()) f.p = d->getProperties();
            out.push_back (std::move (f));
        }
    return out;
}
}

juce::var Patch::toVar (bool withTables) const
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty ("shizuku", 1);
    o->setProperty ("name", name); o->setProperty ("author", author); o->setProperty ("desc", desc); o->setProperty ("source", source);
    juce::Array<juce::var> oa;
    for (auto& s : osc)
    {
        juce::DynamicObject::Ptr x = new juce::DynamicObject();
        x->setProperty ("on", s.on); x->setProperty ("sample", s.sample); x->setProperty ("tablePath", s.tablePath); x->setProperty ("tableName", s.tableName);
        x->setProperty ("samplePath", s.samplePath); x->setProperty ("sampleRoot", s.sampleRoot);
        x->setProperty ("oct", s.oct); x->setProperty ("semi", s.semi); x->setProperty ("fine", s.fine); x->setProperty ("vol", s.vol); x->setProperty ("pan", s.pan);
        x->setProperty ("uni", s.uni); x->setProperty ("det", s.det); x->setProperty ("blend", s.blend); x->setProperty ("stereo", s.stereo); x->setProperty ("pos", s.pos);
        x->setProperty ("warp", s.warp); x->setProperty ("warpAmt", s.warpAmt); x->setProperty ("phase", s.phase); x->setProperty ("rand", s.rand);
        if (withTables) { x->setProperty ("embedded", floatsVar (s.embedded)); x->setProperty ("sampleData", floatsVar (s.sampleData)); x->setProperty ("sampleRate", s.sampleRate); }
        oa.add (juce::var (x.get()));
    }
    o->setProperty ("osc", oa);
    { juce::DynamicObject::Ptr x = new juce::DynamicObject(); x->setProperty ("on", sub.on); x->setProperty ("shape", sub.shape); x->setProperty ("oct", sub.oct); x->setProperty ("vol", sub.vol); x->setProperty ("pan", sub.pan); o->setProperty ("sub", juce::var (x.get())); }
    { juce::DynamicObject::Ptr x = new juce::DynamicObject(); x->setProperty ("on", noise.on); x->setProperty ("type", noise.type); x->setProperty ("vol", noise.vol); x->setProperty ("color", noise.color); x->setProperty ("pan", noise.pan); x->setProperty ("samplePath", noise.samplePath); o->setProperty ("noise", juce::var (x.get())); }
    juce::Array<juce::var> ra; for (auto& r : route) { juce::DynamicObject::Ptr x = new juce::DynamicObject(); x->setProperty ("dest", r.dest); x->setProperty ("bal", r.bal); x->setProperty ("bus1", r.bus1); x->setProperty ("bus2", r.bus2); ra.add (juce::var (x.get())); }
    o->setProperty ("route", ra);
    juce::Array<juce::var> fa; for (auto& f : filt) { juce::DynamicObject::Ptr x = new juce::DynamicObject(); x->setProperty ("on", f.on); x->setProperty ("type", f.type); x->setProperty ("freq", f.freq); x->setProperty ("reso", f.reso); x->setProperty ("drive", f.drive); x->setProperty ("vari", f.vari); x->setProperty ("key", f.key); fa.add (juce::var (x.get())); }
    o->setProperty ("filt", fa);
    juce::Array<juce::var> ea; for (auto& e : env) { juce::DynamicObject::Ptr x = new juce::DynamicObject(); x->setProperty ("a", e.a); x->setProperty ("h", e.h); x->setProperty ("d", e.d); x->setProperty ("s", e.s); x->setProperty ("r", e.r); x->setProperty ("c1", e.c1); x->setProperty ("c2", e.c2); x->setProperty ("c3", e.c3); ea.add (juce::var (x.get())); }
    o->setProperty ("env", ea);
    juce::Array<juce::var> la;
    for (auto& l : lfo)
    {
        juce::DynamicObject::Ptr x = new juce::DynamicObject();
        x->setProperty ("mode", l.mode); x->setProperty ("type", l.type); x->setProperty ("sync", l.sync); x->setProperty ("mono", l.mono); x->setProperty ("x10", l.x10);
        x->setProperty ("rate", l.rate); x->setProperty ("smooth", l.smooth);
        juce::Array<juce::var> pa; for (auto& p : l.pts) pa.add (juce::Array<juce::var> { p.x, p.y, p.c });
        x->setProperty ("pts", pa); la.add (juce::var (x.get()));
    }
    o->setProperty ("lfo", la);
    juce::Array<juce::var> ma; for (auto& m : mod) { juce::DynamicObject::Ptr x = new juce::DynamicObject(); x->setProperty ("src", m.src); x->setProperty ("aux", m.aux); x->setProperty ("mod", m.mod); x->setProperty ("id", m.id); x->setProperty ("p", m.p); x->setProperty ("amt", m.amt); x->setProperty ("bip", m.bip); ma.add (juce::var (x.get())); }
    o->setProperty ("mod", ma);
    juce::Array<juce::var> mc; for (auto& m : macro) { juce::DynamicObject::Ptr x = new juce::DynamicObject(); x->setProperty ("name", m.name); x->setProperty ("val", m.val); mc.add (juce::var (x.get())); }
    o->setProperty ("macro", mc);
    o->setProperty ("fx", fxList (fx)); o->setProperty ("bus1", fxList (bus1)); o->setProperty ("bus2", fxList (bus2));
    { juce::DynamicObject::Ptr x = new juce::DynamicObject(); x->setProperty ("vol", glob.vol); x->setProperty ("mono", glob.mono); x->setProperty ("legato", glob.legato); x->setProperty ("portaAlways", glob.portaAlways); x->setProperty ("porta", glob.porta); x->setProperty ("modwheel", glob.modwheel); x->setProperty ("bend", glob.bend); x->setProperty ("tuning", glob.tuning); o->setProperty ("glob", juce::var (x.get())); }
    { juce::DynamicObject::Ptr x = new juce::DynamicObject(); x->setProperty ("on", arp.on); x->setProperty ("mode", arp.mode); x->setProperty ("rate", arp.rate); x->setProperty ("oct", arp.oct); x->setProperty ("gate", arp.gate); o->setProperty ("arp", juce::var (x.get())); }
    o->setProperty ("missing", missing.joinIntoString ("\n"));
    return juce::var (o.get());
}

Patch Patch::fromVar (const juce::var& v)
{
    Patch P;
    P.name = str (v, "name", "Init"); P.author = str (v, "author"); P.desc = str (v, "desc"); P.source = str (v, "source", "shizuku");
    if (auto* a = get (v, "osc").getArray())
        for (int i = 0; i < juce::jmin (3, a->size()); ++i)
        {
            const auto& x = a->getReference (i); auto& s = P.osc[(size_t) i];
            s.on = (bool) get (x, "on"); s.sample = (bool) get (x, "sample"); s.tablePath = str (x, "tablePath"); s.tableName = str (x, "tableName", "Default Shapes");
            s.samplePath = str (x, "samplePath"); s.sampleRoot = num (x, "sampleRoot", 60);
            s.oct = num (x, "oct", 0); s.semi = num (x, "semi", 0); s.fine = num (x, "fine", 0); s.vol = num (x, "vol", 0.5625f); s.pan = num (x, "pan", 0);
            s.uni = (int) num (x, "uni", 1); s.det = num (x, "det", 0.25f); s.blend = num (x, "blend", 75); s.stereo = num (x, "stereo", 100); s.pos = num (x, "pos", 0);
            s.warp = str (x, "warp", "none"); s.warpAmt = num (x, "warpAmt", 0); s.phase = num (x, "phase", 0); s.rand = num (x, "rand", 100);
            s.embedded = floatsFrom (get (x, "embedded")); s.sampleData = floatsFrom (get (x, "sampleData")); s.sampleRate = num (x, "sampleRate", 44100);
        }
    { const auto& x = get (v, "sub"); P.sub.on = (bool) get (x, "on"); P.sub.shape = str (x, "shape", "sine"); P.sub.oct = num (x, "oct", 0); P.sub.vol = num (x, "vol", 0.5f); P.sub.pan = num (x, "pan", 0); }
    { const auto& x = get (v, "noise"); P.noise.on = (bool) get (x, "on"); P.noise.type = str (x, "type", "White"); P.noise.vol = num (x, "vol", 0.5f); P.noise.color = num (x, "color", 0.5f); P.noise.pan = num (x, "pan", 0); P.noise.samplePath = str (x, "samplePath"); }
    if (auto* a = get (v, "route").getArray()) for (int i = 0; i < juce::jmin (5, a->size()); ++i) { const auto& x = a->getReference (i); auto& r = P.route[(size_t) i]; r.dest = (int) num (x, "dest", 1); r.bal = num (x, "bal", -100); r.bus1 = num (x, "bus1", 0); r.bus2 = num (x, "bus2", 0); }
    if (auto* a = get (v, "filt").getArray()) for (int i = 0; i < juce::jmin (2, a->size()); ++i) { const auto& x = a->getReference (i); auto& f = P.filt[(size_t) i]; f.on = (bool) get (x, "on"); f.type = str (x, "type", "MgL12"); f.freq = num (x, "freq", 0.5f); f.reso = num (x, "reso", 10); f.drive = num (x, "drive", 0); f.vari = num (x, "vari", 0); f.key = num (x, "key", 0); }
    if (auto* a = get (v, "env").getArray()) for (int i = 0; i < juce::jmin (4, a->size()); ++i) { const auto& x = a->getReference (i); auto& e = P.env[(size_t) i]; e.a = num (x, "a", 0.0005f); e.h = num (x, "h", 0); e.d = num (x, "d", 1); e.s = num (x, "s", 1); e.r = num (x, "r", 0.015f); e.c1 = num (x, "c1", 50); e.c2 = num (x, "c2", 66.6f); e.c3 = num (x, "c3", 66.6f); }
    if (auto* a = get (v, "lfo").getArray())
        for (int i = 0; i < juce::jmin (10, a->size()); ++i)
        {
            const auto& x = a->getReference (i); auto& l = P.lfo[(size_t) i];
            l.mode = str (x, "mode", "Trig"); l.type = str (x, "type", "curve"); l.sync = (bool) get (x, "sync"); l.mono = (bool) get (x, "mono"); l.x10 = (bool) get (x, "x10");
            l.rate = num (x, "rate", 2); l.smooth = num (x, "smooth", 0);
            if (auto* pa = get (x, "pts").getArray(); pa && pa->size() >= 2)
            {
                l.pts.clear();
                for (auto& p : *pa) if (auto* q = p.getArray(); q && q->size() >= 3) l.pts.push_back ({ (float) (double) (*q)[0], (float) (double) (*q)[1], (float) (double) (*q)[2] });
            }
        }
    if (auto* a = get (v, "mod").getArray())
        for (auto& x : *a) { Mod m; m.src = (int) num (x, "src", 0); m.aux = (int) num (x, "aux", 0); m.mod = str (x, "mod"); m.id = (int) num (x, "id", 0); m.p = str (x, "p"); m.amt = num (x, "amt", 0); m.bip = (bool) get (x, "bip"); P.mod.push_back (m); }
    if (auto* a = get (v, "macro").getArray()) for (int i = 0; i < juce::jmin (8, a->size()); ++i) { const auto& x = a->getReference (i); P.macro[(size_t) i].name = str (x, "name", "Macro " + juce::String (i + 1)); P.macro[(size_t) i].val = num (x, "val", 0); }
    P.fx = fxFrom (get (v, "fx")); P.bus1 = fxFrom (get (v, "bus1")); P.bus2 = fxFrom (get (v, "bus2"));
    { const auto& x = get (v, "glob"); P.glob.vol = num (x, "vol", 0.5f); P.glob.mono = (bool) get (x, "mono"); P.glob.legato = (bool) get (x, "legato"); P.glob.portaAlways = (bool) get (x, "portaAlways"); P.glob.porta = num (x, "porta", 0); P.glob.modwheel = num (x, "modwheel", 0); P.glob.bend = num (x, "bend", 2); P.glob.tuning = num (x, "tuning", 440); }
    { const auto& x = get (v, "arp"); P.arp.on = (bool) get (x, "on"); P.arp.mode = str (x, "mode", "up"); P.arp.rate = num (x, "rate", 4); P.arp.oct = (int) num (x, "oct", 1); P.arp.gate = num (x, "gate", 80); }
    const auto ms = str (v, "missing"); if (ms.isNotEmpty()) P.missing = juce::StringArray::fromLines (ms);
    return P;
}

//==============================================================================
static FxUnit fxu (int type, std::initializer_list<std::pair<const char*, juce::var>> ps)
{
    FxUnit f; f.type = type; f.name = fxName (type);
    for (auto& kv : ps) f.p.set (juce::Identifier (kv.first), kv.second);
    return f;
}

std::vector<Patch> factoryPatches()
{
    std::vector<Patch> v;
    { Patch p; p.name = U ("しずく Pluck"); auto& o = p.osc[0]; o.pos = 150; o.uni = 3; o.det = 0.12f; o.vol = 0.6f;
      p.filt[0].on = true; p.filt[0].freq = 0.42f; p.filt[0].reso = 28;
      p.env[0].d = 0.45f; p.env[0].s = 0.15f; p.env[0].r = 0.25f; p.env[1].d = 0.22f; p.env[1].s = 0;
      p.mod.push_back ({ 3, 0, "VoiceFilter", "kParamFreq", 0, 38, false });
      p.fx = { fxu (3, { { "kParamWet", 30 } }), fxu (4, { { "kParamTimeL", 0.234 }, { "kParamTimeR", 0.351 }, { "kParamWet", 22 }, { "kParamFeedback", 35 } }), fxu (6, { { "kParamWet", 18 }, { "kParamDelay", 55 } }) };
      p.macro[0] = { "BRIGHT", 0 }; p.mod.push_back ({ 25, 0, "VoiceFilter", "kParamFreq", 0, 30, false }); v.push_back (p); }
    { Patch p; p.name = U ("雨のスーパーソウ"); auto& o = p.osc[0]; o.pos = 100; o.uni = 9; o.det = 0.18f; o.vol = 0.55f;
      auto& b = p.osc[1]; b.on = true; b.pos = 100; b.oct = -1; b.uni = 5; b.det = 0.12f; b.vol = 0.35f;
      p.sub.on = true; p.sub.oct = -1; p.sub.vol = 0.35f; p.env[0].a = 0.01f; p.env[0].r = 0.4f; p.filt[0].on = true; p.filt[0].freq = 0.75f; p.filt[0].reso = 5; p.route[1].dest = 0;
      p.fx = { fxu (9, { { "kParamWet", 35 } }), fxu (5, { { "kParamMultiband", 1 }, { "kParamThresh", 0.55 }, { "kParamMakeup", 3 }, { "kParamWet", 45 } }), fxu (6, { { "kParamWet", 24 }, { "kParamDelay", 70 }, { "kParamSize", 70 } }) };
      v.push_back (p); }
    { Patch p; p.name = U ("深海サブ"); auto& o = p.osc[0]; o.pos = 40; o.vol = 0.7f; o.oct = -1; p.sub.on = true; p.sub.oct = -1; p.sub.vol = 0.6f;
      p.glob.mono = true; p.glob.legato = true; p.glob.porta = 0.06f; p.filt[0].on = true; p.filt[0].freq = 0.35f; p.filt[0].drive = 30; p.env[0].r = 0.08f;
      p.fx = { fxu (0, { { "kParamMode", "kTube" }, { "kParamDrive", 30 }, { "kParamWet", 60 } }) }; v.push_back (p); }
    { Patch p; p.name = U ("夜明けパッド"); auto& o = p.osc[0]; o.pos = 60; o.uni = 6; o.det = 0.1f; o.vol = 0.5f;
      auto& b = p.osc[1]; b.on = true; b.pos = 200; b.oct = 1; b.uni = 4; b.det = 0.08f; b.vol = 0.25f;
      p.env[0].a = 1.2f; p.env[0].d = 2; p.env[0].s = 0.8f; p.env[0].r = 2.5f; p.filt[0].on = true; p.filt[0].freq = 0.55f; p.filt[0].reso = 15; p.route[1].dest = 0;
      p.lfo[0].mode = "Free"; p.lfo[0].rate = 0.25f;
      p.mod.push_back ({ 6, 0, "Oscillator", "kParamTablePos", 0, 25, true }); p.mod.push_back ({ 6, 0, "VoiceFilter", "kParamFreq", 0, 10, true });
      p.fx = { fxu (3, { { "kParamWet", 45 } }), fxu (6, { { "kParamWet", 40 }, { "kParamDelay", 85 }, { "kParamSize", 90 } }) }; v.push_back (p); }
    { Patch p; p.name = U ("アシッドの波紋"); auto& o = p.osc[0]; o.pos = 128; o.vol = 0.6f; p.glob.mono = true; p.glob.porta = 0.05f;
      p.filt[0].on = true; p.filt[0].type = "MgL24"; p.filt[0].freq = 0.3f; p.filt[0].reso = 70; p.filt[0].drive = 20;
      p.env[1].d = 0.18f; p.env[1].s = 0; p.mod.push_back ({ 3, 0, "VoiceFilter", "kParamFreq", 0, 45, false });
      p.arp.on = true; p.arp.rate = 4; p.arp.oct = 2; p.arp.gate = 60;
      p.fx = { fxu (0, { { "kParamMode", "kAsym" }, { "kParamDrive", 35 }, { "kParamWet", 70 } }), fxu (4, { { "kParamTimeL", 0.351 }, { "kParamTimeR", 0.234 }, { "kParamWet", 20 }, { "kParamMode", 1 } }) };
      v.push_back (p); }
    return v;
}
}
