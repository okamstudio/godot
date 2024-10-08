/**************************************************************************/
/*  rasterizer_canvas_gles3.h                                             */
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

#ifndef RASTERIZER_CANVAS_GLES3_H
#define RASTERIZER_CANVAS_GLES3_H

#ifdef GLES3_ENABLED

#include "rasterizer_scene_gles3.h"
#include "servers/rendering/renderer_canvas_render.h"
#include "servers/rendering/renderer_compositor.h"
#include "storage/material_storage.h"
#include "storage/texture_storage.h"

#include "drivers/gles3/shaders/canvas.glsl.gen.h"
#include "drivers/gles3/shaders/canvas_occlusion.glsl.gen.h"

class RasterizerSceneGLES3;

class RasterizerCanvasGLES3 : public RendererCanvasRender {
	static RasterizerCanvasGLES3 *singleton;

	_FORCE_INLINE_ void _update_transform_2d_to_mat2x4(const Transform2D &p_transform, float *p_mat2x4);
	_FORCE_INLINE_ void _update_transform_2d_to_mat2x3(const Transform2D &p_transform, float *p_mat2x3);

	_FORCE_INLINE_ void _update_transform_2d_to_mat4(const Transform2D &p_transform, float *p_mat4);
	_FORCE_INLINE_ void _update_transform_to_mat4(const Transform3D &p_transform, float *p_mat4);

	enum {

		FLAGS_INSTANCING_MASK = 0x7F,
		FLAGS_INSTANCING_HAS_COLORS = (1 << 7),
		FLAGS_INSTANCING_HAS_CUSTOM_DATA = (1 << 8),

		FLAGS_CLIP_RECT_UV = (1 << 9),
		FLAGS_TRANSPOSE_RECT = (1 << 10),

		FLAGS_NINEPACH_DRAW_CENTER = (1 << 12),

		FLAGS_USE_SKELETON = (1 << 15),
		FLAGS_NINEPATCH_H_MODE_SHIFT = 16,
		FLAGS_NINEPATCH_V_MODE_SHIFT = 18,
		FLAGS_LIGHT_COUNT_SHIFT = 20,

		FLAGS_DEFAULT_NORMAL_MAP_USED = (1 << 26),
		FLAGS_DEFAULT_SPECULAR_MAP_USED = (1 << 27),

		FLAGS_USE_MSDF = (1 << 28),
		FLAGS_USE_LCD = (1 << 29),

		FLAGS_FLIP_H = (1 << 30),
		FLAGS_FLIP_V = (1 << 31),
	};

	enum {
		LIGHT_FLAGS_TEXTURE_MASK = 0xFFFF,
		LIGHT_FLAGS_BLEND_SHIFT = 16,
		LIGHT_FLAGS_BLEND_MASK = (3 << 16),
		LIGHT_FLAGS_BLEND_MODE_ADD = (0 << 16),
		LIGHT_FLAGS_BLEND_MODE_SUB = (1 << 16),
		LIGHT_FLAGS_BLEND_MODE_MIX = (2 << 16),
		LIGHT_FLAGS_BLEND_MODE_MASK = (3 << 16),
		LIGHT_FLAGS_HAS_SHADOW = (1 << 20),
		LIGHT_FLAGS_FILTER_SHIFT = 22

	};

	enum {
		MAX_RENDER_ITEMS = 256 * 1024,
		MAX_LIGHT_TEXTURES = 1024,
		MAX_LIGHTS_PER_ITEM = 16,
		DEFAULT_MAX_LIGHTS_PER_RENDER = 256,
	};

	/******************/
	/**** LIGHTING ****/
	/******************/

	struct CanvasLight {
		RID texture;
		struct {
			bool enabled = false;
			float z_far;
			float y_offset;
			Transform2D directional_xform;
		} shadow;
	};

	RID_Owner<CanvasLight> canvas_light_owner;

	struct OccluderPolygon {
		RS::CanvasOccluderPolygonCullMode cull_mode = RS::CANVAS_OCCLUDER_POLYGON_CULL_DISABLED;
		int line_point_count = 0;
		GLuint vertex_buffer = 0;
		GLuint vertex_array = 0;
		GLuint index_buffer = 0;

		int sdf_point_count = 0;
		int sdf_index_count = 0;
		GLuint sdf_vertex_buffer = 0;
		GLuint sdf_vertex_array = 0;
		GLuint sdf_index_buffer = 0;
		bool sdf_is_lines = false;
	};

	RID_Owner<OccluderPolygon> occluder_polygon_owner;

	void _update_shadow_atlas();

	struct {
		CanvasOcclusionShaderGLES3 shader;
		RID shader_version;
	} shadow_render;

	struct LightUniform {
		float matrix[8]; //light to texture coordinate matrix
		float shadow_matrix[8]; //light to shadow coordinate matrix
		float color[4];

		uint8_t shadow_color[4];
		uint32_t flags; //index to light texture
		float shadow_pixel_size;
		float height;

		float position[2];
		float shadow_z_far_inv;
		float shadow_y_ofs;

		float atlas_rect[4];
	};

	static_assert(sizeof(LightUniform) % 16 == 0, "2D light UBO size must be a multiple of 16 bytes");

public:
	enum {
		BASE_UNIFORM_LOCATION = 0,
		GLOBAL_UNIFORM_LOCATION = 1,
		LIGHT_UNIFORM_LOCATION = 2,
		INSTANCE_UNIFORM_LOCATION = 3,
		MATERIAL_UNIFORM_LOCATION = 4,
	};

	struct StateBuffer {
		float canvas_transform[16];
		float screen_transform[16];
		float canvas_normal_transform[16];
		float canvas_modulate[4];

		float screen_pixel_size[2];
		float time;
		uint32_t use_pixel_snap;

		float sdf_to_tex[4];
		float sdf_to_screen[2];
		float screen_to_sdf[2];

		uint32_t directional_light_count;
		float tex_to_sdf;
		uint32_t pad1;
		uint32_t pad2;
	};

	static_assert(sizeof(StateBuffer) % 16 == 0, "2D state UBO size must be a multiple of 16 bytes");

	struct PolygonBuffers {
		GLuint vertex_buffer = 0;
		GLuint vertex_array = 0;
		GLuint index_buffer = 0;
		int count = 0;
		bool color_disabled = false;
		Color color = Color(1.0, 1.0, 1.0, 1.0);
	};

	struct {
		HashMap<PolygonID, PolygonBuffers> polygons;
		PolygonID last_id = 0;
	} polygon_buffers;

	RendererCanvasRender::PolygonID request_polygon(const Vector<int> &p_indices, const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs = Vector<Point2>(), const Vector<int> &p_bones = Vector<int>(), const Vector<float> &p_weights = Vector<float>()) override;
	void free_polygon(PolygonID p_polygon) override;

	struct InstanceData {
		float world[6];
		float color_texture_pixel_size[2];
		union {
			//rect
			struct {
				float modulation[4];
				union {
					float msdf[4];
					float ninepatch_margins[4];
				};
				float dst_rect[4];
				float src_rect[4];
				float pad[2];
			};
			//primitive
			struct {
				float points[6]; // vec2 points[3]
				float uvs[6]; // vec2 points[3]
				uint32_t colors[6]; // colors encoded as half
			};
		};
		uint32_t flags;
		uint32_t specular_shininess;
		uint32_t lights[4];
	};

	static_assert(sizeof(InstanceData) == 128, "2D instance data struct size must be 128 bytes");

	struct Data {
		GLuint canvas_quad_vertices;
		GLuint canvas_quad_array;

		GLuint indexed_quad_buffer;
		GLuint indexed_quad_array;

		GLuint particle_quad_vertices;
		GLuint particle_quad_array;

		GLuint ninepatch_vertices;
		GLuint ninepatch_elements;

		RID canvas_shader_default_version;

		uint32_t max_lights_per_render = 256;
		uint32_t max_lights_per_item = 16;
		uint32_t max_instances_per_buffer = 16384;
		uint32_t max_instance_buffer_size = 16384 * 128;
	} data;

	struct Batch {
		// Position in the UBO measured in bytes
		uint32_t start = 0;
		uint32_t instance_count = 0;
		uint32_t instance_buffer_index = 0;

		RID tex;
		RS::CanvasItemTextureFilter filter = RS::CANVAS_ITEM_TEXTURE_FILTER_MAX;
		RS::CanvasItemTextureRepeat repeat = RS::CANVAS_ITEM_TEXTURE_REPEAT_MAX;

		GLES3::CanvasShaderData::BlendMode blend_mode = GLES3::CanvasShaderData::BLEND_MODE_MIX;
		Color blend_color = Color(1.0, 1.0, 1.0, 1.0);

		Item *clip = nullptr;

		RID material;
		GLES3::CanvasMaterialData *material_data = nullptr;
		CanvasShaderGLES3::ShaderVariant shader_variant = CanvasShaderGLES3::MODE_QUAD;
		uint64_t vertex_input_mask = RS::ARRAY_FORMAT_VERTEX | RS::ARRAY_FORMAT_COLOR | RS::ARRAY_FORMAT_TEX_UV;

		const Item::Command *command = nullptr;
		Item::Command::Type command_type = Item::Command::TYPE_ANIMATION_SLICE; // Can default to any type that doesn't form a batch.
		uint32_t primitive_points = 0;

		bool lights_disabled = false;
	};

	// DataBuffer contains our per-frame data. I.e. the resources that are updated each frame.
	// We track them and ensure that they don't get reused until at least 2 frames have passed
	// to avoid the GPU stalling to wait for a resource to become available.
	struct DataBuffer {
		Vector<GLuint> instance_buffers;
		GLuint light_ubo = 0;
		GLuint state_ubo = 0;
		uint64_t last_frame_used = -3;
		GLsync fence = GLsync();
	};

	struct State {
		LocalVector<DataBuffer> canvas_instance_data_buffers;
		LocalVector<Batch> canvas_instance_batches;
		uint32_t current_data_buffer_index = 0;
		uint32_t current_instance_buffer_index = 0;
		uint32_t current_batch_index = 0;
		uint32_t last_item_index = 0;

		InstanceData *instance_data_array = nullptr;

		LightUniform *light_uniforms = nullptr;

		GLuint shadow_texture = 0;
		GLuint shadow_depth_buffer = 0;
		GLuint shadow_fb = 0;
		int shadow_texture_size = 2048;

		bool using_directional_lights = false;

		RID current_tex;
		RS::CanvasItemTextureFilter current_filter_mode = RS::CANVAS_ITEM_TEXTURE_FILTER_MAX;
		RS::CanvasItemTextureRepeat current_repeat_mode = RS::CANVAS_ITEM_TEXTURE_REPEAT_MAX;

		bool transparent_render_target = false;

		double time = 0.0;

		RS::CanvasItemTextureFilter default_filter = RS::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT;
		RS::CanvasItemTextureRepeat default_repeat = RS::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT;
	} state;

	Item *items[MAX_RENDER_ITEMS];

	RID default_canvas_texture;
	RID default_canvas_group_material;
	RID default_canvas_group_shader;
	RID default_clip_children_material;
	RID default_clip_children_shader;

	typedef void Texture;

	void canvas_begin(RID p_to_render_target, bool p_to_backbuffer, bool p_backbuffer_has_mipmaps);

	//virtual void draw_window_margins(int *black_margin, RID *black_image) override;
	void draw_lens_distortion_rect(const Rect2 &p_rect, float p_k1, float p_k2, const Vector2 &p_eye_center, float p_oversample);

	void reset_canvas();

	RID light_create() override;
	void light_set_texture(RID p_rid, RID p_texture) override;
	void light_set_use_shadow(RID p_rid, bool p_enable) override;
	void light_update_shadow(RID p_rid, int p_shadow_index, const Transform2D &p_light_xform, int p_light_mask, float p_near, float p_far, LightOccluderInstance *p_occluders) override;
	void light_update_directional_shadow(RID p_rid, int p_shadow_index, const Transform2D &p_light_xform, int p_light_mask, float p_cull_distance, const Rect2 &p_clip_rect, LightOccluderInstance *p_occluders) override;

	void render_sdf(RID p_render_target, LightOccluderInstance *p_occluders) override;
	RID occluder_polygon_create() override;
	void occluder_polygon_set_shape(RID p_occluder, const Vector<Vector2> &p_points, bool p_closed) override;
	void occluder_polygon_set_cull_mode(RID p_occluder, RS::CanvasOccluderPolygonCullMode p_mode) override;
	void set_shadow_texture_size(int p_size) override;

	bool free(RID p_rid) override;
	void update() override;

	void _bind_canvas_texture(RID p_texture, RS::CanvasItemTextureFilter p_base_filter, RS::CanvasItemTextureRepeat p_base_repeat);
	void _prepare_canvas_texture(RID p_texture, RS::CanvasItemTextureFilter p_base_filter, RS::CanvasItemTextureRepeat p_base_repeat, uint32_t &r_index, Size2 &r_texpixel_size);

	void canvas_render_items(RID p_to_render_target, Item *p_item_list, const Color &p_modulate, Light *p_light_list, Light *p_directional_list, const Transform2D &p_canvas_transform, RS::CanvasItemTextureFilter p_default_filter, RS::CanvasItemTextureRepeat p_default_repeat, bool p_snap_2d_vertices_to_pixel, bool &r_sdf_used, RenderingMethod::RenderInfo *r_render_info = nullptr) override;
	void _render_items(RID p_to_render_target, int p_item_count, const Transform2D &p_canvas_transform_inverse, Light *p_lights, bool &r_sdf_used, bool p_to_backbuffer = false, RenderingMethod::RenderInfo *r_render_info = nullptr, bool p_backbuffer_has_mipmaps = false);
	void _record_item_commands(const Item *p_item, RID p_render_target, const Transform2D &p_canvas_transform_inverse, Item *&current_clip, GLES3::CanvasShaderData::BlendMode p_blend_mode, Light *p_lights, uint32_t &r_index, bool &r_break_batch, bool &r_sdf_used, const Point2 &p_repeat_offset);
	void _render_batch(Light *p_lights, uint32_t p_index, RenderingMethod::RenderInfo *r_render_info = nullptr);
	bool _bind_material(GLES3::CanvasMaterialData *p_material_data, CanvasShaderGLES3::ShaderVariant p_variant, uint64_t p_specialization);
	void _new_batch(bool &r_batch_broken);
	void _add_to_batch(uint32_t &r_index, bool &r_batch_broken);
	void _allocate_instance_data_buffer();
	void _allocate_instance_buffer();
	void _enable_attributes(uint32_t p_start, bool p_primitive, uint32_t p_rate = 1);

	void set_time(double p_time);

	virtual void set_debug_redraw(bool p_enabled, double p_time, const Color &p_color) override {
		if (p_enabled) {
			WARN_PRINT_ONCE("Debug CanvasItem Redraw is not available yet when using the GL Compatibility backend.");
		}
	}

	virtual uint32_t get_pipeline_compilations(RS::PipelineSource p_source) override { return 0; }

	static RasterizerCanvasGLES3 *get_singleton();
	RasterizerCanvasGLES3();
	~RasterizerCanvasGLES3();
};

#endif // GLES3_ENABLED

#endif // RASTERIZER_CANVAS_GLES3_H
