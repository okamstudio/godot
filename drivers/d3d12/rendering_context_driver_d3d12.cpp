/**************************************************************************/
/*  rendering_context_driver_d3d12.cpp                                    */
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

#include "rendering_context_driver_d3d12.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/string/ustring.h"
#include "core/templates/local_vector.h"
#include "core/version.h"
#include "servers/rendering/rendering_device.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wswitch"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#pragma clang diagnostic ignored "-Wstring-plus-int"
#pragma clang diagnostic ignored "-Wswitch"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif

#include "dxcapi.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(__clang__)
#pragma clang diagnostic pop
#endif

#if !defined(_MSC_VER)
#include <guiddef.h>

#include <dxguids.h>
#endif

// Note: symbols are not available in MinGW and old MSVC import libraries.
// GUID values from https://github.com/microsoft/DirectX-Headers/blob/7a9f4d06911d30eecb56a4956dab29dcca2709ed/include/directx/d3d12.idl#L5877-L5881
const GUID CLSID_D3D12DeviceFactoryGodot = { 0x114863bf, 0xc386, 0x4aee, { 0xb3, 0x9d, 0x8f, 0x0b, 0xbb, 0x06, 0x29, 0x55 } };
const GUID CLSID_D3D12DebugGodot = { 0xf2352aeb, 0xdd84, 0x49fe, { 0xb9, 0x7b, 0xa9, 0xdc, 0xfd, 0xcc, 0x1b, 0x4f } };
const GUID CLSID_D3D12SDKConfigurationGodot = { 0x7cda6aca, 0xa03e, 0x49c8, { 0x94, 0x58, 0x03, 0x34, 0xd2, 0x0e, 0x07, 0xce } };

#ifdef PIX_ENABLED
#if defined(__GNUC__)
#define _MSC_VER 1800
#endif
#define USE_PIX
#include "WinPixEventRuntime/pix3.h"
#if defined(__GNUC__)
#undef _MSC_VER
#endif
#endif

RenderingContextDriverD3D12::RenderingContextDriverD3D12() {}

RenderingContextDriverD3D12::~RenderingContextDriverD3D12() {
	// Let's release manually everything that may still be holding
	// onto the DLLs before freeing them.
	device_factory.Reset();
	dxgi_factory.Reset();

	if (lib_d3d12) {
		FreeLibrary(lib_d3d12);
	}
	if (lib_dxgi) {
		FreeLibrary(lib_dxgi);
	}
	if (lib_dcomp) {
		FreeLibrary(lib_dcomp);
	}
}

Error RenderingContextDriverD3D12::_init_device_factory() {
	uint32_t agility_sdk_version = GLOBAL_GET("rendering/rendering_device/d3d12/agility_sdk_version");
	String agility_sdk_path = String(".\\") + Engine::get_singleton()->get_architecture_name();

	lib_d3d12 = LoadLibraryW(L"D3D12.dll");
	ERR_FAIL_NULL_V(lib_d3d12, ERR_CANT_CREATE);

	lib_dxgi = LoadLibraryW(L"DXGI.dll");
	ERR_FAIL_NULL_V(lib_dxgi, ERR_CANT_CREATE);

	lib_dcomp = LoadLibraryW(L"Dcomp.dll");
	ERR_FAIL_NULL_V(lib_dcomp, ERR_CANT_CREATE);

	// Note: symbol is not available in MinGW import library.
	PFN_D3D12_GET_INTERFACE d3d_D3D12GetInterface = (PFN_D3D12_GET_INTERFACE)(void *)GetProcAddress(lib_d3d12, "D3D12GetInterface");
	if (!d3d_D3D12GetInterface) {
		return OK; // Fallback to the system loader.
	}

	ID3D12SDKConfiguration *sdk_config = nullptr;
	if (SUCCEEDED(d3d_D3D12GetInterface(CLSID_D3D12SDKConfigurationGodot, IID_PPV_ARGS(&sdk_config)))) {
		ID3D12SDKConfiguration1 *sdk_config1 = nullptr;
		if (SUCCEEDED(sdk_config->QueryInterface(&sdk_config1))) {
			if (SUCCEEDED(sdk_config1->CreateDeviceFactory(agility_sdk_version, agility_sdk_path.ascii().get_data(), IID_PPV_ARGS(device_factory.GetAddressOf())))) {
				d3d_D3D12GetInterface(CLSID_D3D12DeviceFactoryGodot, IID_PPV_ARGS(device_factory.GetAddressOf()));
			} else if (SUCCEEDED(sdk_config1->CreateDeviceFactory(agility_sdk_version, ".\\", IID_PPV_ARGS(device_factory.GetAddressOf())))) {
				d3d_D3D12GetInterface(CLSID_D3D12DeviceFactoryGodot, IID_PPV_ARGS(device_factory.GetAddressOf()));
			}
			sdk_config1->Release();
		}
		sdk_config->Release();
	}
	return OK;
}

Error RenderingContextDriverD3D12::_initialize_debug_layers() {
	ComPtr<ID3D12Debug> debug_controller;
	HRESULT res;

	if (device_factory) {
		res = device_factory->GetConfigurationInterface(CLSID_D3D12DebugGodot, IID_PPV_ARGS(&debug_controller));
	} else {
		PFN_D3D12_GET_DEBUG_INTERFACE d3d_D3D12GetDebugInterface = (PFN_D3D12_GET_DEBUG_INTERFACE)(void *)GetProcAddress(lib_d3d12, "D3D12GetDebugInterface");
		ERR_FAIL_NULL_V(d3d_D3D12GetDebugInterface, ERR_CANT_CREATE);

		res = d3d_D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller));
	}

	ERR_FAIL_COND_V(!SUCCEEDED(res), ERR_QUERY_FAILED);
	debug_controller->EnableDebugLayer();
	return OK;
}

Error RenderingContextDriverD3D12::_initialize_devices() {
	const UINT dxgi_factory_flags = use_validation_layers() ? DXGI_CREATE_FACTORY_DEBUG : 0;

	typedef HRESULT(WINAPI * PFN_DXGI_CREATE_DXGI_FACTORY2)(UINT, REFIID, void **);
	PFN_DXGI_CREATE_DXGI_FACTORY2 dxgi_CreateDXGIFactory2 = (PFN_DXGI_CREATE_DXGI_FACTORY2)(void *)GetProcAddress(lib_dxgi, "CreateDXGIFactory2");
	ERR_FAIL_NULL_V(dxgi_CreateDXGIFactory2, ERR_CANT_CREATE);

	HRESULT res = dxgi_CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&dxgi_factory));
	ERR_FAIL_COND_V(!SUCCEEDED(res), ERR_CANT_CREATE);

	// Enumerate all possible adapters.
	LocalVector<IDXGIAdapter1 *> adapters;
	IDXGIAdapter1 *adapter = nullptr;
	do {
		adapter = create_adapter(adapters.size());
		if (adapter != nullptr) {
			adapters.push_back(adapter);
		}
	} while (adapter != nullptr);

	ERR_FAIL_COND_V_MSG(adapters.is_empty(), ERR_CANT_CREATE, "Adapters enumeration reported zero accessible devices.");

	// Fill the device descriptions with the adapters.
	driver_devices.resize(adapters.size());
	for (uint32_t i = 0; i < adapters.size(); ++i) {
		DXGI_ADAPTER_DESC1 desc = {};
		adapters[i]->GetDesc1(&desc);

		Device &device = driver_devices[i];
		device.name = desc.Description;
		device.vendor = desc.VendorId;
		device.workarounds = Workarounds();

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			device.type = DEVICE_TYPE_CPU;
		} else {
			const bool has_dedicated_vram = desc.DedicatedVideoMemory > 0;
			device.type = has_dedicated_vram ? DEVICE_TYPE_DISCRETE_GPU : DEVICE_TYPE_INTEGRATED_GPU;
		}
	}

	// Release all created adapters.
	for (uint32_t i = 0; i < adapters.size(); ++i) {
		adapters[i]->Release();
	}

	ComPtr<IDXGIFactory5> factory_5;
	dxgi_factory.As(&factory_5);
	if (factory_5 != nullptr) {
		// The type is important as in general, sizeof(bool) != sizeof(BOOL).
		BOOL feature_supported = FALSE;
		res = factory_5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &feature_supported, sizeof(feature_supported));
		if (SUCCEEDED(res)) {
			tearing_supported = feature_supported;
		} else {
			ERR_PRINT("CheckFeatureSupport failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");
		}
	}

	return OK;
}

bool RenderingContextDriverD3D12::use_validation_layers() const {
	return Engine::get_singleton()->is_validation_layers_enabled();
}

Error RenderingContextDriverD3D12::initialize() {
	Error err = _init_device_factory();
	ERR_FAIL_COND_V(err != OK, ERR_CANT_CREATE);

	if (use_validation_layers()) {
		err = _initialize_debug_layers();
		ERR_FAIL_COND_V(err != OK, ERR_CANT_CREATE);
	}

	err = _initialize_devices();
	ERR_FAIL_COND_V(err != OK, ERR_CANT_CREATE);

	return OK;
}

const RenderingContextDriver::Device &RenderingContextDriverD3D12::device_get(uint32_t p_device_index) const {
	DEV_ASSERT(p_device_index < driver_devices.size());
	return driver_devices[p_device_index];
}

uint32_t RenderingContextDriverD3D12::device_get_count() const {
	return driver_devices.size();
}

bool RenderingContextDriverD3D12::device_supports_present(uint32_t p_device_index, SurfaceID p_surface) const {
	// All devices should support presenting to any surface.
	return true;
}

RenderingDeviceDriver *RenderingContextDriverD3D12::driver_create() {
	return memnew(RenderingDeviceDriverD3D12(this));
}

void RenderingContextDriverD3D12::driver_free(RenderingDeviceDriver *p_driver) {
	memdelete(p_driver);
}

RenderingContextDriver::SurfaceID RenderingContextDriverD3D12::surface_create(const void *p_platform_data) {
	const WindowPlatformData *wpd = (const WindowPlatformData *)(p_platform_data);
	Surface *surface = memnew(Surface);
	surface->hwnd = wpd->window;
	return SurfaceID(surface);
}

void RenderingContextDriverD3D12::surface_set_size(SurfaceID p_surface, uint32_t p_width, uint32_t p_height) {
	Surface *surface = (Surface *)(p_surface);
	surface->width = p_width;
	surface->height = p_height;
	surface->needs_resize = true;
}

void RenderingContextDriverD3D12::surface_set_vsync_mode(SurfaceID p_surface, DisplayServer::VSyncMode p_vsync_mode) {
	Surface *surface = (Surface *)(p_surface);
	surface->vsync_mode = p_vsync_mode;
	surface->needs_resize = true;
}

DisplayServer::VSyncMode RenderingContextDriverD3D12::surface_get_vsync_mode(SurfaceID p_surface) const {
	Surface *surface = (Surface *)(p_surface);
	return surface->vsync_mode;
}

uint32_t RenderingContextDriverD3D12::surface_get_width(SurfaceID p_surface) const {
	Surface *surface = (Surface *)(p_surface);
	return surface->width;
}

uint32_t RenderingContextDriverD3D12::surface_get_height(SurfaceID p_surface) const {
	Surface *surface = (Surface *)(p_surface);
	return surface->height;
}

void RenderingContextDriverD3D12::surface_set_needs_resize(SurfaceID p_surface, bool p_needs_resize) {
	Surface *surface = (Surface *)(p_surface);
	surface->needs_resize = p_needs_resize;
}

bool RenderingContextDriverD3D12::surface_get_needs_resize(SurfaceID p_surface) const {
	Surface *surface = (Surface *)(p_surface);
	return surface->needs_resize;
}

void RenderingContextDriverD3D12::surface_destroy(SurfaceID p_surface) {
	Surface *surface = (Surface *)(p_surface);
	memdelete(surface);
}

bool RenderingContextDriverD3D12::is_debug_utils_enabled() const {
#ifdef PIX_ENABLED
	return true;
#else
	return false;
#endif
}

IDXGIAdapter1 *RenderingContextDriverD3D12::create_adapter(uint32_t p_adapter_index) const {
	ComPtr<IDXGIFactory6> factory_6;
	dxgi_factory.As(&factory_6);

	// TODO: Use IDXCoreAdapterList, which gives more comprehensive information.
	IDXGIAdapter1 *adapter = nullptr;
	if (factory_6) {
		if (factory_6->EnumAdapterByGpuPreference(p_adapter_index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)) == DXGI_ERROR_NOT_FOUND) {
			return nullptr;
		}
	} else {
		if (dxgi_factory->EnumAdapters1(p_adapter_index, &adapter) == DXGI_ERROR_NOT_FOUND) {
			return nullptr;
		}
	}

	return adapter;
}

ID3D12DeviceFactory *RenderingContextDriverD3D12::device_factory_get() const {
	return device_factory.Get();
}

IDXGIFactory2 *RenderingContextDriverD3D12::dxgi_factory_get() const {
	return dxgi_factory.Get();
}

bool RenderingContextDriverD3D12::get_tearing_supported() const {
	return tearing_supported;
}
