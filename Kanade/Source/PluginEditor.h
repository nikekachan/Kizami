#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include <set>
#include <optional>

namespace kanade::ui
{
namespace col
{
    const juce::Colour bg     { 0xff080c1a };
    const juce::Colour panel  { 0xff10172b };
    const juce::Colour pane   { 0xff172038 };
    const juce::Colour pane2  { 0xff1f2a47 };
    const juce::Colour ink    { 0xffeef1ff };
    const juce::Colour muted  { 0xff93a0c4 };
    const juce::Colour line   = ink.withAlpha (0.09f);
    const juce::Colour line2  = ink.withAlpha (0.2f);
    const juce::Colour gold   { 0xffffc861 };
    const juce::Colour gold2  { 0xffffdf9e };
    const juce::Colour sel    { 0xffff7e8e };
    const juce::Colour curve  { 0xff59e3cf };
    const juce::Colour danger { 0xffff8a80 };
    const juce::Colour rec    { 0xffff5a5a };
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
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
};

class KanadeEditor;

/** A knob with a caption and a value line (not a host parameter: it acts on the notes). */
class Knob : public juce::Component
{
public:
    Knob (const juce::String& name, juce::Colour c, std::function<juce::String (double)> fmt);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider slider;
private:
    juce::String name;
    std::function<juce::String (double)> fmt;
};

/** The astronaut; dances on the host tempo when clicked. */
class Logo : public juce::Component
{
public:
    explicit Logo (KanadeEditor& e);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool dancing = false;
    double phaseBeats = 0;
private:
    KanadeEditor& editor;
    juce::Image img;
};

/** Little square: drag it to FL's playlist (or click to save the WAV). */
class DragSquare : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit DragSquare (KanadeEditor& e) : editor (e) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    KanadeEditor& editor;
    bool dragging = false;
};

/** Small panel under the detailed note edit: voice thickness, snap, reset. */
class MiniPanel : public juce::Component
{
public:
    explicit MiniPanel (KanadeEditor& e);
    void paint (juce::Graphics&) override;
    void resized() override;
    void show (const Note&);
    juce::Slider form;
    juce::TextButton snap, reset, close;
private:
    KanadeEditor& editor;
    juce::String title;
};

class PianoRoll : public juce::Component
{
public:
    explicit PianoRoll (KanadeEditor& e);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void modifierKeysChanged (const juce::ModifierKeys&) override;

    void fitView();
    void zoomH (float f);
    void zoomV (float f);
    void refreshCurves();
    void closeAdv();
    void openAdv (int noteId);
    void tick();                           // called by the editor's timer

    static constexpr float KB = 92, RH = 26, WH = 58;
    struct View { double t0 = 0, pps = 80; float mTop = 76, lane = 30; } view;
    std::set<int> sel;
    bool splitTool = false, follow = true;
    double cursorSec = 0;
    int adv = -1;
    MiniPanel mini;

private:
    KanadeEditor& editor;
    KanadeProcessor& proc;
    Curves curves; int curvesGen = -1;
    // geometry
    double fr() const;
    double frameSec (double f) const;
    float tx (double t) const { return KB + (float) ((t - view.t0) * view.pps); }
    double xt (float px) const { return view.t0 + (px - KB) / view.pps; }
    float py (float m) const { return RH + (view.mTop - m) * view.lane; }
    float ym (float y) const { return view.mTop - (y - RH) / view.lane; }
    float pBot() const { return (float) getHeight() - WH; }
    Note* hitNote (juce::Point<float>);
    Note* noteById (int id);
    bool toolSplitNow() const;
    double snapCut (double t) const;
    float snapPitch (float m, bool freeMove) const;
    void updHover();
    void doSplit (Note&, float x);
    // detailed edit
    struct AdvGeom { Note* n; float x0, w, y0, cx; double dur; float mx (double ms) const { return x0 + (float) (ms / dur) * w; } float ex (double ms) const { return x0 + w - (float) (ms / dur) * w; } };
    std::optional<AdvGeom> advGeom();
    juce::String advHit (juce::Point<float>);
    void drawAdv (juce::Graphics&);
    void drawKeyboard (juce::Graphics&);
    void drawRuler (juce::Graphics&);
    void drawWave (juce::Graphics&);

    struct Drag { int type = 0; juce::Point<float> p0, p1; int prim = -1; std::vector<std::pair<int, float>> orig; bool moved = false; std::set<int> base; } drag;  // 1 move, 2 band
    struct AdvDrag { juce::String k; juce::Point<float> p0; Note v; } advDrag; bool advDragging = false;
    std::optional<juce::Point<float>> mouse;
    struct HoverCut { float x; int id; }; std::optional<HoverCut> hoverCut;
    bool shiftHeld = false;
    juce::uint32 lastLive = 0;
    static constexpr float BAND = 64;
};

class Content : public juce::Component
{
public:
    static constexpr int W = 1240, H = 900;
    explicit Content (KanadeEditor& e);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

    KanadeEditor& editor;
    Logo logo;
    juce::TextButton scaleBtn, snapBtn, undoBtn, redoBtn, exportBtn;
    DragSquare dragSq;
    juce::TextButton playBtn, followBtn, stopBtn, afterBtn, beforeBtn, recBtn, openBtn, clearBtn, selBtn, splitBtn, mergeBtn;
    juce::TextButton zhOut, zhIn, zvOut, zvIn, zFit, applyBtn;
    juce::Label timeLbl, bpmLbl, status, targetLbl;
    juce::ComboBox cutBox;
    Knob kC, kV, kT;
    juce::Slider formant;
    PianoRoll roll;
    juce::uint32 clearArm = 0;
};

class KanadeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit KanadeEditor (KanadeProcessor&);
    ~KanadeEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    KanadeProcessor& proc;
    void setStatus (const juce::String&, bool error = false);
    void edited (const std::vector<int>& ids, bool all = false);   // notes changed: redraw + background render
    std::vector<Note*> targets();
    void applyKnob (int which, bool undoFirst);   // 0 center, 1 variation, 2 trans, 3 all
    void showScaleMenu();
    juce::String scaleName() const;
    void setTool (bool split);
    std::unique_ptr<juce::FileChooser> chooser;
    Look look;
    Content content;
    juce::TooltipWindow tips { this, 600 };
    bool knobUndoPending = true;
private:
    void timerCallback() override;
    int lastGen = -1, statusTimeout = 0;
    const Session* lastSession = nullptr;
    double danceBeats = 0; juce::uint32 lastTick = 0;
    juce::uint32 recArm = 0;
};
} // namespace kanade::ui

using KanadeEditor = kanade::ui::KanadeEditor;
