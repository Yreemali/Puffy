import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    visible: true
    width: 1180
    height: 760
    minimumWidth: 900
    minimumHeight: 600
    title: "puffy — soundboard"

    property color pageColor: "#fff8f5"
    property color panelColor: "#ffffff"
    property color accentColor: "#d98ca4"
    property color accentSoftColor: "#f4dbe4"
    property color textColor: "#453b49"

    color: pageColor

    function toggleColor() {
        accentColor = accentColor === "#d98ca4" ? "#8fa7d8" : "#d98ca4"
        accentSoftColor = accentColor === "#d98ca4" ? "#f4dbe4" : "#dfe7f8"
    }

    function keyName(key) {
        if (key >= Qt.Key_F1 && key <= Qt.Key_F12) return "F" + (key - Qt.Key_F1 + 1)
        if (key >= Qt.Key_A && key <= Qt.Key_Z) return String.fromCharCode(65 + key - Qt.Key_A)
        if (key >= Qt.Key_0 && key <= Qt.Key_9) return String.fromCharCode(48 + key - Qt.Key_0)
        if (key === Qt.Key_Space) return "SPACE"
        if (key === Qt.Key_Escape) return "ESCAPE"
        if (key === Qt.Key_Tab) return "TAB"
        if (key === Qt.Key_Return || key === Qt.Key_Enter) return "RETURN"
        return ""
    }

    FileDialog {
        id: addSoundDialog
        title: "Add sounds to puffy"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Audio files (*.wav *.WAV *.flac *.ogg *.oga *.aiff *.au)", "All files (*)"]
        onAccepted: soundModel.addSounds(selectedFiles.map(function(url) { return url.toString().replace("file://", "") }))
    }
    FileDialog {
        id: exportProfileDialog
        title: "Export puffy profile"
        fileMode: FileDialog.SaveFile
        nameFilters: ["puffy profile (*.json)", "All files (*)"]
        onAccepted: soundModel.exportProfile(selectedFile.toString().replace("file://", ""))
    }
    FileDialog {
        id: importProfileDialog
        title: "Import puffy profile"
        fileMode: FileDialog.OpenFile
        nameFilters: ["puffy profile (*.json)", "All files (*)"]
        onAccepted: soundModel.importProfile(selectedFile.toString().replace("file://", ""))
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 18

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label { text: "puffy"; font.pixelSize: 32; font.bold: true; color: window.textColor }
                Label { text: "your tiny cloud of sounds"; color: "#8c7f8d"; font.pixelSize: 14 }
            }

            Button {
                text: "Change palette"
                onClicked: window.toggleColor()
                background: Rectangle { radius: 14; color: window.accentSoftColor }
                contentItem: Label { text: parent.text; color: window.textColor; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
            Label {
                text: "●  " + soundModel.audioState.toUpper()
                color: soundModel.audioState === "Running" ? "#71a58d" : (soundModel.audioState === "Degraded" ? "#c79755" : "#b65b67")
                font.bold: true
            }
            ComboBox { model: soundModel.profileNames; currentIndex: soundModel.currentProfile; onActivated: soundModel.currentProfile = currentIndex }
            Button { text: "Export"; flat: true; onClicked: exportProfileDialog.open() }
            Button { text: "Import"; flat: true; onClicked: importProfileDialog.open() }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 18

            Rectangle {
                Layout.preferredWidth: 210
                Layout.fillHeight: true
                radius: 22
                color: window.panelColor
                border.color: "#f0e5e8"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 8
                    Label { text: "WORKSPACE"; color: "#aa9ba8"; font.pixelSize: 11; font.bold: true; Layout.leftMargin: 8 }
                    Repeater {
                        model: ["Soundboard", "Library", "Playlists", "Microphone", "Effects", "Hotkeys", "Settings"]
                        delegate: Button {
                            Layout.fillWidth: true
                            text: modelData
                            flat: true
                            leftPadding: 14
                            horizontalAlignment: Text.AlignLeft
                            background: Rectangle { radius: 12; color: index === 0 ? window.accentSoftColor : "transparent" }
                            contentItem: Label { text: parent.text; color: window.textColor; verticalAlignment: Text.AlignVCenter }
                        }
                    }
                    Item { Layout.fillHeight: true }
                        Label { text: "48 kHz  •  128 frames"; color: "#9e919c"; font.pixelSize: 11; Layout.leftMargin: 8 }
                    Label { text: soundModel.bufferFrames + " frames  •  estimated latency " + soundModel.estimatedLatencyMs.toFixed(1) + " ms"; color: "#9e919c"; font.pixelSize: 11; Layout.leftMargin: 8 }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 16

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 82
                    radius: 20
                    color: window.accentSoftColor
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 18; spacing: 14
                        ColumnLayout { Layout.fillWidth: true; Label { text: "FULL KEYBOARD MODE"; color: window.textColor; font.bold: true; font.pixelSize: 16 }; Label { text: "Every allowed key can trigger a sound"; color: "#806f7d" } }
                        Switch { checked: soundModel.fullKeyboardEnabled; text: checked ? "ON" : "OFF"; onToggled: soundModel.fullKeyboardEnabled = checked }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 104
                    radius: 18
                    color: "#fffefd"
                    border.color: "#f0e5e8"
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 14; spacing: 8
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "Mode"; color: window.textColor; font.bold: true; Layout.fillWidth: true }
                            ComboBox { model: ["Random", "Playlist / sequential", "Single sound"]; currentIndex: soundModel.fullKeyboardMode; onActivated: soundModel.fullKeyboardMode = currentIndex }
                            ComboBox { visible: soundModel.fullKeyboardMode === 1; model: playlistModel; textRole: "name"; currentIndex: playlistModel.currentPlaylist; onActivated: playlistModel.currentPlaylist = currentIndex }
                            ComboBox { visible: soundModel.fullKeyboardMode === 2; model: soundModel; textRole: "name"; onActivated: soundModel.fullKeyboardSingleSound = soundModel.soundIdAt(currentIndex) }
                            CheckBox { text: "Avoid repeats"; checked: soundModel.avoidImmediateRepeats; onToggled: soundModel.avoidImmediateRepeats = checked }
                            CheckBox { text: "OS repeat"; checked: soundModel.triggerOnRepeat; onToggled: soundModel.triggerOnRepeat = checked }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "Ignore in full mode:"; color: "#806f7d"; Layout.fillWidth: true }
                            CheckBox { text: "Ctrl"; checked: soundModel.ignoreCtrl; onToggled: soundModel.ignoreCtrl = checked }
                            CheckBox { text: "Shift"; checked: soundModel.ignoreShift; onToggled: soundModel.ignoreShift = checked }
                            CheckBox { text: "Alt"; checked: soundModel.ignoreAlt; onToggled: soundModel.ignoreAlt = checked }
                            CheckBox { text: "Super"; checked: soundModel.ignoreSuper; onToggled: soundModel.ignoreSuper = checked }
                        }
                        Label { text: "Source: entire sound library  •  all options are off by default"; color: "#9e919c"; font.pixelSize: 12 }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "Ignore keycodes:"; color: "#806f7d" }
                            TextField { Layout.fillWidth: true; text: soundModel.ignoredKeyCodes; placeholderText: "e.g. 9, 65, 108"; onEditingFinished: soundModel.ignoredKeyCodes = text }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 86
                    radius: 18
                    color: "#fffefd"
                    border.color: "#f0e5e8"
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 14; spacing: 16
                        Label { text: "Microphone effects"; color: window.textColor; font.bold: true; Layout.preferredWidth: 150 }
                        ColumnLayout { Layout.fillWidth: true; Label { text: "Gain  " + Math.round(soundModel.microphoneGain * 100) + "%"; color: "#806f7d" }; Slider { Layout.fillWidth: true; from: 0; to: 4; value: soundModel.microphoneGain; onMoved: soundModel.microphoneGain = value } }
                        ColumnLayout { Layout.fillWidth: true; Label { text: "Gate  " + soundModel.gateThreshold.toFixed(3); color: "#806f7d" }; Slider { Layout.fillWidth: true; from: 0; to: 0.2; value: soundModel.gateThreshold; onMoved: soundModel.gateThreshold = value } }
                        ColumnLayout { Layout.fillWidth: true; Label { text: "Limiter  " + Math.round(soundModel.limiterCeiling * 100) + "%"; color: "#806f7d" }; Slider { Layout.fillWidth: true; from: 0.5; to: 1; value: soundModel.limiterCeiling; onMoved: soundModel.limiterCeiling = value } }
                    }
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 14; spacing: 16
                        Label { text: "Voice shaping"; color: window.textColor; font.bold: true; Layout.preferredWidth: 150 }
                        ColumnLayout { Layout.fillWidth: true; Label { text: "Comp ratio  " + soundModel.compressorRatio.toFixed(1); color: "#806f7d" }; Slider { Layout.fillWidth: true; from: 1; to: 20; value: soundModel.compressorRatio; onMoved: soundModel.compressorRatio = value } }
                        ColumnLayout { Layout.fillWidth: true; Label { text: "Low-pass  " + Math.round(soundModel.lowPassCutoff) + " Hz"; color: "#806f7d" }; Slider { Layout.fillWidth: true; from: 1000; to: 20000; value: soundModel.lowPassCutoff; onMoved: soundModel.lowPassCutoff = value } }
                        ColumnLayout { Layout.fillWidth: true; Label { text: "High-pass  " + Math.round(soundModel.highPassCutoff) + " Hz"; color: "#806f7d" }; Slider { Layout.fillWidth: true; from: 20; to: 1000; value: soundModel.highPassCutoff; onMoved: soundModel.highPassCutoff = value } }
                        ColumnLayout { Layout.fillWidth: true; Label { text: "Delay  " + Math.round(soundModel.delayMix * 100) + "%"; color: "#806f7d" }; Slider { Layout.fillWidth: true; from: 0; to: 1; value: soundModel.delayMix; onMoved: soundModel.delayMix = value } }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "My sounds"; color: window.textColor; font.pixelSize: 22; font.bold: true; Layout.fillWidth: true }
                    Button { text: "+ Add sound"; onClicked: addSoundDialog.open(); background: Rectangle { radius: 14; color: window.accentColor }; contentItem: Label { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter } }
                }

                Rectangle {
                    visible: soundModel.lastError.length > 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    radius: 12
                    color: "#fde4e4"
                    Label { anchors.fill: parent; anchors.margins: 12; text: soundModel.lastError; color: "#a34f5b"; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 3
                    rowSpacing: 14
                    columnSpacing: 14
                    Repeater {
                        model: soundModel
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 140
                            radius: 20
                            color: window.panelColor
                            border.color: "#f0e5e8"
                            ColumnLayout {
                                anchors.fill: parent; anchors.margins: 16; spacing: 6
                                Label { text: name; color: window.textColor; font.bold: true; font.pixelSize: 17; Layout.fillWidth: true }
                                RowLayout {
                                    Layout.fillWidth: true
                                    TextField { Layout.fillWidth: true; text: name; onEditingFinished: soundModel.renameSound(index, text); background: Rectangle { radius: 8; color: "#fff8f5"; border.color: "#f0e5e8" } }
                                    Button { text: "×"; flat: true; onClicked: soundModel.removeSound(index) }
                                }
                                Label { text: duration > 0 ? duration.toFixed(1) + " sec" : "duration unknown"; color: "#9e919c"; Layout.fillWidth: true }
                                TextField {
                                    id: hotkeyField
                                    property bool recording: false
                                    Layout.fillWidth: true
                                    text: hotkey
                                    placeholderText: "Hotkey, e.g. CTRL+65"
                                    selectByMouse: true
                                    onEditingFinished: soundModel.assignHotkey(index, text)
                                    Keys.onPressed: function(event) {
                                        if (!recording) return
                                        var name = window.keyName(event.key)
                                        if (name.length === 0) return
                                        var modifiers = []
                                        if (event.modifiers & Qt.ControlModifier) modifiers.push("CTRL")
                                        if (event.modifiers & Qt.ShiftModifier) modifiers.push("SHIFT")
                                        if (event.modifiers & Qt.AltModifier) modifiers.push("ALT")
                                        if (event.modifiers & Qt.MetaModifier) modifiers.push("SUPER")
                                        modifiers.push(name)
                                        text = modifiers.join("+")
                                        recording = false
                                        soundModel.assignHotkey(index, text)
                                        event.accepted = true
                                    }
                                    background: Rectangle { radius: 8; color: soundModel.hasHotkeyConflict(index, hotkeyField.text) ? "#fff0f0" : "#fff8f5"; border.color: soundModel.hasHotkeyConflict(index, hotkeyField.text) ? "#df8894" : "#f0e5e8" }
                                }
                                Button { text: hotkeyField.recording ? "Press key…" : "Record"; onClicked: { hotkeyField.recording = true; hotkeyField.forceActiveFocus() }; flat: true }
                                Label { visible: soundModel.hasHotkeyConflict(index, hotkeyField.text); text: "Hotkey already used"; color: "#b65b67"; font.pixelSize: 11 }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label { text: "Volume"; color: "#9e919c"; font.pixelSize: 11 }
                                    Slider { Layout.fillWidth: true; from: 0; to: 2; value: volume; onMoved: soundModel.setSoundVolume(index, value) }
                                    ComboBox { model: ["None", "Headphones", "Virtual mic", "Both"]; currentIndex: route; onActivated: soundModel.setSoundRoute(index, currentIndex) }
                                    CheckBox { text: "Loop"; checked: loop; onToggled: soundModel.setSoundLoop(index, checked) }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label { text: "Speed"; color: "#9e919c"; font.pixelSize: 11 }
                                    Slider { Layout.fillWidth: true; from: 0.25; to: 3; value: speed; onMoved: soundModel.setSoundSpeed(index, value) }
                                    Label { text: speed.toFixed(2) + "x"; color: window.accentColor; font.pixelSize: 11 }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label { text: "Fade"; color: "#9e919c"; font.pixelSize: 11 }
                                    Slider { Layout.fillWidth: true; from: 0; to: 2; value: fadeIn; onMoved: soundModel.setSoundFades(index, value, fadeOut) }
                                    Slider { Layout.fillWidth: true; from: 0; to: 2; value: fadeOut; onMoved: soundModel.setSoundFades(index, fadeIn, value) }
                                }
                                Item { Layout.fillHeight: true }
                                RowLayout { Layout.fillWidth: true; Label { text: hotkey.length > 0 ? hotkey : "—"; color: window.accentColor; font.bold: true; Layout.fillWidth: true }; Button { text: "▶"; flat: true; onClicked: soundModel.trigger(index); contentItem: Label { text: parent.text; color: window.accentColor; font.pixelSize: 18 } } }
                            }
                        }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 68
                    radius: 18; color: "#fffefd"; border.color: "#f0e5e8"
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 14
                        Label { text: "Input"; color: "#806f7d" }
                        ComboBox { Layout.fillWidth: true; model: deviceModel.inputNames; currentIndex: 0; onActivated: deviceModel.selectedInput = deviceModel.inputIdAt(currentIndex) }
                        Label { text: "Output"; color: "#806f7d" }
                        ComboBox { Layout.fillWidth: true; model: deviceModel.outputNames; currentIndex: 0; onActivated: deviceModel.selectedOutput = deviceModel.outputIdAt(currentIndex) }
                        Button { text: "↻"; onClicked: deviceModel.refresh() }
                    }
                }
                Item { Layout.fillHeight: true }
            }
        }
    }
}
