cmake_minimum_required(VERSION 3.22)
project(MotionFX VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JUCE_DIR "" CACHE PATH "Path to a JUCE checkout")
if (JUCE_DIR)
    add_subdirectory("${JUCE_DIR}" JUCE)
elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.13
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(MotionFX
    COMPANY_NAME "Prototype"
    PLUGIN_MANUFACTURER_CODE Prot
    PLUGIN_CODE Mfx1
    FORMATS VST3
    PRODUCT_NAME "MotionFX"
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_SYNTH FALSE
    VST3_CATEGORIES Fx
)

target_sources(MotionFX PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetManager.cpp
)

target_compile_definitions(MotionFX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(MotionFX PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_audio_processors
)

add_executable(MotionFXAudioTest
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetManager.cpp
    Source/Tests/AudioIntegrityTest.cpp
)
target_compile_definitions(MotionFXAudioTest PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
target_link_libraries(MotionFXAudioTest PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_gui_extra
)

juce_add_gui_app(MotionFXGuiTest
    PRODUCT_NAME "MotionFX GUI Test"
)

target_sources(MotionFXGuiTest PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetManager.cpp
    Source/Tests/GuiResizeTest.cpp
)
target_compile_definitions(MotionFXGuiTest PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
target_link_libraries(MotionFXGuiTest PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_gui_extra
)
