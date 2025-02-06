/**************************************************************************/
/*  gltf_texture_sampler.h                                                */
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

#include "scene/resources/material.h"

class GLTFTextureSampler : public Resource {
	GDCLASS(GLTFTextureSampler, Resource);

public:
	enum FilterMode {
		NEAREST = 9728,
		LINEAR = 9729,
		NEAREST_MIPMAP_NEAREST = 9984,
		LINEAR_MIPMAP_NEAREST = 9985,
		NEAREST_MIPMAP_LINEAR = 9986,
		LINEAR_MIPMAP_LINEAR = 9987
	};

	enum WrapMode {
		CLAMP_TO_EDGE = 33071,
		MIRRORED_REPEAT = 33648,
		REPEAT = 10497,
		DEFAULT = REPEAT
	};

	int get_mag_filter() const {
		return mag_filter;
	}

	void set_mag_filter(const int filter_mode) {
		mag_filter = (FilterMode)filter_mode;
	}

	int get_min_filter() const {
		return min_filter;
	}

	void set_min_filter(const int filter_mode) {
		min_filter = (FilterMode)filter_mode;
	}

	int get_wrap_s() const {
		return wrap_s;
	}

	void set_wrap_s(const int wrap_mode) {
		wrap_s = (WrapMode)wrap_mode;
	}

	int get_wrap_t() const {
		return wrap_t;
	}

	void set_wrap_t(const int wrap_mode) {
		wrap_s = (WrapMode)wrap_mode;
	}

	StandardMaterial3D::TextureFilter get_filter_mode() const {
		using TextureFilter = StandardMaterial3D::TextureFilter;

		switch (min_filter) {
			case NEAREST:
				return TextureFilter::TEXTURE_FILTER_NEAREST;
			case LINEAR:
				return TextureFilter::TEXTURE_FILTER_LINEAR;
			case NEAREST_MIPMAP_NEAREST:
			case NEAREST_MIPMAP_LINEAR:
				return TextureFilter::TEXTURE_FILTER_NEAREST_WITH_MIPMAPS;
			case LINEAR_MIPMAP_NEAREST:
			case LINEAR_MIPMAP_LINEAR:
			default:
				return TextureFilter::TEXTURE_FILTER_LINEAR_WITH_MIPMAPS;
		}
	}

	void set_filter_mode(StandardMaterial3D::TextureFilter mode) {
		using TextureFilter = StandardMaterial3D::TextureFilter;

		switch (mode) {
			case TextureFilter::TEXTURE_FILTER_NEAREST:
				min_filter = FilterMode::NEAREST;
				mag_filter = FilterMode::NEAREST;
				break;
			case TextureFilter::TEXTURE_FILTER_LINEAR:
				min_filter = FilterMode::LINEAR;
				mag_filter = FilterMode::LINEAR;
				break;
			case TextureFilter::TEXTURE_FILTER_NEAREST_WITH_MIPMAPS:
			case TextureFilter::TEXTURE_FILTER_NEAREST_WITH_MIPMAPS_ANISOTROPIC:
				min_filter = FilterMode::NEAREST_MIPMAP_LINEAR;
				mag_filter = FilterMode::NEAREST;
				break;
			case TextureFilter::TEXTURE_FILTER_LINEAR_WITH_MIPMAPS:
			case TextureFilter::TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC:
			default:
				min_filter = FilterMode::LINEAR_MIPMAP_LINEAR;
				mag_filter = FilterMode::LINEAR;
				break;
		}
	}

	bool get_wrap_mode() const {
		// BaseMaterial3D presents wrapping as a boolean property. Either the texture is repeated
		// in both dimensions, non-mirrored, or it isn't repeated at all. This will cause oddities
		// when people import models having other wrapping mode combinations.
		return (wrap_s == WrapMode::REPEAT) && (wrap_t == WrapMode::REPEAT);
	}

	void set_wrap_mode(bool mat_repeats) {
		if (mat_repeats) {
			wrap_s = WrapMode::REPEAT;
			wrap_t = WrapMode::REPEAT;
		} else {
			wrap_s = WrapMode::CLAMP_TO_EDGE;
			wrap_t = WrapMode::CLAMP_TO_EDGE;
		}
	}

protected:
	static void _bind_methods();

private:
	FilterMode mag_filter = FilterMode::LINEAR;
	FilterMode min_filter = FilterMode::LINEAR_MIPMAP_LINEAR;
	WrapMode wrap_s = WrapMode::REPEAT;
	WrapMode wrap_t = WrapMode::REPEAT;
};

VARIANT_ENUM_CAST(GLTFTextureSampler::FilterMode);
VARIANT_ENUM_CAST(GLTFTextureSampler::WrapMode);
