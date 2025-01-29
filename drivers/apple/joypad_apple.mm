/**************************************************************************/
/*  joypad_apple.mm                                                       */
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

#import "joypad_apple.h"

#include <CoreHaptics/CoreHaptics.h>
#import <os/log.h>

#include "core/config/project_settings.h"
#include "main/main.h"

class API_AVAILABLE(macos(11), ios(14.0), tvos(14.0)) RumbleMotor {
	CHHapticEngine *engine;
	id<CHHapticPatternPlayer> player;
	bool is_started;

	RumbleMotor(GCController *p_controller, GCHapticsLocality p_locality) {
		engine = [p_controller.haptics createEngineWithLocality:p_locality];
		engine.autoShutdownEnabled = YES;
	}

public:
	static RumbleMotor *create(GCController *p_controller, GCHapticsLocality p_locality) {
		if ([p_controller.haptics.supportedLocalities containsObject:p_locality]) {
			return memnew(RumbleMotor(p_controller, p_locality));
		}
		return nullptr;
	}

	_ALWAYS_INLINE_ bool has_active_player() {
		return player != nil;
	}

	void execute_pattern(CHHapticPattern *p_pattern) {
		NSError *error;
		if (!is_started) {
			ERR_FAIL_COND_MSG(![engine startAndReturnError:&error], "Couldn't start controller haptic engine: " + String::utf8(error.localizedDescription.UTF8String));
			is_started = YES;
		}

		player = [engine createPlayerWithPattern:p_pattern error:&error];
		ERR_FAIL_COND_MSG(error, "Couldn't create controller haptic pattern player: " + String::utf8(error.localizedDescription.UTF8String));
		ERR_FAIL_COND_MSG(![player startAtTime:CHHapticTimeImmediate error:&error], "Couldn't execute controller haptic pattern: " + String::utf8(error.localizedDescription.UTF8String));
	}

	void stop() {
		id<CHHapticPatternPlayer> old_player = player;
		player = nil;

		NSError *error;
		ERR_FAIL_COND_MSG(![old_player stopAtTime:CHHapticTimeImmediate error:&error], "Couldn't stop controller haptic pattern: " + String::utf8(error.localizedDescription.UTF8String));
	}
};

class API_AVAILABLE(macos(11), ios(14.0), tvos(14.0)) RumbleContext {
	RumbleMotor *weak_motor;
	RumbleMotor *strong_motor;

public:
	RumbleContext(GCController *p_controller) {
		weak_motor = RumbleMotor::create(p_controller, GCHapticsLocalityRightHandle);
		strong_motor = RumbleMotor::create(p_controller, GCHapticsLocalityLeftHandle);
	}

	~RumbleContext() {
		if (weak_motor) {
			memdelete(weak_motor);
		}
		if (strong_motor) {
			memdelete(strong_motor);
		}
	}

	_ALWAYS_INLINE_ bool has_motors() {
		return weak_motor != nullptr && strong_motor != nullptr;
	}

	_ALWAYS_INLINE_ bool has_active_players() {
		if (!has_motors()) {
			return false;
		}
		return (weak_motor && weak_motor->has_active_player()) || (strong_motor && strong_motor->has_active_player());
	}

	void stop() {
		if (weak_motor) {
			weak_motor->stop();
		}
		if (strong_motor) {
			strong_motor->stop();
		}
	}

	void play_weak_pattern(CHHapticPattern *p_pattern) {
		if (weak_motor) {
			weak_motor->execute_pattern(p_pattern);
		}
	}

	void play_strong_pattern(CHHapticPattern *p_pattern) {
		if (strong_motor) {
			strong_motor->execute_pattern(p_pattern);
		}
	}
};

GameController::GameController(int p_joy_id, GCController *p_controller) :
		joy_id(p_joy_id), controller(p_controller) {
	force_feedback = NO;
	if (@available(macOS 11.0, iOS 14.0, tvOS 14.0, *)) {
		if (controller.haptics != nil) {
			// Create a rumble context for the controller.
			rumble_context = memnew(RumbleContext(p_controller));

			// If the rumble motors aren't available, disable force feedback.
			force_feedback = rumble_context->has_motors();
		}
	}

	int l_joy_id = joy_id;

	auto BUTTON = [l_joy_id](JoyButton p_button) {
		return ^(GCControllerButtonInput *button, float value, BOOL pressed) {
			Input::get_singleton()->joy_button(l_joy_id, p_button, pressed);
		};
	};

	if (controller.extendedGamepad != nil) {
		GCExtendedGamepad *gamepad = controller.extendedGamepad;

		gamepad.buttonA.pressedChangedHandler = BUTTON(JoyButton::A);
		gamepad.buttonB.pressedChangedHandler = BUTTON(JoyButton::B);
		gamepad.buttonX.pressedChangedHandler = BUTTON(JoyButton::X);
		gamepad.buttonY.pressedChangedHandler = BUTTON(JoyButton::Y);
		gamepad.leftShoulder.pressedChangedHandler = BUTTON(JoyButton::LEFT_SHOULDER);
		gamepad.rightShoulder.pressedChangedHandler = BUTTON(JoyButton::RIGHT_SHOULDER);
		gamepad.dpad.up.pressedChangedHandler = BUTTON(JoyButton::DPAD_UP);
		gamepad.dpad.down.pressedChangedHandler = BUTTON(JoyButton::DPAD_DOWN);
		gamepad.dpad.left.pressedChangedHandler = BUTTON(JoyButton::DPAD_LEFT);
		gamepad.dpad.right.pressedChangedHandler = BUTTON(JoyButton::DPAD_RIGHT);

		gamepad.leftThumbstick.valueChangedHandler = ^(GCControllerDirectionPad *dpad, float xValue, float yValue) {
			Input::get_singleton()->joy_axis(l_joy_id, JoyAxis::LEFT_X, xValue);
			Input::get_singleton()->joy_axis(l_joy_id, JoyAxis::LEFT_Y, -yValue);
		};

		gamepad.rightThumbstick.valueChangedHandler = ^(GCControllerDirectionPad *dpad, float xValue, float yValue) {
			Input::get_singleton()->joy_axis(l_joy_id, JoyAxis::RIGHT_X, xValue);
			Input::get_singleton()->joy_axis(l_joy_id, JoyAxis::RIGHT_Y, -yValue);
		};
		gamepad.leftTrigger.valueChangedHandler = ^(GCControllerButtonInput *button, float value, BOOL pressed) {
			Input::get_singleton()->joy_axis(l_joy_id, JoyAxis::TRIGGER_LEFT, value);
		};
		gamepad.rightTrigger.valueChangedHandler = ^(GCControllerButtonInput *button, float value, BOOL pressed) {
			Input::get_singleton()->joy_axis(l_joy_id, JoyAxis::TRIGGER_RIGHT, value);
		};

		if (@available(macOS 10.14.1, iOS 12.1, tvOS 12.1, *)) {
			gamepad.leftThumbstickButton.pressedChangedHandler = BUTTON(JoyButton::LEFT_STICK);
			gamepad.rightThumbstickButton.pressedChangedHandler = BUTTON(JoyButton::RIGHT_STICK);
		}

		if (@available(macOS 10.15, iOS 13.0, tvOS 13.0, *)) {
			gamepad.buttonOptions.pressedChangedHandler = BUTTON(JoyButton::BACK);
			gamepad.buttonMenu.pressedChangedHandler = BUTTON(JoyButton::START);
		}

		if (@available(macOS 11, iOS 14.0, tvOS 14.0, *)) {
			gamepad.buttonHome.pressedChangedHandler = BUTTON(JoyButton::GUIDE);
			if ([gamepad isKindOfClass:[GCXboxGamepad class]]) {
				GCXboxGamepad *xboxGamepad = (GCXboxGamepad *)gamepad;
				xboxGamepad.paddleButton1.pressedChangedHandler = BUTTON(JoyButton::PADDLE1);
				xboxGamepad.paddleButton2.pressedChangedHandler = BUTTON(JoyButton::PADDLE2);
				xboxGamepad.paddleButton3.pressedChangedHandler = BUTTON(JoyButton::PADDLE3);
				xboxGamepad.paddleButton4.pressedChangedHandler = BUTTON(JoyButton::PADDLE4);
			}
		}

		if (@available(macOS 12, iOS 15.0, tvOS 15.0, *)) {
			if ([gamepad isKindOfClass:[GCXboxGamepad class]]) {
				GCXboxGamepad *xboxGamepad = (GCXboxGamepad *)gamepad;
				xboxGamepad.buttonShare.pressedChangedHandler = BUTTON(JoyButton::MISC1);
			}
		}
	} else if (controller.microGamepad != nil) {
		GCMicroGamepad *gamepad = controller.microGamepad;

		gamepad.buttonA.pressedChangedHandler = BUTTON(JoyButton::A);
		gamepad.buttonX.pressedChangedHandler = BUTTON(JoyButton::X);
		gamepad.dpad.up.pressedChangedHandler = BUTTON(JoyButton::DPAD_UP);
		gamepad.dpad.down.pressedChangedHandler = BUTTON(JoyButton::DPAD_DOWN);
		gamepad.dpad.left.pressedChangedHandler = BUTTON(JoyButton::DPAD_LEFT);
		gamepad.dpad.right.pressedChangedHandler = BUTTON(JoyButton::DPAD_RIGHT);
	}

	// TODO: Need to add support for controller.motion which gives us access to
	// the orientation of the device (if supported).
}

GameController::~GameController() {
	if (@available(macOS 11.0, iOS 14.0, tvOS 14.0, *)) {
		if (rumble_context) {
			memdelete(rumble_context);
		}
	}
}

JoypadApple::JoypadApple() {
	connect_observer = [NSNotificationCenter.defaultCenter
			addObserverForName:GCControllerDidConnectNotification
						object:nil
						 queue:NSOperationQueue.mainQueue
					usingBlock:^(NSNotification *notification) {
						GCController *controller = notification.object;
						if (!controller) {
							return;
						}
						add_joypad(controller);
					}];

	disconnect_observer = [NSNotificationCenter.defaultCenter
			addObserverForName:GCControllerDidDisconnectNotification
						object:nil
						 queue:NSOperationQueue.mainQueue
					usingBlock:^(NSNotification *notification) {
						GCController *controller = notification.object;
						if (!controller) {
							return;
						}
						remove_joypad(controller);
					}];

	if (@available(macOS 11.3, iOS 14.5, tvOS 14.5, *)) {
		GCController.shouldMonitorBackgroundEvents = YES;
	}
}

JoypadApple::~JoypadApple() {
	for (KeyValue<int, GameController *> &E : joypads) {
		memdelete(E.value);
		E.value = nullptr;
	}

	[NSNotificationCenter.defaultCenter removeObserver:connect_observer];
	[NSNotificationCenter.defaultCenter removeObserver:disconnect_observer];
}

// Finds the rightmost set bit in a number, n.
// variation of https://www.geeksforgeeks.org/position-of-rightmost-set-bit/
int rightmost_one(int n) {
	return __builtin_ctz(n & -n) + 1;
}

GCControllerPlayerIndex JoypadApple::get_free_player_index() {
	// player_set will be a bitfield where each bit represents a player index.
	__block uint32_t player_set = 0;
	for (const KeyValue<GCController *, int> &E : controller_to_joy_id) {
		player_set |= 1U << E.key.playerIndex;
	}

	// invert, as we want to find the first unset player index.
	int n = rightmost_one((int)(~player_set));
	if (n >= 5) {
		return GCControllerPlayerIndexUnset;
	}

	return (GCControllerPlayerIndex)(n - 1);
}

void JoypadApple::add_joypad(GCController *p_controller) {
	if (controller_to_joy_id.has(p_controller)) {
		return;
	}

	// Get a new id for our controller.
	int joy_id = Input::get_singleton()->get_unused_joy_id();

	if (joy_id == -1) {
		print_verbose("Couldn't retrieve new joy ID.");
		return;
	}

	// Assign our player index.
	if (p_controller.playerIndex == GCControllerPlayerIndexUnset) {
		p_controller.playerIndex = get_free_player_index();
	}

	// Tell Godot about our new controller.
	Input::get_singleton()->joy_connection_changed(joy_id, true, String::utf8(p_controller.vendorName.UTF8String));

	// Assign our player index.
	joypads.insert(joy_id, memnew(GameController(joy_id, p_controller)));
	controller_to_joy_id.insert(p_controller, joy_id);
}

void JoypadApple::remove_joypad(GCController *p_controller) {
	if (!controller_to_joy_id.has(p_controller)) {
		return;
	}

	int joy_id = controller_to_joy_id[p_controller];
	controller_to_joy_id.erase(p_controller);

	// Tell Godot this joystick is no longer there.
	Input::get_singleton()->joy_connection_changed(joy_id, false, "");

	// And remove it from our dictionary.
	GameController **old = joypads.getptr(joy_id);
	memdelete(*old);
	*old = nullptr;
	joypads.erase(joy_id);
}

API_AVAILABLE(macos(10.15), ios(13.0), tvos(14.0))
CHHapticPattern *get_vibration_pattern(float p_magnitude, float p_duration) {
	// Creates a vibration pattern with an intensity and duration.
	NSDictionary *hapticDict = @{
		CHHapticPatternKeyPattern : @[
			@{
				CHHapticPatternKeyEvent : @{
					CHHapticPatternKeyEventType : CHHapticEventTypeHapticContinuous,
					CHHapticPatternKeyTime : @(CHHapticTimeImmediate),
					CHHapticPatternKeyEventDuration : [NSNumber numberWithFloat:p_duration],

					CHHapticPatternKeyEventParameters : @[
						@{
							CHHapticPatternKeyParameterID : CHHapticEventParameterIDHapticIntensity,
							CHHapticPatternKeyParameterValue : [NSNumber numberWithFloat:p_magnitude]
						},
					],
				},
			},
		],
	};
	NSError *error;
	CHHapticPattern *pattern = [[CHHapticPattern alloc] initWithDictionary:hapticDict error:&error];
	return pattern;
}

void JoypadApple::joypad_vibration_start(GameController &p_joypad, float p_weak_magnitude, float p_strong_magnitude, float p_duration, uint64_t p_timestamp) {
	if (!p_joypad.force_feedback || p_weak_magnitude < 0.f || p_weak_magnitude > 1.f || p_strong_magnitude < 0.f || p_strong_magnitude > 1.f) {
		return;
	}

	// If there is active vibration players, stop them.
	if (p_joypad.rumble_context->has_active_players()) {
		joypad_vibration_stop(p_joypad, p_timestamp);
	}

	// Gets the default vibration pattern and creates a player for each motor.
	CHHapticPattern *weak_pattern = get_vibration_pattern(p_weak_magnitude, p_duration);
	CHHapticPattern *strong_pattern = get_vibration_pattern(p_strong_magnitude, p_duration);

	p_joypad.rumble_context->play_weak_pattern(weak_pattern);
	p_joypad.rumble_context->play_strong_pattern(strong_pattern);

	p_joypad.ff_effect_timestamp = p_timestamp;
}

void JoypadApple::joypad_vibration_stop(GameController &p_joypad, uint64_t p_timestamp) {
	if (!p_joypad.force_feedback) {
		return;
	}
	// If there is no active vibration players, exit.
	if (!p_joypad.rumble_context->has_active_players()) {
		return;
	}

	p_joypad.rumble_context->stop();

	p_joypad.ff_effect_timestamp = p_timestamp;
}

void JoypadApple::process_joypads() {
	if (@available(macOS 11.0, iOS 14.0, tvOS 14.0, *)) {
		for (KeyValue<int, GameController *> &E : joypads) {
			int id = E.key;
			GameController &joypad = *E.value;

			if (joypad.force_feedback) {
				Input *input = Input::get_singleton();
				uint64_t timestamp = input->get_joy_vibration_timestamp(id);

				if (timestamp > (unsigned)joypad.ff_effect_timestamp) {
					Vector2 strength = input->get_joy_vibration_strength(id);
					float duration = input->get_joy_vibration_duration(id);
					if (duration == 0) {
						duration = GCHapticDurationInfinite;
					}

					if (strength.x == 0 && strength.y == 0) {
						joypad_vibration_stop(joypad, timestamp);
					} else {
						joypad_vibration_start(joypad, strength.x, strength.y, duration, timestamp);
					}
				}
			}
		}
	}
}
