cmake_minimum_required(VERSION 3.22)
project(YUGAMI_TEST VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
add_subdirectory(../../JUCE juce)

juce_add_binary_data(YugamiTestAssets SOURCES ../Assets/astronaut.png ../Assets/Comfortaa-Bold.ttf ../Assets/factory.json)

juce_add_console_app(YugamiTest PRODUCT_NAME "YugamiTest")
target_sources(YugamiTest PRIVATE
    TestMain.cpp
    ../Source/Bank.cpp ../Source/Engine.cpp ../Source/PluginProcessor.cpp ../Source/PluginEditor.cpp)
target_compile_definitions(YugamiTest PRIVATE
    JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0
    JucePlugin_Name="YUGAMI" JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(YugamiTest PRIVATE
    YugamiTestAssets
    juce::juce_audio_processors juce::juce_gui_basics
    juce::juce_recommended_config_flags)
