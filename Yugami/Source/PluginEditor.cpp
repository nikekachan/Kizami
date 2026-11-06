#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>

namespace yugami::ui
{
static const char* const kKeys = "awsedftgyhujkolp";
static const char* const kLetters[] = { "A","W","S","E","D","F","T","G","Y","H","U","J","K","O","L","P" };

juce::Colour hue (float h, float s, float b) { h -= std::floor (h); return juce::Colour::fromHSV (h, s, b, 1.0f); }

/** neon outline that cycles through the spectrum around a shape */
static juce::ColourGradient rainbow (juce::Rectangle<float> r, float shift, float alpha = 1.0f)
{
    juce::ColourGradient g (hue (shift).withAlpha (alpha), r.getX(), r.getY(), hue (shift + 0.5f).withAlpha (alpha), r.getRight(), r.getBottom(), false);
    for (int k = 1; k < 6; ++k) g.addColour (k / 6.0, hue (shift + k / 12.0f).withAlpha (alpha));
    return g;
}

//==============================================================================
Fonts::Fonts()
{
    comfortaa = juce::Typeface::createSystemTypefaceFor (BinaryData::ComfortaaBold_ttf, (size_t) BinaryData::ComfortaaBold_ttfSize);
}
juce::Font Fonts::body (float h, bool bold) const { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
juce::Font Fonts::display (float h) const { return comfortaa != nullptr ? juce::Font (juce::FontOptions (comfortaa).withHeight (h)) : body (h, true); }
juce::Font Fonts::mono (float h) const { return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, juce::Font::bold)); }
const Fonts& fonts() { static Fonts f; return f; }

//==============================================================================
Look::Look()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::TextButton::buttonColourId, col::cell);
    setColour (juce::TextButton::buttonOnColourId, col::time);
    setColour (juce::TextButton::textColourOffId, col::ink);
    setColour (juce::TextButton::textColourOnId, col::bg);
    setColour (juce::ComboBox::backgroundColourId, col::cell);
    setColour (juce::ComboBox::textColourId, col::ink);
    setColour (juce::ComboBox::outlineColourId, col::line2);
    setColour (juce::ComboBox::arrowColourId, col::ink);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::purple);
    setColour (juce::PopupMenu::highlightedTextColourId, col::ink);
    setColour (juce::Label::textColourId, col::ink);
    setColour (juce::TextEditor::backgroundColourId, col::cell);
    setColour (juce::TextEditor::textColourId, col::ink);
    setColour (juce::TextEditor::outlineColourId, col::line2);
    setColour (juce::TextEditor::focusedOutlineColourId, col::time);
    setColour (juce::TextEditor::highlightColourId, col::purple.withAlpha (0.5f));
    setColour (juce::CaretComponent::caretColourId, col::time);
    setColour (juce::ToggleButton::textColourId, col::ink);
    setColour (juce::TooltipWindow::backgroundColourId, col::panel);
    setColour (juce::TooltipWindow::textColourId, col::ink);
    setColour (juce::TooltipWindow::outlineColourId, col::purple);
    setColour (juce::ScrollBar::thumbColourId, col::purple);
}

void Look::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.8f);
    const float rad = juce::jmin (8.0f, r.getHeight() * 0.5f);
    const auto on = b.findColour (juce::TextButton::buttonOnColourId);
    if (b.getToggleState()) { g.setColour (on.withAlpha (down ? 0.8f : 1.0f)); g.fillRoundedRectangle (r, rad); }
    else
    {
        g.setColour (col::cell.withAlpha (0.92f)); g.fillRoundedRectangle (r, rad);
        if (over || down) { g.setColour (col::ink.withAlpha (down ? 0.16f : 0.08f)); g.fillRoundedRectangle (r, rad); }
    }
    g.setColour (b.getToggleState() ? on : (over ? col::ink.withAlpha (0.6f) : col::line2));
    g.drawRoundedRectangle (r, rad, 1.2f);
}

void Look::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour ((b.getToggleState() ? col::bg : b.findColour (juce::TextButton::textColourOffId)).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.4f));
    g.setFont (fonts().body (juce::jmin (13.5f, b.getHeight() * 0.5f), true));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1, 0.8f);
}

void Look::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    auto box = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.22f);
    if (b.getToggleState())
    {
        g.setColour (col::time); g.fillRoundedRectangle (box, 3.5f);
        juce::Path tick;
        tick.startNewSubPath (box.getX() + box.getWidth() * 0.22f, box.getCentreY());
        tick.lineTo (box.getX() + box.getWidth() * 0.43f, box.getBottom() - box.getHeight() * 0.25f);
        tick.lineTo (box.getRight() - box.getWidth() * 0.2f, box.getY() + box.getHeight() * 0.25f);
        g.setColour (col::bg);
        g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else { g.setColour (over ? col::ink : col::line2); g.drawRoundedRectangle (box, 3.5f, 1.4f); }
    g.setColour (col::ink);
    g.setFont (fonts().body (13.5f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (5.0f), juce::Justification::centredLeft);
}

void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.8f);
    g.setColour (col::cell); g.fillRoundedRectangle (r, 7.0f);
    g.setColour (col::line2); g.drawRoundedRectangle (r, 7.0f, 1.2f);
    juce::Path p;
    const float cx = (float) w - 14.0f, cy = (float) h * 0.5f;
    p.startNewSubPath (cx - 4.0f, cy - 2.0f); p.lineTo (cx, cy + 2.5f); p.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (col::ink);
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
juce::Font Look::getComboBoxFont (juce::ComboBox&) { return fonts().body (13.0f); }
void Look::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}
juce::Font Look::getPopupMenuFont() { return fonts().body (14.0f); }

void Look::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor& t)
{
    g.setColour (t.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f), 6.0f);
}
void Look::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& t)
{
    g.setColour (t.hasKeyboardFocus (true) ? col::time : col::line2);
    g.drawRoundedRectangle (juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.8f), 6.0f, 1.2f);
}
void Look::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int ts, int tsz, bool over, bool)
{
    auto r = vertical ? juce::Rectangle<int> (x, ts, w, tsz) : juce::Rectangle<int> (ts, y, tsz, h);
    g.setColour (col::purple.withAlpha (over ? 0.9f : 0.6f));
    g.fillRoundedRectangle (r.toFloat().reduced (2.5f), 3.0f);
}

void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const float cx = x + w * 0.5f, cy = y + h * 0.5f, r = juce::jmin (w, h) * 0.5f - 3.0f;
    const float ang = a0 + pos * (a1 - a0);
    const auto c = s.findColour (juce::Slider::rotarySliderFillColourId);
    juce::Path bgArc, valArc;
    bgArc.addCentredArc (cx, cy, r, r, 0.0f, a0, a1, true);
    g.setColour (col::line2);
    g.strokePath (bgArc, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (pos > 0.003f)
    {
        valArc.addCentredArc (cx, cy, r, r, 0.0f, a0, ang, true);
        g.setGradientFill (juce::ColourGradient (col::purple, cx - r, cy + r, c, cx + r, cy - r, false));
        g.strokePath (valArc, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    const float cr = r * 0.64f;
    g.setColour (col::cell2); g.fillEllipse (cx - cr, cy - cr, cr * 2, cr * 2);
    g.setColour (col::line2); g.drawEllipse (cx - cr, cy - cr, cr * 2, cr * 2, 1.3f);
    g.setColour (col::ink);
    g.drawLine (cx, cy, cx + std::sin (ang) * r * 0.56f, cy - std::cos (ang) * r * 0.56f, 2.4f);
}

//==============================================================================
Knob::Knob (YugamiProcessor& p, const juce::String& id, juce::Colour c, bool m) : prm (p.apvts.getParameter (id)), mini (m)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (180);
    slider.setColour (juce::Slider::rotarySliderFillColourId, c);
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id, slider);
    slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
    slider.setTitle (prm->getName (64));
    slider.setTooltip (prm->getName (64));
}

void Knob::resized()
{
    if (mini) slider.setBounds (getLocalBounds());
    else slider.setBounds (getWidth() / 2 - 24, 0, 48, 48);
}

void Knob::paint (juce::Graphics& g)
{
    if (mini) return;
    g.setColour (col::muted);
    g.setFont (fonts().body (12.0f));
    g.drawText (prm->getName (64), 0, 49, getWidth(), 16, juce::Justification::centred);
    g.setColour (col::ink);
    g.setFont (fonts().mono (11.5f));
    g.drawText (prm->getCurrentValueAsText(), 0, 65, getWidth(), 16, juce::Justification::centred);
}

//==============================================================================
Backdrop::Backdrop()
{
    setInterceptsMouseClicks (false, false);
    logo = juce::ImageCache::getFromMemory (BinaryData::astronaut_png, BinaryData::astronaut_pngSize);
    field = juce::Image (juce::Image::RGB, 124, 88, false);
    tick (0);
}

void Backdrop::tick (double t)
{
    time = t;
    const int w = field.getWidth(), h = field.getHeight();
    juce::Image::BitmapData bd (field, juce::Image::BitmapData::writeOnly);
    const float tt = (float) t;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            const float u = (x - w * 0.5f) / (float) h, v = (y - h * 0.48f) / (float) h;
            const float r = std::sqrt (u * u + v * v), a = std::atan2 (v, u);
            const float val = std::sin (r * 22.0f - tt * 1.3f + std::sin (a * 5.0f + tt * 0.35f) * 1.6f)
                            + 0.6f * std::sin (u * 9.0f + std::sin (v * 7.0f + tt * 0.5f) * 2.0f + tt * 0.6f);
            const float hh = 0.72f + val * 0.11f + tt * 0.025f + r * 0.35f;
            const float b = 0.30f + 0.32f * (0.5f + 0.5f * std::sin (val * 2.2f + tt * 0.8f));
            bd.setPixelColour (x, y, hue (hh, 0.95f, b));
        }
    repaint();
}

void Backdrop::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
    g.drawImage (field, getLocalBounds().toFloat());
    // a soft vignette keeps text readable
    juce::ColourGradient vg (juce::Colours::transparentBlack, getWidth() * 0.5f, getHeight() * 0.45f, col::bg.withAlpha (0.55f), 0, 0, true);
    g.setGradientFill (vg); g.fillAll();

    const float shift = (float) (time * 0.05);
    for (auto& p : panels)
    {
        g.setColour (col::panel.withAlpha (0.86f));
        g.fillRoundedRectangle (p, 14.0f);
        g.setGradientFill (rainbow (p, shift, 0.85f));
        g.drawRoundedRectangle (p.reduced (0.6f), 14.0f, 1.6f);
    }

    // header: astronaut + wavy rainbow wordmark
    if (logo.isValid())
        g.drawImage (logo, juce::Rectangle<float> (18, 10, 42, 64), juce::RectanglePlacement::centred);
    const juce::String word ("YUGAMI");
    auto f = fonts().display (44.0f);
    float x = 72.0f;
    for (int i = 0; i < word.length(); ++i)
    {
        const auto ch = word.substring (i, i + 1);
        const float cw = juce::GlyphArrangement::getStringWidth (f, ch);
        const float dy = std::sin ((float) time * 2.2f + i * 0.9f) * 4.0f;
        juce::GlyphArrangement ga; ga.addLineOfText (f, ch, x, 54.0f + dy);
        juce::Path p; ga.createPath (p);
        const float sk = std::sin ((float) time * 1.7f + i * 1.3f) * 0.12f;
        p.applyTransform (juce::AffineTransform::shear (sk, 0).translated (-sk * 54.0f, 0));
        g.setColour (hue (shift * 3 + i * 0.13f, 0.9f, 1.0f).withAlpha (0.35f));
        g.strokePath (p, juce::PathStrokeType (5.0f));
        g.setGradientFill (juce::ColourGradient (hue (shift * 3 + i * 0.13f), x, 20, hue (shift * 3 + i * 0.13f + 0.3f), x + cw, 58, false));
        g.fillPath (p);
        x += cw + 3.0f;
    }
    g.setColour (col::muted);
    g.setFont (fonts().body (12.5f));
    g.drawText (jp ("DJ KATSUNII · かつにい — 流れている音をカーブでゆがめるタイム/ボリューム・エフェクト"), 74, 60, 640, 18, juce::Justification::left);
}

//==============================================================================
void Dial::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    const float cx = r.getCentreX(), cy = r.getCentreY(), rad = r.getWidth() * 0.5f;
    g.setColour (col::cell2); g.fillEllipse (r);
    g.setColour (col::line2); g.drawEllipse (r, 1.5f);
    for (int k = 0; k < 16; ++k)
    {
        const float a = k / 16.0f * juce::MathConstants<float>::twoPi;
        g.setColour (col::ink.withAlpha (k % 4 ? 0.2f : 0.55f));
        g.drawLine (cx + std::sin (a) * (rad - 5), cy - std::cos (a) * (rad - 5), cx + std::sin (a) * rad, cy - std::cos (a) * rad, 1.2f);
    }
    if (! proc.running.load()) return;
    const float ph = proc.phaseT.load(), d = juce::jmin (1.0f, proc.delayBars.load());
    juce::Path arc; arc.addCentredArc (cx, cy, rad - 1, rad - 1, 0, 0, ph * juce::MathConstants<float>::twoPi, true);
    g.setGradientFill (juce::ColourGradient (col::time, cx, cy - rad, col::vol, cx, cy + rad, false));
    g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float a = ph * juce::MathConstants<float>::twoPi, rr = (rad - 7) * (1.0f - d * 0.8f);
    g.setColour (col::time);
    g.fillEllipse (cx + std::sin (a) * rr - 3.5f, cy - std::cos (a) * rr - 3.5f, 7, 7);
}

//==============================================================================
juce::Rectangle<float> SlotGrid::cellRect (int i) const
{
    const float cw = getWidth() / 4.0f, ch = getHeight() / 9.0f;
    return { (i % 4) * cw + 1.0f, (i / 4) * ch + 1.0f, cw - 2.0f, ch - 2.0f };
}
int SlotGrid::indexAt (juce::Point<float> p) const
{
    const int c = (int) (p.x / (getWidth() / 4.0f)), r = (int) (p.y / (getHeight() / 9.0f));
    if (c < 0 || c > 3 || r < 0 || r > 8) return -1;
    return r * 4 + c;
}
juce::String SlotGrid::getTooltipFor (int i) const
{
    auto& s = editor.proc.slot (isTime, i);
    juce::String t = juce::String (i + 1) + ". " + s.name;
    if (i < 16) t << jp ("(キー ") << (isTime ? "" : "Shift+") << kLetters[i] << jp (")");
    t << " · MIDI " << ((isTime ? 36 : 72) + i);
    return t;
}
void SlotGrid::paint (juce::Graphics& g)
{
    const int base = isTime ? editor.baseT() : editor.baseV();
    const int hot = isTime ? editor.proc.hotT.load() : editor.proc.hotV.load();
    const auto acc = isTime ? col::time : col::vol;
    for (int i = 0; i < kSlots; ++i)
    {
        auto r = cellRect (i);
        auto& s = editor.proc.slot (isTime, i);
        const bool isHot = i == hot, isBase = i == base, isEdit = editor.proc.editTime == isTime && editor.proc.editIdx == i;
        g.setColour (isHot ? acc : (i == hover ? col::cell2 : col::cell));
        g.fillRoundedRectangle (r, 4.0f);
        if (isBase && ! isHot) { g.setColour (acc); g.drawRoundedRectangle (r.reduced (0.6f), 4.0f, 1.4f); }
        if (isEdit) { g.setColour (col::sun); g.fillRect (r.getX() + 4, r.getBottom() - 2.5f, r.getWidth() - 8, 2.0f); }
        g.setColour (isHot ? col::bg : (isBase ? acc : (s.isEmpty() ? col::muted.withAlpha (0.7f) : col::ink)));
        g.setFont (fonts().body (11.5f, isBase || isHot));
        g.drawFittedText (s.name, r.reduced (5, 0).toNearestInt(), juce::Justification::centredLeft, 1, 0.75f);
    }
}
void SlotGrid::mouseMove (const juce::MouseEvent& e)
{
    const int i = indexAt (e.position);
    if (i != hover) { hover = i; repaint(); setTooltip (i >= 0 ? getTooltipFor (i) : juce::String()); }
}
void SlotGrid::mouseDown (const juce::MouseEvent& e)
{
    const int i = indexAt (e.position);
    if (i >= 0) editor.selectSlot (isTime, i);
}

//==============================================================================
static constexpr float PL = 46, PR = 14, PT = 14, PB = 26;
float EnvEditor::X (float x) const { return PL + x * ((float) getWidth() - PL - PR); }
float EnvEditor::Y (float y) const { return PT + (editor.proc.editTime ? y : 1.0f - y) * ((float) getHeight() - PT - PB); }
float EnvEditor::ux (float px) const { return juce::jlimit (0.0f, 1.0f, (px - PL) / ((float) getWidth() - PL - PR)); }
float EnvEditor::uy (float py) const { const float u = juce::jlimit (0.0f, 1.0f, (py - PT) / ((float) getHeight() - PT - PB)); return editor.proc.editTime ? u : 1.0f - u; }
float EnvEditor::snapX (float x) const
{
    if (snap <= 0) return x;
    const float st = snap / editor.editSlot().len;
    return juce::jlimit (0.0f, 1.0f, std::round (x / st) * st);
}
float EnvEditor::snapY (float y) const
{
    if (snap <= 0 || ! editor.proc.editTime) return y;
    const float st = snap / 4.0f;
    return juce::jlimit (0.0f, 1.0f, std::round (y / st) * st);
}

EnvEditor::Hit EnvEditor::hitTest (juce::Point<float> p) const
{
    auto& c = const_cast<YugamiEditor&> (editor).editSlot().nodes;
    for (int i = (int) c.size() - 1; i >= 0; --i)
        if (p.getDistanceFrom ({ X (c[(size_t) i].x), Y (c[(size_t) i].y) }) < 9.0f) return { 1, i };
    for (size_t i = 0; i + 1 < c.size(); ++i)
    {
        if (c[i + 1].x - c[i].x < 0.004f || std::abs (c[i + 1].y - c[i].y) < 1e-6f) continue;
        const float xm = (c[i].x + c[i + 1].x) * 0.5f;
        if (p.getDistanceFrom ({ X (xm), Y (evalCurve (c, xm)) }) < 8.0f) return { 2, (int) i };
    }
    return {};
}

void EnvEditor::paint (juce::Graphics& g)
{
    auto& proc = editor.proc;
    const bool tk = proc.editTime;
    auto& s = editor.editSlot();
    const auto acc = tk ? col::time : col::vol;
    const float W = (float) getWidth(), H = (float) getHeight(), L = s.len;
    auto area = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (col::panel.brighter (0.06f), 0, 0, col::bg, 0, H, false));
    g.fillRoundedRectangle (area, 10.0f);
    g.setColour (col::line2); g.drawRoundedRectangle (area.reduced (0.5f), 10.0f, 1.0f);

    // vertical grid
    if (snap > 0)
    {
        g.setColour (col::line);
        for (float b = 0; b <= L + 1e-4f; b += snap) g.drawVerticalLine ((int) X (b / L), PT, H - PB);
    }
    g.setFont (fonts().mono (11.0f));
    for (int b = 0; b <= (int) L; ++b)
    {
        g.setColour (b % 4 == 0 ? col::line2.withAlpha (0.5f) : col::line2.withAlpha (0.3f));
        g.drawVerticalLine ((int) X (b / L), PT, H - PB);
        if (b < (int) L)
        {
            g.setColour (col::muted);
            g.drawText (b % 4 == 0 ? jp ("小節") + juce::String (b / 4 + 1) : jp ("拍") + juce::String (b % 4 + 1),
                        juce::Rectangle<float> (X ((b + 0.5f) / L) - 40, H - PB + 4, 80, 16), juce::Justification::centred);
        }
    }
    // horizontal grid + axis
    for (int k = 0; k <= 4; ++k)
    {
        const float y = Y (k / 4.0f);
        g.setColour (k == 0 || k == 4 ? col::line2.withAlpha (0.5f) : col::line);
        g.drawHorizontalLine ((int) y, PL, W - PR);
        g.setColour (col::muted);
        const juce::String lbl = tk ? (k == 0 ? jp ("いま") : "-" + juce::String (k) + jp ("拍")) : juce::String (k * 25) + "%";
        g.drawText (lbl, juce::Rectangle<float> (0, y - 8, PL - 6, 16), juce::Justification::centredRight);
    }
    if (tk)
    {
        const float fz = juce::jmin (1.0f, L / 4.0f);
        juce::Path dl; dl.startNewSubPath (X (0), Y (0)); dl.lineTo (X (juce::jmin (1.0f, 1.0f / fz)), Y (juce::jmin (1.0f, fz)));
        juce::Path dash; const float dashes[] = { 4, 5 };
        juce::PathStrokeType (1.0f).createDashedStroke (dash, dl, dashes, 2);
        g.setColour (col::ink.withAlpha (0.22f)); g.fillPath (dash);
        g.setColour (col::muted);
        g.setFont (fonts().body (11.5f));
        g.drawText (jp ("点線と同じ傾き=止まる / 急な下り=逆再生 / 縦の線=ジャンプ"), juce::Rectangle<float> (PL + 8, H - PB - 22, 420, 16), juce::Justification::left);
    }

    // the other envelope in use, faintly
    {
        const bool otherTime = ! tk;
        const int hot = otherTime ? proc.hotT.load() : proc.hotV.load();
        const int idx = hot >= 0 ? hot : (otherTime ? editor.baseT() : editor.baseV());
        auto& o = proc.slot (otherTime, juce::jlimit (0, kSlots - 1, idx));
        juce::Path p;
        for (int i = 0; i <= 300; ++i)
        {
            const float x = i / 300.0f, ph = std::fmod (x * L / o.len, 1.0f), v = evalCurve (o.nodes, ph);
            const float yy = PT + (tk ? 1.0f - v : juce::jlimit (0.0f, 1.0f, v)) * (H - PT - PB);
            if (i == 0) p.startNewSubPath (X (x), yy); else p.lineTo (X (x), yy);
        }
        g.setColour ((otherTime ? col::time : col::vol).withAlpha (0.33f));
        g.strokePath (p, juce::PathStrokeType (1.4f));
    }

    // the curve
    auto& c = s.nodes;
    juce::Path path;
    const int steps = juce::jmax (600, (int) W * 2);
    for (int i = 0; i <= steps; ++i)
    {
        const float x = (float) i / (float) steps, y = evalCurve (c, x);
        if (i == 0) path.startNewSubPath (X (x), Y (y)); else path.lineTo (X (x), Y (y));
    }
    juce::Path fill (path);
    fill.lineTo (X (1), tk ? PT : H - PB); fill.lineTo (X (0), tk ? PT : H - PB); fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (acc.withAlpha (0.32f), 0, tk ? PT : H - PB, col::purple.withAlpha (0.10f), 0, tk ? H - PB : PT, false));
    g.fillPath (fill);
    g.setColour (acc.withAlpha (0.35f)); g.strokePath (path, juce::PathStrokeType (5.0f));
    g.setColour (acc); g.strokePath (path, juce::PathStrokeType (2.0f));
    for (size_t i = 0; i + 1 < c.size(); ++i)
    {
        if (c[i + 1].x - c[i].x < 0.004f || std::abs (c[i + 1].y - c[i].y) < 1e-6f) continue;
        const float xm = (c[i].x + c[i + 1].x) * 0.5f, px = X (xm), py = Y (evalCurve (c, xm));
        juce::Path d; d.startNewSubPath (px, py - 4.5f); d.lineTo (px + 4.5f, py); d.lineTo (px, py + 4.5f); d.lineTo (px - 4.5f, py); d.closeSubPath();
        g.setColour (col::panel); g.fillPath (d); g.setColour (col::sun); g.strokePath (d, juce::PathStrokeType (1.2f));
    }
    if (c.size() <= 160)
        for (auto& q : c)
        {
            g.setColour (col::panel); g.fillEllipse (X (q.x) - 4.5f, Y (q.y) - 4.5f, 9, 9);
            g.setColour (acc); g.drawEllipse (X (q.x) - 4.5f, Y (q.y) - 4.5f, 9, 9, 1.7f);
        }

    // playhead
    if (proc.running.load())
    {
        const int hot = tk ? proc.hotT.load() : proc.hotV.load();
        const int inUse = hot >= 0 ? hot : (tk ? editor.baseT() : editor.baseV());
        if (inUse == proc.editIdx)
        {
            const float ph = tk ? proc.phaseT.load() : proc.phaseV.load();
            g.setColour (col::sun); g.drawLine (X (ph), PT, X (ph), H - PB, 2.0f);
        }
    }

    // hover readout
    if (hoverX >= 0)
    {
        const float x = hoverX, y = evalCurve (c, x);
        juce::String txt;
        if (tk)
        {
            const float dx = 0.004f, a = juce::jmax (0.0f, x - dx), b = juce::jmin (1.0f, x + dx);
            const float sl = (evalCurve (c, b) - evalCurve (c, a)) / (b - a), sp = 1.0f - sl * 4.0f / L;
            txt = juce::String (y * 4.0f, 2) + jp ("拍前を再生 · 速さ ") + (std::abs (sp) < 0.02f ? jp ("0(止まる)") : juce::String (sp, 2) + "x") + (sp < -0.02f ? jp ("(逆再生)") : "");
        }
        else txt = jp ("音量 ") + juce::String (juce::roundToInt (y * 100)) + "%";
        auto f = fonts().body (12.5f, true);
        const float tw = juce::GlyphArrangement::getStringWidth (f, txt) + 14;
        const float bx = juce::jlimit (PL, W - PR - tw, hoverPx + 12);
        g.setColour (col::bg.withAlpha (0.85f)); g.fillRoundedRectangle (bx, PT + 6, tw, 22, 5);
        g.setColour (col::ink); g.setFont (f); g.drawText (txt, juce::Rectangle<float> (bx + 7, PT + 6, tw, 22), juce::Justification::centredLeft);
    }
    g.setColour (col::ink.withAlpha (0.06f));
    g.setFont (fonts().display (46.0f));
    g.drawText ("YUGAMI", juce::Rectangle<float> (W - PR - 320, H - PB - 64, 310, 56), juce::Justification::right);
}

void EnvEditor::mouseMove (const juce::MouseEvent& e)
{
    hoverX = ux (e.position.x); hoverPx = e.position.x;
    const auto h = hitTest (e.position);
    setMouseCursor (h.type == 1 ? juce::MouseCursor::DraggingHandCursor : h.type == 2 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::CrosshairCursor);
    repaint();
}
void EnvEditor::mouseDown (const juce::MouseEvent& e)
{
    drag = hitTest (e.position);
    if (drag.type != 0) { dragPy0 = e.position.y; dragB0 = editor.editSlot().nodes[(size_t) drag.i].b; }
}
void EnvEditor::mouseDrag (const juce::MouseEvent& e)
{
    hoverX = ux (e.position.x); hoverPx = e.position.x;
    if (drag.type == 0) return;
    auto& c = editor.editSlot().nodes;
    const int i = drag.i;
    if (i < 0 || i >= (int) c.size()) return;
    if (drag.type == 1)
    {
        float x = snapX (ux (e.position.x));
        if (i == 0) x = 0; else if (i == (int) c.size() - 1) x = 1; else x = juce::jlimit (c[(size_t) i - 1].x, c[(size_t) i + 1].x, x);
        c[(size_t) i].x = x; c[(size_t) i].y = snapY (uy (e.position.y));
    }
    else
    {
        const bool tk = editor.proc.editTime;
        const float up = (tk ? -1.0f : 1.0f) * (c[(size_t) i + 1].y >= c[(size_t) i].y ? 1.0f : -1.0f);
        c[(size_t) i].b = juce::jlimit (-1.0f, 1.0f, dragB0 + (e.position.y - dragPy0) / 120.0f * (tk ? 1.0f : -1.0f) * up);
    }
    const auto now = juce::Time::getMillisecondCounter();
    if (now - lastCommit > 40) { lastCommit = now; editor.edited (true); }
    repaint();
}
void EnvEditor::mouseUp (const juce::MouseEvent&)
{
    if (drag.type != 0) { drag = {}; editor.edited (true); }
}
void EnvEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    auto& s = editor.editSlot();
    auto& c = s.nodes;
    const auto h = hitTest (e.position);
    if (h.type == 1) { if (h.i > 0 && h.i < (int) c.size() - 1) c.erase (c.begin() + h.i); }
    else
    {
        const float x = snapX (ux (e.position.x));
        auto it = std::find_if (c.begin(), c.end(), [x] (const Node& q) { return q.x > x; });
        c.insert (it, { x, snapY (uy (e.position.y)), 0 });
    }
    if (s.name.startsWith (jp ("空き"))) s.name = jp ("自作 ") + juce::String (editor.proc.editIdx + 1);
    editor.edited (true);
    editor.refreshAll();
}

//==============================================================================
Library::Library (YugamiEditor& e) : editor (e), grid (*this)
{
    saveBtn.setButtonText (jp ("★ いまの形を保存"));
    closeBtn.setButtonText (jp ("閉じる"));
    addAndMakeVisible (saveBtn); addAndMakeVisible (closeBtn);
    saveBtn.onClick = [this] { editor.saveMine(); };
    closeBtn.onClick = [this] { editor.content.setLibraryOpen (false); };
    view.setViewedComponent (&grid, false);
    view.setScrollBarsShown (true, false);
    view.setScrollBarThickness (10);
    addAndMakeVisible (view);
    cat = jp ("全部");
}

void Library::rebuild()
{
    const bool tk = editor.proc.editTime;
    const auto& f = factory();
    juce::StringArray cats { jp ("全部") };
    for (auto& c : (tk ? f.categoriesTime : f.categoriesVol)) cats.add (c);
    cats.add (jp ("マイプリセット"));
    if (! cats.contains (cat)) cat = jp ("全部");
    tabs.clear();
    for (auto& c : cats)
    {
        auto* b = tabs.add (new juce::TextButton (c));
        b->setToggleState (c == cat, juce::dontSendNotification);
        b->setColour (juce::TextButton::buttonOnColourId, tk ? col::time : col::vol);
        b->onClick = [this, c] { cat = c; rebuild(); };
        addAndMakeVisible (b);
    }
    items.clear();
    if (cat == jp ("マイプリセット"))
    {
        auto& m = tk ? editor.proc.mineTime : editor.proc.mineVol;
        for (int i = 0; i < (int) m.size(); ++i) items.push_back ({ &m[(size_t) i], i });
    }
    else
        for (auto& p : (tk ? f.time : f.vol))
            if (cat == jp ("全部") || p.cat == cat) items.push_back ({ &p, -1 });
    resized();
    repaint(); grid.repaint();
}

void Library::resized()
{
    float x = 10;
    for (auto* b : tabs)
    {
        const int w = (int) juce::GlyphArrangement::getStringWidth (fonts().body (13.0f, true), b->getButtonText()) + 22;
        b->setBounds ((int) x, 8, w, 26); x += (float) w + 4;
    }
    closeBtn.setBounds (getWidth() - 76, 8, 66, 26);
    saveBtn.setBounds (closeBtn.getX() - 156, 8, 150, 26);
    view.setBounds (6, 42, getWidth() - 12, getHeight() - 48);
    const int rows = ((int) items.size() + grid.cols() - 1) / juce::jmax (1, grid.cols());
    grid.setSize (view.getWidth() - 12, juce::jmax (view.getHeight(), rows * 66 + 8));
}

void Library::paint (juce::Graphics& g)
{
    g.setColour (col::panel.withAlpha (0.97f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);
    g.setColour (editor.proc.editTime ? col::time : col::vol);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.6f), 10.0f, 1.3f);
    if (items.empty())
    {
        g.setColour (col::muted); g.setFont (fonts().body (13.5f));
        g.drawText (jp ("まだありません。形を作って「★ 保存」を押すとここに入ります。"), getLocalBounds().withTrimmedTop (60).reduced (16, 0), juce::Justification::centredTop);
    }
}

int Library::Grid::cols() const { return juce::jmax (1, getWidth() / 128); }
juce::Rectangle<float> Library::Grid::cardRect (int i) const
{
    const int c = cols(); const float cw = (float) getWidth() / (float) c;
    return { (i % c) * cw + 3, (float) (i / c) * 66.0f + 4, cw - 6, 60 };
}
juce::Rectangle<float> Library::Grid::delRect (int i) const { auto r = cardRect (i); return { r.getRight() - 21, r.getY() + 3, 18, 16 }; }

void Library::Grid::paint (juce::Graphics& g)
{
    const bool tk = lib.editor.proc.editTime;
    const auto acc = tk ? col::time : col::vol;
    for (int i = 0; i < (int) lib.items.size(); ++i)
    {
        auto r = cardRect (i);
        const auto& p = *lib.items[(size_t) i].p;
        g.setColour (i == hover ? col::cell2 : col::cell); g.fillRoundedRectangle (r, 7);
        g.setColour (i == hover ? acc : col::line2); g.drawRoundedRectangle (r.reduced (0.5f), 7, 1.0f);
        auto th = r.reduced (6, 5).removeFromTop (34);
        float top = 1.0f;
        if (tk) { top = 0.25f; for (auto& q : p.nodes) top = juce::jmax (top, q.y); }
        auto Yv = [&] (float v) { return tk ? th.getY() + v / top * th.getHeight() : th.getBottom() - v * th.getHeight(); };
        juce::Path path;
        const int N = juce::jmax (120, (int) th.getWidth() * 2);
        for (int k = 0; k <= N; ++k)
        {
            const float x = (float) k / N, y = evalCurve (p.nodes, x);
            if (k == 0) path.startNewSubPath (th.getX(), Yv (y)); else path.lineTo (th.getX() + x * th.getWidth(), Yv (y));
        }
        juce::Path fill (path);
        fill.lineTo (th.getRight(), tk ? th.getY() : th.getBottom()); fill.lineTo (th.getX(), tk ? th.getY() : th.getBottom()); fill.closeSubPath();
        g.setColour (acc.withAlpha (0.18f)); g.fillPath (fill);
        g.setColour (acc); g.strokePath (path, juce::PathStrokeType (1.3f));
        g.setColour (col::ink); g.setFont (fonts().body (11.5f));
        g.drawFittedText (p.name, r.reduced (7, 0).withTop (r.getBottom() - 18).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        if (lib.items[(size_t) i].mineIndex >= 0)
        {
            auto d = delRect (i);
            g.setColour (col::bg); g.fillRoundedRectangle (d, 4);
            g.setColour (col::danger); g.setFont (fonts().body (11.0f, true)); g.drawText (juce::String::fromUTF8 ("×"), d, juce::Justification::centred);
        }
    }
}
void Library::Grid::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (int i = 0; i < (int) lib.items.size(); ++i) if (cardRect (i).contains (e.position)) h = i;
    if (h != hover) { hover = h; repaint(); }
}
void Library::Grid::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < (int) lib.items.size(); ++i)
    {
        if (! cardRect (i).contains (e.position)) continue;
        const auto it = lib.items[(size_t) i];
        if (it.mineIndex >= 0 && delRect (i).contains (e.position))
        {
            const auto name = it.p->name;
            lib.editor.proc.deleteMine (lib.editor.proc.editTime, it.mineIndex);
            lib.rebuild();
            lib.editor.setStatus (jp ("「") + name + jp ("」をマイプリセットから削除しました。"));
            return;
        }
        lib.editor.loadPreset (*it.p);
        return;
    }
}

//==============================================================================
Content::Content (YugamiEditor& e)
    : editor (e), timeGrid (e, true), volGrid (e, false), env (e), library (e), dial (e.proc)
{
    auto& p = e.proc;
    addAndMakeVisible (backdrop);
    for (auto* b : { &exportBtn, &importBtn, &resetBtn, &pgPrev, &pgNext, &pgAdd, &pgDel, &holdBtn, &libBtn, &saveBtn, &copyBtn, &pasteBtn })
        addAndMakeVisible (b);
    exportBtn.setButtonText (jp ("セットを書き出し"));
    importBtn.setButtonText (jp ("読み込み"));
    resetBtn.setButtonText (jp ("標準に戻す"));
    resetBtn.setColour (juce::TextButton::textColourOffId, col::danger);
    pgDel.setButtonText (jp ("削除"));
    pgDel.setColour (juce::TextButton::textColourOffId, col::danger);
    libBtn.setButtonText (jp ("プリセット一覧"));
    libBtn.setClickingTogglesState (false);
    saveBtn.setButtonText (jp ("★ 保存"));
    copyBtn.setButtonText (jp ("コピー"));
    pasteBtn.setButtonText (jp ("貼り付け"));
    holdBtn.setClickingTogglesState (true);
    holdBtn.setTooltip (jp ("オンにすると、キーやMIDIで呼んだスロットが離しても残ります"));
    holdAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "hold", holdBtn);
    limiter.setButtonText (jp ("リミッター"));
    addAndMakeVisible (limiter);
    limAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "limiter", limiter);
    pgAdd.setTooltip (jp ("空のページを追加"));

    for (auto* l : { &pgNum, &status, &timeLbl, &volLbl, &kindLbl }) addAndMakeVisible (l);
    pgNum.setJustificationType (juce::Justification::centred);
    pgNum.setFont (fonts().mono (12.0f)); pgNum.setColour (juce::Label::textColourId, col::muted);
    status.setFont (fonts().body (13.0f)); status.setColour (juce::Label::textColourId, col::muted);
    status.setMinimumHorizontalScale (0.7f);
    timeLbl.setText (jp ("タイム"), juce::dontSendNotification); timeLbl.setFont (fonts().display (19.0f)); timeLbl.setColour (juce::Label::textColourId, col::time);
    volLbl.setText (jp ("ボリューム"), juce::dontSendNotification); volLbl.setFont (fonts().display (19.0f)); volLbl.setColour (juce::Label::textColourId, col::vol);
    kindLbl.setFont (fonts().display (16.0f));

    addAndMakeVisible (pgName);
    pgName.setJustification (juce::Justification::centred);
    pgName.setFont (fonts().body (13.5f, true));
    pgName.setInputRestrictions (14);
    pgName.onTextChange = [this] { auto& pg = editor.proc.pages[(size_t) editor.proc.page]; pg.name = pgName.getText().isEmpty() ? jp ("ページ") : pgName.getText(); };
    pgName.onReturnKey = [this] { editor.grabKeyboardFocus(); };

    addAndMakeVisible (slotName);
    slotName.setFont (fonts().body (14.0f, true));
    slotName.setInputRestrictions (12);
    slotName.onTextChange = [this] { auto& s = editor.editSlot(); s.name = slotName.getText().isEmpty() ? jp ("名前なし") : slotName.getText(); timeGrid.repaint(); volGrid.repaint(); };
    slotName.onReturnKey = [this] { editor.grabKeyboardFocus(); };

    addAndMakeVisible (lenBox);
    lenBox.addItem (jp ("1拍"), 1); lenBox.addItem (jp ("2拍"), 2); lenBox.addItem (jp ("1小節"), 4); lenBox.addItem (jp ("2小節"), 8);
    lenBox.onChange = [this] { const int id = lenBox.getSelectedId(); if (id > 0 && (float) id != editor.editSlot().len) { editor.editSlot().len = (float) id; editor.edited(); } };
    addAndMakeVisible (snapBox);
    const char* snaps[] = { "オフ", "1拍", "1/2拍", "1/4拍", "1/8拍", "3連" };
    for (int i = 0; i < 6; ++i) snapBox.addItem (jp (snaps[i]), i + 1);
    snapBox.setSelectedId (4, juce::dontSendNotification);
    snapBox.onChange = [this] { const float v[] = { 0, 1, 0.5f, 0.25f, 0.125f, 1.0f / 3.0f }; env.snap = v[juce::jlimit (0, 5, snapBox.getSelectedId() - 1)]; env.repaint(); };
    addAndMakeVisible (trigBox);
    trigBox.addItemList (p.apvts.getParameter ("trigger")->getAllValueStrings(), 1);
    trigAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "trigger", trigBox);

    addAndMakeVisible (timeGrid); addAndMakeVisible (volGrid); addAndMakeVisible (env); addAndMakeVisible (dial);
    addChildComponent (library);

    tMix = knobs.add (new Knob (p, "tMix", col::time, true));
    vMix = knobs.add (new Knob (p, "vMix", col::vol, true));
    out = knobs.add (new Knob (p, "output", col::sun, true));
    knobs.add (new Knob (p, "attack", col::vol));
    knobs.add (new Knob (p, "release", col::vol));
    knobs.add (new Knob (p, "smooth", col::time));
    for (auto* k : knobs) addAndMakeVisible (k);
    out->slider.onValueChange = [this] { repaint (meterR.expanded (120, 20).toNearestInt()); };

    pgPrev.onClick = [this] { editor.proc.setPage ((editor.proc.page + (int) editor.proc.pages.size() - 1) % (int) editor.proc.pages.size()); editor.refreshAll(); };
    pgNext.onClick = [this] { editor.proc.setPage ((editor.proc.page + 1) % (int) editor.proc.pages.size()); editor.refreshAll(); };
    pgAdd.onClick = [this] {
        if (editor.proc.addPage()) { editor.refreshAll(); editor.setStatus (jp ("空のページを追加しました。プリセット一覧から形を入れたり、自分で描いたりできます。")); }
        else editor.setStatus (jp ("ページは12枚までです。"), true); };
    pgDel.onClick = [this] {
        if (editor.proc.pages.size() < 2) return;
        const auto now = juce::Time::getMillisecondCounter();
        if (now - editor.delArm > 3000) { editor.delArm = now; pgDel.setButtonText (jp ("本当に?")); return; }
        editor.delArm = 0; pgDel.setButtonText (jp ("削除"));
        const auto name = editor.proc.pages[(size_t) editor.proc.page].name;
        editor.proc.deletePage(); editor.refreshAll(); editor.setStatus (jp ("ページ「") + name + jp ("」を削除しました。")); };
    libBtn.onClick = [this] { setLibraryOpen (! library.isVisible()); };
    saveBtn.onClick = [this] { editor.saveMine(); };
    copyBtn.onClick = [this] { editor.clip = editor.editSlot(); editor.clipTime = editor.proc.editTime; editor.setStatus (jp ("このスロットをコピーしました。別のスロットを選んで「貼り付け」できます。")); };
    pasteBtn.onClick = [this] {
        if (! editor.clip || editor.clipTime != editor.proc.editTime)
        { editor.setStatus (editor.proc.editTime ? jp ("タイムのスロットをコピーしてから貼り付けてください。") : jp ("ボリュームのスロットをコピーしてから貼り付けてください。"), true); return; }
        editor.editSlot() = *editor.clip; editor.edited(); editor.refreshAll(); };
    exportBtn.onClick = [this] {
        editor.chooser = std::make_unique<juce::FileChooser> (jp ("YUGAMIのセットを書き出し"), juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("YUGAMI-set.json"), "*.json");
        editor.chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc) {
            auto f = fc.getResult(); if (f == juce::File()) return;
            if (! f.hasFileExtension ("json")) f = f.withFileExtension ("json");
            if (editor.proc.exportSet (f)) editor.setStatus (jp ("全") + juce::String ((int) editor.proc.pages.size()) + jp ("ページとマイプリセットを「") + f.getFileName() + jp ("」に書き出しました。"));
            else editor.setStatus (jp ("書き出せませんでした。"), true); }); };
    importBtn.onClick = [this] {
        editor.chooser = std::make_unique<juce::FileChooser> (jp ("YUGAMIのセットを読み込み"), juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.json");
        editor.chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc) {
            auto f = fc.getResult(); if (! f.existsAsFile()) return;
            juce::String err;
            if (editor.proc.importSet (f, err)) { editor.refreshAll(); editor.setStatus (jp ("「") + f.getFileName() + jp ("」を読み込みました(") + juce::String ((int) editor.proc.pages.size()) + jp ("ページ)。")); }
            else editor.setStatus (err, true); }); };
    resetBtn.onClick = [this] {
        const auto now = juce::Time::getMillisecondCounter();
        if (now - editor.resetArm > 3000) { editor.resetArm = now; resetBtn.setButtonText (jp ("本当に戻す")); return; }
        editor.resetArm = 0; resetBtn.setButtonText (jp ("標準に戻す"));
        editor.proc.resetFactory(); editor.refreshAll(); editor.setStatus (jp ("ページを標準に戻しました(マイプリセットは残っています)。")); };

    rebuildTools();
}

void Content::rebuildTools()
{
    tools.clear();
    const bool tk = editor.proc.editTime;
    toolsTime = tk;
    for (auto& t : (tk ? timeTools() : volTools()))
    {
        auto* b = tools.add (new juce::TextButton (t.name));
        auto make = t.make; auto name = t.name;
        b->onClick = [this, make, name] {
            auto& s = editor.editSlot();
            s.nodes = make (s.len);
            if (s.name.startsWith (jp ("空き")) && name != jp ("リセット")) s.name = name;
            editor.edited(); editor.refreshAll(); };
        addAndMakeVisible (b);
    }
    resized();
}

void Content::setLibraryOpen (bool open)
{
    library.setVisible (open);
    env.setVisible (! open);
    libBtn.setToggleState (open, juce::dontSendNotification);
    libBtn.setColour (juce::TextButton::buttonOnColourId, editor.proc.editTime ? col::time : col::vol);
    if (open) library.rebuild();
}

void Content::refreshPager()
{
    auto& p = editor.proc;
    pgNum.setText (juce::String (p.page + 1) + "/" + juce::String ((int) p.pages.size()), juce::dontSendNotification);
    if (! pgName.hasKeyboardFocus (true)) pgName.setText (p.pages[(size_t) p.page].name, false);
    pgDel.setEnabled (p.pages.size() > 1);
}

void Content::refreshEditTarget()
{
    auto& p = editor.proc;
    const bool tk = p.editTime;
    kindLbl.setText (tk ? jp ("タイム:") : jp ("ボリューム:"), juce::dontSendNotification);
    kindLbl.setColour (juce::Label::textColourId, tk ? col::time : col::vol);
    if (! slotName.hasKeyboardFocus (true)) slotName.setText (editor.editSlot().name, false);
    lenBox.setSelectedId ((int) editor.editSlot().len, juce::dontSendNotification);
    saveBtn.setTooltip (jp ("この形をマイプリセットに保存"));
}

void Content::resized()
{
    backdrop.setBounds (getLocalBounds());
    headerR = { 0, 0, (float) W, 82 };
    stripR = { 16, 88, (float) W - 32, 64 };
    railR = { 16, 164, 376, 700 };
    mainR = { 404, 164, (float) W - 420, 700 };
    backdrop.panels = { stripR, railR, mainR };

    resetBtn.setBounds (W - 16 - 104, 28, 104, 28);
    importBtn.setBounds (resetBtn.getX() - 92, 28, 86, 28);
    exportBtn.setBounds (importBtn.getX() - 140, 28, 134, 28);

    dial.setBounds (26, 94, 52, 52);
    status.setBounds (88, 100, 720, 40);
    meterR = { 836, 116, 110, 8 };
    limiter.setBounds (960, 104, 104, 32);
    out->setBounds (1080, 100, 40, 40);

    pgPrev.setBounds (28, 176, 30, 26);
    pgNum.setBounds (60, 176, 40, 26);
    pgName.setBounds (102, 176, 150, 26);
    pgNext.setBounds (256, 176, 30, 26);
    pgAdd.setBounds (290, 176, 30, 26);
    pgDel.setBounds (324, 176, 56, 26);

    tMix->setBounds (28, 210, 36, 36);
    timeLbl.setBounds (68, 212, 140, 32);
    holdBtn.setBounds (316, 214, 64, 26);
    timeGrid.setBounds (24, 250, 360, 216);
    vMix->setBounds (28, 472, 36, 36);
    volLbl.setBounds (68, 474, 160, 32);
    volGrid.setBounds (24, 512, 360, 216);
    for (int k = 0; k < 3; ++k) knobs[3 + k]->setBounds (34 + k * 114, 750, 100, 84);

    int x = 416;
    for (auto* b : tools)
    {
        const int w = (int) juce::GlyphArrangement::getStringWidth (fonts().body (13.0f, true), b->getButtonText()) + 24;
        b->setBounds (x, 176, w, 28); x += w + 5;
    }
    x = 416;
    for (auto* b : { &libBtn, &saveBtn, &copyBtn, &pasteBtn })
    {
        const int w = (int) juce::GlyphArrangement::getStringWidth (fonts().body (13.0f, true), b->getButtonText()) + 26;
        b->setBounds (x, 210, w, 28); x += w + 5;
    }
    env.setBounds (416, 246, W - 428 - 16, 556);
    library.setBounds (env.getBounds());
    kindLbl.setBounds (418, 814, 108, 36);
    slotName.setBounds (528, 818, 176, 28);
    lenBox.setBounds (764, 818, 92, 28);
    snapBox.setBounds (940, 818, 92, 28);
    trigBox.setBounds (1088, 818, W - 16 - 12 - 1088, 28);
}

void Content::paint (juce::Graphics&) {}

void Content::paintOverChildren (juce::Graphics& g)
{
    g.setColour (col::muted);
    g.setFont (fonts().body (12.0f, true));
    g.drawText (jp ("長さ"), 716, 818, 44, 28, juce::Justification::centredRight);
    g.drawText (jp ("スナップ"), 870, 818, 66, 28, juce::Justification::centredRight);
    g.drawText (jp ("トリガー"), 1032, 818, 52, 28, juce::Justification::centredRight);
    g.drawText (jp ("出力"), 1122, 98, 60, 18, juce::Justification::left);
    g.setColour (col::ink); g.setFont (fonts().mono (11.5f));
    g.drawText (editor.proc.apvts.getParameter ("output")->getCurrentValueAsText(), 1122, 118, 96, 18, juce::Justification::left);
    // meter
    g.setColour (col::cell2); g.fillRoundedRectangle (meterR, 4);
    const float pk = juce::jlimit (0.0f, 1.0f, editor.proc.peak.load());
    g.setGradientFill (juce::ColourGradient (col::time, meterR.getX(), 0, col::vol, meterR.getRight(), 0, false));
    g.fillRoundedRectangle (meterR.withWidth (meterR.getWidth() * pk), 4);
    g.setColour (col::muted); g.setFont (fonts().body (11.0f));
    g.drawText (jp ("出力レベル"), meterR.translated (0, -18).withHeight (16), juce::Justification::left);
    // footer hint
    g.setColour (col::muted.withAlpha (0.85f)); g.setFont (fonts().body (11.5f));
    g.drawText (jp ("キー A〜P(AWSEDFTGYHUJKOLP)でタイム、Shift+キーでボリューム"),
                juce::Rectangle<float> (24, 836, 368, 26), juce::Justification::centred);
}

//==============================================================================
YugamiEditor::YugamiEditor (YugamiProcessor& p) : AudioProcessorEditor (&p), proc (p), content (*this)
{
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    setWantsKeyboardFocus (true);
    setResizable (true, true);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) Content::W / Content::H);
    setResizeLimits (744, 528, 1860, 1320);
    setSize (1054, 748);
    refreshAll();
    setStatus (jp ("FLを再生すると、流れている音にかかります。いま「") + proc.slot (true, baseT()).name + jp ("」。左のスロットをクリックで切り替え、◀ ▶ でページ切り替え。"));
    startTime = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

YugamiEditor::~YugamiEditor()
{
    stopTimer();
    for (int k = 0; k < 16; ++k) if (keyDown[(size_t) k]) proc.pushTrigger (keyDown[(size_t) k] == 1, k, false);
    setLookAndFeel (nullptr);
}

void YugamiEditor::paint (juce::Graphics& g) { g.fillAll (col::bg); }
void YugamiEditor::resized()
{
    content.setBounds (0, 0, Content::W, Content::H);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) Content::W));
}

void YugamiEditor::setStatus (const juce::String& s, bool error)
{
    content.status.setText (s, juce::dontSendNotification);
    content.status.setColour (juce::Label::textColourId, error ? col::danger : col::muted);
    statusTimeout = 30 * 12;
}

void YugamiEditor::refreshAll()
{
    if (content.toolsTime != proc.editTime || content.tools.isEmpty()) content.rebuildTools();
    content.refreshPager();
    content.refreshEditTarget();
    content.timeGrid.repaint(); content.volGrid.repaint(); content.env.repaint();
    if (content.library.isVisible()) content.library.rebuild();
    content.libBtn.setColour (juce::TextButton::buttonOnColourId, proc.editTime ? col::time : col::vol);
}

void YugamiEditor::selectSlot (bool time, int i)
{
    proc.setParam (time ? "tSlot" : "vSlot", (float) (i + 1));
    proc.editTime = time; proc.editIdx = i;
    refreshAll();
}

void YugamiEditor::edited (bool)
{
    proc.slotEdited();
    content.timeGrid.repaint(); content.volGrid.repaint(); content.env.repaint();
}

void YugamiEditor::loadPreset (const Preset& p)
{
    auto& s = editSlot();
    s = p.toSlot();
    s.name = s.name.substring (0, 12);
    edited(); refreshAll();
    setStatus (jp ("「") + p.name + jp ("」を") + (proc.editTime ? jp ("タイム") : jp ("ボリューム")) + jp ("のスロット") + juce::String (proc.editIdx + 1) + jp ("に入れました。"));
}

void YugamiEditor::saveMine()
{
    juce::String name;
    proc.saveMine (proc.editTime, editSlot(), name);
    content.library.cat = jp ("マイプリセット");
    content.setLibraryOpen (true);
    setStatus (jp ("「") + name + jp ("」をマイプリセットに保存しました。どのページ・どのプロジェクトからも呼び出せます。"));
}

bool YugamiEditor::keyPressed (const juce::KeyPress& k)
{
    if (content.slotName.hasKeyboardFocus (true) || content.pgName.hasKeyboardFocus (true)) return false;
    const auto c = (char) juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) k.getKeyCode());
    for (int i = 0; i < 16; ++i)
        if (c == kKeys[i])
        {
            if (keyDown[(size_t) i] == 0)
            {
                const bool time = ! k.getModifiers().isShiftDown();
                keyDown[(size_t) i] = time ? 1 : 2;
                proc.pushTrigger (time, i, true);
            }
            return true;
        }
    return false;
}

bool YugamiEditor::keyStateChanged (bool)
{
    bool any = false;
    for (int i = 0; i < 16; ++i)
        if (keyDown[(size_t) i] && ! juce::KeyPress::isKeyCurrentlyDown (kKeys[i]) && ! juce::KeyPress::isKeyCurrentlyDown (juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) kKeys[i])))
        {
            proc.pushTrigger (keyDown[(size_t) i] == 1, i, false);
            keyDown[(size_t) i] = 0; any = true;
        }
    return any;
}

void YugamiEditor::focusLost (FocusChangeType)
{
    for (int k = 0; k < 16; ++k) if (keyDown[(size_t) k]) { proc.pushTrigger (keyDown[(size_t) k] == 1, k, false); keyDown[(size_t) k] = 0; }
}

void YugamiEditor::timerCallback()
{
    const double t = (juce::Time::getMillisecondCounterHiRes() - startTime) / 1000.0;
    if ((++frame & 1) == 0) content.backdrop.tick (t);
    if (statusTimeout > 0 && --statusTimeout == 0) content.status.setText ({}, juce::dontSendNotification);
    const int gen = proc.bankGeneration.load();
    if (gen != lastGen) { lastGen = gen; refreshAll(); }
    const int ht = proc.hotT.load(), hv = proc.hotV.load(), bt = baseT(), bv = baseV();
    if (ht != lastHotT || bt != lastBaseT) { lastHotT = ht; lastBaseT = bt; content.timeGrid.repaint(); content.env.repaint(); }
    if (hv != lastHotV || bv != lastBaseV) { lastHotV = hv; lastBaseV = bv; content.volGrid.repaint(); content.env.repaint(); }
    if (proc.running.load()) { content.env.repaint(); content.dial.repaint(); }
    content.repaint (content.meterR.expanded (4, 22).toNearestInt());
    if (content.pgDel.getButtonText() != jp ("削除") && juce::Time::getMillisecondCounter() - delArm > 3000) content.pgDel.setButtonText (jp ("削除"));
    if (content.resetBtn.getButtonText() != jp ("標準に戻す") && juce::Time::getMillisecondCounter() - resetArm > 3000) content.resetBtn.setButtonText (jp ("標準に戻す"));
}
} // namespace yugami::ui
