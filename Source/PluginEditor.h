#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace kizami::ui
{
namespace col
{
    const juce::Colour bg      { 0xff14171b };
    const juce::Colour panel   { 0xff1d2127 };
    const juce::Colour panel2  { 0xff242931 };
    const juce::Colour ink     { 0xffe6e9ed };
    const juce::Colour muted   { 0xff8e98a4 };
    const juce::Colour line    { 0xff2e353e };
    const juce::Colour pad     { 0xff2a3038 };
    const juce::Colour padBeat { 0xff353c47 };
    const juce::Colour accent  { 0xffff7432 };
    const juce::Colour danger  { 0xfff0645a };
    const juce::Colour solo    { 0xffe7bb2a };
    juce::Colour track (int index);
}

juce::Font font (float height, bool bold = false);
juce::String jp (const char* utf8);
juce::String noteName (int note); // FL Studio naming: 60 = C5

enum class InputMode { onOff, velocity, roll };

class KizamiLook : public juce::LookAndFeel_V4
{
public:
    KizamiLook();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int height) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
};

class KizamiEditor;

//==============================================================================
class StepGrid : public juce::Component
{
public:
    StepGrid (KizamiEditor& e, TrackPtr t, juce::Colour c);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setPlayStep (int step);
    int cellAt (juce::Point<float> p) const;
    juce::Rectangle<float> cellBounds (int visibleIndex) const;

private:
    KizamiEditor& editor;
    TrackPtr track;
    juce::Colour colour;
    int playStep = -1;
    int paintValue = -1; // drag painting: 1 = on, 0 = off
    int lastCell = -1;
};

//==============================================================================
class TrackRow : public juce::Component
{
public:
    TrackRow (KizamiEditor& e, TrackPtr t, int index);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    int getUid() const { return track->uid; }

    StepGrid grid;

private:
    void openDetail();

    KizamiEditor& editor;
    TrackPtr track;
    juce::Colour colour;
    juce::Label name;
    juce::ComboBox sound;
    juce::TextButton mute { "M" }, solo { "S" }, more { "..." };
};

//==============================================================================
class TrackDetail : public juce::Component
{
public:
    TrackDetail (KizamiEditor& e, TrackPtr t);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void updateLabels();

    KizamiEditor& editor;
    TrackPtr track;
    juce::Slider volume, pan, pitch, decay;
    juce::Label volL, panL, pitchL, decayL, noteL;
    juce::TextButton noteDown { "-" }, noteUp { "+" };
    juce::TextButton loadSample, randomise, clear, duplicate, moveUp, moveDown, remove;
};

//==============================================================================
class PadButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    PadButton (KizamiEditor& e, TrackPtr t, int index);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void flash();
    void tick();
    int getUid() const { return track->uid; }

private:
    KizamiEditor& editor;
    TrackPtr track;
    int index;
    juce::Colour colour;
    float glow = 0.0f;
    int lastCounter = 0;
};

//==============================================================================
/** Drag this into FL Studio's playlist or channel rack. */
class DragTile : public juce::Component, public juce::SettableTooltipClient
{
public:
    DragTile (KizamiEditor& e, bool midi);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    KizamiEditor& editor;
    bool isMidi;
    bool dragging = false;
};

class PlayButton : public juce::Button
{
public:
    PlayButton() : juce::Button ("play") {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    bool playing = false;
};

class RecButton : public juce::Button
{
public:
    RecButton() : juce::Button ("rec") {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    bool armed = false;
    bool blink = false;
};

//==============================================================================
class KizamiEditor : public juce::AudioProcessorEditor,
                     public juce::FileDragAndDropTarget,
                     public juce::DragAndDropContainer,
                     private juce::Timer
{
public:
    explicit KizamiEditor (KizamiProcessor&);
    ~KizamiEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int x, int y) override;

    // used by child components
    KizamiProcessor& proc;
    Engine& engine() { return proc.engine; }
    InputMode mode = InputMode::onOff;
    int page = 0;           // which 4-bar page is visible
    int visibleFirstStep() const { return page * 4 * kStepsPerBar; }
    int visibleStepCount() const;
    bool isPlaying() const { return proc.engine.currentStep.load() >= 0; }
    void audition (const Track& t, float vel = 1.0f);
    void rebuild();
    void requestRebuild() { rebuildPending = true; }
    void setStatus (const juce::String& text, bool error = false);
    void chooseSample (int uid);
    void showDetail (TrackPtr t, juce::Component& anchor);
    juce::File makeDragFile (bool midi);

private:
    void timerCallback() override;
    void layoutTracks();
    void syncControls();
    void refreshPresetList();
    void loadPresetFromBox();
    void savePreset();
    void deletePreset();
    void exportToFolder();
    void setBars (int bars, bool copy);
    void addTrack (Sound s);
    void updatePageButtons();

    KizamiLook look;
    juce::TooltipWindow tooltips { this, 700 };

    PlayButton play;
    RecButton rec;
    juce::Label bpmLabel, swingLabel, masterLabel, barsLabel, modeLabel, titleLabel, subtitleLabel;
    juce::Slider swing, master;
    juce::TextButton barMinus { "-" }, barCopy, barEmpty;
    juce::TextButton modeOn, modeVel, modeRoll;
    juce::ToggleButton metronome, follow;

    juce::Label presetLabel;
    juce::ComboBox presets;
    juce::TextButton presetSave, presetDelete;
    juce::OwnedArray<juce::TextButton> pageButtons;

    juce::Label padsTitle, padsHint;
    juce::Component padArea;
    juce::OwnedArray<PadButton> pads;

    juce::Viewport viewport;
    juce::Component trackList;
    juce::OwnedArray<TrackRow> rows;

    juce::ComboBox addSound;
    juce::Label loopsLabel;
    juce::ComboBox loops;
    DragTile wavTile { *this, false }, midiTile { *this, true };
    juce::TextButton exportButton;
    juce::Label status;

    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::AlertWindow> nameDialog;
    juce::Array<juce::File> userPresetFiles;

    bool rebuildPending = false;
    int lastGeneration = -1;
    int lastStep = -2;
    int blinkCounter = 0;
    int statusTimeout = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KizamiEditor)
};

} // namespace kizami::ui

using KizamiEditor = kizami::ui::KizamiEditor;
