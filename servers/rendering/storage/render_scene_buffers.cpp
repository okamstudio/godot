/**************************************************************************/
/*  render_scene_buffers.cpp                                              */
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

#include "render_scene_buffers.h"

void RenderSceneBuffersConfiguration::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_render_target"), &RenderSceneBuffersConfiguration::get_render_target);
	ClassDB::bind_method(D_METHOD("set_render_target", "render_target"), &RenderSceneBuffersConfiguration::set_render_target);
	ADD_PROPERTY(PropertyInfo(Variant::RID, "render_target"), "set_render_target", "get_render_target");

	ClassDB::bind_method(D_METHOD("get_internal_size"), &RenderSceneBuffersConfiguration::get_internal_size);
	ClassDB::bind_method(D_METHOD("set_internal_size", "internal_size"), &RenderSceneBuffersConfiguration::set_internal_size);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "internal_size"), "set_internal_size", "get_internal_size");

	ClassDB::bind_method(D_METHOD("get_target_size"), &RenderSceneBuffersConfiguration::get_target_size);
	ClassDB::bind_method(D_METHOD("set_target_size", "target_size"), &RenderSceneBuffersConfiguration::set_target_size);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "target_size"), "set_target_size", "get_target_size");

	ClassDB::bind_method(D_METHOD("get_view_count"), &RenderSceneBuffersConfiguration::get_view_count);
	ClassDB::bind_method(D_METHOD("set_view_count", "view_count"), &RenderSceneBuffersConfiguration::set_view_count);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "view_count"), "set_view_count", "get_view_count");

	ClassDB::bind_method(D_METHOD("get_scaling_3d_mode"), &RenderSceneBuffersConfiguration::get_scaling_3d_mode);
	ClassDB::bind_method(D_METHOD("set_scaling_3d_mode", "scaling_3d_mode"), &RenderSceneBuffersConfiguration::set_scaling_3d_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "scaling_3d_mode", PROPERTY_HINT_ENUM, "Bilinear (Fastest),FSR 1.0 (Fast),FSR 2.2 (Slow)"), "set_scaling_3d_mode", "get_scaling_3d_mode"); // TODO VIEWPORT_SCALING_3D_MODE_OFF is possible here too, but we can't specify an enum string for it.

	ClassDB::bind_method(D_METHOD("get_msaa_3d"), &RenderSceneBuffersConfiguration::get_msaa_3d);
	ClassDB::bind_method(D_METHOD("set_msaa_3d", "msaa_3d"), &RenderSceneBuffersConfiguration::set_msaa_3d);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "msaa_3d", PROPERTY_HINT_ENUM, "Disabled,2x,4x,8x"), "set_msaa_3d", "get_msaa_3d");

	ClassDB::bind_method(D_METHOD("get_screen_space_aa"), &RenderSceneBuffersConfiguration::get_screen_space_aa);
	ClassDB::bind_method(D_METHOD("set_screen_space_aa", "screen_space_aa"), &RenderSceneBuffersConfiguration::set_screen_space_aa);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "screen_space_aa", PROPERTY_HINT_ENUM, "Disabled,FXAA"), "set_screen_space_aa", "get_screen_space_aa");

	ClassDB::bind_method(D_METHOD("get_fsr_sharpness"), &RenderSceneBuffersConfiguration::get_fsr_sharpness);
	ClassDB::bind_method(D_METHOD("set_fsr_sharpness", "fsr_sharpness"), &RenderSceneBuffersConfiguration::set_fsr_sharpness);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "fsr_sharpness"), "set_fsr_sharpness", "get_fsr_sharpness");

	ClassDB::bind_method(D_METHOD("get_texture_mipmap_bias"), &RenderSceneBuffersConfiguration::get_texture_mipmap_bias);
	ClassDB::bind_method(D_METHOD("set_texture_mipmap_bias", "texture_mipmap_bias"), &RenderSceneBuffersConfiguration::set_texture_mipmap_bias);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "texture_mipmap_bias"), "set_texture_mipmap_bias", "get_texture_mipmap_bias");

	ClassDB::bind_method(D_METHOD("get_anisotropic_filtering_level"), &RenderSceneBuffersConfiguration::get_anisotropic_filtering_level);
	ClassDB::bind_method(D_METHOD("set_anisotropic_filtering_level", "anisotropic_filtering_level"), &RenderSceneBuffersConfiguration::set_anisotropic_filtering_level);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "anisotropic_filtering_level"), "set_anisotropic_filtering_level", "get_anisotropic_filtering_level");
}

void RenderSceneBuffers::_bind_methods() {
	ClassDB::bind_method(D_METHOD("configure", "config"), &RenderSceneBuffers::configure);
}

void RenderSceneBuffersExtension::_bind_methods() {
	GDVIRTUAL_BIND(_configure, "config");
	GDVIRTUAL_BIND(_set_fsr_sharpness, "fsr_sharpness");
	GDVIRTUAL_BIND(_set_texture_mipmap_bias, "texture_mipmap_bias");
	GDVIRTUAL_BIND(_set_anisotropic_filtering_level, "anisotropic_filtering_level");
	GDVIRTUAL_BIND(_set_use_debanding, "use_debanding");
}

void RenderSceneBuffersExtension::configure(const RenderSceneBuffersConfiguration *p_config) {
	GDVIRTUAL_CALL(_configure, p_config);
}

void RenderSceneBuffersExtension::set_fsr_sharpness(float p_fsr_sharpness) {
	GDVIRTUAL_CALL(_set_fsr_sharpness, p_fsr_sharpness);
}

void RenderSceneBuffersExtension::set_texture_mipmap_bias(float p_texture_mipmap_bias) {
	GDVIRTUAL_CALL(_set_texture_mipmap_bias, p_texture_mipmap_bias);
}

void RenderSceneBuffersExtension::set_anisotropic_filtering_level(RS::ViewportAnisotropicFiltering p_anisotropic_filtering_level) {
	GDVIRTUAL_CALL(_set_anisotropic_filtering_level, p_anisotropic_filtering_level);
}

void RenderSceneBuffersExtension::set_use_debanding(bool p_use_debanding) {
	GDVIRTUAL_CALL(_set_use_debanding, p_use_debanding);
}
