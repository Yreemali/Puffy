#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct puffy_native_context puffy_native_context;

typedef enum puffy_native_route {
    PUFFY_ROUTE_NONE = 0,
    PUFFY_ROUTE_HEADPHONES = 1,
    PUFFY_ROUTE_MICROPHONE = 2,
    PUFFY_ROUTE_BOTH = 3,
} puffy_native_route;

puffy_native_context* puffy_native_create(const char* database_path);
void puffy_native_destroy(puffy_native_context* context);

int puffy_native_start(puffy_native_context* context);
void puffy_native_stop(puffy_native_context* context);
int puffy_native_play_sound(puffy_native_context* context, int64_t sound_id,
                            puffy_native_route route);
int puffy_native_stop_all(puffy_native_context* context);
int puffy_native_set_master_volume(puffy_native_context* context, float value);
int puffy_native_set_soundboard_volume(puffy_native_context* context, float value);
int puffy_native_set_monitoring_volume(puffy_native_context* context, float value);
int puffy_native_set_virtual_output_volume(puffy_native_context* context, float value);
int puffy_native_set_monitoring_muted(puffy_native_context* context, int muted);
int puffy_native_set_virtual_microphone_muted(puffy_native_context* context, int muted);
const char* puffy_native_last_error(const puffy_native_context* context);
const char* puffy_native_library_snapshot(const puffy_native_context* context);
int puffy_native_add_sound(puffy_native_context* context, const char* path);
int puffy_native_set_sound_volume(puffy_native_context* context, int64_t sound_id, float value);
int puffy_native_set_sound_route(puffy_native_context* context, int64_t sound_id, puffy_native_route route);
int puffy_native_set_sound_playback_mode(puffy_native_context* context, int64_t sound_id, int mode);
int puffy_native_set_sound_hotkey(puffy_native_context* context, int64_t sound_id, const char* hotkey);
int puffy_native_set_full_keyboard(puffy_native_context* context, int enabled, int mode,
                                   int64_t playlist_id, int64_t single_sound_id,
                                   int avoid_repeats, int trigger_on_repeat,
                                   int ignore_ctrl, int ignore_shift, int ignore_alt,
                                   int ignore_super);
int64_t puffy_native_create_playlist(puffy_native_context* context, const char* name);
int puffy_native_add_sound_to_playlist(puffy_native_context* context, int64_t playlist_id, int64_t sound_id);
int puffy_native_set_playlist_hotkey(puffy_native_context* context, int64_t playlist_id, const char* hotkey, int mode, const char* next_hotkey);
int puffy_native_set_microphone_gain(puffy_native_context* context, float value);
int puffy_native_set_monitor_microphone(puffy_native_context* context, int enabled);
int puffy_native_set_effect_parameter(puffy_native_context* context, const char* effect, const char* parameter, float value);

#ifdef __cplusplus
}
#endif
