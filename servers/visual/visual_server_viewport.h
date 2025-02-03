/**************************************************************************/
/*  visual_server_viewport.h                                              */
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

#ifndef VISUAL_SERVER_VIEWPORT_H
#define VISUAL_SERVER_VIEWPORT_H

#include "core/self_list.h"
#include "rasterizer.h"
#include "servers/arvr/arvr_interface.h"
#include "servers/visual_server.h"

class VisualServerViewport {
public:
	struct CanvasBase : public RID_Data {
	};

	struct Viewport : public RID_Data {
		RID self;
		RID parent;

		bool use_arvr; /* use arvr interface to override camera positioning and projection matrices and control output */

		Size2i size;
		RID camera;
		RID scenario;

		VS::ViewportUpdateMode update_mode;
		RID render_target;
		RID render_target_texture;

		bool allow_oq;

		int viewport_to_screen;
		Rect2 viewport_to_screen_rect;
		bool viewport_render_direct_to_screen;

		bool hide_scenario;
		bool hide_canvas;
		bool disable_environment;
		bool disable_3d;
		bool disable_3d_by_usage;
		bool keep_3d_linear;

		RID shadow_atlas;
		int shadow_atlas_size;

		int render_info[VS::VIEWPORT_RENDER_INFO_MAX];
		VS::ViewportDebugDraw debug_draw;

		VS::ViewportClearMode clear_mode;

		bool transparent_bg;

		struct CanvasKey {
			int64_t stacking;
			RID canvas;
			bool operator<(const CanvasKey &p_canvas) const {
				if (stacking == p_canvas.stacking) {
					return canvas < p_canvas.canvas;
				}
				return stacking < p_canvas.stacking;
			}
			CanvasKey() {
				stacking = 0;
			}
			CanvasKey(const RID &p_canvas, int p_layer, int p_sublayer) {
				canvas = p_canvas;
				int64_t sign = p_layer < 0 ? -1 : 1;
				stacking = sign * (((int64_t)ABS(p_layer)) << 32) + p_sublayer;
			}
			int get_layer() const { return stacking >> 32; }
		};

		struct CanvasData {
			CanvasBase *canvas;
			Transform2D transform;
			int layer;
			int sublayer;
		};

		Transform2D global_transform;

		Map<RID, CanvasData> canvas_map;

		Viewport() {
			allow_oq = true;
			update_mode = VS::VIEWPORT_UPDATE_WHEN_VISIBLE;
			clear_mode = VS::VIEWPORT_CLEAR_ALWAYS;
			transparent_bg = false;
			disable_environment = false;
			viewport_to_screen = 0;
			shadow_atlas_size = 0;
			disable_3d = false;
			disable_3d_by_usage = false;
			keep_3d_linear = false;
			debug_draw = VS::VIEWPORT_DEBUG_DRAW_DISABLED;
			for (int i = 0; i < VS::VIEWPORT_RENDER_INFO_MAX; i++) {
				render_info[i] = 0;
			}
			use_arvr = false;
		}
	};

	mutable RID_Owner<Viewport> viewport_owner;

	struct ViewportSort {
		_FORCE_INLINE_ bool operator()(const Viewport *p_left, const Viewport *p_right) const {
			bool left_to_screen = p_left->viewport_to_screen_rect.size != Size2();
			bool right_to_screen = p_right->viewport_to_screen_rect.size != Size2();

			if (left_to_screen == right_to_screen) {
				return p_left->parent == p_right->self;
			}
			return right_to_screen;
		}
	};

	Vector<Viewport *> active_viewports;

	RID current_viewport_id = RID();

private:
	Color clear_color;
	void _draw_3d(Viewport *p_viewport, ARVRInterface::Eyes p_eye);
	void _draw_viewport(Viewport *p_viewport, ARVRInterface::Eyes p_eye = ARVRInterface::EYE_MONO);

public:
	RID viewport_create();

	void viewport_set_use_arvr(RID p_viewport, bool p_use_arvr);

	void viewport_set_size(RID p_viewport, int p_width, int p_height);

	void viewport_attach_to_screen(RID p_viewport, const Rect2 &p_rect = Rect2(), int p_screen = 0);
	void viewport_set_render_direct_to_screen(RID p_viewport, bool p_enable);
	void viewport_detach(RID p_viewport);

	void viewport_set_active(RID p_viewport, bool p_active);
	void viewport_set_parent_viewport(RID p_viewport, RID p_parent_viewport);
	VS::ViewportUpdateMode viewport_get_update_mode(RID p_viewport);
	void viewport_set_update_mode(RID p_viewport, VS::ViewportUpdateMode p_mode);
	void viewport_set_vflip(RID p_viewport, bool p_enable);

	void viewport_set_allow_occlusion_queries(RID p_viewport, bool p_allow);
	bool viewport_get_allow_occlusion_queries(RID p_viewport) const;

	void viewport_set_clear_mode(RID p_viewport, VS::ViewportClearMode p_clear_mode);

	RID viewport_get_texture(RID p_viewport) const;

	void viewport_set_hide_scenario(RID p_viewport, bool p_hide);
	void viewport_set_hide_canvas(RID p_viewport, bool p_hide);
	void viewport_set_disable_environment(RID p_viewport, bool p_disable);
	void viewport_set_disable_3d(RID p_viewport, bool p_disable);
	void viewport_set_keep_3d_linear(RID p_viewport, bool p_keep_3d_linear);

	void viewport_attach_camera(RID p_viewport, RID p_camera);
	void viewport_set_scenario(RID p_viewport, RID p_scenario);
	void viewport_attach_canvas(RID p_viewport, RID p_canvas);
	void viewport_remove_canvas(RID p_viewport, RID p_canvas);
	void viewport_set_canvas_transform(RID p_viewport, RID p_canvas, const Transform2D &p_offset);
	void viewport_set_transparent_background(RID p_viewport, bool p_enabled);

	void viewport_set_global_canvas_transform(RID p_viewport, const Transform2D &p_transform);
	void viewport_set_canvas_stacking(RID p_viewport, RID p_canvas, int p_layer, int p_sublayer);

	void viewport_set_shadow_atlas_size(RID p_viewport, int p_size);
	void viewport_set_shadow_atlas_quadrant_subdivision(RID p_viewport, int p_quadrant, int p_subdiv);

	void viewport_set_msaa(RID p_viewport, VS::ViewportMSAA p_msaa);
	void viewport_set_use_fxaa(RID p_viewport, bool p_fxaa);
	void viewport_set_use_debanding(RID p_viewport, bool p_debanding);
	void viewport_set_sharpen_intensity(RID p_viewport, float p_intensity);
	void viewport_set_hdr(RID p_viewport, bool p_enabled);
	void viewport_set_use_32_bpc_depth(RID p_viewport, bool p_enabled);
	void viewport_set_usage(RID p_viewport, VS::ViewportUsage p_usage);

	virtual int viewport_get_render_info(RID p_viewport, VS::ViewportRenderInfo p_info);
	virtual void viewport_set_debug_draw(RID p_viewport, VS::ViewportDebugDraw p_draw);

	void set_default_clear_color(const Color &p_color);
	void draw_viewports();

	bool free(RID p_rid);

	VisualServerViewport();
	virtual ~VisualServerViewport() {}
};

#endif // VISUAL_SERVER_VIEWPORT_H
