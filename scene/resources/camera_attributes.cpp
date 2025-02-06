/**************************************************************************/
/*  camera_attributes.cpp                                                 */
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

#include "camera_attributes.h"

#include "core/config/project_settings.h"
#include "servers/rendering_server.h"

void CameraAttributes::set_exposure_multiplier(float p_multiplier) {
	exposure_multiplier = p_multiplier;
	_update_exposure();
	emit_changed();
}

float CameraAttributes::get_exposure_multiplier() const {
	return exposure_multiplier;
}

void CameraAttributes::set_exposure_sensitivity(float p_sensitivity) {
	exposure_sensitivity = p_sensitivity;
	_update_exposure();
	emit_changed();
}

float CameraAttributes::get_exposure_sensitivity() const {
	return exposure_sensitivity;
}

void CameraAttributes::_update_exposure() {
	float exposure_normalization = 1.0;
	// Ignore physical properties if not using physical light units.
	if (GLOBAL_GET("rendering/lights_and_shadows/use_physical_light_units")) {
		exposure_normalization = calculate_exposure_normalization();
	}

	RS::get_singleton()->camera_attributes_set_exposure(camera_attributes, exposure_multiplier, exposure_normalization);
}

void CameraAttributes::set_auto_exposure_enabled(bool p_enabled) {
	auto_exposure_enabled = p_enabled;
	_update_auto_exposure();
	notify_property_list_changed();
	emit_changed();
}

bool CameraAttributes::is_auto_exposure_enabled() const {
	return auto_exposure_enabled;
}

void CameraAttributes::set_auto_exposure_speed(float p_auto_exposure_speed) {
	auto_exposure_speed = p_auto_exposure_speed;
	_update_auto_exposure();
	emit_changed();
}

float CameraAttributes::get_auto_exposure_speed() const {
	return auto_exposure_speed;
}

void CameraAttributes::set_auto_exposure_scale(float p_auto_exposure_scale) {
	auto_exposure_scale = p_auto_exposure_scale;
	_update_auto_exposure();
	emit_changed();
}

float CameraAttributes::get_auto_exposure_scale() const {
	return auto_exposure_scale;
}

RID CameraAttributes::get_rid() const {
	return camera_attributes;
}

void CameraAttributes::_validate_property(PropertyInfo &p_property) const {
	if (!GLOBAL_GET("rendering/lights_and_shadows/use_physical_light_units") && p_property.name == "exposure_sensitivity") {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL;
		return;
	}

	if (p_property.name.begins_with("auto_exposure_") && p_property.name != "auto_exposure_enabled" && !auto_exposure_enabled) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL;
		return;
	}
}

void CameraAttributes::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_exposure_multiplier", "multiplier"), &CameraAttributes::set_exposure_multiplier);
	ClassDB::bind_method(D_METHOD("get_exposure_multiplier"), &CameraAttributes::get_exposure_multiplier);
	ClassDB::bind_method(D_METHOD("set_exposure_sensitivity", "sensitivity"), &CameraAttributes::set_exposure_sensitivity);
	ClassDB::bind_method(D_METHOD("get_exposure_sensitivity"), &CameraAttributes::get_exposure_sensitivity);

	ClassDB::bind_method(D_METHOD("set_auto_exposure_enabled", "enabled"), &CameraAttributes::set_auto_exposure_enabled);
	ClassDB::bind_method(D_METHOD("is_auto_exposure_enabled"), &CameraAttributes::is_auto_exposure_enabled);
	ClassDB::bind_method(D_METHOD("set_auto_exposure_speed", "exposure_speed"), &CameraAttributes::set_auto_exposure_speed);
	ClassDB::bind_method(D_METHOD("get_auto_exposure_speed"), &CameraAttributes::get_auto_exposure_speed);
	ClassDB::bind_method(D_METHOD("set_auto_exposure_scale", "exposure_grey"), &CameraAttributes::set_auto_exposure_scale);
	ClassDB::bind_method(D_METHOD("get_auto_exposure_scale"), &CameraAttributes::get_auto_exposure_scale);

	ADD_GROUP("Exposure", "exposure_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "exposure_sensitivity", PROPERTY_HINT_RANGE, "0.1,32000.0,0.1,suffix:ISO"), "set_exposure_sensitivity", "get_exposure_sensitivity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "exposure_multiplier", PROPERTY_HINT_RANGE, "0.0,8.0,0.001,or_greater"), "set_exposure_multiplier", "get_exposure_multiplier");

	ADD_GROUP("Auto Exposure", "auto_exposure_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_exposure_enabled"), "set_auto_exposure_enabled", "is_auto_exposure_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "auto_exposure_scale", PROPERTY_HINT_RANGE, "0.01,64,0.01"), "set_auto_exposure_scale", "get_auto_exposure_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "auto_exposure_speed", PROPERTY_HINT_RANGE, "0.01,64,0.01"), "set_auto_exposure_speed", "get_auto_exposure_speed");
}

CameraAttributes::CameraAttributes() {
	camera_attributes = RS::get_singleton()->camera_attributes_create();
}

CameraAttributes::~CameraAttributes() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RS::get_singleton()->free(camera_attributes);
}

//////////////////////////////////////////////////////
/* CameraAttributesPractical */

void CameraAttributesPractical::set_dof_blur_far_enabled(bool p_enabled) {
	dof_blur_far_enabled = p_enabled;
	_update_dof_blur();
	notify_property_list_changed();
	emit_changed();
}

bool CameraAttributesPractical::is_dof_blur_far_enabled() const {
	return dof_blur_far_enabled;
}

void CameraAttributesPractical::set_dof_blur_far_distance(float p_distance) {
	dof_blur_far_distance = p_distance;
	_update_dof_blur();
	emit_changed();
}

float CameraAttributesPractical::get_dof_blur_far_distance() const {
	return dof_blur_far_distance;
}

void CameraAttributesPractical::set_dof_blur_far_transition(float p_distance) {
	dof_blur_far_transition = p_distance;
	_update_dof_blur();
	emit_changed();
}

float CameraAttributesPractical::get_dof_blur_far_transition() const {
	return dof_blur_far_transition;
}

void CameraAttributesPractical::set_dof_blur_near_enabled(bool p_enabled) {
	dof_blur_near_enabled = p_enabled;
	_update_dof_blur();
	notify_property_list_changed();
	emit_changed();
}

bool CameraAttributesPractical::is_dof_blur_near_enabled() const {
	return dof_blur_near_enabled;
}

void CameraAttributesPractical::set_dof_blur_near_distance(float p_distance) {
	dof_blur_near_distance = p_distance;
	_update_dof_blur();
	emit_changed();
}

float CameraAttributesPractical::get_dof_blur_near_distance() const {
	return dof_blur_near_distance;
}

void CameraAttributesPractical::set_dof_blur_near_transition(float p_distance) {
	dof_blur_near_transition = p_distance;
	_update_dof_blur();
	emit_changed();
}

float CameraAttributesPractical::get_dof_blur_near_transition() const {
	return dof_blur_near_transition;
}

void CameraAttributesPractical::set_dof_blur_amount(float p_amount) {
	dof_blur_amount = p_amount;
	_update_dof_blur();
	emit_changed();
}

float CameraAttributesPractical::get_dof_blur_amount() const {
	return dof_blur_amount;
}

void CameraAttributesPractical::_update_dof_blur() {
	RS::get_singleton()->camera_attributes_set_dof_blur(
			get_rid(),
			dof_blur_far_enabled,
			dof_blur_far_distance,
			dof_blur_far_transition,
			dof_blur_near_enabled,
			dof_blur_near_distance,
			dof_blur_near_transition,
			dof_blur_amount);
}

float CameraAttributesPractical::calculate_exposure_normalization() const {
	return exposure_sensitivity / 3072007.0; // Matches exposure normalization for default CameraAttributesPhysical at ISO 100.
}

void CameraAttributesPractical::set_auto_exposure_min_sensitivity(float p_min) {
	auto_exposure_min = p_min;
	_update_auto_exposure();
	emit_changed();
}

float CameraAttributesPractical::get_auto_exposure_min_sensitivity() const {
	return auto_exposure_min;
}

void CameraAttributesPractical::set_auto_exposure_max_sensitivity(float p_max) {
	auto_exposure_max = p_max;
	_update_auto_exposure();
	emit_changed();
}

float CameraAttributesPractical::get_auto_exposure_max_sensitivity() const {
	return auto_exposure_max;
}

void CameraAttributesPractical::_update_auto_exposure() {
	RS::get_singleton()->camera_attributes_set_auto_exposure(
			get_rid(),
			auto_exposure_enabled,
			auto_exposure_min * ((12.5 / 100.0) / exposure_sensitivity), // Convert from Sensitivity to Luminance
			auto_exposure_max * ((12.5 / 100.0) / exposure_sensitivity), // Convert from Sensitivity to Luminance
			auto_exposure_speed,
			auto_exposure_scale);
}

void CameraAttributesPractical::_validate_property(PropertyInfo &p_property) const {
	if ((!dof_blur_far_enabled && (p_property.name == "dof_blur_far_distance" || p_property.name == "dof_blur_far_transition")) ||
			(!dof_blur_near_enabled && (p_property.name == "dof_blur_near_distance" || p_property.name == "dof_blur_near_transition"))) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL;
	}
}

void CameraAttributesPractical::_bind_methods() {
	// DOF blur

	ClassDB::bind_method(D_METHOD("set_dof_blur_far_enabled", "enabled"), &CameraAttributesPractical::set_dof_blur_far_enabled);
	ClassDB::bind_method(D_METHOD("is_dof_blur_far_enabled"), &CameraAttributesPractical::is_dof_blur_far_enabled);
	ClassDB::bind_method(D_METHOD("set_dof_blur_far_distance", "distance"), &CameraAttributesPractical::set_dof_blur_far_distance);
	ClassDB::bind_method(D_METHOD("get_dof_blur_far_distance"), &CameraAttributesPractical::get_dof_blur_far_distance);
	ClassDB::bind_method(D_METHOD("set_dof_blur_far_transition", "distance"), &CameraAttributesPractical::set_dof_blur_far_transition);
	ClassDB::bind_method(D_METHOD("get_dof_blur_far_transition"), &CameraAttributesPractical::get_dof_blur_far_transition);

	ClassDB::bind_method(D_METHOD("set_dof_blur_near_enabled", "enabled"), &CameraAttributesPractical::set_dof_blur_near_enabled);
	ClassDB::bind_method(D_METHOD("is_dof_blur_near_enabled"), &CameraAttributesPractical::is_dof_blur_near_enabled);
	ClassDB::bind_method(D_METHOD("set_dof_blur_near_distance", "distance"), &CameraAttributesPractical::set_dof_blur_near_distance);
	ClassDB::bind_method(D_METHOD("get_dof_blur_near_distance"), &CameraAttributesPractical::get_dof_blur_near_distance);
	ClassDB::bind_method(D_METHOD("set_dof_blur_near_transition", "distance"), &CameraAttributesPractical::set_dof_blur_near_transition);
	ClassDB::bind_method(D_METHOD("get_dof_blur_near_transition"), &CameraAttributesPractical::get_dof_blur_near_transition);
	ClassDB::bind_method(D_METHOD("set_dof_blur_amount", "amount"), &CameraAttributesPractical::set_dof_blur_amount);
	ClassDB::bind_method(D_METHOD("get_dof_blur_amount"), &CameraAttributesPractical::get_dof_blur_amount);

	ClassDB::bind_method(D_METHOD("set_auto_exposure_max_sensitivity", "max_sensitivity"), &CameraAttributesPractical::set_auto_exposure_max_sensitivity);
	ClassDB::bind_method(D_METHOD("get_auto_exposure_max_sensitivity"), &CameraAttributesPractical::get_auto_exposure_max_sensitivity);
	ClassDB::bind_method(D_METHOD("set_auto_exposure_min_sensitivity", "min_sensitivity"), &CameraAttributesPractical::set_auto_exposure_min_sensitivity);
	ClassDB::bind_method(D_METHOD("get_auto_exposure_min_sensitivity"), &CameraAttributesPractical::get_auto_exposure_min_sensitivity);

	ADD_GROUP("DOF Blur", "dof_blur_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "dof_blur_far_enabled"), "set_dof_blur_far_enabled", "is_dof_blur_far_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "dof_blur_far_distance", PROPERTY_HINT_RANGE, "0.01,8192,0.01,exp,suffix:m"), "set_dof_blur_far_distance", "get_dof_blur_far_distance");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "dof_blur_far_transition", PROPERTY_HINT_RANGE, "-1,8192,0.01"), "set_dof_blur_far_transition", "get_dof_blur_far_transition");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "dof_blur_near_enabled"), "set_dof_blur_near_enabled", "is_dof_blur_near_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "dof_blur_near_distance", PROPERTY_HINT_RANGE, "0.01,8192,0.01,exp,suffix:m"), "set_dof_blur_near_distance", "get_dof_blur_near_distance");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "dof_blur_near_transition", PROPERTY_HINT_RANGE, "-1,8192,0.01"), "set_dof_blur_near_transition", "get_dof_blur_near_transition");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "dof_blur_amount", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_dof_blur_amount", "get_dof_blur_amount");

	ADD_GROUP("Auto Exposure", "auto_exposure_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "auto_exposure_min_sensitivity", PROPERTY_HINT_RANGE, "0,1600,0.01,or_greater,suffic:ISO"), "set_auto_exposure_min_sensitivity", "get_auto_exposure_min_sensitivity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "auto_exposure_max_sensitivity", PROPERTY_HINT_RANGE, "0,64000,0.1,or_greater,suffic:ISO"), "set_auto_exposure_max_sensitivity", "get_auto_exposure_max_sensitivity");
}

CameraAttributesPractical::CameraAttributesPractical() {
	_update_dof_blur();
	_update_exposure();
	auto_exposure_min = 0.0;
	auto_exposure_max = 800.0;
	_update_auto_exposure();
	notify_property_list_changed();
}

CameraAttributesPractical::~CameraAttributesPractical() {
}

//////////////////////////////////////////////////////
/* CameraAttributesPhysical */

void CameraAttributesPhysical::set_aperture(float p_aperture) {
	exposure_aperture = p_aperture;
	_update_exposure();
	_update_frustum();
}

float CameraAttributesPhysical::get_aperture() const {
	return exposure_aperture;
}

void CameraAttributesPhysical::set_shutter_speed(float p_shutter_speed) {
	exposure_shutter_speed = p_shutter_speed;
	_update_exposure();
}

float CameraAttributesPhysical::get_shutter_speed() const {
	return exposure_shutter_speed;
}

void CameraAttributesPhysical::set_focal_length(float p_focal_length) {
	frustum_focal_length = p_focal_length;
	_update_frustum();
	emit_changed();
}

float CameraAttributesPhysical::get_focal_length() const {
	return frustum_focal_length;
}

void CameraAttributesPhysical::set_focus_distance(float p_focus_distance) {
	frustum_focus_distance = p_focus_distance;
	_update_frustum();
	emit_changed();
}

float CameraAttributesPhysical::get_focus_distance() const {
	return frustum_focus_distance;
}

void CameraAttributesPhysical::set_near(real_t p_near) {
	frustum_near = p_near;
	_update_frustum();
	emit_changed();
}

real_t CameraAttributesPhysical::get_near() const {
	return frustum_near;
}

void CameraAttributesPhysical::set_far(real_t p_far) {
	frustum_far = p_far;
	_update_frustum();
	emit_changed();
}

real_t CameraAttributesPhysical::get_far() const {
	return frustum_far;
}

real_t CameraAttributesPhysical::get_fov() const {
	return frustum_fov;
}

void CameraAttributesPhysical::_update_frustum() {
	//https://en.wikipedia.org/wiki/Circle_of_confusion#Circle_of_confusion_diameter_limit_based_on_d/1500
	Vector2i sensor_size = Vector2i(36, 24); // Matches high-end DSLR, could be made variable if there is demand.
	float CoC = sensor_size.length() / 1500.0;

	frustum_fov = Math::rad_to_deg(2 * atan(sensor_size.height / (2 * frustum_focal_length)));

	// Based on https://en.wikipedia.org/wiki/Depth_of_field.
	float u = MAX(frustum_focus_distance * 1000.0, frustum_focal_length + 1.0); // Focus distance expressed in mm and clamped to at least 1 mm away from lens.
	float hyperfocal_length = frustum_focal_length + ((frustum_focal_length * frustum_focal_length) / (exposure_aperture * CoC));

	// This computes the start and end of the depth of field. Anything between these two points has a Circle of Confusino so small
	// that it is not picked up by the camera sensors.
	// To be properly physically-based, we would run the DoF shader at all depths. To be efficient, we are only running it where the CoC
	// will be visible, this introduces some value shifts in the near field that we have to compensate for below.
	float depth_near = ((hyperfocal_length * u) / (hyperfocal_length + (u - frustum_focal_length))) / 1000.0; // In meters.
	float depth_far = ((hyperfocal_length * u) / (hyperfocal_length - (u - frustum_focal_length))) / 1000.0; // In meters.
	float scale = (frustum_focal_length / (u - frustum_focal_length)) * (frustum_focal_length / exposure_aperture);

	bool use_far = (depth_far < frustum_far) && (depth_far > 0.0);
	bool use_near = depth_near > frustum_near;
#ifdef DEBUG_ENABLED
	if (OS::get_singleton()->get_current_rendering_method() == "gl_compatibility") {
		// Force disable DoF in editor builds to suppress warnings.
		use_far = false;
		use_near = false;
	}
#endif
	RS::get_singleton()->camera_attributes_set_dof_blur(
			get_rid(),
			use_far,
			u / 1000.0, // Focus distance clampd to focal length expressed in meters.
			-1.0, // Negative to tell Bokeh effect to use physically-based scaling.
			use_near,
			u / 1000.0,
			-1.0,
			scale / 5.0); // Arbitrary scaling to get close to how much blur there should be.
}

float CameraAttributesPhysical::calculate_exposure_normalization() const {
	const float e = (exposure_aperture * exposure_aperture) * exposure_shutter_speed * (100.0 / exposure_sensitivity);
	return 1.0 / (e * 1.2);
}

void CameraAttributesPhysical::set_auto_exposure_min_exposure_value(float p_min) {
	auto_exposure_min = p_min;
	_update_auto_exposure();
	emit_changed();
}

float CameraAttributesPhysical::get_auto_exposure_min_exposure_value() const {
	return auto_exposure_min;
}

void CameraAttributesPhysical::set_auto_exposure_max_exposure_value(float p_max) {
	auto_exposure_max = p_max;
	_update_auto_exposure();
	emit_changed();
}

float CameraAttributesPhysical::get_auto_exposure_max_exposure_value() const {
	return auto_exposure_max;
}

void CameraAttributesPhysical::_update_auto_exposure() {
	RS::get_singleton()->camera_attributes_set_auto_exposure(
			get_rid(),
			auto_exposure_enabled,
			pow(2.0, auto_exposure_min) * (12.5 / exposure_sensitivity), // Convert from EV100 to Luminance
			pow(2.0, auto_exposure_max) * (12.5 / exposure_sensitivity), // Convert from EV100 to Luminance
			auto_exposure_speed,
			auto_exposure_scale);
}

void CameraAttributesPhysical::_validate_property(PropertyInfo &property) const {
	if (!GLOBAL_GET("rendering/lights_and_shadows/use_physical_light_units") && (property.name == "exposure_aperture" || property.name == "exposure_shutter_speed")) {
		property.usage = PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL;
		return;
	}
}

void CameraAttributesPhysical::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_aperture", "aperture"), &CameraAttributesPhysical::set_aperture);
	ClassDB::bind_method(D_METHOD("get_aperture"), &CameraAttributesPhysical::get_aperture);
	ClassDB::bind_method(D_METHOD("set_shutter_speed", "shutter_speed"), &CameraAttributesPhysical::set_shutter_speed);
	ClassDB::bind_method(D_METHOD("get_shutter_speed"), &CameraAttributesPhysical::get_shutter_speed);

	ClassDB::bind_method(D_METHOD("set_focal_length", "focal_length"), &CameraAttributesPhysical::set_focal_length);
	ClassDB::bind_method(D_METHOD("get_focal_length"), &CameraAttributesPhysical::get_focal_length);
	ClassDB::bind_method(D_METHOD("set_focus_distance", "focus_distance"), &CameraAttributesPhysical::set_focus_distance);
	ClassDB::bind_method(D_METHOD("get_focus_distance"), &CameraAttributesPhysical::get_focus_distance);
	ClassDB::bind_method(D_METHOD("set_near", "near"), &CameraAttributesPhysical::set_near);
	ClassDB::bind_method(D_METHOD("get_near"), &CameraAttributesPhysical::get_near);
	ClassDB::bind_method(D_METHOD("set_far", "far"), &CameraAttributesPhysical::set_far);
	ClassDB::bind_method(D_METHOD("get_far"), &CameraAttributesPhysical::get_far);
	ClassDB::bind_method(D_METHOD("get_fov"), &CameraAttributesPhysical::get_fov);

	ClassDB::bind_method(D_METHOD("set_auto_exposure_max_exposure_value", "exposure_value_max"), &CameraAttributesPhysical::set_auto_exposure_max_exposure_value);
	ClassDB::bind_method(D_METHOD("get_auto_exposure_max_exposure_value"), &CameraAttributesPhysical::get_auto_exposure_max_exposure_value);
	ClassDB::bind_method(D_METHOD("set_auto_exposure_min_exposure_value", "exposure_value_min"), &CameraAttributesPhysical::set_auto_exposure_min_exposure_value);
	ClassDB::bind_method(D_METHOD("get_auto_exposure_min_exposure_value"), &CameraAttributesPhysical::get_auto_exposure_min_exposure_value);

	ADD_GROUP("Frustum", "frustum_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "frustum_focus_distance", PROPERTY_HINT_RANGE, "0.01,4000.0,0.01,suffix:m"), "set_focus_distance", "get_focus_distance");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "frustum_focal_length", PROPERTY_HINT_RANGE, "1.0,800.0,0.01,exp,suffix:mm"), "set_focal_length", "get_focal_length");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "frustum_near", PROPERTY_HINT_RANGE, "0.001,10,0.001,or_greater,exp,suffix:m"), "set_near", "get_near");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "frustum_far", PROPERTY_HINT_RANGE, "0.01,4000,0.01,or_greater,exp,suffix:m"), "set_far", "get_far");

	ADD_GROUP("Exposure", "exposure_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "exposure_aperture", PROPERTY_HINT_RANGE, "0.5,64.0,0.01,exp,suffix:f-stop"), "set_aperture", "get_aperture");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "exposure_shutter_speed", PROPERTY_HINT_RANGE, "0.1,8000.0,0.001,suffix:1/s"), "set_shutter_speed", "get_shutter_speed");

	ADD_GROUP("Auto Exposure", "auto_exposure_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "auto_exposure_min_exposure_value", PROPERTY_HINT_RANGE, "-16.0,16.0,0.01,or_greater,suffix:EV100"), "set_auto_exposure_min_exposure_value", "get_auto_exposure_min_exposure_value");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "auto_exposure_max_exposure_value", PROPERTY_HINT_RANGE, "-16.0,16.0,0.01,or_greater,suffix:EV100"), "set_auto_exposure_max_exposure_value", "get_auto_exposure_max_exposure_value");
}

CameraAttributesPhysical::CameraAttributesPhysical() {
	_update_exposure();
	_update_frustum();
	auto_exposure_min = -8;
	auto_exposure_min = 10; // Use a wide range by default to feel more like a real camera.
	_update_auto_exposure();
	notify_property_list_changed();
}

CameraAttributesPhysical::~CameraAttributesPhysical() {
}
