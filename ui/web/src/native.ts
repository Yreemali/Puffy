import type { Sound, Playlist } from './types'
import { invoke } from '@tauri-apps/api/core'

/** Stable boundary for Tauri/C++ IPC. The UI never talks to PipeWire directly. */
export interface NativeBridge {
  playSound(id: number, route?: string): Promise<void>
  stopAll(): Promise<void>
  librarySnapshot(): Promise<string>
  soundWaveform(id: number, points: number): Promise<number[]>
  mixerLevels(): Promise<{ microphone: number; soundboard: number; monitoring: number; virtualOutput: number }>
  addSounds(paths: string[]): Promise<string>
  setSoundVolume(id: number, value: number): Promise<void>
  setSoundRoute(id: number, route: string): Promise<void>
  setPlaybackMode(id: number, mode: number): Promise<void>
  setSoundHotkey(id: number, hotkey: string): Promise<void>
  setFullKeyboard(config: { enabled: boolean; mode: number; playlistId: number; singleSoundId: number; avoidRepeats: boolean; triggerOnRepeat: boolean; ignoreCtrl: boolean; ignoreShift: boolean; ignoreAlt: boolean; ignoreSuper: boolean }): Promise<void>
  createPlaylist(name: string): Promise<string>
  addSoundToPlaylist(playlistId: number, soundId: number): Promise<string>
  setPlaylistHotkey(playlistId: number, hotkey: string, mode: number, nextHotkey: string): Promise<void>
  setMicrophoneGain(value: number): Promise<void>
  setMonitorMicrophone(enabled: boolean): Promise<void>
  setMasterVolume(value: number): Promise<void>
  setSoundboardVolume(value: number): Promise<void>
  setMonitoringVolume(value: number): Promise<void>
  setVirtualOutputVolume(value: number): Promise<void>
  setMonitoringMuted(muted: boolean): Promise<void>
  setVirtualMicrophoneMuted(muted: boolean): Promise<void>
  setEffectParameter(effect: string, parameter: string, value: number): Promise<void>
  savePlaylist(playlist: Playlist): Promise<void>
}

type InjectedPuffyBridge = NativeBridge & { isNative?: boolean }

declare global {
  interface Window { __PUFFY_NATIVE__?: InjectedPuffyBridge }
}

const demoBridge: NativeBridge = {
  async playSound() {}, async stopAll() {}, async librarySnapshot() { return JSON.stringify({ sounds: demoSounds, playlists: demoPlaylists }) },
  async soundWaveform(id, points) { return Array.from({ length: points }, (_, index) => .15 + Math.abs(Math.sin((index + id) * .41)) * .75) },
  async mixerLevels() { return { microphone: 0, soundboard: 0, monitoring: 0, virtualOutput: 0 } },
  async addSounds() { return JSON.stringify({ sounds: demoSounds, playlists: demoPlaylists }) },
  async setSoundVolume() {}, async setSoundRoute() {}, async setPlaybackMode() {}, async setSoundHotkey() {}, async setFullKeyboard() {}, async createPlaylist() { return JSON.stringify({ sounds: demoSounds, playlists: demoPlaylists }) }, async addSoundToPlaylist() { return JSON.stringify({ sounds: demoSounds, playlists: demoPlaylists }) }, async setMicrophoneGain() {}, async setMonitorMicrophone() {},
  async setMasterVolume() {}, async setSoundboardVolume() {}, async setMonitoringVolume() {}, async setVirtualOutputVolume() {}, async setMonitoringMuted() {}, async setVirtualMicrophoneMuted() {}, async setEffectParameter() {}, async setPlaylistHotkey() {}, async savePlaylist() {},
}

const tauriBridge: NativeBridge = {
  playSound: (id, route) => invoke('play_sound', { soundId: id, route: route ?? 'both' }),
  stopAll: () => invoke('stop_all'),
  librarySnapshot: () => invoke<string>('library_snapshot'),
  soundWaveform: (id, points) => invoke<number[]>('sound_waveform', { soundId: id, points }),
  mixerLevels: () => invoke('mixer_levels'),
  addSounds: paths => invoke<string>('add_sounds', { paths }),
  setSoundVolume: (id, value) => invoke('set_sound_volume', { soundId: id, value }),
  setSoundRoute: (id, route) => invoke('set_sound_route', { soundId: id, route }),
  setPlaybackMode: (id, mode) => invoke('set_sound_playback_mode', { soundId: id, mode }),
  setSoundHotkey: (id, hotkey) => invoke('set_sound_hotkey', { soundId: id, hotkey }),
  setFullKeyboard: config => invoke('set_full_keyboard', { config }),
  createPlaylist: name => invoke<string>('create_playlist', { name }),
  addSoundToPlaylist: (playlistId, soundId) => invoke<string>('add_sound_to_playlist', { playlistId, soundId }),
  setPlaylistHotkey: (playlistId, hotkey, mode, nextHotkey) => invoke('set_playlist_hotkey', { playlistId, hotkey, mode, nextHotkey }),
  setMicrophoneGain: value => invoke('set_microphone_gain', { value }),
  setMonitorMicrophone: enabled => invoke('set_monitor_microphone', { enabled }),
  setMasterVolume: value => invoke('set_master_volume', { value }),
  setSoundboardVolume: value => invoke('set_soundboard_volume', { value }),
  setMonitoringVolume: value => invoke('set_monitoring_volume', { value }),
  setVirtualOutputVolume: value => invoke('set_virtual_output_volume', { value }),
  setMonitoringMuted: muted => invoke('set_monitoring_muted', { muted }),
  setVirtualMicrophoneMuted: muted => invoke('set_virtual_microphone_muted', { muted }),
  setEffectParameter: (effect, parameter, value) => invoke('set_effect_parameter', { effect, parameter, value }),
  savePlaylist: playlist => invoke('save_playlist', { playlist }),
}

/**
 * The desktop host injects this object before mounting React. Keeping the
 * fallback makes the UI usable in Vite without pretending the browser owns
 * realtime audio devices.
 */
export const isNative = Boolean((window as Window & { __TAURI_INTERNALS__?: unknown }).__TAURI_INTERNALS__ || window.__PUFFY_NATIVE__)
export const native: NativeBridge = window.__PUFFY_NATIVE__ ?? (isNative ? tauriBridge : demoBridge)

export const demoSounds: Sound[] = [
  { id: 1, name: 'Airhorn', duration: 4.2, volume: .9, playbackMode: 1, hotkey: 'F1', route: 'both', favorite: true, lastPlayed: 'Just now', color: '#ef8a59', playlistIds: [1] },
  { id: 2, name: 'Deep laugh', duration: 2.8, volume: .72, playbackMode: 1, hotkey: 'F2', route: 'microphone', favorite: false, lastPlayed: '12 min ago', color: '#9b8af2', playlistIds: [1, 2] },
  { id: 3, name: 'Bruh moment', duration: 1.7, volume: 1, playbackMode: 1, hotkey: 'F3', route: 'both', favorite: true, lastPlayed: 'Yesterday', color: '#4cae9c', playlistIds: [2] },
  { id: 4, name: 'Tiny applause', duration: 5.4, volume: .8, playbackMode: 1, hotkey: 'CTRL + 1', route: 'headphones', favorite: false, color: '#e9b85b', playlistIds: [1] },
  { id: 5, name: 'Suspense hit', duration: 8.1, volume: .65, playbackMode: 1, hotkey: 'ALT + Q', route: 'both', favorite: false, color: '#507ac8', playlistIds: [3] },
  { id: 6, name: 'What was that?', duration: 2.1, volume: 1, playbackMode: 1, route: 'microphone', favorite: false, color: '#d46b92', playlistIds: [2] },
]

export const demoPlaylists: Playlist[] = [
  { id: 1, name: 'Daily drivers', description: 'The sounds you actually use', color: '#d97892', soundIds: [1, 2, 4] },
  { id: 2, name: 'Memes', description: 'A little chaos, responsibly applied', color: '#8c7cf0', soundIds: [2, 3, 6] },
  { id: 3, name: 'Stream kit', description: 'Ready for OBS and calls', color: '#4cae9c', soundIds: [5] },
]
