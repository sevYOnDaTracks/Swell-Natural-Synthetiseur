cmake_minimum_required(VERSION 3.22)

# Mac: universal binaries (Apple Silicon + Intel). Must be set before project().
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(SwellNaturalSynthetiser VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# JUCE: local checkout if present, otherwise fetched.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

option(SWELL_BUILD_PLUGIN "Build the Swell Natural Synthetiser plug-in" ON)
option(SWELL_BUILD_BANK_BUILDER "Build the Swell Library bank builder tool" ON)
option(SWELL_COPY_AFTER_BUILD "Install built plug-ins in the user plug-in folders" ${APPLE})

# ---------------------------------------------------------------- bank builder
if(SWELL_BUILD_BANK_BUILDER)
    juce_add_console_app(SwellBankBuilder PRODUCT_NAME "SwellBankBuilder")
    target_sources(SwellBankBuilder PRIVATE tools/BankBuilder.cpp)
    target_compile_definitions(SwellBankBuilder PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_FLAC=1)
    target_link_libraries(SwellBankBuilder PRIVATE
        juce::juce_audio_formats
        juce::juce_recommended_config_flags)
endif()

# ---------------------------------------------------------------- plug-in
if(SWELL_BUILD_PLUGIN)
    set(SWELL_FORMATS Standalone VST3)
    if(APPLE)
        list(APPEND SWELL_FORMATS AU)
    endif()

    juce_add_plugin(SwellNaturalSynth
        ICON_BIG Resources/swell_icon.png
        COMPANY_NAME "Swell-audio"
        BUNDLE_ID com.swell.naturalsynthetiser
        PLUGIN_MANUFACTURER_CODE Swel
        PLUGIN_CODE Snat
        FORMATS ${SWELL_FORMATS}
        PRODUCT_NAME "Swell - Natural Synthetiser"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD ${SWELL_COPY_AFTER_BUILD}
        VST3_CATEGORIES Instrument Sampler
        AU_MAIN_TYPE kAudioUnitType_MusicDevice)

    file(GLOB SWELL_HERO_PHOTOS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/hero/*.jpg")
    juce_add_binary_data(SwellNaturalSynthData SOURCES Resources/swell_mark.png ${SWELL_HERO_PHOTOS})

    target_sources(SwellNaturalSynth PRIVATE
        Source/SwellBank.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

    target_compile_definitions(SwellNaturalSynth PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_USE_FLAC=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(SwellNaturalSynth
        PRIVATE
            SwellNaturalSynthData
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()

# ---------------------------------------------------------------- headless test
option(SWELL_BUILD_TESTS "Build the headless render test" OFF)
if(SWELL_BUILD_TESTS AND SWELL_BUILD_PLUGIN)
    juce_add_console_app(SwellRenderTest PRODUCT_NAME "SwellRenderTest")
    target_sources(SwellRenderTest PRIVATE Tests/RenderTest.cpp)
    target_link_libraries(SwellRenderTest PRIVATE SwellNaturalSynth)
    target_include_directories(SwellRenderTest PRIVATE Source $<TARGET_PROPERTY:SwellNaturalSynth,INCLUDE_DIRECTORIES>)
    target_compile_definitions(SwellRenderTest PRIVATE $<TARGET_PROPERTY:SwellNaturalSynth,COMPILE_DEFINITIONS>)

    juce_add_console_app(SwellCapture PRODUCT_NAME "SwellCapture")
    target_sources(SwellCapture PRIVATE Tests/Capture.cpp)
    target_link_libraries(SwellCapture PRIVATE SwellNaturalSynth)
    target_include_directories(SwellCapture PRIVATE Source $<TARGET_PROPERTY:SwellNaturalSynth,INCLUDE_DIRECTORIES>)
    target_compile_definitions(SwellCapture PRIVATE $<TARGET_PROPERTY:SwellNaturalSynth,COMPILE_DEFINITIONS>)

    juce_add_console_app(SwellScoreRender PRODUCT_NAME "SwellScoreRender")
    target_sources(SwellScoreRender PRIVATE Tests/ScoreRender.cpp)
    target_link_libraries(SwellScoreRender PRIVATE SwellNaturalSynth)
    target_include_directories(SwellScoreRender PRIVATE Source $<TARGET_PROPERTY:SwellNaturalSynth,INCLUDE_DIRECTORIES>)
    target_compile_definitions(SwellScoreRender PRIVATE $<TARGET_PROPERTY:SwellNaturalSynth,COMPILE_DEFINITIONS>)
endif()
