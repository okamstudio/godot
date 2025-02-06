/**************************************************************************/
/*  virtual_controller_ios.mm                                             */
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

#import "virtual_controller_ios.h"

#include "core/config/project_settings.h"

IOSVirtualController::IOSVirtualController() {
	initialize();
}

IOSVirtualController::~IOSVirtualController() {
	if (@available(iOS 15.0, *)) {
		gcv_controller = nullptr;
	}
}

void IOSVirtualController::initialize() {
	if (@available(iOS 15.0, *)) {
		if (!gcv_controller) {
			read_project_settings();

			GCVirtualControllerConfiguration *config = [[GCVirtualControllerConfiguration alloc] init];

			NSMutableSet *elements = [[NSMutableSet alloc] init];

			if (is_enabled_left_thumbstick()) {
				[elements addObject:GCInputLeftThumbstick];
			}
			if (is_enabled_right_thumbstick()) {
				[elements addObject:GCInputRightThumbstick];
			}
			if (is_enabled_button_a()) {
				[elements addObject:GCInputButtonA];
			}
			if (is_enabled_button_b()) {
				[elements addObject:GCInputButtonB];
			}
			if (is_enabled_button_x()) {
				[elements addObject:GCInputButtonX];
			}
			if (is_enabled_button_y()) {
				[elements addObject:GCInputButtonY];
			}

			config.elements = elements;

			dispatch_async(dispatch_get_main_queue(), ^{
				gcv_controller = [[GCVirtualController alloc] initWithConfiguration:config];
			});
		}
	}
}

void IOSVirtualController::read_project_settings() {
	enabled = GLOBAL_GET("input_devices/virtual_controller/ios/enable_controller");
	enabled_left_thumbstick = GLOBAL_GET("input_devices/virtual_controller/ios/enable_left_thumbstick");
	enabled_right_thumbstick = GLOBAL_GET("input_devices/virtual_controller/ios/enable_right_thumbstick");
	enabled_button_a = GLOBAL_GET("input_devices/virtual_controller/ios/enable_button_a");
	enabled_button_b = GLOBAL_GET("input_devices/virtual_controller/ios/enable_button_b");
	enabled_button_x = GLOBAL_GET("input_devices/virtual_controller/ios/enable_button_x");
	enabled_button_y = GLOBAL_GET("input_devices/virtual_controller/ios/enable_button_y");
}

void IOSVirtualController::elements_changed(GCInputElementName p_name, bool p_hidden) {
	if (@available(iOS 15.0, *)) {
		dispatch_async(dispatch_get_main_queue(), ^{
			if (gcv_controller) {
				[gcv_controller updateConfigurationForElement:p_name
												configuration:^(GCVirtualControllerElementConfiguration *configuration) {
													configuration.hidden = p_hidden;
													return configuration;
												}];
			}
		});
	}
}

void IOSVirtualController::enable() {
	enabled = true;
	update_state();
}

void IOSVirtualController::disable() {
	enabled = false;
	update_state();
}

void IOSVirtualController::update_state() {
	if (is_enabled()) {
		connect_controller();
	} else {
		disconnect_controller();
	}
}

bool IOSVirtualController::is_enabled() {
	return enabled;
}

void IOSVirtualController::connect_controller() {
	if (@available(iOS 15.0, *)) {
		dispatch_async(dispatch_get_main_queue(), ^{
			if (GCController.controllers.count == 0 && gcv_controller != nil) {
				[gcv_controller connectWithReplyHandler:nil];
			}
		});
	}
}

void IOSVirtualController::disconnect_controller() {
	if (@available(iOS 15.0, *)) {
		dispatch_async(dispatch_get_main_queue(), ^{
			if (gcv_controller) {
				[gcv_controller disconnect];
			}
		});
	}
}

void IOSVirtualController::controller_connected() {
	if (@available(iOS 15.0, *)) {
		if (gcv_controller != nil) {
			BOOL has_physical_controller = NO;
			for (GCController *controller in GCController.controllers) {
				if (controller != gcv_controller.controller) {
					has_physical_controller = YES;
					break;
				}
			}
			if (has_physical_controller) {
				disconnect_controller();
			}
		}
	}
}

void IOSVirtualController::controller_disconnected() {
	if (is_enabled()) {
		connect_controller();
	}
}

void IOSVirtualController::set_enabled_left_thumbstick(bool p_enabled) {
	if (enabled_left_thumbstick != p_enabled) {
		enabled_left_thumbstick = p_enabled;
		if (@available(iOS 15.0, *)) {
			elements_changed(GCInputLeftThumbstick, !p_enabled);
		}
	}
}

bool IOSVirtualController::is_enabled_left_thumbstick() {
	return enabled_left_thumbstick;
}

void IOSVirtualController::set_enabled_right_thumbstick(bool p_enabled) {
	if (enabled_right_thumbstick != p_enabled) {
		enabled_right_thumbstick = p_enabled;
		if (@available(iOS 15.0, *)) {
			elements_changed(GCInputRightThumbstick, !p_enabled);
		}
	}
}

bool IOSVirtualController::is_enabled_right_thumbstick() {
	return enabled_right_thumbstick;
}

void IOSVirtualController::set_enabled_button_a(bool p_enabled) {
	if (enabled_button_a != p_enabled) {
		enabled_button_a = p_enabled;
		if (@available(iOS 15.0, *)) {
			elements_changed(GCInputButtonA, !p_enabled);
		}
	}
}

bool IOSVirtualController::is_enabled_button_a() {
	return enabled_button_a;
}

void IOSVirtualController::set_enabled_button_b(bool p_enabled) {
	if (enabled_button_b != p_enabled) {
		enabled_button_b = p_enabled;
		if (@available(iOS 15.0, *)) {
			elements_changed(GCInputButtonB, !p_enabled);
		}
	}
}

bool IOSVirtualController::is_enabled_button_b() {
	return enabled_button_b;
}

void IOSVirtualController::set_enabled_button_x(bool p_enabled) {
	if (enabled_button_x != p_enabled) {
		enabled_button_x = p_enabled;
		if (@available(iOS 15.0, *)) {
			elements_changed(GCInputButtonX, !p_enabled);
		}
	}
}

bool IOSVirtualController::is_enabled_button_x() {
	return enabled_button_x;
}

void IOSVirtualController::set_enabled_button_y(bool p_enabled) {
	if (enabled_button_y != p_enabled) {
		enabled_button_y = p_enabled;
		if (@available(iOS 15.0, *)) {
			elements_changed(GCInputButtonY, !p_enabled);
		}
	}
}

bool IOSVirtualController::is_enabled_button_y() {
	return enabled_button_y;
}
