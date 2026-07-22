# host_probe -- TD-001 throwaway diagnostic host (loads the real VST3 and
# renders long offline sessions). Standalone project so the plugin's root
# CMakeLists stays untouched. Reuses the repo's JUCE source cache.
#
# Configure/build (from C:\the-dreamer):
#   cmake -B build-hostprobe -G "Visual Studio 17 2022" -A x64 tools/hostprobe
#   cmake --build build-hostprobe --config Release
# Run:
#   build-hostprobe\host_probe_artefacts\Release\host_probe.exe ["path\to\The Dreamer.vst3"]

cmake_minimum_required(VERSION 3.22)
project(HostProbe VERSION 0.0.1)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory("C:/the-dreamer/deps/JUCE" "${CMAKE_BINARY_DIR}/JUCE")

juce_add_console_app(host_probe PRODUCT_NAME "host_probe")
target_sources(host_probe PRIVATE main.cpp)
target_compile_definitions(host_probe PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)
target_link_libraries(host_probe PRIVATE
    juce::juce_audio_processors
    juce::juce_events
    juce::juce_core
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
