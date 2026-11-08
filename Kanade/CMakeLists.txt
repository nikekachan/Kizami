cmake_minimum_required(VERSION 3.22)

# On a Mac, configure with:
#   cmake -S Kanade -B build-yugami -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0

project(KANADE VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(KANADE_FORMATS VST3 AU Standalone)
else()
    set(KANADE_FORMATS VST3)
endif()

# JUCE is cloned at the repository root (shared with Kizami / Nagare)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE ${CMAKE_BINARY_DIR}/JUCE)

juce_add_binary_data(KanadeAssets SOURCES
    Assets/astronaut.png
    Assets/Comfortaa-Bold.ttf)

juce_add_plugin(Kanade
    COMPANY_NAME "DJ KATSUNII"
    BUNDLE_ID "com.djkatsunii.kanade"
    PRODUCT_NAME "KANADE"
    PLUGIN_MANUFACTURER_CODE Ktsn
    PLUGIN_CODE Knd1
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Pitch Shift
    AU_MAIN_TYPE kAudioUnitType_Effect
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "KANADE (standalone) records the vocal from the audio input."
    FORMATS ${KANADE_FORMATS})

target_sources(Kanade PRIVATE
    Source/Core.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Kanade PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_MP3AUDIOFORMAT=1)

if(NOT APPLE)
    target_compile_definitions(Kanade PUBLIC JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0)
endif()

if(APPLE)
    target_link_libraries(Kanade PRIVATE juce::juce_audio_utils)
endif()

target_link_libraries(Kanade
    PRIVATE
        KanadeAssets
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
