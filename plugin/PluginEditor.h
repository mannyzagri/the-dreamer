// The Dreamer WebView editor: JUCE 8 WebView2 hosting plugin/gui/editor.html
// (BinaryData), design-handoff v13 (1140x660 control panel, collapsible to
// 848 with the keyboard/wheels strip; uniform scaling). Panel controls bind via Web*Relay
// attachments by APVTS ID; non-parameter traffic is getInfo (version/build
// stamp) plus the v11 keyboard/wheel native fns (noteOn/noteOff/pitchBend/
// modWheel -> processor lock-free MIDI FIFO).
// House rules: relays constructed BEFORE the WebBrowserComponent, attachments
// after; BinaryData served by original filename; check_native_interop.js
// must ship or all page JS dies silently.
#pragma once
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

class TheDreamerEditor : public juce::AudioProcessorEditor,
                         private juce::Timer {
public:
    explicit TheDreamerEditor(TheDreamerProcessor&);
    ~TheDreamerEditor() override;

    void resized() override;

private:
    void timerCallback() override;   // 30 Hz output-meter feed to the page
    void setKeyboardFolded(bool folded);   // v12 collapsible keyboard (host resize)
    bool keyboardFolded_ = true;           // v13: boot collapsed (660); set in ctor too
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
