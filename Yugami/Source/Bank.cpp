#include "Bank.h"
#include <BinaryData.h>
#include <cmath>

namespace yugami
{
float evalCurve (const std::vector<Node>& c, float x)
{
    if (c.empty()) return 0.0f;
    if (x <= c.front().x) return c.front().y;
    const size_t n = c.size();
    for (size_t i = 0; i + 1 < n; ++i)
    {
        const auto& a = c[i];
        const auto& b = c[i + 1];
        if (x < b.x || (i == n - 2 && x <= b.x))
        {
            if (b.x <= a.x) continue;
            const float u = (x - a.x) / (b.x - a.x);
            const float s = a.b >= 0 ? std::pow (u, 1.0f + 4.0f * a.b) : 1.0f - std::pow (1.0f - u, 1.0f - 4.0f * a.b);
            return a.y + (b.y - a.y) * s;
        }
    }
    return c.back().y;
}

void compile (const Slot& s, float* t)
{
    for (int i = 0; i < kTab; ++i) t[i] = evalCurve (s.nodes, (float) i / (float) (kTab - 1));
}

bool Slot::isEmpty() const
{
    return name.startsWith (jp ("空き")) && nodes.size() == 2 && nodes[0].y == nodes[1].y;
}

Slot emptyTime (int i) { return { jp ("空き ") + juce::String (i + 1), 4.0f, { { 0, 0, 0 }, { 1, 0, 0 } } }; }
Slot emptyVol (int i)  { return { jp ("空き ") + juce::String (i + 1), 4.0f, { { 0, 1, 0 }, { 1, 1, 0 } } }; }
Page emptyPage (const juce::String& name)
{
    Page p; p.name = name;
    for (int i = 0; i < kSlots; ++i) { p.time[(size_t) i] = emptyTime (i); p.vol[(size_t) i] = emptyVol (i); }
    return p;
}

//==============================================================================
static std::vector<Node> nodesFromVar (const juce::var& v)
{
    std::vector<Node> out;
    if (auto* a = v.getArray())
        for (auto& q : *a)
            if (auto* p = q.getArray(); p != nullptr && p->size() >= 2)
                out.push_back ({ juce::jlimit (0.0f, 1.0f, (float) (double) (*p)[0]), juce::jlimit (0.0f, 1.0f, (float) (double) (*p)[1]),
                                 p->size() > 2 ? juce::jlimit (-1.0f, 1.0f, (float) (double) (*p)[2]) : 0.0f });
    // keep x sorted, pin the ends
    for (size_t i = 1; i < out.size(); ++i) out[i].x = juce::jmax (out[i].x, out[i - 1].x);
    if (out.size() >= 2) { out.front().x = 0; out.back().x = 1; }
    return out;
}
static juce::var nodesToVar (const std::vector<Node>& n)
{
    juce::Array<juce::var> a;
    for (auto& q : n) a.add (juce::Array<juce::var> { q.x, q.y, q.b });
    return a;
}

juce::var toVar (const Slot& s)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("n", s.name); o->setProperty ("l", s.len); o->setProperty ("k", nodesToVar (s.nodes));
    return o;
}
juce::var toVar (const Preset& s)
{
    auto v = toVar (s.toSlot());
    v.getDynamicObject()->setProperty ("c", s.cat);
    return v;
}
juce::var toVar (const Page& p)
{
    auto* o = new juce::DynamicObject();
    juce::Array<juce::var> t, v;
    for (auto& s : p.time) t.add (toVar (s));
    for (auto& s : p.vol) v.add (toVar (s));
    o->setProperty ("name", p.name); o->setProperty ("time", t); o->setProperty ("vol", v);
    return o;
}
bool fromVar (const juce::var& v, Slot& s)
{
    if (! v.isObject()) return false;
    auto nodes = nodesFromVar (v["k"]);
    if (nodes.size() < 2) return false;
    s.name = v["n"].toString().substring (0, 12);
    if (s.name.isEmpty()) s.name = jp ("名前なし");
    const float l = (float) (double) v["l"];
    s.len = (l == 1.0f || l == 2.0f || l == 4.0f || l == 8.0f) ? l : 4.0f;
    s.nodes = std::move (nodes);
    return true;
}
bool fromVar (const juce::var& v, Preset& p)
{
    Slot s;
    if (! fromVar (v, s)) return false;
    p.name = s.name; p.len = s.len; p.nodes = s.nodes; p.cat = v["c"].toString();
    return true;
}
bool fromVar (const juce::var& v, Page& p)
{
    auto* t = v["time"].getArray();
    auto* vo = v["vol"].getArray();
    if (t == nullptr || vo == nullptr || t->size() != kSlots || vo->size() != kSlots) return false;
    Page q; q.name = v["name"].toString().substring (0, 14);
    if (q.name.isEmpty()) q.name = jp ("ページ");
    for (int i = 0; i < kSlots; ++i)
    {
        if (! fromVar ((*t)[i], q.time[(size_t) i])) q.time[(size_t) i] = emptyTime (i);
        if (! fromVar ((*vo)[i], q.vol[(size_t) i])) q.vol[(size_t) i] = emptyVol (i);
    }
    p = std::move (q);
    return true;
}

//==============================================================================
const Factory& factory()
{
    static Factory f = [] {
        Factory r;
        auto json = juce::JSON::parse (juce::String::fromUTF8 (BinaryData::factory_json, BinaryData::factory_jsonSize));
        auto load = [] (const juce::var& arr, std::vector<Preset>& out, std::vector<juce::String>& cats) {
            if (auto* a = arr.getArray())
                for (auto& x : *a)
                {
                    Preset p;
                    if (fromVar (x, p)) { out.push_back (p); if (std::find (cats.begin(), cats.end(), p.cat) == cats.end()) cats.push_back (p.cat); }
                }
        };
        load (json["time"], r.time, r.categoriesTime);
        load (json["vol"], r.vol, r.categoriesVol);
        return r;
    }();
    return f;
}

std::vector<Page> factoryPages()
{
    const auto& f = factory();
    std::vector<Page> pages;
    const char* names[] = { "スタンダード", "カクカク" };
    for (int p = 0; p < 2; ++p)
    {
        Page pg = emptyPage (jp (names[p]));
        for (int i = 0; i < kSlots; ++i)
        {
            const size_t k = (size_t) (p * kSlots + i);
            if (k < f.time.size()) pg.time[(size_t) i] = f.time[k].toSlot();
            if (k < f.vol.size()) pg.vol[(size_t) i] = f.vol[k].toSlot();
        }
        pages.push_back (std::move (pg));
    }
    pages.push_back (emptyPage (jp ("マイページ")));
    return pages;
}

//==============================================================================
static std::vector<Node> steps (float from, float to, int n, float L)
{
    std::vector<Node> a;
    for (int k = 0; k < n; ++k)
    {
        const float x0 = from + (to - from) * (float) k / (float) n, x1 = from + (to - from) * (float) (k + 1) / (float) n;
        const float y = juce::jmin (1.0f, (float) k * (to - from) / (float) n * L / 4.0f);
        a.push_back ({ x0, y, 0 }); a.push_back ({ x1, y, 0 });
    }
    return a;
}
static std::vector<Node> wobble (float lo, float hi)
{
    std::vector<Node> a;
    for (int i = 0; i <= 16; ++i) a.push_back ({ (float) i / 16.0f, (i % 2) ? lo : hi, (i % 2) ? 0.4f : -0.4f });
    return a;
}
static std::vector<Node> duck (int n, float floor)
{
    std::vector<Node> a;
    for (int b = 0; b < n; ++b) { const float x = (float) b / (float) n, w = 1.0f / (float) n; a.push_back ({ x, floor, -0.5f }); a.push_back ({ x + w * 0.55f, 1, 0 }); a.push_back ({ x + w - 0.0005f, 1, 0 }); }
    a.push_back ({ 1, floor, 0 });
    return a;
}
static std::vector<Node> gate (int n, float on)
{
    std::vector<Node> a;
    for (int b = 0; b < n; ++b) { const float x = (float) b / (float) n, w = 1.0f / (float) n; a.push_back ({ x, 1, 0 }); a.push_back ({ x + w * on, 1, 0 }); a.push_back ({ x + w * on, 0, 0 }); a.push_back ({ x + w, 0, 0 }); }
    return a;
}

const std::vector<Tool>& timeTools()
{
    static std::vector<Tool> t {
        { jp ("リセット"), [] (float) { return std::vector<Node> { { 0, 0, 0 }, { 1, 0, 0 } }; } },
        { jp ("ハーフ"), [] (float L) { return std::vector<Node> { { 0, 0, 0 }, { 1, juce::jmin (1.0f, 0.5f * L / 4.0f), 0 } }; } },
        { jp ("逆再生"), [] (float L) { return std::vector<Node> { { 0, 0, 0 }, { 1, juce::jmin (1.0f, L / 2.0f), 0 } }; } },
        { jp ("テープストップ"), [] (float L) { return std::vector<Node> { { 0, 0, 0.25f }, { 1, juce::jmin (1.0f, 0.5f * L / 4.0f), 0 } }; } },
        { jp ("階段"), [] (float L) { std::vector<Node> a; const int beats = juce::jmax (1, (int) L);
              for (int b = 0; b < beats; ++b) { float y = 0;
                  for (int k = 0; k < 4; ++k) { const float t0 = (float) b + 0.25f * (float) k;
                      a.push_back ({ t0 / L, y, 0 }); y += 0.03125f; a.push_back ({ (t0 + 0.125f) / L, y, 0 }); a.push_back ({ (t0 + 0.25f) / L, y, 0 }); } }
              return a; } },
        { jp ("リピート1拍"), [] (float L) { return steps (0, 1, juce::jmax (1, (int) L), L); } },
        { jp ("ロール1/16"), [] (float L) { std::vector<Node> a { { 0, 0, 0 }, { 1 - 1 / L, 0, 0 } }; auto s = steps (1 - 1 / L, 1, 4, L); a.insert (a.end(), s.begin(), s.end()); return a; } },
        { jp ("揺らす"), [] (float) { return wobble (0.006f, 0.0f); } },
    };
    return t;
}
const std::vector<Tool>& volTools()
{
    static std::vector<Tool> t {
        { jp ("リセット"), [] (float) { return std::vector<Node> { { 0, 1, 0 }, { 1, 1, 0 } }; } },
        { jp ("ダッキング"), [] (float L) { return duck (juce::jmax (1, (int) L), 0); } },
        { jp ("ゲート1/16"), [] (float L) { return gate (juce::jmax (1, (int) L) * 4, 0.55f); } },
        { jp ("3連ゲート"), [] (float L) { return gate (juce::jmax (1, (int) L) * 3, 0.5f); } },
        { jp ("フェードイン"), [] (float) { return std::vector<Node> { { 0, 0, 0.3f }, { 1, 1, 0 } }; } },
        { jp ("フェードアウト"), [] (float) { return std::vector<Node> { { 0, 1, -0.3f }, { 1, 0, 0 } }; } },
        { jp ("トレモロ"), [] (float) { return wobble (0.25f, 1.0f); } },
    };
    return t;
}
} // namespace yugami
