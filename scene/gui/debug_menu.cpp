/**************************************************************************/
/*  debug_menu.cpp                                                        */
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

#include "debug_menu.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/os/time.h"
#include "core/version.h"
#include "scene/3d/camera_3d.h"
#include "scene/gui/label.h"
#include "scene/gui/panel.h"
#include "scene/resources/environment.h"
#include "scene/resources/style_box_flat.h"

void DebugMenu::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			Viewport *viewport_node = get_parent()->get_viewport();
			viewport_rid = viewport_node->get_viewport_rid();
			// Used to update the resolution display when the window is resized.
			viewport_node->connect("size_changed", callable_mp(this, &DebugMenu::_update_settings_label));

			previous_measure_render_time = RenderingServer::get_singleton()->viewport_is_measuring_render_time(viewport_rid);
			// Enable viewport render time measurements, which are required for the debug menu to display CPU and GPU frametimes.
			RenderingServer::get_singleton()->viewport_set_measure_render_time(viewport_rid, true);

		} break;
		case NOTIFICATION_READY: {
			// Make sure the compact setter is run, as this is the default mode when the debug menu is first created.
			set_display_mode(SceneTree::DEBUG_MENU_DISPLAY_MODE_COMPACT);
			_update_information_label();
			_update_settings_label();

			last_tick = Time::get_singleton()->get_ticks_usec();

			// Reset graphs to prevent them from looking strange before `HISTORY_NUM_FRAMES` frames
			// have been drawn.
			fps_history.resize(HISTORY_NUM_FRAMES);
			frame_history_total.resize(HISTORY_NUM_FRAMES);
			frame_history_cpu.resize(HISTORY_NUM_FRAMES);
			frame_history_gpu.resize(HISTORY_NUM_FRAMES);
			const float fps_last = Engine::get_singleton()->get_frames_per_second();
			const float frametime_last = (Time::get_singleton()->get_ticks_usec() - last_tick) * 0.001;
			const float render_time_cpu = RenderingServer::get_singleton()->viewport_get_measured_render_time_cpu(viewport_rid) + RenderingServer::get_singleton()->get_frame_setup_time_cpu();
			const float render_time_gpu = RenderingServer::get_singleton()->viewport_get_measured_render_time_gpu(viewport_rid);
			for (int i = 0; i < HISTORY_NUM_FRAMES; i++) {
				fps_history.write[i] = fps_last;
				frame_history_total.write[i] = frametime_last;
				frame_history_cpu.write[i] = render_time_cpu;
				frame_history_gpu.write[i] = render_time_gpu;
			}
		} break;
		case NOTIFICATION_PROCESS: {
			if (display_mode == SceneTree::DEBUG_MENU_DISPLAY_MODE_DETAILED) {
				fps_graph_panel->queue_redraw();
				total_graph_panel->queue_redraw();
				cpu_graph_panel->queue_redraw();
				gpu_graph_panel->queue_redraw();
			}

			// Difference between the last two rendered frames in milliseconds.
			const float frametime_total = (Time::get_singleton()->get_ticks_usec() - last_tick) * 0.001;

			frame_history_total.push_back(frametime_total);
			if (frame_history_total.size() > HISTORY_NUM_FRAMES) {
				frame_history_total.remove_at(0);
			}

			// Frametimes are colored following FPS logic (red = 10 FPS, yellow = 60 FPS, green = 110 FPS, cyan = 160 FPS).
			// This makes the color gradient non-linear.
			float frametime_total_sum = 0.0f;
			for (const float ft : frame_history_total) {
				frametime_total_sum += ft;
			}
			frametime_total_avg = frametime_total_sum / frame_history_total.size();
			total_avg->set_text(String::num(frametime_total_avg).pad_decimals(2));
			total_avg->set_modulate(frame_time_gradient->get_color_at_offset(Math::remap(1000.0 / frametime_total_avg, GRAPH_MIN_FPS, GRAPH_MAX_FPS, 0.0, 1.0)));

			float frametime_total_min = FLT_MAX;
			for (const float ft : frame_history_total) {
				frametime_total_min = MIN(frametime_total_min, ft);
			}
			total_best->set_text(String::num(frametime_total_min).pad_decimals(2));
			total_best->set_modulate(frame_time_gradient->get_color_at_offset(Math::remap(1000.0 / frametime_total_min, GRAPH_MIN_FPS, GRAPH_MAX_FPS, 0.0, 1.0)));

			float frametime_total_max = 0.0f;
			for (const float ft : frame_history_total) {
				frametime_total_max = MAX(frametime_total_max, ft);
			}
			total_worst->set_text(String::num(frametime_total_max).pad_decimals(2));
			total_worst->set_modulate(frame_time_gradient->get_color_at_offset(Math::remap(1000.0 / frametime_total_max, GRAPH_MIN_FPS, GRAPH_MAX_FPS, 0.0, 1.0)));

			total_last->set_text(String::num(frametime_total).pad_decimals(2));
			total_last->set_modulate(frame_time_gradient->get_color_at_offset(Math::remap(1000.0 / frametime_total, GRAPH_MIN_FPS, GRAPH_MAX_FPS, 0.0, 1.0)));

			const float frametime_cpu = RenderingServer::get_singleton()->viewport_get_measured_render_time_cpu(viewport_rid) + RenderingServer::get_singleton()->get_frame_setup_time_cpu();
			frame_history_cpu.push_back(frametime_cpu);
			if (frame_history_cpu.size() > HISTORY_NUM_FRAMES) {
				frame_history_cpu.remove_at(0);
			}

			float frametime_cpu_sum = 0.0f;
			for (const float ft : frame_history_cpu) {
				frametime_cpu_sum += ft;
			}
			frametime_cpu_avg = frametime_cpu_sum / frame_history_cpu.size();
			cpu_avg->set_text(String::num(frametime_cpu_avg).pad_decimals(2));
			cpu_avg->set_modulate(frame_time_gradient->get_color_at_offset(Math::remap(1000.0 / frametime_cpu_avg, GRAPH_MIN_FPS, GRAPH_MAX_FPS, 0.0, 1.0)));

			float frametime_cpu_min = FLT_MAX;
			for (const float ft : frame_history_cpu) {
				frametime_cpu_min = MIN(frametime_cpu_min, ft);
			}
			cpu_best->set_text(String::num(frametime_cpu_min).pad_decimals(2));
			cpu_best->set_modulate(frame_time_gradient->get_color_at_offset(Math::remap(1000.0 / frametime_cpu_min, GRAPH_MIN_FPS, GRAPH_MAX_FPS, 0.0, 1.0)));

			float frametime_cpu_max = 0.0f;
			for (const float ft : frame_history_cpu) {
				frametime_cpu_max = MAX(frametime_cpu_max, ft);
			}
			cpu_worst->set_text(String::num(frametime_cpu_max).pad_decimals(2));
			cpu_worst->set_modulate(frame_time_gradient->get_color_at_offset(Math::remap(1000.0 / frametime_cpu_max, GRAPH_MIN_FPS, GRAPH_MAX_FPS, 0.0, 1.0)));

			cpu_last->set_text(String::num(frametime_cpu).pad_decimals(2));
			cpu_last->set_modulate(frame_time_gradient->get_color_at_offset(Math::remap(1000.0 / frametime_cpu, GRAPH_MIN_FPS, GRAPH_MAX_FPS, 0.0, 1.0)));

			const float frametime_gpu = RenderingServer::get_singleton()->viewport_get_measured_render_time_gpu(viewport_rid);
			frame_history_gpu.push_back(frametime_gpu);
			if (frame_history_gpu.size() > HISTORY_NUM_FRAMES) {
				frame_history_gpu.remove_at(0);
			}

			float frametime_gpu_sum = 0.0f;
			for (const float ft : frame_history_gpu) {
				frametime_gpu_sum += ft;
			}
			frametime_gpu_avg = frametime_gpu_sum / frame_history_gpu.size();
			gpu_avg->set_text(String::num(frametime_gpu_avg).pad_decimals(2));
			gpu_avg->set_modulate(frame_time_gradient->get_color_at_offset(Math::remap(1000.0 / frametime_gpu_avg, GRAPH_MIN_FPS, GRAPH_MAX_FPS, 0.0, 1.0)));

			float frametime_gpu_min = FLT_MAX;
			for (const float ft : frame_history_gpu) {
				frametime_gpu_min = MIN(frametime_gpu_min, ft);
			}
			gpu_best->set_text(String::num(frametime_gpu_min).pad_decimals(2));
			gpu_best->set_modulate(frame_time_gradient->get_color_at_offset(Math::remap(1000.0 / frametime_gpu_min, GRAPH_MIN_FPS, GRAPH_MAX_FPS, 0.0, 1.0)));

			float frametime_gpu_max = 0.0f;
			for (const float ft : frame_history_gpu) {
				frametime_gpu_max = MAX(frametime_gpu_max, ft);
			}
			gpu_worst->set_text(String::num(frametime_gpu_max).pad_decimals(2));
			gpu_worst->set_modulate(frame_time_gradient->get_color_at_offset(Math::remap(1000.0 / frametime_gpu_max, GRAPH_MIN_FPS, GRAPH_MAX_FPS, 0.0, 1.0)));

			gpu_last->set_text(String::num(frametime_gpu).pad_decimals(2));
			gpu_last->set_modulate(frame_time_gradient->get_color_at_offset(Math::remap(1000.0 / frametime_gpu, GRAPH_MIN_FPS, GRAPH_MAX_FPS, 0.0, 1.0)));

			const float fps = Engine::get_singleton()->get_frames_per_second();
			fps_history.push_back(fps);
			if (fps_history.size() > HISTORY_NUM_FRAMES) {
				fps_history.remove_at(0);
			}

			frames_per_second->set_text(itos(fps) + " FPS");
			const Color fps_color = frame_time_gradient->get_color_at_offset(Math::remap(fps, GRAPH_MIN_FPS, GRAPH_MAX_FPS, 0.0f, 1.0f));
			frames_per_second->set_modulate(fps_color);

			String frametime_string = rtos(1000.0 / fps).pad_decimals(2) + " mspf";

			String vsync_string;
			switch (DisplayServer::get_singleton()->window_get_vsync_mode(DisplayServer::MAIN_WINDOW_ID)) {
				case DisplayServer::VSYNC_DISABLED:
					vsync_string = "";
					break;
				case DisplayServer::VSYNC_ENABLED:
					vsync_string = "V-Sync";
					break;
				case DisplayServer::VSYNC_ADAPTIVE:
					vsync_string = "Adaptive V-Sync";
					break;
				case DisplayServer::VSYNC_MAILBOX:
					vsync_string = "Mailbox V-Sync";
					break;
			}

			String fps_cap_text;
			if (Engine::get_singleton()->get_max_fps() > 0 || OS::get_singleton()->is_in_low_processor_usage_mode()) {
				// Display FPS cap determined by `Engine.max_fps` or low-processor usage mode sleep duration
				// (the lowest FPS cap is used).
				double fps_cap = DBL_MAX;
				if (OS::get_singleton()->is_in_low_processor_usage_mode()) {
					fps_cap = Math::round(1000000.0 / OS::get_singleton()->get_low_processor_usage_mode_sleep_usec());
				}
				if (Engine::get_singleton()->get_max_fps() > 0) {
					fps_cap = MIN(fps_cap, Engine::get_singleton()->get_max_fps());
				}
				if (!Math::is_equal_approx(fps_cap, DBL_MAX)) {
					frametime_string += " (cap: " + itos(fps_cap) + " FPS";
				}

				if (!vsync_string.is_empty()) {
					frametime_string += " + " + vsync_string;
				}

				frametime_string += ")";
			} else if (!vsync_string.is_empty()) {
				frametime_string += " (" + vsync_string + ")";
			}

			frametime->set_text(frametime_string);
			frametime->set_modulate(fps_color);

			// Only update visible nodes to avoid wasting CPU cycles.
			if (frame_number->is_visible()) {
				frame_number->set_text("Frame: " + itos(Engine::get_singleton()->get_frames_drawn()));
			}

			last_tick = Time::get_singleton()->get_ticks_usec();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			// Restore previous state of viewport render time measurements.
			RenderingServer::get_singleton()->viewport_set_measure_render_time(viewport_rid, previous_measure_render_time);
		} break;
	}
}

void DebugMenu::set_display_mode(SceneTree::DebugMenuDisplayMode p_mode) {
	if (p_mode == display_mode) {
		return;
	}

	display_mode = p_mode;

	switch (display_mode) {
		case SceneTree::DEBUG_MENU_DISPLAY_MODE_COMPACT:
			frame_number->hide();
			frame_time_history->hide();
			fps_graph->hide();
			total_graph->hide();
			cpu_graph->hide();
			gpu_graph->hide();
			information->hide();
			settings->hide();
			break;
		case SceneTree::DEBUG_MENU_DISPLAY_MODE_DETAILED:
			frame_number->show();
			frame_time_history->show();
			fps_graph->show();
			total_graph->show();
			cpu_graph->show();
			gpu_graph->show();
			information->show();
			settings->show();
			break;
		default:
			// We don't handle `DEBUG_MENU_DISPLAY_MODE_HIDDEN` as the node is freed immediately when it's set.
			break;
	}
}

SceneTree::DebugMenuDisplayMode DebugMenu::get_display_mode() const {
	return display_mode;
}

void DebugMenu::_update_information_label() {
	String adapter_string;
	if (RenderingServer::get_singleton()->get_video_adapter_name().contains(RenderingServer::get_singleton()->get_video_adapter_vendor())) {
		// Avoid repeating vendor name before adapter name.
		adapter_string = RenderingServer::get_singleton()->get_video_adapter_name();
	} else {
		adapter_string = RenderingServer::get_singleton()->get_video_adapter_vendor() + " - " + RenderingServer::get_singleton()->get_video_adapter_name();
	}

	// Graphics driver version information isn't always available.
	Vector<String> driver_info = OS::get_singleton()->get_video_adapter_driver_info();
	String driver_info_string;
	if (driver_info.size() >= 2) {
		driver_info_string = driver_info[1];
	} else {
		driver_info_string = "(unknown)";
	}

	String release_string = "";
#if defined(TOOLS_ENABLED)
	// Editor build (implies `debug template`).
	release_string = "editor";
#elif defined(DEBUG_ENABLED)
	// Debug export template build.
	release_string = "debug";
#else
	// Release export template build.
	release_string = "release";
#endif

	String graphics_api_string = OS::get_singleton()->get_current_rendering_driver_name();
	if (OS::get_singleton()->get_current_rendering_method() != "gl_compatibility") {
		if (OS::get_singleton()->get_current_rendering_driver_name() == "d3d12") {
			graphics_api_string = "Direct3D 12";
		} else if (OS::get_singleton()->get_current_rendering_driver_name() == "metal") {
			graphics_api_string = "Metal";
		} else if (OS::get_singleton()->get_current_rendering_driver_name() == "vulkan") {
#if defined(MACOS_ENABLED) || defined(IOS_ENABLED)
			graphics_api_string = "Vulkan via MoltenVK";
#else
			graphics_api_string = "Vulkan";
#endif
		}
	} else {
		if (OS::get_singleton()->get_current_rendering_driver_name() == "opengl3_angle") {
			graphics_api_string = "OpenGL via ANGLE";
		} else if (OS::get_singleton()->has_feature("mobile") || OS::get_singleton()->get_current_rendering_driver_name() == "opengl3_es") {
			graphics_api_string = "OpenGL ES";
		} else if (OS::get_singleton()->has_feature("web")) {
			graphics_api_string = "WebGL";
		} else if (OS::get_singleton()->get_current_rendering_driver_name() == "opengl3") {
			graphics_api_string = "OpenGL";
		}
	}

#ifdef REAL_T_IS_DOUBLE
	const String precision_string = "double";
#else
	const String precision_string = "single";
#endif

	information->set_text(
			vformat("%s, %d threads\n", OS::get_singleton()->get_processor_name().replace("(R)", "").replace("(TM)", ""), OS::get_singleton()->get_processor_count()) + vformat("%s, %s\n", adapter_string, driver_info_string) + vformat("%s %s (%s %s), %s %s", OS::get_singleton()->get_name(), OS::get_singleton()->has_feature("64") ? "64-bit" : "32-bit", release_string, precision_string, graphics_api_string, RenderingServer::get_singleton()->get_video_adapter_api_version()));
}

void DebugMenu::_update_settings_label() {
	String settings_text;

	const String version = ProjectSettings::get_singleton()->get_setting_with_override("application/config/version");
	if (!version.is_empty()) {
		settings_text += vformat("Project version: %s\n", version);
	}

	settings_text += "Engine version: " VERSION_FULL_CONFIG "\n";

	Engine::get_singleton()->get_version_info();

	String rendering_method_string;
	if (OS::get_singleton()->get_current_rendering_method() == "forward_plus") {
		rendering_method_string = "Forward+";
		settings->set_modulate(Color(0.6, 1.0, 0.6, 0.75));
	} else if (OS::get_singleton()->get_current_rendering_method() == "mobile") {
		rendering_method_string = "Mobile";
		settings->set_modulate(Color(1.0, 0.6, 1.0, 0.75));
	} else if (OS::get_singleton()->get_current_rendering_method() == "gl_compatibility") {
		rendering_method_string = "Compatibility";
		settings->set_modulate(Color(0.6, 1.0, 1.0, 0.75));
	} else {
		rendering_method_string = OS::get_singleton()->get_current_rendering_method();
		settings->set_modulate(Color(1.0, 1.0, 1.0, 0.75));
	}

	settings_text += vformat("Rendering method: %s\n", rendering_method_string);

	const Window *window = get_tree()->get_root();
	if (window) {
		// The size of the viewport rendering, which determines which resolution 3D is rendered at.
		Vector2i viewport_render_size;

		if (window->get_content_scale_mode() == Window::CONTENT_SCALE_MODE_VIEWPORT) {
			viewport_render_size = window->get_visible_rect().size;
			settings_text += vformat(U"Viewport: %d×%d, Window: %d×%d\n", window->get_visible_rect().size.x, window->get_visible_rect().size.y, window->get_size().x, window->get_size().y);
		} else {
			// Window size matches viewport size.
			viewport_render_size = window->get_size();
			settings_text += vformat(U"Viewport: %d×%d\n", window->get_size().x, window->get_size().y);
		}

#ifndef DISABLE_3D
		// Display 3D settings only if relevant.
		if (window->get_camera_3d()) {
			String scaling_3d_mode_string;
			switch (window->get_scaling_3d_mode()) {
				case Viewport::SCALING_3D_MODE_BILINEAR:
					scaling_3d_mode_string = "Bilinear";
					break;
				case Viewport::SCALING_3D_MODE_FSR:
					scaling_3d_mode_string = "FSR 1.0";
					break;
				case Viewport::SCALING_3D_MODE_FSR2:
					scaling_3d_mode_string = "FSR 2.2";
					break;
				default:
					scaling_3d_mode_string = "(unknown)";
					break;
			}

			settings_text += vformat(U"3D scale (%s): %d%% = %d×%d",
					scaling_3d_mode_string,
					window->get_scaling_3d_scale() * 100,
					viewport_render_size.x * window->get_scaling_3d_scale(),
					viewport_render_size.y * window->get_scaling_3d_scale());

			String antialiasing_3d_string;
			if (window->get_scaling_3d_mode() == Viewport::SCALING_3D_MODE_FSR2) {
				// The FSR2 scaling mode includes its own temporal antialiasing implementation.
				antialiasing_3d_string += String(!antialiasing_3d_string.is_empty() ? " + " : "") + "FSR 2.2";
			}
			if (window->get_scaling_3d_mode() != Viewport::SCALING_3D_MODE_FSR2 && window->is_using_taa()) {
				// Godot's own TAA is ignored when using FSR2 scaling mode, as FSR2 provides its own TAA implementation.
				antialiasing_3d_string += String(!antialiasing_3d_string.is_empty() ? " + " : "") + "TAA";
			}
			if (window->get_msaa_3d() >= Viewport::MSAA_2X) {
				antialiasing_3d_string += String(!antialiasing_3d_string.is_empty() ? " + " : "") + vformat(U"%d× MSAA", Math::pow(2.0, window->get_msaa_3d()));
			}
			if (window->get_screen_space_aa() == Viewport::SCREEN_SPACE_AA_FXAA) {
				antialiasing_3d_string += String(!antialiasing_3d_string.is_empty() ? " + " : "") + "FXAA";
			}

			if (!antialiasing_3d_string.is_empty()) {
				settings_text += vformat("\n3D antialiasing: %s", antialiasing_3d_string);
			}

			const Ref<Environment> environment = window->get_camera_3d()->get_world_3d()->get_environment();
			if (environment.is_valid()) {
				if (environment->is_ssr_enabled()) {
					settings_text += vformat("\nSSR: %d Steps", environment->get_ssr_max_steps());
				}

				if (environment->is_ssao_enabled()) {
					settings_text += "\nSSAO: On";
				}

				if (environment->is_ssil_enabled()) {
					settings_text += "\nSSIL: On";
				}

				if (environment->is_sdfgi_enabled()) {
					settings_text += vformat("\nSDFGI: %d Cascades", environment->get_sdfgi_cascades());
				}

				if (environment->is_glow_enabled()) {
					settings_text += "\nGlow: On";
				}

				if (environment->is_volumetric_fog_enabled()) {
					settings_text += "\nVolumetric fog: On";
				}
			}
		}
#endif

		String antialiasing_2d_string;
		if (window->get_msaa_2d() >= Viewport::MSAA_2X) {
			antialiasing_2d_string = vformat(U"%d× MSAA", Math::pow(2.0, window->get_msaa_2d()));
		}

		if (!antialiasing_2d_string.is_empty()) {
			settings_text += vformat("\n2D antialiasing: %s", antialiasing_2d_string);
		}
	}

	settings->set_text(settings_text);
}

void DebugMenu::_graph_draw(GraphType p_type) {
	PackedVector2Array polyline;
	polyline.resize(HISTORY_NUM_FRAMES);

	PackedFloat32Array data_source;
	float data_scale = 0.0f;
	Panel *target_panel = nullptr;
	switch (p_type) {
		case GRAPH_TYPE_FPS:
			data_source = fps_history;
			data_scale = Engine::get_singleton()->get_frames_per_second();
			target_panel = fps_graph_panel;
			break;
		case GRAPH_TYPE_FRAMETIME_TOTAL:
			data_source = frame_history_total;
			data_scale = 1000.0 / frametime_total_avg;
			target_panel = total_graph_panel;
			break;
		case GRAPH_TYPE_FRAMETIME_CPU:
			data_source = frame_history_cpu;
			data_scale = 1000.0 / frametime_cpu_avg;
			target_panel = cpu_graph_panel;
			break;
		case GRAPH_TYPE_FRAMETIME_GPU:
			data_source = frame_history_gpu;
			data_scale = 1000.0 / frametime_gpu_avg;
			target_panel = gpu_graph_panel;
			break;
	}

	for (uint32_t i = 0; i < data_source.size(); i++) {
		polyline.write[i] = Vector2(
				Math::remap(i, 0.0f, data_source.size(), 0, GRAPH_SIZE_X),
				Math::remap(CLAMP(data_source[i], float(GRAPH_MIN_FPS), float(GRAPH_MAX_FPS)), GRAPH_MIN_FPS, GRAPH_MAX_FPS, GRAPH_SIZE_Y, 0.0f));
	}

	// Don't use antialiasing to speed up line drawing, but use a width that scales with
	// viewport scale to keep the line easily readable on hiDPI displays.
	target_panel->draw_polyline(polyline, frame_time_gradient->get_color_at_offset(Math::remap(data_scale, float(GRAPH_MIN_FPS), float(GRAPH_MAX_FPS), 0.0f, 1.0f)), 1.0);
}

DebugMenu::DebugMenu() {
	// Set on the highest layer, so that nothing else can draw on top.
	set_layer(128);

	set_process(true);
	// Process even when paused, so that frametime reporting keeps working.
	set_process_mode(Node::PROCESS_MODE_ALWAYS);

	MarginContainer *margin_container = memnew(MarginContainer);
	margin_container->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	const float default_base_scale = margin_container->get_theme_default_base_scale();
	const float default_font_size = margin_container->get_theme_default_font_size();
	margin_container->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	margin_container->add_theme_constant_override("margin_top", Math::round(10 * default_base_scale));
	margin_container->add_theme_constant_override("margin_right", Math::round(17 * default_base_scale));
	margin_container->add_theme_constant_override("margin_bottom", Math::round(10 * default_base_scale));
	margin_container->add_theme_constant_override("margin_left", Math::round(17 * default_base_scale));
	add_child(margin_container);

	VBoxContainer *vbox_container = memnew(VBoxContainer);
	vbox_container->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	vbox_container->set_anchors_and_offsets_preset(Control::PRESET_TOP_RIGHT);
	vbox_container->add_theme_constant_override("separation", Math::round(2 * default_base_scale));
	margin_container->add_child(vbox_container);

	frame_time_gradient.instantiate();
	frame_time_gradient->set_color(0, Color(1, 0, 0));
	frame_time_gradient->set_color(1, Color(0, 0.6, 1));
	frame_time_gradient->add_point(0.25, Color(1, 1, 0));
	frame_time_gradient->add_point(0.5, Color(0, 1, 0));
	frame_time_gradient->add_point(0.75, Color(0, 1, 1));

	Ref<LabelSettings> label_settings_large = memnew(LabelSettings);
	label_settings_large.instantiate();
	label_settings_large->set_font_size(Math::round(1.125 * default_font_size));
	label_settings_large->set_outline_size(Math::round(7 * default_base_scale));
	label_settings_large->set_outline_color(Color(0, 0, 0));

	Ref<LabelSettings> label_settings_small = memnew(LabelSettings);
	label_settings_small.instantiate();
	label_settings_small->set_font_size(Math::round(0.75 * default_font_size));
	label_settings_small->set_outline_size(Math::round(5 * default_base_scale));
	label_settings_small->set_outline_color(Color(0, 0, 0));

	frames_per_second = memnew(Label);
	frames_per_second->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	frames_per_second->set_label_settings(label_settings_large);
	frames_per_second->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	vbox_container->add_child(frames_per_second);

	frametime = memnew(Label);
	frametime->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	frametime->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	frametime->set_label_settings(label_settings_small);
	vbox_container->add_child(frametime);

	frame_number = memnew(Label);
	frame_number->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	frame_number->set_label_settings(label_settings_small);
	frame_number->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	frame_number->set_modulate(Color(1, 1, 1, 0.75));
	vbox_container->add_child(frame_number);

	frame_time_history = memnew(GridContainer);
	frame_time_history->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	frame_time_history->set_columns(5);
	frame_time_history->set_h_size_flags(Control::SIZE_SHRINK_END);
	// Reduce separation between items.
	frame_time_history->add_theme_constant_override("h_separation", 0);
	frame_time_history->add_theme_constant_override("v_separation", 0);
	vbox_container->add_child(frame_time_history);

	spacer_header = memnew(Control);
	spacer_header->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	spacer_header->set_custom_minimum_size(Vector2(50, 0) * default_base_scale);
	frame_time_history->add_child(spacer_header);

	avg_header = memnew(Label);
	avg_header->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	avg_header->set_label_settings(label_settings_small);
	avg_header->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	avg_header->set_custom_minimum_size(Vector2(50, 0) * default_base_scale);
	avg_header->set_text("Average");
	frame_time_history->add_child(avg_header);

	best_header = memnew(Label);
	best_header->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	best_header->set_label_settings(label_settings_small);
	best_header->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	best_header->set_custom_minimum_size(Vector2(50, 0) * default_base_scale);
	best_header->set_text("Best");
	frame_time_history->add_child(best_header);

	worst_header = memnew(Label);
	worst_header->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	worst_header->set_label_settings(label_settings_small);
	worst_header->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	worst_header->set_custom_minimum_size(Vector2(50, 0) * default_base_scale);
	worst_header->set_text("Worst");
	frame_time_history->add_child(worst_header);

	last_header = memnew(Label);
	last_header->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	last_header->set_label_settings(label_settings_small);
	last_header->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	last_header->set_custom_minimum_size(Vector2(50, 0) * default_base_scale);
	last_header->set_text("Last");
	frame_time_history->add_child(last_header);

	total_header = memnew(Label);
	total_header->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	total_header->set_label_settings(label_settings_small);
	total_header->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	total_header->set_custom_minimum_size(Vector2(50, 0) * default_base_scale);
	total_header->set_text("Total:");
	frame_time_history->add_child(total_header);

	total_avg = memnew(Label);
	total_avg->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	total_avg->set_label_settings(label_settings_small);
	total_avg->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	total_avg->set_custom_minimum_size(Vector2(50, 0) * default_base_scale);
	frame_time_history->add_child(total_avg);

	total_best = memnew(Label);
	total_best->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	total_best->set_label_settings(label_settings_small);
	total_best->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	total_best->set_custom_minimum_size(Vector2(50, 0) * default_base_scale);
	frame_time_history->add_child(total_best);

	total_worst = memnew(Label);
	total_worst->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	total_worst->set_label_settings(label_settings_small);
	total_worst->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	total_worst->set_custom_minimum_size(Vector2(50, 0) * default_base_scale);
	frame_time_history->add_child(total_worst);

	total_last = memnew(Label);
	total_last->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	total_last->set_label_settings(label_settings_small);
	total_last->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	total_last->set_custom_minimum_size(Vector2(50, 0) * default_base_scale);
	frame_time_history->add_child(total_last);

	cpu_header = memnew(Label);
	cpu_header->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	cpu_header->set_label_settings(label_settings_small);
	cpu_header->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	cpu_header->set_custom_minimum_size(Vector2(50, 0) * default_base_scale);
	cpu_header->set_text("CPU:");
	frame_time_history->add_child(cpu_header);

	cpu_avg = memnew(Label);
	cpu_avg->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	cpu_avg->set_label_settings(label_settings_small);
	cpu_avg->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	cpu_avg->set_custom_minimum_size(Vector2(50, 0) * default_base_scale);
	frame_time_history->add_child(cpu_avg);

	cpu_best = memnew(Label);
	cpu_best->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	cpu_best->set_label_settings(label_settings_small);
	cpu_best->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	cpu_best->set_custom_minimum_size(Vector2(50, 0) * default_base_scale);
	frame_time_history->add_child(cpu_best);

	cpu_worst = memnew(Label);
	cpu_worst->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	cpu_worst->set_label_settings(label_settings_small);
	cpu_worst->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	cpu_worst->set_custom_minimum_size(Vector2(50, 0) * default_base_scale);
	frame_time_history->add_child(cpu_worst);

	cpu_last = memnew(Label);
	cpu_last->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	cpu_last->set_label_settings(label_settings_small);
	cpu_last->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	cpu_last->set_custom_minimum_size(Vector2(50, 0) * default_base_scale);
	frame_time_history->add_child(cpu_last);

	gpu_header = memnew(Label);
	gpu_header->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	gpu_header->set_label_settings(label_settings_small);
	gpu_header->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	gpu_header->set_custom_minimum_size(Vector2(50, 0) * default_base_scale);
	gpu_header->set_text("GPU:");
	frame_time_history->add_child(gpu_header);

	gpu_avg = memnew(Label);
	gpu_avg->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	gpu_avg->set_label_settings(label_settings_small);
	gpu_avg->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	gpu_avg->set_custom_minimum_size(Vector2(50, 0) * default_base_scale);
	frame_time_history->add_child(gpu_avg);

	gpu_best = memnew(Label);
	gpu_best->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	gpu_best->set_label_settings(label_settings_small);
	gpu_best->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	gpu_best->set_custom_minimum_size(Vector2(50, 0) * default_base_scale);
	frame_time_history->add_child(gpu_best);

	gpu_worst = memnew(Label);
	gpu_worst->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	gpu_worst->set_label_settings(label_settings_small);
	gpu_worst->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	gpu_worst->set_custom_minimum_size(Vector2(50, 0) * default_base_scale);
	frame_time_history->add_child(gpu_worst);

	gpu_last = memnew(Label);
	gpu_last->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	gpu_last->set_label_settings(label_settings_small);
	gpu_last->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	gpu_last->set_custom_minimum_size(Vector2(50, 0) * default_base_scale);
	frame_time_history->add_child(gpu_last);

	Ref<StyleBoxFlat> graph_background;
	graph_background.instantiate();
	graph_background->set_bg_color(Color(0, 0, 0, 0.3));

	fps_graph = memnew(HBoxContainer);
	fps_graph->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	fps_graph->set_alignment(BoxContainer::ALIGNMENT_END);
	vbox_container->add_child(fps_graph);

	fps_graph_title = memnew(Label);
	fps_graph_title->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	fps_graph_title->set_label_settings(label_settings_small);
	fps_graph_title->set_text(U"FPS: ↑");
	fps_graph_title->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	fps_graph_title->set_h_size_flags(Control::SIZE_SHRINK_END);
	fps_graph->add_child(fps_graph_title);

	fps_graph_panel = memnew(Panel);
	fps_graph_panel->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	fps_graph_panel->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
	fps_graph_panel->set_custom_minimum_size(Vector2(GRAPH_SIZE_X, GRAPH_SIZE_Y) * default_base_scale);
	fps_graph_panel->add_theme_style_override("panel", graph_background);
	fps_graph_panel->connect("draw", callable_mp(this, &DebugMenu::_graph_draw).bind(GRAPH_TYPE_FPS));
	fps_graph->add_child(fps_graph_panel);

	total_graph = memnew(HBoxContainer);
	total_graph->set_alignment(BoxContainer::ALIGNMENT_END);
	vbox_container->add_child(total_graph);

	total_graph_title = memnew(Label);
	total_graph_title->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	total_graph_title->set_label_settings(label_settings_small);
	total_graph_title->set_text(U"Total: ↓");
	total_graph_title->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	total_graph_title->set_h_size_flags(Control::SIZE_SHRINK_END);
	total_graph->add_child(total_graph_title);

	total_graph_panel = memnew(Panel);
	total_graph_panel->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	total_graph_panel->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
	total_graph_panel->set_custom_minimum_size(Vector2(GRAPH_SIZE_X, GRAPH_SIZE_Y) * default_base_scale);
	total_graph_panel->add_theme_style_override("panel", graph_background);
	total_graph_panel->connect("draw", callable_mp(this, &DebugMenu::_graph_draw).bind(GRAPH_TYPE_FRAMETIME_TOTAL));
	total_graph->add_child(total_graph_panel);

	cpu_graph = memnew(HBoxContainer);
	cpu_graph->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	cpu_graph->set_alignment(BoxContainer::ALIGNMENT_END);
	vbox_container->add_child(cpu_graph);

	cpu_graph_title = memnew(Label);
	cpu_graph_title->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	cpu_graph_title->set_label_settings(label_settings_small);
	cpu_graph_title->set_text(U"CPU: ↓");
	cpu_graph_title->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	cpu_graph_title->set_h_size_flags(Control::SIZE_SHRINK_END);
	cpu_graph->add_child(cpu_graph_title);

	cpu_graph_panel = memnew(Panel);
	cpu_graph_panel->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	cpu_graph_panel->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
	cpu_graph_panel->set_custom_minimum_size(Vector2(GRAPH_SIZE_X, GRAPH_SIZE_Y) * default_base_scale);
	cpu_graph_panel->add_theme_style_override("panel", graph_background);
	cpu_graph_panel->connect("draw", callable_mp(this, &DebugMenu::_graph_draw).bind(GRAPH_TYPE_FRAMETIME_CPU));
	cpu_graph->add_child(cpu_graph_panel);

	gpu_graph = memnew(HBoxContainer);
	gpu_graph->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	gpu_graph->set_alignment(BoxContainer::ALIGNMENT_END);
	vbox_container->add_child(gpu_graph);

	gpu_graph_title = memnew(Label);
	gpu_graph_title->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	gpu_graph_title->set_label_settings(label_settings_small);
	gpu_graph_title->set_text(U"GPU: ↓");
	gpu_graph_title->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	gpu_graph_title->set_h_size_flags(Control::SIZE_SHRINK_END);
	gpu_graph->add_child(gpu_graph_title);

	gpu_graph_panel = memnew(Panel);
	gpu_graph_panel->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	gpu_graph_panel->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
	gpu_graph_panel->set_custom_minimum_size(Vector2(GRAPH_SIZE_X, GRAPH_SIZE_Y) * default_base_scale);
	gpu_graph_panel->add_theme_style_override("panel", graph_background);
	gpu_graph_panel->connect("draw", callable_mp(this, &DebugMenu::_graph_draw).bind(GRAPH_TYPE_FRAMETIME_GPU));
	gpu_graph->add_child(gpu_graph_panel);

	information = memnew(Label);
	information->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	information->set_modulate(Color(1, 1, 1, 0.75));
	information->set_label_settings(label_settings_small);
	information->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	vbox_container->add_child(information);

	settings = memnew(Label);
	settings->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	settings->set_label_settings(label_settings_small);
	settings->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	vbox_container->add_child(settings);
}
