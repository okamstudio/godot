/**************************************************************************/
/*  joypad_sdl.cpp                                                        */
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

#include "joypad_sdl.h"
#ifdef SDL_ENABLED
#include "core/error/error_macros.h"

#ifdef SOWRAP_ENABLED
#include "SDL2-so_wrap.h"
#endif

#include "core/input/default_controller_mappings.h"
#include "thirdparty/sdl_headers/SDL.h"

void JoypadSDL::process_inputs_thread_func(void *p_userdata) {
	JoypadSDL *joy = static_cast<JoypadSDL *>(p_userdata);
	joy->process_inputs_run();
}

#define HANDLE_SDL_ERROR(call) \
	error = call;              \
	ERR_FAIL_COND_V_MSG(error != 0, FAILED, SDL_GetError())

void JoypadSDL::process_inputs_run() {
	while (!process_inputs_exit.is_set()) {
		for (int i = 0; i < Input::JOYPADS_MAX; i++) {
			float ff_weak;
			float ff_strong;
			SDL_Joystick *joy = nullptr;
			uint32_t ff_duration_ms;

			joypads_lock[i].lock();
			if (joypads[i].attached && joypads[i].supports_force_feedback && joypads[i].needs_ff_update) {
				joy = SDL_JoystickFromInstanceID(joypads[i].sdl_instance_idx);
				ff_weak = joypads[i].ff_weak;
				ff_strong = joypads[i].ff_strong;
				ff_duration_ms = joypads[i].ff_duration_ms;
				joypads[i].needs_ff_update = false;
			}
			joypads_lock[i].unlock();

			// It may be that we've closed the joystick but the main thread isn't aware of this fact yet
			// because the event queue hasn't been processed
			if (joy == nullptr) {
				continue;
			}
			uint16_t weak = ff_weak * UINT16_MAX;
			uint16_t strong = ff_strong * UINT16_MAX;
			SDL_JoystickRumble(joy, strong, weak, ff_duration_ms);
		}

		SDL_Event e;
		int has_event = SDL_WaitEventTimeout(&e, 16);
		if (has_event != 0) {
			switch (e.type) {
				case SDL_JOYDEVICEADDED: {
					JoypadEvent joypad_event = {};
					joypad_event.type = JoypadEventType::DEVICE_ADDED;
					SDL_Joystick *joy = nullptr;
					SDL_GameController *game_controller = nullptr;

					// Game controllers must be opened with GameControllerOpen to get their special remapped events
					if (SDL_IsGameController(e.jdevice.which) == SDL_TRUE) {
						joypad_event.device_type = JoypadType::GAME_CONTROLLER;
						game_controller = SDL_GameControllerOpen(e.jdevice.which);

						ERR_CONTINUE_MSG(!game_controller, vformat("Error opening game controller at index %d", e.jdevice.which));

						joypad_event.device_name = SDL_GameControllerName(game_controller);
						joy = SDL_GameControllerGetJoystick(game_controller);
						if (is_print_verbose_enabled()) {
							print_line(vformat("SDL: Game controller %s connected", SDL_GameControllerName(game_controller)));
						}
					} else {
						joypad_event.device_type = JoypadType::JOYSTICK;
						joy = SDL_JoystickOpen(e.jdevice.which);
						ERR_CONTINUE_MSG(!joy, vformat("Error opening joy device %d: %s", SDL_GetError()));
						if (is_print_verbose_enabled()) {
							print_line(vformat("SDL: Joystick %s connected", SDL_JoystickName(joy)));
						}
					}

					joypad_event.sdl_joystick_instance_id = SDL_JoystickInstanceID(joy);
					joypad_event.device_name = String(SDL_JoystickName(joy));

					const int MAX_GUID_SIZE = 64;
					char guid[MAX_GUID_SIZE] = {};

					SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joy), guid, MAX_GUID_SIZE);
					joypad_event.device_guid = String(guid);
					joypad_event.device_supports_force_feedback = SDL_JoystickHasRumble(joy);

					MutexLock lock(joypad_event_queue_lock);
					joypad_event_queue.push_back(joypad_event);
				} break;
				case SDL_JOYDEVICEREMOVED: {
					JoypadEvent joypad_event = {};
					joypad_event.type = JoypadEventType::DEVICE_REMOVED;
					joypad_event.sdl_joystick_instance_id = e.jdevice.which;

					SDL_GameController *game_controller = SDL_GameControllerFromInstanceID(e.jdevice.which);
					if (game_controller != nullptr) {
						SDL_GameControllerClose(game_controller);
					} else {
						SDL_JoystickClose(SDL_JoystickFromInstanceID(e.jdevice.which));
					}

					MutexLock lock(joypad_event_queue_lock);
					joypad_event_queue.push_back(joypad_event);
				} break;
				case SDL_JOYAXISMOTION: {
					if (SDL_GameControllerFromInstanceID(e.jbutton.which) != nullptr) {
						continue;
					}
					JoypadEvent joypad_event = {};
					joypad_event.type = JoypadEventType::AXIS;
					// Godot joy axis constants are already intentionally the same as SDL's
					joypad_event.axis = static_cast<JoyAxis>(e.jaxis.axis);

					joypad_event.sdl_joystick_instance_id = e.jaxis.which;

					joypad_event.value = (e.jaxis.value - SDL_JOYSTICK_AXIS_MIN) / (float)(SDL_JOYSTICK_AXIS_MAX - SDL_JOYSTICK_AXIS_MIN);
					joypad_event.value -= 0.5f;
					joypad_event.value *= 2.0f;

					MutexLock lock(joypad_event_queue_lock);
					joypad_event_queue.push_back(joypad_event);
				} break;
				case SDL_JOYBUTTONUP:
				case SDL_JOYBUTTONDOWN: {
					if (SDL_GameControllerFromInstanceID(e.jbutton.which) != nullptr) {
						continue;
					}
					JoypadEvent joypad_event = {};
					joypad_event.type = JoypadEventType::BUTTON;
					joypad_event.sdl_joystick_instance_id = e.jbutton.which;
					joypad_event.pressed = e.jbutton.state == SDL_PRESSED;

					// Godot button constants are intentionally the same as SDL's, so we can just straight up use them
					joypad_event.button = static_cast<JoyButton>(e.jbutton.button);

					MutexLock lock(joypad_event_queue_lock);
					joypad_event_queue.push_back(joypad_event);
				} break;
				case SDL_JOYHATMOTION: {
					if (SDL_GameControllerFromInstanceID(e.jbutton.which) != nullptr) {
						continue;
					}
					// Godot hat masks are identical to SDL hat masks, so we can just use them as-is.
					JoypadEvent joypad_event = {};
					joypad_event.type = JoypadEventType::HAT;
					joypad_event.hat_mask = e.jhat.value;
					joypad_event.sdl_joystick_instance_id = e.jhat.which;

					MutexLock lock(joypad_event_queue_lock);
					joypad_event_queue.push_back(joypad_event);
				} break;
				case SDL_CONTROLLERAXISMOTION: {
					JoypadEvent joypad_event = {};
					joypad_event.type = JoypadEventType::AXIS;
					// Godot joy axis constants are already intentionally the same as SDL's
					joypad_event.axis = static_cast<JoyAxis>(e.caxis.axis);

					joypad_event.sdl_joystick_instance_id = e.caxis.which;

					if (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
						// Game controller triggers go from 0 to SDL_JOYSTICK_AXIS_MAX
						joypad_event.value = e.caxis.value / (float)SDL_JOYSTICK_AXIS_MAX;
					} else {
						// Other axis go from SDL_JOYSTICK_AXIS_MIN to SDL_JOYSTICK_AXIS_MAX
						joypad_event.value = (e.caxis.value - SDL_JOYSTICK_AXIS_MIN) / (float)(SDL_JOYSTICK_AXIS_MAX - SDL_JOYSTICK_AXIS_MIN);
						joypad_event.value -= 0.5f;
						joypad_event.value *= 2.0f;
					}

					MutexLock lock(joypad_event_queue_lock);
					joypad_event_queue.push_back(joypad_event);
				} break;
				// Do note SDL game controllers do not have separate events for the dpad
				case SDL_CONTROLLERBUTTONUP:
				case SDL_CONTROLLERBUTTONDOWN: {
					JoypadEvent joypad_event = {};
					joypad_event.type = JoypadEventType::BUTTON;
					joypad_event.sdl_joystick_instance_id = e.cbutton.which;
					joypad_event.pressed = e.cbutton.state == SDL_PRESSED;

					// Godot button constants are intentionally the same as SDL's, so we can just straight up use them
					joypad_event.button = static_cast<JoyButton>(e.cbutton.button);

					MutexLock lock(joypad_event_queue_lock);
					joypad_event_queue.push_back(joypad_event);
				} break;
			}
		}
	}
}

void JoypadSDL::joypad_vibration_start(int p_pad_idx, float p_weak, float p_strong, float p_duration, uint64_t p_timestamp) {
	Joypad &pad = joypads[p_pad_idx];

	uint32_t duration_msec = p_duration * 1000;

	MutexLock lock(joypads_lock[p_pad_idx]);
	pad.needs_ff_update = true;
	pad.ff_duration_ms = duration_msec;
	pad.ff_weak = p_weak;
	pad.ff_strong = p_strong;
	pad.ff_effect_timestamp = p_timestamp;
}

void JoypadSDL::joypad_vibration_stop(int p_pad_idx, uint64_t p_timestamp) {
	Joypad &pad = joypads[p_pad_idx];

	MutexLock lock(joypads_lock[p_pad_idx]);
	pad.needs_ff_update = true;
	pad.ff_duration_ms = 0;
	pad.ff_weak = 0;
	pad.ff_strong = 0;
	pad.ff_effect_timestamp = p_timestamp;
}

JoypadSDL::JoypadSDL(Input *in) {
	input = in;
}

JoypadSDL::~JoypadSDL() {
	if (process_inputs_thread.is_started()) {
		process_inputs_exit.set();
		process_inputs_thread.wait_to_finish();
		// Process any remaining input events
		process_events();
		for (int i = 0; i < Input::JOYPADS_MAX; i++) {
			if (joypads[i].attached) {
				if (joypads[i].type == JoypadType::GAME_CONTROLLER) {
					SDL_GameController *controller = SDL_GameControllerFromInstanceID(joypads[i].sdl_instance_idx);
					SDL_GameControllerClose(controller);
				} else {
					SDL_Joystick *joy = SDL_JoystickFromInstanceID(joypads[i].sdl_instance_idx);
					SDL_JoystickClose(joy);
				}
			}
		}
		SDL_Quit();
	}
}

Error JoypadSDL::initialize() {
#ifdef SOWRAP_ENABLED
#ifdef DEBUG_ENABLED
	int dylibloader_verbose = 1;
#else
	int dylibloader_verbose = 0;
#endif
	if (initialize_SDL2(dylibloader_verbose)) {
		print_verbose("SDL: Failed to open, probably not present in the system.");
		return ERR_CANT_OPEN;
	}
#endif

	SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");

	int error;
	SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "1");
	HANDLE_SDL_ERROR(SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER));

	// Add godot's mapping database from memory
	int i = 0;
	while (DefaultControllerMappings::mappings[i]) {
		String mapping_string = DefaultControllerMappings::mappings[i++];
		CharString data = mapping_string.utf8();
		SDL_RWops *rw = SDL_RWFromMem((void *)data.ptr(), data.size());
		SDL_GameControllerAddMappingsFromRW(rw, 1);
	}

	print_verbose("SDL: Init OK!");

	process_inputs_thread.start(&JoypadSDL::process_inputs_thread_func, this);
	return OK;
}

void JoypadSDL::process_events() {
	Vector<JoypadEvent> events;
	joypad_event_queue_lock.lock();
	events = joypad_event_queue;
	joypad_event_queue.clear();
	joypad_event_queue_lock.unlock();

	for (int i = 0; i < events.size(); i++) {
		JoypadEvent event = events[i];

		switch (event.type) {
			case DEVICE_ADDED: {
				int joy_id = Input::get_singleton()->get_unused_joy_id();
				if (joy_id == -1) {
					// There ain't no space for more joypads...
					print_error("Joypad limit reached!");
				}
				joypads[joy_id].attached = true;
				joypads[joy_id].sdl_instance_idx = event.sdl_joystick_instance_id;
				joypads[joy_id].supports_force_feedback = event.device_supports_force_feedback;
				joypads[joy_id].type = event.device_type;

				sdl_instance_id_to_joypad_id.insert(event.sdl_joystick_instance_id, joy_id);
				// Don't give joysticks of type GAME_CONTROLLER a GUID to prevent godot from messing us up with its own remapping logic
				if (event.device_type == JoypadType::GAME_CONTROLLER) {
					input->joy_connection_changed(joy_id, true, event.device_name, "");
				} else {
					input->joy_connection_changed(joy_id, true, event.device_name, event.device_guid);
				}
			} break;
			case DEVICE_REMOVED: {
				if (sdl_instance_id_to_joypad_id.has(event.sdl_joystick_instance_id)) {
					int joy_id = sdl_instance_id_to_joypad_id.get(event.sdl_joystick_instance_id);
					MutexLock lock(joypads_lock[joy_id]);
					joypads[joy_id].attached = false;
					sdl_instance_id_to_joypad_id.erase(event.sdl_joystick_instance_id);
					input->joy_connection_changed(joy_id, false, "");

					joypads[joy_id].needs_ff_update = false;
				}
			} break;
			case AXIS: {
				if (sdl_instance_id_to_joypad_id.has(event.sdl_joystick_instance_id)) {
					int joy_id = sdl_instance_id_to_joypad_id.get(event.sdl_joystick_instance_id);
					input->joy_axis(joy_id, event.axis, event.value);
				}
			} break;

			case BUTTON: {
				if (sdl_instance_id_to_joypad_id.has(event.sdl_joystick_instance_id)) {
					int joy_id = sdl_instance_id_to_joypad_id.get(event.sdl_joystick_instance_id);

					input->joy_button(joy_id, event.button, event.pressed);
				}
			} break;
			case HAT: {
				if (sdl_instance_id_to_joypad_id.has(event.sdl_joystick_instance_id)) {
					int joy_id = sdl_instance_id_to_joypad_id.get(event.sdl_joystick_instance_id);

					input->joy_hat(joy_id, event.hat_mask);
				}
			} break;
		}
	}
	for (int i = 0; i < Input::JOYPADS_MAX; i++) {
		Joypad &joy = joypads[i];
		if (joy.attached && joy.supports_force_feedback) {
			uint64_t timestamp = input->get_joy_vibration_timestamp(i);
			if (timestamp > joy.ff_effect_timestamp) {
				Vector2 strength = input->get_joy_vibration_strength(i);
				float duration = input->get_joy_vibration_duration(i);
				if (strength.x == 0 && strength.y == 0) {
					joypad_vibration_stop(i, timestamp);
				} else {
					joypad_vibration_start(i, strength.x, strength.y, duration, timestamp);
				}
			}
		}
	}
}
#endif
