cmake_minimum_required(VERSION 3.22)
project(TheDreamer VERSION 0.6.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(TheDreamer
    PLUGIN_MANUFACTURER_CODE Mnsh
    PLUGIN_CODE Drmr
    COMPANY_NAME "Menashe Audio"
    BUNDLE_ID com.menasheaudio.thedreamer
    PRODUCT_NAME "The Dreamer"
    FORMATS VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(TheDreamerAssets SOURCES
    plugin/gui/editor.html
    plugin/gui/style.css
    plugin/gui/app.js
    plugin/gui/index.js
    plugin/gui/check_native_interop.js
    plugin/gui/BarlowSemiCondensed-SemiBold.ttf
    plugin/gui/BarlowSemiCondensed-Bold.ttf
    plugin/gui/Doto-Variable.ttf
    plugin/gui/Michroma-Regular.ttf
)

target_sources(TheDreamer PRIVATE
    plugin/PluginProcessor.cpp
    plugin/PluginEditor.cpp
)

target_include_directories(TheDreamer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(TheDreamer PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    RHINO_TEST_FEATURES=1
    _USE_MATH_DEFINES=1)

target_link_libraries(TheDreamer
    PRIVATE TheDreamerAssets
            juce::juce_audio_utils
            juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)
