cmake_minimum_required(VERSION 3.22)
project(DC6 VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- JUCE: use a local copy if present, otherwise download it ----
set(JUCE_LOCAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" CACHE PATH "Optional local JUCE checkout")
if(EXISTS "${JUCE_LOCAL_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_LOCAL_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

option(DC6_BUILD_TESTS "Build the offline render test" OFF)

set(DC6_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets.cpp
    Source/dsp/Slicer.cpp)

set(DC6_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

set(DC6_LIBS
    juce::juce_audio_utils
    juce::juce_dsp)

juce_add_plugin(DC6
    COMPANY_NAME "TRANCE-6"
    PLUGIN_MANUFACTURER_CODE Trc6
    PLUGIN_CODE Dc6x
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "DECONSTRUCT-6"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Distortion
    COPY_PLUGIN_AFTER_BUILD FALSE)

# Embedded artwork (the header logo)
juce_add_binary_data(DC6Data SOURCES Resources/logo.png)

target_sources(DC6 PRIVATE ${DC6_SOURCES})
target_compile_definitions(DC6 PUBLIC ${DC6_DEFS})
target_link_libraries(DC6
    PRIVATE ${DC6_LIBS} DC6Data
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

if(DC6_BUILD_TESTS)
    juce_add_console_app(DC6Render PRODUCT_NAME "DC6Render")
    target_sources(DC6Render PRIVATE ${DC6_SOURCES} tools/RenderTest.cpp)
    target_compile_definitions(DC6Render PRIVATE ${DC6_DEFS} JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(DC6Render PRIVATE ${DC6_LIBS} DC6Data juce::juce_recommended_config_flags)
endif()
