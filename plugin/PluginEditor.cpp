#include "PluginEditor.h"
#include "BinaryData.h"

namespace {

// Design canvas (handoff v13): 1140x660 control panel (collapsed, DEFAULT)
// expanding to 1140x848 when the keyboard/wheels strip folds out. 2x assets =>
// the PNG master is 2280x1320 (collapsed). The page scales the fixed layout
// uniformly into the host window; fold flips the base height 660<->848.
constexpr int kBaseW = 1140;
constexpr int kBaseH = 848;      // expanded (keyboard visible)
constexpr int kFoldedH = 660;    // v13: keyboard collapsed (control panel + rubber band) -- DEFAULT

// GUI-bound parameter IDs by relay type. IDs = APVTS IDs (plugin/Params.h);
// the page JS asks for the same ids via getSliderState/getToggleState/
// getComboBoxState. Built by loop (4 tone blocks a_-d_ share suffixes).
// DSP_BUILD.md section-9 ids; per-tone params take suffixes _a.._d
// v15 production face (design_handoff_dreamer_gui) -- lists mirror app.js's
// GLOBAL_ID/TONE_ID + KIND/TONE_KIND binding contract EXACTLY (per relay type).
juce::StringArray makeSliderIds() {
    juce::StringArray ids;
    for (const char* sx : { "_a", "_b", "_c", "_d" })
        for (const char* s : { "level", "oct", "semi", "fine", "start", "velo", "pan",
                               "shape_depth", "noise", "noise_color", "dir", "vint",
                               "aux_amt", "hit_stretch", "hit_pitchtrim",
                               "loop_rate", "detune_amount",
                               "lfo1_rate", "lfo1_depth", "lfo2_rate", "lfo2_depth",
                               "tvf_cut", "tvf_res", "tvf_env", "tvf_kf",
                               "tvf_a", "tvf_d", "tvf_s", "tvf_r",
                               "tva_a", "tva_d", "tva_s", "tva_r",
                               "aux_a", "aux_d", "aux_s", "aux_r" })
            ids.add(juce::String(s) + sx);
    for (const char* s : { "master", "vec_phase", "vec_orbit_rate",
                           "vec_penv_start", "vec_penv_end", "vec_penv_time",
                           "lfo_rate", "lfo2_rate",   // v16: global LFO 1 + 2
                           "mtx1_amt", "mtx2_amt", "mtx3_amt",
                           "flt1_cut", "flt1_res", "flt1_env",
                           "flt2_cut", "flt2_res", "flt2_env", "flt2_morph", "flt_bal",
                           // v15 FX: one PARAMS proxy knob per slot (focus-shadow)
                           "modfx_rate", "modfx_depth", "modfx_mix", "modfx_param",
                           "dly_time", "dly_fb", "dly_mix", "dly_param",
                           "rev_size", "rev_damp", "rev_mix", "rev_param",
                           // LO-FI raw (GUI focus-routes to these) + inert lofi_param;
                           // WIDTH; TALK proxy param
                           "lofi_bits", "lofi_srate", "lofi_compand", "lofi_alias", "lofi_param",
                           "width", "width_haas", "talk_param",
                           // D5 global offsets, D8 g-octave
                           "g_env_a", "g_env_d", "g_env_s", "g_env_r",
                           "g_cutoff", "g_res", "g_octave" })
        ids.add(s);
    return ids;
}

juce::StringArray makeToggleIds() {
    juce::StringArray ids;
    for (const char* sx : { "_a", "_b", "_c", "_d" })
        for (const char* s : { "on", "start_random", "lfo1_sync", "lfo2_sync",
                               "loop_rate_sync", "loop_varispeed" })
            ids.add(juce::String(s) + sx);
    for (const char* s : { "flt_route", "fx_prepost",              // v15: now Bool
                           "vec_orbit_on", "vec_orbit_voice",
                           "vec_penv_on", "vec_penv_loop",
                           "lfo_sync", "lfo2_sync",   // v16 global LFO 1 + 2 sync
                           "modfx_on", "dly_on", "dly_sync", "rev_on",
                           "lofi_on", "width_on", "width_bassmono", "talk_on",
                           "limiter_on" })
        ids.add(s);
    return ids;
}

juce::StringArray makeComboIds() {
    juce::StringArray ids;
    for (const char* sx : { "_a", "_b", "_c", "_d" })
        for (const char* s : { "wave", "shape", "tvf_type",
                               "aux_dest", "lfo1_dest", "lfo2_dest",
                               "lfo1_shape", "lfo2_shape",
                               "voicing", "dreamy_spread", "loop_mode", "hit_play",
                               "loop_rate_beats", "detune_voices" })  // v15: now Choice
            ids.add(juce::String(s) + sx);
    for (const char* s : { "vec_orbit_shape", "lfo_shape", "lfo2_shape",  // v16 G-LFO 1 + 2
                           "mtx1_src", "mtx2_src", "mtx3_src",
                           "mtx1_dst", "mtx2_dst", "mtx3_dst",
                           "flt1_type", "flt2_type",
                           "modfx_type", "dly_mode", "rev_type",
                           // FX PARAMS focus selectors (incl v15 lofi/talk)
                           "modfx_pfocus", "dly_pfocus", "rev_pfocus",
                           "lofi_pfocus", "talk_pfocus" })
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
    // aspect; the page's fit() scales the fixed layout uniformly into the host
    // window frame (whole frame incl. screws scales as one unit). v13: boot
    // COLLAPSED (660) to match the master + the page's default fold state; the
    // KEYS fold pill expands to 848 via keyboardFold() -> setKeyboardFolded().
    keyboardFolded_ = true;
    setResizable(true, true);
    getConstrainer()->setFixedAspectRatio((double)kBaseW / kFoldedH);   // 1140/660
    setResizeLimits(kBaseW / 2, kFoldedH / 2, kBaseW * 2, kFoldedH * 2); // 570,330,2280,1320
    setSize(kBaseW, kFoldedH);

    startTimerHz(30);   // output-meter feed
}

TheDreamerEditor::~TheDreamerEditor()
{
    // Editor teardown while audio runs: any note held on the on-screen
    // keyboard loses its pointerup, so flush GUI-originated notes to avoid a
    // hung voice (host MIDI notes are unaffected).
    processor.guiAllNotesOff();
}

void TheDreamerEditor::setKeyboardFolded(bool folded)
{
    if (folded == keyboardFolded_) return;
    keyboardFolded_ = folded;
    const int baseH = folded ? kFoldedH : kBaseH;   // new logical height
    const int w = getWidth();
    getConstrainer()->setFixedAspectRatio((double)kBaseW / (double)baseH);
    setResizeLimits(kBaseW / 2, baseH / 2, kBaseW * 2, baseH * 2);
    setSize(w, juce::roundToInt((double)w * (double)baseH / (double)kBaseW));
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

    // Audit B9: the page's header preset name staled on host-side program
    // changes (Cubase preset menu) — it only tracked GUI-initiated loads.
    // Push {index,name} whenever the program moves; -1 forces the first push.
    // TD-010: the latch ALSO fires when the EFFECTIVE display name changes
    // (a user-preset load / session restore keeps the program index but the
    // name is the truth). Effective name = loadedUserPresetName if non-empty,
    // else the factory program name; user:true/false flags which.
    if (webView != nullptr)
    {
        const int prog = processor.getCurrentProgram();
        const juce::String& userName = processor.getLoadedUserPresetName();
        const bool user = userName.isNotEmpty();
        const juce::String effective = user ? userName : processor.getProgramName(prog);
        if (prog != lastPushedProgram_ || effective != lastPushedName_)
        {
            lastPushedProgram_ = prog;
            lastPushedName_    = effective;
            const auto name = effective
                                  .replace("\\", "\\\\").replace("'", "\\'");
            webView->evaluateJavascript(
                "window.uiProgram && window.uiProgram({index:" + juce::String(prog)
                + ",name:'" + name + "',user:" + (user ? "true" : "false") + "});");
        }
    }
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
        // TD-010: also reports the EFFECTIVE preset name (user preset name if
        // one is loaded, else the factory program name) + index + user flag,
        // so the page header is right on boot before the first uiProgram push.
        // Additive fields -- existing consumers of version/build unaffected.
        .withNativeFunction("getInfo",
            [this](const juce::Array<juce::var>&,
               juce::WebBrowserComponent::NativeFunctionCompletion completion)
            {
                auto* obj = new juce::DynamicObject();
                obj->setProperty("version", JucePlugin_VersionString);
                obj->setProperty("build", juce::String(__DATE__) + " " + juce::String(__TIME__));
                const int prog = processor.getCurrentProgram();
                const juce::String& userName = processor.getLoadedUserPresetName();
                const bool user = userName.isNotEmpty();
                obj->setProperty("presetIndex", prog);
                obj->setProperty("presetName", user ? userName : processor.getProgramName(prog));
                obj->setProperty("presetUser", user);
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
            })
        // v12: KEYS fold button toggles the collapsible keyboard. Native fns
        // run on the message thread, so resizing the editor here is safe; the
        // host window follows via the constrainer + setSize.
        .withNativeFunction("keyboardFold",
            [this](const juce::Array<juce::var>& a,
                   juce::WebBrowserComponent::NativeFunctionCompletion completion)
            {
                setKeyboardFolded(a.size() >= 1 && (bool)a[0]);
                completion(juce::var());
            })

        // Factory preset bank (processor-owned). getPresetList mirrors getInfo:
        // it returns the bank as a JSON array of {name,category} so the page can
        // populate its preset browser from the processor instead of a hardcoded
        // JS array. loadPreset(index) drives recall through the processor's
        // standard program interface; the APVTS relays then update every control
        // on the panel automatically. Both run on the message thread.
        .withNativeFunction("getPresetList",
            [this](const juce::Array<juce::var>&,
                   juce::WebBrowserComponent::NativeFunctionCompletion completion)
            {
                completion(processor.getPresetList());
            })
        // Bank-authoritative wave list {category,name,tag} so the wave overlay
        // shows the REAL loop/hit names (PAD_01, HIT_CHIFF, ...) not placeholders.
        .withNativeFunction("getWaveList",
            [this](const juce::Array<juce::var>&,
                   juce::WebBrowserComponent::NativeFunctionCompletion completion)
            {
                completion(processor.getWaveList());
            })
        .withNativeFunction("loadPreset",
            [this](const juce::Array<juce::var>& a,
                   juce::WebBrowserComponent::NativeFunctionCompletion completion)
            {
                // GUI preset list is 0-based over the FACTORY bank; call
                // applyPreset directly (setCurrentProgram is offset by the D4
                // synthetic INIT at host program 0).
                if (a.size() >= 1) processor.applyPreset((int)a[0]);
                completion(juce::var());
            })
        // D3 analyzer tap: last N final-output samples for the GUI FFT.
        .withNativeFunction("getScopeData",
            [this](const juce::Array<juce::var>& a,
                   juce::WebBrowserComponent::NativeFunctionCompletion completion)
            {
                completion(processor.getScopeData(a.size() >= 1 ? (int)a[0] : 2048));
            })
        // D12 output-limiter gain reduction (dB) for the LIM activity LED.
        .withNativeFunction("getLimiterGR",
            [this](const juce::Array<juce::var>&,
                   juce::WebBrowserComponent::NativeFunctionCompletion completion)
            {
                completion(juce::var(processor.getLimiterGR()));
            })
        // D13 SOUND OFF panic (all notes off + FX flush).
        .withNativeFunction("panic",
            [this](const juce::Array<juce::var>&,
                   juce::WebBrowserComponent::NativeFunctionCompletion completion)
            {
                processor.panic();
                completion(juce::var());
            })
        // D14 user preset bank (factory stays read-only via getPresetList).
        .withNativeFunction("getUserPresetList",
            [this](const juce::Array<juce::var>&,
                   juce::WebBrowserComponent::NativeFunctionCompletion completion)
            {
                completion(processor.getUserPresetList());
            })
        .withNativeFunction("saveUserPreset",
            [this](const juce::Array<juce::var>& a,
                   juce::WebBrowserComponent::NativeFunctionCompletion completion)
            {
                if (a.size() >= 1) processor.saveUserPreset(a[0].toString());
                completion(juce::var());
            })
        .withNativeFunction("renameUserPreset",
            [this](const juce::Array<juce::var>& a,
                   juce::WebBrowserComponent::NativeFunctionCompletion completion)
            {
                if (a.size() >= 2) processor.renameUserPreset(a[0].toString(), a[1].toString());
                completion(juce::var());
            })
        .withNativeFunction("deleteUserPreset",
            [this](const juce::Array<juce::var>& a,
                   juce::WebBrowserComponent::NativeFunctionCompletion completion)
            {
                if (a.size() >= 1) processor.deleteUserPreset(a[0].toString());
                completion(juce::var());
            })
        .withNativeFunction("loadUserPreset",
            [this](const juce::Array<juce::var>& a,
                   juce::WebBrowserComponent::NativeFunctionCompletion completion)
            {
                if (a.size() >= 1) processor.loadUserPreset(a[0].toString());
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
