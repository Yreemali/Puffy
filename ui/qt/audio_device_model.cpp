#include "ui/qt/audio_device_model.hpp"

namespace puffy::ui {

AudioDeviceModel::AudioDeviceModel(audio::IAudioCapture* capture, audio::IAudioOutput* output, QObject* parent)
    : QObject(parent), capture_(capture), output_(output) { refresh(); }

void AudioDeviceModel::refresh() {
    inputNames_.clear(); outputNames_.clear(); inputIds_.clear(); outputIds_.clear();
    if (capture_ != nullptr) for (const auto& device : capture_->devices()) { inputNames_.append(QString::fromStdString(device.name)); inputIds_.append(QString::fromStdString(device.id)); if (device.isDefault) selectedInput_ = QString::fromStdString(device.id); }
    if (output_ != nullptr) for (const auto& device : output_->devices()) { outputNames_.append(QString::fromStdString(device.name)); outputIds_.append(QString::fromStdString(device.id)); if (device.isDefault) selectedOutput_ = QString::fromStdString(device.id); }
    emit devicesChanged(); emit selectionChanged();
}

QString AudioDeviceModel::inputIdAt(int index) const { return index >= 0 && index < inputIds_.size() ? inputIds_.at(index) : QString{}; }
QString AudioDeviceModel::outputIdAt(int index) const { return index >= 0 && index < outputIds_.size() ? outputIds_.at(index) : QString{}; }

void AudioDeviceModel::setSelectedInput(const QString& value) { if (selectedInput_ == value) return; selectedInput_ = value; emit selectionChanged(); }
void AudioDeviceModel::setSelectedOutput(const QString& value) { if (selectedOutput_ == value) return; selectedOutput_ = value; emit selectionChanged(); }

} // namespace puffy::ui
