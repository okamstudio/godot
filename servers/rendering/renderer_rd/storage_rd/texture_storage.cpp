/**************************************************************************/
/*  texture_storage.cpp                                                   */
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

#include "texture_storage.h"

#include "../effects/copy_effects.h"
#include "../framebuffer_cache_rd.h"
#include "material_storage.h"
#include "servers/rendering/renderer_rd/renderer_scene_render_rd.h"

using namespace RendererRD;

///////////////////////////////////////////////////////////////////////////
// TextureStorage::CanvasTexture

void TextureStorage::CanvasTexture::clear_cache() {
	info_cache[0] = CanvasTextureCache();
	info_cache[1] = CanvasTextureCache();
}

TextureStorage::CanvasTexture::~CanvasTexture() {
}

///////////////////////////////////////////////////////////////////////////
// TextureStorage::Texture

void TextureStorage::Texture::cleanup() {
	if (RD::get_singleton()->texture_is_valid(rd_texture_srgb)) {
		//erase this first, as it's a dependency of the one below
		RD::get_singleton()->free(rd_texture_srgb);
	}
	if (RD::get_singleton()->texture_is_valid(rd_texture)) {
		RD::get_singleton()->free(rd_texture);
	}
	if (canvas_texture) {
		memdelete(canvas_texture);
	}
}

///////////////////////////////////////////////////////////////////////////
// TextureStorage

TextureStorage *TextureStorage::singleton = nullptr;

TextureStorage *TextureStorage::get_singleton() {
	return singleton;
}

TextureStorage::TextureStorage() {
	singleton = this;

	{ //create default textures

		RD::TextureFormat tformat;
		tformat.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
		tformat.width = 4;
		tformat.height = 4;
		tformat.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;
		tformat.texture_type = RD::TEXTURE_TYPE_2D;

		Vector<uint8_t> pv;
		pv.resize(16 * 4);
		for (int i = 0; i < 16; i++) {
			// Opaque white.
			pv.set(i * 4 + 0, 255);
			pv.set(i * 4 + 1, 255);
			pv.set(i * 4 + 2, 255);
			pv.set(i * 4 + 3, 255);
		}

		{
			Vector<Vector<uint8_t>> vpv;
			vpv.push_back(pv);
			default_rd_textures[DEFAULT_RD_TEXTURE_WHITE] = RD::get_singleton()->texture_create(tformat, RD::TextureView(), vpv);
		}

		for (int i = 0; i < 16; i++) {
			// Opaque black.
			pv.set(i * 4 + 0, 0);
			pv.set(i * 4 + 1, 0);
			pv.set(i * 4 + 2, 0);
			pv.set(i * 4 + 3, 255);
		}

		{
			Vector<Vector<uint8_t>> vpv;
			vpv.push_back(pv);
			default_rd_textures[DEFAULT_RD_TEXTURE_BLACK] = RD::get_singleton()->texture_create(tformat, RD::TextureView(), vpv);
		}

		for (int i = 0; i < 16; i++) {
			// Transparent black.
			pv.set(i * 4 + 0, 0);
			pv.set(i * 4 + 1, 0);
			pv.set(i * 4 + 2, 0);
			pv.set(i * 4 + 3, 0);
		}

		{
			Vector<Vector<uint8_t>> vpv;
			vpv.push_back(pv);
			default_rd_textures[DEFAULT_RD_TEXTURE_TRANSPARENT] = RD::get_singleton()->texture_create(tformat, RD::TextureView(), vpv);
		}

		for (int i = 0; i < 16; i++) {
			// Opaque normal map "flat" color.
			pv.set(i * 4 + 0, 128);
			pv.set(i * 4 + 1, 128);
			pv.set(i * 4 + 2, 255);
			pv.set(i * 4 + 3, 255);
		}

		{
			Vector<Vector<uint8_t>> vpv;
			vpv.push_back(pv);
			default_rd_textures[DEFAULT_RD_TEXTURE_NORMAL] = RD::get_singleton()->texture_create(tformat, RD::TextureView(), vpv);
		}

		for (int i = 0; i < 16; i++) {
			// Opaque flowmap "flat" color.
			pv.set(i * 4 + 0, 255);
			pv.set(i * 4 + 1, 128);
			pv.set(i * 4 + 2, 255);
			pv.set(i * 4 + 3, 255);
		}

		{
			Vector<Vector<uint8_t>> vpv;
			vpv.push_back(pv);
			default_rd_textures[DEFAULT_RD_TEXTURE_ANISO] = RD::get_singleton()->texture_create(tformat, RD::TextureView(), vpv);
		}

		{
			RD::TextureFormat tf;
			tf.format = RD::DATA_FORMAT_D16_UNORM;
			tf.width = 4;
			tf.height = 4;
			tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT | RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			tf.texture_type = RD::TEXTURE_TYPE_2D;

			Vector<uint8_t> sv;
			sv.resize(16 * 2);
			uint16_t *ptr = (uint16_t *)sv.ptrw();
			for (int i = 0; i < 16; i++) {
				ptr[i] = Math::make_half_float(1.0f);
			}

			default_rd_textures[DEFAULT_RD_TEXTURE_DEPTH] = RD::get_singleton()->texture_create(tf, RD::TextureView());
			RD::get_singleton()->texture_update(default_rd_textures[DEFAULT_RD_TEXTURE_DEPTH], 0, sv);
		}

		for (int i = 0; i < 16; i++) {
			pv.set(i * 4 + 0, 0);
			pv.set(i * 4 + 1, 0);
			pv.set(i * 4 + 2, 0);
			pv.set(i * 4 + 3, 0);
		}

		default_rd_textures[DEFAULT_RD_TEXTURE_MULTIMESH_BUFFER] = RD::get_singleton()->texture_buffer_create(16, RD::DATA_FORMAT_R8G8B8A8_UNORM, pv);

		for (int i = 0; i < 16; i++) {
			pv.set(i * 4 + 0, 0);
			pv.set(i * 4 + 1, 0);
			pv.set(i * 4 + 2, 0);
			pv.set(i * 4 + 3, 0);
		}

		{
			tformat.format = RD::DATA_FORMAT_R8G8B8A8_UINT;
			Vector<Vector<uint8_t>> vpv;
			vpv.push_back(pv);
			default_rd_textures[DEFAULT_RD_TEXTURE_2D_UINT] = RD::get_singleton()->texture_create(tformat, RD::TextureView(), vpv);
		}
	}

	{ //create default black cubemap array

		RD::TextureFormat tformat;
		tformat.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
		tformat.width = 4;
		tformat.height = 4;
		tformat.array_layers = 6;
		tformat.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;
		tformat.texture_type = RD::TEXTURE_TYPE_CUBE_ARRAY;

		Vector<uint8_t> pv;
		pv.resize(16 * 4);
		for (int i = 0; i < 16; i++) {
			pv.set(i * 4 + 0, 0);
			pv.set(i * 4 + 1, 0);
			pv.set(i * 4 + 2, 0);
			pv.set(i * 4 + 3, 0);
		}

		{
			Vector<Vector<uint8_t>> vpv;
			for (int i = 0; i < 6; i++) {
				vpv.push_back(pv);
			}
			default_rd_textures[DEFAULT_RD_TEXTURE_CUBEMAP_ARRAY_BLACK] = RD::get_singleton()->texture_create(tformat, RD::TextureView(), vpv);
		}
	}

	{ //create default white cubemap array

		RD::TextureFormat tformat;
		tformat.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
		tformat.width = 4;
		tformat.height = 4;
		tformat.array_layers = 6;
		tformat.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;
		tformat.texture_type = RD::TEXTURE_TYPE_CUBE_ARRAY;

		Vector<uint8_t> pv;
		pv.resize(16 * 4);
		for (int i = 0; i < 16; i++) {
			pv.set(i * 4 + 0, 255);
			pv.set(i * 4 + 1, 255);
			pv.set(i * 4 + 2, 255);
			pv.set(i * 4 + 3, 255);
		}

		{
			Vector<Vector<uint8_t>> vpv;
			for (int i = 0; i < 6; i++) {
				vpv.push_back(pv);
			}
			default_rd_textures[DEFAULT_RD_TEXTURE_CUBEMAP_ARRAY_WHITE] = RD::get_singleton()->texture_create(tformat, RD::TextureView(), vpv);
		}
	}

	{ //create default black cubemap

		RD::TextureFormat tformat;
		tformat.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
		tformat.width = 4;
		tformat.height = 4;
		tformat.array_layers = 6;
		tformat.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;
		tformat.texture_type = RD::TEXTURE_TYPE_CUBE;

		Vector<uint8_t> pv;
		pv.resize(16 * 4);
		for (int i = 0; i < 16; i++) {
			pv.set(i * 4 + 0, 0);
			pv.set(i * 4 + 1, 0);
			pv.set(i * 4 + 2, 0);
			pv.set(i * 4 + 3, 0);
		}

		{
			Vector<Vector<uint8_t>> vpv;
			for (int i = 0; i < 6; i++) {
				vpv.push_back(pv);
			}
			default_rd_textures[DEFAULT_RD_TEXTURE_CUBEMAP_BLACK] = RD::get_singleton()->texture_create(tformat, RD::TextureView(), vpv);
		}
	}

	{ //create default white cubemap

		RD::TextureFormat tformat;
		tformat.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
		tformat.width = 4;
		tformat.height = 4;
		tformat.array_layers = 6;
		tformat.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;
		tformat.texture_type = RD::TEXTURE_TYPE_CUBE;

		Vector<uint8_t> pv;
		pv.resize(16 * 4);
		for (int i = 0; i < 16; i++) {
			pv.set(i * 4 + 0, 255);
			pv.set(i * 4 + 1, 255);
			pv.set(i * 4 + 2, 255);
			pv.set(i * 4 + 3, 255);
		}

		{
			Vector<Vector<uint8_t>> vpv;
			for (int i = 0; i < 6; i++) {
				vpv.push_back(pv);
			}
			default_rd_textures[DEFAULT_RD_TEXTURE_CUBEMAP_WHITE] = RD::get_singleton()->texture_create(tformat, RD::TextureView(), vpv);
		}
	}

	{ //create default 3D

		RD::TextureFormat tformat;
		tformat.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
		tformat.width = 4;
		tformat.height = 4;
		tformat.depth = 4;
		tformat.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;
		tformat.texture_type = RD::TEXTURE_TYPE_3D;

		Vector<uint8_t> pv;
		pv.resize(64 * 4);
		for (int i = 0; i < 64; i++) {
			pv.set(i * 4 + 0, 0);
			pv.set(i * 4 + 1, 0);
			pv.set(i * 4 + 2, 0);
			pv.set(i * 4 + 3, 0);
		}

		{
			Vector<Vector<uint8_t>> vpv;
			vpv.push_back(pv);
			default_rd_textures[DEFAULT_RD_TEXTURE_3D_BLACK] = RD::get_singleton()->texture_create(tformat, RD::TextureView(), vpv);
		}
		for (int i = 0; i < 64; i++) {
			pv.set(i * 4 + 0, 255);
			pv.set(i * 4 + 1, 255);
			pv.set(i * 4 + 2, 255);
			pv.set(i * 4 + 3, 255);
		}

		{
			Vector<Vector<uint8_t>> vpv;
			vpv.push_back(pv);
			default_rd_textures[DEFAULT_RD_TEXTURE_3D_WHITE] = RD::get_singleton()->texture_create(tformat, RD::TextureView(), vpv);
		}
	}

	{ //create default array white

		RD::TextureFormat tformat;
		tformat.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
		tformat.width = 4;
		tformat.height = 4;
		tformat.array_layers = 1;
		tformat.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;
		tformat.texture_type = RD::TEXTURE_TYPE_2D_ARRAY;

		Vector<uint8_t> pv;
		pv.resize(16 * 4);
		for (int i = 0; i < 16; i++) {
			pv.set(i * 4 + 0, 255);
			pv.set(i * 4 + 1, 255);
			pv.set(i * 4 + 2, 255);
			pv.set(i * 4 + 3, 255);
		}

		{
			Vector<Vector<uint8_t>> vpv;
			vpv.push_back(pv);
			default_rd_textures[DEFAULT_RD_TEXTURE_2D_ARRAY_WHITE] = RD::get_singleton()->texture_create(tformat, RD::TextureView(), vpv);
		}
	}

	{ //create default array black

		RD::TextureFormat tformat;
		tformat.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
		tformat.width = 4;
		tformat.height = 4;
		tformat.array_layers = 1;
		tformat.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;
		tformat.texture_type = RD::TEXTURE_TYPE_2D_ARRAY;

		Vector<uint8_t> pv;
		pv.resize(16 * 4);
		for (int i = 0; i < 16; i++) {
			pv.set(i * 4 + 0, 0);
			pv.set(i * 4 + 1, 0);
			pv.set(i * 4 + 2, 0);
			pv.set(i * 4 + 3, 0);
		}

		{
			Vector<Vector<uint8_t>> vpv;
			vpv.push_back(pv);
			default_rd_textures[DEFAULT_RD_TEXTURE_2D_ARRAY_BLACK] = RD::get_singleton()->texture_create(tformat, RD::TextureView(), vpv);
		}
	}

	{ //create default array normal

		RD::TextureFormat tformat;
		tformat.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
		tformat.width = 4;
		tformat.height = 4;
		tformat.array_layers = 1;
		tformat.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;
		tformat.texture_type = RD::TEXTURE_TYPE_2D_ARRAY;

		Vector<uint8_t> pv;
		pv.resize(16 * 4);
		for (int i = 0; i < 16; i++) {
			pv.set(i * 4 + 0, 128);
			pv.set(i * 4 + 1, 128);
			pv.set(i * 4 + 2, 255);
			pv.set(i * 4 + 3, 255);
		}

		{
			Vector<Vector<uint8_t>> vpv;
			vpv.push_back(pv);
			default_rd_textures[DEFAULT_RD_TEXTURE_2D_ARRAY_NORMAL] = RD::get_singleton()->texture_create(tformat, RD::TextureView(), vpv);
		}
	}

	{ //create default array depth

		RD::TextureFormat tformat;
		tformat.format = RD::DATA_FORMAT_D16_UNORM;
		tformat.width = 4;
		tformat.height = 4;
		tformat.array_layers = 1;
		tformat.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT | RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		tformat.texture_type = RD::TEXTURE_TYPE_2D_ARRAY;

		Vector<uint8_t> sv;
		sv.resize(16 * 2);
		uint16_t *ptr = (uint16_t *)sv.ptrw();
		for (int i = 0; i < 16; i++) {
			ptr[i] = Math::make_half_float(1.0f);
		}

		{
			default_rd_textures[DEFAULT_RD_TEXTURE_2D_ARRAY_DEPTH] = RD::get_singleton()->texture_create(tformat, RD::TextureView());
			RD::get_singleton()->texture_update(default_rd_textures[DEFAULT_RD_TEXTURE_2D_ARRAY_DEPTH], 0, sv);
		}
	}

	{ // default atlas texture
		RD::TextureFormat tformat;
		tformat.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
		tformat.width = 4;
		tformat.height = 4;
		tformat.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;
		tformat.texture_type = RD::TEXTURE_TYPE_2D;

		Vector<uint8_t> pv;
		pv.resize(16 * 4);
		for (int i = 0; i < 16; i++) {
			pv.set(i * 4 + 0, 0);
			pv.set(i * 4 + 1, 0);
			pv.set(i * 4 + 2, 0);
			pv.set(i * 4 + 3, 255);
		}

		{
			Vector<Vector<uint8_t>> vpv;
			vpv.push_back(pv);
			decal_atlas.texture = RD::get_singleton()->texture_create(tformat, RD::TextureView(), vpv);
			decal_atlas.texture_srgb = decal_atlas.texture;
		}
	}

	{ //create default VRS

		RD::TextureFormat tformat;
		tformat.format = RD::DATA_FORMAT_R8_UINT;
		tformat.width = 4;
		tformat.height = 4;
		tformat.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT | RD::TEXTURE_USAGE_VRS_ATTACHMENT_BIT;
		tformat.texture_type = RD::TEXTURE_TYPE_2D;
		if (!RD::get_singleton()->has_feature(RD::SUPPORTS_ATTACHMENT_VRS)) {
			tformat.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;
		}

		Vector<uint8_t> pv;
		pv.resize(4 * 4);
		for (int i = 0; i < 4 * 4; i++) {
			pv.set(i, 0);
		}

		{
			Vector<Vector<uint8_t>> vpv;
			vpv.push_back(pv);
			default_rd_textures[DEFAULT_RD_TEXTURE_VRS] = RD::get_singleton()->texture_create(tformat, RD::TextureView(), vpv);
		}
	}

	{
		Vector<String> sdf_modes;
		sdf_modes.push_back("\n#define MODE_LOAD\n");
		sdf_modes.push_back("\n#define MODE_LOAD_SHRINK\n");
		sdf_modes.push_back("\n#define MODE_PROCESS\n");
		sdf_modes.push_back("\n#define MODE_PROCESS_OPTIMIZED\n");
		sdf_modes.push_back("\n#define MODE_STORE\n");
		sdf_modes.push_back("\n#define MODE_STORE_SHRINK\n");

		rt_sdf.shader.initialize(sdf_modes);

		rt_sdf.shader_version = rt_sdf.shader.version_create();

		for (int i = 0; i < RenderTargetSDF::SHADER_MAX; i++) {
			rt_sdf.pipelines[i] = RD::get_singleton()->compute_pipeline_create(rt_sdf.shader.version_get_shader(rt_sdf.shader_version, i));
		}
	}

	// Initialize texture placeholder data for the `texture_*_placeholder_initialize()` methods.

	constexpr int placeholder_size = 4;
	texture_2d_placeholder = Image::create_empty(placeholder_size, placeholder_size, false, Image::FORMAT_RGBA8);
	// Draw a magenta/black checkerboard pattern.
	for (int i = 0; i < placeholder_size * placeholder_size; i++) {
		const int x = i % placeholder_size;
		const int y = i / placeholder_size;
		texture_2d_placeholder->set_pixel(x, y, (x + y) % 2 == 0 ? Color(1, 0, 1) : Color(0, 0, 0));
	}

	texture_2d_array_placeholder.push_back(texture_2d_placeholder);

	for (int i = 0; i < 6; i++) {
		cubemap_placeholder.push_back(texture_2d_placeholder);
	}

	Ref<Image> texture_2d_placeholder_rotated;
	texture_2d_placeholder_rotated.instantiate();
	texture_2d_placeholder_rotated->copy_from(texture_2d_placeholder);
	texture_2d_placeholder_rotated->rotate_90(CLOCKWISE);
	for (int i = 0; i < 4; i++) {
		// Alternate checkerboard pattern on odd layers (by using a copy that is rotated 90 degrees).
		texture_3d_placeholder.push_back(i % 2 == 0 ? texture_2d_placeholder : texture_2d_placeholder_rotated);
	}
}

TextureStorage::~TextureStorage() {
	rt_sdf.shader.version_free(rt_sdf.shader_version);

	free_decal_data();

	if (decal_atlas.textures.size()) {
		ERR_PRINT("Decal Atlas: " + itos(decal_atlas.textures.size()) + " textures were not removed from the atlas.");
	}

	if (decal_atlas.texture.is_valid()) {
		RD::get_singleton()->free(decal_atlas.texture);
	}

	//def textures
	for (int i = 0; i < DEFAULT_RD_TEXTURE_MAX; i++) {
		if (default_rd_textures[i].is_valid()) {
			RD::get_singleton()->free(default_rd_textures[i]);
		}
	}

	singleton = nullptr;
}

bool TextureStorage::free(RID p_rid) {
	if (owns_texture(p_rid)) {
		texture_free(p_rid);
		return true;
	} else if (owns_canvas_texture(p_rid)) {
		canvas_texture_free(p_rid);
		return true;
	} else if (owns_decal(p_rid)) {
		decal_free(p_rid);
		return true;
	} else if (owns_decal_instance(p_rid)) {
		decal_instance_free(p_rid);
		return true;
	} else if (owns_render_target(p_rid)) {
		render_target_free(p_rid);
		return true;
	}

	return false;
}

/* Canvas Texture API */

RID TextureStorage::canvas_texture_allocate() {
	return canvas_texture_owner.allocate_rid();
}

void TextureStorage::canvas_texture_initialize(RID p_rid) {
	canvas_texture_owner.initialize_rid(p_rid);
}

void TextureStorage::canvas_texture_free(RID p_rid) {
	canvas_texture_owner.free(p_rid);
}

void TextureStorage::canvas_texture_set_channel(RID p_canvas_texture, RS::CanvasTextureChannel p_channel, RID p_texture) {
	CanvasTexture *ct = canvas_texture_owner.get_or_null(p_canvas_texture);
	ERR_FAIL_NULL(ct);

	switch (p_channel) {
		case RS::CANVAS_TEXTURE_CHANNEL_DIFFUSE: {
			ct->diffuse = p_texture;
		} break;
		case RS::CANVAS_TEXTURE_CHANNEL_NORMAL: {
			ct->normal_map = p_texture;
		} break;
		case RS::CANVAS_TEXTURE_CHANNEL_SPECULAR: {
			ct->specular = p_texture;
		} break;
	}
	ct->clear_cache();
}

void TextureStorage::canvas_texture_set_shading_parameters(RID p_canvas_texture, const Color &p_specular_color, float p_shininess) {
	CanvasTexture *ct = canvas_texture_owner.get_or_null(p_canvas_texture);
	ERR_FAIL_NULL(ct);

	ct->specular_color.r = p_specular_color.r;
	ct->specular_color.g = p_specular_color.g;
	ct->specular_color.b = p_specular_color.b;
	ct->specular_color.a = p_shininess;
}

void TextureStorage::canvas_texture_set_texture_filter(RID p_canvas_texture, RS::CanvasItemTextureFilter p_filter) {
	CanvasTexture *ct = canvas_texture_owner.get_or_null(p_canvas_texture);
	ERR_FAIL_NULL(ct);

	ct->texture_filter = p_filter;
}

void TextureStorage::canvas_texture_set_texture_repeat(RID p_canvas_texture, RS::CanvasItemTextureRepeat p_repeat) {
	CanvasTexture *ct = canvas_texture_owner.get_or_null(p_canvas_texture);
	ERR_FAIL_NULL(ct);

	ct->texture_repeat = p_repeat;
}

TextureStorage::CanvasTextureInfo TextureStorage::canvas_texture_get_info(RID p_texture, RS::CanvasItemTextureFilter p_base_filter, RS::CanvasItemTextureRepeat p_base_repeat, bool p_use_srgb, bool p_texture_is_data) {
	MaterialStorage *material_storage = MaterialStorage::get_singleton();

	CanvasTexture *ct = nullptr;
	Texture *t = get_texture(p_texture);

	if (t) {
		//regular texture
		if (!t->canvas_texture) {
			t->canvas_texture = memnew(CanvasTexture);
			t->canvas_texture->diffuse = p_texture;
		}

		ct = t->canvas_texture;
		if (t->render_target) {
			t->render_target->was_used = true;
		}
	} else {
		ct = canvas_texture_owner.get_or_null(p_texture);
	}

	if (!ct) {
		return CanvasTextureInfo(); //invalid texture RID
	}

	RS::CanvasItemTextureFilter filter = ct->texture_filter != RS::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT ? ct->texture_filter : p_base_filter;
	ERR_FAIL_COND_V(filter == RS::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT, CanvasTextureInfo());

	RS::CanvasItemTextureRepeat repeat = ct->texture_repeat != RS::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT ? ct->texture_repeat : p_base_repeat;
	ERR_FAIL_COND_V(repeat == RS::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT, CanvasTextureInfo());

	CanvasTextureCache &ctc = ct->info_cache[int(p_use_srgb)];
	if (!RD::get_singleton()->texture_is_valid(ctc.diffuse) ||
			!RD::get_singleton()->texture_is_valid(ctc.normal) ||
			!RD::get_singleton()->texture_is_valid(ctc.specular)) {
		{ //diffuse
			t = get_texture(ct->diffuse);
			if (!t) {
				ctc.diffuse = texture_rd_get_default(DEFAULT_RD_TEXTURE_WHITE);
				ct->size_cache = Size2i(1, 1);
			} else {
				ctc.diffuse = t->rd_texture_srgb.is_valid() && p_use_srgb && !p_texture_is_data ? t->rd_texture_srgb : t->rd_texture;
				ct->size_cache = Size2i(t->width_2d, t->height_2d);
				if (t->render_target) {
					t->render_target->was_used = true;
				}
			}
		}
		{ //normal
			t = get_texture(ct->normal_map);
			if (!t) {
				ctc.normal = texture_rd_get_default(DEFAULT_RD_TEXTURE_NORMAL);
				ct->use_normal_cache = false;
			} else {
				ctc.normal = t->rd_texture;
				ct->use_normal_cache = true;
				if (t->render_target) {
					t->render_target->was_used = true;
				}
			}
		}
		{ //specular
			t = get_texture(ct->specular);
			if (!t) {
				ctc.specular = texture_rd_get_default(DEFAULT_RD_TEXTURE_WHITE);
				ct->use_specular_cache = false;
			} else {
				ctc.specular = t->rd_texture;
				ct->use_specular_cache = true;
				if (t->render_target) {
					t->render_target->was_used = true;
				}
			}
		}
	}

	CanvasTextureInfo res;
	res.diffuse = ctc.diffuse;
	res.normal = ctc.normal;
	res.specular = ctc.specular;
	res.sampler = material_storage->sampler_rd_get_default(filter, repeat);
	res.size = ct->size_cache;
	res.specular_color = ct->specular_color;
	res.use_normal = ct->use_normal_cache;
	res.use_specular = ct->use_specular_cache;

	return res;
}

/* Texture API */

RID TextureStorage::texture_allocate() {
	return texture_owner.allocate_rid();
}

void TextureStorage::texture_free(RID p_texture) {
	Texture *t = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(t);
	ERR_FAIL_COND(t->is_render_target);

	t->cleanup();

	if (t->is_proxy && t->proxy_to.is_valid()) {
		Texture *proxy_to = texture_owner.get_or_null(t->proxy_to);
		if (proxy_to) {
			proxy_to->proxies.erase(p_texture);
		}
	}

	decal_atlas_remove_texture(p_texture);

	for (int i = 0; i < t->proxies.size(); i++) {
		Texture *p = texture_owner.get_or_null(t->proxies[i]);
		ERR_CONTINUE(!p);
		p->proxy_to = RID();
		p->rd_texture = RID();
		p->rd_texture_srgb = RID();
	}

	texture_owner.free(p_texture);
}

void TextureStorage::texture_2d_initialize(RID p_texture, const Ref<Image> &p_image) {
	ERR_FAIL_COND(p_image.is_null());

	TextureToRDFormat ret_format;
	Ref<Image> image = _validate_texture_format(p_image, ret_format);

	Texture texture;

	texture.type = TextureStorage::TYPE_2D;

	texture.width = p_image->get_width();
	texture.height = p_image->get_height();
	texture.layers = 1;
	texture.mipmaps = p_image->get_mipmap_count() + 1;
	texture.depth = 1;
	texture.format = p_image->get_format();
	texture.validated_format = image->get_format();

	texture.rd_type = RD::TEXTURE_TYPE_2D;
	texture.rd_format = ret_format.format;
	texture.rd_format_srgb = ret_format.format_srgb;

	RD::TextureFormat rd_format;
	RD::TextureView rd_view;
	{ //attempt register
		rd_format.format = texture.rd_format;
		rd_format.width = texture.width;
		rd_format.height = texture.height;
		rd_format.depth = 1;
		rd_format.array_layers = 1;
		rd_format.mipmaps = texture.mipmaps;
		rd_format.texture_type = texture.rd_type;
		rd_format.samples = RD::TEXTURE_SAMPLES_1;
		rd_format.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
		if (texture.rd_format_srgb != RD::DATA_FORMAT_MAX) {
			rd_format.shareable_formats.push_back(texture.rd_format);
			rd_format.shareable_formats.push_back(texture.rd_format_srgb);
		}
	}
	{
		rd_view.swizzle_r = ret_format.swizzle_r;
		rd_view.swizzle_g = ret_format.swizzle_g;
		rd_view.swizzle_b = ret_format.swizzle_b;
		rd_view.swizzle_a = ret_format.swizzle_a;
	}
	Vector<uint8_t> data = image->get_data(); //use image data
	Vector<Vector<uint8_t>> data_slices;
	data_slices.push_back(data);
	texture.rd_texture = RD::get_singleton()->texture_create(rd_format, rd_view, data_slices);
	ERR_FAIL_COND(texture.rd_texture.is_null());
	if (texture.rd_format_srgb != RD::DATA_FORMAT_MAX) {
		rd_view.format_override = texture.rd_format_srgb;
		texture.rd_texture_srgb = RD::get_singleton()->texture_create_shared(rd_view, texture.rd_texture);
		if (texture.rd_texture_srgb.is_null()) {
			RD::get_singleton()->free(texture.rd_texture);
			ERR_FAIL_COND(texture.rd_texture_srgb.is_null());
		}
	}

	//used for 2D, overridable
	texture.width_2d = texture.width;
	texture.height_2d = texture.height;
	texture.is_render_target = false;
	texture.rd_view = rd_view;
	texture.is_proxy = false;

	texture_owner.initialize_rid(p_texture, texture);
}

void TextureStorage::texture_2d_layered_initialize(RID p_texture, const Vector<Ref<Image>> &p_layers, RS::TextureLayeredType p_layered_type) {
	ERR_FAIL_COND(p_layers.is_empty());

	ERR_FAIL_COND(p_layered_type == RS::TEXTURE_LAYERED_CUBEMAP && p_layers.size() != 6);
	ERR_FAIL_COND(p_layered_type == RS::TEXTURE_LAYERED_CUBEMAP_ARRAY && (p_layers.size() < 6 || (p_layers.size() % 6) != 0));

	TextureToRDFormat ret_format;
	Vector<Ref<Image>> images;
	{
		int valid_width = 0;
		int valid_height = 0;
		bool valid_mipmaps = false;
		Image::Format valid_format = Image::FORMAT_MAX;

		for (int i = 0; i < p_layers.size(); i++) {
			ERR_FAIL_COND(p_layers[i]->is_empty());

			if (i == 0) {
				valid_width = p_layers[i]->get_width();
				valid_height = p_layers[i]->get_height();
				valid_format = p_layers[i]->get_format();
				valid_mipmaps = p_layers[i]->has_mipmaps();
			} else {
				ERR_FAIL_COND(p_layers[i]->get_width() != valid_width);
				ERR_FAIL_COND(p_layers[i]->get_height() != valid_height);
				ERR_FAIL_COND(p_layers[i]->get_format() != valid_format);
				ERR_FAIL_COND(p_layers[i]->has_mipmaps() != valid_mipmaps);
			}

			images.push_back(_validate_texture_format(p_layers[i], ret_format));
		}
	}

	Texture texture;

	texture.type = TextureStorage::TYPE_LAYERED;
	texture.layered_type = p_layered_type;

	texture.width = p_layers[0]->get_width();
	texture.height = p_layers[0]->get_height();
	texture.layers = p_layers.size();
	texture.mipmaps = p_layers[0]->get_mipmap_count() + 1;
	texture.depth = 1;
	texture.format = p_layers[0]->get_format();
	texture.validated_format = images[0]->get_format();

	switch (p_layered_type) {
		case RS::TEXTURE_LAYERED_2D_ARRAY: {
			texture.rd_type = RD::TEXTURE_TYPE_2D_ARRAY;
		} break;
		case RS::TEXTURE_LAYERED_CUBEMAP: {
			texture.rd_type = RD::TEXTURE_TYPE_CUBE;
		} break;
		case RS::TEXTURE_LAYERED_CUBEMAP_ARRAY: {
			texture.rd_type = RD::TEXTURE_TYPE_CUBE_ARRAY;
		} break;
		default:
			ERR_FAIL(); // Shouldn't happen, silence warnings.
	}

	texture.rd_format = ret_format.format;
	texture.rd_format_srgb = ret_format.format_srgb;

	RD::TextureFormat rd_format;
	RD::TextureView rd_view;
	{ //attempt register
		rd_format.format = texture.rd_format;
		rd_format.width = texture.width;
		rd_format.height = texture.height;
		rd_format.depth = 1;
		rd_format.array_layers = texture.layers;
		rd_format.mipmaps = texture.mipmaps;
		rd_format.texture_type = texture.rd_type;
		rd_format.samples = RD::TEXTURE_SAMPLES_1;
		rd_format.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
		if (texture.rd_format_srgb != RD::DATA_FORMAT_MAX) {
			rd_format.shareable_formats.push_back(texture.rd_format);
			rd_format.shareable_formats.push_back(texture.rd_format_srgb);
		}
	}
	{
		rd_view.swizzle_r = ret_format.swizzle_r;
		rd_view.swizzle_g = ret_format.swizzle_g;
		rd_view.swizzle_b = ret_format.swizzle_b;
		rd_view.swizzle_a = ret_format.swizzle_a;
	}
	Vector<Vector<uint8_t>> data_slices;
	for (int i = 0; i < images.size(); i++) {
		Vector<uint8_t> data = images[i]->get_data(); //use image data
		data_slices.push_back(data);
	}
	texture.rd_texture = RD::get_singleton()->texture_create(rd_format, rd_view, data_slices);
	ERR_FAIL_COND(texture.rd_texture.is_null());
	if (texture.rd_format_srgb != RD::DATA_FORMAT_MAX) {
		rd_view.format_override = texture.rd_format_srgb;
		texture.rd_texture_srgb = RD::get_singleton()->texture_create_shared(rd_view, texture.rd_texture);
		if (texture.rd_texture_srgb.is_null()) {
			RD::get_singleton()->free(texture.rd_texture);
			ERR_FAIL_COND(texture.rd_texture_srgb.is_null());
		}
	}

	//used for 2D, overridable
	texture.width_2d = texture.width;
	texture.height_2d = texture.height;
	texture.is_render_target = false;
	texture.rd_view = rd_view;
	texture.is_proxy = false;

	texture_owner.initialize_rid(p_texture, texture);
}

void TextureStorage::texture_3d_initialize(RID p_texture, Image::Format p_format, int p_width, int p_height, int p_depth, bool p_mipmaps, const Vector<Ref<Image>> &p_data) {
	ERR_FAIL_COND(p_data.is_empty());

	Image::Image3DValidateError verr = Image::validate_3d_image(p_format, p_width, p_height, p_depth, p_mipmaps, p_data);
	if (verr != Image::VALIDATE_3D_OK) {
		ERR_FAIL_MSG(Image::get_3d_image_validation_error_text(verr));
	}

	TextureToRDFormat ret_format;
	Image::Format validated_format = Image::FORMAT_MAX;
	Vector<uint8_t> all_data;
	uint32_t mipmap_count = 0;
	Vector<Texture::BufferSlice3D> slices;
	{
		Vector<Ref<Image>> images;
		uint32_t all_data_size = 0;
		images.resize(p_data.size());
		for (int i = 0; i < p_data.size(); i++) {
			TextureToRDFormat f;
			images.write[i] = _validate_texture_format(p_data[i], f);
			if (i == 0) {
				ret_format = f;
				validated_format = images[0]->get_format();
			}

			all_data_size += images[i]->get_data().size();
		}

		all_data.resize(all_data_size); //consolidate all data here
		uint32_t offset = 0;
		Size2i prev_size;
		for (int i = 0; i < p_data.size(); i++) {
			uint32_t s = images[i]->get_data().size();

			memcpy(&all_data.write[offset], images[i]->get_data().ptr(), s);
			{
				Texture::BufferSlice3D slice;
				slice.size.width = images[i]->get_width();
				slice.size.height = images[i]->get_height();
				slice.offset = offset;
				slice.buffer_size = s;
				slices.push_back(slice);
			}
			offset += s;

			Size2i img_size(images[i]->get_width(), images[i]->get_height());
			if (img_size != prev_size) {
				mipmap_count++;
			}
			prev_size = img_size;
		}
	}

	Texture texture;

	texture.type = TextureStorage::TYPE_3D;
	texture.width = p_width;
	texture.height = p_height;
	texture.depth = p_depth;
	texture.mipmaps = mipmap_count;
	texture.format = p_data[0]->get_format();
	texture.validated_format = validated_format;

	texture.buffer_size_3d = all_data.size();
	texture.buffer_slices_3d = slices;

	texture.rd_type = RD::TEXTURE_TYPE_3D;
	texture.rd_format = ret_format.format;
	texture.rd_format_srgb = ret_format.format_srgb;

	RD::TextureFormat rd_format;
	RD::TextureView rd_view;
	{ //attempt register
		rd_format.format = texture.rd_format;
		rd_format.width = texture.width;
		rd_format.height = texture.height;
		rd_format.depth = texture.depth;
		rd_format.array_layers = 1;
		rd_format.mipmaps = texture.mipmaps;
		rd_format.texture_type = texture.rd_type;
		rd_format.samples = RD::TEXTURE_SAMPLES_1;
		rd_format.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
		if (texture.rd_format_srgb != RD::DATA_FORMAT_MAX) {
			rd_format.shareable_formats.push_back(texture.rd_format);
			rd_format.shareable_formats.push_back(texture.rd_format_srgb);
		}
	}
	{
		rd_view.swizzle_r = ret_format.swizzle_r;
		rd_view.swizzle_g = ret_format.swizzle_g;
		rd_view.swizzle_b = ret_format.swizzle_b;
		rd_view.swizzle_a = ret_format.swizzle_a;
	}
	Vector<Vector<uint8_t>> data_slices;
	data_slices.push_back(all_data); //one slice

	texture.rd_texture = RD::get_singleton()->texture_create(rd_format, rd_view, data_slices);
	ERR_FAIL_COND(texture.rd_texture.is_null());
	if (texture.rd_format_srgb != RD::DATA_FORMAT_MAX) {
		rd_view.format_override = texture.rd_format_srgb;
		texture.rd_texture_srgb = RD::get_singleton()->texture_create_shared(rd_view, texture.rd_texture);
		if (texture.rd_texture_srgb.is_null()) {
			RD::get_singleton()->free(texture.rd_texture);
			ERR_FAIL_COND(texture.rd_texture_srgb.is_null());
		}
	}

	//used for 2D, overridable
	texture.width_2d = texture.width;
	texture.height_2d = texture.height;
	texture.is_render_target = false;
	texture.rd_view = rd_view;
	texture.is_proxy = false;

	texture_owner.initialize_rid(p_texture, texture);
}

void TextureStorage::texture_external_initialize(RID p_texture, int p_width, int p_height, uint64_t p_external_buffer) {
}

void TextureStorage::texture_proxy_initialize(RID p_texture, RID p_base) {
	Texture *tex = texture_owner.get_or_null(p_base);
	ERR_FAIL_NULL(tex);
	Texture proxy_tex = *tex;

	proxy_tex.rd_view.format_override = tex->rd_format;
	proxy_tex.rd_texture = RD::get_singleton()->texture_create_shared(proxy_tex.rd_view, tex->rd_texture);
	if (proxy_tex.rd_texture_srgb.is_valid()) {
		proxy_tex.rd_view.format_override = tex->rd_format_srgb;
		proxy_tex.rd_texture_srgb = RD::get_singleton()->texture_create_shared(proxy_tex.rd_view, tex->rd_texture);
	}
	proxy_tex.proxy_to = p_base;
	proxy_tex.is_render_target = false;
	proxy_tex.is_proxy = true;
	proxy_tex.proxies.clear();

	texture_owner.initialize_rid(p_texture, proxy_tex);

	tex->proxies.push_back(p_texture);
}

// Note: We make some big assumptions about format and usage. If developers need more control,
// they should use RD::texture_create_from_extension() instead.
RID TextureStorage::texture_create_from_native_handle(RS::TextureType p_type, Image::Format p_format, uint64_t p_native_handle, int p_width, int p_height, int p_depth, int p_layers, RS::TextureLayeredType p_layered_type) {
	RD::TextureType type;
	switch (p_type) {
		case RS::TEXTURE_TYPE_2D:
			type = RD::TEXTURE_TYPE_2D;
			break;

		case RS::TEXTURE_TYPE_3D:
			type = RD::TEXTURE_TYPE_3D;
			break;

		case RS::TEXTURE_TYPE_LAYERED:
			if (p_layered_type == RS::TEXTURE_LAYERED_2D_ARRAY) {
				type = RD::TEXTURE_TYPE_2D_ARRAY;
			} else if (p_layered_type == RS::TEXTURE_LAYERED_CUBEMAP) {
				type = RD::TEXTURE_TYPE_CUBE;
			} else if (p_layered_type == RS::TEXTURE_LAYERED_CUBEMAP_ARRAY) {
				type = RD::TEXTURE_TYPE_CUBE_ARRAY;
			} else {
				// Arbitrary fallback.
				type = RD::TEXTURE_TYPE_2D_ARRAY;
			}
			break;

		default:
			// Arbitrary fallback.
			type = RD::TEXTURE_TYPE_2D;
	}

	// Only a rough conversion - see note above.
	RD::DataFormat format;
	switch (p_format) {
		case Image::FORMAT_L8:
		case Image::FORMAT_R8:
			format = RD::DATA_FORMAT_R8_UNORM;
			break;

		case Image::FORMAT_LA8:
		case Image::FORMAT_RG8:
			format = RD::DATA_FORMAT_R8G8_UNORM;
			break;

		case Image::FORMAT_RGB8:
			format = RD::DATA_FORMAT_R8G8B8_UNORM;
			break;

		case Image::FORMAT_RGBA8:
			format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
			break;

		case Image::FORMAT_RGBA4444:
			format = RD::DATA_FORMAT_B4G4R4A4_UNORM_PACK16;
			break;

		case Image::FORMAT_RGB565:
			format = RD::DATA_FORMAT_B5G6R5_UNORM_PACK16;
			break;

		case Image::FORMAT_RF:
			format = RD::DATA_FORMAT_R32_SFLOAT;
			break;

		case Image::FORMAT_RGF:
			format = RD::DATA_FORMAT_R32G32_SFLOAT;
			break;

		case Image::FORMAT_RGBF:
			format = RD::DATA_FORMAT_R32G32B32_SFLOAT;
			break;

		case Image::FORMAT_RGBAF:
			format = RD::DATA_FORMAT_R32G32B32_SFLOAT;
			break;

		case Image::FORMAT_RH:
			format = RD::DATA_FORMAT_R16_SFLOAT;
			break;

		case Image::FORMAT_RGH:
			format = RD::DATA_FORMAT_R16G16_SFLOAT;
			break;

		case Image::FORMAT_RGBH:
			format = RD::DATA_FORMAT_R16G16B16_SFLOAT;
			break;

		case Image::FORMAT_RGBAH:
			format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
			break;

		case Image::FORMAT_RGBE9995:
			format = RD::DATA_FORMAT_E5B9G9R9_UFLOAT_PACK32;
			break;

		case Image::FORMAT_DXT1:
			format = RD::DATA_FORMAT_BC1_RGB_UNORM_BLOCK;
			break;

		case Image::FORMAT_DXT3:
			format = RD::DATA_FORMAT_BC2_UNORM_BLOCK;
			break;

		case Image::FORMAT_DXT5:
			format = RD::DATA_FORMAT_BC3_UNORM_BLOCK;
			break;

		case Image::FORMAT_RGTC_R:
			format = RD::DATA_FORMAT_BC4_UNORM_BLOCK;
			break;

		case Image::FORMAT_RGTC_RG:
			format = RD::DATA_FORMAT_BC5_UNORM_BLOCK;
			break;

		case Image::FORMAT_BPTC_RGBA:
			format = RD::DATA_FORMAT_BC7_UNORM_BLOCK;
			break;

		case Image::FORMAT_BPTC_RGBF:
			format = RD::DATA_FORMAT_BC6H_SFLOAT_BLOCK;
			break;

		case Image::FORMAT_BPTC_RGBFU:
			format = RD::DATA_FORMAT_BC6H_UFLOAT_BLOCK;
			break;

		case Image::FORMAT_ETC:
			format = RD::DATA_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
			break;

		case Image::FORMAT_ETC2_R11:
			format = RD::DATA_FORMAT_EAC_R11_UNORM_BLOCK;
			break;

		case Image::FORMAT_ETC2_R11S:
			format = RD::DATA_FORMAT_EAC_R11_SNORM_BLOCK;
			break;

		case Image::FORMAT_ETC2_RG11:
			format = RD::DATA_FORMAT_EAC_R11G11_UNORM_BLOCK;
			break;

		case Image::FORMAT_ETC2_RG11S:
			format = RD::DATA_FORMAT_EAC_R11G11_SNORM_BLOCK;
			break;

		case Image::FORMAT_ETC2_RGB8:
			format = RD::DATA_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
			break;

		case Image::FORMAT_ETC2_RGBA8:
			format = RD::DATA_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
			break;

		case Image::FORMAT_ETC2_RGB8A1:
			format = RD::DATA_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
			break;

		case Image::FORMAT_ETC2_RA_AS_RG:
			format = RD::DATA_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
			break;

		case Image::FORMAT_DXT5_RA_AS_RG:
			format = RD::DATA_FORMAT_BC3_UNORM_BLOCK;
			break;

		case Image::FORMAT_ASTC_4x4:
		case Image::FORMAT_ASTC_4x4_HDR:
			format = RD::DATA_FORMAT_ASTC_4x4_UNORM_BLOCK;
			break;

		case Image::FORMAT_ASTC_8x8:
		case Image::FORMAT_ASTC_8x8_HDR:
			format = RD::DATA_FORMAT_ASTC_8x8_UNORM_BLOCK;
			break;

		default:
			// Arbitrary fallback.
			format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
	}

	// Assumed to be a color attachment - see note above.
	uint64_t usage_flags = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

	return RD::get_singleton()->texture_create_from_extension(type, format, RD::TEXTURE_SAMPLES_1, usage_flags, p_native_handle, p_width, p_height, p_depth, p_layers);
}

void TextureStorage::_texture_2d_update(RID p_texture, const Ref<Image> &p_image, int p_layer, bool p_immediate) {
	ERR_FAIL_COND(p_image.is_null() || p_image->is_empty());

	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(tex);
	ERR_FAIL_COND(tex->is_render_target);
	ERR_FAIL_COND(p_image->get_width() != tex->width || p_image->get_height() != tex->height);
	ERR_FAIL_COND(p_image->get_format() != tex->format);

	if (tex->type == TextureStorage::TYPE_LAYERED) {
		ERR_FAIL_INDEX(p_layer, tex->layers);
	}

#ifdef TOOLS_ENABLED
	tex->image_cache_2d.unref();
#endif
	TextureToRDFormat f;
	Ref<Image> validated = _validate_texture_format(p_image, f);

	RD::get_singleton()->texture_update(tex->rd_texture, p_layer, validated->get_data());
}

void TextureStorage::texture_2d_update(RID p_texture, const Ref<Image> &p_image, int p_layer) {
	_texture_2d_update(p_texture, p_image, p_layer, false);
}

void TextureStorage::texture_3d_update(RID p_texture, const Vector<Ref<Image>> &p_data) {
	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(tex);
	ERR_FAIL_COND(tex->type != TextureStorage::TYPE_3D);

	Image::Image3DValidateError verr = Image::validate_3d_image(tex->format, tex->width, tex->height, tex->depth, tex->mipmaps > 1, p_data);
	if (verr != Image::VALIDATE_3D_OK) {
		ERR_FAIL_MSG(Image::get_3d_image_validation_error_text(verr));
	}

	Vector<uint8_t> all_data;
	{
		Vector<Ref<Image>> images;
		uint32_t all_data_size = 0;
		images.resize(p_data.size());
		for (int i = 0; i < p_data.size(); i++) {
			Ref<Image> image = p_data[i];
			if (image->get_format() != tex->validated_format) {
				image = image->duplicate();
				image->convert(tex->validated_format);
			}
			all_data_size += image->get_data().size();
			images.write[i] = image;
		}

		all_data.resize(all_data_size); //consolidate all data here
		uint32_t offset = 0;

		for (int i = 0; i < p_data.size(); i++) {
			uint32_t s = images[i]->get_data().size();
			memcpy(&all_data.write[offset], images[i]->get_data().ptr(), s);
			offset += s;
		}
	}

	RD::get_singleton()->texture_update(tex->rd_texture, 0, all_data);
}

void TextureStorage::texture_external_update(RID p_texture, int p_width, int p_height, uint64_t p_external_buffer) {
}

void TextureStorage::texture_proxy_update(RID p_texture, RID p_proxy_to) {
	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(tex);
	ERR_FAIL_COND(!tex->is_proxy);
	Texture *proxy_to = texture_owner.get_or_null(p_proxy_to);
	ERR_FAIL_NULL(proxy_to);
	ERR_FAIL_COND(proxy_to->is_proxy);

	if (tex->proxy_to.is_valid()) {
		//unlink proxy
		if (RD::get_singleton()->texture_is_valid(tex->rd_texture)) {
			RD::get_singleton()->free(tex->rd_texture);
			tex->rd_texture = RID();
		}
		if (RD::get_singleton()->texture_is_valid(tex->rd_texture_srgb)) {
			RD::get_singleton()->free(tex->rd_texture_srgb);
			tex->rd_texture_srgb = RID();
		}
		Texture *prev_tex = texture_owner.get_or_null(tex->proxy_to);
		ERR_FAIL_NULL(prev_tex);
		prev_tex->proxies.erase(p_texture);
	}

	// Copy canvas_texture so it doesn't leak.
	CanvasTexture *canvas_texture = tex->canvas_texture;

	*tex = *proxy_to;

	tex->proxy_to = p_proxy_to;
	tex->is_render_target = false;
	tex->is_proxy = true;
	tex->proxies.clear();
	proxy_to->proxies.push_back(p_texture);
	tex->canvas_texture = canvas_texture;

	tex->rd_view.format_override = tex->rd_format;
	tex->rd_texture = RD::get_singleton()->texture_create_shared(tex->rd_view, proxy_to->rd_texture);
	if (tex->rd_texture_srgb.is_valid()) {
		tex->rd_view.format_override = tex->rd_format_srgb;
		tex->rd_texture_srgb = RD::get_singleton()->texture_create_shared(tex->rd_view, proxy_to->rd_texture);
	}
}

//these two APIs can be used together or in combination with the others.
void TextureStorage::texture_2d_placeholder_initialize(RID p_texture) {
	texture_2d_initialize(p_texture, texture_2d_placeholder);
}

void TextureStorage::texture_2d_layered_placeholder_initialize(RID p_texture, RS::TextureLayeredType p_layered_type) {
	if (p_layered_type == RS::TEXTURE_LAYERED_2D_ARRAY) {
		texture_2d_layered_initialize(p_texture, texture_2d_array_placeholder, p_layered_type);
	} else {
		texture_2d_layered_initialize(p_texture, cubemap_placeholder, p_layered_type);
	}
}

void TextureStorage::texture_3d_placeholder_initialize(RID p_texture) {
	texture_3d_initialize(p_texture, Image::FORMAT_RGBA8, 4, 4, 4, false, texture_3d_placeholder);
}

Ref<Image> TextureStorage::texture_2d_get(RID p_texture) const {
	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(tex, Ref<Image>());

#ifdef TOOLS_ENABLED
	if (tex->image_cache_2d.is_valid() && !tex->is_render_target) {
		return tex->image_cache_2d;
	}
#endif
	Vector<uint8_t> data = RD::get_singleton()->texture_get_data(tex->rd_texture, 0);
	ERR_FAIL_COND_V(data.is_empty(), Ref<Image>());
	Ref<Image> image;

	// Expand RGB10_A2 into RGBAH. This is needed for capturing viewport data
	// when using the mobile renderer with HDR mode on.
	if (tex->rd_format == RD::DATA_FORMAT_A2B10G10R10_UNORM_PACK32) {
		Vector<uint8_t> new_data;
		new_data.resize(data.size() * 2);
		uint16_t *ndp = (uint16_t *)new_data.ptr();

		uint32_t *ptr = (uint32_t *)data.ptr();
		uint32_t num_pixels = data.size() / 4;

		for (uint32_t ofs = 0; ofs < num_pixels; ofs++) {
			uint32_t px = ptr[ofs];
			uint32_t r = (px & 0x3FF);
			uint32_t g = ((px >> 10) & 0x3FF);
			uint32_t b = ((px >> 20) & 0x3FF);
			uint32_t a = ((px >> 30) & 0x3);

			ndp[ofs * 4 + 0] = Math::make_half_float(float(r) / 1023.0);
			ndp[ofs * 4 + 1] = Math::make_half_float(float(g) / 1023.0);
			ndp[ofs * 4 + 2] = Math::make_half_float(float(b) / 1023.0);
			ndp[ofs * 4 + 3] = Math::make_half_float(float(a) / 3.0);
		}
		image = Image::create_from_data(tex->width, tex->height, tex->mipmaps > 1, tex->validated_format, new_data);
	} else {
		image = Image::create_from_data(tex->width, tex->height, tex->mipmaps > 1, tex->validated_format, data);
	}

	if (image->is_empty()) {
		const String &path_str = tex->path.is_empty() ? "with no path" : vformat("with path '%s'", tex->path);
		ERR_FAIL_V_MSG(Ref<Image>(), vformat("Texture %s has no data.", path_str));
	}

	if (tex->format != tex->validated_format) {
		image->convert(tex->format);
	}

#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint() && !tex->is_render_target) {
		tex->image_cache_2d = image;
	}
#endif

	return image;
}

Ref<Image> TextureStorage::texture_2d_layer_get(RID p_texture, int p_layer) const {
	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(tex, Ref<Image>());

	Vector<uint8_t> data = RD::get_singleton()->texture_get_data(tex->rd_texture, p_layer);
	ERR_FAIL_COND_V(data.is_empty(), Ref<Image>());
	Ref<Image> image = Image::create_from_data(tex->width, tex->height, tex->mipmaps > 1, tex->validated_format, data);
	if (image->is_empty()) {
		const String &path_str = tex->path.is_empty() ? "with no path" : vformat("with path '%s'", tex->path);
		ERR_FAIL_V_MSG(Ref<Image>(), vformat("Texture %s has no data.", path_str));
	}
	if (tex->format != tex->validated_format) {
		image->convert(tex->format);
	}

	return image;
}

Vector<Ref<Image>> TextureStorage::texture_3d_get(RID p_texture) const {
	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(tex, Vector<Ref<Image>>());
	ERR_FAIL_COND_V(tex->type != TextureStorage::TYPE_3D, Vector<Ref<Image>>());

	Vector<uint8_t> all_data = RD::get_singleton()->texture_get_data(tex->rd_texture, 0);

	ERR_FAIL_COND_V(all_data.size() != (int)tex->buffer_size_3d, Vector<Ref<Image>>());

	Vector<Ref<Image>> ret;

	for (int i = 0; i < tex->buffer_slices_3d.size(); i++) {
		const Texture::BufferSlice3D &bs = tex->buffer_slices_3d[i];
		ERR_FAIL_COND_V(bs.offset >= (uint32_t)all_data.size(), Vector<Ref<Image>>());
		ERR_FAIL_COND_V(bs.offset + bs.buffer_size > (uint32_t)all_data.size(), Vector<Ref<Image>>());
		Vector<uint8_t> sub_region = all_data.slice(bs.offset, bs.offset + bs.buffer_size);

		Ref<Image> img = Image::create_from_data(bs.size.width, bs.size.height, false, tex->validated_format, sub_region);
		ERR_FAIL_COND_V(img->is_empty(), Vector<Ref<Image>>());
		if (img->is_empty()) {
			const String &path_str = tex->path.is_empty() ? "with no path" : vformat("with path '%s'", tex->path);
			ERR_FAIL_V_MSG(Vector<Ref<Image>>(), vformat("Texture %s has no data.", path_str));
		}
		if (tex->format != tex->validated_format) {
			img->convert(tex->format);
		}

		ret.push_back(img);
	}

	return ret;
}

void TextureStorage::texture_replace(RID p_texture, RID p_by_texture) {
	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(tex);
	ERR_FAIL_COND(tex->proxy_to.is_valid()); //can't replace proxy
	Texture *by_tex = texture_owner.get_or_null(p_by_texture);
	ERR_FAIL_NULL(by_tex);
	ERR_FAIL_COND(by_tex->proxy_to.is_valid()); //can't replace proxy

	if (tex == by_tex) {
		return;
	}

	if (tex->rd_texture_srgb.is_valid()) {
		RD::get_singleton()->free(tex->rd_texture_srgb);
	}
	RD::get_singleton()->free(tex->rd_texture);

	if (tex->canvas_texture) {
		memdelete(tex->canvas_texture);
		tex->canvas_texture = nullptr;
	}

	Vector<RID> proxies_to_update = tex->proxies;
	Vector<RID> proxies_to_redirect = by_tex->proxies;

	*tex = *by_tex;

	tex->proxies = proxies_to_update; //restore proxies, so they can be updated

	if (tex->canvas_texture) {
		tex->canvas_texture->diffuse = p_texture; //update
	}

	for (int i = 0; i < proxies_to_update.size(); i++) {
		texture_proxy_update(proxies_to_update[i], p_texture);
	}
	for (int i = 0; i < proxies_to_redirect.size(); i++) {
		texture_proxy_update(proxies_to_redirect[i], p_texture);
	}
	//delete last, so proxies can be updated
	texture_owner.free(p_by_texture);

	decal_atlas_mark_dirty_on_texture(p_texture);
}

void TextureStorage::texture_set_size_override(RID p_texture, int p_width, int p_height) {
	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(tex);
	ERR_FAIL_COND(tex->type != TextureStorage::TYPE_2D);

	tex->width_2d = p_width;
	tex->height_2d = p_height;
}

void TextureStorage::texture_set_path(RID p_texture, const String &p_path) {
	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(tex);

	tex->path = p_path;
}

String TextureStorage::texture_get_path(RID p_texture) const {
	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(tex, String());

	return tex->path;
}

Image::Format TextureStorage::texture_get_format(RID p_texture) const {
	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(tex, Image::FORMAT_MAX);

	return tex->format;
}

void TextureStorage::texture_set_detect_3d_callback(RID p_texture, RS::TextureDetectCallback p_callback, void *p_userdata) {
	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(tex);

	tex->detect_3d_callback_ud = p_userdata;
	tex->detect_3d_callback = p_callback;
}

void TextureStorage::texture_set_detect_normal_callback(RID p_texture, RS::TextureDetectCallback p_callback, void *p_userdata) {
	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(tex);

	tex->detect_normal_callback_ud = p_userdata;
	tex->detect_normal_callback = p_callback;
}

void TextureStorage::texture_set_detect_roughness_callback(RID p_texture, RS::TextureDetectRoughnessCallback p_callback, void *p_userdata) {
	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(tex);

	tex->detect_roughness_callback_ud = p_userdata;
	tex->detect_roughness_callback = p_callback;
}

void TextureStorage::texture_debug_usage(List<RS::TextureInfo> *r_info) {
	List<RID> textures;
	texture_owner.get_owned_list(&textures);

	for (List<RID>::Element *E = textures.front(); E; E = E->next()) {
		Texture *t = texture_owner.get_or_null(E->get());
		if (!t) {
			continue;
		}
		RS::TextureInfo tinfo;
		tinfo.path = t->path;
		tinfo.format = t->format;
		tinfo.width = t->width;
		tinfo.height = t->height;
		tinfo.bytes = Image::get_image_data_size(t->width, t->height, t->format, t->mipmaps > 1);

		switch (t->type) {
			case TextureType::TYPE_3D:
				tinfo.depth = t->depth;
				tinfo.bytes *= t->depth;
				break;

			case TextureType::TYPE_LAYERED:
				tinfo.depth = t->layers;
				tinfo.bytes *= t->layers;
				break;

			default:
				tinfo.depth = 0;
				break;
		}

		r_info->push_back(tinfo);
	}
}

void TextureStorage::texture_set_force_redraw_if_visible(RID p_texture, bool p_enable) {
}

Size2 TextureStorage::texture_size_with_proxy(RID p_proxy) {
	return texture_2d_get_size(p_proxy);
}

void TextureStorage::texture_rd_initialize(RID p_texture, const RID &p_rd_texture, const RS::TextureLayeredType p_layer_type) {
	ERR_FAIL_COND(!RD::get_singleton()->texture_is_valid(p_rd_texture));

	// TODO : investigate if we can support this, will need to be able to obtain the order and obtain the slice info
	ERR_FAIL_COND_MSG(RD::get_singleton()->texture_is_shared(p_rd_texture), "Please create the texture object using the original texture");

	RD::TextureFormat tf = RD::get_singleton()->texture_get_format(p_rd_texture);
	ERR_FAIL_COND(!(tf.usage_bits & RD::TEXTURE_USAGE_SAMPLING_BIT));

	TextureFromRDFormat imfmt;
	_texture_format_from_rd(tf.format, imfmt);
	ERR_FAIL_COND(imfmt.image_format == Image::FORMAT_MAX);

	Texture texture;

	switch (tf.texture_type) {
		case RD::TEXTURE_TYPE_2D: {
			ERR_FAIL_COND(tf.array_layers != 1);
			texture.type = TextureStorage::TYPE_2D;
		} break;
		case RD::TEXTURE_TYPE_2D_ARRAY:
		case RD::TEXTURE_TYPE_CUBE:
		case RD::TEXTURE_TYPE_CUBE_ARRAY: {
			// RenderingDevice doesn't distinguish between Array textures and Cube textures
			// this condition covers TextureArrays, TextureCube, and TextureCubeArray.
			ERR_FAIL_COND(tf.array_layers == 1);
			texture.type = TextureStorage::TYPE_LAYERED;
			texture.layered_type = p_layer_type;
		} break;
		case RD::TEXTURE_TYPE_3D: {
			ERR_FAIL_COND(tf.array_layers != 1);
			texture.type = TextureStorage::TYPE_3D;
		} break;
		default: {
			ERR_FAIL_MSG("This RD texture can't be used as a render texture");
		} break;
	}

	texture.width = tf.width;
	texture.height = tf.height;
	texture.depth = tf.depth;
	texture.layers = tf.array_layers;
	texture.mipmaps = tf.mipmaps;
	texture.format = imfmt.image_format;
	texture.validated_format = texture.format; // ??

	RD::TextureView rd_view;
	rd_view.format_override = imfmt.rd_format == tf.format ? RD::DATA_FORMAT_MAX : imfmt.rd_format;
	rd_view.swizzle_r = imfmt.swizzle_r;
	rd_view.swizzle_g = imfmt.swizzle_g;
	rd_view.swizzle_b = imfmt.swizzle_b;
	rd_view.swizzle_a = imfmt.swizzle_a;

	texture.rd_type = tf.texture_type;
	texture.rd_view = rd_view;
	texture.rd_format = imfmt.rd_format;
	// We create a shared texture here even if our view matches, so we don't obtain ownership.
	texture.rd_texture = RD::get_singleton()->texture_create_shared(rd_view, p_rd_texture);
	if (imfmt.rd_format_srgb != RD::DATA_FORMAT_MAX) {
		rd_view.format_override = imfmt.rd_format_srgb == tf.format ? RD::DATA_FORMAT_MAX : imfmt.rd_format;
		texture.rd_format_srgb = imfmt.rd_format_srgb;
		// We create a shared texture here even if our view matches, so we don't obtain ownership.
		texture.rd_texture_srgb = RD::get_singleton()->texture_create_shared(rd_view, p_rd_texture);
	}

	// TODO figure out what to do with slices

	texture.width_2d = texture.width;
	texture.height_2d = texture.height;
	texture.is_render_target = false;
	texture.is_proxy = false;

	texture_owner.initialize_rid(p_texture, texture);
}

RID TextureStorage::texture_get_rd_texture(RID p_texture, bool p_srgb) const {
	if (p_texture.is_null()) {
		return RID();
	}

	Texture *tex = texture_owner.get_or_null(p_texture);
	if (!tex) {
		return RID();
	}

	return (p_srgb && tex->rd_texture_srgb.is_valid()) ? tex->rd_texture_srgb : tex->rd_texture;
}

uint64_t TextureStorage::texture_get_native_handle(RID p_texture, bool p_srgb) const {
	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(tex, 0);

	if (p_srgb && tex->rd_texture_srgb.is_valid()) {
		return RD::get_singleton()->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE, tex->rd_texture_srgb);
	} else {
		return RD::get_singleton()->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE, tex->rd_texture);
	}
}

Ref<Image> TextureStorage::_validate_texture_format(const Ref<Image> &p_image, TextureToRDFormat &r_format) {
	Ref<Image> image = p_image->duplicate();

	switch (p_image->get_format()) {
		case Image::FORMAT_L8: {
			r_format.format = RD::DATA_FORMAT_R8_UNORM;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break; //luminance
		case Image::FORMAT_LA8: {
			r_format.format = RD::DATA_FORMAT_R8G8_UNORM;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_G;
		} break; //luminance-alpha
		case Image::FORMAT_R8: {
			r_format.format = RD::DATA_FORMAT_R8_UNORM;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break;
		case Image::FORMAT_RG8: {
			r_format.format = RD::DATA_FORMAT_R8G8_UNORM;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break;
		case Image::FORMAT_RGB8: {
			//this format is not mandatory for specification, check if supported first
			if (false && RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_R8G8B8_UNORM, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT) && RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_R8G8B8_SRGB, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT)) {
				r_format.format = RD::DATA_FORMAT_R8G8B8_UNORM;
				r_format.format_srgb = RD::DATA_FORMAT_R8G8B8_SRGB;
			} else {
				//not supported, reconvert
				r_format.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
				r_format.format_srgb = RD::DATA_FORMAT_R8G8B8A8_SRGB;
				image->convert(Image::FORMAT_RGBA8);
			}
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;

		} break;
		case Image::FORMAT_RGBA8: {
			r_format.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
			r_format.format_srgb = RD::DATA_FORMAT_R8G8B8A8_SRGB;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;
		} break;
		case Image::FORMAT_RGBA4444: {
			r_format.format = RD::DATA_FORMAT_B4G4R4A4_UNORM_PACK16;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_B; //needs swizzle
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;
		} break;
		case Image::FORMAT_RGB565: {
			r_format.format = RD::DATA_FORMAT_B5G6R5_UNORM_PACK16;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;
		} break;
		case Image::FORMAT_RF: {
			r_format.format = RD::DATA_FORMAT_R32_SFLOAT;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break; //float
		case Image::FORMAT_RGF: {
			r_format.format = RD::DATA_FORMAT_R32G32_SFLOAT;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break;
		case Image::FORMAT_RGBF: {
			//this format is not mandatory for specification, check if supported first
			if (RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_R32G32B32_SFLOAT, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT)) {
				r_format.format = RD::DATA_FORMAT_R32G32B32_SFLOAT;
			} else {
				//not supported, reconvert
				r_format.format = RD::DATA_FORMAT_R32G32B32A32_SFLOAT;
				image->convert(Image::FORMAT_RGBAF);
			}

			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break;
		case Image::FORMAT_RGBAF: {
			r_format.format = RD::DATA_FORMAT_R32G32B32A32_SFLOAT;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;

		} break;
		case Image::FORMAT_RH: {
			r_format.format = RD::DATA_FORMAT_R16_SFLOAT;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;

		} break; //half float
		case Image::FORMAT_RGH: {
			r_format.format = RD::DATA_FORMAT_R16G16_SFLOAT;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;

		} break;
		case Image::FORMAT_RGBH: {
			//this format is not mandatory for specification, check if supported first
			if (RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_R16G16B16_SFLOAT, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT)) {
				r_format.format = RD::DATA_FORMAT_R16G16B16_SFLOAT;
			} else {
				//not supported, reconvert
				r_format.format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
				image->convert(Image::FORMAT_RGBAH);
			}

			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break;
		case Image::FORMAT_RGBAH: {
			r_format.format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;

		} break;
		case Image::FORMAT_RGBE9995: {
			r_format.format = RD::DATA_FORMAT_E5B9G9R9_UFLOAT_PACK32;
			// TODO: Need to make a function in Image to swap bits for this.
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_IDENTITY;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_IDENTITY;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_IDENTITY;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_IDENTITY;
		} break;
		case Image::FORMAT_DXT1: {
			if (RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_BC1_RGB_UNORM_BLOCK, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT)) {
				r_format.format = RD::DATA_FORMAT_BC1_RGB_UNORM_BLOCK;
				r_format.format_srgb = RD::DATA_FORMAT_BC1_RGB_SRGB_BLOCK;
			} else {
				//not supported, reconvert
				r_format.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
				r_format.format_srgb = RD::DATA_FORMAT_R8G8B8A8_SRGB;
				image->decompress();
				image->convert(Image::FORMAT_RGBA8);
			}
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;

		} break; //s3tc bc1
		case Image::FORMAT_DXT3: {
			if (RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_BC2_UNORM_BLOCK, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT)) {
				r_format.format = RD::DATA_FORMAT_BC2_UNORM_BLOCK;
				r_format.format_srgb = RD::DATA_FORMAT_BC2_SRGB_BLOCK;
			} else {
				//not supported, reconvert
				r_format.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
				r_format.format_srgb = RD::DATA_FORMAT_R8G8B8A8_SRGB;
				image->decompress();
				image->convert(Image::FORMAT_RGBA8);
			}
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;

		} break; //bc2
		case Image::FORMAT_DXT5: {
			if (RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_BC3_UNORM_BLOCK, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT)) {
				r_format.format = RD::DATA_FORMAT_BC3_UNORM_BLOCK;
				r_format.format_srgb = RD::DATA_FORMAT_BC3_SRGB_BLOCK;
			} else {
				//not supported, reconvert
				r_format.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
				r_format.format_srgb = RD::DATA_FORMAT_R8G8B8A8_SRGB;
				image->decompress();
				image->convert(Image::FORMAT_RGBA8);
			}
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;
		} break; //bc3
		case Image::FORMAT_RGTC_R: {
			if (RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_BC4_UNORM_BLOCK, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT)) {
				r_format.format = RD::DATA_FORMAT_BC4_UNORM_BLOCK;
			} else {
				//not supported, reconvert
				r_format.format = RD::DATA_FORMAT_R8_UNORM;
				image->decompress();
				image->convert(Image::FORMAT_R8);
			}
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;

		} break;
		case Image::FORMAT_RGTC_RG: {
			if (RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_BC5_UNORM_BLOCK, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT)) {
				r_format.format = RD::DATA_FORMAT_BC5_UNORM_BLOCK;
			} else {
				//not supported, reconvert
				r_format.format = RD::DATA_FORMAT_R8G8_UNORM;
				image->decompress();
				image->convert(Image::FORMAT_RG8);
			}
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;

		} break;
		case Image::FORMAT_BPTC_RGBA: {
			if (RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_BC7_UNORM_BLOCK, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT)) {
				r_format.format = RD::DATA_FORMAT_BC7_UNORM_BLOCK;
				r_format.format_srgb = RD::DATA_FORMAT_BC7_SRGB_BLOCK;
			} else {
				//not supported, reconvert
				r_format.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
				r_format.format_srgb = RD::DATA_FORMAT_R8G8B8A8_SRGB;
				image->decompress();
				image->convert(Image::FORMAT_RGBA8);
			}
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;

		} break; //btpc bc7
		case Image::FORMAT_BPTC_RGBF: {
			if (RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_BC6H_SFLOAT_BLOCK, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT)) {
				r_format.format = RD::DATA_FORMAT_BC6H_SFLOAT_BLOCK;
			} else {
				//not supported, reconvert
				r_format.format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
				image->decompress();
				image->convert(Image::FORMAT_RGBAH);
			}
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break; //float bc6h
		case Image::FORMAT_BPTC_RGBFU: {
			if (RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_BC6H_UFLOAT_BLOCK, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT)) {
				r_format.format = RD::DATA_FORMAT_BC6H_UFLOAT_BLOCK;
			} else {
				//not supported, reconvert
				r_format.format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
				image->decompress();
				image->convert(Image::FORMAT_RGBAH);
			}
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break; //unsigned float bc6hu
		case Image::FORMAT_ETC2_R11: {
			if (RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_EAC_R11_UNORM_BLOCK, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT)) {
				r_format.format = RD::DATA_FORMAT_EAC_R11_UNORM_BLOCK;
			} else {
				//not supported, reconvert
				r_format.format = RD::DATA_FORMAT_R8_UNORM;
				image->decompress();
				image->convert(Image::FORMAT_R8);
			}
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;

		} break; //etc2
		case Image::FORMAT_ETC2_R11S: {
			if (RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_EAC_R11_SNORM_BLOCK, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT)) {
				r_format.format = RD::DATA_FORMAT_EAC_R11_SNORM_BLOCK;
			} else {
				//not supported, reconvert
				r_format.format = RD::DATA_FORMAT_R8_SNORM;
				image->decompress();
				image->convert(Image::FORMAT_R8);
			}
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break; //signed: {} break; NOT srgb.
		case Image::FORMAT_ETC2_RG11: {
			if (RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_EAC_R11G11_UNORM_BLOCK, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT)) {
				r_format.format = RD::DATA_FORMAT_EAC_R11G11_UNORM_BLOCK;
			} else {
				//not supported, reconvert
				r_format.format = RD::DATA_FORMAT_R8G8_UNORM;
				image->decompress();
				image->convert(Image::FORMAT_RG8);
			}
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break;
		case Image::FORMAT_ETC2_RG11S: {
			if (RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_EAC_R11G11_SNORM_BLOCK, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT)) {
				r_format.format = RD::DATA_FORMAT_EAC_R11G11_SNORM_BLOCK;
			} else {
				//not supported, reconvert
				r_format.format = RD::DATA_FORMAT_R8G8_SNORM;
				image->decompress();
				image->convert(Image::FORMAT_RG8);
			}
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break;
		case Image::FORMAT_ETC:
		case Image::FORMAT_ETC2_RGB8: {
			//ETC2 is backwards compatible with ETC1, and all modern platforms support it
			if (RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_ETC2_R8G8B8_UNORM_BLOCK, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT)) {
				r_format.format = RD::DATA_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
				r_format.format_srgb = RD::DATA_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
			} else {
				//not supported, reconvert
				r_format.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
				r_format.format_srgb = RD::DATA_FORMAT_R8G8B8A8_SRGB;
				image->decompress();
				image->convert(Image::FORMAT_RGBA8);
			}
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;

		} break;
		case Image::FORMAT_ETC2_RGBA8: {
			if (RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT)) {
				r_format.format = RD::DATA_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
				r_format.format_srgb = RD::DATA_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
			} else {
				//not supported, reconvert
				r_format.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
				r_format.format_srgb = RD::DATA_FORMAT_R8G8B8A8_SRGB;
				image->decompress();
				image->convert(Image::FORMAT_RGBA8);
			}
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;
		} break;
		case Image::FORMAT_ETC2_RGB8A1: {
			if (RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT)) {
				r_format.format = RD::DATA_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
				r_format.format_srgb = RD::DATA_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK;
			} else {
				//not supported, reconvert
				r_format.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
				r_format.format_srgb = RD::DATA_FORMAT_R8G8B8A8_SRGB;
				image->decompress();
				image->convert(Image::FORMAT_RGBA8);
			}
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;
		} break;
		case Image::FORMAT_ETC2_RA_AS_RG: {
			if (RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT)) {
				r_format.format = RD::DATA_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
				r_format.format_srgb = RD::DATA_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
			} else {
				//not supported, reconvert
				r_format.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
				r_format.format_srgb = RD::DATA_FORMAT_R8G8B8A8_SRGB;
				image->decompress();
				image->convert(Image::FORMAT_RGBA8);
			}
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_A;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break;
		case Image::FORMAT_DXT5_RA_AS_RG: {
			if (RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_BC3_UNORM_BLOCK, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT)) {
				r_format.format = RD::DATA_FORMAT_BC3_UNORM_BLOCK;
				r_format.format_srgb = RD::DATA_FORMAT_BC3_SRGB_BLOCK;
			} else {
				//not supported, reconvert
				r_format.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
				r_format.format_srgb = RD::DATA_FORMAT_R8G8B8A8_SRGB;
				image->decompress();
				image->convert(Image::FORMAT_RGBA8);
			}
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_A;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break;
		case Image::FORMAT_ASTC_4x4:
		case Image::FORMAT_ASTC_4x4_HDR: {
			if (RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_ASTC_4x4_UNORM_BLOCK, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT)) {
				r_format.format = RD::DATA_FORMAT_ASTC_4x4_UNORM_BLOCK;
				if (p_image->get_format() == Image::FORMAT_ASTC_4x4) {
					r_format.format_srgb = RD::DATA_FORMAT_ASTC_4x4_SRGB_BLOCK;
				}
			} else {
				//not supported, reconvert
				image->decompress();
				if (p_image->get_format() == Image::FORMAT_ASTC_4x4) {
					r_format.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
					r_format.format_srgb = RD::DATA_FORMAT_R8G8B8A8_SRGB;
					image->convert(Image::FORMAT_RGBA8);
				} else {
					r_format.format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
					image->convert(Image::FORMAT_RGBAH);
				}
			}
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;

		} break; // astc 4x4
		case Image::FORMAT_ASTC_8x8:
		case Image::FORMAT_ASTC_8x8_HDR: {
			if (RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_ASTC_8x8_UNORM_BLOCK, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT)) {
				r_format.format = RD::DATA_FORMAT_ASTC_8x8_UNORM_BLOCK;
				if (p_image->get_format() == Image::FORMAT_ASTC_8x8) {
					r_format.format_srgb = RD::DATA_FORMAT_ASTC_8x8_SRGB_BLOCK;
				}
			} else {
				//not supported, reconvert
				image->decompress();
				if (p_image->get_format() == Image::FORMAT_ASTC_8x8) {
					r_format.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
					r_format.format_srgb = RD::DATA_FORMAT_R8G8B8A8_SRGB;
					image->convert(Image::FORMAT_RGBA8);
				} else {
					r_format.format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
					image->convert(Image::FORMAT_RGBAH);
				}
			}
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;

		} break; // astc 8x8

		default: {
		}
	}

	return image;
}

void TextureStorage::_texture_format_from_rd(RD::DataFormat p_rd_format, TextureFromRDFormat &r_format) {
	switch (p_rd_format) {
		case RD::DATA_FORMAT_R8_UNORM: {
			r_format.image_format = Image::FORMAT_L8;
			r_format.rd_format = RD::DATA_FORMAT_R8_UNORM;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break; //luminance
		case RD::DATA_FORMAT_R8G8_UNORM: {
			r_format.image_format = Image::FORMAT_LA8;
			r_format.rd_format = RD::DATA_FORMAT_R8G8_UNORM;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_G;
		} break; //luminance-alpha
		/* already maps to L8/LA8
		case RD::DATA_FORMAT_R8_UNORM: {
			r_format.image_format = Image::FORMAT_R8;
			r_format.rd_format = RD::DATA_FORMAT_R8_UNORM;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break;
		case RD::DATA_FORMAT_R8G8_UNORM: {
			r_format.image_format = Image::FORMAT_RG8;
			r_format.rd_format = RD::DATA_FORMAT_R8G8_UNORM;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break;
		*/
		case RD::DATA_FORMAT_R8G8B8_UNORM:
		case RD::DATA_FORMAT_R8G8B8_SRGB: {
			r_format.image_format = Image::FORMAT_RGB8;
			r_format.rd_format = RD::DATA_FORMAT_R8G8B8_UNORM;
			r_format.rd_format_srgb = RD::DATA_FORMAT_R8G8B8_SRGB;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;

		} break;
		case RD::DATA_FORMAT_R8G8B8A8_UNORM:
		case RD::DATA_FORMAT_R8G8B8A8_SRGB: {
			r_format.image_format = Image::FORMAT_RGBA8;
			r_format.rd_format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
			r_format.rd_format_srgb = RD::DATA_FORMAT_R8G8B8A8_SRGB;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;
		} break;
		case RD::DATA_FORMAT_B8G8R8A8_UNORM:
		case RD::DATA_FORMAT_B8G8R8A8_SRGB: {
			r_format.image_format = Image::FORMAT_RGBA8;
			r_format.rd_format = RD::DATA_FORMAT_B8G8R8A8_UNORM;
			r_format.rd_format_srgb = RD::DATA_FORMAT_B8G8R8A8_SRGB;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;
		} break;
		case RD::DATA_FORMAT_B4G4R4A4_UNORM_PACK16: {
			r_format.image_format = Image::FORMAT_RGBA4444;
			r_format.rd_format = RD::DATA_FORMAT_B4G4R4A4_UNORM_PACK16;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_B; //needs swizzle
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;
		} break;
		case RD::DATA_FORMAT_B5G6R5_UNORM_PACK16: {
			r_format.image_format = Image::FORMAT_RGB565;
			r_format.rd_format = RD::DATA_FORMAT_B5G6R5_UNORM_PACK16;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;
		} break;
		case RD::DATA_FORMAT_R32_SFLOAT: {
			r_format.image_format = Image::FORMAT_RF;
			r_format.rd_format = RD::DATA_FORMAT_R32_SFLOAT;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break; //float
		case RD::DATA_FORMAT_R32G32_SFLOAT: {
			r_format.image_format = Image::FORMAT_RGF;
			r_format.rd_format = RD::DATA_FORMAT_R32G32_SFLOAT;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break;
		case RD::DATA_FORMAT_R32G32B32_SFLOAT: {
			r_format.image_format = Image::FORMAT_RGBF;
			r_format.rd_format = RD::DATA_FORMAT_R32G32B32_SFLOAT;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break;
		case RD::DATA_FORMAT_R32G32B32A32_SFLOAT: {
			r_format.image_format = Image::FORMAT_RGBF;
			r_format.rd_format = RD::DATA_FORMAT_R32G32B32A32_SFLOAT;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;

		} break;
		case RD::DATA_FORMAT_R16_SFLOAT: {
			r_format.image_format = Image::FORMAT_RH;
			r_format.rd_format = RD::DATA_FORMAT_R16_SFLOAT;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;

		} break; //half float
		case RD::DATA_FORMAT_R16G16_SFLOAT: {
			r_format.image_format = Image::FORMAT_RGH;
			r_format.rd_format = RD::DATA_FORMAT_R16G16_SFLOAT;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;

		} break;
		case RD::DATA_FORMAT_R16G16B16_SFLOAT: {
			r_format.image_format = Image::FORMAT_RGBH;
			r_format.rd_format = RD::DATA_FORMAT_R16G16B16_SFLOAT;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break;
		case RD::DATA_FORMAT_R16G16B16A16_SFLOAT: {
			r_format.image_format = Image::FORMAT_RGBAH;
			r_format.rd_format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;

		} break;
		case RD::DATA_FORMAT_E5B9G9R9_UFLOAT_PACK32: {
			r_format.image_format = Image::FORMAT_RGBE9995;
			r_format.rd_format = RD::DATA_FORMAT_E5B9G9R9_UFLOAT_PACK32;
			// TODO: Need to make a function in Image to swap bits for this.
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_IDENTITY;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_IDENTITY;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_IDENTITY;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_IDENTITY;
		} break;
		case RD::DATA_FORMAT_BC1_RGB_UNORM_BLOCK:
		case RD::DATA_FORMAT_BC1_RGB_SRGB_BLOCK: {
			r_format.image_format = Image::FORMAT_DXT1;
			r_format.rd_format = RD::DATA_FORMAT_BC1_RGB_UNORM_BLOCK;
			r_format.rd_format_srgb = RD::DATA_FORMAT_BC1_RGB_SRGB_BLOCK;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;

		} break; //s3tc bc1
		case RD::DATA_FORMAT_BC2_UNORM_BLOCK:
		case RD::DATA_FORMAT_BC2_SRGB_BLOCK: {
			r_format.image_format = Image::FORMAT_DXT3;
			r_format.rd_format = RD::DATA_FORMAT_BC2_UNORM_BLOCK;
			r_format.rd_format_srgb = RD::DATA_FORMAT_BC2_SRGB_BLOCK;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;

		} break; //bc2
		case RD::DATA_FORMAT_BC3_UNORM_BLOCK:
		case RD::DATA_FORMAT_BC3_SRGB_BLOCK: {
			r_format.image_format = Image::FORMAT_DXT5;
			r_format.rd_format = RD::DATA_FORMAT_BC3_UNORM_BLOCK;
			r_format.rd_format_srgb = RD::DATA_FORMAT_BC3_SRGB_BLOCK;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;
		} break; //bc3
		case RD::DATA_FORMAT_BC4_UNORM_BLOCK: {
			r_format.image_format = Image::FORMAT_RGTC_R;
			r_format.rd_format = RD::DATA_FORMAT_BC4_UNORM_BLOCK;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;

		} break;
		case RD::DATA_FORMAT_BC5_UNORM_BLOCK: {
			r_format.image_format = Image::FORMAT_RGTC_RG;
			r_format.rd_format = RD::DATA_FORMAT_BC5_UNORM_BLOCK;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;

		} break;
		case RD::DATA_FORMAT_BC7_UNORM_BLOCK:
		case RD::DATA_FORMAT_BC7_SRGB_BLOCK: {
			r_format.image_format = Image::FORMAT_BPTC_RGBA;
			r_format.rd_format = RD::DATA_FORMAT_BC7_UNORM_BLOCK;
			r_format.rd_format_srgb = RD::DATA_FORMAT_BC7_SRGB_BLOCK;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;

		} break; //btpc bc7
		case RD::DATA_FORMAT_BC6H_SFLOAT_BLOCK: {
			r_format.image_format = Image::FORMAT_BPTC_RGBF;
			r_format.rd_format = RD::DATA_FORMAT_BC6H_SFLOAT_BLOCK;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break; //float bc6h
		case RD::DATA_FORMAT_BC6H_UFLOAT_BLOCK: {
			r_format.image_format = Image::FORMAT_BPTC_RGBFU;
			r_format.rd_format = RD::DATA_FORMAT_BC6H_UFLOAT_BLOCK;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break; //unsigned float bc6hu
		case RD::DATA_FORMAT_EAC_R11_UNORM_BLOCK: {
			r_format.image_format = Image::FORMAT_ETC2_R11;
			r_format.rd_format = RD::DATA_FORMAT_EAC_R11_UNORM_BLOCK;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;

		} break; //etc2
		case RD::DATA_FORMAT_EAC_R11_SNORM_BLOCK: {
			r_format.image_format = Image::FORMAT_ETC2_R11S;
			r_format.rd_format = RD::DATA_FORMAT_EAC_R11_SNORM_BLOCK;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break; //signed: {} break; NOT srgb.
		case RD::DATA_FORMAT_EAC_R11G11_UNORM_BLOCK: {
			r_format.image_format = Image::FORMAT_ETC2_RG11;
			r_format.rd_format = RD::DATA_FORMAT_EAC_R11G11_UNORM_BLOCK;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break;
		case RD::DATA_FORMAT_EAC_R11G11_SNORM_BLOCK: {
			r_format.image_format = Image::FORMAT_ETC2_RG11S;
			r_format.rd_format = RD::DATA_FORMAT_EAC_R11G11_SNORM_BLOCK;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break;
		case RD::DATA_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
		case RD::DATA_FORMAT_ETC2_R8G8B8_SRGB_BLOCK: {
			r_format.image_format = Image::FORMAT_ETC2_RGB8;
			r_format.rd_format = RD::DATA_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
			r_format.rd_format_srgb = RD::DATA_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;

		} break;
		/* already maps to FORMAT_ETC2_RGBA8
		case RD::DATA_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
		case RD::DATA_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK: {
			r_format.image_format = Image::FORMAT_ETC2_RGBA8;
			r_format.rd_format = RD::DATA_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
			r_format.rd_format_srgb = RD::DATA_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;
		} break;
		*/
		case RD::DATA_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
		case RD::DATA_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK: {
			r_format.image_format = Image::FORMAT_ETC2_RGB8A1;
			r_format.rd_format = RD::DATA_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
			r_format.rd_format_srgb = RD::DATA_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;
		} break;
		case RD::DATA_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
		case RD::DATA_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK: {
			r_format.image_format = Image::FORMAT_ETC2_RA_AS_RG;
			r_format.rd_format = RD::DATA_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
			r_format.rd_format_srgb = RD::DATA_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_A;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break;
		/* already maps to FORMAT_DXT5
		case RD::DATA_FORMAT_BC3_UNORM_BLOCK:
		case RD::DATA_FORMAT_BC3_SRGB_BLOCK: {
			r_format.image_format = Image::FORMAT_DXT5_RA_AS_RG;
			r_format.rd_format = RD::DATA_FORMAT_BC3_UNORM_BLOCK;
			r_format.rd_format_srgb = RD::DATA_FORMAT_BC3_SRGB_BLOCK;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_A;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_ZERO;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		} break;
		*/
		case RD::DATA_FORMAT_ASTC_4x4_UNORM_BLOCK: {
			// Q: Do we do as we do below, just create the sRGB variant?
			r_format.image_format = Image::FORMAT_ASTC_4x4;
			r_format.rd_format = RD::DATA_FORMAT_ASTC_4x4_UNORM_BLOCK;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;
		} break;
		case RD::DATA_FORMAT_ASTC_4x4_SRGB_BLOCK: {
			r_format.image_format = Image::FORMAT_ASTC_4x4_HDR;
			r_format.rd_format = RD::DATA_FORMAT_ASTC_4x4_UNORM_BLOCK;
			r_format.rd_format_srgb = RD::DATA_FORMAT_ASTC_4x4_SRGB_BLOCK;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;

		} break; // astc 4x4
		case RD::DATA_FORMAT_ASTC_8x8_UNORM_BLOCK: {
			// Q: Do we do as we do below, just create the sRGB variant?
			r_format.image_format = Image::FORMAT_ASTC_8x8;
			r_format.rd_format = RD::DATA_FORMAT_ASTC_8x8_UNORM_BLOCK;
		} break;
		case RD::DATA_FORMAT_ASTC_8x8_SRGB_BLOCK: {
			r_format.image_format = Image::FORMAT_ASTC_8x8_HDR;
			r_format.rd_format = RD::DATA_FORMAT_ASTC_8x8_UNORM_BLOCK;
			r_format.rd_format_srgb = RD::DATA_FORMAT_ASTC_8x8_SRGB_BLOCK;
			r_format.swizzle_r = RD::TEXTURE_SWIZZLE_R;
			r_format.swizzle_g = RD::TEXTURE_SWIZZLE_G;
			r_format.swizzle_b = RD::TEXTURE_SWIZZLE_B;
			r_format.swizzle_a = RD::TEXTURE_SWIZZLE_A;

		} break; // astc 8x8

		default: {
			ERR_FAIL_MSG("Unsupported image format");
		}
	}
}

/* DECAL API */

RID TextureStorage::decal_atlas_get_texture() const {
	return decal_atlas.texture;
}

RID TextureStorage::decal_atlas_get_texture_srgb() const {
	return decal_atlas.texture_srgb;
}

RID TextureStorage::decal_allocate() {
	return decal_owner.allocate_rid();
}

void TextureStorage::decal_initialize(RID p_decal) {
	decal_owner.initialize_rid(p_decal, Decal());
}

void TextureStorage::decal_free(RID p_rid) {
	Decal *decal = decal_owner.get_or_null(p_rid);
	for (int i = 0; i < RS::DECAL_TEXTURE_MAX; i++) {
		if (decal->textures[i].is_valid() && owns_texture(decal->textures[i])) {
			texture_remove_from_decal_atlas(decal->textures[i]);
		}
	}
	decal->dependency.deleted_notify(p_rid);
	decal_owner.free(p_rid);
}

void TextureStorage::decal_set_size(RID p_decal, const Vector3 &p_size) {
	Decal *decal = decal_owner.get_or_null(p_decal);
	ERR_FAIL_NULL(decal);
	decal->size = p_size;
	decal->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

void TextureStorage::decal_set_texture(RID p_decal, RS::DecalTexture p_type, RID p_texture) {
	Decal *decal = decal_owner.get_or_null(p_decal);
	ERR_FAIL_NULL(decal);
	ERR_FAIL_INDEX(p_type, RS::DECAL_TEXTURE_MAX);

	if (decal->textures[p_type] == p_texture) {
		return;
	}

	ERR_FAIL_COND(p_texture.is_valid() && !owns_texture(p_texture));

	if (decal->textures[p_type].is_valid() && owns_texture(decal->textures[p_type])) {
		texture_remove_from_decal_atlas(decal->textures[p_type]);
	}

	decal->textures[p_type] = p_texture;

	if (decal->textures[p_type].is_valid()) {
		texture_add_to_decal_atlas(decal->textures[p_type]);
	}

	decal->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_DECAL);
}

void TextureStorage::decal_set_emission_energy(RID p_decal, float p_energy) {
	Decal *decal = decal_owner.get_or_null(p_decal);
	ERR_FAIL_NULL(decal);
	decal->emission_energy = p_energy;
}

void TextureStorage::decal_set_albedo_mix(RID p_decal, float p_mix) {
	Decal *decal = decal_owner.get_or_null(p_decal);
	ERR_FAIL_NULL(decal);
	decal->albedo_mix = p_mix;
}

void TextureStorage::decal_set_modulate(RID p_decal, const Color &p_modulate) {
	Decal *decal = decal_owner.get_or_null(p_decal);
	ERR_FAIL_NULL(decal);
	decal->modulate = p_modulate;
}

void TextureStorage::decal_set_cull_mask(RID p_decal, uint32_t p_layers) {
	Decal *decal = decal_owner.get_or_null(p_decal);
	ERR_FAIL_NULL(decal);
	decal->cull_mask = p_layers;
	decal->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_DECAL);
}

void TextureStorage::decal_set_distance_fade(RID p_decal, bool p_enabled, float p_begin, float p_length) {
	Decal *decal = decal_owner.get_or_null(p_decal);
	ERR_FAIL_NULL(decal);
	decal->distance_fade = p_enabled;
	decal->distance_fade_begin = p_begin;
	decal->distance_fade_length = p_length;
}

void TextureStorage::decal_set_fade(RID p_decal, float p_above, float p_below) {
	Decal *decal = decal_owner.get_or_null(p_decal);
	ERR_FAIL_NULL(decal);
	decal->upper_fade = p_above;
	decal->lower_fade = p_below;
}

void TextureStorage::decal_set_normal_fade(RID p_decal, float p_fade) {
	Decal *decal = decal_owner.get_or_null(p_decal);
	ERR_FAIL_NULL(decal);
	decal->normal_fade = p_fade;
}

void TextureStorage::decal_atlas_mark_dirty_on_texture(RID p_texture) {
	if (decal_atlas.textures.has(p_texture)) {
		//belongs to decal atlas..

		decal_atlas.dirty = true; //mark it dirty since it was most likely modified
	}
}

void TextureStorage::decal_atlas_remove_texture(RID p_texture) {
	if (decal_atlas.textures.has(p_texture)) {
		decal_atlas.textures.erase(p_texture);
		//there is not much a point of making it dirty, just let it be.
	}
}

AABB TextureStorage::decal_get_aabb(RID p_decal) const {
	Decal *decal = decal_owner.get_or_null(p_decal);
	ERR_FAIL_NULL_V(decal, AABB());

	return AABB(-decal->size / 2, decal->size);
}

uint32_t TextureStorage::decal_get_cull_mask(RID p_decal) const {
	Decal *decal = decal_owner.get_or_null(p_decal);
	ERR_FAIL_NULL_V(decal, 0);

	return decal->cull_mask;
}

Dependency *TextureStorage::decal_get_dependency(RID p_decal) {
	Decal *decal = decal_owner.get_or_null(p_decal);
	ERR_FAIL_NULL_V(decal, nullptr);

	return &decal->dependency;
}

void TextureStorage::update_decal_atlas() {
	CopyEffects *copy_effects = CopyEffects::get_singleton();
	ERR_FAIL_NULL(copy_effects);

	if (!decal_atlas.dirty) {
		return; //nothing to do
	}

	decal_atlas.dirty = false;

	if (decal_atlas.texture.is_valid()) {
		RD::get_singleton()->free(decal_atlas.texture);
		decal_atlas.texture = RID();
		decal_atlas.texture_srgb = RID();
		decal_atlas.texture_mipmaps.clear();
	}

	int border = 1 << decal_atlas.mipmaps;

	if (decal_atlas.textures.size()) {
		//generate atlas
		Vector<DecalAtlas::SortItem> itemsv;
		itemsv.resize(decal_atlas.textures.size());
		uint32_t base_size = 8;

		int idx = 0;

		for (const KeyValue<RID, DecalAtlas::Texture> &E : decal_atlas.textures) {
			DecalAtlas::SortItem &si = itemsv.write[idx];

			Texture *src_tex = get_texture(E.key);

			si.size.width = (src_tex->width / border) + 1;
			si.size.height = (src_tex->height / border) + 1;
			si.pixel_size = Size2i(src_tex->width, src_tex->height);

			if (base_size < (uint32_t)si.size.width) {
				base_size = nearest_power_of_2_templated(si.size.width);
			}

			si.texture = E.key;
			idx++;
		}

		//sort items by size
		itemsv.sort();

		//attempt to create atlas
		int item_count = itemsv.size();
		DecalAtlas::SortItem *items = itemsv.ptrw();

		int atlas_height = 0;

		while (true) {
			Vector<int> v_offsetsv;
			v_offsetsv.resize(base_size);

			int *v_offsets = v_offsetsv.ptrw();
			memset(v_offsets, 0, sizeof(int) * base_size);

			int max_height = 0;

			for (int i = 0; i < item_count; i++) {
				//best fit
				DecalAtlas::SortItem &si = items[i];
				int best_idx = -1;
				int best_height = 0x7FFFFFFF;
				for (uint32_t j = 0; j <= base_size - si.size.width; j++) {
					int height = 0;
					for (int k = 0; k < si.size.width; k++) {
						int h = v_offsets[k + j];
						if (h > height) {
							height = h;
							if (height > best_height) {
								break; //already bad
							}
						}
					}

					if (height < best_height) {
						best_height = height;
						best_idx = j;
					}
				}

				//update
				for (int k = 0; k < si.size.width; k++) {
					v_offsets[k + best_idx] = best_height + si.size.height;
				}

				si.pos.x = best_idx;
				si.pos.y = best_height;

				if (si.pos.y + si.size.height > max_height) {
					max_height = si.pos.y + si.size.height;
				}
			}

			if ((uint32_t)max_height <= base_size * 2) {
				atlas_height = max_height;
				break; //good ratio, break;
			}

			base_size *= 2;
		}

		decal_atlas.size.width = base_size * border;
		decal_atlas.size.height = nearest_power_of_2_templated(atlas_height * border);

		for (int i = 0; i < item_count; i++) {
			DecalAtlas::Texture *t = decal_atlas.textures.getptr(items[i].texture);
			t->uv_rect.position = items[i].pos * border + Vector2i(border / 2, border / 2);
			t->uv_rect.size = items[i].pixel_size;

			t->uv_rect.position /= Size2(decal_atlas.size);
			t->uv_rect.size /= Size2(decal_atlas.size);
		}
	} else {
		//use border as size, so it at least has enough mipmaps
		decal_atlas.size.width = border;
		decal_atlas.size.height = border;
	}

	//blit textures

	RD::TextureFormat tformat;
	tformat.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
	tformat.width = decal_atlas.size.width;
	tformat.height = decal_atlas.size.height;
	tformat.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT;
	tformat.texture_type = RD::TEXTURE_TYPE_2D;
	tformat.mipmaps = decal_atlas.mipmaps;
	tformat.shareable_formats.push_back(RD::DATA_FORMAT_R8G8B8A8_UNORM);
	tformat.shareable_formats.push_back(RD::DATA_FORMAT_R8G8B8A8_SRGB);

	decal_atlas.texture = RD::get_singleton()->texture_create(tformat, RD::TextureView());
	RD::get_singleton()->texture_clear(decal_atlas.texture, Color(0, 0, 0, 0), 0, decal_atlas.mipmaps, 0, 1);

	{
		//create the framebuffer

		Size2i s = decal_atlas.size;

		for (int i = 0; i < decal_atlas.mipmaps; i++) {
			DecalAtlas::MipMap mm;
			mm.texture = RD::get_singleton()->texture_create_shared_from_slice(RD::TextureView(), decal_atlas.texture, 0, i);
			Vector<RID> fb;
			fb.push_back(mm.texture);
			mm.fb = RD::get_singleton()->framebuffer_create(fb);
			mm.size = s;
			decal_atlas.texture_mipmaps.push_back(mm);

			s = Vector2i(s.width >> 1, s.height >> 1).maxi(1);
		}
		{
			//create the SRGB variant
			RD::TextureView rd_view;
			rd_view.format_override = RD::DATA_FORMAT_R8G8B8A8_SRGB;
			decal_atlas.texture_srgb = RD::get_singleton()->texture_create_shared(rd_view, decal_atlas.texture);
		}
	}

	RID prev_texture;
	for (int i = 0; i < decal_atlas.texture_mipmaps.size(); i++) {
		const DecalAtlas::MipMap &mm = decal_atlas.texture_mipmaps[i];

		Color clear_color(0, 0, 0, 0);

		if (decal_atlas.textures.size()) {
			if (i == 0) {
				Vector<Color> cc;
				cc.push_back(clear_color);

				RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(mm.fb, RD::DRAW_CLEAR_ALL, cc);

				for (const KeyValue<RID, DecalAtlas::Texture> &E : decal_atlas.textures) {
					DecalAtlas::Texture *t = decal_atlas.textures.getptr(E.key);
					Texture *src_tex = get_texture(E.key);
					copy_effects->copy_to_atlas_fb(src_tex->rd_texture, mm.fb, t->uv_rect, draw_list, false, t->panorama_to_dp_users > 0);
				}

				RD::get_singleton()->draw_list_end();

				prev_texture = mm.texture;
			} else {
				copy_effects->copy_to_fb_rect(prev_texture, mm.fb, Rect2i(Point2i(), mm.size));
				prev_texture = mm.texture;
			}
		} else {
			RD::get_singleton()->texture_clear(mm.texture, clear_color, 0, 1, 0, 1);
		}
	}
}

void TextureStorage::texture_add_to_decal_atlas(RID p_texture, bool p_panorama_to_dp) {
	if (!decal_atlas.textures.has(p_texture)) {
		DecalAtlas::Texture t;
		t.users = 1;
		t.panorama_to_dp_users = p_panorama_to_dp ? 1 : 0;
		decal_atlas.textures[p_texture] = t;
		decal_atlas.dirty = true;
	} else {
		DecalAtlas::Texture *t = decal_atlas.textures.getptr(p_texture);
		t->users++;
		if (p_panorama_to_dp) {
			t->panorama_to_dp_users++;
		}
	}
}

void TextureStorage::texture_remove_from_decal_atlas(RID p_texture, bool p_panorama_to_dp) {
	DecalAtlas::Texture *t = decal_atlas.textures.getptr(p_texture);
	ERR_FAIL_NULL(t);
	t->users--;
	if (p_panorama_to_dp) {
		ERR_FAIL_COND(t->panorama_to_dp_users == 0);
		t->panorama_to_dp_users--;
	}
	if (t->users == 0) {
		decal_atlas.textures.erase(p_texture);
		//do not mark it dirty, there is no need to since it remains working
	}
}

/* DECAL INSTANCE API */

RID TextureStorage::decal_instance_create(RID p_decal) {
	DecalInstance di;
	di.decal = p_decal;
	di.forward_id = ForwardIDStorage::get_singleton()->allocate_forward_id(FORWARD_ID_TYPE_DECAL);
	return decal_instance_owner.make_rid(di);
}

void TextureStorage::decal_instance_free(RID p_decal_instance) {
	DecalInstance *di = decal_instance_owner.get_or_null(p_decal_instance);
	ForwardIDStorage::get_singleton()->free_forward_id(FORWARD_ID_TYPE_DECAL, di->forward_id);
	decal_instance_owner.free(p_decal_instance);
}

void TextureStorage::decal_instance_set_transform(RID p_decal_instance, const Transform3D &p_transform) {
	DecalInstance *di = decal_instance_owner.get_or_null(p_decal_instance);
	ERR_FAIL_NULL(di);
	di->transform = p_transform;
}

void TextureStorage::decal_instance_set_sorting_offset(RID p_decal_instance, float p_sorting_offset) {
	DecalInstance *di = decal_instance_owner.get_or_null(p_decal_instance);
	ERR_FAIL_NULL(di);
	di->sorting_offset = p_sorting_offset;
}

/* DECAL DATA API */

void TextureStorage::free_decal_data() {
	if (decal_buffer.is_valid()) {
		RD::get_singleton()->free(decal_buffer);
		decal_buffer = RID();
	}

	if (decals != nullptr) {
		memdelete_arr(decals);
		decals = nullptr;
	}

	if (decal_sort != nullptr) {
		memdelete_arr(decal_sort);
		decal_sort = nullptr;
	}
}

void TextureStorage::set_max_decals(const uint32_t p_max_decals) {
	max_decals = p_max_decals;
	uint32_t decal_buffer_size = max_decals * sizeof(DecalData);
	decals = memnew_arr(DecalData, max_decals);
	decal_sort = memnew_arr(DecalInstanceSort, max_decals);
	decal_buffer = RD::get_singleton()->storage_buffer_create(decal_buffer_size);
}

void TextureStorage::update_decal_buffer(const PagedArray<RID> &p_decals, const Transform3D &p_camera_xform) {
	ForwardIDStorage *forward_id_storage = ForwardIDStorage::get_singleton();

	Transform3D uv_xform;
	uv_xform.basis.scale(Vector3(2.0, 1.0, 2.0));
	uv_xform.origin = Vector3(-1.0, 0.0, -1.0);

	uint32_t decals_size = p_decals.size();

	decal_count = 0;

	for (uint32_t i = 0; i < decals_size; i++) {
		if (decal_count == max_decals) {
			break;
		}

		DecalInstance *decal_instance = decal_instance_owner.get_or_null(p_decals[i]);
		if (!decal_instance) {
			continue;
		}
		Decal *decal = decal_owner.get_or_null(decal_instance->decal);

		Transform3D xform = decal_instance->transform;

		real_t distance = p_camera_xform.origin.distance_to(xform.origin);

		if (decal->distance_fade) {
			float fade_begin = decal->distance_fade_begin;
			float fade_length = decal->distance_fade_length;

			if (distance > fade_begin) {
				if (distance > fade_begin + fade_length) {
					continue; // do not use this decal, its invisible
				}
			}
		}

		decal_sort[decal_count].decal_instance = decal_instance;
		decal_sort[decal_count].decal = decal;
		decal_sort[decal_count].depth = distance - decal_instance->sorting_offset;
		decal_count++;
	}

	if (decal_count > 0) {
		SortArray<DecalInstanceSort> sort_array;
		sort_array.sort(decal_sort, decal_count);
	}

	bool using_forward_ids = forward_id_storage->uses_forward_ids();
	for (uint32_t i = 0; i < decal_count; i++) {
		DecalInstance *decal_instance = decal_sort[i].decal_instance;
		Decal *decal = decal_sort[i].decal;

		if (using_forward_ids) {
			forward_id_storage->map_forward_id(FORWARD_ID_TYPE_DECAL, decal_instance->forward_id, i, RSG::rasterizer->get_frame_number());
		}

		decal_instance->cull_mask = decal->cull_mask;

		float fade = 1.0;

		if (decal->distance_fade) {
			const real_t distance = decal_sort[i].depth + decal_instance->sorting_offset;
			const float fade_begin = decal->distance_fade_begin;
			const float fade_length = decal->distance_fade_length;

			if (distance > fade_begin) {
				// Use `smoothstep()` to make opacity changes more gradual and less noticeable to the player.
				fade = Math::smoothstep(0.0f, 1.0f, 1.0f - float(distance - fade_begin) / fade_length);
			}
		}

		DecalData &dd = decals[i];

		Vector3 decal_extents = decal->size / 2;

		Transform3D scale_xform;
		scale_xform.basis.scale(decal_extents);

		Transform3D xform = decal_instance->transform;

		Transform3D camera_inverse_xform = p_camera_xform.affine_inverse();

		Transform3D to_decal_xform = (camera_inverse_xform * xform * scale_xform * uv_xform).affine_inverse();
		MaterialStorage::store_transform(to_decal_xform, dd.xform);

		Vector3 normal = xform.basis.get_column(Vector3::AXIS_Y).normalized();
		normal = camera_inverse_xform.basis.xform(normal); //camera is normalized, so fine

		dd.normal[0] = normal.x;
		dd.normal[1] = normal.y;
		dd.normal[2] = normal.z;
		dd.normal_fade = decal->normal_fade;

		RID albedo_tex = decal->textures[RS::DECAL_TEXTURE_ALBEDO];
		RID emission_tex = decal->textures[RS::DECAL_TEXTURE_EMISSION];
		if (albedo_tex.is_valid()) {
			Rect2 rect = decal_atlas_get_texture_rect(albedo_tex);
			dd.albedo_rect[0] = rect.position.x;
			dd.albedo_rect[1] = rect.position.y;
			dd.albedo_rect[2] = rect.size.x;
			dd.albedo_rect[3] = rect.size.y;
		} else {
			if (!emission_tex.is_valid()) {
				continue; //no albedo, no emission, no decal.
			}
			dd.albedo_rect[0] = 0;
			dd.albedo_rect[1] = 0;
			dd.albedo_rect[2] = 0;
			dd.albedo_rect[3] = 0;
		}

		RID normal_tex = decal->textures[RS::DECAL_TEXTURE_NORMAL];

		if (normal_tex.is_valid()) {
			Rect2 rect = decal_atlas_get_texture_rect(normal_tex);
			dd.normal_rect[0] = rect.position.x;
			dd.normal_rect[1] = rect.position.y;
			dd.normal_rect[2] = rect.size.x;
			dd.normal_rect[3] = rect.size.y;

			Basis normal_xform = camera_inverse_xform.basis * xform.basis.orthonormalized();
			MaterialStorage::store_basis_3x4(normal_xform, dd.normal_xform);
		} else {
			dd.normal_rect[0] = 0;
			dd.normal_rect[1] = 0;
			dd.normal_rect[2] = 0;
			dd.normal_rect[3] = 0;
		}

		RID orm_tex = decal->textures[RS::DECAL_TEXTURE_ORM];
		if (orm_tex.is_valid()) {
			Rect2 rect = decal_atlas_get_texture_rect(orm_tex);
			dd.orm_rect[0] = rect.position.x;
			dd.orm_rect[1] = rect.position.y;
			dd.orm_rect[2] = rect.size.x;
			dd.orm_rect[3] = rect.size.y;
		} else {
			dd.orm_rect[0] = 0;
			dd.orm_rect[1] = 0;
			dd.orm_rect[2] = 0;
			dd.orm_rect[3] = 0;
		}

		if (emission_tex.is_valid()) {
			Rect2 rect = decal_atlas_get_texture_rect(emission_tex);
			dd.emission_rect[0] = rect.position.x;
			dd.emission_rect[1] = rect.position.y;
			dd.emission_rect[2] = rect.size.x;
			dd.emission_rect[3] = rect.size.y;
		} else {
			dd.emission_rect[0] = 0;
			dd.emission_rect[1] = 0;
			dd.emission_rect[2] = 0;
			dd.emission_rect[3] = 0;
		}

		Color modulate = decal->modulate.srgb_to_linear();
		dd.modulate[0] = modulate.r;
		dd.modulate[1] = modulate.g;
		dd.modulate[2] = modulate.b;
		dd.modulate[3] = modulate.a * fade;
		dd.emission_energy = decal->emission_energy * fade;
		dd.albedo_mix = decal->albedo_mix;
		dd.mask = decal->cull_mask;
		dd.upper_fade = decal->upper_fade;
		dd.lower_fade = decal->lower_fade;

		// hook for subclass to do further processing.
		RendererSceneRenderRD::get_singleton()->setup_added_decal(xform, decal_extents);
	}

	if (decal_count > 0) {
		RD::get_singleton()->buffer_update(decal_buffer, 0, sizeof(DecalData) * decal_count, decals);
	}
}

/* RENDER TARGET API */

RID TextureStorage::RenderTarget::get_framebuffer() {
	// Note that if we're using an overridden color buffer, we're likely cycling through a texture chain.
	// this is where our framebuffer cache comes in clutch..

	if (msaa != RS::VIEWPORT_MSAA_DISABLED) {
		return FramebufferCacheRD::get_singleton()->get_cache_multiview(view_count, color_multisample, overridden.color.is_valid() ? overridden.color : color);
	} else {
		return FramebufferCacheRD::get_singleton()->get_cache_multiview(view_count, overridden.color.is_valid() ? overridden.color : color);
	}
}

void TextureStorage::_clear_render_target(RenderTarget *rt) {
	// clear overrides, we assume these are freed by the object that created them
	rt->overridden.color = RID();
	rt->overridden.depth = RID();
	rt->overridden.velocity = RID();
	rt->overridden.cached_slices.clear(); // these are automatically freed when their parent textures are freed so just clear

	// free in reverse dependency order
	if (rt->framebuffer_uniform_set.is_valid()) {
		rt->framebuffer_uniform_set = RID(); //chain deleted
	}

	if (rt->color.is_valid()) {
		RD::get_singleton()->free(rt->color);
	}
	rt->color_slices.clear(); // these are automatically freed.

	if (rt->color_multisample.is_valid()) {
		RD::get_singleton()->free(rt->color_multisample);
	}

	if (rt->backbuffer.is_valid()) {
		RD::get_singleton()->free(rt->backbuffer);
		rt->backbuffer = RID();
		rt->backbuffer_mipmaps.clear();
		rt->backbuffer_uniform_set = RID(); //chain deleted
	}

	_render_target_clear_sdf(rt);

	rt->color = RID();
	rt->color_multisample = RID();
	if (rt->texture.is_valid()) {
		Texture *tex = get_texture(rt->texture);
		tex->render_target = nullptr;
	}
}

void TextureStorage::_update_render_target(RenderTarget *rt) {
	if (rt->texture.is_null()) {
		//create a placeholder until updated
		rt->texture = texture_allocate();
		texture_2d_placeholder_initialize(rt->texture);
		Texture *tex = get_texture(rt->texture);
		tex->is_render_target = true;
		tex->path = "Render Target (Internal)";
	}

	_clear_render_target(rt);

	if (rt->size.width == 0 || rt->size.height == 0) {
		return;
	}

	rt->color_format = render_target_get_color_format(rt->use_hdr, false);
	rt->color_format_srgb = render_target_get_color_format(rt->use_hdr, true);

	if (rt->use_hdr) {
		rt->image_format = rt->is_transparent ? Image::FORMAT_RGBAH : Image::FORMAT_RGBH;
	} else {
		rt->image_format = rt->is_transparent ? Image::FORMAT_RGBA8 : Image::FORMAT_RGB8;
	}

	RD::TextureFormat rd_color_attachment_format;
	RD::TextureView rd_view;
	{ //attempt register
		rd_color_attachment_format.format = rt->color_format;
		rd_color_attachment_format.width = rt->size.width;
		rd_color_attachment_format.height = rt->size.height;
		rd_color_attachment_format.depth = 1;
		rd_color_attachment_format.array_layers = rt->view_count; // for stereo we create two (or more) layers, need to see if we can make fallback work like this too if we don't have multiview
		rd_color_attachment_format.mipmaps = 1;
		if (rd_color_attachment_format.array_layers > 1) { // why are we not using rt->texture_type ??
			rd_color_attachment_format.texture_type = RD::TEXTURE_TYPE_2D_ARRAY;
		} else {
			rd_color_attachment_format.texture_type = RD::TEXTURE_TYPE_2D;
		}
		rd_color_attachment_format.samples = RD::TEXTURE_SAMPLES_1;
		rd_color_attachment_format.usage_bits = render_target_get_color_usage_bits(false);
		rd_color_attachment_format.shareable_formats.push_back(rt->color_format);
		rd_color_attachment_format.shareable_formats.push_back(rt->color_format_srgb);
		if (rt->msaa != RS::VIEWPORT_MSAA_DISABLED) {
			rd_color_attachment_format.is_resolve_buffer = true;
		}
	}

	// TODO see if we can lazy create this once we actually use it as we may not need to create this if we have an overridden color buffer...
	rt->color = RD::get_singleton()->texture_create(rd_color_attachment_format, rd_view);
	ERR_FAIL_COND(rt->color.is_null());

	if (rt->msaa != RS::VIEWPORT_MSAA_DISABLED) {
		// Use the texture format of the color attachment for the multisample color attachment.
		RD::TextureFormat rd_color_multisample_format = rd_color_attachment_format;
		const RD::TextureSamples texture_samples[RS::VIEWPORT_MSAA_MAX] = {
			RD::TEXTURE_SAMPLES_1,
			RD::TEXTURE_SAMPLES_2,
			RD::TEXTURE_SAMPLES_4,
			RD::TEXTURE_SAMPLES_8,
		};
		rd_color_multisample_format.samples = texture_samples[rt->msaa];
		rd_color_multisample_format.usage_bits = render_target_get_color_usage_bits(true);
		RD::TextureView rd_view_multisample;
		rd_color_multisample_format.is_resolve_buffer = false;
		rt->color_multisample = RD::get_singleton()->texture_create(rd_color_multisample_format, rd_view_multisample);
		ERR_FAIL_COND(rt->color_multisample.is_null());
	}

	{ //update texture

		Texture *tex = get_texture(rt->texture);

		//free existing textures
		if (RD::get_singleton()->texture_is_valid(tex->rd_texture)) {
			RD::get_singleton()->free(tex->rd_texture);
		}
		if (RD::get_singleton()->texture_is_valid(tex->rd_texture_srgb)) {
			RD::get_singleton()->free(tex->rd_texture_srgb);
		}

		tex->rd_texture = RID();
		tex->rd_texture_srgb = RID();
		tex->render_target = rt;

		//create shared textures to the color buffer,
		//so transparent can be supported
		RD::TextureView view;
		view.format_override = rt->color_format;
		if (!rt->is_transparent) {
			view.swizzle_a = RD::TEXTURE_SWIZZLE_ONE;
		}
		tex->rd_texture = RD::get_singleton()->texture_create_shared(view, rt->color);
		if (rt->color_format_srgb != RD::DATA_FORMAT_MAX) {
			view.format_override = rt->color_format_srgb;
			tex->rd_texture_srgb = RD::get_singleton()->texture_create_shared(view, rt->color);
		}
		tex->rd_view = view;
		tex->width = rt->size.width;
		tex->height = rt->size.height;
		tex->width_2d = rt->size.width;
		tex->height_2d = rt->size.height;
		tex->rd_format = rt->color_format;
		tex->rd_format_srgb = rt->color_format_srgb;
		tex->format = rt->image_format;
		tex->validated_format = rt->use_hdr ? Image::FORMAT_RGBAH : Image::FORMAT_RGBA8;

		Vector<RID> proxies = tex->proxies; //make a copy, since update may change it
		for (int i = 0; i < proxies.size(); i++) {
			texture_proxy_update(proxies[i], rt->texture);
		}
	}
}

void TextureStorage::_create_render_target_backbuffer(RenderTarget *rt) {
	ERR_FAIL_COND(rt->backbuffer.is_valid());

	uint32_t mipmaps_required = Image::get_image_required_mipmaps(rt->size.width, rt->size.height, Image::FORMAT_RGBA8);
	RD::TextureFormat tf;
	tf.format = rt->color_format;
	tf.width = rt->size.width;
	tf.height = rt->size.height;
	tf.texture_type = RD::TEXTURE_TYPE_2D;
	tf.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT;
	tf.mipmaps = mipmaps_required;

	rt->backbuffer = RD::get_singleton()->texture_create(tf, RD::TextureView());
	RD::get_singleton()->set_resource_name(rt->backbuffer, "Render Target Back Buffer");
	rt->backbuffer_mipmap0 = RD::get_singleton()->texture_create_shared_from_slice(RD::TextureView(), rt->backbuffer, 0, 0);
	RD::get_singleton()->set_resource_name(rt->backbuffer_mipmap0, "Back Buffer slice mipmap 0");

	{
		Vector<RID> fb_tex;
		fb_tex.push_back(rt->backbuffer_mipmap0);
		rt->backbuffer_fb = RD::get_singleton()->framebuffer_create(fb_tex);
	}

	if (rt->framebuffer_uniform_set.is_valid() && RD::get_singleton()->uniform_set_is_valid(rt->framebuffer_uniform_set)) {
		//the new one will require the backbuffer.
		RD::get_singleton()->free(rt->framebuffer_uniform_set);
		rt->framebuffer_uniform_set = RID();
	}
	//create mipmaps
	for (uint32_t i = 1; i < mipmaps_required; i++) {
		RID mipmap = RD::get_singleton()->texture_create_shared_from_slice(RD::TextureView(), rt->backbuffer, 0, i);
		RD::get_singleton()->set_resource_name(mipmap, "Back Buffer slice mip: " + itos(i));

		rt->backbuffer_mipmaps.push_back(mipmap);
	}
}

RID TextureStorage::render_target_create() {
	RenderTarget render_target;

	render_target.was_used = false;
	render_target.clear_requested = false;

	_update_render_target(&render_target);
	return render_target_owner.make_rid(render_target);
}

void TextureStorage::render_target_free(RID p_rid) {
	RenderTarget *rt = render_target_owner.get_or_null(p_rid);

	_clear_render_target(rt);

	if (rt->texture.is_valid()) {
		Texture *tex = get_texture(rt->texture);
		tex->is_render_target = false;
		texture_free(rt->texture);
	}

	render_target_owner.free(p_rid);
}

void TextureStorage::render_target_set_position(RID p_render_target, int p_x, int p_y) {
	//unused for this render target
}

Point2i TextureStorage::render_target_get_position(RID p_render_target) const {
	//unused for this render target
	return Point2i();
}

void TextureStorage::render_target_set_size(RID p_render_target, int p_width, int p_height, uint32_t p_view_count) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	if (rt->size.x != p_width || rt->size.y != p_height || rt->view_count != p_view_count) {
		rt->size.x = p_width;
		rt->size.y = p_height;
		rt->view_count = p_view_count;
		_update_render_target(rt);
	}
}

Size2i TextureStorage::render_target_get_size(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, Size2i());

	return rt->size;
}

RID TextureStorage::render_target_get_texture(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());

	return rt->texture;
}

void TextureStorage::render_target_set_override(RID p_render_target, RID p_color_texture, RID p_depth_texture, RID p_velocity_texture, RID p_velocity_depth_texture) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	rt->overridden.color = p_color_texture;
	rt->overridden.depth = p_depth_texture;
	rt->overridden.velocity = p_velocity_texture;
}

RID TextureStorage::render_target_get_override_color(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());

	return rt->overridden.color;
}

RID TextureStorage::render_target_get_override_depth(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());

	return rt->overridden.depth;
}

RID TextureStorage::render_target_get_override_depth_slice(RID p_render_target, const uint32_t p_layer) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());

	if (rt->overridden.depth.is_null()) {
		return RID();
	} else if (rt->view_count == 1) {
		return rt->overridden.depth;
	} else {
		RenderTarget::RTOverridden::SliceKey key(rt->overridden.depth, p_layer);

		if (!rt->overridden.cached_slices.has(key)) {
			rt->overridden.cached_slices[key] = RD::get_singleton()->texture_create_shared_from_slice(RD::TextureView(), rt->overridden.depth, p_layer, 0);
		}

		return rt->overridden.cached_slices[key];
	}
}

RID TextureStorage::render_target_get_override_velocity(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());

	return rt->overridden.velocity;
}

RID TextureStorage::render_target_get_override_velocity_slice(RID p_render_target, const uint32_t p_layer) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());

	if (rt->overridden.velocity.is_null()) {
		return RID();
	} else if (rt->view_count == 1) {
		return rt->overridden.velocity;
	} else {
		RenderTarget::RTOverridden::SliceKey key(rt->overridden.velocity, p_layer);

		if (!rt->overridden.cached_slices.has(key)) {
			rt->overridden.cached_slices[key] = RD::get_singleton()->texture_create_shared_from_slice(RD::TextureView(), rt->overridden.velocity, p_layer, 0);
		}

		return rt->overridden.cached_slices[key];
	}
}

void RendererRD::TextureStorage::render_target_set_render_region(RID p_render_target, const Rect2i &p_render_region) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	rt->render_region = p_render_region;
}

Rect2i RendererRD::TextureStorage::render_target_get_render_region(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, Rect2i());

	return rt->render_region;
}

void TextureStorage::render_target_set_transparent(RID p_render_target, bool p_is_transparent) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	rt->is_transparent = p_is_transparent;
	_update_render_target(rt);
}

bool TextureStorage::render_target_get_transparent(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, false);

	return rt->is_transparent;
}

void TextureStorage::render_target_set_direct_to_screen(RID p_render_target, bool p_value) {
}

bool TextureStorage::render_target_get_direct_to_screen(RID p_render_target) const {
	return false;
}

bool TextureStorage::render_target_was_used(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, false);
	return rt->was_used;
}

void TextureStorage::render_target_set_as_unused(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	rt->was_used = false;
}

void TextureStorage::render_target_set_msaa(RID p_render_target, RS::ViewportMSAA p_msaa) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	if (p_msaa == rt->msaa) {
		return;
	}

	rt->msaa = p_msaa;
	_update_render_target(rt);
}

RS::ViewportMSAA TextureStorage::render_target_get_msaa(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RS::VIEWPORT_MSAA_DISABLED);

	return rt->msaa;
}

void TextureStorage::render_target_set_msaa_needs_resolve(RID p_render_target, bool p_needs_resolve) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	rt->msaa_needs_resolve = p_needs_resolve;
}

bool TextureStorage::render_target_get_msaa_needs_resolve(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, false);

	return rt->msaa_needs_resolve;
}

void TextureStorage::render_target_do_msaa_resolve(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	if (!rt->msaa_needs_resolve) {
		return;
	}
	RD::get_singleton()->draw_list_begin(rt->get_framebuffer());
	RD::get_singleton()->draw_list_end();
	rt->msaa_needs_resolve = false;
}

void TextureStorage::render_target_set_use_hdr(RID p_render_target, bool p_use_hdr) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	if (p_use_hdr == rt->use_hdr) {
		return;
	}

	rt->use_hdr = p_use_hdr;
	_update_render_target(rt);
}

bool TextureStorage::render_target_is_using_hdr(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, false);

	return rt->use_hdr;
}

RID TextureStorage::render_target_get_rd_framebuffer(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());

	return rt->get_framebuffer();
}

RID TextureStorage::render_target_get_rd_texture(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());

	if (rt->overridden.color.is_valid()) {
		return rt->overridden.color;
	} else {
		return rt->color;
	}
}

RID TextureStorage::render_target_get_rd_texture_slice(RID p_render_target, uint32_t p_layer) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());

	if (rt->view_count == 1) {
		return rt->color;
	} else {
		ERR_FAIL_UNSIGNED_INDEX_V(p_layer, rt->view_count, RID());
		if (rt->color_slices.size() == 0) {
			for (uint32_t v = 0; v < rt->view_count; v++) {
				RID slice = RD::get_singleton()->texture_create_shared_from_slice(RD::TextureView(), rt->color, v, 0);
				rt->color_slices.push_back(slice);
			}
		}
		return rt->color_slices[p_layer];
	}
}

RID TextureStorage::render_target_get_rd_texture_msaa(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());

	return rt->color_multisample;
}

RID TextureStorage::render_target_get_rd_backbuffer(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());
	return rt->backbuffer;
}

RID TextureStorage::render_target_get_rd_backbuffer_framebuffer(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());

	if (!rt->backbuffer.is_valid()) {
		_create_render_target_backbuffer(rt);
	}

	return rt->backbuffer_fb;
}

void TextureStorage::render_target_request_clear(RID p_render_target, const Color &p_clear_color) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	rt->clear_requested = true;
	rt->clear_color = p_clear_color;
}

bool TextureStorage::render_target_is_clear_requested(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, false);
	return rt->clear_requested;
}

Color TextureStorage::render_target_get_clear_request_color(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, Color());
	return rt->use_hdr ? rt->clear_color.srgb_to_linear() : rt->clear_color;
}

void TextureStorage::render_target_disable_clear_request(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	rt->clear_requested = false;
}

void TextureStorage::render_target_do_clear_request(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	if (!rt->clear_requested) {
		return;
	}
	Vector<Color> clear_colors;
	clear_colors.push_back(rt->use_hdr ? rt->clear_color.srgb_to_linear() : rt->clear_color);
	RD::get_singleton()->draw_list_begin(rt->get_framebuffer(), RD::DRAW_CLEAR_COLOR_0, clear_colors);
	RD::get_singleton()->draw_list_end();
	rt->clear_requested = false;
	rt->msaa_needs_resolve = false;
}

void TextureStorage::render_target_set_sdf_size_and_scale(RID p_render_target, RS::ViewportSDFOversize p_size, RS::ViewportSDFScale p_scale) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	if (rt->sdf_oversize == p_size && rt->sdf_scale == p_scale) {
		return;
	}

	rt->sdf_oversize = p_size;
	rt->sdf_scale = p_scale;

	_render_target_clear_sdf(rt);
}

Rect2i TextureStorage::_render_target_get_sdf_rect(const RenderTarget *rt) const {
	Size2i margin;
	int scale;
	switch (rt->sdf_oversize) {
		case RS::VIEWPORT_SDF_OVERSIZE_100_PERCENT: {
			scale = 100;
		} break;
		case RS::VIEWPORT_SDF_OVERSIZE_120_PERCENT: {
			scale = 120;
		} break;
		case RS::VIEWPORT_SDF_OVERSIZE_150_PERCENT: {
			scale = 150;
		} break;
		case RS::VIEWPORT_SDF_OVERSIZE_200_PERCENT: {
			scale = 200;
		} break;
		default: {
			ERR_PRINT("Invalid viewport SDF oversize, defaulting to 100%.");
			scale = 100;
		} break;
	}

	margin = (rt->size * scale / 100) - rt->size;

	Rect2i r(Vector2i(), rt->size);
	r.position -= margin;
	r.size += margin * 2;

	return r;
}

Rect2i TextureStorage::render_target_get_sdf_rect(RID p_render_target) const {
	const RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, Rect2i());

	return _render_target_get_sdf_rect(rt);
}

void TextureStorage::render_target_mark_sdf_enabled(RID p_render_target, bool p_enabled) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	rt->sdf_enabled = p_enabled;
}

bool TextureStorage::render_target_is_sdf_enabled(RID p_render_target) const {
	const RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, false);

	return rt->sdf_enabled;
}

RID TextureStorage::render_target_get_sdf_texture(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());
	if (rt->sdf_buffer_read.is_null()) {
		// no texture, create a dummy one for the 2D uniform set
		RD::TextureFormat tformat;
		tformat.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
		tformat.width = 4;
		tformat.height = 4;
		tformat.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT;
		tformat.texture_type = RD::TEXTURE_TYPE_2D;

		Vector<uint8_t> pv;
		pv.resize(16 * 4);
		memset(pv.ptrw(), 0, 16 * 4);
		Vector<Vector<uint8_t>> vpv;

		rt->sdf_buffer_read = RD::get_singleton()->texture_create(tformat, RD::TextureView(), vpv);
	}

	return rt->sdf_buffer_read;
}

void TextureStorage::_render_target_allocate_sdf(RenderTarget *rt) {
	ERR_FAIL_COND(rt->sdf_buffer_write_fb.is_valid());
	if (rt->sdf_buffer_read.is_valid()) {
		RD::get_singleton()->free(rt->sdf_buffer_read);
		rt->sdf_buffer_read = RID();
	}

	Size2i size = _render_target_get_sdf_rect(rt).size;

	RD::TextureFormat tformat;
	tformat.format = RD::DATA_FORMAT_R8_UNORM;
	tformat.width = size.width;
	tformat.height = size.height;
	tformat.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
	tformat.texture_type = RD::TEXTURE_TYPE_2D;

	rt->sdf_buffer_write = RD::get_singleton()->texture_create(tformat, RD::TextureView());

	{
		Vector<RID> write_fb;
		write_fb.push_back(rt->sdf_buffer_write);
		rt->sdf_buffer_write_fb = RD::get_singleton()->framebuffer_create(write_fb);
	}

	int scale;
	switch (rt->sdf_scale) {
		case RS::VIEWPORT_SDF_SCALE_100_PERCENT: {
			scale = 100;
		} break;
		case RS::VIEWPORT_SDF_SCALE_50_PERCENT: {
			scale = 50;
		} break;
		case RS::VIEWPORT_SDF_SCALE_25_PERCENT: {
			scale = 25;
		} break;
		default: {
			ERR_PRINT("Invalid viewport SDF scale, defaulting to 100%.");
			scale = 100;
		} break;
	}

	rt->process_size = size * scale / 100;
	rt->process_size = rt->process_size.maxi(1);

	tformat.format = RD::DATA_FORMAT_R16G16_SINT;
	tformat.width = rt->process_size.width;
	tformat.height = rt->process_size.height;
	tformat.usage_bits = RD::TEXTURE_USAGE_STORAGE_BIT;

	rt->sdf_buffer_process[0] = RD::get_singleton()->texture_create(tformat, RD::TextureView());
	rt->sdf_buffer_process[1] = RD::get_singleton()->texture_create(tformat, RD::TextureView());

	tformat.format = RD::DATA_FORMAT_R16_SNORM;
	tformat.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT;

	rt->sdf_buffer_read = RD::get_singleton()->texture_create(tformat, RD::TextureView());

	{
		Vector<RD::Uniform> uniforms;
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 1;
			u.append_id(rt->sdf_buffer_write);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 2;
			u.append_id(rt->sdf_buffer_read);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 3;
			u.append_id(rt->sdf_buffer_process[0]);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 4;
			u.append_id(rt->sdf_buffer_process[1]);
			uniforms.push_back(u);
		}

		rt->sdf_buffer_process_uniform_sets[0] = RD::get_singleton()->uniform_set_create(uniforms, rt_sdf.shader.version_get_shader(rt_sdf.shader_version, 0), 0);
		RID aux2 = uniforms.write[2].get_id(0);
		RID aux3 = uniforms.write[3].get_id(0);
		uniforms.write[2].set_id(0, aux3);
		uniforms.write[3].set_id(0, aux2);
		rt->sdf_buffer_process_uniform_sets[1] = RD::get_singleton()->uniform_set_create(uniforms, rt_sdf.shader.version_get_shader(rt_sdf.shader_version, 0), 0);
	}
}

void TextureStorage::_render_target_clear_sdf(RenderTarget *rt) {
	if (rt->sdf_buffer_read.is_valid()) {
		RD::get_singleton()->free(rt->sdf_buffer_read);
		rt->sdf_buffer_read = RID();
	}
	if (rt->sdf_buffer_write_fb.is_valid()) {
		RD::get_singleton()->free(rt->sdf_buffer_write);
		RD::get_singleton()->free(rt->sdf_buffer_process[0]);
		RD::get_singleton()->free(rt->sdf_buffer_process[1]);
		rt->sdf_buffer_write = RID();
		rt->sdf_buffer_write_fb = RID();
		rt->sdf_buffer_process[0] = RID();
		rt->sdf_buffer_process[1] = RID();
		rt->sdf_buffer_process_uniform_sets[0] = RID();
		rt->sdf_buffer_process_uniform_sets[1] = RID();
	}
}

RID TextureStorage::render_target_get_sdf_framebuffer(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());

	if (rt->sdf_buffer_write_fb.is_null()) {
		_render_target_allocate_sdf(rt);
	}

	return rt->sdf_buffer_write_fb;
}
void TextureStorage::render_target_sdf_process(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	ERR_FAIL_COND(rt->sdf_buffer_write_fb.is_null());

	RenderTargetSDF::PushConstant push_constant;

	Rect2i r = _render_target_get_sdf_rect(rt);

	push_constant.size[0] = r.size.width;
	push_constant.size[1] = r.size.height;
	push_constant.stride = 0;
	push_constant.shift = 0;
	push_constant.base_size[0] = r.size.width;
	push_constant.base_size[1] = r.size.height;

	bool shrink = false;

	switch (rt->sdf_scale) {
		case RS::VIEWPORT_SDF_SCALE_50_PERCENT: {
			push_constant.size[0] >>= 1;
			push_constant.size[1] >>= 1;
			push_constant.shift = 1;
			shrink = true;
		} break;
		case RS::VIEWPORT_SDF_SCALE_25_PERCENT: {
			push_constant.size[0] >>= 2;
			push_constant.size[1] >>= 2;
			push_constant.shift = 2;
			shrink = true;
		} break;
		default: {
		};
	}

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();

	/* Load */

	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, rt_sdf.pipelines[shrink ? RenderTargetSDF::SHADER_LOAD_SHRINK : RenderTargetSDF::SHADER_LOAD]);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, rt->sdf_buffer_process_uniform_sets[1], 0); //fill [0]
	RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(RenderTargetSDF::PushConstant));

	RD::get_singleton()->compute_list_dispatch_threads(compute_list, push_constant.size[0], push_constant.size[1], 1);

	/* Process */

	int stride = nearest_power_of_2_templated(MAX(push_constant.size[0], push_constant.size[1]) / 2);

	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, rt_sdf.pipelines[RenderTargetSDF::SHADER_PROCESS]);

	RD::get_singleton()->compute_list_add_barrier(compute_list);
	bool swap = false;

	//jumpflood
	while (stride > 0) {
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, rt->sdf_buffer_process_uniform_sets[swap ? 1 : 0], 0);
		push_constant.stride = stride;
		RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(RenderTargetSDF::PushConstant));
		RD::get_singleton()->compute_list_dispatch_threads(compute_list, push_constant.size[0], push_constant.size[1], 1);
		stride /= 2;
		swap = !swap;
		RD::get_singleton()->compute_list_add_barrier(compute_list);
	}

	/* Store */

	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, rt_sdf.pipelines[shrink ? RenderTargetSDF::SHADER_STORE_SHRINK : RenderTargetSDF::SHADER_STORE]);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, rt->sdf_buffer_process_uniform_sets[swap ? 1 : 0], 0);
	RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(RenderTargetSDF::PushConstant));
	RD::get_singleton()->compute_list_dispatch_threads(compute_list, push_constant.size[0], push_constant.size[1], 1);

	RD::get_singleton()->compute_list_end();
}

void TextureStorage::render_target_copy_to_back_buffer(RID p_render_target, const Rect2i &p_region, bool p_gen_mipmaps) {
	CopyEffects *copy_effects = CopyEffects::get_singleton();
	ERR_FAIL_NULL(copy_effects);

	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	if (!rt->backbuffer.is_valid()) {
		_create_render_target_backbuffer(rt);
	}

	Rect2i region;
	if (p_region == Rect2i()) {
		region.size = rt->size;
	} else {
		region = Rect2i(Size2i(), rt->size).intersection(p_region);
		if (region.size == Size2i()) {
			return; //nothing to do
		}
	}

	// TODO figure out stereo support here

	if (RendererSceneRenderRD::get_singleton()->_render_buffers_can_be_storage()) {
		copy_effects->copy_to_rect(rt->color, rt->backbuffer_mipmap0, region, false, false, false, !rt->use_hdr, true);
	} else {
		Rect2 src_rect = Rect2(region);
		src_rect.position /= Size2(rt->size);
		src_rect.size /= Size2(rt->size);
		copy_effects->copy_to_fb_rect(rt->color, rt->backbuffer_fb, region, false, false, false, false, RID(), false, true, false, false, src_rect);
	}

	if (!p_gen_mipmaps) {
		return;
	}
	RD::get_singleton()->draw_command_begin_label("Gaussian Blur Mipmaps");
	//then mipmap blur
	RID prev_texture = rt->color; //use color, not backbuffer, as bb has mipmaps.

	Size2i texture_size = rt->size;

	for (int i = 0; i < rt->backbuffer_mipmaps.size(); i++) {
		region.position.x >>= 1;
		region.position.y >>= 1;
		region.size = Size2i(region.size.x >> 1, region.size.y >> 1).maxi(1);
		texture_size = Size2i(texture_size.x >> 1, texture_size.y >> 1).maxi(1);

		RID mipmap = rt->backbuffer_mipmaps[i];
		if (RendererSceneRenderRD::get_singleton()->_render_buffers_can_be_storage()) {
			copy_effects->gaussian_blur(prev_texture, mipmap, region, texture_size, !rt->use_hdr);
		} else {
			copy_effects->gaussian_blur_raster(prev_texture, mipmap, region, texture_size);
		}
		prev_texture = mipmap;
	}
	RD::get_singleton()->draw_command_end_label();
}

void TextureStorage::render_target_clear_back_buffer(RID p_render_target, const Rect2i &p_region, const Color &p_color) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	CopyEffects *copy_effects = CopyEffects::get_singleton();
	ERR_FAIL_NULL(copy_effects);

	if (!rt->backbuffer.is_valid()) {
		_create_render_target_backbuffer(rt);
	}

	Rect2i region;
	if (p_region == Rect2i()) {
		region.size = rt->size;
	} else {
		region = Rect2i(Size2i(), rt->size).intersection(p_region);
		if (region.size == Size2i()) {
			return; //nothing to do
		}
	}

	// Single texture copy for backbuffer.
	if (RendererSceneRenderRD::get_singleton()->_render_buffers_can_be_storage()) {
		copy_effects->set_color(rt->backbuffer_mipmap0, p_color, region, !rt->use_hdr);
	} else {
		copy_effects->set_color_raster(rt->backbuffer_mipmap0, p_color, region);
	}
}

void TextureStorage::render_target_gen_back_buffer_mipmaps(RID p_render_target, const Rect2i &p_region) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	CopyEffects *copy_effects = CopyEffects::get_singleton();
	ERR_FAIL_NULL(copy_effects);

	if (!rt->backbuffer.is_valid()) {
		_create_render_target_backbuffer(rt);
	}

	Rect2i region;
	if (p_region == Rect2i()) {
		region.size = rt->size;
	} else {
		region = Rect2i(Size2i(), rt->size).intersection(p_region);
		if (region.size == Size2i()) {
			return; //nothing to do
		}
	}
	RD::get_singleton()->draw_command_begin_label("Gaussian Blur Mipmaps2");
	//then mipmap blur
	RID prev_texture = rt->backbuffer_mipmap0;
	Size2i texture_size = rt->size;

	for (int i = 0; i < rt->backbuffer_mipmaps.size(); i++) {
		region.position.x >>= 1;
		region.position.y >>= 1;
		region.size = Size2i(region.size.x >> 1, region.size.y >> 1).maxi(1);
		texture_size = Size2i(texture_size.x >> 1, texture_size.y >> 1).maxi(1);

		RID mipmap = rt->backbuffer_mipmaps[i];

		if (RendererSceneRenderRD::get_singleton()->_render_buffers_can_be_storage()) {
			copy_effects->gaussian_blur(prev_texture, mipmap, region, texture_size, !rt->use_hdr);
		} else {
			copy_effects->gaussian_blur_raster(prev_texture, mipmap, region, texture_size);
		}
		prev_texture = mipmap;
	}
	RD::get_singleton()->draw_command_end_label();
}

RID TextureStorage::render_target_get_framebuffer_uniform_set(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());
	return rt->framebuffer_uniform_set;
}
RID TextureStorage::render_target_get_backbuffer_uniform_set(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());
	return rt->backbuffer_uniform_set;
}

void TextureStorage::render_target_set_framebuffer_uniform_set(RID p_render_target, RID p_uniform_set) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	rt->framebuffer_uniform_set = p_uniform_set;
}

void TextureStorage::render_target_set_backbuffer_uniform_set(RID p_render_target, RID p_uniform_set) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	rt->backbuffer_uniform_set = p_uniform_set;
}

void TextureStorage::render_target_set_vrs_mode(RID p_render_target, RS::ViewportVRSMode p_mode) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	rt->vrs_mode = p_mode;
}

RS::ViewportVRSMode TextureStorage::render_target_get_vrs_mode(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RS::VIEWPORT_VRS_DISABLED);

	return rt->vrs_mode;
}

void TextureStorage::render_target_set_vrs_update_mode(RID p_render_target, RS::ViewportVRSUpdateMode p_mode) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	rt->vrs_update_mode = p_mode;
}

RS::ViewportVRSUpdateMode TextureStorage::render_target_get_vrs_update_mode(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RS::VIEWPORT_VRS_UPDATE_DISABLED);

	return rt->vrs_update_mode;
}

void TextureStorage::render_target_set_vrs_texture(RID p_render_target, RID p_texture) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	rt->vrs_texture = p_texture;
}

RID TextureStorage::render_target_get_vrs_texture(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());

	return rt->vrs_texture;
}

RD::DataFormat TextureStorage::render_target_get_color_format(bool p_use_hdr, bool p_srgb) {
	if (p_use_hdr) {
		return RendererSceneRenderRD::get_singleton()->_render_buffers_get_color_format();
	} else {
		return p_srgb ? RD::DATA_FORMAT_R8G8B8A8_SRGB : RD::DATA_FORMAT_R8G8B8A8_UNORM;
	}
}

uint32_t TextureStorage::render_target_get_color_usage_bits(bool p_msaa) {
	if (p_msaa) {
		return RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
	} else {
		// FIXME: Storage bit should only be requested when FSR is required.
		return RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RD::TEXTURE_USAGE_STORAGE_BIT;
	}
}
