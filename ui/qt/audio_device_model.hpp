#pragma once

#include "core/audio/audio_ports.hpp"

#include <QObject>
#include <QStringList>

namespace puffy::ui {

class AudioDeviceModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList inputNames READ inputNames NOTIFY devicesChanged)
    Q_PROPERTY(QStringList outputNames READ outputNames NOTIFY devicesChanged)
    Q_PROPERTY(QString selectedInput READ selectedInput WRITE setSelectedInput NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedOutput READ selectedOutput WRITE setSelectedOutput NOTIFY selectionChanged)

public:
    AudioDeviceModel(audio::IAudioCapture* capture, audio::IAudioOutput* output, QObject* parent = nullptr);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QString inputIdAt(int index) const;
    Q_INVOKABLE QString outputIdAt(int index) const;
    QStringList inputNames() const { return inputNames_; }
    QStringList outputNames() const { return outputNames_; }
    QString selectedInput() const { return selectedInput_; }
    QString selectedOutput() const { return selectedOutput_; }
    void setSelectedInput(const QString& value);
    void setSelectedOutput(const QString& value);

signals:
    void devicesChanged();
    void selectionChanged();

private:
    audio::IAudioCapture* capture_;
    audio::IAudioOutput* output_;
    QStringList inputNames_;
    QStringList outputNames_;
    QStringList inputIds_;
    QStringList outputIds_;
    QString selectedInput_;
    QString selectedOutput_;
};

} // namespace puffy::ui
