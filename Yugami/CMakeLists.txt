cmake_minimum_required(VERSION 3.22)

# On a Mac, configure with:
#   cmake -S Yugami -B build-yugami -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0

project(YUGAMI VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(YUGAMI_FORMATS VST3 AU Standalone)
else()
    set(YUGAMI_FORMATS VST3)
endif()

# JUCE is cloned at the repository root (shared with Kizami / Nagare)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE ${CMAKE_BINARY_DIR}/JUCE)

juce_add_binary_data(YugamiAssets SOURCES
    Assets/astronaut.png
    Assets/Comfortaa-Bold.ttf
    Assets/factory.json)

juce_add_plugin(Yugami
    COMPANY_NAME "DJ KATSUNII"
    BUNDLE_ID "com.djkatsunii.yugami"
    PRODUCT_NAME "YUGAMI"
    PLUGIN_MANUFACTURER_CODE Ktsn
    PLUGIN_CODE Ygm1
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Delay Dynamics
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "YUGAMI (standalone) uses the audio input to process live sound."
    FORMATS ${YUGAMI_FORMATS})

target_sources(Yugami PRIVATE
    Source/Bank.cpp
    Source/Engine.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Yugami PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(NOT APPLE)
    target_compile_definitions(Yugami PUBLIC JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0)
endif()

if(APPLE)
    target_link_libraries(Yugami PRIVATE juce::juce_audio_utils)
endif()

target_link_libraries(Yugami
    PRIVATE
        YugamiAssets
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
