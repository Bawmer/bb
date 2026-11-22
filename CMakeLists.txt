cmake_minimum_required(VERSION 3.22)
project(BubLeonMixRack VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime: the .vst3 runs without the VC++ redistributable
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

set(MR_FORMATS VST3 Standalone CACHE STRING "Plugin formats to build")

juce_add_plugin(BubLeonMixRack
    COMPANY_NAME              "BL Audio"
    COMPANY_WEBSITE           "https://www.thebonbonz.de"
    PRODUCT_NAME              "Bub Leon Mix Rack"
    PLUGIN_MANUFACTURER_CODE  Bubl
    PLUGIN_CODE               BlMr
    FORMATS                   ${MR_FORMATS}
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES           Fx Dynamics EQ
    COPY_PLUGIN_AFTER_BUILD   FALSE)

file(GLOB MR_SKIN CONFIGURE_DEPENDS Resources/skin/*.jpg)
juce_add_binary_data(MixRackSkin SOURCES ${MR_SKIN})
if(NOT MSVC)
    set_target_properties(MixRackSkin PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
endif()

set(MR_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/GUI/ModuleUnit.cpp
    Source/Presets/FactoryPresets.cpp)

target_sources(BubLeonMixRack PRIVATE ${MR_SOURCES})
target_include_directories(BubLeonMixRack PRIVATE Source)

target_compile_definitions(BubLeonMixRack PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(BubLeonMixRack
    PRIVATE
        MixRackSkin
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# End-to-end rack tests: null test, latency, state recall, all modules and factory presets
option(MR_BUILD_TESTS "Build the rack test" ON)
if(MR_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(rack_test PRODUCT_NAME "rack_test")
    target_sources(rack_test PRIVATE tests/rack_test.cpp ${MR_SOURCES})

    target_include_directories(rack_test PRIVATE Source)
    target_compile_definitions(rack_test PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
                               JucePlugin_Name="Bub Leon Mix Rack" JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(rack_test PRIVATE MixRackSkin juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
    add_test(NAME rack_test COMMAND rack_test)
endif()
