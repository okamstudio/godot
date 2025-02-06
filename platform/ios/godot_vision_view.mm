/**************************************************************************/
/*  godot_view.mm                                                         */
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
#import "godot_vision_view.h"

#import "display_layer.h"
#import "display_server_ios.h"
#import "godot_view_renderer.h"
#import "godot_vision_view.h"

#include "core/config/project_settings.h"
#include "core/os/keyboard.h"
#include "core/string/ustring.h"
#import <CompositorServices/CompositorServices.h>
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>


#import <CoreMotion/CoreMotion.h>

static const float earth_gravity = 9.80665;


@interface GodotView () {
}

@property(assign, nonatomic) BOOL isActive;

@property(strong, nonatomic) CMMotionManager *motionManager;


@end

@implementation GodotView

- (CGSize)screen_get_size:(int)p_screen{
	return self.size;
}
- (CGRect)get_display_safe_area{
	CGSize s = [self screen_get_size:0];
	return CGRectMake( 0,0,s.width,s.height);
}
bool _is_initialized;
- (GodotView<DisplayLayer> *)initializeRenderingForDriver:(NSString *)driverName {
	if (_is_initialized) {
		return self;
	}

	if (![driverName isEqualToString:@"metal"]) {
		print_verbose("error: only metal driver is not supported on VisionOS");
		return nil;
	}

	// [self initializeDisplayLayer];
	_is_initialized = YES;
	return self;
}


- (instancetype)init{
	if (self) {
		[self godot_commonInit];
	}
	return self;
}

- (void)dealloc {
	[self stopRendering];

	self.renderer = nil;

//TODO: Call into swift and remove the view
	// if (self.renderingLayer) {
	// 	[self.renderingLayer removeFromSuperlayer];
	// 	self.renderingLayer = nil;
	// }

	if (self.motionManager) {
		[self.motionManager stopDeviceMotionUpdates];
		self.motionManager = nil;
	}
}

- (void)godot_commonInit {

	// Configure and start accelerometer
	if (!self.motionManager) {
		self.motionManager = [[CMMotionManager alloc] init];
		if (self.motionManager.deviceMotionAvailable) {
			self.motionManager.deviceMotionUpdateInterval = 1.0 / 70.0;
			[self.motionManager startDeviceMotionUpdatesUsingReferenceFrame:CMAttitudeReferenceFrameXMagneticNorthZVertical];
		} else {
			self.motionManager = nil;
		}
	}
}

- (void)system_theme_changed {
	DisplayServerIOS *ds = (DisplayServerIOS *)DisplayServer::get_singleton();
	if (ds) {
		ds->emit_system_theme_changed();
	}
}
- (BOOL)setup:(cp_layer_renderer_t)renderer {
	NSLog(@"setup Called");
	self.layerRenderer = renderer;
	return YES;
}

- (void)stopRendering {
	if (!self.isActive) {
		return;
	}

	self.isActive = NO;
	print_verbose("Stop animation!");
}

- (void)startRendering {
	if (self.isActive) {
		return;
	}

	self.isActive = YES;
	print_verbose("Start animation!");
	runWorldTrackingARSession();
}

- (void)drawView {
	if (!self.isActive) {
		print_verbose("Draw view not active!");
//		return;
	}
	RenderingDevice *rendering_device = RenderingDevice::get_singleton();
	if (rendering_device) {
		rendering_device->make_current();
		// NSLog(@"RenderingDevice is now current.");
//		return;
	}

	if (!self.renderer) {
		return;
	}

	//The UIView is never used...
	if ([self.renderer setupView:nil]) {
		return;
	}

	if (self.delegate) {
		BOOL delegateFinishedSetup = [self.delegate godotViewFinishedSetup:self];

		if (!delegateFinishedSetup) {
			return;
		}
	}
	

	// [self handleMotion];
	//VISIONOS STuff
	//TODO: Move this to pre-draw in the
	if(![self shouldDraw]){
		NSLog(@"Should not draw");
		return;
	}
	
	[self.renderer renderOnView:nil];

	cp_frame_end_submission(_frame);
}


-(CGRect)bounds{
	CGSize s = self.size;
	return CGRectMake(0,0,s.width,s.height);
}


CGSize _size = CGSizeZero;
- (CGSize)size{
	if(self.drawable){
		id<MTLTexture> texture = cp_drawable_get_color_texture(self.drawable, 0);
		if(texture){
			_size = CGSizeMake(texture.width, texture.height);
			return _size;
		}
	}
	if(_size.width == 0 || _size.height == 0){
		#if VISIONOS_SIMULATOR
		_size = CGSizeMake(2732, 2048);
		#else
		//The max size for the VisionPro on device is 1920x1824 when you create the display buffer
		_size = CGSizeMake(1920, 1824);
		#endif
	}
	return _size;
}
- (void)setsize:(CGSize)newValue{
	_size = newValue;
}

- (bool)shouldDraw {
	if (!self.layerRenderer) {
		NSLog(@"Layer renderer is null.");
		return false;
	}

    // Query the next frame from the layer renderer
    cp_frame_t __frame = cp_layer_renderer_query_next_frame(self.layerRenderer);

	_frame = __frame;
	if(_frame == nullptr) {
		NSLog(@"Frame is null");
		return false;
	}

	cp_frame_timing_t __timing = cp_frame_predict_timing(self.frame);
	self.timing = __timing;
	if (self.timing == nullptr) {
		NSLog(@"Timing is null");
		return false;
	}
	//TODO: We are going to move this out of here
	bool shouldContinue = [self pre_draw_viewport];
	if (!shouldContinue) {
		NSLog(@"pre_draw is not ready");
		return false;
	}

	return true;
	//TODO: We need to move this to the end of the drawing
	//We need to hook into the end of the drawing and call cp_frame_end_submission(frame) to submit the frame
//	cp_frame_end_submission(_frame);
}


- (bool)pre_draw_viewport {
	
	RenderingDevice *rendering_device = RenderingDevice::get_singleton();
	if (!rendering_device) {
		NSLog(@"RenderingDevice is null.");
		return false;
	}
	rendering_device->make_current();
	cp_frame_start_update(self.frame);
	
	cp_time_wait_until(cp_frame_timing_get_optimal_input_time(self.timing));
	
	cp_frame_start_submission(self.frame);
	cp_drawable_t __drawable = cp_frame_query_drawable(self.frame);
	if (__drawable == nullptr) {
		//If no drawable, we can't render
		//so we will return false on the pre_draw_viewport
		self.drawable = nullptr;
		NSLog(@"No drawable found");
		return false;
	}
	//Now that we have the drawable, we can
	//This is all the normal drawing code!
	self.drawable = __drawable;
	//  MTLViewport vp = [self viewportForViewIndex:0];
	//  NSLog(@"Viewport: %f %f %f %f %f %f", vp.originX, vp.originY, vp.width, vp.height, vp.znear, vp.zfar);
	//We will setup and do the AR stuff in pre_draw_viewport
	cp_frame_timing_t actualTiming = cp_drawable_get_frame_timing(self.drawable);
	if(actualTiming == nullptr){
		return false;
	}
	self.timing = actualTiming;
	// return true;
	ar_device_anchor_t anchor = createPoseForTiming(actualTiming);
	cp_drawable_set_device_anchor(self.drawable, anchor);
	return true;
}
- (void)stopRenderDisplayLayer{
	NSLog(@"stopRenderDisplayLayer");
	cp_frame_end_submission(self.frame);
}

void runWorldTrackingARSession() {
	ar_world_tracking_configuration_t worldTrackingConfiguration = ar_world_tracking_configuration_create();
	_worldTrackingProvider = ar_world_tracking_provider_create(worldTrackingConfiguration);

	ar_data_providers_t dataProviders = ar_data_providers_create_with_data_providers(_worldTrackingProvider, nil);

	_arSession = ar_session_create();
	ar_session_run(_arSession, dataProviders);
}

ar_device_anchor_t createPoseForTiming(cp_frame_timing_t timing) {
	ar_device_anchor_t outAnchor = ar_device_anchor_create();
	cp_time_t presentationTime = cp_frame_timing_get_presentation_time(timing);
	CFTimeInterval queryTime = cp_time_to_cf_time_interval(presentationTime);
	ar_device_anchor_query_status_t status = ar_world_tracking_provider_query_device_anchor_at_timestamp(_worldTrackingProvider, queryTime, outAnchor);
	if (status != ar_device_anchor_query_status_success) {
		NSLog(@"Failed to get estimated pose from world tracking provider for presentation timestamp %0.3f", queryTime);
	}
	return outAnchor;
}

ar_session_t _arSession;
ar_world_tracking_provider_t _worldTrackingProvider;
bool _running = true;

// MARK: Motion

- (void)handleMotion {
	if (!self.motionManager) {
		return;
	}

	// Just using polling approach for now, we can set this up so it sends
	// data to us in intervals, might be better. See Apple reference pages
	// for more details:
	// https://developer.apple.com/reference/coremotion/cmmotionmanager?language=objc

	// Apple splits our accelerometer date into a gravity and user movement
	// component. We add them back together.
	CMAcceleration gravity = self.motionManager.deviceMotion.gravity;
	CMAcceleration acceleration = self.motionManager.deviceMotion.userAcceleration;

	// To be consistent with Android we convert the unit of measurement from g (Earth's gravity)
	// to m/s^2.
	gravity.x *= earth_gravity;
	gravity.y *= earth_gravity;
	gravity.z *= earth_gravity;
	acceleration.x *= earth_gravity;
	acceleration.y *= earth_gravity;
	acceleration.z *= earth_gravity;

	///@TODO We don't seem to be getting data here, is my device broken or
	/// is this code incorrect?
	CMMagneticField magnetic = self.motionManager.deviceMotion.magneticField.field;

	///@TODO we can access rotationRate as a CMRotationRate variable
	///(processed date) or CMGyroData (raw data), have to see what works
	/// best
	CMRotationRate rotation = self.motionManager.deviceMotion.rotationRate;

	DisplayServerIOS::get_singleton()->update_gravity(Vector3(gravity.x, gravity.y, gravity.z));
	DisplayServerIOS::get_singleton()->update_accelerometer(Vector3(acceleration.x + gravity.x, acceleration.y + gravity.y, acceleration.z + gravity.z));
	DisplayServerIOS::get_singleton()->update_magnetometer(Vector3(magnetic.x, magnetic.y, magnetic.z));
	DisplayServerIOS::get_singleton()->update_gyroscope(Vector3(rotation.x, rotation.y, rotation.z));

}

@end
#endif
