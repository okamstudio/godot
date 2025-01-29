/**************************************************************************/
/*  godot_content_view.mm                                                 */
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

#include "godot_content_view.h"

#include "display_server_macos.h"
#include "key_mapping_macos.h"

#include "main/main.h"

@implementation GodotContentLayerDelegate

- (id)init {
	self = [super init];
	window_id = DisplayServer::INVALID_WINDOW_ID;
	need_redraw = false;
	return self;
}

- (void)setWindowID:(DisplayServerMacOS::WindowID)wid {
	window_id = wid;
}

- (void)setNeedRedraw:(bool)redraw {
	need_redraw = redraw;
}

- (void)displayLayer:(CALayer *)layer {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (OS::get_singleton()->get_main_loop() && ds->get_is_resizing() && need_redraw) {
		Main::force_redraw();
		if (!Main::is_iterating()) { // Avoid cyclic loop.
			Main::iteration();
		}
		need_redraw = false;
	}
}

@end

@implementation GodotContentView

- (void)setFrameSize:(NSSize)newSize {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (ds && ds->has_window(window_id)) {
		DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
		NSRect frameRect = [wd.window_object frame];
		if (wd.fs_transition || wd.initial_size) {
			self.layerContentsPlacement = NSViewLayerContentsPlacementScaleAxesIndependently;
			wd.initial_size = false;
		} else {
			bool left = (wd.last_frame_rect.origin.x != frameRect.origin.x);
			bool bottom = (wd.last_frame_rect.origin.y != frameRect.origin.y);
			bool right = (wd.last_frame_rect.origin.x + wd.last_frame_rect.size.width != frameRect.origin.x + frameRect.size.width);
			bool top = (wd.last_frame_rect.origin.y + wd.last_frame_rect.size.height != frameRect.origin.y + frameRect.size.height);

			if (left && top) {
				self.layerContentsPlacement = NSViewLayerContentsPlacementBottomRight;
			} else if (left && bottom) {
				self.layerContentsPlacement = NSViewLayerContentsPlacementTopRight;
			} else if (left) {
				self.layerContentsPlacement = NSViewLayerContentsPlacementRight;
			} else if (right && top) {
				self.layerContentsPlacement = NSViewLayerContentsPlacementBottomLeft;
			} else if (right && bottom) {
				self.layerContentsPlacement = NSViewLayerContentsPlacementTopLeft;
			} else if (right) {
				self.layerContentsPlacement = NSViewLayerContentsPlacementLeft;
			}
		}
		wd.last_frame_rect = frameRect;
	}

	[super setFrameSize:newSize];
	[layer_delegate setNeedRedraw:true];
	[self.layer setNeedsDisplay]; // Force "drawRect" call.
}

- (void)updateLayerDelegate {
	self.layer.delegate = layer_delegate;
	self.layer.autoresizingMask = kCALayerHeightSizable | kCALayerWidthSizable;
	self.layer.needsDisplayOnBoundsChange = YES;
}

- (id)init {
	self = [super init];
	layer_delegate = [[GodotContentLayerDelegate alloc] init];
	window_id = DisplayServer::INVALID_WINDOW_ID;
	tracking_area = nil;
	ime_input_event_in_progress = false;
	mouse_down_control = false;
	ignore_momentum_scroll = false;
	last_pen_inverted = false;
	[self updateTrackingAreas];

	self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawDuringViewResize;
	self.layerContentsPlacement = NSViewLayerContentsPlacementTopLeft;

	[self registerForDraggedTypes:[NSArray arrayWithObject:NSPasteboardTypeFileURL]];
	marked_text = [[NSMutableAttributedString alloc] init];
	return self;
}

- (void)setWindowID:(DisplayServerMacOS::WindowID)wid {
	window_id = wid;
	[layer_delegate setWindowID:window_id];
}

// MARK: Backing Layer

- (CALayer *)makeBackingLayer {
	return [[CAMetalLayer class] layer];
}

- (BOOL)wantsUpdateLayer {
	return YES;
}

- (BOOL)isOpaque {
	return YES;
}

// MARK: IME

- (BOOL)hasMarkedText {
	return (marked_text.length > 0);
}

- (NSRange)markedRange {
	return NSMakeRange(0, marked_text.length);
}

- (NSRange)selectedRange {
	static const NSRange kEmptyRange = { NSNotFound, 0 };
	return kEmptyRange;
}

- (void)setMarkedText:(id)aString selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange {
	if ([aString isKindOfClass:[NSAttributedString class]]) {
		marked_text = [[NSMutableAttributedString alloc] initWithAttributedString:aString];
	} else {
		marked_text = [[NSMutableAttributedString alloc] initWithString:aString];
	}
	if (marked_text.length == 0) {
		[self unmarkText];
		return;
	}

	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
	if (wd.im_active) {
		ime_input_event_in_progress = true;
		ds->pop_last_key_event();
		ds->update_im_text(Point2i(selectedRange.location, selectedRange.length), String::utf8([[marked_text mutableString] UTF8String]));
	}
}

- (void)doCommandBySelector:(SEL)aSelector {
	[self tryToPerform:aSelector with:self];
}

- (void)unmarkText {
	if (ime_input_event_in_progress) {
		ime_suppress_next_keyup = true;
	}
	ime_input_event_in_progress = false;
	[[marked_text mutableString] setString:@""];

	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
	if (wd.im_active) {
		ds->update_im_text(Point2i(), String());
	}
}

- (NSArray *)validAttributesForMarkedText {
	return [NSArray array];
}

- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)aRange actualRange:(NSRangePointer)actualRange {
	return nil;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)aPoint {
	return 0;
}

- (NSRect)firstRectForCharacterRange:(NSRange)aRange actualRange:(NSRangePointer)actualRange {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return NSMakeRect(0, 0, 0, 0);
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
	const NSRect content_rect = [wd.window_view frame];
	const float scale = ds->screen_get_max_scale();
	NSRect point_in_window_rect = NSMakeRect(wd.im_position.x / scale, content_rect.size.height - (wd.im_position.y / scale) - 1, 0, 0);
	NSPoint point_on_screen = [wd.window_object convertRectToScreen:point_in_window_rect].origin;

	return NSMakeRect(point_on_screen.x, point_on_screen.y, 0, 0);
}

- (void)cancelComposition {
	[self unmarkText];
	[[NSTextInputContext currentInputContext] discardMarkedText];
}

- (void)insertText:(id)aString {
	[self insertText:aString replacementRange:NSMakeRange(0, 0)];
}

- (void)insertText:(id)aString replacementRange:(NSRange)replacementRange {
	NSString *characters;
	if ([aString isKindOfClass:[NSAttributedString class]]) {
		characters = [aString string];
	} else {
		characters = (NSString *)aString;
	}

	NSCharacterSet *ctrl_chars = [NSCharacterSet controlCharacterSet];
	NSCharacterSet *wsnl_chars = [NSCharacterSet whitespaceAndNewlineCharacterSet];
	if ([characters rangeOfCharacterFromSet:ctrl_chars].length && [characters rangeOfCharacterFromSet:wsnl_chars].length == 0) {
		[[NSTextInputContext currentInputContext] discardMarkedText];
		[self cancelComposition];
		return;
	}

	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		[self cancelComposition];
		return;
	}

	Char16String text;
	text.resize([characters length] + 1);
	[characters getCharacters:(unichar *)text.ptrw() range:NSMakeRange(0, [characters length])];

	String u32text;
	u32text.parse_utf16(text.ptr(), text.length());

	for (int i = 0; i < u32text.length(); i++) {
		const char32_t codepoint = u32text[i];
		if ((codepoint & 0xFF00) == 0xF700) {
			continue;
		}

		DisplayServerMacOS::KeyEvent ke;

		ke.window_id = window_id;
		ke.macos_state = 0;
		ke.pressed = true;
		ke.echo = false;
		ke.raw = false; // IME input event.
		ke.keycode = Key::NONE;
		ke.physical_keycode = Key::NONE;
		ke.key_label = Key::NONE;
		ke.unicode = fix_unicode(codepoint);

		ds->push_to_key_event_buffer(ke);
	}
	[self cancelComposition];
}

// MARK: Drag and drop

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
	return NSDragOperationCopy;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
	return NSDragOperationCopy;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return NO;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
	if (wd.drop_files_callback.is_valid()) {
		Vector<String> files;
		NSPasteboard *pboard = [sender draggingPasteboard];

		NSArray *items = pboard.pasteboardItems;
		for (NSPasteboardItem *item in items) {
			NSString *url = [item stringForType:NSPasteboardTypeFileURL];
			NSString *file = [NSURL URLWithString:url].path;
			files.push_back(String::utf8([file UTF8String]));
		}
		Variant v_files = files;
		const Variant *v_args[1] = { &v_files };
		Variant ret;
		Callable::CallError ce;
		wd.drop_files_callback.callp((const Variant **)&v_args, 1, ret, ce);
		if (ce.error != Callable::CallError::CALL_OK) {
			ERR_FAIL_V_MSG(NO, vformat("Failed to execute drop files callback: %s.", Variant::get_callable_error_text(wd.drop_files_callback, v_args, 1, ce)));
		}
		return YES;
	}

	return NO;
}

// MARK: Focus

- (BOOL)canBecomeKeyView {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return YES;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
	return !wd.no_focus;
}

- (BOOL)acceptsFirstResponder {
	return YES;
}

// MARK: Mouse

- (void)cursorUpdate:(NSEvent *)event {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds) {
		return;
	}

	ds->cursor_update_shape();
}

- (void)processMouseEvent:(NSEvent *)event index:(MouseButton)index pressed:(bool)pressed outofstream:(bool)outofstream {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);

	Ref<InputEventMouseButton> mb;
	mb.instantiate();
	mb->set_window_id(window_id);
	if (outofstream) {
		ds->update_mouse_pos(wd, [wd.window_object mouseLocationOutsideOfEventStream]);
	} else {
		ds->update_mouse_pos(wd, [event locationInWindow]);
	}
	ds->get_key_modifier_state([event modifierFlags], mb);
	mb->set_button_index(index);
	mb->set_pressed(pressed);
	mb->set_position(wd.mouse_pos);
	mb->set_global_position(wd.mouse_pos);
	mb->set_button_mask(ds->mouse_get_button_state());
	if (!outofstream && index == MouseButton::LEFT && pressed) {
		mb->set_double_click([event clickCount] == 2);
	}

	Input::get_singleton()->parse_input_event(mb);
}

- (void)mouseDown:(NSEvent *)event {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (ds && ds->has_window(window_id)) {
		DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
		wd.edge = DisplayServer::WINDOW_EDGE_MAX;
	}
	if (([event modifierFlags] & NSEventModifierFlagControl)) {
		mouse_down_control = true;
		[self processMouseEvent:event index:MouseButton::RIGHT pressed:true outofstream:false];
	} else {
		mouse_down_control = false;
		[self processMouseEvent:event index:MouseButton::LEFT pressed:true outofstream:false];
	}
}

- (void)mouseDragged:(NSEvent *)event {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (ds && ds->has_window(window_id)) {
		DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
		if (wd.edge != DisplayServer::WINDOW_EDGE_MAX) {
			Size2i max_size = wd.max_size / ds->screen_get_max_scale();
			Size2i min_size = wd.min_size / ds->screen_get_max_scale();
			NSRect frame = [wd.window_object frame];
			switch (wd.edge) {
				case DisplayServer::WINDOW_EDGE_TOP_LEFT: {
					int clamped_dx = CLAMP(frame.size.width - event.deltaX, min_size.x, max_size.x) - frame.size.width;
					int clamped_dy = CLAMP(frame.size.height - event.deltaY, min_size.y, max_size.y) - frame.size.height;
					[wd.window_object setFrame:NSMakeRect(frame.origin.x - clamped_dx, frame.origin.y, frame.size.width + clamped_dx, frame.size.height + clamped_dy) display:YES];
				} break;
				case DisplayServer::WINDOW_EDGE_TOP: {
					int clamped_dy = CLAMP(frame.size.height - event.deltaY, min_size.y, max_size.y) - frame.size.height;
					[wd.window_object setFrame:NSMakeRect(frame.origin.x, frame.origin.y, frame.size.width, frame.size.height + clamped_dy) display:YES];
				} break;
				case DisplayServer::WINDOW_EDGE_TOP_RIGHT: {
					int clamped_dx = CLAMP(frame.size.width + event.deltaX, min_size.x, max_size.x) - frame.size.width;
					int clamped_dy = CLAMP(frame.size.height - event.deltaY, min_size.y, max_size.y) - frame.size.height;
					[wd.window_object setFrame:NSMakeRect(frame.origin.x, frame.origin.y, frame.size.width + clamped_dx, frame.size.height + clamped_dy) display:YES];
				} break;
				case DisplayServer::WINDOW_EDGE_LEFT: {
					int clamped_dx = CLAMP(frame.size.width - event.deltaX, min_size.x, max_size.x) - frame.size.width;
					[wd.window_object setFrame:NSMakeRect(frame.origin.x - clamped_dx, frame.origin.y, frame.size.width + clamped_dx, frame.size.height) display:YES];
				} break;
				case DisplayServer::WINDOW_EDGE_RIGHT: {
					int clamped_dx = CLAMP(frame.size.width + event.deltaX, min_size.x, max_size.x) - frame.size.width;
					[wd.window_object setFrame:NSMakeRect(frame.origin.x, frame.origin.y, frame.size.width + clamped_dx, frame.size.height) display:YES];
				} break;
				case DisplayServer::WINDOW_EDGE_BOTTOM_LEFT: {
					int clamped_dx = CLAMP(frame.size.width - event.deltaX, min_size.x, max_size.x) - frame.size.width;
					int clamped_dy = CLAMP(frame.size.height + event.deltaY, min_size.y, max_size.y) - frame.size.height;
					[wd.window_object setFrame:NSMakeRect(frame.origin.x - clamped_dx, frame.origin.y - clamped_dy, frame.size.width + clamped_dx, frame.size.height + clamped_dy) display:YES];
				} break;
				case DisplayServer::WINDOW_EDGE_BOTTOM: {
					int clamped_dy = CLAMP(frame.size.height + event.deltaY, min_size.y, max_size.y) - frame.size.height;
					[wd.window_object setFrame:NSMakeRect(frame.origin.x, frame.origin.y - clamped_dy, frame.size.width, frame.size.height + clamped_dy) display:YES];
				} break;
				case DisplayServer::WINDOW_EDGE_BOTTOM_RIGHT: {
					int clamped_dx = CLAMP(frame.size.width + event.deltaX, min_size.x, max_size.x) - frame.size.width;
					int clamped_dy = CLAMP(frame.size.height + event.deltaY, min_size.y, max_size.y) - frame.size.height;
					[wd.window_object setFrame:NSMakeRect(frame.origin.x, frame.origin.y - clamped_dy, frame.size.width + clamped_dx, frame.size.height + clamped_dy) display:YES];
				} break;
				default:
					break;
			}
			return;
		}
	}
	[self mouseMoved:event];
}

- (void)mouseUp:(NSEvent *)event {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (ds && ds->has_window(window_id)) {
		DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
		wd.edge = DisplayServer::WINDOW_EDGE_MAX;
	}
	if (mouse_down_control) {
		[self processMouseEvent:event index:MouseButton::RIGHT pressed:false outofstream:false];
	} else {
		[self processMouseEvent:event index:MouseButton::LEFT pressed:false outofstream:false];
	}
}

- (void)mouseMoved:(NSEvent *)event {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);

	NSPoint delta = NSMakePoint([event deltaX], [event deltaY]);
	NSPoint mpos = [event locationInWindow];

	if (ds->update_mouse_wrap(wd, delta, mpos, [event timestamp])) {
		return;
	}

	Ref<InputEventMouseMotion> mm;
	mm.instantiate();

	mm->set_window_id(window_id);
	mm->set_button_mask(ds->mouse_get_button_state());
	ds->update_mouse_pos(wd, mpos);
	mm->set_position(wd.mouse_pos);
	mm->set_pressure([event pressure]);
	NSEventSubtype subtype = [event subtype];
	if (subtype == NSEventSubtypeTabletPoint) {
		const NSPoint p = [event tilt];
		mm->set_tilt(Vector2(p.x, -p.y));
		mm->set_pen_inverted(last_pen_inverted);
	} else if (subtype == NSEventSubtypeTabletProximity) {
		// Check if using the eraser end of pen only on proximity event.
		last_pen_inverted = [event pointingDeviceType] == NSPointingDeviceTypeEraser;
		mm->set_pen_inverted(last_pen_inverted);
	}
	mm->set_global_position(wd.mouse_pos);
	mm->set_velocity(Input::get_singleton()->get_last_mouse_velocity());
	mm->set_screen_velocity(mm->get_velocity());
	const Vector2i relativeMotion = Vector2i(delta.x, delta.y) * ds->screen_get_max_scale();
	mm->set_relative(relativeMotion);
	mm->set_relative_screen_position(relativeMotion);
	ds->get_key_modifier_state([event modifierFlags], mm);

	Input::get_singleton()->parse_input_event(mm);
}

- (void)rightMouseDown:(NSEvent *)event {
	[self processMouseEvent:event index:MouseButton::RIGHT pressed:true outofstream:false];
}

- (void)rightMouseDragged:(NSEvent *)event {
	[self mouseMoved:event];
}

- (void)rightMouseUp:(NSEvent *)event {
	[self processMouseEvent:event index:MouseButton::RIGHT pressed:false outofstream:false];
}

- (void)otherMouseDown:(NSEvent *)event {
	if ((int)[event buttonNumber] == 2) {
		[self processMouseEvent:event index:MouseButton::MIDDLE pressed:true outofstream:false];
	} else if ((int)[event buttonNumber] == 3) {
		[self processMouseEvent:event index:MouseButton::MB_XBUTTON1 pressed:true outofstream:false];
	} else if ((int)[event buttonNumber] == 4) {
		[self processMouseEvent:event index:MouseButton::MB_XBUTTON2 pressed:true outofstream:false];
	} else {
		return;
	}
}

- (void)otherMouseDragged:(NSEvent *)event {
	[self mouseMoved:event];
}

- (void)otherMouseUp:(NSEvent *)event {
	if ((int)[event buttonNumber] == 2) {
		[self processMouseEvent:event index:MouseButton::MIDDLE pressed:false outofstream:false];
	} else if ((int)[event buttonNumber] == 3) {
		[self processMouseEvent:event index:MouseButton::MB_XBUTTON1 pressed:false outofstream:false];
	} else if ((int)[event buttonNumber] == 4) {
		[self processMouseEvent:event index:MouseButton::MB_XBUTTON2 pressed:false outofstream:false];
	} else {
		return;
	}
}

- (void)swipeWithEvent:(NSEvent *)event {
	// Swipe gesture on Trackpad/Magic Mouse, or physical back/forward mouse buttons.
	if ([event phase] == NSEventPhaseEnded || [event phase] == NSEventPhaseChanged) {
		if (Math::is_equal_approx([event deltaX], 1.0)) {
			// Swipe left (back).
			[self processMouseEvent:event index:MouseButton::MB_XBUTTON1 pressed:true outofstream:true];
			[self processMouseEvent:event index:MouseButton::MB_XBUTTON1 pressed:false outofstream:true];
		} else if (Math::is_equal_approx([event deltaX], -1.0)) {
			// Swipe right (forward).
			[self processMouseEvent:event index:MouseButton::MB_XBUTTON2 pressed:true outofstream:true];
			[self processMouseEvent:event index:MouseButton::MB_XBUTTON2 pressed:false outofstream:true];
		}
	}
}

- (void)mouseExited:(NSEvent *)event {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	if (ds->mouse_get_mode() != DisplayServer::MOUSE_MODE_CAPTURED) {
		ds->mouse_exit_window(window_id);
	}
}

- (void)mouseEntered:(NSEvent *)event {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	if (ds->mouse_get_mode() != DisplayServer::MOUSE_MODE_CAPTURED) {
		ds->mouse_enter_window(window_id);
	}

	ds->cursor_update_shape();
}

- (void)magnifyWithEvent:(NSEvent *)event {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);

	Ref<InputEventMagnifyGesture> ev;
	ev.instantiate();
	ev->set_window_id(window_id);
	ds->get_key_modifier_state([event modifierFlags], ev);
	ds->update_mouse_pos(wd, [event locationInWindow]);
	ev->set_position(wd.mouse_pos);
	ev->set_factor([event magnification] + 1.0);

	Input::get_singleton()->parse_input_event(ev);
}

- (void)updateTrackingAreas {
	if (tracking_area != nil) {
		[self removeTrackingArea:tracking_area];
	}

	NSTrackingAreaOptions options = NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow | NSTrackingCursorUpdate | NSTrackingInVisibleRect;
	tracking_area = [[NSTrackingArea alloc] initWithRect:[self bounds] options:options owner:self userInfo:nil];

	[self addTrackingArea:tracking_area];
	[super updateTrackingAreas];
}

// MARK: Keyboard

- (void)keyDown:(NSEvent *)event {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);

	ignore_momentum_scroll = true;

	// Ignore all input if IME input is in progress.
	if (!ime_input_event_in_progress) {
		NSString *characters = [event characters];
		NSUInteger length = [characters length];

		if (!wd.im_active && length > 0 && keycode_has_unicode(KeyMappingMacOS::remap_key([event keyCode], [event modifierFlags], true))) {
			// Fallback unicode character handler used if IME is not active.
			Char16String text;
			text.resize([characters length] + 1);
			[characters getCharacters:(unichar *)text.ptrw() range:NSMakeRange(0, [characters length])];

			String u32text;
			u32text.parse_utf16(text.ptr(), text.length());

			DisplayServerMacOS::KeyEvent ke;
			ke.window_id = window_id;
			ke.macos_state = [event modifierFlags];
			ke.pressed = true;
			ke.echo = [event isARepeat];
			ke.keycode = KeyMappingMacOS::remap_key([event keyCode], [event modifierFlags], false);
			ke.physical_keycode = KeyMappingMacOS::translate_key([event keyCode]);
			ke.key_label = KeyMappingMacOS::remap_key([event keyCode], [event modifierFlags], true);
			ke.raw = true;

			if (u32text.is_empty()) {
				ke.unicode = 0;
				ds->push_to_key_event_buffer(ke);
			}
			for (int i = 0; i < u32text.length(); i++) {
				const char32_t codepoint = u32text[i];
				ke.unicode = fix_unicode(codepoint);
				ds->push_to_key_event_buffer(ke);
			}
		} else {
			DisplayServerMacOS::KeyEvent ke;

			ke.window_id = window_id;
			ke.macos_state = [event modifierFlags];
			ke.pressed = true;
			ke.echo = [event isARepeat];
			ke.keycode = KeyMappingMacOS::remap_key([event keyCode], [event modifierFlags], false);
			ke.physical_keycode = KeyMappingMacOS::translate_key([event keyCode]);
			ke.key_label = KeyMappingMacOS::remap_key([event keyCode], [event modifierFlags], true);
			ke.unicode = 0;
			ke.location = KeyMappingMacOS::translate_location([event keyCode]);
			ke.raw = false;

			ds->push_to_key_event_buffer(ke);
		}
	}

	// Pass events to IME handler
	if (wd.im_active) {
		[self interpretKeyEvents:[NSArray arrayWithObject:event]];
	}
}

- (void)flagsChanged:(NSEvent *)event {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}
	ignore_momentum_scroll = true;

	DisplayServerMacOS::KeyEvent ke;

	ke.window_id = window_id;
	ke.echo = false;
	ke.raw = true;

	int key = [event keyCode];
	int mod = [event modifierFlags];

	if (key == 0x36 || key == 0x37) {
		if (mod & NSEventModifierFlagCommand) {
			mod &= ~NSEventModifierFlagCommand;
			ke.pressed = true;
		} else {
			ke.pressed = false;
		}
	} else if (key == 0x38 || key == 0x3c) {
		if (mod & NSEventModifierFlagShift) {
			mod &= ~NSEventModifierFlagShift;
			ke.pressed = true;
		} else {
			ke.pressed = false;
		}
	} else if (key == 0x3a || key == 0x3d) {
		if (mod & NSEventModifierFlagOption) {
			mod &= ~NSEventModifierFlagOption;
			ke.pressed = true;
		} else {
			ke.pressed = false;
		}
	} else if (key == 0x3b || key == 0x3e) {
		if (mod & NSEventModifierFlagControl) {
			mod &= ~NSEventModifierFlagControl;
			ke.pressed = true;
		} else {
			ke.pressed = false;
		}
	} else {
		return;
	}

	ke.macos_state = mod;
	ke.keycode = KeyMappingMacOS::remap_key(key, mod, false);
	ke.physical_keycode = KeyMappingMacOS::translate_key(key);
	ke.key_label = KeyMappingMacOS::remap_key(key, mod, true);
	ke.unicode = 0;
	ke.location = KeyMappingMacOS::translate_location(key);

	ds->push_to_key_event_buffer(ke);
}

- (void)keyUp:(NSEvent *)event {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	// Ignore all input if IME input is in progress.
	if (ime_suppress_next_keyup) {
		ime_suppress_next_keyup = false;
		return;
	}

	if (!ime_input_event_in_progress) {
		DisplayServerMacOS::KeyEvent ke;

		ke.window_id = window_id;
		ke.macos_state = [event modifierFlags];
		ke.pressed = false;
		ke.echo = [event isARepeat];
		ke.keycode = KeyMappingMacOS::remap_key([event keyCode], [event modifierFlags], false);
		ke.physical_keycode = KeyMappingMacOS::translate_key([event keyCode]);
		ke.key_label = KeyMappingMacOS::remap_key([event keyCode], [event modifierFlags], true);
		ke.unicode = 0;
		ke.location = KeyMappingMacOS::translate_location([event keyCode]);
		ke.raw = true;

		ds->push_to_key_event_buffer(ke);
	}
}

// MARK: Scroll and pan

- (void)processScrollEvent:(NSEvent *)event button:(MouseButton)button factor:(double)factor {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
	MouseButtonMask mask = mouse_button_to_mask(button);

	Ref<InputEventMouseButton> sc;
	sc.instantiate();

	sc->set_window_id(window_id);
	ds->get_key_modifier_state([event modifierFlags], sc);
	sc->set_button_index(button);
	sc->set_factor(factor);
	sc->set_pressed(true);
	sc->set_position(wd.mouse_pos);
	sc->set_global_position(wd.mouse_pos);
	BitField<MouseButtonMask> scroll_mask = ds->mouse_get_button_state();
	scroll_mask.set_flag(mask);
	sc->set_button_mask(scroll_mask);

	Input::get_singleton()->parse_input_event(sc);

	sc.instantiate();
	sc->set_window_id(window_id);
	sc->set_button_index(button);
	sc->set_factor(factor);
	sc->set_pressed(false);
	sc->set_position(wd.mouse_pos);
	sc->set_global_position(wd.mouse_pos);
	scroll_mask.clear_flag(mask);
	sc->set_button_mask(scroll_mask);

	Input::get_singleton()->parse_input_event(sc);
}

- (void)processPanEvent:(NSEvent *)event dx:(double)dx dy:(double)dy {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);

	Ref<InputEventPanGesture> pg;
	pg.instantiate();

	pg->set_window_id(window_id);
	ds->get_key_modifier_state([event modifierFlags], pg);
	pg->set_position(wd.mouse_pos);
	pg->set_delta(Vector2(-dx, -dy));

	Input::get_singleton()->parse_input_event(pg);
}

- (void)scrollWheel:(NSEvent *)event {
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (!ds || !ds->has_window(window_id)) {
		return;
	}

	DisplayServerMacOS::WindowData &wd = ds->get_window(window_id);
	ds->update_mouse_pos(wd, [event locationInWindow]);

	double delta_x = [event scrollingDeltaX];
	double delta_y = [event scrollingDeltaY];

	if ([event hasPreciseScrollingDeltas]) {
		delta_x *= 0.03;
		delta_y *= 0.03;
	}

	if ([event momentumPhase] != NSEventPhaseNone) {
		if (ignore_momentum_scroll) {
			return;
		}
	} else {
		ignore_momentum_scroll = false;
	}

	if ([event phase] != NSEventPhaseNone || [event momentumPhase] != NSEventPhaseNone) {
		[self processPanEvent:event dx:delta_x dy:delta_y];
	} else {
		if (fabs(delta_x)) {
			[self processScrollEvent:event button:(0 > delta_x ? MouseButton::WHEEL_RIGHT : MouseButton::WHEEL_LEFT) factor:fabs(delta_x * 0.3)];
		}
		if (fabs(delta_y)) {
			[self processScrollEvent:event button:(0 < delta_y ? MouseButton::WHEEL_UP : MouseButton::WHEEL_DOWN) factor:fabs(delta_y * 0.3)];
		}
	}
}

@end
