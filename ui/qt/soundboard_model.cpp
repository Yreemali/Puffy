#include "ui/qt/soundboard_model.hpp"

#include <QFileInfo>

#include <algorithm>
#include <QSettings>
#include <QStringList>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace puffy::ui {

SoundboardModel::SoundboardModel(library::SoundLibrary& library, profiles::ProfileStore& profiles, audio::AudioEngine& engine, QObject* parent)
    : QAbstractListModel(parent), library_(library), engine_(engine), profiles_(profiles) {
    profiles_.open();
    profileData_ = profiles_.profiles();
    if (profileData_.empty()) { profiles::Profile defaultProfile; profiles_.saveProfile(defaultProfile); profileData_ = profiles_.profiles(); }
    for (const auto& profile : profileData_) profileNames_.append(QString::fromStdString(profile.name));
    QSettings settings;
    fullKeyboardEnabled_ = settings.value("fullKeyboard/enabled", false).toBool();
    fullKeyboardMode_ = settings.value("fullKeyboard/mode", 0).toInt();
    avoidImmediateRepeats_ = settings.value("fullKeyboard/avoidRepeats", true).toBool();
    triggerOnRepeat_ = settings.value("fullKeyboard/triggerOnRepeat", false).toBool();
    ignoreCtrl_ = settings.value("fullKeyboard/ignoreCtrl", false).toBool();
    ignoreShift_ = settings.value("fullKeyboard/ignoreShift", false).toBool();
    ignoreAlt_ = settings.value("fullKeyboard/ignoreAlt", false).toBool();
    ignoreSuper_ = settings.value("fullKeyboard/ignoreSuper", false).toBool();
    ignoredKeyCodes_ = settings.value("fullKeyboard/ignoredKeyCodes", "").toString();
    microphoneGain_ = settings.value("effects/microphoneGain", 1.0).toDouble();
    gateThreshold_ = settings.value("effects/gateThreshold", 0.015).toDouble();
    limiterCeiling_ = settings.value("effects/limiterCeiling", 0.98).toDouble();
    compressorThreshold_ = settings.value("effects/compressorThreshold", 0.5).toDouble();
    compressorRatio_ = settings.value("effects/compressorRatio", 4.0).toDouble();
    lowPassCutoff_ = settings.value("effects/lowPassCutoff", 12000.0).toDouble();
    highPassCutoff_ = settings.value("effects/highPassCutoff", 80.0).toDouble();
    delayMix_ = settings.value("effects/delayMix", 0.0).toDouble();
    bufferFrames_ = settings.value("audio/bufferFrames", 128).toInt();
    engine_.setBufferFrames(static_cast<std::size_t>(bufferFrames_));
    estimatedLatencyMs_ = engine_.latency().totalMs;
    gainEffect_.setGain(static_cast<float>(microphoneGain_));
    gateEffect_.setThreshold(static_cast<float>(gateThreshold_));
    limiterEffect_.setCeiling(static_cast<float>(limiterCeiling_));
    compressorEffect_.setThreshold(static_cast<float>(compressorThreshold_));
    compressorEffect_.setRatio(static_cast<float>(compressorRatio_));
    lowPassEffect_.setCutoff(static_cast<float>(lowPassCutoff_));
    highPassEffect_.setCutoff(static_cast<float>(highPassCutoff_));
    delayEffect_.setMix(static_cast<float>(delayMix_));
    engine_.addMicrophoneEffect(gainEffect_);
    engine_.addMicrophoneEffect(gateEffect_);
    engine_.addMicrophoneEffect(compressorEffect_);
    engine_.addMicrophoneEffect(lowPassEffect_);
    engine_.addMicrophoneEffect(highPassEffect_);
    engine_.addMicrophoneEffect(delayEffect_);
    engine_.addMicrophoneEffect(limiterEffect_);
    refresh();
}

void SoundboardModel::setCompressorThreshold(double value) { value = std::clamp(value, 0.01, 1.0); if (compressorThreshold_ == value) return; compressorThreshold_ = value; compressorEffect_.setThreshold(value); QSettings().setValue("effects/compressorThreshold", value); emit compressorThresholdChanged(); }
void SoundboardModel::setCompressorRatio(double value) { value = std::clamp(value, 1.0, 20.0); if (compressorRatio_ == value) return; compressorRatio_ = value; compressorEffect_.setRatio(value); QSettings().setValue("effects/compressorRatio", value); emit compressorRatioChanged(); }
void SoundboardModel::setLowPassCutoff(double value) { value = std::clamp(value, 100.0, 20000.0); if (lowPassCutoff_ == value) return; lowPassCutoff_ = value; lowPassEffect_.setCutoff(value); lowPassEffect_.prepare(48000.0, 2048, 2); QSettings().setValue("effects/lowPassCutoff", value); emit lowPassCutoffChanged(); }
void SoundboardModel::setHighPassCutoff(double value) { value = std::clamp(value, 20.0, 1000.0); if (highPassCutoff_ == value) return; highPassCutoff_ = value; highPassEffect_.setCutoff(value); highPassEffect_.prepare(48000.0, 2048, 2); QSettings().setValue("effects/highPassCutoff", value); emit highPassCutoffChanged(); }
void SoundboardModel::setDelayMix(double value) { value = std::clamp(value, 0.0, 1.0); if (delayMix_ == value) return; delayMix_ = value; delayEffect_.setMix(value); QSettings().setValue("effects/delayMix", value); emit delayMixChanged(); }

void SoundboardModel::saveProfileState() {
    if (currentProfile_ < 0 || currentProfile_ >= static_cast<int>(profileData_.size())) return;
    auto& profile = profileData_[static_cast<std::size_t>(currentProfile_)];
    profile.fullKeyboardEnabled = fullKeyboardEnabled_; profile.fullKeyboardMode = static_cast<soundboard::FullKeyboardMode>(fullKeyboardMode_); profile.singleSoundId = fullKeyboardSingleSound_; profile.avoidImmediateRepeats = avoidImmediateRepeats_; profile.triggerOnRepeat = triggerOnRepeat_; profile.ignoreCtrl = ignoreCtrl_; profile.ignoreShift = ignoreShift_; profile.ignoreAlt = ignoreAlt_; profile.ignoreSuper = ignoreSuper_; profile.ignoredKeyCodes = ignoredKeyCodes_.toStdString();
    profiles_.updateProfile(profile);
}

void SoundboardModel::loadProfileState(const profiles::Profile& profile) {
    fullKeyboardEnabled_ = profile.fullKeyboardEnabled; fullKeyboardMode_ = static_cast<int>(profile.fullKeyboardMode); fullKeyboardSingleSound_ = static_cast<int>(profile.singleSoundId); avoidImmediateRepeats_ = profile.avoidImmediateRepeats; triggerOnRepeat_ = profile.triggerOnRepeat; ignoreCtrl_ = profile.ignoreCtrl; ignoreShift_ = profile.ignoreShift; ignoreAlt_ = profile.ignoreAlt; ignoreSuper_ = profile.ignoreSuper; ignoredKeyCodes_ = QString::fromStdString(profile.ignoredKeyCodes);
    if (hotkeyRouter_) { hotkeyRouter_->setFullKeyboardEnabled(fullKeyboardEnabled_); hotkeyRouter_->setFullKeyboardMode(static_cast<soundboard::FullKeyboardMode>(fullKeyboardMode_)); hotkeyRouter_->setFullKeyboardSingleSound(fullKeyboardSingleSound_); hotkeyRouter_->setFullKeyboardAvoidImmediateRepeats(avoidImmediateRepeats_); hotkeyRouter_->setFullKeyboardTriggerOnRepeat(triggerOnRepeat_); hotkeyRouter_->setFullKeyboardIgnoreCtrl(ignoreCtrl_); hotkeyRouter_->setFullKeyboardIgnoreShift(ignoreShift_); hotkeyRouter_->setFullKeyboardIgnoreAlt(ignoreAlt_); hotkeyRouter_->setFullKeyboardIgnoreSuper(ignoreSuper_); }
    gainEffect_.setGain(static_cast<float>(microphoneGain_)); gateEffect_.setThreshold(static_cast<float>(gateThreshold_)); limiterEffect_.setCeiling(static_cast<float>(limiterCeiling_));
    emit fullKeyboardEnabledChanged(); emit fullKeyboardModeChanged(); emit fullKeyboardSingleSoundChanged(); emit avoidImmediateRepeatsChanged(); emit triggerOnRepeatChanged(); emit ignoreCtrlChanged(); emit ignoreShiftChanged(); emit ignoreAltChanged(); emit ignoreSuperChanged(); emit ignoredKeyCodesChanged();
}

void SoundboardModel::setCurrentProfile(int index) {
    if (index < 0 || index >= static_cast<int>(profileData_.size()) || index == currentProfile_) return;
    saveProfileState(); currentProfile_ = index; loadProfileState(profileData_[static_cast<std::size_t>(index)]); emit currentProfileChanged();
}

bool SoundboardModel::createProfile(const QString& name) {
    const auto trimmed = name.trimmed(); if (trimmed.isEmpty()) return false;
    profiles::Profile profile; profile.name = trimmed.toStdString(); if (!profiles_.saveProfile(profile)) return false;
    profileData_ = profiles_.profiles(); profileNames_.clear(); for (const auto& item : profileData_) profileNames_.append(QString::fromStdString(item.name)); emit profilesChanged(); return true;
}

bool SoundboardModel::exportProfile(const QString& path) {
    if (currentProfile_ < 0 || currentProfile_ >= static_cast<int>(profileData_.size())) return false;
    saveProfileState();
    const auto& profile = profileData_[static_cast<std::size_t>(currentProfile_)];
    QJsonObject object;
    object["name"] = QString::fromStdString(profile.name);
    object["fullKeyboardEnabled"] = profile.fullKeyboardEnabled;
    object["fullKeyboardMode"] = static_cast<int>(profile.fullKeyboardMode);
    object["singleSoundId"] = static_cast<qint64>(profile.singleSoundId);
    object["avoidImmediateRepeats"] = profile.avoidImmediateRepeats;
    object["triggerOnRepeat"] = profile.triggerOnRepeat;
    object["ignoreCtrl"] = profile.ignoreCtrl; object["ignoreShift"] = profile.ignoreShift;
    object["ignoreAlt"] = profile.ignoreAlt; object["ignoreSuper"] = profile.ignoreSuper;
    object["ignoredKeyCodes"] = QString::fromStdString(profile.ignoredKeyCodes);
    QJsonArray sounds;
    for (const auto& sound : sounds_) {
        QJsonObject item; item["id"] = static_cast<qint64>(sound.id); item["name"] = QString::fromStdString(sound.name); item["filePath"] = QString::fromStdString(sound.filePath.string()); item["hotkey"] = QString::fromStdString(sound.hotkey); item["volume"] = sound.volume; item["route"] = static_cast<int>(sound.route); sounds.append(item);
    }
    object["sounds"] = sounds;
    QFile file(path); if (!file.open(QIODevice::WriteOnly)) return false; file.write(QJsonDocument(object).toJson(QJsonDocument::Indented)); return true;
}

bool SoundboardModel::importProfile(const QString& path) {
    QFile file(path); if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonParseError parseError; const auto document = QJsonDocument::fromJson(file.readAll(), &parseError); if (parseError.error != QJsonParseError::NoError || !document.isObject()) return false;
    const auto object = document.object(); profiles::Profile profile; profile.name = object.value("name").toString("Imported").toStdString(); profile.fullKeyboardEnabled = object.value("fullKeyboardEnabled").toBool(); profile.fullKeyboardMode = static_cast<soundboard::FullKeyboardMode>(object.value("fullKeyboardMode").toInt()); profile.singleSoundId = object.value("singleSoundId").toInteger(-1); profile.avoidImmediateRepeats = object.value("avoidImmediateRepeats").toBool(true); profile.triggerOnRepeat = object.value("triggerOnRepeat").toBool(); profile.ignoreCtrl = object.value("ignoreCtrl").toBool(); profile.ignoreShift = object.value("ignoreShift").toBool(); profile.ignoreAlt = object.value("ignoreAlt").toBool(); profile.ignoreSuper = object.value("ignoreSuper").toBool(); profile.ignoredKeyCodes = object.value("ignoredKeyCodes").toString().toStdString();
    if (!profiles_.saveProfile(profile)) return false;
    profileData_ = profiles_.profiles(); profileNames_.clear(); for (const auto& item : profileData_) profileNames_.append(QString::fromStdString(item.name)); emit profilesChanged(); return true;
}

void SoundboardModel::setMicrophoneGain(double value) {
    value = std::clamp(value, 0.0, 4.0); if (microphoneGain_ == value) return; microphoneGain_ = value;
    gainEffect_.setGain(static_cast<float>(value)); QSettings().setValue("effects/microphoneGain", value); emit microphoneGainChanged();
}
void SoundboardModel::setGateThreshold(double value) {
    value = std::clamp(value, 0.0, 1.0); if (gateThreshold_ == value) return; gateThreshold_ = value;
    gateEffect_.setThreshold(static_cast<float>(value)); QSettings().setValue("effects/gateThreshold", value); emit gateThresholdChanged();
}
void SoundboardModel::setLimiterCeiling(double value) {
    value = std::clamp(value, 0.01, 1.0); if (limiterCeiling_ == value) return; limiterCeiling_ = value;
    limiterEffect_.setCeiling(static_cast<float>(value)); QSettings().setValue("effects/limiterCeiling", value); emit limiterCeilingChanged();
}

void SoundboardModel::setHotkeyRouter(hotkeys::HotkeyRouter* router) noexcept {
    hotkeyRouter_ = router;
    if (router == nullptr) return;
    router->setFullKeyboardEnabled(fullKeyboardEnabled_);
    router->setFullKeyboardMode(static_cast<soundboard::FullKeyboardMode>(fullKeyboardMode_));
    router->setFullKeyboardAvoidImmediateRepeats(avoidImmediateRepeats_);
    router->setFullKeyboardTriggerOnRepeat(triggerOnRepeat_);
    router->setFullKeyboardIgnoreCtrl(ignoreCtrl_);
    router->setFullKeyboardIgnoreShift(ignoreShift_);
    router->setFullKeyboardIgnoreAlt(ignoreAlt_);
    router->setFullKeyboardIgnoreSuper(ignoreSuper_);
    for (const auto& token : ignoredKeyCodes_.split(',', Qt::SkipEmptyParts)) {
        bool ok = false; const auto code = token.trimmed().toInt(&ok);
        if (ok) router->setFullKeyboardIgnoredKey(code, true);
    }
}

void SoundboardModel::refreshAudioState() {
    QString state = "Stopped";
    switch (engine_.state()) {
    case audio::AudioEngine::State::Starting: state = "Starting"; break;
    case audio::AudioEngine::State::Running: state = "Running"; break;
    case audio::AudioEngine::State::Degraded: state = "Degraded"; break;
    case audio::AudioEngine::State::Failed: state = "Failed"; break;
    case audio::AudioEngine::State::Stopped: break;
    }
    if (audioState_ == state) return;
    audioState_ = std::move(state);
    emit audioStateChanged();
}

bool SoundboardModel::recoverAudio() {
    if (engine_.devicesHealthy()) return true;
    const auto ok = engine_.restart("default", "default");
    refreshAudioState();
    return ok;
}

void SoundboardModel::setBufferFrames(int frames) {
    frames = std::clamp(frames, 32, 2048); if (bufferFrames_ == frames) return; bufferFrames_ = frames;
    engine_.setBufferFrames(static_cast<std::size_t>(frames)); estimatedLatencyMs_ = engine_.latency().totalMs;
    QSettings().setValue("audio/bufferFrames", frames); emit bufferFramesChanged(); emit latencyChanged();
}

void SoundboardModel::stopAllSounds() { engine_.stopAllSounds(); }
void SoundboardModel::pauseAllSounds() { engine_.pauseAllSounds(); }
void SoundboardModel::resumeAllSounds() { engine_.resumeAllSounds(); }
void SoundboardModel::fadeOutAllSounds() { engine_.fadeOutAllSounds(); }

int SoundboardModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(sounds_.size());
}

QVariant SoundboardModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) return {};
    const auto& sound = sounds_[static_cast<std::size_t>(index.row())];
    switch (role) {
    case SoundIdRole: return QVariant::fromValue<qlonglong>(sound.id);
    case NameRole: return QString::fromStdString(sound.name);
    case HotkeyRole: return QString::fromStdString(sound.hotkey);
    case DurationRole: return sound.durationSeconds;
    case VolumeRole: return sound.volume;
    case FavoriteRole: return sound.favorite;
    case RouteRole: return static_cast<int>(sound.route);
    case LoopRole: return sound.loop;
    case SpeedRole: return sound.speed;
    case FadeInRole: return sound.fadeInSeconds;
    case FadeOutRole: return sound.fadeOutSeconds;
    default: return {};
    }
}

QHash<int, QByteArray> SoundboardModel::roleNames() const {
    return {{SoundIdRole, "soundId"}, {NameRole, "name"}, {HotkeyRole, "hotkey"},
            {DurationRole, "duration"}, {VolumeRole, "volume"}, {FavoriteRole, "favorite"}, {RouteRole, "route"}, {LoopRole, "loop"}, {SpeedRole, "speed"}, {FadeInRole, "fadeIn"}, {FadeOutRole, "fadeOut"}};
}

bool SoundboardModel::addSound(const QString& path) {
    const QFileInfo file(path);
    if (!file.exists() || !file.isFile()) return setError("Sound file does not exist");
    library::Sound sound;
    sound.name = file.completeBaseName().toStdString();
    sound.filePath = file.absoluteFilePath().toStdString();
    if (!library_.add(sound)) return setError(QString::fromStdString(library_.lastError()));
    clearError();
    refresh();
    return true;
}

bool SoundboardModel::addSounds(const QStringList& paths) {
    bool success = true;
    for (const auto& path : paths) {
        if (!addSound(path)) success = false;
    }
    return success;
}

bool SoundboardModel::removeSound(int row) {
    if (row < 0 || row >= rowCount()) return false;
    const auto id = sounds_[static_cast<std::size_t>(row)].id;
    if (!library_.remove(id)) return setError(QString::fromStdString(library_.lastError()));
    clearError(); refresh(); return true;
}

bool SoundboardModel::renameSound(int row, const QString& name) {
    if (row < 0 || row >= rowCount() || name.trimmed().isEmpty()) return false;
    auto sound = sounds_[static_cast<std::size_t>(row)]; sound.name = name.trimmed().toStdString();
    if (!library_.update(sound)) return setError(QString::fromStdString(library_.lastError()));
    clearError(); refresh(); return true;
}

void SoundboardModel::refresh() {
    beginResetModel();
    sounds_ = library_.all();
    endResetModel();
}

void SoundboardModel::trigger(int row) {
    if (row < 0 || row >= rowCount()) return;
    const auto id = sounds_[static_cast<std::size_t>(row)].id;
    if (service_ != nullptr && !service_->trigger(id, true)) {
        setError(QString::fromStdString(service_->lastError()));
        return;
    }
    emit soundTriggered(id);
}

void SoundboardModel::setFavorite(int row, bool favorite) {
    if (row < 0 || row >= rowCount()) return;
    auto sound = sounds_[static_cast<std::size_t>(row)];
    sound.favorite = favorite;
    if (!library_.update(sound)) { setError(QString::fromStdString(library_.lastError())); return; }
    refresh();
}

qint64 SoundboardModel::soundIdAt(int row) const {
    if (row < 0 || row >= rowCount()) return -1;
    return sounds_[static_cast<std::size_t>(row)].id;
}

bool SoundboardModel::assignHotkey(int row, const QString& hotkey) {
    if (row < 0 || row >= rowCount()) return false;
    if (hasHotkeyConflict(row, hotkey)) return setError("This hotkey is already assigned to another sound");
    auto sound = sounds_[static_cast<std::size_t>(row)];
    sound.hotkey = hotkey.trimmed().toStdString();
    if (!library_.update(sound)) return setError(QString::fromStdString(library_.lastError()));
    clearError();
    refresh();
    return true;
}

bool SoundboardModel::setSoundVolume(int row, double value) {
    if (row < 0 || row >= rowCount()) return false;
    auto sound = sounds_[static_cast<std::size_t>(row)]; sound.volume = static_cast<float>(std::clamp(value, 0.0, 2.0));
    if (!library_.update(sound)) return setError(QString::fromStdString(library_.lastError())); refresh(); return true;
}

bool SoundboardModel::setSoundRoute(int row, int route) {
    if (row < 0 || row >= rowCount() || route < 0 || route > 3) return false;
    auto sound = sounds_[static_cast<std::size_t>(row)]; sound.route = static_cast<audio::OutputRoute>(route);
    if (!library_.update(sound)) return setError(QString::fromStdString(library_.lastError())); refresh(); return true;
}

bool SoundboardModel::setSoundLoop(int row, bool loop) {
    if (row < 0 || row >= rowCount()) return false;
    auto sound = sounds_[static_cast<std::size_t>(row)]; sound.loop = loop;
    if (!library_.update(sound)) return setError(QString::fromStdString(library_.lastError())); refresh(); return true;
}

bool SoundboardModel::setSoundSpeed(int row, double value) {
    if (row < 0 || row >= rowCount()) return false;
    auto sound = sounds_[static_cast<std::size_t>(row)]; sound.speed = static_cast<float>(std::clamp(value, 0.25, 3.0));
    if (!library_.update(sound)) return setError(QString::fromStdString(library_.lastError())); refresh(); return true;
}

bool SoundboardModel::setSoundFades(int row, double fadeIn, double fadeOut) {
    if (row < 0 || row >= rowCount()) return false;
    auto sound = sounds_[static_cast<std::size_t>(row)]; sound.fadeInSeconds = static_cast<float>(std::clamp(fadeIn, 0.0, 10.0)); sound.fadeOutSeconds = static_cast<float>(std::clamp(fadeOut, 0.0, 10.0));
    if (!library_.update(sound)) return setError(QString::fromStdString(library_.lastError())); refresh(); return true;
}

bool SoundboardModel::hasHotkeyConflict(int row, const QString& hotkey) const {
    const auto normalized = hotkey.trimmed().toUpper();
    if (normalized.isEmpty()) return false;
    for (int index = 0; index < rowCount(); ++index) {
        if (index == row) continue;
        if (QString::fromStdString(sounds_[static_cast<std::size_t>(index)].hotkey).trimmed().toUpper() == normalized) return true;
    }
    return false;
}

bool SoundboardModel::setError(QString error) {
    if (lastError_ == error) return false;
    lastError_ = std::move(error);
    emit lastErrorChanged();
    return false;
}

void SoundboardModel::clearError() {
    if (lastError_.isEmpty()) return;
    lastError_.clear();
    emit lastErrorChanged();
}

void SoundboardModel::setFullKeyboardEnabled(bool enabled) {
    if (fullKeyboardEnabled_ == enabled) return;
    fullKeyboardEnabled_ = enabled;
    QSettings().setValue("fullKeyboard/enabled", enabled); saveProfileState();
    if (hotkeyRouter_ != nullptr) hotkeyRouter_->setFullKeyboardEnabled(enabled);
    emit fullKeyboardEnabledChanged();
}

void SoundboardModel::setFullKeyboardMode(int mode) {
    mode = std::clamp(mode, 0, 2);
    if (fullKeyboardMode_ == mode) return;
    fullKeyboardMode_ = mode;
    QSettings().setValue("fullKeyboard/mode", mode); saveProfileState();
    if (hotkeyRouter_ != nullptr) hotkeyRouter_->setFullKeyboardMode(static_cast<soundboard::FullKeyboardMode>(mode));
    emit fullKeyboardModeChanged();
}

void SoundboardModel::setAvoidImmediateRepeats(bool enabled) {
    if (avoidImmediateRepeats_ == enabled) return;
    avoidImmediateRepeats_ = enabled;
    QSettings().setValue("fullKeyboard/avoidRepeats", enabled); saveProfileState();
    if (hotkeyRouter_ != nullptr) hotkeyRouter_->setFullKeyboardAvoidImmediateRepeats(enabled);
    emit avoidImmediateRepeatsChanged();
}

void SoundboardModel::setTriggerOnRepeat(bool enabled) {
    if (triggerOnRepeat_ == enabled) return;
    triggerOnRepeat_ = enabled;
    QSettings().setValue("fullKeyboard/triggerOnRepeat", enabled); saveProfileState();
    if (hotkeyRouter_ != nullptr) hotkeyRouter_->setFullKeyboardTriggerOnRepeat(enabled);
    emit triggerOnRepeatChanged();
}

void SoundboardModel::setFullKeyboardSingleSound(int soundId) {
    if (fullKeyboardSingleSound_ == soundId) return;
    fullKeyboardSingleSound_ = soundId;
    if (hotkeyRouter_ != nullptr) hotkeyRouter_->setFullKeyboardSingleSound(soundId);
    emit fullKeyboardSingleSoundChanged();
}

void SoundboardModel::setIgnoreCtrl(bool value) {
    if (ignoreCtrl_ == value) return; ignoreCtrl_ = value;
    QSettings().setValue("fullKeyboard/ignoreCtrl", value);
    if (hotkeyRouter_ != nullptr) hotkeyRouter_->setFullKeyboardIgnoreCtrl(value);
    emit ignoreCtrlChanged();
}
void SoundboardModel::setIgnoreShift(bool value) {
    if (ignoreShift_ == value) return; ignoreShift_ = value;
    QSettings().setValue("fullKeyboard/ignoreShift", value);
    if (hotkeyRouter_ != nullptr) hotkeyRouter_->setFullKeyboardIgnoreShift(value);
    emit ignoreShiftChanged();
}
void SoundboardModel::setIgnoreAlt(bool value) {
    if (ignoreAlt_ == value) return; ignoreAlt_ = value;
    QSettings().setValue("fullKeyboard/ignoreAlt", value);
    if (hotkeyRouter_ != nullptr) hotkeyRouter_->setFullKeyboardIgnoreAlt(value);
    emit ignoreAltChanged();
}
void SoundboardModel::setIgnoreSuper(bool value) {
    if (ignoreSuper_ == value) return; ignoreSuper_ = value;
    QSettings().setValue("fullKeyboard/ignoreSuper", value);
    if (hotkeyRouter_ != nullptr) hotkeyRouter_->setFullKeyboardIgnoreSuper(value);
    emit ignoreSuperChanged();
}

void SoundboardModel::setIgnoredKeyCodes(const QString& value) {
    if (ignoredKeyCodes_ == value) return;
    ignoredKeyCodes_ = value;
    QSettings().setValue("fullKeyboard/ignoredKeyCodes", value);
    if (hotkeyRouter_ != nullptr) {
        for (const auto& token : value.split(',', Qt::SkipEmptyParts)) {
            bool ok = false; const auto code = token.trimmed().toInt(&ok);
            if (ok) hotkeyRouter_->setFullKeyboardIgnoredKey(code, true);
        }
    }
    emit ignoredKeyCodesChanged();
}

} // namespace puffy::ui
