cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build universal binaries")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(ButterSpread VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BS_BUILD_TOOLS "Build the offscreen editor snapshot tool" OFF)

add_subdirectory(libs/JUCE)

# Butter Spread: a free stereo widener. Every control is a felt object on a breakfast table.
file(GLOB BS_ART CONFIGURE_DEPENDS Resources/art/*)

juce_add_binary_data(ButterSpreadArt NAMESPACE ButterArt HEADER_NAME ButterArt.h SOURCES ${BS_ART})

juce_add_plugin(ButterSpread
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Plugins for Breakfast"
    PLUGIN_MANUFACTURER_CODE Pfbk
    PLUGIN_CODE Btsp
    FORMATS AU VST3
    PRODUCT_NAME "Butter Spread"
    BUNDLE_ID com.pluginsforbreakfast.butterspread
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Spatial
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(ButterSpread
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Presets.cpp)

target_compile_definitions(ButterSpread
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ButterSpread
    PRIVATE
        ButterSpreadArt
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(BS_BUILD_TOOLS)
    # Offscreen editor snapshot, for checking the table without opening a host
    juce_add_console_app(ButterSpreadSnapshot PRODUCT_NAME "ButterSpreadSnapshot")
    target_sources(ButterSpreadSnapshot PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Presets.cpp tools/Snapshot.cpp)
    target_compile_definitions(ButterSpreadSnapshot PRIVATE JucePlugin_Name="Butter Spread" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(ButterSpreadSnapshot PRIVATE ButterSpreadArt juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
