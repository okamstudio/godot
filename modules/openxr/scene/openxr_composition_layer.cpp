/**************************************************************************/
/*  openxr_composition_layer.cpp                                          */
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

#include "openxr_composition_layer.h"

#include "../extensions/openxr_composition_layer_extension.h"
#include "../openxr_api.h"
#include "../openxr_interface.h"

#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/xr_nodes.h"
#include "scene/main/viewport.h"

#include "platform/android/api/java_class_wrapper.h"

Vector<OpenXRCompositionLayer *> OpenXRCompositionLayer::composition_layer_nodes;

static const char *HOLE_PUNCH_SHADER_CODE =
		"shader_type spatial;\n"
		"render_mode blend_mix, depth_draw_opaque, cull_back, shadow_to_opacity, shadows_disabled;\n"
		"void fragment() {\n"
		"\tALBEDO = vec3(0.0, 0.0, 0.0);\n"
		"}\n";

OpenXRCompositionLayer::OpenXRCompositionLayer(XrCompositionLayerBaseHeader *p_composition_layer) {
	composition_layer_base_header = p_composition_layer;
	openxr_layer_provider = memnew(OpenXRViewportCompositionLayerProvider(composition_layer_base_header));
	swapchain_state = openxr_layer_provider->get_swapchain_state();

	openxr_api = OpenXRAPI::get_singleton();
	composition_layer_extension = OpenXRCompositionLayerExtension::get_singleton();

	if (openxr_api) {
		openxr_session_running = openxr_api->is_running();
	}

	Ref<OpenXRInterface> openxr_interface = XRServer::get_singleton()->find_interface("OpenXR");
	if (openxr_interface.is_valid()) {
		openxr_interface->connect("session_begun", callable_mp(this, &OpenXRCompositionLayer::_on_openxr_session_begun));
		openxr_interface->connect("session_stopping", callable_mp(this, &OpenXRCompositionLayer::_on_openxr_session_stopping));
	}

	set_process_internal(true);
	set_notify_local_transform(true);

	if (Engine::get_singleton()->is_editor_hint()) {
		// In the editor, create the fallback right away.
		_create_fallback_node();
	}
}

OpenXRCompositionLayer::~OpenXRCompositionLayer() {
	Ref<OpenXRInterface> openxr_interface = XRServer::get_singleton()->find_interface("OpenXR");
	if (openxr_interface.is_valid()) {
		openxr_interface->disconnect("session_begun", callable_mp(this, &OpenXRCompositionLayer::_on_openxr_session_begun));
		openxr_interface->disconnect("session_stopping", callable_mp(this, &OpenXRCompositionLayer::_on_openxr_session_stopping));
	}

	composition_layer_nodes.erase(this);

	if (openxr_layer_provider != nullptr) {
		_clear_composition_layer_provider();
		memdelete(openxr_layer_provider);
		openxr_layer_provider = nullptr;
	}
}

void OpenXRCompositionLayer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_layer_viewport", "viewport"), &OpenXRCompositionLayer::set_layer_viewport);
	ClassDB::bind_method(D_METHOD("get_layer_viewport"), &OpenXRCompositionLayer::get_layer_viewport);

	ClassDB::bind_method(D_METHOD("set_use_android_surface", "enable"), &OpenXRCompositionLayer::set_use_android_surface);
	ClassDB::bind_method(D_METHOD("get_use_android_surface"), &OpenXRCompositionLayer::get_use_android_surface);

	ClassDB::bind_method(D_METHOD("set_android_surface_size", "size"), &OpenXRCompositionLayer::set_android_surface_size);
	ClassDB::bind_method(D_METHOD("get_android_surface_size"), &OpenXRCompositionLayer::get_android_surface_size);

	ClassDB::bind_method(D_METHOD("set_enable_hole_punch", "enable"), &OpenXRCompositionLayer::set_enable_hole_punch);
	ClassDB::bind_method(D_METHOD("get_enable_hole_punch"), &OpenXRCompositionLayer::get_enable_hole_punch);

	ClassDB::bind_method(D_METHOD("set_sort_order", "order"), &OpenXRCompositionLayer::set_sort_order);
	ClassDB::bind_method(D_METHOD("get_sort_order"), &OpenXRCompositionLayer::get_sort_order);

	ClassDB::bind_method(D_METHOD("set_alpha_blend", "enabled"), &OpenXRCompositionLayer::set_alpha_blend);
	ClassDB::bind_method(D_METHOD("get_alpha_blend"), &OpenXRCompositionLayer::get_alpha_blend);

	ClassDB::bind_method(D_METHOD("get_android_surface"), &OpenXRCompositionLayer::get_android_surface);
	ClassDB::bind_method(D_METHOD("is_natively_supported"), &OpenXRCompositionLayer::is_natively_supported);

	ClassDB::bind_method(D_METHOD("set_min_filter", "mode"), &OpenXRCompositionLayer::set_min_filter);
	ClassDB::bind_method(D_METHOD("get_min_filter"), &OpenXRCompositionLayer::get_min_filter);

	ClassDB::bind_method(D_METHOD("set_mag_filter", "mode"), &OpenXRCompositionLayer::set_mag_filter);
	ClassDB::bind_method(D_METHOD("get_mag_filter"), &OpenXRCompositionLayer::get_mag_filter);

	ClassDB::bind_method(D_METHOD("set_mipmap_mode", "mode"), &OpenXRCompositionLayer::set_mipmap_mode);
	ClassDB::bind_method(D_METHOD("get_mipmap_mode"), &OpenXRCompositionLayer::get_mipmap_mode);

	ClassDB::bind_method(D_METHOD("set_horizontal_wrap", "mode"), &OpenXRCompositionLayer::set_horizontal_wrap);
	ClassDB::bind_method(D_METHOD("get_horizontal_wrap"), &OpenXRCompositionLayer::get_horizontal_wrap);

	ClassDB::bind_method(D_METHOD("set_vertical_wrap", "mode"), &OpenXRCompositionLayer::set_vertical_wrap);
	ClassDB::bind_method(D_METHOD("get_vertical_wrap"), &OpenXRCompositionLayer::get_vertical_wrap);

	ClassDB::bind_method(D_METHOD("set_red_swizzle", "mode"), &OpenXRCompositionLayer::set_red_swizzle);
	ClassDB::bind_method(D_METHOD("get_red_swizzle"), &OpenXRCompositionLayer::get_red_swizzle);

	ClassDB::bind_method(D_METHOD("set_green_swizzle", "mode"), &OpenXRCompositionLayer::set_green_swizzle);
	ClassDB::bind_method(D_METHOD("get_green_swizzle"), &OpenXRCompositionLayer::get_green_swizzle);

	ClassDB::bind_method(D_METHOD("set_blue_swizzle", "mode"), &OpenXRCompositionLayer::set_blue_swizzle);
	ClassDB::bind_method(D_METHOD("get_blue_swizzle"), &OpenXRCompositionLayer::get_blue_swizzle);

	ClassDB::bind_method(D_METHOD("set_alpha_swizzle", "mode"), &OpenXRCompositionLayer::set_alpha_swizzle);
	ClassDB::bind_method(D_METHOD("get_alpha_swizzle"), &OpenXRCompositionLayer::get_alpha_swizzle);

	ClassDB::bind_method(D_METHOD("set_max_anisotropy", "value"), &OpenXRCompositionLayer::set_max_anisotropy);
	ClassDB::bind_method(D_METHOD("get_max_anisotropy"), &OpenXRCompositionLayer::get_max_anisotropy);

	ClassDB::bind_method(D_METHOD("set_border_color", "color"), &OpenXRCompositionLayer::set_border_color);
	ClassDB::bind_method(D_METHOD("get_border_color"), &OpenXRCompositionLayer::get_border_color);

	ClassDB::bind_method(D_METHOD("intersects_ray", "origin", "direction"), &OpenXRCompositionLayer::intersects_ray);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "layer_viewport", PROPERTY_HINT_NODE_TYPE, "SubViewport"), "set_layer_viewport", "get_layer_viewport");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_android_surface", PROPERTY_HINT_NONE, ""), "set_use_android_surface", "get_use_android_surface");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "android_surface_size", PROPERTY_HINT_NONE, ""), "set_android_surface_size", "get_android_surface_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sort_order", PROPERTY_HINT_NONE, ""), "set_sort_order", "get_sort_order");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "alpha_blend", PROPERTY_HINT_NONE, ""), "set_alpha_blend", "get_alpha_blend");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enable_hole_punch", PROPERTY_HINT_NONE, ""), "set_enable_hole_punch", "get_enable_hole_punch");

	ADD_GROUP("Swapchain State", "swapchain_state_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "swapchain_state_min_filter", PROPERTY_HINT_ENUM, "Nearest,Linear,Cubic"), "set_min_filter", "get_min_filter");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "swapchain_state_mag_filter", PROPERTY_HINT_ENUM, "Nearest,Linear,Cubic"), "set_mag_filter", "get_mag_filter");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "swapchain_state_mipmap_mode", PROPERTY_HINT_ENUM, "Disabled,Nearest,Linear"), "set_mipmap_mode", "get_mipmap_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "swapchain_state_horizontal_wrap", PROPERTY_HINT_ENUM, "Clamp to Border,Clamp to Edge,Repeat,Mirrored Repeat,Mirror Clamp to Edge"), "set_horizontal_wrap", "get_horizontal_wrap");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "swapchain_state_vertical_wrap", PROPERTY_HINT_ENUM, "Clamp to Border,Clamp to Edge,Repeat,Mirrored Repeat,Mirror Clamp to Edge"), "set_vertical_wrap", "get_vertical_wrap");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "swapchain_state_red_swizzle", PROPERTY_HINT_ENUM, "Red,Green,Blue,Alpha,Zero,One"), "set_red_swizzle", "get_red_swizzle");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "swapchain_state_green_swizzle", PROPERTY_HINT_ENUM, "Red,Green,Blue,Alpha,Zero,One"), "set_green_swizzle", "get_green_swizzle");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "swapchain_state_blue_swizzle", PROPERTY_HINT_ENUM, "Red,Green,Blue,Alpha,Zero,One"), "set_blue_swizzle", "get_blue_swizzle");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "swapchain_state_alpha_swizzle", PROPERTY_HINT_ENUM, "Red,Green,Blue,Alpha,Zero,One"), "set_alpha_swizzle", "get_alpha_swizzle");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "swapchain_state_max_anisotropy", PROPERTY_HINT_RANGE, "1.0,16.0,0.001"), "set_max_anisotropy", "get_max_anisotropy");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "swapchain_state_border_color"), "set_border_color", "get_border_color");
	ADD_GROUP("", "");

	BIND_ENUM_CONSTANT(FILTER_NEAREST);
	BIND_ENUM_CONSTANT(FILTER_LINEAR);
	BIND_ENUM_CONSTANT(FILTER_CUBIC);

	BIND_ENUM_CONSTANT(MIPMAP_MODE_DISABLED);
	BIND_ENUM_CONSTANT(MIPMAP_MODE_NEAREST);
	BIND_ENUM_CONSTANT(MIPMAP_MODE_LINEAR);

	BIND_ENUM_CONSTANT(WRAP_CLAMP_TO_BORDER);
	BIND_ENUM_CONSTANT(WRAP_CLAMP_TO_EDGE);
	BIND_ENUM_CONSTANT(WRAP_REPEAT);
	BIND_ENUM_CONSTANT(WRAP_MIRRORED_REPEAT);
	BIND_ENUM_CONSTANT(WRAP_MIRROR_CLAMP_TO_EDGE);

	BIND_ENUM_CONSTANT(SWIZZLE_RED);
	BIND_ENUM_CONSTANT(SWIZZLE_GREEN);
	BIND_ENUM_CONSTANT(SWIZZLE_BLUE);
	BIND_ENUM_CONSTANT(SWIZZLE_ALPHA);
	BIND_ENUM_CONSTANT(SWIZZLE_ZERO);
	BIND_ENUM_CONSTANT(SWIZZLE_ONE);
}

bool OpenXRCompositionLayer::_should_use_fallback_node() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return true;
	} else if (openxr_session_running) {
		return enable_hole_punch || (!is_natively_supported() && !use_android_surface);
	}
	return false;
}

void OpenXRCompositionLayer::_create_fallback_node() {
	ERR_FAIL_COND(fallback);
	fallback = memnew(MeshInstance3D);
	fallback->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(fallback, false, INTERNAL_MODE_FRONT);
	should_update_fallback_mesh = true;
}

void OpenXRCompositionLayer::_remove_fallback_node() {
	ERR_FAIL_COND(fallback != nullptr);
	remove_child(fallback);
	fallback->queue_free();
	fallback = nullptr;
}

void OpenXRCompositionLayer::_setup_composition_layer_provider() {
	if (use_android_surface || layer_viewport) {
		if (composition_layer_extension) {
			composition_layer_extension->register_viewport_composition_layer_provider(openxr_layer_provider);
		}

		// NOTE: We don't setup/clear when using Android surfaces, so we don't destroy the surface unexpectedly.
		if (layer_viewport) {
			// Set our properties on the layer provider, which will create all the necessary resources (ex swap chains).
			openxr_layer_provider->set_viewport(layer_viewport->get_viewport_rid(), layer_viewport->get_size());
		}
	}
}

void OpenXRCompositionLayer::_clear_composition_layer_provider() {
	if (composition_layer_extension) {
		composition_layer_extension->unregister_viewport_composition_layer_provider(openxr_layer_provider);
	}

	// NOTE: We don't setup/clear when using Android surfaces, so we don't destroy the surface unexpectedly.
	if (!use_android_surface) {
		// This will reset the viewport and free all the resources (ex swap chains) used by the layer.
		openxr_layer_provider->set_viewport(RID(), Size2i());
	}
}

void OpenXRCompositionLayer::_on_openxr_session_begun() {
	openxr_session_running = true;
	if (is_natively_supported() && is_visible() && is_inside_tree()) {
		_setup_composition_layer_provider();
	}
	if (!fallback && _should_use_fallback_node()) {
		_create_fallback_node();
	}
}

void OpenXRCompositionLayer::_on_openxr_session_stopping() {
	openxr_session_running = false;
	if (fallback && !_should_use_fallback_node()) {
		_remove_fallback_node();
	}
	_clear_composition_layer_provider();
}

void OpenXRCompositionLayer::update_fallback_mesh() {
	should_update_fallback_mesh = true;
}

XrPosef OpenXRCompositionLayer::get_openxr_pose() {
	Transform3D reference_frame = XRServer::get_singleton()->get_reference_frame();
	Transform3D transform = reference_frame.inverse() * get_transform();
	Quaternion quat(transform.basis.orthonormalized());
	return {
		{ (float)quat.x, (float)quat.y, (float)quat.z, (float)quat.w },
		{ (float)transform.origin.x, (float)transform.origin.y, (float)transform.origin.z }
	};
}

bool OpenXRCompositionLayer::is_viewport_in_use(SubViewport *p_viewport) {
	ERR_FAIL_NULL_V(p_viewport, false);
	for (const OpenXRCompositionLayer *other_composition_layer : composition_layer_nodes) {
		if (other_composition_layer != this && other_composition_layer->is_inside_tree() && other_composition_layer->get_layer_viewport() == p_viewport) {
			return true;
		}
	}
	return false;
}

void OpenXRCompositionLayer::set_layer_viewport(SubViewport *p_viewport) {
	if (layer_viewport == p_viewport) {
		return;
	}

	if (p_viewport != nullptr) {
		ERR_FAIL_COND_EDMSG(is_viewport_in_use(p_viewport), RTR("Cannot use the same SubViewport with multiple OpenXR composition layers. Clear it from its current layer first."));
	}
	if (use_android_surface) {
		ERR_FAIL_COND_MSG(p_viewport != nullptr, RTR("Cannot set SubViewport on an OpenXR composition layer when using an Android surface."));
	}

	layer_viewport = p_viewport;

	if (layer_viewport) {
		SubViewport::UpdateMode update_mode = layer_viewport->get_update_mode();
		if (update_mode == SubViewport::UPDATE_WHEN_VISIBLE || update_mode == SubViewport::UPDATE_WHEN_PARENT_VISIBLE) {
			WARN_PRINT_ONCE("OpenXR composition layers cannot use SubViewports with UPDATE_WHEN_VISIBLE or UPDATE_WHEN_PARENT_VISIBLE. Switching to UPDATE_ALWAYS.");
			layer_viewport->set_update_mode(SubViewport::UPDATE_ALWAYS);
		}
	}

	if (fallback) {
		_reset_fallback_material();
	} else if (openxr_session_running && is_visible() && is_inside_tree()) {
		if (layer_viewport) {
			openxr_layer_provider->set_viewport(layer_viewport->get_viewport_rid(), layer_viewport->get_size());
		} else {
			openxr_layer_provider->set_viewport(RID(), Size2i());
		}
	}
}

void OpenXRCompositionLayer::set_use_android_surface(bool p_use_android_surface) {
	if (use_android_surface == p_use_android_surface) {
		return;
	}

	use_android_surface = p_use_android_surface;
	if (use_android_surface) {
		set_layer_viewport(nullptr);
		openxr_layer_provider->set_use_android_surface(true, android_surface_size);
	} else {
		openxr_layer_provider->set_use_android_surface(false, Size2i());
	}

	notify_property_list_changed();
}

bool OpenXRCompositionLayer::get_use_android_surface() const {
	return use_android_surface;
}

void OpenXRCompositionLayer::set_android_surface_size(Size2i p_size) {
	if (android_surface_size == p_size) {
		return;
	}

	android_surface_size = p_size;
	if (use_android_surface) {
		openxr_layer_provider->set_use_android_surface(true, android_surface_size);
	}
}

Size2i OpenXRCompositionLayer::get_android_surface_size() const {
	return android_surface_size;
}

SubViewport *OpenXRCompositionLayer::get_layer_viewport() const {
	return layer_viewport;
}

void OpenXRCompositionLayer::set_enable_hole_punch(bool p_enable) {
	if (enable_hole_punch == p_enable) {
		return;
	}

	enable_hole_punch = p_enable;
	if (_should_use_fallback_node()) {
		if (fallback) {
			_reset_fallback_material();
		} else {
			_create_fallback_node();
		}
	} else if (fallback) {
		_remove_fallback_node();
	}

	update_configuration_warnings();
}

bool OpenXRCompositionLayer::get_enable_hole_punch() const {
	return enable_hole_punch;
}

void OpenXRCompositionLayer::set_sort_order(int p_order) {
	openxr_layer_provider->set_sort_order(p_order);
	update_configuration_warnings();
}

int OpenXRCompositionLayer::get_sort_order() const {
	return openxr_layer_provider->get_sort_order();
}

void OpenXRCompositionLayer::set_alpha_blend(bool p_alpha_blend) {
	openxr_layer_provider->set_alpha_blend(p_alpha_blend);
	if (fallback) {
		_reset_fallback_material();
	}
}

bool OpenXRCompositionLayer::get_alpha_blend() const {
	return openxr_layer_provider->get_alpha_blend();
}

bool OpenXRCompositionLayer::is_natively_supported() const {
	if (composition_layer_extension) {
		return composition_layer_extension->is_available(openxr_layer_provider->get_openxr_type());
	}
	return false;
}

void OpenXRCompositionLayer::set_min_filter(Filter p_mode) {
	if (swapchain_state->min_filter == (OpenXRViewportCompositionLayerProvider::Filter)p_mode) {
		return;
	}

	swapchain_state->min_filter = (OpenXRViewportCompositionLayerProvider::Filter)p_mode;
	swapchain_state->dirty = true;
}

OpenXRCompositionLayer::Filter OpenXRCompositionLayer::get_min_filter() {
	return (OpenXRCompositionLayer::Filter)swapchain_state->min_filter;
}

void OpenXRCompositionLayer::set_mag_filter(Filter p_mode) {
	if (swapchain_state->mag_filter == (OpenXRViewportCompositionLayerProvider::Filter)p_mode) {
		return;
	}

	swapchain_state->mag_filter = (OpenXRViewportCompositionLayerProvider::Filter)p_mode;
	swapchain_state->dirty = true;
}

OpenXRCompositionLayer::Filter OpenXRCompositionLayer::get_mag_filter() {
	return (OpenXRCompositionLayer::Filter)swapchain_state->mag_filter;
}

void OpenXRCompositionLayer::set_mipmap_mode(MipmapMode p_mode) {
	if (swapchain_state->mipmap_mode == (OpenXRViewportCompositionLayerProvider::MipmapMode)p_mode) {
		return;
	}

	swapchain_state->mipmap_mode = (OpenXRViewportCompositionLayerProvider::MipmapMode)p_mode;
	swapchain_state->dirty = true;
}

OpenXRCompositionLayer::MipmapMode OpenXRCompositionLayer::get_mipmap_mode() {
	return (OpenXRCompositionLayer::MipmapMode)swapchain_state->mipmap_mode;
}

void OpenXRCompositionLayer::set_horizontal_wrap(Wrap p_mode) {
	if (swapchain_state->horizontal_wrap == (OpenXRViewportCompositionLayerProvider::Wrap)p_mode) {
		return;
	}

	swapchain_state->horizontal_wrap = (OpenXRViewportCompositionLayerProvider::Wrap)p_mode;
	swapchain_state->dirty = true;
}

OpenXRCompositionLayer::Wrap OpenXRCompositionLayer::get_horizontal_wrap() {
	return (OpenXRCompositionLayer::Wrap)swapchain_state->horizontal_wrap;
}

void OpenXRCompositionLayer::set_vertical_wrap(Wrap p_mode) {
	if (swapchain_state->vertical_wrap == (OpenXRViewportCompositionLayerProvider::Wrap)p_mode) {
		return;
	}

	swapchain_state->vertical_wrap = (OpenXRViewportCompositionLayerProvider::Wrap)p_mode;
	swapchain_state->dirty = true;
}

OpenXRCompositionLayer::Wrap OpenXRCompositionLayer::get_vertical_wrap() {
	return (OpenXRCompositionLayer::Wrap)swapchain_state->vertical_wrap;
}

void OpenXRCompositionLayer::set_red_swizzle(Swizzle p_mode) {
	if (swapchain_state->red_swizzle == (OpenXRViewportCompositionLayerProvider::Swizzle)p_mode) {
		return;
	}

	swapchain_state->red_swizzle = (OpenXRViewportCompositionLayerProvider::Swizzle)p_mode;
	swapchain_state->dirty = true;
}

OpenXRCompositionLayer::Swizzle OpenXRCompositionLayer::get_red_swizzle() {
	return (OpenXRCompositionLayer::Swizzle)swapchain_state->red_swizzle;
}

void OpenXRCompositionLayer::set_green_swizzle(Swizzle p_mode) {
	if (swapchain_state->green_swizzle == (OpenXRViewportCompositionLayerProvider::Swizzle)p_mode) {
		return;
	}

	swapchain_state->green_swizzle = (OpenXRViewportCompositionLayerProvider::Swizzle)p_mode;
	swapchain_state->dirty = true;
}

OpenXRCompositionLayer::Swizzle OpenXRCompositionLayer::get_green_swizzle() {
	return (OpenXRCompositionLayer::Swizzle)swapchain_state->green_swizzle;
}

void OpenXRCompositionLayer::set_blue_swizzle(Swizzle p_mode) {
	if (swapchain_state->blue_swizzle == (OpenXRViewportCompositionLayerProvider::Swizzle)p_mode) {
		return;
	}

	swapchain_state->blue_swizzle = (OpenXRViewportCompositionLayerProvider::Swizzle)p_mode;
	swapchain_state->dirty = true;
}

OpenXRCompositionLayer::Swizzle OpenXRCompositionLayer::get_blue_swizzle() {
	return (OpenXRCompositionLayer::Swizzle)swapchain_state->blue_swizzle;
}

void OpenXRCompositionLayer::set_alpha_swizzle(Swizzle p_mode) {
	if (swapchain_state->alpha_swizzle == (OpenXRViewportCompositionLayerProvider::Swizzle)p_mode) {
		return;
	}

	swapchain_state->alpha_swizzle = (OpenXRViewportCompositionLayerProvider::Swizzle)p_mode;
	swapchain_state->dirty = true;
}

OpenXRCompositionLayer::Swizzle OpenXRCompositionLayer::get_alpha_swizzle() {
	return (OpenXRCompositionLayer::Swizzle)swapchain_state->alpha_swizzle;
}

void OpenXRCompositionLayer::set_max_anisotropy(float p_value) {
	if (swapchain_state->max_anisotropy == p_value) {
		return;
	}

	swapchain_state->max_anisotropy = p_value;
	swapchain_state->dirty = true;
}

float OpenXRCompositionLayer::get_max_anisotropy() {
	return swapchain_state->max_anisotropy;
}

void OpenXRCompositionLayer::set_border_color(Color p_color) {
	if (swapchain_state->border_color == p_color) {
		return;
	}

	swapchain_state->border_color = p_color;
	swapchain_state->dirty = true;
}

Color OpenXRCompositionLayer::get_border_color() {
	return swapchain_state->border_color;
}

Ref<JavaObject> OpenXRCompositionLayer::get_android_surface() {
	return openxr_layer_provider->get_android_surface();
}

Vector2 OpenXRCompositionLayer::intersects_ray(const Vector3 &p_origin, const Vector3 &p_direction) const {
	return Vector2(-1.0, -1.0);
}

void OpenXRCompositionLayer::_reset_fallback_material() {
	ERR_FAIL_NULL(fallback);

	if (fallback->get_mesh().is_null()) {
		return;
	}

	if (enable_hole_punch && !Engine::get_singleton()->is_editor_hint() && is_natively_supported()) {
		Ref<ShaderMaterial> material = fallback->get_surface_override_material(0);
		if (material.is_null()) {
			Ref<Shader> shader;
			shader.instantiate();
			shader->set_code(HOLE_PUNCH_SHADER_CODE);

			material.instantiate();
			material->set_shader(shader);

			fallback->set_surface_override_material(0, material);
		}
	} else if (layer_viewport) {
		Ref<StandardMaterial3D> material = fallback->get_surface_override_material(0);
		if (material.is_null()) {
			material.instantiate();
			material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
			material->set_local_to_scene(true);
			fallback->set_surface_override_material(0, material);
		}

		material->set_flag(StandardMaterial3D::FLAG_DISABLE_DEPTH_TEST, !enable_hole_punch);
		material->set_transparency(get_alpha_blend() ? StandardMaterial3D::TRANSPARENCY_ALPHA : StandardMaterial3D::TRANSPARENCY_DISABLED);
		material->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, layer_viewport->get_texture());
	} else {
		fallback->set_surface_override_material(0, Ref<Material>());
	}
}

void OpenXRCompositionLayer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POSTINITIALIZE: {
			composition_layer_nodes.push_back(this);

			for (OpenXRExtensionWrapper *extension : OpenXRAPI::get_registered_extension_wrappers()) {
				extension_property_values.merge(extension->get_viewport_composition_layer_extension_property_defaults());
			}
			openxr_layer_provider->set_extension_property_values(extension_property_values);
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			if (fallback) {
				if (should_update_fallback_mesh) {
					fallback->set_mesh(_create_fallback_mesh());
					_reset_fallback_material();
					should_update_fallback_mesh = false;
				}
			}
		} break;
		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (is_natively_supported() && openxr_session_running && is_inside_tree()) {
				if (is_visible()) {
					_setup_composition_layer_provider();
				} else {
					_clear_composition_layer_provider();
				}
			}
			update_configuration_warnings();
		} break;
		case NOTIFICATION_LOCAL_TRANSFORM_CHANGED: {
			update_configuration_warnings();
		} break;
		case NOTIFICATION_ENTER_TREE: {
			if (layer_viewport && is_viewport_in_use(layer_viewport)) {
				_clear_composition_layer_provider();
			} else if (openxr_session_running && is_visible()) {
				_setup_composition_layer_provider();
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			// This will clean up existing resources.
			_clear_composition_layer_provider();
		} break;
	}
}

void OpenXRCompositionLayer::_get_property_list(List<PropertyInfo> *p_property_list) const {
	List<PropertyInfo> extension_properties;
	for (OpenXRExtensionWrapper *extension : OpenXRAPI::get_registered_extension_wrappers()) {
		extension->get_viewport_composition_layer_extension_properties(&extension_properties);
	}

	for (const PropertyInfo &pinfo : extension_properties) {
		StringName prop_name = pinfo.name;
		if (!String(prop_name).contains_char('/')) {
			WARN_PRINT_ONCE(vformat("Discarding OpenXRCompositionLayer property name '%s' from extension because it doesn't contain a '/'."));
			continue;
		}
		p_property_list->push_back(pinfo);
	}
}

bool OpenXRCompositionLayer::_get(const StringName &p_property, Variant &r_value) const {
	if (extension_property_values.has(p_property)) {
		r_value = extension_property_values[p_property];
	}

	return true;
}

bool OpenXRCompositionLayer::_set(const StringName &p_property, const Variant &p_value) {
	extension_property_values[p_property] = p_value;

	openxr_layer_provider->set_extension_property_values(extension_property_values);

	return true;
}

void OpenXRCompositionLayer::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "layer_viewport") {
		if (use_android_surface) {
			p_property.usage &= ~PROPERTY_USAGE_EDITOR;
		} else {
			p_property.usage |= PROPERTY_USAGE_EDITOR;
		}
	} else if (p_property.name == "android_surface_size") {
		if (use_android_surface) {
			p_property.usage |= PROPERTY_USAGE_EDITOR;
		} else {
			p_property.usage &= ~PROPERTY_USAGE_EDITOR;
		}
	}
}

PackedStringArray OpenXRCompositionLayer::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();

	if (is_visible() && is_inside_tree()) {
		XROrigin3D *origin = Object::cast_to<XROrigin3D>(get_parent());
		if (origin == nullptr) {
			warnings.push_back(RTR("OpenXR composition layers must have an XROrigin3D node as their parent."));
		}
	}

	if (!get_transform().basis.is_orthonormal()) {
		warnings.push_back(RTR("OpenXR composition layers must have orthonormalized transforms (ie. no scale or shearing)."));
	}

	if (enable_hole_punch && get_sort_order() >= 0) {
		warnings.push_back(RTR("Hole punching won't work as expected unless the sort order is less than zero."));
	}

	return warnings;
}
