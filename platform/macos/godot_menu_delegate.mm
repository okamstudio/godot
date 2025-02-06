/**************************************************************************/
/*  godot_menu_delegate.mm                                                */
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

#import "godot_menu_delegate.h"

#import "display_server_macos.h"
#import "godot_menu_item.h"
#import "key_mapping_macos.h"
#import "native_menu_macos.h"

@implementation GodotMenuDelegate

- (void)doNothing:(id)sender {
}

- (void)menuWillOpen:(NSMenu *)menu {
	if (NativeMenu::get_singleton()) {
		NativeMenuMacOS *nmenu = (NativeMenuMacOS *)NativeMenu::get_singleton();
		nmenu->_menu_open(menu);
	}
}

- (void)menuNeedsUpdate:(NSMenu *)menu {
	if (NativeMenu::get_singleton()) {
		NativeMenuMacOS *nmenu = (NativeMenuMacOS *)NativeMenu::get_singleton();
		nmenu->_menu_need_update(menu);
	}
}

- (void)menuDidClose:(NSMenu *)menu {
	if (NativeMenu::get_singleton()) {
		NativeMenuMacOS *nmenu = (NativeMenuMacOS *)NativeMenu::get_singleton();
		nmenu->_menu_close(menu);
	}
}

- (void)menu:(NSMenu *)menu willHighlightItem:(NSMenuItem *)item {
	if (item) {
		GodotMenuItem *value = [item representedObject];
		if (value && value->hover_callback.is_valid()) {
			// If custom callback is set, use it.
			Variant ret;
			Callable::CallError ce;
			const Variant *args[1] = { &value->meta };

			value->hover_callback.callp(args, 1, ret, ce);
			if (ce.error != Callable::CallError::CALL_OK) {
				ERR_PRINT(vformat("Failed to execute menu hover callback: %s.", Variant::get_callable_error_text(value->hover_callback, args, 1, ce)));
			}
		}
	}
}

- (BOOL)menuHasKeyEquivalent:(NSMenu *)menu forEvent:(NSEvent *)event target:(id *)target action:(SEL *)action {
	NSString *ev_key = [[event charactersIgnoringModifiers] lowercaseString];
	NSUInteger ev_modifiers = [event modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask;
	for (int i = 0; i < [menu numberOfItems]; i++) {
		const NSMenuItem *menu_item = [menu itemAtIndex:i];
		if ([menu_item isEnabled] && [[menu_item keyEquivalent] compare:ev_key] == NSOrderedSame) {
			NSUInteger item_modifiers = [menu_item keyEquivalentModifierMask];

			if (ev_modifiers == item_modifiers) {
				GodotMenuItem *value = [menu_item representedObject];
				if (value) {
					if (value->key_callback.is_valid()) {
						// If custom callback is set, use it.
						Variant ret;
						Callable::CallError ce;
						const Variant *args[1] = { &value->meta };

						value->key_callback.callp(args, 1, ret, ce);
						if (ce.error != Callable::CallError::CALL_OK) {
							ERR_PRINT(vformat("Failed to execute menu key callback: %s.", Variant::get_callable_error_text(value->key_callback, args, 1, ce)));
						}
					} else {
						// Otherwise redirect event to the engine.
						if (DisplayServer::get_singleton()) {
							if ([[NSApplication sharedApplication] keyWindow].sheet) {
								[[[[NSApplication sharedApplication] keyWindow] sheetParent] sendEvent:event];
							} else {
								[[[NSApplication sharedApplication] keyWindow] sendEvent:event];
							}
						}
					}

					// Suppress default menu action.
					*target = self;
					*action = @selector(doNothing:);
				}
				return YES;
			}
		}
	}
	return NO;
}

@end
