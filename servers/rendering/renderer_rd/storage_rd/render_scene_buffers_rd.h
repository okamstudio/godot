/**************************************************************************/
/*  render_scene_buffers_rd.h                                             */
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

#ifndef RENDER_SCENE_BUFFERS_RD_H
#define RENDER_SCENE_BUFFERS_RD_H

#include "../effects/vrs.h"
#include "core/templates/hash_map.h"
#include "material_storage.h"
#include "render_buffer_custom_data_rd.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_device_binds.h"
#include "servers/rendering/storage/render_scene_buffers.h"

#define RB_SCOPE_BUFFERS SNAME("render_buffers")
#define RB_SCOPE_VRS SNAME("VRS")

#define RB_TEXTURE SNAME("texture")
#define RB_TEX_COLOR SNAME("color")
#define RB_TEX_COLOR_MSAA SNAME("color_msaa")
#define RB_TEX_COLOR_UPSCALED SNAME("color_upscaled")
#define RB_TEX_DEPTH SNAME("depth")
#define RB_TEX_DEPTH_MSAA SNAME("depth_msaa")
#define RB_TEX_VELOCITY SNAME("velocity")
#define RB_TEX_VELOCITY_MSAA SNAME("velocity_msaa")

#define RB_TEX_BLUR_0 SNAME("blur_0")
#define RB_TEX_BLUR_1 SNAME("blur_1")
#define RB_TEX_HALF_BLUR SNAME("half_blur") // only for raster!

#define RB_TEX_BACK_COLOR SNAME("back_color")
#define RB_TEX_BACK_DEPTH SNAME("back_depth")

class RenderSceneBuffersRD : public RenderSceneBuffers {
	GDCLASS(RenderSceneBuffersRD, RenderSceneBuffers);

private:
	bool can_be_storage = true;
	uint32_t max_cluster_elements = 512;
	RD::DataFormat base_data_format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
	RendererRD::VRS *vrs = nullptr;
	uint64_t auto_exposure_version = 1;

	// Our render target represents our final destination that we display on screen.
	RID render_target;
	Size2i target_size = Size2i(0, 0);
	uint32_t view_count = 1;

	// The internal size of the textures we render 3D to in case we render at a lower resolution and upscale
	Size2i internal_size = Size2i(0, 0);
	RS::ViewportScaling3DMode scaling_3d_mode = RS::VIEWPORT_SCALING_3D_MODE_OFF;
	float fsr_sharpness = 0.2f;
	float texture_mipmap_bias = 0.0f;
	RS::ViewportAnisotropicFiltering anisotropic_filtering_level = RS::VIEWPORT_ANISOTROPY_4X;

	// Aliassing settings
	RS::ViewportMSAA msaa_3d = RS::VIEWPORT_MSAA_DISABLED;
	RS::ViewportScreenSpaceAA screen_space_aa = RS::VIEWPORT_SCREEN_SPACE_AA_DISABLED;
	bool use_taa = false;
	bool use_debanding = false;
	RD::TextureSamples texture_samples = RD::TEXTURE_SAMPLES_1;

	// Named Textures

	struct NTKey {
		StringName context;
		StringName buffer_name;

		bool operator==(const NTKey &p_val) const {
			return (context == p_val.context) && (buffer_name == p_val.buffer_name);
		}

		static uint32_t hash(const NTKey &p_val) {
			uint32_t h = p_val.context.hash();
			h = hash_murmur3_one_32(p_val.buffer_name.hash(), h);
			return hash_fmix32(h);
		}

		NTKey() {}
		NTKey(const StringName &p_context, const StringName &p_texture_name) {
			context = p_context;
			buffer_name = p_texture_name;
		}
	};

	struct NTSliceKey {
		uint32_t layer;
		uint32_t layers;
		uint32_t mipmap;
		uint32_t mipmaps;
		RD::TextureView texture_view;

		bool operator==(const NTSliceKey &p_val) const {
			return (layer == p_val.layer) && (layers == p_val.layers) && (mipmap == p_val.mipmap) && (mipmaps == p_val.mipmaps) && (texture_view == p_val.texture_view);
		}

		static uint32_t hash(const NTSliceKey &p_val) {
			uint32_t h = hash_murmur3_one_32(p_val.layer);
			h = hash_murmur3_one_32(p_val.layers, h);
			h = hash_murmur3_one_32(p_val.mipmap, h);
			h = hash_murmur3_one_32(p_val.mipmaps, h);
			h = hash_murmur3_one_32(p_val.texture_view.format_override, h);
			h = hash_murmur3_one_32(p_val.texture_view.swizzle_r, h);
			h = hash_murmur3_one_32(p_val.texture_view.swizzle_g, h);
			h = hash_murmur3_one_32(p_val.texture_view.swizzle_b, h);
			h = hash_murmur3_one_32(p_val.texture_view.swizzle_a, h);
			return hash_fmix32(h);
		}

		NTSliceKey() {}
		NTSliceKey(uint32_t p_layer, uint32_t p_layers, uint32_t p_mipmap, uint32_t p_mipmaps, RD::TextureView p_texture_view) {
			layer = p_layer;
			layers = p_layers;
			mipmap = p_mipmap;
			mipmaps = p_mipmaps;
			texture_view = p_texture_view;
		}
	};

	struct NamedTexture {
		// Cache the data used to create our texture
		RD::TextureFormat format;
		bool is_unique; // If marked as unique, we return it into our pool

		// Our texture objects, slices are lazy (i.e. only created when requested).
		RID texture;
		mutable HashMap<NTSliceKey, RID, NTSliceKey> slices;
		Vector<Size2i> sizes;
	};

	mutable HashMap<NTKey, NamedTexture, NTKey> named_textures;
	void update_sizes(NamedTexture &p_named_texture);
	void free_named_texture(NamedTexture &p_named_texture);

	// Data buffers
	mutable HashMap<StringName, Ref<RenderBufferCustomDataRD>> data_buffers;

	// Samplers.
	RendererRD::MaterialStorage::Samplers samplers;

	void update_samplers();

protected:
	static void _bind_methods();

public:
	RenderSceneBuffersRD();
	virtual ~RenderSceneBuffersRD();

	// info from our renderer
	void set_can_be_storage(const bool p_can_be_storage) { can_be_storage = p_can_be_storage; }
	bool get_can_be_storage() const { return can_be_storage; }
	void set_max_cluster_elements(const uint32_t p_max_elements) { max_cluster_elements = p_max_elements; }
	uint32_t get_max_cluster_elements() { return max_cluster_elements; }
	void set_base_data_format(const RD::DataFormat p_base_data_format) { base_data_format = p_base_data_format; }
	RD::DataFormat get_base_data_format() const { return base_data_format; }
	void set_vrs(RendererRD::VRS *p_vrs) { vrs = p_vrs; }

	void cleanup();
	virtual void configure(const RenderSceneBuffersConfiguration *p_config) override;
	void configure_for_reflections(const Size2i p_reflection_size);
	virtual void set_fsr_sharpness(float p_fsr_sharpness) override;
	virtual void set_texture_mipmap_bias(float p_texture_mipmap_bias) override;
	virtual void set_anisotropic_filtering_level(RS::ViewportAnisotropicFiltering p_anisotropic_filtering_level) override;
	virtual void set_use_debanding(bool p_use_debanding) override;

	// Named Textures

	bool has_texture(const StringName &p_context, const StringName &p_texture_name) const;
	RID create_texture(const StringName &p_context, const StringName &p_texture_name, const RD::DataFormat p_data_format, const uint32_t p_usage_bits, const RD::TextureSamples p_texture_samples = RD::TEXTURE_SAMPLES_1, const Size2i p_size = Size2i(0, 0), const uint32_t p_layers = 0, const uint32_t p_mipmaps = 1, bool p_unique = true, bool p_discardable = false);
	RID create_texture_from_format(const StringName &p_context, const StringName &p_texture_name, const RD::TextureFormat &p_texture_format, RD::TextureView p_view = RD::TextureView(), bool p_unique = true);
	RID create_texture_view(const StringName &p_context, const StringName &p_texture_name, const StringName &p_view_name, RD::TextureView p_view = RD::TextureView());
	RID get_texture(const StringName &p_context, const StringName &p_texture_name) const;
	const RD::TextureFormat get_texture_format(const StringName &p_context, const StringName &p_texture_name) const;
	RID get_texture_slice(const StringName &p_context, const StringName &p_texture_name, const uint32_t p_layer, const uint32_t p_mipmap, const uint32_t p_layers = 1, const uint32_t p_mipmaps = 1);
	RID get_texture_slice_view(const StringName &p_context, const StringName &p_texture_name, const uint32_t p_layer, const uint32_t p_mipmap, const uint32_t p_layers = 1, const uint32_t p_mipmaps = 1, RD::TextureView p_view = RD::TextureView());
	Size2i get_texture_slice_size(const StringName &p_context, const StringName &p_texture_name, const uint32_t p_mipmap);

	void clear_context(const StringName &p_context);

	// Allocate shared buffers
	void allocate_blur_textures();

	// Custom data
	bool has_custom_data(const StringName &p_name);
	void set_custom_data(const StringName &p_name, Ref<RenderBufferCustomDataRD> p_data);
	Ref<RenderBufferCustomDataRD> get_custom_data(const StringName &p_name) const;

	// Getters

	_FORCE_INLINE_ RID get_render_target() const { return render_target; }
	_FORCE_INLINE_ uint32_t get_view_count() const { return view_count; }
	_FORCE_INLINE_ Size2i get_internal_size() const { return internal_size; }
	_FORCE_INLINE_ Size2i get_target_size() const { return target_size; }
	_FORCE_INLINE_ RS::ViewportScaling3DMode get_scaling_3d_mode() const { return scaling_3d_mode; }
	_FORCE_INLINE_ float get_fsr_sharpness() const { return fsr_sharpness; }
	_FORCE_INLINE_ RS::ViewportMSAA get_msaa_3d() const { return msaa_3d; }
	_FORCE_INLINE_ RD::TextureSamples get_texture_samples() const { return texture_samples; }
	_FORCE_INLINE_ RS::ViewportScreenSpaceAA get_screen_space_aa() const { return screen_space_aa; }
	_FORCE_INLINE_ bool get_use_taa() const { return use_taa; }
	_FORCE_INLINE_ bool get_use_debanding() const { return use_debanding; }

	uint64_t get_auto_exposure_version() const { return auto_exposure_version; }
	void set_auto_exposure_version(const uint64_t p_auto_exposure_version) { auto_exposure_version = p_auto_exposure_version; }

	// For our internal textures we provide some easy access methods.

	_FORCE_INLINE_ bool has_internal_texture() const {
		return has_texture(RB_SCOPE_BUFFERS, RB_TEX_COLOR);
	}
	_FORCE_INLINE_ RID get_internal_texture() const {
		return get_texture(RB_SCOPE_BUFFERS, RB_TEX_COLOR);
	}
	_FORCE_INLINE_ RID get_internal_texture(const uint32_t p_layer) {
		return get_texture_slice(RB_SCOPE_BUFFERS, RB_TEX_COLOR, p_layer, 0);
	}
	_FORCE_INLINE_ RID get_internal_texture_reactive(const uint32_t p_layer) {
		RD::TextureView alpha_only_view;
		alpha_only_view.swizzle_r = RD::TEXTURE_SWIZZLE_A;
		alpha_only_view.swizzle_g = RD::TEXTURE_SWIZZLE_A;
		alpha_only_view.swizzle_b = RD::TEXTURE_SWIZZLE_A;
		alpha_only_view.swizzle_a = RD::TEXTURE_SWIZZLE_A;
		return get_texture_slice_view(RB_SCOPE_BUFFERS, RB_TEX_COLOR, p_layer, 0, 1, 1, alpha_only_view);
	}
	_FORCE_INLINE_ RID get_color_msaa() const {
		return get_texture(RB_SCOPE_BUFFERS, RB_TEX_COLOR_MSAA);
	}
	_FORCE_INLINE_ RID get_color_msaa(uint32_t p_layer) {
		return get_texture_slice(RB_SCOPE_BUFFERS, RB_TEX_COLOR_MSAA, p_layer, 0);
	}

	bool has_depth_texture();
	RID get_depth_texture();
	RID get_depth_texture(const uint32_t p_layer);

	RID get_depth_msaa() const {
		return get_texture(RB_SCOPE_BUFFERS, RB_TEX_DEPTH_MSAA);
	}
	RID get_depth_msaa(uint32_t p_layer) {
		return get_texture_slice(RB_SCOPE_BUFFERS, RB_TEX_DEPTH_MSAA, p_layer, 0);
	}

	// back buffer (color)
	RID get_back_buffer_texture() const {
		// Prefer returning the dedicated backbuffer color texture if it was created. Return the reused blur texture otherwise.
		if (has_texture(RB_SCOPE_BUFFERS, RB_TEX_BACK_COLOR)) {
			return get_texture(RB_SCOPE_BUFFERS, RB_TEX_BACK_COLOR);
		} else if (has_texture(RB_SCOPE_BUFFERS, RB_TEX_BLUR_0)) {
			return get_texture(RB_SCOPE_BUFFERS, RB_TEX_BLUR_0);
		} else {
			return RID();
		}
	}

	// Upscaled.
	void ensure_upscaled();

	_FORCE_INLINE_ bool has_upscaled_texture() const {
		return has_texture(RB_SCOPE_BUFFERS, RB_TEX_COLOR_UPSCALED);
	}
	_FORCE_INLINE_ RID get_upscaled_texture() const {
		return get_texture(RB_SCOPE_BUFFERS, RB_TEX_COLOR_UPSCALED);
	}
	_FORCE_INLINE_ RID get_upscaled_texture(const uint32_t p_layer) {
		return get_texture_slice(RB_SCOPE_BUFFERS, RB_TEX_COLOR_UPSCALED, p_layer, 0);
	}

	// Velocity, currently only used by TAA (Clustered) but we'll be using this in other places soon too.

	void ensure_velocity();
	bool has_velocity_buffer(bool p_has_msaa);
	RID get_velocity_buffer(bool p_get_msaa);
	RID get_velocity_buffer(bool p_get_msaa, uint32_t p_layer);

	// Samplers adjusted with the mipmap bias that is best fit for the configuration of these render buffers.

	_FORCE_INLINE_ RendererRD::MaterialStorage::Samplers get_samplers() const {
		return samplers;
	}

	_FORCE_INLINE_ static RD::TextureSamples msaa_to_samples(RS::ViewportMSAA p_msaa) {
		switch (p_msaa) {
			case RS::VIEWPORT_MSAA_DISABLED:
				return RD::TEXTURE_SAMPLES_1;
			case RS::VIEWPORT_MSAA_2X:
				return RD::TEXTURE_SAMPLES_2;
			case RS::VIEWPORT_MSAA_4X:
				return RD::TEXTURE_SAMPLES_4;
			case RS::VIEWPORT_MSAA_8X:
				return RD::TEXTURE_SAMPLES_8;
			default:
				DEV_ASSERT(false && "Unknown MSAA option.");
				return RD::TEXTURE_SAMPLES_1;
		}
	}

	static uint32_t get_color_usage_bits(bool p_resolve, bool p_msaa, bool p_storage);
	static RD::DataFormat get_depth_format(bool p_resolve, bool p_msaa, bool p_storage);
	static uint32_t get_depth_usage_bits(bool p_resolve, bool p_msaa, bool p_storage);
	static RD::DataFormat get_velocity_format();
	static uint32_t get_velocity_usage_bits(bool p_resolve, bool p_msaa, bool p_storage);
	static RD::DataFormat get_vrs_format();
	static uint32_t get_vrs_usage_bits();

private:
	////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Our classDB doesn't support calling our normal exposed functions

	RID _create_texture_from_format(const StringName &p_context, const StringName &p_texture_name, const Ref<RDTextureFormat> &p_texture_format, const Ref<RDTextureView> &p_view = Ref<RDTextureView>(), bool p_unique = true);
	RID _create_texture_view(const StringName &p_context, const StringName &p_texture_name, const StringName &p_view_name, const Ref<RDTextureView> p_view = Ref<RDTextureView>());
	Ref<RDTextureFormat> _get_texture_format(const StringName &p_context, const StringName &p_texture_name) const;
	RID _get_texture_slice_view(const StringName &p_context, const StringName &p_texture_name, const uint32_t p_layer, const uint32_t p_mipmap, const uint32_t p_layers = 1, const uint32_t p_mipmaps = 1, const Ref<RDTextureView> p_view = Ref<RDTextureView>());

	// For color and depth as exposed to extensions:
	// - we need separately named functions to access the layer,
	// - we don't output an error for missing buffers but just return an empty RID.
	RID _get_color_texture(bool p_msaa = false) {
		if (p_msaa) {
			if (has_texture(RB_SCOPE_BUFFERS, RB_TEX_COLOR_MSAA)) {
				return get_texture(RB_SCOPE_BUFFERS, RB_TEX_COLOR_MSAA);
			} else {
				return RID();
			}
		} else if (has_internal_texture()) {
			return get_internal_texture();
		} else {
			return RID();
		}
	}

	RID _get_color_layer(const uint32_t p_layer, bool p_msaa = false) {
		if (p_msaa) {
			if (has_texture(RB_SCOPE_BUFFERS, RB_TEX_COLOR_MSAA)) {
				return get_texture_slice(RB_SCOPE_BUFFERS, RB_TEX_COLOR_MSAA, p_layer, 0);
			} else {
				return RID();
			}
		} else if (has_internal_texture()) {
			return get_internal_texture(p_layer);
		} else {
			return RID();
		}
	}

	RID _get_depth_texture(bool p_msaa = false) {
		if (p_msaa) {
			if (has_texture(RB_SCOPE_BUFFERS, RB_TEX_DEPTH_MSAA)) {
				return get_texture(RB_SCOPE_BUFFERS, RB_TEX_DEPTH_MSAA);
			} else {
				return RID();
			}
		} else if (has_depth_texture()) {
			return get_depth_texture();
		} else {
			return RID();
		}
	}

	RID _get_depth_layer(const uint32_t p_layer, bool p_msaa = false) {
		if (p_msaa) {
			if (has_texture(RB_SCOPE_BUFFERS, RB_TEX_DEPTH_MSAA)) {
				return get_texture_slice(RB_SCOPE_BUFFERS, RB_TEX_DEPTH_MSAA, p_layer, 0);
			} else {
				return RID();
			}
		} else if (has_depth_texture()) {
			return get_depth_texture(p_layer);
		} else {
			return RID();
		}
	}

	RID _get_velocity_texture(bool p_msaa = false) {
		if (has_velocity_buffer(p_msaa)) {
			return get_velocity_buffer(p_msaa);
		} else {
			return RID();
		}
	}

	RID _get_velocity_layer(const uint32_t p_layer, bool p_msaa = false) {
		if (has_velocity_buffer(p_msaa)) {
			return get_velocity_buffer(p_msaa, p_layer);
		} else {
			return RID();
		}
	}

#ifndef DISABLE_DEPRECATED

	RID _get_color_texture_compat_80214();
	RID _get_color_layer_compat_80214(const uint32_t p_layer);
	RID _get_depth_texture_compat_80214();
	RID _get_depth_layer_compat_80214(const uint32_t p_layer);
	RID _get_velocity_texture_compat_80214();
	RID _get_velocity_layer_compat_80214(const uint32_t p_layer);

	RID _create_texture_bind_compat_98670(const StringName &p_context, const StringName &p_texture_name, const RD::DataFormat p_data_format, const uint32_t p_usage_bits, const RD::TextureSamples p_texture_samples, const Size2i p_size, const uint32_t p_layers, const uint32_t p_mipmaps, bool p_unique);

	static void _bind_compatibility_methods();

#endif // DISABLE_DEPRECATED

	////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Everything after this needs to be re-evaluated, this is all old implementation
public:
	struct WeightBuffers {
		RID weight;
		RID fb; // FB with both texture and weight writing into one level lower
	};

	// 2 full size, 2 half size
	WeightBuffers weight_buffers[4]; // Only used in raster
};

#endif // RENDER_SCENE_BUFFERS_RD_H
