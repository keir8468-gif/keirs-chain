#include "PluginEditor.h"
#include "BinaryData.h"

namespace kcui {

using juce::String;
static String u8(const char* s) { return String::fromUTF8(s); }
static const String DASH = u8("\xE2\x80\x94"), MINUS = u8("\xE2\x88\x92"), DOT = u8(" \xC2\xB7 ");
static String sgn(double v, int dp = 1) { return (v > 0.0001 ? "+" : (v < -0.0001 ? MINUS : String())) + String(std::abs(v), dp); }

constexpr int W = 900, MAIN = 750, HEAD = 140, TILE_W = 375, TILE_H = 380;

/* ---------------- fonts ---------------- */
Fonts::Fonts()
{
    display = juce::Typeface::createSystemTypefaceFor(BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
    displayBold = juce::Typeface::createSystemTypefaceFor(BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize);
    body = juce::Typeface::createSystemTypefaceFor(BinaryData::BarlowMedium_ttf, BinaryData::BarlowMedium_ttfSize);
    lcd = juce::Typeface::createSystemTypefaceFor(BinaryData::VT323Regular_ttf, BinaryData::VT323Regular_ttfSize);
}
juce::Font Fonts::d(float h, bool bold) const { return juce::Font(juce::FontOptions(bold ? displayBold : display).withHeight(h).withKerningFactor(0.12f)); }
juce::Font Fonts::b(float h) const { return juce::Font(juce::FontOptions(body).withHeight(h)); }
juce::Font Fonts::l(float h) const { return juce::Font(juce::FontOptions(lcd).withHeight(h)); }

/* ---------------- knob ---------------- */
void KnobLook::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const float size = (float)std::min(w, h), sc = size / 100.f;
    const float cx = x + w * 0.5f, cy = y + h * 0.5f;
    const float alpha = s.isEnabled() ? 1.f : 0.35f;
    auto pol = [&](float ang, float r) { return juce::Point<float>(cx + r * sc * std::sin(ang), cy - r * sc * std::cos(ang)); };
    g.setColour(col::ink2.withMultipliedAlpha(alpha));
    for (int i = 0; i <= 10; i++) { const float ang = a0 + (a1 - a0) * i / 10.f; g.drawLine(juce::Line<float>(pol(ang, 44), pol(ang, 48)), 1.6f * sc); }
    const double detent = s.getProperties().getWithDefault("detent", s.getMinimum());
    const float aD = a0 + (float)s.valueToProportionOfLength(detent) * (a1 - a0), a = a0 + pos * (a1 - a0);
    if (std::abs(a - aD) > 0.01f) {
        juce::Path arc; arc.addCentredArc(cx, cy, 39 * sc, 39 * sc, 0, std::min(a, aD), std::max(a, aD), true);
        g.setColour(col::accent.withMultipliedAlpha(alpha)); g.strokePath(arc, juce::PathStrokeType(4 * sc, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour(col::cap); g.fillEllipse(cx - 33 * sc, cy - 33 * sc, 66 * sc, 66 * sc);
    g.setColour(col::capEdge); g.drawEllipse(cx - 33 * sc, cy - 33 * sc, 66 * sc, 66 * sc, 1.5f * sc);
    g.setColour(col::ink.withMultipliedAlpha(alpha));
    g.drawLine(juce::Line<float>({ cx, cy }, pol(a, 26)), 3.5f * sc);
}

/* ---------------- LED toggle ---------------- */
LedToggle::LedToggle(juce::RangedAudioParameter& p)
    : att(p, [this](float v) { on = v > 0.5f; repaint(); })
{
    att.sendInitialUpdate(); setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setTitle(p.getName(64)); setHelpText("On / off");
}
void LedToggle::paint(juce::Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre(); const float r = 4.5f;
    if (on) { g.setColour(col::accent.withAlpha(0.35f)); g.fillEllipse(c.x - r * 2, c.y - r * 2, r * 4, r * 4); }
    g.setColour(on ? col::accent : col::groove); g.fillEllipse(c.x - r, c.y - r, r * 2, r * 2);
}
void LedToggle::mouseUp(const juce::MouseEvent& e) { if (contains(e.getPosition())) att.setValueAsCompleteGesture(on ? 0.f : 1.f); }

/* ---------------- segmented choice ---------------- */
Segmented::Segmented(juce::RangedAudioParameter& p, juce::StringArray l, int columns, const Fonts& f)
    : att(p, [this](float v) { idx = juce::roundToInt(v); repaint(); }), labels(std::move(l)), cols(columns), fonts(f)
{
    att.sendInitialUpdate(); setMouseCursor(juce::MouseCursor::PointingHandCursor);
}
void Segmented::paint(juce::Graphics& g)
{
    const int rows = (labels.size() + cols - 1) / cols; const auto b = getLocalBounds().toFloat();
    const float cw = b.getWidth() / cols, rh = b.getHeight() / rows;
    for (int i = 0; i < labels.size(); i++) {
        const juce::Rectangle<float> cell(b.getX() + (i % cols) * cw, b.getY() + (i / cols) * rh, cw, rh);
        const bool sel = i == idx;
        g.setColour(sel ? col::accent : col::cap); g.fillRect(cell);
        g.setColour(sel ? col::accentInk : col::ink2); g.setFont(fonts.d(13.5f));
        g.drawText(labels[i].toUpperCase(), cell, juce::Justification::centred);
    }
    g.setColour(col::capEdge);
    for (int c = 1; c < cols; c++) g.drawVerticalLine(juce::roundToInt(b.getX() + c * cw), b.getY(), b.getBottom());
    for (int r = 1; r < rows; r++) g.drawHorizontalLine(juce::roundToInt(b.getY() + r * rh), b.getX(), b.getRight());
    g.drawRect(b, 1.f);
}
void Segmented::mouseUp(const juce::MouseEvent& e)
{
    if (!contains(e.getPosition())) return;
    const int rows = (labels.size() + cols - 1) / cols;
    const int c = juce::jlimit(0, cols - 1, (int)(e.x * cols / (float)getWidth())), r = juce::jlimit(0, rows - 1, (int)(e.y * rows / (float)getHeight()));
    const int i = r * cols + c;
    if (i < labels.size()) att.setValueAsCompleteGesture((float)i);
}

/* ---------------- bypass ---------------- */
BypassButton::BypassButton(juce::RangedAudioParameter& p, const Fonts& f)
    : att(p, [this](float v) { bypassed = v > 0.5f; repaint(); }), fonts(f)
{
    att.sendInitialUpdate(); setMouseCursor(juce::MouseCursor::PointingHandCursor); setTitle("Bypass");
}
void BypassButton::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(bypassed ? juce::Colours::black : col::cap); g.fillRoundedRectangle(b, 4);
    g.setColour(col::capEdge); g.drawRoundedRectangle(b, 4, 1);
    const String t = bypassed ? "BYPASSED" : "ACTIVE";
    g.setFont(fonts.d(15.f));
    const float tw = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), t), led = 10, gap = 9;
    const float x0 = b.getCentreX() - (led + gap + tw) / 2, cy = b.getCentreY();
    if (!bypassed) { g.setColour(col::segG.withAlpha(0.35f)); g.fillEllipse(x0 - 5, cy - 10, 20, 20); }
    g.setColour(bypassed ? col::groove : col::segG); g.fillEllipse(x0, cy - led / 2, led, led);
    g.setColour(bypassed ? col::ink2 : col::ink);
    g.drawText(t, juce::Rectangle<float>(x0 + led + gap, b.getY(), tw + 4, b.getHeight()), juce::Justification::centredLeft);
}
void BypassButton::mouseUp(const juce::MouseEvent& e) { if (contains(e.getPosition())) att.setValueAsCompleteGesture(bypassed ? 0.f : 1.f); }

void FlatButton::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced(0.5f);
    if (lcdStyle) { g.setColour(col::lcdInk); g.drawRoundedRectangle(b, 3, 1); g.setFont(fonts.l(16)); }
    else { g.setColour(col::cap); g.fillRoundedRectangle(b, 3); g.setColour(col::capEdge); g.drawRoundedRectangle(b, 3, 1); g.setColour(col::ink); g.setFont(fonts.d(13.f)); }
    g.drawText(text, b, juce::Justification::centred);
}

/* ---------------- panel ---------------- */
juce::Slider* Panel::makeKnob(juce::Slider& s, const char* id, double detent, std::unique_ptr<SA>& a)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    s.setMouseDragSensitivity(220);
    s.setLookAndFeel(&knobLook);
    s.getProperties().set("detent", detent);
    s.setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    addAndMakeVisible(s);
    a = std::make_unique<SA>(proc.apvts, id, s);
    s.setDoubleClickReturnValue(true, detent);
    return &s;
}

Panel::Panel(KeirsChainProcessor& p)
    : proc(p), keyReset("Reset", true, fonts), peakReset("RESET PEAK", false, fonts)
{
    setOpaque(true);
    auto& ap = proc.apvts;
    makeKnob(swing, ParamID::swing, 50, aSwing);
    makeKnob(shift, ParamID::shiftAmt, 0, aShift);
    makeKnob(trim, ParamID::trim, 0, aTrim);
    makeKnob(bright, ParamID::bright, 0, aBright);
    makeKnob(freq, ParamID::brightFreq, 3000, aFreq);
    makeKnob(outK, ParamID::out, 0, aOut);
    ledTc = std::make_unique<LedToggle>(*ap.getParameter(ParamID::tcOn));
    ledMk = std::make_unique<LedToggle>(*ap.getParameter(ParamID::mkOn));
    ledEq = std::make_unique<LedToggle>(*ap.getParameter(ParamID::eqOn));
    ledXl = std::make_unique<LedToggle>(*ap.getParameter(ParamID::xlOn));
    for (auto* l : { ledTc.get(), ledMk.get(), ledEq.get(), ledXl.get() }) addAndMakeVisible(l);
    gridSeg = std::make_unique<Segmented>(*ap.getParameter(ParamID::grid), juce::StringArray{ "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T" }, 3, fonts);
    shiftSeg = std::make_unique<Segmented>(*ap.getParameter(ParamID::shiftDir), juce::StringArray{ "Earlier", "Later" }, 2, fonts);
    eqPosSeg = std::make_unique<Segmented>(*ap.getParameter(ParamID::eqPos), juce::StringArray{ "Before A/D", "After D/A" }, 2, fonts);
    bypass = std::make_unique<BypassButton>(*ap.getParameter(ParamID::bypass), fonts);
    for (juce::Component* c : { (juce::Component*)gridSeg.get(), (juce::Component*)shiftSeg.get(), (juce::Component*)eqPosSeg.get(), (juce::Component*)bypass.get() }) addAndMakeVisible(c);
    keyReset.onClick = [this] { proc.resetKey(); };
    peakReset.onClick = [this] { outHold = -99; outClip = 0; xlHoldL = xlHoldR = 0; repaint(); };
    addAndMakeVisible(keyReset); addAndMakeVisible(peakReset);
    setSize(W, W);                     // last, so resized() only runs once every control exists
    startTimerHz(30);
}

Panel::~Panel()
{
    for (auto* s : { &swing, &shift, &trim, &bright, &freq, &outK }) s->setLookAndFeel(nullptr);
}

void Panel::resized()
{
    // tile origins: MPC timing (0,HEAD), Mackie (TILE_W,HEAD), W492 (0,HEAD+TILE_H), 2000XL (TILE_W,HEAD+TILE_H)
    const int t0x = 0, t1x = TILE_W, ty0 = HEAD, ty1 = HEAD + TILE_H;
    for (auto [led, x, y] : { std::tuple{ ledTc.get(), t0x, ty0 }, { ledMk.get(), t1x, ty0 }, { ledEq.get(), t0x, ty1 }, { ledXl.get(), t1x, ty1 } })
        led->setBounds(x + 8, y + 10, 24, 24);
    // MPC timing
    gridSeg->setBounds(t0x + 22, ty0 + 104, 331, 56);
    swing.setBounds(t0x + 105, ty0 + 168, 76, 76);
    shift.setBounds(t0x + 205, ty0 + 175, 62, 62);
    shiftSeg->setBounds(t0x + 22, ty0 + 302, 200, 30);
    // Mackie
    trim.setBounds(t1x + 145, ty0 + 72, 86, 86);
    // W 492
    bright.setBounds(t0x + 82, ty1 + 80, 86, 86);
    freq.setBounds(t0x + 222, ty1 + 88, 70, 70);
    eqPosSeg->setBounds(t0x + 40, ty1 + 236, 295, 30);
    // header + master strip
    keyReset.setBounds(MAIN - 16 - 252 + 186, 14 + 84, 56, 22);
    outK.setBounds(MAIN + 39, 46, 72, 72);
    peakReset.setBounds(MAIN + 22, 744, 106, 28);
    bypass->setBounds(MAIN + 12, 800, 126, 44);
}

void Panel::tileHeader(juce::Graphics& g, juce::Rectangle<int> tile, const String& name) const
{
    g.setColour(col::ink); g.setFont(fonts.d(15.f));
    g.drawText(name, tile.getX() + 36, tile.getY() + 10, tile.getWidth() - 48, 24, juce::Justification::centredLeft);
}

void Panel::knobLabel(juce::Graphics& g, juce::Rectangle<int> k, const String& label, const String& value, bool dim) const
{
    g.setColour(col::ink2.withMultipliedAlpha(dim ? 0.5f : 1.f)); g.setFont(fonts.d(13.f));
    g.drawText(label, k.getCentreX() - 70, k.getBottom() + 2, 140, 16, juce::Justification::centred);
    g.setColour(col::ink.withMultipliedAlpha(dim ? 0.5f : 1.f)); g.setFont(fonts.b(13.f));
    g.drawText(value, k.getCentreX() - 70, k.getBottom() + 19, 140, 16, juce::Justification::centred);
}

static void lcdBox(juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour(col::lcd); g.fillRoundedRectangle(r, 3); g.setColour(col::lcdEdge); g.drawRoundedRectangle(r, 3, 1);
}

void Panel::paint(juce::Graphics& g)
{
    using J = juce::Justification;
    g.fillAll(col::bg);

    // ---------- header ----------
    g.setColour(col::ink); g.setFont(fonts.d(32.f, true));
    g.drawText(u8("KEIR\xE2\x80\x99S CHAIN"), 18, 16, 440, 40, J::centredLeft);
    g.setColour(col::ink2); g.setFont(fonts.d(12.f));
    g.drawText(u8("MPC TIMING \xE2\x86\x92 MACKIE \xE2\x86\x92 W 492 \xE2\x86\x92 2000XL"), 19, 58, 440, 18, J::centredLeft);

    const juce::Rectangle<float> kb((float)(MAIN - 16 - 252), 14.f, 252.f, 112.f);
    lcdBox(g, kb);
    {
        const float x = kb.getX() + 12, y = kb.getY();
        g.setFont(fonts.l(18)); g.setColour(col::lcdDim);
        g.drawText("KEY", (int)x, (int)y + 8, 60, 24, J::centredLeft);
        g.drawText("CHORD", (int)x, (int)y + 54, 60, 24, J::centredLeft);
        String keyText = DASH, camelot, alt, state;
        if (!key.keyValid) state = key.heard > 0 ? u8("Listening\xE2\x80\xA6 ") + String((int)key.heard) + " s" : "Waiting for audio";
        else {
            keyText = key.key; camelot = key.camelot;
            if (key.margin < 0.04) alt = "or " + key.alt + DOT.trim() + " " + key.altCamelot;
            const int t = juce::roundToInt(key.tuning * 100);
            state = String(key.silent ? "Holding" : "Live") + (std::abs(t) >= 5 ? DOT + "tuning " + sgn(t, 0) + " c" : String());
        }
        g.setColour(col::lcdInk); g.setFont(fonts.l(27));
        g.drawText(keyText, (int)x + 58, (int)y + 5, 140, 30, J::centredLeft);
        g.drawText(key.chordValid ? key.chord : DASH, (int)x + 58, (int)y + 51, 170, 30, J::centredLeft);
        if (camelot.isNotEmpty()) {
            const juce::Rectangle<float> cb(kb.getRight() - 50, y + 9, 38, 24);
            g.drawRoundedRectangle(cb, 3, 1.5f); g.setFont(fonts.l(22)); g.drawText(camelot, cb, J::centred);
        }
        g.setColour(col::lcdDim); g.setFont(fonts.l(16));
        if (alt.isNotEmpty()) g.drawText(alt, (int)x + 58, (int)y + 33, 180, 18, J::centredLeft);
        g.drawText(state, (int)x, (int)y + 86, 176, 18, J::centredLeft);
    }

    // ---------- grid lines ----------
    g.setColour(col::line);
    g.drawHorizontalLine(HEAD, 0, (float)MAIN);
    g.drawHorizontalLine(HEAD + TILE_H, 0, (float)MAIN);
    g.drawVerticalLine(TILE_W, (float)HEAD, (float)W);
    g.drawVerticalLine(MAIN, 0, (float)W);

    const juce::Rectangle<int> tTc(0, HEAD, TILE_W, TILE_H), tMk(TILE_W, HEAD, TILE_W, TILE_H), tEq(0, HEAD + TILE_H, TILE_W, TILE_H), tXl(TILE_W, HEAD + TILE_H, TILE_W, TILE_H);
    tileHeader(g, tTc, "MPC TIMING"); tileHeader(g, tMk, "MACKIE CHANNEL"); tileHeader(g, tEq, "W 492 HIGH SHELF"); tileHeader(g, tXl, u8("2000XL A/D \xC2\xB7 D/A"));
    auto dimIfOff = [&](const char* id, juce::Rectangle<int> t) { if (val(id) < 0.5f) { g.setColour(col::bg.withAlpha(0.55f)); g.fillRect(t.withTrimmedTop(40).reduced(1)); } };

    // ---------- MPC timing ----------
    {
        const int gi = juce::roundToInt(val(ParamID::grid)); static const int ticks[] = { 48, 32, 24, 16, 12, 8 };
        static const char* gname[] = { "1/8", "1/8(3)", "1/16", "1/16(3)", "1/32", "1/32(3)" };
        const bool swingable = gi == 0 || gi == 2; const int sw = juce::roundToInt(val(ParamID::swing));
        const int maxShift = ticks[gi] / 2, sh = std::min(maxShift, juce::roundToInt(val(ParamID::shiftAmt)));
        const bool earlier = juce::roundToInt(val(ParamID::shiftDir)) == 0;
        swing.setEnabled(swingable);
        const juce::Rectangle<float> lc(22.f, (float)HEAD + 46, 331.f, 50.f); lcdBox(g, lc);
        g.setFont(fonts.l(18)); g.setColour(col::lcdInk);
        g.drawText(String("Note value:") + gname[gi], (int)lc.getX() + 9, (int)lc.getY() + 4, 200, 20, J::centredLeft);
        if (swingable) g.drawText("Swing%:" + String(sw), (int)lc.getRight() - 129, (int)lc.getY() + 4, 120, 20, J::centredRight);
        g.drawText(String("Shift timing:") + (earlier ? "EARLIER" : "LATER"), (int)lc.getX() + 9, (int)lc.getY() + 25, 200, 20, J::centredLeft);
        g.drawText("amount:" + String(sh).paddedLeft(' ', 2), (int)lc.getRight() - 129, (int)lc.getY() + 25, 120, 20, J::centredRight);
        knobLabel(g, swing.getBounds(), "SWING", swingable ? String(sw) + "%" : DASH, !swingable);
        knobLabel(g, shift.getBounds(), "SHIFT", String(sh) + " tk");
        // tempo comes from Live
        const auto br = juce::Rectangle<float>(232.f, (float)HEAD + 302, 121.f, 30.f); lcdBox(g, br);
        g.setColour(proc.hostBpmKnown() ? col::lcdInk : col::lcdDim); g.setFont(fonts.l(19));
        g.drawText(String(proc.getHostBpm(), 1) + " BPM", br, J::centred);
        const int et = proc.getEvenTicks();
        g.setColour(col::ink2); g.setFont(fonts.b(12.f));
        const double msT = 60000.0 / (proc.getHostBpm() * 96);
        g.drawText(et ? "+" + String(et) + " tk" + DOT + String(et * msT, 1) + " ms on even notes" : String("Straight"), 22, HEAD + 340, 331, 16, J::centred);
        dimIfOff(ParamID::tcOn, tTc);
    }

    // ---------- Mackie ----------
    {
        const float d = val(ParamID::trim);
        knobLabel(g, trim.getBounds(), "TRIM", std::abs(d) < 0.25f ? String("U") : sgn(d) + " dB");
        static const float LAD[] = { -20, -10, -5, 0, 4, 7, 10, 13, 16, 19, 21, 22.2f };
        static const char* LADL[] = { "-20", "-10", "-5", "0", "+4", "+7", "+10", "+13", "+16", "+19", "+21", "OL" };
        const float x0 = TILE_W + 40, y0 = HEAD + 216, wSeg = (295 - 11 * 4) / 12.f;
        for (int i = 0; i < 12; i++) {
            const bool on = i == 11 ? mkOl > 0 : mkLevel >= LAD[i];
            const auto c = i < 7 ? col::segG : (i < 10 ? col::segY : col::segR);
            const juce::Rectangle<float> r(x0 + i * (wSeg + 4), (float)y0, wSeg, 11);
            if (on) { g.setColour(c.withAlpha(0.3f)); g.fillRoundedRectangle(r.expanded(2), 3); }
            g.setColour(on ? c : col::segOff); g.fillRoundedRectangle(r, 2);
            g.setColour(col::ink2); g.setFont(fonts.b(10.f));
            g.drawText(LADL[i], r.withY(r.getBottom() + 5).withHeight(12).expanded(4, 0), J::centred);
        }
        dimIfOff(ParamID::mkOn, tMk);
    }

    // ---------- W 492 ----------
    {
        knobLabel(g, bright.getBounds(), "BRIGHTNESS", sgn(val(ParamID::bright)) + " dB");
        knobLabel(g, freq.getBounds(), "FREQ", String(val(ParamID::brightFreq) / 1000.f, 1) + " kHz");
        dimIfOff(ParamID::eqOn, tEq);
    }

    // ---------- 2000XL ----------
    {
        const juce::Rectangle<float> lc((float)TILE_W + 30, (float)HEAD + TILE_H + 118, 315.f, 122.f); lcdBox(g, lc);
        const bool on = val(ParamID::xlOn) > 0.5f, byp = val(ParamID::bypass) > 0.5f;
        g.setFont(fonts.l(19)); g.setColour(col::lcdInk);
        const int lx = (int)lc.getX() + 10, ly = (int)lc.getY() + 6;
        g.drawText("Input:ANALOG", lx, ly, 160, 22, J::centredLeft);
        g.drawText(byp ? "BYPASS" : (on ? "READY" : "OFF"), (int)lc.getRight() - 110, ly, 100, 22, J::centredRight);
        auto meter = [&](int row, const char* lab, float v, float hold) {
            const int y = ly + 28 + row * 28;
            g.setColour(col::lcdInk); g.drawText(lab, lx, y, 70, 22, J::centredLeft);
            const juce::Rectangle<float> bar((float)lx + 70, (float)y + 5, lc.getRight() - lx - 80, 13);
            g.drawRect(bar, 1.f);
            auto pos = [](float lin) { const float db = 20 * std::log10(std::max(lin, 1e-6f)); return juce::jlimit(0.f, 1.f, (db + 60) / 60); };
            const float wFill = (bar.getWidth() - 4) * pos(v);
            for (float xx = 0; xx + 5 <= wFill; xx += 7) g.fillRect(bar.getX() + 2 + xx, bar.getY() + 2, 5.f, bar.getHeight() - 4);
            g.fillRect(bar.getX() + 2 + (bar.getWidth() - 4) * pos(hold) - 1, bar.getY() - 3, 2.f, bar.getHeight() + 6);
            if (hold >= 1) g.drawRect(bar.expanded(2.5f), 1.5f);
        };
        meter(0, "LEFT :", xlL, xlHoldL); meter(1, "RIGHT:", xlR, xlHoldR);
        g.setColour(col::lcdDim);
        g.drawText("16BIT 44.1kHz", lx, ly + 86, 160, 22, J::centredLeft);
        const float ph = std::max(xlHoldL, xlHoldR);
        g.drawText(ph >= 1 ? String("PEAK OVER") : "PEAK " + (ph > 0 ? sgn(20 * std::log10(ph)) + "dB" : String("--")), (int)lc.getRight() - 160, ly + 86, 150, 22, J::centredRight);
        dimIfOff(ParamID::xlOn, tXl);
    }

    // ---------- master strip ----------
    {
        g.setColour(col::masterBg); g.fillRect(MAIN + 1, 0, W - MAIN - 1, W);
        g.setColour(col::ink2); g.setFont(fonts.d(14.f));
        g.drawText("MASTER", MAIN, 18, W - MAIN, 20, J::centred);
        knobLabel(g, outK.getBounds(), "OUTPUT", sgn(val(ParamID::out)) + " dB");
        static const float OUTL[] = { -48, -42, -36, -30, -24, -18, -15, -12, -9, -6, -4, -3, -2, -1, -0.5f, 0 };
        static const char* OUTN[] = { "-48", "-42", "-36", "-30", "-24", "-18", "-15", "-12", "-9", "-6", "-4", "-3", "-2", "-1", "-.5", "CLIP" };
        const float top = 162, bottom = 690, segH = (bottom - top - 15 * 3) / 16.f, mx = MAIN + 50;
        for (int i = 0; i < 16; i++) {
            const bool on = i == 15 ? outClip > 0 : outLevel >= OUTL[i];
            const auto c = OUTL[i] < -12 ? col::segG : (OUTL[i] < -3 ? col::segY : col::segR);
            const juce::Rectangle<float> r(mx, bottom - (i + 1) * segH - i * 3, 26, segH);
            if (on) { g.setColour(c.withAlpha(0.3f)); g.fillRoundedRectangle(r.expanded(2), 3); }
            g.setColour(on ? c : col::segOff); g.fillRoundedRectangle(r, 2);
            g.setColour(col::ink2); g.setFont(fonts.b(10.f));
            g.drawText(OUTN[i], (int)mx + 32, (int)r.getY(), 40, (int)segH, J::centredLeft);
        }
        g.setColour(outHold >= 0 ? col::segR : col::ink); g.setFont(fonts.b(13.f));
        g.drawText(outHold <= -98 ? MINUS + u8("\xE2\x88\x9E dBFS") : (outHold >= 0 ? "OVER " : "") + sgn(outHold) + " dBFS", MAIN, 712, W - MAIN, 20, J::centred);
    }
}

void Panel::timerCallback()
{
    const auto m = proc.takeMeters();
    // Mackie ladder (dBu at the preamp output) and OL hold
    const float dbu = m.mkClip > 0 ? 20 * std::log10(m.mkClip * 14.5f / (0.775f * std::sqrt(2.f))) : -99.f;
    mkLevel = std::max(dbu, mkLevel - 2.1f);
    if (m.mkClip >= 1) mkOl = 13; else if (mkOl > 0) mkOl--;
    // 2000XL screen
    xlL = m.xlL; xlR = m.xlR;
    xlHoldL = std::max(xlHoldL, m.xlL); xlHoldR = std::max(xlHoldR, m.xlR);
    // master meter
    const float d = m.out > 0 ? 20 * std::log10(m.out) : -99.f;
    outLevel = std::max(d, outLevel - 2.1f); outHold = std::max(outHold, d);
    if (m.out >= 1) outClip = 17; else if (outClip > 0) outClip--;
    key = proc.getKey();
    repaint();
}

} // namespace kcui

KeirsChainEditor::KeirsChainEditor(KeirsChainProcessor& p)
    : AudioProcessorEditor(&p), panel(p)
{
    addAndMakeVisible(panel);
    setResizable(true, true);
    setResizeLimits(540, 540, 1100, 1100);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio(1.0);
    setSize(720, 720);
}

void KeirsChainEditor::resized()
{
    const float s = getWidth() / (float)kcui::W;
    panel.setTransform(juce::AffineTransform::scale(s));
    panel.setBounds(0, 0, kcui::W, kcui::W);
}
