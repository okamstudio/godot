/**************************************************************************/
/*  view_controller.h                                                     */
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
#import <UIKit/UIKit.h>
#import <CompositorServices/CompositorServices.h>

@class GodotView;
@class GodotNativeVideoView;
@class GodotKeyboardInputView;

@protocol SwiftVisionController <NSObject>
@required
- (void) finishedLoading;
- (void) setImmersiveSpace:(bool) immersive;
- (void)presentViewController:(UIViewController *)viewControllerToPresent;
@end



@interface ViewController : NSObject

@property(nonatomic, readonly, strong) GodotView *godotView;
@property(nonatomic, readonly, strong) GodotKeyboardInputView *keyboardView;
@property(nonatomic, readwrite) id<SwiftVisionController>  swiftController;

-(void)loadView;
-(void)presentViewController:(UIViewController *)viewControllerToPresent animated:(BOOL)flag completion:(void (^)(void))completion;
-(void)viewDidAppear;
-(void)runLoop;
-(BOOL)setup:(cp_layer_renderer_t)renderer;
@end
#endif