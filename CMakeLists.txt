cmake_minimum_required(VERSION 3.22)
project(GrainChop VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

if(APPLE)
  set(GC_FORMATS VST3 AU Standalone)
else()
  set(GC_FORMATS VST3 Standalone)
endif()

juce_add_plugin(GrainChop
  COMPANY_NAME "Maahy"
  BUNDLE_ID com.maahy.grainchop
  PLUGIN_MANUFACTURER_CODE Mahy
  PLUGIN_CODE Grch
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  FORMATS ${GC_FORMATS}
  VST3_CATEGORIES Instrument Synth
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  PRODUCT_NAME "GrainChop")

juce_add_binary_data(GrainChopData SOURCES Resources/sources.wav)

target_sources(GrainChop PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(GrainChop PRIVATE Source)

target_compile_definitions(GrainChop PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(GrainChop
  PRIVATE GrainChopData juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

option(GC_TESTS "Build the offline render test" OFF)
set(GC_TEST_MAIN Test/main.cpp CACHE STRING "test entry")
if(GC_TESTS)
  juce_add_console_app(GCTest PRODUCT_NAME "GCTest")
  target_sources(GCTest PRIVATE ${GC_TEST_MAIN} Source/PluginProcessor.cpp Source/PluginEditor.cpp)
  target_include_directories(GCTest PRIVATE Source)
  target_compile_definitions(GCTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="GrainChop")
  target_link_libraries(GCTest PRIVATE GrainChopData juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
