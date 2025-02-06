/**************************************************************************/
/*  joypad_windows.h                                                      */
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

#include "os_windows.h"

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <xinput.h>

#include <mmsystem.h>

#ifndef SAFE_RELEASE // when Windows Media Device M? is not present
#define SAFE_RELEASE(x) \
	if (x != nullptr) { \
		x->Release();   \
		x = nullptr;    \
	}
#endif

#ifndef XUSER_MAX_COUNT
#define XUSER_MAX_COUNT 4
#endif

class JoypadWindows {
public:
	JoypadWindows();
	JoypadWindows(HWND *hwnd);
	~JoypadWindows();

	void probe_joypads();
	void process_joypads();

private:
	enum {
		JOYPADS_MAX = 16,
		JOY_AXIS_COUNT = 6,
		MIN_JOY_AXIS = 10,
		MAX_JOY_AXIS = 32768,
		MAX_JOY_BUTTONS = 128,
		KEY_EVENT_BUFFER_SIZE = 512,
		MAX_TRIGGER = 255
	};

	struct dinput_gamepad {
		int id;
		bool attached;
		bool confirmed;
		bool last_buttons[MAX_JOY_BUTTONS];
		DWORD last_pad;

		LPDIRECTINPUTDEVICE8 di_joy;
		LocalVector<LONG> joy_axis;
		GUID guid;

		dinput_gamepad() {
			id = -1;
			last_pad = -1;
			attached = false;
			confirmed = false;
			di_joy = nullptr;
			guid = {};

			for (int i = 0; i < MAX_JOY_BUTTONS; i++) {
				last_buttons[i] = false;
			}
		}
	};

	struct xinput_gamepad {
		int id = 0;
		bool attached = false;
		bool vibrating = false;
		DWORD last_packet = 0;
		XINPUT_STATE state;
		uint64_t ff_timestamp = 0;
		uint64_t ff_end_timestamp = 0;
	};

	typedef DWORD(WINAPI *XInputGetState_t)(DWORD dwUserIndex, XINPUT_STATE *pState);
	typedef DWORD(WINAPI *XInputSetState_t)(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration);

	typedef MMRESULT(WINAPI *joyGetDevCaps_t)(UINT uJoyID, LPJOYCAPSW pjc, UINT cbjc);

	HWND *hWnd = nullptr;
	HANDLE xinput_dll;
	HANDLE winmm_dll;
	LPDIRECTINPUT8 dinput;
	Input *input = nullptr;

	int id_to_change;
	int slider_count;
	int x_joypad_probe_count; // XInput equivalent to dinput_gamepad.confirmed.
	int d_joypad_count;
	bool attached_joypads[JOYPADS_MAX];
	dinput_gamepad d_joypads[JOYPADS_MAX];
	xinput_gamepad x_joypads[XUSER_MAX_COUNT];

	static BOOL CALLBACK enumCallback(const DIDEVICEINSTANCE *p_instance, void *p_context);
	static BOOL CALLBACK objectsCallback(const DIDEVICEOBJECTINSTANCE *instance, void *context);

	void setup_d_joypad_object(const DIDEVICEOBJECTINSTANCE *ob, int p_joy_id);
	void close_d_joypad(int id = -1);
	void load_xinput();
	void unload_xinput();
	void unload_winmm();

	void post_hat(int p_device, DWORD p_dpad);

	bool is_d_joypad_known(const GUID &p_guid);
	bool is_xinput_joypad(const GUID *p_guid);
	bool setup_dinput_joypad(const DIDEVICEINSTANCE *instance);
	void probe_xinput_joypad(const String &name = ""); // Handles connect, disconnect & re-connect for XInput joypads.
	void joypad_vibration_start_xinput(int p_device, float p_weak_magnitude, float p_strong_magnitude, float p_duration, uint64_t p_timestamp);
	void joypad_vibration_stop_xinput(int p_device, uint64_t p_timestamp);

	float axis_correct(int p_val, bool p_xinput = false, bool p_trigger = false, bool p_negate = false) const;
	XInputGetState_t xinput_get_state;
	XInputSetState_t xinput_set_state;
	joyGetDevCaps_t winmm_get_joycaps; // Only for reading info on XInput joypads.
};
