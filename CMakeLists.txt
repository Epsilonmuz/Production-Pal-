cmake_minimum_required(VERSION 3.22)

# Production Pal: an AI production buddy for FL Studio.
#   PalEars        - VST3 / AU / Standalone meter that streams readings to the app
#   ProductionPal  - the companion app (hotkeys, screenshot, Claude, voice)
#   analyzer_tests - engine tests (no JUCE)
project(ProductionPal VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
    enable_language(OBJCXX)
endif()

option(PAL_BUILD_PLUGIN "Build the Pal Ears plugin" ON)
option(PAL_BUILD_APP "Build the Production Pal app" ON)
option(PAL_BUILD_TESTS "Build the engine tests" ON)

# Optimise every configuration (Debug included) so Xcode Build/Run never makes a stuttering plugin.
if(NOT MSVC)
    add_compile_options("$<$<CONFIG:Debug>:-O3>")
endif()

add_subdirectory(JUCE)

#==============================================================================
if(PAL_BUILD_PLUGIN)
    juce_add_plugin(PalEars
        COMPANY_NAME "Epsilon Sounds"
        COMPANY_WEBSITE "https://github.com/Epsilonmuz/Production-Pal-"
        BUNDLE_ID "com.epsilonsounds.palears"
        PLUGIN_MANUFACTURER_CODE Epsn
        PLUGIN_CODE PEar
        FORMATS VST3 AU Standalone
        PRODUCT_NAME "Pal Ears"
        VERSION ${PROJECT_VERSION}
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        VST3_CATEGORIES Analyzer Tools
        AU_MAIN_TYPE kAudioUnitType_Effect
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(PalEars PRIVATE
        PalEars/Source/PluginProcessor.cpp
        PalEars/Source/PluginEditor.cpp)

    target_compile_definitions(PalEars PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(PalEars PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
endif()

#==============================================================================
if(PAL_BUILD_APP)
    juce_add_gui_app(ProductionPalApp
        PRODUCT_NAME "Production Pal"
        COMPANY_NAME "Epsilon Sounds"
        BUNDLE_ID "com.epsilonsounds.productionpal"
        VERSION ${PROJECT_VERSION}
        MICROPHONE_PERMISSION_ENABLED FALSE
        PLIST_TO_MERGE "<plist><dict><key>NSAppleEventsUsageDescription</key><string>Production Pal reads your screen only when you press its hotkey.</string></dict></plist>")

    set(PAL_APP_SOURCES
        ProductionPal/Source/Main.cpp
        ProductionPal/Source/MainComponent.cpp
        ProductionPal/Source/ReadingsHub.cpp
        ProductionPal/Source/ClaudeClient.cpp
        ProductionPal/Source/ScreenCapture.cpp
        ProductionPal/Source/Speech.cpp
        ProductionPal/Source/GlobalHotkeys_other.cpp)
    if(APPLE)
        list(APPEND PAL_APP_SOURCES ProductionPal/Source/GlobalHotkeys_mac.mm)
    endif()
    target_sources(ProductionPalApp PRIVATE ${PAL_APP_SOURCES})

    target_compile_definitions(ProductionPalApp PRIVATE
        PRODUCTION_PAL_VERSION="${PROJECT_VERSION}"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=$<IF:$<PLATFORM_ID:Linux>,0,1>
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:ProductionPalApp,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:ProductionPalApp,JUCE_VERSION>"
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(ProductionPalApp PRIVATE
        juce::juce_gui_extra
        juce::juce_audio_devices
        juce::juce_data_structures
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

    if(APPLE)
        target_link_libraries(ProductionPalApp PRIVATE "-framework Carbon")
    endif()
endif()

#==============================================================================
if(PAL_BUILD_TESTS)
    add_executable(analyzer_tests tests/AnalyzerTests.cpp)
    target_include_directories(analyzer_tests PRIVATE Shared)
    enable_testing()
    add_test(NAME analyzer_tests COMMAND analyzer_tests)

    juce_add_console_app(client_tests PRODUCT_NAME "client_tests")
    target_sources(client_tests PRIVATE tests/ClientTests.cpp ProductionPal/Source/ClaudeClient.cpp)
    target_compile_definitions(client_tests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_link_libraries(client_tests PRIVATE juce::juce_core juce::juce_events
                          PUBLIC juce::juce_recommended_config_flags)
    add_test(NAME client_tests COMMAND client_tests)
endif()
