cmake_minimum_required(VERSION 3.22)

# On a Mac, configure with:
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0

project(KIZAMI VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(KIZAMI_FORMATS VST3 AU Standalone)
else()
    set(KIZAMI_FORMATS VST3)
endif()

# JUCE lives next to this file (git clone https://github.com/juce-framework/JUCE.git)
add_subdirectory(JUCE)

juce_add_plugin(Kizami
    COMPANY_NAME "Kizami"
    BUNDLE_ID "com.kizami.beatmaker"
    PRODUCT_NAME "Kizami"
    PLUGIN_MANUFACTURER_CODE Kzmi
    PLUGIN_CODE Kzm1
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Drum
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    MICROPHONE_PERMISSION_ENABLED FALSE
    FORMATS ${KIZAMI_FORMATS})

target_sources(Kizami PRIVATE
    Source/Engine.cpp
    Source/DrumSynth.cpp
    Source/Exporter.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Kizami PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(NOT APPLE)
    # Only affects the Linux verification build (headers not needed at runtime)
    target_compile_definitions(Kizami PUBLIC
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0)
endif()

if(APPLE)
    # The Standalone app needs audio device support
    target_link_libraries(Kizami PRIVATE juce::juce_audio_utils)
endif()

target_link_libraries(Kizami
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
