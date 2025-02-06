/**************************************************************************/
/*  openxr_composition_layer.h                                            */
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

#ifndef OPENXR_COMPOSITION_LAYER_H
#define OPENXR_COMPOSITION_LAYER_H

#include <openxr/openxr.h>

#include "scene/3d/node_3d.h"

class JavaObject;
class MeshInstance3D;
class Mesh;
class OpenXRAPI;
class OpenXRCompositionLayerExtension;
class OpenXRViewportCompositionLayerProvider;
class SubViewport;
struct SwapchainState;

class OpenXRCompositionLayer : public Node3D {
	GDCLASS(OpenXRCompositionLayer, Node3D);

public:
	// Must be identical to Filter enum definition in OpenXRViewportCompositionLayerProvider.
	enum Filter {
		FILTER_NEAREST,
		FILTER_LINEAR,
		FILTER_CUBIC,
	};

	// Must be identical to MipmapMode enum definition in OpenXRViewportCompositionLayerProvider.
	enum MipmapMode {
		MIPMAP_MODE_DISABLED,
		MIPMAP_MODE_NEAREST,
		MIPMAP_MODE_LINEAR,
	};

	// Must be identical to Wrap enum definition in OpenXRViewportCompositionLayerProvider.
	enum Wrap {
		WRAP_CLAMP_TO_BORDER,
		WRAP_CLAMP_TO_EDGE,
		WRAP_REPEAT,
		WRAP_MIRRORED_REPEAT,
		WRAP_MIRROR_CLAMP_TO_EDGE,
	};

	// Must be identical to Swizzle enum definition in OpenXRViewportCompositionLayerProvider.
	enum Swizzle {
		SWIZZLE_RED,
		SWIZZLE_GREEN,
		SWIZZLE_BLUE,
		SWIZZLE_ALPHA,
		SWIZZLE_ZERO,
		SWIZZLE_ONE,
	};

private:
	XrCompositionLayerBaseHeader *composition_layer_base_header = nullptr;
	OpenXRViewportCompositionLayerProvider *openxr_layer_provider = nullptr;

	SubViewport *layer_viewport = nullptr;
	bool use_android_surface = false;
	Size2i android_surface_size = Size2i(1024, 1024);
	bool enable_hole_punch = false;
	MeshInstance3D *fallback = nullptr;
	bool should_update_fallback_mesh = false;
	bool openxr_session_running = false;

	SwapchainState *swapchain_state;

	Dictionary extension_property_values;

	bool _should_use_fallback_node();
	void _create_fallback_node();
	void _reset_fallback_material();
	void _remove_fallback_node();

	void _setup_composition_layer_provider();
	void _clear_composition_layer_provider();

protected:
	OpenXRAPI *openxr_api = nullptr;
	OpenXRCompositionLayerExtension *composition_layer_extension = nullptr;

	static void _bind_methods();

	void _notification(int p_what);
	void _get_property_list(List<PropertyInfo> *p_property_list) const;
	bool _get(const StringName &p_property, Variant &r_value) const;
	bool _set(const StringName &p_property, const Variant &p_value);
	void _validate_property(PropertyInfo &p_property) const;

	virtual void _on_openxr_session_begun();
	virtual void _on_openxr_session_stopping();

	virtual Ref<Mesh> _create_fallback_mesh() = 0;

	void update_fallback_mesh();

	XrPosef get_openxr_pose();

	static Vector<OpenXRCompositionLayer *> composition_layer_nodes;
	bool is_viewport_in_use(SubViewport *p_viewport);

	OpenXRCompositionLayer(XrCompositionLayerBaseHeader *p_composition_layer);

public:
	void set_layer_viewport(SubViewport *p_viewport);
	SubViewport *get_layer_viewport() const;

	void set_use_android_surface(bool p_use_android_surface);
	bool get_use_android_surface() const;

	void set_android_surface_size(Size2i p_size);
	Size2i get_android_surface_size() const;

	void set_enable_hole_punch(bool p_enable);
	bool get_enable_hole_punch() const;

	void set_sort_order(int p_order);
	int get_sort_order() const;

	void set_alpha_blend(bool p_alpha_blend);
	bool get_alpha_blend() const;

	Ref<JavaObject> get_android_surface();
	bool is_natively_supported() const;

	void set_min_filter(Filter p_mode);
	Filter get_min_filter();

	void set_mag_filter(Filter p_mode);
	Filter get_mag_filter();

	void set_mipmap_mode(MipmapMode p_mode);
	MipmapMode get_mipmap_mode();

	void set_horizontal_wrap(Wrap p_mode);
	Wrap get_horizontal_wrap();

	void set_vertical_wrap(Wrap p_mode);
	Wrap get_vertical_wrap();

	void set_red_swizzle(Swizzle p_mode);
	Swizzle get_red_swizzle();

	void set_green_swizzle(Swizzle p_mode);
	Swizzle get_green_swizzle();

	void set_blue_swizzle(Swizzle p_mode);
	Swizzle get_blue_swizzle();

	void set_alpha_swizzle(Swizzle p_mode);
	Swizzle get_alpha_swizzle();

	void set_max_anisotropy(float p_value);
	float get_max_anisotropy();

	void set_border_color(Color p_color);
	Color get_border_color();

	virtual PackedStringArray get_configuration_warnings() const override;

	virtual Vector2 intersects_ray(const Vector3 &p_origin, const Vector3 &p_direction) const;

	~OpenXRCompositionLayer();
};

VARIANT_ENUM_CAST(OpenXRCompositionLayer::Filter)
VARIANT_ENUM_CAST(OpenXRCompositionLayer::MipmapMode)
VARIANT_ENUM_CAST(OpenXRCompositionLayer::Wrap)
VARIANT_ENUM_CAST(OpenXRCompositionLayer::Swizzle)

#endif // OPENXR_COMPOSITION_LAYER_H
