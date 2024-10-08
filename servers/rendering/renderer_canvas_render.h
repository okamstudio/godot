/**************************************************************************/
/*  renderer_canvas_render.h                                              */
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

#ifndef RENDERER_CANVAS_RENDER_H
#define RENDERER_CANVAS_RENDER_H

#include "servers/rendering/rendering_method.h"
#include "servers/rendering_server.h"

class RendererCanvasRender {
public:
	static RendererCanvasRender *singleton;

	enum CanvasRectFlags {
		CANVAS_RECT_REGION = 1,
		CANVAS_RECT_TILE = 2,
		CANVAS_RECT_FLIP_H = 4,
		CANVAS_RECT_FLIP_V = 8,
		CANVAS_RECT_TRANSPOSE = 16,
		CANVAS_RECT_CLIP_UV = 32,
		CANVAS_RECT_IS_GROUP = 64,
		CANVAS_RECT_MSDF = 128,
		CANVAS_RECT_LCD = 256,
	};

	struct Light {
		bool enabled : 1;
		bool on_interpolate_transform_list : 1;
		bool interpolated : 1;
		Color color;
		Transform2D xform_curr;
		Transform2D xform_prev;
		float height;
		float energy;
		float scale;
		int z_min;
		int z_max;
		int layer_min;
		int layer_max;
		int item_mask;
		int item_shadow_mask;
		float directional_distance;
		RS::CanvasLightMode mode;
		RS::CanvasLightBlendMode blend_mode;
		RID texture;
		Vector2 texture_offset;
		RID canvas;
		bool use_shadow;
		int shadow_buffer_size;
		RS::CanvasLightShadowFilter shadow_filter;
		Color shadow_color;
		float shadow_smooth;

		//void *texture_cache; // implementation dependent
		Rect2 rect_cache;
		Transform2D xform_cache;
		float radius_cache; //used for shadow far plane
		//Projection shadow_matrix_cache;

		Transform2D light_shader_xform;
		//Vector2 light_shader_pos;

		Light *shadows_next_ptr = nullptr;
		Light *filter_next_ptr = nullptr;
		Light *next_ptr = nullptr;
		Light *directional_next_ptr = nullptr;

		RID light_internal;
		uint64_t version;

		int32_t render_index_cache;

		Light() {
			version = 0;
			enabled = true;
			on_interpolate_transform_list = false;
			interpolated = true;
			color = Color(1, 1, 1);
			shadow_color = Color(0, 0, 0, 0);
			height = 0;
			z_min = -1024;
			z_max = 1024;
			layer_min = 0;
			layer_max = 0;
			item_mask = 1;
			scale = 1.0;
			energy = 1.0;
			item_shadow_mask = 1;
			mode = RS::CANVAS_LIGHT_MODE_POINT;
			blend_mode = RS::CANVAS_LIGHT_BLEND_MODE_ADD;
			//			texture_cache = nullptr;
			next_ptr = nullptr;
			directional_next_ptr = nullptr;
			filter_next_ptr = nullptr;
			use_shadow = false;
			shadow_buffer_size = 2048;
			shadow_filter = RS::CANVAS_LIGHT_FILTER_NONE;
			shadow_smooth = 0.0;
			render_index_cache = -1;
			directional_distance = 10000.0;
		}
	};

	//easier wrap to avoid mistakes

	struct Item;

	typedef uint64_t PolygonID;
	virtual PolygonID request_polygon(const Vector<int> &p_indices, const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs = Vector<Point2>(), const Vector<int> &p_bones = Vector<int>(), const Vector<float> &p_weights = Vector<float>()) = 0;
	virtual void free_polygon(PolygonID p_polygon) = 0;

	//also easier to wrap to avoid mistakes
	struct Polygon {
		PolygonID polygon_id;
		Rect2 rect_cache;

		_FORCE_INLINE_ void create(const Vector<int> &p_indices, const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs = Vector<Point2>(), const Vector<int> &p_bones = Vector<int>(), const Vector<float> &p_weights = Vector<float>()) {
			ERR_FAIL_COND(polygon_id != 0);
			{
				uint32_t pc = p_points.size();
				const Vector2 *v2 = p_points.ptr();
				rect_cache.position = *v2;
				for (uint32_t i = 1; i < pc; i++) {
					rect_cache.expand_to(v2[i]);
				}
			}
			polygon_id = singleton->request_polygon(p_indices, p_points, p_colors, p_uvs, p_bones, p_weights);
		}

		_FORCE_INLINE_ Polygon() { polygon_id = 0; }
		_FORCE_INLINE_ ~Polygon() {
			if (polygon_id) {
				singleton->free_polygon(polygon_id);
			}
		}
	};

	//item

	struct Item {
		//commands are allocated in blocks of 4k to improve performance
		//and cache coherence.
		//blocks always grow but never shrink.

		struct CommandBlock {
			enum {
				MAX_SIZE = 4096
			};
			uint32_t usage;
			uint8_t *memory = nullptr;
		};

		struct Command {
			enum Type {
				TYPE_RECT,
				TYPE_NINEPATCH,
				TYPE_POLYGON,
				TYPE_PRIMITIVE,
				TYPE_MESH,
				TYPE_MULTIMESH,
				TYPE_PARTICLES,
				TYPE_TRANSFORM,
				TYPE_CLIP_IGNORE,
				TYPE_ANIMATION_SLICE,
			};

			Command *next = nullptr;
			Type type;
			virtual ~Command() {}
		};

		struct CommandRect : public Command {
			Rect2 rect;
			Color modulate;
			Rect2 source;
			uint16_t flags;
			float outline;
			float px_range;

			RID texture;

			CommandRect() {
				flags = 0;
				outline = 0;
				px_range = 1;
				type = TYPE_RECT;
			}
		};

		struct CommandNinePatch : public Command {
			Rect2 rect;
			Rect2 source;
			float margin[4];
			bool draw_center;
			Color color;
			RS::NinePatchAxisMode axis_x;
			RS::NinePatchAxisMode axis_y;

			RID texture;

			CommandNinePatch() {
				draw_center = true;
				type = TYPE_NINEPATCH;
			}
		};

		struct CommandPolygon : public Command {
			RS::PrimitiveType primitive;
			Polygon polygon;

			RID texture;

			CommandPolygon() {
				type = TYPE_POLYGON;
			}
		};

		struct CommandPrimitive : public Command {
			uint32_t point_count;
			Vector2 points[4];
			Vector2 uvs[4];
			Color colors[4];

			RID texture;

			CommandPrimitive() {
				type = TYPE_PRIMITIVE;
			}
		};

		struct CommandMesh : public Command {
			RID mesh;
			Transform2D transform;
			Color modulate;
			RID mesh_instance;

			RID texture;

			CommandMesh() { type = TYPE_MESH; }
			~CommandMesh();
		};

		struct CommandMultiMesh : public Command {
			RID multimesh;

			RID texture;

			CommandMultiMesh() { type = TYPE_MULTIMESH; }
		};

		struct CommandParticles : public Command {
			RID particles;
			RID texture;

			CommandParticles() { type = TYPE_PARTICLES; }
		};

		struct CommandTransform : public Command {
			Transform2D xform;
			CommandTransform() { type = TYPE_TRANSFORM; }
		};

		struct CommandClipIgnore : public Command {
			bool ignore;
			CommandClipIgnore() {
				type = TYPE_CLIP_IGNORE;
				ignore = false;
			}
		};

		struct CommandAnimationSlice : public Command {
			double animation_length = 0;
			double slice_begin = 0;
			double slice_end = 0;
			double offset = 0;

			CommandAnimationSlice() {
				type = TYPE_ANIMATION_SLICE;
			}
		};

		struct ViewportRender {
			RenderingServer *owner = nullptr;
			void *udata = nullptr;
			Rect2 rect;
		};

		// For interpolation we store the current local xform,
		// and the previous xform from the previous tick.
		Transform2D xform_curr;
		Transform2D xform_prev;

		bool clip : 1;
		bool visible : 1;
		bool behind : 1;
		bool update_when_visible : 1;
		bool on_interpolate_transform_list : 1;
		bool interpolated : 1;

		struct CanvasGroup {
			RS::CanvasGroupMode mode;
			bool fit_empty;
			float fit_margin;
			bool blur_mipmaps;
			float clear_margin;
		};

		CanvasGroup *canvas_group = nullptr;
		bool use_canvas_group = false;
		int light_mask;
		int z_final;

		mutable bool custom_rect;
		mutable bool rect_dirty;
		mutable Rect2 rect;
		RID material;
		RID skeleton;

		Item *next = nullptr;

		struct CopyBackBuffer {
			Rect2 rect;
			Rect2 screen_rect;
			bool full;
		};
		CopyBackBuffer *copy_back_buffer = nullptr;

		Color final_modulate;
		Transform2D final_transform;
		Rect2 final_clip_rect;
		Item *final_clip_owner = nullptr;
		Item *material_owner = nullptr;
		Item *canvas_group_owner = nullptr;
		ViewportRender *vp_render = nullptr;
		bool distance_field;
		bool light_masked;
		bool repeat_source;
		Point2 repeat_size;
		int repeat_times = 1;
		Item *repeat_source_item = nullptr;

		Rect2 global_rect_cache;

		const Rect2 &get_rect() const;

		Command *commands = nullptr;
		Command *last_command = nullptr;
		Vector<CommandBlock> blocks;
		uint32_t current_block;
#ifdef DEBUG_ENABLED
		mutable double debug_redraw_time = 0;
#endif

		template <typename T>
		T *alloc_command() {
			T *command = nullptr;
			if (commands == nullptr) {
				// As the most common use case of canvas items is to
				// use only one command, the first is done with it's
				// own allocation. The rest of them use blocks.
				command = memnew(T);
				command->next = nullptr;
				commands = command;
				last_command = command;
			} else {
				//Subsequent commands go into a block.

				while (true) {
					if (unlikely(current_block == (uint32_t)blocks.size())) {
						// If we need more blocks, we allocate them
						// (they won't be freed until this CanvasItem is
						// deleted, though).
						CommandBlock cb;
						cb.memory = (uint8_t *)memalloc(CommandBlock::MAX_SIZE);
						cb.usage = 0;
						blocks.push_back(cb);
					}

					CommandBlock *c = &blocks.write[current_block];
					size_t space_left = CommandBlock::MAX_SIZE - c->usage;
					if (space_left < sizeof(T)) {
						current_block++;
						continue;
					}

					//allocate block and add to the linked list
					void *memory = c->memory + c->usage;
					command = memnew_placement(memory, T);
					command->next = nullptr;
					last_command->next = command;
					last_command = command;
					c->usage += sizeof(T);
					break;
				}
			}

			rect_dirty = true;
			return command;
		}

		void clear() {
			// The first one is always allocated on heap
			// the rest go in the blocks
			Command *c = commands;
			while (c) {
				Command *n = c->next;
				if (c == commands) {
					memdelete(commands);
					commands = nullptr;
				} else {
					c->~Command();
				}
				c = n;
			}
			{
				uint32_t cbc = MIN((current_block + 1), (uint32_t)blocks.size());
				CommandBlock *blockptr = blocks.ptrw();
				for (uint32_t i = 0; i < cbc; i++) {
					blockptr[i].usage = 0;
				}
			}

			last_command = nullptr;
			commands = nullptr;
			current_block = 0;
			clip = false;
			rect_dirty = true;
			final_clip_owner = nullptr;
			material_owner = nullptr;
			light_masked = false;
		}

		RS::CanvasItemTextureFilter texture_filter;
		RS::CanvasItemTextureRepeat texture_repeat;

		Item() {
			commands = nullptr;
			last_command = nullptr;
			current_block = 0;
			light_mask = 1;
			vp_render = nullptr;
			next = nullptr;
			final_clip_owner = nullptr;
			canvas_group_owner = nullptr;
			clip = false;
			final_modulate = Color(1, 1, 1, 1);
			visible = true;
			rect_dirty = true;
			custom_rect = false;
			behind = false;
			material_owner = nullptr;
			copy_back_buffer = nullptr;
			distance_field = false;
			light_masked = false;
			update_when_visible = false;
			z_final = 0;
			texture_filter = RS::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT;
			texture_repeat = RS::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT;
			repeat_source = false;
			on_interpolate_transform_list = false;
			interpolated = true;
		}
		virtual ~Item() {
			clear();
			for (int i = 0; i < blocks.size(); i++) {
				memfree(blocks[i].memory);
			}
			if (copy_back_buffer) {
				memdelete(copy_back_buffer);
			}
		}
	};

	virtual void canvas_render_items(RID p_to_render_target, Item *p_item_list, const Color &p_modulate, Light *p_light_list, Light *p_directional_list, const Transform2D &p_canvas_transform, RS::CanvasItemTextureFilter p_default_filter, RS::CanvasItemTextureRepeat p_default_repeat, bool p_snap_2d_vertices_to_pixel, bool &r_sdf_used, RenderingMethod::RenderInfo *r_render_info = nullptr) = 0;

	struct LightOccluderInstance {
		bool enabled : 1;
		bool on_interpolate_transform_list : 1;
		bool interpolated : 1;
		RID canvas;
		RID polygon;
		RID occluder;
		Rect2 aabb_cache;
		Transform2D xform_curr;
		Transform2D xform_prev;
		Transform2D xform_cache;
		int light_mask;
		bool sdf_collision;
		RS::CanvasOccluderPolygonCullMode cull_cache;

		LightOccluderInstance *next = nullptr;

		LightOccluderInstance() {
			enabled = true;
			on_interpolate_transform_list = false;
			interpolated = false;
			sdf_collision = false;
			next = nullptr;
			light_mask = 1;
			cull_cache = RS::CANVAS_OCCLUDER_POLYGON_CULL_DISABLED;
		}
	};

	virtual RID light_create() = 0;
	virtual void light_set_texture(RID p_rid, RID p_texture) = 0;
	virtual void light_set_use_shadow(RID p_rid, bool p_enable) = 0;
	virtual void light_update_shadow(RID p_rid, int p_shadow_index, const Transform2D &p_light_xform, int p_light_mask, float p_near, float p_far, LightOccluderInstance *p_occluders) = 0;
	virtual void light_update_directional_shadow(RID p_rid, int p_shadow_index, const Transform2D &p_light_xform, int p_light_mask, float p_cull_distance, const Rect2 &p_clip_rect, LightOccluderInstance *p_occluders) = 0;

	virtual void render_sdf(RID p_render_target, LightOccluderInstance *p_occluders) = 0;

	virtual RID occluder_polygon_create() = 0;
	virtual void occluder_polygon_set_shape(RID p_occluder, const Vector<Vector2> &p_points, bool p_closed) = 0;
	virtual void occluder_polygon_set_cull_mode(RID p_occluder, RS::CanvasOccluderPolygonCullMode p_mode) = 0;
	virtual void set_shadow_texture_size(int p_size) = 0;

	virtual bool free(RID p_rid) = 0;
	virtual void update() = 0;

	virtual void set_debug_redraw(bool p_enabled, double p_time, const Color &p_color) = 0;
	virtual uint32_t get_pipeline_compilations(RS::PipelineSource p_source) = 0;

	RendererCanvasRender() {
		ERR_FAIL_COND_MSG(singleton != nullptr, "A RendererCanvasRender singleton already exists.");
		singleton = this;
	}
	virtual ~RendererCanvasRender() {
		singleton = nullptr;
	}
};

#endif // RENDERER_CANVAS_RENDER_H
