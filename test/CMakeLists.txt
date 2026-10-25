cmake_minimum_required(VERSION 3.22)
project(KIZAMI_TEST VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
add_subdirectory(../JUCE juce)

juce_add_console_app(KizamiTest PRODUCT_NAME "KizamiTest")
target_sources(KizamiTest PRIVATE
    TestMain.cpp
    ../Source/Engine.cpp ../Source/DrumSynth.cpp ../Source/Exporter.cpp
    ../Source/PluginProcessor.cpp ../Source/PluginEditor.cpp)
target_compile_definitions(KizamiTest PRIVATE
    JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0
    JucePlugin_Name="Kizami" JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(KizamiTest PRIVATE
    juce::juce_audio_processors juce::juce_audio_formats juce::juce_gui_basics
    juce::juce_recommended_config_flags)
