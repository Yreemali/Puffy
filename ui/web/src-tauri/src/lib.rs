use serde::{Deserialize, Serialize};
use std::ffi::{c_char, c_float, c_int, c_void, CString, CStr};
use std::fs;
use std::path::PathBuf;
use std::sync::Mutex;

#[repr(C)]
enum NativeRoute { None = 0, Headphones = 1, Microphone = 2, Both = 3 }

extern "C" {
    fn puffy_native_create(database_path: *const c_char) -> *mut c_void;
    fn puffy_native_destroy(context: *mut c_void);
    fn puffy_native_start(context: *mut c_void) -> c_int;
    fn puffy_native_stop_all(context: *mut c_void) -> c_int;
    fn puffy_native_play_sound(context: *mut c_void, sound_id: i64, route: NativeRoute) -> c_int;
    fn puffy_native_set_master_volume(context: *mut c_void, value: c_float) -> c_int;
    fn puffy_native_set_soundboard_volume(context: *mut c_void, value: c_float) -> c_int;
    fn puffy_native_set_monitoring_volume(context: *mut c_void, value: c_float) -> c_int;
    fn puffy_native_set_virtual_output_volume(context: *mut c_void, value: c_float) -> c_int;
    fn puffy_native_set_monitoring_muted(context: *mut c_void, muted: c_int) -> c_int;
    fn puffy_native_set_virtual_microphone_muted(context: *mut c_void, muted: c_int) -> c_int;
    fn puffy_native_library_snapshot(context: *const c_void) -> *const c_char;
    fn puffy_native_sound_waveform(context: *mut c_void, sound_id: i64, points: usize) -> *const c_char;
    fn puffy_native_mixer_levels(context: *mut c_void) -> *const c_char;
    fn puffy_native_add_sound(context: *mut c_void, path: *const c_char) -> c_int;
    fn puffy_native_set_sound_volume(context: *mut c_void, sound_id: i64, value: c_float) -> c_int;
    fn puffy_native_set_sound_route(context: *mut c_void, sound_id: i64, route: NativeRoute) -> c_int;
    fn puffy_native_set_sound_playback_mode(context: *mut c_void, sound_id: i64, mode: c_int) -> c_int;
    fn puffy_native_set_sound_hotkey(context: *mut c_void, sound_id: i64, hotkey: *const c_char) -> c_int;
    fn puffy_native_set_full_keyboard(context: *mut c_void, enabled: c_int, mode: c_int, playlist_id: i64, single_sound_id: i64, avoid_repeats: c_int, trigger_on_repeat: c_int, ignore_ctrl: c_int, ignore_shift: c_int, ignore_alt: c_int, ignore_super: c_int) -> c_int;
    fn puffy_native_create_playlist(context: *mut c_void, name: *const c_char) -> i64;
    fn puffy_native_add_sound_to_playlist(context: *mut c_void, playlist_id: i64, sound_id: i64) -> c_int;
    fn puffy_native_set_playlist_hotkey(context: *mut c_void, playlist_id: i64, hotkey: *const c_char, mode: c_int, next_hotkey: *const c_char) -> c_int;
    fn puffy_native_set_microphone_gain(context: *mut c_void, value: c_float) -> c_int;
    fn puffy_native_set_monitor_microphone(context: *mut c_void, enabled: c_int) -> c_int;
    fn puffy_native_set_effect_parameter(context: *mut c_void, effect: *const c_char, parameter: *const c_char, value: c_float) -> c_int;
    fn puffy_native_last_error(context: *const c_void) -> *const c_char;
}

struct NativeAudio { context: *mut c_void }
unsafe impl Send for NativeAudio {}
unsafe impl Sync for NativeAudio {}
impl Drop for NativeAudio { fn drop(&mut self) { if !self.context.is_null() { unsafe { puffy_native_destroy(self.context) } } } }

fn native_error(context: *const c_void) -> String {
    unsafe { CStr::from_ptr(puffy_native_last_error(context)).to_string_lossy().into_owned() }
}

#[derive(Debug, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
struct PlaylistPayload {
    id: i64,
    name: String,
    description: String,
    color: String,
    sound_ids: Vec<i64>,
}

#[tauri::command]
fn play_sound(state: tauri::State<'_, Mutex<NativeAudio>>, sound_id: i64, route: String) -> Result<(), String> {
    if route.is_empty() { return Err("Audio route cannot be empty".into()); }
    let route = match route.as_str() { "headphones" => NativeRoute::Headphones, "microphone" => NativeRoute::Microphone, "none" => NativeRoute::None, _ => NativeRoute::Both };
    let audio = state.lock().map_err(|_| "Audio state is unavailable")?;
    let result = unsafe { puffy_native_play_sound(audio.context, sound_id, route) };
    if result == 0 { Err(native_error(audio.context)) } else { Ok(()) }
}

#[tauri::command]
fn stop_all(state: tauri::State<'_, Mutex<NativeAudio>>) -> Result<(), String> {
    let audio = state.lock().map_err(|_| "Audio state is unavailable")?;
    if unsafe { puffy_native_stop_all(audio.context) } == 0 { Err(native_error(audio.context)) } else { Ok(()) }
}

#[tauri::command]
fn set_master_volume(state: tauri::State<'_, Mutex<NativeAudio>>, value: f32) -> Result<(), String> {
    if !value.is_finite() || !(0.0..=2.0).contains(&value) { return Err("Master volume must be between 0 and 2".into()); }
    let audio = state.lock().map_err(|_| "Audio state is unavailable")?;
    if unsafe { puffy_native_set_master_volume(audio.context, value) } == 0 { Err(native_error(audio.context)) } else { Ok(()) }
}

#[tauri::command]
fn set_soundboard_volume(state: tauri::State<'_, Mutex<NativeAudio>>, value: f32) -> Result<(), String> {
    let audio = state.lock().map_err(|_| "Audio state lock poisoned".to_string())?;
    if unsafe { puffy_native_set_soundboard_volume(audio.context, value) } == 0 { Err(native_error(audio.context)) } else { Ok(()) }
}

#[tauri::command]
fn set_monitoring_volume(state: tauri::State<'_, Mutex<NativeAudio>>, value: f32) -> Result<(), String> {
    let audio = state.lock().map_err(|_| "Audio state lock poisoned".to_string())?;
    if unsafe { puffy_native_set_monitoring_volume(audio.context, value) } == 0 { Err(native_error(audio.context)) } else { Ok(()) }
}

#[tauri::command]
fn set_virtual_output_volume(state: tauri::State<'_, Mutex<NativeAudio>>, value: f32) -> Result<(), String> {
    let audio = state.lock().map_err(|_| "Audio state lock poisoned".to_string())?;
    if unsafe { puffy_native_set_virtual_output_volume(audio.context, value) } == 0 { Err(native_error(audio.context)) } else { Ok(()) }
}

#[tauri::command]
fn set_monitoring_muted(state: tauri::State<'_, Mutex<NativeAudio>>, muted: bool) -> Result<(), String> {
    let audio = state.lock().map_err(|_| "Audio state lock poisoned".to_string())?;
    if unsafe { puffy_native_set_monitoring_muted(audio.context, i32::from(muted)) } == 0 { Err(native_error(audio.context)) } else { Ok(()) }
}

#[tauri::command]
fn set_virtual_microphone_muted(state: tauri::State<'_, Mutex<NativeAudio>>, muted: bool) -> Result<(), String> {
    let audio = state.lock().map_err(|_| "Audio state lock poisoned".to_string())?;
    if unsafe { puffy_native_set_virtual_microphone_muted(audio.context, i32::from(muted)) } == 0 { Err(native_error(audio.context)) } else { Ok(()) }
}

#[tauri::command]
fn import_audio() -> Result<Vec<String>, String> {
    // The dialog plugin is initialized in the host. File selection and decode
    // will be connected to SoundLibrary/SoundCache in the C++ facade.
    Ok(Vec::new())
}

#[tauri::command]
fn save_playlist(playlist: PlaylistPayload) -> Result<(), String> {
    if playlist.name.trim().is_empty() { return Err("Playlist name cannot be empty".into()); }
    Ok(())
}

#[tauri::command]
fn library_snapshot(state: tauri::State<'_, Mutex<NativeAudio>>) -> Result<String, String> {
    let audio = state.lock().map_err(|_| "Audio state is unavailable")?;
    Ok(unsafe { CStr::from_ptr(puffy_native_library_snapshot(audio.context)).to_string_lossy().into_owned() })
}

#[tauri::command]
fn sound_waveform(state: tauri::State<'_, Mutex<NativeAudio>>, sound_id: i64, points: usize) -> Result<Vec<f32>, String> {
    let audio = state.lock().map_err(|_| "Audio state is unavailable")?;
    let raw = unsafe { CStr::from_ptr(puffy_native_sound_waveform(audio.context, sound_id, points.clamp(16, 256))) }.to_string_lossy();
    serde_json::from_str(&raw).map_err(|error| format!("Invalid waveform data: {error}"))
}

#[tauri::command]
fn mixer_levels(state: tauri::State<'_, Mutex<NativeAudio>>) -> Result<serde_json::Value, String> {
    let audio = state.lock().map_err(|_| "Audio state is unavailable")?;
    let raw = unsafe { CStr::from_ptr(puffy_native_mixer_levels(audio.context)) }.to_string_lossy();
    serde_json::from_str(&raw).map_err(|error| format!("Invalid mixer level data: {error}"))
}

#[tauri::command]
fn add_sounds(state: tauri::State<'_, Mutex<NativeAudio>>, paths: Vec<String>) -> Result<String, String> {
    let audio = state.lock().map_err(|_| "Audio state is unavailable")?;
    for path in paths {
        let path = CString::new(path).map_err(|_| "Invalid path")?;
        if unsafe { puffy_native_add_sound(audio.context, path.as_ptr()) } == 0 { return Err(native_error(audio.context)); }
    }
    Ok(unsafe { CStr::from_ptr(puffy_native_library_snapshot(audio.context)).to_string_lossy().into_owned() })
}

#[tauri::command]
fn set_sound_volume(state: tauri::State<'_, Mutex<NativeAudio>>, sound_id: i64, value: f32) -> Result<(), String> {
    let audio = state.lock().map_err(|_| "Audio state is unavailable")?;
    if unsafe { puffy_native_set_sound_volume(audio.context, sound_id, value) } == 0 { Err(native_error(audio.context)) } else { Ok(()) }
}

#[tauri::command]
fn set_sound_route(state: tauri::State<'_, Mutex<NativeAudio>>, sound_id: i64, route: String) -> Result<(), String> {
    let route = match route.as_str() { "headphones" => NativeRoute::Headphones, "microphone" => NativeRoute::Microphone, "none" => NativeRoute::None, _ => NativeRoute::Both };
    let audio = state.lock().map_err(|_| "Audio state is unavailable")?;
    if unsafe { puffy_native_set_sound_route(audio.context, sound_id, route) } == 0 { Err(native_error(audio.context)) } else { Ok(()) }
}

#[tauri::command]
fn set_sound_playback_mode(state: tauri::State<'_, Mutex<NativeAudio>>, sound_id: i64, mode: i32) -> Result<(), String> {
    let audio = state.lock().map_err(|_| "Audio state is unavailable")?;
    if unsafe { puffy_native_set_sound_playback_mode(audio.context, sound_id, mode) } == 0 { Err(native_error(audio.context)) } else { Ok(()) }
}

#[tauri::command]
fn set_sound_hotkey(state: tauri::State<'_, Mutex<NativeAudio>>, sound_id: i64, hotkey: String) -> Result<(), String> {
    let hotkey = CString::new(hotkey).map_err(|_| "Invalid hotkey")?;
    let audio = state.lock().map_err(|_| "Audio state is unavailable")?;
    if unsafe { puffy_native_set_sound_hotkey(audio.context, sound_id, hotkey.as_ptr()) } == 0 { Err(native_error(audio.context)) } else { Ok(()) }
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
struct FullKeyboardPayload { enabled: bool, mode: i32, playlist_id: i64, single_sound_id: i64, avoid_repeats: bool, trigger_on_repeat: bool, ignore_ctrl: bool, ignore_shift: bool, ignore_alt: bool, ignore_super: bool }

#[tauri::command]
fn set_full_keyboard(state: tauri::State<'_, Mutex<NativeAudio>>, config: FullKeyboardPayload) -> Result<(), String> {
    let audio = state.lock().map_err(|_| "Audio state is unavailable")?;
    let ok = unsafe { puffy_native_set_full_keyboard(audio.context, i32::from(config.enabled), config.mode, config.playlist_id, config.single_sound_id, i32::from(config.avoid_repeats), i32::from(config.trigger_on_repeat), i32::from(config.ignore_ctrl), i32::from(config.ignore_shift), i32::from(config.ignore_alt), i32::from(config.ignore_super)) };
    if ok == 0 { Err(native_error(audio.context)) } else { Ok(()) }
}

#[tauri::command]
fn create_playlist(state: tauri::State<'_, Mutex<NativeAudio>>, name: String) -> Result<String, String> {
    let name = CString::new(name).map_err(|_| "Invalid playlist name")?;
    let audio = state.lock().map_err(|_| "Audio state is unavailable")?;
    if unsafe { puffy_native_create_playlist(audio.context, name.as_ptr()) } == 0 { Err(native_error(audio.context)) } else { Ok(unsafe { CStr::from_ptr(puffy_native_library_snapshot(audio.context)).to_string_lossy().into_owned() }) }
}

#[tauri::command]
fn add_sound_to_playlist(state: tauri::State<'_, Mutex<NativeAudio>>, playlist_id: i64, sound_id: i64) -> Result<String, String> {
    let audio = state.lock().map_err(|_| "Audio state is unavailable")?;
    if unsafe { puffy_native_add_sound_to_playlist(audio.context, playlist_id, sound_id) } == 0 { Err(native_error(audio.context)) } else { Ok(unsafe { CStr::from_ptr(puffy_native_library_snapshot(audio.context)).to_string_lossy().into_owned() }) }
}

#[tauri::command]
fn set_playlist_hotkey(state: tauri::State<'_, Mutex<NativeAudio>>, playlist_id: i64, hotkey: String, mode: i32, next_hotkey: String) -> Result<(), String> {
    if !(0..=1).contains(&mode) { return Err("Playlist mode must be sequential or random".into()); }
    let hotkey = CString::new(hotkey).map_err(|_| "Invalid playlist hotkey")?;
    let next_hotkey = CString::new(next_hotkey).map_err(|_| "Invalid next-track hotkey")?;
    let audio = state.lock().map_err(|_| "Audio state is unavailable")?;
    if unsafe { puffy_native_set_playlist_hotkey(audio.context, playlist_id, hotkey.as_ptr(), mode, next_hotkey.as_ptr()) } == 0 { Err(native_error(audio.context)) } else { Ok(()) }
}

#[tauri::command]
fn set_microphone_gain(state: tauri::State<'_, Mutex<NativeAudio>>, value: f32) -> Result<(), String> {
    let audio = state.lock().map_err(|_| "Audio state is unavailable")?;
    if unsafe { puffy_native_set_microphone_gain(audio.context, value) } == 0 { Err(native_error(audio.context)) } else { Ok(()) }
}

#[tauri::command]
fn set_monitor_microphone(state: tauri::State<'_, Mutex<NativeAudio>>, enabled: bool) -> Result<(), String> {
    let audio = state.lock().map_err(|_| "Audio state is unavailable")?;
    if unsafe { puffy_native_set_monitor_microphone(audio.context, i32::from(enabled)) } == 0 { Err(native_error(audio.context)) } else { Ok(()) }
}

#[tauri::command]
fn set_effect_parameter(state: tauri::State<'_, Mutex<NativeAudio>>, effect: String, parameter: String, value: f32) -> Result<(), String> {
    if !value.is_finite() { return Err("Effect parameter must be finite".into()); }
    let effect = CString::new(effect).map_err(|_| "Invalid effect")?;
    let parameter = CString::new(parameter).map_err(|_| "Invalid parameter")?;
    let audio = state.lock().map_err(|_| "Audio state is unavailable")?;
    if unsafe { puffy_native_set_effect_parameter(audio.context, effect.as_ptr(), parameter.as_ptr(), value) } == 0 { Err(native_error(audio.context)) } else { Ok(()) }
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .manage(Mutex::new(NativeAudio { context: native_context() }))
        .plugin(tauri_plugin_dialog::init())
        .invoke_handler(tauri::generate_handler![play_sound, stop_all, set_master_volume, set_soundboard_volume, set_monitoring_volume, set_virtual_output_volume, set_monitoring_muted, set_virtual_microphone_muted, import_audio, save_playlist, library_snapshot, sound_waveform, mixer_levels, add_sounds, set_sound_volume, set_sound_route, set_sound_playback_mode, set_sound_hotkey, set_full_keyboard, create_playlist, add_sound_to_playlist, set_playlist_hotkey, set_microphone_gain, set_monitor_microphone, set_effect_parameter])
        .run(tauri::generate_context!())
        .expect("error while running puffy");
}

fn native_context() -> *mut c_void {
    let path = database_path();
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).expect("unable to create Puffy application data directory");
    }
    let legacy = PathBuf::from("puffy.sqlite");
    if !path.exists() && legacy.exists() && legacy != path {
        let _ = fs::copy(&legacy, &path);
    }
    let path = path.to_string_lossy().into_owned();
    let path = CString::new(path).expect("database path cannot contain NUL");
    let context = unsafe { puffy_native_create(path.as_ptr()) };
    if context.is_null() { panic!("unable to create puffy native context") }
    if unsafe { puffy_native_start(context) } == 0 {
        eprintln!("puffy native audio: {}", native_error(context));
    }
    context
}

fn database_path() -> PathBuf {
    if let Some(path) = std::env::var_os("PUFFY_DATABASE") {
        return PathBuf::from(path);
    }
    #[cfg(target_os = "windows")]
    let base = std::env::var_os("LOCALAPPDATA").map(PathBuf::from);
    #[cfg(target_os = "macos")]
    let base = std::env::var_os("HOME")
        .map(PathBuf::from)
        .map(|home| home.join("Library").join("Application Support"));
    #[cfg(all(unix, not(target_os = "macos")))]
    let base = std::env::var_os("XDG_DATA_HOME").map(PathBuf::from).or_else(|| {
        std::env::var_os("HOME").map(PathBuf::from).map(|home| home.join(".local").join("share"))
    });
    base.unwrap_or_else(std::env::temp_dir).join("Puffy").join("puffy.sqlite")
}
