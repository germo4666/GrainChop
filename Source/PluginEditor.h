#pragma once
#include "PluginProcessor.h"

// ============================================================================
//  GrainChop UI: OP-1 inspired. Light aluminium body, black animated screen,
//  four-colour encoders, chunky keys.
// ============================================================================
namespace gc
{
namespace ui
{
    const juce::Colour bodyTop { 0xffe7e7e3 }, bodyBot { 0xffd3d3cf }, keyFace { 0xfff3f3f0 }, keyEdge { 0xffbdbdb8 },
                       ink { 0xff26262a }, inkSoft { 0xff8c8c8a }, screen { 0xff000000 },
                       blue { 0xff3d73e0 }, green { 0xff35b56e }, white { 0xfff2f2f0 }, orange { 0xfff2671f };
    inline juce::Colour encoderColour (int i) { static const juce::Colour c[4] = { blue, green, white, orange }; return c[i % 4]; }
    inline juce::Colour screenColour (int i) { static const juce::Colour c[4] = { blue, green, juce::Colour (0xffd8d8d8), orange }; return c[i % 4]; }
}

class Look : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
};

// A square OP-1 style key. Momentary or toggle, with an eased press animation.
class Key : public juce::Component
{
public:
    juce::String label, glyph;
    std::function<void()> onPress, onRelease;
    std::function<bool()> isLit;
    juce::Colour litColour { ui::orange };
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { down = true; if (onPress) onPress(); }
    void mouseUp (const juce::MouseEvent&) override { down = false; if (onRelease) onRelease(); }
    void tick();
    bool down = false; float press = 0, glow = 0, glowTarget = 0; juce::Colour glowColour { ui::orange };
};

struct Particle { float x, y, vx, vy, life, maxLife, size; juce::Colour c; };

class Screen : public juce::Component
{
public:
    explicit Screen (GrainChopProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void tick (double now);
    void showValue (const juce::String& name, const juce::String& value, juce::Colour c);
    void setPage (int p) { if (p != page) { lastPage = page; page = p; pageT = 0; } }
private:
    GrainChopProcessor& proc;
    int page = 0, lastPage = 0; float pageT = 1;
    double now = 0, last = 0;
    std::vector<Particle> parts;
    struct Ring { float r, life, maxLife; };
    std::vector<Ring> rings;
    float reelAngle = 0, spin = 0, pumpViz = 0;
    juce::String popName, popValue; juce::Colour popColour; float popT = 0;
    std::array<float, 12> pcGlow {};

    void drawPage (juce::Graphics&, juce::Rectangle<float>, int pg, float alpha);
    void drawNote (juce::Graphics&, juce::Rectangle<float>);
    void drawGrain (juce::Graphics&, juce::Rectangle<float>);
    void drawColor (juce::Graphics&, juce::Rectangle<float>);
    void drawRepeat (juce::Graphics&, juce::Rectangle<float>);
    void drawDirt (juce::Graphics&, juce::Rectangle<float>);
    void drawSpace (juce::Graphics&, juce::Rectangle<float>);
    void drawTape (juce::Graphics&, juce::Rectangle<float>);
    void drawMassive (juce::Graphics&, juce::Rectangle<float>);
};
} // namespace gc

class GrainChopEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GrainChopEditor (GrainChopProcessor&);
    ~GrainChopEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setPage (int page);
    void stepPreset (int d);

    GrainChopProcessor& proc;
    gc::Look look;
    gc::Screen screen;
    int page = 0;
    bool reattaching = false;
    std::array<juce::Slider, 8> knobs;
    std::array<float, 8> knobAnim {};
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> knobAtt;
    juce::OwnedArray<gc::Key> pageKeys, funcKeys, pads;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainChopEditor)
};
