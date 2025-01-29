/**************************************************************************/
/*  texture_storage.h                                                     */
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

#ifndef TEXTURE_STORAGE_H
#define TEXTURE_STORAGE_H

#include "servers/rendering_server.h"

class RendererTextureStorage {
private:
	Color default_clear_color;

public:
	void set_default_clear_color(const Color &p_color) {
		default_clear_color = p_color;
	}

	Color get_default_clear_color() const {
		return default_clear_color;
	}

	/* Canvas Texture API */

	virtual RID canvas_texture_allocate() = 0;
	virtual void canvas_texture_initialize(RID p_rid) = 0;
	virtual void canvas_texture_free(RID p_rid) = 0;

	virtual void canvas_texture_set_channel(RID p_canvas_texture, RS::CanvasTextureChannel p_channel, RID p_texture) = 0;
	virtual void canvas_texture_set_shading_parameters(RID p_canvas_texture, const Color &p_base_color, float p_shininess) = 0;

	virtual void canvas_texture_set_texture_filter(RID p_item, RS::CanvasItemTextureFilter p_filter) = 0;
	virtual void canvas_texture_set_texture_repeat(RID p_item, RS::CanvasItemTextureRepeat p_repeat) = 0;

	/* Texture API */

	virtual ~RendererTextureStorage() {}

	virtual RID texture_allocate() = 0;
	virtual void texture_free(RID p_rid) = 0;

	virtual void texture_2d_initialize(RID p_texture, const Ref<Image> &p_image) = 0;
	virtual void texture_2d_layered_initialize(RID p_texture, const Vector<Ref<Image>> &p_layers, RS::TextureLayeredType p_layered_type) = 0;
	virtual void texture_3d_initialize(RID p_texture, Image::Format, int p_width, int p_height, int p_depth, bool p_mipmaps, const Vector<Ref<Image>> &p_data) = 0;
	virtual void texture_external_initialize(RID p_texture, int p_width, int p_height, uint64_t p_external_buffer) = 0;
	virtual void texture_proxy_initialize(RID p_texture, RID p_base) = 0; //all slices, then all the mipmaps, must be coherent

	virtual RID texture_create_from_native_handle(RS::TextureType p_type, Image::Format p_format, uint64_t p_native_handle, int p_width, int p_height, int p_depth, int p_layers = 1, RS::TextureLayeredType p_layered_type = RS::TEXTURE_LAYERED_2D_ARRAY) = 0;

	virtual void texture_2d_update(RID p_texture, const Ref<Image> &p_image, int p_layer = 0) = 0;
	virtual void texture_3d_update(RID p_texture, const Vector<Ref<Image>> &p_data) = 0;
	virtual void texture_external_update(RID p_proxy, int p_width, int p_height, uint64_t p_external_buffer) = 0;
	virtual void texture_proxy_update(RID p_proxy, RID p_base) = 0;

	//these two APIs can be used together or in combination with the others.
	virtual void texture_2d_placeholder_initialize(RID p_texture) = 0;
	virtual void texture_2d_layered_placeholder_initialize(RID p_texture, RenderingServer::TextureLayeredType p_layered_type) = 0;
	virtual void texture_3d_placeholder_initialize(RID p_texture) = 0;

	virtual Ref<Image> texture_2d_get(RID p_texture) const = 0;
	virtual Ref<Image> texture_2d_layer_get(RID p_texture, int p_layer) const = 0;
	virtual Vector<Ref<Image>> texture_3d_get(RID p_texture) const = 0;

	virtual void texture_replace(RID p_texture, RID p_by_texture) = 0;
	virtual void texture_set_size_override(RID p_texture, int p_width, int p_height) = 0;

	virtual void texture_set_path(RID p_texture, const String &p_path) = 0;
	virtual String texture_get_path(RID p_texture) const = 0;

	virtual Image::Format texture_get_format(RID p_texture) const = 0;

	virtual void texture_set_detect_3d_callback(RID p_texture, RS::TextureDetectCallback p_callback, void *p_userdata) = 0;
	virtual void texture_set_detect_normal_callback(RID p_texture, RS::TextureDetectCallback p_callback, void *p_userdata) = 0;
	virtual void texture_set_detect_roughness_callback(RID p_texture, RS::TextureDetectRoughnessCallback p_callback, void *p_userdata) = 0;

	virtual void texture_debug_usage(List<RS::TextureInfo> *r_info) = 0;

	virtual void texture_set_force_redraw_if_visible(RID p_texture, bool p_enable) = 0;

	virtual Size2 texture_size_with_proxy(RID p_proxy) = 0;

	virtual void texture_rd_initialize(RID p_texture, const RID &p_rd_texture, const RS::TextureLayeredType p_layer_type = RS::TEXTURE_LAYERED_2D_ARRAY) = 0;
	virtual RID texture_get_rd_texture(RID p_texture, bool p_srgb = false) const = 0;
	virtual uint64_t texture_get_native_handle(RID p_texture, bool p_srgb = false) const = 0;

	/* Decal API */
	virtual RID decal_allocate() = 0;
	virtual void decal_initialize(RID p_rid) = 0;
	virtual void decal_free(RID p_rid) = 0;

	virtual void decal_set_size(RID p_decal, const Vector3 &p_size) = 0;
	virtual void decal_set_texture(RID p_decal, RS::DecalTexture p_type, RID p_texture) = 0;
	virtual void decal_set_emission_energy(RID p_decal, float p_energy) = 0;
	virtual void decal_set_albedo_mix(RID p_decal, float p_mix) = 0;
	virtual void decal_set_modulate(RID p_decal, const Color &p_modulate) = 0;
	virtual void decal_set_cull_mask(RID p_decal, uint32_t p_layers) = 0;
	virtual void decal_set_distance_fade(RID p_decal, bool p_enabled, float p_begin, float p_length) = 0;
	virtual void decal_set_fade(RID p_decal, float p_above, float p_below) = 0;
	virtual void decal_set_normal_fade(RID p_decal, float p_fade) = 0;

	virtual AABB decal_get_aabb(RID p_decal) const = 0;
	virtual uint32_t decal_get_cull_mask(RID p_decal) const = 0;

	virtual void texture_add_to_decal_atlas(RID p_texture, bool p_panorama_to_dp = false) = 0;
	virtual void texture_remove_from_decal_atlas(RID p_texture, bool p_panorama_to_dp = false) = 0;

	/* DECAL INSTANCE */

	virtual RID decal_instance_create(RID p_decal) = 0;
	virtual void decal_instance_free(RID p_decal_instance) = 0;
	virtual void decal_instance_set_transform(RID p_decal_instance, const Transform3D &p_transform) = 0;
	virtual void decal_instance_set_sorting_offset(RID p_decal_instance, float p_sorting_offset) = 0;

	/* RENDER TARGET */

	virtual RID render_target_create() = 0;
	virtual void render_target_free(RID p_rid) = 0;

	virtual void render_target_set_position(RID p_render_target, int p_x, int p_y) = 0; // Q change input to const Point2i &p_position ?
	virtual Point2i render_target_get_position(RID p_render_target) const = 0;
	virtual void render_target_set_size(RID p_render_target, int p_width, int p_height, uint32_t p_view_count) = 0; // Q change input to const Size2i &p_size ?
	virtual Size2i render_target_get_size(RID p_render_target) const = 0;
	virtual void render_target_set_transparent(RID p_render_target, bool p_is_transparent) = 0;
	virtual bool render_target_get_transparent(RID p_render_target) const = 0;
	virtual void render_target_set_direct_to_screen(RID p_render_target, bool p_direct_to_screen) = 0;
	virtual bool render_target_get_direct_to_screen(RID p_render_target) const = 0;
	virtual bool render_target_was_used(RID p_render_target) const = 0;
	virtual void render_target_set_as_unused(RID p_render_target) = 0;
	virtual void render_target_set_msaa(RID p_render_target, RS::ViewportMSAA p_msaa) = 0;
	virtual RS::ViewportMSAA render_target_get_msaa(RID p_render_target) const = 0;
	virtual void render_target_set_msaa_needs_resolve(RID p_render_target, bool p_needs_resolve) = 0;
	virtual bool render_target_get_msaa_needs_resolve(RID p_render_target) const = 0;
	virtual void render_target_do_msaa_resolve(RID p_render_target) = 0;
	virtual void render_target_set_use_hdr(RID p_render_target, bool p_use_hdr) = 0;
	virtual bool render_target_is_using_hdr(RID p_render_target) const = 0;

	virtual void render_target_request_clear(RID p_render_target, const Color &p_clear_color) = 0;
	virtual bool render_target_is_clear_requested(RID p_render_target) = 0;
	virtual Color render_target_get_clear_request_color(RID p_render_target) = 0;
	virtual void render_target_disable_clear_request(RID p_render_target) = 0;
	virtual void render_target_do_clear_request(RID p_render_target) = 0;

	virtual void render_target_set_sdf_size_and_scale(RID p_render_target, RS::ViewportSDFOversize p_size, RS::ViewportSDFScale p_scale) = 0;
	virtual Rect2i render_target_get_sdf_rect(RID p_render_target) const = 0;
	virtual void render_target_mark_sdf_enabled(RID p_render_target, bool p_enabled) = 0;

	virtual void render_target_set_vrs_mode(RID p_render_target, RS::ViewportVRSMode p_mode) = 0;
	virtual RS::ViewportVRSMode render_target_get_vrs_mode(RID p_render_target) const = 0;
	virtual void render_target_set_vrs_update_mode(RID p_render_target, RS::ViewportVRSUpdateMode p_mode) = 0;
	virtual RS::ViewportVRSUpdateMode render_target_get_vrs_update_mode(RID p_render_target) const = 0;
	virtual void render_target_set_vrs_texture(RID p_render_target, RID p_texture) = 0;
	virtual RID render_target_get_vrs_texture(RID p_render_target) const = 0;

	// override color, depth and velocity buffers (depth and velocity only for 3D)
	virtual void render_target_set_override(RID p_render_target, RID p_color_texture, RID p_depth_texture, RID p_velocity_texture, RID p_velocity_depth_texture) = 0;
	virtual RID render_target_get_override_color(RID p_render_target) const = 0;
	virtual RID render_target_get_override_depth(RID p_render_target) const = 0;
	virtual RID render_target_get_override_velocity(RID p_render_target) const = 0;
	virtual RID render_target_get_override_velocity_depth(RID p_render_target) const = 0;

	virtual void render_target_set_render_region(RID p_render_target, const Rect2i &p_render_region) = 0;
	virtual Rect2i render_target_get_render_region(RID p_render_target) const = 0;

	// get textures
	virtual RID render_target_get_texture(RID p_render_target) = 0;

	// Motion vectors
	virtual void render_target_set_velocity_target_size(RID p_render_target, const Size2i &p_target_size) = 0;
	virtual Size2i render_target_get_velocity_target_size(RID p_render_target) const = 0;
};

#endif // TEXTURE_STORAGE_H
