cmake_minimum_required(VERSION 3.22)

# On a Mac, configure with:
#   cmake -S Nagare -B build-nagare -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0

project(NAGARE VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(NAGARE_FORMATS VST3 AU Standalone)
else()
    set(NAGARE_FORMATS VST3)
endif()

# JUCE is cloned at the repository root (shared with Kizami)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE ${CMAKE_BINARY_DIR}/JUCE)

juce_add_binary_data(NagareAssets SOURCES
    Assets/astronaut.png
    Assets/Comfortaa-Bold.ttf
    Assets/GreatVibes-Regular.ttf
    Assets/jelly.svg
    Assets/fish.svg
    Assets/shell.svg
    Assets/weed.svg
    Assets/star.svg
    Assets/octo.svg
    Assets/angler.svg
    Assets/bub.svg)

juce_add_plugin(Nagare
    COMPANY_NAME "DJ KATSUNII"
    BUNDLE_ID "com.djkatsunii.nagare"
    PRODUCT_NAME "Nagare"
    PLUGIN_MANUFACTURER_CODE Ktsn
    PLUGIN_CODE Ngr1
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth Sampler
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    MICROPHONE_PERMISSION_ENABLED FALSE
    FORMATS ${NAGARE_FORMATS})

target_sources(Nagare PRIVATE
    Source/SampleData.cpp
    Source/Synth.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Nagare PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(NOT APPLE)
    # Linux verification build only
    target_compile_definitions(Nagare PUBLIC JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0)
endif()

if(APPLE)
    target_link_libraries(Nagare PRIVATE juce::juce_audio_utils)
endif()

target_link_libraries(Nagare
    PRIVATE
        NagareAssets
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
