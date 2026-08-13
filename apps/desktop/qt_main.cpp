#include <QApplication>
#include <QQuickStyle>
#include <QQmlContext>
#include <QQmlApplicationEngine>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <memory>

#include "ui/qt/soundboard_model.hpp"
#include "ui/qt/audio_device_model.hpp"
#include "ui/qt/playlist_model.hpp"
#include "core/audio/audio_engine.hpp"
#include "core/library/sound_cache.hpp"
#include "core/soundboard/soundboard_service.hpp"
#include "core/profiles/profile_store.hpp"
#include "core/hotkeys/hotkey_router.hpp"

#ifdef PUFFY_HAS_PIPEWIRE
#include "platform/linux/pipewire_capture.hpp"
#include "platform/linux/pipewire_output.hpp"
#include "platform/linux/pipewire_virtual_microphone.hpp"
#endif
#ifdef PUFFY_HAS_X11_GLOBAL_HOTKEYS
#include "platform/linux/x11_global_keyboard_listener.hpp"
#endif

int main(int argc, char* argv[]) {
    QQuickStyle::setStyle("Material");
    QApplication application(argc, argv);
    application.setApplicationName("puffy");
    application.setOrganizationName("puffy");

    const auto databasePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/library.sqlite";
    puffy::library::SoundLibrary library(databasePath.toStdString());
    if (!library.open()) return 1;
    puffy::library::SoundCache cache;
    puffy::profiles::ProfileStore profileStore(databasePath.toStdString());
    profileStore.open();
    puffy::audio::SndFileDecoder decoder;
    puffy::audio::AudioEngine audioEngine({{48000, 2}, 2048, 128, {2048, 2, 32}});
    puffy::soundboard::SoundboardService soundboard(library, cache, decoder, audioEngine);
#ifdef PUFFY_HAS_PIPEWIRE
    puffy::platform::linux::PipeWireCapture capture;
    puffy::platform::linux::PipeWireOutput output;
    puffy::platform::linux::PipeWireVirtualMicrophone virtualMicrophone;
    puffy::ui::AudioDeviceModel deviceModel(&capture, &output);
#else
    puffy::ui::AudioDeviceModel deviceModel(nullptr, nullptr);
#endif
    puffy::ui::PlaylistModel playlistModel(profileStore);
    puffy::ui::SoundboardModel soundModel(library, profileStore, audioEngine);
    soundboard.prepareAll();

#ifdef PUFFY_HAS_PIPEWIRE
    audioEngine.start(capture, output, &virtualMicrophone);
#endif
#ifdef PUFFY_HAS_X11_GLOBAL_HOTKEYS
    puffy::platform::linux::X11GlobalKeyboardListener keyboardListener;
    puffy::hotkeys::HotkeyRouter hotkeyRouter(keyboardListener, soundboard);
    for (const auto& sound : library.all()) {
        if (!sound.hotkey.empty()) hotkeyRouter.bindSoundText(sound.hotkey, sound.id);
    }
    std::vector<int> fullKeyboardSounds;
    for (const auto& sound : library.all()) fullKeyboardSounds.push_back(static_cast<int>(sound.id));
    hotkeyRouter.setFullKeyboardPlaylist(std::move(fullKeyboardSounds));
    hotkeyRouter.start();
    soundModel.setHotkeyRouter(&hotkeyRouter);
    playlistModel.setHotkeyRouter(&hotkeyRouter);
#endif
    soundModel.refreshAudioState();
    soundModel.setService(&soundboard);
    QTimer recoveryTimer;
    QObject::connect(&recoveryTimer, &QTimer::timeout, [&soundModel] { soundModel.recoverAudio(); });
    recoveryTimer.start(2000);
    QObject::connect(&deviceModel, &puffy::ui::AudioDeviceModel::selectionChanged, [&] {
        if (audioEngine.restart(deviceModel.selectedInput().toStdString(), deviceModel.selectedOutput().toStdString()))
            soundModel.refreshAudioState();
    });
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("soundModel", &soundModel);
    engine.rootContext()->setContextProperty("deviceModel", &deviceModel);
    engine.rootContext()->setContextProperty("playlistModel", &playlistModel);
    const auto trayIcon = QIcon::fromTheme("audio-input-microphone", QIcon::fromTheme("audio-card"));
    QSystemTrayIcon tray(trayIcon);
    QMenu trayMenu;
    trayMenu.addAction("Open", [&engine] { if (!engine.rootObjects().isEmpty()) engine.rootObjects().first()->setProperty("visible", true); });
    trayMenu.addAction("Stop All Sounds", [&soundModel] { soundModel.stopAllSounds(); });
    trayMenu.addAction("Fade Out All", [&soundModel] { soundModel.fadeOutAllSounds(); });
    trayMenu.addAction("Pause All", [&soundModel] { soundModel.pauseAllSounds(); });
    trayMenu.addAction("Resume All", [&soundModel] { soundModel.resumeAllSounds(); });
    trayMenu.addSeparator();
    trayMenu.addAction("Exit", &application, &QGuiApplication::quit);
    tray.setContextMenu(&trayMenu);
    if (!tray.icon().isNull()) tray.show();
    engine.load(QUrl(QStringLiteral("qrc:/ui/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    return application.exec();
}
