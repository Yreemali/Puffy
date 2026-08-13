#pragma once

#include "core/library/sound_library.hpp"
#include "core/soundboard/soundboard_service.hpp"
#include "core/hotkeys/hotkey_router.hpp"
#include "core/effects/audio_effect.hpp"
#include "core/profiles/profile_store.hpp"

#include <QAbstractListModel>
#include <QString>

namespace puffy::ui {

class SoundboardModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool fullKeyboardEnabled READ fullKeyboardEnabled WRITE setFullKeyboardEnabled NOTIFY fullKeyboardEnabledChanged)
    Q_PROPERTY(int fullKeyboardMode READ fullKeyboardMode WRITE setFullKeyboardMode NOTIFY fullKeyboardModeChanged)
    Q_PROPERTY(bool avoidImmediateRepeats READ avoidImmediateRepeats WRITE setAvoidImmediateRepeats NOTIFY avoidImmediateRepeatsChanged)
    Q_PROPERTY(bool triggerOnRepeat READ triggerOnRepeat WRITE setTriggerOnRepeat NOTIFY triggerOnRepeatChanged)
    Q_PROPERTY(int fullKeyboardSingleSound READ fullKeyboardSingleSound WRITE setFullKeyboardSingleSound NOTIFY fullKeyboardSingleSoundChanged)
    Q_PROPERTY(bool ignoreCtrl READ ignoreCtrl WRITE setIgnoreCtrl NOTIFY ignoreCtrlChanged)
    Q_PROPERTY(bool ignoreShift READ ignoreShift WRITE setIgnoreShift NOTIFY ignoreShiftChanged)
    Q_PROPERTY(bool ignoreAlt READ ignoreAlt WRITE setIgnoreAlt NOTIFY ignoreAltChanged)
    Q_PROPERTY(bool ignoreSuper READ ignoreSuper WRITE setIgnoreSuper NOTIFY ignoreSuperChanged)
    Q_PROPERTY(QString ignoredKeyCodes READ ignoredKeyCodes WRITE setIgnoredKeyCodes NOTIFY ignoredKeyCodesChanged)
    Q_PROPERTY(double microphoneGain READ microphoneGain WRITE setMicrophoneGain NOTIFY microphoneGainChanged)
    Q_PROPERTY(double soundboardGain READ soundboardGain WRITE setSoundboardGain NOTIFY soundboardGainChanged)
    Q_PROPERTY(bool hearMicrophone READ hearMicrophone WRITE setHearMicrophone NOTIFY hearMicrophoneChanged)
    Q_PROPERTY(double gateThreshold READ gateThreshold WRITE setGateThreshold NOTIFY gateThresholdChanged)
    Q_PROPERTY(double limiterCeiling READ limiterCeiling WRITE setLimiterCeiling NOTIFY limiterCeilingChanged)
    Q_PROPERTY(double compressorThreshold READ compressorThreshold WRITE setCompressorThreshold NOTIFY compressorThresholdChanged)
    Q_PROPERTY(double compressorRatio READ compressorRatio WRITE setCompressorRatio NOTIFY compressorRatioChanged)
    Q_PROPERTY(double lowPassCutoff READ lowPassCutoff WRITE setLowPassCutoff NOTIFY lowPassCutoffChanged)
    Q_PROPERTY(double highPassCutoff READ highPassCutoff WRITE setHighPassCutoff NOTIFY highPassCutoffChanged)
    Q_PROPERTY(double delayMix READ delayMix WRITE setDelayMix NOTIFY delayMixChanged)
    Q_PROPERTY(QStringList profileNames READ profileNames NOTIFY profilesChanged)
    Q_PROPERTY(int currentProfile READ currentProfile WRITE setCurrentProfile NOTIFY currentProfileChanged)
    Q_PROPERTY(QString audioState READ audioState NOTIFY audioStateChanged)
    Q_PROPERTY(int bufferFrames READ bufferFrames WRITE setBufferFrames NOTIFY bufferFramesChanged)
    Q_PROPERTY(double estimatedLatencyMs READ estimatedLatencyMs NOTIFY latencyChanged)

public:
    enum Roles {
        SoundIdRole = Qt::UserRole + 1,
        NameRole,
        HotkeyRole,
        DurationRole,
        VolumeRole,
        FavoriteRole,
        RouteRole,
        LoopRole,
        SpeedRole,
        FadeInRole,
        FadeOutRole,
    };

    explicit SoundboardModel(library::SoundLibrary& library, profiles::ProfileStore& profiles, audio::AudioEngine& engine, QObject* parent = nullptr);
    void setService(soundboard::SoundboardService* service) noexcept { service_ = service; }
    void setHotkeyRouter(hotkeys::HotkeyRouter* router) noexcept;

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE bool addSound(const QString& path);
    Q_INVOKABLE bool addSounds(const QStringList& paths);
    Q_INVOKABLE bool removeSound(int row);
    Q_INVOKABLE bool renameSound(int row, const QString& name);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void trigger(int row);
    Q_INVOKABLE void setFavorite(int row, bool favorite);
    Q_INVOKABLE qint64 soundIdAt(int row) const;
    Q_INVOKABLE bool assignHotkey(int row, const QString& hotkey);
    Q_INVOKABLE bool setSoundVolume(int row, double value);
    Q_INVOKABLE bool setSoundRoute(int row, int route);
    Q_INVOKABLE bool setSoundLoop(int row, bool loop);
    Q_INVOKABLE bool setSoundSpeed(int row, double value);
    Q_INVOKABLE bool setSoundFades(int row, double fadeIn, double fadeOut);
    Q_INVOKABLE bool hasHotkeyConflict(int row, const QString& hotkey) const;

    [[nodiscard]] QString lastError() const noexcept { return lastError_; }
    [[nodiscard]] bool fullKeyboardEnabled() const noexcept { return fullKeyboardEnabled_; }
    [[nodiscard]] int fullKeyboardMode() const noexcept { return fullKeyboardMode_; }
    [[nodiscard]] bool avoidImmediateRepeats() const noexcept { return avoidImmediateRepeats_; }
    [[nodiscard]] bool triggerOnRepeat() const noexcept { return triggerOnRepeat_; }

    void setFullKeyboardEnabled(bool enabled);
    void setFullKeyboardMode(int mode);
    void setAvoidImmediateRepeats(bool enabled);
    void setTriggerOnRepeat(bool enabled);
    void setFullKeyboardSingleSound(int soundId);
    void setIgnoreCtrl(bool value);
    void setIgnoreShift(bool value);
    void setIgnoreAlt(bool value);
    void setIgnoreSuper(bool value);
    void setIgnoredKeyCodes(const QString& value);
    double microphoneGain() const noexcept { return microphoneGain_; }
    double soundboardGain() const noexcept { return soundboardGain_; }
    bool hearMicrophone() const noexcept { return hearMicrophone_; }
    double gateThreshold() const noexcept { return gateThreshold_; }
    double limiterCeiling() const noexcept { return limiterCeiling_; }
    void setMicrophoneGain(double value);
    void setSoundboardGain(double value);
    void setHearMicrophone(bool enabled);
    void setGateThreshold(double value);
    void setLimiterCeiling(double value);
    double compressorThreshold() const noexcept { return compressorThreshold_; }
    double compressorRatio() const noexcept { return compressorRatio_; }
    double lowPassCutoff() const noexcept { return lowPassCutoff_; }
    void setCompressorThreshold(double value);
    void setCompressorRatio(double value);
    void setLowPassCutoff(double value);
    double highPassCutoff() const noexcept { return highPassCutoff_; }
    double delayMix() const noexcept { return delayMix_; }
    void setHighPassCutoff(double value);
    void setDelayMix(double value);
    QStringList profileNames() const { return profileNames_; }
    int currentProfile() const noexcept { return currentProfile_; }
    void setCurrentProfile(int index);
    Q_INVOKABLE bool createProfile(const QString& name);
    Q_INVOKABLE bool exportProfile(const QString& path);
    Q_INVOKABLE bool importProfile(const QString& path);
    Q_INVOKABLE void refreshAudioState();
    Q_INVOKABLE bool recoverAudio();
    int bufferFrames() const noexcept { return bufferFrames_; }
    double estimatedLatencyMs() const noexcept { return estimatedLatencyMs_; }
    void setBufferFrames(int frames);
    Q_INVOKABLE void stopAllSounds();
    Q_INVOKABLE void pauseAllSounds();
    Q_INVOKABLE void resumeAllSounds();
    Q_INVOKABLE void fadeOutAllSounds();
    QString audioState() const { return audioState_; }
    bool ignoreCtrl() const noexcept { return ignoreCtrl_; }
    bool ignoreShift() const noexcept { return ignoreShift_; }
    bool ignoreAlt() const noexcept { return ignoreAlt_; }
    bool ignoreSuper() const noexcept { return ignoreSuper_; }
    QString ignoredKeyCodes() const { return ignoredKeyCodes_; }
    [[nodiscard]] int fullKeyboardSingleSound() const noexcept { return fullKeyboardSingleSound_; }

signals:
    void soundTriggered(qint64 soundId);
    void lastErrorChanged();
    void fullKeyboardEnabledChanged();
    void fullKeyboardModeChanged();
    void avoidImmediateRepeatsChanged();
    void triggerOnRepeatChanged();
    void fullKeyboardSingleSoundChanged();
    void ignoreCtrlChanged();
    void ignoreShiftChanged();
    void ignoreAltChanged();
    void ignoreSuperChanged();
    void ignoredKeyCodesChanged();
    void microphoneGainChanged();
    void soundboardGainChanged();
    void hearMicrophoneChanged();
    void gateThresholdChanged();
    void limiterCeilingChanged();
    void compressorThresholdChanged();
    void compressorRatioChanged();
    void lowPassCutoffChanged();
    void highPassCutoffChanged();
    void delayMixChanged();
    void profilesChanged();
    void currentProfileChanged();
    void audioStateChanged();
    void bufferFramesChanged();
    void latencyChanged();

private:
    bool setError(QString error);
    void clearError();

    library::SoundLibrary& library_;
    soundboard::SoundboardService* service_{nullptr};
    hotkeys::HotkeyRouter* hotkeyRouter_{nullptr};
    std::vector<library::Sound> sounds_;
    QString lastError_;
    bool fullKeyboardEnabled_{false};
    int fullKeyboardMode_{0};
    bool avoidImmediateRepeats_{true};
    bool triggerOnRepeat_{false};
    int fullKeyboardSingleSound_{-1};
    bool ignoreCtrl_{false};
    bool ignoreShift_{false};
    bool ignoreAlt_{false};
    bool ignoreSuper_{false};
    QString ignoredKeyCodes_;
    audio::AudioEngine& engine_;
    profiles::ProfileStore& profiles_;
    effects::Gain gainEffect_;
    effects::NoiseGate gateEffect_;
    effects::Compressor compressorEffect_;
    effects::OnePoleFilter lowPassEffect_{effects::OnePoleFilter::Type::LowPass};
    effects::OnePoleFilter highPassEffect_{effects::OnePoleFilter::Type::HighPass};
    effects::Delay delayEffect_;
    effects::Limiter limiterEffect_;
    double microphoneGain_{1.0};
    double soundboardGain_{1.0};
    bool hearMicrophone_{false};
    double gateThreshold_{0.015};
    double limiterCeiling_{0.98};
    double compressorThreshold_{0.5};
    double compressorRatio_{4.0};
    double lowPassCutoff_{12000.0};
    double highPassCutoff_{80.0};
    double delayMix_{0.0};
    QStringList profileNames_;
    std::vector<profiles::Profile> profileData_;
    int currentProfile_{0};
    QString audioState_{"Stopped"};
    int bufferFrames_{128};
    double estimatedLatencyMs_{8.0};

    void saveProfileState();
    void loadProfileState(const profiles::Profile& profile);
};

} // namespace puffy::ui
