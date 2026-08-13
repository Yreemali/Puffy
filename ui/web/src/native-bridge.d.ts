export {}

declare global {
  interface Window {
    __PUFFY_NATIVE__?: {
      isNative?: boolean
      playSound(id: number, route?: string): Promise<void>
      stopAll(): Promise<void>
      importAudio(): Promise<string[]>
      setMasterVolume(value: number): Promise<void>
      savePlaylist(playlist: import('./types').Playlist): Promise<void>
    }
  }
}
