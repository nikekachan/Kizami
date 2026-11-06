cmake_minimum_required(VERSION 3.22)
project(NAGARE_TEST VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
add_subdirectory(../../JUCE juce)

juce_add_binary_data(NagareTestAssets SOURCES
    ../Assets/astronaut.png ../Assets/Comfortaa-Bold.ttf ../Assets/GreatVibes-Regular.ttf
    ../Assets/jelly.svg ../Assets/fish.svg ../Assets/shell.svg ../Assets/weed.svg
    ../Assets/star.svg ../Assets/octo.svg ../Assets/angler.svg ../Assets/bub.svg)

juce_add_console_app(NagareTest PRODUCT_NAME "NagareTest")
target_sources(NagareTest PRIVATE
    TestMain.cpp
    ../Source/SampleData.cpp ../Source/Synth.cpp ../Source/PluginProcessor.cpp ../Source/PluginEditor.cpp)
target_compile_definitions(NagareTest PRIVATE
    JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0
    JucePlugin_Name="Nagare" JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(NagareTest PRIVATE
    NagareTestAssets
    juce::juce_audio_processors juce::juce_audio_formats juce::juce_dsp juce::juce_gui_basics
    juce::juce_recommended_config_flags)
