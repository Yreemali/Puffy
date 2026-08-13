#include "core/hotkeys/hotkey_manager.hpp"
#include "core/mixer/mixer.hpp"
#include "core/library/sound_library.hpp"
#include "core/audio/audio_decoder.hpp"
#include "core/audio/audio_engine.hpp"
#include "core/soundboard/playback_policy.hpp"
#include "core/library/sound_cache.hpp"
#include "core/soundboard/soundboard_service.hpp"
#include "core/profiles/profile_store.hpp"
#include "core/effects/audio_effect.hpp"
#include "core/soundboard/full_keyboard_mode.hpp"

#include <cassert>
#include <vector>
#include <filesystem>
#include <fstream>
#include <string>

namespace {
class MockOutput final : public puffy::audio::IAudioOutput {
public:
    std::vector<puffy::audio::AudioDeviceInfo> devices() const override { return {}; }
    bool open(const std::string&, puffy::audio::AudioFormat format) override { format_ = format; opened = true; return true; }
    bool start() override { started = opened; return started; }
    void stop() noexcept override { started = false; }
    bool write(std::span<const float> samples, std::size_t frames) noexcept override {
        if (!started) return false;
        last.assign(samples.begin(), samples.end()); lastFrames = frames; return true;
    }
    puffy::audio::AudioFormat format_{};
    std::vector<float> last;
    std::size_t lastFrames{0};
    bool opened{false};
    bool started{false};
};

class MockVirtualMicrophone final : public puffy::audio::IVirtualMicrophone {
public:
    bool initialize() override { return true; }
    bool start() override { started = true; return true; }
    void stop() noexcept override { started = false; }
    bool pushAudio(std::span<const float> samples, std::size_t frames, int, int) noexcept override {
        received.assign(samples.begin(), samples.end()); receivedFrames = frames; return started;
    }
    std::vector<float> received;
    std::size_t receivedFrames{0};
    bool started{false};
};

class MockCapture final : public puffy::audio::IAudioCapture {
public:
    std::vector<puffy::audio::AudioDeviceInfo> devices() const override { return {}; }
    bool open(const std::string&, puffy::audio::AudioFormat, Callback callback) override { callback_ = std::move(callback); return true; }
    void close() noexcept override { callback_ = {}; }
    void emit(std::span<const float> samples, std::size_t frames, int channels) { callback_({samples, frames, channels}); }
private:
    Callback callback_;
};
} // namespace

int main() {
    using namespace puffy;
    soundboard::FullKeyboardModeController mode;
    mode.setEnabled(true);
    mode.setMode(soundboard::FullKeyboardMode::Sequential);
    mode.setPlaylist({10, 20});
    assert(mode.onKeyPress(1) == 10);
    assert(mode.onKeyPress(1) == 20);
    assert(mode.onKeyPress(1) == 10);
    mode.setIgnoredKey(1, true);
    assert(mode.onKeyPress(1) == -1);
    mode.setIgnoredKey(1, false);
    assert(mode.onKeyPress(1, true) == -1);
    mode.setTriggerOnRepeat(true);
    assert(mode.onKeyPress(1, true) >= 0);

    int calls = 0;
    hotkeys::HotkeyManager manager;
    manager.bind({65, false, false, false, false}, [&] { ++calls; });
    manager.handle({65, true, false, false, false, false, false});
    assert(calls == 1);

    mixer::Mixer mixer({16, 2, 2});
    std::vector<float> samples(4, 1.0F), monitor(4), virtualMic(4);
    mixer.addVoice({samples, 2, 2}, 1.0F, audio::OutputRoute::VirtualMicrophone);
    mixer.process({}, monitor, virtualMic, 2, 2);
    assert(virtualMic[0] == 0.98F && monitor[0] == 0.0F);
    mixer.setMasterCeiling(0.5F);
    mixer.clearVoices();
    mixer.addVoice({samples, 2, 2}, 2.0F, audio::OutputRoute::VirtualMicrophone);
    mixer.process({}, monitor, virtualMic, 2, 2);
    assert(virtualMic[0] == 0.5F);

    const auto databasePath = std::filesystem::temp_directory_path() / "puffy-library-test.sqlite";
    std::filesystem::remove(databasePath);
    library::SoundLibrary library(databasePath);
    assert(library.open());
    library::Sound soundRecord;
    soundRecord.name = "Airhorn";
    soundRecord.filePath = "sounds/airhorn.wav";
    soundRecord.route = audio::OutputRoute::VirtualMicrophone;
    assert(library.add(soundRecord));
    assert(soundRecord.id > 0);
    assert(library.search("air").size() == 1);
    auto loaded = library.find(soundRecord.id);
    assert(loaded && loaded->route == audio::OutputRoute::VirtualMicrophone);
    soundRecord.favorite = true;
    assert(library.update(soundRecord));
    assert(library.remove(soundRecord.id));
    std::filesystem::remove(databasePath);

    const auto wavPath = std::filesystem::temp_directory_path() / "puffy-decoder-test.wav";
    {
        std::ofstream wav(wavPath, std::ios::binary);
        const auto write16 = [&](std::uint16_t value) { wav.put(static_cast<char>(value)); wav.put(static_cast<char>(value >> 8U)); };
        const auto write32 = [&](std::uint32_t value) { for (int i = 0; i < 4; ++i) { wav.put(static_cast<char>(value >> (i * 8))); } };
        wav.write("RIFF", 4); write32(40); wav.write("WAVEfmt ", 8); write32(16); write16(1); write16(1); write32(48000); write32(96000); write16(2); write16(16);
        wav.write("data", 4); write32(4); write16(0); write16(16384);
    }
    audio::WavDecoder decoder;
    std::string decodeError;
    const auto decoded = decoder.decode(wavPath, decodeError);
    assert(decoded && decoded->sampleRate == 48000 && decoded->channels == 1 && decoded->frames() == 2);
    assert(decoded->samples[1] > 0.49F && decoded->samples[1] < 0.51F);
    std::filesystem::remove(wavPath);

    MockCapture capture;
    MockOutput output;
    MockVirtualMicrophone virtualMicrophone;
    audio::AudioEngine engine({{48000, 2}, 16, 8, {16, 2, 2}});
    assert(engine.start(capture, output, &virtualMicrophone));
    std::vector<float> microphoneSamples{0.25F, 0.5F, 0.25F, 0.5F};
    capture.emit(microphoneSamples, 2, 2);
    assert(output.lastFrames == 2 && output.last[0] == 0.0F);
    assert(virtualMicrophone.receivedFrames == 2 && virtualMicrophone.received[1] == 0.5F);
    engine.setMonitorMicrophone(true);
    capture.emit(microphoneSamples, 2, 2);
    assert(output.last[0] == 0.25F);
    engine.setMonitoringMuted(true);
    capture.emit(microphoneSamples, 2, 2);
    assert(output.last[0] == 0.25F);
    engine.stop();

    auto soundAsset = std::make_shared<audio::DecodedAudio>();
    soundAsset->sampleRate = 48000;
    soundAsset->channels = 2;
    soundAsset->samples = {0.75F, 0.75F, 0.5F, 0.5F};
    audio::AudioEngine soundEngine({{48000, 2}, 16, 8, {16, 2, 2}});
    MockCapture soundCapture;
    MockOutput soundOutput;
    MockVirtualMicrophone soundVirtual;
    assert(soundEngine.registerSound(99, soundAsset));
    assert(soundEngine.start(soundCapture, soundOutput, &soundVirtual));
    assert(soundEngine.playSound(99, audio::OutputRoute::VirtualMicrophone));
    soundCapture.emit(microphoneSamples, 2, 2);
    assert(soundOutput.last[0] == 0.0F);
    assert(soundVirtual.received[0] == 0.98F && soundVirtual.received[1] == 0.98F);
    assert(soundEngine.stopSound(99));
    soundEngine.stop();

    effects::Gain gain(2.0F); effects::Limiter limiter;
    std::vector<float> effectSamples{0.3F, -0.3F, 0.6F, -0.6F};
    gain.prepare(48000, 2, 2); gain.process(effectSamples, 2, 2);
    limiter.process(effectSamples, 2, 2);
    assert(effectSamples[0] == 0.6F && effectSamples[2] < 1.0F);

    library::SoundCache cache(16);
    audio::WavDecoder cacheDecoder;
    std::string cacheError;
    const auto cachedA = cache.load(wavPath, cacheDecoder, cacheError);
    assert(cachedA == nullptr); // fixture was removed above; failed loads are not cached

    const auto cachePathA = std::filesystem::temp_directory_path() / "puffy-cache-a.wav";
    const auto cachePathB = std::filesystem::temp_directory_path() / "puffy-cache-b.wav";
    const auto writeTestWav = [](const std::filesystem::path& path, std::uint16_t sample) {
        std::ofstream wav(path, std::ios::binary);
        const auto write16 = [&](std::uint16_t value) { wav.put(static_cast<char>(value)); wav.put(static_cast<char>(value >> 8U)); };
        const auto write32 = [&](std::uint32_t value) { for (int i = 0; i < 4; ++i) wav.put(static_cast<char>(value >> (i * 8))); };
        wav.write("RIFF", 4); write32(40); wav.write("WAVEfmt ", 8); write32(16); write16(1); write16(1); write32(48000); write32(96000); write16(2); write16(16);
        wav.write("data", 4); write32(4); write16(0); write16(sample);
    };
    writeTestWav(cachePathA, 8192);
    writeTestWav(cachePathB, 4096);
    cache.setMaxBytes(32);
    const auto cachedFirst = cache.load(cachePathA, cacheDecoder, cacheError);
    const auto cachedHit = cache.load(cachePathA, cacheDecoder, cacheError);
    assert(cachedFirst && cachedFirst == cachedHit && cache.bytesUsed() == 8);
    const auto cachedSecond = cache.load(cachePathB, cacheDecoder, cacheError);
    assert(cachedSecond && cache.bytesUsed() == 16);
    cache.setMaxBytes(8);
    assert(cache.bytesUsed() == 8);

    const auto serviceDatabasePath = std::filesystem::temp_directory_path() / "puffy-service-test.sqlite";
    std::filesystem::remove(serviceDatabasePath);
    library::SoundLibrary serviceLibrary(serviceDatabasePath);
    assert(serviceLibrary.open());
    library::Sound serviceSound;
    serviceSound.name = "Service sound";
    serviceSound.filePath = cachePathA;
    serviceSound.route = audio::OutputRoute::VirtualMicrophone;
    serviceSound.playbackMode = library::PlaybackMode::Restart;
    assert(serviceLibrary.add(serviceSound));
    audio::AudioEngine serviceEngine({{48000, 2}, 16, 8, {16, 2, 2}});
    soundboard::SoundboardService service(serviceLibrary, cache, cacheDecoder, serviceEngine);
    assert(service.prepareSound(serviceSound.id));
    assert(service.trigger(serviceSound.id, true));
    assert(service.trigger(serviceSound.id, true));
    service.stopAll();
    std::filesystem::remove(serviceDatabasePath);

    const auto profileDatabasePath = std::filesystem::temp_directory_path() / "puffy-profile-test.sqlite";
    std::filesystem::remove(profileDatabasePath);
    profiles::ProfileStore profileStore(profileDatabasePath.string());
    assert(profileStore.open());
    profiles::Profile profile; profile.name = "Gaming"; profile.fullKeyboardEnabled = true; profile.ignoreCtrl = true; profile.ignoredKeyCodes = "9,65";
    assert(profileStore.saveProfile(profile));
    assert(profileStore.profiles().size() == 1 && profileStore.profiles()[0].ignoreCtrl);
    profiles::Playlist playlist; playlist.name = "Memes"; playlist.soundIds = {1, 2, 3};
    assert(profileStore.savePlaylist(playlist));
    const auto storedPlaylists = profileStore.playlists();
    assert(storedPlaylists.size() == 1 && storedPlaylists[0].soundIds.size() == 3);
    std::filesystem::remove(profileDatabasePath);
    std::filesystem::remove(cachePathA);
    std::filesystem::remove(cachePathB);

    soundboard::PlaybackPolicy playback;
    const auto firstRestart = playback.press(1, library::PlaybackMode::Restart, audio::OutputRoute::Both, 1.0F);
    assert(firstRestart.size() == 1 && firstRestart[0].type == soundboard::PlaybackCommandType::Start);
    const auto restart = playback.press(1, library::PlaybackMode::Restart, audio::OutputRoute::Both, 1.0F);
    assert(restart.size() == 2 && restart[0].type == soundboard::PlaybackCommandType::Stop && restart[1].type == soundboard::PlaybackCommandType::Start);
    playback.markFinished(1);
    const auto ignored = playback.press(2, library::PlaybackMode::IgnoreIfPlaying, audio::OutputRoute::VirtualMicrophone, 1.0F);
    assert(ignored.size() == 1);
    assert(playback.press(2, library::PlaybackMode::IgnoreIfPlaying, audio::OutputRoute::VirtualMicrophone, 1.0F).empty());
    const auto holdStart = playback.press(3, library::PlaybackMode::Hold, audio::OutputRoute::Both, 1.0F);
    const auto holdStop = playback.release(3, library::PlaybackMode::Hold, audio::OutputRoute::Both, 1.0F);
    assert(holdStart.size() == 1 && holdStop.size() == 1 && holdStop[0].type == soundboard::PlaybackCommandType::Stop);
    return 0;
}
