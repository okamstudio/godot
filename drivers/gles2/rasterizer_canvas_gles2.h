/**************************************************************************/
/*  rasterizer_canvas_gles2.h                                             */
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

#ifndef RASTERIZER_CANVAS_GLES2_H
#define RASTERIZER_CANVAS_GLES2_H

#include "drivers/gles_common/rasterizer_canvas_batcher.h"
#include "rasterizer_canvas_base_gles2.h"

class RasterizerSceneGLES2;

class RasterizerCanvasGLES2 : public RasterizerCanvasBaseGLES2, public RasterizerCanvasBatcher<RasterizerCanvasGLES2, RasterizerStorageGLES2> {
	friend class RasterizerCanvasBatcher<RasterizerCanvasGLES2, RasterizerStorageGLES2>;

public:
	virtual void canvas_render_items_begin(const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform);
	virtual void canvas_render_items_end();
	virtual void canvas_render_items(Item *p_item_list, int p_z, const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform);
	virtual void canvas_begin();
	virtual void canvas_end();

	void canvas_adapt();

private:
	// for batching
	class BatchGLData {
	public:
		BatchGLData(RasterizerCanvasGLES2 &rasterizer) :
				rasterizer(rasterizer) {}

		inline size_t size() const {
			return current_size;
		}

		inline void request(size_t new_size) {
			resize(new_size);
		}

		inline void resize(size_t new_size) {
			if (new_size == 0) {
				new_size = 1;
			}
			int max_size_diff = new_size - vertex_buffers.size();
			if (max_size_diff > 0) {
				vertex_buffers.request_with_grow(max_size_diff);
				index_buffers.request_with_grow(max_size_diff);
			}
			int size_diff = new_size - current_size;
			if (size_diff > 0) {
				glGenBuffers(size_diff, &vertex_buffers[current_size]);
				glGenBuffers(size_diff, &index_buffers[current_size]);
				for (int i = 0; i < size_diff; i++) {
					rasterizer.initialize_buffer(vertex_buffers[current_size + i], index_buffers[current_size + i]);
				}
			} else if (size_diff < 0) {
				glDeleteBuffers(-size_diff, &vertex_buffers[current_size + size_diff - 1]);
				glDeleteBuffers(-size_diff, &index_buffers[current_size + size_diff - 1]);
			}
			current_size = new_size;
			current_idx = 0;
		}

		inline void reset() {
			resize(1);
		}

		inline unsigned int current_vertex_buffer() const {
			return vertex_buffers[current_idx];
		}

		inline unsigned int current_index_buffer() const {
			return index_buffers[current_idx];
		}

		inline void next() {
			new_size++;
			current_idx = (current_idx + 1) % current_size;
		}

		inline void adapt() {
			request(new_size);
			new_size = 0;
		}

	private:
		size_t current_idx = 0;
		size_t current_size = 0;
		size_t new_size = 0;
		RasterizerArray<unsigned int> vertex_buffers;
		RasterizerArray<unsigned int> index_buffers;
		RasterizerCanvasGLES2 &rasterizer;
	} batch_gl_data;

	// legacy codepath .. to remove after testing
	void _legacy_canvas_render_item(Item *p_ci, RenderItemState &r_ris);

	// high level batch funcs
	void canvas_render_items_implementation(Item *p_item_list, int p_z, const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform);
	void render_joined_item(const BItemJoined &p_bij, RenderItemState &r_ris);
	bool try_join_item(Item *p_ci, RenderItemState &r_ris, bool &r_batch_break);
	void render_batches(Item *p_current_clip, bool &r_reclip, RasterizerStorageGLES2::Material *p_material);

	// low level batch funcs
	void _batch_upload_buffers();
	void _batch_render_generic(const Batch &p_batch, RasterizerStorageGLES2::Material *p_material);
	void _batch_render_lines(const Batch &p_batch, RasterizerStorageGLES2::Material *p_material, bool p_anti_alias);

	// funcs used from rasterizer_canvas_batcher template
	void gl_enable_scissor(int p_x, int p_y, int p_width, int p_height) const;
	void gl_disable_scissor() const;

	void initialize_buffer(GLuint vertex_buffer, GLuint index_buffer);

public:
	void initialize();
	RasterizerCanvasGLES2();
};

#endif // RASTERIZER_CANVAS_GLES2_H
