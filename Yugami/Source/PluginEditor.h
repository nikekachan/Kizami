#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace yugami::ui
{
namespace col
{
    const juce::Colour bg     { 0xff0a0612 };
    const juce::Colour panel  { 0xff120a1f };
    const juce::Colour cell   { 0xff1d1331 };
    const juce::Colour cell2  { 0xff2a1c45 };
    const juce::Colour ink    { 0xfff6f0ff };
    const juce::Colour muted  { 0xffab9fc6 };
    const juce::Colour line   = ink.withAlpha (0.14f);
    const juce::Colour line2  = ink.withAlpha (0.26f);
    const juce::Colour time   { 0xff2ff3e0 };
    const juce::Colour vol    { 0xffff5ad1 };
    const juce::Colour sun    { 0xffffd84a };
    const juce::Colour purple { 0xff9b5cff };
    const juce::Colour danger { 0xffff8a80 };
}
juce::Colour hue (float h, float s = 0.85f, float b = 1.0f);

struct Fonts
{
    Fonts();
    juce::Font body (float h, bool bold = false) const;
    juce::Font display (float h) const;
    juce::Font mono (float h) const;
    juce::Typeface::Ptr comfortaa;
};
const Fonts& fonts();

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over, bool down) override;
};

class YugamiEditor;

//==============================================================================
class Knob : public juce::Component
{
public:
    Knob (YugamiProcessor& p, const juce::String& id, juce::Colour c, bool mini = false);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider slider;
private:
    juce::RangedAudioParameter* prm;
    bool mini;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class SlotGrid : public juce::Component, public juce::SettableTooltipClient
{
public:
    SlotGrid (YugamiEditor& e, bool time) : editor (e), isTime (time) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    juce::String getTooltipFor (int i) const;
private:
    int indexAt (juce::Point<float>) const;
    juce::Rectangle<float> cellRect (int i) const;
    YugamiEditor& editor;
    bool isTime;
    int hover = -1;
};

class EnvEditor : public juce::Component
{
public:
    explicit EnvEditor (YugamiEditor& e) : editor (e) {}
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverX = -1; repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    float snap = 0.25f;
private:
    struct Hit { int type = 0, i = -1; }; // 1 = point, 2 = bend
    Hit hitTest (juce::Point<float>) const;
    float X (float x) const; float Y (float y) const; float ux (float px) const; float uy (float py) const;
    float snapX (float x) const; float snapY (float y) const;
    YugamiEditor& editor;
    Hit drag; float dragPy0 = 0, dragB0 = 0; juce::uint32 lastCommit = 0;
    float hoverX = -1, hoverPx = 0;
};

class Dial : public juce::Component
{
public:
    explicit Dial (YugamiProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    YugamiProcessor& proc;
};

/** Preset library: category tabs + a scrolling grid of cards with thumbnails. */
class Library : public juce::Component
{
public:
    explicit Library (YugamiEditor& e);
    void paint (juce::Graphics&) override;
    void resized() override;
    void rebuild();
    juce::String cat;
    struct Item { const Preset* p; int mineIndex; };
private:
    class Grid : public juce::Component
    {
    public:
        explicit Grid (Library& l) : lib (l) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
        juce::Rectangle<float> cardRect (int i) const;
        juce::Rectangle<float> delRect (int i) const;
        int cols() const;
        Library& lib;
        int hover = -1;
    };
    YugamiEditor& editor;
    juce::OwnedArray<juce::TextButton> tabs;
    juce::TextButton saveBtn, closeBtn;
    juce::Viewport view;
    Grid grid;
public:
    std::vector<Item> items;
    friend class Grid;
};

/** Animated psychedelic backdrop + the panel shapes everything sits on. */
class Backdrop : public juce::Component
{
public:
    Backdrop();
    void paint (juce::Graphics&) override;
    void tick (double t);
    std::vector<juce::Rectangle<float>> panels;
    juce::Image logo;
private:
    juce::Image field;
    double time = 0;
};

//==============================================================================
class Content : public juce::Component
{
public:
    static constexpr int W = 1240, H = 880;
    explicit Content (YugamiEditor& e);
    void resized() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void refreshEditTarget();
    void refreshPager();
    void rebuildTools();
    void setLibraryOpen (bool);

    YugamiEditor& editor;
    Backdrop backdrop;
    juce::TextButton exportBtn, importBtn, resetBtn;
    juce::TextButton pgPrev { juce::String::fromUTF8 ("◀") }, pgNext { juce::String::fromUTF8 ("▶") }, pgAdd { "+" }, pgDel;
    juce::TextEditor pgName;
    juce::Label pgNum, status, timeLbl, volLbl, kindLbl;
    juce::TextButton holdBtn { "HOLD" }, libBtn, saveBtn, copyBtn, pasteBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> holdAtt, limAtt;
    juce::ToggleButton limiter;
    juce::OwnedArray<juce::TextButton> tools;
    SlotGrid timeGrid, volGrid;
    EnvEditor env;
    Library library;
    Dial dial;
    juce::OwnedArray<Knob> knobs;
    Knob* tMix = nullptr; Knob* vMix = nullptr; Knob* out = nullptr;
    juce::TextEditor slotName;
    juce::ComboBox lenBox, snapBox, trigBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> trigAtt;
    juce::Rectangle<float> stripR, railR, mainR, meterR, headerR;
    double time = 0;
    bool toolsTime = true;
};

class YugamiEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit YugamiEditor (YugamiProcessor&);
    ~YugamiEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;
    void focusLost (FocusChangeType) override;

    YugamiProcessor& proc;
    Slot& editSlot() { return proc.slot (proc.editTime, proc.editIdx); }
    void selectSlot (bool time, int i);    // use it + edit it
    void edited (bool live = true);        // the edit slot changed
    void setStatus (const juce::String&, bool error = false);
    void loadPreset (const Preset&);
    void saveMine();
    void refreshAll();
    int baseT() const { return (int) proc.param ("tSlot") - 1; }
    int baseV() const { return (int) proc.param ("vSlot") - 1; }

    std::unique_ptr<juce::FileChooser> chooser;
    Look look;
    Content content;
    juce::TooltipWindow tips { this, 600 };
    std::optional<Slot> clip; bool clipTime = true;
    juce::uint32 resetArm = 0, delArm = 0;

private:
    void timerCallback() override;
    std::array<int, 16> keyDown {};     // 0 none, 1 time, 2 volume
    int statusTimeout = 0, lastGen = -1, lastHotT = -2, lastHotV = -2, lastBaseT = -2, lastBaseV = -2;
    double startTime = 0;
    int frame = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YugamiEditor)
};
} // namespace yugami::ui

using YugamiEditor = yugami::ui::YugamiEditor;
