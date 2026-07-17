// Placeholder processor -- replaced in the port/plugin phase.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

class TheDreamerProcessor : public juce::AudioProcessor {
public:
    TheDreamerProcessor()
        : juce::AudioProcessor(BusesProperties().withOutput(
              "Output", juce::AudioChannelSet::stereo(), true)) {}

    const juce::String getName() const override { return "The Dreamer"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override {
        buffer.clear();
    }
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TheDreamerProcessor)
};
