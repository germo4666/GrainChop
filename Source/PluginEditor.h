#pragma once
#include "PluginProcessor.h"

namespace gc
{
namespace col
{
    const juce::Colour body { 0xff131415 }, panel { 0xff1b1d1f }, edge { 0xff2c2f32 }, key { 0xff232629 }, screen { 0xff040505 },
                       fg { 0xffdde0da }, mute { 0xff858a84 }, dim { 0xff4b4f4c }, tape { 0xffe8b45a };
}

class GCLook : public juce::LookAndFeel_V4
{
public:
    GCLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (13.0f)); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::Font (juce::FontOptions (12.5f, juce::Font::bold)); }
    juce::Colour accent { 0xffff6b4a };
};

class Pad : public juce::Component
{
public:
    Pad (GrainChopProcessor& p, int i) : proc (p), index (i) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    juce::String label, sub; juce::Colour colour; float level = 0; bool isRoot = false, current = false, down = false;
private:
    GrainChopProcessor& proc; int index;
};

class Screen : public juce::Component
{
public:
    explicit Screen (GrainChopProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void resized() override { overview.clear(); }
    void pull();
    juce::String presetLabel; int pageColour = 0;
private:
    struct Ev { GrainEvent e; double t; };
    GrainChopProcessor& proc;
    std::vector<Ev> evs;
    std::vector<std::pair<float, float>> overview;
    void buildOverview (int w);
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
    void refreshPads();

    GrainChopProcessor& proc;
    gc::GCLook look;
    gc::Screen screen;
    int page = 0;
    juce::OwnedArray<juce::TextButton> tabs;
    std::array<juce::Slider, 8> knobs;
    std::array<juce::Label, 8> knobNames, knobValues;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> knobAtt;
    juce::OwnedArray<gc::Pad> pads;

    juce::ComboBox presetBox, keyBox, scaleBox, chordBox, srcBox;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" }, octDown { "-" }, octUp { "+" };
    juce::Label octLabel, keyHdr, chordHdr, srcHdr, macroHdr, perfHdr;
    juce::TextButton holdBtn { "Hold" }, perPadBtn { "Per pad" }, randomBtn { "Randomize" }, stopBtn { "Stop all" };
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAtt;

    static constexpr int kNumMacros = 7;
    std::array<juce::Slider, kNumMacros> macros;
    std::array<juce::Label, kNumMacros> macroNames;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, kNumMacros> macroAtt;

    int shownPreset = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainChopEditor)
};
