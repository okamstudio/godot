/**************************************************************************/
/*  godot_window.mm                                                       */
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

#import "godot_window.h"

#import "display_server_macos.h"

@implementation GodotWindow

- (id)init {
	self = [super init];
	window_id = DisplayServer::INVALID_WINDOW_ID;
	anim_duration = -1.0f;
	return self;
}

- (void)setAnimDuration:(NSTimeInterval)duration {
	anim_duration = duration;
}

- (NSTimeInterval)animationResizeTime:(NSRect)newFrame {
	if (anim_duration > 0) {
		return anim_duration;
	} else {
		return [super animationResizeTime:newFrame];
	}
}

- (void)setWindowID:(DisplayServerMacOS::WindowID)wid {
	window_id = wid;
}

- (BOOL)canBecomeKeyWindow {
	// Required for NSWindowStyleMaskBorderless windows.
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return YES;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
	return !wd.no_focus;
}

- (BOOL)canBecomeMainWindow {
	// Required for NSWindowStyleMaskBorderless windows.
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return YES;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
	return !wd.no_focus && !wd.is_popup;
}

@end
