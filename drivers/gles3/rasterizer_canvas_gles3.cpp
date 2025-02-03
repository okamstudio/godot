/**************************************************************************/
/*  rasterizer_canvas_gles3.cpp                                           */
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

#include "rasterizer_canvas_gles3.h"

#include "drivers/gles3/rasterizer_gles3.h"
#include "drivers/gles_common/rasterizer_asserts.h"
#include "servers/visual/visual_server_raster.h"

static const GLenum gl_primitive[] = {
	GL_POINTS,
	GL_LINES,
	GL_LINE_STRIP,
	GL_LINE_LOOP,
	GL_TRIANGLES,
	GL_TRIANGLE_STRIP,
	GL_TRIANGLE_FAN
};

void RasterizerCanvasGLES3::canvas_end() {
	batch_canvas_end();
	RasterizerCanvasBaseGLES3::canvas_end();
}

void RasterizerCanvasGLES3::canvas_begin() {
	batch_canvas_begin();
	RasterizerCanvasBaseGLES3::canvas_begin();
}

void RasterizerCanvasGLES3::canvas_adapt() {
	batch_gl_data.adapt();
}

void RasterizerCanvasGLES3::canvas_render_items_begin(const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform) {
	batch_canvas_render_items_begin(p_modulate, p_light, p_base_transform);
}

void RasterizerCanvasGLES3::canvas_render_items_end() {
	batch_canvas_render_items_end();
}

void RasterizerCanvasGLES3::canvas_render_items(Item *p_item_list, int p_z, const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform) {
	batch_canvas_render_items(p_item_list, p_z, p_modulate, p_light, p_base_transform);
}

void RasterizerCanvasGLES3::gl_enable_scissor(int p_x, int p_y, int p_width, int p_height) const {
	glEnable(GL_SCISSOR_TEST);
	glScissor(p_x, p_y, p_width, p_height);
}

void RasterizerCanvasGLES3::gl_disable_scissor() const {
	glDisable(GL_SCISSOR_TEST);
}

// Legacy non-batched implementation for regression testing.
// Should be removed after testing phase to avoid duplicate codepaths.
void RasterizerCanvasGLES3::_legacy_canvas_render_item(Item *p_ci, RenderItemState &r_ris) {
	storage->info.render._2d_item_count++;

	if (r_ris.prev_distance_field != p_ci->distance_field) {
		state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_DISTANCE_FIELD, p_ci->distance_field);
		r_ris.prev_distance_field = p_ci->distance_field;
		r_ris.rebind_shader = true;
	}

	if (r_ris.current_clip != p_ci->final_clip_owner) {
		r_ris.current_clip = p_ci->final_clip_owner;

		//setup clip
		if (r_ris.current_clip) {
			glEnable(GL_SCISSOR_TEST);
			int y = storage->frame.current_rt->height - (r_ris.current_clip->final_clip_rect.position.y + r_ris.current_clip->final_clip_rect.size.y);
			if (storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_VFLIP]) {
				y = r_ris.current_clip->final_clip_rect.position.y;
			}

			glScissor(r_ris.current_clip->final_clip_rect.position.x, y, r_ris.current_clip->final_clip_rect.size.x, r_ris.current_clip->final_clip_rect.size.y);

		} else {
			glDisable(GL_SCISSOR_TEST);
		}
	}

	if (p_ci->copy_back_buffer) {
		if (p_ci->copy_back_buffer->full) {
			_copy_texscreen(Rect2());
		} else {
			_copy_texscreen(p_ci->copy_back_buffer->rect);
		}
	}

	RasterizerStorageGLES3::Skeleton *skeleton = nullptr;

	{
		//skeleton handling
		if (p_ci->skeleton.is_valid() && storage->skeleton_owner.owns(p_ci->skeleton)) {
			skeleton = storage->skeleton_owner.get(p_ci->skeleton);
			if (!skeleton->use_2d) {
				skeleton = nullptr;
			} else {
				state.skeleton_transform = r_ris.item_group_base_transform * skeleton->base_transform_2d;
				state.skeleton_transform_inverse = state.skeleton_transform.affine_inverse();
			}
		}

		bool use_skeleton = skeleton != nullptr;
		if (r_ris.prev_use_skeleton != use_skeleton) {
			r_ris.rebind_shader = true;
			state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_SKELETON, use_skeleton);
			r_ris.prev_use_skeleton = use_skeleton;
		}

		if (skeleton) {
			WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE0 + storage->config.max_texture_image_units - 4);
			glBindTexture(GL_TEXTURE_2D, skeleton->texture);
			state.using_skeleton = true;
		} else {
			state.using_skeleton = false;
		}
	}

	//begin rect
	Item *material_owner = p_ci->material_owner ? p_ci->material_owner : p_ci;

	RID material = material_owner->material;

	if (material != r_ris.canvas_last_material || r_ris.rebind_shader) {
		RasterizerStorageGLES3::Material *material_ptr = storage->material_owner.getornull(material);
		RasterizerStorageGLES3::Shader *shader_ptr = nullptr;

		if (material_ptr) {
			shader_ptr = material_ptr->shader;

			if (shader_ptr && shader_ptr->mode != VS::SHADER_CANVAS_ITEM) {
				shader_ptr = nullptr; //do not use non canvasitem shader
			}
		}

		if (shader_ptr) {
			if (shader_ptr->canvas_item.uses_screen_texture && !state.canvas_texscreen_used) {
				//copy if not copied before
				_copy_texscreen(Rect2());

				// blend mode will have been enabled so make sure we disable it again later on
				r_ris.last_blend_mode = r_ris.last_blend_mode != RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_DISABLED ? r_ris.last_blend_mode : -1;
			}

			if (shader_ptr != r_ris.shader_cache || r_ris.rebind_shader) {
				if (shader_ptr->canvas_item.uses_time) {
					VisualServerRaster::redraw_request(false);
				}

				state.canvas_shader.set_custom_shader(shader_ptr->custom_code_id);
				state.canvas_shader.bind();
			}

			if (material_ptr->ubo_id) {
				glBindBufferBase(GL_UNIFORM_BUFFER, 2, material_ptr->ubo_id);
			}

			int tc = material_ptr->textures.size();
			RID *textures = material_ptr->textures.ptrw();
			ShaderLanguage::ShaderNode::Uniform::Hint *texture_hints = shader_ptr->texture_hints.ptrw();

			for (int i = 0; i < tc; i++) {
				WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE2 + i);

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
					VisualServerRaster::redraw_request();
				}

				t = t->get_ptr();

				if (storage->config.srgb_decode_supported && t->using_srgb) {
					//no srgb in 2D
					glTexParameteri(t->target, _TEXTURE_SRGB_DECODE_EXT, _SKIP_DECODE_EXT);
					t->using_srgb = false;
				}

				glBindTexture(t->target, t->tex_id);
			}

		} else {
			state.canvas_shader.set_custom_shader(0);
			state.canvas_shader.bind();
		}

		r_ris.shader_cache = shader_ptr;

		r_ris.canvas_last_material = material;
		r_ris.rebind_shader = false;
	}

	int blend_mode = r_ris.shader_cache ? r_ris.shader_cache->canvas_item.blend_mode : RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MIX;
	if (blend_mode == RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_DISABLED && (!storage->frame.current_rt || !storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT])) {
		blend_mode = RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MIX;
	}
	bool unshaded = r_ris.shader_cache && (r_ris.shader_cache->canvas_item.light_mode == RasterizerStorageGLES3::Shader::CanvasItem::LIGHT_MODE_UNSHADED || (blend_mode != RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MIX && blend_mode != RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_PMALPHA));
	bool reclip = false;

	if (r_ris.last_blend_mode != blend_mode) {
		if (r_ris.last_blend_mode == RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_DISABLED) {
			// re-enable it
			glEnable(GL_BLEND);
		} else if (blend_mode == RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_DISABLED) {
			// disable it
			glDisable(GL_BLEND);
		}

		switch (blend_mode) {
			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_DISABLED: {
				// nothing to do here

			} break;
			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MIX: {
				glBlendEquation(GL_FUNC_ADD);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				} else {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
				}

			} break;
			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_ADD: {
				glBlendEquation(GL_FUNC_ADD);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA, GL_ONE);
				} else {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);
				}

			} break;
			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_SUB: {
				glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA, GL_ONE);
				} else {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);
				}
			} break;
			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MUL: {
				glBlendEquation(GL_FUNC_ADD);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_DST_ALPHA, GL_ZERO);
				} else {
					glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_ZERO, GL_ONE);
				}

			} break;
			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_PMALPHA: {
				glBlendEquation(GL_FUNC_ADD);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				} else {
					glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
				}

			} break;
		}

		r_ris.last_blend_mode = blend_mode;
	}

	state.canvas_item_modulate = unshaded ? p_ci->final_modulate : Color(p_ci->final_modulate.r * r_ris.item_group_modulate.r, p_ci->final_modulate.g * r_ris.item_group_modulate.g, p_ci->final_modulate.b * r_ris.item_group_modulate.b, p_ci->final_modulate.a * r_ris.item_group_modulate.a);

	state.final_transform = p_ci->final_transform;
	state.extra_matrix = Transform2D();

	if (state.using_skeleton) {
		state.canvas_shader.set_uniform(CanvasShaderGLES3::SKELETON_TRANSFORM, state.skeleton_transform);
		state.canvas_shader.set_uniform(CanvasShaderGLES3::SKELETON_TRANSFORM_INVERSE, state.skeleton_transform_inverse);
	}

	state.canvas_shader.set_uniform(CanvasShaderGLES3::FINAL_MODULATE, state.canvas_item_modulate);
	state.canvas_shader.set_uniform(CanvasShaderGLES3::MODELVIEW_MATRIX, state.final_transform);
	state.canvas_shader.set_uniform(CanvasShaderGLES3::EXTRA_MATRIX, state.extra_matrix);
	if (storage->frame.current_rt) {
		state.canvas_shader.set_uniform(CanvasShaderGLES3::SCREEN_PIXEL_SIZE, Vector2(1.0 / storage->frame.current_rt->width, 1.0 / storage->frame.current_rt->height));
	} else {
		state.canvas_shader.set_uniform(CanvasShaderGLES3::SCREEN_PIXEL_SIZE, Vector2(1.0, 1.0));
	}
	if (unshaded || (state.canvas_item_modulate.a > 0.001 && (!r_ris.shader_cache || r_ris.shader_cache->canvas_item.light_mode != RasterizerStorageGLES3::Shader::CanvasItem::LIGHT_MODE_LIGHT_ONLY) && !p_ci->light_masked)) {
		_legacy_canvas_item_render_commands(p_ci, r_ris.current_clip, reclip, nullptr);
	}

	if ((blend_mode == RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MIX || blend_mode == RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_PMALPHA) && r_ris.item_group_light && !unshaded) {
		Light *light = r_ris.item_group_light;
		bool light_used = false;
		VS::CanvasLightMode mode = VS::CANVAS_LIGHT_MODE_ADD;
		state.canvas_item_modulate = p_ci->final_modulate; // remove the canvas modulate

		while (light) {
			if (p_ci->light_mask & light->item_mask && r_ris.item_group_z >= light->z_min && r_ris.item_group_z <= light->z_max && p_ci->global_rect_cache.intersects_transformed(light->xform_cache, light->rect_cache)) {
				//intersects this light

				if (!light_used || mode != light->mode) {
					mode = light->mode;

					switch (mode) {
						case VS::CANVAS_LIGHT_MODE_ADD: {
							glBlendEquation(GL_FUNC_ADD);
							glBlendFunc(GL_SRC_ALPHA, GL_ONE);

						} break;
						case VS::CANVAS_LIGHT_MODE_SUB: {
							glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
							glBlendFunc(GL_SRC_ALPHA, GL_ONE);
						} break;
						case VS::CANVAS_LIGHT_MODE_MIX:
						case VS::CANVAS_LIGHT_MODE_MASK: {
							glBlendEquation(GL_FUNC_ADD);
							glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

						} break;
					}
				}

				if (!light_used) {
					state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_LIGHTING, true);
					light_used = true;
				}

				bool has_shadow = light->shadow_buffer.is_valid() && p_ci->light_mask & light->item_shadow_mask;

				state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_SHADOWS, has_shadow);
				if (has_shadow) {
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_USE_GRADIENT, light->shadow_gradient_length > 0);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_NEAREST, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_NONE);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF3, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_PCF3);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF5, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_PCF5);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF7, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_PCF7);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF9, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_PCF9);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF13, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_PCF13);
				}

				bool light_rebind = state.canvas_shader.bind();

				if (light_rebind) {
					state.canvas_shader.set_uniform(CanvasShaderGLES3::FINAL_MODULATE, state.canvas_item_modulate);
					state.canvas_shader.set_uniform(CanvasShaderGLES3::MODELVIEW_MATRIX, state.final_transform);
					state.canvas_shader.set_uniform(CanvasShaderGLES3::EXTRA_MATRIX, Transform2D());
					if (storage->frame.current_rt) {
						state.canvas_shader.set_uniform(CanvasShaderGLES3::SCREEN_PIXEL_SIZE, Vector2(1.0 / storage->frame.current_rt->width, 1.0 / storage->frame.current_rt->height));
					} else {
						state.canvas_shader.set_uniform(CanvasShaderGLES3::SCREEN_PIXEL_SIZE, Vector2(1.0, 1.0));
					}
					if (state.using_skeleton) {
						state.canvas_shader.set_uniform(CanvasShaderGLES3::SKELETON_TRANSFORM, state.skeleton_transform);
						state.canvas_shader.set_uniform(CanvasShaderGLES3::SKELETON_TRANSFORM_INVERSE, state.skeleton_transform_inverse);
					}
				}

				glBindBufferBase(GL_UNIFORM_BUFFER, 1, static_cast<LightInternal *>(light_internal_owner.get(light->light_internal))->ubo);

				if (has_shadow) {
					RasterizerStorageGLES3::CanvasLightShadow *cls = storage->canvas_light_shadow_owner.get(light->shadow_buffer);
					WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE0 + storage->config.max_texture_image_units - 2);
					glBindTexture(GL_TEXTURE_2D, cls->distance);

					/*canvas_shader.set_uniform(CanvasShaderGLES3::SHADOW_MATRIX,light->shadow_matrix_cache);
					canvas_shader.set_uniform(CanvasShaderGLES3::SHADOW_ESM_MULTIPLIER,light->shadow_esm_mult);
					canvas_shader.set_uniform(CanvasShaderGLES3::LIGHT_SHADOW_COLOR,light->shadow_color);*/
				}

				WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE0 + storage->config.max_texture_image_units - 1);
				RasterizerStorageGLES3::Texture *t = storage->texture_owner.getornull(light->texture);
				if (!t) {
					glBindTexture(GL_TEXTURE_2D, storage->resources.white_tex);
				} else {
					t = t->get_ptr();

					glBindTexture(t->target, t->tex_id);
				}

				WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE0);
				_legacy_canvas_item_render_commands(p_ci, r_ris.current_clip, reclip, nullptr); //redraw using light
			}

			light = light->next_ptr;
		}

		if (light_used) {
			state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_LIGHTING, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_SHADOWS, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_NEAREST, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF3, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF5, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF7, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF9, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF13, false);

			state.canvas_shader.bind();

			r_ris.last_blend_mode = -1;

			/*
			//this is set again, so it should not be needed anyway?
			state.canvas_item_modulate = unshaded ? ci->final_modulate : Color(
						ci->final_modulate.r * p_modulate.r,
						ci->final_modulate.g * p_modulate.g,
						ci->final_modulate.b * p_modulate.b,
						ci->final_modulate.a * p_modulate.a );


			state.canvas_shader.set_uniform(CanvasShaderGLES3::MODELVIEW_MATRIX,state.final_transform);
			state.canvas_shader.set_uniform(CanvasShaderGLES3::EXTRA_MATRIX,Transform2D());
			state.canvas_shader.set_uniform(CanvasShaderGLES3::FINAL_MODULATE,state.canvas_item_modulate);

			glBlendEquation(GL_FUNC_ADD);

			if (storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
				glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			} else {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}

			//@TODO RESET canvas_blend_mode
			*/
		}
	}

	if (reclip) {
		glEnable(GL_SCISSOR_TEST);
		int y = storage->frame.current_rt->height - (r_ris.current_clip->final_clip_rect.position.y + r_ris.current_clip->final_clip_rect.size.y);
		if (storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_VFLIP]) {
			y = r_ris.current_clip->final_clip_rect.position.y;
		}
		glScissor(r_ris.current_clip->final_clip_rect.position.x, y, r_ris.current_clip->final_clip_rect.size.width, r_ris.current_clip->final_clip_rect.size.height);
	}
}

void RasterizerCanvasGLES3::render_batches(Item *p_current_clip, bool &r_reclip, RasterizerStorageGLES3::Material *p_material) {
	//	bdata.reset_flush();
	//	return;

	int num_batches = bdata.batches.size();

	for (int batch_num = 0; batch_num < num_batches; batch_num++) {
		const Batch &batch = bdata.batches[batch_num];

		switch (batch.type) {
			case RasterizerStorageCommon::BT_RECT: {
				_batch_render_generic(batch, p_material);
			} break;
			case RasterizerStorageCommon::BT_POLY: {
				_batch_render_generic(batch, p_material);
			} break;
			case RasterizerStorageCommon::BT_LINE: {
				_batch_render_lines(batch, p_material, false);
			} break;
			case RasterizerStorageCommon::BT_LINE_AA: {
				_batch_render_lines(batch, p_material, true);
			} break;
			default: {
				int end_command = batch.first_command + batch.num_commands;

				DEV_ASSERT(batch.item);
				RasterizerCanvas::Item::Command *const *commands = batch.item->commands.ptr();

				for (int i = batch.first_command; i < end_command; i++) {
					Item::Command *c = commands[i];

					switch (c->type) {
						case Item::Command::TYPE_LINE: {
							Item::CommandLine *line = static_cast<Item::CommandLine *>(c);
							_set_texture_rect_mode(false);

							_bind_canvas_texture(RID(), RID());

							glVertexAttrib4f(VS::ARRAY_COLOR, line->color.r, line->color.g, line->color.b, line->color.a);

							if (line->width <= 1) {
								Vector2 verts[2] = {
									Vector2(line->from.x, line->from.y),
									Vector2(line->to.x, line->to.y)
								};

#ifdef GLES_OVER_GL
								if (line->antialiased) {
									glEnable(GL_LINE_SMOOTH);
								}
#endif
								//glLineWidth(line->width);
								_draw_gui_primitive(2, verts, nullptr, nullptr);

#ifdef GLES_OVER_GL
								if (line->antialiased) {
									glDisable(GL_LINE_SMOOTH);
								}
#endif
							} else {
								//thicker line

								Vector2 t = (line->from - line->to).normalized().tangent() * line->width * 0.5;

								Vector2 verts[4] = {
									line->from - t,
									line->from + t,
									line->to + t,
									line->to - t,
								};

								//glLineWidth(line->width);
								_draw_gui_primitive(4, verts, nullptr, nullptr);
#ifdef GLES_OVER_GL
								if (line->antialiased) {
									glEnable(GL_LINE_SMOOTH);
									for (int j = 0; j < 4; j++) {
										Vector2 vertsl[2] = {
											verts[j],
											verts[(j + 1) % 4],
										};
										_draw_gui_primitive(2, vertsl, nullptr, nullptr);
									}
									glDisable(GL_LINE_SMOOTH);
								}
#endif
							}
						} break;
						case Item::Command::TYPE_POLYLINE: {
							Item::CommandPolyLine *pline = static_cast<Item::CommandPolyLine *>(c);
							_set_texture_rect_mode(false);

							_bind_canvas_texture(RID(), RID());

							if (pline->triangles.size()) {
#ifdef RASTERIZER_EXTRA_CHECKS
								if (pline->triangle_colors.ptr() && (pline->triangle_colors.size() != 1)) {
									RAST_DEV_DEBUG_ASSERT(pline->triangle_colors.size() == pline->triangles.size());
								}
#endif
								_draw_generic(GL_TRIANGLE_STRIP, pline->triangles.size(), pline->triangles.ptr(), nullptr, pline->triangle_colors.ptr(), pline->triangle_colors.size() == 1);
#ifdef GLES_OVER_GL
								glEnable(GL_LINE_SMOOTH);
								if (pline->multiline) {
									//needs to be different
								} else {
									_draw_generic(GL_LINE_LOOP, pline->lines.size(), pline->lines.ptr(), nullptr, pline->line_colors.ptr(), pline->line_colors.size() == 1);
								}
								glDisable(GL_LINE_SMOOTH);
#endif
							} else {
#ifdef GLES_OVER_GL
								if (pline->antialiased) {
									glEnable(GL_LINE_SMOOTH);
								}
#endif

								if (pline->multiline) {
									int todo = pline->lines.size() / 2;
									int max_per_call = data.polygon_buffer_size / (sizeof(real_t) * 4);
									int offset = 0;

									while (todo) {
										int to_draw = MIN(max_per_call, todo);
										_draw_generic(GL_LINES, to_draw * 2, &pline->lines.ptr()[offset], nullptr, pline->line_colors.size() == 1 ? pline->line_colors.ptr() : &pline->line_colors.ptr()[offset], pline->line_colors.size() == 1);
										todo -= to_draw;
										offset += to_draw * 2;
									}

								} else {
									_draw_generic(GL_LINE_STRIP, pline->lines.size(), pline->lines.ptr(), nullptr, pline->line_colors.ptr(), pline->line_colors.size() == 1);
								}

#ifdef GLES_OVER_GL
								if (pline->antialiased) {
									glDisable(GL_LINE_SMOOTH);
								}
#endif
							}

						} break;
						case Item::Command::TYPE_RECT: {
							Item::CommandRect *rect = static_cast<Item::CommandRect *>(c);

							//set color
							glVertexAttrib4f(VS::ARRAY_COLOR, rect->modulate.r, rect->modulate.g, rect->modulate.b, rect->modulate.a);

							RasterizerStorageGLES3::Texture *texture = _bind_canvas_texture(rect->texture, rect->normal_map);

							if (use_nvidia_rect_workaround) {
								render_rect_nvidia_workaround(rect, texture);
							} else {
								_set_texture_rect_mode(true);

								if (texture) {
									bool untile = false;

									if (rect->flags & CANVAS_RECT_TILE && !(texture->flags & VS::TEXTURE_FLAG_REPEAT)) {
										glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
										glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
										untile = true;
									}

									Size2 texpixel_size(1.0 / texture->width, 1.0 / texture->height);
									Rect2 src_rect = (rect->flags & CANVAS_RECT_REGION) ? Rect2(rect->source.position * texpixel_size, rect->source.size * texpixel_size) : Rect2(0, 0, 1, 1);
									Rect2 dst_rect = Rect2(rect->rect.position, rect->rect.size);

									if (dst_rect.size.width < 0) {
										dst_rect.position.x += dst_rect.size.width;
										dst_rect.size.width *= -1;
									}
									if (dst_rect.size.height < 0) {
										dst_rect.position.y += dst_rect.size.height;
										dst_rect.size.height *= -1;
									}

									if (rect->flags & CANVAS_RECT_FLIP_H) {
										src_rect.size.x *= -1;
									}

									if (rect->flags & CANVAS_RECT_FLIP_V) {
										src_rect.size.y *= -1;
									}

									if (rect->flags & CANVAS_RECT_TRANSPOSE) {
										dst_rect.size.x *= -1; // Encoding in the dst_rect.z uniform
									}

									state.canvas_shader.set_uniform(CanvasShaderGLES3::COLOR_TEXPIXEL_SIZE, texpixel_size);

									state.canvas_shader.set_uniform(CanvasShaderGLES3::DST_RECT, Color(dst_rect.position.x, dst_rect.position.y, dst_rect.size.x, dst_rect.size.y));
									state.canvas_shader.set_uniform(CanvasShaderGLES3::SRC_RECT, Color(src_rect.position.x, src_rect.position.y, src_rect.size.x, src_rect.size.y));
									state.canvas_shader.set_uniform(CanvasShaderGLES3::CLIP_RECT_UV, rect->flags & CANVAS_RECT_CLIP_UV);

									glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
									storage->info.render._2d_draw_call_count++;

									if (untile) {
										glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
										glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
									}

								} else {
									Rect2 dst_rect = Rect2(rect->rect.position, rect->rect.size);

									if (dst_rect.size.width < 0) {
										dst_rect.position.x += dst_rect.size.width;
										dst_rect.size.width *= -1;
									}
									if (dst_rect.size.height < 0) {
										dst_rect.position.y += dst_rect.size.height;
										dst_rect.size.height *= -1;
									}

									state.canvas_shader.set_uniform(CanvasShaderGLES3::DST_RECT, Color(dst_rect.position.x, dst_rect.position.y, dst_rect.size.x, dst_rect.size.y));
									state.canvas_shader.set_uniform(CanvasShaderGLES3::SRC_RECT, Color(0, 0, 1, 1));
									state.canvas_shader.set_uniform(CanvasShaderGLES3::CLIP_RECT_UV, false);
									glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
									storage->info.render._2d_draw_call_count++;
								}
							} // if not use nvidia workaround
						} break;
						case Item::Command::TYPE_NINEPATCH: {
							Item::CommandNinePatch *np = static_cast<Item::CommandNinePatch *>(c);

							_set_texture_rect_mode(true, true);

							glVertexAttrib4f(VS::ARRAY_COLOR, np->color.r, np->color.g, np->color.b, np->color.a);

							RasterizerStorageGLES3::Texture *texture = _bind_canvas_texture(np->texture, np->normal_map);

							Size2 texpixel_size;

							if (!texture) {
								texpixel_size = Size2(1, 1);

								state.canvas_shader.set_uniform(CanvasShaderGLES3::SRC_RECT, Color(0, 0, 1, 1));

							} else {
								if (np->source != Rect2()) {
									texpixel_size = Size2(1.0 / np->source.size.width, 1.0 / np->source.size.height);
									state.canvas_shader.set_uniform(CanvasShaderGLES3::SRC_RECT, Color(np->source.position.x / texture->width, np->source.position.y / texture->height, np->source.size.x / texture->width, np->source.size.y / texture->height));
								} else {
									texpixel_size = Size2(1.0 / texture->width, 1.0 / texture->height);
									state.canvas_shader.set_uniform(CanvasShaderGLES3::SRC_RECT, Color(0, 0, 1, 1));
								}
							}

							state.canvas_shader.set_uniform(CanvasShaderGLES3::COLOR_TEXPIXEL_SIZE, texpixel_size);
							state.canvas_shader.set_uniform(CanvasShaderGLES3::CLIP_RECT_UV, false);
							state.canvas_shader.set_uniform(CanvasShaderGLES3::NP_REPEAT_H, int(np->axis_x));
							state.canvas_shader.set_uniform(CanvasShaderGLES3::NP_REPEAT_V, int(np->axis_y));
							state.canvas_shader.set_uniform(CanvasShaderGLES3::NP_DRAW_CENTER, np->draw_center);
							state.canvas_shader.set_uniform(CanvasShaderGLES3::NP_MARGINS, Color(np->margin[MARGIN_LEFT], np->margin[MARGIN_TOP], np->margin[MARGIN_RIGHT], np->margin[MARGIN_BOTTOM]));
							state.canvas_shader.set_uniform(CanvasShaderGLES3::DST_RECT, Color(np->rect.position.x, np->rect.position.y, np->rect.size.x, np->rect.size.y));

							glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

							storage->info.render._2d_draw_call_count++;
						} break;
						case Item::Command::TYPE_PRIMITIVE: {
							Item::CommandPrimitive *primitive = static_cast<Item::CommandPrimitive *>(c);
							_set_texture_rect_mode(false);

							ERR_CONTINUE(primitive->points.size() < 1);

							RasterizerStorageGLES3::Texture *texture = _bind_canvas_texture(primitive->texture, primitive->normal_map);

							if (texture) {
								Size2 texpixel_size(1.0 / texture->width, 1.0 / texture->height);
								state.canvas_shader.set_uniform(CanvasShaderGLES3::COLOR_TEXPIXEL_SIZE, texpixel_size);
							}

							// we need a temporary because this must be nulled out
							// if only a single color specified
							const Color *colors = primitive->colors.ptr();
							if (primitive->colors.size() == 1 && primitive->points.size() > 1) {
								Color col = primitive->colors[0];
								glVertexAttrib4f(VS::ARRAY_COLOR, col.r, col.g, col.b, col.a);
								colors = nullptr;

							} else if (primitive->colors.empty()) {
								glVertexAttrib4f(VS::ARRAY_COLOR, 1, 1, 1, 1);
							}
#ifdef RASTERIZER_EXTRA_CHECKS
							else {
								RAST_DEV_DEBUG_ASSERT(primitive->colors.size() == primitive->points.size());
							}

							if (primitive->uvs.ptr()) {
								RAST_DEV_DEBUG_ASSERT(primitive->uvs.size() == primitive->points.size());
							}
#endif

							_draw_gui_primitive(primitive->points.size(), primitive->points.ptr(), colors, primitive->uvs.ptr());

						} break;
						case Item::Command::TYPE_POLYGON: {
							Item::CommandPolygon *polygon = static_cast<Item::CommandPolygon *>(c);
							_set_texture_rect_mode(false);

							RasterizerStorageGLES3::Texture *texture = _bind_canvas_texture(polygon->texture, polygon->normal_map);

							if (texture) {
								Size2 texpixel_size(1.0 / texture->width, 1.0 / texture->height);
								state.canvas_shader.set_uniform(CanvasShaderGLES3::COLOR_TEXPIXEL_SIZE, texpixel_size);
							}

							_draw_polygon(polygon->indices.ptr(), polygon->count, polygon->points.size(), polygon->points.ptr(), polygon->uvs.ptr(), polygon->colors.ptr(), polygon->colors.size() == 1, polygon->bones.ptr(), polygon->weights.ptr());
#ifdef GLES_OVER_GL
							if (polygon->antialiased) {
								glEnable(GL_LINE_SMOOTH);
								if (polygon->antialiasing_use_indices) {
									_draw_generic_indices(GL_LINE_STRIP, polygon->indices.ptr(), polygon->count, polygon->points.size(), polygon->points.ptr(), polygon->uvs.ptr(), polygon->colors.ptr(), polygon->colors.size() == 1);
								} else {
									_draw_generic(GL_LINE_LOOP, polygon->points.size(), polygon->points.ptr(), polygon->uvs.ptr(), polygon->colors.ptr(), polygon->colors.size() == 1);
								}
								glDisable(GL_LINE_SMOOTH);
							}
#endif

						} break;
						case Item::Command::TYPE_MESH: {
							Item::CommandMesh *mesh = static_cast<Item::CommandMesh *>(c);
							_set_texture_rect_mode(false);

							RasterizerStorageGLES3::Texture *texture = _bind_canvas_texture(mesh->texture, mesh->normal_map);

							if (texture) {
								Size2 texpixel_size(1.0 / texture->width, 1.0 / texture->height);
								state.canvas_shader.set_uniform(CanvasShaderGLES3::COLOR_TEXPIXEL_SIZE, texpixel_size);
							}

							state.canvas_shader.set_uniform(CanvasShaderGLES3::MODELVIEW_MATRIX, state.final_transform * mesh->transform);

							RasterizerStorageGLES3::Mesh *mesh_data = storage->mesh_owner.getornull(mesh->mesh);
							if (mesh_data) {
								for (int j = 0; j < mesh_data->surfaces.size(); j++) {
									RasterizerStorageGLES3::Surface *s = mesh_data->surfaces[j];
									// materials are ignored in 2D meshes, could be added but many things (ie, lighting mode, reading from screen, etc) would break as they are not meant be set up at this point of drawing
									glBindVertexArray(s->array_id);

									glVertexAttrib4f(VS::ARRAY_COLOR, mesh->modulate.r, mesh->modulate.g, mesh->modulate.b, mesh->modulate.a);

									if (s->index_array_len) {
										glDrawElements(gl_primitive[s->primitive], s->index_array_len, (s->array_len >= (1 << 16)) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, nullptr);
									} else {
										glDrawArrays(gl_primitive[s->primitive], 0, s->array_len);
									}
									storage->info.render._2d_draw_call_count++;

									glBindVertexArray(0);
								}
							}
							state.canvas_shader.set_uniform(CanvasShaderGLES3::MODELVIEW_MATRIX, state.final_transform);

						} break;
						case Item::Command::TYPE_MULTIMESH: {
							Item::CommandMultiMesh *mmesh = static_cast<Item::CommandMultiMesh *>(c);

							RasterizerStorageGLES3::MultiMesh *multi_mesh = storage->multimesh_owner.getornull(mmesh->multimesh);

							if (!multi_mesh) {
								break;
							}

							RasterizerStorageGLES3::Mesh *mesh_data = storage->mesh_owner.getornull(multi_mesh->mesh);

							if (!mesh_data) {
								break;
							}

							int amount = MIN(multi_mesh->size, multi_mesh->visible_instances);

							if (amount == -1) {
								amount = multi_mesh->size;
							}

							if (!amount) {
								break;
							}

							RasterizerStorageGLES3::Texture *texture = _bind_canvas_texture(mmesh->texture, mmesh->normal_map);

							state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_INSTANCE_CUSTOM, multi_mesh->custom_data_format != VS::MULTIMESH_CUSTOM_DATA_NONE);
							state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_INSTANCING, true);
							//reset shader and force rebind
							state.using_texture_rect = true;
							_set_texture_rect_mode(false);

							if (texture) {
								Size2 texpixel_size(1.0 / texture->width, 1.0 / texture->height);
								state.canvas_shader.set_uniform(CanvasShaderGLES3::COLOR_TEXPIXEL_SIZE, texpixel_size);
							}

							glVertexAttrib4f(VS::ARRAY_COLOR, 1.0, 1.0, 1.0, 1.0);

							for (int j = 0; j < mesh_data->surfaces.size(); j++) {
								RasterizerStorageGLES3::Surface *s = mesh_data->surfaces[j];
								// materials are ignored in 2D meshes, could be added but many things (ie, lighting mode, reading from screen, etc) would break as they are not meant be set up at this point of drawing
								glBindVertexArray(s->instancing_array_id);

								glBindBuffer(GL_ARRAY_BUFFER, multi_mesh->buffer); //modify the buffer

								int stride = (multi_mesh->xform_floats + multi_mesh->color_floats + multi_mesh->custom_data_floats) * 4;
								glEnableVertexAttribArray(8);
								glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(0));
								glVertexAttribDivisor(8, 1);
								glEnableVertexAttribArray(9);
								glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(4 * 4));
								glVertexAttribDivisor(9, 1);

								int color_ofs;

								if (multi_mesh->transform_format == VS::MULTIMESH_TRANSFORM_3D) {
									glEnableVertexAttribArray(10);
									glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(8 * 4));
									glVertexAttribDivisor(10, 1);
									color_ofs = 12 * 4;
								} else {
									glDisableVertexAttribArray(10);
									glVertexAttrib4f(10, 0, 0, 1, 0);
									color_ofs = 8 * 4;
								}

								int custom_data_ofs = color_ofs;

								switch (multi_mesh->color_format) {
									case VS::MULTIMESH_COLOR_MAX:
									case VS::MULTIMESH_COLOR_NONE: {
										glDisableVertexAttribArray(11);
										glVertexAttrib4f(11, 1, 1, 1, 1);
									} break;
									case VS::MULTIMESH_COLOR_8BIT: {
										glEnableVertexAttribArray(11);
										glVertexAttribPointer(11, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, CAST_INT_TO_UCHAR_PTR(color_ofs));
										glVertexAttribDivisor(11, 1);
										custom_data_ofs += 4;

									} break;
									case VS::MULTIMESH_COLOR_FLOAT: {
										glEnableVertexAttribArray(11);
										glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(color_ofs));
										glVertexAttribDivisor(11, 1);
										custom_data_ofs += 4 * 4;
									} break;
								}

								switch (multi_mesh->custom_data_format) {
									case VS::MULTIMESH_CUSTOM_DATA_MAX:
									case VS::MULTIMESH_CUSTOM_DATA_NONE: {
										glDisableVertexAttribArray(12);
										glVertexAttrib4f(12, 1, 1, 1, 1);
									} break;
									case VS::MULTIMESH_CUSTOM_DATA_8BIT: {
										glEnableVertexAttribArray(12);
										glVertexAttribPointer(12, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, CAST_INT_TO_UCHAR_PTR(custom_data_ofs));
										glVertexAttribDivisor(12, 1);

									} break;
									case VS::MULTIMESH_CUSTOM_DATA_FLOAT: {
										glEnableVertexAttribArray(12);
										glVertexAttribPointer(12, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(custom_data_ofs));
										glVertexAttribDivisor(12, 1);
									} break;
								}

								if (s->index_array_len) {
									glDrawElementsInstanced(gl_primitive[s->primitive], s->index_array_len, (s->array_len >= (1 << 16)) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, nullptr, amount);
								} else {
									glDrawArraysInstanced(gl_primitive[s->primitive], 0, s->array_len, amount);
								}
								storage->info.render._2d_draw_call_count++;

								glBindVertexArray(0);
							}

							state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_INSTANCE_CUSTOM, false);
							state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_INSTANCING, false);
							state.using_texture_rect = true;
							_set_texture_rect_mode(false);

						} break;
						case Item::Command::TYPE_PARTICLES: {
							Item::CommandParticles *particles_cmd = static_cast<Item::CommandParticles *>(c);

							RasterizerStorageGLES3::Particles *particles = storage->particles_owner.getornull(particles_cmd->particles);
							if (!particles) {
								break;
							}

							if (particles->inactive && !particles->emitting) {
								break;
							}

							glVertexAttrib4f(VS::ARRAY_COLOR, 1, 1, 1, 1); //not used, so keep white

							VisualServerRaster::redraw_request(false);

							storage->particles_request_process(particles_cmd->particles);
							//enable instancing

							state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_INSTANCE_CUSTOM, true);
							state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_PARTICLES, true);
							state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_INSTANCING, true);
							//reset shader and force rebind
							state.using_texture_rect = true;
							_set_texture_rect_mode(false);

							RasterizerStorageGLES3::Texture *texture = _bind_canvas_texture(particles_cmd->texture, particles_cmd->normal_map);

							if (texture) {
								Size2 texpixel_size(1.0 / texture->width, 1.0 / texture->height);
								state.canvas_shader.set_uniform(CanvasShaderGLES3::COLOR_TEXPIXEL_SIZE, texpixel_size);
							} else {
								state.canvas_shader.set_uniform(CanvasShaderGLES3::COLOR_TEXPIXEL_SIZE, Vector2(1.0, 1.0));
							}

							if (!particles->use_local_coords) {
								Transform2D inv_xf;
								inv_xf.set_axis(0, Vector2(particles->emission_transform.basis.get_axis(0).x, particles->emission_transform.basis.get_axis(0).y));
								inv_xf.set_axis(1, Vector2(particles->emission_transform.basis.get_axis(1).x, particles->emission_transform.basis.get_axis(1).y));
								inv_xf.set_origin(Vector2(particles->emission_transform.get_origin().x, particles->emission_transform.get_origin().y));
								inv_xf.affine_invert();

								state.canvas_shader.set_uniform(CanvasShaderGLES3::MODELVIEW_MATRIX, state.final_transform * inv_xf);
							}

							glBindVertexArray(data.particle_quad_array); //use particle quad array
							glBindBuffer(GL_ARRAY_BUFFER, particles->particle_buffers[0]); //bind particle buffer

							int stride = sizeof(float) * 4 * 6;

							int amount = particles->amount;

							if (particles->draw_order != VS::PARTICLES_DRAW_ORDER_LIFETIME) {
								glEnableVertexAttribArray(8); //xform x
								glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 3));
								glVertexAttribDivisor(8, 1);
								glEnableVertexAttribArray(9); //xform y
								glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 4));
								glVertexAttribDivisor(9, 1);
								glEnableVertexAttribArray(10); //xform z
								glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 5));
								glVertexAttribDivisor(10, 1);
								glEnableVertexAttribArray(11); //color
								glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE, stride, nullptr);
								glVertexAttribDivisor(11, 1);
								glEnableVertexAttribArray(12); //custom
								glVertexAttribPointer(12, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 2));
								glVertexAttribDivisor(12, 1);

								glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, amount);
								storage->info.render._2d_draw_call_count++;
							} else {
								//split
								int split = int(Math::ceil(particles->phase * particles->amount));

								if (amount - split > 0) {
									glEnableVertexAttribArray(8); //xform x
									glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(stride * split + sizeof(float) * 4 * 3));
									glVertexAttribDivisor(8, 1);
									glEnableVertexAttribArray(9); //xform y
									glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(stride * split + sizeof(float) * 4 * 4));
									glVertexAttribDivisor(9, 1);
									glEnableVertexAttribArray(10); //xform z
									glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(stride * split + sizeof(float) * 4 * 5));
									glVertexAttribDivisor(10, 1);
									glEnableVertexAttribArray(11); //color
									glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(stride * split + 0));
									glVertexAttribDivisor(11, 1);
									glEnableVertexAttribArray(12); //custom
									glVertexAttribPointer(12, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(stride * split + sizeof(float) * 4 * 2));
									glVertexAttribDivisor(12, 1);

									glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, amount - split);
									storage->info.render._2d_draw_call_count++;
								}

								if (split > 0) {
									glEnableVertexAttribArray(8); //xform x
									glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 3));
									glVertexAttribDivisor(8, 1);
									glEnableVertexAttribArray(9); //xform y
									glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 4));
									glVertexAttribDivisor(9, 1);
									glEnableVertexAttribArray(10); //xform z
									glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 5));
									glVertexAttribDivisor(10, 1);
									glEnableVertexAttribArray(11); //color
									glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE, stride, nullptr);
									glVertexAttribDivisor(11, 1);
									glEnableVertexAttribArray(12); //custom
									glVertexAttribPointer(12, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 2));
									glVertexAttribDivisor(12, 1);

									glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, split);
									storage->info.render._2d_draw_call_count++;
								}
							}

							glBindVertexArray(0);

							state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_INSTANCE_CUSTOM, false);
							state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_PARTICLES, false);
							state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_INSTANCING, false);
							state.using_texture_rect = true;
							_set_texture_rect_mode(false);

						} break;
						case Item::Command::TYPE_CIRCLE: {
							_set_texture_rect_mode(false);

							Item::CommandCircle *circle = static_cast<Item::CommandCircle *>(c);
							static const int numpoints = 32;
							Vector2 points[numpoints + 1];
							points[numpoints] = circle->pos;
							int indices[numpoints * 3];

							for (int j = 0; j < numpoints; j++) {
								points[j] = circle->pos + Vector2(Math::sin(j * Math_PI * 2.0 / numpoints), Math::cos(j * Math_PI * 2.0 / numpoints)) * circle->radius;
								indices[j * 3 + 0] = j;
								indices[j * 3 + 1] = (j + 1) % numpoints;
								indices[j * 3 + 2] = numpoints;
							}

							_bind_canvas_texture(RID(), RID());
							_draw_polygon(indices, numpoints * 3, numpoints + 1, points, nullptr, &circle->color, true, nullptr, nullptr);

							//_draw_polygon(numpoints*3,indices,points,NULL,&circle->color,RID(),true);
							//canvas_draw_circle(circle->indices.size(),circle->indices.ptr(),circle->points.ptr(),circle->uvs.ptr(),circle->colors.ptr(),circle->texture,circle->colors.size()==1);
						} break;
						case Item::Command::TYPE_TRANSFORM: {
							Item::CommandTransform *transform = static_cast<Item::CommandTransform *>(c);
							state.extra_matrix = transform->xform;
							state.canvas_shader.set_uniform(CanvasShaderGLES3::EXTRA_MATRIX, state.extra_matrix);

						} break;
						case Item::Command::TYPE_CLIP_IGNORE: {
							Item::CommandClipIgnore *ci = static_cast<Item::CommandClipIgnore *>(c);
							if (p_current_clip) {
								if (ci->ignore != r_reclip) {
									if (ci->ignore) {
										glDisable(GL_SCISSOR_TEST);
										r_reclip = true;
									} else {
										glEnable(GL_SCISSOR_TEST);
										//glScissor(viewport.x+current_clip->final_clip_rect.pos.x,viewport.y+ (viewport.height-(current_clip->final_clip_rect.pos.y+current_clip->final_clip_rect.size.height)),
										//current_clip->final_clip_rect.size.width,current_clip->final_clip_rect.size.height);
										int y = storage->frame.current_rt->height - (p_current_clip->final_clip_rect.position.y + p_current_clip->final_clip_rect.size.y);
										if (storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_VFLIP]) {
											y = p_current_clip->final_clip_rect.position.y;
										}

										glScissor(p_current_clip->final_clip_rect.position.x, y, p_current_clip->final_clip_rect.size.x, p_current_clip->final_clip_rect.size.y);

										r_reclip = false;
									}
								}
							}

						} break;

						default: {
							// FIXME: Proper error handling if relevant
							//print_line("other");
						} break;
					}
				}

			} // default
			break;
		}
	}
}

void RasterizerCanvasGLES3::render_joined_item(const BItemJoined &p_bij, RenderItemState &r_ris) {
	storage->info.render._2d_item_count++;

#if defined(TOOLS_ENABLED) && defined(DEBUG_ENABLED)
	if (bdata.diagnose_frame) {
		bdata.frame_string += _diagnose_make_item_joined_string(p_bij);
	}
#endif

	// all the joined items will share the same state with the first item
	Item *p_ci = bdata.item_refs[p_bij.first_item_ref].item;

	if (r_ris.prev_distance_field != p_ci->distance_field) {
		state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_DISTANCE_FIELD, p_ci->distance_field);
		r_ris.prev_distance_field = p_ci->distance_field;
		r_ris.rebind_shader = true;
	}

	if (r_ris.current_clip != p_ci->final_clip_owner) {
		r_ris.current_clip = p_ci->final_clip_owner;

		//setup clip
		if (r_ris.current_clip) {
			glEnable(GL_SCISSOR_TEST);
			int y = storage->frame.current_rt->height - (r_ris.current_clip->final_clip_rect.position.y + r_ris.current_clip->final_clip_rect.size.y);
			if (storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_VFLIP]) {
				y = r_ris.current_clip->final_clip_rect.position.y;
			}

			glScissor(r_ris.current_clip->final_clip_rect.position.x, y, r_ris.current_clip->final_clip_rect.size.x, r_ris.current_clip->final_clip_rect.size.y);

		} else {
			glDisable(GL_SCISSOR_TEST);
		}
	}

	if (p_ci->copy_back_buffer) {
		if (p_ci->copy_back_buffer->full) {
			_copy_texscreen(Rect2());
		} else {
			_copy_texscreen(p_ci->copy_back_buffer->rect);
		}
	}

	if (!bdata.settings_use_batching || !bdata.settings_use_software_skinning) {
		RasterizerStorageGLES3::Skeleton *skeleton = nullptr;

		//skeleton handling
		if (p_ci->skeleton.is_valid() && storage->skeleton_owner.owns(p_ci->skeleton)) {
			skeleton = storage->skeleton_owner.get(p_ci->skeleton);
			if (!skeleton->use_2d) {
				skeleton = nullptr;
			} else {
				state.skeleton_transform = r_ris.item_group_base_transform * skeleton->base_transform_2d;
				state.skeleton_transform_inverse = state.skeleton_transform.affine_inverse();
			}
		}

		bool use_skeleton = skeleton != nullptr;
		if (r_ris.prev_use_skeleton != use_skeleton) {
			r_ris.rebind_shader = true;
			state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_SKELETON, use_skeleton);
			r_ris.prev_use_skeleton = use_skeleton;
		}

		if (skeleton) {
			WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE0 + storage->config.max_texture_image_units - 4);
			glBindTexture(GL_TEXTURE_2D, skeleton->texture);
			state.using_skeleton = true;
		} else {
			state.using_skeleton = false;
		}

	} // if not using batching

	//begin rect
	Item *material_owner = p_ci->material_owner ? p_ci->material_owner : p_ci;

	RID material = material_owner->material;

	if (material != r_ris.canvas_last_material || r_ris.rebind_shader) {
		RasterizerStorageGLES3::Material *material_ptr = storage->material_owner.getornull(material);
		RasterizerStorageGLES3::Shader *shader_ptr = nullptr;

		if (material_ptr) {
			shader_ptr = material_ptr->shader;

			if (shader_ptr && shader_ptr->mode != VS::SHADER_CANVAS_ITEM) {
				shader_ptr = nullptr; //do not use non canvasitem shader
			}
		}

		if (shader_ptr) {
			if (shader_ptr->canvas_item.uses_screen_texture && !state.canvas_texscreen_used) {
				//copy if not copied before
				_copy_texscreen(Rect2());

				// blend mode will have been enabled so make sure we disable it again later on
				r_ris.last_blend_mode = r_ris.last_blend_mode != RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_DISABLED ? r_ris.last_blend_mode : -1;
			}

			if (shader_ptr != r_ris.shader_cache || r_ris.rebind_shader) {
				if (shader_ptr->canvas_item.uses_time) {
					VisualServerRaster::redraw_request(false);
				}

				state.canvas_shader.set_custom_shader(shader_ptr->custom_code_id);
				state.canvas_shader.bind();
			}

			if (material_ptr->ubo_id) {
				glBindBufferBase(GL_UNIFORM_BUFFER, 2, material_ptr->ubo_id);
			}

			int tc = material_ptr->textures.size();
			RID *textures = material_ptr->textures.ptrw();
			ShaderLanguage::ShaderNode::Uniform::Hint *texture_hints = shader_ptr->texture_hints.ptrw();

			for (int i = 0; i < tc; i++) {
				WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE2 + i);

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

		} else {
			state.canvas_shader.set_custom_shader(0);
			state.canvas_shader.bind();
		}

		r_ris.shader_cache = shader_ptr;

		r_ris.canvas_last_material = material;
		r_ris.rebind_shader = false;
	}

	int blend_mode = r_ris.shader_cache ? r_ris.shader_cache->canvas_item.blend_mode : RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MIX;
	if (blend_mode == RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_DISABLED && (!storage->frame.current_rt || !storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT])) {
		blend_mode = RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MIX;
	}
	bool unshaded = r_ris.shader_cache && (r_ris.shader_cache->canvas_item.light_mode == RasterizerStorageGLES3::Shader::CanvasItem::LIGHT_MODE_UNSHADED || (blend_mode != RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MIX && blend_mode != RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_PMALPHA));
	bool reclip = false;

	if (r_ris.last_blend_mode != blend_mode) {
		if (r_ris.last_blend_mode == RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_DISABLED) {
			// re-enable it
			glEnable(GL_BLEND);
		} else if (blend_mode == RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_DISABLED) {
			// disable it
			glDisable(GL_BLEND);
		}

		switch (blend_mode) {
			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_DISABLED: {
				// nothing to do here

			} break;
			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MIX: {
				glBlendEquation(GL_FUNC_ADD);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				} else {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
				}

			} break;
			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_ADD: {
				glBlendEquation(GL_FUNC_ADD);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA, GL_ONE);
				} else {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);
				}

			} break;
			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_SUB: {
				glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA, GL_ONE);
				} else {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);
				}
			} break;
			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MUL: {
				glBlendEquation(GL_FUNC_ADD);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_DST_ALPHA, GL_ZERO);
				} else {
					glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_ZERO, GL_ONE);
				}

			} break;
			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_PMALPHA: {
				glBlendEquation(GL_FUNC_ADD);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				} else {
					glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
				}

			} break;
		}

		r_ris.last_blend_mode = blend_mode;
	}

	//state.canvas_item_modulate = unshaded ? p_ci->final_modulate : Color(p_ci->final_modulate.r * r_ris.item_group_modulate.r, p_ci->final_modulate.g * r_ris.item_group_modulate.g, p_ci->final_modulate.b * r_ris.item_group_modulate.b, p_ci->final_modulate.a * r_ris.item_group_modulate.a);

	//	state.final_transform = p_ci->final_transform;
	//	state.extra_matrix = Transform2D();

	// using software transform?
	// (i.e. don't send the transform matrix, send identity, and either use baked verts,
	// or large fvf where the transform is done in the shader from transform stored in the fvf.)
	if (!p_bij.is_single_item()) {
		state.final_transform = Transform2D();
		// final_modulate will be baked per item ref so the final_modulate can be an identity color
		state.canvas_item_modulate = Color(1, 1, 1, 1);
	} else {
		state.final_transform = p_ci->final_transform;
		// could use the stored version of final_modulate in item ref? Test which is faster NYI
		state.canvas_item_modulate = unshaded ? p_ci->final_modulate : (p_ci->final_modulate * r_ris.item_group_modulate);
	}
	state.extra_matrix = Transform2D();

	if (state.using_skeleton) {
		state.canvas_shader.set_uniform(CanvasShaderGLES3::SKELETON_TRANSFORM, state.skeleton_transform);
		state.canvas_shader.set_uniform(CanvasShaderGLES3::SKELETON_TRANSFORM_INVERSE, state.skeleton_transform_inverse);
	}

	state.canvas_shader.set_uniform(CanvasShaderGLES3::FINAL_MODULATE, state.canvas_item_modulate);
	state.canvas_shader.set_uniform(CanvasShaderGLES3::MODELVIEW_MATRIX, state.final_transform);
	state.canvas_shader.set_uniform(CanvasShaderGLES3::EXTRA_MATRIX, state.extra_matrix);
	if (storage->frame.current_rt) {
		state.canvas_shader.set_uniform(CanvasShaderGLES3::SCREEN_PIXEL_SIZE, Vector2(1.0 / storage->frame.current_rt->width, 1.0 / storage->frame.current_rt->height));
	} else {
		state.canvas_shader.set_uniform(CanvasShaderGLES3::SCREEN_PIXEL_SIZE, Vector2(1.0, 1.0));
	}
	if (unshaded || (state.canvas_item_modulate.a > 0.001 && (!r_ris.shader_cache || r_ris.shader_cache->canvas_item.light_mode != RasterizerStorageGLES3::Shader::CanvasItem::LIGHT_MODE_LIGHT_ONLY) && !p_ci->light_masked)) {
		RasterizerStorageGLES3::Material *material_ptr = nullptr;
		render_joined_item_commands(p_bij, nullptr, reclip, material_ptr, false, r_ris);
	}

	if ((blend_mode == RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MIX || blend_mode == RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_PMALPHA) && r_ris.item_group_light && !unshaded) {
		Light *light = r_ris.item_group_light;
		bool light_used = false;
		VS::CanvasLightMode mode = VS::CANVAS_LIGHT_MODE_ADD;

		// we leave this set to 1, 1, 1, 1 if using software because the colors are baked into the vertices
		if (p_bij.is_single_item()) {
			state.canvas_item_modulate = p_ci->final_modulate; // remove the canvas modulate
		}

		while (light) {
			// use the bounding rect of the joined items, NOT only the bounding rect of the first item.
			// note this is a cost of batching, the light culling will be less effective

			// note that the r_ris.item_group_z will be out of date because we are using deferred rendering till canvas_render_items_end()
			// so we have to test z against the stored value in the joined item
			if (p_ci->light_mask & light->item_mask && p_bij.z_index >= light->z_min && p_bij.z_index <= light->z_max && p_bij.bounding_rect.intersects_transformed(light->xform_cache, light->rect_cache)) {
				//intersects this light

				if (!light_used || mode != light->mode) {
					mode = light->mode;

					switch (mode) {
						case VS::CANVAS_LIGHT_MODE_ADD: {
							glBlendEquation(GL_FUNC_ADD);
							glBlendFunc(GL_SRC_ALPHA, GL_ONE);

						} break;
						case VS::CANVAS_LIGHT_MODE_SUB: {
							glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
							glBlendFunc(GL_SRC_ALPHA, GL_ONE);
						} break;
						case VS::CANVAS_LIGHT_MODE_MIX:
						case VS::CANVAS_LIGHT_MODE_MASK: {
							glBlendEquation(GL_FUNC_ADD);
							glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

						} break;
					}
				}

				if (!light_used) {
					state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_LIGHTING, true);
					light_used = true;
				}

				bool has_shadow = light->shadow_buffer.is_valid() && p_ci->light_mask & light->item_shadow_mask;

				state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_SHADOWS, has_shadow);
				if (has_shadow) {
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_USE_GRADIENT, light->shadow_gradient_length > 0);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_NEAREST, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_NONE);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF3, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_PCF3);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF5, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_PCF5);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF7, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_PCF7);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF9, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_PCF9);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF13, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_PCF13);
				}

				bool light_rebind = state.canvas_shader.bind();

				if (light_rebind) {
					state.canvas_shader.set_uniform(CanvasShaderGLES3::FINAL_MODULATE, state.canvas_item_modulate);
					state.canvas_shader.set_uniform(CanvasShaderGLES3::MODELVIEW_MATRIX, state.final_transform);
					state.canvas_shader.set_uniform(CanvasShaderGLES3::EXTRA_MATRIX, Transform2D());
					if (storage->frame.current_rt) {
						state.canvas_shader.set_uniform(CanvasShaderGLES3::SCREEN_PIXEL_SIZE, Vector2(1.0 / storage->frame.current_rt->width, 1.0 / storage->frame.current_rt->height));
					} else {
						state.canvas_shader.set_uniform(CanvasShaderGLES3::SCREEN_PIXEL_SIZE, Vector2(1.0, 1.0));
					}
					if (state.using_skeleton) {
						state.canvas_shader.set_uniform(CanvasShaderGLES3::SKELETON_TRANSFORM, state.skeleton_transform);
						state.canvas_shader.set_uniform(CanvasShaderGLES3::SKELETON_TRANSFORM_INVERSE, state.skeleton_transform_inverse);
					}
				}

				glBindBufferBase(GL_UNIFORM_BUFFER, 1, static_cast<LightInternal *>(light_internal_owner.get(light->light_internal))->ubo);

				if (has_shadow) {
					RasterizerStorageGLES3::CanvasLightShadow *cls = storage->canvas_light_shadow_owner.get(light->shadow_buffer);
					WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE0 + storage->config.max_texture_image_units - 2);
					glBindTexture(GL_TEXTURE_2D, cls->distance);

					/*canvas_shader.set_uniform(CanvasShaderGLES3::SHADOW_MATRIX,light->shadow_matrix_cache);
					canvas_shader.set_uniform(CanvasShaderGLES3::SHADOW_ESM_MULTIPLIER,light->shadow_esm_mult);
					canvas_shader.set_uniform(CanvasShaderGLES3::LIGHT_SHADOW_COLOR,light->shadow_color);*/
				}

				WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE0 + storage->config.max_texture_image_units - 1);
				RasterizerStorageGLES3::Texture *t = storage->texture_owner.getornull(light->texture);
				if (!t) {
					glBindTexture(GL_TEXTURE_2D, storage->resources.white_tex);
				} else {
					t = t->get_ptr();

					glBindTexture(t->target, t->tex_id);
				}

				WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE0);

				// redraw using light.
				// if there is no clip item, we can consider scissoring to the intersection area between the light and the item
				// this can greatly reduce fill rate ..
				// at the cost of glScissor commands, so is optional
				if (!bdata.settings_scissor_lights || r_ris.current_clip) {
					render_joined_item_commands(p_bij, nullptr, reclip, nullptr, true, r_ris);
				} else {
					bool scissor = _light_scissor_begin(p_bij.bounding_rect, light->xform_cache, light->rect_cache);
					render_joined_item_commands(p_bij, nullptr, reclip, nullptr, true, r_ris);
					if (scissor) {
						glDisable(GL_SCISSOR_TEST);
					}
				}
			}

			light = light->next_ptr;
		}

		if (light_used) {
			state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_LIGHTING, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_SHADOWS, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_NEAREST, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF3, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF5, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF7, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF9, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF13, false);

			state.canvas_shader.bind();

			r_ris.last_blend_mode = -1;

			/*
			//this is set again, so it should not be needed anyway?
			state.canvas_item_modulate = unshaded ? ci->final_modulate : Color(
						ci->final_modulate.r * p_modulate.r,
						ci->final_modulate.g * p_modulate.g,
						ci->final_modulate.b * p_modulate.b,
						ci->final_modulate.a * p_modulate.a );


			state.canvas_shader.set_uniform(CanvasShaderGLES3::MODELVIEW_MATRIX,state.final_transform);
			state.canvas_shader.set_uniform(CanvasShaderGLES3::EXTRA_MATRIX,Transform2D());
			state.canvas_shader.set_uniform(CanvasShaderGLES3::FINAL_MODULATE,state.canvas_item_modulate);

			glBlendEquation(GL_FUNC_ADD);

			if (storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
				glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			} else {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}

			//@TODO RESET canvas_blend_mode
			*/
		}
	}

	if (reclip) {
		glEnable(GL_SCISSOR_TEST);
		int y = storage->frame.current_rt->height - (r_ris.current_clip->final_clip_rect.position.y + r_ris.current_clip->final_clip_rect.size.y);
		if (storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_VFLIP]) {
			y = r_ris.current_clip->final_clip_rect.position.y;
		}
		glScissor(r_ris.current_clip->final_clip_rect.position.x, y, r_ris.current_clip->final_clip_rect.size.width, r_ris.current_clip->final_clip_rect.size.height);
	}
}

// This function is a dry run of the state changes when drawing the item.
// It should duplicate the logic in _canvas_render_item,
// to decide whether items are similar enough to join
// i.e. no state differences between the 2 items.
bool RasterizerCanvasGLES3::try_join_item(Item *p_ci, RenderItemState &r_ris, bool &r_batch_break) {
	// if we set max join items to zero we can effectively prevent any joining, so
	// none of the other logic needs to run. Good for testing regression bugs, and
	// could conceivably be faster in some games.
	if (!bdata.settings_max_join_item_commands) {
		return false;
	}

	// if there are any state changes we change join to false
	// we also set r_batch_break to true if we don't want this item joined to the next
	// (e.g. an item that must not be joined at all)
	r_batch_break = false;
	bool join = true;

	// light_masked objects we just don't currently support for joining
	// (this could possibly be improved at a later date)
	if (p_ci->light_masked) {
		join = false;
		r_batch_break = true;
	}

	// we will now allow joining even if final modulate is different
	// we will instead bake the final modulate into the vertex colors
	//	if (p_ci->final_modulate != r_ris.final_modulate) {
	//		join = false;
	//		r_ris.final_modulate = p_ci->final_modulate;
	//	}

	if (r_ris.current_clip != p_ci->final_clip_owner) {
		r_ris.current_clip = p_ci->final_clip_owner;
		join = false;
	}

	// TODO: copy back buffer

	if (p_ci->copy_back_buffer) {
		join = false;
	}

	RasterizerStorageGLES3::Skeleton *skeleton = nullptr;

	{
		//skeleton handling
		if (p_ci->skeleton.is_valid() && storage->skeleton_owner.owns(p_ci->skeleton)) {
			skeleton = storage->skeleton_owner.get(p_ci->skeleton);
			if (!skeleton->use_2d) {
				skeleton = nullptr;
			}
		}

		bool skeleton_prevent_join = false;

		bool use_skeleton = skeleton != nullptr;
		if (r_ris.prev_use_skeleton != use_skeleton) {
			if (!bdata.settings_use_software_skinning) {
				r_ris.rebind_shader = true;
			}

			r_ris.prev_use_skeleton = use_skeleton;
			//			join = false;
			skeleton_prevent_join = true;
		}

		if (skeleton) {
			//			join = false;
			skeleton_prevent_join = true;
			state.using_skeleton = true;
		} else {
			state.using_skeleton = false;
		}

		if (skeleton_prevent_join) {
			if (!bdata.settings_use_software_skinning) {
				join = false;
			}
		}
	}

	Item *material_owner = p_ci->material_owner ? p_ci->material_owner : p_ci;

	RID material = material_owner->material;
	RasterizerStorageGLES3::Material *material_ptr = storage->material_owner.getornull(material);

	if (material != r_ris.canvas_last_material || r_ris.rebind_shader) {
		join = false;
		RasterizerStorageGLES3::Shader *shader_ptr = nullptr;

		if (material_ptr) {
			shader_ptr = material_ptr->shader;

			// special case, if the user has made an error in the shader code
			if (shader_ptr && !shader_ptr->valid) {
				join = false;
				r_batch_break = true;
			}

			if (shader_ptr && shader_ptr->mode != VS::SHADER_CANVAS_ITEM) {
				shader_ptr = nullptr; // not a canvas item shader, don't use.
			}
		}

		if (shader_ptr) {
			if (shader_ptr->canvas_item.uses_screen_texture) {
				if (!state.canvas_texscreen_used) {
					join = false;
				}
			}
		}

		r_ris.shader_cache = shader_ptr;

		r_ris.canvas_last_material = material;

		r_ris.rebind_shader = false;
	}

	int blend_mode = r_ris.shader_cache ? r_ris.shader_cache->canvas_item.blend_mode : RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MIX;
	bool unshaded = r_ris.shader_cache && (r_ris.shader_cache->canvas_item.light_mode == RasterizerStorageGLES3::Shader::CanvasItem::LIGHT_MODE_UNSHADED || (blend_mode != RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MIX && blend_mode != RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_PMALPHA));
	bool reclip = false;

	// we are precalculating the final_modulate ahead of time because we need this for baking of final modulate into vertex colors
	// (only in software transform mode)
	// This maybe inefficient storing it...
	r_ris.final_modulate = unshaded ? p_ci->final_modulate : (p_ci->final_modulate * r_ris.item_group_modulate);

	if (r_ris.last_blend_mode != blend_mode) {
		join = false;
		r_ris.last_blend_mode = blend_mode;
	}

	// does the shader contain BUILTINs which should break the batching?
	bdata.joined_item_batch_flags = 0;
	if (r_ris.shader_cache) {
		unsigned int and_flags = r_ris.shader_cache->canvas_item.batch_flags & (RasterizerStorageCommon::PREVENT_COLOR_BAKING | RasterizerStorageCommon::PREVENT_VERTEX_BAKING | RasterizerStorageCommon::PREVENT_ITEM_JOINING);
		if (and_flags) {
			// special case for preventing item joining altogether
			if (and_flags & RasterizerStorageCommon::PREVENT_ITEM_JOINING) {
				join = false;
				//r_batch_break = true; // don't think we need a batch break

				// save the flags so that they don't need to be recalculated in the 2nd pass
				bdata.joined_item_batch_flags |= r_ris.shader_cache->canvas_item.batch_flags;
			} else {
				bool use_larger_fvfs = true;

				if (and_flags == RasterizerStorageCommon::PREVENT_COLOR_BAKING) {
					// in some circumstances, if the modulate is identity, we still allow baking because reading modulate / color
					// will still be okay to do in the shader with no ill effects
					if (r_ris.final_modulate == Color(1, 1, 1, 1)) {
						use_larger_fvfs = false;
					}
				}

				// new .. always use large FVF
				if (use_larger_fvfs) {
					if (and_flags == RasterizerStorageCommon::PREVENT_COLOR_BAKING) {
						bdata.joined_item_batch_flags |= RasterizerStorageCommon::USE_MODULATE_FVF;
					} else {
						// we need to save on the joined item that it should use large fvf.
						// This info will then be used in filling and rendering
						bdata.joined_item_batch_flags |= RasterizerStorageCommon::USE_LARGE_FVF;
					}

					bdata.joined_item_batch_flags |= r_ris.shader_cache->canvas_item.batch_flags;
				}
			} // if not prevent item joining
		}
	}

	if ((blend_mode == RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MIX || blend_mode == RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_PMALPHA) && r_ris.item_group_light && !unshaded) {
		// we cannot join lit items easily.
		// it is possible, but not if they overlap, because
		// a + light_blend + b + light_blend IS NOT THE SAME AS
		// a + b + light_blend

		bool light_allow_join = true;

		// this is a quick getout if we have turned off light joining
		if ((bdata.settings_light_max_join_items == 0) || r_ris.light_region.too_many_lights) {
			light_allow_join = false;
		} else {
			// do light joining...

			// first calculate the light bitfield
			uint64_t light_bitfield = 0;
			uint64_t shadow_bitfield = 0;
			Light *light = r_ris.item_group_light;

			int light_count = -1;
			while (light) {
				light_count++;
				uint64_t light_bit = 1ULL << light_count;

				// note that as a cost of batching, the light culling will be less effective
				if (p_ci->light_mask & light->item_mask && r_ris.item_group_z >= light->z_min && r_ris.item_group_z <= light->z_max) {
					// Note that with the above test, it is possible to also include a bound check.
					// Tests so far have indicated better performance without it, but there may be reason to change this at a later stage,
					// so I leave the line here for reference:
					// && p_ci->global_rect_cache.intersects_transformed(light->xform_cache, light->rect_cache)) {

					light_bitfield |= light_bit;

					bool has_shadow = light->shadow_buffer.is_valid() && p_ci->light_mask & light->item_shadow_mask;

					if (has_shadow) {
						shadow_bitfield |= light_bit;
					}
				}

				light = light->next_ptr;
			}

			// now compare to previous
			if ((r_ris.light_region.light_bitfield != light_bitfield) || (r_ris.light_region.shadow_bitfield != shadow_bitfield)) {
				light_allow_join = false;

				r_ris.light_region.light_bitfield = light_bitfield;
				r_ris.light_region.shadow_bitfield = shadow_bitfield;
			} else {
				// only do these checks if necessary
				if (join && (!r_batch_break)) {
					// we still can't join, even if the lights are exactly the same, if there is overlap between the previous and this item
					if (r_ris.joined_item && light_bitfield) {
						if ((int)r_ris.joined_item->num_item_refs <= bdata.settings_light_max_join_items) {
							for (uint32_t r = 0; r < r_ris.joined_item->num_item_refs; r++) {
								Item *pRefItem = bdata.item_refs[r_ris.joined_item->first_item_ref + r].item;
								if (p_ci->global_rect_cache.intersects(pRefItem->global_rect_cache)) {
									light_allow_join = false;
									break;
								}
							}

#ifdef DEBUG_ENABLED
							if (light_allow_join) {
								bdata.stats_light_items_joined++;
							}
#endif

						} // if below max join items
						else {
							// just don't allow joining if above overlap check max items
							light_allow_join = false;
						}
					}

				} // if not batch broken already (no point in doing expensive overlap tests if not needed)
			} // if bitfields don't match
		} // if do light joining

		if (!light_allow_join) {
			// can't join
			join = false;
			// we also dont want to allow joining this item with the next item, because the next item could have no lights!
			r_batch_break = true;
		}

	} else {
		// if the last item had lights, we should not join it to this one (which has no lights)
		if (r_ris.light_region.light_bitfield || r_ris.light_region.shadow_bitfield) {
			join = false;

			// setting these to zero ensures that any following item with lights will, by definition,
			// be affected by a different set of lights, and thus prevent a join
			r_ris.light_region.light_bitfield = 0;
			r_ris.light_region.shadow_bitfield = 0;
		}
	}

	if (reclip) {
		join = false;
	}

	if (r_ris.prev_distance_field != p_ci->distance_field) {
		r_ris.prev_distance_field = p_ci->distance_field;
		join = false;
		r_batch_break = true;
	}

	// non rects will break the batching anyway, we don't want to record item changes, detect this
	if (!r_batch_break && _detect_item_batch_break(r_ris, p_ci, r_batch_break)) {
		join = false;
		r_batch_break = true;
	}

	return join;
}

void RasterizerCanvasGLES3::canvas_render_items_implementation(Item *p_item_list, int p_z, const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform) {
	// parameters are easier to pass around in a structure
	RenderItemState ris;
	ris.item_group_z = p_z;
	ris.item_group_modulate = p_modulate;
	ris.item_group_light = p_light;
	ris.item_group_base_transform = p_base_transform;
	ris.prev_distance_field = false;

	glBindBuffer(GL_UNIFORM_BUFFER, state.canvas_item_ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(CanvasItemUBO), &state.canvas_item_ubo_data, _buffer_upload_usage_flag);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	state.current_tex = RID();
	state.current_tex_ptr = nullptr;
	state.current_normal = RID();
	WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, storage->resources.white_tex);

	//	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_SKELETON, false);
	//	state.current_tex = RID();
	//	state.current_tex_ptr = NULL;
	//	state.current_normal = RID();
	//	state.canvas_texscreen_used = false;
	//	WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE0);
	//	glBindTexture(GL_TEXTURE_2D, storage->resources.white_tex);

	if (bdata.settings_use_batching) {
		for (int j = 0; j < bdata.items_joined.size(); j++) {
			render_joined_item(bdata.items_joined[j], ris);
		}
	} else {
		while (p_item_list) {
			Item *ci = p_item_list;
			_legacy_canvas_render_item(ci, ris);
			p_item_list = p_item_list->next;
		}
	}

	if (ris.current_clip) {
		glDisable(GL_SCISSOR_TEST);
	}

	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_SKELETON, false);
}

void RasterizerCanvasGLES3::_batch_upload_buffers() {
	batch_gl_data.next();

	// noop?
	if (!bdata.vertices.size()) {
		return;
	}

	const GLuint gl_vertex_buffer = batch_gl_data.current_vertex_buffer();

	glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buffer);

	// usage flag is a project setting
	GLenum buffer_usage_flag = GL_DYNAMIC_DRAW;
	if (bdata.buffer_mode_batch_upload_flag_stream) {
		buffer_usage_flag = GL_STREAM_DRAW;
	}

	// orphan the old (for now)
	if (bdata.buffer_mode_batch_upload_send_null) {
		glBufferData(GL_ARRAY_BUFFER, 0, nullptr, buffer_usage_flag); // GL_DYNAMIC_DRAW);
	}

	switch (bdata.fvf) {
		case RasterizerStorageCommon::FVF_UNBATCHED: // should not happen
			break;
		case RasterizerStorageCommon::FVF_REGULAR: // no change
			glBufferData(GL_ARRAY_BUFFER, sizeof(BatchVertex) * bdata.vertices.size(), bdata.vertices.get_data(), buffer_usage_flag);
			break;
		case RasterizerStorageCommon::FVF_COLOR:
			glBufferData(GL_ARRAY_BUFFER, sizeof(BatchVertexColored) * bdata.unit_vertices.size(), bdata.unit_vertices.get_unit(0), buffer_usage_flag);
			break;
		case RasterizerStorageCommon::FVF_LIGHT_ANGLE:
			glBufferData(GL_ARRAY_BUFFER, sizeof(BatchVertexLightAngled) * bdata.unit_vertices.size(), bdata.unit_vertices.get_unit(0), buffer_usage_flag);
			break;
		case RasterizerStorageCommon::FVF_MODULATED:
			glBufferData(GL_ARRAY_BUFFER, sizeof(BatchVertexModulated) * bdata.unit_vertices.size(), bdata.unit_vertices.get_unit(0), buffer_usage_flag);
			break;
		case RasterizerStorageCommon::FVF_LARGE:
			glBufferData(GL_ARRAY_BUFFER, sizeof(BatchVertexLarge) * bdata.unit_vertices.size(), bdata.unit_vertices.get_unit(0), buffer_usage_flag);
			break;
	}

	// might not be necessary
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RasterizerCanvasGLES3::_batch_render_lines(const Batch &p_batch, RasterizerStorageGLES3::Material *p_material, bool p_anti_alias) {
	_set_texture_rect_mode(false);

	_bind_canvas_texture(RID(), RID());

	glBindVertexArray(batch_gl_data.current_vertex_array()[0]);

	glDisableVertexAttribArray(VS::ARRAY_COLOR);
	glVertexAttrib4fv(VS::ARRAY_COLOR, (float *)&p_batch.color);

	int64_t offset = p_batch.first_vert; // 6 inds per quad at 2 bytes each

	int num_elements = p_batch.num_commands * 2;

#ifdef GLES_OVER_GL
	if (p_anti_alias) {
		glEnable(GL_LINE_SMOOTH);
	}
#endif

	glDrawArrays(GL_LINES, offset, num_elements);

	storage->info.render._2d_draw_call_count++;

	glBindVertexArray(0);

#ifdef GLES_OVER_GL
	if (p_anti_alias) {
		glDisable(GL_LINE_SMOOTH);
	}
#endif
}

void RasterizerCanvasGLES3::_batch_render_prepare() {
	//const bool &colored_verts = bdata.use_colored_vertices;
	const bool &use_light_angles = bdata.use_light_angles;
	const bool &use_modulate = bdata.use_modulate;
	const bool &use_large_verts = bdata.use_large_verts;

	_set_texture_rect_mode(false, false, use_light_angles, use_modulate, use_large_verts);

	//	state.canvas_shader.set_uniform(CanvasShaderGLES3::CLIP_RECT_UV, p_rect->flags & CANVAS_RECT_CLIP_UV);
	state.canvas_shader.set_uniform(CanvasShaderGLES3::CLIP_RECT_UV, false);

	switch (bdata.fvf) {
		case RasterizerStorageCommon::FVF_UNBATCHED: // should not happen
			return;
			break;
		case RasterizerStorageCommon::FVF_REGULAR: // no change
			glBindVertexArray(batch_gl_data.current_vertex_array()[0]);
			break;
		case RasterizerStorageCommon::FVF_COLOR:
			glBindVertexArray(batch_gl_data.current_vertex_array()[1]);
			break;
		case RasterizerStorageCommon::FVF_LIGHT_ANGLE:
			glBindVertexArray(batch_gl_data.current_vertex_array()[2]);
			break;
		case RasterizerStorageCommon::FVF_MODULATED:
			glBindVertexArray(batch_gl_data.current_vertex_array()[3]);
			break;
		case RasterizerStorageCommon::FVF_LARGE:
			glBindVertexArray(batch_gl_data.current_vertex_array()[4]);
			break;
	}
}

void RasterizerCanvasGLES3::_batch_render_generic(const Batch &p_batch, RasterizerStorageGLES3::Material *p_material) {
	ERR_FAIL_COND(p_batch.num_commands <= 0);

	const bool &use_light_angles = bdata.use_light_angles;
	const bool &use_modulate = bdata.use_modulate;
	const bool &use_large_verts = bdata.use_large_verts;
	const bool &colored_verts = bdata.use_colored_vertices | use_light_angles | use_modulate | use_large_verts;

	_set_texture_rect_mode(false, false, use_light_angles, use_modulate, use_large_verts);

	//	state.canvas_shader.set_uniform(CanvasShaderGLES3::CLIP_RECT_UV, p_rect->flags & CANVAS_RECT_CLIP_UV);
	state.canvas_shader.set_uniform(CanvasShaderGLES3::CLIP_RECT_UV, false);

	switch (bdata.fvf) {
		case RasterizerStorageCommon::FVF_UNBATCHED: // should not happen
			return;
			break;
		case RasterizerStorageCommon::FVF_REGULAR: // no change
			glBindVertexArray(batch_gl_data.current_vertex_array()[0]);
			break;
		case RasterizerStorageCommon::FVF_COLOR:
			glBindVertexArray(batch_gl_data.current_vertex_array()[1]);
			break;
		case RasterizerStorageCommon::FVF_LIGHT_ANGLE:
			glBindVertexArray(batch_gl_data.current_vertex_array()[2]);
			break;
		case RasterizerStorageCommon::FVF_MODULATED:
			glBindVertexArray(batch_gl_data.current_vertex_array()[3]);
			break;
		case RasterizerStorageCommon::FVF_LARGE:
			glBindVertexArray(batch_gl_data.current_vertex_array()[4]);
			break;
	}

	// batch tex
	const BatchTex &tex = bdata.batch_textures[p_batch.batch_texture_id];

	_bind_canvas_texture(tex.RID_texture, tex.RID_normal);

	if (!colored_verts) {
		// may not need this disable
		glDisableVertexAttribArray(VS::ARRAY_COLOR);
		glVertexAttrib4fv(VS::ARRAY_COLOR, p_batch.color.get_data());
	}

	// We only want to set the GL wrapping mode if the texture is not already tiled (i.e. set in Import).
	// This  is an optimization left over from the legacy renderer.
	// If we DID set tiling in the API, and reverted to clamped, then the next draw using this texture
	// may use clamped mode incorrectly.
	bool tex_is_already_tiled = tex.flags & VS::TEXTURE_FLAG_REPEAT;

	switch (tex.tile_mode) {
		case BatchTex::TILE_NORMAL: {
			// if the texture is imported as tiled, no need to set GL state, as it will already be bound with repeat
			if (!tex_is_already_tiled) {
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			}
		} break;
		default: {
		} break;
	}

	// we need to convert explicitly from pod Vec2 to Vector2 ...
	// could use a cast but this might be unsafe in future
	Vector2 tps;
	tex.tex_pixel_size.to(tps);
	state.canvas_shader.set_uniform(CanvasShaderGLES3::COLOR_TEXPIXEL_SIZE, tps);

	switch (p_batch.type) {
		default: {
			// prevent compiler warning
		} break;
		case RasterizerStorageCommon::BT_RECT: {
			int64_t offset = p_batch.first_vert * 3; // 6 inds per quad at 2 bytes each

			int num_elements = p_batch.num_commands * 6;
			glDrawElements(GL_TRIANGLES, num_elements, GL_UNSIGNED_SHORT, (void *)offset);
		} break;
		case RasterizerStorageCommon::BT_POLY: {
			int64_t offset = p_batch.first_vert;

			int num_elements = p_batch.num_commands;
			glDrawArrays(GL_TRIANGLES, offset, num_elements);
		} break;
	}

	storage->info.render._2d_draw_call_count++;

	glBindVertexArray(0);

	switch (tex.tile_mode) {
		case BatchTex::TILE_NORMAL: {
			// if the texture is imported as tiled, no need to revert GL state
			if (!tex_is_already_tiled) {
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}
		} break;
		default: {
		} break;
	}

	/*
	// may not be necessary .. state change optimization still TODO
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	*/
}

void RasterizerCanvasGLES3::initialize_buffer(GLuint vertex_buffer, GLuint index_buffer, GLuint vertex_array[5]) {
	// just reserve some space (may not be needed as we are orphaning, but hey ho)
	if (bdata.vertex_buffer_size_bytes) {
		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
		glBufferData(GL_ARRAY_BUFFER, bdata.vertex_buffer_size_bytes, nullptr, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// pre fill index buffer, the indices never need to change so can be static
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);

		Vector<uint16_t> indices;
		indices.resize(bdata.index_buffer_size_units);

		for (unsigned int q = 0; q < bdata.max_quads; q++) {
			int i_pos = q * 6; //  6 inds per quad
			int q_pos = q * 4; // 4 verts per quad
			indices.set(i_pos, q_pos);
			indices.set(i_pos + 1, q_pos + 1);
			indices.set(i_pos + 2, q_pos + 2);
			indices.set(i_pos + 3, q_pos);
			indices.set(i_pos + 4, q_pos + 2);
			indices.set(i_pos + 5, q_pos + 3);

			// we can only use 16 bit indices in GLES2!
#ifdef DEBUG_ENABLED
			CRASH_COND((q_pos + 3) > 65535);
#endif
		}

		glBufferData(GL_ELEMENT_ARRAY_BUFFER, bdata.index_buffer_size_bytes, &indices[0], GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	} // only if there is a vertex buffer (batching is on)

	// vertex array objects
	for (int vao = 0; vao < 5; vao++) {
		int sizeof_vert;
		switch (vao) {
			case 0:
				sizeof_vert = sizeof(BatchVertex);
				break;
			case 1:
				sizeof_vert = sizeof(BatchVertexColored);
				break;
			case 2:
				sizeof_vert = sizeof(BatchVertexLightAngled);
				break;
			case 3:
				sizeof_vert = sizeof(BatchVertexModulated);
				break;
			case 4:
				sizeof_vert = sizeof(BatchVertexLarge);
				break;
		}

		glBindVertexArray(vertex_array[vao]);
		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);

		uint64_t pointer = 0;
		glEnableVertexAttribArray(VS::ARRAY_VERTEX);
		glVertexAttribPointer(VS::ARRAY_VERTEX, 2, GL_FLOAT, GL_FALSE, sizeof_vert, (const void *)pointer);

		// always send UVs, even within a texture specified because a shader can still use UVs
		glEnableVertexAttribArray(VS::ARRAY_TEX_UV);
		glVertexAttribPointer(VS::ARRAY_TEX_UV, 2, GL_FLOAT, GL_FALSE, sizeof_vert, CAST_INT_TO_UCHAR_PTR(pointer + (2 * 4)));

		// optional attributes
		bool a_color = false;
		bool a_light_angle = false;
		bool a_modulate = false;
		bool a_large = false;

		switch (vao) {
			case 0:
				break;
			case 1: {
				a_color = true;
			} break;
			case 2: {
				a_color = true;
				a_light_angle = true;
			} break;
			case 3: {
				a_color = true;
				a_light_angle = true;
				a_modulate = true;
			} break;
			case 4: {
				a_color = true;
				a_light_angle = true;
				a_modulate = true;
				a_large = true;
			} break;
		}

		if (a_color) {
			glEnableVertexAttribArray(VS::ARRAY_COLOR);
			glVertexAttribPointer(VS::ARRAY_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof_vert, CAST_INT_TO_UCHAR_PTR(pointer + (4 * 4)));
		}
		if (a_light_angle) {
			glEnableVertexAttribArray(VS::ARRAY_TANGENT);
			glVertexAttribPointer(VS::ARRAY_TANGENT, 1, GL_FLOAT, GL_FALSE, sizeof_vert, CAST_INT_TO_UCHAR_PTR(pointer + (8 * 4)));
		}
		if (a_modulate) {
			glEnableVertexAttribArray(VS::ARRAY_TEX_UV2);
			glVertexAttribPointer(VS::ARRAY_TEX_UV2, 4, GL_FLOAT, GL_FALSE, sizeof_vert, CAST_INT_TO_UCHAR_PTR(pointer + (9 * 4)));
		}
		if (a_large) {
			glEnableVertexAttribArray(VS::ARRAY_BONES);
			glVertexAttribPointer(VS::ARRAY_BONES, 2, GL_FLOAT, GL_FALSE, sizeof_vert, CAST_INT_TO_UCHAR_PTR(pointer + (13 * 4)));
			glEnableVertexAttribArray(VS::ARRAY_WEIGHTS);
			glVertexAttribPointer(VS::ARRAY_WEIGHTS, 4, GL_FLOAT, GL_FALSE, sizeof_vert, CAST_INT_TO_UCHAR_PTR(pointer + (15 * 4)));
		}

		glBindVertexArray(0);
	} // for vao
}

void RasterizerCanvasGLES3::initialize() {
	RasterizerGLES3::gl_check_errors();
	RasterizerCanvasBaseGLES3::initialize();

	batch_initialize();

	batch_gl_data.resize(1);

	// deal with ninepatch mode option
	if (bdata.settings_ninepatch_mode == 1) {
		state.canvas_shader.add_custom_define("#define USE_NINEPATCH_SCALING\n");
	}

	RasterizerGLES3::gl_check_errors();
}

RasterizerCanvasGLES3::RasterizerCanvasGLES3() :
		batch_gl_data(*this) {
	batch_constructor();
}
