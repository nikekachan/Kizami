#include "Tables.h"
#include <juce_data_structures/juce_data_structures.h>
#include <functional>
#include <cmath>
#include <cstring>
#include <complex>

namespace shz
{
bool parseWav (const void* data, size_t size, Wav& out)
{
    auto* u8 = (const uint8_t*) data;
    auto tag = [&] (size_t o, const char* t) { return o + 4 <= size && std::memcmp (u8 + o, t, 4) == 0; };
    auto r16 = [&] (size_t o) { return (int) (u8[o] | (u8[o + 1] << 8)); };
    auto r32 = [&] (size_t o) { return (uint32_t) (u8[o] | (u8[o + 1] << 8) | (u8[o + 2] << 16) | ((uint32_t) u8[o + 3] << 24)); };
    if (size < 12 || ! tag (0, "RIFF") || ! tag (8, "WAVE")) return false;
    size_t p = 12; int fmtTag = 0, ch = 0, bits = 0; uint32_t sr = 44100; size_t dOff = 0, dLen = 0; bool haveFmt = false;
    out.frameSize = 0;
    while (p + 8 <= size)
    {
        const uint32_t sz = r32 (p + 4); const size_t b = p + 8;
        if (tag (p, "fmt ") && b + 16 <= size) { fmtTag = r16 (b); ch = r16 (b + 2); sr = r32 (b + 4); bits = r16 (b + 14); haveFmt = true; if (fmtTag == 0xFFFE && b + 26 <= size) fmtTag = r16 (b + 24); }
        else if (tag (p, "data")) { dOff = b; dLen = juce::jmin ((size_t) sz, size - b); }
        else if (tag (p, "clm ") && b + sz <= size)
        {
            const juce::String s ((const char*) u8 + b, (size_t) sz);
            const int i = s.indexOf ("<!>");
            if (i >= 0) out.frameSize = s.substring (i + 3).getIntValue();
        }
        p = b + sz + (sz & 1);
    }
    if (! haveFmt || ! dOff || ch <= 0 || bits <= 0) return false;
    const int bps = bits / 8; const size_t n = dLen / (size_t) (bps * ch);
    out.sr = sr; out.samples.resize (n);
    for (size_t i = 0; i < n; ++i)
    {
        const size_t o = dOff + i * (size_t) (bps * ch);
        float v = 0;
        if (fmtTag == 3 && bits == 32) std::memcpy (&v, u8 + o, 4);
        else if (fmtTag == 3 && bits == 64) { double d; std::memcpy (&d, u8 + o, 8); v = (float) d; }
        else if (bits == 16) v = (int16_t) r16 (o) / 32768.0f;
        else if (bits == 24) { int32_t x = (int32_t) (u8[o] | (u8[o + 1] << 8) | (u8[o + 2] << 16)); if (x & 0x800000) x -= 0x1000000; v = x / 8388608.0f; }
        else if (bits == 32) v = (int32_t) r32 (o) / 2147483648.0f;
        else if (bits == 8) v = (u8[o] - 128) / 128.0f;
        out.samples[i] = v;
    }
    return true;
}

std::vector<Frame> framesFrom (const std::vector<float>& s, int frameSize)
{
    const int fs = frameSize > 0 ? frameSize : 2048;
    std::vector<Frame> frames;
    int count = (int) (s.size() / (size_t) fs);
    if (count < 1)
    {
        Frame f (2048, 0.0f);
        if (! s.empty()) for (int i = 0; i < 2048; ++i) f[(size_t) i] = s[(size_t) ((double) i / 2048 * (double) s.size())];
        frames.push_back (f); return frames;
    }
    count = juce::jmin (count, 256);
    for (int k = 0; k < count; ++k)
    {
        Frame f (2048);
        for (int i = 0; i < 2048; ++i)
        {
            if (fs == 2048) { f[(size_t) i] = s[(size_t) (k * fs + i)]; continue; }
            const double x = (double) i / 2048 * fs; const int j = (int) x; const float t = (float) (x - j);
            const float a = s[(size_t) (k * fs + j)], b = s[(size_t) (k * fs + (j + 1) % fs)];
            f[(size_t) i] = a + (b - a) * t;
        }
        frames.push_back (std::move (f));
    }
    return frames;
}

static void fft (std::vector<std::complex<double>>& a, bool inv)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = 2 * juce::MathConstants<double>::pi / (double) len * (inv ? 1 : -1);
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1);
            for (size_t j = 0; j < len / 2; ++j) { auto u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; }
        }
    }
    if (inv) for (auto& x : a) x /= (double) n;
}

TablePtr buildTable (const std::vector<Frame>& in, int maxFrames)
{
    auto T = std::make_shared<Table>();
    std::vector<Frame> fr = in;
    if ((int) fr.size() > maxFrames)
    {
        std::vector<Frame> o;
        for (int k = 0; k < maxFrames; ++k) o.push_back (in[(size_t) std::lround ((double) k / (maxFrames - 1) * (double) (in.size() - 1))]);
        fr = std::move (o);
    }
    if (fr.empty()) fr.push_back (Frame (2048, 0.0f));
    const int n = (int) fr.size(), LV = 11;
    T->n = n; T->lv.assign ((size_t) LV, std::vector<float> ((size_t) n * 2049));
    float peak = 0; for (auto& f : fr) for (float x : f) peak = juce::jmax (peak, std::abs (x));
    const double gn = peak > 0 ? 1.0 / peak : 1.0;
    std::vector<std::complex<double>> spec (2048), tmp (2048);
    for (int k = 0; k < n; ++k)
    {
        for (int i = 0; i < 2048; ++i) spec[(size_t) i] = fr[(size_t) k][(size_t) i] * gn;
        fft (spec, false);
        spec[0] = 0;
        for (int L = 0; L < LV; ++L)
        {
            const int H = juce::jmax (1, 1024 >> L);
            std::fill (tmp.begin(), tmp.end(), std::complex<double> (0));
            for (int h = 1; h <= juce::jmin (H, 1023); ++h) { tmp[(size_t) h] = spec[(size_t) h]; tmp[(size_t) (2048 - h)] = spec[(size_t) (2048 - h)]; }
            fft (tmp, true);
            auto& a = T->lv[(size_t) L]; const size_t o = (size_t) k * 2049;
            for (int i = 0; i < 2048; ++i) a[o + (size_t) i] = (float) tmp[(size_t) i].real();
            a[o + 2048] = a[o];
        }
        Frame disp (2048); for (int i = 0; i < 2048; ++i) disp[(size_t) i] = T->lv[0][(size_t) k * 2049 + (size_t) i];
        T->frames.push_back (std::move (disp));
    }
    return T;
}

const std::map<juce::String, std::vector<Frame>>& builtinTables()
{
    static const auto tables = []
    {
        using Fn = std::function<float (double)>;
        auto mk = [] (const Fn& f) { Frame fr (2048); for (int i = 0; i < 2048; ++i) fr[(size_t) i] = f (i / 2048.0); return fr; };
        Fn sine = [] (double t) { return (float) std::sin (2 * juce::MathConstants<double>::pi * t); };
        Fn tri = [] (double t) { return (float) (t < 0.25 ? 4 * t : t < 0.75 ? 2 - 4 * t : 4 * t - 4); };
        Fn saw = [] (double t) { return (float) (t < 0.5 ? 2 * t : 2 * t - 2); };
        Fn sq = [] (double t) { return t < 0.5 ? 1.0f : -1.0f; };
        Fn pulse = [] (double t) { return t < 0.25 ? 1.0f : -1.0f; };
        auto morph = [&] (std::vector<Fn> s, int per)
        {
            std::vector<Frame> out;
            for (size_t i = 0; i + 1 < s.size(); ++i)
                for (int k = 0; k < per; ++k) { const double m = (double) k / per; auto a = s[i], b = s[i + 1]; out.push_back (mk ([=] (double t) { return (float) (a (t) * (1 - m) + b (t) * m); })); }
            out.push_back (mk (s.back()));
            return out;
        };
        std::map<juce::String, std::vector<Frame>> m;
        m["Default Shapes"] = morph ({ sine, tri, saw, sq, pulse }, 2);
        m["Basic Shapes"] = morph ({ sine, tri, saw, sq }, 2);
        return m;
    }();
    return tables;
}

//==============================================================================
TableLibrary& TableLibrary::get() { static TableLibrary t; return t; }
TableLibrary::TableLibrary()
{
    juce::PropertiesFile::Options o; o.applicationName = "SHIZUKU"; o.folderName = "DJ KATSUNII/SHIZUKU"; o.filenameSuffix = ".settings"; o.osxLibrarySubFolder = "Application Support";
    juce::PropertiesFile pf (o);
    extra = juce::File (pf.getValue ("tablesFolder"));
    rescan();
}
juce::Array<juce::File> TableLibrary::defaultRoots()
{
    juce::Array<juce::File> r;
    const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    for (auto base : { home.getChildFile ("Library/Audio/Presets/Xfer Records/Serum 2 Presets"),
                       juce::File ("/Library/Audio/Presets/Xfer Records/Serum 2 Presets"),
                       home.getChildFile ("Documents/Xfer/Serum 2 Presets"),
                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Xfer/Serum 2 Presets"),
                       juce::File ("C:\\ProgramData\\Xfer\\Serum 2 Presets") })
        r.addIfNotAlreadyThere (base.getChildFile ("Tables"));
    return r;
}
void TableLibrary::setExtraRoot (const juce::File& f)
{
    { std::lock_guard<std::mutex> l (m); extra = f; }
    juce::PropertiesFile::Options o; o.applicationName = "SHIZUKU"; o.folderName = "DJ KATSUNII/SHIZUKU"; o.filenameSuffix = ".settings"; o.osxLibrarySubFolder = "Application Support";
    juce::PropertiesFile pf (o); pf.setValue ("tablesFolder", f.getFullPathName()); pf.saveIfNeeded();
    rescan();
}
juce::File TableLibrary::extraRoot() const { std::lock_guard<std::mutex> l (m); return extra; }
int TableLibrary::count() const { std::lock_guard<std::mutex> l (m); return (int) byName.size(); }
juce::StringArray TableLibrary::rootsFound() const { std::lock_guard<std::mutex> l (m); return roots; }

void TableLibrary::rescan()
{
    if (thread) thread->stopThread (4000);
    scanned = false;
    struct T : juce::Thread { TableLibrary& o; explicit T (TableLibrary& x) : juce::Thread ("SHIZUKU tables"), o (x) {} void run() override { o.scan(); } };
    thread = std::make_unique<T> (*this);
    thread->startThread (juce::Thread::Priority::low);
}
void TableLibrary::scan()
{
    std::map<juce::String, juce::File> p, n; juce::StringArray rs;
    auto list = defaultRoots();
    juce::File ex; { std::lock_guard<std::mutex> l (m); ex = extra; }
    if (ex.isDirectory()) list.insert (0, ex);
    for (auto& root : list)
    {
        if (! root.isDirectory()) continue;
        rs.add (root.getFullPathName());
        for (const auto& e : juce::RangedDirectoryIterator (root, true, "*.wav", juce::File::findFiles))
        {
            if (juce::Thread::currentThreadShouldExit()) return;
            const auto f = e.getFile();
            const auto rel = f.getRelativePathFrom (root).replaceCharacter ('\\', '/').toLowerCase();
            auto parts = juce::StringArray::fromTokens (rel, "/", "");
            for (int k = 0; k < parts.size(); ++k)
            {
                juce::StringArray tail; for (int j = k; j < parts.size(); ++j) tail.add (parts[j]);
                const auto key = tail.joinIntoString ("/");
                if (p.find (key) == p.end()) p[key] = f;
            }
            if (n.find (f.getFileName().toLowerCase()) == n.end()) n[f.getFileName().toLowerCase()] = f;
        }
    }
    { std::lock_guard<std::mutex> l (m); byPath = std::move (p); byName = std::move (n); roots = rs; }
    scanned = true;
}
juce::File TableLibrary::find (const juce::String& rel) const
{
    std::lock_guard<std::mutex> l (m);
    auto key = rel.replaceCharacter ('\\', '/').trimCharactersAtStart ("/").toLowerCase();
    while (key.isNotEmpty())
    {
        if (auto it = byPath.find (key); it != byPath.end()) return it->second;
        if (! key.contains ("/")) break;
        key = key.fromFirstOccurrenceOf ("/", false, false);
    }
    if (auto it = byName.find (rel.fromLastOccurrenceOf ("/", false, false).toLowerCase()); it != byName.end()) return it->second;
    return {};
}
}
