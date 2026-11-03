#pragma once
#include "Engine.h"

class GrainChopProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener,
                           private juce::Timer
{
public:
    GrainChopProcessor();
    ~GrainChopProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GrainChop"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return gc::kNumPresets; }
    int getCurrentProgram() override { return currentPreset.load(); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- shared with the editor
    juce::AudioProcessorValueTreeState apvts;
    gc::Engine engine;
    juce::MidiMessageCollector uiMidi;          // pads clicked in the plugin window (channel 16)
    std::array<std::atomic<int>, 16> padSrc {};
    std::atomic<int> lastPad { 0 }, currentPreset { 0 };
    std::atomic<bool> presetDirty { false };
    float param (int i) const { return raw[(size_t) i]->load(); }
    gc::Music music() const;
    void setParam (int i, float plainValue);
    void randomizePadSources();
    void stopAll() { stopRequest.store (true); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String& id, float) override;
    void timerCallback() override;
    void handleNote (int trigger, int note, float velocity, bool fromPads);

    std::array<std::atomic<float>*, gc::kNumParams> raw {};
    std::array<float, gc::kNumParams> p {};
    double internalPpq = 0;
    float modwheel = 0;
    std::atomic<bool> stopRequest { false }, applyingPreset { false };
    int lastSeenSrc = -1, lastSyncedPad = -1;
    double sr = 44100;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainChopProcessor)
};
