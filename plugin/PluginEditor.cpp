#include "PluginEditor.h"
#include "BinaryData.h"

namespace {

// Design canvas (handoff v5): fixed 1140x660, uniform scale in the page.
constexpr int kBaseW = 1140;
constexpr int kBaseH = 660;

// GUI-bound parameter IDs by relay type. IDs = APVTS IDs (plugin/Params.h);
// the page JS asks for the same ids via getSliderState/getToggleState/
// getComboBoxState. Built by loop (4 tone blocks a_-d_ share suffixes).
juce::StringArray makeSliderIds() {
    juce::StringArray ids;
    for (const char* px : { "a_", "b_", "c_", "d_" })
        for (const char* s : { "coarse", "fine", "level", "velsens", "start",
                               "pan", "dir", "int", "shape_depth",
                               "cutoff", "reso", "env_amt", "keyfollow",
                               "tvf_att", "tvf_dec", "tvf_sus", "tvf_rel",
                               "tva_att", "tva_dec", "tva_sus", "tva_rel",
                               "aux_att", "aux_dec", "aux_sus", "aux_rel",
                               "aux_amt", "lfo_depth" })
            ids.add(juce::String(px) + s);
    for (const char* s : { "vec_phase", "orbit_rate",
                           "penv_start", "penv_end", "penv_time",
                           "glfo_rate",
                           "mod1_amt", "mod2_amt", "mod3_amt",
                           "f1_cutoff", "f1_reso", "f1_env",
                           "f2_cutoff", "f2_reso", "f2_morph",
                           "modfx_rate", "modfx_depth", "modfx_mix",
                           "delay_time", "delay_fb", "delay_mix",
                           "rev_size", "rev_damp", "rev_mix",
                           "volume", "output" })
        ids.add(s);
    return ids;
}

juce::StringArray makeToggleIds() {
    juce::StringArray ids;
    for (const char* px : { "a_", "b_", "c_", "d_" })
        ids.add(juce::String(px) + "on");
    for (const char* s : { "orbit_on", "penv_on", "penv_loop",
                           "modfx_on", "delay_on", "rev_on" })
        ids.add(s);
    return ids;
}

juce::StringArray makeComboIds() {
    juce::StringArray ids;
    for (const char* px : { "a_", "b_", "c_", "d_" })
        for (const char* s : { "wave", "shape_type", "tvf_type",
                               "aux_dest", "lfo_dest" })
            ids.add(juce::String(px) + s);
    for (const char* s : { "glfo_shape",
                           "mod1_src", "mod2_src", "mod3_src",
                           "mod1_dest", "mod2_dest", "mod3_dest",
                           "f1_type", "f2_type", "filt_routing",
                           "modfx_type", "delay_mode", "rev_type",
                           "interp", "engine" })
        ids.add(s);
    return ids;
}

const char* getMimeForExtension(const juce::String& ext) {
    if (ext == "html") return "text/html";
    if (ext == "js")   return "text/javascript";
    if (ext == "css")  return "text/css";
    if (ext == "ttf")  return "font/ttf";
    if (ext == "svg")  return "image/svg+xml";
    if (ext == "png")  return "image/png";
    return "application/octet-stream";
}

} // namespace

//==============================================================================
TheDreamerEditor::TheDreamerEditor(TheDreamerProcessor& p)
    : AudioProcessorEditor(p), processor(p)
{
    const auto sliderIds = makeSliderIds();
    const auto toggleIds = makeToggleIds();
    const auto comboIds  = makeComboIds();

    for (const auto& id : sliderIds)
        sliderRelays.push_back(std::make_unique<juce::WebSliderRelay>(id));
    for (const auto& id : toggleIds)
        toggleRelays.push_back(std::make_unique<juce::WebToggleButtonRelay>(id));
    for (const auto& id : comboIds)
        comboRelays.push_back(std::make_unique<juce::WebComboBoxRelay>(id));

    webView = std::make_unique<juce::WebBrowserComponent>(makeOptions());

    for (int i = 0; i < sliderIds.size(); ++i)
        sliderAttachments.push_back(std::make_unique<juce::WebSliderParameterAttachment>(
            *processor.apvts.getParameter(sliderIds[i]), *sliderRelays[(size_t)i],
            processor.apvts.undoManager));
    for (int i = 0; i < toggleIds.size(); ++i)
        toggleAttachments.push_back(std::make_unique<juce::WebToggleButtonParameterAttachment>(
            *processor.apvts.getParameter(toggleIds[i]), *toggleRelays[(size_t)i],
            processor.apvts.undoManager));
    for (int i = 0; i < comboIds.size(); ++i)
        comboAttachments.push_back(std::make_unique<juce::WebComboBoxParameterAttachment>(
            *processor.apvts.getParameter(comboIds[i]), *comboRelays[(size_t)i],
            processor.apvts.undoManager));

    addAndMakeVisible(*webView);
    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

    setResizable(true, true);
    getConstrainer()->setFixedAspectRatio((double)kBaseW / kBaseH);
    setResizeLimits(kBaseW / 2, kBaseH / 2, kBaseW * 5 / 2, kBaseH * 5 / 2);
    setSize(kBaseW, kBaseH);
}

//==============================================================================
juce::WebBrowserComponent::Options TheDreamerEditor::makeOptions()
{
    auto options = juce::WebBrowserComponent::Options{}
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options(juce::WebBrowserComponent::Options::WinWebView2{}
            .withUserDataFolder(juce::File::getSpecialLocation(juce::File::tempDirectory)))
        .withNativeIntegrationEnabled()
        .withResourceProvider([this](const auto& url) { return getResource(url); })

        // Version + build stamp ("which build is this?" -- house rule)
        .withNativeFunction("getInfo",
            [](const juce::Array<juce::var>&,
               juce::WebBrowserComponent::NativeFunctionCompletion completion)
            {
                auto* obj = new juce::DynamicObject();
                obj->setProperty("version", JucePlugin_VersionString);
                obj->setProperty("build", juce::String(__DATE__) + " " + juce::String(__TIME__));
                completion(juce::var(obj));
            });

    for (auto& r : sliderRelays) options = options.withOptionsFrom(*r);
    for (auto& r : toggleRelays) options = options.withOptionsFrom(*r);
    for (auto& r : comboRelays)  options = options.withOptionsFrom(*r);

    return options;
}

//==============================================================================
std::optional<juce::WebBrowserComponent::Resource>
TheDreamerEditor::getResource(const juce::String& url)
{
    const auto path = url == "/" ? juce::String("editor.html")
                                 : url.fromFirstOccurrenceOf("/", false, false);
    const auto filename = path.fromLastOccurrenceOf("/", false, false);

    // BinaryData by original filename (proven DAT-D8 / rubber-rhino pattern)
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i) {
        if (juce::String(BinaryData::originalFilenames[i]) == filename) {
            int dataSize = 0;
            if (const char* data = BinaryData::getNamedResource(
                    BinaryData::namedResourceList[i], dataSize)) {
                const auto ext = filename.fromLastOccurrenceOf(".", false, false);
                return juce::WebBrowserComponent::Resource {
                    std::vector<std::byte>(reinterpret_cast<const std::byte*>(data),
                                           reinterpret_cast<const std::byte*>(data) + dataSize),
                    getMimeForExtension(ext) };
            }
        }
    }
    return std::nullopt;
}

//==============================================================================
void TheDreamerEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds(getLocalBounds());
}
