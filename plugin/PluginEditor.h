// Generic APVTS editor -- the WebView GUI handoff replaces this later.
#pragma once
#include "PluginProcessor.h"

class TheDreamerEditor : public juce::GenericAudioProcessorEditor {
public:
    explicit TheDreamerEditor(TheDreamerProcessor& p)
        : juce::GenericAudioProcessorEditor(p) {}
};
