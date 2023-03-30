/**************************************************************************/
/*  godot_application_delegate.h                                          */
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

#ifndef GODOT_APPLICATION_DELEGATE_H
#define GODOT_APPLICATION_DELEGATE_H

#include "core/os/os.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

@interface GodotApplicationDelegate : NSObject <NSUserInterfaceItemSearching, NSApplicationDelegate> {
	bool high_contrast;
	bool reduce_motion;
	bool reduce_transparency;
	bool voice_over;
}

- (void)forceUnbundledWindowActivationHackStep1;
- (void)forceUnbundledWindowActivationHackStep2;
- (void)forceUnbundledWindowActivationHackStep3;
- (void)handleAppleEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent;
- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context;
- (void)accessibilityDisplayOptionsChange:(NSNotification *)notification;
- (bool)getHighContrast;
- (bool)getReduceMotion;
- (bool)getReduceTransparency;
- (bool)getVoiceOver;
@end

#endif // GODOT_APPLICATION_DELEGATE_H
