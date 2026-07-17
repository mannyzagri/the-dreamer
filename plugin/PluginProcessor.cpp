// Placeholder processor -- replaced in the port/plugin phase.
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorEditor* TheDreamerProcessor::createEditor() {
    return new TheDreamerEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new TheDreamerProcessor();
}
