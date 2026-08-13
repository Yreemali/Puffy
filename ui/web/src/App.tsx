import { useEffect, useMemo, useState } from 'react'
import type { CSSProperties } from 'react'
import { Check, Command, Moon, Palette, Sun, Volume2 } from 'lucide-react'
import { Sidebar, TopBar, PlayerBar, HomeView, LibraryView, PlaylistsView, SoundboardView, ControlView } from './components'
import { useAppStore } from './store'
import type { Page, PaletteName, ThemeMode, UserProfile } from './types'
import { isNative, native } from './native'

const titles: Record<Page, string> = { home: 'Home', library: 'Library', playlists: 'Playlists', soundboard: 'Soundboard', hotkeys: 'Hotkeys', microphone: 'Microphone', effects: 'Effects', devices: 'Audio devices', mixer: 'Mixer', history: 'History', settings: 'Settings' }

export function App() {
  const page = useAppStore(s => s.page)
  const sidebarCollapsed = useAppStore(s => s.sidebarCollapsed)
  const hydrate = useAppStore(s => s.hydrate)
  const error = useAppStore(s => s.error)
  const clearError = useAppStore(s => s.clearError)
  const [profile, setProfile] = useState<UserProfile | null>(() => loadProfile())
  const [profileOpen, setProfileOpen] = useState(false)
  useEffect(() => { void native.librarySnapshot().then(hydrate).catch(error => { if (isNative) useAppStore.setState({ sounds: [], playlists: [], error: String(error) }) }) }, [hydrate])
  useEffect(() => {
    const update = (event: Event) => {
      const next = (event as CustomEvent<UserProfile>).detail
      if (next?.name) { saveProfile(next); setProfile(next) }
    }
    window.addEventListener('puffy-profile-updated', update)
    return () => window.removeEventListener('puffy-profile-updated', update)
  }, [])
  const content = useMemo(() => {
    if (page === 'home') return <HomeView />
    if (page === 'library') return <LibraryView />
    if (page === 'playlists') return <PlaylistsView />
    if (page === 'soundboard') return <SoundboardView />
    return <ControlView type={page} />
  }, [page])
  if (!profile) return <Onboarding onComplete={next => { saveProfile(next); setProfile(next) }}/>
  return <div className={`app-shell ${sidebarCollapsed ? 'sidebar-is-collapsed' : ''}`} data-theme={profile.theme} data-palette={profile.palette} style={{ '--accent': paletteColors[profile.palette] } as CSSProperties}><Sidebar/><main className="main"><TopBar title={titles[page]} profile={profile} onProfileClick={() => setProfileOpen(true)}/><section className="content">{content}</section></main><PlayerBar/><CommandPalette/>{profileOpen && <ProfileEditor profile={profile} onClose={() => setProfileOpen(false)} onSave={next => { saveProfile(next); setProfile(next); setProfileOpen(false) }}/>} {error && <button className="native-error" onClick={clearError}>Audio error: {error} ×</button>}</div>
}

const paletteColors: Record<PaletteName, string> = { rose: '#e084a9', lavender: '#8e9eea', mint: '#69c5ae', peach: '#e7a276', ocean: '#54b8d5' }
function loadProfile(): UserProfile | null { try { const value = JSON.parse(localStorage.getItem('puffy.profile') ?? 'null'); return value?.name ? value as UserProfile : null } catch { return null } }
function saveProfile(profile: UserProfile) { localStorage.setItem('puffy.profile', JSON.stringify(profile)) }

function AvatarPicker({ avatar, onChange }: { avatar: string; onChange: (avatar: string) => void }) { const chooseImage = (file?: File) => { if (!file || !file.type.startsWith('image/')) return; const reader = new FileReader(); reader.onload = event => { const result = event.target?.result; if (typeof result === 'string') onChange(result) }; reader.readAsDataURL(file) }; return <><div className="avatar-options"><label className={avatar.startsWith('data:image/') ? 'avatar-upload-tile selected' : 'avatar-upload-tile'}>{avatar.startsWith('data:image/') ? <img src={avatar} alt="Selected avatar"/> : <span>+</span>}<input type="file" accept="image/png,image/jpeg,image/webp,image/gif" onChange={e => { chooseImage(e.target.files?.[0]); e.currentTarget.value = '' }}/></label>{['🐱', '🐰', '🦊', '🐼', '🐸', '🦄'].map(item => <button type="button" className={avatar === item ? 'avatar-option selected' : 'avatar-option'} onClick={() => onChange(item)} key={item}>{item}{avatar === item && <Check size={14}/>}</button>)}</div><small className="avatar-hint">Choose an emoji or a local image. Images are stored locally on this device.</small></> }
function ThemePicker({ theme, palette, onTheme, onPalette }: { theme: ThemeMode; palette: PaletteName; onTheme: (theme: ThemeMode) => void; onPalette: (palette: PaletteName) => void }) { return <><div className="appearance-options"><button className={theme === 'dark' ? 'appearance-option selected' : 'appearance-option'} onClick={() => onTheme('dark')}><Moon/> Dark</button><button className={theme === 'light' ? 'appearance-option selected' : 'appearance-option'} onClick={() => onTheme('light')}><Sun/> Light</button></div><div className="palette-options">{(Object.keys(paletteColors) as PaletteName[]).map(item => <button className={palette === item ? 'palette-option selected' : 'palette-option'} style={{ '--swatch': paletteColors[item] } as CSSProperties} onClick={() => onPalette(item)} key={item}><i/>{item}</button>)}</div></> }
function Onboarding({ onComplete }: { onComplete: (profile: UserProfile) => void }) {
  const [name, setName] = useState(''); const [avatar, setAvatar] = useState('🐱'); const [theme, setTheme] = useState<ThemeMode>('dark'); const [palette, setPalette] = useState<PaletteName>('rose')
  const submit = () => { if (name.trim()) onComplete({ name: name.trim(), avatar, theme, palette }) }
  return <div className="onboarding" data-theme={theme} data-palette={palette} style={{ '--accent': paletteColors[palette] } as CSSProperties}><div className="onboarding-card"><span className="logo-crop onboarding-logo"><img src="/puffy-logo.png" alt="Puffy"/></span><span className="eyebrow">WELCOME TO PUFFY</span><h1>Let’s make some noise.</h1><p className="onboarding-copy">A tiny local profile is all we need to make Puffy feel like yours.</p><label className="onboarding-field"><span>What should we call you?</span><input autoFocus value={name} onChange={e => setName(e.target.value)} onKeyDown={e => e.key === 'Enter' && submit()} placeholder="Your name" maxLength={40}/></label><div className="onboarding-section"><span>Choose an avatar</span><AvatarPicker avatar={avatar} onChange={setAvatar}/></div><div className="onboarding-section"><span>Appearance</span><ThemePicker theme={theme} palette={palette} onTheme={setTheme} onPalette={setPalette}/></div><button className="primary-button onboarding-submit" disabled={!name.trim()} onClick={submit}>Create my profile</button><small>Stored only on this device. No account or telemetry.</small></div></div>
}

function ProfileEditor({ profile, onClose, onSave }: { profile: UserProfile; onClose: () => void; onSave: (profile: UserProfile) => void }) { const [draft, setDraft] = useState(profile); return <div className="profile-overlay" onClick={onClose}><div className="profile-editor" onClick={e => e.stopPropagation()}><div className="profile-editor-head"><div><span className="eyebrow">YOUR PROFILE</span><h2>Make Puffy yours</h2></div><button className="icon-button" onClick={onClose}>×</button></div><label className="onboarding-field"><span>Display name</span><input autoFocus value={draft.name} onChange={e => setDraft({ ...draft, name: e.target.value })} maxLength={40}/></label><div className="onboarding-section"><span>Avatar</span><AvatarPicker avatar={draft.avatar} onChange={avatar => setDraft({ ...draft, avatar })}/></div><div className="onboarding-section"><span>Theme</span><ThemePicker theme={draft.theme} palette={draft.palette} onTheme={theme => setDraft({ ...draft, theme })} onPalette={palette => setDraft({ ...draft, palette })}/></div><div className="profile-editor-actions"><button className="secondary-button" onClick={onClose}>Cancel</button><button className="primary-button" disabled={!draft.name.trim()} onClick={() => onSave({ ...draft, name: draft.name.trim() })}>Save changes</button></div></div></div> }

function CommandPalette() {
  const { query, setQuery, setPage, sounds, play } = useAppStore()
  if (!query.startsWith('>')) return null
  const term = query.slice(1).trim().toLowerCase()
  const results = sounds.filter(s => s.name.toLowerCase().includes(term)).slice(0, 5)
  return <div className="command-overlay" onClick={() => setQuery('')}><div className="command-palette" onClick={e => e.stopPropagation()}><div className="command-input"><Command/><input autoFocus value={query.slice(1)} onChange={e => setQuery('>' + e.target.value)} placeholder="Search commands…"/><kbd>ESC</kbd></div><div className="command-results"><button onClick={() => setPage('microphone')}><Volume2/> Open microphone controls</button>{results.map(sound => <button key={sound.id} onClick={() => { play(sound.id); setQuery('') }}><Volume2/> Play {sound.name}<span>↵</span></button>)}</div></div></div>
}
