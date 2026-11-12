cmake_minimum_required(VERSION 3.22)
project(SHIZUKU_TEST VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
add_subdirectory(../../JUCE juce)
juce_add_binary_data(ShizukuTestAssets SOURCES ../Assets/astronaut.png ../Assets/Comfortaa-Bold.ttf)
add_library(tzstd STATIC ../Source/zstd/zstddeclib.c)
target_compile_definitions(tzstd PRIVATE ZSTD_DISABLE_ASM=1)
juce_add_console_app(ShizukuTest PRODUCT_NAME "ShizukuTest")
target_sources(ShizukuTest PRIVATE TestMain.cpp ../Source/Patch.cpp ../Source/Tables.cpp ../Source/Engine.cpp ../Source/PluginProcessor.cpp ../Source/PluginEditor.cpp)
target_compile_definitions(ShizukuTest PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALSA=0 JUCE_JACK=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0
    JucePlugin_Name="SHIZUKU" JucePlugin_IsSynth=1 JucePlugin_WantsMidiInput=1 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(ShizukuTest PRIVATE ShizukuTestAssets tzstd juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_basics juce::juce_recommended_config_flags)
