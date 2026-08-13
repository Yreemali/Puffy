cmake_minimum_required(VERSION 3.21)
project(puffy VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

option(PUFFY_BUILD_TESTS "Build puffy unit tests" ON)
option(PUFFY_BUILD_DESKTOP "Build the Qt desktop application" OFF)

find_package(SQLite3 REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(puffy_core
    core/audio/audio_types.cpp
    core/audio/audio_decoder.cpp
    core/audio/sndfile_decoder.cpp
    core/audio/audio_engine.cpp
    core/mixer/mixer.cpp
    core/hotkeys/hotkey_manager.cpp
    core/hotkeys/global_keyboard_listener.hpp
    core/hotkeys/hotkey_router.cpp
    core/soundboard/full_keyboard_mode.cpp
    core/soundboard/playback_policy.cpp
    core/soundboard/soundboard_service.cpp
    core/library/sound_library.cpp
    core/library/sound_cache.cpp
    core/profiles/profile_store.cpp
    core/effects/audio_effect.cpp
    core/effects/effects_chain.cpp
)
target_include_directories(puffy_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(puffy_core PUBLIC cxx_std_20)
target_link_libraries(puffy_core PUBLIC SQLite3::SQLite3)

if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SNDFILE IMPORTED_TARGET sndfile)
    if(SNDFILE_FOUND)
        target_link_libraries(puffy_core PRIVATE PkgConfig::SNDFILE)
        target_compile_definitions(puffy_core PUBLIC PUFFY_HAS_SNDFILE=1)
    endif()
    pkg_check_modules(PIPEWIRE IMPORTED_TARGET libpipewire-0.3)
    if(PIPEWIRE_FOUND)
        target_sources(puffy_core PRIVATE platform/linux/pipewire_output.cpp)
        target_sources(puffy_core PRIVATE platform/linux/pipewire_capture.cpp)
        target_sources(puffy_core PRIVATE platform/linux/pipewire_virtual_microphone.cpp)
        target_link_libraries(puffy_core PRIVATE PkgConfig::PIPEWIRE)
        target_compile_definitions(puffy_core PUBLIC PUFFY_HAS_PIPEWIRE=1)
    endif()
    pkg_check_modules(X11_XTST IMPORTED_TARGET x11 xtst)
    if(X11_XTST_FOUND)
        target_sources(puffy_core PRIVATE platform/linux/x11_global_keyboard_listener.cpp)
        target_link_libraries(puffy_core PRIVATE PkgConfig::X11_XTST)
        target_compile_definitions(puffy_core PUBLIC PUFFY_HAS_X11_GLOBAL_HOTKEYS=1)
    endif()
endif()

if(WIN32)
    target_sources(puffy_core PRIVATE platform/windows/windows_audio_backend.cpp)
    target_compile_definitions(puffy_core PUBLIC PUFFY_HAS_WASAPI_BACKEND=1)
endif()

if(APPLE)
    target_sources(puffy_core PRIVATE platform/macos/macos_audio_backend.cpp)
    target_compile_definitions(puffy_core PUBLIC PUFFY_HAS_COREAUDIO_BACKEND=1)
endif()

if(MSVC)
    target_compile_options(puffy_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(puffy_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(puffy_core_demo apps/desktop/main.cpp)
target_link_libraries(puffy_core_demo PRIVATE puffy_core)

install(TARGETS puffy_core_demo RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} BUNDLE DESTINATION .)
install(DIRECTORY docs DESTINATION ${CMAKE_INSTALL_DOCDIR})
include(CPack)

if(PUFFY_BUILD_TESTS)
    enable_testing()
    add_executable(puffy_tests
        tests/core_tests.cpp
    )
    target_link_libraries(puffy_tests PRIVATE puffy_core)
    add_test(NAME puffy_core_tests COMMAND puffy_tests)
endif()

if(PUFFY_BUILD_DESKTOP)
    set(CMAKE_AUTOMOC ON)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Widgets)
    add_executable(puffy_desktop apps/desktop/qt_main.cpp ui/qt/soundboard_model.cpp ui/qt/audio_device_model.cpp ui/qt/playlist_model.cpp)
    target_link_libraries(puffy_desktop PRIVATE puffy_core Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Widgets)
    qt_add_resources(puffy_desktop puffy_qml
        PREFIX "/"
        FILES ui/qml/Main.qml
    )
endif()
