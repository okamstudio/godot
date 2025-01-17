/**************************************************************************/
/*  joypad_sdl.h                                                          */
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

#ifndef JOYPAD_SDL_H
#define JOYPAD_SDL_H

#ifdef SDL_ENABLED

#include "core/input/input.h"
#include "core/os/thread.h"

typedef int32_t SDL_JoystickID;

class JoypadSDL {
	// SDL differentiates between game controllers and generic joysticks
	// game controllers refer to playstation/xbox style controllers
	enum JoypadType {
		GAME_CONTROLLER,
		JOYSTICK
	};

	struct Joypad {
		bool attached = false;
		JoypadType type;

		SDL_JoystickID sdl_instance_idx;

		bool supports_force_feedback = false;
		uint64_t ff_effect_timestamp;
		bool needs_ff_update = false;
		float ff_weak = 0.0f;
		float ff_strong = 0.0f;
		int ff_duration_ms = 0;
	};

	Joypad joypads[Input::JOYPADS_MAX];
	HashMap<SDL_JoystickID, int> sdl_instance_id_to_joypad_id;
	Mutex joypads_lock[Input::JOYPADS_MAX];

	Input *input;

	enum JoypadEventType {
		DEVICE_ADDED,
		DEVICE_REMOVED,
		AXIS,
		BUTTON,
		HAT
	};

	struct JoypadEvent {
		String device_name;
		String device_guid;
		JoypadEventType type;
		SDL_JoystickID sdl_joystick_instance_id;
		union {
			JoyAxis axis;
			JoyButton button;
			JoypadType device_type;
		};
		BitField<HatMask> hat_mask;
		union {
			float value = 0.0f;
			bool pressed;
			bool device_supports_force_feedback;
		};
	};

	Vector<JoypadEvent> joypad_event_queue;
	Mutex joypad_event_queue_lock;

	SafeFlag process_inputs_exit;
	Thread process_inputs_thread;
	static void process_inputs_thread_func(void *p_userdata);
	void process_inputs_run();
	void joypad_vibration_start(int p_pad_idx, float p_weak, float p_strong, float p_duration, uint64_t timestamp);
	void joypad_vibration_stop(int p_pad_idx, uint64_t timestamp);

public:
	JoypadSDL(Input *in);
	~JoypadSDL();
	Error initialize();
	void process_events();
};

#endif // SDL_ENABLED

#endif // JOYPAD_SDL_H
