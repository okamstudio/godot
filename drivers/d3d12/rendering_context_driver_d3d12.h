/**************************************************************************/
/*  rendering_context_driver_d3d12.h                                      */
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

#ifndef RENDERING_CONTEXT_DRIVER_D3D12_H
#define RENDERING_CONTEXT_DRIVER_D3D12_H

#include "core/error/error_list.h"
#include "core/os/mutex.h"
#include "core/string/ustring.h"
#include "core/templates/rid_owner.h"
#include "rendering_device_driver_d3d12.h"
#include "servers/display_server.h"
#include "servers/rendering/rendering_context_driver.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wswitch"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#pragma clang diagnostic ignored "-Wstring-plus-int"
#pragma clang diagnostic ignored "-Wswitch"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#endif

#if defined(AS)
#undef AS
#endif

#if (WINVER < _WIN32_WINNT_WIN8) && defined(_MSC_VER)
#pragma push_macro("NTDDI_VERSION")
#pragma push_macro("WINVER")
#undef NTDDI_VERSION
#undef WINVER
#define NTDDI_VERSION NTDDI_WIN8
#define WINVER _WIN32_WINNT_WIN8
#include <dcomp.h>
#pragma pop_macro("WINVER")
#pragma pop_macro("NTDDI_VERSION")
#else
#include <dcomp.h>
#endif

#include "d3dx12.h"
#include <dxgi1_6.h>

#include <wrl/client.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(__clang__)
#pragma clang diagnostic pop
#endif

using Microsoft::WRL::ComPtr;

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

class RenderingContextDriverD3D12 : public RenderingContextDriver {
	ComPtr<ID3D12DeviceFactory> device_factory;
	ComPtr<IDXGIFactory2> dxgi_factory;
	TightLocalVector<Device> driver_devices;
	bool tearing_supported = false;

	Error _init_device_factory();
	Error _initialize_debug_layers();
	Error _initialize_devices();

public:
	virtual Error initialize() override;
	virtual const Device &device_get(uint32_t p_device_index) const override;
	virtual uint32_t device_get_count() const override;
	virtual bool device_supports_present(uint32_t p_device_index, SurfaceID p_surface) const override;
	virtual RenderingDeviceDriver *driver_create() override;
	virtual void driver_free(RenderingDeviceDriver *p_driver) override;
	virtual SurfaceID surface_create(const void *p_platform_data) override;
	virtual void surface_set_size(SurfaceID p_surface, uint32_t p_width, uint32_t p_height) override;
	virtual void surface_set_vsync_mode(SurfaceID p_surface, DisplayServer::VSyncMode p_vsync_mode) override;
	virtual DisplayServer::VSyncMode surface_get_vsync_mode(SurfaceID p_surface) const override;
	virtual uint32_t surface_get_width(SurfaceID p_surface) const override;
	virtual uint32_t surface_get_height(SurfaceID p_surface) const override;
	virtual void surface_set_needs_resize(SurfaceID p_surface, bool p_needs_resize) override;
	virtual bool surface_get_needs_resize(SurfaceID p_surface) const override;
	virtual void surface_destroy(SurfaceID p_surface) override;
	virtual bool is_debug_utils_enabled() const override;

	// Platform-specific data for the Windows embedded in this driver.
	struct WindowPlatformData {
		HWND window;
	};

	// D3D12-only methods.
	struct Surface {
		HWND hwnd = nullptr;
		uint32_t width = 0;
		uint32_t height = 0;
		DisplayServer::VSyncMode vsync_mode = DisplayServer::VSYNC_ENABLED;
		bool needs_resize = false;
		ComPtr<IDCompositionDevice> composition_device;
		ComPtr<IDCompositionTarget> composition_target;
		ComPtr<IDCompositionVisual> composition_visual;
	};

	HMODULE lib_d3d12 = nullptr;
	HMODULE lib_dxgi = nullptr;
	HMODULE lib_dcomp = nullptr;

	IDXGIAdapter1 *create_adapter(uint32_t p_adapter_index) const;
	ID3D12DeviceFactory *device_factory_get() const;
	IDXGIFactory2 *dxgi_factory_get() const;
	bool get_tearing_supported() const;
	bool use_validation_layers() const;

	RenderingContextDriverD3D12();
	virtual ~RenderingContextDriverD3D12() override;
};

#endif // RENDERING_CONTEXT_DRIVER_D3D12_H
