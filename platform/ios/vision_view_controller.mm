/**************************************************************************/
/*  view_controller.mm                                                    */
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
#if defined(VISIONOS)
#import "vision_view_controller.h"

#import "display_server_ios.h"
#import "godot_vision_view.h"
#import "godot_view_renderer.h"
#import "key_mapping_ios.h"
#import "keyboard_input_view.h"
#import "os_ios.h"
#import "app_delegate.h"

#include "core/config/project_settings.h"

#import <AVFoundation/AVFoundation.h>
#import <GameController/GameController.h>
#import <Foundation/Foundation.h>


@interface RenderThread : NSThread {
    cp_layer_renderer_t _layerRenderer;
	// ViewController * _viewController;
}

- (instancetype)initWithLayerRenderer:(cp_layer_renderer_t)layerRenderer;
@end
@implementation RenderThread

- (instancetype)initWithLayerRenderer:(cp_layer_renderer_t)layerRenderer
{
    if (self = [self init]) {
        _layerRenderer = layerRenderer;
		// _viewController = [AppDelegate viewController];
    }
    return self;
}

- (void)main {
    [[AppDelegate viewController] runLoop];
}


@end


@interface ViewController () <GodotViewDelegate>

@property(strong, nonatomic) GodotViewRenderer *renderer;
@property(strong, nonatomic) GodotKeyboardInputView *keyboardView;
@property(strong,readwrite, nonatomic) GodotView *view;
@property (nonatomic, assign) cp_layer_renderer_t __unsafe_unretained layerRenderer;

// @property(strong, nonatomic) UIView *godotLoadingOverlay;

@end

@implementation ViewController

- (GodotView *)godotView {
	return (GodotView *)self.view;
}
- (BOOL)setup:(cp_layer_renderer_t)renderer {
	self.layerRenderer = renderer;
	RenderThread *renderThread = [[RenderThread alloc] initWithLayerRenderer:renderer];
    renderThread.name = @"Spatial Renderer Thread";
    [renderThread start];
	return true;
}

- (void)presentViewController:(UIViewController *)viewControllerToPresent animated:(BOOL)flag completion:(void (^)(void))completion {
	// [self.swiftController = presentViewController:viewControllerToPresent];
	//TODO: Send this to the swift code
}

-(void) viewDidAppear{
	[self.swiftController setImmersiveSpace:true];
}

 bool _running = true;
 -(void) runLoop {
		[self.view setup:self.layerRenderer];
        while (_running) {
            @autoreleasepool {
                switch (cp_layer_renderer_get_state(self.layerRenderer)) {
                    case cp_layer_renderer_state_paused:
                        cp_layer_renderer_wait_until_running(self.layerRenderer);
                        break;
                        
                    case cp_layer_renderer_state_running:
                        [self.view drawView];
                        break;
                        
                        
                    case cp_layer_renderer_state_invalidated:
                        _running = false;
                        break;
                }
            }
        }
    }

- (void)pressesBegan:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent *)event {
	// [super pressesBegan:presses withEvent:event];

	if (!DisplayServerIOS::get_singleton() || DisplayServerIOS::get_singleton()->is_keyboard_active()) {
		return;
	}
	if (@available(iOS 13.4, *)) {
		for (UIPress *press in presses) {
			String u32lbl = String::utf8([press.key.charactersIgnoringModifiers UTF8String]);
			String u32text = String::utf8([press.key.characters UTF8String]);
			Key key = KeyMappingIOS::remap_key(press.key.keyCode);

			if (press.key.keyCode == 0 && u32text.is_empty() && u32lbl.is_empty()) {
				continue;
			}

			char32_t us = 0;
			if (!u32lbl.is_empty() && !u32lbl.begins_with("UIKey")) {
				us = u32lbl[0];
			}

			KeyLocation location = KeyMappingIOS::key_location(press.key.keyCode);

			if (!u32text.is_empty() && !u32text.begins_with("UIKey")) {
				for (int i = 0; i < u32text.length(); i++) {
					const char32_t c = u32text[i];
					DisplayServerIOS::get_singleton()->key(fix_keycode(us, key), c, fix_key_label(us, key), key, press.key.modifierFlags, true, location);
				}
			} else {
				DisplayServerIOS::get_singleton()->key(fix_keycode(us, key), 0, fix_key_label(us, key), key, press.key.modifierFlags, true, location);
			}
		}
	}
}

- (void)pressesEnded:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent *)event {
	// [super pressesEnded:presses withEvent:event];

	if (!DisplayServerIOS::get_singleton() || DisplayServerIOS::get_singleton()->is_keyboard_active()) {
		return;
	}
	if (@available(iOS 13.4, *)) {
		for (UIPress *press in presses) {
			String u32lbl = String::utf8([press.key.charactersIgnoringModifiers UTF8String]);
			Key key = KeyMappingIOS::remap_key(press.key.keyCode);

			if (press.key.keyCode == 0 && u32lbl.is_empty()) {
				continue;
			}

			char32_t us = 0;
			if (!u32lbl.is_empty() && !u32lbl.begins_with("UIKey")) {
				us = u32lbl[0];
			}

			KeyLocation location = KeyMappingIOS::key_location(press.key.keyCode);

			DisplayServerIOS::get_singleton()->key(fix_keycode(us, key), 0, fix_key_label(us, key), key, press.key.modifierFlags, false, location);
		}
	}
}

- (void)loadView {
	GodotView *view = [[GodotView alloc] init];
	GodotViewRenderer *renderer = [[GodotViewRenderer alloc] init];

	self.renderer = renderer;
	self.view = view;

	view.renderer = self.renderer;
	view.delegate = self;
	[self observeKeyboard];
}

- (void)observeKeyboard {
	self.keyboardView = [GodotKeyboardInputView new];
	//TODO: Figure out the keyboard view
	// [self.view addSubview:self.keyboardView];

	[[NSNotificationCenter defaultCenter]
			addObserver:self
			   selector:@selector(keyboardOnScreen:)
				   name:UIKeyboardDidShowNotification
				 object:nil];
	[[NSNotificationCenter defaultCenter]
			addObserver:self
			   selector:@selector(keyboardHidden:)
				   name:UIKeyboardDidHideNotification
				 object:nil];
}

- (BOOL)godotViewFinishedSetup:(GodotView *)view {
	
	if(self.swiftController) {
		[self.swiftController finishedLoading];
	}
	return YES;
}

- (void)dealloc {
	self.keyboardView = nil;

	self.renderer = nil;


	[[NSNotificationCenter defaultCenter] removeObserver:self];
}

// MARK: Orientation


// MARK: Keyboard

- (void)keyboardOnScreen:(NSNotification *)notification {
	// NSDictionary *info = notification.userInfo;
	// NSValue *value = info[UIKeyboardFrameEndUserInfoKey];

	// CGRect rawFrame = [value CGRectValue];
	// CGRect keyboardFrame = [self.view convertRect:rawFrame fromView:nil];

	// if (DisplayServerIOS::get_singleton()) {
	// 	DisplayServerIOS::get_singleton()->virtual_keyboard_set_height(keyboardFrame.size.height);
	// }
}

- (void)keyboardHidden:(NSNotification *)notification {
	if (DisplayServerIOS::get_singleton()) {
		DisplayServerIOS::get_singleton()->virtual_keyboard_set_height(0);
	}
}

@end
#endif
