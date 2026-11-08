cmake_minimum_required(VERSION 3.22)
project(KANADE_TEST VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
add_subdirectory(../../JUCE juce)
juce_add_binary_data(KanadeTestAssets SOURCES ../Assets/astronaut.png ../Assets/Comfortaa-Bold.ttf)
juce_add_console_app(KanadeTest PRODUCT_NAME "KanadeTest")
target_sources(KanadeTest PRIVATE TestMain.cpp ../Source/Core.cpp ../Source/PluginProcessor.cpp ../Source/PluginEditor.cpp)
target_compile_definitions(KanadeTest PRIVATE
    JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0
    JucePlugin_Name="KANADE" JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(KanadeTest PRIVATE KanadeTestAssets juce::juce_audio_processors juce::juce_audio_formats juce::juce_gui_basics juce::juce_recommended_config_flags)
