/**************************************************************************/
/*  rasterizer_canvas_batcher.h                                           */
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

#ifndef RASTERIZER_CANVAS_BATCHER_H
#define RASTERIZER_CANVAS_BATCHER_H

#include "core/os/os.h"
#include "core/project_settings.h"
#include "rasterizer_array.h"
#include "rasterizer_asserts.h"
#include "rasterizer_storage_common.h"
#include "servers/visual/rasterizer.h"
#include "servers/visual/visual_server_canvas_helper.h"

// We are using the curiously recurring template pattern
// https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
// For static polymorphism.

// This makes it super easy to access
// data / call funcs in the derived rasterizers from the base without writing and
// maintaining a boatload of virtual functions.
// In addition it assures that vtable will not be used and the function calls can be optimized,
// because it gives compile time static polymorphism.

// These macros makes it simpler and less verbose to define (and redefine) the inline functions
// template preamble
#define T_PREAMBLE template <class T, typename T_STORAGE>
// class preamble
#define C_PREAMBLE RasterizerCanvasBatcher<T, T_STORAGE>
// generic preamble
#define PREAMBLE(RET_T) \
	T_PREAMBLE          \
	RET_T C_PREAMBLE

template <class T, typename T_STORAGE>
class RasterizerCanvasBatcher {
public:
	// used to determine whether we use hardware transform (none)
	// software transform all verts, or software transform just a translate
	// (no rotate or scale)
	enum TransformMode {
		TM_NONE,
		TM_ALL,
		TM_TRANSLATE,
	};

	// pod versions of vector and color and RID, need to be 32 bit for vertex format
	struct BatchVector2 {
		float x, y;
		void set(float xx, float yy) {
			x = xx;
			y = yy;
		}
		void set(const Vector2 &p_o) {
			x = p_o.x;
			y = p_o.y;
		}
		void to(Vector2 &r_o) const {
			r_o.x = x;
			r_o.y = y;
		}
	};

	struct BatchColor {
		float r, g, b, a;
		void set_white() {
			r = 1.0f;
			g = 1.0f;
			b = 1.0f;
			a = 1.0f;
		}
		void set(const Color &p_c) {
			r = p_c.r;
			g = p_c.g;
			b = p_c.b;
			a = p_c.a;
		}
		void set(float rr, float gg, float bb, float aa) {
			r = rr;
			g = gg;
			b = bb;
			a = aa;
		}
		bool operator==(const BatchColor &p_c) const {
			return (r == p_c.r) && (g == p_c.g) && (b == p_c.b) && (a == p_c.a);
		}
		bool operator!=(const BatchColor &p_c) const { return (*this == p_c) == false; }
		bool equals(const Color &p_c) const {
			return (r == p_c.r) && (g == p_c.g) && (b == p_c.b) && (a == p_c.a);
		}
		const float *get_data() const { return &r; }
		String to_string() const {
			String sz = "{";
			const float *data = get_data();
			for (int c = 0; c < 4; c++) {
				float f = data[c];
				int val = ((f * 255.0f) + 0.5f);
				sz += String(Variant(val)) + " ";
			}
			sz += "}";
			return sz;
		}
	};

	// simplest FVF - local or baked position
	struct BatchVertex {
		// must be 32 bit pod
		BatchVector2 pos;
		BatchVector2 uv;
	};

	// simple FVF but also incorporating baked color
	struct BatchVertexColored : public BatchVertex {
		// must be 32 bit pod
		BatchColor col;
	};

	// if we are using normal mapping, we need light angles to be sent
	struct BatchVertexLightAngled : public BatchVertexColored {
		// must be pod
		float light_angle;
	};

	// CUSTOM SHADER vertex formats. These are larger but will probably
	// be needed with custom shaders in order to have the data accessible in the shader.

	// if we are using COLOR in vertex shader but not position (VERTEX)
	struct BatchVertexModulated : public BatchVertexLightAngled {
		BatchColor modulate;
	};

	struct BatchTransform {
		BatchVector2 translate;
		BatchVector2 basis[2];
	};

	// last resort, specially for custom shader, we put everything possible into a huge FVF
	// not very efficient, but better than no batching at all.
	struct BatchVertexLarge : public BatchVertexModulated {
		// must be pod
		BatchTransform transform;
	};

	// Batch should be as small as possible, and ideally nicely aligned (is 32 bytes at the moment)
	struct Batch {
		RasterizerStorageCommon::BatchType type; // should be 16 bit
		uint16_t batch_texture_id;

		// also item reference number
		uint32_t first_command;

		// in the case of DEFAULT, this is num commands.
		// with rects, is number of command and rects.
		// with lines, is number of lines
		// with polys, is number of indices (actual rendered verts)
		uint32_t num_commands;

		// first vertex of this batch in the vertex lists
		uint32_t first_vert;

		// we can keep the batch structure small because we either need to store
		// the color if a handled batch, or the parent item if a default batch, so
		// we can reference the correct originating command
		union {
			BatchColor color;

			// for default batches we will store the parent item
			const RasterizerCanvas::Item *item;
		};

		uint32_t get_num_verts() const {
			switch (type) {
				default: {
				} break;
				case RasterizerStorageCommon::BT_RECT: {
					return num_commands * 4;
				} break;
				case RasterizerStorageCommon::BT_LINE: {
					return num_commands * 2;
				} break;
				case RasterizerStorageCommon::BT_LINE_AA: {
					return num_commands * 2;
				} break;
				case RasterizerStorageCommon::BT_POLY: {
					return num_commands;
				} break;
			}

			// error condition
			WARN_PRINT_ONCE("reading num_verts from incorrect batch type");
			return 0;
		}
	};

	struct BatchTex {
		enum TileMode : uint32_t {
			TILE_OFF,
			TILE_NORMAL,
			TILE_FORCE_REPEAT,
		};
		RID RID_texture;
		RID RID_normal;
		TileMode tile_mode;
		BatchVector2 tex_pixel_size;
		uint32_t flags;
	};

	// items in a list to be sorted prior to joining
	struct BSortItem {
		// have a function to keep as pod, rather than operator
		void assign(const BSortItem &o) {
			item = o.item;
			z_index = o.z_index;
		}
		RasterizerCanvas::Item *item;
		int z_index;
	};

	// batch item may represent 1 or more items
	struct BItemJoined {
		uint32_t first_item_ref;
		uint32_t num_item_refs;

		Rect2 bounding_rect;

		// note the z_index  may only be correct for the first of the joined item references
		// this has implications for light culling with z ranged lights.
		int16_t z_index;

		// these are defined in RasterizerStorageCommon::BatchFlags
		uint16_t flags;

		// we are always splitting items with lots of commands,
		// and items with unhandled primitives (default)
		bool is_single_item() const { return (num_item_refs == 1); }
		bool use_attrib_transform() const { return flags & RasterizerStorageCommon::USE_LARGE_FVF; }
	};

	struct BItemRef {
		RasterizerCanvas::Item *item;
		Color final_modulate;
	};

	struct BLightRegion {
		void reset() {
			light_bitfield = 0;
			shadow_bitfield = 0;
			too_many_lights = false;
		}
		uint64_t light_bitfield;
		uint64_t shadow_bitfield;
		bool too_many_lights; // we can only do light region optimization if there are 64 or less lights
	};

	struct BatchData {
		BatchData() {
			reset_flush();
			reset_joined_item();

			max_quads = 0;
			vertex_buffer_size_units = 0;
			vertex_buffer_size_bytes = 0;
			index_buffer_size_units = 0;
			index_buffer_size_bytes = 0;

			use_colored_vertices = false;

			settings_use_batching = false;
			settings_max_join_item_commands = 0;
			settings_colored_vertex_format_threshold = 0.0f;
			scissor_threshold_area = 0.0f;
			joined_item_batch_flags = 0;
			diagnose_frame = false;
			next_diagnose_tick = 10000;
			diagnose_frame_number = 9999999999; // some high number
			join_across_z_indices = true;
			settings_item_reordering_lookahead = 0;

			settings_use_batching_original_choice = false;
			settings_flash_batching = false;
			settings_diagnose_frame = false;
			settings_scissor_lights = false;
			settings_scissor_threshold = -1.0f;
			settings_use_single_rect_fallback = false;
			settings_use_software_skinning = true;
			settings_ninepatch_mode = 0; // default
			settings_light_max_join_items = 16;

			settings_uv_contract = false;
			settings_uv_contract_amount = 0.0f;

			buffer_mode_batch_upload_send_null = true;
			buffer_mode_batch_upload_flag_stream = false;

			stats_items_sorted = 0;
			stats_light_items_joined = 0;
		}

		// called for each joined item
		void reset_joined_item() {
			// noop but left in as a stub
		}

		// called after each flush
		void reset_flush() {
			batches.reset();
			batch_textures.reset();

			vertices.reset();
			light_angles.reset();
			vertex_colors.reset();
			vertex_modulates.reset();
			vertex_transforms.reset();

			total_quads = 0;
			total_verts = 0;
			total_color_changes = 0;

			use_light_angles = false;
			use_modulate = false;
			use_large_verts = false;
			fvf = RasterizerStorageCommon::FVF_REGULAR;
		}

		uint32_t max_quads;
		uint32_t vertex_buffer_size_units;
		uint32_t vertex_buffer_size_bytes;
		uint32_t index_buffer_size_units;
		uint32_t index_buffer_size_bytes;

		// small vertex FVF type - pos and UV.
		// This will always be written to initially, but can be translated
		// to larger FVFs if necessary.
		RasterizerArray<BatchVertex> vertices;

		// extra data which can be stored during prefilling, for later translation to larger FVFs
		RasterizerArray<float> light_angles;
		RasterizerArray<BatchColor> vertex_colors; // these aren't usually used, but are for polys
		RasterizerArray<BatchColor> vertex_modulates;
		RasterizerArray<BatchTransform> vertex_transforms;

		// instead of having a different buffer for each vertex FVF type
		// we have a special array big enough for the biggest FVF
		// which can have a changeable unit size, and reuse it.
		RasterizerUnitArray unit_vertices;

		RasterizerArray<Batch> batches;
		RasterizerArray<Batch> batches_temp; // used for translating to colored vertex batches
		RasterizerArray_non_pod<BatchTex> batch_textures; // the only reason this is non-POD is because of RIDs

		// SHOULD THESE BE IN FILLSTATE?
		// flexible vertex format.
		// all verts have pos and UV.
		// some have color, some light angles etc.
		RasterizerStorageCommon::FVF fvf;
		bool use_colored_vertices;
		bool use_light_angles;
		bool use_modulate;
		bool use_large_verts;

		// if the shader is using MODULATE, we prevent baking color so the final_modulate can
		// be read in the shader.
		// if the shader is reading VERTEX, we prevent baking vertex positions with extra matrices etc
		// to prevent the read position being incorrect.
		// These flags are defined in RasterizerStorageCommon::BatchFlags
		uint32_t joined_item_batch_flags;

		RasterizerArray<BItemJoined> items_joined;
		RasterizerArray<BItemRef> item_refs;

		// items are sorted prior to joining
		RasterizerArray<BSortItem> sort_items;

		// counts
		int total_quads;
		int total_verts;

		// we keep a record of how many color changes caused new batches
		// if the colors are causing an excessive number of batches, we switch
		// to alternate batching method and add color to the vertex format.
		int total_color_changes;

		// measured in pixels, recalculated each frame
		float scissor_threshold_area;

		// diagnose this frame, every nTh frame when settings_diagnose_frame is on
		bool diagnose_frame;
		String frame_string;
		uint32_t next_diagnose_tick;
		uint64_t diagnose_frame_number;

		// whether to join items across z_indices - this can interfere with z ranged lights,
		// so has to be disabled in some circumstances
		bool join_across_z_indices;

		// global settings
		bool settings_use_batching; // the current use_batching (affected by flash)
		bool settings_use_batching_original_choice; // the choice entered in project settings
		bool settings_flash_batching; // for regression testing, flash between non-batched and batched renderer
		bool settings_diagnose_frame; // print out batches to help optimize / regression test
		int settings_max_join_item_commands;
		float settings_colored_vertex_format_threshold;
		bool settings_scissor_lights;
		float settings_scissor_threshold; // 0.0 to 1.0
		int settings_item_reordering_lookahead;
		bool settings_use_single_rect_fallback;
		bool settings_use_software_skinning;
		int settings_light_max_join_items;
		int settings_ninepatch_mode;

		// buffer orphaning modes
		bool buffer_mode_batch_upload_send_null;
		bool buffer_mode_batch_upload_flag_stream;

		// uv contraction
		bool settings_uv_contract;
		float settings_uv_contract_amount;

		// only done on diagnose frame
		void reset_stats() {
			stats_items_sorted = 0;
			stats_light_items_joined = 0;
		}

		// frame stats (just for monitoring and debugging)
		int stats_items_sorted;
		int stats_light_items_joined;
	} bdata;

	struct FillState {
		void reset_flush() {
			// don't reset members that need to be preserved after flushing
			// half way through a list of commands
			curr_batch = nullptr;
			batch_tex_id = -1;
			texpixel_size = Vector2(1, 1);
			contract_uvs = false;

			sequence_batch_type_flags = 0;
		}

		void reset_joined_item(bool p_is_single_item, bool p_use_attrib_transform) {
			reset_flush();
			is_single_item = p_is_single_item;
			use_attrib_transform = p_use_attrib_transform;
			use_software_transform = !is_single_item && !use_attrib_transform;

			extra_matrix_sent = false;
		}

		// for batching multiple types, we don't allow mixing RECTs / LINEs etc.
		// using flags allows quicker rejection of sequences with different batch types
		uint32_t sequence_batch_type_flags;

		Batch *curr_batch;
		int batch_tex_id;

		bool is_single_item;
		bool use_attrib_transform;
		bool use_software_transform;

		bool contract_uvs;
		Vector2 texpixel_size;
		Color final_modulate;
		TransformMode transform_mode;
		TransformMode orig_transform_mode;

		// support for extra matrices
		bool extra_matrix_sent; // whether sent on this item (in which case software transform can't be used untl end of item)
		int transform_extra_command_number_p1; // plus one to allow fast checking against zero
		Transform2D transform_combined; // final * extra
		Transform2D skeleton_base_inverse_xform; // used in software skinning
	};

	// used during try_join
	struct RenderItemState {
		RenderItemState() { reset(); }
		void reset() {
			current_clip = nullptr;
			shader_cache = nullptr;
			rebind_shader = true;
			prev_use_skeleton = false;
			last_blend_mode = -1;
			canvas_last_material = RID();
			item_group_z = 0;
			item_group_light = nullptr;
			final_modulate = Color(-1.0, -1.0, -1.0, -1.0); // just something unlikely

			joined_item_batch_type_flags_curr = 0;
			joined_item_batch_type_flags_prev = 0;

			joined_item = nullptr;
		}

		RasterizerCanvas::Item *current_clip;
		typename T_STORAGE::Shader *shader_cache;
		bool rebind_shader;
		bool prev_use_skeleton;
		bool prev_distance_field;
		int last_blend_mode;
		RID canvas_last_material;
		Color final_modulate;

		// used for joining items only
		BItemJoined *joined_item;
		bool join_batch_break;
		BLightRegion light_region;

		// we need some logic to prevent joining items that have vastly different batch types
		// these are defined in RasterizerStorageCommon::BatchTypeFlags
		uint32_t joined_item_batch_type_flags_curr;
		uint32_t joined_item_batch_type_flags_prev;

		// 'item group' is data over a single call to canvas_render_items
		int item_group_z;
		Color item_group_modulate;
		RasterizerCanvas::Light *item_group_light;
		Transform2D item_group_base_transform;
	} _render_item_state;

	bool use_nvidia_rect_workaround;

	//////////////////////////////////////////////////////////////////////////////
	// End of structs used by the batcher. Beginning of funcs.
private:
	// curiously recurring template pattern - allows access to functions in the DERIVED class
	// this is kind of like using virtual functions but more efficient as they are resolved at compile time
	T_STORAGE *get_storage() { return static_cast<const T *>(this)->storage; }
	const T_STORAGE *get_storage() const { return static_cast<const T *>(this)->storage; }
	T *get_this() { return static_cast<T *>(this); }
	const T *get_this() const { return static_cast<const T *>(this); }

protected:
	// main functions called from the rasterizer canvas
	void batch_constructor();
	void batch_initialize();

	void batch_canvas_begin();
	void batch_canvas_end();
	void batch_canvas_render_items_begin(const Color &p_modulate, RasterizerCanvas::Light *p_light, const Transform2D &p_base_transform);
	void batch_canvas_render_items_end();
	void batch_canvas_render_items(RasterizerCanvas::Item *p_item_list, int p_z, const Color &p_modulate, RasterizerCanvas::Light *p_light, const Transform2D &p_base_transform);

	// recording and sorting items from the initial pass
	void record_items(RasterizerCanvas::Item *p_item_list, int p_z);
	void join_sorted_items();
	void sort_items();
	bool _sort_items_match(const BSortItem &p_a, const BSortItem &p_b) const;
	bool sort_items_from(int p_start);

	// joining logic
	bool _disallow_item_join_if_batch_types_too_different(RenderItemState &r_ris, uint32_t btf_allowed);
	bool _detect_item_batch_break(RenderItemState &r_ris, RasterizerCanvas::Item *p_ci, bool &r_batch_break);

	// drives the loop filling batches and flushing
	void render_joined_item_commands(const BItemJoined &p_bij, RasterizerCanvas::Item *p_current_clip, bool &r_reclip, typename T_STORAGE::Material *p_material, bool p_lit, const RenderItemState &p_ris);

private:
	// flush once full or end of joined item
	void flush_render_batches(RasterizerCanvas::Item *p_first_item, RasterizerCanvas::Item *p_current_clip, bool &r_reclip, typename T_STORAGE::Material *p_material, uint32_t p_sequence_batch_type_flags);

	// a single joined item can contain multiple itemrefs, and thus create lots of batches
	bool prefill_joined_item(FillState &r_fill_state, int &r_command_start, RasterizerCanvas::Item *p_item, RasterizerCanvas::Item *p_current_clip, bool &r_reclip, typename T_STORAGE::Material *p_material);

	// prefilling different types of batch

	// default batch is an 'unhandled' legacy type batch that will be drawn with the legacy path,
	// all other batches are accelerated.
	void _prefill_default_batch(FillState &r_fill_state, int p_command_num, const RasterizerCanvas::Item &p_item);

	// accelerated batches
	bool _prefill_line(RasterizerCanvas::Item::CommandLine *p_line, FillState &r_fill_state, int &r_command_start, int command_num, int command_count, RasterizerCanvas::Item *p_item, bool multiply_final_modulate);
	template <bool SEND_LIGHT_ANGLES>
	bool _prefill_ninepatch(RasterizerCanvas::Item::CommandNinePatch *p_np, FillState &r_fill_state, int &r_command_start, int command_num, int command_count, RasterizerCanvas::Item *p_item, bool multiply_final_modulate);
	template <bool SEND_LIGHT_ANGLES>
	bool _prefill_polygon(RasterizerCanvas::Item::CommandPolygon *p_poly, FillState &r_fill_state, int &r_command_start, int command_num, int command_count, RasterizerCanvas::Item *p_item, bool multiply_final_modulate);
	template <bool SEND_LIGHT_ANGLES>
	bool _prefill_rect(RasterizerCanvas::Item::CommandRect *rect, FillState &r_fill_state, int &r_command_start, int command_num, int command_count, RasterizerCanvas::Item::Command *const *commands, RasterizerCanvas::Item *p_item, bool multiply_final_modulate);
	template <bool SEND_LIGHT_ANGLES>
	bool _prefill_multirect(RasterizerCanvas::Item::CommandMultiRect *mrect, FillState &r_fill_state, int &r_command_start, int command_num, bool multiply_final_modulate);

	// dealing with textures
	int _batch_find_or_create_tex(const RID &p_texture, const RID &p_normal, bool p_tile, int p_previous_match);

protected:
	// legacy support for non batched mode
	void _legacy_canvas_item_render_commands(RasterizerCanvas::Item *p_item, RasterizerCanvas::Item *p_current_clip, bool &r_reclip, typename T_STORAGE::Material *p_material);

	// light scissoring
	bool _light_scissor_begin(const Rect2 &p_item_rect, const Transform2D &p_light_xform, const Rect2 &p_light_rect) const;
	bool _light_find_intersection(const Rect2 &p_item_rect, const Transform2D &p_light_xform, const Rect2 &p_light_rect, Rect2 &r_cliprect) const;
	void _calculate_scissor_threshold_area();

private:
	// translating vertex formats prior to rendering
	void _translate_batches_to_vertex_colored_FVF();
	template <class BATCH_VERTEX_TYPE, bool INCLUDE_LIGHT_ANGLES, bool INCLUDE_MODULATE, bool INCLUDE_LARGE>
	void _translate_batches_to_larger_FVF(uint32_t p_sequence_batch_type_flags);

protected:
	// accessory funcs
	void _software_transform_vertex(BatchVector2 &r_v, const Transform2D &p_tr) const;
	void _software_transform_vertex(Vector2 &r_v, const Transform2D &p_tr) const;
	TransformMode _find_transform_mode(const Transform2D &p_tr) const {
		// decided whether to do translate only for software transform
		if ((p_tr.elements[0].x == 1.0f) &&
				(p_tr.elements[0].y == 0.0f) &&
				(p_tr.elements[1].x == 0.0f) &&
				(p_tr.elements[1].y == 1.0f)) {
			return TM_TRANSLATE;
		}

		return TM_ALL;
	}
	bool _software_skin_poly(RasterizerCanvas::Item::CommandPolygon *p_poly, RasterizerCanvas::Item *p_item, BatchVertex *bvs, BatchColor *vertex_colors, const FillState &p_fill_state, const BatchColor *p_precalced_colors);

	typename T_STORAGE::Texture *_get_canvas_texture(const RID &p_texture) const {
		if (p_texture.is_valid()) {
			typename T_STORAGE::Texture *texture = get_storage()->texture_owner.getornull(p_texture);

			if (texture) {
				// could be a proxy texture (e.g. animated)
				if (texture->proxy) {
					// take care to prevent infinite loop
					int count = 0;
					while (texture->proxy) {
						texture = texture->proxy;
						count++;
						ERR_FAIL_COND_V_MSG(count == 16, nullptr, "Texture proxy infinite loop detected.");
					}
				}

				return texture->get_ptr();
			}
		}

		return nullptr;
	}

public:
	Batch *_batch_request_new(bool p_blank = true) {
		Batch *batch = bdata.batches.request();
		if (!batch) {
			// grow the batches
			bdata.batches.grow();

			// and the temporary batches (used for color verts)
			bdata.batches_temp.reset();
			bdata.batches_temp.grow();

			// this should always succeed after growing
			batch = bdata.batches.request();
			CRASH_COND_MSG(!batch, "Out of memory");
		}

		if (p_blank) {
			memset(batch, 0, sizeof(Batch));
		} else {
			batch->item = nullptr;
		}

		return batch;
	}

	BatchVertex *_batch_vertex_request_new() {
		return bdata.vertices.request();
	}

protected:
	// no need to compile these in in release, they are unneeded outside the editor and only add to executable size
#if defined(TOOLS_ENABLED) && defined(DEBUG_ENABLED)
#include "batch_diagnose.inc"
#endif
};

PREAMBLE(void)::batch_canvas_begin() {
	// diagnose_frame?
	bdata.frame_string = ""; // just in case, always set this as we don't want a string leak in release...
#if defined(TOOLS_ENABLED) && defined(DEBUG_ENABLED)
	if (bdata.settings_diagnose_frame) {
		bdata.diagnose_frame = false;

		uint32_t tick = OS::get_singleton()->get_ticks_msec();
		uint64_t frame = Engine::get_singleton()->get_frames_drawn();

		if (tick >= bdata.next_diagnose_tick) {
			bdata.next_diagnose_tick = tick + 10000;

			// the plus one is prevent starting diagnosis half way through frame
			bdata.diagnose_frame_number = frame + 1;
		}

		if (frame == bdata.diagnose_frame_number) {
			bdata.diagnose_frame = true;
			bdata.reset_stats();
		}

		if (bdata.diagnose_frame) {
			bdata.frame_string = "canvas_begin FRAME " + itos(frame) + "\n";
		}
	}
#endif
}

PREAMBLE(void)::batch_canvas_end() {
#if defined(TOOLS_ENABLED) && defined(DEBUG_ENABLED)
	if (bdata.diagnose_frame) {
		bdata.frame_string += "canvas_end\n";
		if (bdata.stats_items_sorted) {
			bdata.frame_string += "\titems reordered: " + itos(bdata.stats_items_sorted) + "\n";
		}
		if (bdata.stats_light_items_joined) {
			bdata.frame_string += "\tlight items joined: " + itos(bdata.stats_light_items_joined) + "\n";
		}

		print_line(bdata.frame_string);
	}
#endif
}

PREAMBLE(void)::batch_canvas_render_items_begin(const Color &p_modulate, RasterizerCanvas::Light *p_light, const Transform2D &p_base_transform) {
	// if we are debugging, flash each frame between batching renderer and old version to compare for regressions
	if (bdata.settings_flash_batching) {
		if ((Engine::get_singleton()->get_frames_drawn() % 2) == 0) {
			bdata.settings_use_batching = true;
		} else {
			bdata.settings_use_batching = false;
		}
	}

	if (!bdata.settings_use_batching) {
		return;
	}

	// this only needs to be done when screen size changes, but this should be
	// infrequent enough
	_calculate_scissor_threshold_area();

	// set up render item state for all the z_indexes (this is common to all z_indexes)
	_render_item_state.reset();
	_render_item_state.item_group_modulate = p_modulate;
	_render_item_state.item_group_light = p_light;
	_render_item_state.item_group_base_transform = p_base_transform;
	_render_item_state.light_region.reset();

	// batch break must be preserved over the different z indices,
	// to prevent joining to an item on a previous index if not allowed
	_render_item_state.join_batch_break = false;

	// whether to join across z indices depends on whether there are z ranged lights.
	// joined z_index items can be wrongly classified with z ranged lights.
	bdata.join_across_z_indices = true;

	int light_count = 0;
	while (p_light) {
		light_count++;

		if ((p_light->z_min != VS::CANVAS_ITEM_Z_MIN) || (p_light->z_max != VS::CANVAS_ITEM_Z_MAX)) {
			// prevent joining across z indices. This would have caused visual regressions
			bdata.join_across_z_indices = false;
		}

		p_light = p_light->next_ptr;
	}

	// can't use the light region bitfield if there are too many lights
	// hopefully most games won't blow this limit..
	// if they do they will work but it won't batch join items just in case
	if (light_count > 64) {
		_render_item_state.light_region.too_many_lights = true;
	}
}

PREAMBLE(void)::batch_canvas_render_items_end() {
	if (!bdata.settings_use_batching) {
		return;
	}

	join_sorted_items();

#if defined(TOOLS_ENABLED) && defined(DEBUG_ENABLED)
	if (bdata.diagnose_frame) {
		bdata.frame_string += "items\n";
	}
#endif

	// batching render is deferred until after going through all the z_indices, joining all the items
	get_this()->canvas_render_items_implementation(nullptr, 0, _render_item_state.item_group_modulate,
			_render_item_state.item_group_light,
			_render_item_state.item_group_base_transform);

	bdata.items_joined.reset();
	bdata.item_refs.reset();
	bdata.sort_items.reset();
}

PREAMBLE(void)::batch_canvas_render_items(RasterizerCanvas::Item *p_item_list, int p_z, const Color &p_modulate, RasterizerCanvas::Light *p_light, const Transform2D &p_base_transform) {
	// stage 1 : join similar items, so that their state changes are not repeated,
	// and commands from joined items can be batched together
	if (bdata.settings_use_batching) {
		record_items(p_item_list, p_z);
		return;
	}

	// only legacy renders at this stage, batched renderer doesn't render until canvas_render_items_end()
	get_this()->canvas_render_items_implementation(p_item_list, p_z, p_modulate, p_light, p_base_transform);
}

// Default batches will not occur in software transform only items
// EXCEPT IN THE CASE OF SINGLE RECTS (and this may well not occur, check the logic in prefill_join_item TYPE_RECT)
// but can occur where transform commands have been sent during hardware batch
PREAMBLE(void)::_prefill_default_batch(FillState &r_fill_state, int p_command_num, const RasterizerCanvas::Item &p_item) {
	if (r_fill_state.curr_batch->type == RasterizerStorageCommon::BT_DEFAULT) {
		// don't need to flush an extra transform command?
		if (!r_fill_state.transform_extra_command_number_p1) {
			// another default command, just add to the existing batch
			r_fill_state.curr_batch->num_commands++;

			// Note this is getting hit, needs investigation as to whether this is a bug or a false flag
			// DEV_CHECK_ONCE(r_fill_state.curr_batch->num_commands <= p_command_num);
		} else {
#if defined(TOOLS_ENABLED) && defined(DEBUG_ENABLED)
			if (r_fill_state.transform_extra_command_number_p1 != p_command_num) {
				WARN_PRINT_ONCE("_prefill_default_batch : transform_extra_command_number_p1 != p_command_num");
			}
#endif
			// if the first member of the batch is a transform we have to be careful
			if (!r_fill_state.curr_batch->num_commands) {
				// there can be leading useless extra transforms (sometimes happens with debug collision polys)
				// we need to rejig the first_command for the first useful transform
				r_fill_state.curr_batch->first_command += r_fill_state.transform_extra_command_number_p1 - 1;
			}

			// we do have a pending extra transform command to flush
			// either the extra transform is in the prior command, or not, in which case we need 2 batches
			r_fill_state.curr_batch->num_commands += 2;

			r_fill_state.transform_extra_command_number_p1 = 0; // mark as sent
			r_fill_state.extra_matrix_sent = true;

			// the original mode should always be hardware transform ..
			// test this assumption
			//CRASH_COND(r_fill_state.orig_transform_mode != TM_NONE);
			r_fill_state.transform_mode = r_fill_state.orig_transform_mode;

			// do we need to restore anything else?
		}
	} else {
		// end of previous different type batch, so start new default batch

		// first consider whether there is a dirty extra matrix to send
		if (r_fill_state.transform_extra_command_number_p1) {
			// get which command the extra is in, and blank all the records as it no longer is stored CPU side
			int extra_command = r_fill_state.transform_extra_command_number_p1 - 1; // plus 1 based
			r_fill_state.transform_extra_command_number_p1 = 0;
			r_fill_state.extra_matrix_sent = true;

			// send the extra to the GPU in a batch
			r_fill_state.curr_batch = _batch_request_new();
			r_fill_state.curr_batch->type = RasterizerStorageCommon::BT_DEFAULT;
			r_fill_state.curr_batch->first_command = extra_command;
			r_fill_state.curr_batch->num_commands = 1;
			r_fill_state.curr_batch->item = &p_item;

			// revert to the original transform mode
			// e.g. go back to NONE if we were in hardware transform mode
			r_fill_state.transform_mode = r_fill_state.orig_transform_mode;

			// reset the original transform if we are going back to software mode,
			// because the extra is now done on the GPU...
			// (any subsequent extras are sent directly to the GPU, no deferring)
			if (r_fill_state.orig_transform_mode != TM_NONE) {
				r_fill_state.transform_combined = p_item.final_transform;
			}

			// can possibly combine batch with the next one in some cases
			// this is more efficient than having an extra batch especially for the extra
			if ((extra_command + 1) == p_command_num) {
				r_fill_state.curr_batch->num_commands = 2;
				return;
			}
		}

		// start default batch
		r_fill_state.curr_batch = _batch_request_new();
		r_fill_state.curr_batch->type = RasterizerStorageCommon::BT_DEFAULT;
		r_fill_state.curr_batch->first_command = p_command_num;
		r_fill_state.curr_batch->num_commands = 1;
		r_fill_state.curr_batch->item = &p_item;
	}
}

PREAMBLE(int)::_batch_find_or_create_tex(const RID &p_texture, const RID &p_normal, bool p_tile, int p_previous_match) {
	// optimization .. in 99% cases the last matched value will be the same, so no need to traverse the list
	if (p_previous_match > 0) // if it is zero, it will get hit first in the linear search anyway
	{
		const BatchTex &batch_texture = bdata.batch_textures[p_previous_match];

		// note for future reference, if RID implementation changes, this could become more expensive
		if ((batch_texture.RID_texture == p_texture) && (batch_texture.RID_normal == p_normal)) {
			// tiling mode must also match
			bool tiles = batch_texture.tile_mode != BatchTex::TILE_OFF;

			if (tiles == p_tile) {
				// match!
				return p_previous_match;
			}
		}
	}

	// not the previous match .. we will do a linear search ... slower, but should happen
	// not very often except with non-batchable runs, which are going to be slow anyway
	// n.b. could possibly be replaced later by a fast hash table
	for (int n = 0; n < bdata.batch_textures.size(); n++) {
		const BatchTex &batch_texture = bdata.batch_textures[n];
		if ((batch_texture.RID_texture == p_texture) && (batch_texture.RID_normal == p_normal)) {
			// tiling mode must also match
			bool tiles = batch_texture.tile_mode != BatchTex::TILE_OFF;

			if (tiles == p_tile) {
				// match!
				return n;
			}
		}
	}

	// pushing back from local variable .. not ideal but has to use a Vector because non pod
	// due to RIDs
	BatchTex new_batch_tex;
	new_batch_tex.RID_texture = p_texture;
	new_batch_tex.RID_normal = p_normal;

	// get the texture
	typename T_STORAGE::Texture *texture = _get_canvas_texture(p_texture);

	if (texture) {
		// special case, there can be textures with no width or height
		int w = texture->width;
		int h = texture->height;

		if (!w || !h) {
			w = 1;
			h = 1;
		}

		new_batch_tex.tex_pixel_size.x = 1.0 / w;
		new_batch_tex.tex_pixel_size.y = 1.0 / h;
		new_batch_tex.flags = texture->flags;
	} else {
		// maybe doesn't need doing...
		new_batch_tex.tex_pixel_size.x = 1.0f;
		new_batch_tex.tex_pixel_size.y = 1.0f;
		new_batch_tex.flags = 0;
	}

	if (p_tile) {
		if (texture) {
			// default
			new_batch_tex.tile_mode = BatchTex::TILE_NORMAL;

			// no hardware support for non power of 2 tiling
			if (!get_storage()->config.support_npot_repeat_mipmap) {
				if (next_power_of_2(texture->alloc_width) != (unsigned int)texture->alloc_width && next_power_of_2(texture->alloc_height) != (unsigned int)texture->alloc_height) {
					new_batch_tex.tile_mode = BatchTex::TILE_FORCE_REPEAT;
				}
			}
		} else {
			// this should not happen?
			new_batch_tex.tile_mode = BatchTex::TILE_OFF;
		}
	} else {
		new_batch_tex.tile_mode = BatchTex::TILE_OFF;
	}

	// push back
	bdata.batch_textures.push_back(new_batch_tex);

	return bdata.batch_textures.size() - 1;
}

PREAMBLE(void)::batch_constructor() {
	bdata.settings_use_batching = false;

#ifdef GLES_OVER_GL
	use_nvidia_rect_workaround = GLOBAL_GET("rendering/2d/options/use_nvidia_rect_flicker_workaround");
#else
	// Not needed (a priori) on GLES devices
	use_nvidia_rect_workaround = false;
#endif
}

PREAMBLE(void)::batch_initialize() {
	bdata.settings_use_batching = GLOBAL_GET("rendering/batching/options/use_batching");
	bdata.settings_max_join_item_commands = GLOBAL_GET("rendering/batching/parameters/max_join_item_commands");
	bdata.settings_colored_vertex_format_threshold = GLOBAL_GET("rendering/batching/parameters/colored_vertex_format_threshold");
	bdata.settings_item_reordering_lookahead = GLOBAL_GET("rendering/batching/parameters/item_reordering_lookahead");
	bdata.settings_light_max_join_items = GLOBAL_GET("rendering/batching/lights/max_join_items");
	bdata.settings_use_single_rect_fallback = GLOBAL_GET("rendering/batching/options/single_rect_fallback");
	bdata.settings_use_software_skinning = GLOBAL_GET("rendering/2d/options/use_software_skinning");
	bdata.settings_ninepatch_mode = GLOBAL_GET("rendering/2d/options/ninepatch_mode");

	// allow user to override the api usage techniques using project settings
	int send_null_mode = GLOBAL_GET("rendering/2d/opengl/batching_send_null");
	switch (send_null_mode) {
		default: {
			bdata.buffer_mode_batch_upload_send_null = true;
		} break;
		case 1: {
			bdata.buffer_mode_batch_upload_send_null = false;
		} break;
		case 2: {
			bdata.buffer_mode_batch_upload_send_null = true;
		} break;
	}

	int stream_mode = GLOBAL_GET("rendering/2d/opengl/batching_stream");
	switch (stream_mode) {
		default: {
			bdata.buffer_mode_batch_upload_flag_stream = false;
		} break;
		case 1: {
			bdata.buffer_mode_batch_upload_flag_stream = false;
		} break;
		case 2: {
			bdata.buffer_mode_batch_upload_flag_stream = true;
		} break;
	}

	// alternatively only enable uv contract if pixel snap in use,
	// but with this enable bool, it should not be necessary
	bdata.settings_uv_contract = GLOBAL_GET("rendering/batching/precision/uv_contract");
	bdata.settings_uv_contract_amount = (float)GLOBAL_GET("rendering/batching/precision/uv_contract_amount") / 1000000.0f;

	// we can use the threshold to determine whether to turn scissoring off or on
	bdata.settings_scissor_threshold = GLOBAL_GET("rendering/batching/lights/scissor_area_threshold");
	if (bdata.settings_scissor_threshold > 0.999f) {
		bdata.settings_scissor_lights = false;
	} else {
		bdata.settings_scissor_lights = true;

		// apply power of 4 relationship for the area, as most of the important changes
		// will be happening at low values of scissor threshold
		bdata.settings_scissor_threshold *= bdata.settings_scissor_threshold;
		bdata.settings_scissor_threshold *= bdata.settings_scissor_threshold;
	}

	// The sweet spot on my desktop for cache is actually smaller than the max, and this
	// is the default. This saves memory too so we will use it for now, needs testing to see whether this varies according
	// to device / platform.
	int batch_buffer_num_verts_requested = GLOBAL_GET("rendering/batching/parameters/batch_buffer_size");

	// override the use_batching setting in the editor
	// (note that if the editor can't start, you can't change the use_batching project setting!)
	if (Engine::get_singleton()->is_editor_hint()) {
		bool use_in_editor = GLOBAL_GET("rendering/batching/options/use_batching_in_editor");
		bdata.settings_use_batching = use_in_editor;

		// fix some settings in the editor, as the performance not worth the risk
		bdata.settings_use_single_rect_fallback = false;
	}

	// if we are using batching, we will purposefully disable the nvidia workaround.
	// This is because the only reason to use the single rect fallback is the approx 2x speed
	// of the uniform drawing technique. If we used nvidia workaround, speed would be
	// approx equal to the batcher drawing technique (indexed primitive + VB).
	if (bdata.settings_use_batching) {
		use_nvidia_rect_workaround = false;
	}

	// For debugging, if flash is set in project settings, it will flash on alternate frames
	// between the non-batched renderer and the batched renderer,
	// in order to find regressions.
	// This should not be used except during development.
	// make a note of the original choice in case we are flashing on and off the batching
	bdata.settings_use_batching_original_choice = bdata.settings_use_batching;
	bdata.settings_flash_batching = GLOBAL_GET("rendering/batching/debug/flash_batching");
	if (!bdata.settings_use_batching) {
		// no flash when batching turned off
		bdata.settings_flash_batching = false;
	}

	// frame diagnosis. print out the batches every nth frame
	bdata.settings_diagnose_frame = false;
	if (!Engine::get_singleton()->is_editor_hint() && bdata.settings_use_batching) {
		bdata.settings_diagnose_frame = GLOBAL_GET("rendering/batching/debug/diagnose_frame");
	}

	// the maximum num quads in a batch is limited by GLES2. We can have only 16 bit indices,
	// which means we can address a vertex buffer of max size 65535. 4 vertices are needed per quad.

	// Note this determines the memory use by the vertex buffer vector. max quads (65536/4)-1
	// but can be reduced to save memory if really required (will result in more batches though)
	const int max_possible_quads = (65536 / 4) - 1;

	// We must have enough quads to fit in a MultiRect
	const int min_possible_quads = MAX(8, MultiRect::MAX_RECTS); // some reasonable small value

	// value from project settings
	int max_quads = batch_buffer_num_verts_requested / 4;

	bool use_multirect = GLOBAL_GET("rendering/batching/options/use_multirect");
	VisualServerCanvasHelper::_multirect_enabled = (bdata.settings_use_batching && use_multirect);

	// sanity checks
	max_quads = CLAMP(max_quads, min_possible_quads, max_possible_quads);
	bdata.settings_max_join_item_commands = CLAMP(bdata.settings_max_join_item_commands, 0, 65535);
	bdata.settings_colored_vertex_format_threshold = CLAMP(bdata.settings_colored_vertex_format_threshold, 0.0f, 1.0f);
	bdata.settings_scissor_threshold = CLAMP(bdata.settings_scissor_threshold, 0.0f, 1.0f);
	bdata.settings_light_max_join_items = CLAMP(bdata.settings_light_max_join_items, 0, 65535);
	bdata.settings_item_reordering_lookahead = CLAMP(bdata.settings_item_reordering_lookahead, 0, 65535);

	// special case, for colored vertex format threshold.
	// as the comparison is >=, we want to be able to totally turn on or off
	// conversion to colored vertex format at the extremes, so we will force
	// 1.0 to be just above 1.0
	if (bdata.settings_colored_vertex_format_threshold > 0.995f) {
		bdata.settings_colored_vertex_format_threshold = 1.01f;
	}

	// save memory when batching off
	if (!bdata.settings_use_batching) {
		max_quads = 0;
	}

	uint32_t sizeof_batch_vert = sizeof(BatchVertex);

	bdata.max_quads = max_quads;

	// 4 verts per quad
	bdata.vertex_buffer_size_units = max_quads * 4;

	// the index buffer can be longer than 65535, only the indices need to be within this range
	bdata.index_buffer_size_units = max_quads * 6;

	const int max_verts = bdata.vertex_buffer_size_units;

	// this comes out at approx 64K for non-colored vertex buffer, and 128K for colored vertex buffer
	bdata.vertex_buffer_size_bytes = max_verts * sizeof_batch_vert;
	bdata.index_buffer_size_bytes = bdata.index_buffer_size_units * 2; // 16 bit inds

	// For debug purposes, output a string with the batching options.
	if (bdata.settings_use_batching) {
		String batching_options_string = "OpenGL ES 2D Batching: ON\n";
		batching_options_string += "Batching Options:\n";
		batching_options_string += "\tmax_join_item_commands " + itos(bdata.settings_max_join_item_commands) + "\n";
		batching_options_string += "\tcolored_vertex_format_threshold " + String(Variant(bdata.settings_colored_vertex_format_threshold)) + "\n";
		batching_options_string += "\tbatch_buffer_effective_size " + itos(bdata.vertex_buffer_size_units) + "\n";
		batching_options_string += "\tlight_scissor_area_threshold " + String(Variant(bdata.settings_scissor_threshold)) + "\n";
		batching_options_string += "\titem_reordering_lookahead " + itos(bdata.settings_item_reordering_lookahead) + "\n";
		batching_options_string += "\tlight_max_join_items " + itos(bdata.settings_light_max_join_items) + "\n";
		batching_options_string += "\tsingle_rect_fallback " + String(Variant(bdata.settings_use_single_rect_fallback)) + "\n";
		batching_options_string += "\tdebug_flash " + String(Variant(bdata.settings_flash_batching)) + "\n";
		batching_options_string += "\tdiagnose_frame " + String(Variant(bdata.settings_diagnose_frame));
		print_verbose(batching_options_string);
	}

	// create equal number of normal and (max) unit sized verts (as the normal may need to be translated to a larger FVF)
	bdata.vertices.create(max_verts); // 512k
	bdata.unit_vertices.create(max_verts, sizeof(BatchVertexLarge));

	// extra data per vert needed for larger FVFs
	bdata.light_angles.create(max_verts);
	bdata.vertex_colors.create(max_verts);
	bdata.vertex_modulates.create(max_verts);
	bdata.vertex_transforms.create(max_verts);

	// num batches will be auto increased dynamically if required
	bdata.batches.create(1024);
	bdata.batches_temp.create(bdata.batches.max_size());

	// batch textures can also be increased dynamically
	bdata.batch_textures.create(32);
}

PREAMBLE(bool)::_light_scissor_begin(const Rect2 &p_item_rect, const Transform2D &p_light_xform, const Rect2 &p_light_rect) const {
	float area_item = p_item_rect.size.x * p_item_rect.size.y; // double check these are always positive

	// quick reject .. the area of pixels saved can never be more than the area of the item
	if (area_item < bdata.scissor_threshold_area) {
		return false;
	}

	Rect2 cliprect;
	if (!_light_find_intersection(p_item_rect, p_light_xform, p_light_rect, cliprect)) {
		// should not really occur .. but just in case
		cliprect = Rect2(0, 0, 0, 0);
	} else {
		// some conditions not to scissor
		// determine the area (fill rate) that will be saved
		float area_cliprect = cliprect.size.x * cliprect.size.y;
		float area_saved = area_item - area_cliprect;

		// if area saved is too small, don't scissor
		if (area_saved < bdata.scissor_threshold_area) {
			return false;
		}
	}

	int rh = get_storage()->frame.current_rt->height;

	// using the exact size was leading to off by one errors,
	// possibly due to pixel snap. For this reason we will boost
	// the scissor area by 1 pixel, this will take care of any rounding
	// issues, and shouldn't significantly negatively impact performance.
	int y = rh - (cliprect.position.y + cliprect.size.y);
	y += 1; // off by 1 boost before flipping

	if (get_storage()->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_VFLIP]) {
		y = cliprect.position.y;
	}

	get_this()->gl_enable_scissor(cliprect.position.x - 1, y, cliprect.size.width + 2, cliprect.size.height + 2);

	return true;
}

PREAMBLE(bool)::_light_find_intersection(const Rect2 &p_item_rect, const Transform2D &p_light_xform, const Rect2 &p_light_rect, Rect2 &r_cliprect) const {
	// transform light to world space (note this is done in the earlier intersection test, so could
	// be made more efficient)
	Vector2 pts[4] = {
		p_light_xform.xform(p_light_rect.position),
		p_light_xform.xform(Vector2(p_light_rect.position.x + p_light_rect.size.x, p_light_rect.position.y)),
		p_light_xform.xform(Vector2(p_light_rect.position.x, p_light_rect.position.y + p_light_rect.size.y)),
		p_light_xform.xform(Vector2(p_light_rect.position.x + p_light_rect.size.x, p_light_rect.position.y + p_light_rect.size.y)),
	};

	// calculate the light bound rect in world space
	Rect2 lrect(pts[0].x, pts[0].y, 0, 0);
	for (int n = 1; n < 4; n++) {
		lrect.expand_to(pts[n]);
	}

	// intersection between the 2 rects
	// they should probably always intersect, because of earlier check, but just in case...
	if (!p_item_rect.intersects(lrect)) {
		return false;
	}

	// note this does almost the same as Rect2.clip but slightly more efficient for our use case
	r_cliprect.position.x = MAX(p_item_rect.position.x, lrect.position.x);
	r_cliprect.position.y = MAX(p_item_rect.position.y, lrect.position.y);

	Point2 item_rect_end = p_item_rect.position + p_item_rect.size;
	Point2 lrect_end = lrect.position + lrect.size;

	r_cliprect.size.x = MIN(item_rect_end.x, lrect_end.x) - r_cliprect.position.x;
	r_cliprect.size.y = MIN(item_rect_end.y, lrect_end.y) - r_cliprect.position.y;

	return true;
}

PREAMBLE(void)::_calculate_scissor_threshold_area() {
	if (!bdata.settings_scissor_lights) {
		return;
	}

	// scissor area threshold is 0.0 to 1.0 in the settings for ease of use.
	// we need to translate to an absolute area to determine quickly whether
	// to scissor.
	if (bdata.settings_scissor_threshold < 0.0001f) {
		bdata.scissor_threshold_area = -1.0f; // will always pass
	} else {
		// in pixels
		int w = get_storage()->frame.current_rt->width;
		int h = get_storage()->frame.current_rt->height;

		int screen_area = w * h;

		bdata.scissor_threshold_area = bdata.settings_scissor_threshold * screen_area;
	}
}

PREAMBLE(bool)::_prefill_line(RasterizerCanvas::Item::CommandLine *p_line, FillState &r_fill_state, int &r_command_start, int command_num, int command_count, RasterizerCanvas::Item *p_item, bool multiply_final_modulate) {
	bool change_batch = false;

	// we have separate batch types for non and anti aliased lines.
	// You can't batch the different types together.
	RasterizerStorageCommon::BatchType line_batch_type = RasterizerStorageCommon::BT_LINE;
	uint32_t line_batch_flags = RasterizerStorageCommon::BTF_LINE;
#ifdef GLES_OVER_GL
	if (p_line->antialiased) {
		line_batch_type = RasterizerStorageCommon::BT_LINE_AA;
		line_batch_flags = RasterizerStorageCommon::BTF_LINE_AA;
	}
#endif

	// conditions for creating a new batch
	if (r_fill_state.curr_batch->type != line_batch_type) {
		if (r_fill_state.sequence_batch_type_flags & (~line_batch_flags)) {
			// don't allow joining to a different sequence type
			r_command_start = command_num;
			return true;
		}
		r_fill_state.sequence_batch_type_flags |= line_batch_flags;

		change_batch = true;
	}

	// get the baked line color
	Color col = p_line->color;

	if (multiply_final_modulate) {
		col *= r_fill_state.final_modulate;
	}

	BatchColor bcol;
	bcol.set(col);

	// if the color has changed we need a new batch
	// (only single color line batches supported so far)
	if (!change_batch && r_fill_state.curr_batch->color != bcol) {
		change_batch = true;
	}

	// not sure if needed
	r_fill_state.batch_tex_id = -1;

	// try to create vertices BEFORE creating a batch,
	// because if the vertex buffer is full, we need to finish this
	// function, draw what we have so far, and then start a new set of batches

	// request multiple vertices at a time, this is more efficient
	BatchVertex *bvs = bdata.vertices.request(2);
	if (!bvs) {
		// run out of space in the vertex buffer .. finish this function and draw what we have so far
		// return where we got to
		r_command_start = command_num;
		return true;
	}

	if (change_batch) {
		// open new batch (this should never fail, it dynamically grows)
		r_fill_state.curr_batch = _batch_request_new(false);

		r_fill_state.curr_batch->type = line_batch_type;
		r_fill_state.curr_batch->color = bcol;
		// cast is to stop sanitizer benign warning .. watch though in case destination type changes
		r_fill_state.curr_batch->batch_texture_id = (uint16_t)-1;
		r_fill_state.curr_batch->first_command = command_num;
		r_fill_state.curr_batch->num_commands = 1;
		//r_fill_state.curr_batch->first_quad = bdata.total_quads;
		r_fill_state.curr_batch->first_vert = bdata.total_verts;
	} else {
		// we could alternatively do the count when closing a batch .. perhaps more efficient
		r_fill_state.curr_batch->num_commands++;
	}

	// fill the geometry
	Vector2 from = p_line->from;
	Vector2 to = p_line->to;

	const bool use_large_verts = bdata.use_large_verts;

	if ((r_fill_state.transform_mode != TM_NONE) && (!use_large_verts)) {
		_software_transform_vertex(from, r_fill_state.transform_combined);
		_software_transform_vertex(to, r_fill_state.transform_combined);
	}

	bvs[0].pos.set(from);
	bvs[0].uv.set(0, 0); // may not be necessary
	bvs[1].pos.set(to);
	bvs[1].uv.set(0, 0);

	bdata.total_verts += 2;

	return false;
}

//unsigned int _ninepatch_apply_tiling_modes(RasterizerCanvas::Item::CommandNinePatch *p_np, Rect2 &r_source) {
//	unsigned int rect_flags = 0;

//	switch (p_np->axis_x) {
//		default:
//			break;
//		case VisualServer::NINE_PATCH_TILE: {
//			r_source.size.x = p_np->rect.size.x;
//			rect_flags = RasterizerCanvas::CANVAS_RECT_TILE;
//		} break;
//		case VisualServer::NINE_PATCH_TILE_FIT: {
//			// prevent divide by zero (may never happen)
//			if (r_source.size.x) {
//				int units = p_np->rect.size.x / r_source.size.x;
//				if (!units)
//					units++;
//				r_source.size.x = r_source.size.x * units;
//				rect_flags = RasterizerCanvas::CANVAS_RECT_TILE;
//			}
//		} break;
//	}

//	switch (p_np->axis_y) {
//		default:
//			break;
//		case VisualServer::NINE_PATCH_TILE: {
//			r_source.size.y = p_np->rect.size.y;
//			rect_flags = RasterizerCanvas::CANVAS_RECT_TILE;
//		} break;
//		case VisualServer::NINE_PATCH_TILE_FIT: {
//			// prevent divide by zero (may never happen)
//			if (r_source.size.y) {
//				int units = p_np->rect.size.y / r_source.size.y;
//				if (!units)
//					units++;
//				r_source.size.y = r_source.size.y * units;
//				rect_flags = RasterizerCanvas::CANVAS_RECT_TILE;
//			}
//		} break;
//	}

//	return rect_flags;
//}

T_PREAMBLE
template <bool SEND_LIGHT_ANGLES>
bool C_PREAMBLE::_prefill_ninepatch(RasterizerCanvas::Item::CommandNinePatch *p_np, FillState &r_fill_state, int &r_command_start, int command_num, int command_count, RasterizerCanvas::Item *p_item, bool multiply_final_modulate) {
	typename T_STORAGE::Texture *tex = _get_canvas_texture(p_np->texture);

	if (!tex) {
		// FIXME: Handle textureless ninepatch gracefully
		WARN_PRINT("NinePatch without texture not supported yet, skipping.");
		return false;
	}

	if (tex->width == 0 || tex->height == 0) {
		WARN_PRINT("Cannot set empty texture to NinePatch.");
		return false;
	}

	// cope with ninepatch of zero area. These cannot be created by the user interface or gdscript, but can
	// be created programmatically from c++, e.g. by the Godot UI for sliders. We will just not draw these.
	if ((p_np->rect.size.x * p_np->rect.size.y) <= 0.0f) {
		return false;
	}

	// conditions for creating a new batch
	if (r_fill_state.curr_batch->type != RasterizerStorageCommon::BT_RECT) {
		// don't allow joining to a different sequence type
		if (r_fill_state.sequence_batch_type_flags & (~RasterizerStorageCommon::BTF_RECT)) {
			// don't allow joining to a different sequence type
			r_command_start = command_num;
			return true;
		}
	}

	// first check there are enough verts for this to complete successfully
	if (bdata.vertices.size() + (4 * 9) > bdata.vertices.max_size()) {
		// return where we got to
		r_command_start = command_num;
		return true;
	}

	// create a temporary rect so we can reuse the rect routine
	RasterizerCanvas::Item::CommandRect trect;

	trect.texture = p_np->texture;
	trect.normal_map = p_np->normal_map;
	trect.modulate = p_np->color;
	trect.flags = RasterizerCanvas::CANVAS_RECT_REGION;

	//Size2 texpixel_size(1.0f / tex->width, 1.0f / tex->height);

	Rect2 source = p_np->source;
	if (source.size.x == 0 && source.size.y == 0) {
		source.size.x = tex->width;
		source.size.y = tex->height;
	}

	float screen_scale = 1.0f;

	// optional crazy ninepatch scaling mode
	if ((bdata.settings_ninepatch_mode == 1) && (source.size.x != 0) && (source.size.y != 0)) {
		screen_scale = MIN(p_np->rect.size.x / source.size.x, p_np->rect.size.y / source.size.y);
		screen_scale = MIN(1.0, screen_scale);
	}

	// deal with nine patch texture wrapping modes
	// this is switched off because it may not be possible with batching
	// trect.flags |= _ninepatch_apply_tiling_modes(p_np, source);

	// translate to rects
	Rect2 &rt = trect.rect;
	Rect2 &src = trect.source;

	float tex_margin_left = p_np->margin[MARGIN_LEFT];
	float tex_margin_right = p_np->margin[MARGIN_RIGHT];
	float tex_margin_top = p_np->margin[MARGIN_TOP];
	float tex_margin_bottom = p_np->margin[MARGIN_BOTTOM];

	float x[4];
	x[0] = p_np->rect.position.x;
	x[1] = x[0] + (p_np->margin[MARGIN_LEFT] * screen_scale);
	x[3] = x[0] + (p_np->rect.size.x);
	x[2] = x[3] - (p_np->margin[MARGIN_RIGHT] * screen_scale);

	float y[4];
	y[0] = p_np->rect.position.y;
	y[1] = y[0] + (p_np->margin[MARGIN_TOP] * screen_scale);
	y[3] = y[0] + (p_np->rect.size.y);
	y[2] = y[3] - (p_np->margin[MARGIN_BOTTOM] * screen_scale);

	float u[4];
	u[0] = source.position.x;
	u[1] = u[0] + tex_margin_left;
	u[3] = u[0] + source.size.x;
	u[2] = u[3] - tex_margin_right;

	float v[4];
	v[0] = source.position.y;
	v[1] = v[0] + tex_margin_top;
	v[3] = v[0] + source.size.y;
	v[2] = v[3] - tex_margin_bottom;

	// Some protection for the use of ninepatches with rect size smaller than margin size.
	// Note these cannot be produced by the UI, only programmatically, and the results
	// are somewhat undefined, because the margins overlap.
	// Ninepatch get_minimum_size()	forces minimum size to be the sum of the margins.
	// So this should occur very rarely if ever. Consider commenting these 4 lines out for higher speed
	// in ninepatches.
	x[1] = MIN(x[1], x[3]);
	x[2] = MIN(x[2], x[3]);
	y[1] = MIN(y[1], y[3]);
	y[2] = MIN(y[2], y[3]);

	// temporarily override to prevent single rect fallback
	bool single_rect_fallback = bdata.settings_use_single_rect_fallback;
	bdata.settings_use_single_rect_fallback = false;

	// each line of the ninepatch
	for (int line = 0; line < 3; line++) {
		rt.position = Vector2(x[0], y[line]);
		rt.size = Vector2(x[1] - x[0], y[line + 1] - y[line]);
		src.position = Vector2(u[0], v[line]);
		src.size = Vector2(u[1] - u[0], v[line + 1] - v[line]);
		_prefill_rect<SEND_LIGHT_ANGLES>(&trect, r_fill_state, r_command_start, command_num, command_count, nullptr, p_item, multiply_final_modulate);

		if ((line == 1) && (!p_np->draw_center)) {
			;
		} else {
			rt.position.x = x[1];
			rt.size.x = x[2] - x[1];
			src.position.x = u[1];
			src.size.x = u[2] - u[1];
			_prefill_rect<SEND_LIGHT_ANGLES>(&trect, r_fill_state, r_command_start, command_num, command_count, nullptr, p_item, multiply_final_modulate);
		}

		rt.position.x = x[2];
		rt.size.x = x[3] - x[2];
		src.position.x = u[2];
		src.size.x = u[3] - u[2];
		_prefill_rect<SEND_LIGHT_ANGLES>(&trect, r_fill_state, r_command_start, command_num, command_count, nullptr, p_item, multiply_final_modulate);
	}

	// restore single rect fallback
	bdata.settings_use_single_rect_fallback = single_rect_fallback;
	return false;
}

T_PREAMBLE
template <bool SEND_LIGHT_ANGLES>
bool C_PREAMBLE::_prefill_polygon(RasterizerCanvas::Item::CommandPolygon *p_poly, FillState &r_fill_state, int &r_command_start, int command_num, int command_count, RasterizerCanvas::Item *p_item, bool multiply_final_modulate) {
	bool change_batch = false;

	// conditions for creating a new batch
	if (r_fill_state.curr_batch->type != RasterizerStorageCommon::BT_POLY) {
		// don't allow joining to a different sequence type
		if (r_fill_state.sequence_batch_type_flags & (~RasterizerStorageCommon::BTF_POLY)) {
			// don't allow joining to a different sequence type
			r_command_start = command_num;
			return true;
		}
		r_fill_state.sequence_batch_type_flags |= RasterizerStorageCommon::BTF_POLY;

		change_batch = true;
	}

	int num_inds = p_poly->indices.size();

	// nothing to draw?
	if (!num_inds || !p_poly->points.size()) {
		return false;
	}

	// we aren't using indices, so will transform verts more than once .. less efficient.
	// could be done with a temporary vertex buffer
	BatchVertex *bvs = bdata.vertices.request(num_inds);
	if (!bvs) {
		// run out of space in the vertex buffer
		// check for special case where the batching buffer is simply not big enough to fit this primitive.
		if (!bdata.vertices.size()) {
			// can't draw, ignore the primitive, otherwise we would enter an infinite loop
			WARN_PRINT_ONCE("poly has too many indices to draw, increase batch buffer size");
			return false;
		}

		// .. finish this function and draw what we have so far
		// return where we got to
		r_command_start = command_num;
		return true;
	}

	BatchColor *vertex_colors = bdata.vertex_colors.request(num_inds);
	RAST_DEBUG_ASSERT(vertex_colors);

	// are we using large FVF?
	////////////////////////////////////
	const bool use_large_verts = bdata.use_large_verts;
	const bool use_modulate = bdata.use_modulate;

	BatchColor *vertex_modulates = nullptr;
	if (use_modulate) {
		vertex_modulates = bdata.vertex_modulates.request(num_inds);
		RAST_DEBUG_ASSERT(vertex_modulates);
		// precalc the vertex modulate (will be shared by all verts)
		// we store the modulate as an attribute in the fvf rather than a uniform
		vertex_modulates[0].set(r_fill_state.final_modulate);
	}

	BatchTransform *pBT = nullptr;
	if (use_large_verts) {
		pBT = bdata.vertex_transforms.request(num_inds);
		RAST_DEBUG_ASSERT(pBT);
		// precalc the batch transform (will be shared by all verts)
		// we store the transform as an attribute in the fvf rather than a uniform
		const Transform2D &tr = r_fill_state.transform_combined;

		pBT[0].translate.set(tr.elements[2]);
		pBT[0].basis[0].set(tr.elements[0][0], tr.elements[0][1]);
		pBT[0].basis[1].set(tr.elements[1][0], tr.elements[1][1]);
	}
	////////////////////////////////////

	// the modulate is always baked
	Color modulate;
	if (multiply_final_modulate) {
		modulate = r_fill_state.final_modulate;
	} else {
		modulate = Color(1, 1, 1, 1);
	}

	int old_batch_tex_id = r_fill_state.batch_tex_id;
	r_fill_state.batch_tex_id = _batch_find_or_create_tex(p_poly->texture, p_poly->normal_map, false, old_batch_tex_id);

	// conditions for creating a new batch
	if (old_batch_tex_id != r_fill_state.batch_tex_id) {
		change_batch = true;
	}

	// N.B. polygons don't have color thus don't need a batch change with color
	// This code is left as reference in case of problems.
	//	if (!r_fill_state.curr_batch->color.equals(modulate)) {
	//		change_batch = true;
	//		bdata.total_color_changes++;
	//	}

	if (change_batch) {
		// put the tex pixel size  in a local (less verbose and can be a register)
		const BatchTex &batchtex = bdata.batch_textures[r_fill_state.batch_tex_id];
		batchtex.tex_pixel_size.to(r_fill_state.texpixel_size);

		if (bdata.settings_uv_contract) {
			r_fill_state.contract_uvs = (batchtex.flags & VS::TEXTURE_FLAG_FILTER) == 0;
		}

		// open new batch (this should never fail, it dynamically grows)
		r_fill_state.curr_batch = _batch_request_new(false);

		r_fill_state.curr_batch->type = RasterizerStorageCommon::BT_POLY;

		// modulate unused except for debugging?
		r_fill_state.curr_batch->color.set(modulate);
		r_fill_state.curr_batch->batch_texture_id = r_fill_state.batch_tex_id;
		r_fill_state.curr_batch->first_command = command_num;
		r_fill_state.curr_batch->num_commands = num_inds;
		//		r_fill_state.curr_batch->num_elements = num_inds;
		r_fill_state.curr_batch->first_vert = bdata.total_verts;
	} else {
		// we could alternatively do the count when closing a batch .. perhaps more efficient
		r_fill_state.curr_batch->num_commands += num_inds;
	}

	// PRECALCULATE THE COLORS (as there may be less colors than there are indices
	// in either hardware or software paths)
	BatchColor vcol;
	int num_verts = p_poly->points.size();

	// in special cases, only 1 color is specified by convention, so we want to preset this
	// to use in all verts.
	if (p_poly->colors.size()) {
		vcol.set(p_poly->colors[0] * modulate);
	} else {
		// color is undefined, use modulate color straight
		vcol.set(modulate);
	}

	BatchColor *precalced_colors = (BatchColor *)alloca(num_verts * sizeof(BatchColor));

	// two stage, super efficient setup of precalculated colors
	int num_colors_specified = p_poly->colors.size();

	for (int n = 0; n < num_colors_specified; n++) {
		vcol.set(p_poly->colors[n] * modulate);
		precalced_colors[n] = vcol;
	}
	for (int n = num_colors_specified; n < num_verts; n++) {
		precalced_colors[n] = vcol;
	}

	if (!_software_skin_poly(p_poly, p_item, bvs, vertex_colors, r_fill_state, precalced_colors)) {
		bool software_transform = (r_fill_state.transform_mode != TM_NONE) && (!use_large_verts);

		for (int n = 0; n < num_inds; n++) {
			int ind = p_poly->indices[n];

			DEV_CHECK_ONCE(ind < p_poly->points.size());

			// recover at runtime from invalid polys (the editor may send invalid polys)
			if ((unsigned int)ind >= (unsigned int)num_verts) {
				// will recover as long as there is at least one vertex.
				// if there are no verts, we will have quick rejected earlier in this function
				ind = 0;
			}

			// this could be moved outside the loop
			if (software_transform) {
				Vector2 pos = p_poly->points[ind];
				_software_transform_vertex(pos, r_fill_state.transform_combined);
				bvs[n].pos.set(pos.x, pos.y);
			} else {
				const Point2 &pos = p_poly->points[ind];
				bvs[n].pos.set(pos.x, pos.y);
			}

			if (ind < p_poly->uvs.size()) {
				const Point2 &uv = p_poly->uvs[ind];
				bvs[n].uv.set(uv.x, uv.y);
			} else {
				bvs[n].uv.set(0.0f, 0.0f);
			}

			vertex_colors[n] = precalced_colors[ind];

			if (use_modulate) {
				vertex_modulates[n] = vertex_modulates[0];
			}

			if (use_large_verts) {
				// reuse precalced transform (same for each vertex within polygon)
				pBT[n] = pBT[0];
			}
		}
	} // if not software skinning
	else {
		// software skinning extra passes
		if (use_modulate) {
			for (int n = 0; n < num_inds; n++) {
				vertex_modulates[n] = vertex_modulates[0];
			}
		}
		// not sure if this will produce garbage if software skinning is changing vertex pos
		// in the shader, but is included for completeness
		if (use_large_verts) {
			for (int n = 0; n < num_inds; n++) {
				pBT[n] = pBT[0];
			}
		}
	}

	// increment total vert count
	bdata.total_verts += num_inds;

	return false;
}

PREAMBLE(bool)::_software_skin_poly(RasterizerCanvas::Item::CommandPolygon *p_poly, RasterizerCanvas::Item *p_item, BatchVertex *bvs, BatchColor *vertex_colors, const FillState &p_fill_state, const BatchColor *p_precalced_colors) {
	//	alternatively could check get_this()->state.using_skeleton
	if (p_item->skeleton == RID()) {
		return false;
	}

	int num_inds = p_poly->indices.size();
	int num_verts = p_poly->points.size();

	RID skeleton = p_item->skeleton;
	int bone_count = RasterizerStorage::base_singleton->skeleton_get_bone_count(skeleton);

	// we want a temporary buffer of positions to transform
	Vector2 *pTemps = (Vector2 *)alloca(num_verts * sizeof(Vector2));
	memset((void *)pTemps, 0, num_verts * sizeof(Vector2));

	// only the inverse appears to be needed
	const Transform2D &skel_trans_inv = p_fill_state.skeleton_base_inverse_xform;
	// we can't get this from the state, because more than one skeleton item may have been joined together..
	// we need to handle the base skeleton on a per item basis as the joined item is rendered.
	// const Transform2D &skel_trans = get_this()->state.skeleton_transform;
	// const Transform2D &skel_trans_inv = get_this()->state.skeleton_transform_inverse;

	// get the bone transforms.
	// this is not ideal because we don't know in advance which bones are needed
	// for any particular poly, but depends how cheap the skeleton_bone_get_transform_2d call is
	Transform2D *bone_transforms = (Transform2D *)alloca(bone_count * sizeof(Transform2D));
	for (int b = 0; b < bone_count; b++) {
		bone_transforms[b] = RasterizerStorage::base_singleton->skeleton_bone_get_transform_2d(skeleton, b);
	}

	if (num_verts && (p_poly->bones.size() == num_verts * 4) && (p_poly->weights.size() == p_poly->bones.size())) {
		// instead of using the p_item->xform we use the final transform,
		// because we want the poly transform RELATIVE to the base skeleton.
		Transform2D item_transform = skel_trans_inv * p_item->final_transform;

		Transform2D item_transform_inv = item_transform.affine_inverse();

		for (int n = 0; n < num_verts; n++) {
			const Vector2 &src_pos = p_poly->points[n];
			Vector2 &dst_pos = pTemps[n];

			// there can be an offset on the polygon at rigging time, this has to be accounted for
			// note it may be possible that this could be concatenated with the bone transforms to save extra transforms - not sure yet
			Vector2 src_pos_back_transformed = item_transform.xform(src_pos);

			float total_weight = 0.0f;

			for (int k = 0; k < 4; k++) {
				int bone_id = p_poly->bones[n * 4 + k];
				float weight = p_poly->weights[n * 4 + k];
				if (weight == 0.0f) {
					continue;
				}

				total_weight += weight;

				DEV_CHECK_ONCE(bone_id < bone_count);
				const Transform2D &bone_tr = bone_transforms[bone_id];

				Vector2 pos = bone_tr.xform(src_pos_back_transformed);

				dst_pos += pos * weight;
			}

			// this is some unexplained weirdness with verts with no weights,
			// but it seemed to work for the example project ... watch for regressions
			if (total_weight < 0.01f) {
				dst_pos = src_pos;
			} else {
				dst_pos /= total_weight;

				// retransform back from the poly offset space
				dst_pos = item_transform_inv.xform(dst_pos);
			}
		}

	} // if bone format matches
	else {
		// not rigged properly, just copy the verts directly
		for (int n = 0; n < num_verts; n++) {
			const Vector2 &src_pos = p_poly->points[n];
			Vector2 &dst_pos = pTemps[n];

			dst_pos = src_pos;
		}
	}

	// software transform with combined matrix?
	if (p_fill_state.transform_mode != TM_NONE) {
		for (int n = 0; n < num_verts; n++) {
			Vector2 &dst_pos = pTemps[n];
			_software_transform_vertex(dst_pos, p_fill_state.transform_combined);
		}
	}

	// output to the batch verts
	for (int n = 0; n < num_inds; n++) {
		int ind = p_poly->indices[n];

		DEV_CHECK_ONCE(ind < num_verts);

		// recover at runtime from invalid polys (the editor may send invalid polys)
		if ((unsigned int)ind >= (unsigned int)num_verts) {
			// will recover as long as there is at least one vertex.
			// if there are no verts, we will have quick rejected earlier in this function
			ind = 0;
		}

		const Point2 &pos = pTemps[ind];
		bvs[n].pos.set(pos.x, pos.y);

		if (ind < p_poly->uvs.size()) {
			const Point2 &uv = p_poly->uvs[ind];
			bvs[n].uv.set(uv.x, uv.y);
		} else {
			bvs[n].uv.set(0.0f, 0.0f);
		}

		vertex_colors[n] = p_precalced_colors[ind];
	}

	return true;
}

T_PREAMBLE
template <bool SEND_LIGHT_ANGLES>
bool C_PREAMBLE::_prefill_rect(RasterizerCanvas::Item::CommandRect *rect, FillState &r_fill_state, int &r_command_start, int command_num, int command_count, RasterizerCanvas::Item::Command *const *commands, RasterizerCanvas::Item *p_item, bool multiply_final_modulate) {
	bool change_batch = false;

	// conditions for creating a new batch
	if (r_fill_state.curr_batch->type != RasterizerStorageCommon::BT_RECT) {
		// don't allow joining to a different sequence type
		if (r_fill_state.sequence_batch_type_flags & (~RasterizerStorageCommon::BTF_RECT)) {
			// don't allow joining to a different sequence type
			r_command_start = command_num;
			return true;
		}
		r_fill_state.sequence_batch_type_flags |= RasterizerStorageCommon::BTF_RECT;

		change_batch = true;

		// check for special case if there is only a single or small number of rects,
		// in which case we will use the legacy default rect renderer
		// because it is faster for single rects

		// we only want to do this if not a joined item with more than 1 item,
		// because joined items with more than 1, the command * will be incorrect
		// NOTE - this is assuming that use_hardware_transform means that it is a non-joined item!!
		// If that assumption is incorrect this will go horribly wrong.
		if (bdata.settings_use_single_rect_fallback && r_fill_state.is_single_item) {
			bool is_single_rect = false;
			int command_num_next = command_num + 1;
			if (command_num_next < command_count) {
				RasterizerCanvas::Item::Command *command_next = commands[command_num_next];
				if ((command_next->type != RasterizerCanvas::Item::Command::TYPE_RECT) && (command_next->type != RasterizerCanvas::Item::Command::TYPE_TRANSFORM)) {
					is_single_rect = true;
				}
			} else {
				is_single_rect = true;
			}
			// if it is a rect on its own, do exactly the same as the default routine
			if (is_single_rect) {
				_prefill_default_batch(r_fill_state, command_num, *p_item);
				return false;
			}
		} // if use hardware transform
	}

	// try to create vertices BEFORE creating a batch,
	// because if the vertex buffer is full, we need to finish this
	// function, draw what we have so far, and then start a new set of batches

	// request FOUR vertices at a time, this is more efficient
	BatchVertex *bvs = bdata.vertices.request(4);
	if (!bvs) {
		// run out of space in the vertex buffer .. finish this function and draw what we have so far
		// return where we got to
		r_command_start = command_num;
		return true;
	}

	// are we using large FVF?
	const bool use_large_verts = bdata.use_large_verts;
	const bool use_modulate = bdata.use_modulate;

	Color col = rect->modulate;

	// use_modulate and use_large_verts should have been checked in the calling prefill_item function.
	// we don't want to apply the modulate on the CPU if it is stored in the vertex format, it will
	// be applied in the shader
	if (multiply_final_modulate) {
		col *= r_fill_state.final_modulate;
	}

	// instead of doing all the texture preparation for EVERY rect,
	// we build a list of texture combinations and do this once off.
	// This means we have a potentially rather slow step to identify which texture combo
	// using the RIDs.
	int old_batch_tex_id = r_fill_state.batch_tex_id;
	r_fill_state.batch_tex_id = _batch_find_or_create_tex(rect->texture, rect->normal_map, rect->flags & RasterizerCanvas::CANVAS_RECT_TILE, old_batch_tex_id);

	//r_fill_state.use_light_angles = send_light_angles;
	if (SEND_LIGHT_ANGLES) {
		bdata.use_light_angles = true;
	}

	// conditions for creating a new batch
	if (old_batch_tex_id != r_fill_state.batch_tex_id) {
		change_batch = true;
	}

	// we need to treat color change separately because we need to count these
	// to decide whether to switch on the fly to colored vertices.
	if (!change_batch && !r_fill_state.curr_batch->color.equals(col)) {
		change_batch = true;
		bdata.total_color_changes++;
	}

	if (change_batch) {
		// put the tex pixel size  in a local (less verbose and can be a register)
		const BatchTex &batchtex = bdata.batch_textures[r_fill_state.batch_tex_id];
		batchtex.tex_pixel_size.to(r_fill_state.texpixel_size);

		if (bdata.settings_uv_contract) {
			r_fill_state.contract_uvs = (batchtex.flags & VS::TEXTURE_FLAG_FILTER) == 0;
		}

		// need to preserve texpixel_size between items
		//r_fill_state.texpixel_size = r_fill_state.texpixel_size;

		// open new batch (this should never fail, it dynamically grows)
		r_fill_state.curr_batch = _batch_request_new(false);

		r_fill_state.curr_batch->type = RasterizerStorageCommon::BT_RECT;
		r_fill_state.curr_batch->color.set(col);
		r_fill_state.curr_batch->batch_texture_id = r_fill_state.batch_tex_id;
		r_fill_state.curr_batch->first_command = command_num;
		r_fill_state.curr_batch->num_commands = 1;
		//r_fill_state.curr_batch->first_quad = bdata.total_quads;
		r_fill_state.curr_batch->first_vert = bdata.total_verts;
	} else {
		// we could alternatively do the count when closing a batch .. perhaps more efficient
		r_fill_state.curr_batch->num_commands++;
	}

	// fill the quad geometry
	Vector2 mins = rect->rect.position;

	if (r_fill_state.transform_mode == TM_TRANSLATE) {
		if (!use_large_verts) {
			_software_transform_vertex(mins, r_fill_state.transform_combined);
		}
	}

	Vector2 maxs = mins + rect->rect.size;

	// just aliases
	BatchVertex *bA = &bvs[0];
	BatchVertex *bB = &bvs[1];
	BatchVertex *bC = &bvs[2];
	BatchVertex *bD = &bvs[3];

	bA->pos.x = mins.x;
	bA->pos.y = mins.y;

	bB->pos.x = maxs.x;
	bB->pos.y = mins.y;

	bC->pos.x = maxs.x;
	bC->pos.y = maxs.y;

	bD->pos.x = mins.x;
	bD->pos.y = maxs.y;

	// possibility of applying flips here for normal mapping .. but they don't seem to be used
	if (rect->rect.size.x < 0) {
		SWAP(bA->pos, bB->pos);
		SWAP(bC->pos, bD->pos);
	}
	if (rect->rect.size.y < 0) {
		SWAP(bA->pos, bD->pos);
		SWAP(bB->pos, bC->pos);
	}

	if (r_fill_state.transform_mode == TM_ALL) {
		if (!use_large_verts) {
			_software_transform_vertex(bA->pos, r_fill_state.transform_combined);
			_software_transform_vertex(bB->pos, r_fill_state.transform_combined);
			_software_transform_vertex(bC->pos, r_fill_state.transform_combined);
			_software_transform_vertex(bD->pos, r_fill_state.transform_combined);
		}
	}

	// uvs
	Vector2 src_min;
	Vector2 src_max;
	if (rect->flags & RasterizerCanvas::CANVAS_RECT_REGION) {
		src_min = rect->source.position;
		src_max = src_min + rect->source.size;

		src_min *= r_fill_state.texpixel_size;
		src_max *= r_fill_state.texpixel_size;

		const float uv_epsilon = bdata.settings_uv_contract_amount;

		// nudge offset for the maximum to prevent precision error on GPU reading into line outside the source rect
		// this is very difficult to get right.
		if (r_fill_state.contract_uvs) {
			src_min.x += uv_epsilon;
			src_min.y += uv_epsilon;
			src_max.x -= uv_epsilon;
			src_max.y -= uv_epsilon;
		}
	} else {
		src_min = Vector2(0, 0);
		src_max = Vector2(1, 1);
	}

	// 10% faster calculating the max first
	Vector2 uvs[4] = {
		src_min,
		Vector2(src_max.x, src_min.y),
		src_max,
		Vector2(src_min.x, src_max.y),
	};

	// for encoding in light angle
	// flips should be optimized out when not being used for light angle.
	bool flip_h = false;
	bool flip_v = false;

	if (rect->flags & RasterizerCanvas::CANVAS_RECT_TRANSPOSE) {
		SWAP(uvs[1], uvs[3]);
	}

	if (rect->flags & RasterizerCanvas::CANVAS_RECT_FLIP_H) {
		SWAP(uvs[0], uvs[1]);
		SWAP(uvs[2], uvs[3]);
		flip_h = !flip_h;
		flip_v = !flip_v;
	}
	if (rect->flags & RasterizerCanvas::CANVAS_RECT_FLIP_V) {
		SWAP(uvs[0], uvs[3]);
		SWAP(uvs[1], uvs[2]);
		flip_v = !flip_v;
	}

	bA->uv.set(uvs[0]);
	bB->uv.set(uvs[1]);
	bC->uv.set(uvs[2]);
	bD->uv.set(uvs[3]);

	// modulate
	if (use_modulate) {
		// store the final modulate separately from the rect modulate
		BatchColor *pBC = bdata.vertex_modulates.request(4);
		RAST_DEBUG_ASSERT(pBC);
		pBC[0].set(r_fill_state.final_modulate);
		pBC[1] = pBC[0];
		pBC[2] = pBC[0];
		pBC[3] = pBC[0];
	}

	if (use_large_verts) {
		// store the transform separately
		BatchTransform *pBT = bdata.vertex_transforms.request(4);
		RAST_DEBUG_ASSERT(pBT);

		const Transform2D &tr = r_fill_state.transform_combined;

		pBT[0].translate.set(tr.elements[2]);

		pBT[0].basis[0].set(tr.elements[0][0], tr.elements[0][1]);
		pBT[0].basis[1].set(tr.elements[1][0], tr.elements[1][1]);

		pBT[1] = pBT[0];
		pBT[2] = pBT[0];
		pBT[3] = pBT[0];
	}

	if (SEND_LIGHT_ANGLES) {
		// we can either keep the light angles in sync with the verts when writing,
		// or sync them up during translation. We are syncing in translation.
		// N.B. There may be batches that don't require light_angles between batches that do.
		float *angles = bdata.light_angles.request(4);
		RAST_DEBUG_ASSERT(angles);

		float angle = 0.0f;
		const float TWO_PI = Math_PI * 2;

		if (r_fill_state.transform_mode != TM_NONE) {
			const Transform2D &tr = r_fill_state.transform_combined;

			// apply to an x axis
			// the x axis and y axis can be taken directly from the transform (no need to xform identity vectors)
			Vector2 x_axis(tr.elements[0][0], tr.elements[0][1]);

			// have to do a y axis to check for scaling flips
			// this is hassle and extra slowness. We could only allow flips via the flags.
			Vector2 y_axis(tr.elements[1][0], tr.elements[1][1]);

			// has the x / y axis flipped due to scaling?
			float cross = x_axis.cross(y_axis);
			if (cross < 0.0f) {
				flip_v = !flip_v;
			}

			// passing an angle is smaller than a vector, it can be reconstructed in the shader
			angle = x_axis.angle();

			// we don't want negative angles, as negative is used to encode flips.
			// This moves range from -PI to PI to 0 to TWO_PI
			if (angle < 0.0f) {
				angle += TWO_PI;
			}

		} // if transform needed

		// if horizontal flip, angle is shifted by 180 degrees
		if (flip_h) {
			angle += Math_PI;

			// mod to get back to 0 to TWO_PI range
			angle = fmodf(angle, TWO_PI);
		}

		// add 1 (to take care of zero floating point error with sign)
		angle += 1.0f;

		// flip if necessary to indicate a vertical flip in the shader
		if (flip_v) {
			angle *= -1.0f;
		}

		// light angle must be sent for each vert, instead as a single uniform in the uniform draw method
		// this has the benefit of enabling batching with light angles.
		for (int n = 0; n < 4; n++) {
			angles[n] = angle;
		}
	}

	// increment quad count
	bdata.total_quads++;
	bdata.total_verts += 4;

	return false;
}

T_PREAMBLE
template <bool SEND_LIGHT_ANGLES>
bool C_PREAMBLE::_prefill_multirect(RasterizerCanvas::Item::CommandMultiRect *mrect, FillState &r_fill_state, int &r_command_start, int command_num, bool multiply_final_modulate) {
	bool change_batch = false;

	// conditions for creating a new batch
	if (r_fill_state.curr_batch->type != RasterizerStorageCommon::BT_RECT) {
		// don't allow joining to a different sequence type
		if (r_fill_state.sequence_batch_type_flags & (~RasterizerStorageCommon::BTF_RECT)) {
			// don't allow joining to a different sequence type
			r_command_start = command_num;
			return true;
		}
		r_fill_state.sequence_batch_type_flags |= RasterizerStorageCommon::BTF_RECT;

		change_batch = true;
	}

	// try to create vertices BEFORE creating a batch,
	// because if the vertex buffer is full, we need to finish this
	// function, draw what we have so far, and then start a new set of batches

	// request ALL vertices at a time, this is more efficient
	uint32_t total_verts = 4 * mrect->rects.size();

	BatchVertex *bvs = bdata.vertices.request(total_verts);
	if (!bvs) {
		// run out of space in the vertex buffer .. finish this function and draw what we have so far
		// return where we got to
		r_command_start = command_num;

		// Check for an error condition - if we have been creating MultiRects that require more than
		// the maximum number of verts in the buffer, this could cause an infinite loop.
		ERR_FAIL_COND_V(total_verts > bdata.vertex_buffer_size_units, false);
		return true;
	}

	// are we using large FVF?
	const bool use_large_verts = bdata.use_large_verts;
	const bool use_modulate = bdata.use_modulate;

	Color col = mrect->modulate;

	// use_modulate and use_large_verts should have been checked in the calling prefill_item function.
	// we don't want to apply the modulate on the CPU if it is stored in the vertex format, it will
	// be applied in the shader
	if (multiply_final_modulate) {
		col *= r_fill_state.final_modulate;
	}

	// instead of doing all the texture preparation for EVERY rect,
	// we build a list of texture combinations and do this once off.
	// This means we have a potentially rather slow step to identify which texture combo
	// using the RIDs.
	int old_batch_tex_id = r_fill_state.batch_tex_id;
	r_fill_state.batch_tex_id = _batch_find_or_create_tex(mrect->texture, mrect->normal_map, mrect->flags & RasterizerCanvas::CANVAS_RECT_TILE, old_batch_tex_id);

	//r_fill_state.use_light_angles = send_light_angles;
	if (SEND_LIGHT_ANGLES) {
		bdata.use_light_angles = true;
	}

	// conditions for creating a new batch
	if (old_batch_tex_id != r_fill_state.batch_tex_id) {
		change_batch = true;
	}

	// we need to treat color change separately because we need to count these
	// to decide whether to switch on the fly to colored vertices.
	if (!change_batch && !r_fill_state.curr_batch->color.equals(col)) {
		change_batch = true;
		bdata.total_color_changes++;
	}

	uint32_t num_rects = mrect->rects.size();

	if (change_batch) {
		// put the tex pixel size  in a local (less verbose and can be a register)
		const BatchTex &batchtex = bdata.batch_textures[r_fill_state.batch_tex_id];
		batchtex.tex_pixel_size.to(r_fill_state.texpixel_size);

		if (bdata.settings_uv_contract) {
			r_fill_state.contract_uvs = (batchtex.flags & VS::TEXTURE_FLAG_FILTER) == 0;
		}

		// need to preserve texpixel_size between items
		//r_fill_state.texpixel_size = r_fill_state.texpixel_size;

		// open new batch (this should never fail, it dynamically grows)
		r_fill_state.curr_batch = _batch_request_new(false);

		r_fill_state.curr_batch->type = RasterizerStorageCommon::BT_RECT;
		r_fill_state.curr_batch->color.set(col);
		r_fill_state.curr_batch->batch_texture_id = r_fill_state.batch_tex_id;
		r_fill_state.curr_batch->first_command = command_num;
		r_fill_state.curr_batch->num_commands = num_rects;
		//r_fill_state.curr_batch->first_quad = bdata.total_quads;
		r_fill_state.curr_batch->first_vert = bdata.total_verts;
	} else {
		// we could alternatively do the count when closing a batch .. perhaps more efficient
		r_fill_state.curr_batch->num_commands += num_rects;
	}

	// test for simplified pipeline
	const uint8_t disallow_flags = RasterizerCanvas::CANVAS_RECT_TRANSPOSE | RasterizerCanvas::CANVAS_RECT_FLIP_H | RasterizerCanvas::CANVAS_RECT_FLIP_V;
	if ((mrect->flags & RasterizerCanvas::CANVAS_RECT_REGION) && ((mrect->flags & disallow_flags) == 0)) {
		// simplified pipeline
		for (uint32_t n = 0; n < num_rects; n++) {
			const Rect2 &rect = mrect->rects[n];
			const Rect2 &source = mrect->sources[n];

			// fill the quad geometry
			Vector2 mins = rect.position;

			// just aliases
			BatchVertex *bA = &bvs[0];
			BatchVertex *bB = &bvs[1];
			BatchVertex *bC = &bvs[2];
			BatchVertex *bD = &bvs[3];

			// possibility of applying flips here for normal mapping .. but they don't seem to be used
#ifdef TOOLS_ENABLED
			if (rect.size.x < 0) {
				ERR_PRINT_ONCE("MultiRect with negative size detected. Ensure rects are non-negative.");
			}
			if (rect.size.y < 0) {
				ERR_PRINT_ONCE("MultiRect with negative size detected. Ensure rects are non-negative.");
			}
#endif

			if (r_fill_state.transform_mode == TM_TRANSLATE) {
				if (!use_large_verts) {
					_software_transform_vertex(mins, r_fill_state.transform_combined);
				}
			}
			Vector2 maxs = mins + rect.size;

			bA->pos.x = mins.x;
			bA->pos.y = mins.y;

			bB->pos.x = maxs.x;
			bB->pos.y = mins.y;

			bC->pos.x = maxs.x;
			bC->pos.y = maxs.y;

			bD->pos.x = mins.x;
			bD->pos.y = maxs.y;

			if (r_fill_state.transform_mode == TM_ALL) {
				if (!use_large_verts) {
					_software_transform_vertex(bA->pos, r_fill_state.transform_combined);
					_software_transform_vertex(bB->pos, r_fill_state.transform_combined);
					_software_transform_vertex(bC->pos, r_fill_state.transform_combined);
					_software_transform_vertex(bD->pos, r_fill_state.transform_combined);
				}
			}

			// uvs
			Vector2 src_min;
			Vector2 src_max;
			src_min = source.position;
			src_max = src_min + source.size;

			src_min *= r_fill_state.texpixel_size;
			src_max *= r_fill_state.texpixel_size;

			const float uv_epsilon = bdata.settings_uv_contract_amount;

			// nudge offset for the maximum to prevent precision error on GPU reading into line outside the source rect
			// this is very difficult to get right.
			if (r_fill_state.contract_uvs) {
				src_min.x += uv_epsilon;
				src_min.y += uv_epsilon;
				src_max.x -= uv_epsilon;
				src_max.y -= uv_epsilon;
			}

			// 10% faster calculating the max first
			Vector2 uvs[4] = {
				src_min,
				Vector2(src_max.x, src_min.y),
				src_max,
				Vector2(src_min.x, src_max.y),
			};

			bA->uv.set(uvs[0]);
			bB->uv.set(uvs[1]);
			bC->uv.set(uvs[2]);
			bD->uv.set(uvs[3]);

			bvs += 4; // move the destination verts on by 4 each rect
		} // for n through rects

	} else {
		// full pipeline
		for (uint32_t n = 0; n < num_rects; n++) {
			const Rect2 &rect = mrect->rects[n];
			const Rect2 &source = mrect->sources[n];

			// fill the quad geometry
			Vector2 mins = rect.position;

			if (r_fill_state.transform_mode == TM_TRANSLATE) {
				if (!use_large_verts) {
					_software_transform_vertex(mins, r_fill_state.transform_combined);
				}
			}

			Vector2 maxs = mins + rect.size;

			// just aliases
			BatchVertex *bA = &bvs[0];
			BatchVertex *bB = &bvs[1];
			BatchVertex *bC = &bvs[2];
			BatchVertex *bD = &bvs[3];

			bA->pos.x = mins.x;
			bA->pos.y = mins.y;

			bB->pos.x = maxs.x;
			bB->pos.y = mins.y;

			bC->pos.x = maxs.x;
			bC->pos.y = maxs.y;

			bD->pos.x = mins.x;
			bD->pos.y = maxs.y;

			// possibility of applying flips here for normal mapping .. but they don't seem to be used
#ifdef TOOLS_ENABLED
			if (rect.size.x < 0) {
				//SWAP(bA->pos, bB->pos);
				//SWAP(bC->pos, bD->pos);
				ERR_PRINT_ONCE("MultiRect with negative size detected. Ensure rects are non-negative.");
			}
			if (rect.size.y < 0) {
				//SWAP(bA->pos, bD->pos);
				//SWAP(bB->pos, bC->pos);
				ERR_PRINT_ONCE("MultiRect with negative size detected. Ensure rects are non-negative.");
			}
#endif

			if (r_fill_state.transform_mode == TM_ALL) {
				if (!use_large_verts) {
					_software_transform_vertex(bA->pos, r_fill_state.transform_combined);
					_software_transform_vertex(bB->pos, r_fill_state.transform_combined);
					_software_transform_vertex(bC->pos, r_fill_state.transform_combined);
					_software_transform_vertex(bD->pos, r_fill_state.transform_combined);
				}
			}

			// uvs
			Vector2 src_min;
			Vector2 src_max;
			if (mrect->flags & RasterizerCanvas::CANVAS_RECT_REGION) {
				src_min = source.position;
				src_max = src_min + source.size;

				src_min *= r_fill_state.texpixel_size;
				src_max *= r_fill_state.texpixel_size;

				const float uv_epsilon = bdata.settings_uv_contract_amount;

				// nudge offset for the maximum to prevent precision error on GPU reading into line outside the source rect
				// this is very difficult to get right.
				if (r_fill_state.contract_uvs) {
					src_min.x += uv_epsilon;
					src_min.y += uv_epsilon;
					src_max.x -= uv_epsilon;
					src_max.y -= uv_epsilon;
				}
			} else {
				src_min = Vector2(0, 0);
				src_max = Vector2(1, 1);
			}

			// 10% faster calculating the max first
			Vector2 uvs[4] = {
				src_min,
				Vector2(src_max.x, src_min.y),
				src_max,
				Vector2(src_min.x, src_max.y),
			};

			if (mrect->flags & RasterizerCanvas::CANVAS_RECT_TRANSPOSE) {
				SWAP(uvs[1], uvs[3]);
			}

			if (mrect->flags & RasterizerCanvas::CANVAS_RECT_FLIP_H) {
				SWAP(uvs[0], uvs[1]);
				SWAP(uvs[2], uvs[3]);
			}
			if (mrect->flags & RasterizerCanvas::CANVAS_RECT_FLIP_V) {
				SWAP(uvs[0], uvs[3]);
				SWAP(uvs[1], uvs[2]);
			}

			bA->uv.set(uvs[0]);
			bB->uv.set(uvs[1]);
			bC->uv.set(uvs[2]);
			bD->uv.set(uvs[3]);

			bvs += 4; // move the destination verts on by 4 each rect
		} // for n through rects
	} // full pipeline

	// modulate
	if (use_modulate) {
		// store the final modulate separately from the rect modulate
		BatchColor *pBC = bdata.vertex_modulates.request(total_verts);
		RAST_DEBUG_ASSERT(pBC);
		pBC[0].set(r_fill_state.final_modulate);
		for (uint32_t n = 1; n < total_verts; n++) {
			pBC[n] = pBC[0];
		}
	}

	// they will all have the same vertex transforms
	if (use_large_verts) {
		// store the transform separately
		BatchTransform *pBT = bdata.vertex_transforms.request(total_verts);
		RAST_DEBUG_ASSERT(pBT);
		BatchTransform *pBT_first = pBT;

		const Transform2D &tr = r_fill_state.transform_combined;

		pBT[0].translate.set(tr.elements[2]);

		pBT[0].basis[0].set(tr.elements[0][0], tr.elements[0][1]);
		pBT[0].basis[1].set(tr.elements[1][0], tr.elements[1][1]);

		for (uint32_t n = 1; n < num_rects * 4; n++) {
			pBT++;
			*pBT = *pBT_first;
		}
	}

	if (SEND_LIGHT_ANGLES) {
		// SAME FOR ALL
		// for encoding in light angle
		bool flip_h = false;
		bool flip_v = false;

		if (mrect->flags & RasterizerCanvas::CANVAS_RECT_FLIP_H) {
			flip_h = !flip_h;
			flip_v = !flip_v;
		}
		if (mrect->flags & RasterizerCanvas::CANVAS_RECT_FLIP_V) {
			flip_v = !flip_v;
		}

		// we can either keep the light angles in sync with the verts when writing,
		// or sync them up during translation. We are syncing in translation.
		// N.B. There may be batches that don't require light_angles between batches that do.
		float *angles = bdata.light_angles.request(total_verts);
		RAST_DEBUG_ASSERT(angles);

		float angle = 0.0f;
		const float TWO_PI = Math_PI * 2;

		if (r_fill_state.transform_mode != TM_NONE) {
			const Transform2D &tr = r_fill_state.transform_combined;

			// apply to an x axis
			// the x axis and y axis can be taken directly from the transform (no need to xform identity vectors)
			Vector2 x_axis(tr.elements[0][0], tr.elements[0][1]);

			// have to do a y axis to check for scaling flips
			// this is hassle and extra slowness. We could only allow flips via the flags.
			Vector2 y_axis(tr.elements[1][0], tr.elements[1][1]);

			// has the x / y axis flipped due to scaling?
			float cross = x_axis.cross(y_axis);
			if (cross < 0.0f) {
				flip_v = !flip_v;
			}

			// passing an angle is smaller than a vector, it can be reconstructed in the shader
			angle = x_axis.angle();

			// we don't want negative angles, as negative is used to encode flips.
			// This moves range from -PI to PI to 0 to TWO_PI
			if (angle < 0.0f) {
				angle += TWO_PI;
			}

		} // if transform needed

		// if horizontal flip, angle is shifted by 180 degrees
		if (flip_h) {
			angle += Math_PI;

			// mod to get back to 0 to TWO_PI range
			angle = fmodf(angle, TWO_PI);
		}

		// add 1 (to take care of zero floating point error with sign)
		angle += 1.0f;

		// flip if necessary to indicate a vertical flip in the shader
		if (flip_v) {
			angle *= -1.0f;
		}

		// light angle must be sent for each vert, instead as a single uniform in the uniform draw method
		// this has the benefit of enabling batching with light angles.
		for (uint32_t n = 0; n < total_verts; n++) {
			angles[n] = angle;
		}
	}

	// increment quad count
	bdata.total_quads += num_rects;
	bdata.total_verts += total_verts;

	return false;
}

// This function may be called MULTIPLE TIMES for each item, so needs to record how far it has got
PREAMBLE(bool)::prefill_joined_item(FillState &r_fill_state, int &r_command_start, RasterizerCanvas::Item *p_item, RasterizerCanvas::Item *p_current_clip, bool &r_reclip, typename T_STORAGE::Material *p_material) {
	// we will prefill batches and vertices ready for sending in one go to the vertex buffer
	int command_count = p_item->commands.size();
	RasterizerCanvas::Item::Command *const *commands = p_item->commands.ptr();

	// whether to multiply final modulate on the CPU, or pass it in the FVF and apply in the shader
	bool multiply_final_modulate = true;

	if (r_fill_state.is_single_item || bdata.use_modulate || bdata.use_large_verts) {
		multiply_final_modulate = false;
	}

	// start batch is a dummy batch (tex id -1) .. could be made more efficient
	if (!r_fill_state.curr_batch) {
		// allocate dummy batch on the stack, it should always get replaced
		// note that the rest of the structure is uninitialized, this should not matter
		// if the type is checked before anything else.
		r_fill_state.curr_batch = (Batch *)alloca(sizeof(Batch));
		r_fill_state.curr_batch->type = RasterizerStorageCommon::BT_DUMMY;

		// this is assumed to be the case
		//CRASH_COND (r_fill_state.transform_extra_command_number_p1);
	}

	// we need to return which command we got up to, so
	// store this outside the loop
	int command_num;

	// do as many commands as possible until the vertex buffer will be full up
	for (command_num = r_command_start; command_num < command_count; command_num++) {
		RasterizerCanvas::Item::Command *command = commands[command_num];

		switch (command->type) {
			default: {
				_prefill_default_batch(r_fill_state, command_num, *p_item);
			} break;
			case RasterizerCanvas::Item::Command::TYPE_TRANSFORM: {
				// if the extra matrix has been sent already,
				// break this extra matrix software path (as we don't want to unset it on the GPU etc)
				if (r_fill_state.extra_matrix_sent) {
					_prefill_default_batch(r_fill_state, command_num, *p_item);

					// keep track of the combined matrix on the CPU in parallel, in case we use large vertex format
					RasterizerCanvas::Item::CommandTransform *transform = static_cast<RasterizerCanvas::Item::CommandTransform *>(command);
					const Transform2D &extra_matrix = transform->xform;
					r_fill_state.transform_combined = p_item->final_transform * extra_matrix;
				} else {
					// Extra matrix fast path.
					// Instead of sending the command immediately, we store the modified transform (in combined)
					// for software transform, and only flush this transform command if we NEED to (i.e. we want to
					// render some default commands)
					RasterizerCanvas::Item::CommandTransform *transform = static_cast<RasterizerCanvas::Item::CommandTransform *>(command);
					const Transform2D &extra_matrix = transform->xform;

					if (r_fill_state.is_single_item && !r_fill_state.use_attrib_transform) {
						// if we are using hardware transform mode, we have already sent the final transform,
						// so we only want to software transform the extra matrix
						r_fill_state.transform_combined = extra_matrix;
					} else {
						r_fill_state.transform_combined = p_item->final_transform * extra_matrix;
					}
					// after a transform command, always use some form of software transform (either the combined final + extra, or just the extra)
					// until we flush this dirty extra matrix because we need to render default commands.
					r_fill_state.transform_mode = _find_transform_mode(r_fill_state.transform_combined);

					// make a note of which command the dirty extra matrix is store in, so we can send it later
					// if necessary
					r_fill_state.transform_extra_command_number_p1 = command_num + 1; // plus 1 so we can test against zero
				}
			} break;
			case RasterizerCanvas::Item::Command::TYPE_RECT: {
				RasterizerCanvas::Item::CommandRect *rect = static_cast<RasterizerCanvas::Item::CommandRect *>(command);

				// unoptimized - could this be done once per batch / batch texture?
				bool send_light_angles = rect->normal_map != RID();

				bool buffer_full = false;

				// the template params must be explicit for compilation,
				// this forces building the multiple versions of the function.
				if (send_light_angles) {
					buffer_full = _prefill_rect<true>(rect, r_fill_state, r_command_start, command_num, command_count, commands, p_item, multiply_final_modulate);
				} else {
					buffer_full = _prefill_rect<false>(rect, r_fill_state, r_command_start, command_num, command_count, commands, p_item, multiply_final_modulate);
				}

				if (buffer_full) {
					return true;
				}

			} break;
			case RasterizerCanvas::Item::Command::TYPE_MULTIRECT: {
				RasterizerCanvas::Item::CommandMultiRect *mrect = static_cast<RasterizerCanvas::Item::CommandMultiRect *>(command);

				// MultRects with no rects should ideally not be created
				ERR_CONTINUE(!mrect->rects.size());

				bool send_light_angles = mrect->normal_map != RID();

				bool buffer_full = false;

				// the template params must be explicit for compilation,
				// this forces building the multiple versions of the function.
				if (send_light_angles) {
					buffer_full = _prefill_multirect<true>(mrect, r_fill_state, r_command_start, command_num, multiply_final_modulate);
				} else {
					buffer_full = _prefill_multirect<false>(mrect, r_fill_state, r_command_start, command_num, multiply_final_modulate);
				}

				if (buffer_full) {
					return true;
				}

			} break;
			case RasterizerCanvas::Item::Command::TYPE_NINEPATCH: {
				RasterizerCanvas::Item::CommandNinePatch *np = static_cast<RasterizerCanvas::Item::CommandNinePatch *>(command);

				if ((np->axis_x != VisualServer::NINE_PATCH_STRETCH) || (np->axis_y != VisualServer::NINE_PATCH_STRETCH)) {
					// not accelerated
					_prefill_default_batch(r_fill_state, command_num, *p_item);
					continue;
				}

				// unoptimized - could this be done once per batch / batch texture?
				bool send_light_angles = np->normal_map != RID();

				bool buffer_full = false;

				if (send_light_angles) {
					buffer_full = _prefill_ninepatch<true>(np, r_fill_state, r_command_start, command_num, command_count, p_item, multiply_final_modulate);
				} else {
					buffer_full = _prefill_ninepatch<false>(np, r_fill_state, r_command_start, command_num, command_count, p_item, multiply_final_modulate);
				}

				if (buffer_full) {
					return true;
				}

			} break;

			case RasterizerCanvas::Item::Command::TYPE_LINE: {
				RasterizerCanvas::Item::CommandLine *line = static_cast<RasterizerCanvas::Item::CommandLine *>(command);

				if (line->width <= 1) {
					bool buffer_full = _prefill_line(line, r_fill_state, r_command_start, command_num, command_count, p_item, multiply_final_modulate);

					if (buffer_full) {
						return true;
					}
				} else {
					// not accelerated
					_prefill_default_batch(r_fill_state, command_num, *p_item);
				}
			} break;

			case RasterizerCanvas::Item::Command::TYPE_POLYGON: {
				RasterizerCanvas::Item::CommandPolygon *polygon = static_cast<RasterizerCanvas::Item::CommandPolygon *>(command);
#ifdef GLES_OVER_GL
				// anti aliasing not accelerated .. it is problematic because it requires a 2nd line drawn around the outside of each
				// poly, which would require either a second list of indices or a second list of vertices for this step
				bool use_legacy_path = false;

				if (polygon->antialiased) {
					// anti aliasing is also not supported for software skinned meshes.
					// we can't easily revert, so we force software skinned meshes to run through
					// batching path with no AA.
					use_legacy_path = !bdata.settings_use_software_skinning || p_item->skeleton == RID();
				}

				if (use_legacy_path) {
					// not accelerated
					_prefill_default_batch(r_fill_state, command_num, *p_item);
				} else {
#endif
					// not using software skinning?
					if (!bdata.settings_use_software_skinning && get_this()->state.using_skeleton) {
						// not accelerated
						_prefill_default_batch(r_fill_state, command_num, *p_item);
					} else {
						// unoptimized - could this be done once per batch / batch texture?
						bool send_light_angles = polygon->normal_map != RID();

						bool buffer_full = false;

						if (send_light_angles) {
							// polygon with light angles is not yet implemented
							// for batching .. this means software skinned with light angles won't work
							_prefill_default_batch(r_fill_state, command_num, *p_item);
						} else {
							buffer_full = _prefill_polygon<false>(polygon, r_fill_state, r_command_start, command_num, command_count, p_item, multiply_final_modulate);
						}

						if (buffer_full) {
							return true;
						}
					} // if not using hardware skinning path
#ifdef GLES_OVER_GL
				} // if not anti-aliased poly
#endif

			} break;
		}
	}

	// VERY IMPORTANT to return where we got to, because this func may be called multiple
	// times per item.
	// Don't miss out on this step by calling return earlier in the function without setting r_command_start.
	r_command_start = command_num;

	return false;
}

PREAMBLE(void)::flush_render_batches(RasterizerCanvas::Item *p_first_item, RasterizerCanvas::Item *p_current_clip, bool &r_reclip, typename T_STORAGE::Material *p_material, uint32_t p_sequence_batch_type_flags) {
	// some heuristic to decide whether to use colored verts.
	// feel free to tweak this.
	// this could use hysteresis, to prevent jumping between methods
	// .. however probably not necessary
	bdata.use_colored_vertices = false;

	RasterizerStorageCommon::FVF backup_fvf = bdata.fvf;

	// the batch type in this flush can override the fvf from the joined item.
	// The joined item uses the material to determine fvf, assuming a rect...
	// however with custom drawing, lines or polys may be drawn.
	// lines contain no color (this is stored in the batch), and polys contain vertex and color only.
	if (p_sequence_batch_type_flags & (RasterizerStorageCommon::BTF_LINE | RasterizerStorageCommon::BTF_LINE_AA)) {
		// do nothing, use the default regular FVF
		bdata.fvf = RasterizerStorageCommon::FVF_REGULAR;
	} else {
		// switch from regular to colored?
		if (bdata.fvf == RasterizerStorageCommon::FVF_REGULAR) {
			// only check whether to convert if there are quads (prevent divide by zero)
			// and we haven't decided to prevent color baking (due to e.g. MODULATE
			// being used in a shader)
			if (bdata.total_quads && !(bdata.joined_item_batch_flags & RasterizerStorageCommon::PREVENT_COLOR_BAKING)) {
				// minus 1 to prevent single primitives (ratio 1.0) always being converted to colored..
				// in that case it is slightly cheaper to just have the color as part of the batch
				float ratio = (float)(bdata.total_color_changes - 1) / (float)bdata.total_quads;

				// use bigger than or equal so that 0.0 threshold can force always using colored verts
				if (ratio >= bdata.settings_colored_vertex_format_threshold) {
					bdata.use_colored_vertices = true;
					bdata.fvf = RasterizerStorageCommon::FVF_COLOR;
				}
			}

			// if we used vertex colors
			if (bdata.vertex_colors.size()) {
				bdata.use_colored_vertices = true;
				bdata.fvf = RasterizerStorageCommon::FVF_COLOR;
			}

			// needs light angles?
			if (bdata.use_light_angles) {
				bdata.fvf = RasterizerStorageCommon::FVF_LIGHT_ANGLE;
			}
		}

		backup_fvf = bdata.fvf;
	} // if everything else except lines

	// translate if required to larger FVFs
	switch (bdata.fvf) {
		case RasterizerStorageCommon::FVF_UNBATCHED: // should not happen
			break;
		case RasterizerStorageCommon::FVF_REGULAR: // no change
			break;
		case RasterizerStorageCommon::FVF_COLOR: {
			// special case, where vertex colors are used (polys)
			if (!bdata.vertex_colors.size()) {
				_translate_batches_to_larger_FVF<BatchVertexColored, false, false, false>(p_sequence_batch_type_flags);
			} else {
				// normal, reduce number of batches by baking batch colors
				_translate_batches_to_vertex_colored_FVF();
			}
		} break;
		case RasterizerStorageCommon::FVF_LIGHT_ANGLE:
			_translate_batches_to_larger_FVF<BatchVertexLightAngled, true, false, false>(p_sequence_batch_type_flags);
			break;
		case RasterizerStorageCommon::FVF_MODULATED:
			_translate_batches_to_larger_FVF<BatchVertexModulated, true, true, false>(p_sequence_batch_type_flags);
			break;
		case RasterizerStorageCommon::FVF_LARGE:
			_translate_batches_to_larger_FVF<BatchVertexLarge, true, true, true>(p_sequence_batch_type_flags);
			break;
	}

	// send buffers to opengl
	get_this()->_batch_upload_buffers();

#if defined(TOOLS_ENABLED) && defined(DEBUG_ENABLED)
	if (bdata.diagnose_frame) {
		RasterizerCanvas::Item::Command *const *commands = p_first_item->commands.ptr();
		diagnose_batches(commands);
	}
#endif

	get_this()->render_batches(p_current_clip, r_reclip, p_material);

	// if we overrode the fvf for lines, set it back to the joined item fvf
	bdata.fvf = backup_fvf;

	// overwrite source buffers with garbage if error checking
#ifdef RASTERIZER_EXTRA_CHECKS
	_debug_write_garbage();
#endif
}

PREAMBLE(void)::render_joined_item_commands(const BItemJoined &p_bij, RasterizerCanvas::Item *p_current_clip, bool &r_reclip, typename T_STORAGE::Material *p_material, bool p_lit, const RenderItemState &p_ris) {
	RasterizerCanvas::Item *item = nullptr;
	RasterizerCanvas::Item *first_item = bdata.item_refs[p_bij.first_item_ref].item;

	// fill_state and bdata have once off setup per joined item, and a smaller reset on flush
	FillState fill_state;
	fill_state.reset_joined_item(p_bij.is_single_item(), p_bij.use_attrib_transform());

	bdata.reset_joined_item();

	// should this joined item be using large FVF?
	if (p_bij.flags & RasterizerStorageCommon::USE_MODULATE_FVF) {
		bdata.use_modulate = true;
		bdata.fvf = RasterizerStorageCommon::FVF_MODULATED;
	}
	if (p_bij.flags & RasterizerStorageCommon::USE_LARGE_FVF) {
		bdata.use_modulate = true;
		bdata.use_large_verts = true;
		bdata.fvf = RasterizerStorageCommon::FVF_LARGE;
	}

	// make sure the jointed item flags state is up to date, as it is read indirectly in
	// a couple of places from the state rather than from the joined item.
	// we could alternatively make sure to only read directly from the joined item
	// during the render, but it is probably more bug future proof to make sure both
	// are up to date.
	bdata.joined_item_batch_flags = p_bij.flags;

	// in the special case of custom shaders that read from VERTEX (i.e. vertex position)
	// we want to disable software transform of extra matrix
	if (bdata.joined_item_batch_flags & RasterizerStorageCommon::PREVENT_VERTEX_BAKING) {
		fill_state.extra_matrix_sent = true;
	}

	for (unsigned int i = 0; i < p_bij.num_item_refs; i++) {
		const BItemRef &ref = bdata.item_refs[p_bij.first_item_ref + i];
		item = ref.item;

		if (!p_lit) {
			// if not lit we use the complex calculated final modulate
			fill_state.final_modulate = ref.final_modulate;
		} else {
			// if lit we ignore canvas modulate and just use the item modulate
			fill_state.final_modulate = item->final_modulate;
		}

		int command_count = item->commands.size();
		int command_start = 0;

		// ONCE OFF fill state setup, that will be retained over multiple calls to
		// prefill_joined_item()
		fill_state.transform_combined = item->final_transform;

		// calculate skeleton base inverse transform if required for software skinning
		// put in the fill state as this is readily accessible from the software skinner
		if (item->skeleton.is_valid() && bdata.settings_use_software_skinning && get_storage()->skeleton_owner.owns(item->skeleton)) {
			typename T_STORAGE::Skeleton *skeleton = nullptr;
			skeleton = get_storage()->skeleton_owner.get(item->skeleton);

			if (skeleton->use_2d) {
				// with software skinning we still need to know the skeleton inverse transform, the other two aren't needed
				// but are left in for simplicity here
				Transform2D skeleton_transform = p_ris.item_group_base_transform * skeleton->base_transform_2d;
				fill_state.skeleton_base_inverse_xform = skeleton_transform.affine_inverse();
			}
		}

		// decide the initial transform mode, and make a backup
		// in orig_transform_mode in case we need to switch back
		if (fill_state.use_software_transform) {
			fill_state.transform_mode = _find_transform_mode(fill_state.transform_combined);
		} else {
			fill_state.transform_mode = TM_NONE;
		}
		fill_state.orig_transform_mode = fill_state.transform_mode;

		// keep track of when we added an extra matrix
		// so we can defer sending until we see a default command
		fill_state.transform_extra_command_number_p1 = 0;

		while (command_start < command_count) {
			// fill as many batches as possible (until all done, or the vertex buffer is full)
			bool bFull = get_this()->prefill_joined_item(fill_state, command_start, item, p_current_clip, r_reclip, p_material);

			if (bFull) {
				// always pass first item (commands for default are always first item)
				flush_render_batches(first_item, p_current_clip, r_reclip, p_material, fill_state.sequence_batch_type_flags);

				// zero all the batch data ready for a new run
				bdata.reset_flush();

				// don't zero all the fill state, some may need to be preserved
				fill_state.reset_flush();
			}
		}
	}

	// flush if any left
	flush_render_batches(first_item, p_current_clip, r_reclip, p_material, fill_state.sequence_batch_type_flags);

	// zero all the batch data ready for a new run
	bdata.reset_flush();
}

PREAMBLE(void)::_legacy_canvas_item_render_commands(RasterizerCanvas::Item *p_item, RasterizerCanvas::Item *p_current_clip, bool &r_reclip, typename T_STORAGE::Material *p_material) {
	int command_count = p_item->commands.size();

	// legacy .. just create one massive batch and render everything as before
	bdata.batches.reset();
	Batch *batch = _batch_request_new();
	batch->type = RasterizerStorageCommon::BT_DEFAULT;
	batch->num_commands = command_count;
	batch->item = p_item;

	get_this()->render_batches(p_current_clip, r_reclip, p_material);
	bdata.reset_flush();
}

PREAMBLE(void)::record_items(RasterizerCanvas::Item *p_item_list, int p_z) {
	while (p_item_list) {
		BSortItem *s = bdata.sort_items.request_with_grow();

		s->item = p_item_list;
		s->z_index = p_z;

		p_item_list = p_item_list->next;
	}
}

PREAMBLE(void)::join_sorted_items() {
	sort_items();

	int z = VS::CANVAS_ITEM_Z_MIN;
	_render_item_state.item_group_z = z;

	for (int s = 0; s < bdata.sort_items.size(); s++) {
		const BSortItem &si = bdata.sort_items[s];
		RasterizerCanvas::Item *ci = si.item;

		// change z?
		if (si.z_index != z) {
			z = si.z_index;

			// may not be required
			_render_item_state.item_group_z = z;

			// if z ranged lights are present, sometimes we have to disable joining over z_indices.
			// we do this here.
			// Note this restriction may be able to be relaxed with light bitfields, investigate!
			if (!bdata.join_across_z_indices) {
				_render_item_state.join_batch_break = true;
			}
		}

		bool join;

		if (_render_item_state.join_batch_break) {
			// always start a new batch for this item
			join = false;

			// could be another batch break (i.e. prevent NEXT item from joining this)
			// so we still need to run try_join_item
			// even though we know join is false.
			// also we need to run try_join_item for every item because it keeps the state up to date,
			// if we didn't run it the state would be out of date.
			get_this()->try_join_item(ci, _render_item_state, _render_item_state.join_batch_break);
		} else {
			join = get_this()->try_join_item(ci, _render_item_state, _render_item_state.join_batch_break);
		}

		// assume the first item will always return no join
		if (!join) {
			_render_item_state.joined_item = bdata.items_joined.request_with_grow();
			_render_item_state.joined_item->first_item_ref = bdata.item_refs.size();
			_render_item_state.joined_item->num_item_refs = 1;
			_render_item_state.joined_item->bounding_rect = ci->global_rect_cache;
			_render_item_state.joined_item->z_index = z;
			_render_item_state.joined_item->flags = bdata.joined_item_batch_flags;

			// we need some logic to prevent joining items that have vastly different batch types
			_render_item_state.joined_item_batch_type_flags_prev = _render_item_state.joined_item_batch_type_flags_curr;

			// add the reference
			BItemRef *r = bdata.item_refs.request_with_grow();
			r->item = ci;
			// we are storing final_modulate in advance per item reference
			// for baking into vertex colors.
			// this may not be ideal... as we are increasing the size of item reference,
			// but it is stupidly complex to calculate later, which would probably be slower.
			r->final_modulate = _render_item_state.final_modulate;
		} else {
			DEV_ASSERT(_render_item_state.joined_item != nullptr);
			_render_item_state.joined_item->num_item_refs += 1;
			_render_item_state.joined_item->bounding_rect = _render_item_state.joined_item->bounding_rect.merge(ci->global_rect_cache);

			BItemRef *r = bdata.item_refs.request_with_grow();
			r->item = ci;
			r->final_modulate = _render_item_state.final_modulate;

			// joined item references may introduce new flags
			_render_item_state.joined_item->flags |= bdata.joined_item_batch_flags;
		}

	} // for s through sort items
}

PREAMBLE(void)::sort_items() {
	// turned off?
	if (!bdata.settings_item_reordering_lookahead) {
		return;
	}

	for (int s = 0; s < bdata.sort_items.size() - 2; s++) {
		if (sort_items_from(s)) {
#if defined(TOOLS_ENABLED) && defined(DEBUG_ENABLED)
			bdata.stats_items_sorted++;
#endif
		}
	}
}

PREAMBLE(bool)::_sort_items_match(const BSortItem &p_a, const BSortItem &p_b) const {
	const RasterizerCanvas::Item *a = p_a.item;
	const RasterizerCanvas::Item *b = p_b.item;

	if (b->commands.size() != 1) {
		return false;
	}

	// tested outside function
	//	if (a->commands.size() != 1)
	//		return false;

	const RasterizerCanvas::Item::Command &cb = *b->commands[0];
	if ((cb.type != RasterizerCanvas::Item::Command::TYPE_RECT) && (cb.type != RasterizerCanvas::Item::Command::TYPE_MULTIRECT)) {
		return false;
	}

	const RasterizerCanvas::Item::Command &ca = *a->commands[0];
	// tested outside function
	//	if (ca.type != Item::Command::TYPE_RECT)
	//		return false;

	const RasterizerCanvas::Item::CommandRect *rect_a = static_cast<const RasterizerCanvas::Item::CommandRect *>(&ca);
	const RasterizerCanvas::Item::CommandRect *rect_b = static_cast<const RasterizerCanvas::Item::CommandRect *>(&cb);

	if (rect_a->texture != rect_b->texture) {
		return false;
	}

	/* ALTERNATIVE APPROACH NOT LIMITED TO RECTS
const RasterizerCanvas::Item::Command &ca = *a->commands[0];
const RasterizerCanvas::Item::Command &cb = *b->commands[0];

if (ca.type != cb.type)
	return false;

// do textures match?
switch (ca.type)
{
default:
	break;
case RasterizerCanvas::Item::Command::TYPE_RECT:
	{
		const RasterizerCanvas::Item::CommandRect *comm_a = static_cast<const RasterizerCanvas::Item::CommandRect *>(&ca);
		const RasterizerCanvas::Item::CommandRect *comm_b = static_cast<const RasterizerCanvas::Item::CommandRect *>(&cb);
		if (comm_a->texture != comm_b->texture)
			return false;
	}
	break;
case RasterizerCanvas::Item::Command::TYPE_POLYGON:
	{
		const RasterizerCanvas::Item::CommandPolygon *comm_a = static_cast<const RasterizerCanvas::Item::CommandPolygon *>(&ca);
		const RasterizerCanvas::Item::CommandPolygon *comm_b = static_cast<const RasterizerCanvas::Item::CommandPolygon *>(&cb);
		if (comm_a->texture != comm_b->texture)
			return false;
	}
	break;
}
*/

	return true;
}

PREAMBLE(bool)::sort_items_from(int p_start) {
#if defined(TOOLS_ENABLED) && defined(DEBUG_ENABLED)
	ERR_FAIL_COND_V((p_start + 1) >= bdata.sort_items.size(), false);
#endif

	const BSortItem &start = bdata.sort_items[p_start];
	int start_z = start.z_index;

	// check start is the right type for sorting
	if (start.item->commands.size() != 1) {
		return false;
	}
	const RasterizerCanvas::Item::Command &command_start = *start.item->commands[0];
	if ((command_start.type != RasterizerCanvas::Item::Command::TYPE_RECT) && (command_start.type != RasterizerCanvas::Item::Command::TYPE_MULTIRECT)) {
		return false;
	}

	BSortItem &second = bdata.sort_items[p_start + 1];
	if (second.z_index != start_z) {
		// no sorting across z indices (for now)
		return false;
	}

	// if the neighbours are already a good match
	if (_sort_items_match(start, second)) // order is crucial, start first
	{
		return false;
	}

	// local cached aabb
	Rect2 second_AABB = second.item->global_rect_cache;

	// if the start and 2nd items overlap, can do no more
	if (start.item->global_rect_cache.intersects(second_AABB)) {
		return false;
	}

	// disallow sorting over copy back buffer
	if (second.item->copy_back_buffer) {
		return false;
	}

	// which neighbour to test
	int test_last = 2 + bdata.settings_item_reordering_lookahead;
	for (int test = 2; test < test_last; test++) {
		int test_sort_item_id = p_start + test;

		// if we've got to the end of the list, can't sort any more, give up
		if (test_sort_item_id >= bdata.sort_items.size()) {
			return false;
		}

		BSortItem *test_sort_item = &bdata.sort_items[test_sort_item_id];

		// across z indices?
		if (test_sort_item->z_index != start_z) {
			return false;
		}

		RasterizerCanvas::Item *test_item = test_sort_item->item;

		// if the test item overlaps the second item, we can't swap, AT ALL
		// because swapping an item OVER this one would cause artefacts
		if (second_AABB.intersects(test_item->global_rect_cache)) {
			return false;
		}

		// do they match?
		if (!_sort_items_match(start, *test_sort_item)) // order is crucial, start first
		{
			continue;
		}

		// we can only swap if there are no AABB overlaps with sandwiched neighbours
		bool ok = true;

		// start from 2, no need to check 1 as the second has already been checked against this item
		// in the intersection test above
		for (int sn = 2; sn < test; sn++) {
			BSortItem *sandwich_neighbour = &bdata.sort_items[p_start + sn];
			if (test_item->global_rect_cache.intersects(sandwich_neighbour->item->global_rect_cache)) {
				ok = false;
				break;
			}
		}
		if (!ok) {
			continue;
		}

		// it is ok to exchange them!
		BSortItem temp;
		temp.assign(second);
		second.assign(*test_sort_item);
		test_sort_item->assign(temp);

		return true;
	} // for test

	return false;
}

PREAMBLE(void)::_software_transform_vertex(BatchVector2 &r_v, const Transform2D &p_tr) const {
	Vector2 vc(r_v.x, r_v.y);
	vc = p_tr.xform(vc);
	r_v.set(vc);
}

PREAMBLE(void)::_software_transform_vertex(Vector2 &r_v, const Transform2D &p_tr) const {
	r_v = p_tr.xform(r_v);
}

PREAMBLE(void)::_translate_batches_to_vertex_colored_FVF() {
	// zeros the size and sets up how big each unit is
	bdata.unit_vertices.prepare(sizeof(BatchVertexColored));

	const BatchColor *source_vertex_colors = &bdata.vertex_colors[0];
	DEV_ASSERT(bdata.vertex_colors.size() == bdata.vertices.size());

	int num_verts = bdata.vertices.size();

	for (int n = 0; n < num_verts; n++) {
		const BatchVertex &bv = bdata.vertices[n];

		BatchVertexColored *cv = (BatchVertexColored *)bdata.unit_vertices.request();

		cv->pos = bv.pos;
		cv->uv = bv.uv;
		cv->col = *source_vertex_colors++;
	}
}

// Translation always involved adding color to the FVF, which enables
// joining of batches that have different colors.
// There is a trade off. Non colored verts are smaller so work faster, but
// there comes a point where it is better to just use colored verts to avoid lots of
// batches.
// In addition this can optionally add light angles to the FVF, necessary for normal mapping.
T_PREAMBLE
template <class BATCH_VERTEX_TYPE, bool INCLUDE_LIGHT_ANGLES, bool INCLUDE_MODULATE, bool INCLUDE_LARGE>
void C_PREAMBLE::_translate_batches_to_larger_FVF(uint32_t p_sequence_batch_type_flags) {
	bool include_poly_color = false;

	// we ONLY want to include the color verts in translation when using polys,
	// as rects do not write vertex colors, only colors per batch.
	if (p_sequence_batch_type_flags & RasterizerStorageCommon::BTF_POLY) {
		include_poly_color = INCLUDE_LIGHT_ANGLES | INCLUDE_MODULATE | INCLUDE_LARGE;
	}

	// zeros the size and sets up how big each unit is
	bdata.unit_vertices.prepare(sizeof(BATCH_VERTEX_TYPE));
	bdata.batches_temp.reset();

	// As the vertices_colored and batches_temp are 'mirrors' of the non-colored version,
	// the sizes should be equal, and allocations should never fail. Hence the use of debug
	// asserts to check program flow, these should not occur at runtime unless the allocation
	// code has been altered.
	DEV_ASSERT(bdata.unit_vertices.max_size() == bdata.vertices.max_size());
	DEV_ASSERT(bdata.batches_temp.max_size() == bdata.batches.max_size());

	Color curr_col(-1.0f, -1.0f, -1.0f, -1.0f);

	Batch *dest_batch = nullptr;

	const BatchColor *source_vertex_colors = &bdata.vertex_colors[0];
	const float *source_light_angles = &bdata.light_angles[0];
	const BatchColor *source_vertex_modulates = &bdata.vertex_modulates[0];
	const BatchTransform *source_vertex_transforms = &bdata.vertex_transforms[0];

	// translate the batches into vertex colored batches
	for (int n = 0; n < bdata.batches.size(); n++) {
		const Batch &source_batch = bdata.batches[n];

		// does source batch use light angles?
		const BatchTex &btex = bdata.batch_textures[source_batch.batch_texture_id];
		bool source_batch_uses_light_angles = btex.RID_normal != RID();

		bool needs_new_batch = true;

		if (dest_batch) {
			if (dest_batch->type == source_batch.type) {
				if (source_batch.type == RasterizerStorageCommon::BT_RECT) {
					if (dest_batch->batch_texture_id == source_batch.batch_texture_id) {
						// add to previous batch
						dest_batch->num_commands += source_batch.num_commands;
						needs_new_batch = false;

						// create the colored verts (only if not default)
						int first_vert = source_batch.first_vert;
						int num_verts = source_batch.get_num_verts();
						int end_vert = first_vert + num_verts;

						for (int v = first_vert; v < end_vert; v++) {
							RAST_DEV_DEBUG_ASSERT(bdata.vertices.size());
							const BatchVertex &bv = bdata.vertices[v];
							BATCH_VERTEX_TYPE *cv = (BATCH_VERTEX_TYPE *)bdata.unit_vertices.request();
							RAST_DEBUG_ASSERT(cv);
							cv->pos = bv.pos;
							cv->uv = bv.uv;
							cv->col = source_batch.color;

							if (INCLUDE_LIGHT_ANGLES) {
								RAST_DEV_DEBUG_ASSERT(bdata.light_angles.size());
								// this is required to allow compilation with non light angle vertex.
								// it should be compiled out.
								BatchVertexLightAngled *lv = (BatchVertexLightAngled *)cv;
								if (source_batch_uses_light_angles) {
									lv->light_angle = *source_light_angles++;
								} else {
									lv->light_angle = 0.0f; // dummy, unused in vertex shader (could possibly be left uninitialized, but probably bad idea)
								}
							} // if including light angles

							if (INCLUDE_MODULATE) {
								RAST_DEV_DEBUG_ASSERT(bdata.vertex_modulates.size());
								BatchVertexModulated *mv = (BatchVertexModulated *)cv;
								mv->modulate = *source_vertex_modulates++;
							} // including modulate

							if (INCLUDE_LARGE) {
								RAST_DEV_DEBUG_ASSERT(bdata.vertex_transforms.size());
								BatchVertexLarge *lv = (BatchVertexLarge *)cv;
								lv->transform = *source_vertex_transforms++;
							} // if including large
						}
					} // textures match
				} else {
					// default
					// we can still join, but only under special circumstances
					// does this ever happen? not sure at this stage, but left for future expansion
					uint32_t source_last_command = source_batch.first_command + source_batch.num_commands;
					if (source_last_command == dest_batch->first_command) {
						dest_batch->num_commands += source_batch.num_commands;
						needs_new_batch = false;
					} // if the commands line up exactly
				}
			} // if both batches are the same type

		} // if dest batch is valid

		if (needs_new_batch) {
			dest_batch = bdata.batches_temp.request();
			RAST_DEBUG_ASSERT(dest_batch);

			*dest_batch = source_batch;

			// create the colored verts (only if not default)
			if (source_batch.type != RasterizerStorageCommon::BT_DEFAULT) {
				int first_vert = source_batch.first_vert;
				int num_verts = source_batch.get_num_verts();
				int end_vert = first_vert + num_verts;

				for (int v = first_vert; v < end_vert; v++) {
					RAST_DEV_DEBUG_ASSERT(bdata.vertices.size());
					const BatchVertex &bv = bdata.vertices[v];
					BATCH_VERTEX_TYPE *cv = (BATCH_VERTEX_TYPE *)bdata.unit_vertices.request();
					RAST_DEBUG_ASSERT(cv);
					cv->pos = bv.pos;
					cv->uv = bv.uv;

					// polys are special, they can have per vertex colors
					if (!include_poly_color) {
						cv->col = source_batch.color;
					} else {
						RAST_DEV_DEBUG_ASSERT(bdata.vertex_colors.size());
						cv->col = *source_vertex_colors++;
					}

					if (INCLUDE_LIGHT_ANGLES) {
						RAST_DEV_DEBUG_ASSERT(bdata.light_angles.size());
						// this is required to allow compilation with non light angle vertex.
						// it should be compiled out.
						BatchVertexLightAngled *lv = (BatchVertexLightAngled *)cv;
						if (source_batch_uses_light_angles) {
							lv->light_angle = *source_light_angles++;
						} else {
							lv->light_angle = 0.0f; // dummy, unused in vertex shader (could possibly be left uninitialized, but probably bad idea)
						}
					} // if using light angles

					if (INCLUDE_MODULATE) {
						RAST_DEV_DEBUG_ASSERT(bdata.vertex_modulates.size());
						BatchVertexModulated *mv = (BatchVertexModulated *)cv;
						mv->modulate = *source_vertex_modulates++;
					} // including modulate

					if (INCLUDE_LARGE) {
						RAST_DEV_DEBUG_ASSERT(bdata.vertex_transforms.size());
						BatchVertexLarge *lv = (BatchVertexLarge *)cv;
						lv->transform = *source_vertex_transforms++;
					} // if including large
				}
			}
		}
	}

	// copy the temporary batches to the master batch list (this could be avoided but it makes the code cleaner)
	bdata.batches.copy_from(bdata.batches_temp);
}

PREAMBLE(bool)::_disallow_item_join_if_batch_types_too_different(RenderItemState &r_ris, uint32_t btf_allowed) {
	r_ris.joined_item_batch_type_flags_curr |= btf_allowed;

	bool disallow = false;

	if (r_ris.joined_item_batch_type_flags_prev & (~btf_allowed)) {
		disallow = true;
	}

	return disallow;
}

PREAMBLE(bool)::_detect_item_batch_break(RenderItemState &r_ris, RasterizerCanvas::Item *p_ci, bool &r_batch_break) {
	int command_count = p_ci->commands.size();

	// Any item that contains commands that are default
	// (i.e. not handled by software transform and the batching renderer) should not be joined.

	// ALSO batched types that differ in what the vertex format is needed to be should not be
	// joined.

	// In order to work this out, it does a lookahead through the commands,
	// which could potentially be very expensive. As such it makes sense to put a limit on this
	// to some small number, which will catch nearly all cases which need joining,
	// but not be overly expensive in the case of items with large numbers of commands.

	// It is hard to know what this number should be, empirically,
	// and this has not been fully investigated. It works to join single sprite items when set to 1 or above.
	// Note that there is a cost to increasing this because it has to look in advance through
	// the commands.
	// On the other hand joining items where possible will usually be better up to a certain
	// number where the cost of software transform is higher than separate drawcalls with hardware
	// transform.

	// if there are more than this number of commands in the item, we
	// don't allow joining (separate state changes, and hardware transform)
	// This is set to quite a conservative (low) number until investigated properly.
	// const int MAX_JOIN_ITEM_COMMANDS = 16;

	r_ris.joined_item_batch_type_flags_curr = 0;

	if (command_count > bdata.settings_max_join_item_commands) {
		return true;
	} else {
		RasterizerCanvas::Item::Command *const *commands = p_ci->commands.ptr();

		// run through the commands looking for one that could prevent joining
		for (int command_num = 0; command_num < command_count; command_num++) {
			RasterizerCanvas::Item::Command *command = commands[command_num];
			RAST_DEBUG_ASSERT(command);

			switch (command->type) {
				default: {
					//r_batch_break = true;
					return true;
				} break;
				case RasterizerCanvas::Item::Command::TYPE_LINE: {
					// special case, only batches certain lines
					RasterizerCanvas::Item::CommandLine *line = static_cast<RasterizerCanvas::Item::CommandLine *>(command);

					if (line->width > 1) {
						//r_batch_break = true;
						return true;
					}

					if (_disallow_item_join_if_batch_types_too_different(r_ris, RasterizerStorageCommon::BTF_LINE | RasterizerStorageCommon::BTF_LINE_AA)) {
						return true;
					}
				} break;
				case RasterizerCanvas::Item::Command::TYPE_POLYGON: {
					// only allow polygons to join if they aren't skeleton
					RasterizerCanvas::Item::CommandPolygon *poly = static_cast<RasterizerCanvas::Item::CommandPolygon *>(command);

#ifdef GLES_OVER_GL
					// anti aliasing not accelerated
					if (poly->antialiased) {
						return true;
					}
#endif

					// light angles not yet implemented, treat as default
					if (poly->normal_map != RID()) {
						return true;
					}

					if (!get_this()->bdata.settings_use_software_skinning && poly->bones.size()) {
						return true;
					}

					if (_disallow_item_join_if_batch_types_too_different(r_ris, RasterizerStorageCommon::BTF_POLY)) {
						//r_batch_break = true;
						return true;
					}
				} break;
				case RasterizerCanvas::Item::Command::TYPE_RECT: {
					if (_disallow_item_join_if_batch_types_too_different(r_ris, RasterizerStorageCommon::BTF_RECT)) {
						return true;
					}
				} break;
				case RasterizerCanvas::Item::Command::TYPE_MULTIRECT: {
					if (_disallow_item_join_if_batch_types_too_different(r_ris, RasterizerStorageCommon::BTF_RECT)) {
						return true;
					}
				} break;
				case RasterizerCanvas::Item::Command::TYPE_NINEPATCH: {
					// do not handle tiled ninepatches, these can't be batched and need to use legacy method
					RasterizerCanvas::Item::CommandNinePatch *np = static_cast<RasterizerCanvas::Item::CommandNinePatch *>(command);
					if ((np->axis_x != VisualServer::NINE_PATCH_STRETCH) || (np->axis_y != VisualServer::NINE_PATCH_STRETCH)) {
						return true;
					}

					if (_disallow_item_join_if_batch_types_too_different(r_ris, RasterizerStorageCommon::BTF_RECT)) {
						return true;
					}
				} break;
				case RasterizerCanvas::Item::Command::TYPE_TRANSFORM: {
					// compatible with all types
				} break;
			} // switch

		} // for through commands

	} // else

	// special case, back buffer copy, so don't join
	if (p_ci->copy_back_buffer) {
		return true;
	}

	return false;
}

#undef PREAMBLE
#undef T_PREAMBLE
#undef C_PREAMBLE

#endif // RASTERIZER_CANVAS_BATCHER_H
