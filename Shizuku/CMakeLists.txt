cmake_minimum_required(VERSION 3.22)

# On a Mac, configure with:
#   cmake -S Shizuku -B build-shizuku -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0

project(SHIZUKU VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(SHIZUKU_FORMATS VST3 AU Standalone)
else()
    set(SHIZUKU_FORMATS VST3)
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE ${CMAKE_BINARY_DIR}/JUCE)

juce_add_binary_data(ShizukuAssets SOURCES
    Assets/astronaut.png
    Assets/Comfortaa-Bold.ttf)

# zstd single-file decoder (BSD) for Serum 2 presets
add_library(shizuku_zstd STATIC Source/zstd/zstddeclib.c)
target_compile_definitions(shizuku_zstd PRIVATE ZSTD_DISABLE_ASM=1)
set_target_properties(shizuku_zstd PROPERTIES POSITION_INDEPENDENT_CODE ON)

juce_add_plugin(Shizuku
    COMPANY_NAME "DJ KATSUNII"
    BUNDLE_ID "com.djkatsunii.shizuku"
    PRODUCT_NAME "SHIZUKU"
    PLUGIN_MANUFACTURER_CODE Ktsn
    PLUGIN_CODE Shz1
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS ${SHIZUKU_FORMATS})

target_sources(Shizuku PRIVATE
    Source/Patch.cpp
    Source/Tables.cpp
    Source/Engine.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Shizuku PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(NOT APPLE)
    target_compile_definitions(Shizuku PUBLIC JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0)
endif()

target_link_libraries(Shizuku
    PRIVATE
        ShizukuAssets
        shizuku_zstd
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
