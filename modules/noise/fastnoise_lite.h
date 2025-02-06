/**************************************************************************/
/*  fastnoise_lite.h                                                      */
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

#pragma once

#include "noise.h"

#include "thirdparty/misc/FastNoiseLite.h"

typedef fastnoiselite::FastNoiseLite _FastNoiseLite;

class FastNoiseLite : public Noise {
	GDCLASS(FastNoiseLite, Noise);
	OBJ_SAVE_TYPE(FastNoiseLite);

public:
	enum NoiseType {
		TYPE_SIMPLEX = _FastNoiseLite::NoiseType_OpenSimplex2,
		TYPE_SIMPLEX_SMOOTH = _FastNoiseLite::NoiseType_OpenSimplex2S,
		TYPE_CELLULAR = _FastNoiseLite::NoiseType_Cellular,
		TYPE_PERLIN = _FastNoiseLite::NoiseType_Perlin,
		TYPE_VALUE_CUBIC = _FastNoiseLite::NoiseType_ValueCubic,
		TYPE_VALUE = _FastNoiseLite::NoiseType_Value,
	};

	enum FractalType {
		FRACTAL_NONE = _FastNoiseLite::FractalType_None,
		FRACTAL_FBM = _FastNoiseLite::FractalType_FBm,
		FRACTAL_RIDGED = _FastNoiseLite::FractalType_Ridged,
		FRACTAL_PING_PONG = _FastNoiseLite::FractalType_PingPong,
	};

	enum CellularDistanceFunction {
		DISTANCE_EUCLIDEAN = _FastNoiseLite::CellularDistanceFunction_Euclidean,
		DISTANCE_EUCLIDEAN_SQUARED = _FastNoiseLite::CellularDistanceFunction_EuclideanSq,
		DISTANCE_MANHATTAN = _FastNoiseLite::CellularDistanceFunction_Manhattan,
		DISTANCE_HYBRID = _FastNoiseLite::CellularDistanceFunction_Hybrid
	};

	enum CellularReturnType {
		RETURN_CELL_VALUE = _FastNoiseLite::CellularReturnType_CellValue,
		RETURN_DISTANCE = _FastNoiseLite::CellularReturnType_Distance,
		RETURN_DISTANCE2 = _FastNoiseLite::CellularReturnType_Distance2,
		RETURN_DISTANCE2_ADD = _FastNoiseLite::CellularReturnType_Distance2Add,
		RETURN_DISTANCE2_SUB = _FastNoiseLite::CellularReturnType_Distance2Sub,
		RETURN_DISTANCE2_MUL = _FastNoiseLite::CellularReturnType_Distance2Mul,
		RETURN_DISTANCE2_DIV = _FastNoiseLite::CellularReturnType_Distance2Div
	};

	enum DomainWarpType {
		DOMAIN_WARP_SIMPLEX = _FastNoiseLite::DomainWarpType_OpenSimplex2,
		DOMAIN_WARP_SIMPLEX_REDUCED = _FastNoiseLite::DomainWarpType_OpenSimplex2Reduced,
		DOMAIN_WARP_BASIC_GRID = _FastNoiseLite::DomainWarpType_BasicGrid
	};

	enum DomainWarpFractalType {
		DOMAIN_WARP_FRACTAL_NONE,
		DOMAIN_WARP_FRACTAL_PROGRESSIVE,
		DOMAIN_WARP_FRACTAL_INDEPENDENT
	};

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

private:
	_FastNoiseLite _noise;
	_FastNoiseLite _domain_warp_noise;

	Vector3 offset;
	NoiseType noise_type = TYPE_SIMPLEX_SMOOTH;

	int seed = 0;
	real_t frequency = 0.01;

	// Fractal specific.
	FractalType fractal_type = FRACTAL_FBM;
	int fractal_octaves = 5;
	real_t fractal_lacunarity = 2;
	real_t fractal_gain = 0.5;
	real_t fractal_weighted_strength = 0;
	real_t fractal_ping_pong_strength = 2;

	// Cellular specific.
	CellularDistanceFunction cellular_distance_function = DISTANCE_EUCLIDEAN;
	CellularReturnType cellular_return_type = RETURN_DISTANCE;
	real_t cellular_jitter = 1.0;

	// Domain warp specific.
	bool domain_warp_enabled = false;
	DomainWarpType domain_warp_type = DOMAIN_WARP_SIMPLEX;
	real_t domain_warp_amplitude = 30.0;
	real_t domain_warp_frequency = 0.05;
	DomainWarpFractalType domain_warp_fractal_type = DOMAIN_WARP_FRACTAL_PROGRESSIVE;
	int domain_warp_fractal_octaves = 5;
	real_t domain_warp_fractal_lacunarity = 6;
	real_t domain_warp_fractal_gain = 0.5;

	// This needs manual conversion because Godots Inspector property API does not support discontiguous enum indices.
	_FastNoiseLite::FractalType _convert_domain_warp_fractal_type_enum(DomainWarpFractalType p_domain_warp_fractal_type);

public:
	FastNoiseLite();
	~FastNoiseLite();

	// General noise settings.

	void set_noise_type(NoiseType p_noise_type);
	NoiseType get_noise_type() const;

	void set_seed(int p_seed);
	int get_seed() const;

	void set_frequency(real_t p_freq);
	real_t get_frequency() const;

	void set_offset(Vector3 p_offset);
	Vector3 get_offset() const;

	// Fractal specific.

	void set_fractal_type(FractalType p_type);
	FractalType get_fractal_type() const;

	void set_fractal_octaves(int p_octaves);
	int get_fractal_octaves() const;

	void set_fractal_lacunarity(real_t p_lacunarity);
	real_t get_fractal_lacunarity() const;

	void set_fractal_gain(real_t p_gain);
	real_t get_fractal_gain() const;

	void set_fractal_weighted_strength(real_t p_weighted_strength);
	real_t get_fractal_weighted_strength() const;

	void set_fractal_ping_pong_strength(real_t p_ping_pong_strength);
	real_t get_fractal_ping_pong_strength() const;

	// Cellular specific.

	void set_cellular_distance_function(CellularDistanceFunction p_func);
	CellularDistanceFunction get_cellular_distance_function() const;

	void set_cellular_return_type(CellularReturnType p_ret);
	CellularReturnType get_cellular_return_type() const;

	void set_cellular_jitter(real_t p_jitter);
	real_t get_cellular_jitter() const;

	// Domain warp specific.

	void set_domain_warp_enabled(bool p_enabled);
	bool is_domain_warp_enabled() const;

	void set_domain_warp_type(DomainWarpType p_domain_warp_type);
	DomainWarpType get_domain_warp_type() const;

	void set_domain_warp_amplitude(real_t p_amplitude);
	real_t get_domain_warp_amplitude() const;

	void set_domain_warp_frequency(real_t p_frequency);
	real_t get_domain_warp_frequency() const;

	void set_domain_warp_fractal_type(DomainWarpFractalType p_domain_warp_fractal_type);
	DomainWarpFractalType get_domain_warp_fractal_type() const;

	void set_domain_warp_fractal_octaves(int p_octaves);
	int get_domain_warp_fractal_octaves() const;

	void set_domain_warp_fractal_lacunarity(real_t p_lacunarity);
	real_t get_domain_warp_fractal_lacunarity() const;

	void set_domain_warp_fractal_gain(real_t p_gain);
	real_t get_domain_warp_fractal_gain() const;

	// Interface methods.
	real_t get_noise_1d(real_t p_x) const override;

	real_t get_noise_2dv(Vector2 p_v) const override;
	real_t get_noise_2d(real_t p_x, real_t p_y) const override;

	real_t get_noise_3dv(Vector3 p_v) const override;
	real_t get_noise_3d(real_t p_x, real_t p_y, real_t p_z) const override;

	void _changed();
};

VARIANT_ENUM_CAST(FastNoiseLite::NoiseType);
VARIANT_ENUM_CAST(FastNoiseLite::FractalType);
VARIANT_ENUM_CAST(FastNoiseLite::CellularDistanceFunction);
VARIANT_ENUM_CAST(FastNoiseLite::CellularReturnType);
VARIANT_ENUM_CAST(FastNoiseLite::DomainWarpType);
VARIANT_ENUM_CAST(FastNoiseLite::DomainWarpFractalType);
