/**************************************************************************/
/*  godot_view.h                                                          */
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
#import <Foundation/Foundation.h>
#import <CompositorServices/CompositorServices.h>
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>


@class GodotView;

@protocol DisplayLayer;
@protocol GodotViewRendererProtocol;

@protocol GodotViewDelegate

- (BOOL)godotViewFinishedSetup:(GodotView *)view;

@end


@interface GodotView : NSObject{}

@property(assign, nonatomic) id<GodotViewRendererProtocol> renderer;
@property(assign, nonatomic) id<GodotViewDelegate> delegate;

@property(assign, readonly, nonatomic) BOOL isActive;

@property (nonatomic, assign, assign) cp_frame_timing_t timing;
@property (nonatomic, assign, assign) cp_frame_t frame;
@property (nonatomic, assign, assign) cp_drawable_t drawable;
@property (nonatomic, assign) cp_layer_renderer_t __unsafe_unretained layerRenderer;

@property(assign, readonly, nonatomic) CGRect bounds;

- (GodotView<DisplayLayer> *)initializeRenderingForDriver:(NSString *)driverName;
- (void)stopRendering;
- (void)startRendering;
- (void)drawView;
- (BOOL)setup:(cp_layer_renderer_t)renderer;
- (CGSize)screen_get_size:(int)p_screen;
- (CGRect)get_display_safe_area;

@end
#endif