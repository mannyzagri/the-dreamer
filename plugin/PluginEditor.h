// The Dreamer WebView editor: JUCE 8 WebView2 hosting plugin/gui/editor.html
// (BinaryData), design_handoff_dreamer_gui v5 (1140x660 canvas, uniform
// scaling). All panel controls bind via Web*Relay attachments by APVTS ID;
// non-parameter traffic is a single getInfo native fn (version/build stamp).
// House rules: relays constructed BEFORE the WebBrowserComponent, attachments
// after; BinaryData served by original filename; check_native_interop.js
// must ship or all page JS dies silently.
#pragma once
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

class TheDreamerEditor : public juce::AudioProcessorEditor {
public:
    explicit TheDreamerEditor(TheDreamerProcessor&);
    ~TheDreamerEditor() override = default;

    void resized() override;

private:
    juce::WebBrowserComponent::Options makeOptions();
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);

    TheDreamerProcessor& processor;

    std::vector<std::unique_ptr<juce::WebSliderRelay>>       sliderRelays;
    std::vector<std::unique_ptr<juce::WebToggleButtonRelay>> toggleRelays;
    std::vector<std::unique_ptr<juce::WebComboBoxRelay>>     comboRelays;

    std::unique_ptr<juce::WebBrowserComponent> webView;

    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>>       sliderAttachments;
    std::vector<std::unique_ptr<juce::WebToggleButtonParameterAttachment>> toggleAttachments;
    std::vector<std::unique_ptr<juce::WebComboBoxParameterAttachment>>     comboAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TheDreamerEditor)
};
