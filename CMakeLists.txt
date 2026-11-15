cmake_minimum_required(VERSION 3.22)
project(MaxStutter VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.6
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

option(MS_COPY "Plugin nach dem Build installieren" ON)
if(APPLE)
  set(MS_FORMATS VST3 AU)
else()
  set(MS_FORMATS VST3)
endif()

juce_add_plugin(MaxStutter
  COMPANY_NAME "MaxStutter"
  PLUGIN_MANUFACTURER_CODE Mxst
  PLUGIN_CODE Mxs1
  FORMATS ${MS_FORMATS}
  PRODUCT_NAME "Max Stutter"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  COPY_PLUGIN_AFTER_BUILD ${MS_COPY})

target_sources(MaxStutter PRIVATE Source/PluginProcessor.cpp Source/PluginProcessor.h)
target_compile_definitions(MaxStutter PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(MaxStutter
  PRIVATE juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
