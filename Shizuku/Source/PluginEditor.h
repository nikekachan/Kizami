#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace shz::ui
{
namespace col
{
    const juce::Colour bg     { 0xff050b12 };
    const juce::Colour panel  { 0xff0b1622 };
    const juce::Colour pane   { 0xff0f1e2d };
    const juce::Colour pane2  { 0xff16293b };
    const juce::Colour ink    { 0xffe6f6ff };
    const juce::Colour muted  { 0xff89a6b8 };
    const juce::Colour line   = ink.withAlpha (0.08f);
    const juce::Colour line2  = ink.withAlpha (0.18f);
    const juce::Colour aqua   { 0xff5fe0ff };
    const juce::Colour aqua2  { 0xffb8f3ff };
    const juce::Colour mod    { 0xffff8a6b };
    const juce::Colour ok     { 0xff7ff0b0 };
    const juce::Colour warn   { 0xffffd36b };
}

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
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
};

class ShizukuEditor;

struct Dest { juce::String mod; int id = 0; juce::String p; bool valid() const { return mod.isNotEmpty(); } };

/** Knob bound to a patch field (lambda get/set). Accepts modulation-source drops. */
class Knob : public juce::Component, public juce::DragAndDropTarget, public juce::SettableTooltipClient
{
public:
    struct Spec
    {
        juce::String label; double min = 0, max = 1, def = 0; int curve = 0; double pw = 3; double step = 0;
        std::function<juce::String (double)> fmt; std::function<double()> get; std::function<void (double)> set;
        Dest dest; double modScale = 1; bool structural = false;
    };
    Knob (ShizukuEditor& e, Spec s);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dropHi = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHi = false; repaint(); }
    void itemDropped (const SourceDetails&) override;
    std::function<void()> onChange;
    Spec spec;
private:
    double toN (double v) const;
    double fromN (double n) const;
    void setN (double n);
    ShizukuEditor& ed;
    double startN = 0; float startY = 0; bool dropHi = false;
};

class Toggle : public juce::Component, public juce::SettableTooltipClient
{
public:
    Toggle (std::function<bool()> g, std::function<void (bool)> s) : get (std::move (g)), set (std::move (s)) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition())) { set (! get()); repaint(); if (onChange) onChange(); } }
    std::function<void()> onChange;
private:
    std::function<bool()> get; std::function<void (bool)> set;
};

class Logo : public juce::Component
{
public:
    Logo();
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override { dancing = ! dancing; repaint(); }
    bool dancing = false; double phaseBeats = 0;
private:
    juce::Image img;
};

/** A titled panel that holds child components. */
class Panel : public juce::Component
{
public:
    explicit Panel (juce::String t) : title (std::move (t)) {}
    void paint (juce::Graphics&) override;
    juce::String title; bool dim = false; juce::String note; juce::Colour noteCol = col::warn;
};

class WTView : public juce::Component { public: WTView (ShizukuEditor& e, int i) : ed (e), idx (i) {} void paint (juce::Graphics&) override; private: ShizukuEditor& ed; int idx; };
class FilterView : public juce::Component { public: FilterView (ShizukuEditor& e, int j) : ed (e), idx (j) {} void paint (juce::Graphics&) override; private: ShizukuEditor& ed; int idx; };
class EnvView : public juce::Component { public: EnvView (ShizukuEditor& e, int i) : ed (e), idx (i) {} void paint (juce::Graphics&) override; private: ShizukuEditor& ed; int idx; };
class LfoView : public juce::Component
{
public:
    LfoView (ShizukuEditor& e, int i) : ed (e), idx (i) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { drag = -1; }
    void mouseDoubleClick (const juce::MouseEvent&) override;
private:
    juce::Point<float> toLfo (juce::Point<float>) const;
    juce::Point<float> toPx (float x, float y) const;
    int hit (juce::Point<float> px, bool& isCurve) const;
    ShizukuEditor& ed; int idx; int drag = -1; bool dragCurve = false; float y0 = 0, c0 = 0.5f;
};

class SourceChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    SourceChip (ShizukuEditor& e, juce::String key, juce::String label, int srcId) : ed (e), key (std::move (key)), label (std::move (label)), src (srcId) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    juce::String key;
private:
    ShizukuEditor& ed; juce::String label; int src; bool dragging = false;
};

/** Everything inside the (scaled) editor window. */
class Content : public juce::Component
{
public:
    static constexpr int W = 1320, H = 1010;
    explicit Content (ShizukuEditor& e);
    void paint (juce::Graphics&) override;
    void resized() override;
    ShizukuEditor& ed;
};

class ShizukuEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit ShizukuEditor (ShizukuProcessor&);
    ~ShizukuEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

    ShizukuProcessor& proc;
    Patch& P() { return proc.patch; }
    void edited (bool structural = false);
    void rebuild();
    void setStatus (const juce::String& s, bool error = false);
    void selectSource (const juce::String& key);
    void showTab (int t);
    void refreshModRings();
    std::vector<std::pair<juce::String, juce::String>> destOptions() const;
    juce::String destLabel (const Mod& m) const;
    std::unique_ptr<Knob> knob (Knob::Spec s);

    juce::String curSource { "env0" };
    int curTab = 0; juce::String fxRack { "fx" };

private:
    void timerCallback() override;
    void buildPresetBar();
    void buildOscPage();
    void buildFxPage();
    void buildMatrixPage();
    void buildGlobalPage();
    void buildModSection();
    void layoutAll();
    void openPresetMenu();
    void loadIndex (int i);
    void updateNotes();

    Look look;
    Content content;
    friend class Content;
    std::unique_ptr<juce::FileChooser> chooser;

    // header & preset bar
    Logo logo;
    std::unique_ptr<Knob> outKnob;
    juce::TextButton prevB { juce::String::fromUTF8 ("◀") }, nextB { juce::String::fromUTF8 ("▶") }, nameB, openB, tablesB, saveB;
    juce::Label status;
    std::array<juce::TextButton, 4> tabs;
    // pages
    juce::Component oscPage, fxPage, matrixPage, globalPage;
    juce::Viewport fxView, mxView; juce::Component fxInner, mxInner;
    std::vector<std::unique_ptr<juce::Component>> owned[5];   // per page + mod section
    juce::Component modArea;
    std::vector<std::unique_ptr<SourceChip>> chips;
    std::vector<Knob*> allKnobs;
    std::array<WTView*, 3> wtViews {}; std::array<FilterView*, 2> frViews {};
    juce::Component* modView = nullptr;
    juce::MidiKeyboardComponent kbd;
    juce::Slider mw, pb;
    juce::Label notes;
    int lastTablesGen = -1; int statusTimeout = 0; juce::uint32 lastTick = 0;
    bool needRebuild = false;
};
}
