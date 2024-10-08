/**************************************************************************/
/*  fog_material.cpp                                                      */
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

#include "fog_material.h"

#include "core/version.h"

Mutex FogMaterial::shader_mutex;
RID FogMaterial::shader;

void FogMaterial::set_density(float p_density) {
	density = p_density;
	RS::get_singleton()->material_set_param(_get_material(), "density", density);
}

float FogMaterial::get_density() const {
	return density;
}

void FogMaterial::set_albedo(Color p_albedo) {
	albedo = p_albedo;
	RS::get_singleton()->material_set_param(_get_material(), "albedo", albedo);
}

Color FogMaterial::get_albedo() const {
	return albedo;
}

void FogMaterial::set_emission(Color p_emission) {
	emission = p_emission;
	RS::get_singleton()->material_set_param(_get_material(), "emission", emission);
}

Color FogMaterial::get_emission() const {
	return emission;
}

void FogMaterial::set_height_falloff(float p_falloff) {
	height_falloff = MAX(p_falloff, 0.0f);
	RS::get_singleton()->material_set_param(_get_material(), "height_falloff", height_falloff);
}

float FogMaterial::get_height_falloff() const {
	return height_falloff;
}

void FogMaterial::set_edge_fade(float p_edge_fade) {
	edge_fade = MAX(p_edge_fade, 0.0f);
	RS::get_singleton()->material_set_param(_get_material(), "edge_fade", edge_fade);
}

float FogMaterial::get_edge_fade() const {
	return edge_fade;
}

void FogMaterial::set_density_texture(const Ref<Texture3D> &p_texture) {
	density_texture = p_texture;
	Variant tex_rid = p_texture.is_valid() ? Variant(p_texture->get_rid()) : Variant();
	RS::get_singleton()->material_set_param(_get_material(), "density_texture", tex_rid);
}

Ref<Texture3D> FogMaterial::get_density_texture() const {
	return density_texture;
}

Shader::Mode FogMaterial::get_shader_mode() const {
	return Shader::MODE_FOG;
}

RID FogMaterial::get_shader_rid() const {
	_update_shader();
	return shader;
}

RID FogMaterial::get_rid() const {
	_update_shader();
	if (!shader_set) {
		RS::get_singleton()->material_set_shader(_get_material(), shader);
		shader_set = true;
	}
	return _get_material();
}

void FogMaterial::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_density", "density"), &FogMaterial::set_density);
	ClassDB::bind_method(D_METHOD("get_density"), &FogMaterial::get_density);
	ClassDB::bind_method(D_METHOD("set_albedo", "albedo"), &FogMaterial::set_albedo);
	ClassDB::bind_method(D_METHOD("get_albedo"), &FogMaterial::get_albedo);
	ClassDB::bind_method(D_METHOD("set_emission", "emission"), &FogMaterial::set_emission);
	ClassDB::bind_method(D_METHOD("get_emission"), &FogMaterial::get_emission);
	ClassDB::bind_method(D_METHOD("set_height_falloff", "height_falloff"), &FogMaterial::set_height_falloff);
	ClassDB::bind_method(D_METHOD("get_height_falloff"), &FogMaterial::get_height_falloff);
	ClassDB::bind_method(D_METHOD("set_edge_fade", "edge_fade"), &FogMaterial::set_edge_fade);
	ClassDB::bind_method(D_METHOD("get_edge_fade"), &FogMaterial::get_edge_fade);
	ClassDB::bind_method(D_METHOD("set_density_texture", "density_texture"), &FogMaterial::set_density_texture);
	ClassDB::bind_method(D_METHOD("get_density_texture"), &FogMaterial::get_density_texture);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "density", PROPERTY_HINT_RANGE, "-8.0,8.0,0.0001,or_greater,or_less"), "set_density", "get_density");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "albedo", PROPERTY_HINT_COLOR_NO_ALPHA), "set_albedo", "get_albedo");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "emission", PROPERTY_HINT_COLOR_NO_ALPHA), "set_emission", "get_emission");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height_falloff", PROPERTY_HINT_EXP_EASING, "attenuation"), "set_height_falloff", "get_height_falloff");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "edge_fade", PROPERTY_HINT_EXP_EASING), "set_edge_fade", "get_edge_fade");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "density_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture3D"), "set_density_texture", "get_density_texture");
}

void FogMaterial::cleanup_shader() {
	if (shader.is_valid()) {
		ERR_FAIL_NULL(RenderingServer::get_singleton());
		RS::get_singleton()->free(shader);
	}
}

void FogMaterial::_update_shader() {
	MutexLock shader_lock(shader_mutex);
	if (shader.is_null()) {
		shader = RS::get_singleton()->shader_create();

		// Add a comment to describe the shader origin (useful when converting to ShaderMaterial).
		RS::get_singleton()->shader_set_code(shader, R"(
// NOTE: Shader automatically converted from )" VERSION_NAME " " VERSION_FULL_CONFIG R"('s FogMaterial.

shader_type fog;

uniform float density : hint_range(0, 1, 0.0001) = 1.0;
uniform vec4 albedo : source_color = vec4(1.0);
uniform vec4 emission : source_color = vec4(0, 0, 0, 1);
uniform float height_falloff = 0.0;
uniform float edge_fade = 0.1;
uniform sampler3D density_texture: hint_default_white;


void fog() {
    DENSITY = density * clamp(exp2(-height_falloff * (WORLD_POSITION.y - OBJECT_POSITION.y)), 0.0, 1.0);
    DENSITY *= texture(density_texture, UVW).r;
    DENSITY *= pow(clamp(-2.0 * SDF / min(min(SIZE.x, SIZE.y), SIZE.z), 0.0, 1.0), edge_fade);
    ALBEDO = albedo.rgb;
    EMISSION = emission.rgb;
}
)");
	}
}

FogMaterial::FogMaterial() {
	_set_material(RS::get_singleton()->material_create());

	set_density(1.0);
	set_albedo(Color(1, 1, 1, 1));
	set_emission(Color(0, 0, 0, 1));

	set_height_falloff(0.0);
	set_edge_fade(0.1);
}

FogMaterial::~FogMaterial() {
	RS::get_singleton()->material_set_shader(_get_material(), RID());
}
