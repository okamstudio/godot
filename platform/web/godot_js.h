/**************************************************************************/
/*  godot_js.h                                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#define WASM_EXPORT __attribute__((visibility("default")))

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

// Config
extern void godot_js_config_locale_get(char *p_ptr, int p_ptr_max);
extern void godot_js_config_canvas_id_get(char *p_ptr, int p_ptr_max);

// OS
extern void godot_js_os_finish_async(void (*p_callback)());
extern void godot_js_os_request_quit_cb(void (*p_callback)());
extern int godot_js_os_fs_is_persistent();
extern void godot_js_os_fs_sync(void (*p_callback)());
extern int godot_js_os_execute(const char *p_json);
extern void godot_js_os_shell_open(const char *p_uri);
extern int godot_js_os_hw_concurrency_get();
extern int godot_js_os_has_feature(const char *p_ftr);
extern int godot_js_pwa_cb(void (*p_callback)());
extern int godot_js_pwa_update();

// Input
extern void godot_js_input_mouse_button_cb(int (*p_callback)(int p_pressed, int p_button, double p_x, double p_y, int p_modifiers));
extern void godot_js_input_mouse_move_cb(void (*p_callback)(double p_x, double p_y, double p_rel_x, double p_rel_y, int p_modifiers));
extern void godot_js_input_mouse_wheel_cb(int (*p_callback)(double p_delta_x, double p_delta_y));
extern void godot_js_input_touch_cb(void (*p_callback)(int p_type, int p_count), uint32_t *r_identifiers, double *r_coords);
extern void godot_js_input_key_cb(void (*p_callback)(int p_type, int p_repeat, int p_modifiers), char r_code[32], char r_key[32]);
extern void godot_js_input_vibrate_handheld(int p_duration_ms);

extern void godot_js_set_ime_active(int p_active);
extern void godot_js_set_ime_position(int p_x, int p_y);
extern void godot_js_set_ime_cb(void (*p_input)(int p_type, const char *p_text), void (*p_callback)(int p_type, int p_repeat, int p_modifiers), char r_code[32], char r_key[32]);
extern int godot_js_is_ime_focused();

// Input gamepad
extern void godot_js_input_gamepad_cb(void (*p_on_change)(int p_index, int p_connected, const char *p_id, const char *p_guid));
extern int godot_js_input_gamepad_sample();
extern int godot_js_input_gamepad_sample_count();
extern int godot_js_input_gamepad_sample_get(int p_idx, float r_btns[16], int32_t *r_btns_num, float r_axes[10], int32_t *r_axes_num, int32_t *r_standard);
extern void godot_js_input_paste_cb(void (*p_callback)(const char *p_text));
extern void godot_js_input_drop_files_cb(void (*p_callback)(const char **p_filev, int p_filec));

// TTS
extern int godot_js_tts_is_speaking();
extern int godot_js_tts_is_paused();
extern int godot_js_tts_get_voices(void (*p_callback)(int p_size, const char **p_voices));
extern void godot_js_tts_speak(const char *p_text, const char *p_voice, int p_volume, float p_pitch, float p_rate, int p_utterance_id, void (*p_callback)(int p_event, int p_id, int p_pos));
extern void godot_js_tts_pause();
extern void godot_js_tts_resume();
extern void godot_js_tts_stop();

// Display
extern int godot_js_display_screen_dpi_get();
extern double godot_js_display_pixel_ratio_get();
extern void godot_js_display_alert(const char *p_text);
extern int godot_js_display_touchscreen_is_available();
extern int godot_js_display_is_swap_ok_cancel();
extern void godot_js_display_setup_canvas(int p_width, int p_height, int p_fullscreen, int p_hidpi);

// Display canvas
extern void godot_js_display_canvas_focus();
extern int godot_js_display_canvas_is_focused();

// Display window
extern void godot_js_display_desired_size_set(int p_width, int p_height);
extern int godot_js_display_size_update();
extern void godot_js_display_window_size_get(int32_t *p_x, int32_t *p_y);
extern void godot_js_display_screen_size_get(int32_t *p_x, int32_t *p_y);
extern int godot_js_display_fullscreen_request();
extern int godot_js_display_fullscreen_exit();
extern void godot_js_display_window_title_set(const char *p_text);
extern void godot_js_display_window_icon_set(const uint8_t *p_ptr, int p_len);
extern int godot_js_display_has_webgl(int p_version);

// Display clipboard
extern int godot_js_display_clipboard_set(const char *p_text);
extern int godot_js_display_clipboard_get(void (*p_callback)(const char *p_text));

// Display cursor
extern void godot_js_display_cursor_set_shape(const char *p_cursor);
extern int godot_js_display_cursor_is_hidden();
extern void godot_js_display_cursor_set_custom_shape(const char *p_shape, const uint8_t *p_ptr, int p_len, int p_hotspot_x, int p_hotspot_y);
extern void godot_js_display_cursor_set_visible(int p_visible);
extern void godot_js_display_cursor_lock_set(int p_lock);
extern int godot_js_display_cursor_is_locked();

// Display listeners
extern void godot_js_display_fullscreen_cb(void (*p_callback)(int p_fullscreen));
extern void godot_js_display_window_blur_cb(void (*p_callback)());
extern void godot_js_display_notification_cb(void (*p_callback)(int p_notification), int p_enter, int p_exit, int p_in, int p_out);

// Display Virtual Keyboard
extern int godot_js_display_vk_available();
extern int godot_js_display_tts_available();
extern void godot_js_display_vk_cb(void (*p_input)(const char *p_text, int p_cursor));
extern void godot_js_display_vk_show(const char *p_text, int p_type, int p_start, int p_end);
extern void godot_js_display_vk_hide();

#ifdef __cplusplus
}
#endif
