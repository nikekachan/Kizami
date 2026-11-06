#include "PluginEditor.h"
#include <BinaryData.h>

namespace nagare::ui
{
static const char* const kKeys = "awsedftgyhujkolp;";
static const char* const kLetters[] = { "A","W","S","E","D","F","T","G","Y","H","U","J","K","O","L","P",";" };

//==============================================================================
Fonts::Fonts()
{
    comfortaa = juce::Typeface::createSystemTypefaceFor (BinaryData::ComfortaaBold_ttf, (size_t) BinaryData::ComfortaaBold_ttfSize);
    greatVibes = juce::Typeface::createSystemTypefaceFor (BinaryData::GreatVibesRegular_ttf, (size_t) BinaryData::GreatVibesRegular_ttfSize);
}
juce::Font Fonts::body (float h, bool bold) const { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
juce::Font Fonts::display (float h) const { return comfortaa != nullptr ? juce::Font (juce::FontOptions (comfortaa).withHeight (h)) : body (h, true); }
juce::Font Fonts::script (float h) const { return greatVibes != nullptr ? juce::Font (juce::FontOptions (greatVibes).withHeight (h)) : body (h); }
juce::Font Fonts::mono (float h) const { return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, juce::Font::bold)); }
const Fonts& fonts() { static Fonts f; return f; }

//==============================================================================
Look::Look()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::TextButton::buttonColourId, col::bg);
    setColour (juce::TextButton::buttonOnColourId, col::ink);
    setColour (juce::TextButton::textColourOffId, col::ink);
    setColour (juce::TextButton::textColourOnId, col::bg);
    setColour (juce::ComboBox::backgroundColourId, col::bg);
    setColour (juce::ComboBox::textColourId, col::ink);
    setColour (juce::ComboBox::outlineColourId, col::line);
    setColour (juce::ComboBox::arrowColourId, col::ink);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::ink);
    setColour (juce::PopupMenu::highlightedTextColourId, col::bg);
    setColour (juce::PopupMenu::headerTextColourId, col::muted);
    setColour (juce::Label::textColourId, col::ink);
    setColour (juce::TextEditor::backgroundColourId, col::bg);
    setColour (juce::TextEditor::textColourId, col::ink);
    setColour (juce::TextEditor::outlineColourId, col::line);
    setColour (juce::TextEditor::focusedOutlineColourId, col::ink);
    setColour (juce::TextEditor::highlightColourId, col::ink.withAlpha (0.3f));
    setColour (juce::CaretComponent::caretColourId, col::ink);
    setColour (juce::AlertWindow::backgroundColourId, col::panel);
    setColour (juce::AlertWindow::textColourId, col::ink);
    setColour (juce::AlertWindow::outlineColourId, col::stroke);
    setColour (juce::ToggleButton::textColourId, col::ink);
    setColour (juce::TooltipWindow::backgroundColourId, col::panel);
    setColour (juce::TooltipWindow::textColourId, col::ink);
    setColour (juce::TooltipWindow::outlineColourId, col::line);
    setColour (juce::BubbleComponent::backgroundColourId, col::panel);
}

void Look::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.9f);
    const float rad = r.getHeight() * 0.5f;
    if (b.getToggleState() || down) { g.setColour (col::ink.withAlpha (down ? 0.85f : 1.0f)); g.fillRoundedRectangle (r, rad); }
    else if (over) { g.setColour (col::ink.withAlpha (0.12f)); g.fillRoundedRectangle (r, rad); }
    g.setColour (b.isEnabled() ? col::stroke : col::line);
    g.drawRoundedRectangle (r, rad, 1.5f);
}

void Look::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    const bool inv = b.getToggleState() || down;
    g.setColour ((inv ? col::bg : b.findColour (juce::TextButton::textColourOffId)).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.4f));
    g.setFont (fonts().body (juce::jmin (14.0f, b.getHeight() * 0.48f), true));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void Look::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    auto box = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.2f);
    const float a = b.isEnabled() ? 1.0f : 0.4f;
    if (b.getToggleState())
    {
        g.setColour (col::ink.withMultipliedAlpha (a));
        g.fillRoundedRectangle (box, 3.5f);
        juce::Path tick;
        tick.startNewSubPath (box.getX() + box.getWidth() * 0.22f, box.getCentreY());
        tick.lineTo (box.getX() + box.getWidth() * 0.43f, box.getBottom() - box.getHeight() * 0.25f);
        tick.lineTo (box.getRight() - box.getWidth() * 0.2f, box.getY() + box.getHeight() * 0.25f);
        g.setColour (col::bg);
        g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else
    {
        g.setColour ((over ? col::ink : col::stroke).withMultipliedAlpha (a));
        g.drawRoundedRectangle (box, 3.5f, 1.5f);
    }
    g.setColour (col::ink.withMultipliedAlpha (a));
    g.setFont (fonts().body (14.0f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (5.0f), juce::Justification::centredLeft);
}

void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.9f);
    g.setColour (col::bg);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (col::stroke);
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.5f);
    juce::Path p;
    const float cx = (float) w - 16.0f, cy = (float) h * 0.5f;
    p.startNewSubPath (cx - 4.5f, cy - 2.0f); p.lineTo (cx, cy + 2.5f); p.lineTo (cx + 4.5f, cy - 2.0f);
    g.setColour (col::ink);
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
juce::Font Look::getComboBoxFont (juce::ComboBox&) { return fonts().body (14.0f); }
void Look::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (14, 1, box.getWidth() - 40, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}
juce::Font Look::getPopupMenuFont() { return fonts().body (15.0f); }
juce::Font Look::getAlertWindowMessageFont() { return fonts().body (14.0f); }
juce::Font Look::getAlertWindowTitleFont() { return fonts().body (17.0f, true); }

void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const float cx = x + w * 0.5f, cy = y + h * 0.5f, r = juce::jmin (w, h) * 0.5f - 3.0f;
    const float ang = a0 + pos * (a1 - a0);
    juce::Path bgArc, valArc;
    bgArc.addCentredArc (cx, cy, r, r, 0.0f, a0, a1, true);
    g.setColour (col::line);
    g.strokePath (bgArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (pos > 0.003f)
    {
        valArc.addCentredArc (cx, cy, r, r, 0.0f, a0, ang, true);
        g.setColour (col::ink.withMultipliedAlpha (s.isEnabled() ? 1.0f : 0.5f));
        g.strokePath (valArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    const float cr = r * 0.66f;
    g.setColour (col::bg);
    g.fillEllipse (cx - cr, cy - cr, cr * 2, cr * 2);
    g.setColour (col::stroke);
    g.drawEllipse (cx - cr, cy - cr, cr * 2, cr * 2, 1.6f);
    g.setColour (col::ink);
    g.drawLine (cx, cy, cx + std::sin (ang) * r * 0.54f, cy - std::cos (ang) * r * 0.54f, 2.5f);
}

//==============================================================================
Knob::Knob (NagareProcessor& p, const juce::String& id) : proc (p), prm (p.apvts.getParameter (id))
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (180);
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id, slider);
    slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
    slider.setTitle (prm->getName (64));
}

void Knob::resized() { slider.setBounds (getWidth() / 2 - 27, 0, 54, 54); }

void Knob::paint (juce::Graphics& g)
{
    g.setColour (col::muted);
    g.setFont (fonts().body (12.5f));
    g.drawText (prm->getName (64), 0, 56, getWidth(), 17, juce::Justification::centred);
    auto pill = juce::Rectangle<float> (4.0f, 75.0f, (float) getWidth() - 8.0f, 19.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (pill, 9.5f, 1.4f);
    g.setColour (col::ink);
    g.setFont (fonts().mono (12.0f));
    g.drawText (prm->getCurrentValueAsText(), pill, juce::Justification::centred);
}

//==============================================================================
Seg::Seg (NagareProcessor& p, const juce::String& i, juce::StringArray l) : proc (p), id (i), labels (std::move (l)) {}

int Seg::preferredWidth() const
{
    float w = 0;
    for (auto& s : labels) w += juce::GlyphArrangement::getStringWidth (fonts().body (14.0f, true), s) + 24.0f;
    return (int) std::ceil (w) + 2;
}

juce::Rectangle<float> Seg::itemBounds (int i) const
{
    std::vector<float> ws;
    float total = 0;
    for (auto& s : labels) { ws.push_back (juce::GlyphArrangement::getStringWidth (fonts().body (14.0f, true), s) + 24.0f); total += ws.back(); }
    const float scale = ((float) getWidth() - 2.0f) / juce::jmax (1.0f, total);
    float x = 1.0f;
    for (int k = 0; k < i; ++k) x += ws[(size_t) k] * scale;
    return { x, 1.0f, ws[(size_t) i] * scale, (float) getHeight() - 2.0f };
}

void Seg::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.9f);
    const float rad = r.getHeight() * 0.5f;
    juce::Path clip;
    clip.addRoundedRectangle (r, rad);
    const int sel = juce::roundToInt (proc.param (id));
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (clip);
        for (int i = 0; i < labels.size(); ++i)
        {
            auto b = itemBounds (i);
            const bool on = i == sel, en = ! isEnabledItem || isEnabledItem (i);
            if (on) { g.setColour (col::ink); g.fillRect (b); }
            if (i > 0) { g.setColour (col::line); g.fillRect (b.getX() - 0.75f, b.getY(), 1.5f, b.getHeight()); }
            g.setColour ((on ? col::bg : col::ink).withMultipliedAlpha (en ? 1.0f : 0.35f));
            g.setFont (fonts().body (14.0f, on));
            g.drawText (labels[i], b, juce::Justification::centred);
        }
    }
    g.setColour (col::stroke);
    g.drawRoundedRectangle (r, rad, 1.5f);
}

void Seg::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < labels.size(); ++i)
        if (itemBounds (i).contains (e.position))
        {
            if (isEnabledItem && ! isEnabledItem (i)) return;
            proc.setParam (id, (float) i);
            if (onPick) onPick (i);
            repaint();
            return;
        }
}

//==============================================================================
WaveView::WaveView (NagareEditor& e) : editor (e) {}

static bool isSampleSource (NagareProcessor& p) { return juce::roundToInt (p.param ("source")) == 0; }

int WaveView::edgeAt (float px) const
{
    auto snap = editor.proc.getSnapshot();
    auto src = editor.proc.getSource();
    if (! isSampleSource (editor.proc) || snap == nullptr || src == nullptr) return 0;
    const float n = (float) src->raw.size(), W = (float) getWidth();
    const float xs = snap->loopStart / n * W, xe = snap->loopEnd / n * W;
    const float ds = std::abs (px - xs), de = std::abs (px - xe);
    if (juce::jmin (ds, de) <= 10.0f) return ds <= de ? 1 : 2;
    if (px > xs && px < xe) return 3;
    return 0;
}

void WaveView::mouseMove (const juce::MouseEvent& e)
{
    const int m = edgeAt (e.position.x);
    setMouseCursor (m == 3 ? juce::MouseCursor::DraggingHandCursor : m ? juce::MouseCursor::LeftRightResizeCursor
                    : isSampleSource (editor.proc) ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void WaveView::mouseDown (const juce::MouseEvent& e)
{
    if (! isSampleSource (editor.proc) || editor.proc.getSource() == nullptr) return;
    const float f = juce::jlimit (0.0f, 1.0f, e.position.x / (float) getWidth());
    s0 = editor.proc.param ("loopStart");
    e0 = editor.proc.param ("loopEnd");
    f0 = f;
    dragMode = edgeAt (e.position.x);
    if (dragMode == 0) dragMode = f < s0 ? 1 : 2;
    editor.proc.apvts.getParameter ("loopStart")->beginChangeGesture();
    editor.proc.apvts.getParameter ("loopEnd")->beginChangeGesture();
    applyDrag (f);
}

void WaveView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMode) applyDrag (juce::jlimit (0.0f, 1.0f, e.position.x / (float) getWidth()));
}

void WaveView::mouseUp (const juce::MouseEvent&)
{
    if (! dragMode) return;
    dragMode = 0;
    editor.proc.apvts.getParameter ("loopStart")->endChangeGesture();
    editor.proc.apvts.getParameter ("loopEnd")->endChangeGesture();
}

void WaveView::applyDrag (float f)
{
    auto src = editor.proc.getSource();
    if (src == nullptr) return;
    const float m = juce::jmax (0.0015f, (float) (0.01 * src->sampleRate / juce::jmax ((size_t) 1, src->raw.size())));
    float s = editor.proc.param ("loopStart"), en = editor.proc.param ("loopEnd");
    if (dragMode == 1) s = juce::jlimit (0.0f, en - m, f);
    else if (dragMode == 2) en = juce::jlimit (s + m, 1.0f, f);
    else { const float w = e0 - s0; s = juce::jlimit (0.0f, 1.0f - w, s0 + (f - f0)); en = s + w; }
    auto set = [&] (const char* id, float v) { auto* p = editor.proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    set ("loopStart", s);
    set ("loopEnd", en);
    repaint();
}

bool WaveView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (juce::File (f).hasFileExtension ("wav;aif;aiff;mp3;flac;ogg;m4a;caf")) return true;
    return false;
}

void WaveView::filesDropped (const juce::StringArray& files, int, int)
{
    dropping = false;
    repaint();
    if (! files.isEmpty()) editor.loadSample (juce::File (files[0]));
}

void WaveView::paint (juce::Graphics& g)
{
    auto box = getLocalBounds().toFloat().reduced (0.75f);
    g.setColour (col::bg);
    g.fillRoundedRectangle (box, 14.0f);
    const float W = (float) getWidth(), H = (float) getHeight(), mid = H * 0.5f + 6.0f;
    auto& p = editor.proc;
    auto src = p.getSource();
    auto snap = p.getSnapshot();

    if (isSampleSource (p) && src != nullptr && snap != nullptr)
    {
        const int gen = p.sourceGeneration.load();
        const int cols = juce::jmax (1, getWidth());
        if (peaksFor != gen || peaksWidth != cols)
        {
            peaks.assign ((size_t) cols, { 0.0f, 0.0f });
            const size_t n = src->raw.size();
            for (int px = 0; px < cols; ++px)
            {
                const size_t a = n * (size_t) px / (size_t) cols, b = juce::jmax (a + 1, n * (size_t) (px + 1) / (size_t) cols);
                float lo = 0, hi = 0;
                for (size_t i = a; i < b && i < n; i += juce::jmax ((size_t) 1, (b - a) / 64))
                { lo = juce::jmin (lo, src->raw[i]); hi = juce::jmax (hi, src->raw[i]); }
                peaks[(size_t) px] = { lo, hi };
            }
            peaksFor = gen;
            peaksWidth = cols;
        }
        const float n = (float) src->raw.size();
        const float xs = snap->loopStart / n * W, xe = snap->loopEnd / n * W;
        g.setColour (col::ink.withAlpha (0.12f));
        g.fillRect (xs, 18.0f, juce::jmax (3.0f, xe - xs), H - 24.0f);
        const float amp = H * 0.5f - 14.0f;
        for (int px = 0; px < (int) peaks.size(); ++px)
        {
            const bool in = px >= (int) xs - 1 && px <= (int) xe;
            g.setColour (in ? col::ink.withAlpha (0.92f) : col::muted.withAlpha (px < xs ? 0.85f : 0.3f));
            const auto [lo, hi] = peaks[(size_t) px];
            g.fillRect ((float) px, mid - hi * amp, 1.0f, juce::jmax (1.0f, (hi - lo) * amp));
        }
        g.setColour (col::ink);
        for (float x : { xs, xe })
        {
            g.fillRect (x - 0.75f, 18.0f, 1.5f, H - 24.0f);
            g.fillRoundedRectangle (x - 4.0f, H * 0.5f - 10.0f, 8.0f, 20.0f, 3.0f);
        }
        g.setFont (fonts().body (12.0f, true));
        g.drawText (jp ("アタック(1回だけ鳴る)"), 8, 3, 200, 16, juce::Justification::centredLeft);
        const bool waveMode = juce::roundToInt (p.param ("mode")) == 1 && snap->hasWave;
        const auto label = jp (waveMode ? "この部分を波形化して持続" : "この部分を繰り返して持続")
                         + "(" + fmtMs ((float) ((snap->loopEnd - snap->loopStart) / snap->sampleRate * 1000.0)) + ")";
        const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), label);
        const bool right = xe + 8.0f + tw > W - 6.0f;
        g.drawText (label, juce::Rectangle<float> (right ? juce::jmax (4.0f, xs - 8.0f - tw) : xe + 8.0f, 20.0f, tw + 2.0f, 16.0f), juce::Justification::centredLeft);
        g.setColour (col::muted);
        g.setFont (fonts().body (11.5f));
        g.drawText (jp ("ループの端をドラッグで調整 · 音声ファイルはここにドロップ"), 8, getHeight() - 20, getWidth() - 16, 16, juce::Justification::centredLeft);
    }
    else
    {
        const int s = juce::roundToInt (p.param ("source"));
        const auto& t = builtinTable ((Source) s);
        juce::Path path;
        for (int px = 0; px <= getWidth(); ++px)
        {
            const double ph = std::fmod (px / W * 2.0, 1.0);
            float v = t.lookup (ph, 8);
            if ((Source) s == Source::supersaw)
            {
                v = 0;
                const double off[7] = { 0, .13, .29, .47, .61, .78, .9 };
                for (int k = 0; k < 7; ++k) v += t.lookup (std::fmod (px / W * 2.0 * (1.0 + (k - 3) * 0.012) + off[k], 1.0), 8);
                v /= 4.0f;
            }
            const float y = mid - v * (H * 0.5f - 22.0f);
            if (px == 0) path.startNewSubPath (0, y); else path.lineTo ((float) px, y);
        }
        g.setColour (col::ink);
        g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setFont (fonts().body (12.0f, true));
        g.drawText (jp ("内蔵の波形は押している間ずっと同じ音色で鳴ります"), 8, 3, getWidth() - 16, 16, juce::Justification::centredLeft);
    }

    g.setColour (dropping ? col::ink : col::line);
    if (dropping)
    {
        juce::Path pth; pth.addRoundedRectangle (box.reduced (3.0f), 12.0f);
        juce::Path dashed; const float d[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.8f).createDashedStroke (dashed, pth, d, 2);
        g.fillPath (dashed);
    }
    else g.drawRoundedRectangle (box, 14.0f, 1.5f);
}

//==============================================================================
void EnvView::paint (juce::Graphics& g)
{
    auto box = getLocalBounds().toFloat().reduced (0.75f);
    g.setColour (col::bg);
    g.fillRoundedRectangle (box, 14.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (box, 14.0f, 1.5f);

    const float W = (float) getWidth(), H = (float) getHeight(), top = 26.0f, bot = H - 26.0f, h = bot - top;
    const float nA = proc.apvts.getParameter ("attack")->getValue(), nD = proc.apvts.getParameter ("decay")->getValue();
    const float nR = proc.apvts.getParameter ("release")->getValue(), S = proc.param ("sustain");
    const float wA = 0.04f + 0.16f * nA, wD = 0.06f + 0.16f * nD, wH = 0.36f, wR = 0.06f + 0.2f * nR, sc = (W - 20.0f) / (wA + wD + wH + wR);
    const float x0 = 10, xA = x0 + wA * sc, xD = xA + wD * sc, xH = xD + wH * sc, xR = xH + wR * sc, yS = bot - S * h;

    {
        juce::Path dash; dash.startNewSubPath (x0, yS); dash.lineTo (W - 10.0f, yS);
        juce::Path d2; const float d[] = { 3.0f, 4.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (d2, dash, d, 2);
        g.setColour (col::line); g.fillPath (d2);
    }
    juce::Path p;
    p.startNewSubPath (x0, bot);
    p.lineTo (xA, top);
    for (int i = 1; i <= 24; ++i) { const float t = i / 24.0f, v = S + (1 - S) * std::exp (-t * 4); p.lineTo (xA + (xD - xA) * t, bot - v * h); }
    p.lineTo (xH, yS);
    for (int i = 1; i <= 24; ++i) { const float t = i / 24.0f; p.lineTo (xH + (xR - xH) * t, bot - S * std::exp (-t * 4) * h); }
    juce::Path fill (p);
    fill.lineTo (xR, bot);
    fill.closeSubPath();
    g.setColour (col::ink.withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (col::ink);
    g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.fillRect (xD, yS - 2.0f, xH - xD, 4.0f);

    g.setFont (fonts().body (12.0f, true));
    g.drawText (jp ("押している間ずっと持続"), juce::Rectangle<float> (xD, juce::jmax (4.0f, yS - 22.0f), xH - xD, 16.0f), juce::Justification::centred);
    g.setColour (col::muted);
    g.drawText (jp ("鍵盤を離す"), juce::Rectangle<float> (xH - 50, 4, 100, 16), juce::Justification::centred);
    {
        juce::Path v; v.startNewSubPath (xH, 20); v.lineTo (xH, bot);
        juce::Path d2; const float d[] = { 2.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (d2, v, d, 2);
        g.fillPath (d2);
    }
    g.setFont (fonts().body (12.0f, true));
    auto lbl = [&] (const juce::String& s, float a, float b, juce::Colour c) { g.setColour (c); g.drawText (s, juce::Rectangle<float> (a, H - 22, b - a, 16), juce::Justification::centred); };
    lbl ("A", x0, xA, col::muted);
    lbl ("D", xA, xD, col::muted);
    lbl ("S " + fmtDb (S), xD, xH, col::ink);
    lbl ("R", xH, xR, col::muted);
}

//==============================================================================
int KeyboardView::firstNote() const { return 60 + editor.proc.octave.load() * 12 - 12; }

juce::Rectangle<float> KeyboardView::keyRect (int note) const
{
    const int first = firstNote();
    int whites = 0;
    for (int n = first; n <= lastNote(); ++n) whites += isBlack (n) ? 0 : 1;
    const float ww = (float) getWidth() / (float) whites, H = (float) getHeight();
    int wi = 0;
    for (int n = first; n < note; ++n) wi += isBlack (n) ? 0 : 1;
    if (! isBlack (note)) return { wi * ww, 0.0f, ww, H };
    const float bw = ww * 0.62f;
    return { wi * ww - bw * 0.5f, 0.0f, bw, H * 0.6f };
}

int KeyboardView::noteAt (juce::Point<float> p, float& vel) const
{
    for (int pass = 0; pass < 2; ++pass)
        for (int n = firstNote(); n <= lastNote(); ++n)
        {
            if (isBlack (n) != (pass == 0)) continue;
            auto r = keyRect (n);
            if (r.contains (p)) { vel = juce::jlimit (0.3f, 1.0f, 0.45f + 0.55f * (p.y - r.getY()) / r.getHeight()); return n; }
        }
    return -1;
}

void KeyboardView::mouseDown (const juce::MouseEvent& e)
{
    float v = 0.8f;
    const int n = noteAt (e.position, v);
    if (n < 0) return;
    mouseNote = n;
    editor.proc.pushEvent (NagareProcessor::EventType::on, n, v);
    repaint();
}

void KeyboardView::mouseDrag (const juce::MouseEvent& e)
{
    float v = 0.8f;
    const int n = noteAt (e.position, v);
    if (n == mouseNote || n < 0) return;
    if (mouseNote >= 0) editor.proc.pushEvent (NagareProcessor::EventType::off, mouseNote);
    mouseNote = n;
    editor.proc.pushEvent (NagareProcessor::EventType::on, n, 0.8f);
    repaint();
}

void KeyboardView::mouseUp (const juce::MouseEvent&)
{
    if (mouseNote >= 0) editor.proc.pushEvent (NagareProcessor::EventType::off, mouseNote);
    mouseNote = -1;
    repaint();
}

void KeyboardView::paint (juce::Graphics& g)
{
    const int base = 60 + editor.proc.octave.load() * 12;
    auto pressed = [&] (int n) { return n == mouseNote || editor.proc.isPhysicalHeld (n); };
    for (int pass = 0; pass < 2; ++pass)
        for (int n = firstNote(); n <= lastNote(); ++n)
        {
            const bool black = isBlack (n);
            if (black != (pass == 1)) continue;
            auto r = keyRect (n);
            const bool on = pressed (n);
            juce::Path k;
            auto kr = black ? r.withTrimmedTop (-1.0f) : r.reduced (1.5f, 0.0f).withTrimmedBottom (0.8f);
            k.addRoundedRectangle (kr.getX(), kr.getY(), kr.getWidth(), kr.getHeight(), black ? 7.0f : 9.0f, black ? 7.0f : 9.0f, false, false, true, true);
            if (black)
            {
                g.setColour (on ? col::bg : juce::Colour (0xffe9edef));
                g.fillPath (k);
                if (on) { g.setColour (col::stroke); g.strokePath (k, juce::PathStrokeType (1.6f)); }
            }
            else
            {
                g.setColour (on ? col::ink : col::bg);
                g.fillPath (k);
                g.setColour (col::stroke);
                g.strokePath (k, juce::PathStrokeType (1.6f));
            }
            const int li = n - base;
            g.setFont (fonts().mono (11.0f));
            if (li >= 0 && li < 17)
            {
                g.setColour (black ? (on ? col::ink : col::bg.withAlpha (0.75f)) : (on ? col::bg : col::muted));
                g.drawText (kLetters[li], r.withTrimmedBottom (black ? 6.0f : (n % 12 == 0 ? 22.0f : 8.0f)), juce::Justification::centredBottom);
            }
            if (! black && n % 12 == 0)
            {
                g.setColour (on ? col::bg : col::muted);
                g.drawText (noteName (n), r.withTrimmedBottom (7.0f), juce::Justification::centredBottom);
            }
        }
}

//==============================================================================
Creatures::Creatures()
{
    setInterceptsMouseClicks (false, false);
    const std::pair<const char*, int> data[] = {
        { BinaryData::jelly_svg, BinaryData::jelly_svgSize }, { BinaryData::fish_svg, BinaryData::fish_svgSize },
        { BinaryData::shell_svg, BinaryData::shell_svgSize }, { BinaryData::weed_svg, BinaryData::weed_svgSize },
        { BinaryData::star_svg, BinaryData::star_svgSize },   { BinaryData::octo_svg, BinaryData::octo_svgSize },
        { BinaryData::angler_svg, BinaryData::angler_svgSize }, { BinaryData::bub_svg, BinaryData::bub_svgSize } };
    for (int i = 0; i < kinds; ++i) art[(size_t) i] = juce::Drawable::createFromImageData (data[i].first, (size_t) data[i].second);
}

void Creatures::add (int kind, juce::Rectangle<float> r, bool flip, float tilt)
{
    items.push_back ({ kind, r, flip, tilt, juce::Random ((juce::int64) items.size() * 977 + kind).nextDouble() * 8.0 });
}

void Creatures::paint (juce::Graphics& g)
{
    if (sand)
    {
        juce::Path w;
        const float y = (float) getHeight() - 10.0f;
        w.startNewSubPath (0, y);
        for (float x = 0; x < (float) getWidth(); x += 32.0f)
            w.quadraticTo (x + 8.0f, y - 6.0f, x + 16.0f, y), w.quadraticTo (x + 24.0f, y + 6.0f, x + 32.0f, y);
        g.setColour (col::ink.withAlpha (0.6f));
        g.strokePath (w, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    if (constellation)
    {
        const float x0 = (float) getWidth() - 62.0f, y0 = 6.0f;
        const juce::Point<float> pts[] = { { x0 + 6, y0 + 8 }, { x0 + 26, y0 + 30 }, { x0 + 52, y0 + 6 }, { x0 + 26, y0 + 58 } };
        g.setColour (col::ink.withAlpha (0.5f));
        g.drawLine ({ pts[0], pts[1] }, 1.0f); g.drawLine ({ pts[1], pts[2] }, 1.0f); g.drawLine ({ pts[1], pts[3] }, 1.0f);
        g.setColour (col::ink.withAlpha (0.9f));
        for (auto p : pts) g.fillEllipse (p.x - 2.6f, p.y - 2.6f, 5.2f, 5.2f);
    }
    for (auto& it : items)
    {
        auto* d = art[(size_t) it.kind].get();
        if (d == nullptr) continue;
        const double dur[] = { 4.6, 6.5, 6.0, 3.9, 9.0, 5.2, 8.0, 5.5 };
        const double u = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (time + it.phase) / (2.0 * dur[it.kind]));
        const double sw = std::sin (juce::MathConstants<double>::pi * u);
        float dx = 0, dy = 0, rot = 0, sx = 1, sy = 1;
        juce::Point<float> anchor = it.r.getCentre();
        switch (it.kind)
        {
            case jelly:  dy = (float) (-12 * u); sx = (float) (1 + 0.04 * sw - 0.02 * u); sy = (float) (1 - 0.05 * sw + 0.03 * u); break;
            case fish:   dx = (float) (16 * u); dy = (float) (-4 * sw + u); rot = (float) (-3 * sw + 2 * u); break;
            case octo:   dy = (float) (-8 * u); rot = (float) (-3 + 6 * u); anchor = { it.r.getCentreX(), it.r.getY() + it.r.getHeight() * 0.4f }; break;
            case angler: dx = (float) (-14 * u); dy = (float) (5 * u); rot = (float) (-4 * u); break;
            case weed:   rot = (float) (-5 + 10 * u); anchor = { it.r.getCentreX(), it.r.getBottom() }; break;
            case star:   rot = (float) (-6 + 14 * u); break;
            case shell:  rot = (float) (-3 + 6 * u); anchor = { it.r.getCentreX(), it.r.getBottom() }; break;
            case bub:    dy = (float) (-8 * u); break;
            default: break;
        }
        const auto t = juce::AffineTransform::translation (-anchor.x, -anchor.y)
                           .scaled (sx * (it.flip ? -1.0f : 1.0f), sy)
                           .rotated (juce::degreesToRadians (rot + it.tilt))
                           .translated (anchor.x + dx, anchor.y + dy);
        juce::Graphics::ScopedSaveState ss (g);
        g.addTransform (t);
        d->drawWithin (g, it.r, juce::RectanglePlacement::centred, 1.0f);
    }
}

//==============================================================================
static void drawIcon (juce::Graphics& g, int which, juce::Rectangle<float> r)
{
    juce::Path p;
    const float x = r.getX(), y = r.getY(), s = r.getWidth() / 24.0f;
    auto P = [&] (float a, float b) { return juce::Point<float> (x + a * s, y + b * s); };
    switch (which)
    {
        case 0: p.addEllipse (x + 3.5f * s, y + 10.5f * s, 9 * s, 9 * s); p.addEllipse (x + 13.5f * s, y + 5 * s, 6 * s, 6 * s); p.addEllipse (x + 16.4f * s, y + 15.4f * s, 3.2f * s, 3.2f * s); break;
        case 1:
            p.startNewSubPath (P (2, 15)); p.cubicTo (P (5, 10), P (7, 10), P (9, 15)); p.cubicTo (P (11, 20), P (13, 20), P (16, 15)); p.cubicTo (P (18, 10), P (20, 10), P (22, 15));
            p.startNewSubPath (P (6, 4)); p.lineTo (P (7, 6)); p.lineTo (P (9, 7)); p.lineTo (P (7, 8)); p.lineTo (P (6, 10)); p.lineTo (P (5, 8)); p.lineTo (P (3, 7)); p.lineTo (P (5, 6)); p.closeSubPath();
            break;
        case 2:
            for (auto c : { P (5, 17), P (12, 10), P (19, 5), P (18, 16) }) p.addEllipse (c.x - 1.8f * s, c.y - 1.8f * s, 3.6f * s, 3.6f * s);
            p.startNewSubPath (P (6.4f, 15.8f)); p.lineTo (P (10.6f, 11.2f)); p.startNewSubPath (P (13.5f, 9)); p.lineTo (P (17.5f, 6)); p.startNewSubPath (P (13.4f, 11)); p.lineTo (P (16.7f, 14.8f));
            break;
        default:
            p.startNewSubPath (P (3, 12)); p.cubicTo (P (6, 7.5f), P (11, 6.5f), P (15.5f, 9.5f)); p.lineTo (P (20, 6)); p.lineTo (P (20, 18)); p.lineTo (P (15.5f, 14.5f)); p.cubicTo (P (11, 17.5f), P (6, 16.5f), P (3, 12)); p.closeSubPath();
            break;
    }
    g.setColour (col::ink);
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

static void setNoFocus (juce::Component& c)
{
    for (auto* ch : c.getChildren()) { ch->setWantsKeyboardFocus (false); ch->setMouseClickGrabsKeyboardFocus (false); setNoFocus (*ch); }
}

Content::Content (NagareEditor& e)
    : editor (e),
      sourceSeg (e.proc, "source", { jp ("サンプル"), jp ("サイン"), jp ("三角"), jp ("ノコギリ"), jp ("矩形"), jp ("スーパーソー"), jp ("オルガン") }),
      modeSeg (e.proc, "mode", { jp ("ループ"), jp ("波形化") }),
      voiceSeg (e.proc, "voice", { jp ("ポリ(和音)"), jp ("モノ(レガート)") }),
      wave (e), env (e.proc), keyboard (e)
{
    logo = juce::ImageCache::getFromMemory (BinaryData::astronaut_png, BinaryData::astronaut_pngSize);
    juce::Random r (20261106);
    for (int i = 0; i < 520; ++i)
        stars.push_back ({ r.nextFloat() * W, r.nextFloat() * H, 0.5f + r.nextFloat() * 1.3f + (i % 41 == 0 ? 3.5f : 0.0f), 0.2f + r.nextFloat() * 0.55f });

    for (auto* c : std::initializer_list<juce::Component*> { &sourceSeg, &modeSeg, &voiceSeg, &loadBtn, &saveBtn, &delBtn, &octDown, &octUp, &trDown, &trUp,
                                                            &tpReset, &keepLen, &hold, &sampleName, &detect, &status, &octVal, &trVal, &presets, &wave, &env, &keyboard })
        addAndMakeVisible (c);

    modeSeg.isEnabledItem = [this] (int i) { auto s = editor.proc.getSource(); return i == 0 || (s != nullptr && s->pitched); };
    sourceSeg.onPick = [this] (int) { refresh(); wave.repaint(); };

    loadBtn.setButtonText (jp ("サンプルを読み込む"));
    loadBtn.onClick = [this] { editor.chooseSample(); };
    saveBtn.setButtonText (jp ("保存"));
    delBtn.setButtonText (jp ("削除"));
    delBtn.setColour (juce::TextButton::textColourOffId, col::danger);
    tpReset.setButtonText (jp ("0に戻す"));
    octDown.onClick = [this] { editor.setOctave (editor.proc.octave.load() - 1); };
    octUp.onClick = [this] { editor.setOctave (editor.proc.octave.load() + 1); };
    trDown.onClick = [this] { editor.setTranspose (editor.proc.transpose.load() - 1); };
    trUp.onClick = [this] { editor.setTranspose (editor.proc.transpose.load() + 1); };
    tpReset.onClick = [this] { editor.setOctave (0); editor.setTranspose (0); };

    keepLen.setButtonText (jp ("高い音でも長さを保つ"));
    hold.setButtonText (jp ("ホールド"));
    keepAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (e.proc.apvts, "keepLen", keepLen);
    holdAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (e.proc.apvts, "hold", hold);

    sampleName.setFont (fonts().body (15.0f, true));
    detect.setFont (fonts().mono (12.0f));
    detect.setJustificationType (juce::Justification::centred);
    status.setFont (fonts().body (12.5f));
    status.setJustificationType (juce::Justification::centredRight);
    status.setColour (juce::Label::textColourId, col::muted);
    for (auto* l : { &octVal, &trVal }) { l->setFont (fonts().mono (16.0f)); l->setJustificationType (juce::Justification::centred); }

    for (auto id : { "loopStart", "loopEnd", "pitch" }) addAndMakeVisible (srcKnobs.add (new Knob (e.proc, id)));
    for (auto id : { "attack", "decay", "sustain", "release", "volume" }) addAndMakeVisible (envKnobs.add (new Knob (e.proc, id)));
    for (auto id : { "cutoff", "reso", "glide", "delay", "reverb" }) addAndMakeVisible (toneKnobs.add (new Knob (e.proc, id)));

    presets.setTextWhenNothingSelected (jp ("プリセットを選ぶ"));
    refreshPresets();

    // creatures: header gap and the seabed, both clear of every control
    addAndMakeVisible (reef);
    addAndMakeVisible (seabed);
    reef.constellation = true;
    reef.add (Creatures::jelly, { 4, 4, 34, 54 });
    reef.add (Creatures::bub, { 46, 40, 14, 28 });
    reef.add (Creatures::fish, { 64, 26, 46, 26 });
    seabed.sand = true;
    seabed.add (Creatures::weed, { 14, 14, 34, 86 });
    seabed.add (Creatures::weed, { 56, 30, 26, 70 }, true);
    seabed.add (Creatures::shell, { 150, 56, 46, 44 });
    seabed.add (Creatures::bub, { 250, 14, 20, 40 });
    seabed.add (Creatures::angler, { 330, 26, 92, 62 });
    seabed.add (Creatures::star, { 540, 58, 40, 40 }, false, -14.0f);
    seabed.add (Creatures::jelly, { 650, 16, 38, 60 });
    seabed.add (Creatures::octo, { 760, 28, 70, 72 });
    seabed.add (Creatures::bub, { 900, 10, 18, 36 });
    seabed.add (Creatures::fish, { 940, 42, 52, 28 }, true);
    seabed.add (Creatures::weed, { 1040, 14, 34, 86 }, true);

    setNoFocus (*this);
    refresh();
}

void Content::refreshPresets()
{
    presets.clear (juce::dontSendNotification);
    presets.addSectionHeading (jp ("標準"));
    for (int i = 0; i < builtinPresetCount(); ++i) presets.addItem (jp (builtinPreset (i).name), i + 1);
    userPresets.clear();
    auto dir = NagareProcessor::presetFolder();
    if (dir.isDirectory())
    {
        auto found = dir.findChildFiles (juce::File::findFiles, false, "*.nagare");
        std::sort (found.begin(), found.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
        userPresets.addArray (found);
    }
    if (! userPresets.isEmpty())
    {
        presets.addSectionHeading (jp ("保存したプリセット"));
        for (int i = 0; i < userPresets.size(); ++i) presets.addItem (userPresets[i].getFileNameWithoutExtension(), 1000 + i);
    }
}

void Content::refresh()
{
    auto& p = editor.proc;
    const bool isS = isSampleSource (p);
    auto src = p.getSource();
    for (auto* c : std::initializer_list<juce::Component*> { &sampleName, &detect, &modeSeg, &keepLen }) c->setVisible (isS && src != nullptr);
    for (auto* k : srcKnobs) k->setVisible (isS && src != nullptr);
    if (src != nullptr)
    {
        const auto name = src->demo ? jp ("サンプル例(減衰するプラック)") : src->name;
        if (sampleName.getText() != name) sampleName.setText (name, juce::dontSendNotification);
        const auto det = src->pitched ? jp ("検出 ") + noteName (src->rootMidi) + juce::String::fromUTF8 (" · ") + juce::String (src->f0, 1) + " Hz"
                                      : jp ("音程なし → C5扱い");
        if (detect.getText() != det) detect.setText (det, juce::dontSendNotification);
        detect.setColour (juce::Label::textColourId, src->pitched ? col::ink : col::warm);
        keepLen.setEnabled (src->pitched);
    }
    const auto ov = jp ("A = ") + noteName (60 + p.octave.load() * 12);
    if (octVal.getText() != ov) octVal.setText (ov, juce::dontSendNotification);
    const int t = p.transpose.load();
    static const char* pcs[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const auto tv = (t > 0 ? "+" : "") + juce::String (t) + (t != 0 ? jp ("(C→") + pcs[((t % 12) + 12) % 12] + ")" : juce::String());
    if (trVal.getText() != tv) trVal.setText (tv, juce::dontSendNotification);
    toneKnobs[2]->setAlpha (p.param ("voice") > 0.5f ? 1.0f : 0.45f);
    if (statusTimeout > 0 && --statusTimeout == 0) status.setText ({}, juce::dontSendNotification);
    sourceSeg.repaint(); modeSeg.repaint(); voiceSeg.repaint();
}

void Content::resized()
{
    // header
    presets.setBounds (W - 18 - 64 - 8 - 64 - 8 - 262, 30, 262, 34);
    saveBtn.setBounds (W - 18 - 64 - 8 - 64, 30, 64, 34);
    delBtn.setBounds (W - 18 - 64, 30, 64, 34);
    status.setBounds (W - 18 - 420, 72, 420, 20);
    reef.setBounds (530, 14, 140, 96);

    srcPanel = { 18, 132, 572, 374 };
    envPanel = { 602, 132, 480, 374 };
    tonePanel = { 18, 518, 1064, 112 };
    keyPanel = { 18, 642, 1064, 282 };
    seabed.setBounds (0, 928, W, H - 928);

    // source panel
    {
        auto r = srcPanel.reduced (16, 14);
        auto head = r.removeFromTop (32);
        sourceSeg.setBounds (head.removeFromRight (juce::jmin (head.getWidth() - 90, sourceSeg.preferredWidth())));
        r.removeFromTop (10);
        auto row = r.removeFromTop (34);
        loadBtn.setBounds (row.removeFromLeft (164));
        row.removeFromLeft (10);
        detect.setBounds (row.removeFromRight (170).reduced (0, 4));
        sampleName.setBounds (row);
        r.removeFromTop (8);
        auto row2 = r.removeFromTop (30);
        modeSeg.setBounds (row2.removeFromLeft (modeSeg.preferredWidth()));
        row2.removeFromLeft (14);
        keepLen.setBounds (row2.removeFromLeft (230));
        r.removeFromTop (10);
        wave.setBounds (r.removeFromTop (120));
        r.removeFromTop (8);
        for (auto* k : srcKnobs) { k->setBounds (r.removeFromLeft (84).withHeight (96)); r.removeFromLeft (4); }
    }
    // envelope panel
    {
        auto r = envPanel.reduced (16, 14);
        r.removeFromTop (32 + 10);
        env.setBounds (r.removeFromTop (34 + 8 + 30 + 10 + 120));
        r.removeFromTop (8);
        for (auto* k : envKnobs) { k->setBounds (r.removeFromLeft (84).withHeight (96)); r.removeFromLeft (4); }
    }
    // tone panel
    {
        auto r = tonePanel.reduced (16, 8);
        const int vw = voiceSeg.preferredWidth();
        voiceSeg.setBounds (r.removeFromRight (vw).withSizeKeepingCentre (vw, 32));
        auto row = r.withTrimmedLeft (214);
        for (auto* k : toneKnobs) { k->setBounds (row.removeFromLeft (84).withHeight (96)); row.removeFromLeft (4); }
    }
    // keyboard panel
    {
        auto r = keyPanel.reduced (16, 12);
        auto head = r.removeFromTop (34);
        head.removeFromLeft (128);
        auto place = [&] (juce::Component& c, int w) { c.setBounds (head.removeFromLeft (w).withSizeKeepingCentre (w, 32)); };
        head.removeFromLeft (84); // label "オクターブ"
        place (octDown, 32); place (octVal, 92); place (octUp, 32);
        head.removeFromLeft (64 + 24); // key hints + gap
        head.removeFromLeft (108);      // label "トランスポーズ"
        place (trDown, 32); place (trVal, 104); place (trUp, 32);
        head.removeFromLeft (64 + 18);
        place (tpReset, 96);
        head.removeFromLeft (16);
        place (hold, 104);
        r.removeFromTop (12);
        keyboard.setBounds (r.removeFromTop (124));
    }
}

void Content::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    for (auto& s : stars)
    {
        g.setColour (col::ink.withAlpha (s[3]));
        if (s[2] > 3.0f)
        {
            const float k = s[2] + 1.5f, x = s[0], y = s[1];
            juce::Path sp;
            sp.startNewSubPath (x, y - k); sp.quadraticTo (x, y, x + k, y); sp.quadraticTo (x, y, x, y + k); sp.quadraticTo (x, y, x - k, y); sp.quadraticTo (x, y, x, y - k);
            g.setColour (col::ink.withAlpha (0.75f));
            g.fillPath (sp);
        }
        else g.fillEllipse (s[0] - s[2], s[1] - s[2], s[2] * 2, s[2] * 2);
    }

    // header
    if (logo.isValid()) g.drawImageWithin (logo, 18, 12, 62, 100, juce::RectanglePlacement::centred);
    g.setColour (col::ink);
    g.setFont (fonts().display (36.0f));
    g.drawText ("DJ KATSUNII", 94, 14, 440, 44, juce::Justification::centredLeft);
    g.setFont (fonts().body (15.0f, true));
    g.drawText (jp ("かつにい"), 96, 60, 80, 20, juce::Justification::centredLeft);
    g.setColour (col::muted);
    g.setFont (fonts().display (11.5f));
    g.drawText ("N A G A R E   S Y N T H", 168, 60, 240, 20, juce::Justification::centredLeft);
    g.setFont (fonts().body (13.0f));
    g.drawText (jp ("長押ししている間、音が減衰せずに鳴り続けるメロディシンセ"), 96, 82, 460, 20, juce::Justification::centredLeft);

    // tide line
    {
        juce::Path w;
        const float y = 120.0f;
        w.startNewSubPath (18, y);
        for (float x = 18; x < W - 18; x += 48.0f) w.quadraticTo (x + 6, y - 6, x + 12, y), w.quadraticTo (x + 18, y + 6, x + 24, y), w.quadraticTo (x + 30, y - 6, x + 36, y), w.quadraticTo (x + 42, y + 6, x + 48, y);
        g.setColour (col::ink.withAlpha (0.55f));
        g.strokePath (w, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    auto panel = [&] (juce::Rectangle<int> r, int icon, const char* title) {
        auto f = r.toFloat().reduced (0.9f);
        g.setColour (col::panel);
        g.fillRoundedRectangle (f, 20.0f);
        g.setColour (col::stroke);
        g.drawRoundedRectangle (f, 20.0f, 1.8f);
        drawIcon (g, icon, juce::Rectangle<float> ((float) r.getX() + 16, (float) r.getY() + 18, 22, 22));
        g.setColour (col::ink);
        g.setFont (fonts().body (16.0f, true));
        g.drawText (jp (title), r.getX() + 46, r.getY() + 14, 200, 30, juce::Justification::centredLeft);
    };
    panel (srcPanel, 0, "音源");
    panel (envPanel, 1, "エンベロープ");
    panel (tonePanel, 2, "音色と演奏");
    panel (keyPanel, 3, "キーボード");

    if (detect.isVisible())
    {
        g.setColour (detect.findColour (juce::Label::textColourId).withAlpha (0.5f));
        g.drawRoundedRectangle (detect.getBounds().toFloat().reduced (0.75f), detect.getHeight() * 0.5f, 1.4f);
    }
    g.setColour (col::muted);
    g.setFont (fonts().body (12.5f));
    g.drawText (jp ("押している間はサステインの音量のまま鳴り続けます"), envPanel.getX() + 160, envPanel.getY() + 14, envPanel.getWidth() - 176, 30, juce::Justification::centredRight);

    // keyboard panel labels
    {
        const int y = keyPanel.getY() + 12, x0 = keyPanel.getX() + 16 + 128;
        g.setFont (fonts().body (13.0f, true));
        g.drawText (jp ("オクターブ"), x0, y, 80, 34, juce::Justification::centredLeft);
        g.drawText (jp ("トランスポーズ"), x0 + 84 + 156 + 88, y, 104, 34, juce::Justification::centredLeft);
        auto kbd = [&] (const char* k, int x) {
            juce::Rectangle<float> r ((float) x, (float) y + 8, 20, 18);
            g.setColour (col::line); g.drawRoundedRectangle (r, 4.0f, 1.2f);
            g.setColour (col::muted); g.setFont (fonts().mono (11.0f)); g.drawText (k, r, juce::Justification::centred);
        };
        kbd ("Z", x0 + 84 + 156 + 8); kbd ("X", x0 + 84 + 156 + 32);
        kbd ("C", x0 + 84 + 156 + 88 + 108 + 168 + 8); kbd ("V", x0 + 84 + 156 + 88 + 108 + 168 + 32);
    }
    // motto + hint
    g.setColour (col::ink);
    g.setFont (fonts().script (46.0f));
    g.drawText ("Every thing from the Ocean", keyPanel.getX(), keyPanel.getBottom() - 80, keyPanel.getWidth(), 50, juce::Justification::centred);
    g.setColour (col::muted);
    g.setFont (fonts().body (12.5f));
    g.drawText (jp ("パソコンのキーボード A W S E D F T G Y H U J K… で演奏 · Space を押している間はサステインペダル · FL StudioのピアノロールやMIDIキーボードでも弾けます(トランスポーズもかかります)"),
                keyPanel.getX() + 16, keyPanel.getBottom() - 30, keyPanel.getWidth() - 32, 18, juce::Justification::centred);
}

//==============================================================================
NagareEditor::NagareEditor (NagareProcessor& p) : AudioProcessorEditor (&p), proc (p), content (*this)
{
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.addMouseListener (this, true);
    keyNote.fill (-1);
    setWantsKeyboardFocus (true);
    setResizable (true, true);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) Content::W / Content::H);
    setResizeLimits (660, 600, 1650, 1500);
    setSize (902, 856);

    content.presets.onChange = [this] {
        const int id = content.presets.getSelectedId();
        if (id <= 0) return;
        if (id < 1000) { proc.applyBuiltin (id - 1); setStatus (jp ("「") + content.presets.getText() + jp ("」を読み込みました")); }
        else
        {
            juce::String err;
            if (proc.loadPreset (content.userPresets[id - 1000], err)) setStatus (jp ("「") + content.presets.getText() + jp ("」を読み込みました"));
            else setStatus (err, true);
        }
        content.wave.invalidate();
    };
    content.saveBtn.onClick = [this] { savePreset(); };
    content.delBtn.onClick = [this] { deletePreset(); };

    startTime = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

NagareEditor::~NagareEditor()
{
    stopTimer();
    for (int i = 0; i < 17; ++i) if (keyNote[(size_t) i] >= 0) proc.pushEvent (NagareProcessor::EventType::off, keyNote[(size_t) i]);
    if (spaceDown) proc.pushEvent (NagareProcessor::EventType::pedalUp);
    setLookAndFeel (nullptr);
}

void NagareEditor::paint (juce::Graphics& g) { g.fillAll (col::bg); }

void NagareEditor::resized()
{
    content.setBounds (0, 0, Content::W, Content::H);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) Content::W));
}

void NagareEditor::mouseDown (const juce::MouseEvent&) { grabKeyboardFocus(); }

void NagareEditor::timerCallback()
{
    const double t = (juce::Time::getMillisecondCounterHiRes() - startTime) / 1000.0;
    content.reef.tick (t);
    content.seabed.tick (t);
    content.refresh();
    content.keyboard.repaint();
    content.env.repaint();
    content.wave.repaint();
    if (proc.sourceGeneration.load() != lastGeneration) { lastGeneration = proc.sourceGeneration.load(); content.wave.invalidate(); }
}

void NagareEditor::setStatus (const juce::String& s, bool error)
{
    content.status.setText (s, juce::dontSendNotification);
    content.status.setColour (juce::Label::textColourId, error ? col::danger : col::muted);
    content.statusTimeout = 30 * 8;
}

void NagareEditor::setOctave (int o)
{
    for (int i = 0; i < 17; ++i)
        if (keyNote[(size_t) i] >= 0) { proc.pushEvent (NagareProcessor::EventType::off, keyNote[(size_t) i]); keyNote[(size_t) i] = -1; }
    proc.octave.store (juce::jlimit (-4, 3, o));
    content.refresh();
}

void NagareEditor::setTranspose (int t)
{
    proc.transpose.store (juce::jlimit (-24, 24, t));
    content.refresh();
}

bool NagareEditor::keyPressed (const juce::KeyPress& key)
{
    if (key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown() || key.getModifiers().isAltDown()) return false;
    if (key == juce::KeyPress::spaceKey)
    {
        if (! spaceDown) { spaceDown = true; proc.pushEvent (NagareProcessor::EventType::pedalDown); }
        return true;
    }
    const auto ch = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
    if (ch > 0 && ch < 128)
    {
        if (const char* f = std::strchr (kKeys, (int) ch))
        {
            const int idx = (int) (f - kKeys);
            if (keyNote[(size_t) idx] < 0)
            {
                const int phys = 60 + proc.octave.load() * 12 + idx;
                keyNote[(size_t) idx] = phys;
                proc.pushEvent (NagareProcessor::EventType::on, phys, 0.8f);
            }
            return true;
        }
        if (const char* f = std::strchr ("zxcv", (int) ch))
        {
            const int idx = (int) (f - "zxcv");
            if (! cmdKeyDown[(size_t) idx])
            {
                cmdKeyDown[(size_t) idx] = true;
                if (idx == 0) setOctave (proc.octave.load() - 1);
                if (idx == 1) setOctave (proc.octave.load() + 1);
                if (idx == 2) setTranspose (proc.transpose.load() - 1);
                if (idx == 3) setTranspose (proc.transpose.load() + 1);
            }
            return true;
        }
    }
    return false;
}

bool NagareEditor::keyStateChanged (bool)
{
    bool used = false;
    for (int i = 0; i < 17; ++i)
        if (keyNote[(size_t) i] >= 0 && ! juce::KeyPress::isKeyCurrentlyDown (kKeys[i]))
        {
            proc.pushEvent (NagareProcessor::EventType::off, keyNote[(size_t) i]);
            keyNote[(size_t) i] = -1;
            used = true;
        }
    for (int i = 0; i < 4; ++i)
        if (cmdKeyDown[(size_t) i] && ! juce::KeyPress::isKeyCurrentlyDown ("zxcv"[i])) cmdKeyDown[(size_t) i] = false;
    if (spaceDown && ! juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::spaceKey))
    {
        spaceDown = false;
        proc.pushEvent (NagareProcessor::EventType::pedalUp);
        used = true;
    }
    return used;
}

void NagareEditor::focusLost (FocusChangeType)
{
    for (int i = 0; i < 17; ++i)
        if (keyNote[(size_t) i] >= 0) { proc.pushEvent (NagareProcessor::EventType::off, keyNote[(size_t) i]); keyNote[(size_t) i] = -1; }
    if (spaceDown) { spaceDown = false; proc.pushEvent (NagareProcessor::EventType::pedalUp); }
    cmdKeyDown.fill (false);
}

bool NagareEditor::isInterestedInFileDrag (const juce::StringArray& files) { return content.wave.isInterestedInFileDrag (files); }
void NagareEditor::filesDropped (const juce::StringArray& files, int, int) { if (! files.isEmpty()) loadSample (juce::File (files[0])); }

void NagareEditor::chooseSample()
{
    chooser = std::make_unique<juce::FileChooser> (jp ("サンプルを選ぶ"), juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg;*.m4a;*.caf");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc) { const auto f = fc.getResult(); if (f != juce::File()) loadSample (f); });
}

void NagareEditor::loadSample (const juce::File& f)
{
    juce::String err;
    if (proc.loadSampleFile (f, err))
    {
        auto s = proc.getSource();
        setStatus (jp ("「") + f.getFileName() + jp ("」を読み込みました") + (s != nullptr && s->pitched ? jp ("(音程 ") + noteName (s->rootMidi) + ")" : jp ("(音程なし → C5扱い)")));
        content.wave.invalidate();
        content.refresh();
    }
    else setStatus (err, true);
}

void NagareEditor::savePreset()
{
    nameDialog = std::make_unique<juce::AlertWindow> (jp ("プリセットを保存"), jp ("名前をつけて保存します。読み込んだサンプルも一緒に保存されます。"),
                                                      juce::MessageBoxIconType::NoIcon, this);
    const int id = content.presets.getSelectedId();
    nameDialog->addTextEditor ("name", id >= 1000 ? content.presets.getText() : juce::String(), jp ("名前"));
    nameDialog->addButton (jp ("保存"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    nameDialog->addButton (jp ("キャンセル"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    nameDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result) {
        if (result != 1 || nameDialog == nullptr) return;
        auto name = juce::File::createLegalFileName (nameDialog->getTextEditorContents ("name").trim());
        if (name.isEmpty()) name = jp ("プリセット ") + juce::String (content.userPresets.size() + 1);
        const auto file = NagareProcessor::presetFolder().getChildFile (name + ".nagare");
        if (proc.savePreset (file))
        {
            content.refreshPresets();
            for (int i = 0; i < content.userPresets.size(); ++i)
                if (content.userPresets[i] == file) content.presets.setSelectedId (1000 + i, juce::dontSendNotification);
            setStatus (jp ("「") + name + jp ("」として保存しました"));
        }
        else setStatus (jp ("保存できませんでした: ") + file.getFullPathName(), true);
    }), false);
}

void NagareEditor::deletePreset()
{
    const int id = content.presets.getSelectedId();
    if (id < 1000) { setStatus (jp ("削除できるのは自分で保存したプリセットだけです。"), true); return; }
    const auto file = content.userPresets[id - 1000];
    juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::QuestionIcon, jp ("プリセットを削除"),
                                      jp ("「") + file.getFileNameWithoutExtension() + jp ("」を削除しますか?"), jp ("削除"), jp ("キャンセル"), this),
        [this, file] (int result) {
            if (result != 1) return;
            file.moveToTrash();
            content.refreshPresets();
            setStatus (jp ("削除しました"));
        });
}

} // namespace nagare::ui
