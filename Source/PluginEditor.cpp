#include "PluginEditor.h"
#include "Exporter.h"

namespace kizami::ui
{

juce::Colour col::track (int index)
{
    static const juce::Colour c[] = { juce::Colour (0xfff0564c), juce::Colour (0xffff8f3a),
                                      juce::Colour (0xffe7bb2a), juce::Colour (0xff6f9be6) };
    return c[(size_t) (index % 4)];
}

juce::Font font (float height, bool bold)
{
    auto opts = juce::FontOptions (height);
    if (bold) opts = opts.withStyle ("Bold");
    return juce::Font (opts);
}

juce::String jp (const char* utf8) { return juce::String::fromUTF8 (utf8); }

juce::String noteName (int note)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[note % 12]) + juce::String (note / 12);
}

static const char* const padKeys = "asdfghjklzxcvbnm";

//==============================================================================
KizamiLook::KizamiLook()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::TextButton::buttonColourId, col::panel2);
    setColour (juce::TextButton::buttonOnColourId, col::ink);
    setColour (juce::TextButton::textColourOffId, col::ink);
    setColour (juce::TextButton::textColourOnId, col::panel);
    setColour (juce::ComboBox::backgroundColourId, col::panel2);
    setColour (juce::ComboBox::outlineColourId, col::line);
    setColour (juce::ComboBox::textColourId, col::ink);
    setColour (juce::ComboBox::arrowColourId, col::muted);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, col::bg);
    setColour (juce::PopupMenu::headerTextColourId, col::muted);
    setColour (juce::Label::textColourId, col::ink);
    setColour (juce::Label::textWhenEditingColourId, col::ink);
    setColour (juce::Label::backgroundWhenEditingColourId, col::panel2);
    setColour (juce::Label::outlineWhenEditingColourId, col::accent);
    setColour (juce::TextEditor::backgroundColourId, col::panel2);
    setColour (juce::TextEditor::textColourId, col::ink);
    setColour (juce::TextEditor::outlineColourId, col::line);
    setColour (juce::TextEditor::focusedOutlineColourId, col::accent);
    setColour (juce::TextEditor::highlightColourId, col::accent.withAlpha (0.4f));
    setColour (juce::CaretComponent::caretColourId, col::ink);
    setColour (juce::Slider::thumbColourId, col::accent);
    setColour (juce::Slider::trackColourId, col::accent);
    setColour (juce::Slider::backgroundColourId, col::line);
    setColour (juce::ToggleButton::textColourId, col::ink);
    setColour (juce::ToggleButton::tickColourId, col::accent);
    setColour (juce::ToggleButton::tickDisabledColourId, col::muted);
    setColour (juce::ScrollBar::thumbColourId, col::line.brighter (0.3f));
    setColour (juce::AlertWindow::backgroundColourId, col::panel);
    setColour (juce::AlertWindow::textColourId, col::ink);
    setColour (juce::AlertWindow::outlineColourId, col::line);
    setColour (juce::BubbleComponent::backgroundColourId, col::panel);
    setColour (juce::BubbleComponent::outlineColourId, col::line);
}

void KizamiLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto c = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : base;
    if (down) c = c.darker (0.15f);
    else if (over) c = c.brighter (0.08f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (col::line.brighter (over ? 0.25f : 0.0f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

juce::Font KizamiLook::getTextButtonFont (juce::TextButton&, int h) { return font (juce::jmin (13.0f, h * 0.55f)); }
juce::Font KizamiLook::getComboBoxFont (juce::ComboBox&) { return font (13.0f); }
juce::Font KizamiLook::getLabelFont (juce::Label& l) { return l.getFont(); }
juce::Font KizamiLook::getPopupMenuFont() { return font (14.0f); }

void KizamiLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    juce::Path arrow;
    const float cx = (float) w - 11.0f, cy = (float) h * 0.5f;
    arrow.addTriangle (cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

void KizamiLook::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                   juce::Slider::SliderStyle, juce::Slider& s)
{
    const float cy = (float) y + (float) h * 0.5f;
    g.setColour (col::line);
    g.fillRoundedRectangle ((float) x, cy - 2.0f, (float) w, 4.0f, 2.0f);
    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float zero = bipolar ? (float) s.getPositionOfValue (0.0) : (float) x;
    g.setColour (col::accent);
    g.fillRoundedRectangle (juce::jmin (zero, pos), cy - 2.0f, std::abs (pos - zero), 4.0f, 2.0f);
    g.setColour (col::ink);
    g.fillEllipse (pos - 6.0f, cy - 6.0f, 12.0f, 12.0f);
    g.setColour (col::accent);
    g.drawEllipse (pos - 6.0f, cy - 6.0f, 12.0f, 12.0f, 2.0f);
}

void KizamiLook::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    auto box = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.22f);
    g.setColour (b.getToggleState() ? col::accent : col::panel2);
    g.fillRoundedRectangle (box, 3.0f);
    g.setColour (b.getToggleState() ? col::accent : col::line.brighter (over ? 0.3f : 0.0f));
    g.drawRoundedRectangle (box, 3.0f, 1.0f);
    if (b.getToggleState())
    {
        juce::Path tick;
        tick.startNewSubPath (box.getX() + box.getWidth() * 0.22f, box.getCentreY());
        tick.lineTo (box.getX() + box.getWidth() * 0.43f, box.getBottom() - box.getHeight() * 0.25f);
        tick.lineTo (box.getRight() - box.getWidth() * 0.2f, box.getY() + box.getHeight() * 0.25f);
        g.setColour (col::bg);
        g.strokePath (tick, juce::PathStrokeType (2.0f));
    }
    g.setColour (col::ink);
    g.setFont (font (13.0f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (4.0f), juce::Justification::centredLeft);
}

//==============================================================================
void PlayButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (col::accent.brighter (over ? 0.1f : 0.0f).darker (down ? 0.15f : 0.0f));
    g.fillEllipse (r);
    g.setColour (col::bg);
    const auto c = r.getCentre();
    const float s = r.getWidth() * 0.2f;
    if (playing)
    {
        g.fillRect (c.x - s, c.y - s, s * 0.75f, s * 2.0f);
        g.fillRect (c.x + s * 0.25f, c.y - s, s * 0.75f, s * 2.0f);
    }
    else
    {
        juce::Path p;
        p.addTriangle (c.x - s * 0.7f, c.y - s * 1.15f, c.x - s * 0.7f, c.y + s * 1.15f, c.x + s * 1.2f, c.y);
        g.fillPath (p);
    }
}

void RecButton::paintButton (juce::Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (armed ? col::danger : col::panel2.brighter (over ? 0.08f : 0.0f));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (armed ? col::danger : col::line);
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
    const float d = 10.0f;
    g.setColour (armed ? (blink ? col::bg.withAlpha (0.35f) : col::bg) : col::danger);
    g.fillEllipse (r.getX() + 11.0f, r.getCentreY() - d * 0.5f, d, d);
    g.setColour (armed ? col::bg : col::ink);
    g.setFont (font (12.0f, true));
    g.drawText ("REC", r.withTrimmedLeft (26.0f).withTrimmedRight (8.0f), juce::Justification::centred);
}

//==============================================================================
StepGrid::StepGrid (KizamiEditor& e, TrackPtr t, juce::Colour c) : editor (e), track (std::move (t)), colour (c)
{
    setRepaintsOnMouseActivity (false);
}

juce::Rectangle<float> StepGrid::cellBounds (int vi) const
{
    const int n = editor.visibleStepCount();
    const float barGap = 6.0f, gap = 3.0f;
    const int barsShown = juce::jmax (1, n / kStepsPerBar);
    const float totalGaps = gap * (float) (n - barsShown) + barGap * (float) (barsShown - 1);
    const float cw = ((float) getWidth() - totalGaps) / (float) n;
    const int bar = vi / kStepsPerBar, inBar = vi % kStepsPerBar;
    const float x = (float) bar * (cw * kStepsPerBar + gap * (kStepsPerBar - 1) + barGap) + (float) inBar * (cw + gap);
    return { x, 2.0f, cw, (float) getHeight() - 4.0f };
}

int StepGrid::cellAt (juce::Point<float> p) const
{
    const int n = editor.visibleStepCount();
    for (int i = 0; i < n; ++i)
    {
        auto b = cellBounds (i).expanded (1.6f, 4.0f);
        if (b.contains (p)) return i;
    }
    return -1;
}

void StepGrid::paint (juce::Graphics& g)
{
    const int n = editor.visibleStepCount();
    const int first = editor.visibleFirstStep();
    const bool muted = track->mute.load();
    for (int i = 0; i < n; ++i)
    {
        const int step = first + i;
        auto r = cellBounds (i);
        const float v = track->vel[step].load();
        juce::Colour c = (i % 4 == 0) ? col::padBeat : col::pad;
        if (v > 0.0f) c = col::pad.interpolatedWith (colour, 0.35f + 0.65f * v);
        if (muted) c = c.withMultipliedAlpha (0.4f);
        g.setColour (c);
        g.fillRoundedRectangle (r, 3.0f);
        const int roll = track->roll[step].load();
        if (v > 0.0f && roll > 1 && r.getWidth() > 9.0f)
        {
            g.setColour (col::bg.withAlpha (0.85f));
            g.setFont (font (9.5f, true));
            g.drawText (juce::String::charToString (0xd7) + juce::String (roll), r.withTrimmedTop (r.getHeight() * 0.45f),
                        juce::Justification::centred);
        }
        if (step == playStep)
        {
            g.setColour (col::ink);
            g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.6f);
        }
    }
}

void StepGrid::setPlayStep (int s)
{
    if (s == playStep) return;
    playStep = s;
    repaint();
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    const int vi = cellAt (e.position);
    if (vi < 0) return;
    const int step = editor.visibleFirstStep() + vi;
    auto& t = *track;
    lastCell = vi;

    switch (editor.mode)
    {
        case InputMode::onOff:
            paintValue = t.vel[step].load() > 0.0f ? 0 : 1;
            t.vel[step] = paintValue ? 1.0f : 0.0f;
            t.roll[step] = 1;
            if (paintValue) editor.audition (t);
            break;
        case InputMode::velocity:
        {
            const float v = t.vel[step].load();
            const float nv = v <= 0.0f ? 1.0f : v > 0.85f ? 0.7f : v > 0.55f ? 0.4f : 1.0f;
            t.vel[step] = nv;
            editor.audition (t, nv);
            break;
        }
        case InputMode::roll:
        {
            if (t.vel[step].load() <= 0.0f) { t.vel[step] = 1.0f; t.roll[step] = 2; }
            else t.roll[step] = (uint8_t) (t.roll[step].load() >= 4 ? 1 : t.roll[step].load() + 1);
            break;
        }
    }
    repaint();
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (editor.mode != InputMode::onOff || paintValue < 0) return;
    const int vi = cellAt (e.position);
    if (vi < 0 || vi == lastCell) return;
    lastCell = vi;
    const int step = editor.visibleFirstStep() + vi;
    const bool wasOn = track->vel[step].load() > 0.0f;
    if (paintValue == 1 && ! wasOn) { track->vel[step] = 1.0f; track->roll[step] = 1; editor.audition (*track); }
    if (paintValue == 0 && wasOn) { track->vel[step] = 0.0f; track->roll[step] = 1; }
    repaint();
}

void StepGrid::mouseUp (const juce::MouseEvent&) { paintValue = -1; lastCell = -1; }

//==============================================================================
TrackRow::TrackRow (KizamiEditor& e, TrackPtr t, int index)
    : grid (e, t, col::track (index)), editor (e), track (std::move (t)), colour (col::track (index))
{
    addAndMakeVisible (grid);

    name.setFont (font (13.5f, true));
    name.setEditable (false, true, false);
    name.setTooltip (jp ("ダブルクリックで名前を変更"));
    name.onTextChange = [this] { track->name = name.getText().substring (0, 40); editor.requestRebuild(); };
    addAndMakeVisible (name);

    for (int i = 0; i < (int) Sound::count; ++i)
        sound.addItem (soundLabel ((Sound) i), i + 1);
    sound.onChange = [this] {
        const auto s = (Sound) (sound.getSelectedId() - 1);
        if (s == track->sound) return;
        if (s == Sound::sample && track->sample == nullptr) { editor.chooseSample (track->uid); refresh(); return; }
        editor.engine().setSound (track->uid, s);
        editor.audition (*track);
        editor.requestRebuild();
    };
    addAndMakeVisible (sound);

    mute.setClickingTogglesState (true);
    solo.setClickingTogglesState (true);
    mute.setTooltip (jp ("ミュート"));
    solo.setTooltip (jp ("ソロ"));
    mute.setColour (juce::TextButton::buttonOnColourId, col::muted);
    solo.setColour (juce::TextButton::buttonOnColourId, col::solo);
    solo.setColour (juce::TextButton::textColourOnId, col::bg);
    mute.onClick = [this] { track->mute = mute.getToggleState(); grid.repaint(); };
    solo.onClick = [this] { track->solo = solo.getToggleState(); };
    more.setTooltip (jp ("音量・パン・ピッチ・長さ・サンプル読み込みなど"));
    more.onClick = [this] { openDetail(); };
    addAndMakeVisible (mute);
    addAndMakeVisible (solo);
    addAndMakeVisible (more);
    refresh();
}

void TrackRow::refresh()
{
    name.setText (track->name, juce::dontSendNotification);
    sound.setSelectedId ((int) track->sound + 1, juce::dontSendNotification);
    mute.setToggleState (track->mute.load(), juce::dontSendNotification);
    solo.setToggleState (track->solo.load(), juce::dontSendNotification);
    grid.repaint();
}

void TrackRow::openDetail() { editor.showDetail (track, more); }

void TrackRow::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f, 1.5f);
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
    g.setColour (colour);
    g.fillRoundedRectangle (r.getX() + 8.0f, r.getY() + 8.0f, 5.0f, r.getHeight() - 16.0f, 2.0f);
}

void TrackRow::resized()
{
    auto r = getLocalBounds().reduced (6, 5);
    r.removeFromLeft (14);
    name.setBounds (r.removeFromLeft (112));
    r.removeFromLeft (4);
    sound.setBounds (r.removeFromLeft (118).reduced (0, 2));
    r.removeFromLeft (6);
    mute.setBounds (r.removeFromLeft (26).reduced (0, 2));
    r.removeFromLeft (3);
    solo.setBounds (r.removeFromLeft (26).reduced (0, 2));
    r.removeFromLeft (3);
    more.setBounds (r.removeFromLeft (30).reduced (0, 2));
    r.removeFromLeft (10);
    grid.setBounds (r);
}

//==============================================================================
TrackDetail::TrackDetail (KizamiEditor& e, TrackPtr t) : editor (e), track (std::move (t))
{
    auto setup = [this] (juce::Slider& s, juce::Label& l, double lo, double hi, double step, double val) {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRange (lo, hi, step);
        s.setValue (val, juce::dontSendNotification);
        addAndMakeVisible (s);
        l.setFont (font (12.0f));
        l.setColour (juce::Label::textColourId, col::muted);
        addAndMakeVisible (l);
    };
    setup (volume, volL, 0, 100, 1, track->volume.load() * 100.0);
    setup (pan, panL, -100, 100, 1, track->pan.load() * 100.0);
    setup (pitch, pitchL, -12, 12, 1, track->pitch);
    setup (decay, decayL, 25, 300, 1, track->decay * 100.0);
    pan.setDoubleClickReturnValue (true, 0.0);
    pitch.setDoubleClickReturnValue (true, 0.0);
    decay.setDoubleClickReturnValue (true, 100.0);

    volume.onValueChange = [this] { track->volume = (float) volume.getValue() / 100.0f; updateLabels(); };
    pan.onValueChange = [this] { track->pan = (float) pan.getValue() / 100.0f; updateLabels(); };
    pitch.onValueChange = [this] { updateLabels(); };
    decay.onValueChange = [this] { updateLabels(); };
    auto apply = [this] {
        editor.engine().setPitchDecay (track->uid, (float) pitch.getValue(), (float) decay.getValue() / 100.0f);
        editor.audition (*track);
    };
    pitch.onDragEnd = apply;
    decay.onDragEnd = apply;

    noteL.setFont (font (12.0f));
    noteL.setColour (juce::Label::textColourId, col::muted);
    addAndMakeVisible (noteL);
    noteDown.onClick = [this] { track->note = juce::jlimit (0, 127, track->effectiveNote() - 1); updateLabels(); editor.requestRebuild(); };
    noteUp.onClick = [this] { track->note = juce::jlimit (0, 127, track->effectiveNote() + 1); updateLabels(); editor.requestRebuild(); };
    addAndMakeVisible (noteDown);
    addAndMakeVisible (noteUp);

    auto button = [this] (juce::TextButton& b, const char* text, std::function<void()> fn) {
        b.setButtonText (jp (text));
        b.onClick = std::move (fn);
        addAndMakeVisible (b);
    };
    button (loadSample, "サンプルを読み込む", [this] { editor.chooseSample (track->uid); });
    button (randomise, "ランダム", [this] {
        static const float density[] = { .22f, .2f, .1f, .1f, .6f, .12f, .2f, .15f, .25f, .5f, .15f, .05f, .25f };
        const float p = density[(int) track->sound];
        auto& rng = juce::Random::getSystemRandom();
        const int total = editor.engine().getTotalSteps();
        for (int i = 0; i < total; ++i)
        {
            const bool on = rng.nextFloat() < p;
            track->vel[i] = on ? (rng.nextFloat() < 0.6f ? 1.0f : 0.7f) : 0.0f;
            track->roll[i] = (uint8_t) ((on && track->sound == Sound::chh && rng.nextFloat() < 0.15f) ? 2 + rng.nextInt (2) : 1);
        }
        editor.requestRebuild();
    });
    button (clear, "クリア", [this] {
        for (int i = 0; i < kMaxSteps; ++i) { track->vel[i] = 0.0f; track->roll[i] = 1; }
        editor.requestRebuild();
    });
    button (duplicate, "複製", [this] { editor.engine().duplicateTrack (track->uid); editor.requestRebuild(); });
    button (moveUp, "上へ", [this] { editor.engine().moveTrack (track->uid, -1); editor.requestRebuild(); });
    button (moveDown, "下へ", [this] { editor.engine().moveTrack (track->uid, 1); editor.requestRebuild(); });
    button (remove, "削除", [this] {
        const auto uid = track->uid;
        if (auto* box = findParentComponentOfClass<juce::CallOutBox>()) box->dismiss();
        editor.engine().removeTrack (uid);
        editor.requestRebuild();
    });
    remove.setColour (juce::TextButton::textColourOffId, col::danger);

    updateLabels();
    setSize (330, 262);
}

void TrackDetail::updateLabels()
{
    volL.setText (jp ("音量  ") + juce::String ((int) volume.getValue()), juce::dontSendNotification);
    const int p = (int) pan.getValue();
    panL.setText (jp ("パン  ") + (p == 0 ? juce::String ("C") : (p < 0 ? "L" + juce::String (-p) : "R" + juce::String (p))), juce::dontSendNotification);
    const int st = (int) pitch.getValue();
    pitchL.setText (jp ("ピッチ  ") + (st > 0 ? "+" : "") + juce::String (st), juce::dontSendNotification);
    decayL.setText (jp ("長さ  ") + juce::String (decay.getValue() / 100.0, 2) + "x", juce::dontSendNotification);
    noteL.setText (jp ("MIDIノート  ") + noteName (track->effectiveNote()), juce::dontSendNotification);
}

void TrackDetail::paint (juce::Graphics& g)
{
    g.fillAll (col::panel);
    g.setColour (col::ink);
    g.setFont (font (14.0f, true));
    g.drawText (track->name, getLocalBounds().reduced (12, 8).removeFromTop (20), juce::Justification::centredLeft);
}

void TrackDetail::resized()
{
    auto r = getLocalBounds().reduced (12, 8);
    r.removeFromTop (24);
    auto row = [&r] (juce::Label& l, juce::Slider& s) {
        auto line = r.removeFromTop (28);
        l.setBounds (line.removeFromLeft (110));
        s.setBounds (line);
    };
    row (volL, volume);
    row (panL, pan);
    row (pitchL, pitch);
    row (decayL, decay);
    auto nl = r.removeFromTop (30);
    noteL.setBounds (nl.removeFromLeft (150));
    noteDown.setBounds (nl.removeFromLeft (28).reduced (0, 3));
    nl.removeFromLeft (4);
    noteUp.setBounds (nl.removeFromLeft (28).reduced (0, 3));
    r.removeFromTop (6);
    auto b1 = r.removeFromTop (28);
    loadSample.setBounds (b1.removeFromLeft (150));
    b1.removeFromLeft (6);
    randomise.setBounds (b1.removeFromLeft (70));
    b1.removeFromLeft (6);
    clear.setBounds (b1);
    r.removeFromTop (6);
    auto b2 = r.removeFromTop (28);
    const int w = (b2.getWidth() - 18) / 4;
    duplicate.setBounds (b2.removeFromLeft (w)); b2.removeFromLeft (6);
    moveUp.setBounds (b2.removeFromLeft (w)); b2.removeFromLeft (6);
    moveDown.setBounds (b2.removeFromLeft (w)); b2.removeFromLeft (6);
    remove.setBounds (b2);
}

//==============================================================================
PadButton::PadButton (KizamiEditor& e, TrackPtr t, int i)
    : editor (e), track (std::move (t)), index (i), colour (col::track (i))
{
    lastCounter = track->hitCounter.load();
    const juce::String key = index < 16 ? juce::String::charToString ((juce::juce_wchar) padKeys[index]).toUpperCase() : juce::String();
    setTitle (track->name);
    setTooltip (track->name + (key.isNotEmpty() ? jp ("  (キー ") + key + ")" : juce::String()) + "  MIDI " + noteName (track->effectiveNote()));
}

void PadButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (col::panel2.interpolatedWith (colour, glow * 0.6f));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
    g.setColour (colour);
    g.fillRoundedRectangle (r.removeFromBottom (4.0f).reduced (4.0f, 0.0f), 2.0f);

    auto text = getLocalBounds().reduced (9, 7);
    g.setColour (col::ink);
    g.setFont (font (13.0f, true));
    g.drawFittedText (track->name, text.removeFromTop (18), juce::Justification::topLeft, 1, 0.8f);
    g.setColour (col::muted);
    g.setFont (font (11.0f));
    juce::String info;
    if (index < 16) info << juce::String::charToString ((juce::juce_wchar) padKeys[index]).toUpperCase() << "   ";
    info << noteName (track->effectiveNote());
    g.drawText (info, text.removeFromBottom (14), juce::Justification::bottomLeft);
}

void PadButton::mouseDown (const juce::MouseEvent& e)
{
    const float vel = juce::jlimit (0.4f, 1.0f, 0.55f + 0.45f * (1.0f - (float) e.y / (float) juce::jmax (1, getHeight())) * 1.6f);
    editor.engine().pushPadEvent (track->uid, vel, true);
}

void PadButton::flash() { glow = 1.0f; repaint(); }

void PadButton::tick()
{
    const int c = track->hitCounter.load();
    if (c != lastCounter) { lastCounter = c; glow = 1.0f; repaint(); return; }
    if (glow > 0.0f) { glow = juce::jmax (0.0f, glow - 0.25f); repaint(); }
}

//==============================================================================
DragTile::DragTile (KizamiEditor& e, bool midi) : editor (e), isMidi (midi)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip (isMidi ? jp ("FL Studioのピアノロールやプレイリストにドラッグ")
                       : jp ("FL Studioのプレイリストやチャンネルにドラッグ"));
}

void DragTile::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool over = isMouseOver (true);
    g.setColour (over ? col::panel2.brighter (0.06f) : col::panel2);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (over ? col::accent : col::line.brighter (0.2f));
    const float dashes[] = { 4.0f, 3.0f };
    juce::Path p;
    p.addRoundedRectangle (r.reduced (0.5f), 7.0f);
    juce::Path dashed;
    juce::PathStrokeType (1.2f).createDashedStroke (dashed, p, dashes, 2);
    g.fillPath (dashed);

    auto t = getLocalBounds().reduced (12, 6);
    g.setColour (col::ink);
    g.setFont (font (14.0f, true));
    g.drawText (isMidi ? "MIDI" : "WAV", t.removeFromTop (t.getHeight() / 2), juce::Justification::bottomLeft);
    g.setColour (col::muted);
    g.setFont (font (11.5f));
    g.drawText (dragging ? jp ("ドロップしてください") : jp ("ドラッグしてFLへ"), t, juce::Justification::topLeft);
}

void DragTile::mouseDown (const juce::MouseEvent&) { dragging = false; }

void DragTile::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6) return;
    dragging = true;
    repaint();
    const auto file = editor.makeDragFile (isMidi);
    if (file.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                                   [safe = juce::Component::SafePointer<DragTile> (this)] {
                                                                       if (safe != nullptr) { safe->dragging = false; safe->repaint(); }
                                                                   });
    else
        dragging = false;
}

//==============================================================================
KizamiEditor::KizamiEditor (KizamiProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);
    setWantsKeyboardFocus (true);

    titleLabel.setText ("KIZAMI", juce::dontSendNotification);
    titleLabel.setFont (font (24.0f, true));
    titleLabel.setColour (juce::Label::textColourId, col::ink);
    addAndMakeVisible (titleLabel);
    subtitleLabel.setText (jp ("ドラムループ・ステップシーケンサー"), juce::dontSendNotification);
    subtitleLabel.setFont (font (11.5f));
    subtitleLabel.setColour (juce::Label::textColourId, col::muted);
    addAndMakeVisible (subtitleLabel);

    play.setTooltip (jp ("プレビュー再生(Space)。FL Studioを再生すると自動で同期します"));
    play.onClick = [this] {
        if (engine().hostPlaying.load()) { setStatus (jp ("FL Studioの再生中は、FL側で停止してください。")); return; }
        engine().internalPlay = ! engine().internalPlay.load();
    };
    addAndMakeVisible (play);
    rec.setTooltip (jp ("録音:パッド/キーボード/MIDIで叩いたタイミングを一番近いステップに記録"));
    rec.onClick = [this] {
        const bool on = ! engine().recording.load();
        engine().recording = on;
        if (on && ! isPlaying()) engine().internalPlay = true;
    };
    addAndMakeVisible (rec);

    auto smallLabel = [this] (juce::Label& l, const juce::String& text) {
        l.setText (text, juce::dontSendNotification);
        l.setFont (font (11.0f, true));
        l.setColour (juce::Label::textColourId, col::muted);
        addAndMakeVisible (l);
    };
    bpmLabel.setFont (font (20.0f, true));
    bpmLabel.setColour (juce::Label::textColourId, col::ink);
    bpmLabel.setTooltip (jp ("テンポはFL Studioに合わせて自動で変わります"));
    addAndMakeVisible (bpmLabel);

    smallLabel (swingLabel, jp ("スウィング"));
    swing.setSliderStyle (juce::Slider::LinearHorizontal);
    swing.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    swing.setRange (0, 100, 1);
    swing.onValueChange = [this] { engine().swing = (float) swing.getValue() / 100.0f; swingLabel.setText (jp ("スウィング  ") + juce::String ((int) swing.getValue()) + "%", juce::dontSendNotification); };
    addAndMakeVisible (swing);

    smallLabel (masterLabel, jp ("マスター"));
    master.setSliderStyle (juce::Slider::LinearHorizontal);
    master.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    master.setRange (0, 100, 1);
    master.onValueChange = [this] { engine().master = (float) master.getValue() / 100.0f; masterLabel.setText (jp ("マスター  ") + juce::String ((int) master.getValue()), juce::dontSendNotification); };
    addAndMakeVisible (master);

    smallLabel (barsLabel, jp ("小節"));
    barMinus.setTooltip (jp ("最後の小節を削除"));
    barCopy.setButtonText (jp ("+ コピー"));
    barCopy.setTooltip (jp ("最後の小節を複製して追加"));
    barEmpty.setButtonText (jp ("+ 空"));
    barEmpty.setTooltip (jp ("空の小節を追加"));
    barMinus.onClick = [this] { setBars (engine().getBars() - 1, false); };
    barCopy.onClick = [this] { setBars (engine().getBars() + 1, true); };
    barEmpty.onClick = [this] { setBars (engine().getBars() + 1, false); };
    addAndMakeVisible (barMinus);
    addAndMakeVisible (barCopy);
    addAndMakeVisible (barEmpty);

    smallLabel (modeLabel, jp ("入力モード"));
    auto modeButton = [this] (juce::TextButton& b, const char* text, InputMode m, const char* tip) {
        b.setButtonText (jp (text));
        b.setTooltip (jp (tip));
        b.setClickingTogglesState (true);
        b.setRadioGroupId (101);
        b.onClick = [this, m, &b] { if (b.getToggleState()) mode = m; };
        addAndMakeVisible (b);
    };
    modeButton (modeOn, "オン/オフ", InputMode::onOff, "クリックやドラッグで音を置く・消す");
    modeButton (modeVel, "強弱", InputMode::velocity, "クリックするたびに強さが 強→中→弱 と変わる");
    modeButton (modeRoll, "ロール", InputMode::roll, "1ステップを2〜4連打に分割(ハットロール用)");
    modeOn.setToggleState (true, juce::dontSendNotification);

    metronome.setButtonText (jp ("メトロノーム"));
    metronome.onClick = [this] { engine().metronome = metronome.getToggleState(); };
    addAndMakeVisible (metronome);
    follow.setButtonText (jp ("再生位置を追う"));
    follow.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (follow);

    smallLabel (presetLabel, jp ("プリセット"));
    presets.setTextWhenNothingSelected (jp ("プリセットを選ぶ"));
    presets.onChange = [this] { loadPresetFromBox(); };
    addAndMakeVisible (presets);
    presetSave.setButtonText (jp ("保存"));
    presetSave.setTooltip (jp ("今のパターンと音色(サンプル含む)をプリセットとして保存"));
    presetSave.onClick = [this] { savePreset(); };
    presetDelete.setButtonText (jp ("削除"));
    presetDelete.onClick = [this] { deletePreset(); };
    addAndMakeVisible (presetSave);
    addAndMakeVisible (presetDelete);

    padsTitle.setText (jp ("ドラムパッド"), juce::dontSendNotification);
    padsTitle.setFont (font (12.0f, true));
    padsTitle.setColour (juce::Label::textColourId, col::muted);
    addAndMakeVisible (padsTitle);
    padsHint.setText (jp ("クリック / キーボード A S D F… / MIDIキーボードで鳴らせます。RECで録音。"), juce::dontSendNotification);
    padsHint.setFont (font (11.5f));
    padsHint.setColour (juce::Label::textColourId, col::muted);
    addAndMakeVisible (padsHint);
    addAndMakeVisible (padArea);

    viewport.setViewedComponent (&trackList, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);

    addSound.setTextWhenNothingSelected (jp ("+ 音を追加"));
    for (int i = 0; i < (int) Sound::count; ++i)
        addSound.addItem (i == (int) Sound::sample ? jp ("自分のサンプル(WAV/MP3など)") : soundLabel ((Sound) i), i + 1);
    addSound.onChange = [this] {
        const int id = addSound.getSelectedId();
        addSound.setSelectedId (0, juce::dontSendNotification);
        if (id > 0) addTrack ((Sound) (id - 1));
    };
    addAndMakeVisible (addSound);

    smallLabel (loopsLabel, jp ("書き出すループ数"));
    for (int n : { 1, 2, 4, 8 }) loops.addItem (juce::String (n) + jp ("回"), n);
    loops.setSelectedId (proc.exportLoops, juce::dontSendNotification);
    loops.onChange = [this] { proc.exportLoops = loops.getSelectedId(); };
    addAndMakeVisible (loops);
    addAndMakeVisible (wavTile);
    addAndMakeVisible (midiTile);
    exportButton.setButtonText (jp ("フォルダに書き出す"));
    exportButton.setTooltip (jp ("ミックスWAV・トラック別WAV・MIDIをまとめて保存"));
    exportButton.onClick = [this] { exportToFolder(); };
    addAndMakeVisible (exportButton);

    status.setFont (font (12.0f));
    status.setColour (juce::Label::textColourId, col::muted);
    status.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (status);

    refreshPresetList();
    setResizable (true, true);
    setResizeLimits (1080, 600, 2400, 1600);
    setSize (1160, 760);

    lastGeneration = proc.stateGeneration.load();
    syncControls();
    rebuild();
    startTimerHz (30);
}

KizamiEditor::~KizamiEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

int KizamiEditor::visibleStepCount() const
{
    const int bars = proc.engine.getBars();
    return juce::jlimit (1, 4, bars - page * 4) * kStepsPerBar;
}

void KizamiEditor::audition (const Track& t, float vel)
{
    if (! isPlaying()) engine().pushPadEvent (t.uid, vel, false);
}

void KizamiEditor::setStatus (const juce::String& text, bool error)
{
    status.setText (text, juce::dontSendNotification);
    status.setColour (juce::Label::textColourId, error ? col::danger : col::muted);
    statusTimeout = 30 * 8;
}

void KizamiEditor::syncControls()
{
    swing.setValue (engine().swing.load() * 100.0, juce::sendNotificationSync);
    master.setValue (engine().master.load() * 100.0, juce::sendNotificationSync);
    metronome.setToggleState (engine().metronome.load(), juce::dontSendNotification);
    loops.setSelectedId (proc.exportLoops, juce::dontSendNotification);
}

void KizamiEditor::rebuild()
{
    const auto tracks = engine().getTracks();
    page = juce::jlimit (0, (engine().getBars() - 1) / 4, page);

    rows.clear();
    pads.clear();
    for (int i = 0; i < (int) tracks.size(); ++i)
    {
        auto* row = rows.add (new TrackRow (*this, tracks[(size_t) i], i));
        trackList.addAndMakeVisible (row);
        auto* pad = pads.add (new PadButton (*this, tracks[(size_t) i], i));
        padArea.addAndMakeVisible (pad);
    }
    barsLabel.setText (jp ("小節  ") + juce::String (engine().getBars()), juce::dontSendNotification);
    updatePageButtons();
    lastStep = -2;
    resized();
    repaint();
}

void KizamiEditor::updatePageButtons()
{
    pageButtons.clear();
    const int bars = engine().getBars();
    const int pagesCount = (bars + 3) / 4;
    if (pagesCount > 1)
        for (int p = 0; p < pagesCount; ++p)
        {
            const int a = p * 4 + 1, b = juce::jmin (bars, p * 4 + 4);
            auto* btn = pageButtons.add (new juce::TextButton (a == b ? juce::String (a) : juce::String (a) + "-" + juce::String (b)));
            btn->setClickingTogglesState (true);
            btn->setRadioGroupId (202);
            btn->setToggleState (p == page, juce::dontSendNotification);
            btn->setTooltip (jp ("表示する小節"));
            btn->onClick = [this, p] { page = p; follow.setToggleState (false, juce::dontSendNotification); for (auto* r : rows) r->grid.repaint(); };
            addAndMakeVisible (btn);
        }
}

void KizamiEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto r = getLocalBounds().reduced (12);
    // transport panel
    auto top = r.removeFromTop (64).toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (top, 9.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (top.reduced (0.5f), 9.0f, 1.0f);
    // the title's orange letter
    g.setColour (col::accent);
    g.fillRect (titleLabel.getX() + 4, titleLabel.getBottom() - 3, 18, 3);

    // pad panel
    auto padPanel = padArea.getBounds().expanded (12, 8).withTop (padsTitle.getY() - 6).toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (padPanel, 9.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (padPanel.reduced (0.5f), 9.0f, 1.0f);

    // footer panel
    auto foot = getLocalBounds().reduced (12).removeFromBottom (66).toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (foot, 9.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (foot.reduced (0.5f), 9.0f, 1.0f);

    if (rows.isEmpty())
    {
        g.setColour (col::muted);
        g.setFont (font (14.0f));
        g.drawText (jp ("トラックがありません。下の「+ 音を追加」から追加してください。"), viewport.getBounds(), juce::Justification::centred);
    }
}

void KizamiEditor::resized()
{
    auto r = getLocalBounds().reduced (12);

    // ---- transport ----
    auto top = r.removeFromTop (64).reduced (12, 8);
    auto brand = top.removeFromLeft (132);
    titleLabel.setBounds (brand.removeFromTop (28));
    subtitleLabel.setBounds (brand);
    top.removeFromLeft (6);
    play.setBounds (top.removeFromLeft (46).withSizeKeepingCentre (46, 46));
    top.removeFromLeft (8);
    rec.setBounds (top.removeFromLeft (72).withSizeKeepingCentre (72, 30));
    top.removeFromLeft (14);
    bpmLabel.setBounds (top.removeFromLeft (110));

    auto field = [&top] (juce::Label& l, int w) {
        auto c = top.removeFromLeft (w);
        l.setBounds (c.removeFromTop (18));
        top.removeFromLeft (16);
        return c;
    };
    swing.setBounds (field (swingLabel, 120).withSizeKeepingCentre (120, 22));
    {
        auto c = field (barsLabel, 156).withSizeKeepingCentre (156, 26);
        barMinus.setBounds (c.removeFromLeft (26)); c.removeFromLeft (4);
        barCopy.setBounds (c.removeFromLeft (66)); c.removeFromLeft (4);
        barEmpty.setBounds (c);
    }
    master.setBounds (field (masterLabel, 100).withSizeKeepingCentre (100, 22));
    {
        auto c = field (modeLabel, 186).withSizeKeepingCentre (186, 26);
        modeOn.setBounds (c.removeFromLeft (74));
        modeVel.setBounds (c.removeFromLeft (50));
        modeRoll.setBounds (c);
    }

    r.removeFromTop (10);

    // ---- preset row ----
    auto pr = r.removeFromTop (28);
    presetLabel.setBounds (pr.removeFromLeft (70));
    presets.setBounds (pr.removeFromLeft (220));
    pr.removeFromLeft (6);
    presetSave.setBounds (pr.removeFromLeft (56));
    pr.removeFromLeft (4);
    presetDelete.setBounds (pr.removeFromLeft (56));
    for (int i = pageButtons.size(); --i >= 0;)
    {
        pageButtons[i]->setBounds (pr.removeFromRight (54));
        pr.removeFromRight (4);
    }
    if (! pageButtons.isEmpty()) follow.setBounds (pr.removeFromRight (140).withSizeKeepingCentre (140, 24));
    follow.setVisible (! pageButtons.isEmpty());
    pr.removeFromRight (10);
    metronome.setBounds (pr.removeFromRight (130).withSizeKeepingCentre (130, 24));

    r.removeFromTop (18);

    // ---- pads ----
    auto padsHead = r.removeFromTop (18).withTrimmedLeft (12);
    padsTitle.setBounds (padsHead.removeFromLeft (100));
    padsHint.setBounds (padsHead);
    r.removeFromTop (6);
    const int n = pads.size();
    const int padW = 108, padH = 58, gap = 8;
    const int availW = r.getWidth() - 24;
    const int perRow = juce::jmax (1, (availW + gap) / (padW + gap));
    const int padRows = juce::jmax (1, (n + perRow - 1) / perRow);
    auto pa = r.removeFromTop (padRows * padH + (padRows - 1) * gap).reduced (12, 0);
    padArea.setBounds (pa);
    for (int i = 0; i < n; ++i)
        pads[i]->setBounds ((i % perRow) * (padW + gap), (i / perRow) * (padH + gap), padW, padH);
    r.removeFromTop (18);

    // ---- footer ----
    auto foot = r.removeFromBottom (66).reduced (12, 9);
    addSound.setBounds (foot.removeFromLeft (190).withSizeKeepingCentre (190, 30));
    foot.removeFromLeft (20);
    {
        auto c = foot.removeFromLeft (110);
        loopsLabel.setBounds (c.removeFromTop (18));
        loops.setBounds (c.withSizeKeepingCentre (110, 26));
    }
    foot.removeFromLeft (12);
    wavTile.setBounds (foot.removeFromLeft (130));
    foot.removeFromLeft (8);
    midiTile.setBounds (foot.removeFromLeft (130));
    foot.removeFromLeft (12);
    exportButton.setBounds (foot.removeFromLeft (150).withSizeKeepingCentre (150, 30));
    foot.removeFromLeft (12);
    status.setBounds (foot);
    r.removeFromBottom (10);

    // ---- tracks ----
    viewport.setBounds (r);
    layoutTracks();
}

void KizamiEditor::layoutTracks()
{
    const int rowH = 46;
    const int w = viewport.getWidth() - (rows.size() * rowH > viewport.getHeight() ? viewport.getScrollBarThickness() + 2 : 0);
    trackList.setSize (w, juce::jmax (1, rows.size() * rowH));
    for (int i = 0; i < rows.size(); ++i)
        rows[i]->setBounds (0, i * rowH, w, rowH);
}

void KizamiEditor::timerCallback()
{
    if (rebuildPending) { rebuildPending = false; rebuild(); }

    if (proc.stateGeneration.load() != lastGeneration)
    {
        lastGeneration = proc.stateGeneration.load();
        syncControls();
        rebuild();
    }

    const bool playingNow = isPlaying();
    if (play.playing != playingNow) { play.playing = playingNow; play.repaint(); }

    const bool armed = engine().recording.load();
    blinkCounter = (blinkCounter + 1) % 30;
    const bool blink = armed && playingNow && blinkCounter >= 15;
    if (rec.armed != armed || rec.blink != blink) { rec.armed = armed; rec.blink = blink; rec.repaint(); }

    const double bpm = engine().hostBpm.load();
    const auto bpmText = juce::String (bpm, bpm == std::floor (bpm) ? 0 : 1) + " BPM";
    if (bpmLabel.getText() != bpmText) bpmLabel.setText (bpmText, juce::dontSendNotification);

    const int step = engine().currentStep.load();
    if (step != lastStep)
    {
        lastStep = step;
        if (step >= 0 && follow.getToggleState() && pageButtons.size() > 1)
        {
            const int p = step / (4 * kStepsPerBar);
            if (p != page)
            {
                page = p;
                for (int i = 0; i < pageButtons.size(); ++i) pageButtons[i]->setToggleState (i == page, juce::dontSendNotification);
                for (auto* row : rows) row->grid.repaint();
            }
        }
        for (auto* row : rows) row->grid.setPlayStep (step);
    }

    for (auto* pad : pads) pad->tick();

    // steps recorded from pads/MIDI appear while playing
    if (armed && playingNow && blinkCounter % 3 == 0)
        for (auto* row : rows) row->grid.repaint();

    if (statusTimeout > 0 && --statusTimeout == 0) status.setText ({}, juce::dontSendNotification);
}

bool KizamiEditor::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey) { play.triggerClick(); return true; }
    if (key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()) return false;
    const auto ch = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
    const char* found = ch > 0 ? std::strchr (padKeys, (int) ch) : nullptr;
    if (found != nullptr && ch != 0)
    {
        const int index = (int) (found - padKeys);
        const auto tracks = engine().getTracks();
        if (index < (int) tracks.size()) { engine().pushPadEvent (tracks[(size_t) index]->uid, 1.0f, true); return true; }
    }
    return false;
}

//==============================================================================
void KizamiEditor::setBars (int bars, bool copy)
{
    if (bars > kMaxBars) { setStatus (jp ("小節は16まで追加できます。"), true); return; }
    if (bars < 1) return;
    engine().setBars (bars, copy);
    if (bars > engine().getBars() - 1 && copy) page = (bars - 1) / 4;
    rebuild();
}

void KizamiEditor::addTrack (Sound s)
{
    auto t = engine().addTrack (s);
    rebuild();
    viewport.setViewPosition (0, trackList.getHeight());
    if (s == Sound::sample) chooseSample (t->uid);
    else audition (*t);
}

void KizamiEditor::chooseSample (int uid)
{
    chooser = std::make_unique<juce::FileChooser> (jp ("サンプルを選ぶ"),
        juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg;*.m4a");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, uid] (const juce::FileChooser& fc) {
            const auto file = fc.getResult();
            if (file == juce::File()) { requestRebuild(); return; }
            juce::String error;
            if (engine().loadSampleFile (uid, file, error))
            {
                setStatus (jp ("「") + file.getFileName() + jp ("」を読み込みました"));
                if (auto t = engine().findTrack (uid)) audition (*t);
            }
            else setStatus (error, true);
            rebuild();
        });
}

void KizamiEditor::showDetail (TrackPtr t, juce::Component& anchor)
{
    auto content = std::make_unique<TrackDetail> (*this, std::move (t));
    juce::CallOutBox::launchAsynchronously (std::move (content), getLocalArea (&anchor, anchor.getLocalBounds()), this);
}

bool KizamiEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;mp3;flac;ogg;m4a;kizami"))
            return true;
    return false;
}

void KizamiEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    int targetUid = 0;
    for (auto* row : rows)
        if (row->getScreenBounds().contains (localPointToGlobal (juce::Point<int> (x, y))))
            targetUid = row->getUid();
    for (auto* pad : pads)
        if (pad->getScreenBounds().contains (localPointToGlobal (juce::Point<int> (x, y))))
            targetUid = pad->getUid();

    int loaded = 0;
    juce::String error;
    for (auto& path : files)
    {
        const juce::File f (path);
        if (f.hasFileExtension ("kizami"))
        {
            if (auto xml = juce::parseXML (f)) { engine().fromValueTree (juce::ValueTree::fromXml (*xml)); syncControls(); setStatus (jp ("「") + f.getFileNameWithoutExtension() + jp ("」を読み込みました")); }
            continue;
        }
        const bool fresh = (targetUid == 0 || loaded > 0);
        const int uid = fresh ? engine().addTrack (Sound::sample)->uid : targetUid;
        if (engine().loadSampleFile (uid, f, error)) ++loaded;
        else if (fresh) engine().removeTrack (uid);
    }
    rebuild();
    if (loaded > 0) setStatus (juce::String (loaded) + jp ("個のサンプルを読み込みました"));
    else if (error.isNotEmpty()) setStatus (error, true);
}

//==============================================================================
void KizamiEditor::refreshPresetList()
{
    presets.clear (juce::dontSendNotification);
    presets.addSectionHeading (jp ("標準"));
    for (int i = 0; i < Engine::builtInPresetCount(); ++i)
        presets.addItem (Engine::builtInPresetName (i), i + 1);

    userPresetFiles.clear();
    auto folder = KizamiProcessor::presetFolder();
    if (folder.isDirectory())
    {
        auto found = folder.findChildFiles (juce::File::findFiles, false, "*.kizami");
        std::sort (found.begin(), found.end(), [] (const juce::File& a, const juce::File& b) {
            return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0;
        });
        userPresetFiles.addArray (found);
    }
    if (! userPresetFiles.isEmpty())
    {
        presets.addSectionHeading (jp ("保存したプリセット"));
        for (int i = 0; i < userPresetFiles.size(); ++i)
            presets.addItem (userPresetFiles[i].getFileNameWithoutExtension(), 1000 + i);
    }
}

void KizamiEditor::loadPresetFromBox()
{
    const int id = presets.getSelectedId();
    if (id <= 0) return;
    juce::ValueTree v;
    if (id < 1000) v = Engine::builtInPreset (id - 1);
    else if (auto xml = juce::parseXML (userPresetFiles[id - 1000])) v = juce::ValueTree::fromXml (*xml);
    if (! v.isValid()) { setStatus (jp ("プリセットを読み込めませんでした。"), true); return; }
    engine().internalPlay = false;
    engine().fromValueTree (v);
    page = 0;
    syncControls();
    rebuild();
    setStatus (jp ("「") + presets.getText() + jp ("」を読み込みました"));
}

void KizamiEditor::savePreset()
{
    nameDialog = std::make_unique<juce::AlertWindow> (jp ("プリセットを保存"),
        jp ("名前をつけて保存します。サンプル音源も一緒に保存されます。"), juce::MessageBoxIconType::NoIcon, this);
    const int id = presets.getSelectedId();
    nameDialog->addTextEditor ("name", id >= 1000 ? presets.getText() : juce::String(), jp ("名前"));
    nameDialog->addButton (jp ("保存"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    nameDialog->addButton (jp ("キャンセル"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    nameDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result) {
        if (result != 1 || nameDialog == nullptr) return;
        auto name = safeFileName (nameDialog->getTextEditorContents ("name").trim());
        if (nameDialog->getTextEditorContents ("name").trim().isEmpty())
            name = jp ("プリセット ") + juce::String (userPresetFiles.size() + 1);
        auto folder = KizamiProcessor::presetFolder();
        folder.createDirectory();
        auto file = folder.getChildFile (name + ".kizami");
        auto xml = engine().toValueTree (true).createXml();
        if (xml != nullptr && xml->writeTo (file))
        {
            refreshPresetList();
            for (int i = 0; i < userPresetFiles.size(); ++i)
                if (userPresetFiles[i] == file) presets.setSelectedId (1000 + i, juce::dontSendNotification);
            setStatus (jp ("「") + name + jp ("」として保存しました"));
        }
        else setStatus (jp ("保存できませんでした: ") + file.getFullPathName(), true);
    }), false);
}

void KizamiEditor::deletePreset()
{
    const int id = presets.getSelectedId();
    if (id < 1000) { setStatus (jp ("削除できるのは自分で保存したプリセットだけです。"), true); return; }
    const auto file = userPresetFiles[id - 1000];
    juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::QuestionIcon,
                                      jp ("プリセットを削除"), jp ("「") + file.getFileNameWithoutExtension() + jp ("」を削除しますか?"),
                                      jp ("削除"), jp ("キャンセル"), this),
        [this, file] (int result) {
            if (result != 1) return;
            file.moveToTrash();
            refreshPresetList();
            setStatus (jp ("削除しました"));
        });
}

//==============================================================================
juce::File KizamiEditor::makeDragFile (bool midi)
{
    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Kizami");
    folder.createDirectory();
    const double bpm = engine().hostBpm.load();
    const int loopsCount = loops.getSelectedId() > 0 ? loops.getSelectedId() : 4;
    const auto stamp = juce::Time::getCurrentTime().formatted ("%H%M%S");
    const auto file = folder.getChildFile ("Kizami_" + juce::String (juce::roundToInt (bpm)) + "bpm_" + stamp + (midi ? ".mid" : ".wav"));
    const bool ok = midi ? writeMidiFile (engine(), loopsCount, bpm, file)
                         : writeWavFile (engine().renderOffline (loopsCount, bpm, engine().getSampleRate(), 0), engine().getSampleRate(), file);
    if (! ok) setStatus (jp ("ファイルを作れませんでした。"), true);
    return ok ? file : juce::File();
}

void KizamiEditor::exportToFolder()
{
    auto start = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Kizami").getChildFile ("Exports");
    start.createDirectory();
    chooser = std::make_unique<juce::FileChooser> (jp ("書き出し先のフォルダを選ぶ"), start);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& fc) {
            auto dir = fc.getResult();
            if (dir == juce::File()) return;
            const double bpm = engine().hostBpm.load();
            const auto sub = dir.getChildFile ("Kizami_" + juce::String (juce::roundToInt (bpm)) + "bpm_"
                                               + juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S"));
            juce::String error;
            const int count = exportAll (engine(), loops.getSelectedId(), bpm, sub, error);
            if (count > 0)
            {
                setStatus (juce::String (count) + jp ("個のファイルを書き出しました"));
                sub.revealToUser();
            }
            else setStatus (error, true);
        });
}

} // namespace kizami::ui
