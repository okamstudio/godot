/**************************************************************************/
/*  godot_window_delegate.mm                                              */
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

#include "godot_window_delegate.h"

#include "display_server_macos.h"
#include "godot_button_view.h"
#include "godot_window.h"

@implementation GodotWindowDelegate

- (void)setWindowID:(DisplayServer::WindowID)wid {
	window_id = wid;
}

- (BOOL)windowShouldClose:(id)sender {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return YES;
	}

	ds->send_window_event(ds->get_window(window_id), DisplayServerMacOS::WINDOW_EVENT_CLOSE_REQUEST);
	return NO;
}

- (void)windowWillClose:(NSNotification *)notification {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	ds->popup_close(window_id);

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
	while (wd.transient_children.size()) {
		ds->window_set_transient(*wd.transient_children.begin(), DisplayServerMacOS::INVALID_WINDOW_ID);
	}

	if (wd.transient_parent != DisplayServerMacOS::INVALID_WINDOW_ID) {
		ds->window_set_transient(window_id, DisplayServerMacOS::INVALID_WINDOW_ID);
	}

	ds->mouse_exit_window(window_id);
	ds->window_destroy(window_id);
}

- (void)windowWillEnterFullScreen:(NSNotification *)notification {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
	wd.fs_transition = true;

	// Temporary disable borderless and transparent state.
	if ([wd.window_object styleMask] == NSWindowStyleMaskBorderless) {
		[wd.window_object setStyleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable];
	}
	if (wd.layered_window) {
		ds->set_window_per_pixel_transparency_enabled(false, window_id);
	}
}

- (void)windowDidFailToEnterFullScreen:(NSWindow *)window {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
	wd.fs_transition = false;
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
	wd.fullscreen = true;
	wd.fs_transition = false;

	// Reset window size limits.
	[wd.window_object setContentMinSize:NSMakeSize(0, 0)];
	[wd.window_object setContentMaxSize:NSMakeSize(FLT_MAX, FLT_MAX)];

	// Reset custom window buttons.
	if ([wd.window_object styleMask] & NSWindowStyleMaskFullSizeContentView) {
		ds->window_set_custom_window_buttons(wd, false);
	}

	ds->send_window_event(wd, DisplayServerMacOS::WINDOW_EVENT_TITLEBAR_CHANGE);

	// Force window resize event and redraw.
	[self windowDidResize:notification];
}

- (void)windowWillExitFullScreen:(NSNotification *)notification {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
	wd.fs_transition = true;

	// Restore custom window buttons.
	if ([wd.window_object styleMask] & NSWindowStyleMaskFullSizeContentView) {
		ds->window_set_custom_window_buttons(wd, true);
	}

	ds->send_window_event(wd, DisplayServerMacOS::WINDOW_EVENT_TITLEBAR_CHANGE);
}

- (void)windowDidFailToExitFullScreen:(NSWindow *)window {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
	wd.fs_transition = false;

	if ([wd.window_object styleMask] & NSWindowStyleMaskFullSizeContentView) {
		ds->window_set_custom_window_buttons(wd, false);
	}

	ds->send_window_event(wd, DisplayServerMacOS::WINDOW_EVENT_TITLEBAR_CHANGE);
}

- (void)windowDidExitFullScreen:(NSNotification *)notification {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
	if (wd.exclusive_fullscreen) {
		ds->update_presentation_mode();
	}

	wd.fullscreen = false;
	wd.exclusive_fullscreen = false;
	wd.fs_transition = false;

	// Set window size limits.
	const float scale = ds->screen_get_max_scale();
	if (wd.min_size != Size2i()) {
		Size2i size = wd.min_size / scale;
		[wd.window_object setContentMinSize:NSMakeSize(size.x, size.y)];
	}
	if (wd.max_size != Size2i()) {
		Size2i size = wd.max_size / scale;
		[wd.window_object setContentMaxSize:NSMakeSize(size.x, size.y)];
	}

	// Restore borderless, transparent and resizability state.
	if (wd.borderless || wd.layered_window) {
		[wd.window_object setStyleMask:NSWindowStyleMaskBorderless];
	} else {
		[wd.window_object setStyleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | (wd.extend_to_title ? NSWindowStyleMaskFullSizeContentView : 0) | (wd.resize_disabled ? 0 : NSWindowStyleMaskResizable)];
	}
	if (wd.layered_window) {
		ds->set_window_per_pixel_transparency_enabled(true, window_id);
	}

	// Restore on-top state.
	if (ds->is_always_on_top_recursive(window_id)) {
		[wd.window_object setLevel:NSFloatingWindowLevel];
	}

	// Force window resize event and redraw.
	[self windowDidResize:notification];
}

- (void)windowDidChangeBackingProperties:(NSNotification *)notification {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);

	CGFloat new_scale_factor = [wd.window_object backingScaleFactor];
	CGFloat old_scale_factor = [[[notification userInfo] objectForKey:@"NSBackingPropertyOldScaleFactorKey"] doubleValue];

	if (new_scale_factor != old_scale_factor) {
		// Set new display scale and window size.
		const float scale = ds->screen_get_max_scale();
		const NSRect content_rect = [wd.window_view frame];

		wd.size.width = content_rect.size.width * scale;
		wd.size.height = content_rect.size.height * scale;

		ds->send_window_event(wd, DisplayServerMacOS::WINDOW_EVENT_DPI_CHANGE);

		CALayer *layer = [wd.window_view layer];
		if (layer) {
			layer.contentsScale = scale;
		}

		//Force window resize event
		[self windowDidResize:notification];
	}
}

- (void)windowWillStartLiveResize:(NSNotification *)notification {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (ds && ds->has_window(window_id)) {
		DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
		wd.last_frame_rect = [wd.window_object frame];
		ds->set_is_resizing(true);
	}
}

- (void)windowDidEndLiveResize:(NSNotification *)notification {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (ds) {
		ds->set_is_resizing(false);
	}
}

- (void)windowDidResize:(NSNotification *)notification {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
	const NSRect content_rect = [wd.window_view frame];
	const float scale = ds->screen_get_max_scale();
	wd.size.width = content_rect.size.width * scale;
	wd.size.height = content_rect.size.height * scale;

	CALayer *layer = [wd.window_view layer];
	if (layer) {
		layer.contentsScale = scale;
	}

	ds->window_resize(window_id, wd.size.width, wd.size.height);

	if (wd.rect_changed_callback.is_valid()) {
		wd.rect_changed_callback.call(Rect2i(ds->window_get_position(window_id), ds->window_get_size(window_id)));
	}
}

- (void)windowDidChangeScreen:(NSNotification *)notification {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	ds->reparent_check(window_id);
}

- (void)windowDidMove:(NSNotification *)notification {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
	ds->release_pressed_events();

	if (wd.rect_changed_callback.is_valid()) {
		wd.rect_changed_callback.call(Rect2i(ds->window_get_position(window_id), ds->window_get_size(window_id)));
	}
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);

	if (wd.window_button_view) {
		[(GodotButtonView *)wd.window_button_view displayButtons];
	}

	if (ds->mouse_get_mode() == DisplayServer::MOUSE_MODE_CAPTURED) {
		const NSRect content_rect = [wd.window_view frame];
		NSRect point_in_window_rect = NSMakeRect(content_rect.size.width / 2, content_rect.size.height / 2, 0, 0);
		NSPoint point_on_screen = [[wd.window_view window] convertRectToScreen:point_in_window_rect].origin;
		CGPoint mouse_warp_pos = { point_on_screen.x, CGDisplayBounds(CGMainDisplayID()).size.height - point_on_screen.y };
		CGWarpMouseCursorPosition(mouse_warp_pos);
	} else {
		ds->update_mouse_pos(wd, [wd.window_object mouseLocationOutsideOfEventStream]);
	}

	[self windowDidResize:notification]; // Emit resize event, to ensure content is resized if the window was resized while it was hidden.

	wd.focused = true;
	ds->set_last_focused_window(window_id);
	ds->send_window_event(wd, DisplayServerMacOS::WINDOW_EVENT_FOCUS_IN);
}

- (void)windowDidResignKey:(NSNotification *)notification {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);

	if (wd.window_button_view) {
		[(GodotButtonView *)wd.window_button_view displayButtons];
	}

	wd.focused = false;
	ds->release_pressed_events();
	ds->send_window_event(wd, DisplayServerMacOS::WINDOW_EVENT_FOCUS_OUT);
}

- (void)windowDidMiniaturize:(NSNotification *)notification {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);

	wd.focused = false;
	ds->release_pressed_events();
	ds->send_window_event(wd, DisplayServerMacOS::WINDOW_EVENT_FOCUS_OUT);
}

- (void)windowDidDeminiaturize:(NSNotification *)notification {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
	wd.is_visible = ([wd.window_object occlusionState] & NSWindowOcclusionStateVisible) && [wd.window_object isVisible];
	if ([wd.window_object isKeyWindow]) {
		wd.focused = true;
		ds->set_last_focused_window(window_id);
		ds->send_window_event(wd, DisplayServerMacOS::WINDOW_EVENT_FOCUS_IN);
	}
}

- (void)windowDidChangeOcclusionState:(NSNotification *)notification {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}
	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
	wd.is_visible = ([wd.window_object occlusionState] & NSWindowOcclusionStateVisible) && [wd.window_object isVisible];
}

@end
