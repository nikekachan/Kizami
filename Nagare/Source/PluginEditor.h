#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace nagare::ui
{
namespace col
{
    const juce::Colour bg     { 0xff030405 };
    const juce::Colour panel  { 0xff060809 };
    const juce::Colour ink    { 0xfff4f6f7 };
    const juce::Colour muted  { 0xff9aa3a9 };
    const juce::Colour line   = ink.withAlpha (0.22f);
    const juce::Colour stroke = ink.withAlpha (0.9f);
    const juce::Colour warm   { 0xffffd27a };
    const juce::Colour danger { 0xffff8a80 };
}

struct Fonts
{
    Fonts();
    juce::Font body (float h, bool bold = false) const;
    juce::Font display (float h) const;
    juce::Font script (float h) const;
    juce::Font mono (float h) const;
    juce::Typeface::Ptr comfortaa, greatVibes;
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
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowTitleFont() override;
};

class NagareEditor;

//==============================================================================
class Knob : public juce::Component
{
public:
    Knob (NagareProcessor& p, const juce::String& id);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider slider;
private:
    NagareProcessor& proc;
    juce::RangedAudioParameter* prm;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

/** Pill-shaped segmented switch bound to a choice parameter. */
class Seg : public juce::Component
{
public:
    Seg (NagareProcessor& p, const juce::String& id, juce::StringArray labels);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    int preferredWidth() const;
    std::function<bool (int)> isEnabledItem;
    std::function<void (int)> onPick;
private:
    juce::Rectangle<float> itemBounds (int i) const;
    NagareProcessor& proc;
    juce::String id;
    juce::StringArray labels;
};

class WaveView : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    explicit WaveView (NagareEditor& e);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dropping = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dropping = false; repaint(); }
    void invalidate() { peaksFor = -1; repaint(); }
private:
    int edgeAt (float px) const; // 1 start, 2 end, 3 inside, 0 none
    void applyDrag (float f);
    NagareEditor& editor;
    std::vector<std::pair<float, float>> peaks;
    int peaksFor = -1, peaksWidth = 0;
    int dragMode = 0;
    float f0 = 0, s0 = 0, e0 = 0;
    bool dropping = false;
};

class EnvView : public juce::Component
{
public:
    explicit EnvView (NagareProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    NagareProcessor& proc;
};

class KeyboardView : public juce::Component
{
public:
    explicit KeyboardView (NagareEditor& e) : editor (e) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    int firstNote() const;
    int lastNote() const { return firstNote() + 48; }
private:
    int noteAt (juce::Point<float>, float& velocity) const;
    juce::Rectangle<float> keyRect (int note) const;
    static bool isBlack (int n) { const int p = ((n % 12) + 12) % 12; return p == 1 || p == 3 || p == 6 || p == 8 || p == 10; }
    NagareEditor& editor;
    int mouseNote = -1;
};

/** Line-art sea creatures that drift gently. Purely decorative, kept clear of controls. */
class Creatures : public juce::Component
{
public:
    struct Item { int kind; juce::Rectangle<float> r; bool flip; float tilt; double phase; };
    Creatures();
    void add (int kind, juce::Rectangle<float> r, bool flip = false, float tilt = 0.0f);
    void paint (juce::Graphics&) override;
    void tick (double seconds) { time = seconds; repaint(); }
    bool constellation = false, sand = false;
    enum { jelly, fish, shell, weed, star, octo, angler, bub, kinds };
private:
    std::array<std::unique_ptr<juce::Drawable>, kinds> art;
    std::vector<Item> items;
    double time = 0;
};

//==============================================================================
class Content : public juce::Component
{
public:
    static constexpr int W = 1100, H = 1044;
    explicit Content (NagareEditor& e);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();                 // sync visibility / texts (timer)
    void refreshPresets();

    NagareEditor& editor;
    juce::Image logo;
    std::vector<std::array<float, 4>> stars;

    juce::Rectangle<int> srcPanel, envPanel, tonePanel, keyPanel;

    Seg sourceSeg, modeSeg, voiceSeg;
    juce::TextButton loadBtn, saveBtn, delBtn, octDown { "-" }, octUp { "+" }, trDown { "-" }, trUp { "+" }, tpReset;
    juce::ToggleButton keepLen, hold;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> keepAtt, holdAtt;
    juce::Label sampleName, detect, status, octVal, trVal;
    juce::ComboBox presets;
    WaveView wave;
    EnvView env;
    KeyboardView keyboard;
    juce::OwnedArray<Knob> srcKnobs, envKnobs, toneKnobs;
    Creatures reef, seabed;
    juce::Array<juce::File> userPresets;
    int statusTimeout = 0;
};

class NagareEditor : public juce::AudioProcessorEditor,
                     public juce::FileDragAndDropTarget,
                     private juce::Timer
{
public:
    explicit NagareEditor (NagareProcessor&);
    ~NagareEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;
    void mouseDown (const juce::MouseEvent&) override;
    void focusLost (FocusChangeType) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

    NagareProcessor& proc;
    void setStatus (const juce::String& s, bool error = false);
    void chooseSample();
    void loadSample (const juce::File& f);
    void setOctave (int o);
    void setTranspose (int t);
    juce::Rectangle<float> uiPressed; // unused, keeps layout code simple

private:
    void timerCallback() override;
    void savePreset();
    void deletePreset();

    Look look;
    Content content;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::AlertWindow> nameDialog;
    std::array<int, 17> keyNote {};
    bool spaceDown = false;
    std::array<bool, 4> cmdKeyDown {};
    int lastGeneration = -1;
    double startTime = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NagareEditor)
};

} // namespace nagare::ui

using NagareEditor = nagare::ui::NagareEditor;
