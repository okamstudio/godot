/**************************************************************************/
/*  xr_vrs.cpp                                                            */
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

#include "xr_vrs.h"

#include "servers/rendering/renderer_scene_render.h"
#include "servers/rendering_server.h"

void XRVRS::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_vrs_min_radius"), &XRVRS::get_vrs_min_radius);
	ClassDB::bind_method(D_METHOD("set_vrs_min_radius", "radius"), &XRVRS::set_vrs_min_radius);

	ClassDB::bind_method(D_METHOD("get_vrs_strength"), &XRVRS::get_vrs_strength);
	ClassDB::bind_method(D_METHOD("set_vrs_strength", "strength"), &XRVRS::set_vrs_strength);

	ClassDB::bind_method(D_METHOD("get_vrs_render_region"), &XRVRS::get_vrs_render_region);
	ClassDB::bind_method(D_METHOD("set_vrs_render_region", "render_region"), &XRVRS::set_vrs_render_region);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "vrs_min_radius", PROPERTY_HINT_RANGE, "1.0,100.0,1.0"), "set_vrs_min_radius", "get_vrs_min_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "vrs_strength", PROPERTY_HINT_RANGE, "0.1,10.0,0.1"), "set_vrs_strength", "get_vrs_strength");
	ADD_PROPERTY(PropertyInfo(Variant::RECT2I, "vrs_render_region"), "set_vrs_render_region", "get_vrs_render_region");

	ClassDB::bind_method(D_METHOD("make_vrs_texture", "target_size", "eye_foci"), &XRVRS::make_vrs_texture);
}

XRVRS::~XRVRS() {
	if (vrs_texture.is_valid()) {
		ERR_FAIL_NULL(RS::get_singleton());
		RS::get_singleton()->free(vrs_texture);
		vrs_texture = RID();
	}
}

float XRVRS::get_vrs_min_radius() const {
	return vrs_min_radius;
}

void XRVRS::set_vrs_min_radius(float p_vrs_min_radius) {
	if (p_vrs_min_radius < 1.0) {
		WARN_PRINT_ONCE("VRS minimum radius can not be set below 1.0");
		vrs_min_radius = 1.0;
	} else if (p_vrs_min_radius > 100.0) {
		WARN_PRINT_ONCE("VRS minimum radius can not be set above 100.0");
		vrs_min_radius = 100.0;
	} else {
		vrs_min_radius = p_vrs_min_radius;
		vrs_dirty = true;
	}
}

float XRVRS::get_vrs_strength() const {
	return vrs_strength;
}

void XRVRS::set_vrs_strength(float p_vrs_strength) {
	if (p_vrs_strength < 0.1) {
		WARN_PRINT_ONCE("VRS strength can not be set below 0.1");
		vrs_strength = 0.1;
	} else if (p_vrs_strength > 10.0) {
		WARN_PRINT_ONCE("VRS strength can not be set above 10.0");
		vrs_strength = 10.0;
	} else {
		vrs_strength = p_vrs_strength;
		vrs_dirty = true;
	}
}

Rect2i XRVRS::get_vrs_render_region() const {
	return vrs_render_region;
}

void XRVRS::set_vrs_render_region(const Rect2i &p_vrs_render_region) {
	vrs_render_region = p_vrs_render_region;
	vrs_dirty = true;
}

RID XRVRS::make_vrs_texture(const Size2 &p_target_size, const PackedVector2Array &p_eye_foci) {
	ERR_FAIL_COND_V(p_eye_foci.is_empty(), RID());

	int32_t texel_width = RD::get_singleton()->limit_get(RD::LIMIT_VRS_TEXEL_WIDTH);
	int32_t texel_height = RD::get_singleton()->limit_get(RD::LIMIT_VRS_TEXEL_HEIGHT);

	// Should return sensible data or graphics API does not support VRS.
	ERR_FAIL_COND_V(texel_width < 1 || texel_height < 1, RID());

	Size2 vrs_size = Size2(0.5 + p_target_size.x / texel_width, 0.5 + p_target_size.y / texel_height).round();

	// Make sure we have at least one pixel.
	vrs_size = vrs_size.maxf(1.0);

	float max_radius = 0.5 * MIN(vrs_size.x, vrs_size.y); // Maximum radius that fits inside of our image
	float min_radius = vrs_min_radius * max_radius / 100.0; // Minimum radius as a percentage of our size
	real_t outer_radius = MAX(1.0, (max_radius - min_radius) / vrs_strength);
	Size2 vrs_sizei = vrs_size;

	// Our density map is now unified, with a value of (0.0, 0.0) meaning a 1x1 texel size and (1.0, 1.0) an max texel size.
	// For our standard VRS extension on Vulkan this means a maximum of 8x8.
	// For the density map extension this scales depending on the max texel size.

	if (target_size != vrs_sizei || eye_foci != p_eye_foci || vrs_dirty) {
		// Out with the old.
		if (vrs_texture.is_valid()) {
			RS::get_singleton()->free(vrs_texture);
			vrs_texture = RID();
		}

		// In with the new.
		Vector<Ref<Image>> images;
		target_size = vrs_sizei;
		eye_foci = p_eye_foci;

		Size2 region_ratio = Size2(1.0, 1.0);
		Point2i region_offset;
		if (vrs_render_region != Rect2i()) {
			region_ratio = (Size2)vrs_render_region.size / p_target_size;
			region_offset = (Point2)vrs_render_region.position / p_target_size * vrs_sizei;
		}

		for (int i = 0; i < eye_foci.size() && i < RendererSceneRender::MAX_RENDER_VIEWS; i++) {
			PackedByteArray data;
			data.resize(vrs_sizei.x * vrs_sizei.y * 2);
			uint8_t *data_ptr = data.ptrw();

			Vector2i view_center;
			view_center.x = int(vrs_size.x * (eye_foci[i].x + 1.0) * region_ratio.x * 0.5) + region_offset.x;
			view_center.y = int(vrs_size.y * (eye_foci[i].y + 1.0) * region_ratio.y * 0.5) + region_offset.y;

			int d = 0;
			for (int y = 0; y < vrs_sizei.y; y++) {
				for (int x = 0; x < vrs_sizei.x; x++) {
					Vector2 offset = Vector2(x - view_center.x, y - view_center.y) / region_ratio;
					real_t density = 255.0 * MAX(0.0, (Math::abs(offset.x) - min_radius) / outer_radius);
					data_ptr[d++] = MIN(255, density);
					density = 255.0 * MAX(0.0, (Math::abs(offset.y) - min_radius) / outer_radius);
					data_ptr[d++] = MIN(255, density);
				}
			}
			images.push_back(Image::create_from_data(vrs_sizei.x, vrs_sizei.y, false, Image::FORMAT_RG8, data));
		}

		if (images.size() == 1) {
			vrs_texture = RS::get_singleton()->texture_2d_create(images[0]);
		} else {
			vrs_texture = RS::get_singleton()->texture_2d_layered_create(images, RS::TEXTURE_LAYERED_2D_ARRAY);
		}

		vrs_dirty = false;
	}

	return vrs_texture;
}
