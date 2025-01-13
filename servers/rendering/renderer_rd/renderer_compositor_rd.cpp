/**************************************************************************/
/*  renderer_compositor_rd.cpp                                            */
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

#include "renderer_compositor_rd.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"

void RendererCompositorRD::blit_render_targets_to_screen(DisplayServer::WindowID p_screen, const BlitToScreen *p_render_targets, int p_amount) {
	Error err = RD::get_singleton()->screen_prepare_for_drawing(p_screen);
	if (err != OK) {
		// Window is minimized and does not have valid swapchain, skip drawing without printing errors.
		return;
	}

	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin_for_screen(p_screen);
	ERR_FAIL_COND(draw_list == RD::INVALID_ID);

	for (int i = 0; i < p_amount; i++) {
		RID rd_texture = texture_storage->render_target_get_rd_texture(p_render_targets[i].render_target);
		ERR_CONTINUE(rd_texture.is_null());

		if (!render_target_descriptors.has(rd_texture) || !RD::get_singleton()->uniform_set_is_valid(render_target_descriptors[rd_texture])) {
			Vector<RD::Uniform> uniforms;
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
			u.binding = 0;
			u.append_id(blit.sampler);
			u.append_id(rd_texture);
			uniforms.push_back(u);
			RID uniform_set = RD::get_singleton()->uniform_set_create(uniforms, blit.shader.version_get_shader(blit.shader_version, BLIT_MODE_NORMAL), 0);

			render_target_descriptors[rd_texture] = uniform_set;
		}

		Size2 screen_size(RD::get_singleton()->screen_get_width(p_screen), RD::get_singleton()->screen_get_height(p_screen));
		BlitMode mode = p_render_targets[i].lens_distortion.apply ? BLIT_MODE_LENS : (p_render_targets[i].multi_view.use_layer ? BLIT_MODE_USE_LAYER : BLIT_MODE_NORMAL);
		RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, blit.pipelines[mode]);
		RD::get_singleton()->draw_list_bind_index_array(draw_list, blit.array);
		RD::get_singleton()->draw_list_bind_uniform_set(draw_list, render_target_descriptors[rd_texture], 0);

		// We need to invert the phone rotation.
		const int screen_rotation_degrees = -RD::get_singleton()->screen_get_pre_rotation_degrees(p_screen);
		float screen_rotation = Math::deg_to_rad((float)screen_rotation_degrees);

		blit.push_constant.rotation_cos = Math::cos(screen_rotation);
		blit.push_constant.rotation_sin = Math::sin(screen_rotation);
		// Swap width and height when the orientation is not the native one.
		if (screen_rotation_degrees % 180 != 0) {
			SWAP(screen_size.width, screen_size.height);
		}
		blit.push_constant.src_rect[0] = p_render_targets[i].src_rect.position.x;
		blit.push_constant.src_rect[1] = p_render_targets[i].src_rect.position.y;
		blit.push_constant.src_rect[2] = p_render_targets[i].src_rect.size.width;
		blit.push_constant.src_rect[3] = p_render_targets[i].src_rect.size.height;
		blit.push_constant.dst_rect[0] = p_render_targets[i].dst_rect.position.x / screen_size.width;
		blit.push_constant.dst_rect[1] = p_render_targets[i].dst_rect.position.y / screen_size.height;
		blit.push_constant.dst_rect[2] = p_render_targets[i].dst_rect.size.width / screen_size.width;
		blit.push_constant.dst_rect[3] = p_render_targets[i].dst_rect.size.height / screen_size.height;
		blit.push_constant.layer = p_render_targets[i].multi_view.layer;
		blit.push_constant.eye_center[0] = p_render_targets[i].lens_distortion.eye_center.x;
		blit.push_constant.eye_center[1] = p_render_targets[i].lens_distortion.eye_center.y;
		blit.push_constant.k1 = p_render_targets[i].lens_distortion.k1;
		blit.push_constant.k2 = p_render_targets[i].lens_distortion.k2;
		blit.push_constant.upscale = p_render_targets[i].lens_distortion.upscale;
		blit.push_constant.aspect_ratio = p_render_targets[i].lens_distortion.aspect_ratio;
		blit.push_constant.convert_to_srgb = texture_storage->render_target_is_using_hdr(p_render_targets[i].render_target);

		RD::get_singleton()->draw_list_set_push_constant(draw_list, &blit.push_constant, sizeof(BlitPushConstant));
		RD::get_singleton()->draw_list_draw(draw_list, true);
	}

	RD::get_singleton()->draw_list_end();
}

void RendererCompositorRD::begin_frame(double frame_step) {
	frame++;
	delta = frame_step;
	time += frame_step;

	double time_roll_over = GLOBAL_GET("rendering/limits/time/time_rollover_secs");
	time = Math::fmod(time, time_roll_over);

	canvas->set_time(time);
	scene->set_time(time, frame_step);
}

void RendererCompositorRD::end_frame(bool p_present) {
	RD::get_singleton()->swap_buffers(p_present);
}

void RendererCompositorRD::initialize() {
	{
		// Initialize blit
		Vector<String> blit_modes;
		blit_modes.push_back("\n");
		blit_modes.push_back("\n#define USE_LAYER\n");
		blit_modes.push_back("\n#define USE_LAYER\n#define APPLY_LENS_DISTORTION\n");
		blit_modes.push_back("\n");

		blit.shader.initialize(blit_modes);

		blit.shader_version = blit.shader.version_create();

		for (int i = 0; i < BLIT_MODE_MAX; i++) {
			blit.pipelines[i] = RD::get_singleton()->render_pipeline_create(blit.shader.version_get_shader(blit.shader_version, i), RD::get_singleton()->screen_get_framebuffer_format(DisplayServer::MAIN_WINDOW_ID), RD::INVALID_ID, RD::RENDER_PRIMITIVE_TRIANGLES, RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), RD::PipelineDepthStencilState(), i == BLIT_MODE_NORMAL_ALPHA ? RenderingDevice::PipelineColorBlendState::create_blend() : RenderingDevice::PipelineColorBlendState::create_disabled(), 0);

			// Unload shader modules to save memory.
			RD::get_singleton()->shader_destroy_modules(blit.shader.version_get_shader(blit.shader_version, i));
		}

		//create index array for copy shader
		Vector<uint8_t> pv;
		pv.resize(6 * 2);
		{
			uint8_t *w = pv.ptrw();
			uint16_t *p16 = (uint16_t *)w;
			p16[0] = 0;
			p16[1] = 1;
			p16[2] = 2;
			p16[3] = 0;
			p16[4] = 2;
			p16[5] = 3;
		}
		blit.index_buffer = RD::get_singleton()->index_buffer_create(6, RenderingDevice::INDEX_BUFFER_FORMAT_UINT16, pv);
		blit.array = RD::get_singleton()->index_array_create(blit.index_buffer, 0, 6);

		blit.sampler = RD::get_singleton()->sampler_create(RD::SamplerState());
	}
}

uint64_t RendererCompositorRD::frame = 1;

void RendererCompositorRD::finalize() {
	memdelete(scene);
	memdelete(canvas);
	memdelete(fog);
	memdelete(particles_storage);
	memdelete(light_storage);
	memdelete(mesh_storage);
	memdelete(material_storage);
	memdelete(texture_storage);
	memdelete(utilities);

	//only need to erase these, the rest are erased by cascade
	blit.shader.version_free(blit.shader_version);
	RD::get_singleton()->free(blit.index_buffer);
	RD::get_singleton()->free(blit.sampler);
}

void RendererCompositorRD::set_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale, bool p_use_filter) {
	if (p_image.is_null() || p_image->is_empty()) {
		return;
	}

	Error err = RD::get_singleton()->screen_prepare_for_drawing(DisplayServer::MAIN_WINDOW_ID);
	if (err != OK) {
		// Window is minimized and does not have valid swapchain, skip drawing without printing errors.
		return;
	}

	RID texture = texture_storage->texture_allocate();
	texture_storage->texture_2d_initialize(texture, p_image);
	RID rd_texture = texture_storage->texture_get_rd_texture(texture, false);

	RD::SamplerState sampler_state;
	sampler_state.min_filter = p_use_filter ? RD::SAMPLER_FILTER_LINEAR : RD::SAMPLER_FILTER_NEAREST;
	sampler_state.mag_filter = p_use_filter ? RD::SAMPLER_FILTER_LINEAR : RD::SAMPLER_FILTER_NEAREST;
	sampler_state.max_lod = 0;
	RID sampler = RD::get_singleton()->sampler_create(sampler_state);

	RID uset;
	{
		Vector<RD::Uniform> uniforms;
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
		u.binding = 0;
		u.append_id(sampler);
		u.append_id(rd_texture);
		uniforms.push_back(u);
		uset = RD::get_singleton()->uniform_set_create(uniforms, blit.shader.version_get_shader(blit.shader_version, BLIT_MODE_NORMAL), 0);
	}

	Size2 window_size = DisplayServer::get_singleton()->window_get_size();

	Rect2 imgrect(0, 0, p_image->get_width(), p_image->get_height());
	Rect2 screenrect;
	if (p_scale) {
		if (window_size.width > window_size.height) {
			//scale horizontally
			screenrect.size.y = window_size.height;
			screenrect.size.x = imgrect.size.x * window_size.height / imgrect.size.y;
			screenrect.position.x = (window_size.width - screenrect.size.x) / 2;

		} else {
			//scale vertically
			screenrect.size.x = window_size.width;
			screenrect.size.y = imgrect.size.y * window_size.width / imgrect.size.x;
			screenrect.position.y = (window_size.height - screenrect.size.y) / 2;
		}
	} else {
		screenrect = imgrect;
		screenrect.position += ((window_size - screenrect.size) / 2.0).floor();
	}

	screenrect.position /= window_size;
	screenrect.size /= window_size;

	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin_for_screen(DisplayServer::MAIN_WINDOW_ID, p_color);

	RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, blit.pipelines[BLIT_MODE_NORMAL_ALPHA]);
	RD::get_singleton()->draw_list_bind_index_array(draw_list, blit.array);
	RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uset, 0);

	const int screen_rotation_degrees = -RD::get_singleton()->screen_get_pre_rotation_degrees(DisplayServer::MAIN_WINDOW_ID);
	float screen_rotation = Math::deg_to_rad((float)screen_rotation_degrees);
	blit.push_constant.rotation_cos = Math::cos(screen_rotation);
	blit.push_constant.rotation_sin = Math::sin(screen_rotation);
	blit.push_constant.src_rect[0] = 0.0;
	blit.push_constant.src_rect[1] = 0.0;
	blit.push_constant.src_rect[2] = 1.0;
	blit.push_constant.src_rect[3] = 1.0;
	blit.push_constant.dst_rect[0] = screenrect.position.x;
	blit.push_constant.dst_rect[1] = screenrect.position.y;
	blit.push_constant.dst_rect[2] = screenrect.size.width;
	blit.push_constant.dst_rect[3] = screenrect.size.height;
	blit.push_constant.layer = 0;
	blit.push_constant.eye_center[0] = 0;
	blit.push_constant.eye_center[1] = 0;
	blit.push_constant.k1 = 0;
	blit.push_constant.k2 = 0;
	blit.push_constant.upscale = 1.0;
	blit.push_constant.aspect_ratio = 1.0;
	blit.push_constant.convert_to_srgb = false;

	RD::get_singleton()->draw_list_set_push_constant(draw_list, &blit.push_constant, sizeof(BlitPushConstant));
	RD::get_singleton()->draw_list_draw(draw_list, true);

	RD::get_singleton()->draw_list_end();

	RD::get_singleton()->swap_buffers(true);

	texture_storage->texture_free(texture);
	RD::get_singleton()->free(sampler);
}

RendererCompositorRD *RendererCompositorRD::singleton = nullptr;

RendererCompositorRD::RendererCompositorRD() {
	uniform_set_cache = memnew(UniformSetCacheRD);
	framebuffer_cache = memnew(FramebufferCacheRD);

	{
		String shader_cache_dir = Engine::get_singleton()->get_shader_cache_path();
		if (shader_cache_dir.is_empty()) {
			shader_cache_dir = "user://";
		}
		Ref<DirAccess> da = DirAccess::open(shader_cache_dir);
		if (da.is_null()) {
			ERR_PRINT("Can't create shader cache folder, no shader caching will happen: " + shader_cache_dir);
		} else {
			Error err = da->change_dir("shader_cache");
			if (err != OK) {
				err = da->make_dir("shader_cache");
			}
			if (err != OK) {
				ERR_PRINT("Can't create shader cache folder, no shader caching will happen: " + shader_cache_dir);
			} else {
				shader_cache_dir = shader_cache_dir.path_join("shader_cache");

				bool shader_cache_enabled = GLOBAL_GET("rendering/shader_compiler/shader_cache/enabled");
				if (!Engine::get_singleton()->is_editor_hint() && !shader_cache_enabled) {
					shader_cache_dir = String(); //disable only if not editor
				}

				if (!shader_cache_dir.is_empty()) {
					bool compress = GLOBAL_GET("rendering/shader_compiler/shader_cache/compress");
					bool use_zstd = GLOBAL_GET("rendering/shader_compiler/shader_cache/use_zstd_compression");
					bool strip_debug = GLOBAL_GET("rendering/shader_compiler/shader_cache/strip_debug");

					ShaderRD::set_shader_cache_dir(shader_cache_dir);
					ShaderRD::set_shader_cache_save_compressed(compress);
					ShaderRD::set_shader_cache_save_compressed_zstd(use_zstd);
					ShaderRD::set_shader_cache_save_debug(!strip_debug);
				}
			}
		}
	}

	ERR_FAIL_COND_MSG(singleton != nullptr, "A RendererCompositorRD singleton already exists.");
	singleton = this;

	utilities = memnew(RendererRD::Utilities);
	texture_storage = memnew(RendererRD::TextureStorage);
	material_storage = memnew(RendererRD::MaterialStorage);
	mesh_storage = memnew(RendererRD::MeshStorage);
	light_storage = memnew(RendererRD::LightStorage);
	particles_storage = memnew(RendererRD::ParticlesStorage);
	fog = memnew(RendererRD::Fog);
	canvas = memnew(RendererCanvasRenderRD());

	String rendering_method = OS::get_singleton()->get_current_rendering_method();
	uint64_t textures_per_stage = RD::get_singleton()->limit_get(RD::LIMIT_MAX_TEXTURES_PER_SHADER_STAGE);

	if (rendering_method == "mobile" || textures_per_stage < 48) {
		if (rendering_method == "forward_plus") {
			WARN_PRINT_ONCE("Platform supports less than 48 textures per stage which is less than required by the Clustered renderer. Defaulting to Mobile renderer.");
		}
		scene = memnew(RendererSceneRenderImplementation::RenderForwardMobile());
	} else if (rendering_method == "forward_plus") {
		scene = memnew(RendererSceneRenderImplementation::RenderForwardClustered());
	} else {
		// Fall back to our high end renderer.
		ERR_PRINT(vformat("Cannot instantiate RenderingDevice-based renderer with renderer type '%s'. Defaulting to Forward+ renderer.", rendering_method));
		scene = memnew(RendererSceneRenderImplementation::RenderForwardClustered());
	}

	scene->init();
}

RendererCompositorRD::~RendererCompositorRD() {
	singleton = nullptr;
	memdelete(uniform_set_cache);
	memdelete(framebuffer_cache);
	ShaderRD::set_shader_cache_dir(String());
}
