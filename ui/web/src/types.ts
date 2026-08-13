export type Page = 'home' | 'library' | 'playlists' | 'soundboard' | 'hotkeys' | 'microphone' | 'effects' | 'devices' | 'mixer' | 'history' | 'settings'
export type SoundRoute = 'headphones' | 'microphone' | 'both' | 'none'
export type ThemeMode = 'dark' | 'light'
export type PaletteName = 'rose' | 'lavender' | 'mint' | 'peach' | 'ocean'
export interface UserProfile { name: string; avatar: string; theme: ThemeMode; palette: PaletteName }

export interface Sound {
  id: number
  name: string
  duration: number
  volume: number
  playbackMode: number
  hotkey?: string
  route: SoundRoute
  favorite: boolean
  lastPlayed?: string
  color: string
  playlistIds: number[]
  waveform?: number[]
}

export interface Playlist { id: number; name: string; description: string; color: string; soundIds: number[]; hotkey?: string; nextHotkey?: string; playbackMode?: 'sequential' | 'random' | number }
