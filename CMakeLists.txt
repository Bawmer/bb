cmake_minimum_required(VERSION 3.22)
project(BubLeonHarmonyStack VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime: the .vst3 runs without installing the VC++ redistributable
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Use a local JUCE checkout with -DJUCE_DIR=path, otherwise fetch JUCE 8 from GitHub
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(HS_FORMATS VST3 Standalone CACHE STRING "Plugin formats to build")

juce_add_plugin(BubLeonHarmonyStack
    COMPANY_NAME              "Bub Leon"
    COMPANY_WEBSITE           "https://www.thebonbonz.de"
    PRODUCT_NAME              "Bub Leon Harmony Stack"
    PLUGIN_MANUFACTURER_CODE  Bubl
    PLUGIN_CODE               BlHs
    FORMATS                   ${HS_FORMATS}
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES           Fx "Pitch Shift"
    COPY_PLUGIN_AFTER_BUILD   FALSE)

target_sources(BubLeonHarmonyStack PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(BubLeonHarmonyStack PRIVATE Source)

target_compile_definitions(BubLeonHarmonyStack PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_MP3AUDIOFORMAT=1)

target_link_libraries(BubLeonHarmonyStack
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline DSP tests (no JUCE needed): ctest after building
option(HS_BUILD_TESTS "Build the engine test" ON)
if(HS_BUILD_TESTS)
    enable_testing()
    add_executable(engine_test tests/engine_test.cpp)
    add_test(NAME engine_test COMMAND engine_test)
endif()

# End-to-end processor test (links the plugin's shared code)
option(HS_BUILD_PROCESSOR_TEST "Build the processor end-to-end test" OFF)
if(HS_BUILD_PROCESSOR_TEST)
    juce_add_console_app(processor_test PRODUCT_NAME "processor_test")
    target_sources(processor_test PRIVATE tests/processor_test.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(processor_test PRIVATE Source)
    target_compile_definitions(processor_test PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1
                               JucePlugin_Name="Bub Leon Harmony Stack" JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(processor_test PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
    add_test(NAME processor_test COMMAND processor_test)
endif()
