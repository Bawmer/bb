cmake_minimum_required(VERSION 3.24)

project(BubLeonOxfordUltra VERSION 1.0.0 LANGUAGES C CXX)

# Enforce C++20 standard for <numbers> and SIMD alignment
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static MSVC runtime: the .vst3 runs on any Windows 11 machine without VC++ redistributables
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Fetch JUCE 8 automatically if not present in a local ./JUCE subfolder
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# Define the VST3 and Standalone Plugin Target
juce_add_plugin(BubLeonOxfordUltra
    COMPANY_NAME                "BUB LEON"
    COMPANY_COPYRIGHT           "Copyright (c) 2026 BUB LEON Audio"
    PLUGIN_MANUFACTURER_CODE    BubL
    PLUGIN_CODE                 OxUl
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "BL Oxford Ultra Limiter"
    BUNDLE_ID                   com.bubleon.oxfordultralimiter
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE    # set TRUE locally; CI runners can't write to Program Files
    VST3_CATEGORIES             "Fx" "Mastering" "Dynamics"
)

target_sources(BubLeonOxfordUltra
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/OxfordUltraLimiter.h
        Source/LoudnessMeterITU.h
        Source/OxfordHistoryScope.h
        Source/FaceplateLookAndFeel.h
        Source/Presets.h
)

# Faceplate artwork (clean plate: live knobs, scope, HUD and LCD are drawn over it)
juce_add_binary_data(BubLeonOxfordUltraAssets
    SOURCES
        Resources/faceplate.png
)

target_include_directories(BubLeonOxfordUltra
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

# High-performance Windows 11 x64 MSVC / Clang-CL optimization flags
if (MSVC)
    target_compile_options(BubLeonOxfordUltra PRIVATE
        /W4
        /arch:AVX2          # Enables AVX2 & FMA3 instructions for the SIMD True-Peak FIR
        /fp:fast            # Fast floating-point model for DSP loops (ADAA uses explicit double)
        /Oi                 # Generate intrinsic functions
        /Ot                 # Favor fast code
        /permissive-
    )
else()
    target_compile_options(BubLeonOxfordUltra PRIVATE
        -Wall -Wextra -mavx2 -mfma -ffast-math -O3
    )
endif()

target_compile_definitions(BubLeonOxfordUltra
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        NOMINMAX=1
        WIN32_LEAN_AND_MEAN=1
)

target_link_libraries(BubLeonOxfordUltra
    PRIVATE
        BubLeonOxfordUltraAssets
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)
