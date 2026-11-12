#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>

namespace shz::ui
{
using juce::String;
static String U (const char* s) { return String::fromUTF8 (s); }
static constexpr float A0 = -0.75f * juce::MathConstants<float>::pi, A1 = 0.75f * juce::MathConstants<float>::pi;

static String fmtPct (double v) { return String ((int) std::lround (v)) + "%"; }
static String fmtHz (double v) { return v >= 1000 ? String (v / 1000, v >= 10000 ? 1 : 2) + "k" : (v < 10 ? String (v, 2) : String (juce::roundToInt (v))); }
static String fmtSec (double v) { return v < 1 ? (v < 0.01 ? String (v * 1000, 1) : String (juce::roundToInt (v * 1000))) + "ms" : String (v, 2) + "s"; }
static String fmtDb (double v) { return v <= 0.0001 ? String (U ("-∞")) : String (20 * std::log10 (v), 1) + "dB"; }
static String fmtSigned (double v) { return (v > 0 ? "+" : "") + String ((int) std::lround (v)); }

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
    setColour (juce::TextButton::buttonOnColourId, col::aqua);
    setColour (juce::TextButton::textColourOffId, col::ink);
    setColour (juce::TextButton::textColourOnId, juce::Colour (0xff032230));
    setColour (juce::ComboBox::backgroundColourId, col::pane);
    setColour (juce::ComboBox::textColourId, col::ink);
    setColour (juce::ComboBox::arrowColourId, col::ink);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xffc9d6e0));
    setColour (juce::PopupMenu::textColourId, juce::Colour (0xff10202c));
    setColour (juce::PopupMenu::headerTextColourId, juce::Colour (0xff10202c));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff8fb4c8));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff06121b));
    setColour (juce::Label::textColourId, col::ink);
    setColour (juce::TextEditor::backgroundColourId, col::pane);
    setColour (juce::TextEditor::textColourId, col::ink);
    setColour (juce::TextEditor::outlineColourId, col::line2);
    setColour (juce::TextEditor::focusedOutlineColourId, col::aqua);
    setColour (juce::ScrollBar::thumbColourId, col::line2);
    setColour (juce::TooltipWindow::backgroundColourId, col::pane);
    setColour (juce::TooltipWindow::textColourId, col::ink);
    setColour (juce::TooltipWindow::outlineColourId, col::aqua);
    setColour (juce::Slider::trackColourId, col::mod);
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
    g.setFont (fonts().body (juce::jmin (13.0f, b.getHeight() * 0.52f), true));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 0), juce::Justification::centred, 1, 0.75f);
}
void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.8f);
    g.setColour (col::pane); g.fillRoundedRectangle (r, 6.0f);
    g.setColour (col::line2); g.drawRoundedRectangle (r, 6.0f, 1.1f);
    juce::Path p; const float cx = (float) w - 11.0f, cy = (float) h * 0.5f;
    p.startNewSubPath (cx - 4, cy - 2); p.lineTo (cx, cy + 2.5f); p.lineTo (cx + 4, cy - 2);
    g.setColour (col::ink); g.strokePath (p, juce::PathStrokeType (1.5f));
}
juce::Font Look::getComboBoxFont (juce::ComboBox& b) { return fonts().body (juce::jmin (13.0f, b.getHeight() * 0.56f)); }
void Look::positionComboBoxText (juce::ComboBox& b, juce::Label& l) { l.setBounds (6, 1, b.getWidth() - 22, b.getHeight() - 2); l.setFont (getComboBoxFont (b)); }
juce::Font Look::getPopupMenuFont() { return fonts().body (14.0f); }
void Look::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style == juce::Slider::LinearVertical)
    {
        const float cx = x + w * 0.5f;
        g.setColour (col::pane2); g.fillRoundedRectangle (cx - 4, (float) y, 8, (float) h, 4);
        const bool bip = s.getMinimum() < 0;
        const float zero = bip ? y + h * 0.5f : (float) (y + h);
        g.setColour (s.findColour (juce::Slider::trackColourId));
        g.fillRoundedRectangle (cx - 4, juce::jmin (zero, pos), 8, std::abs (zero - pos), 4);
        g.setColour (col::ink); g.fillRoundedRectangle (cx - 9, pos - 4, 18, 8, 3);
        return;
    }
    const float cy = y + h * 0.5f;
    g.setColour (col::pane2); g.fillRoundedRectangle ((float) x, cy - 3, (float) w, 6, 3);
    const bool bip = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float zero = bip ? (float) x + (float) w * (float) ((0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) : (float) x;
    g.setColour (s.findColour (juce::Slider::trackColourId));
    g.fillRoundedRectangle (juce::jmin (zero, pos), cy - 3, std::abs (pos - zero), 6, 3);
    g.setColour (col::ink); g.fillEllipse (pos - 6, cy - 6, 12, 12);
}

//==============================================================================
Knob::Knob (ShizukuEditor& e, Spec s) : spec (std::move (s)), ed (e)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setTitle (spec.label);
    if (spec.dest.valid()) setTooltip (U ("ドラッグで調整 · ダブルクリックで初期値 · ◯をドロップでモジュレーション"));
    else setTooltip (U ("ドラッグで調整 · ダブルクリックで初期値"));
}
double Knob::toN (double v) const
{
    v = juce::jlimit (spec.min, spec.max, v);
    if (spec.curve == 1) return std::log (v / spec.min) / std::log (spec.max / spec.min);
    if (spec.curve == 2) return std::pow ((v - spec.min) / (spec.max - spec.min), 1.0 / spec.pw);
    return (v - spec.min) / (spec.max - spec.min);
}
double Knob::fromN (double n) const
{
    n = juce::jlimit (0.0, 1.0, n);
    double v = spec.curve == 1 ? spec.min * std::pow (spec.max / spec.min, n) : spec.curve == 2 ? spec.min + (spec.max - spec.min) * std::pow (n, spec.pw) : spec.min + (spec.max - spec.min) * n;
    if (spec.step > 0) v = std::round (v / spec.step) * spec.step;
    return v;
}
void Knob::setN (double n)
{
    spec.set (fromN (n));
    repaint();
    if (onChange) onChange(); else ed.edited (spec.structural);
}
void Knob::mouseDown (const juce::MouseEvent& e) { startN = toN (spec.get()); startY = (float) e.position.y; }
void Knob::mouseDrag (const juce::MouseEvent& e) { setN (startN + (startY - e.position.y) / (e.mods.isShiftDown() ? 800.0 : 170.0)); }
void Knob::mouseDoubleClick (const juce::MouseEvent&) { setN (toN (spec.def)); }
void Knob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    juce::ignoreUnused (e);
    setN (toN (spec.get()) + (w.deltaY > 0 ? 1 : -1) * (e.mods.isShiftDown() ? 0.004 : 0.02));
}
bool Knob::isInterestedInDragSource (const SourceDetails& d) { return spec.dest.valid() && d.description.toString().startsWith ("src:"); }
void Knob::itemDropped (const SourceDetails& d)
{
    dropHi = false;
    const int src = d.description.toString().fromFirstOccurrenceOf ("src:", false, false).getIntValue();
    Mod m; m.src = src; m.mod = spec.dest.mod; m.id = spec.dest.id; m.p = spec.dest.p; m.amt = 35;
    ed.P().mod.push_back (m);
    ed.edited (true);
    ed.setStatus (srcName (src) + U (" → ") + spec.label + U ("(量は MATRIX タブで調整)"));
}
void Knob::paint (juce::Graphics& g)
{
    const float w = (float) getWidth();
    const float cx = w * 0.5f, cy = 20, r = 15;
    const double v = spec.get(), n = toN (v);
    const float a = A0 + (float) n * (A1 - A0);
    const bool bip = spec.min < 0 && spec.max > 0;
    const float a0 = bip ? A0 + (float) toN (0) * (A1 - A0) : A0;
    if (dropHi) { g.setColour (col::mod.withAlpha (0.18f)); g.fillRoundedRectangle (getLocalBounds().toFloat(), 8); }
    juce::Path bgA; bgA.addCentredArc (cx, cy, r, r, 0, A0, A1, true);
    g.setColour (col::ink.withAlpha (0.13f)); g.strokePath (bgA, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // modulation ring
    double md = 0;
    if (spec.dest.valid())
        for (auto& m : ed.P().mod)
            if (m.mod == spec.dest.mod && m.id == spec.dest.id && m.p == spec.dest.p)
            {
                const double span = destSpan (m.mod, m.p);
                md += m.amt / 100.0 * (span != 0 ? span : 1) * spec.modScale / (spec.max - spec.min);
            }
    if (std::abs (md) > 1e-4)
    {
        const float a2 = A0 + (float) juce::jlimit (0.0, 1.0, n + md) * (A1 - A0);
        juce::Path mA; mA.addCentredArc (cx, cy, r + 3.5f, r + 3.5f, 0, juce::jmin (a, a2), juce::jmax (a, a2), true);
        g.setColour (col::mod); g.strokePath (mA, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    if (std::abs (a - a0) > 0.01f)
    {
        juce::Path vA; vA.addCentredArc (cx, cy, r, r, 0, juce::jmin (a, a0), juce::jmax (a, a0), true);
        g.setColour (col::aqua); g.strokePath (vA, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    const float cr = r * 0.68f;
    g.setColour (juce::Colour (0xff1d3347)); g.fillEllipse (cx - cr, cy - cr, cr * 2, cr * 2);
    g.setColour (col::ink.withAlpha (0.22f)); g.drawEllipse (cx - cr, cy - cr, cr * 2, cr * 2, 1.0f);
    g.setColour (col::ink); g.drawLine (cx, cy, cx + std::sin (a) * r * 0.62f, cy - std::cos (a) * r * 0.62f, 2.2f);
    g.setColour (std::abs (md) > 1e-4 ? col::mod : col::muted); g.setFont (fonts().body (10.0f, true));
    g.drawText (spec.label.toUpperCase(), 0, 38, getWidth(), 12, juce::Justification::centred);
    g.setColour (col::aqua2); g.setFont (fonts().mono (10.0f));
    g.drawText (spec.fmt ? spec.fmt (v) : String (v, 2), 0, 50, getWidth(), 12, juce::Justification::centred);
}

void Toggle::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().withSizeKeepingCentre (30, 16);
    const bool on = get();
    g.setColour (on ? col::aqua.withAlpha (0.25f) : col::pane2); g.fillRoundedRectangle (r, 8);
    g.setColour (on ? col::aqua : col::line2); g.drawRoundedRectangle (r, 8, 1);
    g.setColour (on ? col::aqua : col::muted); g.fillEllipse (on ? r.getRight() - 14 : r.getX() + 2, r.getY() + 3, 10, 10);
}

Logo::Logo()
{
    img = juce::ImageCache::getFromMemory (BinaryData::astronaut_png, BinaryData::astronaut_pngSize);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle (U ("DJ KATSUNIIの宇宙飛行士(クリックでダンス)"));
}
void Logo::paint (juce::Graphics& g)
{
    if (! img.isValid()) return;
    auto r = getLocalBounds().toFloat().reduced (2);
    const float bh = r.getHeight() - 6, bw = bh * 0.64f, sx = r.getCentreX(), sy = r.getBottom();
    juce::AffineTransform t;
    if (dancing)
    {
        const double ph = std::fmod (phaseBeats, 2.0) / 2.0;
        const float rot = (float) (std::sin (ph * juce::MathConstants<double>::twoPi) * 9.0) * juce::MathConstants<float>::pi / 180.0f;
        const float hop = (float) -std::abs (std::sin (ph * juce::MathConstants<double>::twoPi * 2.0)) * 8.0f;
        const float sq = 1.0f + 0.05f * (float) std::cos (ph * juce::MathConstants<double>::twoPi * 2.0);
        t = juce::AffineTransform::scale (2 - sq, sq, sx, sy).rotated (rot, sx, sy).translated (0, hop);
    }
    g.addTransform (t);
    g.drawImage (img, juce::Rectangle<float> (sx - bw / 2, sy - bh, bw, bh), juce::RectanglePlacement::centred);
}

void Panel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (col::pane); g.fillRoundedRectangle (r, 10);
    g.setColour (col::line); g.drawRoundedRectangle (r, 10, 1);
    if (title.isNotEmpty())
    {
        g.setColour (dim ? col::muted : col::aqua2); g.setFont (fonts().display (13.0f));
        g.drawText (title, 44, 5, 200, 18, juce::Justification::centredLeft);
    }
    if (note.isNotEmpty()) { g.setColour (noteCol); g.setFont (fonts().body (11.5f)); g.drawText (note, 10, 104, getWidth() - 20, 15, juce::Justification::centredLeft, true); }
}

//==============================================================================
void WTView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff07121c)); g.fillRoundedRectangle (r, 8);
    g.setColour (col::line); g.drawRoundedRectangle (r.reduced (0.5f), 8, 1);
    const auto& o = ed.P().osc[(size_t) idx];
    if (o.sample)
    {
        g.setColour (col::muted); g.setFont (fonts().body (13));
        g.drawText (o.sampleData ? U ("サンプル読込済み") : U ("サンプル未読込 → WAVボタンで読込"), r, juce::Justification::centred);
        return;
    }
    const auto T = ed.proc.shownTables[(size_t) idx];
    if (! T || T->frames.empty()) return;
    const int n = (int) T->frames.size(), show = juce::jmin (n, 24);
    const float W = r.getWidth(), H = r.getHeight();
    const float dx = W * 0.18f / (float) show, dy = H * 0.32f / (float) show, w = W * 0.74f, h = H * 0.26f;
    for (int k = show - 1; k >= 0; --k)
    {
        const int fi = (int) std::lround ((double) k / juce::jmax (1, show - 1) * (n - 1));
        const auto& f = T->frames[(size_t) fi];
        const float ox = W * 0.06f + k * dx, oy = H * 0.66f - k * dy;
        juce::Path p;
        for (int x = 0; x <= 96; ++x) { const float v = f[(size_t) (x * 2047 / 96)]; const float px = ox + x / 96.0f * w, py = oy - v * h; if (x) p.lineTo (px, py); else p.startNewSubPath (px, py); }
        g.setColour (col::aqua.withAlpha (0.07f + 0.14f * (1 - (float) k / show))); g.strokePath (p, juce::PathStrokeType (1.0f));
    }
    const double cur = juce::jlimit (0.0, 1.0, o.pos / 256.0) * (n - 1);
    const auto& f0 = T->frames[(size_t) std::floor (cur)]; const auto& f1 = T->frames[(size_t) juce::jmin (n - 1, (int) std::floor (cur) + 1)]; const float t = (float) (cur - std::floor (cur));
    const float k = (float) (cur / juce::jmax (1, n - 1) * (show - 1));
    const float ox = W * 0.06f + k * dx, oy = H * 0.66f - k * dy;
    juce::Path p;
    for (int x = 0; x <= 200; ++x) { const int j = x * 2047 / 200; const float v = f0[(size_t) j] * (1 - t) + f1[(size_t) j] * t; const float px = ox + x / 200.0f * w, py = oy - v * h; if (x) p.lineTo (px, py); else p.startNewSubPath (px, py); }
    g.setColour (col::aqua.withAlpha (0.35f)); g.strokePath (p, juce::PathStrokeType (5.0f));
    g.setColour (col::aqua2); g.strokePath (p, juce::PathStrokeType (2.0f));
    g.setColour (col::muted); g.setFont (fonts().mono (10.5f));
    g.drawText (String ((int) std::floor (cur) + 1) + "/" + String (n), r.reduced (8, 4), juce::Justification::topRight);
}

void FilterView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff07121c)); g.fillRoundedRectangle (r, 8);
    const auto& F = ed.P().filt[(size_t) idx];
    const auto mode = classify (F.type); const bool two = slopeOf (F.type) == 2;
    const double fc = fnorm (F.freq), reso = F.reso / 100.0, k = mode == FMode::peak ? 2 - 1.9 * reso : 2 - 1.97 * std::pow (reso, 0.8);
    const float W = r.getWidth(), H = r.getHeight();
    g.setColour (col::ink.withAlpha (0.06f));
    for (double f : { 100.0, 1000.0, 10000.0 }) { const float x = (float) (std::log (f / 20) / std::log (1000.0)) * W; g.drawVerticalLine ((int) x, 0, H); }
    juce::Path p;
    for (int x = 0; x <= (int) W; x += 2)
    {
        const double f = 20 * std::pow (1000.0, x / W), w = f / fc, re = 1 - w * w, im = k * w, den = re * re + im * im;
        double mag;
        switch (mode)
        {
            case FMode::lp: mag = 1 / std::sqrt (den); break;
            case FMode::hp: mag = w * w / std::sqrt (den); break;
            case FMode::bp: mag = k * w / std::sqrt (den); break;
            case FMode::peak: mag = (1 + w * w) / std::sqrt (den); break;
            case FMode::notch: case FMode::comb: mag = std::abs (re) / std::sqrt (den); break;
            default: mag = 1;
        }
        if (two && mode != FMode::peak) mag *= mag;
        const float y = juce::jlimit (2.0f, H, (float) (H * 0.45f - 20 * std::log10 (std::max (mag, 1e-5)) * H / 60));
        if (x) p.lineTo ((float) x, y); else p.startNewSubPath ((float) x, y);
    }
    g.setColour (F.on ? col::aqua : col::muted.withAlpha (0.6f)); g.strokePath (p, juce::PathStrokeType (2.0f));
    p.lineTo (W, H); p.lineTo (0, H); p.closeSubPath();
    g.setColour (col::aqua.withAlpha (0.08f)); g.fillPath (p);
}

void EnvView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff07121c)); g.fillRoundedRectangle (r, 8);
    const auto& E = ed.P().env[(size_t) idx];
    const double a = tnorm (E.a) + 0.02, h = E.h > 0 ? tnorm (E.h) : 0, d = tnorm (E.d) + 0.02, sus = 0.22, rr = tnorm (E.r) + 0.02, tot = a + h + d + sus + rr;
    const float W = r.getWidth(), top = 28, bot = r.getHeight() - 8, hh = bot - top;
    const double sx = W / tot;
    juce::Path p; bool first = true;
    auto pt = [&] (double x, double y) { const float X = (float) (x * sx), Y = bot - (float) y * hh; if (first) { p.startNewSubPath (X, Y); first = false; } else p.lineTo (X, Y); };
    double x = 0;
    for (int k = 0; k <= 40; ++k) { const double t = k / 40.0; pt (x + a * t, curveY (t, E.c1)); } x += a;
    if (h > 0) { pt (x + h, 1); x += h; }
    for (int k = 0; k <= 40; ++k) { const double t = k / 40.0; pt (x + d * t, 1 + (E.s - 1) * curveY (t, E.c2)); } x += d;
    pt (x + sus, E.s); x += sus;
    for (int k = 0; k <= 40; ++k) { const double t = k / 40.0; pt (x + rr * t, E.s * (1 - curveY (t, E.c3))); }
    g.setColour (col::mod); g.strokePath (p, juce::PathStrokeType (2.2f));
    p.lineTo (W, bot); p.lineTo (0, bot); p.closeSubPath(); g.setColour (col::mod.withAlpha (0.1f)); g.fillPath (p);
    g.setColour (col::muted); g.setFont (fonts().mono (11.5f));
    g.drawText ("A " + fmtSec (E.a) + "   D " + fmtSec (E.d) + "   S " + fmtDb (E.s) + "   R " + fmtSec (E.r), 12, 6, (int) W - 24, 16, juce::Justification::centredLeft);
    const float lv = ed.proc.eng().visEnv[idx].load();
    if (lv > 0.001f) { g.setColour (col::aqua2); g.fillEllipse (W - 22, bot - lv * hh - 5, 10, 10); }
}

juce::Point<float> LfoView::toPx (float x, float y) const { const float H = (float) getHeight(); return { x * getWidth(), H * 0.08f + (1 - y) * H * 0.84f }; }
juce::Point<float> LfoView::toLfo (juce::Point<float> p) const { const float H = (float) getHeight(); return { juce::jlimit (0.0f, 1.0f, p.x / getWidth()), juce::jlimit (0.0f, 1.0f, 1 - (p.y - H * 0.08f) / (H * 0.84f)) }; }
int LfoView::hit (juce::Point<float> px, bool& isCurve) const
{
    const auto& pts = ed.P().lfo[(size_t) idx].pts;
    int best = -1; float bd = 1e9f; isCurve = false;
    for (int k = 0; k < (int) pts.size(); ++k) { const float d = px.getDistanceFrom (toPx (pts[(size_t) k].x, pts[(size_t) k].y)); if (d < bd) { bd = d; best = k; isCurve = false; } }
    for (int k = 0; k + 1 < (int) pts.size(); ++k)
    {
        const auto& a = pts[(size_t) k]; const auto& b = pts[(size_t) k + 1]; if (b.x - a.x < 0.01f) continue;
        const float mx = (a.x + b.x) / 2, my = (float) lfoCurveVal (pts, mx);
        const float d = px.getDistanceFrom (toPx (mx, my));
        if (d < bd - 2) { bd = d; best = k; isCurve = true; }
    }
    return bd < 12 ? best : -1;
}
void LfoView::mouseDown (const juce::MouseEvent& e)
{
    if (ed.P().lfo[(size_t) idx].type != "curve") return;
    drag = hit (e.position, dragCurve);
    if (drag >= 0) { y0 = toLfo (e.position).y; c0 = ed.P().lfo[(size_t) idx].pts[(size_t) drag].c; }
}
void LfoView::mouseDrag (const juce::MouseEvent& e)
{
    if (drag < 0) return;
    auto& pts = ed.P().lfo[(size_t) idx].pts; const auto q = toLfo (e.position); const int last = (int) pts.size() - 1;
    if (dragCurve) { const float dir = pts[(size_t) drag + 1].y > pts[(size_t) drag].y ? 1.0f : -1.0f; pts[(size_t) drag].c = juce::jlimit (0.02f, 0.98f, c0 + (q.y - y0) * dir * 1.2f); }
    else
    {
        if (drag != 0 && drag != last) pts[(size_t) drag].x = juce::jlimit (pts[(size_t) drag - 1].x, pts[(size_t) drag + 1].x, q.x);
        pts[(size_t) drag].y = q.y;
        if (drag == 0) pts[(size_t) last].y = q.y;
        if (drag == last) pts[0].y = q.y;
    }
    repaint(); ed.edited();
}
void LfoView::mouseDoubleClick (const juce::MouseEvent& e)
{
    auto& L = ed.P().lfo[(size_t) idx];
    if (L.type != "curve") return;
    bool c; const int h = hit (e.position, c); auto& pts = L.pts;
    if (h > 0 && ! c && h < (int) pts.size() - 1) pts.erase (pts.begin() + h);
    else { const auto q = toLfo (e.position); size_t k = 0; while (k + 1 < pts.size() && pts[k + 1].x < q.x) ++k; pts.insert (pts.begin() + (long) k + 1, { q.x, q.y, 0.5f }); }
    repaint(); ed.edited();
}
void LfoView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff07121c)); g.fillRoundedRectangle (r, 8);
    const auto& L = ed.P().lfo[(size_t) idx];
    g.setColour (col::ink.withAlpha (0.05f));
    for (int k = 1; k < 8; ++k) g.drawVerticalLine ((int) (k / 8.0f * r.getWidth()), 0, r.getHeight());
    if (L.type != "curve")
    {
        g.setColour (col::muted); g.setFont (fonts().body (14));
        g.drawText (L.type == "RandomSH" ? U ("ランダム(サンプル&ホールド)") : U ("カオス・アトラクタ(") + L.type + ")", r, juce::Justification::centred);
        return;
    }
    juce::Path p;
    for (int x = 0; x <= getWidth(); x += 2) { const auto q = toPx ((float) x / getWidth(), (float) lfoCurveVal (L.pts, (double) x / getWidth())); if (x) p.lineTo (q); else p.startNewSubPath (q); }
    g.setColour (col::mod); g.strokePath (p, juce::PathStrokeType (2.3f));
    p.lineTo (toPx (1, 0)); p.lineTo (toPx (0, 0)); p.closeSubPath(); g.setColour (col::mod.withAlpha (0.09f)); g.fillPath (p);
    for (size_t k = 0; k < L.pts.size(); ++k)
    {
        const auto q = toPx (L.pts[k].x, L.pts[k].y); const bool lastPt = k + 1 == L.pts.size();
        g.setColour (lastPt ? col::muted : juce::Colour (0xffffd9cf)); g.fillEllipse (q.x - (lastPt ? 4 : 6), q.y - (lastPt ? 4 : 6), lastPt ? 8.0f : 12.0f, lastPt ? 8.0f : 12.0f);
    }
    for (size_t k = 0; k + 1 < L.pts.size(); ++k)
    {
        const auto& a = L.pts[k]; const auto& b = L.pts[k + 1]; if (b.x - a.x < 0.01f) continue;
        const float mx = (a.x + b.x) / 2; const auto q = toPx (mx, (float) lfoCurveVal (L.pts, mx));
        g.setColour (col::mod); g.drawEllipse (q.x - 4, q.y - 4, 8, 8, 1.4f);
    }
    if (L.mode != "Free")
    {
        const float ph = ed.proc.eng().visLfoPh[idx].load();
        if (ph > 0 && ph < 1) { g.setColour (col::aqua2.withAlpha (0.7f)); g.drawVerticalLine ((int) (ph * getWidth()), 0, (float) getHeight()); }
    }
}

void SourceChip::paint (juce::Graphics& g)
{
    const bool sel = ed.curSource == key;
    auto r = getLocalBounds().toFloat().reduced (1);
    g.setColour (sel ? col::mod.withAlpha (0.2f) : col::pane2); g.fillRoundedRectangle (r, r.getHeight() / 2);
    g.setColour (sel ? col::mod : col::line2); g.drawRoundedRectangle (r, r.getHeight() / 2, 1);
    g.setColour (col::mod); g.drawEllipse (r.getX() + 7, r.getCentreY() - 5, 10, 10, 2);
    g.setColour (sel ? juce::Colour (0xffffd9cf) : col::ink); g.setFont (fonts().body (11.5f, true));
    g.drawText (label, r.withTrimmedLeft (22), juce::Justification::centredLeft);
}
void SourceChip::mouseDown (const juce::MouseEvent&) { dragging = false; }
void SourceChip::mouseUp (const juce::MouseEvent& e) { if (! dragging && getLocalBounds().contains (e.getPosition())) ed.selectSource (key); }
void SourceChip::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging && e.getDistanceFromDragStart() > 5)
    {
        dragging = true;
        ed.startDragging ("src:" + String (src), this);
    }
}

//==============================================================================
Content::Content (ShizukuEditor& e) : ed (e) {}
void Content::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0f2c44), (float) W / 2, -100, col::bg, (float) W / 2, (float) H * 0.8f, true));
    g.fillAll();
    g.setColour (col::aqua2); g.setFont (fonts().display (28.0f));
    g.drawText ("SHIZUKU", 70, 12, 220, 32, juce::Justification::centredLeft);
    g.setColour (col::aqua); g.setFont (fonts().body (15.0f, true));
    g.drawText (U ("しずく"), 214, 18, 70, 24, juce::Justification::centredLeft);
    g.setColour (col::muted); g.setFont (fonts().body (11.0f));
    g.drawText (U ("WAVETABLE SYNTH  \xc2\xb7  by DJ KATSUNII"), 72, 42, 300, 14, juce::Justification::centredLeft);
    // preset bar background
    g.setColour (col::panel); g.fillRoundedRectangle (16, 72, W - 32, 42, 12);
    g.setColour (col::line2); g.drawRoundedRectangle (16.5f, 72.5f, W - 33, 41, 12, 1);
    // rack background
    g.setColour (col::panel); g.fillRoundedRectangle (16, 147, W - 32, 480, 12);
    g.setColour (col::line2); g.drawRoundedRectangle (16.5f, 147.5f, W - 33, 479, 12, 1);
    // meter
    const float m = juce::jlimit (0.0f, 1.0f, std::sqrt (ed.proc.meter.load()));
    g.setColour (col::pane); g.fillRoundedRectangle ((float) W - 30, 14, 10, 44, 4);
    g.setGradientFill (juce::ColourGradient (col::ok, 0, 58, col::mod, 0, 14, false));
    g.fillRoundedRectangle ((float) W - 30, 58 - 44 * m, 10, 44 * m, 4);
    g.setColour (col::muted); g.setFont (fonts().mono (11.0f));
    g.drawText ("BPM " + String (ed.proc.hostBpm.load(), 1), W - 200, 22, 100, 16, juce::Justification::centredRight);
}
void Content::resized() {}

//==============================================================================
ShizukuEditor::ShizukuEditor (ShizukuProcessor& p)
    : AudioProcessorEditor (&p), proc (p), content (*this), kbd (p.keyboard, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.addAndMakeVisible (logo);
    // output knob → host parameter
    {
        auto* prm = proc.apvts.getParameter ("out");
        Knob::Spec s; s.label = "Output"; s.min = -24; s.max = 12; s.def = 0; s.fmt = [] (double v) { return String (v, 1) + "dB"; };
        s.get = [this] { return (double) proc.apvts.getRawParameterValue ("out")->load(); };
        s.set = [prm] (double v) { prm->setValueNotifyingHost (prm->convertTo0to1 ((float) v)); };
        outKnob = std::make_unique<Knob> (*this, s); outKnob->onChange = [] {};
        content.addAndMakeVisible (*outKnob);
    }
    buildPresetBar();
    const char* tn[] = { "OSC", "FX", "MATRIX", "GLOBAL / ARP" };
    for (int i = 0; i < 4; ++i)
    {
        tabs[(size_t) i].setButtonText (tn[i]); tabs[(size_t) i].setClickingTogglesState (false);
        tabs[(size_t) i].onClick = [this, i] { showTab (i); };
        content.addAndMakeVisible (tabs[(size_t) i]);
    }
    for (auto* c : { &oscPage, &fxPage, &matrixPage, &globalPage }) content.addChildComponent (*c);
    fxPage.addAndMakeVisible (fxView); fxView.setViewedComponent (&fxInner, false); fxView.setScrollBarsShown (true, false);
    matrixPage.addAndMakeVisible (mxView); mxView.setViewedComponent (&mxInner, false); mxView.setScrollBarsShown (true, false);
    content.addAndMakeVisible (modArea);
    kbd.setLowestVisibleKey (36); kbd.setKeyWidth (24); kbd.setOctaveForMiddleC (4);
    kbd.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, col::aqua);
    kbd.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, col::aqua.withAlpha (0.3f));
    content.addAndMakeVisible (kbd);
    for (auto* s : { &mw, &pb })
    {
        s->setSliderStyle (juce::Slider::LinearVertical); s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        content.addAndMakeVisible (*s);
    }
    mw.setTooltip ("MOD WHEEL"); pb.setTooltip ("PITCH BEND");
    mw.setRange (0, 1); mw.setColour (juce::Slider::trackColourId, col::mod); mw.setTitle ("Mod Wheel");
    mw.onValueChange = [this] { proc.uiModWheel.store (mw.getValue()); };
    pb.setRange (-1, 1); pb.setColour (juce::Slider::trackColourId, col::aqua); pb.setTitle ("Pitch Bend");
    pb.onValueChange = [this] { proc.uiBend.store (pb.getValue()); };
    pb.onDragEnd = [this] { pb.setValue (0); };
    notes.setFont (fonts().body (12.0f)); notes.setJustificationType (juce::Justification::topLeft); notes.setColour (juce::Label::textColourId, col::warn);
    content.addAndMakeVisible (notes);
    status.setFont (fonts().body (12.0f)); status.setColour (juce::Label::textColourId, col::muted); status.setJustificationType (juce::Justification::centredRight);
    content.addAndMakeVisible (status);

    layoutAll();
    rebuild();
    showTab (0);
    setResizable (true, true);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) Content::W / Content::H);
    setResizeLimits (792, 606, 1980, 1515);
    setSize (1056, 808);
    setWantsKeyboardFocus (true);
    lastTick = juce::Time::getMillisecondCounter();
    startTimerHz (30);
}
ShizukuEditor::~ShizukuEditor() { stopTimer(); setLookAndFeel (nullptr); }
void ShizukuEditor::paint (juce::Graphics& g) { g.fillAll (col::bg); }
void ShizukuEditor::resized()
{
    content.setBounds (0, 0, Content::W, Content::H);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) Content::W));
}

void ShizukuEditor::layoutAll()
{
    logo.setBounds (16, 6, 50, 60);
    outKnob->setBounds (Content::W - 96, 6, 56, 64);
    prevB.setBounds (26, 80, 30, 26); nextB.setBounds (60, 80, 30, 26);
    nameB.setBounds (98, 80, 420, 26);
    openB.setBounds (528, 80, 170, 26); tablesB.setBounds (704, 80, 130, 26); saveB.setBounds (840, 80, 70, 26);
    status.setBounds (916, 80, Content::W - 16 - 926, 26);
    for (int i = 0; i < 4; ++i) tabs[(size_t) i].setBounds (16 + i * 112, 121, i == 3 ? 128 : 106, 26);
    for (auto* c : { &oscPage, &fxPage, &matrixPage, &globalPage }) c->setBounds (16, 147, Content::W - 32, 480);
    modArea.setBounds (16, 634, Content::W - 32, 240);
    mw.setBounds (24, 884, 22, 78); pb.setBounds (54, 884, 22, 78);
    kbd.setBounds (86, 882, Content::W - 16 - 86, 82);
    notes.setBounds (20, 968, Content::W - 40, 38);
}

void ShizukuEditor::buildPresetBar()
{
    for (auto* b : { &prevB, &nextB, &nameB, &openB, &tablesB, &saveB }) content.addAndMakeVisible (*b);
    prevB.onClick = [this] { loadIndex (proc.browserIndex - 1); };
    nextB.onClick = [this] { loadIndex (proc.browserIndex + 1); };
    nameB.onClick = [this] { openPresetMenu(); };
    nameB.setTooltip (U ("クリックでプリセット一覧(Serum 2のプリセットフォルダも自動で表示)"));
    openB.setButtonText (U ("Serumプリセットを開く")); openB.setToggleState (true, juce::dontSendNotification);
    openB.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (U ("Serum 2 / SHIZUKU のプリセットを開く"), juce::File(), "*.SerumPreset;*.serumpreset;*.shizuku");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult(); if (! f.existsAsFile()) return;
            String err; if (proc.loadFile (f, err)) { rebuild(); setStatus (U ("読み込みました: ") + proc.patch.name); } else setStatus (err, true);
        });
    };
    tablesB.setButtonText (U ("Tablesフォルダ"));
    tablesB.setTooltip (U ("Serum 2 の波形(Tables)は自動で探します。見つからないときだけ場所を選んでください"));
    tablesB.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (U ("Serum 2 の Tables フォルダを選んでください"), TableLibrary::get().extraRoot());
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult(); if (! f.isDirectory()) return;
            TableLibrary::get().setExtraRoot (f);
            setStatus (U ("波形フォルダを読み込み中…"));
            needRebuild = true;
        });
    };
    saveB.setButtonText (U ("保存"));
    saveB.onClick = [this]
    {
        auto dir = ShizukuProcessor::userPresetFolder(); dir.createDirectory();
        chooser = std::make_unique<juce::FileChooser> (U ("SHIZUKUのプリセットを保存"), dir.getChildFile (juce::File::createLegalFileName (proc.patch.name) + ".shizuku"), "*.shizuku");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult(); if (f == juce::File()) return;
            f = f.withFileExtension ("shizuku");
            if (proc.savePatch (f)) { setStatus (U ("保存しました: ") + f.getFileName()); proc.rescanBrowser(); } else setStatus (U ("保存できませんでした"), true);
        });
    };
}

void ShizukuEditor::openPresetMenu()
{
    if (! proc.browserReady) { setStatus (U ("プリセットを探しています…")); return; }
    std::vector<BrowserEntry> list; { list = proc.browser; }
    juce::PopupMenu root;
    std::map<String, juce::PopupMenu> sub; juce::StringArray order;
    for (int i = 0; i < (int) list.size(); ++i)
    {
        const auto& e = list[(size_t) i];
        if (! sub.count (e.folder)) order.add (e.folder);
        sub[e.folder].addItem (i + 1, e.name, true, i == proc.browserIndex);
    }
    for (auto& f : order) root.addSubMenu (f, sub[f]);
    if (list.size() <= (size_t) proc.factory.size()) root.addItem (-1, U ("(Serum 2 のプリセットフォルダは見つかりませんでした)"), false);
    root.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameB), [this] (int r) { if (r > 0) loadIndex (r - 1); });
}
void ShizukuEditor::loadIndex (int i)
{
    String err;
    if (proc.loadBrowser (i, err)) { rebuild(); setStatus (U ("読み込みました: ") + proc.patch.name); }
    else if (err.isNotEmpty()) setStatus (err, true);
}

bool ShizukuEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (f.endsWithIgnoreCase (".serumpreset") || f.endsWithIgnoreCase (".shizuku")) return true;
    return false;
}
void ShizukuEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".serumpreset") || f.endsWithIgnoreCase (".shizuku"))
        {
            String err;
            if (proc.loadFile (juce::File (f), err)) { rebuild(); setStatus (U ("読み込みました: ") + proc.patch.name); }
            else setStatus (err, true);
            return;
        }
}

void ShizukuEditor::setStatus (const String& s, bool error)
{
    status.setText (s, juce::dontSendNotification);
    status.setColour (juce::Label::textColourId, error ? col::mod : col::muted);
    statusTimeout = 30 * 8;
}

void ShizukuEditor::showTab (int t)
{
    curTab = t;
    juce::Component* pages[] = { &oscPage, &fxPage, &matrixPage, &globalPage };
    for (int i = 0; i < 4; ++i) { pages[i]->setVisible (i == t); tabs[(size_t) i].setToggleState (i == t, juce::dontSendNotification); }
    if (t == 2) { juce::Component::SafePointer<ShizukuEditor> self (this); juce::MessageManager::callAsync ([self] { if (self && self->curTab == 2) self->buildMatrixPage(); }); }
}

void ShizukuEditor::edited (bool structural)
{
    if (structural) proc.structureEdited(); else proc.patchEdited();
    refreshModRings();
    for (auto* v : wtViews) if (v) v->repaint();
    for (auto* v : frViews) if (v) v->repaint();
    if (modView) modView->repaint();
    if (structural && curTab == 2)
    {
        juce::Component::SafePointer<ShizukuEditor> self (this);
        juce::MessageManager::callAsync ([self] { if (self && self->curTab == 2) self->buildMatrixPage(); });
    }
}
void ShizukuEditor::refreshModRings() { for (auto* k : allKnobs) k->repaint(); }

std::unique_ptr<Knob> ShizukuEditor::knob (Knob::Spec s)
{
    auto k = std::make_unique<Knob> (*this, std::move (s));
    allKnobs.push_back (k.get());
    return k;
}

void ShizukuEditor::rebuild()
{
    allKnobs.clear(); allKnobs.push_back (outKnob.get());
    wtViews = {}; frViews = {}; modView = nullptr;
    for (auto& o : owned) o.clear();
    oscPage.removeAllChildren(); fxInner.removeAllChildren(); mxInner.removeAllChildren(); globalPage.removeAllChildren(); modArea.removeAllChildren();
    chips.clear();
    buildOscPage(); buildFxPage(); buildMatrixPage(); buildGlobalPage(); buildModSection();
    nameB.setButtonText ((proc.patch.source == "serum" ? U ("SERUM 2  \xc2\xb7  ") : U ("SHIZUKU  \xc2\xb7  ")) + proc.patch.name + (proc.patch.author.isNotEmpty() ? "   by " + proc.patch.author : String()));
    updateNotes();
    lastTablesGen = proc.tablesGeneration.load();
    content.repaint();
}

void ShizukuEditor::updateNotes()
{
    juce::StringArray items (proc.patch.missing);
    for (int i = 0; i < 3; ++i)
    {
        const auto& o = proc.patch.osc[(size_t) i]; if (! o.on) continue;
        const String nm = "OSC " + String::charToString ((juce::juce_wchar) ('A' + i)) + ": ";
        switch (proc.tableStatus[(size_t) i])
        {
            case TableStatus::missing: items.add (nm + U ("波形「") + o.tablePath + U ("」が見つかりません(Tablesフォルダで場所を指定)")); break;
            case TableStatus::approx: items.add (nm + o.tableName + U (" は近似の波形で再生中")); break;
            case TableStatus::sampleMissing: items.add (nm + U ("サンプル「") + o.samplePath.fromLastOccurrenceOf ("/", false, false).fromLastOccurrenceOf ("\\", false, false) + U ("」が必要(OSCの WAV ボタンで読込)")); break;
            default: break;
        }
    }
    if (proc.patch.source == "serum") items.add (U ("Serumの音を近似で再現しています(完全に同じ音にはなりません)"));
    const auto& lib = TableLibrary::get();
    String libTxt = lib.ready() ? (lib.count() > 0 ? U ("Serumの波形: ") + String (lib.count()) + U (" 個見つかりました") : U ("Serumの波形フォルダは見つかりませんでした")) : U ("Serumの波形を探しています…");
    notes.setText ((items.isEmpty() ? U ("問題なし") : items.joinIntoString ("   /   ")) + "\n" + libTxt, juce::dontSendNotification);
    notes.setColour (juce::Label::textColourId, items.isEmpty() ? col::ok : col::warn);
}

//==============================================================================
// combo helper
static std::unique_ptr<juce::ComboBox> makeCombo (std::vector<std::pair<String, String>> items, String current, std::function<void (const String&)> onPick)
{
    auto c = std::make_unique<juce::ComboBox>();
    int sel = 0;
    for (int i = 0; i < (int) items.size(); ++i) { c->addItem (items[(size_t) i].second, i + 1); if (items[(size_t) i].first == current) sel = i + 1; }
    if (sel == 0 && current.isNotEmpty()) { items.push_back ({ current, current }); c->addItem (current, (int) items.size()); sel = (int) items.size(); }
    c->setSelectedId (sel, juce::dontSendNotification);
    auto* raw = c.get();
    c->onChange = [raw, items, onPick] { const int id = raw->getSelectedId(); if (id > 0) onPick (items[(size_t) id - 1].first); };
    return c;
}

template <typename T> static T* own (std::vector<std::unique_ptr<juce::Component>>& v, juce::Component& parent, std::unique_ptr<T> c, juce::Rectangle<int> b)
{
    auto* raw = c.get(); raw->setBounds (b); parent.addAndMakeVisible (*raw); v.push_back (std::move (c)); return raw;
}

void ShizukuEditor::buildOscPage()
{
    auto& O = owned[0];
    const char* warps[][2] = { { "none", "Warp: なし" }, { "kSync", "Sync" }, { "kBendPos", "Bend +" }, { "kBendNeg", "Bend -" }, { "kBendPosNeg", "Bend +/-" }, { "kPWM", "PWM" }, { "kASYMPos", "Asym +" }, { "kASYMNeg", "Asym -" }, { "kASYMPosNeg", "Asym +/-" }, { "kPD_OSC", "Phase Dist." }, { "kMirror", "Mirror" }, { "kQuantize", "Quantize" } };
    std::vector<std::pair<String, String>> warpItems; for (auto& w : warps) warpItems.push_back ({ w[0], U (w[1]) });
    std::vector<std::pair<String, String>> routeItems { { "0", U ("→ Filter") }, { "1", U ("→ FX") }, { "2", U ("→ Out") } };
    for (int i = 0; i < 3; ++i)
    {
        auto* panel = own (O, oscPage, std::make_unique<Panel> ("OSC " + String::charToString ((juce::juce_wchar) ('A' + i))), { 8 + i * 312, 8, 304, 300 });
        auto& o = P().osc[(size_t) i];
        panel->dim = ! o.on;
        auto* tg = own (O, *panel, std::make_unique<Toggle> ([this, i] { return P().osc[(size_t) i].on; }, [this, i] (bool v) { P().osc[(size_t) i].on = v; }), { 6, 5, 34, 18 });
        tg->onChange = [this, panel, i] { panel->dim = ! P().osc[(size_t) i].on; panel->repaint(); edited(); };
        auto lbl = std::make_unique<juce::Label>(); lbl->setText ((o.sample ? U ("🎹 ") : String()) + o.tableName, juce::dontSendNotification); lbl->setFont (fonts().mono (11.0f)); lbl->setColour (juce::Label::textColourId, col::aqua2);
        own (O, *panel, std::move (lbl), { 110, 5, 130, 18 });
        auto wav = std::make_unique<juce::TextButton> ("WAV"); wav->setTooltip (U ("WAVの波形テーブル(またはサンプル)を読み込む"));
        wav->onClick = [this, i]
        {
            chooser = std::make_unique<juce::FileChooser> (U ("WAVを選んでください"), juce::File(), "*.wav");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, i] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult(); if (! f.existsAsFile()) return;
                String err; proc.loadOscWav (i, f, err);
                if (err.isNotEmpty()) setStatus (err, true); else { rebuild(); setStatus (U ("読み込みました: ") + f.getFileName()); }
            });
        };
        own (O, *panel, std::move (wav), { 248, 4, 48, 20 });
        wtViews[(size_t) i] = own (O, *panel, std::make_unique<WTView> (*this, i), { 8, 28, 288, 74 });
        const auto st = proc.tableStatus[(size_t) i];
        if (st == TableStatus::lib) { panel->note = U ("✓ Serumのテーブルを使用"); panel->noteCol = col::ok; }
        else if (st == TableStatus::embedded) { panel->note = U ("✓ プリセット内蔵の波形"); panel->noteCol = col::ok; }
        else if (st == TableStatus::approx) panel->note = U ("≈ 近似の波形で再生中");
        else if (st == TableStatus::missing) panel->note = U ("⚠ 未読込: ") + o.tablePath;
        else if (st == TableStatus::sampleMissing) panel->note = U ("⚠ サンプルが必要です(WAVボタン)");
        else if (st == TableStatus::sampleOk) { panel->note = U ("✓ サンプル読込済み"); panel->noteCol = col::ok; }
        auto D = [i] (const char* p) { return Dest { "Oscillator", i, p }; };
        std::vector<Knob::Spec> specs;
        auto add = [&] (const char* label, double mn, double mx, double df, std::function<double()> g, std::function<void (double)> s, std::function<String (double)> f, Dest d = {}, double step = 0, double ms = 1)
        { Knob::Spec sp; sp.label = label; sp.min = mn; sp.max = mx; sp.def = df; sp.get = std::move (g); sp.set = std::move (s); sp.fmt = std::move (f); sp.dest = d; sp.step = step; sp.modScale = ms; specs.push_back (sp); };
        auto F = [this, i] (float Osc::* m) { return std::make_pair (std::function<double()> ([this, i, m] { return (double) (P().osc[(size_t) i].*m); }), std::function<void (double)> ([this, i, m] (double v) { P().osc[(size_t) i].*m = (float) v; })); };
        { auto [g, s] = F (&Osc::oct); add ("Oct", -4, 4, 0, g, s, fmtSigned, {}, 1); }
        { auto [g, s] = F (&Osc::semi); add ("Semi", -12, 12, 0, g, s, fmtSigned, D ("kParamCoarsePit"), 1); }
        { auto [g, s] = F (&Osc::fine); add ("Fine", -100, 100, 0, g, s, [] (double v) { return String ((int) std::lround (v)) + "c"; }, D ("kParamFine"), 1); }
        add ("Unison", 1, 16, 1, [this, i] { return (double) P().osc[(size_t) i].uni; }, [this, i] (double v) { P().osc[(size_t) i].uni = (int) v; }, [] (double v) { return String ((int) v); }, {}, 1);
        { auto [g, s] = F (&Osc::det); add ("Detune", 0, 1, 0.25, g, s, [] (double v) { return fmtPct (v * 100); }, D ("kParamDetune")); }
        { auto [g, s] = F (&Osc::blend); add ("Blend", 0, 100, 75, g, s, fmtPct); }
        { auto [g, s] = F (&Osc::pos); add ("WT Pos", 0, 256, 0, g, s, [] (double v) { return String ((int) std::lround (v)); }, D ("kParamTablePos")); }
        { auto [g, s] = F (&Osc::warpAmt); add ("Warp", 0, 1, 0, g, s, [] (double v) { return fmtPct (v * 100); }, D ("kParamWarp")); }
        { auto [g, s] = F (&Osc::phase); add ("Phase", 0, 360, 0, g, s, [] (double v) { return String ((int) std::lround (v)) + U ("°"); }); }
        { auto [g, s] = F (&Osc::rand); add ("Rand", 0, 100, 100, g, s, fmtPct); }
        { auto [g, s] = F (&Osc::pan); add ("Pan", -100, 100, 0, g, s, [] (double v) { return String ((int) std::lround (v)); }, D ("kParamPan"), 0, 100); }
        { auto [g, s] = F (&Osc::vol); add ("Level", 0, 1, 0.5625, g, s, [] (double v) { return fmtPct (v * 100); }, D ("kParamVolume")); }
        for (int k = 0; k < (int) specs.size(); ++k)
        {
            auto kn = knob (specs[(size_t) k]);
            if (specs[(size_t) k].label == "WT Pos") { auto* wv = wtViews[(size_t) i]; kn->onChange = [this, wv] { wv->repaint(); edited(); }; }
            own (O, *panel, std::move (kn), { 6 + (k % 6) * 49, 122 + (k / 6) * 66, 48, 64 });
        }
        own (O, *panel, makeCombo (warpItems, o.warp, [this, i] (const String& v) { P().osc[(size_t) i].warp = v; edited(); }), { 8, 266, 112, 24 });
        own (O, *panel, makeCombo (routeItems, String (P().route[(size_t) i].dest), [this, i] (const String& v) { P().route[(size_t) i].dest = v.getIntValue(); edited(); }), { 124, 266, 92, 24 });
        const auto& r = P().route[(size_t) i];
        own (O, *panel, makeCombo ({ { "-100", "F1" }, { "0", "F1+F2" }, { "100", "F2" } }, String (r.bal <= -50 ? -100 : r.bal >= 50 ? 100 : 0), [this, i] (const String& v) { P().route[(size_t) i].bal = (float) v.getIntValue(); edited(); }), { 220, 266, 76, 24 });
    }
    // sub & noise
    {
        auto* sp = own (O, oscPage, std::make_unique<Panel> ("SUB"), { 944, 8, 336, 146 });
        sp->dim = ! P().sub.on;
        auto* tg = own (O, *sp, std::make_unique<Toggle> ([this] { return P().sub.on; }, [this] (bool v) { P().sub.on = v; }), { 6, 5, 34, 18 });
        tg->onChange = [this, sp] { sp->dim = ! P().sub.on; sp->repaint(); edited(); };
        own (O, *sp, makeCombo ({ { "sine", "Sine" }, { "tri", "Triangle" }, { "saw", "Saw" }, { "square", "Square" } }, P().sub.shape, [this] (const String& v) { P().sub.shape = v; edited(); }), { 214, 4, 114, 22 });
        Knob::Spec a; a.label = "Oct"; a.min = -4; a.max = 4; a.step = 1; a.get = [this] { return (double) P().sub.oct; }; a.set = [this] (double v) { P().sub.oct = (float) v; }; a.fmt = fmtSigned;
        Knob::Spec b; b.label = "Pan"; b.min = -100; b.max = 100; b.get = [this] { return (double) P().sub.pan; }; b.set = [this] (double v) { P().sub.pan = (float) v; }; b.fmt = [] (double v) { return String ((int) std::lround (v)); }; b.dest = { "Oscillator", 4, "kParamPan" }; b.modScale = 100;
        Knob::Spec c; c.label = "Level"; c.min = 0; c.max = 1; c.def = 0.5; c.get = [this] { return (double) P().sub.vol; }; c.set = [this] (double v) { P().sub.vol = (float) v; }; c.fmt = [] (double v) { return fmtPct (v * 100); }; c.dest = { "Oscillator", 4, "kParamVolume" };
        int k = 0; for (auto& s : { a, b, c }) own (O, *sp, knob (s), { 10 + (k++) * 52, 34, 48, 64 });
        own (O, *sp, makeCombo ({ { "0", U ("→ Filter") }, { "1", U ("→ FX") }, { "2", U ("→ Out") } }, String (P().route[4].dest), [this] (const String& v) { P().route[4].dest = v.getIntValue(); edited(); }), { 10, 108, 110, 24 });
    }
    {
        auto* np = own (O, oscPage, std::make_unique<Panel> ("NOISE"), { 944, 162, 336, 146 });
        np->dim = ! P().noise.on;
        auto* tg = own (O, *np, std::make_unique<Toggle> ([this] { return P().noise.on; }, [this] (bool v) { P().noise.on = v; }), { 6, 5, 34, 18 });
        tg->onChange = [this, np] { np->dim = ! P().noise.on; np->repaint(); edited(); };
        own (O, *np, makeCombo ({ { "White", "White" }, { "Pink", "Pink" }, { "Brown", "Brown" }, { "Sample", U ("Sample≈Pink") } }, P().noise.type, [this] (const String& v) { P().noise.type = v; edited(); }), { 214, 4, 114, 22 });
        Knob::Spec a; a.label = "Color"; a.def = 0.5; a.get = [this] { return (double) P().noise.color; }; a.set = [this] (double v) { P().noise.color = (float) v; }; a.fmt = [] (double v) { return fmtPct (v * 100); };
        Knob::Spec b; b.label = "Pan"; b.min = -100; b.max = 100; b.get = [this] { return (double) P().noise.pan; }; b.set = [this] (double v) { P().noise.pan = (float) v; }; b.fmt = [] (double v) { return String ((int) std::lround (v)); }; b.dest = { "Oscillator", 3, "kParamPan" }; b.modScale = 100;
        Knob::Spec c; c.label = "Level"; c.def = 0.5; c.get = [this] { return (double) P().noise.vol; }; c.set = [this] (double v) { P().noise.vol = (float) v; }; c.fmt = [] (double v) { return fmtPct (v * 100); }; c.dest = { "Oscillator", 3, "kParamVolume" };
        int k = 0; for (auto& s : { a, b, c }) own (O, *np, knob (s), { 10 + (k++) * 52, 34, 48, 64 });
        own (O, *np, makeCombo ({ { "0", U ("→ Filter") }, { "1", U ("→ FX") }, { "2", U ("→ Out") } }, String (P().route[3].dest), [this] (const String& v) { P().route[3].dest = v.getIntValue(); edited(); }), { 10, 108, 110, 24 });
    }
    // filters
    std::vector<std::pair<String, String>> ft { { "MgL12", "MG Low 12" }, { "MgL18", "MG Low 18" }, { "MgL24", "MG Low 24" }, { "L12", "Low 12" }, { "L24", "Low 24" }, { "H12", "High 12" }, { "H24", "High 24" }, { "B12", "Band 12" }, { "B24", "Band 24" }, { "P12", "Peak 12" }, { "N12", "Notch 12" }, { "BandReject", "Band Reject" }, { "Comb", "Comb" } };
    for (int j = 0; j < 2; ++j)
    {
        auto* fp = own (O, oscPage, std::make_unique<Panel> ("FILTER " + String (j + 1)), { 8 + j * 312, 316, 304, 156 });
        fp->dim = ! P().filt[(size_t) j].on;
        auto* tg = own (O, *fp, std::make_unique<Toggle> ([this, j] { return P().filt[(size_t) j].on; }, [this, j] (bool v) { P().filt[(size_t) j].on = v; }), { 6, 5, 34, 18 });
        tg->onChange = [this, fp, j] { fp->dim = ! P().filt[(size_t) j].on; fp->repaint(); edited(true); };
        own (O, *fp, makeCombo (ft, P().filt[(size_t) j].type, [this, j] (const String& v) { P().filt[(size_t) j].type = v; edited (true); }), { 170, 4, 126, 22 });
        frViews[(size_t) j] = own (O, *fp, std::make_unique<FilterView> (*this, j), { 8, 30, 288, 52 });
        auto D = [j] (const char* p) { return Dest { "VoiceFilter", j, p }; };
        Knob::Spec a; a.label = "Cutoff"; a.def = 0.5; a.get = [this, j] { return (double) P().filt[(size_t) j].freq; }; a.set = [this, j] (double v) { P().filt[(size_t) j].freq = (float) v; }; a.fmt = [] (double v) { return fmtHz (fnorm (v)); }; a.dest = D ("kParamFreq");
        Knob::Spec b; b.label = "Res"; b.max = 100; b.def = 10; b.get = [this, j] { return (double) P().filt[(size_t) j].reso; }; b.set = [this, j] (double v) { P().filt[(size_t) j].reso = (float) v; }; b.fmt = fmtPct; b.dest = D ("kParamReso");
        Knob::Spec c; c.label = "Drive"; c.max = 100; c.get = [this, j] { return (double) P().filt[(size_t) j].drive; }; c.set = [this, j] (double v) { P().filt[(size_t) j].drive = (float) v; }; c.fmt = fmtPct; c.dest = D ("kParamDrive");
        Knob::Spec d; d.label = "Var"; d.max = 100; d.get = [this, j] { return (double) P().filt[(size_t) j].vari; }; d.set = [this, j] (double v) { P().filt[(size_t) j].vari = (float) v; }; d.fmt = fmtPct; d.dest = D ("kParamVar");
        Knob::Spec e; e.label = "Key"; e.get = [this, j] { return (double) P().filt[(size_t) j].key; }; e.set = [this, j] (double v) { P().filt[(size_t) j].key = (float) v; }; e.fmt = [] (double v) { return fmtPct (v * 100); };
        int k = 0; for (auto& s : { a, b, c, d, e }) own (O, *fp, knob (s), { 8 + (k++) * 56, 88, 52, 64 });
    }
    // macros (host-automatable)
    {
        auto* mp = own (O, oscPage, std::make_unique<Panel> ("MACROS"), { 632, 316, 648, 156 });
        for (int i = 0; i < 8; ++i)
        {
            auto* prm = proc.apvts.getParameter ("m" + String (i + 1));
            Knob::Spec s; s.label = "M" + String (i + 1); s.max = 100; s.fmt = fmtPct; s.def = 0;
            s.get = [this, i] { return (double) proc.apvts.getRawParameterValue ("m" + String (i + 1))->load(); };
            s.set = [this, prm, i] (double v) { prm->setValueNotifyingHost (prm->convertTo0to1 ((float) v)); P().macro[(size_t) i].val = (float) v; };
            auto kn = knob (s); kn->onChange = [this] { refreshModRings(); };
            own (O, *mp, std::move (kn), { 10 + i * 79, 30, 70, 64 });
            auto ed2 = std::make_unique<juce::TextEditor>(); ed2->setText (P().macro[(size_t) i].name, false); ed2->setJustification (juce::Justification::centred); ed2->setFont (fonts().body (11.0f, true));
            auto* raw = ed2.get(); ed2->onTextChange = [this, raw, i] { P().macro[(size_t) i].name = raw->getText(); };
            own (O, *mp, std::move (ed2), { 8 + i * 79, 98, 74, 22 });
            auto chip = std::make_unique<SourceChip> (*this, "macro" + String (i), "M" + String (i + 1), 25 + i);
            chip->setTooltip (U ("ドラッグしてノブに割り当て"));
            own (O, *mp, std::move (chip), { 14 + i * 79, 124, 62, 22 });
        }
    }
}

//==============================================================================
struct FxKnob { const char* p; const char* label; double mn, mx, df; int curve; };
struct FxSchema { std::vector<FxKnob> k; const char* selKey = nullptr; std::vector<std::pair<String, String>> sel; const char* modName; };
static const FxSchema* schema (int t)
{
    static std::map<int, FxSchema> m = []
    {
        std::map<int, FxSchema> s;
        s[0] = { { { "kParamDrive", "Drive", 0, 100, 25, 0 }, { "kParamWet", "Mix", 0, 100, 100, 0 }, { "kParamLevelOut", "Out", 0, 1, 0.5, 0 } }, "kParamMode",
                 { { "kTube", "Tube" }, { "kSoftClip", "Soft Clip" }, { "kHardClip", "Hard Clip" }, { "kOverdrive", "Overdrive" }, { "kAsym", "Asym" }, { "kSine", "Sine Fold" }, { "kLinFold", "Linear Fold" }, { "kZeroSquare", "Zero-Square" }, { "kRectify", "Rectify" }, { "kDownsample", "Downsample" }, { "kQuant", "Bitcrush" } }, "FXDistortion" };
        s[1] = { { { "kParamRate", "Rate", 0.01, 10, 0.3, 1 }, { "kParamWidth", "Depth", 0, 400, 100, 0 }, { "kParamFeedback", "Feedback", 0, 98, 50, 0 }, { "kParamWet", "Mix", 0, 100, 100, 0 } }, nullptr, {}, "FXFlanger" };
        s[2] = { { { "kParamFreq", "Freq", 20, 18000, 800, 1 }, { "kParamRate", "Rate", 0.01, 10, 0.3, 1 }, { "kParamDepth", "Depth", 0, 100, 50, 0 }, { "kParamFeedback", "Feedback", 0, 95, 40, 0 }, { "kParamWet", "Mix", 0, 100, 50, 0 } }, nullptr, {}, "FXPhaser" };
        s[3] = { { { "kParamRate", "Rate", 0.01, 10, 0.8, 1 }, { "kParamDelay", "Delay", 1, 40, 8, 0 }, { "kParamDepth", "Depth", 0, 100, 26, 0 }, { "kParamFeedback", "Feedback", 0, 95, 0, 0 }, { "kParamWet", "Mix", 0, 100, 50, 0 } }, nullptr, {}, "FXChorus" };
        s[4] = { { { "kParamTimeL", "Time L", 0.01, 2, 0.25, 1 }, { "kParamTimeR", "Time R", 0.01, 2, 0.25, 1 }, { "kParamFeedback", "Feedback", 0, 98, 40, 0 }, { "kParamFreq", "Freq", 40, 18000, 2000, 1 }, { "kParamBW", "BW", 0.3, 10, 4, 0 }, { "kParamWet", "Mix", 0, 100, 30, 0 } }, "kParamMode",
                 { { "0", "Normal" }, { "1", "Ping-Pong" }, { "2", "Tap" } }, "FXDelay" };
        s[5] = { { { "kParamThresh", "Thresh", 0, 1, 0.5, 0 }, { "kParamRatio", "Ratio", 1, 20, 4, 0 }, { "kParamAttack", "Attack", 0.1, 100, 3, 1 }, { "kParamRelease", "Release", 1, 1000, 80, 1 }, { "kParamMakeup", "Gain", 0, 24, 0, 0 }, { "kParamGain1", "Low", -24, 24, 0, 0 }, { "kParamGain2", "Mid", -24, 24, 0, 0 }, { "kParamGain3", "High", -24, 24, 0, 0 }, { "kParamWet", "Mix", 0, 100, 100, 0 } }, "kParamMultiband",
                 { { "0", "Single" }, { "1", String::fromUTF8 ("Multiband (OTT風)") } }, "FXComp" };
        s[6] = { { { "kParamSize", "Size", 0, 100, 50, 0 }, { "kParamDelay", "Decay", 0, 100, 50, 0 }, { "kParamPreDelay", "Pre", 0, 0.45, 0, 0 }, { "kParamFreq", "Damp", 0, 100, 40, 0 }, { "kParamFreqB", "Low Cut", 0, 100, 0, 0 }, { "kParamWidth", "Width", 0, 100, 100, 0 }, { "kParamWet", "Mix", 0, 100, 20, 0 } }, "kParamType",
                 { { "kHall", "Hall" }, { "kPlate", "Plate" }, { "kAbyss", "Abyss" } }, "FXReverb" };
        s[7] = { { { "kParamFreq1", "Freq 1", 20, 20000, 200, 1 }, { "kParamGain1", "Gain 1", -24, 24, 0, 0 }, { "kParamReso1", "Q 1", 0, 100, 30, 0 }, { "kParamFreq2", "Freq 2", 20, 20000, 3000, 1 }, { "kParamGain2", "Gain 2", -24, 24, 0, 0 }, { "kParamReso2", "Q 2", 0, 100, 30, 0 }, { "kParamLevelOut", "Out", 0, 1, 0.5, 0 } }, nullptr, {}, "FXEQ" };
        s[8] = { { { "kParamFreq", "Cutoff", 0, 1, 0.5, 0 }, { "kParamReso", "Res", 0, 100, 10, 0 }, { "kParamDrive", "Drive", 0, 100, 0, 0 }, { "kParamWet", "Mix", 0, 100, 100, 0 }, { "kParamLevelOut", "Out", 0, 1, 0.5, 0 } }, "kParamType",
                 { { "L12", "Low 12" }, { "L24", "Low 24" }, { "MgL12", "MG Low 12" }, { "H12", "High 12" }, { "H24", "High 24" }, { "B12", "Band 12" }, { "P12", "Peak 12" }, { "N12", "Notch" }, { "BandReject", "Band Reject" }, { "Comb", "Comb" } }, "FXFilter" };
        s[9] = { { { "kParamUnison", "Voices", 1, 7, 4, 0 }, { "kParamDetune", "Detune", 0, 100, 25, 0 }, { "kParamRate", "Rate", 0, 100, 30, 0 }, { "kParamWet", "Mix", 0, 100, 50, 0 }, { "kParamDimEWet", "Dim", 0, 100, 0, 0 } }, nullptr, {}, "FXHyperD" };
        s[12] = { { { "kParamHPF", "HPF", 0, 2000, 0, 0 }, { "kParamLevelOut", "Gain", 0, 1, 0.5, 0 } }, nullptr, {}, "FXUtils" };
        return s;
    }();
    auto it = m.find (t); return it == m.end() ? nullptr : &it->second;
}
static String fxFmt (int type, const String& p, double v)
{
    if (type == 9 && p != "kParamUnison") return fmtPct (v);
    if (p.startsWith ("kParamFreq") && p != "kParamFreqB" && v >= 20) return fmtHz (v);
    if (p == "kParamLevelOut") return fmtDb (4 * v * v);
    if (p.contains ("Time") || p == "kParamPreDelay") return fmtSec (v);
    if (p.contains ("Gain") || p == "kParamMakeup") return String (v, 1) + "dB";
    if (p == "kParamThresh") return String ((int) std::lround ((v - 1) * 48)) + "dB";
    if (p == "kParamRatio") return v >= 20 ? U ("∞") : String (v, 1) + ":1";
    if (p == "kParamRate") return String (v, 2) + "Hz";
    if (p == "kParamUnison") return String ((int) std::lround (v));
    if (p == "kParamFreq" && v <= 1) return fmtHz (fnorm (v));
    return String (std::round (v * 10) / 10);
}

void ShizukuEditor::buildFxPage()
{
    auto& O = owned[1];
    fxPage.removeAllChildren(); fxPage.addAndMakeVisible (fxView);
    fxInner.removeAllChildren();
    own (O, fxPage, makeCombo ({ { "fx", "MAIN FX" }, { "bus1", "BUS 1" }, { "bus2", "BUS 2" } }, fxRack, [this] (const String& v) { fxRack = v; juce::MessageManager::callAsync ([self = juce::Component::SafePointer<ShizukuEditor> (this)] { if (self) { self->rebuild(); self->showTab (1); } }); }), { 8, 8, 130, 24 });
    auto hint = std::make_unique<juce::Label>(); hint->setText (fxRack == "fx" ? U ("上から順にかかります") : U ("各オシレーターの BUS 送り量(Serum側の設定)で送られます"), juce::dontSendNotification);
    hint->setColour (juce::Label::textColourId, col::muted); hint->setFont (fonts().body (12.0f));
    own (O, fxPage, std::move (hint), { 146, 8, 600, 24 });
    std::vector<std::pair<String, String>> addItems { { "", U ("＋ FXを追加") } };
    for (int t : { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12 }) addItems.push_back ({ String (t), fxName (t) });
    own (O, fxPage, makeCombo (addItems, "", [this] (const String& v)
    {
        if (v.isEmpty()) return;
        auto& list = fxRack == "fx" ? P().fx : fxRack == "bus1" ? P().bus1 : P().bus2;
        FxUnit u; u.type = v.getIntValue(); u.name = fxName (u.type); list.push_back (u);
        edited (true); juce::MessageManager::callAsync ([self = juce::Component::SafePointer<ShizukuEditor> (this)] { if (self) { self->rebuild(); self->showTab (1); } });
    }), { Content::W - 32 - 190, 8, 180, 24 });
    fxView.setBounds (8, 40, Content::W - 48, 432);
    auto& list = fxRack == "fx" ? P().fx : fxRack == "bus1" ? P().bus1 : P().bus2;
    const int cardH = 84, w = Content::W - 64;
    fxInner.setSize (w, juce::jmax (40, (int) list.size() * (cardH + 8)));
    if (list.empty())
    {
        auto l = std::make_unique<juce::Label>(); l->setText (U ("FXはまだありません。右上の「＋ FXを追加」から入れられます。"), juce::dontSendNotification); l->setColour (juce::Label::textColourId, col::muted);
        own (O, fxInner, std::move (l), { 4, 4, w - 8, 30 });
    }
    for (int i = 0; i < (int) list.size(); ++i)
    {
        auto& f = list[(size_t) i];
        const auto* S = schema (f.type);
        auto* card = own (O, fxInner, std::make_unique<Panel> (String (f.name).toUpperCase()), { 0, i * (cardH + 8), w, cardH });
        card->dim = ! f.on;
        const String rack = fxRack;
        auto L = [this, rack] () -> std::vector<FxUnit>& { return rack == "fx" ? P().fx : rack == "bus1" ? P().bus1 : P().bus2; };
        auto* tg = own (O, *card, std::make_unique<Toggle> ([L, i] { return L()[(size_t) i].on; }, [L, i] (bool v) { L()[(size_t) i].on = v; }), { 6, 5, 34, 18 });
        tg->onChange = [this, card, L, i] { card->dim = ! L()[(size_t) i].on; card->repaint(); edited(); };
        if (S && S->selKey)
        {
            const char* key = S->selKey;
            const auto* cur = f.p.getVarPointer (key);
            String curS = cur ? (cur->isString() ? cur->toString() : String ((int) (double) *cur)) : S->sel[0].first;
            own (O, *card, makeCombo (S->sel, curS, [this, L, i, key] (const String& v)
            {
                auto& u = L()[(size_t) i];
                if (v.containsOnly ("-0123456789")) u.p.set (key, v.getIntValue()); else u.p.set (key, v);
                edited (true);
            }), { 8, 28, 150, 22 });
        }
        auto mv = [this, L, i] (int d) { auto& l = L(); const int j = i + d; if (j < 0 || j >= (int) l.size()) return; std::swap (l[(size_t) i], l[(size_t) j]); edited (true); juce::MessageManager::callAsync ([self = juce::Component::SafePointer<ShizukuEditor> (this)] { if (self) { self->rebuild(); self->showTab (1); } }); };
        auto up = std::make_unique<juce::TextButton> (U ("↑")); up->onClick = [mv] { mv (-1); }; own (O, *card, std::move (up), { 8, 54, 30, 22 });
        auto dn = std::make_unique<juce::TextButton> (U ("↓")); dn->onClick = [mv] { mv (1); }; own (O, *card, std::move (dn), { 42, 54, 30, 22 });
        auto del = std::make_unique<juce::TextButton> (U ("✕"));
        del->onClick = [this, L, i, rack]
        {
            auto& l = L(); l.erase (l.begin() + i);
            if (rack == "fx")
            {
                auto& mods = P().mod;
                mods.erase (std::remove_if (mods.begin(), mods.end(), [i] (const Mod& m) { return m.mod.startsWith ("FX") && m.id == i; }), mods.end());
                for (auto& m : mods) if (m.mod.startsWith ("FX") && m.id > i) --m.id;
            }
            edited (true); juce::MessageManager::callAsync ([self = juce::Component::SafePointer<ShizukuEditor> (this)] { if (self) { self->rebuild(); self->showTab (1); } });
        };
        own (O, *card, std::move (del), { 76, 54, 30, 22 });
        if (! S)
        {
            auto l = std::make_unique<juce::Label>(); l->setText (U ("このFXはSHIZUKUではまだ再現できません(スルーします)"), juce::dontSendNotification); l->setColour (juce::Label::textColourId, col::muted);
            own (O, *card, std::move (l), { 180, 26, 500, 30 });
            continue;
        }
        int k = 0;
        for (auto& fk : S->k)
        {
            Knob::Spec s; s.label = fk.label; s.min = fk.mn; s.max = fk.mx; s.def = fk.df; s.curve = fk.curve;
            const String key = fk.p; const double def = fk.df; const int type = f.type;
            s.get = [L, i, key, def] { const auto* v = L()[(size_t) i].p.getVarPointer (juce::Identifier (key)); return v && ! v->isString() ? (double) *v : def; };
            s.set = [L, i, key] (double v) { L()[(size_t) i].p.set (juce::Identifier (key), v); };
            s.fmt = [type, key] (double v) { return fxFmt (type, key, v); };
            if (rack == "fx") s.dest = { S->modName, i, key };
            own (O, *card, knob (s), { 180 + (k++) * 58, 10, 56, 64 });
        }
    }
}

//==============================================================================
std::vector<std::pair<String, String>> ShizukuEditor::destOptions() const
{
    std::vector<std::pair<String, String>> o;
    const char* oscP[][2] = { { "kParamVolume", "Level" }, { "kParamPan", "Pan" }, { "kParamTablePos", "WT Pos" }, { "kParamDetune", "Detune" }, { "kParamWarp", "Warp" }, { "kParamCoarsePit", "Coarse Pitch" }, { "kParamFine", "Fine" }, { "kParamUnisonStereo", "Stereo" } };
    for (int i = 0; i < 3; ++i) for (auto& p : oscP) o.push_back ({ "Oscillator|" + String (i) + "|" + p[0], "OSC " + String::charToString ((juce::juce_wchar) ('A' + i)) + " " + p[1] });
    o.push_back ({ "Oscillator|3|kParamVolume", "NOISE Level" }); o.push_back ({ "Oscillator|3|kParamPan", "NOISE Pan" });
    o.push_back ({ "Oscillator|4|kParamVolume", "SUB Level" }); o.push_back ({ "Oscillator|4|kParamPan", "SUB Pan" });
    const char* fp[][2] = { { "kParamFreq", "Cutoff" }, { "kParamReso", "Res" }, { "kParamDrive", "Drive" }, { "kParamVar", "Var" } };
    for (int j = 0; j < 2; ++j) for (auto& p : fp) o.push_back ({ "VoiceFilter|" + String (j) + "|" + p[0], "FILTER " + String (j + 1) + " " + p[1] });
    const char* ep[][2] = { { "kParamAttack", "Attack" }, { "kParamDecay", "Decay" }, { "kParamSustain", "Sustain" }, { "kParamRelease", "Release" } };
    for (int j = 0; j < 4; ++j) for (auto& p : ep) o.push_back ({ "Env|" + String (j) + "|" + p[0], "ENV " + String (j + 1) + " " + p[1] });
    o.push_back ({ "Global|0|kParamMasterTuning", "Master Tune" }); o.push_back ({ "Global|0|kParamMasterVolume", "Master Volume" });
    const auto& fx = proc.patch.fx;
    for (int i = 0; i < (int) fx.size(); ++i)
        if (auto* S = schema (fx[(size_t) i].type))
            for (auto& k : S->k) o.push_back ({ String (S->modName) + "|" + String (i) + "|" + k.p, "FX" + String (i + 1) + " " + fxName (fx[(size_t) i].type) + " " + k.label });
    return o;
}
String ShizukuEditor::destLabel (const Mod& m) const
{
    const String key = m.mod + "|" + String (m.id) + "|" + m.p;
    for (auto& o : destOptions()) if (o.first == key) return o.second;
    return m.mod + " " + String (m.id) + " " + m.p.replace ("kParam", "");
}

void ShizukuEditor::buildMatrixPage()
{
    auto& O = owned[2];
    O.clear();
    matrixPage.removeAllChildren(); matrixPage.addAndMakeVisible (mxView);
    mxInner.removeAllChildren();
    auto cnt = std::make_unique<juce::Label>(); cnt->setText (String ((int) P().mod.size()) + U (" 本のモジュレーション   ·   SOURCE / AUX(かける) / DESTINATION / AMOUNT / BIPOLAR"), juce::dontSendNotification);
    cnt->setColour (juce::Label::textColourId, col::muted); cnt->setFont (fonts().body (12.0f));
    own (O, matrixPage, std::move (cnt), { 8, 8, 800, 24 });
    auto add = std::make_unique<juce::TextButton> (U ("＋ 行を追加"));
    add->onClick = [this] { Mod m; m.src = 6; m.mod = "Oscillator"; m.id = 0; m.p = "kParamTablePos"; m.amt = 25; P().mod.push_back (m); edited (true); };
    own (O, matrixPage, std::move (add), { Content::W - 32 - 130, 8, 120, 24 });
    mxView.setBounds (8, 40, Content::W - 48, 432);
    std::vector<std::pair<String, String>> srcs;
    for (int id : { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 25, 26, 27, 28, 29, 30, 31, 32 }) srcs.push_back ({ String (id), srcName (id) });
    const auto dests = destOptions();
    const int rowH = 34, w = Content::W - 64;
    mxInner.setSize (w, juce::jmax (40, (int) P().mod.size() * rowH + 8));
    for (int i = 0; i < (int) P().mod.size(); ++i)
    {
        const auto& m = P().mod[(size_t) i];
        const int y = i * rowH + 4;
        auto srcItems = srcs; if (! srcName (m.src).startsWith ("#")) {} else srcItems.push_back ({ String (m.src), srcName (m.src) });
        own (O, mxInner, makeCombo (srcItems, String (m.src), [this, i] (const String& v) { P().mod[(size_t) i].src = v.getIntValue(); edited(); }), { 0, y, 140, 26 });
        own (O, mxInner, makeCombo (srcs, String (m.aux), [this, i] (const String& v) { P().mod[(size_t) i].aux = v.getIntValue(); edited(); }), { 148, y, 140, 26 });
        own (O, mxInner, makeCombo (dests, m.mod + "|" + String (m.id) + "|" + m.p, [this, i] (const String& v)
        {
            auto parts = juce::StringArray::fromTokens (v, "|", ""); if (parts.size() < 3) return;
            auto& mm = P().mod[(size_t) i]; mm.mod = parts[0]; mm.id = parts[1].getIntValue(); mm.p = parts[2]; edited();
        }), { 296, y, 280, 26 });
        auto sl = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        sl->setRange (-100, 100, 0.5); sl->setValue (m.amt, juce::dontSendNotification); sl->setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 22);
        sl->setColour (juce::Slider::textBoxTextColourId, col::aqua2); sl->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        auto* raw = sl.get(); sl->onValueChange = [this, raw, i] { P().mod[(size_t) i].amt = (float) raw->getValue(); edited(); };
        sl->onDragEnd = [this] { refreshModRings(); };
        own (O, mxInner, std::move (sl), { 586, y, 330, 26 });
        own (O, mxInner, std::make_unique<Toggle> ([this, i] { return P().mod[(size_t) i].bip; }, [this, i] (bool v) { P().mod[(size_t) i].bip = v; edited(); }), { 930, y + 4, 40, 18 });
        auto del = std::make_unique<juce::TextButton> (U ("✕"));
        del->onClick = [this, i] { auto& mods = P().mod; mods.erase (mods.begin() + i); edited (true); refreshModRings(); };
        own (O, mxInner, std::move (del), { 984, y + 1, 30, 24 });
    }
}

//==============================================================================
void ShizukuEditor::buildGlobalPage()
{
    auto& O = owned[3];
    auto& G = P().glob; juce::ignoreUnused (G);
    auto* vp = own (O, globalPage, std::make_unique<Panel> ("VOICE"), { 8, 8, 420, 220 });
    auto lab = [&] (juce::Component& parent, const String& t, juce::Rectangle<int> b) { auto l = std::make_unique<juce::Label>(); l->setText (t, juce::dontSendNotification); l->setColour (juce::Label::textColourId, col::muted); l->setFont (fonts().body (12.0f, true)); own (O, parent, std::move (l), b); };
    lab (*vp, "MONO", { 12, 34, 60, 20 });   own (O, *vp, std::make_unique<Toggle> ([this] { return P().glob.mono; }, [this] (bool v) { P().glob.mono = v; edited (true); }), { 70, 34, 40, 20 });
    lab (*vp, "LEGATO", { 130, 34, 70, 20 }); own (O, *vp, std::make_unique<Toggle> ([this] { return P().glob.legato; }, [this] (bool v) { P().glob.legato = v; edited(); }), { 200, 34, 40, 20 });
    lab (*vp, U ("PORTA 常に"), { 260, 34, 90, 20 }); own (O, *vp, std::make_unique<Toggle> ([this] { return P().glob.portaAlways; }, [this] (bool v) { P().glob.portaAlways = v; edited(); }), { 350, 34, 40, 20 });
    Knob::Spec a; a.label = "Porta"; a.max = 2; a.curve = 2; a.pw = 2; a.get = [this] { return (double) P().glob.porta; }; a.set = [this] (double v) { P().glob.porta = (float) v; }; a.fmt = fmtSec;
    Knob::Spec b; b.label = "Bend"; b.max = 24; b.step = 1; b.def = 2; b.get = [this] { return (double) P().glob.bend; }; b.set = [this] (double v) { P().glob.bend = (float) v; }; b.fmt = [] (double v) { return String ((int) v) + "st"; };
    Knob::Spec c; c.label = "Master"; c.def = 0.5; c.get = [this] { return (double) P().glob.vol; }; c.set = [this] (double v) { P().glob.vol = (float) v; }; c.fmt = [] (double v) { return fmtPct (v * 100); }; c.dest = { "Global", 0, "kParamMasterVolume" };
    int k = 0; for (auto& s : { a, b, c }) own (O, *vp, knob (s), { 12 + (k++) * 64, 70, 60, 64 });

    auto* ap = own (O, globalPage, std::make_unique<Panel> ("ARPEGGIATOR"), { 436, 8, 420, 220 });
    own (O, *ap, std::make_unique<Toggle> ([this] { return P().arp.on; }, [this] (bool v) { P().arp.on = v; edited (true); }), { 6, 5, 34, 18 });
    own (O, *ap, makeCombo ({ { "up", U ("Up ↑") }, { "down", U ("Down ↓") }, { "updown", "Up-Down" }, { "played", U ("弾いた順") }, { "random", "Random" } }, P().arp.mode, [this] (const String& v) { P().arp.mode = v; edited(); }), { 12, 34, 140, 24 });
    own (O, *ap, makeCombo ({ { "1", "1/4" }, { "2", "1/8" }, { "3", "1/8T" }, { "4", "1/16" }, { "6", "1/16T" }, { "8", "1/32" } }, String ((int) P().arp.rate), [this] (const String& v) { P().arp.rate = (float) v.getIntValue(); edited(); }), { 160, 34, 100, 24 });
    Knob::Spec d; d.label = "Octave"; d.min = 1; d.max = 4; d.step = 1; d.def = 1; d.get = [this] { return (double) P().arp.oct; }; d.set = [this] (double v) { P().arp.oct = (int) v; }; d.fmt = [] (double v) { return String ((int) v); };
    Knob::Spec e; e.label = "Gate"; e.min = 5; e.max = 100; e.def = 80; e.get = [this] { return (double) P().arp.gate; }; e.set = [this] (double v) { P().arp.gate = (float) v; }; e.fmt = fmtPct;
    k = 0; for (auto& s : { d, e }) own (O, *ap, knob (s), { 12 + (k++) * 64, 70, 60, 64 });
    lab (*ap, U ("ONにして鍵盤を押さえると、FLのBPMに合わせて鳴ります。"), { 12, 150, 400, 20 });

    auto* pp = own (O, globalPage, std::make_unique<Panel> ("PRESET"), { 864, 8, 416, 220 });
    auto nm = std::make_unique<juce::TextEditor>(); nm->setText (P().name, false); nm->setFont (fonts().body (14.0f));
    auto* raw = nm.get(); nm->onTextChange = [this, raw] { P().name = raw->getText(); };
    own (O, *pp, std::move (nm), { 12, 34, 392, 28 });
    lab (*pp, P().desc.isNotEmpty() ? U ("説明: ") + P().desc : U ("「保存」で .shizuku に書き出せます(プリセット一覧の My SHIZUKU に出ます)"), { 12, 70, 392, 40 });
    auto lib = std::make_unique<juce::Label>();
    auto roots = TableLibrary::get().rootsFound();
    lib->setText (U ("波形の場所: ") + (roots.isEmpty() ? U ("(見つかりません)") : roots.joinIntoString ("\n")), juce::dontSendNotification);
    lib->setColour (juce::Label::textColourId, col::muted); lib->setFont (fonts().body (11.0f)); lib->setJustificationType (juce::Justification::topLeft);
    own (O, *pp, std::move (lib), { 12, 116, 392, 96 });
}

//==============================================================================
void ShizukuEditor::buildModSection()
{
    auto& O = owned[4];
    auto* panel = own (O, modArea, std::make_unique<Panel> (""), modArea.getLocalBounds());
    auto t = std::make_unique<juce::Label>(); t->setText ("MODULATION", juce::dontSendNotification); t->setFont (fonts().display (13.0f)); t->setColour (juce::Label::textColourId, col::aqua2);
    own (O, *panel, std::move (t), { 8, 4, 130, 20 });
    auto h = std::make_unique<juce::Label>(); h->setText (U ("◯ をノブにドラッグでモジュレーション追加"), juce::dontSendNotification); h->setFont (fonts().body (11.5f)); h->setColour (juce::Label::textColourId, col::muted);
    own (O, *panel, std::move (h), { 140, 4, 400, 20 });
    struct C { String key, label; int src; };
    std::vector<C> list;
    for (int i = 0; i < 4; ++i) list.push_back ({ "env" + String (i), "ENV " + String (i + 1) + (i == 0 ? U (" 音量") : String()), 2 + i });
    for (int i = 0; i < 10; ++i) list.push_back ({ "lfo" + String (i), "LFO " + String (i + 1), 6 + i });
    list.push_back ({ "vel", "VELO", 16 }); list.push_back ({ "note", "NOTE", 17 }); list.push_back ({ "mw", "MOD W", 1 });
    int x = 8;
    for (auto& c : list)
    {
        const int w = c.key == "env0" ? 92 : 70;
        auto chip = std::make_unique<SourceChip> (*this, c.key, c.label, c.src);
        chip->setTooltip (U ("クリックで編集 · ドラッグしてノブに割り当て"));
        own (O, *panel, std::move (chip), { x, 28, w, 24 });
        x += w + 4;
    }
    // the source editor
    auto area = juce::Rectangle<int> (8, 60, 790, 172);
    auto right = juce::Rectangle<int> (806, 60, panel->getWidth() - 814, 172);
    auto modsFor = [this] (int src)
    {
        juce::StringArray s;
        for (auto& m : P().mod) if (m.src == src || m.aux == src) s.add (destLabel (m) + " " + (m.amt > 0 ? "+" : "") + String ((int) std::lround (m.amt)) + "%");
        return s.isEmpty() ? U ("まだどこにも割り当てられていません") : U ("行き先: ") + s.joinIntoString (" / ");
    };
    auto info = [&] (const String& text, juce::Rectangle<int> b)
    { auto l = std::make_unique<juce::Label>(); l->setText (text, juce::dontSendNotification); l->setColour (juce::Label::textColourId, col::muted); l->setFont (fonts().body (11.5f)); l->setJustificationType (juce::Justification::topLeft); own (O, *panel, std::move (l), b); };
    if (curSource.startsWith ("env"))
    {
        const int i = curSource.getTrailingIntValue();
        modView = own (O, *panel, std::make_unique<EnvView> (*this, i), area);
        auto D = [i] (const char* p) { return Dest { "Env", i, p }; };
        auto tk = [&] (const char* label, float Env::* f, double def, Dest d) { Knob::Spec s; s.label = label; s.min = 0.0001; s.max = 32; s.curve = 2; s.pw = 4; s.def = def; s.get = [this, i, f] { return (double) (P().env[(size_t) i].*f); }; s.set = [this, i, f] (double v) { P().env[(size_t) i].*f = (float) v; }; s.fmt = fmtSec; s.dest = d; return s; };
        Knob::Spec sus; sus.label = "Sustain"; sus.def = 1; sus.get = [this, i] { return (double) P().env[(size_t) i].s; }; sus.set = [this, i] (double v) { P().env[(size_t) i].s = (float) v; }; sus.fmt = fmtDb; sus.dest = D ("kParamSustain");
        auto cv = [&] (const char* label, float Env::* f, double def) { Knob::Spec s; s.label = label; s.max = 100; s.def = def; s.get = [this, i, f] { return (double) (P().env[(size_t) i].*f); }; s.set = [this, i, f] (double v) { P().env[(size_t) i].*f = (float) v; }; s.fmt = [] (double v) { return String ((int) std::lround (v)); }; return s; };
        std::vector<Knob::Spec> specs { tk ("Attack", &Env::a, 0.0005, D ("kParamAttack")), tk ("Hold", &Env::h, 0, {}), tk ("Decay", &Env::d, 1, D ("kParamDecay")), sus, tk ("Release", &Env::r, 0.015, D ("kParamRelease")),
                                        cv ("A Curve", &Env::c1, 50), cv ("D Curve", &Env::c2, 66.6), cv ("R Curve", &Env::c3, 66.6) };
        for (int k = 0; k < (int) specs.size(); ++k) own (O, *panel, knob (specs[(size_t) k]), { right.getX() + 4 + (k % 4) * 58, right.getY() + (k / 4) * 66, 56, 64 });
        info ((i == 0 ? U ("ENV 1 は音量エンベロープ。 ") : String()) + modsFor (2 + i), { right.getX() + 4, right.getBottom() - 38, right.getWidth() - 8, 36 });
    }
    else if (curSource.startsWith ("lfo"))
    {
        const int i = curSource.getTrailingIntValue();
        modView = own (O, *panel, std::make_unique<LfoView> (*this, i), area);
        auto& L = P().lfo[(size_t) i];
        own (O, *panel, makeCombo ({ { "Trig", U ("Trig(毎回リセット)") }, { "Free", "Free" }, { "Envelope", U ("Env(1回だけ)") } }, L.mode, [this, i] (const String& v) { P().lfo[(size_t) i].mode = v; edited(); }), { right.getX() + 4, right.getY(), 150, 24 });
        own (O, *panel, makeCombo ({ { "curve", U ("カーブ") }, { "RandomSH", U ("ランダム S&H") }, { "Lorenz", U ("Lorenz(カオス)") }, { "Rossler", U ("Rössler(カオス)") } }, L.type, [this, i] (const String& v) { P().lfo[(size_t) i].type = v; edited(); }), { right.getX() + 160, right.getY(), 140, 24 });
        own (O, *panel, makeCombo ({ { "", U ("形を選ぶ…") }, { "sine", U ("サイン") }, { "tri", U ("三角") }, { "saw", U ("ノコギリ") }, { "ramp", U ("ランプ↓") }, { "square", U ("矩形") }, { "env", U ("ディケイ") } }, "", [this, i] (const String& v)
        {
            auto& l = P().lfo[(size_t) i]; l.type = "curve";
            if (v == "sine") l.pts = { { 0, 0.5f, 0.62f }, { 0.25f, 1, 0.38f }, { 0.5f, 0.5f, 0.62f }, { 0.75f, 0, 0.38f }, { 1, 0.5f, 0.5f } };
            else if (v == "tri") l.pts = { { 0, 0, 0.5f }, { 0.5f, 1, 0.5f }, { 1, 0, 0.5f } };
            else if (v == "saw") l.pts = { { 0, 0, 0.5f }, { 0.999f, 1, 0.5f }, { 1, 0, 0.5f } };
            else if (v == "ramp") l.pts = { { 0, 1, 0.5f }, { 0.999f, 0, 0.5f }, { 1, 1, 0.5f } };
            else if (v == "square") l.pts = { { 0, 1, 0.5f }, { 0.5f, 1, 0.5f }, { 0.5f, 0, 0.5f }, { 1, 0, 0.5f }, { 1, 1, 0.5f } };
            else if (v == "env") l.pts = { { 0, 1, 0.5f }, { 1, 0, 0.2f }, { 1, 1, 0.5f } };
            else return;
            edited(); if (modView) modView->repaint();
        }), { right.getX() + 306, right.getY(), 140, 24 });
        Knob::Spec r; r.label = "Rate"; r.min = 0.01; r.max = 60; r.curve = 1; r.def = 2; r.get = [this, i] { return (double) P().lfo[(size_t) i].rate; }; r.set = [this, i] (double v) { P().lfo[(size_t) i].rate = (float) v; }; r.fmt = [] (double v) { return String (v, v < 1 ? 3 : 2) + "Hz"; };
        Knob::Spec s; s.label = "Smooth"; s.max = 100; s.get = [this, i] { return (double) P().lfo[(size_t) i].smooth; }; s.set = [this, i] (double v) { P().lfo[(size_t) i].smooth = (float) v; }; s.fmt = fmtPct;
        own (O, *panel, knob (r), { right.getX() + 4, right.getY() + 32, 56, 64 });
        own (O, *panel, knob (s), { right.getX() + 62, right.getY() + 32, 56, 64 });
        info (U ("点をドラッグで移動・ダブルクリックで追加/削除・線の中央の小さな丸で曲がり具合\n") + modsFor (6 + i), { right.getX() + 124, right.getY() + 32, right.getWidth() - 128, 64 });
    }
    else
    {
        const int id = curSource == "vel" ? 16 : curSource == "note" ? 17 : 1;
        modView = nullptr;
        info (srcName (id) + "\n" + (id == 1 ? U ("キーボード左のMODスライダー、またはMIDIキーボードのモジュレーションホイールで動かせます。") : U ("弾いた鍵盤から自動で変化します。")) + "\n\n" + modsFor (id), area);
    }
}

void ShizukuEditor::selectSource (const String& key)
{
    if (! (key.startsWith ("env") || key.startsWith ("lfo") || key == "vel" || key == "note" || key == "mw")) return;
    curSource = key;
    juce::Component::SafePointer<ShizukuEditor> self (this);
    juce::MessageManager::callAsync ([self] { if (self) { const int t = self->curTab; self->rebuild(); self->showTab (t); } });
}

//==============================================================================
void ShizukuEditor::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounter();
    const double dt = (now - lastTick) / 1000.0; lastTick = now;
    if (logo.dancing) { logo.phaseBeats += dt * proc.hostBpm.load() / 60.0; logo.repaint(); }
    if (statusTimeout > 0 && --statusTimeout == 0) status.setText ({}, juce::dontSendNotification);
    if (proc.tablesGeneration.load() != lastTablesGen || needRebuild)
    {
        if (needRebuild && TableLibrary::get().ready()) { needRebuild = false; proc.resolveTables(); setStatus (U ("波形フォルダ: ") + String (TableLibrary::get().count()) + U (" 個の波形を見つけました")); }
        if (! needRebuild) { const int tab = curTab; rebuild(); showTab (tab); }
    }
    content.repaint (juce::Rectangle<int> (Content::W - 210, 10, 200, 50));
    if (modView) modView->repaint();
    static int slow = 0;
    if (++slow % 30 == 0) updateNotes();
}
}
