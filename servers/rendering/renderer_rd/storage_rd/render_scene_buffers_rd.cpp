/**************************************************************************/
/*  render_scene_buffers_rd.cpp                                           */
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

#include "render_scene_buffers_rd.h"
#include "render_scene_buffers_rd.compat.inc"

#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"

RenderSceneBuffersRD::RenderSceneBuffersRD() {
}

RenderSceneBuffersRD::~RenderSceneBuffersRD() {
	cleanup();

	data_buffers.clear();

	RendererRD::MaterialStorage::get_singleton()->samplers_rd_free(samplers);
}

void RenderSceneBuffersRD::_bind_methods() {
	ClassDB::bind_method(D_METHOD("has_texture", "context", "name"), &RenderSceneBuffersRD::has_texture);
	ClassDB::bind_method(D_METHOD("create_texture", "context", "name", "data_format", "usage_bits", "texture_samples", "size", "layers", "mipmaps", "unique", "discardable"), &RenderSceneBuffersRD::create_texture);
	ClassDB::bind_method(D_METHOD("create_texture_from_format", "context", "name", "format", "view", "unique"), &RenderSceneBuffersRD::_create_texture_from_format);
	ClassDB::bind_method(D_METHOD("create_texture_view", "context", "name", "view_name", "view"), &RenderSceneBuffersRD::_create_texture_view);
	ClassDB::bind_method(D_METHOD("get_texture", "context", "name"), &RenderSceneBuffersRD::get_texture);
	ClassDB::bind_method(D_METHOD("get_texture_format", "context", "name"), &RenderSceneBuffersRD::_get_texture_format);
	ClassDB::bind_method(D_METHOD("get_texture_slice", "context", "name", "layer", "mipmap", "layers", "mipmaps"), &RenderSceneBuffersRD::get_texture_slice);
	ClassDB::bind_method(D_METHOD("get_texture_slice_view", "context", "name", "layer", "mipmap", "layers", "mipmaps", "view"), &RenderSceneBuffersRD::_get_texture_slice_view);
	ClassDB::bind_method(D_METHOD("get_texture_slice_size", "context", "name", "mipmap"), &RenderSceneBuffersRD::get_texture_slice_size);
	ClassDB::bind_method(D_METHOD("clear_context", "context"), &RenderSceneBuffersRD::clear_context);

	// Access to some core buffers so users don't need to know their names.
	ClassDB::bind_method(D_METHOD("get_color_texture", "msaa"), &RenderSceneBuffersRD::_get_color_texture, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_color_layer", "layer", "msaa"), &RenderSceneBuffersRD::_get_color_layer, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_depth_texture", "msaa"), &RenderSceneBuffersRD::_get_depth_texture, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_depth_layer", "layer", "msaa"), &RenderSceneBuffersRD::_get_depth_layer, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_velocity_texture", "msaa"), &RenderSceneBuffersRD::_get_velocity_texture, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_velocity_layer", "layer", "msaa"), &RenderSceneBuffersRD::_get_velocity_layer, DEFVAL(false));

	// Expose a few properties we're likely to use externally
	ClassDB::bind_method(D_METHOD("get_render_target"), &RenderSceneBuffersRD::get_render_target);
	ClassDB::bind_method(D_METHOD("get_view_count"), &RenderSceneBuffersRD::get_view_count);
	ClassDB::bind_method(D_METHOD("get_internal_size"), &RenderSceneBuffersRD::get_internal_size);
	ClassDB::bind_method(D_METHOD("get_target_size"), &RenderSceneBuffersRD::get_target_size);
	ClassDB::bind_method(D_METHOD("get_scaling_3d_mode"), &RenderSceneBuffersRD::get_scaling_3d_mode);
	ClassDB::bind_method(D_METHOD("get_fsr_sharpness"), &RenderSceneBuffersRD::get_fsr_sharpness);
	ClassDB::bind_method(D_METHOD("get_msaa_3d"), &RenderSceneBuffersRD::get_msaa_3d);
	ClassDB::bind_method(D_METHOD("get_texture_samples"), &RenderSceneBuffersRD::get_texture_samples);
	ClassDB::bind_method(D_METHOD("get_screen_space_aa"), &RenderSceneBuffersRD::get_screen_space_aa);
	ClassDB::bind_method(D_METHOD("get_use_taa"), &RenderSceneBuffersRD::get_use_taa);
	ClassDB::bind_method(D_METHOD("get_use_debanding"), &RenderSceneBuffersRD::get_use_debanding);
}

void RenderSceneBuffersRD::update_sizes(NamedTexture &p_named_texture) {
	ERR_FAIL_COND(p_named_texture.texture.is_null());

	p_named_texture.sizes.resize(p_named_texture.format.mipmaps);

	Size2i mipmap_size = Size2i(p_named_texture.format.width, p_named_texture.format.height);
	for (uint32_t mipmap = 0; mipmap < p_named_texture.format.mipmaps; mipmap++) {
		p_named_texture.sizes.ptrw()[mipmap] = mipmap_size;

		mipmap_size = Size2i(mipmap_size.width >> 1, mipmap_size.height >> 1).maxi(1);
	}
}

void RenderSceneBuffersRD::free_named_texture(NamedTexture &p_named_texture) {
	if (p_named_texture.texture.is_valid()) {
		RD::get_singleton()->free(p_named_texture.texture);
	}
	p_named_texture.texture = RID();
	p_named_texture.slices.clear(); // slices should be freed automatically as dependents...
}

void RenderSceneBuffersRD::update_samplers() {
	float computed_mipmap_bias = texture_mipmap_bias;

	if (use_taa || (scaling_3d_mode == RS::VIEWPORT_SCALING_3D_MODE_FSR2)) {
		// Use negative mipmap LOD bias when TAA or FSR2 is enabled to compensate for loss of sharpness.
		// This restores sharpness in still images to be roughly at the same level as without TAA,
		// but moving scenes will still be blurrier.
		computed_mipmap_bias -= 0.5;
	}

	if (screen_space_aa == RS::VIEWPORT_SCREEN_SPACE_AA_FXAA) {
		// Use negative mipmap LOD bias when FXAA is enabled to compensate for loss of sharpness.
		// If both TAA and FXAA are enabled, combine their negative LOD biases together.
		computed_mipmap_bias -= 0.25;
	}

	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();
	material_storage->samplers_rd_free(samplers);
	samplers = material_storage->samplers_rd_allocate(computed_mipmap_bias, anisotropic_filtering_level);
}

void RenderSceneBuffersRD::cleanup() {
	// Free our data buffers (but don't destroy them)
	for (KeyValue<StringName, Ref<RenderBufferCustomDataRD>> &E : data_buffers) {
		E.value->free_data();
	}

	// Clear our named textures
	for (KeyValue<NTKey, NamedTexture> &E : named_textures) {
		free_named_texture(E.value);
	}
	named_textures.clear();

	// Clear weight_buffer / blur textures.
	for (WeightBuffers &weight_buffer : weight_buffers) {
		if (weight_buffer.weight.is_valid()) {
			RD::get_singleton()->free(weight_buffer.weight);
			weight_buffer.weight = RID();
		}
	}
}

void RenderSceneBuffersRD::configure(const RenderSceneBuffersConfiguration *p_config) {
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();

	render_target = p_config->get_render_target();
	target_size = p_config->get_target_size();
	internal_size = p_config->get_internal_size();
	view_count = p_config->get_view_count();

	scaling_3d_mode = p_config->get_scaling_3d_mode();
	msaa_3d = p_config->get_msaa_3d();
	screen_space_aa = p_config->get_screen_space_aa();

	fsr_sharpness = p_config->get_fsr_sharpness();
	texture_mipmap_bias = p_config->get_texture_mipmap_bias();
	anisotropic_filtering_level = p_config->get_anisotropic_filtering_level();
	use_taa = p_config->get_use_taa();
	use_debanding = p_config->get_use_debanding();

	ERR_FAIL_COND_MSG(view_count == 0, "Must have at least 1 view");

	update_samplers();

	// cleanout any old buffers we had.
	cleanup();

	// Create our color buffer.
	const bool resolve_target = msaa_3d != RS::VIEWPORT_MSAA_DISABLED;
	create_texture(RB_SCOPE_BUFFERS, RB_TEX_COLOR, base_data_format, get_color_usage_bits(resolve_target, false, can_be_storage));

	// TODO: Detect when it is safe to use RD::TEXTURE_USAGE_TRANSIENT_BIT for RB_TEX_DEPTH, RB_TEX_COLOR_MSAA and/or RB_TEX_DEPTH_MSAA.
	// (it means we cannot sample from it, we cannot copy from/to it) to save VRAM (and maybe performance too).

	// Create our depth buffer.
	create_texture(RB_SCOPE_BUFFERS, RB_TEX_DEPTH, get_depth_format(resolve_target, false, can_be_storage), get_depth_usage_bits(resolve_target, false, can_be_storage));

	// Create our MSAA buffers.
	if (msaa_3d == RS::VIEWPORT_MSAA_DISABLED) {
		texture_samples = RD::TEXTURE_SAMPLES_1;
	} else {
		texture_samples = msaa_to_samples(msaa_3d);
		create_texture(RB_SCOPE_BUFFERS, RB_TEX_COLOR_MSAA, base_data_format, get_color_usage_bits(false, true, can_be_storage), texture_samples, Size2i(), 0, 1, true, true);
		create_texture(RB_SCOPE_BUFFERS, RB_TEX_DEPTH_MSAA, get_depth_format(false, true, can_be_storage), get_depth_usage_bits(false, true, can_be_storage), texture_samples, Size2i(), 0, 1, true, true);
	}

	// VRS (note, our vrs object will only be set if VRS is supported)
	RID vrs_texture;
	RS::ViewportVRSMode vrs_mode = texture_storage->render_target_get_vrs_mode(render_target);
	if (vrs && vrs_mode != RS::VIEWPORT_VRS_DISABLED) {
		vrs_texture = create_texture(RB_SCOPE_VRS, RB_TEXTURE, get_vrs_format(), get_vrs_usage_bits(), RD::TEXTURE_SAMPLES_1, vrs->get_vrs_texture_size(internal_size));
	}

	// (re-)configure any named buffers
	for (KeyValue<StringName, Ref<RenderBufferCustomDataRD>> &E : data_buffers) {
		E.value->configure(this);
	}
}

void RenderSceneBuffersRD::configure_for_reflections(const Size2i p_reflection_size) {
	// For now our render buffers for reflections are only used for effects/environment (Sky/Fog/Etc)
	// Possibly at some point move our entire reflection atlas buffer management into this class

	target_size = p_reflection_size;
	internal_size = p_reflection_size;
	render_target = RID();
	scaling_3d_mode = RS::VIEWPORT_SCALING_3D_MODE_OFF;
	fsr_sharpness = 0.0;
	msaa_3d = RS::VIEWPORT_MSAA_DISABLED;
	screen_space_aa = RS::VIEWPORT_SCREEN_SPACE_AA_DISABLED;
	use_taa = false;
	use_debanding = false;
	view_count = 1;

	// cleanout any old buffers we had.
	cleanup();

	// (re-)configure any named buffers
	for (KeyValue<StringName, Ref<RenderBufferCustomDataRD>> &E : data_buffers) {
		E.value->configure(this);
	}
}

void RenderSceneBuffersRD::set_fsr_sharpness(float p_fsr_sharpness) {
	fsr_sharpness = p_fsr_sharpness;
}

void RenderSceneBuffersRD::set_texture_mipmap_bias(float p_texture_mipmap_bias) {
	texture_mipmap_bias = p_texture_mipmap_bias;

	update_samplers();
}

void RenderSceneBuffersRD::set_anisotropic_filtering_level(RS::ViewportAnisotropicFiltering p_anisotropic_filtering_level) {
	anisotropic_filtering_level = p_anisotropic_filtering_level;

	update_samplers();
}

void RenderSceneBuffersRD::set_use_debanding(bool p_use_debanding) {
	use_debanding = p_use_debanding;
}

// Named textures

bool RenderSceneBuffersRD::has_texture(const StringName &p_context, const StringName &p_texture_name) const {
	NTKey key(p_context, p_texture_name);

	return named_textures.has(key);
}

RID RenderSceneBuffersRD::create_texture(const StringName &p_context, const StringName &p_texture_name, const RD::DataFormat p_data_format, const uint32_t p_usage_bits, const RD::TextureSamples p_texture_samples, const Size2i p_size, const uint32_t p_layers, const uint32_t p_mipmaps, bool p_unique, bool p_discardable) {
	// Keep some useful data, we use default values when these are 0.
	Size2i size = p_size == Size2i(0, 0) ? internal_size : p_size;
	uint32_t layers = p_layers == 0 ? view_count : p_layers;
	uint32_t mipmaps = p_mipmaps == 0 ? 1 : p_mipmaps;

	// Create our texture
	RD::TextureFormat tf;
	tf.format = p_data_format;
	if (layers > 1) {
		tf.texture_type = RD::TEXTURE_TYPE_2D_ARRAY;
	}

	tf.width = size.x;
	tf.height = size.y;
	tf.depth = 1;
	tf.array_layers = layers;
	tf.mipmaps = mipmaps;
	tf.usage_bits = p_usage_bits;
	tf.samples = p_texture_samples;
	tf.is_discardable = p_discardable;

	return create_texture_from_format(p_context, p_texture_name, tf, RD::TextureView(), p_unique);
}

RID RenderSceneBuffersRD::_create_texture_from_format(const StringName &p_context, const StringName &p_texture_name, const Ref<RDTextureFormat> &p_texture_format, const Ref<RDTextureView> &p_view, bool p_unique) {
	ERR_FAIL_COND_V(p_texture_format.is_null(), RID());

	RD::TextureView texture_view;
	if (p_view.is_valid()) { // only use when supplied, else default.
		texture_view = p_view->base;
	}

	return create_texture_from_format(p_context, p_texture_name, p_texture_format->base, texture_view, p_unique);
}

RID RenderSceneBuffersRD::create_texture_from_format(const StringName &p_context, const StringName &p_texture_name, const RD::TextureFormat &p_texture_format, RD::TextureView p_view, bool p_unique) {
	// TODO p_unique, if p_unique is true, this is a texture that can be shared. This will be implemented later as an optimization.

	NTKey key(p_context, p_texture_name);

	// check if this is a known texture
	if (named_textures.has(key)) {
		return named_textures[key].texture;
	}

	// Add a new entry..
	NamedTexture &named_texture = named_textures[key];
	named_texture.format = p_texture_format;
	named_texture.is_unique = p_unique;
	named_texture.texture = RD::get_singleton()->texture_create(p_texture_format, p_view);

	Array arr;
	arr.push_back(p_context);
	arr.push_back(p_texture_name);
	RD::get_singleton()->set_resource_name(named_texture.texture, String("RenderBuffer {0}/{1}").format(arr));

	update_sizes(named_texture);

	// The rest is lazy created..

	return named_texture.texture;
}

RID RenderSceneBuffersRD::_create_texture_view(const StringName &p_context, const StringName &p_texture_name, const StringName &p_view_name, const Ref<RDTextureView> p_view) {
	RD::TextureView texture_view;
	if (p_view.is_valid()) { // only use when supplied, else default.
		texture_view = p_view->base;
	}

	return create_texture_view(p_context, p_texture_name, p_view_name, texture_view);
}

RID RenderSceneBuffersRD::create_texture_view(const StringName &p_context, const StringName &p_texture_name, const StringName &p_view_name, RD::TextureView p_view) {
	NTKey view_key(p_context, p_view_name);

	// check if this is a known texture
	if (named_textures.has(view_key)) {
		return named_textures[view_key].texture;
	}

	NTKey key(p_context, p_texture_name);

	ERR_FAIL_COND_V(!named_textures.has(key), RID());

	NamedTexture &named_texture = named_textures[key];
	NamedTexture &view_texture = named_textures[view_key];

	view_texture.format = named_texture.format;
	view_texture.is_unique = named_texture.is_unique;

	view_texture.texture = RD::get_singleton()->texture_create_shared(p_view, named_texture.texture);

	Array arr;
	arr.push_back(p_context);
	arr.push_back(p_view_name);
	RD::get_singleton()->set_resource_name(view_texture.texture, String("RenderBuffer View {0}/{1}").format(arr));

	update_sizes(named_texture);

	return view_texture.texture;
}

RID RenderSceneBuffersRD::get_texture(const StringName &p_context, const StringName &p_texture_name) const {
	NTKey key(p_context, p_texture_name);

	ERR_FAIL_COND_V(!named_textures.has(key), RID());

	return named_textures[key].texture;
}

Ref<RDTextureFormat> RenderSceneBuffersRD::_get_texture_format(const StringName &p_context, const StringName &p_texture_name) const {
	Ref<RDTextureFormat> tf;
	tf.instantiate();

	tf->base = get_texture_format(p_context, p_texture_name);

	return tf;
}

RID RenderSceneBuffersRD::_get_texture_slice_view(const StringName &p_context, const StringName &p_texture_name, const uint32_t p_layer, const uint32_t p_mipmap, const uint32_t p_layers, const uint32_t p_mipmaps, const Ref<RDTextureView> p_view) {
	RD::TextureView texture_view;
	if (p_view.is_valid()) {
		texture_view = p_view->base;
	}

	return get_texture_slice_view(p_context, p_texture_name, p_layer, p_mipmap, p_layers, p_mipmaps, texture_view);
}

const RD::TextureFormat RenderSceneBuffersRD::get_texture_format(const StringName &p_context, const StringName &p_texture_name) const {
	NTKey key(p_context, p_texture_name);

	ERR_FAIL_COND_V(!named_textures.has(key), RD::TextureFormat());

	return named_textures[key].format;
}

RID RenderSceneBuffersRD::get_texture_slice(const StringName &p_context, const StringName &p_texture_name, const uint32_t p_layer, const uint32_t p_mipmap, const uint32_t p_layers, const uint32_t p_mipmaps) {
	return get_texture_slice_view(p_context, p_texture_name, p_layer, p_mipmap, p_layers, p_mipmaps, RD::TextureView());
}

RID RenderSceneBuffersRD::get_texture_slice_view(const StringName &p_context, const StringName &p_texture_name, const uint32_t p_layer, const uint32_t p_mipmap, const uint32_t p_layers, const uint32_t p_mipmaps, RD::TextureView p_view) {
	NTKey key(p_context, p_texture_name);

	// check if this is a known texture
	ERR_FAIL_COND_V(!named_textures.has(key), RID());
	NamedTexture &named_texture = named_textures[key];
	ERR_FAIL_COND_V(named_texture.texture.is_null(), RID());

	// check if we're in bounds
	ERR_FAIL_UNSIGNED_INDEX_V(p_layer, named_texture.format.array_layers, RID());
	ERR_FAIL_COND_V(p_layers == 0, RID());
	ERR_FAIL_COND_V(p_layer + p_layers > named_texture.format.array_layers, RID());
	ERR_FAIL_UNSIGNED_INDEX_V(p_mipmap, named_texture.format.mipmaps, RID());
	ERR_FAIL_COND_V(p_mipmaps == 0, RID());
	ERR_FAIL_COND_V(p_mipmap + p_mipmaps > named_texture.format.mipmaps, RID());

	// asking the whole thing? just return the original
	RD::TextureView default_view = RD::TextureView();
	if (p_layer == 0 && p_mipmap == 0 && named_texture.format.array_layers == p_layers && named_texture.format.mipmaps == p_mipmaps && p_view == default_view) {
		return named_texture.texture;
	}

	// see if we have this
	NTSliceKey slice_key(p_layer, p_layers, p_mipmap, p_mipmaps, p_view);
	if (named_texture.slices.has(slice_key)) {
		return named_texture.slices[slice_key];
	}

	// create our slice
	RID &slice = named_texture.slices[slice_key];
	slice = RD::get_singleton()->texture_create_shared_from_slice(p_view, named_texture.texture, p_layer, p_mipmap, p_mipmaps, p_layers > 1 ? RD::TEXTURE_SLICE_2D_ARRAY : RD::TEXTURE_SLICE_2D, p_layers);

	Array arr;
	arr.push_back(p_context);
	arr.push_back(p_texture_name);
	arr.push_back(itos(p_layer));
	arr.push_back(itos(p_layers));
	arr.push_back(itos(p_mipmap));
	arr.push_back(itos(p_mipmaps));
	arr.push_back(itos(p_view.format_override));
	arr.push_back(itos(p_view.swizzle_r));
	arr.push_back(itos(p_view.swizzle_g));
	arr.push_back(itos(p_view.swizzle_b));
	arr.push_back(itos(p_view.swizzle_a));
	RD::get_singleton()->set_resource_name(slice, String("RenderBuffer {0}/{1}, layer {2}/{3}, mipmap {4}/{5}, view {6}/{7}/{8}/{9}/{10}").format(arr));

	// and return our slice
	return slice;
}

Size2i RenderSceneBuffersRD::get_texture_slice_size(const StringName &p_context, const StringName &p_texture_name, const uint32_t p_mipmap) {
	NTKey key(p_context, p_texture_name);

	// check if this is a known texture
	ERR_FAIL_COND_V(!named_textures.has(key), Size2i());
	NamedTexture &named_texture = named_textures[key];
	ERR_FAIL_COND_V(named_texture.texture.is_null(), Size2i());

	// check if we're in bounds
	ERR_FAIL_UNSIGNED_INDEX_V(p_mipmap, named_texture.format.mipmaps, Size2i());

	// return our size
	return named_texture.sizes[p_mipmap];
}

void RenderSceneBuffersRD::clear_context(const StringName &p_context) {
	Vector<NTKey> to_free; // free these

	// Find all entries for our context, we don't want to free them yet or our loop fails.
	for (KeyValue<NTKey, NamedTexture> &E : named_textures) {
		if (E.key.context == p_context) {
			to_free.push_back(E.key);
		}
	}

	// Now free these and remove them from our textures
	for (NTKey &key : to_free) {
		free_named_texture(named_textures[key]);
		named_textures.erase(key);
	}
}

// Allocate shared buffers
void RenderSceneBuffersRD::allocate_blur_textures() {
	if (has_texture(RB_SCOPE_BUFFERS, RB_TEX_BLUR_0)) {
		// already allocated...
		return;
	}

	Size2i blur_size = internal_size;
	if (scaling_3d_mode == RS::VIEWPORT_SCALING_3D_MODE_FSR2) {
		// The blur texture should be as big as the target size when using an upscaler.
		blur_size = target_size;
	}

	uint32_t mipmaps_required = Image::get_image_required_mipmaps(blur_size.x, blur_size.y, Image::FORMAT_RGBAH);

	uint32_t usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT;
	if (can_be_storage) {
		usage_bits += RD::TEXTURE_USAGE_STORAGE_BIT;
	} else {
		usage_bits += RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
	}

	create_texture(RB_SCOPE_BUFFERS, RB_TEX_BLUR_0, base_data_format, usage_bits, RD::TEXTURE_SAMPLES_1, blur_size, view_count, mipmaps_required);
	create_texture(RB_SCOPE_BUFFERS, RB_TEX_BLUR_1, base_data_format, usage_bits, RD::TEXTURE_SAMPLES_1, Size2i(blur_size.x >> 1, blur_size.y >> 1), view_count, mipmaps_required - 1);

	// if !can_be_storage we need a half width version
	if (!can_be_storage) {
		create_texture(RB_SCOPE_BUFFERS, RB_TEX_HALF_BLUR, base_data_format, usage_bits, RD::TEXTURE_SAMPLES_1, Size2i(blur_size.x >> 1, blur_size.y), 1, mipmaps_required);
	}

	// TODO redo this:
	if (!can_be_storage) {
		// create 4 weight textures, 2 full size, 2 half size

		RD::TextureFormat tf;
		tf.format = RD::DATA_FORMAT_R16_SFLOAT; // We could probably use DATA_FORMAT_R8_SNORM if we don't pre-multiply by blur_size but that depends on whether we can remove DEPTH_GAP
		tf.width = blur_size.x;
		tf.height = blur_size.y;
		tf.texture_type = RD::TEXTURE_TYPE_2D;
		tf.array_layers = 1; // Our DOF effect handles one eye per turn
		tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
		tf.mipmaps = 1;
		for (uint32_t i = 0; i < 4; i++) {
			// associated blur texture
			RID texture;
			if (i == 1) {
				texture = get_texture_slice(RB_SCOPE_BUFFERS, RB_TEX_BLUR_0, 0, 0);
			} else if (i == 2) {
				texture = get_texture_slice(RB_SCOPE_BUFFERS, RB_TEX_BLUR_1, 0, 0);
			} else if (i == 3) {
				texture = get_texture_slice(RB_SCOPE_BUFFERS, RB_TEX_BLUR_0, 0, 1);
			}

			// create weight texture
			weight_buffers[i].weight = RD::get_singleton()->texture_create(tf, RD::TextureView());

			// create frame buffer
			Vector<RID> fb;
			if (i != 0) {
				fb.push_back(texture);
			}
			fb.push_back(weight_buffers[i].weight);
			weight_buffers[i].fb = RD::get_singleton()->framebuffer_create(fb);

			if (i == 1) {
				// next 2 are half size
				tf.width = MAX(1u, tf.width >> 1);
				tf.height = MAX(1u, tf.height >> 1);
			}
		}
	}
}

// Data buffers

bool RenderSceneBuffersRD::has_custom_data(const StringName &p_name) {
	return data_buffers.has(p_name);
}

void RenderSceneBuffersRD::set_custom_data(const StringName &p_name, Ref<RenderBufferCustomDataRD> p_data) {
	if (p_data.is_valid()) {
		data_buffers[p_name] = p_data;
	} else if (has_custom_data(p_name)) {
		data_buffers.erase(p_name);
	}
}

Ref<RenderBufferCustomDataRD> RenderSceneBuffersRD::get_custom_data(const StringName &p_name) const {
	ERR_FAIL_COND_V(!data_buffers.has(p_name), Ref<RenderBufferCustomDataRD>());

	Ref<RenderBufferCustomDataRD> ret = data_buffers[p_name];

	return ret;
}

// Depth texture

bool RenderSceneBuffersRD::has_depth_texture() {
	if (render_target.is_null()) {
		// not applicable when there is no render target (likely this is for a reflection probe)
		return false;
	}

	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();
	RID depth = texture_storage->render_target_get_override_depth(render_target);
	if (depth.is_valid()) {
		return true;
	} else {
		return has_texture(RB_SCOPE_BUFFERS, RB_TEX_DEPTH);
	}
}

RID RenderSceneBuffersRD::get_depth_texture() {
	if (render_target.is_null()) {
		// not applicable when there is no render target (likely this is for a reflection probe)
		return RID();
	}

	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();
	RID depth = texture_storage->render_target_get_override_depth(render_target);
	if (depth.is_valid()) {
		return depth;
	} else {
		return get_texture(RB_SCOPE_BUFFERS, RB_TEX_DEPTH);
	}
}

RID RenderSceneBuffersRD::get_depth_texture(const uint32_t p_layer) {
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();
	RID depth_slice = texture_storage->render_target_get_override_depth_slice(render_target, p_layer);
	if (depth_slice.is_valid()) {
		return depth_slice;
	} else {
		return get_texture_slice(RB_SCOPE_BUFFERS, RB_TEX_DEPTH, p_layer, 0);
	}
}

// Upscaled texture.

void RenderSceneBuffersRD::ensure_upscaled() {
	if (!has_upscaled_texture()) {
		uint32_t usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | (can_be_storage ? RD::TEXTURE_USAGE_STORAGE_BIT : 0) | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
		usage_bits |= RD::TEXTURE_USAGE_INPUT_ATTACHMENT_BIT;
		create_texture(RB_SCOPE_BUFFERS, RB_TEX_COLOR_UPSCALED, base_data_format, usage_bits, RD::TEXTURE_SAMPLES_1, target_size);
	}
}

// Velocity texture.

void RenderSceneBuffersRD::ensure_velocity() {
	if (!has_texture(RB_SCOPE_BUFFERS, RB_TEX_VELOCITY)) {
		const bool msaa = msaa_3d != RS::VIEWPORT_MSAA_DISABLED;
		create_texture(RB_SCOPE_BUFFERS, RB_TEX_VELOCITY, get_velocity_format(), get_velocity_usage_bits(msaa, false, can_be_storage));

		if (msaa) {
			create_texture(RB_SCOPE_BUFFERS, RB_TEX_VELOCITY_MSAA, get_velocity_format(), get_velocity_usage_bits(false, msaa, can_be_storage), texture_samples);
		}
	}
}

bool RenderSceneBuffersRD::has_velocity_buffer(bool p_has_msaa) {
	if (p_has_msaa) {
		return has_texture(RB_SCOPE_BUFFERS, RB_TEX_VELOCITY_MSAA);
	} else {
		RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();
		RID velocity = texture_storage->render_target_get_override_velocity(render_target);
		if (velocity.is_valid()) {
			return true;
		} else {
			return has_texture(RB_SCOPE_BUFFERS, RB_TEX_VELOCITY);
		}
	}
}

RID RenderSceneBuffersRD::get_velocity_buffer(bool p_get_msaa) {
	if (p_get_msaa) {
		if (!has_texture(RB_SCOPE_BUFFERS, RB_TEX_VELOCITY_MSAA)) {
			return RID();
		} else {
			return get_texture(RB_SCOPE_BUFFERS, RB_TEX_VELOCITY_MSAA);
		}
	} else {
		RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();
		RID velocity = texture_storage->render_target_get_override_velocity(render_target);
		if (velocity.is_valid()) {
			return velocity;
		} else if (!has_texture(RB_SCOPE_BUFFERS, RB_TEX_VELOCITY)) {
			return RID();
		} else {
			return get_texture(RB_SCOPE_BUFFERS, RB_TEX_VELOCITY);
		}
	}
}

RID RenderSceneBuffersRD::get_velocity_buffer(bool p_get_msaa, uint32_t p_layer) {
	if (p_get_msaa) {
		return get_texture_slice(RB_SCOPE_BUFFERS, RB_TEX_VELOCITY_MSAA, p_layer, 0);
	} else {
		RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();
		RID velocity_slice = texture_storage->render_target_get_override_velocity_slice(render_target, p_layer);
		if (velocity_slice.is_valid()) {
			return velocity_slice;
		} else {
			return get_texture_slice(RB_SCOPE_BUFFERS, RB_TEX_VELOCITY, p_layer, 0);
		}
	}
}

uint32_t RenderSceneBuffersRD::get_color_usage_bits(bool p_resolve, bool p_msaa, bool p_storage) {
	DEV_ASSERT((!p_resolve && !p_msaa) || (p_resolve != p_msaa));

	uint32_t usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_INPUT_ATTACHMENT_BIT;
	if (p_msaa) {
		usage_bits |= RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
	} else if (p_resolve) {
		usage_bits |= RD::TEXTURE_USAGE_CAN_COPY_TO_BIT | (p_storage ? RD::TEXTURE_USAGE_STORAGE_BIT : 0);
	} else {
		usage_bits |= (p_storage ? RD::TEXTURE_USAGE_STORAGE_BIT : 0);
	}

	return usage_bits;
}

RD::DataFormat RenderSceneBuffersRD::get_depth_format(bool p_resolve, bool p_msaa, bool p_storage) {
	if (p_resolve) {
		return RD::DATA_FORMAT_R32_SFLOAT;
	} else {
		const RenderingDeviceCommons::DataFormat preferred_formats[2] = {
			p_storage ? RD::DATA_FORMAT_D32_SFLOAT_S8_UINT : RD::DATA_FORMAT_D24_UNORM_S8_UINT,
			p_storage ? RD::DATA_FORMAT_D24_UNORM_S8_UINT : RD::DATA_FORMAT_D32_SFLOAT_S8_UINT
		};

		return RD::get_singleton()->texture_is_format_supported_for_usage(preferred_formats[0], get_depth_usage_bits(p_resolve, p_msaa, p_storage)) ? preferred_formats[0] : preferred_formats[1];
	}
}

uint32_t RenderSceneBuffersRD::get_depth_usage_bits(bool p_resolve, bool p_msaa, bool p_storage) {
	DEV_ASSERT((!p_resolve && !p_msaa) || (p_resolve != p_msaa));

	uint32_t usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT;
	if (p_msaa) {
		usage_bits |= RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
	} else if (p_resolve) {
		usage_bits |= RD::TEXTURE_USAGE_CAN_COPY_TO_BIT | (p_storage ? RD::TEXTURE_USAGE_STORAGE_BIT : 0);
	} else {
		usage_bits |= RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}

	return usage_bits;
}

RD::DataFormat RenderSceneBuffersRD::get_velocity_format() {
	return RD::DATA_FORMAT_R16G16_SFLOAT;
}

uint32_t RenderSceneBuffersRD::get_velocity_usage_bits(bool p_resolve, bool p_msaa, bool p_storage) {
	return get_color_usage_bits(p_resolve, p_msaa, p_storage);
}

RD::DataFormat RenderSceneBuffersRD::get_vrs_format() {
	return RD::DATA_FORMAT_R8_UINT;
}

uint32_t RenderSceneBuffersRD::get_vrs_usage_bits() {
	return RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_VRS_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT;
}
