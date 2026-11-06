#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include <functional>

namespace yugami
{
inline juce::String jp (const char* s) { return juce::String::fromUTF8 (s); }

constexpr int kSlots = 36;
constexpr int kTab = 1024;
constexpr int kMaxPages = 12;

/** One envelope point. x = 0..1 across the slot length, y = 0..1, b = bend of the segment that starts here (-1..1).
    Time envelopes: y = how far back to play, in bars (0 = live). Volume envelopes: y = level. */
struct Node { float x = 0, y = 0, b = 0; };

struct Slot
{
    juce::String name;
    float len = 4.0f;              // beats
    std::vector<Node> nodes;
    bool isEmpty() const;
};

struct Page
{
    juce::String name;
    std::array<Slot, kSlots> time, vol;
};

struct Preset
{
    juce::String name, cat;
    float len = 4.0f;
    std::vector<Node> nodes;
    Slot toSlot() const { return { name, len, nodes }; }
};

float evalCurve (const std::vector<Node>& c, float x);
void compile (const Slot& s, float* table);           // kTab samples

Slot emptyTime (int i);
Slot emptyVol (int i);
Page emptyPage (const juce::String& name);

struct Factory
{
    std::vector<Preset> time, vol;      // 72 each: 18 classics + 54 blocky
    std::vector<juce::String> categoriesTime, categoriesVol;
};
const Factory& factory();
std::vector<Page> factoryPages();

juce::var toVar (const Slot&);
juce::var toVar (const Preset&);
juce::var toVar (const Page&);
bool fromVar (const juce::var&, Slot&);
bool fromVar (const juce::var&, Preset&);
bool fromVar (const juce::var&, Page&);

/** shape tools ("形を入れる") for the editor */
struct Tool { juce::String name; std::function<std::vector<Node> (float len)> make; };
const std::vector<Tool>& timeTools();
const std::vector<Tool>& volTools();
} // namespace yugami
