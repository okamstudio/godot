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

#ifndef TEXTURE_STORAGE_DUMMY_H
#define TEXTURE_STORAGE_DUMMY_H

#include "servers/rendering/rendering_server_globals.h"
#include "servers/rendering/storage/texture_storage.h"

namespace RendererDummy {

class TextureStorage : public RendererTextureStorage {
private:
	static TextureStorage *singleton;

	struct DummyTexture {
		Ref<Image> image;
	};
	mutable RID_PtrOwner<DummyTexture> texture_owner;

public:
	static TextureStorage *get_singleton() { return singleton; }

	TextureStorage();
	~TextureStorage();

	/* Canvas Texture API */

	virtual RID canvas_texture_allocate() override { return RID(); }
	virtual void canvas_texture_initialize(RID p_rid) override {}
	virtual void canvas_texture_free(RID p_rid) override {}

	virtual void canvas_texture_set_channel(RID p_canvas_texture, RS::CanvasTextureChannel p_channel, RID p_texture) override {}
	virtual void canvas_texture_set_shading_parameters(RID p_canvas_texture, const Color &p_base_color, float p_shininess) override {}

	virtual void canvas_texture_set_texture_filter(RID p_item, RS::CanvasItemTextureFilter p_filter) override {}
	virtual void canvas_texture_set_texture_repeat(RID p_item, RS::CanvasItemTextureRepeat p_repeat) override {}

	/* Texture API */

	bool owns_texture(RID p_rid) { return texture_owner.owns(p_rid); }

	virtual RID texture_allocate() override {
		DummyTexture *texture = memnew(DummyTexture);
		ERR_FAIL_NULL_V(texture, RID());
		return texture_owner.make_rid(texture);
	}

	virtual void texture_free(RID p_rid) override {
		// delete the texture
		DummyTexture *texture = texture_owner.get_or_null(p_rid);
		ERR_FAIL_NULL(texture);
		texture_owner.free(p_rid);
		memdelete(texture);
	}

	virtual void texture_2d_initialize(RID p_texture, const Ref<Image> &p_image) override {
		DummyTexture *t = texture_owner.get_or_null(p_texture);
		ERR_FAIL_NULL(t);
		t->image = p_image->duplicate();
	}
	virtual void texture_2d_layered_initialize(RID p_texture, const Vector<Ref<Image>> &p_layers, RS::TextureLayeredType p_layered_type) override {}
	virtual void texture_3d_initialize(RID p_texture, Image::Format, int p_width, int p_height, int p_depth, bool p_mipmaps, const Vector<Ref<Image>> &p_data) override {}
	virtual void texture_external_initialize(RID p_texture, int p_width, int p_height, uint64_t p_external_buffer) override {}
	virtual void texture_proxy_initialize(RID p_texture, RID p_base) override {} //all slices, then all the mipmaps, must be coherent

	virtual RID texture_create_from_native_handle(RS::TextureType p_type, Image::Format p_format, uint64_t p_native_handle, int p_width, int p_height, int p_depth, int p_layers = 1, RS::TextureLayeredType p_layered_type = RS::TEXTURE_LAYERED_2D_ARRAY) override { return RID(); }

	virtual void texture_2d_update(RID p_texture, const Ref<Image> &p_image, int p_layer = 0) override {}
	virtual void texture_3d_update(RID p_texture, const Vector<Ref<Image>> &p_data) override {}
	virtual void texture_external_update(RID p_texture, int p_width, int p_height, uint64_t p_external_buffer) override {}
	virtual void texture_proxy_update(RID p_proxy, RID p_base) override {}

	//these two APIs can be used together or in combination with the others.
	virtual void texture_2d_placeholder_initialize(RID p_texture) override {}
	virtual void texture_2d_layered_placeholder_initialize(RID p_texture, RenderingServer::TextureLayeredType p_layered_type) override {}
	virtual void texture_3d_placeholder_initialize(RID p_texture) override {}

	virtual Ref<Image> texture_2d_get(RID p_texture) const override {
		DummyTexture *t = texture_owner.get_or_null(p_texture);
		ERR_FAIL_NULL_V(t, Ref<Image>());
		return t->image;
	}
	virtual Ref<Image> texture_2d_layer_get(RID p_texture, int p_layer) const override { return Ref<Image>(); }
	virtual Vector<Ref<Image>> texture_3d_get(RID p_texture) const override { return Vector<Ref<Image>>(); }

	virtual void texture_replace(RID p_texture, RID p_by_texture) override { texture_free(p_by_texture); }
	virtual void texture_set_size_override(RID p_texture, int p_width, int p_height) override {}

	virtual void texture_set_path(RID p_texture, const String &p_path) override {}
	virtual String texture_get_path(RID p_texture) const override { return String(); }

	virtual Image::Format texture_get_format(RID p_texture) const override { return Image::FORMAT_MAX; }

	virtual void texture_set_detect_3d_callback(RID p_texture, RS::TextureDetectCallback p_callback, void *p_userdata) override {}
	virtual void texture_set_detect_normal_callback(RID p_texture, RS::TextureDetectCallback p_callback, void *p_userdata) override {}
	virtual void texture_set_detect_roughness_callback(RID p_texture, RS::TextureDetectRoughnessCallback p_callback, void *p_userdata) override {}

	virtual void texture_debug_usage(List<RS::TextureInfo> *r_info) override {}

	virtual void texture_set_force_redraw_if_visible(RID p_texture, bool p_enable) override {}

	virtual Size2 texture_size_with_proxy(RID p_proxy) override { return Size2(); }

	virtual void texture_rd_initialize(RID p_texture, const RID &p_rd_texture, const RS::TextureLayeredType p_layer_type = RS::TEXTURE_LAYERED_2D_ARRAY) override {}
	virtual RID texture_get_rd_texture(RID p_texture, bool p_srgb = false) const override { return RID(); }
	virtual uint64_t texture_get_native_handle(RID p_texture, bool p_srgb = false) const override { return 0; }

	/* DECAL API */
	virtual RID decal_allocate() override { return RID(); }
	virtual void decal_initialize(RID p_rid) override {}
	virtual void decal_free(RID p_rid) override {}

	virtual void decal_set_size(RID p_decal, const Vector3 &p_size) override {}
	virtual void decal_set_texture(RID p_decal, RS::DecalTexture p_type, RID p_texture) override {}
	virtual void decal_set_emission_energy(RID p_decal, float p_energy) override {}
	virtual void decal_set_albedo_mix(RID p_decal, float p_mix) override {}
	virtual void decal_set_modulate(RID p_decal, const Color &p_modulate) override {}
	virtual void decal_set_cull_mask(RID p_decal, uint32_t p_layers) override {}
	virtual void decal_set_distance_fade(RID p_decal, bool p_enabled, float p_begin, float p_length) override {}
	virtual void decal_set_fade(RID p_decal, float p_above, float p_below) override {}
	virtual void decal_set_normal_fade(RID p_decal, float p_fade) override {}

	virtual AABB decal_get_aabb(RID p_decal) const override { return AABB(); }
	virtual uint32_t decal_get_cull_mask(RID p_decal) const override { return 0; }

	virtual void texture_add_to_decal_atlas(RID p_texture, bool p_panorama_to_dp = false) override {}
	virtual void texture_remove_from_decal_atlas(RID p_texture, bool p_panorama_to_dp = false) override {}

	/* DECAL INSTANCE */

	virtual RID decal_instance_create(RID p_decal) override { return RID(); }
	virtual void decal_instance_free(RID p_decal_instance) override {}
	virtual void decal_instance_set_transform(RID p_decal, const Transform3D &p_transform) override {}
	virtual void decal_instance_set_sorting_offset(RID p_decal_instance, float p_sorting_offset) override {}

	/* RENDER TARGET */

	virtual RID render_target_create() override { return RID(); }
	virtual void render_target_free(RID p_rid) override {}
	virtual void render_target_set_position(RID p_render_target, int p_x, int p_y) override {}
	virtual Point2i render_target_get_position(RID p_render_target) const override { return Point2i(); }
	virtual void render_target_set_size(RID p_render_target, int p_width, int p_height, uint32_t p_view_count) override {}
	virtual Size2i render_target_get_size(RID p_render_target) const override { return Size2i(); }
	virtual void render_target_set_transparent(RID p_render_target, bool p_is_transparent) override {}
	virtual bool render_target_get_transparent(RID p_render_target) const override { return false; }
	virtual void render_target_set_direct_to_screen(RID p_render_target, bool p_direct_to_screen) override {}
	virtual bool render_target_get_direct_to_screen(RID p_render_target) const override { return false; }
	virtual bool render_target_was_used(RID p_render_target) const override { return false; }
	virtual void render_target_set_as_unused(RID p_render_target) override {}
	virtual void render_target_set_msaa(RID p_render_target, RS::ViewportMSAA p_msaa) override {}
	virtual RS::ViewportMSAA render_target_get_msaa(RID p_render_target) const override { return RS::VIEWPORT_MSAA_DISABLED; }
	virtual void render_target_set_msaa_needs_resolve(RID p_render_target, bool p_needs_resolve) override {}
	virtual bool render_target_get_msaa_needs_resolve(RID p_render_target) const override { return false; }
	virtual void render_target_do_msaa_resolve(RID p_render_target) override {}
	virtual void render_target_set_use_hdr(RID p_render_target, bool p_use_hdr_2d) override {}
	virtual bool render_target_is_using_hdr(RID p_render_target) const override { return false; }

	virtual void render_target_request_clear(RID p_render_target, const Color &p_clear_color) override {}
	virtual bool render_target_is_clear_requested(RID p_render_target) override { return false; }
	virtual Color render_target_get_clear_request_color(RID p_render_target) override { return Color(); }
	virtual void render_target_disable_clear_request(RID p_render_target) override {}
	virtual void render_target_do_clear_request(RID p_render_target) override {}

	virtual void render_target_set_sdf_size_and_scale(RID p_render_target, RS::ViewportSDFOversize p_size, RS::ViewportSDFScale p_scale) override {}
	virtual Rect2i render_target_get_sdf_rect(RID p_render_target) const override { return Rect2i(); }
	virtual void render_target_mark_sdf_enabled(RID p_render_target, bool p_enabled) override {}

	virtual void render_target_set_vrs_mode(RID p_render_target, RS::ViewportVRSMode p_mode) override {}
	virtual RS::ViewportVRSMode render_target_get_vrs_mode(RID p_render_target) const override { return RS::VIEWPORT_VRS_DISABLED; }
	virtual void render_target_set_vrs_update_mode(RID p_render_target, RS::ViewportVRSUpdateMode p_mode) override {}
	virtual RS::ViewportVRSUpdateMode render_target_get_vrs_update_mode(RID p_render_target) const override { return RS::VIEWPORT_VRS_UPDATE_DISABLED; }
	virtual void render_target_set_vrs_texture(RID p_render_target, RID p_texture) override {}
	virtual RID render_target_get_vrs_texture(RID p_render_target) const override { return RID(); }

	virtual void render_target_set_override(RID p_render_target, RID p_color_texture, RID p_depth_texture, RID p_velocity_texture, RID p_velocity_depth_texture) override {}
	virtual RID render_target_get_override_color(RID p_render_target) const override { return RID(); }
	virtual RID render_target_get_override_depth(RID p_render_target) const override { return RID(); }
	virtual RID render_target_get_override_velocity(RID p_render_target) const override { return RID(); }
	virtual RID render_target_get_override_velocity_depth(RID p_render_target) const override { return RID(); }

	virtual RID render_target_get_texture(RID p_render_target) override { return RID(); }

	virtual void render_target_set_velocity_target_size(RID p_render_target, const Size2i &p_target_size) override {}
	virtual Size2i render_target_get_velocity_target_size(RID p_render_target) const override { return Size2i(0, 0); }
};

} // namespace RendererDummy

#endif // TEXTURE_STORAGE_DUMMY_H
