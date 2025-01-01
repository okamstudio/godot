/**************************************************************************/
/*  keyboard_input_view.mm                                                */
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

#import "keyboard_input_view.h"

#import "display_server_ios.h"
#import "os_ios.h"

#include "core/os/keyboard.h"

@interface GodotKeyboardInputView () <UITextViewDelegate>

@property(nonatomic, copy) NSString *previousText;
@property(nonatomic, assign) NSRange previousSelectedRange;

@end

@implementation GodotKeyboardInputView

- (instancetype)initWithCoder:(NSCoder *)coder {
	self = [super initWithCoder:coder];

	if (self) {
		[self godot_commonInit];
	}

	return self;
}

- (instancetype)initWithFrame:(CGRect)frame textContainer:(NSTextContainer *)textContainer {
	self = [super initWithFrame:frame textContainer:textContainer];

	if (self) {
		[self godot_commonInit];
	}

	return self;
}

- (void)godot_commonInit {
	self.hidden = YES;
	self.delegate = self;

	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(observeTextChange:)
												 name:UITextViewTextDidChangeNotification
											   object:self];
}

- (void)dealloc {
	self.delegate = nil;
	[[NSNotificationCenter defaultCenter] removeObserver:self];
}

// MARK: Keyboard

- (BOOL)canBecomeFirstResponder {
	return YES;
}

- (BOOL)becomeFirstResponderWithString:(NSString *)existingString cursorStart:(NSInteger)start cursorEnd:(NSInteger)end {
	self.text = existingString;
	self.previousText = existingString;

	NSInteger safeStartIndex = MAX(start, 0);

	NSRange textRange;

	// Either a simple cursor or a selection.
	if (end > 0) {
		textRange = NSMakeRange(safeStartIndex, end - start);
	} else {
		textRange = NSMakeRange(safeStartIndex, 0);
	}

	self.selectedRange = textRange;
	self.previousSelectedRange = textRange;

	return [self becomeFirstResponder];
}

- (BOOL)resignFirstResponder {
	self.text = nil;
	self.previousText = nil;
	return [super resignFirstResponder];
}

// MARK: OS Messages

- (void)deleteText:(NSInteger)charactersToDelete {
	for (int i = 0; i < charactersToDelete; i++) {
		DisplayServerIOS::get_singleton()->key(Key::BACKSPACE, 0, Key::BACKSPACE, Key::NONE, 0, true, KeyLocation::UNSPECIFIED);
		DisplayServerIOS::get_singleton()->key(Key::BACKSPACE, 0, Key::BACKSPACE, Key::NONE, 0, false, KeyLocation::UNSPECIFIED);
	}
}

- (void)enterText:(NSString *)substring {
	String characters;
	characters.parse_utf8([substring UTF8String]);

	for (int i = 0; i < characters.size(); i++) {
		int character = characters[i];
		Key key = Key::NONE;

		if (character == '\t') { // 0x09
			key = Key::TAB;
		} else if (character == '\n') { // 0x0A
			key = Key::ENTER;
		} else if (character == 0x2006) {
			key = Key::SPACE;
		}

		DisplayServerIOS::get_singleton()->key(key, character, key, Key::NONE, 0, true, KeyLocation::UNSPECIFIED);
		DisplayServerIOS::get_singleton()->key(key, character, key, Key::NONE, 0, false, KeyLocation::UNSPECIFIED);
	}
}

// MARK: Observer

- (void)observeTextChange:(NSNotification *)notification {
	if (notification.object != self) {
		return;
	}

	NSString *substringToDelete = nil;
	if (self.previousSelectedRange.length == 0) {
		// Get previous text to delete.
		substringToDelete = [self.previousText substringToIndex:self.previousSelectedRange.location];
	} else {
		// If text was previously selected we are sending only one `backspace`. It will remove all text from text input.
		[self deleteText:1];
	}

	NSString *substringToEnter = nil;
	if (self.selectedRange.length == 0) {
		// If previous cursor had a selection we have to calculate an inserted text.
		if (self.previousSelectedRange.length != 0) {
			NSInteger rangeEnd = self.selectedRange.location + self.selectedRange.length;
			NSInteger rangeStart = MIN(self.previousSelectedRange.location, self.selectedRange.location);
			NSInteger rangeLength = MAX(0, rangeEnd - rangeStart);

			NSRange calculatedRange;

			if (rangeLength >= 0) {
				calculatedRange = NSMakeRange(rangeStart, rangeLength);
			} else {
				calculatedRange = NSMakeRange(rangeStart, 0);
			}

			substringToEnter = [self.text substringWithRange:calculatedRange];
		} else {
			substringToEnter = [self.text substringToIndex:self.selectedRange.location];
		}
	} else {
		substringToEnter = [self.text substringWithRange:self.selectedRange];
	}

	NSInteger skip = 0;
	if (substringToDelete != nil) {
		for (NSUInteger i = 0; i < MIN([substringToDelete length], [substringToEnter length]); i++) {
			if ([substringToDelete characterAtIndex:i] == [substringToEnter characterAtIndex:i]) {
				skip++;
			} else {
				break;
			}
		}
		[self deleteText:[substringToDelete length] - skip]; // Delete changed part of previous text.
	}
	[self enterText:[substringToEnter substringFromIndex:skip]]; // Enter changed part of new text.

	self.previousText = self.text;
	self.previousSelectedRange = self.selectedRange;
}

@end
