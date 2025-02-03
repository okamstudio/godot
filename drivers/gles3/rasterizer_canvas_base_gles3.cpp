/**************************************************************************/
/*  rasterizer_canvas_base_gles3.cpp                                      */
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

#include "rasterizer_canvas_base_gles3.h"

#include "core/os/os.h"
#include "core/project_settings.h"
#include "drivers/gles_common/rasterizer_asserts.h"
#include "rasterizer_scene_gles3.h"
#include "servers/visual/visual_server_raster.h"

#ifndef GLES_OVER_GL
#define glClearDepth glClearDepthf
#endif

static _FORCE_INLINE_ void store_transform2d(const Transform2D &p_mtx, float *p_array) {
	p_array[0] = p_mtx.elements[0][0];
	p_array[1] = p_mtx.elements[0][1];
	p_array[2] = 0;
	p_array[3] = 0;
	p_array[4] = p_mtx.elements[1][0];
	p_array[5] = p_mtx.elements[1][1];
	p_array[6] = 0;
	p_array[7] = 0;
	p_array[8] = 0;
	p_array[9] = 0;
	p_array[10] = 1;
	p_array[11] = 0;
	p_array[12] = p_mtx.elements[2][0];
	p_array[13] = p_mtx.elements[2][1];
	p_array[14] = 0;
	p_array[15] = 1;
}

static _FORCE_INLINE_ void store_transform(const Transform &p_mtx, float *p_array) {
	p_array[0] = p_mtx.basis.elements[0][0];
	p_array[1] = p_mtx.basis.elements[1][0];
	p_array[2] = p_mtx.basis.elements[2][0];
	p_array[3] = 0;
	p_array[4] = p_mtx.basis.elements[0][1];
	p_array[5] = p_mtx.basis.elements[1][1];
	p_array[6] = p_mtx.basis.elements[2][1];
	p_array[7] = 0;
	p_array[8] = p_mtx.basis.elements[0][2];
	p_array[9] = p_mtx.basis.elements[1][2];
	p_array[10] = p_mtx.basis.elements[2][2];
	p_array[11] = 0;
	p_array[12] = p_mtx.origin.x;
	p_array[13] = p_mtx.origin.y;
	p_array[14] = p_mtx.origin.z;
	p_array[15] = 1;
}

static _FORCE_INLINE_ void store_camera(const CameraMatrix &p_mtx, float *p_array) {
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			p_array[i * 4 + j] = p_mtx.matrix[i][j];
		}
	}
}

RID RasterizerCanvasBaseGLES3::light_internal_create() {
	LightInternal *li = memnew(LightInternal);

	glGenBuffers(1, &li->ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, li->ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(LightInternal::UBOData), nullptr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	return light_internal_owner.make_rid(li);
}

void RasterizerCanvasBaseGLES3::light_internal_update(RID p_rid, Light *p_light) {
	LightInternal *li = light_internal_owner.getornull(p_rid);
	ERR_FAIL_COND(!li);

	store_transform2d(p_light->light_shader_xform, li->ubo_data.light_matrix);
	store_transform2d(p_light->xform_cache.affine_inverse(), li->ubo_data.local_matrix);
	store_camera(p_light->shadow_matrix_cache, li->ubo_data.shadow_matrix);

	for (int i = 0; i < 4; i++) {
		li->ubo_data.color[i] = p_light->color[i] * p_light->energy;
		li->ubo_data.shadow_color[i] = p_light->shadow_color[i];
	}

	li->ubo_data.light_pos[0] = p_light->light_shader_pos.x;
	li->ubo_data.light_pos[1] = p_light->light_shader_pos.y;
	li->ubo_data.shadowpixel_size = (1.0 / p_light->shadow_buffer_size) * (1.0 + p_light->shadow_smooth);
	li->ubo_data.light_outside_alpha = p_light->mode == VS::CANVAS_LIGHT_MODE_MASK ? 1.0 : 0.0;
	li->ubo_data.light_height = p_light->height;
	if (p_light->radius_cache == 0) {
		li->ubo_data.shadow_gradient = 0;
	} else {
		li->ubo_data.shadow_gradient = p_light->shadow_gradient_length / (p_light->radius_cache * 1.1);
	}

	li->ubo_data.shadow_distance_mult = (p_light->radius_cache * 1.1);

	glBindBuffer(GL_UNIFORM_BUFFER, li->ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(LightInternal::UBOData), &li->ubo_data, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void RasterizerCanvasBaseGLES3::light_internal_free(RID p_rid) {
	LightInternal *li = light_internal_owner.getornull(p_rid);
	ERR_FAIL_COND(!li);

	glDeleteBuffers(1, &li->ubo);
	light_internal_owner.free(p_rid);
	memdelete(li);
}

void RasterizerCanvasBaseGLES3::canvas_begin() {
	if (storage->frame.current_rt && storage->frame.clear_request) {
		// a clear request may be pending, so do it
		bool transparent = storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT];

		glClearColor(storage->frame.clear_request_color.r,
				storage->frame.clear_request_color.g,
				storage->frame.clear_request_color.b,
				transparent ? storage->frame.clear_request_color.a : 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
		storage->frame.clear_request = false;
		glColorMask(1, 1, 1, transparent ? 1 : 0);
	}

	reset_canvas();

	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_TEXTURE_RECT, true);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_LIGHTING, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_SHADOWS, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_NEAREST, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF3, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF5, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF7, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF9, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF13, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_DISTANCE_FIELD, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_NINEPATCH, false);

	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_DISTANCE_FIELD, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_ATTRIB_LIGHT_ANGLE, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_ATTRIB_MODULATE, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_ATTRIB_LARGE_VERTEX, false);

	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_SKELETON, false);

	state.canvas_shader.set_custom_shader(0);
	state.canvas_shader.bind();
	state.canvas_shader.set_uniform(CanvasShaderGLES3::FINAL_MODULATE, Color(1, 1, 1, 1));
	state.canvas_shader.set_uniform(CanvasShaderGLES3::MODELVIEW_MATRIX, Transform2D());
	state.canvas_shader.set_uniform(CanvasShaderGLES3::EXTRA_MATRIX, Transform2D());
	if (storage->frame.current_rt) {
		state.canvas_shader.set_uniform(CanvasShaderGLES3::SCREEN_PIXEL_SIZE, Vector2(1.0 / storage->frame.current_rt->width, 1.0 / storage->frame.current_rt->height));
	} else {
		state.canvas_shader.set_uniform(CanvasShaderGLES3::SCREEN_PIXEL_SIZE, Vector2(1.0, 1.0));
	}

	//state.canvas_shader.set_uniform(CanvasShaderGLES3::PROJECTION_MATRIX,state.vp);
	//state.canvas_shader.set_uniform(CanvasShaderGLES3::MODELVIEW_MATRIX,Transform());
	//state.canvas_shader.set_uniform(CanvasShaderGLES3::EXTRA_MATRIX,Transform());

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, state.canvas_item_ubo);
	glBindVertexArray(data.canvas_quad_array);
	state.using_texture_rect = true;
	state.using_ninepatch = false;

	state.using_light_angle = false;
	state.using_modulate = false;
	state.using_large_vertex = false;

	state.using_skeleton = false;
}

void RasterizerCanvasBaseGLES3::canvas_end() {
	glBindVertexArray(0);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, 0);
	glColorMask(1, 1, 1, 1);

	glVertexAttrib4f(VS::ARRAY_COLOR, 1, 1, 1, 1);

	state.using_texture_rect = false;
	state.using_ninepatch = false;
	state.using_light_angle = false;
}

RasterizerStorageGLES3::Texture *RasterizerCanvasBaseGLES3::_bind_canvas_texture(const RID &p_texture, const RID &p_normal_map, bool p_force) {
	RasterizerStorageGLES3::Texture *tex_return = nullptr;

	if (p_texture == state.current_tex && !p_force) {
		tex_return = state.current_tex_ptr;
	} else if (p_texture.is_valid()) {
		RasterizerStorageGLES3::Texture *texture = storage->texture_owner.getornull(p_texture);

		if (!texture) {
			state.current_tex = RID();
			state.current_tex_ptr = nullptr;
			WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, storage->resources.white_tex);

		} else {
			if (texture->redraw_if_visible) { //check before proxy, because this is usually used with proxies
				VisualServerRaster::redraw_request(false);
			}

			texture = texture->get_ptr();

			if (texture->render_target) {
				texture->render_target->used_in_frame = true;
			}

			WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, texture->tex_id);
			state.current_tex = p_texture;
			state.current_tex_ptr = texture;

			tex_return = texture;
		}

	} else {
		WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, storage->resources.white_tex);
		state.current_tex = RID();
		state.current_tex_ptr = nullptr;
	}

	if (p_normal_map == state.current_normal && !p_force) {
		//do none
		state.canvas_shader.set_uniform(CanvasShaderGLES3::USE_DEFAULT_NORMAL, state.current_normal.is_valid());

	} else if (p_normal_map.is_valid()) {
		RasterizerStorageGLES3::Texture *normal_map = storage->texture_owner.getornull(p_normal_map);

		if (!normal_map) {
			state.current_normal = RID();
			WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, storage->resources.normal_tex);
			state.canvas_shader.set_uniform(CanvasShaderGLES3::USE_DEFAULT_NORMAL, false);

		} else {
			if (normal_map->redraw_if_visible) { //check before proxy, because this is usually used with proxies
				VisualServerRaster::redraw_request(false);
			}

			normal_map = normal_map->get_ptr();

			WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, normal_map->tex_id);
			state.current_normal = p_normal_map;
			state.canvas_shader.set_uniform(CanvasShaderGLES3::USE_DEFAULT_NORMAL, true);
		}

	} else {
		state.current_normal = RID();
		WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, storage->resources.normal_tex);
		state.canvas_shader.set_uniform(CanvasShaderGLES3::USE_DEFAULT_NORMAL, false);
	}

	return tex_return;
}

void RasterizerCanvasBaseGLES3::_set_texture_rect_mode(bool p_enable, bool p_ninepatch, bool p_light_angle, bool p_modulate, bool p_large_vertex) {
	// this state check could be done individually
	if (state.using_texture_rect == p_enable && state.using_ninepatch == p_ninepatch && state.using_light_angle == p_light_angle && state.using_modulate == p_modulate && state.using_large_vertex == p_large_vertex) {
		return;
	}

	if (p_enable) {
		glBindVertexArray(data.canvas_quad_array);

	} else {
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_NINEPATCH, p_ninepatch && p_enable);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_TEXTURE_RECT, p_enable);

	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_ATTRIB_LIGHT_ANGLE, p_light_angle);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_ATTRIB_MODULATE, p_modulate);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_ATTRIB_LARGE_VERTEX, p_large_vertex);

	state.canvas_shader.bind();
	state.canvas_shader.set_uniform(CanvasShaderGLES3::FINAL_MODULATE, state.canvas_item_modulate);
	state.canvas_shader.set_uniform(CanvasShaderGLES3::MODELVIEW_MATRIX, state.final_transform);
	state.canvas_shader.set_uniform(CanvasShaderGLES3::EXTRA_MATRIX, state.extra_matrix);
	if (state.using_skeleton) {
		state.canvas_shader.set_uniform(CanvasShaderGLES3::SKELETON_TRANSFORM, state.skeleton_transform);
		state.canvas_shader.set_uniform(CanvasShaderGLES3::SKELETON_TRANSFORM_INVERSE, state.skeleton_transform_inverse);
	}
	if (storage->frame.current_rt) {
		state.canvas_shader.set_uniform(CanvasShaderGLES3::SCREEN_PIXEL_SIZE, Vector2(1.0 / storage->frame.current_rt->width, 1.0 / storage->frame.current_rt->height));
	} else {
		state.canvas_shader.set_uniform(CanvasShaderGLES3::SCREEN_PIXEL_SIZE, Vector2(1.0, 1.0));
	}

	state.using_texture_rect = p_enable;
	state.using_ninepatch = p_ninepatch;

	state.using_light_angle = p_light_angle;
	state.using_modulate = p_modulate;
	state.using_large_vertex = p_large_vertex;
}

void RasterizerCanvasBaseGLES3::_draw_polygon(const int *p_indices, int p_index_count, int p_vertex_count, const Vector2 *p_vertices, const Vector2 *p_uvs, const Color *p_colors, bool p_singlecolor, const int *p_bones, const float *p_weights) {
	glBindVertexArray(data.polygon_buffer_pointer_array);
	glBindBuffer(GL_ARRAY_BUFFER, data.polygon_buffer);

	uint32_t buffer_ofs = 0;
	uint32_t buffer_ofs_after = buffer_ofs + (sizeof(Vector2) * p_vertex_count);
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(buffer_ofs_after > data.polygon_buffer_size);
#endif

	storage->buffer_orphan_and_upload(data.polygon_buffer_size, buffer_ofs, sizeof(Vector2) * p_vertex_count, p_vertices, GL_ARRAY_BUFFER, _buffer_upload_usage_flag);

	glEnableVertexAttribArray(VS::ARRAY_VERTEX);
	glVertexAttribPointer(VS::ARRAY_VERTEX, 2, GL_FLOAT, false, sizeof(Vector2), CAST_INT_TO_UCHAR_PTR(buffer_ofs));
	buffer_ofs = buffer_ofs_after;

	//color
	if (p_singlecolor) {
		glDisableVertexAttribArray(VS::ARRAY_COLOR);
		Color m = *p_colors;
		glVertexAttrib4f(VS::ARRAY_COLOR, m.r, m.g, m.b, m.a);
	} else if (!p_colors) {
		glDisableVertexAttribArray(VS::ARRAY_COLOR);
		glVertexAttrib4f(VS::ARRAY_COLOR, 1, 1, 1, 1);
	} else {
		RAST_FAIL_COND(!storage->safe_buffer_sub_data(data.polygon_buffer_size, GL_ARRAY_BUFFER, buffer_ofs, sizeof(Color) * p_vertex_count, p_colors, buffer_ofs_after));
		glEnableVertexAttribArray(VS::ARRAY_COLOR);
		glVertexAttribPointer(VS::ARRAY_COLOR, 4, GL_FLOAT, false, sizeof(Color), CAST_INT_TO_UCHAR_PTR(buffer_ofs));
		buffer_ofs = buffer_ofs_after;
	}

	if (p_uvs) {
		RAST_FAIL_COND(!storage->safe_buffer_sub_data(data.polygon_buffer_size, GL_ARRAY_BUFFER, buffer_ofs, sizeof(Vector2) * p_vertex_count, p_uvs, buffer_ofs_after));
		glEnableVertexAttribArray(VS::ARRAY_TEX_UV);
		glVertexAttribPointer(VS::ARRAY_TEX_UV, 2, GL_FLOAT, false, sizeof(Vector2), CAST_INT_TO_UCHAR_PTR(buffer_ofs));
		buffer_ofs = buffer_ofs_after;

	} else {
		glDisableVertexAttribArray(VS::ARRAY_TEX_UV);
	}

	if (p_bones && p_weights) {
		RAST_FAIL_COND(!storage->safe_buffer_sub_data(data.polygon_buffer_size, GL_ARRAY_BUFFER, buffer_ofs, sizeof(int) * 4 * p_vertex_count, p_bones, buffer_ofs_after));
		glEnableVertexAttribArray(VS::ARRAY_BONES);
		//glVertexAttribPointer(VS::ARRAY_BONES, 4, GL_UNSIGNED_INT, false, sizeof(int) * 4, ((uint8_t *)0) + buffer_ofs);
		glVertexAttribIPointer(VS::ARRAY_BONES, 4, GL_UNSIGNED_INT, sizeof(int) * 4, CAST_INT_TO_UCHAR_PTR(buffer_ofs));
		buffer_ofs = buffer_ofs_after;

		RAST_FAIL_COND(!storage->safe_buffer_sub_data(data.polygon_buffer_size, GL_ARRAY_BUFFER, buffer_ofs, sizeof(float) * 4 * p_vertex_count, p_weights, buffer_ofs_after));
		glEnableVertexAttribArray(VS::ARRAY_WEIGHTS);
		glVertexAttribPointer(VS::ARRAY_WEIGHTS, 4, GL_FLOAT, false, sizeof(float) * 4, CAST_INT_TO_UCHAR_PTR(buffer_ofs));
		buffer_ofs = buffer_ofs_after;

	} else if (state.using_skeleton) {
		glVertexAttribI4ui(VS::ARRAY_BONES, 0, 0, 0, 0);
		glVertexAttrib4f(VS::ARRAY_WEIGHTS, 0, 0, 0, 0);
	}

#ifdef DEBUG_ENABLED
	ERR_FAIL_COND((sizeof(int) * p_index_count) > data.polygon_index_buffer_size);
#endif

	//bind the indices buffer.
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.polygon_index_buffer);
	storage->buffer_orphan_and_upload(data.polygon_index_buffer_size, 0, sizeof(int) * p_index_count, p_indices, GL_ELEMENT_ARRAY_BUFFER, _buffer_upload_usage_flag);

	//draw the triangles.
	glDrawElements(GL_TRIANGLES, p_index_count, GL_UNSIGNED_INT, nullptr);

	storage->info.render._2d_draw_call_count++;

	if (p_bones && p_weights) {
		//not used so often, so disable when used
		glDisableVertexAttribArray(VS::ARRAY_BONES);
		glDisableVertexAttribArray(VS::ARRAY_WEIGHTS);
	}

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RasterizerCanvasBaseGLES3::_draw_generic(GLuint p_primitive, int p_vertex_count, const Vector2 *p_vertices, const Vector2 *p_uvs, const Color *p_colors, bool p_singlecolor) {
	glBindVertexArray(data.polygon_buffer_pointer_array);
	glBindBuffer(GL_ARRAY_BUFFER, data.polygon_buffer);

	//vertex
	uint32_t buffer_ofs = 0;
	uint32_t buffer_ofs_after = buffer_ofs + (sizeof(Vector2) * p_vertex_count);
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(buffer_ofs_after > data.polygon_buffer_size);
#endif
	storage->buffer_orphan_and_upload(data.polygon_buffer_size, buffer_ofs, sizeof(Vector2) * p_vertex_count, p_vertices, GL_ARRAY_BUFFER, _buffer_upload_usage_flag);

	glEnableVertexAttribArray(VS::ARRAY_VERTEX);
	glVertexAttribPointer(VS::ARRAY_VERTEX, 2, GL_FLOAT, false, sizeof(Vector2), CAST_INT_TO_UCHAR_PTR(buffer_ofs));
	buffer_ofs = buffer_ofs_after;

	//color
	if (p_singlecolor) {
		glDisableVertexAttribArray(VS::ARRAY_COLOR);
		Color m = *p_colors;
		glVertexAttrib4f(VS::ARRAY_COLOR, m.r, m.g, m.b, m.a);
	} else if (!p_colors) {
		glDisableVertexAttribArray(VS::ARRAY_COLOR);
		glVertexAttrib4f(VS::ARRAY_COLOR, 1, 1, 1, 1);
	} else {
		RAST_FAIL_COND(!storage->safe_buffer_sub_data(data.polygon_buffer_size, GL_ARRAY_BUFFER, buffer_ofs, sizeof(Color) * p_vertex_count, p_colors, buffer_ofs_after));
		glEnableVertexAttribArray(VS::ARRAY_COLOR);
		glVertexAttribPointer(VS::ARRAY_COLOR, 4, GL_FLOAT, false, sizeof(Color), CAST_INT_TO_UCHAR_PTR(buffer_ofs));
		buffer_ofs = buffer_ofs_after;
	}

	if (p_uvs) {
		RAST_FAIL_COND(!storage->safe_buffer_sub_data(data.polygon_buffer_size, GL_ARRAY_BUFFER, buffer_ofs, sizeof(Vector2) * p_vertex_count, p_uvs, buffer_ofs_after));
		glEnableVertexAttribArray(VS::ARRAY_TEX_UV);
		glVertexAttribPointer(VS::ARRAY_TEX_UV, 2, GL_FLOAT, false, sizeof(Vector2), CAST_INT_TO_UCHAR_PTR(buffer_ofs));
		buffer_ofs = buffer_ofs_after;

	} else {
		glDisableVertexAttribArray(VS::ARRAY_TEX_UV);
	}

	glDrawArrays(p_primitive, 0, p_vertex_count);

	storage->info.render._2d_draw_call_count++;

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RasterizerCanvasBaseGLES3::_draw_generic_indices(GLuint p_primitive, const int *p_indices, int p_index_count, int p_vertex_count, const Vector2 *p_vertices, const Vector2 *p_uvs, const Color *p_colors, bool p_singlecolor) {
	glBindVertexArray(data.polygon_buffer_pointer_array);
	glBindBuffer(GL_ARRAY_BUFFER, data.polygon_buffer);

	//vertex
	uint32_t buffer_ofs = 0;
	uint32_t buffer_ofs_after = buffer_ofs + (sizeof(Vector2) * p_vertex_count);
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(buffer_ofs_after > data.polygon_buffer_size);
#endif
	storage->buffer_orphan_and_upload(data.polygon_buffer_size, buffer_ofs, sizeof(Vector2) * p_vertex_count, p_vertices, GL_ARRAY_BUFFER, _buffer_upload_usage_flag);

	glEnableVertexAttribArray(VS::ARRAY_VERTEX);
	glVertexAttribPointer(VS::ARRAY_VERTEX, 2, GL_FLOAT, false, sizeof(Vector2), CAST_INT_TO_UCHAR_PTR(buffer_ofs));
	buffer_ofs = buffer_ofs_after;

	//color
	if (p_singlecolor) {
		glDisableVertexAttribArray(VS::ARRAY_COLOR);
		Color m = *p_colors;
		glVertexAttrib4f(VS::ARRAY_COLOR, m.r, m.g, m.b, m.a);
	} else if (!p_colors) {
		glDisableVertexAttribArray(VS::ARRAY_COLOR);
		glVertexAttrib4f(VS::ARRAY_COLOR, 1, 1, 1, 1);
	} else {
		RAST_FAIL_COND(!storage->safe_buffer_sub_data(data.polygon_buffer_size, GL_ARRAY_BUFFER, buffer_ofs, sizeof(Color) * p_vertex_count, p_colors, buffer_ofs_after));
		glEnableVertexAttribArray(VS::ARRAY_COLOR);
		glVertexAttribPointer(VS::ARRAY_COLOR, 4, GL_FLOAT, false, sizeof(Color), CAST_INT_TO_UCHAR_PTR(buffer_ofs));
		buffer_ofs = buffer_ofs_after;
	}

	if (p_uvs) {
		RAST_FAIL_COND(!storage->safe_buffer_sub_data(data.polygon_buffer_size, GL_ARRAY_BUFFER, buffer_ofs, sizeof(Vector2) * p_vertex_count, p_uvs, buffer_ofs_after));
		glEnableVertexAttribArray(VS::ARRAY_TEX_UV);
		glVertexAttribPointer(VS::ARRAY_TEX_UV, 2, GL_FLOAT, false, sizeof(Vector2), CAST_INT_TO_UCHAR_PTR(buffer_ofs));
		buffer_ofs = buffer_ofs_after;

	} else {
		glDisableVertexAttribArray(VS::ARRAY_TEX_UV);
	}

#ifdef RASTERIZER_EXTRA_CHECKS
	// very slow, do not enable in normal use
	for (int n = 0; n < p_index_count; n++) {
		RAST_DEV_DEBUG_ASSERT(p_indices[n] < p_vertex_count);
	}
#endif

#ifdef DEBUG_ENABLED
	ERR_FAIL_COND((sizeof(int) * p_index_count) > data.polygon_index_buffer_size);
#endif

	//bind the indices buffer.
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.polygon_index_buffer);
	storage->buffer_orphan_and_upload(data.polygon_index_buffer_size, 0, sizeof(int) * p_index_count, p_indices, GL_ELEMENT_ARRAY_BUFFER, _buffer_upload_usage_flag);

	//draw the triangles.
	glDrawElements(p_primitive, p_index_count, GL_UNSIGNED_INT, nullptr);

	storage->info.render._2d_draw_call_count++;

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RasterizerCanvasBaseGLES3::_draw_gui_primitive(int p_points, const Vector2 *p_vertices, const Color *p_colors, const Vector2 *p_uvs, const float *p_light_angles) {
	static const GLenum prim[5] = { GL_POINTS, GL_POINTS, GL_LINES, GL_TRIANGLES, GL_TRIANGLE_FAN };

	//#define GLES_USE_PRIMITIVE_BUFFER

	int version = 0;
	int color_ofs = 0;
	int uv_ofs = 0;
	int light_angle_ofs = 0;
	int stride = 2;

	if (p_colors) { //color
		version |= 1;
		color_ofs = stride;
		stride += 4;
	}

	if (p_uvs) { //uv
		version |= 2;
		uv_ofs = stride;
		stride += 2;
	}

	if (p_light_angles) { //light_angles
		version |= 4;
		light_angle_ofs = stride;
		stride += 1;
	}

	DEV_ASSERT(p_points <= 4);
	float b[(2 + 2 + 4 + 1) * 4];

	for (int i = 0; i < p_points; i++) {
		b[stride * i + 0] = p_vertices[i].x;
		b[stride * i + 1] = p_vertices[i].y;
	}

	if (p_colors) {
		for (int i = 0; i < p_points; i++) {
			b[stride * i + color_ofs + 0] = p_colors[i].r;
			b[stride * i + color_ofs + 1] = p_colors[i].g;
			b[stride * i + color_ofs + 2] = p_colors[i].b;
			b[stride * i + color_ofs + 3] = p_colors[i].a;
		}
	}

	if (p_uvs) {
		for (int i = 0; i < p_points; i++) {
			b[stride * i + uv_ofs + 0] = p_uvs[i].x;
			b[stride * i + uv_ofs + 1] = p_uvs[i].y;
		}
	}

	if (p_light_angles) {
		for (int i = 0; i < p_points; i++) {
			b[stride * i + light_angle_ofs] = p_light_angles[i];
		}
	}

	glBindBuffer(GL_ARRAY_BUFFER, data.polygon_buffer);
	storage->buffer_orphan_and_upload(data.polygon_buffer_size, 0, p_points * stride * sizeof(float), &b[0], GL_ARRAY_BUFFER, _buffer_upload_usage_flag);

	glBindVertexArray(data.polygon_buffer_quad_arrays[version]);
	glDrawArrays(prim[p_points], 0, p_points);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	storage->info.render._2d_draw_call_count++;
}

void RasterizerCanvasBaseGLES3::render_rect_nvidia_workaround(const Item::CommandRect *p_rect, const RasterizerStorageGLES3::Texture *p_texture) {
	if (p_texture) {
		bool send_light_angles = false;

		// only need to use light angles when normal mapping
		// otherwise we can use the default shader
		if (state.current_normal != RID()) {
			send_light_angles = true;
		}

		// we don't want to use texture rect, and we want to send light angles if we are using normal mapping
		_set_texture_rect_mode(false, false, send_light_angles);

		bool untile = false;

		if (p_rect->flags & CANVAS_RECT_TILE && !(p_texture->flags & VS::TEXTURE_FLAG_REPEAT)) {
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			untile = true;
		}

		Size2 texpixel_size(1.0 / p_texture->width, 1.0 / p_texture->height);

		state.canvas_shader.set_uniform(CanvasShaderGLES3::CLIP_RECT_UV, p_rect->flags & CANVAS_RECT_CLIP_UV);

		Vector2 points[4] = {
			p_rect->rect.position,
			p_rect->rect.position + Vector2(p_rect->rect.size.x, 0.0),
			p_rect->rect.position + p_rect->rect.size,
			p_rect->rect.position + Vector2(0.0, p_rect->rect.size.y),
		};

		if (p_rect->rect.size.x < 0) {
			SWAP(points[0], points[1]);
			SWAP(points[2], points[3]);
		}
		if (p_rect->rect.size.y < 0) {
			SWAP(points[0], points[3]);
			SWAP(points[1], points[2]);
		}
		Rect2 src_rect = (p_rect->flags & CANVAS_RECT_REGION) ? Rect2(p_rect->source.position * texpixel_size, p_rect->source.size * texpixel_size) : Rect2(0, 0, 1, 1);

		Vector2 uvs[4] = {
			src_rect.position,
			src_rect.position + Vector2(src_rect.size.x, 0.0),
			src_rect.position + src_rect.size,
			src_rect.position + Vector2(0.0, src_rect.size.y),
		};

		// for encoding in light angle
		bool flip_h = false;
		bool flip_v = false;

		if (p_rect->flags & CANVAS_RECT_TRANSPOSE) {
			SWAP(uvs[1], uvs[3]);
		}

		if (p_rect->flags & CANVAS_RECT_FLIP_H) {
			SWAP(uvs[0], uvs[1]);
			SWAP(uvs[2], uvs[3]);
			flip_h = true;
			flip_v = !flip_v;
		}
		if (p_rect->flags & CANVAS_RECT_FLIP_V) {
			SWAP(uvs[0], uvs[3]);
			SWAP(uvs[1], uvs[2]);
			flip_v = !flip_v;
		}

		if (send_light_angles) {
			// for single rects, there is no need to fully utilize the light angle,
			// we only need it to encode flips (horz and vert). But the shader can be reused with
			// batching in which case the angle encodes the transform as well as
			// the flips.
			// Note transpose is NYI. I don't think it worked either with the non-nvidia method.

			// if horizontal flip, angle is 180
			float angle = 0.0f;
			if (flip_h) {
				angle = Math_PI;
			}

			// add 1 (to take care of zero floating point error with sign)
			angle += 1.0f;

			// flip if necessary
			if (flip_v) {
				angle *= -1.0f;
			}

			// light angle must be sent for each vert, instead as a single uniform in the uniform draw method
			// this has the benefit of enabling batching with light angles.
			float light_angles[4] = { angle, angle, angle, angle };

			_draw_gui_primitive(4, points, nullptr, uvs, light_angles);
		} else {
			_draw_gui_primitive(4, points, nullptr, uvs);
		}

		if (untile) {
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}

	} else {
		_set_texture_rect_mode(false);

		state.canvas_shader.set_uniform(CanvasShaderGLES3::CLIP_RECT_UV, false);

		Vector2 points[4] = {
			p_rect->rect.position,
			p_rect->rect.position + Vector2(p_rect->rect.size.x, 0.0),
			p_rect->rect.position + p_rect->rect.size,
			p_rect->rect.position + Vector2(0.0, p_rect->rect.size.y),
		};

		_draw_gui_primitive(4, points, nullptr, nullptr);
	}
}

void RasterizerCanvasBaseGLES3::material_blit(RasterizerStorageGLES3::Material *p_material, RasterizerStorageGLES3::Texture *p_source_tex, RasterizerStorageGLES3::Texture *p_output_tex, const Rect2i &p_source_rect, const Rect2i &p_output_rect) {
	RasterizerStorageGLES3::Shader *shader = p_material->shader;

	int src_width, src_height;
	GLint src_filter;
	if (p_source_tex) {
		src_width = p_source_tex->alloc_width;
		src_height = p_source_tex->alloc_height;
		src_filter = (p_source_tex->flags & VS::TEXTURE_FLAG_FILTER) ? GL_LINEAR : GL_NEAREST;
	} else {
		src_width = p_output_tex->alloc_width;
		src_height = p_output_tex->alloc_height;
		src_filter = GL_NEAREST;
	}

	// Create output framebuffer
	GLuint fbo_output;
	glGenFramebuffers(1, &fbo_output);

	// Configure output texture
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_output);
	glBindTexture(GL_TEXTURE_2D, p_output_tex->tex_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, src_filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, src_filter);
	glBindTexture(GL_TEXTURE_2D, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, p_output_tex->tex_id, 0);

	glDepthMask(GL_FALSE);
	glEnable(GL_SCISSOR_TEST);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glDepthFunc(GL_LEQUAL);
	glColorMask(1, 1, 1, 1);
	glActiveTexture(GL_TEXTURE0);
	if (p_source_tex) {
		glBindTexture(GL_TEXTURE_2D, p_source_tex->tex_id);
	} else {
		glBindTexture(GL_TEXTURE_2D, storage->resources.white_tex);
	}

	glViewport(p_output_rect.position.x, p_output_rect.position.y, p_output_rect.size.x, p_output_rect.size.y);
	glScissor(p_output_rect.position.x, p_output_rect.position.y, p_output_rect.size.x, p_output_rect.size.y);

	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_TEXTURE_RECT, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_LIGHTING, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_SHADOWS, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_NEAREST, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF3, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF5, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF7, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF9, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF13, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_DISTANCE_FIELD, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_NINEPATCH, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_ATTRIB_LIGHT_ANGLE, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_ATTRIB_MODULATE, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_ATTRIB_LARGE_VERTEX, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_SKELETON, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES3::LINEAR_TO_SRGB, false);

	Vector2 proj_off(p_source_rect.position);
	Vector2 proj_scl(p_source_rect.size);
	proj_off.x = proj_off.x / (float)src_width;
	proj_off.y = proj_off.y / (float)src_height;
	proj_scl.x = proj_scl.x / (float)src_width;
	proj_scl.y = proj_scl.y / (float)src_height;
	Vector2 proj_inv_scl(1.0 / proj_scl.x, 1.0 / proj_scl.y);

	Transform proj_tr(
			proj_inv_scl.x, 0.0, 0.0,
			0.0, proj_inv_scl.y, 0.0,
			0.0, 0.0, 1.0,
			-proj_off.x * proj_inv_scl.x * 2.0 + proj_inv_scl.x - 1.0,
			-proj_off.y * proj_inv_scl.y * 2.0 + proj_inv_scl.y - 1.0,
			0.0);
	Transform idt_tr;

	store_transform(proj_tr, state.canvas_item_ubo_data.projection_matrix);

	glBindBuffer(GL_UNIFORM_BUFFER, state.canvas_item_ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(CanvasItemUBO), &state.canvas_item_ubo_data, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, state.canvas_item_ubo);

	if (p_material->ubo_id) {
		glBindBufferBase(GL_UNIFORM_BUFFER, 2, p_material->ubo_id);
	}

	state.canvas_shader.set_custom_shader(shader->custom_code_id);
	state.canvas_shader.bind();
	ERR_FAIL_COND(!ShaderGLES3::get_active());

	int tc = p_material->textures.size();
	RID *textures = p_material->textures.ptrw();
	ShaderLanguage::ShaderNode::Uniform::Hint *texture_hints = shader->texture_hints.ptrw();

	for (int i = 0; i < tc; i++) {
		glActiveTexture(GL_TEXTURE2 + i);

		RasterizerStorageGLES3::Texture *t = storage->texture_owner.getornull(textures[i]);
		if (!t) {
			switch (texture_hints[i]) {
				case ShaderLanguage::ShaderNode::Uniform::HINT_BLACK_ALBEDO:
				case ShaderLanguage::ShaderNode::Uniform::HINT_BLACK: {
					glBindTexture(GL_TEXTURE_2D, storage->resources.black_tex);
				} break;
				case ShaderLanguage::ShaderNode::Uniform::HINT_TRANSPARENT: {
					glBindTexture(GL_TEXTURE_2D, storage->resources.transparent_tex);
				} break;
				case ShaderLanguage::ShaderNode::Uniform::HINT_ANISO: {
					glBindTexture(GL_TEXTURE_2D, storage->resources.aniso_tex);
				} break;
				case ShaderLanguage::ShaderNode::Uniform::HINT_NORMAL: {
					glBindTexture(GL_TEXTURE_2D, storage->resources.normal_tex);
				} break;
				default: {
					glBindTexture(GL_TEXTURE_2D, storage->resources.white_tex);
				} break;
			}

			//check hints

			continue;
		}

		if (t->redraw_if_visible) { //check before proxy, because this is usually used with proxies
			VisualServerRaster::redraw_request(false);
		}

		t = t->get_ptr();

		if (storage->config.srgb_decode_supported && t->using_srgb) {
			//no srgb in 2D
			glTexParameteri(t->target, _TEXTURE_SRGB_DECODE_EXT, _SKIP_DECODE_EXT);
			t->using_srgb = false;
		}

		glBindTexture(t->target, t->tex_id);
	}

	state.canvas_shader.set_uniform(CanvasShaderGLES3::FINAL_MODULATE, Color(1, 1, 1, 1));
	state.canvas_shader.set_uniform(CanvasShaderGLES3::MODELVIEW_MATRIX, idt_tr);
	state.canvas_shader.set_uniform(CanvasShaderGLES3::EXTRA_MATRIX, idt_tr);

	// Clear the output texture
	glClearColor(1.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	// Draw the scene
	glBindVertexArray(storage->resources.quadie_array);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glBindVertexArray(0);

	// Unbind and delete
	glDisable(GL_SCISSOR_TEST);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &fbo_output);
}

void RasterizerCanvasBaseGLES3::_copy_texscreen(const Rect2 &p_rect) {
	ERR_FAIL_COND_MSG(storage->frame.current_rt->effects.mip_maps[0].sizes.size() == 0, "Can't use screen texture copying in a render target configured without copy buffers. To resolve this, change the viewport's Usage property to \"2D\" or \"3D\" instead of \"2D Without Sampling\" or \"3D Without Effects\" respectively.");

	glDisable(GL_BLEND);

	state.canvas_texscreen_used = true;
	//blur diffuse into effect mipmaps using separatable convolution
	//storage->shaders.copy.set_conditional(CopyShaderGLES3::GAUSSIAN_HORIZONTAL,true);

	Vector2 wh(storage->frame.current_rt->width, storage->frame.current_rt->height);

	Color blur_section(p_rect.position.x / wh.x, p_rect.position.y / wh.y, p_rect.size.x / wh.x, p_rect.size.y / wh.y);

	if (p_rect != Rect2()) {
		scene_render->state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::USE_BLUR_SECTION, true);
		storage->shaders.copy.set_conditional(CopyShaderGLES3::USE_COPY_SECTION, true);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, storage->frame.current_rt->effects.mip_maps[0].sizes[0].fbo);
	WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, storage->frame.current_rt->color);

	storage->shaders.copy.bind();
	storage->shaders.copy.set_uniform(CopyShaderGLES3::COPY_SECTION, blur_section);

	scene_render->_copy_screen();

	for (int i = 0; i < storage->frame.current_rt->effects.mip_maps[1].sizes.size(); i++) {
		int vp_w = storage->frame.current_rt->effects.mip_maps[1].sizes[i].width;
		int vp_h = storage->frame.current_rt->effects.mip_maps[1].sizes[i].height;
		glViewport(0, 0, vp_w, vp_h);
		//horizontal pass
		scene_render->state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::GAUSSIAN_HORIZONTAL, true);
		scene_render->state.effect_blur_shader.bind();
		scene_render->state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::PIXEL_SIZE, Vector2(1.0 / vp_w, 1.0 / vp_h));
		scene_render->state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::LOD, float(i));
		scene_render->state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::BLUR_SECTION, blur_section);
		WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, storage->frame.current_rt->effects.mip_maps[0].color); //previous level, since mipmaps[0] starts one level bigger
		glBindFramebuffer(GL_FRAMEBUFFER, storage->frame.current_rt->effects.mip_maps[1].sizes[i].fbo);

		scene_render->_copy_screen();

		scene_render->state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::GAUSSIAN_HORIZONTAL, false);

		//vertical pass
		scene_render->state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::GAUSSIAN_VERTICAL, true);
		scene_render->state.effect_blur_shader.bind();
		scene_render->state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::PIXEL_SIZE, Vector2(1.0 / vp_w, 1.0 / vp_h));
		scene_render->state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::LOD, float(i));
		scene_render->state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::BLUR_SECTION, blur_section);
		WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, storage->frame.current_rt->effects.mip_maps[1].color);
		glBindFramebuffer(GL_FRAMEBUFFER, storage->frame.current_rt->effects.mip_maps[0].sizes[i + 1].fbo); //next level, since mipmaps[0] starts one level bigger

		scene_render->_copy_screen();

		scene_render->state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::GAUSSIAN_VERTICAL, false);
	}

	scene_render->state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::USE_BLUR_SECTION, false);
	storage->shaders.copy.set_conditional(CopyShaderGLES3::USE_COPY_SECTION, false);

	glBindFramebuffer(GL_FRAMEBUFFER, storage->frame.current_rt->fbo); //back to front
	glViewport(0, 0, storage->frame.current_rt->width, storage->frame.current_rt->height);

	// back to canvas, force rebind
	state.using_texture_rect = true;
	_set_texture_rect_mode(false);

	_bind_canvas_texture(state.current_tex, state.current_normal, true);

	glEnable(GL_BLEND);
}

void RasterizerCanvasBaseGLES3::canvas_debug_viewport_shadows(Light *p_lights_with_shadow) {
	Light *light = p_lights_with_shadow;

	canvas_begin(); //reset
	glVertexAttrib4f(VS::ARRAY_COLOR, 1, 1, 1, 1);
	int h = 10;
	int w = storage->frame.current_rt->width;
	int ofs = h;
	glDisable(GL_BLEND);

	while (light) {
		if (light->shadow_buffer.is_valid()) {
			RasterizerStorageGLES3::CanvasLightShadow *sb = storage->canvas_light_shadow_owner.get(light->shadow_buffer);
			if (sb) {
				glBindTexture(GL_TEXTURE_2D, sb->distance);
				draw_generic_textured_rect(Rect2(h, ofs, w - h * 2, h), Rect2(0, 0, 1, 1));
				ofs += h * 2;
			}
		}

		light = light->shadows_next_ptr;
	}

	canvas_end();
}

void RasterizerCanvasBaseGLES3::canvas_light_shadow_buffer_update(RID p_buffer, const Transform2D &p_light_xform, int p_light_mask, float p_near, float p_far, LightOccluderInstance *p_occluders, CameraMatrix *p_xform_cache) {
	RasterizerStorageGLES3::CanvasLightShadow *cls = storage->canvas_light_shadow_owner.get(p_buffer);
	ERR_FAIL_COND(!cls);

	glDisable(GL_BLEND);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_DITHER);
	glDisable(GL_CULL_FACE);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(true);

	glBindFramebuffer(GL_FRAMEBUFFER, cls->fbo);

	state.canvas_shadow_shader.bind();

	glViewport(0, 0, cls->size, cls->height);
	glClearDepth(1.0f);
	glClearColor(1, 1, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	VS::CanvasOccluderPolygonCullMode cull = VS::CANVAS_OCCLUDER_POLYGON_CULL_DISABLED;

	for (int i = 0; i < 4; i++) {
		//make sure it remains orthogonal, makes easy to read angle later

		Transform light;
		light.origin[0] = p_light_xform[2][0];
		light.origin[1] = p_light_xform[2][1];
		light.basis[0][0] = p_light_xform[0][0];
		light.basis[0][1] = p_light_xform[1][0];
		light.basis[1][0] = p_light_xform[0][1];
		light.basis[1][1] = p_light_xform[1][1];

		//light.basis.scale(Vector3(to_light.elements[0].length(),to_light.elements[1].length(),1));

		//p_near=1;
		CameraMatrix projection;
		{
			real_t fov = 90;
			real_t nearp = p_near;
			real_t farp = p_far;
			real_t aspect = 1.0;

			real_t ymax = nearp * Math::tan(Math::deg2rad(fov * 0.5));
			real_t ymin = -ymax;
			real_t xmin = ymin * aspect;
			real_t xmax = ymax * aspect;

			projection.set_frustum(xmin, xmax, ymin, ymax, nearp, farp);
		}

		Vector3 cam_target = Basis(Vector3(0, 0, Math_PI * 2 * (i / 4.0))).xform(Vector3(0, 1, 0));
		projection = projection * CameraMatrix(Transform().looking_at(cam_target, Vector3(0, 0, -1)).affine_inverse());

		state.canvas_shadow_shader.set_uniform(CanvasShadowShaderGLES3::PROJECTION_MATRIX, projection);
		state.canvas_shadow_shader.set_uniform(CanvasShadowShaderGLES3::LIGHT_MATRIX, light);
		state.canvas_shadow_shader.set_uniform(CanvasShadowShaderGLES3::DISTANCE_NORM, 1.0 / p_far);

		if (i == 0) {
			*p_xform_cache = projection;
		}

		glViewport(0, (cls->height / 4) * i, cls->size, cls->height / 4);

		LightOccluderInstance *instance = p_occluders;

		while (instance) {
			RasterizerStorageGLES3::CanvasOccluder *cc = storage->canvas_occluder_owner.getornull(instance->polygon_buffer);
			if (!cc || cc->len == 0 || !(p_light_mask & instance->light_mask)) {
				instance = instance->next;
				continue;
			}

			state.canvas_shadow_shader.set_uniform(CanvasShadowShaderGLES3::WORLD_MATRIX, instance->xform_cache);

			VS::CanvasOccluderPolygonCullMode transformed_cull_cache = instance->cull_cache;

			if (transformed_cull_cache != VS::CANVAS_OCCLUDER_POLYGON_CULL_DISABLED &&
					(p_light_xform.determinant() * instance->xform_cache.determinant()) < 0) {
				transformed_cull_cache = (transformed_cull_cache == VS::CANVAS_OCCLUDER_POLYGON_CULL_CLOCKWISE)
						? VS::CANVAS_OCCLUDER_POLYGON_CULL_COUNTER_CLOCKWISE
						: VS::CANVAS_OCCLUDER_POLYGON_CULL_CLOCKWISE;
			}

			if (cull != transformed_cull_cache) {
				cull = transformed_cull_cache;
				switch (cull) {
					case VS::CANVAS_OCCLUDER_POLYGON_CULL_DISABLED: {
						glDisable(GL_CULL_FACE);

					} break;
					case VS::CANVAS_OCCLUDER_POLYGON_CULL_CLOCKWISE: {
						glEnable(GL_CULL_FACE);
						glCullFace(GL_FRONT);
					} break;
					case VS::CANVAS_OCCLUDER_POLYGON_CULL_COUNTER_CLOCKWISE: {
						glEnable(GL_CULL_FACE);
						glCullFace(GL_BACK);

					} break;
				}
			}

			glBindVertexArray(cc->array_id);
			glDrawElements(GL_TRIANGLES, cc->len * 3, GL_UNSIGNED_SHORT, nullptr);

			instance = instance->next;
		}
	}

	glBindVertexArray(0);
}
void RasterizerCanvasBaseGLES3::reset_canvas() {
	if (storage->frame.current_rt) {
		glBindFramebuffer(GL_FRAMEBUFFER, storage->frame.current_rt->fbo);
		glColorMask(1, 1, 1, 1); //don't touch alpha
	}

	glBindVertexArray(0);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_DITHER);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	} else {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	//glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	//glLineWidth(1.0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	//use for reading from screen
	if (storage->frame.current_rt && !storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_NO_SAMPLING]) {
		WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE0 + storage->config.max_texture_image_units - 3);
		glBindTexture(GL_TEXTURE_2D, storage->frame.current_rt->effects.mip_maps[0].color);
	}

	WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, storage->resources.white_tex);

	glVertexAttrib4f(VS::ARRAY_COLOR, 1, 1, 1, 1);

	Transform canvas_transform;

	if (storage->frame.current_rt) {
		float csy = 1.0;
		if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_VFLIP]) {
			csy = -1.0;
		}
		canvas_transform.translate(-(storage->frame.current_rt->width / 2.0f), -(storage->frame.current_rt->height / 2.0f), 0.0f);
		canvas_transform.scale(Vector3(2.0f / storage->frame.current_rt->width, csy * -2.0f / storage->frame.current_rt->height, 1.0f));
	} else {
		Vector2 ssize = OS::get_singleton()->get_window_size();
		canvas_transform.translate(-(ssize.width / 2.0f), -(ssize.height / 2.0f), 0.0f);
		canvas_transform.scale(Vector3(2.0f / ssize.width, -2.0f / ssize.height, 1.0f));
	}

	state.vp = canvas_transform;

	store_transform(canvas_transform, state.canvas_item_ubo_data.projection_matrix);
	state.canvas_item_ubo_data.time = storage->frame.time[0];

	glBindBuffer(GL_UNIFORM_BUFFER, state.canvas_item_ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(CanvasItemUBO), &state.canvas_item_ubo_data, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	state.canvas_texscreen_used = false;
}

void RasterizerCanvasBaseGLES3::draw_generic_textured_rect(const Rect2 &p_rect, const Rect2 &p_src) {
	state.canvas_shader.set_uniform(CanvasShaderGLES3::DST_RECT, Color(p_rect.position.x, p_rect.position.y, p_rect.size.x, p_rect.size.y));
	state.canvas_shader.set_uniform(CanvasShaderGLES3::SRC_RECT, Color(p_src.position.x, p_src.position.y, p_src.size.x, p_src.size.y));
	state.canvas_shader.set_uniform(CanvasShaderGLES3::CLIP_RECT_UV, false);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void RasterizerCanvasBaseGLES3::draw_lens_distortion_rect(const Rect2 &p_rect, float p_k1, float p_k2, const Vector2 &p_eye_center, float p_oversample) {
	Vector2 half_size;
	if (storage->frame.current_rt) {
		half_size = Vector2(storage->frame.current_rt->width, storage->frame.current_rt->height);
	} else {
		half_size = OS::get_singleton()->get_window_size();
	}
	half_size *= 0.5;
	Vector2 offset((p_rect.position.x - half_size.x) / half_size.x, (p_rect.position.y - half_size.y) / half_size.y);
	Vector2 scale(p_rect.size.x / half_size.x, p_rect.size.y / half_size.y);

	float aspect_ratio = p_rect.size.x / p_rect.size.y;

	// setup our lens shader
	state.lens_shader.bind();
	state.lens_shader.set_uniform(LensDistortedShaderGLES3::OFFSET, offset);
	state.lens_shader.set_uniform(LensDistortedShaderGLES3::SCALE, scale);
	state.lens_shader.set_uniform(LensDistortedShaderGLES3::K1, p_k1);
	state.lens_shader.set_uniform(LensDistortedShaderGLES3::K2, p_k2);
	state.lens_shader.set_uniform(LensDistortedShaderGLES3::EYE_CENTER, p_eye_center);
	state.lens_shader.set_uniform(LensDistortedShaderGLES3::UPSCALE, p_oversample);
	state.lens_shader.set_uniform(LensDistortedShaderGLES3::ASPECT_RATIO, aspect_ratio);

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, state.canvas_item_ubo);
	glBindVertexArray(data.canvas_quad_array);

	// and draw
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glBindVertexArray(0);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, 0);
}

void RasterizerCanvasBaseGLES3::draw_window_margins(int *black_margin, RID *black_image) {
	Vector2 window_size = OS::get_singleton()->get_window_size();
	int window_h = window_size.height;
	int window_w = window_size.width;

	glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES3::system_fbo);
	glViewport(0, 0, window_size.width, window_size.height);
	canvas_begin();

	if (black_image[MARGIN_LEFT].is_valid()) {
		_bind_canvas_texture(black_image[MARGIN_LEFT], RID(), true);
		Size2 sz(storage->texture_get_width(black_image[MARGIN_LEFT]), storage->texture_get_height(black_image[MARGIN_LEFT]));

		draw_generic_textured_rect(Rect2(0, 0, black_margin[MARGIN_LEFT], window_h),
				Rect2(0, 0, (float)black_margin[MARGIN_LEFT] / sz.x, (float)(window_h) / sz.y));
	} else if (black_margin[MARGIN_LEFT]) {
		WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, storage->resources.black_tex);

		draw_generic_textured_rect(Rect2(0, 0, black_margin[MARGIN_LEFT], window_h), Rect2(0, 0, 1, 1));
	}

	if (black_image[MARGIN_RIGHT].is_valid()) {
		_bind_canvas_texture(black_image[MARGIN_RIGHT], RID(), true);
		Size2 sz(storage->texture_get_width(black_image[MARGIN_RIGHT]), storage->texture_get_height(black_image[MARGIN_RIGHT]));
		draw_generic_textured_rect(Rect2(window_w - black_margin[MARGIN_RIGHT], 0, black_margin[MARGIN_RIGHT], window_h),
				Rect2(0, 0, (float)black_margin[MARGIN_RIGHT] / sz.x, (float)window_h / sz.y));
	} else if (black_margin[MARGIN_RIGHT]) {
		WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, storage->resources.black_tex);

		draw_generic_textured_rect(Rect2(window_w - black_margin[MARGIN_RIGHT], 0, black_margin[MARGIN_RIGHT], window_h), Rect2(0, 0, 1, 1));
	}

	if (black_image[MARGIN_TOP].is_valid()) {
		_bind_canvas_texture(black_image[MARGIN_TOP], RID(), true);

		Size2 sz(storage->texture_get_width(black_image[MARGIN_TOP]), storage->texture_get_height(black_image[MARGIN_TOP]));
		draw_generic_textured_rect(Rect2(0, 0, window_w, black_margin[MARGIN_TOP]),
				Rect2(0, 0, (float)window_w / sz.x, (float)black_margin[MARGIN_TOP] / sz.y));

	} else if (black_margin[MARGIN_TOP]) {
		WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, storage->resources.black_tex);

		draw_generic_textured_rect(Rect2(0, 0, window_w, black_margin[MARGIN_TOP]), Rect2(0, 0, 1, 1));
	}

	if (black_image[MARGIN_BOTTOM].is_valid()) {
		_bind_canvas_texture(black_image[MARGIN_BOTTOM], RID(), true);

		Size2 sz(storage->texture_get_width(black_image[MARGIN_BOTTOM]), storage->texture_get_height(black_image[MARGIN_BOTTOM]));
		draw_generic_textured_rect(Rect2(0, window_h - black_margin[MARGIN_BOTTOM], window_w, black_margin[MARGIN_BOTTOM]),
				Rect2(0, 0, (float)window_w / sz.x, (float)black_margin[MARGIN_BOTTOM] / sz.y));

	} else if (black_margin[MARGIN_BOTTOM]) {
		WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, storage->resources.black_tex);

		draw_generic_textured_rect(Rect2(0, window_h - black_margin[MARGIN_BOTTOM], window_w, black_margin[MARGIN_BOTTOM]), Rect2(0, 0, 1, 1));
	}
}

void RasterizerCanvasBaseGLES3::initialize() {
	int flag_stream_mode = GLOBAL_GET("rendering/2d/opengl/legacy_stream");
	switch (flag_stream_mode) {
		default: {
			_buffer_upload_usage_flag = GL_STREAM_DRAW;
		} break;
		case 1: {
			_buffer_upload_usage_flag = GL_DYNAMIC_DRAW;
		} break;
		case 2: {
			_buffer_upload_usage_flag = GL_STREAM_DRAW;
		} break;
	}

	{
		//quad buffers

		glGenBuffers(1, &data.canvas_quad_vertices);
		glBindBuffer(GL_ARRAY_BUFFER, data.canvas_quad_vertices);
		{
			const float qv[8] = {
				0, 0,
				0, 1,
				1, 1,
				1, 0
			};

			glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, qv, GL_STATIC_DRAW);
		}

		glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind

		glGenVertexArrays(1, &data.canvas_quad_array);
		glBindVertexArray(data.canvas_quad_array);
		glBindBuffer(GL_ARRAY_BUFFER, data.canvas_quad_vertices);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
		glEnableVertexAttribArray(0);
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind
	}
	{
		//particle quad buffers

		glGenBuffers(1, &data.particle_quad_vertices);
		glBindBuffer(GL_ARRAY_BUFFER, data.particle_quad_vertices);
		{
			//quad of size 1, with pivot on the center for particles, then regular UVS. Color is general plus fetched from particle
			const float qv[16] = {
				-0.5, -0.5,
				0.0, 0.0,
				-0.5, 0.5,
				0.0, 1.0,
				0.5, 0.5,
				1.0, 1.0,
				0.5, -0.5,
				1.0, 0.0
			};

			glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 16, qv, GL_STATIC_DRAW);
		}

		glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind

		glGenVertexArrays(1, &data.particle_quad_array);
		glBindVertexArray(data.particle_quad_array);
		glBindBuffer(GL_ARRAY_BUFFER, data.particle_quad_vertices);
		glEnableVertexAttribArray(VS::ARRAY_VERTEX);
		glVertexAttribPointer(VS::ARRAY_VERTEX, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
		glEnableVertexAttribArray(VS::ARRAY_TEX_UV);
		glVertexAttribPointer(VS::ARRAY_TEX_UV, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, CAST_INT_TO_UCHAR_PTR(8));
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind
	}
	{
		uint32_t poly_size = GLOBAL_DEF_RST("rendering/limits/buffers/canvas_polygon_buffer_size_kb", 128);
		ProjectSettings::get_singleton()->set_custom_property_info("rendering/limits/buffers/canvas_polygon_buffer_size_kb", PropertyInfo(Variant::INT, "rendering/limits/buffers/canvas_polygon_buffer_size_kb", PROPERTY_HINT_RANGE, "0,256,1,or_greater"));
		poly_size = MAX(poly_size, 2); // minimum 2k, may still see anomalies in editor
		poly_size *= 1024; //kb
		glGenBuffers(1, &data.polygon_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, data.polygon_buffer);
		glBufferData(GL_ARRAY_BUFFER, poly_size, nullptr, GL_DYNAMIC_DRAW); //allocate max size
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		data.polygon_buffer_size = poly_size;

		//quad arrays
		for (int i = 0; i < Data::NUM_QUAD_ARRAY_VARIATIONS; i++) {
			glGenVertexArrays(1, &data.polygon_buffer_quad_arrays[i]);
			glBindVertexArray(data.polygon_buffer_quad_arrays[i]);
			glBindBuffer(GL_ARRAY_BUFFER, data.polygon_buffer);

			int uv_ofs = 0;
			int color_ofs = 0;
			int light_angle_ofs = 0;
			int stride = 2 * 4;

			if (i & 1) { //color
				color_ofs = stride;
				stride += 4 * 4;
			}

			if (i & 2) { //uv
				uv_ofs = stride;
				stride += 2 * 4;
			}

			if (i & 4) { //light_angle
				light_angle_ofs = stride;
				stride += 1 * 4;
			}

			glEnableVertexAttribArray(VS::ARRAY_VERTEX);
			glVertexAttribPointer(VS::ARRAY_VERTEX, 2, GL_FLOAT, GL_FALSE, stride, nullptr);

			if (i & 1) {
				glEnableVertexAttribArray(VS::ARRAY_COLOR);
				glVertexAttribPointer(VS::ARRAY_COLOR, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(color_ofs));
			}

			if (i & 2) {
				glEnableVertexAttribArray(VS::ARRAY_TEX_UV);
				glVertexAttribPointer(VS::ARRAY_TEX_UV, 2, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(uv_ofs));
			}

			if (i & 4) {
				// reusing tangent for light_angle
				glEnableVertexAttribArray(VS::ARRAY_TANGENT);
				glVertexAttribPointer(VS::ARRAY_TANGENT, 1, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(light_angle_ofs));
			}

			glBindVertexArray(0);
		}

		glGenVertexArrays(1, &data.polygon_buffer_pointer_array);

		uint32_t index_size = GLOBAL_DEF_RST("rendering/limits/buffers/canvas_polygon_index_buffer_size_kb", 128);
		ProjectSettings::get_singleton()->set_custom_property_info("rendering/limits/buffers/canvas_polygon_index_buffer_size_kb", PropertyInfo(Variant::INT, "rendering/limits/buffers/canvas_polygon_index_buffer_size_kb", PROPERTY_HINT_RANGE, "0,256,1,or_greater"));
		index_size = MAX(index_size, 2);
		index_size *= 1024; //kb
		glGenBuffers(1, &data.polygon_index_buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.polygon_index_buffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_size, nullptr, GL_DYNAMIC_DRAW); //allocate max size
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		data.polygon_index_buffer_size = index_size;
	}

	store_transform(Transform(), state.canvas_item_ubo_data.projection_matrix);

	glGenBuffers(1, &state.canvas_item_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, state.canvas_item_ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(CanvasItemUBO), &state.canvas_item_ubo_data, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	state.canvas_shader.init();
	state.canvas_shader.set_base_material_tex_index(2);
	state.canvas_shadow_shader.init();
	state.lens_shader.init();

	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_RGBA_SHADOWS, storage->config.use_rgba_2d_shadows);
	state.canvas_shadow_shader.set_conditional(CanvasShadowShaderGLES3::USE_RGBA_SHADOWS, storage->config.use_rgba_2d_shadows);

	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_PIXEL_SNAP, GLOBAL_DEF("rendering/2d/snapping/use_gpu_pixel_snap", false));
}

void RasterizerCanvasBaseGLES3::finalize() {
	glDeleteBuffers(1, &data.canvas_quad_vertices);
	glDeleteVertexArrays(1, &data.canvas_quad_array);

	glDeleteBuffers(1, &data.canvas_quad_vertices);
	glDeleteVertexArrays(1, &data.canvas_quad_array);

	glDeleteVertexArrays(1, &data.polygon_buffer_pointer_array);
}

RasterizerCanvasBaseGLES3::RasterizerCanvasBaseGLES3() {
}
