/**************************************************************************/
/*  fastnoise_lite.cpp                                                    */
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

#include "fastnoise_lite.h"

_FastNoiseLite::FractalType FastNoiseLite::_convert_domain_warp_fractal_type_enum(DomainWarpFractalType p_domain_warp_fractal_type) {
	_FastNoiseLite::FractalType type;
	switch (p_domain_warp_fractal_type) {
		case DOMAIN_WARP_FRACTAL_NONE:
			type = _FastNoiseLite::FractalType_None;
			break;
		case DOMAIN_WARP_FRACTAL_PROGRESSIVE:
			type = _FastNoiseLite::FractalType_DomainWarpProgressive;
			break;
		case DOMAIN_WARP_FRACTAL_INDEPENDENT:
			type = _FastNoiseLite::FractalType_DomainWarpIndependent;
			break;
		default:
			type = _FastNoiseLite::FractalType_None;
	}
	return type;
}

FastNoiseLite::FastNoiseLite() {
	_noise.SetNoiseType((_FastNoiseLite::NoiseType)noise_type);
	_noise.SetSeed(seed);
	_noise.SetFrequency(frequency);

	_noise.SetFractalType((_FastNoiseLite::FractalType)fractal_type);
	_noise.SetFractalOctaves(fractal_octaves);
	_noise.SetFractalLacunarity(fractal_lacunarity);
	_noise.SetFractalGain(fractal_gain);
	_noise.SetFractalWeightedStrength(fractal_weighted_strength);
	_noise.SetFractalPingPongStrength(fractal_ping_pong_strength);

	_noise.SetCellularDistanceFunction((_FastNoiseLite::CellularDistanceFunction)cellular_distance_function);
	_noise.SetCellularReturnType((_FastNoiseLite::CellularReturnType)cellular_return_type);
	_noise.SetCellularJitter(cellular_jitter);

	_domain_warp_noise.SetDomainWarpType((_FastNoiseLite::DomainWarpType)domain_warp_type);
	_domain_warp_noise.SetSeed(seed);
	_domain_warp_noise.SetDomainWarpAmp(domain_warp_amplitude);
	_domain_warp_noise.SetFrequency(domain_warp_frequency);
	_domain_warp_noise.SetFractalType(_convert_domain_warp_fractal_type_enum(domain_warp_fractal_type));
	_domain_warp_noise.SetFractalOctaves(domain_warp_fractal_octaves);
	_domain_warp_noise.SetFractalLacunarity(domain_warp_fractal_lacunarity);
	_domain_warp_noise.SetFractalGain(domain_warp_fractal_gain);
}

FastNoiseLite::~FastNoiseLite() {
}

// General settings.

void FastNoiseLite::set_noise_type(NoiseType p_noise_type) {
	noise_type = p_noise_type;
	_noise.SetNoiseType((_FastNoiseLite::NoiseType)p_noise_type);
	emit_changed();
	notify_property_list_changed();
}

FastNoiseLite::NoiseType FastNoiseLite::get_noise_type() const {
	return noise_type;
}

void FastNoiseLite::set_seed(int p_seed) {
	seed = p_seed;
	_noise.SetSeed(p_seed);
	_domain_warp_noise.SetSeed(p_seed);
	emit_changed();
}

int FastNoiseLite::get_seed() const {
	return seed;
}

void FastNoiseLite::set_frequency(real_t p_freq) {
	frequency = p_freq;
	_noise.SetFrequency(p_freq);
	emit_changed();
}

real_t FastNoiseLite::get_frequency() const {
	return frequency;
}

void FastNoiseLite::set_offset(Vector3 p_offset) {
	offset = p_offset;
	emit_changed();
}

Vector3 FastNoiseLite::get_offset() const {
	return offset;
}

// Fractal.

void FastNoiseLite::set_fractal_type(FractalType p_type) {
	fractal_type = p_type;
	_noise.SetFractalType((_FastNoiseLite::FractalType)p_type);
	emit_changed();
	notify_property_list_changed();
}

FastNoiseLite::FractalType FastNoiseLite::get_fractal_type() const {
	return fractal_type;
}

void FastNoiseLite::set_fractal_octaves(int p_octaves) {
	fractal_octaves = p_octaves;
	_noise.SetFractalOctaves(p_octaves);
	emit_changed();
}

int FastNoiseLite::get_fractal_octaves() const {
	return fractal_octaves;
}

void FastNoiseLite::set_fractal_lacunarity(real_t p_lacunarity) {
	fractal_lacunarity = p_lacunarity;
	_noise.SetFractalLacunarity(p_lacunarity);
	emit_changed();
}

real_t FastNoiseLite::get_fractal_lacunarity() const {
	return fractal_lacunarity;
}

void FastNoiseLite::set_fractal_gain(real_t p_gain) {
	fractal_gain = p_gain;
	_noise.SetFractalGain(p_gain);
	emit_changed();
}

real_t FastNoiseLite::get_fractal_gain() const {
	return fractal_gain;
}

void FastNoiseLite::set_fractal_weighted_strength(real_t p_weighted_strength) {
	fractal_weighted_strength = p_weighted_strength;
	_noise.SetFractalWeightedStrength(p_weighted_strength);
	emit_changed();
}
real_t FastNoiseLite::get_fractal_weighted_strength() const {
	return fractal_weighted_strength;
}

void FastNoiseLite::set_fractal_ping_pong_strength(real_t p_ping_pong_strength) {
	fractal_ping_pong_strength = p_ping_pong_strength;
	_noise.SetFractalPingPongStrength(p_ping_pong_strength);
	emit_changed();
}
real_t FastNoiseLite::get_fractal_ping_pong_strength() const {
	return fractal_ping_pong_strength;
}

// Cellular.

void FastNoiseLite::set_cellular_distance_function(CellularDistanceFunction p_func) {
	cellular_distance_function = p_func;
	_noise.SetCellularDistanceFunction((_FastNoiseLite::CellularDistanceFunction)p_func);
	emit_changed();
}

FastNoiseLite::CellularDistanceFunction FastNoiseLite::get_cellular_distance_function() const {
	return cellular_distance_function;
}

void FastNoiseLite::set_cellular_jitter(real_t p_jitter) {
	cellular_jitter = p_jitter;
	_noise.SetCellularJitter(p_jitter);
	emit_changed();
}

real_t FastNoiseLite::get_cellular_jitter() const {
	return cellular_jitter;
}

void FastNoiseLite::set_cellular_return_type(CellularReturnType p_ret) {
	cellular_return_type = p_ret;
	_noise.SetCellularReturnType((_FastNoiseLite::CellularReturnType)p_ret);
	emit_changed();
}

FastNoiseLite::CellularReturnType FastNoiseLite::get_cellular_return_type() const {
	return cellular_return_type;
}

// Domain warp specific.

void FastNoiseLite::set_domain_warp_enabled(bool p_enabled) {
	if (domain_warp_enabled != p_enabled) {
		domain_warp_enabled = p_enabled;
		emit_changed();
		notify_property_list_changed();
	}
}

bool FastNoiseLite::is_domain_warp_enabled() const {
	return domain_warp_enabled;
}

void FastNoiseLite::set_domain_warp_type(DomainWarpType p_domain_warp_type) {
	domain_warp_type = p_domain_warp_type;
	_domain_warp_noise.SetDomainWarpType((_FastNoiseLite::DomainWarpType)p_domain_warp_type);
	emit_changed();
}

FastNoiseLite::DomainWarpType FastNoiseLite::get_domain_warp_type() const {
	return domain_warp_type;
}

void FastNoiseLite::set_domain_warp_amplitude(real_t p_amplitude) {
	domain_warp_amplitude = p_amplitude;
	_domain_warp_noise.SetDomainWarpAmp(p_amplitude);
	emit_changed();
}
real_t FastNoiseLite::get_domain_warp_amplitude() const {
	return domain_warp_amplitude;
}

void FastNoiseLite::set_domain_warp_frequency(real_t p_frequency) {
	domain_warp_frequency = p_frequency;
	_domain_warp_noise.SetFrequency(p_frequency);
	emit_changed();
}

real_t FastNoiseLite::get_domain_warp_frequency() const {
	return domain_warp_frequency;
}

void FastNoiseLite::set_domain_warp_fractal_type(DomainWarpFractalType p_domain_warp_fractal_type) {
	domain_warp_fractal_type = p_domain_warp_fractal_type;

	_domain_warp_noise.SetFractalType(_convert_domain_warp_fractal_type_enum(p_domain_warp_fractal_type));
	emit_changed();
	notify_property_list_changed();
}

FastNoiseLite::DomainWarpFractalType FastNoiseLite::get_domain_warp_fractal_type() const {
	return domain_warp_fractal_type;
}

void FastNoiseLite::set_domain_warp_fractal_octaves(int p_octaves) {
	domain_warp_fractal_octaves = p_octaves;
	_domain_warp_noise.SetFractalOctaves(p_octaves);
	emit_changed();
}

int FastNoiseLite::get_domain_warp_fractal_octaves() const {
	return domain_warp_fractal_octaves;
}

void FastNoiseLite::set_domain_warp_fractal_lacunarity(real_t p_lacunarity) {
	domain_warp_fractal_lacunarity = p_lacunarity;
	_domain_warp_noise.SetFractalLacunarity(p_lacunarity);
	emit_changed();
}

real_t FastNoiseLite::get_domain_warp_fractal_lacunarity() const {
	return domain_warp_fractal_lacunarity;
}

void FastNoiseLite::set_domain_warp_fractal_gain(real_t p_gain) {
	domain_warp_fractal_gain = p_gain;
	_domain_warp_noise.SetFractalGain(p_gain);
	emit_changed();
}

real_t FastNoiseLite::get_domain_warp_fractal_gain() const {
	return domain_warp_fractal_gain;
}

// Noise interface functions.

real_t FastNoiseLite::get_noise_1d(real_t p_x) const {
	p_x += offset.x;
	if (domain_warp_enabled) {
		// Needed since DomainWarp expects a reference.
		real_t y_dummy = 0;
		_domain_warp_noise.DomainWarp(p_x, y_dummy);
	}
	return get_noise_2d(p_x, 0.0);
}

real_t FastNoiseLite::get_noise_2dv(Vector2 p_v) const {
	return get_noise_2d(p_v.x, p_v.y);
}

real_t FastNoiseLite::get_noise_2d(real_t p_x, real_t p_y) const {
	p_x += offset.x;
	p_y += offset.y;
	if (domain_warp_enabled) {
		_domain_warp_noise.DomainWarp(p_x, p_y);
	}
	return _noise.GetNoise(p_x, p_y);
}

real_t FastNoiseLite::get_noise_3dv(Vector3 p_v) const {
	return get_noise_3d(p_v.x, p_v.y, p_v.z);
}

real_t FastNoiseLite::get_noise_3d(real_t p_x, real_t p_y, real_t p_z) const {
	p_x += offset.x;
	p_y += offset.y;
	p_z += offset.z;
	if (domain_warp_enabled) {
		_domain_warp_noise.DomainWarp(p_x, p_y, p_z);
	}
	return _noise.GetNoise(p_x, p_y, p_z);
}

void FastNoiseLite::_changed() {
	emit_changed();
}

void FastNoiseLite::_bind_methods() {
	// General settings.

	ClassDB::bind_method(D_METHOD("set_noise_type", "type"), &FastNoiseLite::set_noise_type);
	ClassDB::bind_method(D_METHOD("get_noise_type"), &FastNoiseLite::get_noise_type);

	ClassDB::bind_method(D_METHOD("set_seed", "seed"), &FastNoiseLite::set_seed);
	ClassDB::bind_method(D_METHOD("get_seed"), &FastNoiseLite::get_seed);

	ClassDB::bind_method(D_METHOD("set_frequency", "freq"), &FastNoiseLite::set_frequency);
	ClassDB::bind_method(D_METHOD("get_frequency"), &FastNoiseLite::get_frequency);

	ClassDB::bind_method(D_METHOD("set_offset", "offset"), &FastNoiseLite::set_offset);
	ClassDB::bind_method(D_METHOD("get_offset"), &FastNoiseLite::get_offset);

	// Fractal.

	ClassDB::bind_method(D_METHOD("set_fractal_type", "type"), &FastNoiseLite::set_fractal_type);
	ClassDB::bind_method(D_METHOD("get_fractal_type"), &FastNoiseLite::get_fractal_type);

	ClassDB::bind_method(D_METHOD("set_fractal_octaves", "octave_count"), &FastNoiseLite::set_fractal_octaves);
	ClassDB::bind_method(D_METHOD("get_fractal_octaves"), &FastNoiseLite::get_fractal_octaves);

	ClassDB::bind_method(D_METHOD("set_fractal_lacunarity", "lacunarity"), &FastNoiseLite::set_fractal_lacunarity);
	ClassDB::bind_method(D_METHOD("get_fractal_lacunarity"), &FastNoiseLite::get_fractal_lacunarity);

	ClassDB::bind_method(D_METHOD("set_fractal_gain", "gain"), &FastNoiseLite::set_fractal_gain);
	ClassDB::bind_method(D_METHOD("get_fractal_gain"), &FastNoiseLite::get_fractal_gain);

	ClassDB::bind_method(D_METHOD("set_fractal_weighted_strength", "weighted_strength"), &FastNoiseLite::set_fractal_weighted_strength);
	ClassDB::bind_method(D_METHOD("get_fractal_weighted_strength"), &FastNoiseLite::get_fractal_weighted_strength);

	ClassDB::bind_method(D_METHOD("set_fractal_ping_pong_strength", "ping_pong_strength"), &FastNoiseLite::set_fractal_ping_pong_strength);
	ClassDB::bind_method(D_METHOD("get_fractal_ping_pong_strength"), &FastNoiseLite::get_fractal_ping_pong_strength);

	// Cellular.

	ClassDB::bind_method(D_METHOD("set_cellular_distance_function", "func"), &FastNoiseLite::set_cellular_distance_function);
	ClassDB::bind_method(D_METHOD("get_cellular_distance_function"), &FastNoiseLite::get_cellular_distance_function);

	ClassDB::bind_method(D_METHOD("set_cellular_jitter", "jitter"), &FastNoiseLite::set_cellular_jitter);
	ClassDB::bind_method(D_METHOD("get_cellular_jitter"), &FastNoiseLite::get_cellular_jitter);

	ClassDB::bind_method(D_METHOD("set_cellular_return_type", "ret"), &FastNoiseLite::set_cellular_return_type);
	ClassDB::bind_method(D_METHOD("get_cellular_return_type"), &FastNoiseLite::get_cellular_return_type);

	// Domain warp.

	ClassDB::bind_method(D_METHOD("set_domain_warp_enabled", "domain_warp_enabled"), &FastNoiseLite::set_domain_warp_enabled);
	ClassDB::bind_method(D_METHOD("is_domain_warp_enabled"), &FastNoiseLite::is_domain_warp_enabled);

	ClassDB::bind_method(D_METHOD("set_domain_warp_type", "domain_warp_type"), &FastNoiseLite::set_domain_warp_type);
	ClassDB::bind_method(D_METHOD("get_domain_warp_type"), &FastNoiseLite::get_domain_warp_type);

	ClassDB::bind_method(D_METHOD("set_domain_warp_amplitude", "domain_warp_amplitude"), &FastNoiseLite::set_domain_warp_amplitude);
	ClassDB::bind_method(D_METHOD("get_domain_warp_amplitude"), &FastNoiseLite::get_domain_warp_amplitude);

	ClassDB::bind_method(D_METHOD("set_domain_warp_frequency", "domain_warp_frequency"), &FastNoiseLite::set_domain_warp_frequency);
	ClassDB::bind_method(D_METHOD("get_domain_warp_frequency"), &FastNoiseLite::get_domain_warp_frequency);

	ClassDB::bind_method(D_METHOD("set_domain_warp_fractal_type", "domain_warp_fractal_type"), &FastNoiseLite::set_domain_warp_fractal_type);
	ClassDB::bind_method(D_METHOD("get_domain_warp_fractal_type"), &FastNoiseLite::get_domain_warp_fractal_type);

	ClassDB::bind_method(D_METHOD("set_domain_warp_fractal_octaves", "domain_warp_octave_count"), &FastNoiseLite::set_domain_warp_fractal_octaves);
	ClassDB::bind_method(D_METHOD("get_domain_warp_fractal_octaves"), &FastNoiseLite::get_domain_warp_fractal_octaves);

	ClassDB::bind_method(D_METHOD("set_domain_warp_fractal_lacunarity", "domain_warp_lacunarity"), &FastNoiseLite::set_domain_warp_fractal_lacunarity);
	ClassDB::bind_method(D_METHOD("get_domain_warp_fractal_lacunarity"), &FastNoiseLite::get_domain_warp_fractal_lacunarity);

	ClassDB::bind_method(D_METHOD("set_domain_warp_fractal_gain", "domain_warp_gain"), &FastNoiseLite::set_domain_warp_fractal_gain);
	ClassDB::bind_method(D_METHOD("get_domain_warp_fractal_gain"), &FastNoiseLite::get_domain_warp_fractal_gain);

	ClassDB::bind_method(D_METHOD("_changed"), &FastNoiseLite::_changed);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "noise_type", PROPERTY_HINT_ENUM, "Simplex,Simplex Smooth,Cellular,Perlin,Value Cubic,Value"), "set_noise_type", "get_noise_type");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "frequency", PROPERTY_HINT_RANGE, ".0001,1,.0001,exp"), "set_frequency", "get_frequency");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "offset", PROPERTY_HINT_RANGE, "-1000,1000,0.01,or_less,or_greater"), "set_offset", "get_offset");

	ADD_GROUP("Fractal", "fractal_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "fractal_type", PROPERTY_HINT_ENUM, "None,FBM,Ridged,Ping-Pong"), "set_fractal_type", "get_fractal_type");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "fractal_octaves", PROPERTY_HINT_RANGE, "1,10,1"), "set_fractal_octaves", "get_fractal_octaves");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fractal_lacunarity"), "set_fractal_lacunarity", "get_fractal_lacunarity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fractal_gain"), "set_fractal_gain", "get_fractal_gain");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fractal_weighted_strength", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_fractal_weighted_strength", "get_fractal_weighted_strength");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fractal_ping_pong_strength"), "set_fractal_ping_pong_strength", "get_fractal_ping_pong_strength");

	ADD_GROUP("Cellular", "cellular_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cellular_distance_function", PROPERTY_HINT_ENUM, "Euclidean,Euclidean Squared,Manhattan,Hybrid"), "set_cellular_distance_function", "get_cellular_distance_function");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cellular_jitter"), "set_cellular_jitter", "get_cellular_jitter");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cellular_return_type", PROPERTY_HINT_ENUM, "Cell Value,Distance,Distance2,Distance2Add,Distance2Sub,Distance2Mul,Distance2Div"), "set_cellular_return_type", "get_cellular_return_type");

	ADD_GROUP("Domain Warp", "domain_warp_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "domain_warp_enabled"), "set_domain_warp_enabled", "is_domain_warp_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "domain_warp_type", PROPERTY_HINT_ENUM, "Simplex,Simplex Reduced,Basic Grid"), "set_domain_warp_type", "get_domain_warp_type");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "domain_warp_amplitude"), "set_domain_warp_amplitude", "get_domain_warp_amplitude");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "domain_warp_frequency"), "set_domain_warp_frequency", "get_domain_warp_frequency");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "domain_warp_fractal_type", PROPERTY_HINT_ENUM, "None,Progressive,Independent"), "set_domain_warp_fractal_type", "get_domain_warp_fractal_type");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "domain_warp_fractal_octaves", PROPERTY_HINT_RANGE, "1,10,1"), "set_domain_warp_fractal_octaves", "get_domain_warp_fractal_octaves");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "domain_warp_fractal_lacunarity"), "set_domain_warp_fractal_lacunarity", "get_domain_warp_fractal_lacunarity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "domain_warp_fractal_gain"), "set_domain_warp_fractal_gain", "get_domain_warp_fractal_gain");

	BIND_ENUM_CONSTANT(TYPE_VALUE);
	BIND_ENUM_CONSTANT(TYPE_VALUE_CUBIC);
	BIND_ENUM_CONSTANT(TYPE_PERLIN);
	BIND_ENUM_CONSTANT(TYPE_CELLULAR);
	BIND_ENUM_CONSTANT(TYPE_SIMPLEX);
	BIND_ENUM_CONSTANT(TYPE_SIMPLEX_SMOOTH);

	BIND_ENUM_CONSTANT(FRACTAL_NONE);
	BIND_ENUM_CONSTANT(FRACTAL_FBM);
	BIND_ENUM_CONSTANT(FRACTAL_RIDGED);
	BIND_ENUM_CONSTANT(FRACTAL_PING_PONG);

	BIND_ENUM_CONSTANT(DISTANCE_EUCLIDEAN);
	BIND_ENUM_CONSTANT(DISTANCE_EUCLIDEAN_SQUARED);
	BIND_ENUM_CONSTANT(DISTANCE_MANHATTAN);
	BIND_ENUM_CONSTANT(DISTANCE_HYBRID);

	BIND_ENUM_CONSTANT(RETURN_CELL_VALUE);
	BIND_ENUM_CONSTANT(RETURN_DISTANCE);
	BIND_ENUM_CONSTANT(RETURN_DISTANCE2);
	BIND_ENUM_CONSTANT(RETURN_DISTANCE2_ADD);
	BIND_ENUM_CONSTANT(RETURN_DISTANCE2_SUB);
	BIND_ENUM_CONSTANT(RETURN_DISTANCE2_MUL);
	BIND_ENUM_CONSTANT(RETURN_DISTANCE2_DIV);

	BIND_ENUM_CONSTANT(DOMAIN_WARP_SIMPLEX);
	BIND_ENUM_CONSTANT(DOMAIN_WARP_SIMPLEX_REDUCED);
	BIND_ENUM_CONSTANT(DOMAIN_WARP_BASIC_GRID);

	BIND_ENUM_CONSTANT(DOMAIN_WARP_FRACTAL_NONE);
	BIND_ENUM_CONSTANT(DOMAIN_WARP_FRACTAL_PROGRESSIVE);
	BIND_ENUM_CONSTANT(DOMAIN_WARP_FRACTAL_INDEPENDENT);
}

void FastNoiseLite::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name.begins_with("cellular") && get_noise_type() != TYPE_CELLULAR) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		return;
	}

	if (p_property.name != "fractal_type" && p_property.name.begins_with("fractal") && get_fractal_type() == FRACTAL_NONE) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		return;
	}

	if (p_property.name == "fractal_ping_pong_strength" && get_fractal_type() != FRACTAL_PING_PONG) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		return;
	}

	if (p_property.name != "domain_warp_enabled" && p_property.name.begins_with("domain_warp") && !domain_warp_enabled) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		return;
	}

	if (p_property.name == "domain_warp_fractal_lacunarity" && domain_warp_fractal_type == DOMAIN_WARP_FRACTAL_NONE) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		return;
	}
}
