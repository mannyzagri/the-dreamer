#include "PluginEditor.h"
#include "BinaryData.h"

namespace {

// Design canvas (handoff v11): fixed 1140x864 logical (control panel + the
// keyboard/wheels strip below), uniform scale in the page. 2x assets => the
// PNG master is 2280x1728. (v9 was 1140x660; the handoff's resize section
// still quotes the old 660 -- 864 is authoritative per the "Logical size"
// line and the PNG dimensions.)
constexpr int kBaseW = 1140;
constexpr int kBaseH = 864;

// GUI-bound parameter IDs by relay type. IDs = APVTS IDs (plugin/Params.h);
// the page JS asks for the same ids via getSliderState/getToggleState/
// getComboBoxState. Built by loop (4 tone blocks a_-d_ share suffixes).
// DSP_BUILD.md section-9 ids; per-tone params take suffixes _a.._d
juce::StringArray makeSliderIds() {
    juce::StringArray ids;
    for (const char* sx : { "_a", "_b", "_c", "_d" })
        for (const char* s : { "level", "oct", "fine", "start", "velo", "pan",
                               "shape_depth", "noise", "noise_color",
                               "dir", "vint", "aux_amt",
                               "lfo1_rate", "lfo1_depth", "lfo2_rate", "lfo2_depth",
                               "tvf_cut", "tvf_res", "tvf_env", "tvf_kf",  // (per-tone)
                               "tvf_a", "tvf_d", "tvf_s", "tvf_r",
                               "tva_a", "tva_d", "tva_s", "tva_r",
                               "aux_a", "aux_d", "aux_s", "aux_r" })
            ids.add(juce::String(s) + sx);
    for (const char* s : { "master",
                           "vec_phase", "vec_orbit_rate",
                           "vec_penv_start", "vec_penv_end", "vec_penv_time",
                           "lfo_rate",
                           "mtx1_amt", "mtx2_amt", "mtx3_amt",
                           "flt1_cut", "flt1_res", "flt1_env",
                           "flt2_cut", "flt2_res", "flt2_morph", "flt_bal",
                           "modfx_rate", "modfx_depth", "modfx_mix", "modfx_p0",
                           "dly_time", "dly_fb", "dly_mix", "dly_p0",
                           "rev_size", "rev_damp", "rev_mix", "rev_p0",
                           "drift" })
        ids.add(s);
    return ids;
}

juce::StringArray makeToggleIds() {
    juce::StringArray ids;
    for (const char* sx : { "_a", "_b", "_c", "_d" })
        for (const char* s : { "on", "start_random", "lfo1_sync", "lfo2_sync" })
            ids.add(juce::String(s) + sx);
    for (const char* s : { "vec_orbit_on", "vec_orbit_voice",
                           "vec_penv_on", "vec_penv_loop",
                           "modfx_on", "dly_on", "dly_sync", "rev_on" })
        ids.add(s);
    return ids;
}

juce::StringArray makeComboIds() {
    juce::StringArray ids;
    for (const char* sx : { "_a", "_b", "_c", "_d" })
        for (const char* s : { "wave", "shape", "tvf_type",
                               "aux_dest", "lfo1_dest", "lfo2_dest",
                               "lfo1_shape", "lfo2_shape" })   // v11 per-tone LFO shape
            ids.add(juce::String(s) + sx);
    for (const char* s : { "vec_orbit_shape", "lfo_shape",
                           "mtx1_src", "mtx2_src", "mtx3_src",
                           "mtx1_dst", "mtx2_dst", "mtx3_dst",
                           "flt_route", "flt1_type", "flt2_type",
                           "modfx_type", "dly_mode", "rev_type",
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

    // Cubase-style proportional resize: 50%..200% of the design canvas, fixed
    // aspect; the page's fit() scales the 1140x864 layout uniformly into the
    // host window frame (v11: whole frame incl. screws scales as one unit).
    setResizable(true, true);
    getConstrainer()->setFixedAspectRatio((double)kBaseW / kBaseH);
    setResizeLimits(kBaseW / 2, kBaseH / 2, kBaseW * 2, kBaseH * 2);
    setSize(kBaseW, kBaseH);

    startTimerHz(30);   // output-meter feed
}

TheDreamerEditor::~TheDreamerEditor()
{
    // Editor teardown while audio runs: any note held on the on-screen
    // keyboard loses its pointerup, so flush GUI-originated notes to avoid a
    // hung voice (host MIDI notes are unaffected).
    processor.guiAllNotesOff();
}

//==============================================================================
void TheDreamerEditor::timerCallback()
{
    // push the processor's per-block output peak to the page's header meters;
    // peak-hold + decay live in the GUI (window.uiMeters).
    if (webView != nullptr)
        webView->evaluateJavascript(
            "window.uiMeters && window.uiMeters({l:" + juce::String(processor.getMeterL(), 4)
            + ",r:" + juce::String(processor.getMeterR(), 4) + "});");
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
            })

        // v11 on-screen keyboard + pitch/mod wheels. Called on the message
        // thread; each just enqueues onto the processor's lock-free FIFO
        // (gui* -> drainGuiMidi at the top of processBlock). Args from JS:
        //   noteOn(note 0-127, vel 0-1) / noteOff(note) /
        //   pitchBend(norm -1..+1) / modWheel(w 0-1).
        .withNativeFunction("noteOn",
            [this](const juce::Array<juce::var>& a,
                   juce::WebBrowserComponent::NativeFunctionCompletion completion)
            {
                if (a.size() >= 2) processor.guiNoteOn((int)a[0], (float)(double)a[1]);
                completion(juce::var());
            })
        .withNativeFunction("noteOff",
            [this](const juce::Array<juce::var>& a,
                   juce::WebBrowserComponent::NativeFunctionCompletion completion)
            {
                if (a.size() >= 1) processor.guiNoteOff((int)a[0]);
                completion(juce::var());
            })
        .withNativeFunction("pitchBend",
            [this](const juce::Array<juce::var>& a,
                   juce::WebBrowserComponent::NativeFunctionCompletion completion)
            {
                if (a.size() >= 1) processor.guiPitchBend((float)(double)a[0]);
                completion(juce::var());
            })
        .withNativeFunction("modWheel",
            [this](const juce::Array<juce::var>& a,
                   juce::WebBrowserComponent::NativeFunctionCompletion completion)
            {
                if (a.size() >= 1) processor.guiModWheel((float)(double)a[0]);
                completion(juce::var());
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
