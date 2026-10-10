cmake_minimum_required(VERSION 3.22)

project(DeepTrance VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime: end users do not need the VC++ redistributable,
# a common cause of "plugin failed to load" in FL Studio and Serato.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(DT_BUILD_PLUGIN "Build the JUCE plugin targets (VST3 + Standalone)" ON)
option(DT_BUILD_TESTS  "Build the DSP unit tests" ON)

# ---------------------------------------------------------------------------
# DSP core: plain C++, no JUCE. Unit-tested on its own; the plugin wraps it.
# ---------------------------------------------------------------------------
add_library(DeepTranceDSP STATIC
    Source/DSP/TranceGate.cpp
    Source/DSP/VowelFilter.cpp
    Source/DSP/Swirl.cpp
    Source/DSP/Drift.cpp
    Source/DSP/DeepTranceEngine.cpp)

target_include_directories(DeepTranceDSP PUBLIC Source)
set_target_properties(DeepTranceDSP PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(DeepTranceDSP PRIVATE /W4)
else()
    target_compile_options(DeepTranceDSP PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Plugin (JUCE 8)
# ---------------------------------------------------------------------------
if(DT_BUILD_PLUGIN)
    # Point DT_JUCE_PATH at a local JUCE checkout to skip the download.
    set(DT_JUCE_PATH "" CACHE PATH "Optional local JUCE checkout")

    if(DT_JUCE_PATH)
        add_subdirectory(${DT_JUCE_PATH} JUCE)
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG        8.0.15
            GIT_SHALLOW    ON)
        FetchContent_MakeAvailable(JUCE)
    endif()

    juce_add_plugin(DeepTrance
        COMPANY_NAME              "LowKei"
        PLUGIN_MANUFACTURER_CODE  Lwki
        PLUGIN_CODE               Dptr
        PRODUCT_NAME              "DeepTrance"
        FORMATS                   VST3 Standalone
        IS_SYNTH                  FALSE
        NEEDS_MIDI_INPUT          FALSE
        NEEDS_MIDI_OUTPUT         FALSE
        IS_MIDI_EFFECT            FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES           Fx Modulation
        COPY_PLUGIN_AFTER_BUILD   FALSE)

    target_sources(DeepTrance PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Params/ParameterLayout.cpp)

    target_compile_definitions(DeepTrance PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(DeepTrance
        PRIVATE
            DeepTranceDSP
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(DT_BUILD_TESTS)
    enable_testing()
    add_executable(DeepTranceTests Tests/DspTests.cpp)
    target_link_libraries(DeepTranceTests PRIVATE DeepTranceDSP)
    add_test(NAME DeepTranceDSP COMMAND DeepTranceTests)
endif()
