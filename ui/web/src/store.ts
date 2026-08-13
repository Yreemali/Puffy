import { create } from 'zustand'
import type { Page, Sound, Playlist } from './types'
import { demoPlaylists, demoSounds, native } from './native'

function storedHistory(): number[] {
  try {
    const value = JSON.parse(localStorage.getItem('puffy.history') ?? '[]')
    return Array.isArray(value) ? value.filter((id): id is number => typeof id === 'number') : []
  } catch { return [] }
}

interface AppState {
  page: Page; sounds: Sound[]; playlists: Playlist[]; query: string; selectedSound: number | null
  masterVolume: number; isPlaying: boolean; currentSound: number | null; sidebarCollapsed: boolean
  soundboardVolume: number; monitoringVolume: number; virtualOutputVolume: number; monitoringMuted: boolean; virtualMicrophoneMuted: boolean; activeSoundIds: number[]
  history: number[]; playTokens: Record<number, number>
  error: string | null
  setPage: (page: Page) => void; setQuery: (query: string) => void; toggleFavorite: (id: number) => void
  setSoundVolume: (id: number, volume: number) => void; setSoundRoute: (id: number, route: Sound['route']) => void; setPlaybackMode: (id: number, mode: number) => void; play: (id: number) => void; stop: () => void
  hydrate: (snapshot: string) => void
  createPlaylist: (name: string) => void; addToPlaylist: (playlistId: number, soundId: number) => void
  removeFromPlaylist: (playlistId: number, soundId: number) => void; setMasterVolume: (value: number) => void
  clearError: () => void; removeActiveSound: (id: number, token?: number) => void
  setSoundboardVolume: (value: number) => void; setMonitoringVolume: (value: number) => void; setVirtualOutputVolume: (value: number) => void
  setMonitoringMuted: (muted: boolean) => void; setVirtualMicrophoneMuted: (muted: boolean) => void
}

export const useAppStore = create<AppState>((set, get) => ({
  page: 'home', sounds: demoSounds, playlists: demoPlaylists, query: '', selectedSound: null,
  masterVolume: .8, soundboardVolume: .8, monitoringVolume: .6, virtualOutputVolume: 1, monitoringMuted: false, virtualMicrophoneMuted: false, activeSoundIds: [], history: storedHistory(), playTokens: {}, isPlaying: false, currentSound: null, sidebarCollapsed: false, error: null,
  setPage: page => set({ page }), setQuery: query => set({ query }),
  toggleFavorite: id => set(s => ({ sounds: s.sounds.map(x => x.id === id ? { ...x, favorite: !x.favorite } : x) })),
  setSoundVolume: (id, volume) => { set(s => ({ sounds: s.sounds.map(x => x.id === id ? { ...x, volume } : x) })); void native.setSoundVolume(id, volume).catch(() => undefined) },
  setSoundRoute: (id, route) => { set(s => ({ sounds: s.sounds.map(x => x.id === id ? { ...x, route } : x) })); void native.setSoundRoute(id, route).catch(error => set({ error: String(error) })) },
  setPlaybackMode: (id, mode) => { set(s => ({ sounds: s.sounds.map(x => x.id === id ? { ...x, playbackMode: mode } : x) })); void native.setPlaybackMode(id, mode).catch(error => set({ error: String(error) })) },
  hydrate: snapshot => {
    try {
      const parsed = JSON.parse(snapshot) as { sounds?: Sound[]; playlists?: Playlist[] }
      if (Array.isArray(parsed.sounds) && Array.isArray(parsed.playlists)) set({ sounds: parsed.sounds, playlists: parsed.playlists })
    } catch { /* Keep the last usable state if native data is unavailable. */ }
  },
  play: id => {
    const sound = get().sounds.find(item => item.id === id)
    if (!sound) return
    const token = (get().playTokens[id] ?? 0) + 1
    const nextHistory = [id, ...get().history.filter(item => item !== id)].slice(0, 100)
    try { localStorage.setItem('puffy.history', JSON.stringify(nextHistory)) } catch { /* optional persistence */ }
    set(s => ({
      currentSound: id,
      selectedSound: id,
      isPlaying: true,
      activeSoundIds: s.activeSoundIds.includes(id) ? s.activeSoundIds : [...s.activeSoundIds, id],
      history: nextHistory,
      playTokens: { ...s.playTokens, [id]: token },
      sounds: s.sounds.map(item => item.id === id ? { ...item, lastPlayed: 'Just now' } : item),
      error: null,
    }))
    void native.playSound(id).then(() => {
      window.setTimeout(() => get().removeActiveSound(id, token), Math.max(100, sound.duration * 1000 + 100))
    }).catch(error => set({ isPlaying: false, error: String(error) }))
  },
  stop: () => {
    set({ isPlaying: false, currentSound: null, activeSoundIds: [], playTokens: {} })
    void native.stopAll().catch(error => set({ error: String(error) }))
  },
  createPlaylist: name => set(s => ({ playlists: [...s.playlists, { id: Date.now(), name, description: 'A new collection', color: '#668bd3', soundIds: [] }] })),
  addToPlaylist: (playlistId, soundId) => set(s => ({ playlists: s.playlists.map(p => p.id === playlistId && !p.soundIds.includes(soundId) ? { ...p, soundIds: [...p.soundIds, soundId] } : p) })),
  removeFromPlaylist: (playlistId, soundId) => set(s => ({ playlists: s.playlists.map(p => p.id === playlistId ? { ...p, soundIds: p.soundIds.filter(id => id !== soundId) } : p) })),
  setMasterVolume: value => { set({ masterVolume: value }); void native.setMasterVolume(value).catch(error => set({ error: String(error) })) },
  setSoundboardVolume: value => { set({ soundboardVolume: value }); void native.setSoundboardVolume(value).catch(error => set({ error: String(error) })) },
  setMonitoringVolume: value => { set({ monitoringVolume: value }); void native.setMonitoringVolume(value).catch(error => set({ error: String(error) })) },
  setVirtualOutputVolume: value => { set({ virtualOutputVolume: value }); void native.setVirtualOutputVolume(value).catch(error => set({ error: String(error) })) },
  setMonitoringMuted: muted => { set({ monitoringMuted: muted }); void native.setMonitoringMuted(muted).catch(error => set({ error: String(error) })) },
  setVirtualMicrophoneMuted: muted => { set({ virtualMicrophoneMuted: muted }); void native.setVirtualMicrophoneMuted(muted).catch(error => set({ error: String(error) })) },
  removeActiveSound: (id, token) => set(s => {
    if (token !== undefined && s.playTokens[id] !== token) return s
    const active = s.activeSoundIds.filter(item => item !== id)
    const nextCurrent = s.currentSound === id ? (active.at(-1) ?? null) : s.currentSound
    const nextTokens = { ...s.playTokens }
    delete nextTokens[id]
    return { activeSoundIds: active, isPlaying: active.length > 0, currentSound: nextCurrent, playTokens: nextTokens }
  }),
  clearError: () => set({ error: null }),
}))
