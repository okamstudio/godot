/**************************************************************************/
/*  light_3d.cpp                                                          */
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

#include "core/config/project_settings.h"

#include "light_3d.h"

void Light3D::set_param(Param p_param, real_t p_value) {
	ERR_FAIL_INDEX(p_param, PARAM_MAX);
	param[p_param] = p_value;

	RS::get_singleton()->light_set_param(light, RS::LightParam(p_param), p_value);

	if (p_param == PARAM_SPOT_ANGLE || p_param == PARAM_RANGE) {
		update_gizmos();

		if (p_param == PARAM_SPOT_ANGLE) {
			update_configuration_warnings();
		}
	}
}

real_t Light3D::get_param(Param p_param) const {
	ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);
	return param[p_param];
}

void Light3D::set_shadow(bool p_enable) {
	shadow = p_enable;
	RS::get_singleton()->light_set_shadow(light, p_enable);

	notify_property_list_changed();
	update_configuration_warnings();
}

bool Light3D::has_shadow() const {
	return shadow;
}

void Light3D::set_negative(bool p_enable) {
	negative = p_enable;
	RS::get_singleton()->light_set_negative(light, p_enable);
}

bool Light3D::is_negative() const {
	return negative;
}

void Light3D::set_enable_distance_fade(bool p_enable) {
	distance_fade_enabled = p_enable;
	RS::get_singleton()->light_set_distance_fade(light, distance_fade_enabled, distance_fade_begin, distance_fade_shadow, distance_fade_length);
	notify_property_list_changed();
}

bool Light3D::is_distance_fade_enabled() const {
	return distance_fade_enabled;
}

void Light3D::set_distance_fade_begin(real_t p_distance) {
	distance_fade_begin = p_distance;
	RS::get_singleton()->light_set_distance_fade(light, distance_fade_enabled, distance_fade_begin, distance_fade_shadow, distance_fade_length);
}

real_t Light3D::get_distance_fade_begin() const {
	return distance_fade_begin;
}

void Light3D::set_distance_fade_shadow(real_t p_distance) {
	distance_fade_shadow = p_distance;
	RS::get_singleton()->light_set_distance_fade(light, distance_fade_enabled, distance_fade_begin, distance_fade_shadow, distance_fade_length);
}

real_t Light3D::get_distance_fade_shadow() const {
	return distance_fade_shadow;
}

void Light3D::set_distance_fade_length(real_t p_length) {
	distance_fade_length = p_length;
	RS::get_singleton()->light_set_distance_fade(light, distance_fade_enabled, distance_fade_begin, distance_fade_shadow, distance_fade_length);
}

real_t Light3D::get_distance_fade_length() const {
	return distance_fade_length;
}

void Light3D::set_cull_mask(uint32_t p_cull_mask) {
	cull_mask = p_cull_mask;
	RS::get_singleton()->light_set_cull_mask(light, p_cull_mask);
}

uint32_t Light3D::get_cull_mask() const {
	return cull_mask;
}

void Light3D::set_color(const Color &p_color) {
	color = p_color;

	if (GLOBAL_GET("rendering/lights_and_shadows/use_physical_light_units")) {
		Color combined = color.srgb_to_linear();
		combined *= correlated_color.srgb_to_linear();
		RS::get_singleton()->light_set_color(light, combined.linear_to_srgb());
	} else {
		RS::get_singleton()->light_set_color(light, color);
	}
	// The gizmo color depends on the light color, so update it.
	update_gizmos();
}

Color Light3D::get_color() const {
	return color;
}

void Light3D::set_shadow_reverse_cull_face(bool p_enable) {
	reverse_cull = p_enable;
	RS::get_singleton()->light_set_reverse_cull_face_mode(light, reverse_cull);
}

bool Light3D::get_shadow_reverse_cull_face() const {
	return reverse_cull;
}

void Light3D::set_shadow_caster_mask(uint32_t p_caster_mask) {
	shadow_caster_mask = p_caster_mask;
	RS::get_singleton()->light_set_shadow_caster_mask(light, shadow_caster_mask);
}

uint32_t Light3D::get_shadow_caster_mask() const {
	return shadow_caster_mask;
}

AABB Light3D::get_aabb() const {
	if (type == RenderingServer::LIGHT_DIRECTIONAL) {
		return AABB(Vector3(-1, -1, -1), Vector3(2, 2, 2));

	} else if (type == RenderingServer::LIGHT_OMNI) {
		return AABB(Vector3(-1, -1, -1) * param[PARAM_RANGE], Vector3(2, 2, 2) * param[PARAM_RANGE]);

	} else if (type == RenderingServer::LIGHT_SPOT) {
		real_t cone_slant_height = param[PARAM_RANGE];
		real_t cone_angle_rad = Math::deg_to_rad(param[PARAM_SPOT_ANGLE]);

		if (cone_angle_rad > Math_PI / 2.0) {
			// Just return the AABB of an omni light if the spot angle is above 90 degrees.
			return AABB(Vector3(-1, -1, -1) * cone_slant_height, Vector3(2, 2, 2) * cone_slant_height);
		}

		real_t size = Math::sin(cone_angle_rad) * cone_slant_height;
		return AABB(Vector3(-size, -size, -cone_slant_height), Vector3(2 * size, 2 * size, cone_slant_height));
	}

	return AABB();
}

PackedStringArray Light3D::get_configuration_warnings() const {
	PackedStringArray warnings = VisualInstance3D::get_configuration_warnings();

	if (!get_scale().is_equal_approx(Vector3(1, 1, 1))) {
		warnings.push_back(RTR("A light's scale does not affect the visual size of the light."));
	}

	return warnings;
}

void Light3D::set_bake_mode(BakeMode p_mode) {
	bake_mode = p_mode;
	RS::get_singleton()->light_set_bake_mode(light, RS::LightBakeMode(p_mode));
}

Light3D::BakeMode Light3D::get_bake_mode() const {
	return bake_mode;
}

void Light3D::set_projector(const Ref<Texture2D> &p_texture) {
	projector = p_texture;
	RID tex_id = projector.is_valid() ? projector->get_rid() : RID();
	RS::get_singleton()->light_set_projector(light, tex_id);
	update_configuration_warnings();
}

Ref<Texture2D> Light3D::get_projector() const {
	return projector;
}

void Light3D::owner_changed_notify() {
	// For cases where owner changes _after_ entering tree (as example, editor editing).
	_update_visibility();
}

// Temperature expressed in Kelvins. Valid range 1000 - 15000
// First converts to CIE 1960 then to sRGB
// As explained in the Filament documentation: https://google.github.io/filament/Filament.md.html#lighting/directlighting/lightsparameterization
Color _color_from_temperature(float p_temperature) {
	float T2 = p_temperature * p_temperature;
	float u = (0.860117757f + 1.54118254e-4f * p_temperature + 1.28641212e-7f * T2) /
			(1.0f + 8.42420235e-4f * p_temperature + 7.08145163e-7f * T2);
	float v = (0.317398726f + 4.22806245e-5f * p_temperature + 4.20481691e-8f * T2) /
			(1.0f - 2.89741816e-5f * p_temperature + 1.61456053e-7f * T2);

	// Convert to xyY space.
	float d = 1.0f / (2.0f * u - 8.0f * v + 4.0f);
	float x = 3.0f * u * d;
	float y = 2.0f * v * d;

	// Convert to XYZ space
	const float a = 1.0 / MAX(y, 1e-5f);
	Vector3 xyz = Vector3(x * a, 1.0, (1.0f - x - y) * a);

	// Convert from XYZ to sRGB(linear)
	Vector3 linear = Vector3(3.2404542f * xyz.x - 1.5371385f * xyz.y - 0.4985314f * xyz.z,
			-0.9692660f * xyz.x + 1.8760108f * xyz.y + 0.0415560f * xyz.z,
			0.0556434f * xyz.x - 0.2040259f * xyz.y + 1.0572252f * xyz.z);
	linear /= MAX(1e-5f, linear[linear.max_axis_index()]);
	// Normalize, clamp, and convert to sRGB.
	return Color(linear.x, linear.y, linear.z).clamp().linear_to_srgb();
}

void Light3D::set_temperature(const float p_temperature) {
	temperature = p_temperature;
	if (!GLOBAL_GET("rendering/lights_and_shadows/use_physical_light_units")) {
		return;
	}
	correlated_color = _color_from_temperature(temperature);

	Color combined = color.srgb_to_linear() * correlated_color.srgb_to_linear();

	RS::get_singleton()->light_set_color(light, combined.linear_to_srgb());
	// The gizmo color depends on the light color, so update it.
	update_gizmos();
}

Color Light3D::get_correlated_color() const {
	return correlated_color;
}

float Light3D::get_temperature() const {
	return temperature;
}

void Light3D::_update_visibility() {
	if (!is_inside_tree()) {
		return;
	}

	bool editor_ok = true;

#ifdef TOOLS_ENABLED
	if (editor_only) {
		if (!Engine::get_singleton()->is_editor_hint()) {
			editor_ok = false;
		} else {
			editor_ok = (get_tree()->get_edited_scene_root() && (this == get_tree()->get_edited_scene_root() || get_owner() == get_tree()->get_edited_scene_root()));
		}
	}
#else
	if (editor_only) {
		editor_ok = false;
	}
#endif

	RS::get_singleton()->instance_set_visible(get_instance(), is_visible_in_tree() && editor_ok);
}

void Light3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_TRANSFORM_CHANGED: {
			update_configuration_warnings();
		} break;
		case NOTIFICATION_VISIBILITY_CHANGED:
		case NOTIFICATION_ENTER_TREE: {
			_update_visibility();
		} break;
	}
}

void Light3D::set_editor_only(bool p_editor_only) {
	editor_only = p_editor_only;
	_update_visibility();
}

bool Light3D::is_editor_only() const {
	return editor_only;
}

void Light3D::_validate_property(PropertyInfo &p_property) const {
	if (!shadow && (p_property.name == "shadow_bias" || p_property.name == "shadow_normal_bias" || p_property.name == "shadow_reverse_cull_face" || p_property.name == "shadow_transmittance_bias" || p_property.name == "shadow_opacity" || p_property.name == "shadow_blur" || p_property.name == "distance_fade_shadow" || p_property.name == "shadow_caster_mask")) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}

	if (get_light_type() != RS::LIGHT_DIRECTIONAL && (p_property.name == "light_angular_distance" || p_property.name == "light_intensity_lux")) {
		// Angular distance and Light Intensity Lux are only used in DirectionalLight3D.
		p_property.usage = PROPERTY_USAGE_NONE;
	} else if (get_light_type() == RS::LIGHT_DIRECTIONAL && p_property.name == "light_intensity_lumens") {
		p_property.usage = PROPERTY_USAGE_NONE;
	}

	if (!GLOBAL_GET("rendering/lights_and_shadows/use_physical_light_units") && (p_property.name == "light_intensity_lumens" || p_property.name == "light_intensity_lux" || p_property.name == "light_temperature")) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}

	if (!distance_fade_enabled && (p_property.name == "distance_fade_begin" || p_property.name == "distance_fade_shadow" || p_property.name == "distance_fade_length")) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

void Light3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_editor_only", "editor_only"), &Light3D::set_editor_only);
	ClassDB::bind_method(D_METHOD("is_editor_only"), &Light3D::is_editor_only);

	ClassDB::bind_method(D_METHOD("set_param", "param", "value"), &Light3D::set_param);
	ClassDB::bind_method(D_METHOD("get_param", "param"), &Light3D::get_param);

	ClassDB::bind_method(D_METHOD("set_shadow", "enabled"), &Light3D::set_shadow);
	ClassDB::bind_method(D_METHOD("has_shadow"), &Light3D::has_shadow);

	ClassDB::bind_method(D_METHOD("set_negative", "enabled"), &Light3D::set_negative);
	ClassDB::bind_method(D_METHOD("is_negative"), &Light3D::is_negative);

	ClassDB::bind_method(D_METHOD("set_cull_mask", "cull_mask"), &Light3D::set_cull_mask);
	ClassDB::bind_method(D_METHOD("get_cull_mask"), &Light3D::get_cull_mask);

	ClassDB::bind_method(D_METHOD("set_enable_distance_fade", "enable"), &Light3D::set_enable_distance_fade);
	ClassDB::bind_method(D_METHOD("is_distance_fade_enabled"), &Light3D::is_distance_fade_enabled);

	ClassDB::bind_method(D_METHOD("set_distance_fade_begin", "distance"), &Light3D::set_distance_fade_begin);
	ClassDB::bind_method(D_METHOD("get_distance_fade_begin"), &Light3D::get_distance_fade_begin);

	ClassDB::bind_method(D_METHOD("set_distance_fade_shadow", "distance"), &Light3D::set_distance_fade_shadow);
	ClassDB::bind_method(D_METHOD("get_distance_fade_shadow"), &Light3D::get_distance_fade_shadow);

	ClassDB::bind_method(D_METHOD("set_distance_fade_length", "distance"), &Light3D::set_distance_fade_length);
	ClassDB::bind_method(D_METHOD("get_distance_fade_length"), &Light3D::get_distance_fade_length);

	ClassDB::bind_method(D_METHOD("set_color", "color"), &Light3D::set_color);
	ClassDB::bind_method(D_METHOD("get_color"), &Light3D::get_color);

	ClassDB::bind_method(D_METHOD("set_shadow_reverse_cull_face", "enable"), &Light3D::set_shadow_reverse_cull_face);
	ClassDB::bind_method(D_METHOD("get_shadow_reverse_cull_face"), &Light3D::get_shadow_reverse_cull_face);

	ClassDB::bind_method(D_METHOD("set_shadow_caster_mask", "caster_mask"), &Light3D::set_shadow_caster_mask);
	ClassDB::bind_method(D_METHOD("get_shadow_caster_mask"), &Light3D::get_shadow_caster_mask);

	ClassDB::bind_method(D_METHOD("set_bake_mode", "bake_mode"), &Light3D::set_bake_mode);
	ClassDB::bind_method(D_METHOD("get_bake_mode"), &Light3D::get_bake_mode);

	ClassDB::bind_method(D_METHOD("set_projector", "projector"), &Light3D::set_projector);
	ClassDB::bind_method(D_METHOD("get_projector"), &Light3D::get_projector);

	ClassDB::bind_method(D_METHOD("set_temperature", "temperature"), &Light3D::set_temperature);
	ClassDB::bind_method(D_METHOD("get_temperature"), &Light3D::get_temperature);
	ClassDB::bind_method(D_METHOD("get_correlated_color"), &Light3D::get_correlated_color);

	ADD_GROUP("Light", "light_");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "light_intensity_lumens", PROPERTY_HINT_RANGE, "0,100000.0,0.01,or_greater,suffix:lm"), "set_param", "get_param", PARAM_INTENSITY);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "light_intensity_lux", PROPERTY_HINT_RANGE, "0,150000.0,0.01,or_greater,suffix:lx"), "set_param", "get_param", PARAM_INTENSITY);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "light_temperature", PROPERTY_HINT_RANGE, "1000,15000.0,1.0,suffix:k"), "set_temperature", "get_temperature");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "light_color", PROPERTY_HINT_COLOR_NO_ALPHA), "set_color", "get_color");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "light_energy", PROPERTY_HINT_RANGE, "0,16,0.001,or_greater"), "set_param", "get_param", PARAM_ENERGY);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "light_indirect_energy", PROPERTY_HINT_RANGE, "0,16,0.001,or_greater"), "set_param", "get_param", PARAM_INDIRECT_ENERGY);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "light_volumetric_fog_energy", PROPERTY_HINT_RANGE, "0,16,0.001,or_greater"), "set_param", "get_param", PARAM_VOLUMETRIC_FOG_ENERGY);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "light_projector", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_projector", "get_projector");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "light_size", PROPERTY_HINT_RANGE, "0,1,0.001,or_greater,suffix:m"), "set_param", "get_param", PARAM_SIZE);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "light_angular_distance", PROPERTY_HINT_RANGE, "0,90,0.01,degrees"), "set_param", "get_param", PARAM_SIZE);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "light_negative"), "set_negative", "is_negative");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "light_specular", PROPERTY_HINT_RANGE, "0,16,0.001,or_greater"), "set_param", "get_param", PARAM_SPECULAR);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "light_bake_mode", PROPERTY_HINT_ENUM, "Disabled,Static,Dynamic"), "set_bake_mode", "get_bake_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "light_cull_mask", PROPERTY_HINT_LAYERS_3D_RENDER), "set_cull_mask", "get_cull_mask");

	ADD_GROUP("Shadow", "shadow_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "shadow_enabled"), "set_shadow", "has_shadow");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "shadow_bias", PROPERTY_HINT_RANGE, "0,10,0.001"), "set_param", "get_param", PARAM_SHADOW_BIAS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "shadow_normal_bias", PROPERTY_HINT_RANGE, "0,10,0.001"), "set_param", "get_param", PARAM_SHADOW_NORMAL_BIAS);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "shadow_reverse_cull_face"), "set_shadow_reverse_cull_face", "get_shadow_reverse_cull_face");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "shadow_transmittance_bias", PROPERTY_HINT_RANGE, "-16,16,0.001"), "set_param", "get_param", PARAM_TRANSMITTANCE_BIAS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "shadow_opacity", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_param", "get_param", PARAM_SHADOW_OPACITY);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "shadow_blur", PROPERTY_HINT_RANGE, "0,10,0.001"), "set_param", "get_param", PARAM_SHADOW_BLUR);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "shadow_caster_mask", PROPERTY_HINT_LAYERS_3D_RENDER), "set_shadow_caster_mask", "get_shadow_caster_mask");

	ADD_GROUP("Distance Fade", "distance_fade_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "distance_fade_enabled"), "set_enable_distance_fade", "is_distance_fade_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "distance_fade_begin", PROPERTY_HINT_RANGE, "0.0,4096.0,0.01,or_greater,suffix:m"), "set_distance_fade_begin", "get_distance_fade_begin");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "distance_fade_shadow", PROPERTY_HINT_RANGE, "0.0,4096.0,0.01,or_greater,suffix:m"), "set_distance_fade_shadow", "get_distance_fade_shadow");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "distance_fade_length", PROPERTY_HINT_RANGE, "0.0,4096.0,0.01,or_greater,suffix:m"), "set_distance_fade_length", "get_distance_fade_length");

	ADD_GROUP("Editor", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "editor_only"), "set_editor_only", "is_editor_only");

	ADD_GROUP("", "");

	BIND_ENUM_CONSTANT(PARAM_ENERGY);
	BIND_ENUM_CONSTANT(PARAM_INDIRECT_ENERGY);
	BIND_ENUM_CONSTANT(PARAM_VOLUMETRIC_FOG_ENERGY);
	BIND_ENUM_CONSTANT(PARAM_SPECULAR);
	BIND_ENUM_CONSTANT(PARAM_RANGE);
	BIND_ENUM_CONSTANT(PARAM_SIZE);
	BIND_ENUM_CONSTANT(PARAM_ATTENUATION);
	BIND_ENUM_CONSTANT(PARAM_SPOT_ANGLE);
	BIND_ENUM_CONSTANT(PARAM_SPOT_ATTENUATION);
	BIND_ENUM_CONSTANT(PARAM_SHADOW_MAX_DISTANCE);
	BIND_ENUM_CONSTANT(PARAM_SHADOW_SPLIT_1_OFFSET);
	BIND_ENUM_CONSTANT(PARAM_SHADOW_SPLIT_2_OFFSET);
	BIND_ENUM_CONSTANT(PARAM_SHADOW_SPLIT_3_OFFSET);
	BIND_ENUM_CONSTANT(PARAM_SHADOW_FADE_START);
	BIND_ENUM_CONSTANT(PARAM_SHADOW_NORMAL_BIAS);
	BIND_ENUM_CONSTANT(PARAM_SHADOW_BIAS);
	BIND_ENUM_CONSTANT(PARAM_SHADOW_PANCAKE_SIZE);
	BIND_ENUM_CONSTANT(PARAM_SHADOW_OPACITY);
	BIND_ENUM_CONSTANT(PARAM_SHADOW_BLUR);
	BIND_ENUM_CONSTANT(PARAM_TRANSMITTANCE_BIAS);
	BIND_ENUM_CONSTANT(PARAM_INTENSITY);
	BIND_ENUM_CONSTANT(PARAM_MAX);

	BIND_ENUM_CONSTANT(BAKE_DISABLED);
	BIND_ENUM_CONSTANT(BAKE_STATIC);
	BIND_ENUM_CONSTANT(BAKE_DYNAMIC);
}

Light3D::Light3D(RenderingServer::LightType p_type) {
	type = p_type;
	switch (p_type) {
		case RS::LIGHT_DIRECTIONAL:
			light = RenderingServer::get_singleton()->directional_light_create();
			break;
		case RS::LIGHT_OMNI:
			light = RenderingServer::get_singleton()->omni_light_create();
			break;
		case RS::LIGHT_SPOT:
			light = RenderingServer::get_singleton()->spot_light_create();
			break;
		default: {
		};
	}

	RS::get_singleton()->instance_set_base(get_instance(), light);

	set_color(Color(1, 1, 1, 1));
	set_shadow(false);
	set_negative(false);
	set_cull_mask(0xFFFFFFFF);

	set_param(PARAM_ENERGY, 1);
	set_param(PARAM_INDIRECT_ENERGY, 1);
	set_param(PARAM_VOLUMETRIC_FOG_ENERGY, 1);
	set_param(PARAM_SPECULAR, 0.5);
	set_param(PARAM_RANGE, 5);
	set_param(PARAM_SIZE, 0);
	set_param(PARAM_ATTENUATION, 1);
	set_param(PARAM_SPOT_ANGLE, 45);
	set_param(PARAM_SPOT_ATTENUATION, 1);
	set_param(PARAM_SHADOW_MAX_DISTANCE, 0);
	set_param(PARAM_SHADOW_SPLIT_1_OFFSET, 0.1);
	set_param(PARAM_SHADOW_SPLIT_2_OFFSET, 0.2);
	set_param(PARAM_SHADOW_SPLIT_3_OFFSET, 0.5);
	set_param(PARAM_SHADOW_FADE_START, 0.8);
	set_param(PARAM_SHADOW_PANCAKE_SIZE, 20.0);
	set_param(PARAM_SHADOW_OPACITY, 1.0);
	set_param(PARAM_SHADOW_BLUR, 1.0);
	set_param(PARAM_SHADOW_BIAS, 0.1);
	set_param(PARAM_SHADOW_NORMAL_BIAS, 1.0);
	set_param(PARAM_TRANSMITTANCE_BIAS, 0.05);
	set_param(PARAM_SHADOW_FADE_START, 1);
	// For OmniLight3D and SpotLight3D, specified in Lumens.
	set_param(PARAM_INTENSITY, 1000.0);
	set_temperature(6500.0); // Nearly white.
	set_disable_scale(true);
}

Light3D::Light3D() {
	ERR_PRINT("Light3D should not be instantiated directly; use the DirectionalLight3D, OmniLight3D or SpotLight3D subtypes instead.");
}

Light3D::~Light3D() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RS::get_singleton()->instance_set_base(get_instance(), RID());

	if (light.is_valid()) {
		RenderingServer::get_singleton()->free(light);
	}
}

/////////////////////////////////////////

void DirectionalLight3D::set_shadow_mode(ShadowMode p_mode) {
	shadow_mode = p_mode;
	RS::get_singleton()->light_directional_set_shadow_mode(light, RS::LightDirectionalShadowMode(p_mode));
	notify_property_list_changed();
}

DirectionalLight3D::ShadowMode DirectionalLight3D::get_shadow_mode() const {
	return shadow_mode;
}

void DirectionalLight3D::set_blend_splits(bool p_enable) {
	blend_splits = p_enable;
	RS::get_singleton()->light_directional_set_blend_splits(light, p_enable);
}

bool DirectionalLight3D::is_blend_splits_enabled() const {
	return blend_splits;
}

void DirectionalLight3D::set_sky_mode(SkyMode p_mode) {
	sky_mode = p_mode;
	RS::get_singleton()->light_directional_set_sky_mode(light, RS::LightDirectionalSkyMode(p_mode));
}

DirectionalLight3D::SkyMode DirectionalLight3D::get_sky_mode() const {
	return sky_mode;
}

void DirectionalLight3D::_validate_property(PropertyInfo &p_property) const {
	if (shadow_mode == SHADOW_ORTHOGONAL && (p_property.name == "directional_shadow_split_1" || p_property.name == "directional_shadow_blend_splits")) {
		// Split 2 and split blending are only used with the PSSM 2 Splits and PSSM 4 Splits shadow modes.
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}

	if ((shadow_mode == SHADOW_ORTHOGONAL || shadow_mode == SHADOW_PARALLEL_2_SPLITS) && (p_property.name == "directional_shadow_split_2" || p_property.name == "directional_shadow_split_3")) {
		// Splits 3 and 4 are only used with the PSSM 4 Splits shadow mode.
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}

	if (p_property.name == "light_size" || p_property.name == "light_projector" || p_property.name == "light_specular") {
		// Not implemented in DirectionalLight3D (`light_size` is replaced by `light_angular_distance`).
		p_property.usage = PROPERTY_USAGE_NONE;
	}

	if (p_property.name == "distance_fade_enabled" || p_property.name == "distance_fade_begin" || p_property.name == "distance_fade_shadow" || p_property.name == "distance_fade_length") {
		// Not relevant for DirectionalLight3D, as the light LOD system only pertains to point lights.
		// For DirectionalLight3D, `directional_shadow_max_distance` can be used instead.
		p_property.usage = PROPERTY_USAGE_NONE;
	}
}

void DirectionalLight3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_shadow_mode", "mode"), &DirectionalLight3D::set_shadow_mode);
	ClassDB::bind_method(D_METHOD("get_shadow_mode"), &DirectionalLight3D::get_shadow_mode);

	ClassDB::bind_method(D_METHOD("set_blend_splits", "enabled"), &DirectionalLight3D::set_blend_splits);
	ClassDB::bind_method(D_METHOD("is_blend_splits_enabled"), &DirectionalLight3D::is_blend_splits_enabled);

	ClassDB::bind_method(D_METHOD("set_sky_mode", "mode"), &DirectionalLight3D::set_sky_mode);
	ClassDB::bind_method(D_METHOD("get_sky_mode"), &DirectionalLight3D::get_sky_mode);

	ADD_GROUP("Directional Shadow", "directional_shadow_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "directional_shadow_mode", PROPERTY_HINT_ENUM, "Orthogonal (Fast),PSSM 2 Splits (Average),PSSM 4 Splits (Slow)"), "set_shadow_mode", "get_shadow_mode");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "directional_shadow_split_1", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_param", "get_param", PARAM_SHADOW_SPLIT_1_OFFSET);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "directional_shadow_split_2", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_param", "get_param", PARAM_SHADOW_SPLIT_2_OFFSET);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "directional_shadow_split_3", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_param", "get_param", PARAM_SHADOW_SPLIT_3_OFFSET);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "directional_shadow_blend_splits"), "set_blend_splits", "is_blend_splits_enabled");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "directional_shadow_fade_start", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_param", "get_param", PARAM_SHADOW_FADE_START);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "directional_shadow_max_distance", PROPERTY_HINT_RANGE, "0,8192,0.1,or_greater,exp"), "set_param", "get_param", PARAM_SHADOW_MAX_DISTANCE);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "directional_shadow_pancake_size", PROPERTY_HINT_RANGE, "0,1024,0.1,or_greater,exp"), "set_param", "get_param", PARAM_SHADOW_PANCAKE_SIZE);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "sky_mode", PROPERTY_HINT_ENUM, "Light and Sky,Light Only,Sky Only"), "set_sky_mode", "get_sky_mode");

	BIND_ENUM_CONSTANT(SHADOW_ORTHOGONAL);
	BIND_ENUM_CONSTANT(SHADOW_PARALLEL_2_SPLITS);
	BIND_ENUM_CONSTANT(SHADOW_PARALLEL_4_SPLITS);

	BIND_ENUM_CONSTANT(SKY_MODE_LIGHT_AND_SKY);
	BIND_ENUM_CONSTANT(SKY_MODE_LIGHT_ONLY);
	BIND_ENUM_CONSTANT(SKY_MODE_SKY_ONLY);
}

DirectionalLight3D::DirectionalLight3D() :
		Light3D(RenderingServer::LIGHT_DIRECTIONAL) {
	set_param(PARAM_SHADOW_MAX_DISTANCE, 100);
	set_param(PARAM_SHADOW_FADE_START, 0.8);
	// Increase the default shadow normal bias to better suit most scenes.
	set_param(PARAM_SHADOW_NORMAL_BIAS, 2.0);
	set_param(PARAM_INTENSITY, 100000.0); // Specified in Lux, approximate mid-day sun.
	set_shadow_mode(SHADOW_PARALLEL_4_SPLITS);
	blend_splits = false;
	set_sky_mode(SKY_MODE_LIGHT_AND_SKY);
}

void OmniLight3D::set_shadow_mode(ShadowMode p_mode) {
	shadow_mode = p_mode;
	RS::get_singleton()->light_omni_set_shadow_mode(light, RS::LightOmniShadowMode(p_mode));
}

OmniLight3D::ShadowMode OmniLight3D::get_shadow_mode() const {
	return shadow_mode;
}

PackedStringArray OmniLight3D::get_configuration_warnings() const {
	PackedStringArray warnings = Light3D::get_configuration_warnings();

	if (!has_shadow() && get_projector().is_valid()) {
		warnings.push_back(RTR("Projector texture only works with shadows active."));
	}

	if (get_projector().is_valid() && OS::get_singleton()->get_current_rendering_method() == "gl_compatibility") {
		warnings.push_back(RTR("Projector textures are not supported when using the Compatibility renderer yet. Support will be added in a future release."));
	}

	return warnings;
}

void OmniLight3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_shadow_mode", "mode"), &OmniLight3D::set_shadow_mode);
	ClassDB::bind_method(D_METHOD("get_shadow_mode"), &OmniLight3D::get_shadow_mode);

	ADD_GROUP("Omni", "omni_");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "omni_range", PROPERTY_HINT_RANGE, "0,4096,0.001,or_greater,exp"), "set_param", "get_param", PARAM_RANGE);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "omni_attenuation", PROPERTY_HINT_RANGE, "-10,10,0.001,or_greater,or_less"), "set_param", "get_param", PARAM_ATTENUATION);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "omni_shadow_mode", PROPERTY_HINT_ENUM, "Dual Paraboloid,Cube"), "set_shadow_mode", "get_shadow_mode");

	BIND_ENUM_CONSTANT(SHADOW_DUAL_PARABOLOID);
	BIND_ENUM_CONSTANT(SHADOW_CUBE);
}

OmniLight3D::OmniLight3D() :
		Light3D(RenderingServer::LIGHT_OMNI) {
	set_shadow_mode(SHADOW_CUBE);
}

PackedStringArray SpotLight3D::get_configuration_warnings() const {
	PackedStringArray warnings = Light3D::get_configuration_warnings();

	if (has_shadow() && get_param(PARAM_SPOT_ANGLE) >= 90.0) {
		warnings.push_back(RTR("A SpotLight3D with an angle wider than 90 degrees cannot cast shadows."));
	}

	if (!has_shadow() && get_projector().is_valid()) {
		warnings.push_back(RTR("Projector texture only works with shadows active."));
	}

	if (get_projector().is_valid() && OS::get_singleton()->get_current_rendering_method() == "gl_compatibility") {
		warnings.push_back(RTR("Projector textures are not supported when using the Compatibility renderer yet. Support will be added in a future release."));
	}

	return warnings;
}

void SpotLight3D::_bind_methods() {
	ADD_GROUP("Spot", "spot_");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "spot_range", PROPERTY_HINT_RANGE, "0,4096,0.001,or_greater,exp,suffix:m"), "set_param", "get_param", PARAM_RANGE);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "spot_attenuation", PROPERTY_HINT_RANGE, "-10,10,0.01,or_greater,or_less"), "set_param", "get_param", PARAM_ATTENUATION);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "spot_angle", PROPERTY_HINT_RANGE, "0,180,0.01,degrees"), "set_param", "get_param", PARAM_SPOT_ANGLE);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "spot_angle_attenuation", PROPERTY_HINT_EXP_EASING, "attenuation"), "set_param", "get_param", PARAM_SPOT_ATTENUATION);
}

SpotLight3D::SpotLight3D() :
		Light3D(RenderingServer::LIGHT_SPOT) {
	// Decrease the default shadow bias to better suit most scenes.
	set_param(PARAM_SHADOW_BIAS, 0.03);
}
