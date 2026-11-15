cmake_minimum_required(VERSION 3.22)

project(DeepTrance VERSION 1.0.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime: end users do not need the VC++ redistributable,
# a common cause of "plugin failed to load" in FL Studio and Serato.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(DT_BUILD_PLUGIN "Build the JUCE plugin targets (VST3 + CLAP + Standalone)" ON)
option(DT_BUILD_CLAP   "Add the CLAP format (clap-juce-extensions)" ON)
option(DT_BUILD_TESTS  "Build the DSP unit tests" ON)

# ---------------------------------------------------------------------------
# DSP core: plain C++, no JUCE. Unit-tested on its own; the plugin wraps it.
# ---------------------------------------------------------------------------
add_library(DeepTranceDSP STATIC
    Source/DSP/TranceGate.cpp
    Source/DSP/VowelFilter.cpp
    Source/DSP/Swirl.cpp
    Source/DSP/Drift.cpp
    Source/DSP/AcidFilter.cpp
    Source/DSP/BuildUp.cpp
    Source/DSP/Punch.cpp
    Source/DSP/Cloud.cpp
    Source/DSP/FractalDelay.cpp
    Source/DSP/AbyssReverb.cpp
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

    # JUCE has no built-in CLAP, so CLAP comes from clap-juce-extensions.
    # Point DT_CLAP_JUCE_PATH at a local checkout to skip the download.
    if(DT_BUILD_CLAP)
        set(DT_CLAP_JUCE_PATH "" CACHE PATH "Optional local clap-juce-extensions checkout")
        if(DT_CLAP_JUCE_PATH)
            add_subdirectory(${DT_CLAP_JUCE_PATH} clap-juce-extensions EXCLUDE_FROM_ALL)
        else()
            include(FetchContent)
            # Pinned commit (same as Compass, Starfire and the rest of the family):
            # the 0.26.0 release predates JUCE 8.0.11's juce_audio_processors_headless split.
            FetchContent_Declare(clap-juce-extensions
                GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
                GIT_TAG        7adee3a1bd4684d4caa5601100e364abccff4b4d
                GIT_SUBMODULES_RECURSE ON)
            FetchContent_MakeAvailable(clap-juce-extensions)
        endif()
    endif()

    juce_add_plugin(DeepTrance
        COMPANY_NAME              "LowKei Audio"    # vendor shown in DAW plugin lists (display only)
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

    # CLAP wrapper (FL Studio, Bitwig, Reaper). The ID is stored in every host
    # project that uses the plugin, so never change it once shipped.
    if(DT_BUILD_CLAP)
        clap_juce_extensions_plugin(TARGET DeepTrance
            CLAP_ID       "com.lowkeiaudio.deeptrance"
            CLAP_FEATURES audio-effect multi-effects filter delay reverb chorus)
    endif()

    target_sources(DeepTrance PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Params/ParameterLayout.cpp
        Source/Presets/PresetManager.cpp
        Source/Presets/FactoryPresets.cpp
        Source/UI/Controls.cpp
        Source/UI/EasyView.cpp
        Source/UI/ProView.cpp)

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

    # Offline preset check: renders every factory preset through the real processor.
    if(DT_BUILD_TESTS)
        juce_add_console_app(DeepTranceRender PRODUCT_NAME "DeepTranceRender")
        target_sources(DeepTranceRender PRIVATE
            Tests/PresetRender.cpp
            Source/PluginProcessor.cpp
            Source/PluginEditor.cpp
            Source/Params/ParameterLayout.cpp
            Source/Presets/PresetManager.cpp
            Source/Presets/FactoryPresets.cpp
            Source/UI/Controls.cpp
            Source/UI/EasyView.cpp
            Source/UI/ProView.cpp)
        target_include_directories(DeepTranceRender PRIVATE Source)
        target_compile_definitions(DeepTranceRender PRIVATE
            JucePlugin_Name="DeepTrance"
            JucePlugin_VersionString="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0)
        target_link_libraries(DeepTranceRender PRIVATE
            DeepTranceDSP
            juce::juce_audio_utils
            juce::juce_recommended_config_flags)
    endif()
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
