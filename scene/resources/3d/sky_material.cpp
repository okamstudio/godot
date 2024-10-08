/**************************************************************************/
/*  sky_material.cpp                                                      */
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

#include "sky_material.h"

#include "core/config/project_settings.h"
#include "core/version.h"

Mutex ProceduralSkyMaterial::shader_mutex;
RID ProceduralSkyMaterial::shader_cache[2];

void ProceduralSkyMaterial::set_sky_top_color(const Color &p_sky_top) {
	sky_top_color = p_sky_top;
	RS::get_singleton()->material_set_param(_get_material(), "sky_top_color", sky_top_color);
}

Color ProceduralSkyMaterial::get_sky_top_color() const {
	return sky_top_color;
}

void ProceduralSkyMaterial::set_sky_horizon_color(const Color &p_sky_horizon) {
	sky_horizon_color = p_sky_horizon;
	RS::get_singleton()->material_set_param(_get_material(), "sky_horizon_color", sky_horizon_color);
}

Color ProceduralSkyMaterial::get_sky_horizon_color() const {
	return sky_horizon_color;
}

void ProceduralSkyMaterial::set_sky_curve(float p_curve) {
	sky_curve = p_curve;
	RS::get_singleton()->material_set_param(_get_material(), "sky_curve", sky_curve);
}

float ProceduralSkyMaterial::get_sky_curve() const {
	return sky_curve;
}

void ProceduralSkyMaterial::set_sky_energy_multiplier(float p_multiplier) {
	sky_energy_multiplier = p_multiplier;
	RS::get_singleton()->material_set_param(_get_material(), "sky_energy", sky_energy_multiplier);
}

float ProceduralSkyMaterial::get_sky_energy_multiplier() const {
	return sky_energy_multiplier;
}

void ProceduralSkyMaterial::set_sky_cover(const Ref<Texture2D> &p_sky_cover) {
	sky_cover = p_sky_cover;
	if (p_sky_cover.is_valid()) {
		RS::get_singleton()->material_set_param(_get_material(), "sky_cover", p_sky_cover->get_rid());
	} else {
		RS::get_singleton()->material_set_param(_get_material(), "sky_cover", Variant());
	}
}

Ref<Texture2D> ProceduralSkyMaterial::get_sky_cover() const {
	return sky_cover;
}

void ProceduralSkyMaterial::set_sky_cover_modulate(const Color &p_sky_cover_modulate) {
	sky_cover_modulate = p_sky_cover_modulate;
	RS::get_singleton()->material_set_param(_get_material(), "sky_cover_modulate", sky_cover_modulate);
}

Color ProceduralSkyMaterial::get_sky_cover_modulate() const {
	return sky_cover_modulate;
}

void ProceduralSkyMaterial::set_ground_bottom_color(const Color &p_ground_bottom) {
	ground_bottom_color = p_ground_bottom;
	RS::get_singleton()->material_set_param(_get_material(), "ground_bottom_color", ground_bottom_color);
}

Color ProceduralSkyMaterial::get_ground_bottom_color() const {
	return ground_bottom_color;
}

void ProceduralSkyMaterial::set_ground_horizon_color(const Color &p_ground_horizon) {
	ground_horizon_color = p_ground_horizon;
	RS::get_singleton()->material_set_param(_get_material(), "ground_horizon_color", ground_horizon_color);
}

Color ProceduralSkyMaterial::get_ground_horizon_color() const {
	return ground_horizon_color;
}

void ProceduralSkyMaterial::set_ground_curve(float p_curve) {
	ground_curve = p_curve;
	RS::get_singleton()->material_set_param(_get_material(), "ground_curve", ground_curve);
}

float ProceduralSkyMaterial::get_ground_curve() const {
	return ground_curve;
}

void ProceduralSkyMaterial::set_ground_energy_multiplier(float p_multiplier) {
	ground_energy_multiplier = p_multiplier;
	RS::get_singleton()->material_set_param(_get_material(), "ground_energy", ground_energy_multiplier);
}

float ProceduralSkyMaterial::get_ground_energy_multiplier() const {
	return ground_energy_multiplier;
}

void ProceduralSkyMaterial::set_sun_angle_max(float p_angle) {
	sun_angle_max = p_angle;
	RS::get_singleton()->material_set_param(_get_material(), "sun_angle_max", Math::deg_to_rad(sun_angle_max));
}

float ProceduralSkyMaterial::get_sun_angle_max() const {
	return sun_angle_max;
}

void ProceduralSkyMaterial::set_sun_curve(float p_curve) {
	sun_curve = p_curve;
	RS::get_singleton()->material_set_param(_get_material(), "sun_curve", sun_curve);
}

float ProceduralSkyMaterial::get_sun_curve() const {
	return sun_curve;
}

void ProceduralSkyMaterial::set_use_debanding(bool p_use_debanding) {
	use_debanding = p_use_debanding;
	_update_shader();
	// Only set if shader already compiled
	if (shader_set) {
		RS::get_singleton()->material_set_shader(_get_material(), shader_cache[int(use_debanding)]);
	}
}

bool ProceduralSkyMaterial::get_use_debanding() const {
	return use_debanding;
}

void ProceduralSkyMaterial::set_energy_multiplier(float p_multiplier) {
	global_energy_multiplier = p_multiplier;
	RS::get_singleton()->material_set_param(_get_material(), "exposure", global_energy_multiplier);
}

float ProceduralSkyMaterial::get_energy_multiplier() const {
	return global_energy_multiplier;
}

Shader::Mode ProceduralSkyMaterial::get_shader_mode() const {
	return Shader::MODE_SKY;
}

RID ProceduralSkyMaterial::get_rid() const {
	_update_shader();
	if (!shader_set) {
		RS::get_singleton()->material_set_shader(_get_material(), shader_cache[1 - int(use_debanding)]);
		RS::get_singleton()->material_set_shader(_get_material(), shader_cache[int(use_debanding)]);
		shader_set = true;
	}
	return _get_material();
}

RID ProceduralSkyMaterial::get_shader_rid() const {
	_update_shader();
	return shader_cache[int(use_debanding)];
}

void ProceduralSkyMaterial::_validate_property(PropertyInfo &p_property) const {
	if ((p_property.name == "sky_luminance" || p_property.name == "ground_luminance") && !GLOBAL_GET("rendering/lights_and_shadows/use_physical_light_units")) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

void ProceduralSkyMaterial::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_sky_top_color", "color"), &ProceduralSkyMaterial::set_sky_top_color);
	ClassDB::bind_method(D_METHOD("get_sky_top_color"), &ProceduralSkyMaterial::get_sky_top_color);

	ClassDB::bind_method(D_METHOD("set_sky_horizon_color", "color"), &ProceduralSkyMaterial::set_sky_horizon_color);
	ClassDB::bind_method(D_METHOD("get_sky_horizon_color"), &ProceduralSkyMaterial::get_sky_horizon_color);

	ClassDB::bind_method(D_METHOD("set_sky_curve", "curve"), &ProceduralSkyMaterial::set_sky_curve);
	ClassDB::bind_method(D_METHOD("get_sky_curve"), &ProceduralSkyMaterial::get_sky_curve);

	ClassDB::bind_method(D_METHOD("set_sky_energy_multiplier", "multiplier"), &ProceduralSkyMaterial::set_sky_energy_multiplier);
	ClassDB::bind_method(D_METHOD("get_sky_energy_multiplier"), &ProceduralSkyMaterial::get_sky_energy_multiplier);

	ClassDB::bind_method(D_METHOD("set_sky_cover", "sky_cover"), &ProceduralSkyMaterial::set_sky_cover);
	ClassDB::bind_method(D_METHOD("get_sky_cover"), &ProceduralSkyMaterial::get_sky_cover);

	ClassDB::bind_method(D_METHOD("set_sky_cover_modulate", "color"), &ProceduralSkyMaterial::set_sky_cover_modulate);
	ClassDB::bind_method(D_METHOD("get_sky_cover_modulate"), &ProceduralSkyMaterial::get_sky_cover_modulate);

	ClassDB::bind_method(D_METHOD("set_ground_bottom_color", "color"), &ProceduralSkyMaterial::set_ground_bottom_color);
	ClassDB::bind_method(D_METHOD("get_ground_bottom_color"), &ProceduralSkyMaterial::get_ground_bottom_color);

	ClassDB::bind_method(D_METHOD("set_ground_horizon_color", "color"), &ProceduralSkyMaterial::set_ground_horizon_color);
	ClassDB::bind_method(D_METHOD("get_ground_horizon_color"), &ProceduralSkyMaterial::get_ground_horizon_color);

	ClassDB::bind_method(D_METHOD("set_ground_curve", "curve"), &ProceduralSkyMaterial::set_ground_curve);
	ClassDB::bind_method(D_METHOD("get_ground_curve"), &ProceduralSkyMaterial::get_ground_curve);

	ClassDB::bind_method(D_METHOD("set_ground_energy_multiplier", "energy"), &ProceduralSkyMaterial::set_ground_energy_multiplier);
	ClassDB::bind_method(D_METHOD("get_ground_energy_multiplier"), &ProceduralSkyMaterial::get_ground_energy_multiplier);

	ClassDB::bind_method(D_METHOD("set_sun_angle_max", "degrees"), &ProceduralSkyMaterial::set_sun_angle_max);
	ClassDB::bind_method(D_METHOD("get_sun_angle_max"), &ProceduralSkyMaterial::get_sun_angle_max);

	ClassDB::bind_method(D_METHOD("set_sun_curve", "curve"), &ProceduralSkyMaterial::set_sun_curve);
	ClassDB::bind_method(D_METHOD("get_sun_curve"), &ProceduralSkyMaterial::get_sun_curve);

	ClassDB::bind_method(D_METHOD("set_use_debanding", "use_debanding"), &ProceduralSkyMaterial::set_use_debanding);
	ClassDB::bind_method(D_METHOD("get_use_debanding"), &ProceduralSkyMaterial::get_use_debanding);

	ClassDB::bind_method(D_METHOD("set_energy_multiplier", "multiplier"), &ProceduralSkyMaterial::set_energy_multiplier);
	ClassDB::bind_method(D_METHOD("get_energy_multiplier"), &ProceduralSkyMaterial::get_energy_multiplier);

	ADD_GROUP("Sky", "sky_");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "sky_top_color", PROPERTY_HINT_COLOR_NO_ALPHA), "set_sky_top_color", "get_sky_top_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "sky_horizon_color", PROPERTY_HINT_COLOR_NO_ALPHA), "set_sky_horizon_color", "get_sky_horizon_color");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sky_curve", PROPERTY_HINT_EXP_EASING), "set_sky_curve", "get_sky_curve");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sky_energy_multiplier", PROPERTY_HINT_RANGE, "0,64,0.01"), "set_sky_energy_multiplier", "get_sky_energy_multiplier");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "sky_cover", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_sky_cover", "get_sky_cover");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "sky_cover_modulate"), "set_sky_cover_modulate", "get_sky_cover_modulate");

	ADD_GROUP("Ground", "ground_");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "ground_bottom_color", PROPERTY_HINT_COLOR_NO_ALPHA), "set_ground_bottom_color", "get_ground_bottom_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "ground_horizon_color", PROPERTY_HINT_COLOR_NO_ALPHA), "set_ground_horizon_color", "get_ground_horizon_color");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ground_curve", PROPERTY_HINT_EXP_EASING), "set_ground_curve", "get_ground_curve");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ground_energy_multiplier", PROPERTY_HINT_RANGE, "0,64,0.01"), "set_ground_energy_multiplier", "get_ground_energy_multiplier");

	ADD_GROUP("Sun", "sun_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sun_angle_max", PROPERTY_HINT_RANGE, "0,360,0.01,degrees"), "set_sun_angle_max", "get_sun_angle_max");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sun_curve", PROPERTY_HINT_EXP_EASING), "set_sun_curve", "get_sun_curve");

	ADD_GROUP("", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_debanding"), "set_use_debanding", "get_use_debanding");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "energy_multiplier", PROPERTY_HINT_RANGE, "0,128,0.01"), "set_energy_multiplier", "get_energy_multiplier");
}

void ProceduralSkyMaterial::cleanup_shader() {
	if (shader_cache[0].is_valid()) {
		RS::get_singleton()->free(shader_cache[0]);
		RS::get_singleton()->free(shader_cache[1]);
	}
}

void ProceduralSkyMaterial::_update_shader() {
	MutexLock shader_lock(shader_mutex);
	if (shader_cache[0].is_null()) {
		for (int i = 0; i < 2; i++) {
			shader_cache[i] = RS::get_singleton()->shader_create();

			// Add a comment to describe the shader origin (useful when converting to ShaderMaterial).
			RS::get_singleton()->shader_set_code(shader_cache[i], vformat(R"(
// NOTE: Shader automatically converted from )" VERSION_NAME " " VERSION_FULL_CONFIG R"('s ProceduralSkyMaterial.

shader_type sky;
%s

uniform vec4 sky_top_color : source_color = vec4(0.385, 0.454, 0.55, 1.0);
uniform vec4 sky_horizon_color : source_color = vec4(0.646, 0.656, 0.67, 1.0);
uniform float sky_curve : hint_range(0, 1) = 0.15;
uniform float sky_energy = 1.0; // In Lux.
uniform sampler2D sky_cover : filter_linear, source_color, hint_default_black;
uniform vec4 sky_cover_modulate : source_color = vec4(1.0, 1.0, 1.0, 1.0);
uniform vec4 ground_bottom_color : source_color = vec4(0.2, 0.169, 0.133, 1.0);
uniform vec4 ground_horizon_color : source_color = vec4(0.646, 0.656, 0.67, 1.0);
uniform float ground_curve : hint_range(0, 1) = 0.02;
uniform float ground_energy = 1.0;
uniform float sun_angle_max = 30.0;
uniform float sun_curve : hint_range(0, 1) = 0.15;
uniform float exposure : hint_range(0, 128) = 1.0;

void sky() {
	float v_angle = acos(clamp(EYEDIR.y, -1.0, 1.0));
	float c = (1.0 - v_angle / (PI * 0.5));
	vec3 sky = mix(sky_horizon_color.rgb, sky_top_color.rgb, clamp(1.0 - pow(1.0 - c, 1.0 / sky_curve), 0.0, 1.0));
	sky *= sky_energy;

	if (LIGHT0_ENABLED) {
		float sun_angle = acos(dot(LIGHT0_DIRECTION, EYEDIR));
		if (sun_angle < LIGHT0_SIZE) {
			sky = LIGHT0_COLOR * LIGHT0_ENERGY;
		} else if (sun_angle < sun_angle_max) {
			float c2 = (sun_angle - LIGHT0_SIZE) / (sun_angle_max - LIGHT0_SIZE);
			sky = mix(LIGHT0_COLOR * LIGHT0_ENERGY, sky, clamp(1.0 - pow(1.0 - c2, 1.0 / sun_curve), 0.0, 1.0));
		}
	}

	if (LIGHT1_ENABLED) {
		float sun_angle = acos(dot(LIGHT1_DIRECTION, EYEDIR));
		if (sun_angle < LIGHT1_SIZE) {
			sky = LIGHT1_COLOR * LIGHT1_ENERGY;
		} else if (sun_angle < sun_angle_max) {
			float c2 = (sun_angle - LIGHT1_SIZE) / (sun_angle_max - LIGHT1_SIZE);
			sky = mix(LIGHT1_COLOR * LIGHT1_ENERGY, sky, clamp(1.0 - pow(1.0 - c2, 1.0 / sun_curve), 0.0, 1.0));
		}
	}

	if (LIGHT2_ENABLED) {
		float sun_angle = acos(dot(LIGHT2_DIRECTION, EYEDIR));
		if (sun_angle < LIGHT2_SIZE) {
			sky = LIGHT2_COLOR * LIGHT2_ENERGY;
		} else if (sun_angle < sun_angle_max) {
			float c2 = (sun_angle - LIGHT2_SIZE) / (sun_angle_max - LIGHT2_SIZE);
			sky = mix(LIGHT2_COLOR * LIGHT2_ENERGY, sky, clamp(1.0 - pow(1.0 - c2, 1.0 / sun_curve), 0.0, 1.0));
		}
	}

	if (LIGHT3_ENABLED) {
		float sun_angle = acos(dot(LIGHT3_DIRECTION, EYEDIR));
		if (sun_angle < LIGHT3_SIZE) {
			sky = LIGHT3_COLOR * LIGHT3_ENERGY;
		} else if (sun_angle < sun_angle_max) {
			float c2 = (sun_angle - LIGHT3_SIZE) / (sun_angle_max - LIGHT3_SIZE);
			sky = mix(LIGHT3_COLOR * LIGHT3_ENERGY, sky, clamp(1.0 - pow(1.0 - c2, 1.0 / sun_curve), 0.0, 1.0));
		}
	}

	vec4 sky_cover_texture = texture(sky_cover, SKY_COORDS);
	sky += (sky_cover_texture.rgb * sky_cover_modulate.rgb) * sky_cover_texture.a * sky_cover_modulate.a * sky_energy;

	c = (v_angle - (PI * 0.5)) / (PI * 0.5);
	vec3 ground = mix(ground_horizon_color.rgb, ground_bottom_color.rgb, clamp(1.0 - pow(1.0 - c, 1.0 / ground_curve), 0.0, 1.0));
	ground *= ground_energy;

	COLOR = mix(ground, sky, step(0.0, EYEDIR.y)) * exposure;
}
)",
																		  i ? "render_mode use_debanding;" : ""));
		}
	}
}

ProceduralSkyMaterial::ProceduralSkyMaterial() {
	_set_material(RS::get_singleton()->material_create());
	set_sky_top_color(Color(0.385, 0.454, 0.55));
	set_sky_horizon_color(Color(0.6463, 0.6558, 0.6708));
	set_sky_curve(0.15);
	set_sky_energy_multiplier(1.0);
	set_sky_cover_modulate(Color(1, 1, 1));

	set_ground_bottom_color(Color(0.2, 0.169, 0.133));
	set_ground_horizon_color(Color(0.6463, 0.6558, 0.6708));
	set_ground_curve(0.02);
	set_ground_energy_multiplier(1.0);

	set_sun_angle_max(30.0);
	set_sun_curve(0.15);
	set_use_debanding(true);
	set_energy_multiplier(1.0);
}

ProceduralSkyMaterial::~ProceduralSkyMaterial() {
}

/////////////////////////////////////////
/* PanoramaSkyMaterial */

void PanoramaSkyMaterial::set_panorama(const Ref<Texture2D> &p_panorama) {
	panorama = p_panorama;
	if (p_panorama.is_valid()) {
		RS::get_singleton()->material_set_param(_get_material(), "source_panorama", p_panorama->get_rid());
	} else {
		RS::get_singleton()->material_set_param(_get_material(), "source_panorama", Variant());
	}
}

Ref<Texture2D> PanoramaSkyMaterial::get_panorama() const {
	return panorama;
}

void PanoramaSkyMaterial::set_filtering_enabled(bool p_enabled) {
	filter = p_enabled;
	notify_property_list_changed();
	_update_shader();
	// Only set if shader already compiled
	if (shader_set) {
		RS::get_singleton()->material_set_shader(_get_material(), shader_cache[int(filter)]);
	}
}

bool PanoramaSkyMaterial::is_filtering_enabled() const {
	return filter;
}

void PanoramaSkyMaterial::set_energy_multiplier(float p_multiplier) {
	energy_multiplier = p_multiplier;
	RS::get_singleton()->material_set_param(_get_material(), "exposure", energy_multiplier);
}

float PanoramaSkyMaterial::get_energy_multiplier() const {
	return energy_multiplier;
}

Shader::Mode PanoramaSkyMaterial::get_shader_mode() const {
	return Shader::MODE_SKY;
}

RID PanoramaSkyMaterial::get_rid() const {
	_update_shader();
	// Don't compile shaders until first use, then compile both
	if (!shader_set) {
		RS::get_singleton()->material_set_shader(_get_material(), shader_cache[1 - int(filter)]);
		RS::get_singleton()->material_set_shader(_get_material(), shader_cache[int(filter)]);
		shader_set = true;
	}
	return _get_material();
}

RID PanoramaSkyMaterial::get_shader_rid() const {
	_update_shader();
	return shader_cache[int(filter)];
}

void PanoramaSkyMaterial::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_panorama", "texture"), &PanoramaSkyMaterial::set_panorama);
	ClassDB::bind_method(D_METHOD("get_panorama"), &PanoramaSkyMaterial::get_panorama);

	ClassDB::bind_method(D_METHOD("set_filtering_enabled", "enabled"), &PanoramaSkyMaterial::set_filtering_enabled);
	ClassDB::bind_method(D_METHOD("is_filtering_enabled"), &PanoramaSkyMaterial::is_filtering_enabled);

	ClassDB::bind_method(D_METHOD("set_energy_multiplier", "multiplier"), &PanoramaSkyMaterial::set_energy_multiplier);
	ClassDB::bind_method(D_METHOD("get_energy_multiplier"), &PanoramaSkyMaterial::get_energy_multiplier);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "panorama", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_panorama", "get_panorama");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "filter"), "set_filtering_enabled", "is_filtering_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "energy_multiplier", PROPERTY_HINT_RANGE, "0,128,0.01"), "set_energy_multiplier", "get_energy_multiplier");
}

Mutex PanoramaSkyMaterial::shader_mutex;
RID PanoramaSkyMaterial::shader_cache[2];

void PanoramaSkyMaterial::cleanup_shader() {
	if (shader_cache[0].is_valid()) {
		RS::get_singleton()->free(shader_cache[0]);
		RS::get_singleton()->free(shader_cache[1]);
	}
}

void PanoramaSkyMaterial::_update_shader() {
	MutexLock shader_lock(shader_mutex);
	if (shader_cache[0].is_null()) {
		for (int i = 0; i < 2; i++) {
			shader_cache[i] = RS::get_singleton()->shader_create();

			// Add a comment to describe the shader origin (useful when converting to ShaderMaterial).
			RS::get_singleton()->shader_set_code(shader_cache[i], vformat(R"(
// NOTE: Shader automatically converted from )" VERSION_NAME " " VERSION_FULL_CONFIG R"('s PanoramaSkyMaterial.

shader_type sky;

uniform sampler2D source_panorama : %s, source_color, hint_default_black;
uniform float exposure : hint_range(0, 128) = 1.0;

void sky() {
	COLOR = texture(source_panorama, SKY_COORDS).rgb * exposure;
}
)",
																		  i ? "filter_linear" : "filter_nearest"));
		}
	}
}

PanoramaSkyMaterial::PanoramaSkyMaterial() {
	_set_material(RS::get_singleton()->material_create());
	set_energy_multiplier(1.0);
}

PanoramaSkyMaterial::~PanoramaSkyMaterial() {
}

//////////////////////////////////
/* PhysicalSkyMaterial */

void PhysicalSkyMaterial::set_rayleigh_coefficient(float p_rayleigh) {
	rayleigh = p_rayleigh;
	RS::get_singleton()->material_set_param(_get_material(), "rayleigh", rayleigh);
}

float PhysicalSkyMaterial::get_rayleigh_coefficient() const {
	return rayleigh;
}

void PhysicalSkyMaterial::set_rayleigh_color(Color p_rayleigh_color) {
	rayleigh_color = p_rayleigh_color;
	RS::get_singleton()->material_set_param(_get_material(), "rayleigh_color", rayleigh_color);
}

Color PhysicalSkyMaterial::get_rayleigh_color() const {
	return rayleigh_color;
}

void PhysicalSkyMaterial::set_mie_coefficient(float p_mie) {
	mie = p_mie;
	RS::get_singleton()->material_set_param(_get_material(), "mie", mie);
}

float PhysicalSkyMaterial::get_mie_coefficient() const {
	return mie;
}

void PhysicalSkyMaterial::set_mie_eccentricity(float p_eccentricity) {
	mie_eccentricity = p_eccentricity;
	RS::get_singleton()->material_set_param(_get_material(), "mie_eccentricity", mie_eccentricity);
}

float PhysicalSkyMaterial::get_mie_eccentricity() const {
	return mie_eccentricity;
}

void PhysicalSkyMaterial::set_mie_color(Color p_mie_color) {
	mie_color = p_mie_color;
	RS::get_singleton()->material_set_param(_get_material(), "mie_color", mie_color);
}

Color PhysicalSkyMaterial::get_mie_color() const {
	return mie_color;
}

void PhysicalSkyMaterial::set_turbidity(float p_turbidity) {
	turbidity = p_turbidity;
	RS::get_singleton()->material_set_param(_get_material(), "turbidity", turbidity);
}

float PhysicalSkyMaterial::get_turbidity() const {
	return turbidity;
}

void PhysicalSkyMaterial::set_sun_disk_scale(float p_sun_disk_scale) {
	sun_disk_scale = p_sun_disk_scale;
	RS::get_singleton()->material_set_param(_get_material(), "sun_disk_scale", sun_disk_scale);
}

float PhysicalSkyMaterial::get_sun_disk_scale() const {
	return sun_disk_scale;
}

void PhysicalSkyMaterial::set_ground_color(Color p_ground_color) {
	ground_color = p_ground_color;
	RS::get_singleton()->material_set_param(_get_material(), "ground_color", ground_color);
}

Color PhysicalSkyMaterial::get_ground_color() const {
	return ground_color;
}

void PhysicalSkyMaterial::set_energy_multiplier(float p_multiplier) {
	energy_multiplier = p_multiplier;
	RS::get_singleton()->material_set_param(_get_material(), "exposure", energy_multiplier);
}

float PhysicalSkyMaterial::get_energy_multiplier() const {
	return energy_multiplier;
}

void PhysicalSkyMaterial::set_use_debanding(bool p_use_debanding) {
	use_debanding = p_use_debanding;
	_update_shader();
	// Only set if shader already compiled
	if (shader_set) {
		RS::get_singleton()->material_set_shader(_get_material(), shader_cache[int(use_debanding)]);
	}
}

bool PhysicalSkyMaterial::get_use_debanding() const {
	return use_debanding;
}

void PhysicalSkyMaterial::set_night_sky(const Ref<Texture2D> &p_night_sky) {
	night_sky = p_night_sky;
	if (p_night_sky.is_valid()) {
		RS::get_singleton()->material_set_param(_get_material(), "night_sky", p_night_sky->get_rid());
	} else {
		RS::get_singleton()->material_set_param(_get_material(), "night_sky", Variant());
	}
}

Ref<Texture2D> PhysicalSkyMaterial::get_night_sky() const {
	return night_sky;
}

Shader::Mode PhysicalSkyMaterial::get_shader_mode() const {
	return Shader::MODE_SKY;
}

RID PhysicalSkyMaterial::get_rid() const {
	_update_shader();
	if (!shader_set) {
		RS::get_singleton()->material_set_shader(_get_material(), shader_cache[1 - int(use_debanding)]);
		RS::get_singleton()->material_set_shader(_get_material(), shader_cache[int(use_debanding)]);
		shader_set = true;
	}
	return _get_material();
}

RID PhysicalSkyMaterial::get_shader_rid() const {
	_update_shader();
	return shader_cache[int(use_debanding)];
}

void PhysicalSkyMaterial::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "exposure_value" && !GLOBAL_GET("rendering/lights_and_shadows/use_physical_light_units")) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

Mutex PhysicalSkyMaterial::shader_mutex;
RID PhysicalSkyMaterial::shader_cache[2];

void PhysicalSkyMaterial::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_rayleigh_coefficient", "rayleigh"), &PhysicalSkyMaterial::set_rayleigh_coefficient);
	ClassDB::bind_method(D_METHOD("get_rayleigh_coefficient"), &PhysicalSkyMaterial::get_rayleigh_coefficient);

	ClassDB::bind_method(D_METHOD("set_rayleigh_color", "color"), &PhysicalSkyMaterial::set_rayleigh_color);
	ClassDB::bind_method(D_METHOD("get_rayleigh_color"), &PhysicalSkyMaterial::get_rayleigh_color);

	ClassDB::bind_method(D_METHOD("set_mie_coefficient", "mie"), &PhysicalSkyMaterial::set_mie_coefficient);
	ClassDB::bind_method(D_METHOD("get_mie_coefficient"), &PhysicalSkyMaterial::get_mie_coefficient);

	ClassDB::bind_method(D_METHOD("set_mie_eccentricity", "eccentricity"), &PhysicalSkyMaterial::set_mie_eccentricity);
	ClassDB::bind_method(D_METHOD("get_mie_eccentricity"), &PhysicalSkyMaterial::get_mie_eccentricity);

	ClassDB::bind_method(D_METHOD("set_mie_color", "color"), &PhysicalSkyMaterial::set_mie_color);
	ClassDB::bind_method(D_METHOD("get_mie_color"), &PhysicalSkyMaterial::get_mie_color);

	ClassDB::bind_method(D_METHOD("set_turbidity", "turbidity"), &PhysicalSkyMaterial::set_turbidity);
	ClassDB::bind_method(D_METHOD("get_turbidity"), &PhysicalSkyMaterial::get_turbidity);

	ClassDB::bind_method(D_METHOD("set_sun_disk_scale", "scale"), &PhysicalSkyMaterial::set_sun_disk_scale);
	ClassDB::bind_method(D_METHOD("get_sun_disk_scale"), &PhysicalSkyMaterial::get_sun_disk_scale);

	ClassDB::bind_method(D_METHOD("set_ground_color", "color"), &PhysicalSkyMaterial::set_ground_color);
	ClassDB::bind_method(D_METHOD("get_ground_color"), &PhysicalSkyMaterial::get_ground_color);

	ClassDB::bind_method(D_METHOD("set_energy_multiplier", "multiplier"), &PhysicalSkyMaterial::set_energy_multiplier);
	ClassDB::bind_method(D_METHOD("get_energy_multiplier"), &PhysicalSkyMaterial::get_energy_multiplier);

	ClassDB::bind_method(D_METHOD("set_use_debanding", "use_debanding"), &PhysicalSkyMaterial::set_use_debanding);
	ClassDB::bind_method(D_METHOD("get_use_debanding"), &PhysicalSkyMaterial::get_use_debanding);

	ClassDB::bind_method(D_METHOD("set_night_sky", "night_sky"), &PhysicalSkyMaterial::set_night_sky);
	ClassDB::bind_method(D_METHOD("get_night_sky"), &PhysicalSkyMaterial::get_night_sky);

	ADD_GROUP("Rayleigh", "rayleigh_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rayleigh_coefficient", PROPERTY_HINT_RANGE, "0,64,0.01"), "set_rayleigh_coefficient", "get_rayleigh_coefficient");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "rayleigh_color", PROPERTY_HINT_COLOR_NO_ALPHA), "set_rayleigh_color", "get_rayleigh_color");

	ADD_GROUP("Mie", "mie_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mie_coefficient", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_mie_coefficient", "get_mie_coefficient");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mie_eccentricity", PROPERTY_HINT_RANGE, "-1,1,0.01"), "set_mie_eccentricity", "get_mie_eccentricity");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "mie_color", PROPERTY_HINT_COLOR_NO_ALPHA), "set_mie_color", "get_mie_color");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "turbidity", PROPERTY_HINT_RANGE, "0,1000,0.01"), "set_turbidity", "get_turbidity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sun_disk_scale", PROPERTY_HINT_RANGE, "0,360,0.01"), "set_sun_disk_scale", "get_sun_disk_scale");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "ground_color", PROPERTY_HINT_COLOR_NO_ALPHA), "set_ground_color", "get_ground_color");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "energy_multiplier", PROPERTY_HINT_RANGE, "0,128,0.01"), "set_energy_multiplier", "get_energy_multiplier");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_debanding"), "set_use_debanding", "get_use_debanding");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "night_sky", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_night_sky", "get_night_sky");
}

void PhysicalSkyMaterial::cleanup_shader() {
	if (shader_cache[0].is_valid()) {
		RS::get_singleton()->free(shader_cache[0]);
		RS::get_singleton()->free(shader_cache[1]);
	}
}

void PhysicalSkyMaterial::_update_shader() {
	MutexLock shader_lock(shader_mutex);
	if (shader_cache[0].is_null()) {
		for (int i = 0; i < 2; i++) {
			shader_cache[i] = RS::get_singleton()->shader_create();

			// Add a comment to describe the shader origin (useful when converting to ShaderMaterial).
			RS::get_singleton()->shader_set_code(shader_cache[i], vformat(R"(
// NOTE: Shader automatically converted from )" VERSION_NAME " " VERSION_FULL_CONFIG R"('s PhysicalSkyMaterial.

shader_type sky;
%s

uniform float rayleigh : hint_range(0, 64) = 2.0;
uniform vec4 rayleigh_color : source_color = vec4(0.3, 0.405, 0.6, 1.0);
uniform float mie : hint_range(0, 1) = 0.005;
uniform float mie_eccentricity : hint_range(-1, 1) = 0.8;
uniform vec4 mie_color : source_color = vec4(0.69, 0.729, 0.812, 1.0);

uniform float turbidity : hint_range(0, 1000) = 10.0;
uniform float sun_disk_scale : hint_range(0, 360) = 1.0;
uniform vec4 ground_color : source_color = vec4(0.1, 0.07, 0.034, 1.0);
uniform float exposure : hint_range(0, 128) = 1.0;

uniform sampler2D night_sky : filter_linear, source_color, hint_default_black;

const vec3 UP = vec3( 0.0, 1.0, 0.0 );

// Optical length at zenith for molecules.
const float rayleigh_zenith_size = 8.4e3;
const float mie_zenith_size = 1.25e3;

float henyey_greenstein(float cos_theta, float g) {
	const float k = 0.0795774715459;
	return k * (1.0 - g * g) / (pow(1.0 + g * g - 2.0 * g * cos_theta, 1.5));
}

void sky() {
	if (LIGHT0_ENABLED) {
		float zenith_angle = clamp( dot(UP, normalize(LIGHT0_DIRECTION)), -1.0, 1.0 );
		float sun_energy = max(0.0, 1.0 - exp(-((PI * 0.5) - acos(zenith_angle)))) * LIGHT0_ENERGY;
		float sun_fade = 1.0 - clamp(1.0 - exp(LIGHT0_DIRECTION.y), 0.0, 1.0);

		// Rayleigh coefficients.
		float rayleigh_coefficient = rayleigh - ( 1.0 * ( 1.0 - sun_fade ) );
		vec3 rayleigh_beta = rayleigh_coefficient * rayleigh_color.rgb * 0.0001;
		// mie coefficients from Preetham
		vec3 mie_beta = turbidity * mie * mie_color.rgb * 0.000434;

		// Optical length.
		float zenith = acos(max(0.0, dot(UP, EYEDIR)));
		float optical_mass = 1.0 / (cos(zenith) + 0.15 * pow(93.885 - degrees(zenith), -1.253));
		float rayleigh_scatter = rayleigh_zenith_size * optical_mass;
		float mie_scatter = mie_zenith_size * optical_mass;

		// Light extinction based on thickness of atmosphere.
		vec3 extinction = exp(-(rayleigh_beta * rayleigh_scatter + mie_beta * mie_scatter));

		// In scattering.
		float cos_theta = dot(EYEDIR, normalize(LIGHT0_DIRECTION));

		float rayleigh_phase = (3.0 / (16.0 * PI)) * (1.0 + pow(cos_theta * 0.5 + 0.5, 2.0));
		vec3 betaRTheta = rayleigh_beta * rayleigh_phase;

		float mie_phase = henyey_greenstein(cos_theta, mie_eccentricity);
		vec3 betaMTheta = mie_beta * mie_phase;

		vec3 Lin = pow(sun_energy * ((betaRTheta + betaMTheta) / (rayleigh_beta + mie_beta)) * (1.0 - extinction), vec3(1.5));
		// Hack from https://github.com/mrdoob/three.js/blob/master/examples/jsm/objects/Sky.js
		Lin *= mix(vec3(1.0), pow(sun_energy * ((betaRTheta + betaMTheta) / (rayleigh_beta + mie_beta)) * extinction, vec3(0.5)), clamp(pow(1.0 - zenith_angle, 5.0), 0.0, 1.0));

		// Hack in the ground color.
		Lin  *= mix(ground_color.rgb, vec3(1.0), smoothstep(-0.1, 0.1, dot(UP, EYEDIR)));

		// Solar disk and out-scattering.
		float sunAngularDiameterCos = cos(LIGHT0_SIZE * sun_disk_scale);
		float sunAngularDiameterCos2 = cos(LIGHT0_SIZE * sun_disk_scale*0.5);
		float sundisk = smoothstep(sunAngularDiameterCos, sunAngularDiameterCos2, cos_theta);
		vec3 L0 = (sun_energy * extinction) * sundisk * LIGHT0_COLOR;
		L0 += texture(night_sky, SKY_COORDS).xyz * extinction;

		vec3 color = Lin + L0;
		COLOR = pow(color, vec3(1.0 / (1.2 + (1.2 * sun_fade))));
		COLOR *= exposure;
	} else {
		// There is no sun, so display night_sky and nothing else.
		COLOR = texture(night_sky, SKY_COORDS).xyz;
		COLOR *= exposure;
	}
}
)",
																		  i ? "render_mode use_debanding;" : ""));
		}
	}
}

PhysicalSkyMaterial::PhysicalSkyMaterial() {
	_set_material(RS::get_singleton()->material_create());
	set_rayleigh_coefficient(2.0);
	set_rayleigh_color(Color(0.3, 0.405, 0.6));
	set_mie_coefficient(0.005);
	set_mie_eccentricity(0.8);
	set_mie_color(Color(0.69, 0.729, 0.812));
	set_turbidity(10.0);
	set_sun_disk_scale(1.0);
	set_ground_color(Color(0.1, 0.07, 0.034));
	set_energy_multiplier(1.0);
	set_use_debanding(true);
}

PhysicalSkyMaterial::~PhysicalSkyMaterial() {
}
