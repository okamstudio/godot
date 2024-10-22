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

#ifndef TEXTURE_STORAGE_RD_H
#define TEXTURE_STORAGE_RD_H

#include "core/templates/local_vector.h"
#include "core/templates/paged_array.h"
#include "core/templates/rid_owner.h"
#include "servers/rendering/renderer_rd/shaders/canvas_sdf.glsl.gen.h"
#include "servers/rendering/renderer_rd/storage_rd/forward_id_storage.h"
#include "servers/rendering/rendering_server_default.h"
#include "servers/rendering/storage/texture_storage.h"
#include "servers/rendering/storage/utilities.h"

namespace RendererRD {

class LightStorage;
class MaterialStorage;

class TextureStorage : public RendererTextureStorage {
public:
	enum DefaultRDTexture {
		DEFAULT_RD_TEXTURE_WHITE,
		DEFAULT_RD_TEXTURE_BLACK,
		DEFAULT_RD_TEXTURE_TRANSPARENT,
		DEFAULT_RD_TEXTURE_NORMAL,
		DEFAULT_RD_TEXTURE_ANISO,
		DEFAULT_RD_TEXTURE_DEPTH,
		DEFAULT_RD_TEXTURE_MULTIMESH_BUFFER,
		DEFAULT_RD_TEXTURE_CUBEMAP_BLACK,
		DEFAULT_RD_TEXTURE_CUBEMAP_ARRAY_BLACK,
		DEFAULT_RD_TEXTURE_CUBEMAP_WHITE,
		DEFAULT_RD_TEXTURE_CUBEMAP_ARRAY_WHITE,
		DEFAULT_RD_TEXTURE_3D_WHITE,
		DEFAULT_RD_TEXTURE_3D_BLACK,
		DEFAULT_RD_TEXTURE_2D_ARRAY_WHITE,
		DEFAULT_RD_TEXTURE_2D_ARRAY_BLACK,
		DEFAULT_RD_TEXTURE_2D_ARRAY_NORMAL,
		DEFAULT_RD_TEXTURE_2D_ARRAY_DEPTH,
		DEFAULT_RD_TEXTURE_2D_UINT,
		DEFAULT_RD_TEXTURE_VRS,
		DEFAULT_RD_TEXTURE_MAX
	};

	enum TextureType {
		TYPE_2D,
		TYPE_LAYERED,
		TYPE_3D
	};

	struct CanvasTextureInfo {
		RID diffuse;
		RID normal;
		RID specular;
		RID sampler;
		Size2i size;
		Color specular_color;

		bool use_normal = false;
		bool use_specular = false;

		_FORCE_INLINE_ bool is_valid() const { return diffuse.is_valid(); }
		_FORCE_INLINE_ bool is_null() const { return diffuse.is_null(); }
	};

private:
	friend class LightStorage;
	friend class MaterialStorage;

	static TextureStorage *singleton;

	RID default_rd_textures[DEFAULT_RD_TEXTURE_MAX];

	/* Canvas Texture API */

	struct CanvasTextureCache {
		RID diffuse = RID();
		RID normal = RID();
		RID specular = RID();
	};

	class CanvasTexture {
	public:
		RID diffuse;
		RID normal_map;
		RID specular;
		Color specular_color = Color(1, 1, 1, 1);
		float shininess = 1.0;

		RS::CanvasItemTextureFilter texture_filter = RS::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT;
		RS::CanvasItemTextureRepeat texture_repeat = RS::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT;
		CanvasTextureCache info_cache[2];

		Size2i size_cache = Size2i(1, 1);
		bool use_normal_cache = false;
		bool use_specular_cache = false;
		bool cleared_cache = true;

		void clear_cache();
		~CanvasTexture();
	};

	RID_Owner<CanvasTexture, true> canvas_texture_owner;

	/* Texture API */

	struct RenderTarget;

	class Texture {
	public:
		TextureType type;
		RS::TextureLayeredType layered_type = RS::TEXTURE_LAYERED_2D_ARRAY;

		RenderingDevice::TextureType rd_type;
		RID rd_texture;
		RID rd_texture_srgb;
		RenderingDevice::DataFormat rd_format;
		RenderingDevice::DataFormat rd_format_srgb;

		RD::TextureView rd_view;

		Image::Format format;
		Image::Format validated_format;

		int width;
		int height;
		int depth;
		int layers;
		int mipmaps;

		int height_2d;
		int width_2d;

		struct BufferSlice3D {
			Size2i size;
			uint32_t offset = 0;
			uint32_t buffer_size = 0;
		};
		Vector<BufferSlice3D> buffer_slices_3d;
		uint32_t buffer_size_3d = 0;

		RenderTarget *render_target = nullptr;
		bool is_render_target;
		bool is_proxy;

		Ref<Image> image_cache_2d;
		String path;

		RID proxy_to;
		Vector<RID> proxies;

		HashSet<RID> lightmap_users;

		RS::TextureDetectCallback detect_3d_callback = nullptr;
		void *detect_3d_callback_ud = nullptr;

		RS::TextureDetectCallback detect_normal_callback = nullptr;
		void *detect_normal_callback_ud = nullptr;

		RS::TextureDetectRoughnessCallback detect_roughness_callback = nullptr;
		void *detect_roughness_callback_ud = nullptr;

		CanvasTexture *canvas_texture = nullptr;

		void cleanup();
	};

	// Textures can be created from threads, so this RID_Owner is thread safe.
	mutable RID_Owner<Texture, true> texture_owner;
	Texture *get_texture(RID p_rid) { return texture_owner.get_or_null(p_rid); };

	struct TextureToRDFormat {
		RD::DataFormat format;
		RD::DataFormat format_srgb;
		RD::TextureSwizzle swizzle_r;
		RD::TextureSwizzle swizzle_g;
		RD::TextureSwizzle swizzle_b;
		RD::TextureSwizzle swizzle_a;
		TextureToRDFormat() {
			format = RD::DATA_FORMAT_MAX;
			format_srgb = RD::DATA_FORMAT_MAX;
			swizzle_r = RD::TEXTURE_SWIZZLE_R;
			swizzle_g = RD::TEXTURE_SWIZZLE_G;
			swizzle_b = RD::TEXTURE_SWIZZLE_B;
			swizzle_a = RD::TEXTURE_SWIZZLE_A;
		}
	};

	Ref<Image> _validate_texture_format(const Ref<Image> &p_image, TextureToRDFormat &r_format);
	void _texture_2d_update(RID p_texture, const Ref<Image> &p_image, int p_layer = 0, bool p_immediate = false);

	struct TextureFromRDFormat {
		Image::Format image_format;
		RD::DataFormat rd_format;
		RD::DataFormat rd_format_srgb;
		RD::TextureSwizzle swizzle_r;
		RD::TextureSwizzle swizzle_g;
		RD::TextureSwizzle swizzle_b;
		RD::TextureSwizzle swizzle_a;
		TextureFromRDFormat() {
			image_format = Image::FORMAT_MAX;
			rd_format = RD::DATA_FORMAT_MAX;
			rd_format_srgb = RD::DATA_FORMAT_MAX;
			swizzle_r = RD::TEXTURE_SWIZZLE_R;
			swizzle_g = RD::TEXTURE_SWIZZLE_G;
			swizzle_b = RD::TEXTURE_SWIZZLE_B;
			swizzle_a = RD::TEXTURE_SWIZZLE_A;
		}
	};

	void _texture_format_from_rd(RD::DataFormat p_rd_format, TextureFromRDFormat &r_format);

	/* DECAL API */

	struct DecalAtlas {
		struct Texture {
			int panorama_to_dp_users;
			int users;
			Rect2 uv_rect;
		};

		struct SortItem {
			RID texture;
			Size2i pixel_size;
			Size2i size;
			Point2i pos;

			bool operator<(const SortItem &p_item) const {
				//sort larger to smaller
				if (size.height == p_item.size.height) {
					return size.width > p_item.size.width;
				} else {
					return size.height > p_item.size.height;
				}
			}
		};

		HashMap<RID, Texture> textures;
		bool dirty = true;
		int mipmaps = 5;

		RID texture;
		RID texture_srgb;
		struct MipMap {
			RID fb;
			RID texture;
			Size2i size;
		};
		Vector<MipMap> texture_mipmaps;

		Size2i size;
	} decal_atlas;

	struct Decal {
		Vector3 size = Vector3(2, 2, 2);
		RID textures[RS::DECAL_TEXTURE_MAX];
		float emission_energy = 1.0;
		float albedo_mix = 1.0;
		Color modulate = Color(1, 1, 1, 1);
		uint32_t cull_mask = (1 << 20) - 1;
		float upper_fade = 0.3;
		float lower_fade = 0.3;
		bool distance_fade = false;
		float distance_fade_begin = 40.0;
		float distance_fade_length = 10.0;
		float normal_fade = 0.0;

		Dependency dependency;
	};

	mutable RID_Owner<Decal, true> decal_owner;

	/* DECAL INSTANCE */

	struct DecalInstance {
		RID decal;
		Transform3D transform;
		float sorting_offset = 0.0;
		uint32_t cull_mask = 0;
		RendererRD::ForwardID forward_id = -1;
	};

	mutable RID_Owner<DecalInstance> decal_instance_owner;

	/* DECAL DATA (UBO) */

	struct DecalData {
		float xform[16];
		float inv_extents[3];
		float albedo_mix;
		float albedo_rect[4];
		float normal_rect[4];
		float orm_rect[4];
		float emission_rect[4];
		float modulate[4];
		float emission_energy;
		uint32_t mask;
		float upper_fade;
		float lower_fade;
		float normal_xform[12];
		float normal[3];
		float normal_fade;
	};

	struct DecalInstanceSort {
		float depth;
		DecalInstance *decal_instance;
		Decal *decal;
		bool operator<(const DecalInstanceSort &p_sort) const {
			return depth < p_sort.depth;
		}
	};

	uint32_t max_decals = 0;
	uint32_t decal_count = 0;
	DecalData *decals = nullptr;
	DecalInstanceSort *decal_sort = nullptr;
	RID decal_buffer;

	/* RENDER TARGET API */

	struct RenderTarget {
		Size2i size;
		uint32_t view_count;
		RID color;
		Vector<RID> color_slices;
		RID color_multisample; // Needed when 2D MSAA is enabled.

		RS::ViewportMSAA msaa = RS::VIEWPORT_MSAA_DISABLED; // 2D MSAA mode
		bool msaa_needs_resolve = false; // 2D MSAA needs resolved

		//used for retrieving from CPU
		RD::DataFormat color_format = RD::DATA_FORMAT_R4G4_UNORM_PACK8;
		RD::DataFormat color_format_srgb = RD::DATA_FORMAT_R4G4_UNORM_PACK8;
		Image::Format image_format = Image::FORMAT_L8;

		bool is_transparent = false;
		bool use_hdr = false;

		bool sdf_enabled = false;

		RID backbuffer; //used for effects
		RID backbuffer_fb;
		RID backbuffer_mipmap0;

		Vector<RID> backbuffer_mipmaps;

		RID framebuffer_uniform_set;
		RID backbuffer_uniform_set;

		RID sdf_buffer_write;
		RID sdf_buffer_write_fb;
		RID sdf_buffer_process[2];
		RID sdf_buffer_read;
		RID sdf_buffer_process_uniform_sets[2];
		RS::ViewportSDFOversize sdf_oversize = RS::VIEWPORT_SDF_OVERSIZE_120_PERCENT;
		RS::ViewportSDFScale sdf_scale = RS::VIEWPORT_SDF_SCALE_50_PERCENT;
		Size2i process_size;

		// VRS
		RS::ViewportVRSMode vrs_mode = RS::VIEWPORT_VRS_DISABLED;
		RS::ViewportVRSUpdateMode vrs_update_mode = RS::VIEWPORT_VRS_UPDATE_ONCE;
		RID vrs_texture;

		// overridden textures
		struct RTOverridden {
			RID color;
			RID depth;
			RID velocity;

			// In a multiview scenario, which is the most likely where we
			// override our destination textures, we need to obtain slices
			// for each layer of these textures.
			// These are likely changing every frame as we loop through
			// texture chains hence we add a cache to manage these slices.
			// For this we define a key using the RID of the texture and
			// the layer for which we create a slice.
			struct SliceKey {
				RID rid;
				uint32_t layer = 0;

				bool operator==(const SliceKey &p_val) const {
					return (rid == p_val.rid) && (layer == p_val.layer);
				}

				static uint32_t hash(const SliceKey &p_val) {
					uint32_t h = hash_one_uint64(p_val.rid.get_id());
					h = hash_murmur3_one_32(p_val.layer, h);
					return hash_fmix32(h);
				}

				SliceKey() {}
				SliceKey(RID p_rid, uint32_t p_layer) {
					rid = p_rid;
					layer = p_layer;
				}
			};

			mutable HashMap<SliceKey, RID, SliceKey> cached_slices;
		} overridden;

		//texture generated for this owner (nor RD).
		RID texture;
		bool was_used;

		//clear request
		bool clear_requested;
		Color clear_color;

		RID get_framebuffer();
	};

	mutable RID_Owner<RenderTarget> render_target_owner;
	RenderTarget *get_render_target(RID p_rid) const { return render_target_owner.get_or_null(p_rid); };

	void _clear_render_target(RenderTarget *rt);
	void _update_render_target(RenderTarget *rt);
	void _create_render_target_backbuffer(RenderTarget *rt);
	void _render_target_allocate_sdf(RenderTarget *rt);
	void _render_target_clear_sdf(RenderTarget *rt);
	Rect2i _render_target_get_sdf_rect(const RenderTarget *rt) const;

	struct RenderTargetSDF {
		enum {
			SHADER_LOAD,
			SHADER_LOAD_SHRINK,
			SHADER_PROCESS,
			SHADER_PROCESS_OPTIMIZED,
			SHADER_STORE,
			SHADER_STORE_SHRINK,
			SHADER_MAX
		};

		struct PushConstant {
			int32_t size[2];
			int32_t stride;
			int32_t shift;
			int32_t base_size[2];
			int32_t pad[2];
		};

		CanvasSdfShaderRD shader;
		RID shader_version;
		RID pipelines[SHADER_MAX];
	} rt_sdf;

public:
	static TextureStorage *get_singleton();

	_FORCE_INLINE_ RID texture_rd_get_default(DefaultRDTexture p_texture) {
		return default_rd_textures[p_texture];
	}

	TextureStorage();
	virtual ~TextureStorage();

	bool free(RID p_rid);

	/* Canvas Texture API */

	bool owns_canvas_texture(RID p_rid) { return canvas_texture_owner.owns(p_rid); };

	virtual RID canvas_texture_allocate() override;
	virtual void canvas_texture_initialize(RID p_rid) override;
	virtual void canvas_texture_free(RID p_rid) override;

	virtual void canvas_texture_set_channel(RID p_canvas_texture, RS::CanvasTextureChannel p_channel, RID p_texture) override;
	virtual void canvas_texture_set_shading_parameters(RID p_canvas_texture, const Color &p_base_color, float p_shininess) override;

	virtual void canvas_texture_set_texture_filter(RID p_item, RS::CanvasItemTextureFilter p_filter) override;
	virtual void canvas_texture_set_texture_repeat(RID p_item, RS::CanvasItemTextureRepeat p_repeat) override;

	CanvasTextureInfo canvas_texture_get_info(RID p_texture, RS::CanvasItemTextureFilter p_base_filter, RS::CanvasItemTextureRepeat p_base_repeat, bool p_use_srgb, bool p_texture_is_data);

	/* Texture API */

	bool owns_texture(RID p_rid) const { return texture_owner.owns(p_rid); };

	virtual RID texture_allocate() override;
	virtual void texture_free(RID p_rid) override;

	virtual void texture_2d_initialize(RID p_texture, const Ref<Image> &p_image) override;
	virtual void texture_2d_layered_initialize(RID p_texture, const Vector<Ref<Image>> &p_layers, RS::TextureLayeredType p_layered_type) override;
	virtual void texture_3d_initialize(RID p_texture, Image::Format, int p_width, int p_height, int p_depth, bool p_mipmaps, const Vector<Ref<Image>> &p_data) override;
	virtual void texture_external_initialize(RID p_texture, int p_width, int p_height, uint64_t p_external_buffer) override;
	virtual void texture_proxy_initialize(RID p_texture, RID p_base) override; //all slices, then all the mipmaps, must be coherent

	virtual RID texture_create_from_native_handle(RS::TextureType p_type, Image::Format p_format, uint64_t p_native_handle, int p_width, int p_height, int p_depth, int p_layers = 1, RS::TextureLayeredType p_layered_type = RS::TEXTURE_LAYERED_2D_ARRAY) override;

	virtual void texture_2d_update(RID p_texture, const Ref<Image> &p_image, int p_layer = 0) override;
	virtual void texture_3d_update(RID p_texture, const Vector<Ref<Image>> &p_data) override;
	virtual void texture_external_update(RID p_texture, int p_width, int p_height, uint64_t p_external_buffer) override;
	virtual void texture_proxy_update(RID p_proxy, RID p_base) override;

	Ref<Image> texture_2d_placeholder;
	Vector<Ref<Image>> texture_2d_array_placeholder;
	Vector<Ref<Image>> cubemap_placeholder;
	Vector<Ref<Image>> texture_3d_placeholder;

	//these two APIs can be used together or in combination with the others.
	virtual void texture_2d_placeholder_initialize(RID p_texture) override;
	virtual void texture_2d_layered_placeholder_initialize(RID p_texture, RenderingServer::TextureLayeredType p_layered_type) override;
	virtual void texture_3d_placeholder_initialize(RID p_texture) override;

	virtual Ref<Image> texture_2d_get(RID p_texture) const override;
	virtual Ref<Image> texture_2d_layer_get(RID p_texture, int p_layer) const override;
	virtual Vector<Ref<Image>> texture_3d_get(RID p_texture) const override;

	virtual void texture_replace(RID p_texture, RID p_by_texture) override;
	virtual void texture_set_size_override(RID p_texture, int p_width, int p_height) override;

	virtual void texture_set_path(RID p_texture, const String &p_path) override;
	virtual String texture_get_path(RID p_texture) const override;

	virtual Image::Format texture_get_format(RID p_texture) const override;

	virtual void texture_set_detect_3d_callback(RID p_texture, RS::TextureDetectCallback p_callback, void *p_userdata) override;
	virtual void texture_set_detect_normal_callback(RID p_texture, RS::TextureDetectCallback p_callback, void *p_userdata) override;
	virtual void texture_set_detect_roughness_callback(RID p_texture, RS::TextureDetectRoughnessCallback p_callback, void *p_userdata) override;

	virtual void texture_debug_usage(List<RS::TextureInfo> *r_info) override;

	virtual void texture_set_force_redraw_if_visible(RID p_texture, bool p_enable) override;

	virtual Size2 texture_size_with_proxy(RID p_proxy) override;

	virtual void texture_rd_initialize(RID p_texture, const RID &p_rd_texture, const RS::TextureLayeredType p_layer_type = RS::TEXTURE_LAYERED_2D_ARRAY) override;
	virtual RID texture_get_rd_texture(RID p_texture, bool p_srgb = false) const override;
	virtual uint64_t texture_get_native_handle(RID p_texture, bool p_srgb = false) const override;

	//internal usage
	_FORCE_INLINE_ TextureType texture_get_type(RID p_texture) {
		RendererRD::TextureStorage::Texture *tex = texture_owner.get_or_null(p_texture);
		if (tex == nullptr) {
			return TYPE_2D;
		}

		return tex->type;
	}

	_FORCE_INLINE_ int texture_get_layers(RID p_texture) {
		RendererRD::TextureStorage::Texture *tex = texture_owner.get_or_null(p_texture);
		if (tex == nullptr) {
			return 1;
		}

		return tex->layers;
	}

	_FORCE_INLINE_ Size2i texture_2d_get_size(RID p_texture) {
		if (p_texture.is_null()) {
			return Size2i();
		}
		RendererRD::TextureStorage::Texture *tex = texture_owner.get_or_null(p_texture);

		if (!tex) {
			return Size2i();
		}
		return Size2i(tex->width_2d, tex->height_2d);
	}

	/* DECAL API */

	void update_decal_atlas();

	bool owns_decal(RID p_rid) const { return decal_owner.owns(p_rid); };

	RID decal_atlas_get_texture() const;
	RID decal_atlas_get_texture_srgb() const;
	_FORCE_INLINE_ Rect2 decal_atlas_get_texture_rect(RID p_texture) {
		DecalAtlas::Texture *t = decal_atlas.textures.getptr(p_texture);
		if (!t) {
			return Rect2();
		}

		return t->uv_rect;
	}

	virtual RID decal_allocate() override;
	virtual void decal_initialize(RID p_decal) override;
	virtual void decal_free(RID p_rid) override;

	virtual void decal_set_size(RID p_decal, const Vector3 &p_size) override;
	virtual void decal_set_texture(RID p_decal, RS::DecalTexture p_type, RID p_texture) override;
	virtual void decal_set_emission_energy(RID p_decal, float p_energy) override;
	virtual void decal_set_albedo_mix(RID p_decal, float p_mix) override;
	virtual void decal_set_modulate(RID p_decal, const Color &p_modulate) override;
	virtual void decal_set_cull_mask(RID p_decal, uint32_t p_layers) override;
	virtual void decal_set_distance_fade(RID p_decal, bool p_enabled, float p_begin, float p_length) override;
	virtual void decal_set_fade(RID p_decal, float p_above, float p_below) override;
	virtual void decal_set_normal_fade(RID p_decal, float p_fade) override;

	void decal_atlas_mark_dirty_on_texture(RID p_texture);
	void decal_atlas_remove_texture(RID p_texture);

	virtual void texture_add_to_decal_atlas(RID p_texture, bool p_panorama_to_dp = false) override;
	virtual void texture_remove_from_decal_atlas(RID p_texture, bool p_panorama_to_dp = false) override;

	_FORCE_INLINE_ Vector3 decal_get_size(RID p_decal) {
		const Decal *decal = decal_owner.get_or_null(p_decal);
		return decal->size;
	}

	_FORCE_INLINE_ RID decal_get_texture(RID p_decal, RS::DecalTexture p_texture) {
		const Decal *decal = decal_owner.get_or_null(p_decal);
		return decal->textures[p_texture];
	}

	_FORCE_INLINE_ Color decal_get_modulate(RID p_decal) {
		const Decal *decal = decal_owner.get_or_null(p_decal);
		return decal->modulate;
	}

	_FORCE_INLINE_ float decal_get_emission_energy(RID p_decal) {
		const Decal *decal = decal_owner.get_or_null(p_decal);
		return decal->emission_energy;
	}

	_FORCE_INLINE_ float decal_get_albedo_mix(RID p_decal) {
		const Decal *decal = decal_owner.get_or_null(p_decal);
		return decal->albedo_mix;
	}

	_FORCE_INLINE_ uint32_t decal_get_cull_mask(RID p_decal) {
		const Decal *decal = decal_owner.get_or_null(p_decal);
		return decal->cull_mask;
	}

	_FORCE_INLINE_ float decal_get_upper_fade(RID p_decal) {
		const Decal *decal = decal_owner.get_or_null(p_decal);
		return decal->upper_fade;
	}

	_FORCE_INLINE_ float decal_get_lower_fade(RID p_decal) {
		const Decal *decal = decal_owner.get_or_null(p_decal);
		return decal->lower_fade;
	}

	_FORCE_INLINE_ float decal_get_normal_fade(RID p_decal) {
		const Decal *decal = decal_owner.get_or_null(p_decal);
		return decal->normal_fade;
	}

	_FORCE_INLINE_ bool decal_is_distance_fade_enabled(RID p_decal) {
		const Decal *decal = decal_owner.get_or_null(p_decal);
		return decal->distance_fade;
	}

	_FORCE_INLINE_ float decal_get_distance_fade_begin(RID p_decal) {
		const Decal *decal = decal_owner.get_or_null(p_decal);
		return decal->distance_fade_begin;
	}

	_FORCE_INLINE_ float decal_get_distance_fade_length(RID p_decal) {
		const Decal *decal = decal_owner.get_or_null(p_decal);
		return decal->distance_fade_length;
	}

	virtual AABB decal_get_aabb(RID p_decal) const override;
	virtual uint32_t decal_get_cull_mask(RID p_decal) const override;
	Dependency *decal_get_dependency(RID p_decal);

	/* DECAL INSTANCE API */

	bool owns_decal_instance(RID p_rid) const { return decal_instance_owner.owns(p_rid); }

	virtual RID decal_instance_create(RID p_decal) override;
	virtual void decal_instance_free(RID p_decal_instance) override;
	virtual void decal_instance_set_transform(RID p_decal_instance, const Transform3D &p_transform) override;
	virtual void decal_instance_set_sorting_offset(RID p_decal_instance, float p_sorting_offset) override;

	_FORCE_INLINE_ RID decal_instance_get_base(RID p_decal_instance) const {
		DecalInstance *di = decal_instance_owner.get_or_null(p_decal_instance);
		return di->decal;
	}

	_FORCE_INLINE_ RendererRD::ForwardID decal_instance_get_forward_id(RID p_decal_instance) const {
		DecalInstance *di = decal_instance_owner.get_or_null(p_decal_instance);
		return di->forward_id;
	}

	_FORCE_INLINE_ Transform3D decal_instance_get_transform(RID p_decal_instance) const {
		DecalInstance *di = decal_instance_owner.get_or_null(p_decal_instance);
		return di->transform;
	}

	_FORCE_INLINE_ ForwardID decal_instance_get_forward_id(RID p_decal_instance) {
		DecalInstance *di = decal_instance_owner.get_or_null(p_decal_instance);
		return di->forward_id;
	}

	_FORCE_INLINE_ void decal_instance_set_cullmask(RID p_decal_instance, uint32_t p_cull_mask) const {
		DecalInstance *di = decal_instance_owner.get_or_null(p_decal_instance);
		di->cull_mask = p_cull_mask;
	}

	/* DECAL DATA API */

	void free_decal_data();
	void set_max_decals(const uint32_t p_max_decals);
	RID get_decal_buffer() { return decal_buffer; }
	void update_decal_buffer(const PagedArray<RID> &p_decals, const Transform3D &p_camera_xform);

	/* RENDER TARGET API */

	bool owns_render_target(RID p_rid) const { return render_target_owner.owns(p_rid); };

	virtual RID render_target_create() override;
	virtual void render_target_free(RID p_rid) override;

	virtual void render_target_set_position(RID p_render_target, int p_x, int p_y) override;
	virtual Point2i render_target_get_position(RID p_render_target) const override;
	virtual void render_target_set_size(RID p_render_target, int p_width, int p_height, uint32_t p_view_count) override;
	virtual Size2i render_target_get_size(RID p_render_target) const override;
	virtual void render_target_set_transparent(RID p_render_target, bool p_is_transparent) override;
	virtual bool render_target_get_transparent(RID p_render_target) const override;
	virtual void render_target_set_direct_to_screen(RID p_render_target, bool p_direct_to_screen) override;
	virtual bool render_target_get_direct_to_screen(RID p_render_target) const override;
	virtual bool render_target_was_used(RID p_render_target) const override;
	virtual void render_target_set_as_unused(RID p_render_target) override;
	virtual void render_target_set_msaa(RID p_render_target, RS::ViewportMSAA p_msaa) override;
	virtual RS::ViewportMSAA render_target_get_msaa(RID p_render_target) const override;
	virtual void render_target_set_msaa_needs_resolve(RID p_render_target, bool p_needs_resolve) override;
	virtual bool render_target_get_msaa_needs_resolve(RID p_render_target) const override;
	virtual void render_target_do_msaa_resolve(RID p_render_target) override;
	virtual void render_target_set_use_hdr(RID p_render_target, bool p_use_hdr) override;
	virtual bool render_target_is_using_hdr(RID p_render_target) const override;

	void render_target_copy_to_back_buffer(RID p_render_target, const Rect2i &p_region, bool p_gen_mipmaps);
	void render_target_clear_back_buffer(RID p_render_target, const Rect2i &p_region, const Color &p_color);
	void render_target_gen_back_buffer_mipmaps(RID p_render_target, const Rect2i &p_region);
	RID render_target_get_back_buffer_uniform_set(RID p_render_target, RID p_base_shader);

	virtual void render_target_request_clear(RID p_render_target, const Color &p_clear_color) override;
	virtual bool render_target_is_clear_requested(RID p_render_target) override;
	virtual Color render_target_get_clear_request_color(RID p_render_target) override;
	virtual void render_target_disable_clear_request(RID p_render_target) override;
	virtual void render_target_do_clear_request(RID p_render_target) override;

	virtual void render_target_set_sdf_size_and_scale(RID p_render_target, RS::ViewportSDFOversize p_size, RS::ViewportSDFScale p_scale) override;
	RID render_target_get_sdf_texture(RID p_render_target);
	RID render_target_get_sdf_framebuffer(RID p_render_target);
	void render_target_sdf_process(RID p_render_target);
	virtual Rect2i render_target_get_sdf_rect(RID p_render_target) const override;
	virtual void render_target_mark_sdf_enabled(RID p_render_target, bool p_enabled) override;
	bool render_target_is_sdf_enabled(RID p_render_target) const;

	virtual void render_target_set_vrs_mode(RID p_render_target, RS::ViewportVRSMode p_mode) override;
	virtual RS::ViewportVRSMode render_target_get_vrs_mode(RID p_render_target) const override;
	virtual void render_target_set_vrs_update_mode(RID p_render_target, RS::ViewportVRSUpdateMode p_mode) override;
	virtual RS::ViewportVRSUpdateMode render_target_get_vrs_update_mode(RID p_render_target) const override;
	virtual void render_target_set_vrs_texture(RID p_render_target, RID p_texture) override;
	virtual RID render_target_get_vrs_texture(RID p_render_target) const override;

	virtual void render_target_set_override(RID p_render_target, RID p_color_texture, RID p_depth_texture, RID p_velocity_texture) override;
	virtual RID render_target_get_override_color(RID p_render_target) const override;
	virtual RID render_target_get_override_depth(RID p_render_target) const override;
	RID render_target_get_override_depth_slice(RID p_render_target, const uint32_t p_layer) const;
	virtual RID render_target_get_override_velocity(RID p_render_target) const override;
	RID render_target_get_override_velocity_slice(RID p_render_target, const uint32_t p_layer) const;

	virtual RID render_target_get_texture(RID p_render_target) override;

	RID render_target_get_rd_framebuffer(RID p_render_target);
	RID render_target_get_rd_texture(RID p_render_target);
	RID render_target_get_rd_texture_slice(RID p_render_target, uint32_t p_layer);
	RID render_target_get_rd_texture_msaa(RID p_render_target);
	RID render_target_get_rd_backbuffer(RID p_render_target);
	RID render_target_get_rd_backbuffer_framebuffer(RID p_render_target);

	RID render_target_get_framebuffer_uniform_set(RID p_render_target);
	RID render_target_get_backbuffer_uniform_set(RID p_render_target);

	void render_target_set_framebuffer_uniform_set(RID p_render_target, RID p_uniform_set);
	void render_target_set_backbuffer_uniform_set(RID p_render_target, RID p_uniform_set);

	static RD::DataFormat render_target_get_color_format(bool p_use_hdr, bool p_srgb);
	static uint32_t render_target_get_color_usage_bits(bool p_msaa);
};

} // namespace RendererRD

#endif // TEXTURE_STORAGE_RD_H
