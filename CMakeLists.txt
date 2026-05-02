cmake_minimum_required(VERSION 3.24)

project(NebulaJEsser
    VERSION 0.1.0
    DESCRIPTION "Nebula J-Esser 64-bit de-esser by Nebula Audio"
    LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(NebulaCompilerOptions)

option(NJ_FETCH_DEPS "Fetch JUCE and CLAP bridge when local paths are not supplied" ON)
option(NJ_ENABLE_CLAP "Build CLAP using clap-juce-extensions" ON)
option(NJ_BUILD_TESTS "Build DSP validation and stress tests" ON)

set(NJ_JUCE_GIT_TAG "8.0.8" CACHE STRING "JUCE tag used when NJ_FETCH_DEPS is enabled")
set(NJ_CLAP_JUCE_EXTENSIONS_GIT_TAG "main" CACHE STRING "clap-juce-extensions tag/branch")
set(NJ_JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout")
set(NJ_CLAP_EXTENSIONS_PATH "" CACHE PATH "Path to a local clap-juce-extensions checkout")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target")
endif()

include(FetchContent)

if(NJ_JUCE_PATH AND EXISTS "${NJ_JUCE_PATH}/CMakeLists.txt")
    add_subdirectory("${NJ_JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
elseif(NJ_FETCH_DEPS)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG "${NJ_JUCE_GIT_TAG}"
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
else()
    find_package(JUCE CONFIG REQUIRED)
endif()

set(NJ_FORMATS VST3 LV2)
if(APPLE)
    list(APPEND NJ_FORMATS AU)
endif()

add_library(NebulaJEsserCore STATIC)
target_sources(NebulaJEsserCore
    PRIVATE
        src/dsp/Analyzer.cpp
        src/dsp/DeEsserEngine.cpp
        src/dsp/Metrics.cpp)
target_include_directories(NebulaJEsserCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(NebulaJEsserCore PUBLIC cxx_std_20)
target_compile_definitions(NebulaJEsserCore
    PUBLIC
        JUCE_MODAL_LOOPS_PERMITTED=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_USE_STUDIO_ONE_COMPATIBLE_PARAMETERS=1
        JUCER_ENABLE_GPL_MODE=1)
target_link_libraries(NebulaJEsserCore
    PUBLIC
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_events)
nebula_target_warnings(NebulaJEsserCore)
nebula_target_optimizations(NebulaJEsserCore)

juce_add_plugin(NebulaJEsser
    COMPANY_NAME "Nebula Audio"
    COMPANY_COPYRIGHT "Copyright 2026 Nebula Audio"
    COMPANY_EMAIL ""
    COMPANY_WEBSITE ""
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Nbau
    PLUGIN_CODE Njes
    FORMATS ${NJ_FORMATS}
    PRODUCT_NAME "Nebula J-Esser"
    BUNDLE_ID "audio.nebula.NebulaJEsser"
    VST3_CATEGORIES Fx Dynamics EQ
    AU_MAIN_TYPE kAudioUnitType_Effect
    LV2URI "urn:nebula-audio:NebulaJEsser")

target_sources(NebulaJEsser
    PRIVATE
        src/NebulaJEsserEditor.cpp
        src/NebulaJEsserProcessor.cpp
        src/state/Parameters.cpp
        src/ui/Components.cpp
        src/ui/SciFiLookAndFeel.cpp)
target_include_directories(NebulaJEsser PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(NebulaJEsser PRIVATE cxx_std_20)
target_compile_definitions(NebulaJEsser
    PRIVATE
        JUCE_VST3_CAN_REPLACE_VST2=0
        NJ_PLUGIN_NAME="Nebula J-Esser"
        NJ_COMPANY_NAME="Nebula Audio")
target_link_libraries(NebulaJEsser
    PRIVATE
        NebulaJEsserCore
        juce::juce_audio_utils
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
nebula_target_warnings(NebulaJEsser)
nebula_target_optimizations(NebulaJEsser)

if(NJ_ENABLE_CLAP)
    if(NJ_CLAP_EXTENSIONS_PATH AND EXISTS "${NJ_CLAP_EXTENSIONS_PATH}/CMakeLists.txt")
        add_subdirectory("${NJ_CLAP_EXTENSIONS_PATH}" "${CMAKE_BINARY_DIR}/clap-juce-extensions" EXCLUDE_FROM_ALL)
    elseif(NJ_FETCH_DEPS)
        FetchContent_Declare(clap_juce_extensions
            GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
            GIT_TAG "${NJ_CLAP_JUCE_EXTENSIONS_GIT_TAG}"
            GIT_SHALLOW TRUE
            GIT_SUBMODULES_RECURSE TRUE)
        FetchContent_MakeAvailable(clap_juce_extensions)
    endif()

    if(COMMAND clap_juce_extensions_plugin)
        clap_juce_extensions_plugin(TARGET NebulaJEsser
            CLAP_ID "audio.nebula.nebula-j-esser"
            CLAP_FEATURES audio-effect dynamics filter stereo mastering)
    else()
        message(WARNING "CLAP requested, but clap_juce_extensions_plugin() is unavailable. Supply NJ_CLAP_EXTENSIONS_PATH or enable NJ_FETCH_DEPS.")
    endif()
endif()

if(NJ_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
