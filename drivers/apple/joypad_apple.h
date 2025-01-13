/**************************************************************************/
/*  joypad_apple.h                                                        */
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

#include "core/input/input.h"

#define Key _QKey
#import <GameController/GameController.h>
#undef Key

@class GCController;
class RumbleContext;

struct GameController {
	int joy_id;
	GCController *controller;
	RumbleContext *rumble_context API_AVAILABLE(macos(11.0), ios(14.0), tvos(14.0)) = nil;
	NSInteger ff_effect_timestamp = 0;
	bool force_feedback = false;

	GameController(int p_joy_id, GCController *p_controller);
	~GameController();
};

class JoypadApple {
private:
	id<NSObject> connect_observer = nil;
	id<NSObject> disconnect_observer = nil;
	HashMap<int, GameController *> joypads;
	HashMap<GCController *, int> controller_to_joy_id;

	GCControllerPlayerIndex get_free_player_index();

	void add_joypad(GCController *p_controller);
	void remove_joypad(GCController *p_controller);

public:
	JoypadApple();
	~JoypadApple();

	void joypad_vibration_start(GameController &p_joypad, float p_weak_magnitude, float p_strong_magnitude, float p_duration, uint64_t p_timestamp) API_AVAILABLE(macos(11.0), ios(14.0), tvos(14.0));
	void joypad_vibration_stop(GameController &p_joypad, uint64_t p_timestamp) API_AVAILABLE(macos(11.0), ios(14.0), tvos(14.0));

	void process_joypads();
};
