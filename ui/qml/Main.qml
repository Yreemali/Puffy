import QtQuick 6.5
import QtQuick.Controls 6.5
import QtQuick.Layouts 6.5
import QtQuick.Dialogs 6.5

ApplicationWindow {
    id: window
    visible: true
    width: 1280
    height: 800
    minimumWidth: 980
    minimumHeight: 640
    title: "puffy — soundboard"

    property int selectedPage: 0
    property int activeSound: -1
    property color backgroundColor: "#121214"
    property color sidebarColor: "#0d0d0f"
    property color surfaceColor: "#1b1b1f"
    property color surfaceHover: "#27272d"
    property color textColor: "#f5f2f6"
    property color mutedColor: "#a19da8"
    property color accentColor: "#ee82a9"
    property color accentSoftColor: "#39232f"

    color: backgroundColor
    palette.window: backgroundColor
    palette.windowText: textColor
    palette.button: surfaceColor
    palette.buttonText: textColor
    palette.highlight: accentColor
    palette.highlightedText: "#ffffff"

    function toggleColor() {
        accentColor = accentColor === "#ee82a9" ? "#8db4ff" : "#ee82a9"
        accentSoftColor = accentColor === "#ee82a9" ? "#39232f" : "#202e49"
    }

    FileDialog {
        id: addSoundDialog
        title: "Add sounds to puffy"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Audio files (*.wav *.WAV *.mp3 *.MP3 *.flac *.ogg *.oga *.aiff *.au)", "All files (*)"]
        onAccepted: soundModel.addSounds(selectedFiles.map(function(url) { return url.toString().replace("file://", "") }))
    }
    FileDialog { id: exportProfileDialog; title: "Export puffy profile"; fileMode: FileDialog.SaveFile; onAccepted: soundModel.exportProfile(selectedFile.toString().replace("file://", "")) }
    FileDialog { id: importProfileDialog; title: "Import puffy profile"; fileMode: FileDialog.OpenFile; onAccepted: soundModel.importProfile(selectedFile.toString().replace("file://", "")) }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 228
            color: window.sidebarColor

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 22
                spacing: 0
                Label { text: "puffy"; color: window.textColor; font.pixelSize: 30; font.bold: true }
                Label { text: "your tiny cloud of sounds"; color: window.mutedColor; font.pixelSize: 11; Layout.topMargin: 2 }
                Item { Layout.preferredHeight: 34 }
                Label { text: "LIBRARY"; color: "#6f6b75"; font.pixelSize: 10; font.bold: true; Layout.leftMargin: 10; Layout.bottomMargin: 10 }
                Repeater {
                    model: ["Soundboard", "Library", "Playlists", "Microphone", "Effects", "Hotkeys", "Settings"]
                    delegate: Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        text: modelData
                        flat: true
                        leftPadding: 12
                        onClicked: window.selectedPage = index
                        background: Rectangle { radius: 10; color: window.selectedPage === index ? window.accentSoftColor : "transparent" }
                        contentItem: RowLayout {
                            spacing: 12
                            Label { text: ["●", "▦", "≡", "♩", "✦", "⌨", "⚙"][index]; color: window.selectedPage === index ? window.accentColor : window.mutedColor; font.pixelSize: 16 }
                            Label { text: parent.parent.text; color: window.selectedPage === index ? window.textColor : window.mutedColor; font.pixelSize: 13; Layout.fillWidth: true }
                        }
                    }
                }
                Item { Layout.fillHeight: true }
                Rectangle { Layout.fillWidth: true; height: 1; color: "#25252a"; Layout.bottomMargin: 16 }
                Label { text: "AUDIO ENGINE"; color: "#6f6b75"; font.pixelSize: 10; font.bold: true; Layout.leftMargin: 10 }
                Label { text: "●  " + soundModel.audioState.toUpperCase(); color: soundModel.audioState === "Running" ? "#72d6a1" : "#e2aa68"; font.bold: true; Layout.topMargin: 8; Layout.leftMargin: 10 }
                Label { text: "48 kHz  ·  " + soundModel.bufferFrames + " frames"; color: window.mutedColor; font.pixelSize: 11; Layout.topMargin: 4; Layout.leftMargin: 10 }
                Label { text: "Estimated latency  " + soundModel.estimatedLatencyMs.toFixed(1) + " ms"; color: window.mutedColor; font.pixelSize: 11; Layout.topMargin: 3; Layout.leftMargin: 10 }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 76
                color: window.backgroundColor
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 34
                    anchors.rightMargin: 34
                    spacing: 16
                    Label { text: ["Soundboard", "Library", "Playlists", "Microphone", "Effects", "Hotkeys", "Settings"][window.selectedPage]; color: window.textColor; font.pixelSize: 20; font.bold: true; Layout.fillWidth: true }
                    Button { text: "Palette"; flat: true; onClicked: window.toggleColor() }
                    ComboBox { model: soundModel.profileNames; currentIndex: soundModel.currentProfile; onActivated: soundModel.currentProfile = currentIndex; Layout.preferredWidth: 120 }
                    Button { text: "Export"; flat: true; onClicked: exportProfileDialog.open() }
                    Button { text: "Import"; flat: true; onClicked: importProfileDialog.open() }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: window.backgroundColor
                visible: window.selectedPage === 0
                Flickable {
                    anchors.fill: parent
                    anchors.leftMargin: 34
                    anchors.rightMargin: 34
                    anchors.bottomMargin: 84
                    contentWidth: width
                    contentHeight: contentColumn.height
                    clip: true
                    ColumnLayout {
                        id: contentColumn
                        width: parent.width
                        spacing: 24
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 150
                            radius: 18
                            color: window.accentSoftColor
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 24; spacing: 20
                                ColumnLayout { Layout.fillWidth: true; Label { text: "Make some noise."; color: window.textColor; font.pixelSize: 30; font.bold: true } Label { text: "Your soundboard, finally dressed for the occasion."; color: window.mutedColor; font.pixelSize: 14 } }
                                ColumnLayout { Layout.preferredWidth: 220; Label { text: "FULL KEYBOARD"; color: window.mutedColor; font.pixelSize: 10; font.bold: true } Switch { checked: soundModel.fullKeyboardEnabled; text: checked ? "Enabled" : "Disabled"; onToggled: soundModel.fullKeyboardEnabled = checked } }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "Your sounds"; color: window.textColor; font.pixelSize: 22; font.bold: true; Layout.fillWidth: true }
                            Label { text: soundModel.count + " sounds"; color: window.mutedColor }
                    Label { text: "Master"; color: window.mutedColor; font.pixelSize: 12 }
                    Slider { from: 0; to: 2; value: soundModel.soundboardGain; onMoved: soundModel.soundboardGain = value; Layout.preferredWidth: 130 }
                    Label { text: Math.round(soundModel.soundboardGain * 100) + "%"; color: window.mutedColor; font.pixelSize: 12 }
                    Button { text: "+ Add sound"; onClicked: addSoundDialog.open(); highlighted: true }
                        }
                        Rectangle { visible: soundModel.lastError.length > 0; Layout.fillWidth: true; Layout.preferredHeight: 38; radius: 8; color: "#4a202b"; Label { anchors.fill: parent; anchors.margins: 12; text: soundModel.lastError; color: "#ffb4c5"; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter } }
                        GridLayout {
                            Layout.fillWidth: true
                            columns: width > 850 ? 3 : 2
                            rowSpacing: 14; columnSpacing: 14
                            Repeater {
                                model: soundModel
                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 146
                                    radius: 14
                                    color: mouse.containsMouse ? window.surfaceHover : window.surfaceColor
                                    property bool playing: window.activeSound === index
                                    ColumnLayout {
                                        anchors.fill: parent; anchors.margins: 16; spacing: 8
                                        RowLayout { Layout.fillWidth: true; Label { text: name; color: window.textColor; font.pixelSize: 15; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true } Label { text: favorite ? "♥" : ""; color: window.accentColor } }
                                        Label { text: duration > 0 ? Math.floor(duration / 60) + ":" + (Math.floor(duration) % 60 < 10 ? "0" : "") + (Math.floor(duration) % 60) : "Audio file"; color: window.mutedColor; font.pixelSize: 12 }
                                        Rectangle { Layout.fillWidth: true; height: 28; radius: 6; color: "#24242a"; Row { anchors.fill: parent; anchors.margins: 7; spacing: 4; Repeater { model: 28; delegate: Rectangle { width: 3; height: 5 + ((index * 17 + modelData * 11) % 15); radius: 2; color: window.accentColor; anchors.verticalCenter: parent.verticalCenter } } } }
                                        RowLayout { Layout.fillWidth: true; Button { text: playing ? "■" : "▶"; highlighted: true; onClicked: { window.activeSound = index; soundModel.trigger(index) } } Label { text: hotkey.length > 0 ? hotkey : "No hotkey"; color: window.mutedColor; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight } Label { text: Math.round(volume * 100) + "%"; color: window.mutedColor; font.pixelSize: 11 } Button { text: "⋯"; flat: true; onClicked: soundModel.setFavorite(index, !favorite) } }
                                        RowLayout { Layout.fillWidth: true; Label { text: "Volume"; color: window.mutedColor; font.pixelSize: 10 } Slider { Layout.fillWidth: true; from: 0; to: 2; value: volume; onMoved: soundModel.setSoundVolume(index, value) } }
                                    }
                                    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; z: -1 }
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: window.backgroundColor
                visible: window.selectedPage === 2
                RowLayout {
                    anchors.fill: parent; anchors.margins: 34; spacing: 20
                    Rectangle {
                        Layout.preferredWidth: 270; Layout.fillHeight: true; radius: 16; color: window.surfaceColor
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 16; spacing: 8
                            Label { text: "Playlists"; color: window.textColor; font.pixelSize: 20; font.bold: true }
                            RowLayout { Layout.fillWidth: true; TextField { id: newPlaylistName; Layout.fillWidth: true; placeholderText: "New playlist" } Button { text: "+"; highlighted: true; onClicked: { if (playlistModel.createPlaylist(newPlaylistName.text)) newPlaylistName.clear() } } }
                            ListView {
                                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 4; clip: true; model: playlistModel
                                delegate: Button { Layout.fillWidth: true; text: name; flat: true; highlighted: index === playlistModel.currentPlaylist; onClicked: playlistModel.currentPlaylist = index }
                            }
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; Layout.fillHeight: true; spacing: 18
                        RowLayout { Layout.fillWidth: true; Label { text: playlistModel.currentPlaylist >= 0 ? "Playlist sounds" : "Choose a playlist"; color: window.textColor; font.pixelSize: 26; font.bold: true; Layout.fillWidth: true } Button { text: "Delete playlist"; flat: true; enabled: playlistModel.currentPlaylist >= 0; onClicked: playlistModel.removePlaylist(playlistModel.currentPlaylist) } }
                        Rectangle {
                            Layout.fillWidth: true; Layout.preferredHeight: 46; radius: 10; color: window.surfaceColor
                            Label { anchors.fill: parent; anchors.margins: 14; text: "Add sounds from your library below. Order follows the add sequence."; color: window.mutedColor; verticalAlignment: Text.AlignVCenter }
                        }
                        ListView {
                            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 6; model: soundModel
                            delegate: Rectangle {
                                width: ListView.view.width; height: 54; radius: 10; color: window.surfaceColor
                                RowLayout { anchors.fill: parent; anchors.margins: 12; Label { text: name; color: window.textColor; Layout.fillWidth: true } Label { text: duration > 0 ? duration.toFixed(1) + " s" : "audio"; color: window.mutedColor } Button { text: playlistModel.containsSound(soundId) ? "Remove" : "Add"; flat: true; enabled: playlistModel.currentPlaylist >= 0; onClicked: playlistModel.containsSound(soundId) ? playlistModel.removeSoundFromCurrent(soundId) : playlistModel.addSoundToCurrent(soundId) } }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: window.backgroundColor
                visible: window.selectedPage !== 0 && window.selectedPage !== 2
                ColumnLayout { anchors.centerIn: parent; spacing: 12; Label { text: ["Soundboard", "Library", "Playlists", "Microphone", "Effects", "Hotkeys", "Settings"][window.selectedPage]; color: window.textColor; font.pixelSize: 30; font.bold: true; Layout.alignment: Qt.AlignHCenter } Label { text: "This workspace is ready for the next cozy pass."; color: window.mutedColor; Layout.alignment: Qt.AlignHCenter } }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 76
                color: "#18181b"
                border.color: "#29292e"
                RowLayout { anchors.fill: parent; anchors.leftMargin: 28; anchors.rightMargin: 28; spacing: 16; Label { text: window.activeSound >= 0 ? "Playing sound " + (window.activeSound + 1) : "Ready when you are"; color: window.textColor; font.bold: true; Layout.fillWidth: true } Label { text: "Monitoring: " + (soundModel.hearMicrophone ? "mic on" : "sounds only"); color: window.mutedColor } Button { text: "Stop all"; flat: true; onClicked: soundModel.stopAllSounds() } }
            }
        }
    }
}
