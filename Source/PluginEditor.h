#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace kcui {

namespace col {
const juce::Colour bg{ 0xff0a0a0b }, masterBg{ 0xff050506 }, ink{ 0xffe6e6e3 }, ink2{ 0xff85858a }, line{ 0xff1f1f22 },
    groove{ 0xff2a2a2e }, cap{ 0xff161618 }, capEdge{ 0xff2e2e33 }, accent{ 0xffff7a1a }, accentInk{ 0xff000000 },
    lcd{ 0xff0e1311 }, lcdEdge{ 0xff16201b }, lcdInk{ 0xffa6e3c3 }, lcdDim{ 0xff4c6659 },
    segOff{ 0xff1b1b1e }, segG{ 0xff35d65a }, segY{ 0xffffcc33 }, segR{ 0xffff3b30 };
}

struct Fonts {
    juce::Typeface::Ptr display, displayBold, body, lcd;
    Fonts();
    juce::Font d(float h, bool bold = false) const;
    juce::Font b(float h) const;
    juce::Font l(float h) const;
};

/* rotary knob drawn like the prototype: ticks, accent arc from the detent, dark cap, light pointer */
class KnobLook : public juce::LookAndFeel_V4 {
public:
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
};

/* small round LED that is also the section's on/off switch */
class LedToggle : public juce::Component {
public:
    LedToggle(juce::RangedAudioParameter& p);
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    juce::ParameterAttachment att; bool on = true;
};

/* row/grid of flat cells bound to a choice parameter */
class Segmented : public juce::Component {
public:
    Segmented(juce::RangedAudioParameter& p, juce::StringArray labels, int columns, const Fonts& f);
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    juce::ParameterAttachment att; juce::StringArray labels; int cols, idx = 0; const Fonts& fonts;
};

/* bypass button with a green light that goes out when bypassed */
class BypassButton : public juce::Component {
public:
    BypassButton(juce::RangedAudioParameter& p, const Fonts& f);
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    juce::ParameterAttachment att; bool bypassed = false; const Fonts& fonts;
};

/* plain text button (Reset, Reset peak) */
class FlatButton : public juce::Component {
public:
    FlatButton(juce::String text, bool lcdStyle, const Fonts& f) : text(std::move(text)), lcdStyle(lcdStyle), fonts(f) { setMouseCursor(juce::MouseCursor::PointingHandCursor); }
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent& e) override { if (contains(e.getPosition()) && onClick) onClick(); }
    std::function<void()> onClick;
private:
    juce::String text; bool lcdStyle; const Fonts& fonts;
};

/* the 900 × 900 panel; the editor scales it to the window */
class Panel : public juce::Component, private juce::Timer {
public:
    explicit Panel(KeirsChainProcessor&);
    ~Panel() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    juce::Slider* makeKnob(juce::Slider& s, const char* id, double detent, std::unique_ptr<SA>& a);
    float val(const char* id) const { return proc.apvts.getRawParameterValue(id)->load(); }
    void tileHeader(juce::Graphics&, juce::Rectangle<int> tile, const juce::String& name) const;
    void knobLabel(juce::Graphics&, juce::Rectangle<int> knob, const juce::String& label, const juce::String& value, bool dim = false) const;

    KeirsChainProcessor& proc; Fonts fonts; KnobLook knobLook;
    juce::Slider swing, shift, trim, bright, freq, outK;
    std::unique_ptr<SA> aSwing, aShift, aTrim, aBright, aFreq, aOut;
    std::unique_ptr<LedToggle> ledTc, ledMk, ledEq, ledXl;
    std::unique_ptr<Segmented> gridSeg, shiftSeg, eqPosSeg;
    std::unique_ptr<BypassButton> bypass;
    FlatButton keyReset, peakReset;

    // meter state
    float mkLevel = -99, xlL = 0, xlR = 0, xlHoldL = 0, xlHoldR = 0, outLevel = -99, outHold = -99; int mkOl = 0, outClip = 0, xlHoldT = 0;
    KeirsChainProcessor::KeySnapshot key;
};

} // namespace kcui

class KeirsChainEditor : public juce::AudioProcessorEditor {
public:
    explicit KeirsChainEditor(KeirsChainProcessor&);
    void resized() override;
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colours::black); }
private:
    kcui::Panel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeirsChainEditor)
};
