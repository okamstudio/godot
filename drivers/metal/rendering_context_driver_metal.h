/**************************************************************************/
/*  rendering_context_driver_metal.h                                      */
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

#ifndef RENDERING_CONTEXT_DRIVER_METAL_H
#define RENDERING_CONTEXT_DRIVER_METAL_H

#ifdef METAL_ENABLED

#import "servers/rendering/rendering_context_driver.h"
#import "servers/rendering/rendering_device_driver.h"

#import <CoreGraphics/CGGeometry.h>

#ifdef __OBJC__
#import "metal_objects.h"

#import <Metal/Metal.h>
#import <QuartzCore/CALayer.h>

@class CAMetalLayer;
@protocol CAMetalDrawable;
#else
typedef enum MTLPixelFormat {
	MTLPixelFormatBGRA8Unorm = 80,
} MTLPixelFormat;
class MDCommandBuffer;
#endif

class PixelFormats;
class MDResourceCache;

class API_AVAILABLE(macos(11.0), ios(14.0), tvos(14.0)) RenderingContextDriverMetal : public RenderingContextDriver {
protected:
#ifdef __OBJC__
	id<MTLDevice> metal_device = nullptr;
#else
	void *metal_device = nullptr;
#endif
	Device device; // There is only one device on Apple Silicon.

public:
	Error initialize() final override;
	const Device &device_get(uint32_t p_device_index) const final override;
	uint32_t device_get_count() const final override;
	bool device_supports_present(uint32_t p_device_index, SurfaceID p_surface) const final override { return true; }
	RenderingDeviceDriver *driver_create() final override;
	void driver_free(RenderingDeviceDriver *p_driver) final override;
	SurfaceID surface_create(const void *p_platform_data) final override;
	void surface_set_size(SurfaceID p_surface, uint32_t p_width, uint32_t p_height) final override;
	void surface_set_vsync_mode(SurfaceID p_surface, DisplayServer::VSyncMode p_vsync_mode) final override;
	DisplayServer::VSyncMode surface_get_vsync_mode(SurfaceID p_surface) const final override;
	uint32_t surface_get_width(SurfaceID p_surface) const final override;
	uint32_t surface_get_height(SurfaceID p_surface) const final override;
	void surface_set_needs_resize(SurfaceID p_surface, bool p_needs_resize) final override;
	bool surface_get_needs_resize(SurfaceID p_surface) const final override;
	void surface_destroy(SurfaceID p_surface) final override;
	bool is_debug_utils_enabled() const final override { return true; }

#pragma mark - Metal-specific methods

	// Platform-specific data for the Windows embedded in this driver.
	struct WindowPlatformData {
#ifdef __OBJC__
		CAMetalLayer *__unsafe_unretained layer;
#else
		void *layer;
#endif
	};

	class API_AVAILABLE(macos(11.0), ios(14.0), tvos(14.0)) Surface {
	protected:
#ifdef __OBJC__
		id<MTLDevice> device;
#else
		void *device;
#endif

	public:
		uint32_t width = 0;
		uint32_t height = 0;
		DisplayServer::VSyncMode vsync_mode = DisplayServer::VSYNC_ENABLED;
		bool needs_resize = false;
		double present_minimum_duration = 0.0;

		Surface(
#ifdef __OBJC__
				id<MTLDevice> p_device
#else
				void *p_device
#endif
				) :
				device(p_device) {
		}
		virtual ~Surface() = default;

		MTLPixelFormat get_pixel_format() const { return MTLPixelFormatBGRA8Unorm; }
		virtual Error resize(uint32_t p_desired_framebuffer_count) = 0;
		virtual RDD::FramebufferID acquire_next_frame_buffer() = 0;
		virtual void present(MDCommandBuffer *p_cmd_buffer) = 0;
		void set_max_fps(int p_max_fps) { present_minimum_duration = p_max_fps ? 1.0 / p_max_fps : 0.0; }
	};

#ifdef __OBJC__
	id<MTLDevice>
#else
	void *
#endif
	get_metal_device() const {
		return metal_device;
	}

#pragma mark - Initialization

	RenderingContextDriverMetal();
	~RenderingContextDriverMetal() override;
};

#endif // METAL_ENABLED

#endif // RENDERING_CONTEXT_DRIVER_METAL_H
