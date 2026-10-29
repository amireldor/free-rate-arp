cmake_minimum_required(VERSION 3.22)
project(FreeRateArp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.13
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(FreeRateArp
    COMPANY_NAME "Amir Audio"
    BUNDLE_ID "com.amiraudio.freeratearp"
    PLUGIN_MANUFACTURER_CODE AmrA
    PLUGIN_CODE FrAr
    FORMATS VST3
    PRODUCT_NAME "Free Rate Arp"
    DESCRIPTION "Free-running MIDI note repeater"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(FreeRateArp PRIVATE Source/PluginProcessor.cpp Source/PluginProcessor.h)
target_compile_definitions(FreeRateArp PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(FreeRateArp PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
