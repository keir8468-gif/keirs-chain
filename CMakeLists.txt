cmake_minimum_required(VERSION 3.22)

project(KeirsChain VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Universal binary for Apple Silicon + Intel Macs; macOS 11 or later
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

# JUCE: use a local checkout if given (-DJUCE_DIR=...), otherwise fetch the pinned release
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(KC_FORMATS AU VST3)
else()
    set(KC_FORMATS VST3)        # Linux is only used for compile checks
endif()

juce_add_plugin(KeirsChain
    COMPANY_NAME "Keir"
    BUNDLE_ID "com.keir.keirschain"
    PLUGIN_MANUFACTURER_CODE Keir
    PLUGIN_CODE Kcha
    FORMATS ${KC_FORMATS}
    PRODUCT_NAME "Keir's Chain"
    DESCRIPTION "MPC timing, Mackie channel, W 492 shelf and MPC2000XL A/D-D/A"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics EQ
    AU_MAIN_TYPE kAudioUnitType_Effect)

juce_add_binary_data(KeirsChainFonts SOURCES
    Resources/BarlowCondensed-SemiBold.ttf
    Resources/BarlowCondensed-Bold.ttf
    Resources/Barlow-Medium.ttf
    Resources/VT323-Regular.ttf)

target_sources(KeirsChain PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(KeirsChain PRIVATE Source)

target_compile_definitions(KeirsChain PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_ALSA=0
    JUCE_JACK=0)

target_link_libraries(KeirsChain
    PRIVATE
        KeirsChainFonts
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
