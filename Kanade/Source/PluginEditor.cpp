#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>

namespace kanade::ui
{
static const char* const NOTE_NAMES[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static juce::String midiName (float m) { const int r = (int) std::lround (m); return juce::String (NOTE_NAMES[(r % 12 + 12) % 12]) + juce::String ((int) std::floor (r / 12.0) - 1); }
static juce::String cents (float m) { const int r = (int) std::lround (m), c = (int) std::lround ((m - (float) r) * 100); return c ? (c > 0 ? " +" : " ") + juce::String (c) : juce::String(); }
static juce::String fmtTime (double s) { if (s < 0) s = 0; const int m = (int) (s / 60); return juce::String (m) + ":" + juce::String (std::fmod (s, 60.0), 1).paddedLeft ('0', 4); }

//==============================================================================
Fonts::Fonts() { comfortaa = juce::Typeface::createSystemTypefaceFor (BinaryData::ComfortaaBold_ttf, (size_t) BinaryData::ComfortaaBold_ttfSize); }
juce::Font Fonts::body (float h, bool bold) const { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
juce::Font Fonts::display (float h) const { return comfortaa != nullptr ? juce::Font (juce::FontOptions (comfortaa).withHeight (h)) : body (h, true); }
juce::Font Fonts::mono (float h) const { return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, juce::Font::bold)); }
const Fonts& fonts() { static Fonts f; return f; }

Look::Look()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::TextButton::buttonColourId, col::pane2);
    setColour (juce::TextButton::buttonOnColourId, col::curve);
    setColour (juce::TextButton::textColourOffId, col::ink);
    setColour (juce::TextButton::textColourOnId, juce::Colour (0xff06201c));
    setColour (juce::ComboBox::backgroundColourId, col::pane);
    setColour (juce::ComboBox::textColourId, col::ink);
    setColour (juce::ComboBox::arrowColourId, col::ink);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xffc9d0dc));
    setColour (juce::PopupMenu::textColourId, juce::Colour (0xff1b2333));
    setColour (juce::PopupMenu::headerTextColourId, juce::Colour (0xff1b2333));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff9aa4b6));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff10172b));
    setColour (juce::Label::textColourId, col::ink);
    setColour (juce::Slider::thumbColourId, col::gold);
    setColour (juce::TooltipWindow::backgroundColourId, col::pane);
    setColour (juce::TooltipWindow::textColourId, col::ink);
    setColour (juce::TooltipWindow::outlineColourId, col::gold);
}
void Look::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.8f);
    const float rad = juce::jmin (7.0f, r.getHeight() * 0.5f);
    const auto on = b.findColour (juce::TextButton::buttonOnColourId);
    if (b.getToggleState()) { g.setColour (on.withAlpha (down ? 0.85f : 1.0f)); g.fillRoundedRectangle (r, rad); }
    else { g.setColour (b.findColour (juce::TextButton::buttonColourId)); g.fillRoundedRectangle (r, rad); if (over || down) { g.setColour (col::ink.withAlpha (down ? 0.14f : 0.07f)); g.fillRoundedRectangle (r, rad); } }
    g.setColour (b.getToggleState() ? on : (over ? col::muted : col::line2));
    g.drawRoundedRectangle (r, rad, 1.1f);
}
void Look::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour ((b.getToggleState() ? b.findColour (juce::TextButton::textColourOnId) : b.findColour (juce::TextButton::textColourOffId)).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.4f));
    g.setFont (fonts().body (juce::jmin (13.0f, b.getHeight() * 0.5f), true));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1, 0.8f);
}
void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.8f);
    g.setColour (col::pane); g.fillRoundedRectangle (r, 7.0f);
    g.setColour (col::line2); g.drawRoundedRectangle (r, 7.0f, 1.1f);
    juce::Path p; const float cx = (float) w - 13.0f, cy = (float) h * 0.5f;
    p.startNewSubPath (cx - 4, cy - 2); p.lineTo (cx, cy + 2.5f); p.lineTo (cx + 4, cy - 2);
    g.setColour (col::ink); g.strokePath (p, juce::PathStrokeType (1.6f));
}
juce::Font Look::getComboBoxFont (juce::ComboBox&) { return fonts().body (13.0f); }
void Look::positionComboBoxText (juce::ComboBox& b, juce::Label& l) { l.setBounds (8, 1, b.getWidth() - 26, b.getHeight() - 2); l.setFont (getComboBoxFont (b)); }
juce::Font Look::getPopupMenuFont() { return fonts().body (14.5f); }
void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const float cx = x + w * 0.5f, cy = y + h * 0.5f, r = juce::jmin (w, h) * 0.5f - 3.0f, ang = a0 + pos * (a1 - a0);
    juce::Path bgA, vA;
    bgA.addCentredArc (cx, cy, r, r, 0, a0, a1, true);
    g.setColour (col::line2); g.strokePath (bgA, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (pos > 0.003f) { vA.addCentredArc (cx, cy, r, r, 0, a0, ang, true); g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId)); g.strokePath (vA, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
    const float cr = r * 0.66f;
    g.setColour (col::pane2); g.fillEllipse (cx - cr, cy - cr, cr * 2, cr * 2);
    g.setColour (col::line2); g.drawEllipse (cx - cr, cy - cr, cr * 2, cr * 2, 1.3f);
    g.setColour (col::ink); g.drawLine (cx, cy, cx + std::sin (ang) * r * 0.55f, cy - std::cos (ang) * r * 0.55f, 2.5f);
}
void Look::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    const float cy = y + h * 0.5f;
    g.setColour (col::pane2); g.fillRoundedRectangle ((float) x, cy - 3, (float) w, 6, 3);
    g.setColour (s.findColour (juce::Slider::trackColourId)); g.fillRoundedRectangle ((float) x, cy - 3, pos - (float) x, 6, 3);
    g.setColour (col::gold); g.fillEllipse (pos - 7, cy - 7, 14, 14);
}

//==============================================================================
Knob::Knob (const juce::String& n, juce::Colour c, std::function<juce::String (double)> f) : name (n), fmt (std::move (f))
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (200);
    slider.setColour (juce::Slider::rotarySliderFillColourId, c);
    slider.setRange (0, 1);
    addAndMakeVisible (slider);
    slider.onValueChange = [this] { repaint(); };
}
void Knob::resized() { slider.setBounds (getWidth() / 2 - 24, 0, 48, 48); }
void Knob::paint (juce::Graphics& g)
{
    g.setColour (col::ink); g.setFont (fonts().display (12.0f));
    g.drawText (name, 0, 50, getWidth(), 15, juce::Justification::centred);
    g.setColour (col::muted); g.setFont (fonts().mono (11.0f));
    g.drawText (fmt (slider.getValue()), 0, 65, getWidth(), 15, juce::Justification::centred);
}

//==============================================================================
Logo::Logo (KanadeEditor& e) : editor (e)
{
    img = juce::ImageCache::getFromMemory (BinaryData::astronaut_png, BinaryData::astronaut_pngSize);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle (jp ("DJ KATSUNIIの宇宙飛行士(クリックでダンス)"));
}
void Logo::mouseUp (const juce::MouseEvent&)
{
    dancing = ! dancing;
    editor.setStatus (dancing ? jp ("かつにいが BPM ") + juce::String (editor.proc.hostBpm.load(), 1) + jp (" でダンス中(もう一度クリックで止まる)") : jp ("ダンス終了"));
    repaint();
}
void Logo::paint (juce::Graphics& g)
{
    if (! img.isValid()) return;
    auto r = getLocalBounds().toFloat().reduced (2);
    const float bw = 40, bh = 62, sx = r.getCentreX(), sy = r.getBottom();
    juce::AffineTransform t;
    if (dancing)
    {
        // two beats per sway: lean left, hop, lean right, hop (like the browser version)
        const double ph = std::fmod (phaseBeats, 2.0) / 2.0;
        const float rot = (float) (std::sin (ph * juce::MathConstants<double>::twoPi) * 9.0) * juce::MathConstants<float>::pi / 180.0f;
        const float hop = (float) -std::abs (std::sin (ph * juce::MathConstants<double>::twoPi * 2.0)) * 12.0f;
        const float sq = 1.0f + 0.05f * (float) std::cos (ph * juce::MathConstants<double>::twoPi * 2.0);
        t = juce::AffineTransform::scale (2 - sq, sq, sx, sy).rotated (rot, sx, sy).translated (0, hop);
    }
    g.addTransform (t);
    g.drawImage (img, juce::Rectangle<float> (sx - bw / 2, sy - bh, bw, bh), juce::RectanglePlacement::centred);
}

//==============================================================================
void DragSquare::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1);
    g.setColour (col::pane2); g.fillRoundedRectangle (r, 6);
    g.setColour (col::gold); g.drawRoundedRectangle (r, 6, 1.5f);
    auto i = r.reduced (8);
    g.drawRoundedRectangle (i, 2, 1.4f);
    juce::Path p; p.startNewSubPath (i.getCentreX() - 3, i.getCentreY() + 0.5f); p.lineTo (i.getCentreX(), i.getCentreY() - 2.5f); p.lineTo (i.getCentreX() + 3, i.getCentreY() + 0.5f);
    p.startNewSubPath (i.getCentreX(), i.getCentreY() - 2.5f); p.lineTo (i.getCentreX(), i.getBottom() - 2);
    g.strokePath (p, juce::PathStrokeType (1.4f));
}
void DragSquare::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 4) return;
    if (! editor.proc.session) { editor.setStatus (jp ("先に歌を取り込んでください。"), true); return; }
    dragging = true;
    const auto f = editor.proc.dragFile();
    if (! f.existsAsFile()) { editor.setStatus (jp ("書き出せませんでした。"), true); return; }
    editor.setStatus (jp ("FLのプレイリストに落としてください(補正した歌がオーディオクリップとして入ります)。"));
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [this] { dragging = false; });
}
void DragSquare::mouseUp (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() < 4 && ! dragging) editor.content.exportBtn.triggerClick();
    dragging = false;
}

//==============================================================================
MiniPanel::MiniPanel (KanadeEditor& e) : editor (e)
{
    form.setSliderStyle (juce::Slider::LinearHorizontal);
    form.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    form.setRange (-6, 6, 0.5);
    form.setColour (juce::Slider::trackColourId, col::gold.withAlpha (0.6f));
    snap.setButtonText (jp ("中心を合わせる")); reset.setButtonText (jp ("この音符を元に戻す")); close.setButtonText (juce::String::fromUTF8 ("×"));
    for (auto* c : std::initializer_list<juce::Component*> { &form, &snap, &reset, &close }) addAndMakeVisible (c);
}
void MiniPanel::show (const Note& n)
{
    title = jp ("音符 ") + midiName (n.pitch) + cents (n.pitch) + jp ("(元 ") + midiName (n.center) + cents (n.center) + ")";
    form.setValue (n.formant, juce::dontSendNotification);
    setVisible (true); repaint();
}
void MiniPanel::resized()
{
    close.setBounds (getWidth() - 30, 6, 24, 22);
    form.setBounds (96, 34, getWidth() - 150, 22);
    snap.setBounds (10, 64, 116, 26);
    reset.setBounds (132, 64, 150, 26);
}
void MiniPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (col::pane); g.fillRoundedRectangle (r, 10);
    g.setColour (col::gold); g.drawRoundedRectangle (r, 10, 1.2f);
    g.setColour (col::gold2); g.setFont (fonts().body (13.0f, true)); g.drawText (title, 12, 6, getWidth() - 50, 22, juce::Justification::centredLeft);
    g.setColour (col::ink); g.setFont (fonts().body (12.5f)); g.drawText (jp ("声の太さ"), 12, 34, 80, 22, juce::Justification::centredLeft);
    g.setFont (fonts().mono (11.5f)); g.drawText ((form.getValue() > 0 ? "+" : "") + juce::String (form.getValue(), 1), getWidth() - 52, 34, 44, 22, juce::Justification::centredRight);
}

//==============================================================================
PianoRoll::PianoRoll (KanadeEditor& e) : mini (e), editor (e), proc (e.proc)
{
    addChildComponent (mini);
    setWantsKeyboardFocus (false);
    mini.close.onClick = [this] { closeAdv(); };
    mini.snap.onClick = [this] { if (auto* n = noteById (adv)) { proc.pushUndo(); n->pitch = snapToScale (n->center, proc.key, proc.scale); editor.edited ({ n->id }); mini.show (*n); } };
    mini.reset.onClick = [this] { if (auto* n = noteById (adv)) { proc.pushUndo(); const Note keep = *n; *n = Note(); n->id = keep.id; n->a = keep.a; n->b = keep.b; n->center = n->pitch = keep.center; editor.edited ({ n->id }); mini.show (*n); } };
    mini.form.onDragStart = [this] { proc.pushUndo(); };
    mini.form.onValueChange = [this] { if (auto* n = noteById (adv)) { n->formant = (float) mini.form.getValue(); editor.edited ({ n->id }); mini.repaint(); } };
}

double PianoRoll::fr() const { return proc.session ? proc.session->A.frameSec() : HOP / 44100.0; }
double PianoRoll::frameSec (double f) const { return (proc.session ? proc.session->startSec() : 0.0) + f * fr(); }
Note* PianoRoll::noteById (int id) { for (auto& n : proc.notes) if (n.id == id) return &n; return nullptr; }

void PianoRoll::resized() { if (adv >= 0) openAdv (adv); }

void PianoRoll::fitView()
{
    if (! proc.session) return;
    const auto& s = *proc.session;
    const double dur = s.endSec() - s.startSec();
    view.pps = juce::jmax (5.0, (getWidth() - KB - 10) / juce::jmax (0.5, dur));
    view.t0 = s.startSec();
    float lo = 1e9f, hi = -1e9f;
    for (auto& n : proc.notes) { lo = juce::jmin (lo, n.center, n.pitch); hi = juce::jmax (hi, n.center, n.pitch); }
    if (lo > hi) { lo = 55; hi = 72; }
    const float area = pBot() - RH;
    view.lane = juce::jlimit (14.0f, 40.0f, area / (hi - lo + 6));
    view.mTop = hi + ((area / view.lane) - (hi - lo)) / 2;
    repaint();
}
void PianoRoll::zoomH (float f) { const double c = xt ((getWidth() + KB) / 2); view.pps = juce::jlimit (5.0, 3000.0, view.pps * f); view.t0 = c - (getWidth() - KB) / 2 / view.pps; repaint(); }
void PianoRoll::zoomV (float f) { const float mid = ym ((RH + pBot()) / 2); view.lane = juce::jlimit (6.0f, 48.0f, view.lane * f); view.mTop = mid + ((pBot() - RH) / 2) / view.lane; repaint(); if (adv >= 0) openAdv (adv); }
void PianoRoll::refreshCurves() { if (proc.session) curves = buildCurves (proc.notes, proc.session->A, proc.globalFormant); else curves = {}; curvesGen = proc.generation.load(); }

Note* PianoRoll::hitNote (juce::Point<float> p)
{
    for (int k = (int) proc.notes.size() - 1; k >= 0; --k)
    {
        auto& n = proc.notes[(size_t) k];
        const float x0 = tx (frameSec (n.a)), x1 = tx (frameSec (n.b)), yc = py (n.pitch);
        if (p.x >= x0 - 2 && p.x <= x1 + 2 && std::abs (p.y - yc) <= juce::jmax (7.0f, view.lane * 0.6f)) return &n;
    }
    return nullptr;
}
bool PianoRoll::toolSplitNow() const { return shiftHeld ? ! splitTool : splitTool; }
double PianoRoll::snapCut (double t) const
{
    if (! proc.cutDiv) return t;
    const double g = (4.0 / proc.cutDiv) * 60.0 / proc.hostBpm.load();
    return std::round (t / g) * g;
}
float PianoRoll::snapPitch (float m, bool freeMove) const
{
    if (freeMove) return std::round (m * 100) / 100;
    return snapToScale (m, proc.key, proc.snapOn ? proc.scale : chromatic);
}

void PianoRoll::updHover()
{
    const auto prev = hoverCut;
    hoverCut.reset();
    if (proc.session && mouse && drag.type == 0 && ! advDragging)
    {
        const auto p = *mouse;
        const bool inArea = p.y > RH && p.y < pBot() && p.x > KB;
        Note* n = inArea && ! (adv >= 0 && advHit (p).isNotEmpty()) ? hitNote (p) : nullptr;
        if (n && toolSplitNow())
        {
            const int f = (int) std::lround ((snapCut (xt (p.x)) - proc.session->startSec()) / fr());
            if (f - n->a >= 6 && n->b - f >= 6) hoverCut = HoverCut { tx (frameSec (f)), n->id };
        }
        setMouseCursor (n ? (toolSplitNow() ? juce::MouseCursor::CrosshairCursor : juce::MouseCursor::UpDownResizeCursor)
                          : (! inArea && p.x > KB ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor));
    }
    const bool changed = prev.has_value() != hoverCut.has_value() || (prev && hoverCut && (prev->x != hoverCut->x || prev->id != hoverCut->id));
    if (changed) repaint();
}

void PianoRoll::modifierKeysChanged (const juce::ModifierKeys& m)
{
    if (m.isShiftDown() != shiftHeld) { shiftHeld = m.isShiftDown(); updHover(); }
}

void PianoRoll::doSplit (Note& n, float x)
{
    const int f = (int) std::lround ((snapCut (xt (x)) - proc.session->startSec()) / fr());
    if (f - n.a < 6 || n.b - f < 6)
    {
        editor.setStatus (proc.cutDiv ? jp ("この音符の中に 1/") + juce::String (proc.cutDiv) + jp (" の線がありません。細かい単位か No snap にしてください。") : jp ("音符の端に近すぎて分けられません。"), true);
        return;
    }
    proc.pushUndo();
    const int id = n.id;
    Note m = makeNote (f, n.b, proc.session->A, proc.nextId++);
    Note* np = noteById (id);
    const float off = np->pitch - np->center;
    np->b = f; recenter (*np, proc.session->A); np->pitch = np->center + off;
    m.pitch = m.center + off; m.drift = np->drift; m.gain = np->gain; m.formant = np->formant; m.inMs = np->inMs; m.outMs = np->outMs;
    const int mid = m.id;
    proc.notes.push_back (m);
    std::sort (proc.notes.begin(), proc.notes.end(), [] (const Note& a, const Note& b) { return a.a < b.a; });
    sel = { mid };
    editor.edited ({ id, mid });
    editor.setStatus (jp ("音符を2つに分けました(後ろ側を選んでいます)。"));
}

//==============================================================================
void PianoRoll::mouseDown (const juce::MouseEvent& e)
{
    if (! proc.session) return;
    const auto p = e.position;
    if (adv >= 0)
    {
        const auto h = advHit (p);
        if (h.isNotEmpty()) { if (h != "inside") { auto* n = noteById (adv); proc.pushUndo(); advDrag = { h, p, *n }; advDragging = true; } return; }
        closeAdv();
    }
    if (p.y < RH || p.y > pBot())
    {
        if (p.x > KB) { cursorSec = juce::jmax (0.0, xt (p.x)); if (proc.previewOn) proc.preview (cursorSec); repaint(); }
        return;
    }
    if (p.x < KB) return;
    const bool split = e.mods.isShiftDown() ? ! splitTool : splitTool;
    Note* n = hitNote (p);
    if (n && split) { doSplit (*n, p.x); return; }
    if (n)
    {
        if (e.mods.isCommandDown() || e.mods.isCtrlDown()) { if (sel.count (n->id)) sel.erase (n->id); else sel.insert (n->id); }
        else if (! sel.count (n->id)) sel = { n->id };
        drag = {}; drag.type = 1; drag.p0 = p; drag.prim = n->id;
        for (int id : sel) if (auto* q = noteById (id)) drag.orig.push_back ({ id, q->pitch });
    }
    else
    {
        if (! (e.mods.isCommandDown() || e.mods.isCtrlDown())) sel.clear();
        drag = {}; drag.type = 2; drag.p0 = drag.p1 = p; drag.base = sel;
    }
    repaint();
}

void PianoRoll::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = e.position;
    mouse = p;
    if (advDragging)
    {
        auto G = advGeom(); if (! G) return;
        Note& n = *G->n; const Note& v = advDrag.v;
        const float dy = advDrag.p0.y - p.y;
        auto msAt = [&] (float px) { return (px - G->x0) / G->w * G->dur; };
        const auto k = advDrag.k;
        if (k == "fadeIn") n.fadeIn = (float) juce::jlimit (0.0, G->dur - n.fadeOut, msAt (p.x));
        else if (k == "fadeOut") n.fadeOut = (float) juce::jlimit (0.0, G->dur - n.fadeIn, G->dur - msAt (p.x));
        else if (k == "varIn") n.varIn = (float) juce::jlimit (0.0, G->dur - n.varOut, msAt (p.x));
        else if (k == "varOut") n.varOut = (float) juce::jlimit (0.0, G->dur - n.varIn, G->dur - msAt (p.x));
        else if (k == "inMs") n.inMs = (float) juce::jlimit (0.0, juce::jmin (400.0, G->dur * 0.8), msAt (p.x));
        else if (k == "outMs") n.outMs = (float) juce::jlimit (0.0, juce::jmin (400.0, G->dur * 0.8), G->dur - msAt (p.x));
        else if (k == "gain") n.gain = juce::jlimit (-24.0f, 12.0f, std::round ((v.gain + dy * 0.1f) * 10) / 10);
        else if (k == "pitch") n.pitch = std::round ((v.pitch + dy / (e.mods.isAltDown() ? BAND * 4 : BAND)) * 100) / 100;
        else if (k == "var") n.drift = 1 - juce::jlimit (0.0f, 2.0f, (1 - v.drift) + dy / 90.0f);
        refreshCurves(); repaint();
        const auto now = juce::Time::getMillisecondCounter();
        if (now - lastLive > 200) { lastLive = now; editor.edited ({ n.id }); }
        editor.setStatus (midiName (n.pitch) + cents (n.pitch) + jp (" · 音量 ") + juce::String (n.gain, 1) + jp (" dB(入り ") + juce::String ((int) n.fadeIn) + " / " + jp ("終わり ") + juce::String ((int) n.fadeOut)
                          + jp (" ms) · 音程のつながり 入り ") + juce::String ((int) n.inMs) + " / " + jp ("終わり ") + juce::String ((int) n.outMs) + jp (" ms · 抑揚 ") + juce::String (juce::roundToInt ((1 - n.drift) * 100)) + "%");
        return;
    }
    if (drag.type == 1)
    {
        if (! drag.moved && std::abs (drag.p0.y - p.y) < 3) return;
        if (! drag.moved) { proc.pushUndo(); drag.moved = true; }
        float origPrim = 0; for (auto& o : drag.orig) if (o.first == drag.prim) origPrim = o.second;
        const float target = snapPitch (origPrim + (drag.p0.y - p.y) / view.lane, e.mods.isAltDown());
        const float delta = target - origPrim;
        for (auto& o : drag.orig) if (auto* n = noteById (o.first)) n->pitch = o.second + delta;
        refreshCurves(); repaint();
        const auto now = juce::Time::getMillisecondCounter();
        if (now - lastLive > 220) { lastLive = now; std::vector<int> ids; for (auto& o : drag.orig) ids.push_back (o.first); editor.edited (ids); }
        if (auto* n = noteById (drag.prim)) editor.setStatus (midiName (n->pitch) + cents (n->pitch) + jp ("(元: ") + midiName (n->center) + cents (n->center) + ")");
    }
    else if (drag.type == 2)
    {
        drag.p1 = p;
        const auto r = juce::Rectangle<float> (drag.p0, drag.p1);
        sel = drag.base;
        for (auto& n : proc.notes)
        {
            const float x0 = tx (frameSec (n.a)), x1 = tx (frameSec (n.b)), yc = py (n.pitch);
            if (x1 >= r.getX() && x0 <= r.getRight() && yc >= r.getY() && yc <= r.getBottom()) sel.insert (n.id);
        }
        repaint();
    }
}

void PianoRoll::mouseUp (const juce::MouseEvent&)
{
    if (advDragging) { advDragging = false; if (auto* n = noteById (adv)) editor.edited ({ n->id }); repaint(); return; }
    if (drag.type == 1 && drag.moved) { std::vector<int> ids; for (auto& o : drag.orig) ids.push_back (o.first); editor.edited (ids); }
    drag = {};
    repaint();
}

void PianoRoll::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! proc.session) return;
    if (adv >= 0 && advHit (e.position).isNotEmpty()) return;
    if (auto* n = hitNote (e.position)) openAdv (n->id);
}

void PianoRoll::mouseMove (const juce::MouseEvent& e)
{
    mouse = e.position;
    shiftHeld = e.mods.isShiftDown();
    if (adv >= 0) { const auto h = advHit (e.position); if (h.isNotEmpty()) {
        setMouseCursor (h == "gain" || h == "pitch" || h == "var" ? juce::MouseCursor::UpDownResizeCursor : h == "inside" ? juce::MouseCursor::NormalCursor : juce::MouseCursor::LeftRightResizeCursor);
        if (hoverCut) { hoverCut.reset(); repaint(); } return; } }
    updHover();
}
void PianoRoll::mouseExit (const juce::MouseEvent&) { mouse.reset(); updHover(); }

void PianoRoll::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (! proc.session) return;
    const float X = e.position.x;
    const float dy = -w.deltaY * 120.0f, dx = -w.deltaX * 120.0f;
    if (e.mods.isAltDown()) zoomV (std::exp (-dy * 0.002f));
    else if (e.mods.isCommandDown() || e.mods.isCtrlDown()) { const double t = xt (X); view.pps = juce::jlimit (5.0, 3000.0, view.pps * std::exp (-(dy != 0 ? dy : dx) * 0.0022)); view.t0 = t - (X - KB) / view.pps; }
    else if (e.mods.isShiftDown() || std::abs (dx) > std::abs (dy)) view.t0 += (dx != 0 ? dx : dy) / view.pps;
    else view.mTop += -dy / view.lane * 0.6f;
    mouse = e.position; shiftHeld = e.mods.isShiftDown(); hoverCut.reset(); updHover();
    if (adv >= 0) openAdv (adv);
    repaint();
}

//==============================================================================
std::optional<PianoRoll::AdvGeom> PianoRoll::advGeom()
{
    Note* n = noteById (adv);
    if (n == nullptr || ! proc.session) return std::nullopt;
    const float nx0 = tx (frameSec (n->a)), nx1 = tx (frameSec (n->b));
    const float w = juce::jlimit (380.0f, juce::jmax (380.0f, (float) getWidth() - KB - 16), nx1 - nx0);
    const float x0 = juce::jlimit (KB + 8, juce::jmax (KB + 8, (float) getWidth() - 8 - w), (nx0 + nx1) / 2 - w / 2);
    const float yc = py (n->pitch);
    const float y0 = juce::jlimit (RH + 4, juce::jmax (RH + 4, pBot() - 3 * BAND - 4), yc - BAND * 1.5f);
    return AdvGeom { n, x0, w, y0, x0 + w / 2, (n->b - n->a) * fr() * 1000.0 };
}

juce::String PianoRoll::advHit (juce::Point<float> p)
{
    auto G = advGeom(); if (! G) return {};
    const Note& n = *G->n;
    if (p.x < G->x0 - 8 || p.x > G->x0 + G->w + 8 || p.y < G->y0 - 10 || p.y > G->y0 + 3 * BAND + 10) return {};
    const int band = juce::jlimit (0, 2, (int) std::floor ((p.y - G->y0) / BAND));
    if (band == 0) { if (std::abs (p.x - G->mx (n.fadeIn)) < 8) return "fadeIn"; if (std::abs (p.x - G->ex (n.fadeOut)) < 8) return "fadeOut"; if (std::abs (p.x - G->cx) < 60) return "gain"; return "inside"; }
    if (band == 1) { if (std::abs (p.x - G->mx (n.inMs)) < 16) return "inMs"; if (std::abs (p.x - G->ex (n.outMs)) < 16) return "outMs"; if (std::abs (p.x - G->cx) < 60) return "pitch"; return "inside"; }
    if (std::abs (p.x - G->mx (n.varIn)) < 8) return "varIn"; if (std::abs (p.x - G->ex (n.varOut)) < 8) return "varOut"; if (std::abs (p.x - G->cx) < 60) return "var"; return "inside";
}

void PianoRoll::openAdv (int id)
{
    adv = id; sel = { id };
    auto G = advGeom(); if (! G) { adv = -1; return; }
    mini.setSize (300, 98);
    float top = G->y0 + 3 * BAND + 8;
    if (top + mini.getHeight() > pBot()) top = G->y0 - mini.getHeight() - 8;
    mini.setTopLeftPosition ((int) juce::jlimit (8.0f, (float) getWidth() - 308, G->x0), (int) juce::jlimit (RH, pBot() - mini.getHeight(), top));
    mini.show (*G->n);
    repaint();
}
void PianoRoll::closeAdv() { adv = -1; mini.setVisible (false); repaint(); }

static void tri (juce::Graphics& g, float x, float y, float w, float h, int dir, juce::Colour c)
{
    juce::Path p;
    if (dir == 0) p.addTriangle (x - w / 2, y, x + w / 2, y, x, y + h);           // down
    else if (dir == 1) p.addTriangle (x - w / 2, y, x + w / 2, y, x, y - h);      // up
    else if (dir == 2) p.addTriangle (x, y - h / 2, x, y + h / 2, x - w, y);      // left
    else p.addTriangle (x, y - h / 2, x, y + h / 2, x + w, y);                    // right
    g.setColour (c); g.fillPath (p);
}

void PianoRoll::drawAdv (juce::Graphics& g)
{
    auto G = advGeom(); if (! G) return;
    const Note& n = *G->n;
    const auto& A = proc.session->A;
    const juce::Colour LIGHT (0xffdde3ef), GRN (0x9e6ea878), GRN_L (0xff2f7d3d), RED (0x99c47680), RED_L (0xff8c2a22), ORG (0xfff0a24e), GREY (0x9e46506a), INK (0xff26304a);
    const float x0 = G->x0, w = G->w, y0 = G->y0, cx = G->cx;
    const int N = juce::jmax (2, n.b - n.a);
    auto fx = [&] (int i) { return x0 + (float) (i - n.a) / (float) N * w; };
    juce::DropShadow (juce::Colours::black.withAlpha (0.5f), 22, {}).drawForRectangle (g, juce::Rectangle<int> ((int) x0, (int) y0, (int) w, (int) (3 * BAND)));
    g.setColour (LIGHT); g.fillRect (x0, y0, w, 3 * BAND);
    float rmax = 1e-6f; for (int i = n.a; i < n.b; ++i) rmax = juce::jmax (rmax, A.rms[(size_t) i]);
    // volume
    {
        const float gl = std::pow (10.0f, n.gain / 20.0f);
        juce::Path p; p.startNewSubPath (x0, y0);
        for (int i = n.a; i < n.b; ++i)
        {
            const float t = (float) ((i - n.a + 0.5) * fr() * 1000), te = (float) ((n.b - i - 0.5) * fr() * 1000);
            float f = 1; if (n.fadeIn > 0) f *= ss (t / n.fadeIn); if (n.fadeOut > 0) f *= ss (te / n.fadeOut);
            p.lineTo (fx (i), y0 + juce::jmin (BAND - 2, A.rms[(size_t) i] / rmax * BAND * 0.62f * gl * f + 3));
        }
        p.lineTo (x0 + w, y0); p.closeSubPath();
        g.setColour (GREY); g.fillPath (p);
        g.setColour (GRN); g.fillRect (x0, y0, G->mx (n.fadeIn) - x0, BAND);
        g.setColour (RED); g.fillRect (G->ex (n.fadeOut), y0, x0 + w - G->ex (n.fadeOut), BAND);
        g.setColour (GRN_L); g.fillRect (G->mx (n.fadeIn) - 1.5f, y0, 3.0f, BAND);
        g.setColour (RED_L); g.fillRect (G->ex (n.fadeOut) - 1.5f, y0, 3.0f, BAND);
        tri (g, cx, y0, 64, 16, 0, ORG);
    }
    // pitch
    const float y2 = y0 + BAND, mid = y2 + BAND / 2;
    {
        g.setColour (juce::Colour (0xffe8ecf5)); g.fillRect (x0, y2, w, BAND);
        g.setColour (juce::Colours::white); g.drawRect (x0, y2, w, BAND, 1.0f);
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (juce::Rectangle<float> (x0, y2, w, BAND).toNearestInt());
        tri (g, cx, mid - 6, 46, 20, 1, GRN.withAlpha (0.85f)); tri (g, cx, mid + 6, 46, 20, 0, RED.withAlpha (0.85f));
        for (float hx : { G->mx (n.inMs), G->ex (n.outMs) }) { tri (g, hx - 3, mid, 18, BAND * 0.86f, 2, GRN.withAlpha (0.85f)); tri (g, hx + 3, mid, 18, BAND * 0.86f, 3, RED.withAlpha (0.85f)); }
        juce::Path line; bool pen = false;
        for (int i = n.a; i < n.b; ++i)
        {
            const float v = A.f0[(size_t) i];
            if (! voiced (v)) { pen = false; continue; }
            const float pp = v + (i < (int) curves.shift.size() ? curves.shift[(size_t) i] : 0.0f), Y = mid - (pp - n.pitch) * BAND * 0.42f;
            if (pen) line.lineTo (fx (i), Y); else line.startNewSubPath (fx (i), Y);
            pen = true;
        }
        g.setColour (ORG); g.strokePath (line, juce::PathStrokeType (4.0f));
        g.setColour (INK); g.strokePath (line, juce::PathStrokeType (1.6f));
    }
    // variation
    const float y3 = y0 + 2 * BAND;
    {
        float dmax = 0.15f; for (int i = n.a; i < n.b; ++i) { const float v = A.f0[(size_t) i]; if (voiced (v)) dmax = juce::jmax (dmax, std::abs (v - n.center)); }
        juce::Path p; p.startNewSubPath (x0, y3 + BAND);
        for (int i = n.a; i < n.b; ++i)
        {
            const float v = A.f0[(size_t) i], d = voiced (v) ? std::abs (v - n.center) : 0.0f;
            const float k = juce::jmax (0.0f, 1 - n.drift * varWeight (n, i, fr()));
            p.lineTo (fx (i), y3 + BAND - juce::jmin (BAND - 3, d / dmax * BAND * 0.5f * k + 3));
        }
        p.lineTo (x0 + w, y3 + BAND); p.closeSubPath();
        g.setColour (GREY); g.fillPath (p);
        g.setColour (GRN); g.fillRect (x0, y3, G->mx (n.varIn) - x0, BAND);
        g.setColour (RED); g.fillRect (G->ex (n.varOut), y3, x0 + w - G->ex (n.varOut), BAND);
        g.setColour (GRN_L); g.fillRect (G->mx (n.varIn) - 1.5f, y3, 3.0f, BAND);
        g.setColour (RED_L); g.fillRect (G->ex (n.varOut) - 1.5f, y3, 3.0f, BAND);
        tri (g, cx, y3 + BAND, 64, 16, 1, ORG);
    }
    g.setColour (INK); g.setFont (fonts().body (13.0f, true));
    g.drawText (jp ("音量 ") + (n.gain > 0 ? "+" : "") + juce::String (n.gain, 1) + " dB", juce::Rectangle<float> (cx - 100, y0 + BAND * 0.62f - 10, 200, 18), juce::Justification::centred);
    g.drawText (midiName (n.pitch) + cents (n.pitch), juce::Rectangle<float> (cx + 30, mid - 9, 90, 18), juce::Justification::centredLeft);
    g.drawText (jp ("抑揚 ") + juce::String (juce::roundToInt ((1 - n.drift) * 100)) + "%", juce::Rectangle<float> (cx - 100, y3 + BAND * 0.45f - 10, 200, 18), juce::Justification::centred);
    g.setColour (ORG); g.drawRect (x0, y0, w, 3 * BAND, 1.6f);
}

//==============================================================================
static bool isBlack (int pc) { return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }

void PianoRoll::paint (juce::Graphics& g)
{
    const float W = (float) getWidth(), H = (float) getHeight(), pTop = RH, pb = pBot();
    g.setColour (col::panel); g.fillRect (getLocalBounds());
    if (curvesGen != proc.generation.load()) refreshCurves();
    static const int MAJ[] = { 0, 2, 4, 5, 7, 9, 11 }, MIN[] = { 0, 2, 3, 5, 7, 8, 10 };
    auto inScale = [&] (int m) { if (proc.scale == chromatic) return false; const int pc = ((m - proc.key) % 12 + 12) % 12; const int* s = proc.scale == major ? MAJ : MIN; return std::find (s, s + 7, pc) != s + 7; };
    const int mHi = (int) std::ceil (view.mTop) + 1, mLo = (int) std::floor (ym (pb)) - 1;
    // lanes
    for (int m = mLo; m <= mHi; ++m)
    {
        const float y0 = py (m + 0.5f), y1 = py (m - 0.5f);
        if (y1 < pTop || y0 > pb) continue;
        const int pc = (m % 12 + 12) % 12;
        g.setColour (inScale (m) ? col::gold.withAlpha (0.045f) : isBlack (pc) ? juce::Colours::black.withAlpha (0.18f) : juce::Colours::white.withAlpha (0.012f));
        g.fillRect (KB, juce::jmax (pTop, y0), W - KB, juce::jmin (pb, y1) - juce::jmax (pTop, y0));
        g.setColour (pc == 0 ? col::line2 : col::line); g.drawHorizontalLine ((int) y1, KB, W);
    }
    // vertical grid
    const double bpm = proc.hostBpm.load();
    auto vlines = [&] (double step, juce::Colour c) {
        if (step * view.pps < 5) return;
        g.setColour (c);
        for (double t = std::floor (view.t0 / step) * step; t < xt (W); t += step) { const float X = tx (t); if (X >= KB) g.drawVerticalLine ((int) X, RH, H); } };
    if (proc.cutDiv) { vlines ((4.0 / proc.cutDiv) * 60 / bpm, col::gold.withAlpha (0.07f)); vlines (60 / bpm, col::ink.withAlpha (0.1f)); vlines (240 / bpm, col::ink.withAlpha (0.26f)); }
    else { static const double st[] = { 0.1, 0.25, 0.5, 1, 2, 5, 10, 30, 60 }; double step = 60; for (double s : st) if (s * view.pps >= 70) { step = s; break; } vlines (step, col::line); }

    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (juce::Rectangle<float> (KB, pTop, W - KB, pb - pTop).toNearestInt());
        if (proc.session)
        {
            const auto& A = proc.session->A;
            float rmax = 1e-6f; for (float v : A.rms) rmax = juce::jmax (rmax, v);
            const int fstep = juce::jmax (1, (int) (1.0 / (view.pps * fr()) / 1.5));
            const double iL = (view.t0 - proc.session->startSec()) / fr() - 2, iR = (xt (W) - proc.session->startSec()) / fr() + 2;
            for (auto& n : proc.notes)
            {
                if (n.b < iL || n.a > iR) continue;
                const bool isSel = sel.count (n.id) > 0;
                const float yc = py (n.pitch);
                if (std::abs (n.pitch - n.center) > 0.01f)
                {
                    const float yo = py (n.center);
                    juce::Path r; r.addRectangle (tx (frameSec (n.a)), yo - view.lane * 0.35f, (float) ((n.b - n.a) * fr() * view.pps), view.lane * 0.7f);
                    juce::Path d; const float dl[] = { 3, 4 }; juce::PathStrokeType (1.0f).createDashedStroke (d, r, dl, 2);
                    g.setColour (col::ink.withAlpha (0.28f)); g.fillPath (d);
                }
                juce::Path blob; std::vector<juce::Point<float>> bot;
                bool first = true;
                for (int i = n.a; i < n.b; i += fstep)
                {
                    const float h = juce::jmax (2.2f, view.lane * 0.5f * std::sqrt (A.rms[(size_t) i] / rmax)), X = tx (frameSec (i));
                    if (first) { blob.startNewSubPath (X, yc - h); first = false; } else blob.lineTo (X, yc - h);
                    bot.push_back ({ X, yc + h });
                }
                const float X1 = tx (frameSec (n.b));
                blob.lineTo (X1, yc - 2); blob.lineTo (X1, yc + 2);
                for (auto it = bot.rbegin(); it != bot.rend(); ++it) blob.lineTo (*it);
                blob.closeSubPath();
                g.setGradientFill (juce::ColourGradient (isSel ? juce::Colour (0xffffb3bd) : juce::Colour (0xffffe2a6), 0, yc - view.lane * 0.5f, isSel ? col::sel : col::gold, 0, yc + view.lane * 0.5f, false));
                g.setOpacity (0.92f); g.fillPath (blob); g.setOpacity (1.0f);
                if (isSel) { g.setColour (juce::Colours::white); g.strokePath (blob, juce::PathStrokeType (1.2f)); }
                const float wpx = (float) ((n.b - n.a) * fr() * view.pps);
                if (wpx > 34 && view.lane >= 11)
                {
                    g.setColour (isSel ? juce::Colour (0xffffd1d7) : col::gold); g.setFont (fonts().body (11.5f, true));
                    g.drawText (midiName (n.pitch) + cents (n.pitch), juce::Rectangle<float> (tx (frameSec (n.a)) + 2, yc - view.lane * 0.55f - 17, 140, 15), juce::Justification::left);
                }
            }
            // original pitch (faint) and corrected pitch, inside the notes
            for (int pass = 0; pass < 2; ++pass)
            {
                juce::Path p;
                for (auto& n : proc.notes)
                {
                    if (n.b < iL || n.a > iR) continue;
                    bool pen = false;
                    for (int i = juce::jmax (n.a, (int) iL); i < juce::jmin (n.b, (int) iR + 1); ++i)
                    {
                        const float v = A.f0[(size_t) i];
                        if (! voiced (v)) { pen = false; continue; }
                        const float m = pass == 0 ? v : v + (i < (int) curves.shift.size() ? curves.shift[(size_t) i] : 0.0f);
                        const float X = tx (frameSec (i)), Y = py (m);
                        if (pen) p.lineTo (X, Y); else p.startNewSubPath (X, Y);
                        pen = true;
                    }
                }
                g.setColour (pass == 0 ? col::ink.withAlpha (0.3f) : col::curve);
                g.strokePath (p, juce::PathStrokeType (pass == 0 ? 1.0f : 1.8f));
            }
            if (hoverCut)
                if (auto* n = noteById (hoverCut->id))
                {
                    const float yc = py (n->pitch), h = juce::jmax (10.0f, view.lane * 0.75f);
                    juce::Path l; l.startNewSubPath (hoverCut->x, yc - h); l.lineTo (hoverCut->x, yc + h);
                    juce::Path d; const float dl[] = { 4, 3 }; juce::PathStrokeType (2.0f).createDashedStroke (d, l, dl, 2);
                    g.setColour (juce::Colours::white); g.fillPath (d);
                    g.setColour (col::sel); g.setFont (fonts().body (15.0f, true));
                    g.drawText (juce::String::fromUTF8 ("✂"), juce::Rectangle<float> (hoverCut->x - 12, yc - h - 20, 24, 18), juce::Justification::centred);
                }
        }
        else
        {
            g.setColour (col::muted); g.setFont (fonts().body (15.0f));
            g.drawFittedText (jp ("FLを再生しながら「● 取り込み」で歌を取り込むか、「ファイルを開く」でボーカルを読み込んでください。\n取り込み終わったら(FLを止めると)自動で分析して音符が出ます。"),
                              juce::Rectangle<int> ((int) KB + 20, (int) pTop, (int) (W - KB - 40), (int) (pb - pTop)), juce::Justification::centred, 3);
        }
        if (drag.type == 2)
        {
            const auto r = juce::Rectangle<float> (drag.p0, drag.p1);
            g.setColour (col::curve.withAlpha (0.12f)); g.fillRect (r);
            g.setColour (col::curve); g.drawRect (r, 1.0f);
        }
    }
    if (proc.session) drawAdv (g);
    // playhead + start marker
    const double ph = proc.hostPlaying.load() ? proc.hostSec.load() : proc.previewOn.load() ? proc.previewSec.load() : cursorSec;
    const float px = tx (ph);
    if ((proc.hostPlaying.load() || proc.previewOn.load()))
    {
        const float cx = tx (cursorSec);
        if (proc.previewOn.load() && cx >= KB && cx <= W) { juce::Path l; l.startNewSubPath (cx, RH); l.lineTo (cx, H); juce::Path d; const float dl[] = { 4, 4 }; juce::PathStrokeType (1.0f).createDashedStroke (d, l, dl, 2); g.setColour (col::gold); g.fillPath (d); }
    }
    if (px >= KB && px <= W)
    {
        g.setColour (col::ink); g.drawLine (px, 0, px, H, 1.5f);
        juce::Path t; t.addTriangle (px - 5, 0, px + 5, 0, px, 7); g.fillPath (t);
    }
    drawKeyboard (g);
    drawRuler (g);
    drawWave (g);
    g.setColour (col::ink.withAlpha (0.05f)); g.setFont (fonts().display (34.0f));
    g.drawText ("KANADE", juce::Rectangle<float> (W - 330, pb - 54, 316, 40), juce::Justification::right);
    if (proc.analysing.load() || proc.recArmed.load())
    {
        g.setColour (col::bg.withAlpha (0.72f)); g.fillRect (KB, RH, W - KB, pb - RH);
        g.setColour (col::ink); g.setFont (fonts().body (15.0f, true));
        const bool rec = proc.recArmed.load();
        const juce::String msg = rec ? (proc.capturedSec.load() > 0 ? jp ("取り込み中… ") + fmtTime (proc.capturedSec.load()) + jp ("(FLを止めると分析します)") : jp ("取り込み待ち:FLで歌の部分を再生してください"))
                                     : jp ("歌を分析中… ") + juce::String (juce::roundToInt (proc.progress.load() * 100)) + "%";
        g.drawText (msg, juce::Rectangle<float> (KB, (RH + pb) / 2 - 30, W - KB, 24), juce::Justification::centred);
        if (! rec) { const juce::Rectangle<float> bar ((KB + W) / 2 - 120, (RH + pb) / 2 + 4, 240, 6);
            g.setColour (col::pane2); g.fillRoundedRectangle (bar, 3); g.setColour (col::gold); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * proc.progress.load()), 3); }
        else { g.setColour (col::rec); g.fillEllipse ((KB + W) / 2 - 7, (RH + pb) / 2 + 2, 14, 14); }
    }
}

void PianoRoll::drawKeyboard (juce::Graphics& g)
{
    const float H = (float) getHeight(), pb = pBot();
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (juce::Rectangle<float> (0, RH, KB, pb - RH).toNearestInt());
    g.setColour (col::pane); g.fillRect (0.0f, 0.0f, KB, H);
    static const int WHITE[] = { 0, 2, 4, 5, 7, 9, 11 };
    const int mHi = (int) std::ceil (view.mTop) + 1, mLo = (int) std::floor (ym (pb)) - 1;
    for (int o = (int) std::floor (mLo / 12.0) - 1; o <= (int) std::floor (mHi / 12.0) + 1; ++o)
    {
        const int base = o * 12;
        const float kh = 12 * view.lane / 7;
        for (int k = 0; k < 7; ++k)
        {
            const float yb = py (base - 0.5f) - k * kh, ya = yb - kh;
            g.setColour (WHITE[k] == 0 ? juce::Colour (base + WHITE[k] == 60 ? 0xffc9cfe0 : 0xffd3d8e6) : juce::Colour (0xffe6e9f2));
            g.fillRect (0.0f, ya, KB, kh);
            g.setColour (juce::Colour (0x73141a2e)); g.drawHorizontalLine ((int) yb, 0, KB);
            if (WHITE[k] == 0)
            {
                g.setColour (juce::Colour (0xff3a4360)); g.setFont (fonts().body (juce::jlimit (10.0f, 15.0f, view.lane * 0.5f)));
                g.drawText (midiName ((float) base), juce::Rectangle<float> (0, yb - juce::jmin (kh, view.lane) - 1, KB - 6, juce::jmin (kh, view.lane)), juce::Justification::centredRight);
            }
        }
        for (int pc : { 1, 3, 6, 8, 10 })
        {
            const float y0 = py (base + pc + 0.5f), y1 = py (base + pc - 0.5f), bw = KB * 0.6f;
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff11152a), 0, 0, juce::Colour (0xff2b3150), bw * 0.85f, 0, false));
            g.fillRect (0.0f, y0 + 0.5f, bw, y1 - y0 - 1);
            g.setColour (juce::Colours::white.withAlpha (0.08f)); g.fillRect (bw - 4, y0 + 1, 3.0f, y1 - y0 - 2);
        }
    }
    g.setColour (col::line2); g.drawVerticalLine ((int) KB, 0, H);
}

void PianoRoll::drawRuler (juce::Graphics& g)
{
    const float W = (float) getWidth();
    g.setColour (col::pane.withAlpha (0.95f)); g.fillRect (0.0f, 0.0f, W, RH - 1);
    g.setColour (col::line2); g.drawHorizontalLine ((int) RH - 1, 0, W);
    g.setFont (fonts().mono (10.5f));
    const double bpm = proc.hostBpm.load();
    if (proc.cutDiv)
    {
        const double bs = 240 / bpm, beat = 60 / bpm;
        const int every = juce::jmax (1, (int) std::ceil (46 / (bs * view.pps)));
        for (int b = juce::jmax (0, (int) std::floor (view.t0 / bs)); b * bs < xt (W); ++b)
        {
            const float X = tx (b * bs); if (X < KB || b % every) continue;
            g.setColour (col::gold); g.drawText (juce::String (b + 1), juce::Rectangle<float> (X + 4, 4, 60, 16), juce::Justification::left);
        }
        if (beat * view.pps > 40)
            for (int k = juce::jmax (0, (int) std::floor (view.t0 / beat)); k * beat < xt (W); ++k)
            {
                if (k % 4 == 0) continue;
                const float X = tx (k * beat); if (X < KB) continue;
                g.setColour (col::muted); g.drawText (juce::String (k / 4 + 1) + "." + juce::String (k % 4 + 1), juce::Rectangle<float> (X + 4, 4, 60, 16), juce::Justification::left);
            }
    }
    else
    {
        static const double st[] = { 0.1, 0.25, 0.5, 1, 2, 5, 10, 30, 60 };
        double step = 60; for (double s : st) if (s * view.pps >= 70) { step = s; break; }
        for (double t = std::floor (view.t0 / step) * step; t < xt (W); t += step)
        {
            const float X = tx (t); if (X < KB) continue;
            g.setColour (col::muted); g.drawText (step < 0.5 ? juce::String ((int) (t / 60)) + ":" + juce::String (std::fmod (t, 60.0), 2).paddedLeft ('0', 5) : fmtTime (t), juce::Rectangle<float> (X + 4, 4, 70, 16), juce::Justification::left);
        }
    }
    g.setColour (col::pane); g.fillRect (0.0f, 0.0f, KB, RH - 1);
}

void PianoRoll::drawWave (juce::Graphics& g)
{
    const float W = (float) getWidth(), H = (float) getHeight(), pb = pBot();
    g.setColour (col::pane); g.fillRect (0.0f, pb + 1, W, WH);
    g.setColour (col::line2); g.drawHorizontalLine ((int) pb, 0, W);
    const bool before = proc.bypass.load();
    g.setColour (col::muted); g.setFont (fonts().body (11.0f, true));
    g.drawText (before ? jp ("補正前") : jp ("補正後"), 8, (int) pb + 6, 80, 14, juce::Justification::left);
    const Playback* P = proc.livePlayback();
    if (P == nullptr || P->s == nullptr) return;
    const auto& data = before ? P->s->audio : P->out;
    const double sr = P->s->sr; const juce::int64 st = P->s->start;
    const float mid = pb + 1 + WH / 2;
    g.setColour (before ? col::ink.withAlpha (0.45f) : col::curve.withAlpha (0.65f));
    for (int X = (int) KB; X < (int) W; ++X)
    {
        const juce::int64 s0 = (juce::int64) (xt ((float) X) * sr) - st, s1 = (juce::int64) (xt ((float) X + 1) * sr) - st;
        if (s1 < 0 || s0 >= (juce::int64) data.size()) continue;
        float mn = 0, mx = 0;
        const juce::int64 stp = juce::jmax<juce::int64> (1, (s1 - s0) >> 5);
        for (juce::int64 s = juce::jmax<juce::int64> (0, s0); s < juce::jmin<juce::int64> ((juce::int64) data.size(), s1); s += stp) { const float v = data[(size_t) s]; mn = juce::jmin (mn, v); mx = juce::jmax (mx, v); }
        g.fillRect ((float) X, mid - mx * (WH / 2 - 4), 1.0f, juce::jmax (1.0f, (mx - mn) * (WH / 2 - 4)));
    }
    juce::ignoreUnused (H);
}

void PianoRoll::tick()
{
    const bool moving = proc.hostPlaying.load() || proc.previewOn.load();
    if (moving && follow && proc.session)
    {
        const double p = proc.hostPlaying.load() ? proc.hostSec.load() : proc.previewSec.load();
        const float X = tx (p);
        if (X > getWidth() - 40 || X < KB) { view.t0 = p - (getWidth() - KB) * 0.15 / view.pps; if (adv >= 0) openAdv (adv); }
    }
    if (moving || proc.analysing.load() || proc.recArmed.load() || proc.rendering.load()) repaint();
}

//==============================================================================
Content::Content (KanadeEditor& e)
    : editor (e), logo (e), dragSq (e),
      kC ("CENTER", col::gold, [] (double v) { return juce::String (juce::jmax (1, juce::roundToInt (v * 100))); }),
      kV ("VARIATION", col::curve, [] (double v) { return juce::String (juce::roundToInt (v * 200)) + "%"; }),
      kT ("TRANS", col::sel, [] (double v) { return v >= 0.995 ? jp ("ケロケロ") : juce::String (juce::roundToInt ((1 - v) * 300)) + " ms"; }),
      roll (e)
{
    auto& p = e.proc;
    addAndMakeVisible (logo);
    for (auto* b : { &scaleBtn, &snapBtn, &undoBtn, &redoBtn, &exportBtn, &playBtn, &followBtn, &stopBtn, &afterBtn, &beforeBtn, &recBtn, &openBtn, &clearBtn,
                     &selBtn, &splitBtn, &mergeBtn, &zhOut, &zhIn, &zvOut, &zvIn, &zFit, &applyBtn })
        addAndMakeVisible (b);
    addAndMakeVisible (dragSq);
    dragSq.setTooltip (jp ("ドラッグ:FLのプレイリストへ(補正した歌がオーディオクリップになります) / クリック:WAVを保存"));
    scaleBtn.setButtonText (editor.scaleName());
    snapBtn.setButtonText (jp ("スケールにスナップ")); snapBtn.setToggleState (p.snapOn, juce::dontSendNotification);
    snapBtn.setTooltip (jp ("音符を動かすとき、選んだスケールの音に吸着(オフ=半音ごと)"));
    undoBtn.setButtonText (juce::String::fromUTF8 ("↶")); redoBtn.setButtonText (juce::String::fromUTF8 ("↷"));
    undoBtn.setTooltip (jp ("元に戻す (⌘Z)")); redoBtn.setTooltip (jp ("やり直す (⇧⌘Z)"));
    exportBtn.setButtonText (jp ("WAVで書き出し"));
    exportBtn.setColour (juce::TextButton::buttonColourId, col::gold); exportBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff241a05));
    playBtn.setButtonText (juce::String::fromUTF8 ("▶")); playBtn.setColour (juce::TextButton::buttonColourId, col::gold); playBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff241a05));
    playBtn.setTooltip (jp ("クリックした位置から試聴(FLが止まっているとき・何度でもそこから)"));
    followBtn.setButtonText (juce::String::fromUTF8 ("❙❙▶")); followBtn.setToggleState (true, juce::dontSendNotification);
    followBtn.setTooltip (jp ("再生位置を追いかける(オン/オフ)"));
    stopBtn.setButtonText (juce::String::fromUTF8 ("■")); stopBtn.setTooltip (jp ("試聴を止める(開始位置に戻る)"));
    afterBtn.setButtonText (jp ("補正後")); beforeBtn.setButtonText (jp ("補正前"));
    for (auto* b : { &afterBtn, &beforeBtn }) { b->setColour (juce::TextButton::buttonOnColourId, col::ink); b->setColour (juce::TextButton::textColourOnId, col::bg); }
    afterBtn.setToggleState (! p.bypass.load(), juce::dontSendNotification); beforeBtn.setToggleState (p.bypass.load(), juce::dontSendNotification);
    recBtn.setButtonText (jp ("● 取り込み")); recBtn.setColour (juce::TextButton::buttonOnColourId, col::rec); recBtn.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    recBtn.setTooltip (jp ("オンにしてFLで歌の部分を再生 → FLを止めると取り込んで分析します"));
    openBtn.setButtonText (jp ("ファイルを開く")); clearBtn.setButtonText (jp ("クリア")); clearBtn.setColour (juce::TextButton::textColourOffId, col::danger);
    selBtn.setButtonText (jp ("選択")); splitBtn.setButtonText (jp ("分割 C")); mergeBtn.setButtonText (jp ("つなげる"));
    selBtn.setToggleState (true, juce::dontSendNotification);
    zhOut.setButtonText (juce::String::fromUTF8 ("－")); zhIn.setButtonText ("+"); zvOut.setButtonText (juce::String::fromUTF8 ("－")); zvIn.setButtonText ("+"); zFit.setButtonText (jp ("全体"));
    applyBtn.setButtonText (jp ("この設定で補正する")); applyBtn.setColour (juce::TextButton::buttonColourId, col::gold); applyBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff241a05));
    for (auto* l : { &timeLbl, &bpmLbl, &status, &targetLbl }) addAndMakeVisible (l);
    timeLbl.setFont (fonts().mono (15.0f)); bpmLbl.setFont (fonts().mono (12.5f)); bpmLbl.setColour (juce::Label::textColourId, col::muted);
    status.setFont (fonts().body (13.0f)); status.setColour (juce::Label::textColourId, col::muted); status.setMinimumHorizontalScale (0.7f);
    targetLbl.setFont (fonts().body (12.0f, true)); targetLbl.setColour (juce::Label::textColourId, col::muted);
    addAndMakeVisible (cutBox);
    for (int d : { 2, 4, 8, 16, 32 }) cutBox.addItem ("1/" + juce::String (d), d);
    cutBox.addItem ("No snap", 99);
    cutBox.setSelectedId (p.cutDiv ? p.cutDiv : 99, juce::dontSendNotification);
    cutBox.onChange = [this] { const int id = cutBox.getSelectedId(); editor.proc.cutDiv = id == 99 ? 0 : id; roll.repaint();
        editor.setStatus (editor.proc.cutDiv ? jp ("切る位置をFLのテンポの 1/") + juce::String (editor.proc.cutDiv) + jp (" に合わせます(上の数字は小節)。") : jp ("切る位置は自由です(No snap)。")); };
    for (auto* k : { &kC, &kV, &kT }) addAndMakeVisible (k);
    kC.slider.setValue (p.kCenter, juce::dontSendNotification); kV.slider.setValue (p.kVariation, juce::dontSendNotification); kT.slider.setValue (p.kTrans, juce::dontSendNotification);
    kC.slider.setDoubleClickReturnValue (true, 1.0); kV.slider.setDoubleClickReturnValue (true, 0.5); kT.slider.setDoubleClickReturnValue (true, 0.9);
    kC.slider.setTooltip (jp ("センター:音程の中心をスケールの音に合わせる濃さ(1〜100、100でぴったり)"));
    kV.slider.setTooltip (jp ("バリエーション:音程の線の抑揚。0%で平ら、100%で元のまま、200%で強調"));
    kT.slider.setTooltip (jp ("トランス:上げるほど音の変わり目が速く(いちばん上でケロケロ)、下げるほど滑らかに"));
    for (auto* k : { &kC, &kV, &kT }) { k->slider.onDragStart = [this] { editor.knobUndoPending = true; }; }
    kC.slider.onValueChange = [this] { editor.proc.kCenter = (float) juce::jmax (0.01, kC.slider.getValue()); editor.applyKnob (0, true); };
    kV.slider.onValueChange = [this] { editor.proc.kVariation = (float) kV.slider.getValue(); editor.applyKnob (1, true); };
    kT.slider.onValueChange = [this] { editor.proc.kTrans = (float) kT.slider.getValue(); editor.applyKnob (2, true); };
    addAndMakeVisible (formant);
    formant.setSliderStyle (juce::Slider::LinearHorizontal); formant.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    formant.setRange (-6, 6, 0.5); formant.setValue (p.globalFormant, juce::dontSendNotification);
    formant.setColour (juce::Slider::trackColourId, col::gold.withAlpha (0.6f));
    formant.setDoubleClickReturnValue (true, 0);
    formant.onDragStart = [this] { editor.proc.pushUndo(); };
    formant.onValueChange = [this] { editor.proc.globalFormant = (float) formant.getValue(); editor.edited ({}, true); repaint(); };
    addAndMakeVisible (roll);

    scaleBtn.onClick = [this] { editor.showScaleMenu(); };
    snapBtn.onClick = [this] { auto& pp = editor.proc; pp.snapOn = ! pp.snapOn; snapBtn.setToggleState (pp.snapOn, juce::dontSendNotification);
        editor.setStatus (pp.snapOn ? jp ("スケールにスナップ:オン(音符を動かすとスケールの音に吸着)") : jp ("スケールにスナップ:オフ(半音ごとに吸着)")); };
    undoBtn.onClick = [this] { if (editor.proc.undo()) { roll.refreshCurves(); refresh(); roll.repaint(); } };
    redoBtn.onClick = [this] { if (editor.proc.redo()) { roll.refreshCurves(); refresh(); roll.repaint(); } };
    playBtn.onClick = [this] { if (! editor.proc.session) return; if (editor.proc.hostPlaying.load()) { editor.setStatus (jp ("FLの再生中はFLの再生で聴けます。試聴はFLを止めてから。")); return; } editor.proc.preview (roll.cursorSec); };
    stopBtn.onClick = [this] { editor.proc.stopPreview(); roll.repaint(); };
    followBtn.onClick = [this] { roll.follow = ! roll.follow; followBtn.setToggleState (roll.follow, juce::dontSendNotification);
        editor.setStatus (roll.follow ? jp ("再生位置を追いかける:オン") : jp ("再生位置を追いかける:オフ(画面は動きません)")); };
    afterBtn.onClick = [this] { editor.proc.bypass = false; afterBtn.setToggleState (true, juce::dontSendNotification); beforeBtn.setToggleState (false, juce::dontSendNotification); roll.repaint(); };
    beforeBtn.onClick = [this] { editor.proc.bypass = true; afterBtn.setToggleState (false, juce::dontSendNotification); beforeBtn.setToggleState (true, juce::dontSendNotification); roll.repaint(); };
    recBtn.onClick = [this] {
        auto& pp = editor.proc;
        if (pp.recArmed.load()) { pp.armRecord (false); refresh(); return; }
        const auto now = juce::Time::getMillisecondCounter();
        if (pp.session && now - clearArm > 3000) { clearArm = now; editor.setStatus (jp ("もう一度押すと取り込み直します(今の編集は消えます)。"), true); return; }
        clearArm = 0;
        pp.armRecord (true); roll.closeAdv(); refresh();
        editor.setStatus (jp ("取り込み待ち:FLで直したい歌の部分を再生して、終わったらFLを止めてください。")); };
    openBtn.onClick = [this] {
        editor.chooser = std::make_unique<juce::FileChooser> (jp ("ボーカルのファイルを開く"), juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        editor.chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc) {
            const auto f = fc.getResult(); if (! f.existsAsFile()) return;
            juce::String err;
            if (editor.proc.loadFile (f, err)) { roll.closeAdv(); editor.setStatus (jp ("「") + f.getFileName() + jp ("」を分析しています…(曲の頭 = 1小節目に置きます)")); }
            else editor.setStatus (err, true); }); };
    clearBtn.onClick = [this] {
        const auto now = juce::Time::getMillisecondCounter();
        if (now - clearArm > 3000) { clearArm = now; clearBtn.setButtonText (jp ("本当に?")); return; }
        clearArm = 0; clearBtn.setButtonText (jp ("クリア"));
        editor.proc.clearSession(); roll.closeAdv(); roll.sel.clear(); refresh(); roll.repaint(); };
    selBtn.onClick = [this] { editor.setTool (false); };
    splitBtn.onClick = [this] { editor.setTool (true); };
    mergeBtn.onClick = [this] {
        auto& pp = editor.proc;
        std::vector<Note*> s; for (auto& n : pp.notes) if (roll.sel.count (n.id)) s.push_back (&n);
        if (s.size() < 2) { editor.setStatus (jp ("隣り合う音符を2つ以上選んでから押してください。"), true); return; }
        std::sort (s.begin(), s.end(), [] (Note* a, Note* b) { return a->a < b->a; });
        bool any = false; for (size_t k = 1; k < s.size(); ++k) if (s[k]->a - s[k - 1]->b <= 3) any = true;
        if (! any) { editor.setStatus (jp ("隣り合っていない音符はつなげられません。"), true); return; }
        pp.pushUndo();
        std::vector<int> gone; int merged = 0;
        Note* head = s[0];
        for (size_t k = 1; k < s.size(); ++k)
        {
            if (s[k]->a - head->b <= 3) { head->b = s[k]->b; gone.push_back (s[k]->id); ++merged; }
            else head = s[k];
        }
        for (auto& n : pp.notes) if (std::find (gone.begin(), gone.end(), n.id) == gone.end()) { /* keep */ }
        std::vector<int> heads; for (auto* n : s) if (std::find (gone.begin(), gone.end(), n->id) == gone.end()) heads.push_back (n->id);
        for (auto& n : pp.notes) if (std::find (heads.begin(), heads.end(), n.id) != heads.end()) { const float off = n.pitch - n.center; recenter (n, pp.session->A); n.pitch = n.center + off; }
        pp.notes.erase (std::remove_if (pp.notes.begin(), pp.notes.end(), [&] (const Note& n) { return std::find (gone.begin(), gone.end(), n.id) != gone.end(); }), pp.notes.end());
        roll.sel = std::set<int> (heads.begin(), heads.end());
        editor.edited ({}, true);
        editor.setStatus (juce::String (merged + 1) + jp ("個の音符を1つにしました。")); };
    zhOut.onClick = [this] { roll.zoomH (1 / 1.5f); }; zhIn.onClick = [this] { roll.zoomH (1.5f); };
    zvOut.onClick = [this] { roll.zoomV (1 / 1.25f); }; zvIn.onClick = [this] { roll.zoomV (1.25f); };
    zFit.onClick = [this] { roll.fitView(); };
    applyBtn.onClick = [this] {
        if (! editor.proc.session) { editor.setStatus (jp ("先に歌を取り込んでください。"), true); return; }
        editor.applyKnob (3, true);
        auto& pp = editor.proc;
        editor.setStatus (juce::String ((int) editor.targets().size()) + jp ("個の音符を補正しました(CENTER ") + juce::String (juce::roundToInt (pp.kCenter * 100)) + " / VARIATION " + juce::String (juce::roundToInt (pp.kVariation * 200))
                          + "% / TRANS " + (pp.kTrans >= 0.995f ? jp ("ケロケロ") : juce::String (juce::roundToInt ((1 - pp.kTrans) * 300)) + " ms") + jp (" · ") + editor.scaleName().upToFirstOccurrenceOf (juce::String::fromUTF8 (" ▾"), false, false) + ")"); };
    exportBtn.onClick = [this] {
        if (! editor.proc.session) { editor.setStatus (jp ("先に歌を取り込んでください。"), true); return; }
        editor.chooser = std::make_unique<juce::FileChooser> (jp ("補正した歌をWAVで書き出し"), juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile (juce::File::createLegalFileName (editor.proc.session->name) + "_KANADE.wav"), "*.wav");
        editor.chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc) {
            auto f = fc.getResult(); if (f == juce::File()) return;
            if (! f.hasFileExtension ("wav")) f = f.withFileExtension ("wav");
            if (editor.proc.writeWav (f)) editor.setStatus (jp ("「") + f.getFileName() + jp ("」を書き出しました(24bit)。FLにドラッグして使えます。"));
            else editor.setStatus (jp ("書き出せませんでした。"), true); }); };
}

void Content::refresh()
{
    auto& p = editor.proc;
    scaleBtn.setButtonText (editor.scaleName());
    undoBtn.setEnabled (p.canUndo()); redoBtn.setEnabled (p.canRedo());
    recBtn.setToggleState (p.recArmed.load(), juce::dontSendNotification);
    recBtn.setButtonText (p.recArmed.load() ? (p.capturedSec.load() > 0 ? jp ("● 取り込み中 ") + juce::String (p.capturedSec.load(), 1) + "s" : jp ("● 待機中")) : jp ("● 取り込み"));
    targetLbl.setText (roll.sel.empty() ? jp ("対象:全部") : jp ("対象:選んだ") + juce::String ((int) roll.sel.size()) + jp ("個"), juce::dontSendNotification);
    const double t = p.hostPlaying.load() ? p.hostSec.load() : p.previewOn.load() ? p.previewSec.load() : roll.cursorSec;
    timeLbl.setText (fmtTime (t), juce::dontSendNotification);
    bpmLbl.setText ("BPM " + juce::String (p.hostBpm.load(), 1) + " (FL)", juce::dontSendNotification);
    formant.setValue (p.globalFormant, juce::dontSendNotification);
}

void Content::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b2550), (float) W / 2, -80, col::bg, (float) W / 2, (float) H * 0.8f, true));
    g.fillAll();
    // wordmark
    g.setGradientFill (juce::ColourGradient (col::gold, 76, 0, col::curve, 300, 0, false));
    g.setFont (fonts().display (30.0f));
    g.drawText ("K A N A D E", 76, 12, 240, 36, juce::Justification::left);
    g.setColour (col::gold2); g.setFont (fonts().body (17.0f, true)); g.drawText (juce::String::fromUTF8 ("奏"), 244, 16, 30, 30, juce::Justification::left);
    g.setColour (col::muted); g.setFont (fonts().body (12.0f));
    g.drawText (jp ("DJ KATSUNII · かつにい — 歌の音程を音符ごとに直す、メロダイン風ピッチ補正"), 78, 50, 560, 18, juce::Justification::left);
    // bars
    for (auto r : { juce::Rectangle<float> (16, 86, W - 32, 58), juce::Rectangle<float> (16, 152, W - 32, 92) })
    {
        g.setColour (col::panel); g.fillRoundedRectangle (r, 12);
        g.setColour (col::line2); g.drawRoundedRectangle (r, 12, 1.0f);
    }
    g.setColour (col::muted); g.setFont (fonts().body (11.0f, true));
    g.drawText (jp ("切る位置"), 1040, 90, 80, 14, juce::Justification::left);
    g.drawText (jp ("ノブは選んだ音符に(選んでいなければ全部に)すぐかかります"), 300, 160, 330, 16, juce::Justification::left);
    g.drawText (jp ("声の太さ(フォルマント・全体)"), 680, 166, 220, 14, juce::Justification::left);
    g.drawText (jp ("横"), 920, 196, 20, 16, juce::Justification::left);
    g.drawText (jp ("縦"), 1020, 196, 20, 16, juce::Justification::left);
    g.setColour (col::ink); g.setFont (fonts().mono (12.0f));
    g.drawText ((editor.proc.globalFormant > 0 ? "+" : "") + juce::String (editor.proc.globalFormant, 1), 880, 186, 40, 22, juce::Justification::left);
}

void Content::resized()
{
    logo.setBounds (14, 6, 56, 74);
    exportBtn.setBounds (W - 16 - 124, 26, 124, 30);
    dragSq.setBounds (exportBtn.getX() - 40, 26, 32, 30);
    redoBtn.setBounds (dragSq.getX() - 44, 26, 34, 30);
    undoBtn.setBounds (redoBtn.getX() - 38, 26, 34, 30);
    snapBtn.setBounds (undoBtn.getX() - 152, 26, 142, 30);
    scaleBtn.setBounds (snapBtn.getX() - 176, 26, 168, 30);

    playBtn.setBounds (28, 96, 38, 38); followBtn.setBounds (72, 96, 44, 38); stopBtn.setBounds (122, 96, 38, 38);
    timeLbl.setBounds (166, 100, 90, 30);
    afterBtn.setBounds (258, 102, 64, 28); beforeBtn.setBounds (322, 102, 64, 28);
    recBtn.setBounds (398, 102, 132, 28); openBtn.setBounds (534, 102, 116, 28); clearBtn.setBounds (654, 102, 70, 28);
    selBtn.setBounds (736, 102, 52, 28); splitBtn.setBounds (792, 102, 66, 28); mergeBtn.setBounds (862, 102, 74, 28);
    bpmLbl.setBounds (940, 104, 96, 24);
    cutBox.setBounds (1040, 106, 100, 26);

    kC.setBounds (28, 158, 84, 84); kV.setBounds (116, 158, 84, 84); kT.setBounds (204, 158, 84, 84);
    applyBtn.setBounds (300, 182, 168, 30); targetLbl.setBounds (476, 184, 160, 26);
    formant.setBounds (676, 186, 200, 22);
    zhOut.setBounds (938, 190, 32, 28); zhIn.setBounds (974, 190, 32, 28);
    zvOut.setBounds (1038, 190, 32, 28); zvIn.setBounds (1074, 190, 32, 28); zFit.setBounds (1116, 190, 96, 28);
    status.setBounds (16, 248, W - 32, 22);
    roll.setBounds (16, 274, W - 32, H - 274 - 12);
}

//==============================================================================
KanadeEditor::KanadeEditor (KanadeProcessor& p) : AudioProcessorEditor (&p), proc (p), content (*this)
{
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    setWantsKeyboardFocus (true);
    setResizable (true, true);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) Content::W / Content::H);
    setResizeLimits (744, 540, 1860, 1350);
    setSize (1054, 765);
    content.refresh();
    setStatus (proc.session ? jp ("音符をドラッグで直せます。ダブルクリックで詳しい編集。") : jp ("「● 取り込み」を押してFLで歌を再生 → FLを止めると分析して音符が出ます。"));
    lastTick = juce::Time::getMillisecondCounter();
    startTimerHz (30);
}
KanadeEditor::~KanadeEditor() { stopTimer(); proc.stopPreview(); setLookAndFeel (nullptr); }
void KanadeEditor::paint (juce::Graphics& g) { g.fillAll (col::bg); }
void KanadeEditor::resized()
{
    content.setBounds (0, 0, Content::W, Content::H);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) Content::W));
}

void KanadeEditor::setStatus (const juce::String& s, bool error)
{
    content.status.setText (s, juce::dontSendNotification);
    content.status.setColour (juce::Label::textColourId, error ? col::danger : col::muted);
    statusTimeout = 30 * 14;
}

juce::String KanadeEditor::scaleName() const
{
    if (proc.scale == chromatic) return jp ("スケール:なし ▾");
    return jp ("スケール:") + NOTE_NAMES[proc.key] + (proc.scale == major ? " Major" : " Minor") + juce::String::fromUTF8 (" ▾");
}

void KanadeEditor::showScaleMenu()
{
    juce::PopupMenu m;
    m.addItem (1, jp ("なし(半音ごと)"), true, proc.scale == chromatic);
    m.addItem (2, jp ("自動で推定"), proc.session != nullptr);
    m.addSectionHeader ("Major");
    for (int k = 0; k < 12; ++k) m.addItem (100 + k, NOTE_NAMES[k], true, proc.scale == major && proc.key == k);
    m.addColumnBreak();
    m.addSectionHeader ("Minor");
    for (int k = 0; k < 12; ++k) m.addItem (200 + k, NOTE_NAMES[k], true, proc.scale == minor && proc.key == k);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&content.scaleBtn), [this] (int r) {
        if (r == 0) return;
        if (r == 1) proc.scale = chromatic;
        else if (r == 2) { const auto k = detectKey (proc.notes); proc.key = k.first; proc.scale = k.second; setStatus (jp ("キーは ") + juce::String (NOTE_NAMES[k.first]) + (k.second == major ? " Major" : " Minor") + jp (" と推定しました。違っていたら選び直してください。")); }
        else if (r >= 200) { proc.scale = minor; proc.key = r - 200; }
        else if (r >= 100) { proc.scale = major; proc.key = r - 100; }
        content.refresh(); content.roll.repaint(); });
}

void KanadeEditor::setTool (bool split)
{
    content.roll.splitTool = split;
    content.selBtn.setToggleState (! split, juce::dontSendNotification);
    content.splitBtn.setToggleState (split, juce::dontSendNotification);
    setStatus (split ? jp ("分割モード(C で選択に戻る / Shiftを押している間は選択)") : jp ("選択モード(C で分割 / Shiftを押しながらクリックで分割)"));
}

void KanadeEditor::edited (const std::vector<int>& ids, bool all)
{
    content.roll.refreshCurves();
    content.roll.repaint();
    proc.notesChanged (ids, all);
    content.refresh();
}

std::vector<Note*> KanadeEditor::targets()
{
    std::vector<Note*> t;
    for (auto& n : proc.notes) if (content.roll.sel.count (n.id)) t.push_back (&n);
    if (t.empty()) for (auto& n : proc.notes) t.push_back (&n);
    return t;
}

void KanadeEditor::applyKnob (int which, bool undoFirst)
{
    if (! proc.session) return;
    if (undoFirst && knobUndoPending) { proc.pushUndo(); knobUndoPending = which != 3 ? false : true; }
    std::vector<int> ids;
    const int sc = proc.snapOn ? proc.scale : chromatic;
    for (auto* n : targets())
    {
        if (which == 0 || which == 3) { const float t = snapToScale (n->center, proc.key, sc); n->pitch = n->center + (t - n->center) * proc.kCenter; }
        if (which == 1 || which == 3) n->drift = 1 - 2 * proc.kVariation;
        if (which == 2 || which == 3) n->inMs = n->outMs = (float) juce::roundToInt ((1 - proc.kTrans) * 300);
        ids.push_back (n->id);
    }
    edited (ids);
}

bool KanadeEditor::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    const int code = k.getKeyCode();
    if (mods.isCommandDown() && (code == 'Z' || code == 'z')) { (mods.isShiftDown() ? content.redoBtn : content.undoBtn).triggerClick(); return true; }
    if (mods.isCommandDown() && (code == 'A' || code == 'a')) { content.roll.sel.clear(); for (auto& n : proc.notes) content.roll.sel.insert (n.id); content.roll.repaint(); content.refresh(); return true; }
    if (! mods.isAnyModifierKeyDown() && (code == 'C' || code == 'c')) { setTool (! content.roll.splitTool); return true; }
    if (code == juce::KeyPress::escapeKey) { if (content.roll.adv >= 0) content.roll.closeAdv(); else { content.roll.sel.clear(); content.roll.repaint(); } return true; }
    if (code == juce::KeyPress::spaceKey) { if (proc.previewOn.load()) proc.stopPreview(); else content.playBtn.triggerClick(); return true; }
    if ((code == juce::KeyPress::upKey || code == juce::KeyPress::downKey) && ! content.roll.sel.empty())
    {
        proc.pushUndo();
        const float d = (code == juce::KeyPress::upKey ? 1.0f : -1.0f) * (mods.isAltDown() ? 0.1f : 1.0f);
        std::vector<int> ids;
        for (auto& n : proc.notes) if (content.roll.sel.count (n.id)) { n.pitch = mods.isAltDown() ? std::round ((n.pitch + d) * 100) / 100 : snapToScale (n.pitch + d, proc.key, proc.snapOn ? proc.scale : chromatic); ids.push_back (n.id); }
        edited (ids);
        return true;
    }
    return false;
}

void KanadeEditor::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounter();
    const double dt = (now - lastTick) / 1000.0; lastTick = now;
    if (content.logo.dancing)
    {
        if (proc.hostPlaying.load()) danceBeats = proc.hostSec.load() * proc.hostBpm.load() / 60.0;
        else danceBeats += dt * proc.hostBpm.load() / 60.0;
        content.logo.phaseBeats = danceBeats; content.logo.repaint();
    }
    if (statusTimeout > 0 && --statusTimeout == 0) content.status.setText ({}, juce::dontSendNotification);
    const int gen = proc.generation.load();
    if (gen != lastGen)
    {
        lastGen = gen;
        content.roll.refreshCurves();
        if (proc.session.get() != lastSession)
        {
            lastSession = proc.session.get();
            content.roll.sel.clear(); content.roll.closeAdv();
            if (proc.session) { content.roll.fitView(); content.roll.cursorSec = proc.session->startSec();
                const auto k = detectKey (proc.notes);
                setStatus (juce::String ((int) proc.notes.size()) + jp ("個の音符を見つけました。推定キーは ") + NOTE_NAMES[k.first] + (k.second == major ? " Major" : " Minor")
                           + jp ("。右上の「スケール」でキーを選び、「この設定で補正する」かノブ、または音符をドラッグで直してください。")); }
        }
        content.roll.repaint();
    }
    content.refresh();
    content.roll.tick();
}
} // namespace kanade::ui
