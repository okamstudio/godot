/**************************************************************************/
/*  viewport.h                                                            */
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

#ifndef VIEWPORT_H
#define VIEWPORT_H

#include "core/math/transform_2d.h"
#include "scene/main/node.h"
#include "scene/resources/texture.h"
#include "scene/resources/world_2d.h"

class Camera;
class Camera2D;
class Listener;
class Listener2D;
class Control;
class CanvasItem;
class CanvasLayer;
class Panel;
class Label;
class Timer;
class Viewport;
class CollisionObject;
class SceneTreeTimer;

class ViewportTexture : public Texture {
	GDCLASS(ViewportTexture, Texture);

	NodePath path;

	friend class Viewport;
	Viewport *vp;
	uint32_t flags;

	RID proxy;

protected:
	static void _bind_methods();

public:
	void set_viewport_path_in_scene(const NodePath &p_path);
	NodePath get_viewport_path_in_scene() const;

	virtual void setup_local_to_scene();

	virtual int get_width() const;
	virtual int get_height() const;
	virtual Size2 get_size() const;
	virtual RID get_rid() const;

	virtual bool has_alpha() const;

	virtual void set_flags(uint32_t p_flags);
	virtual uint32_t get_flags() const;

	virtual Ref<Image> get_data() const;

	ViewportTexture();
	~ViewportTexture();
};

class Viewport : public Node {
	GDCLASS(Viewport, Node);

public:
	enum UpdateMode {
		UPDATE_DISABLED,
		UPDATE_ONCE, //then goes to disabled
		UPDATE_WHEN_VISIBLE, // default
		UPDATE_ALWAYS
	};

	enum ShadowAtlasQuadrantSubdiv {
		SHADOW_ATLAS_QUADRANT_SUBDIV_DISABLED,
		SHADOW_ATLAS_QUADRANT_SUBDIV_1,
		SHADOW_ATLAS_QUADRANT_SUBDIV_4,
		SHADOW_ATLAS_QUADRANT_SUBDIV_16,
		SHADOW_ATLAS_QUADRANT_SUBDIV_64,
		SHADOW_ATLAS_QUADRANT_SUBDIV_256,
		SHADOW_ATLAS_QUADRANT_SUBDIV_1024,
		SHADOW_ATLAS_QUADRANT_SUBDIV_MAX,

	};

	enum MSAA {
		MSAA_DISABLED,
		MSAA_2X,
		MSAA_4X,
		MSAA_8X,
		MSAA_16X,
	};

	enum Usage {
		USAGE_2D,
		USAGE_2D_NO_SAMPLING,
		USAGE_3D,
		USAGE_3D_NO_EFFECTS,
	};

	enum RenderInfo {

		RENDER_INFO_OBJECTS_IN_FRAME,
		RENDER_INFO_VERTICES_IN_FRAME,
		RENDER_INFO_MATERIAL_CHANGES_IN_FRAME,
		RENDER_INFO_SHADER_CHANGES_IN_FRAME,
		RENDER_INFO_SURFACE_CHANGES_IN_FRAME,
		RENDER_INFO_DRAW_CALLS_IN_FRAME,
		RENDER_INFO_2D_ITEMS_IN_FRAME,
		RENDER_INFO_2D_DRAW_CALLS_IN_FRAME,
		RENDER_INFO_MAX
	};

	enum DebugDraw {
		DEBUG_DRAW_DISABLED,
		DEBUG_DRAW_UNSHADED,
		DEBUG_DRAW_OVERDRAW,
		DEBUG_DRAW_WIREFRAME,
	};

	enum ClearMode {

		CLEAR_MODE_ALWAYS,
		CLEAR_MODE_NEVER,
		CLEAR_MODE_ONLY_NEXT_FRAME
	};

private:
	friend class ViewportTexture;

	Viewport *parent;

	Listener *listener;
	Set<Listener *> listeners;

	bool arvr;

	struct CameraOverrideData {
		Transform transform;
		enum Projection {
			PROJECTION_PERSPECTIVE,
			PROJECTION_ORTHOGONAL
		};
		Projection projection;
		float fov;
		float size;
		float z_near;
		float z_far;
		RID rid;

		operator bool() const {
			return rid != RID();
		}
	} camera_override;

	Camera *camera;
	Set<Camera *> cameras;
	Listener2D *listener_2d = nullptr;
	Set<CanvasLayer *> canvas_layers;

	RID viewport;
	RID current_canvas;

	bool audio_listener;
	RID internal_listener;

	bool audio_listener_2d;
	RID internal_listener_2d;

	bool override_canvas_transform;

	Transform2D canvas_transform_override;
	Transform2D canvas_transform;
	Transform2D global_canvas_transform;
	Transform2D stretch_transform;

	Size2 size;
	Rect2 to_screen_rect;
	bool render_direct_to_screen;

	RID contact_2d_debug;
	RID contact_3d_debug_multimesh;
	RID contact_3d_debug_instance;

	bool size_override;
	bool size_override_stretch;
	Size2 size_override_size;
	Size2 size_override_margin;

	Rect2 last_vp_rect;

	bool transparent_bg;
	bool vflip;
	ClearMode clear_mode;
	bool filter;
	bool gen_mipmaps;

	bool snap_controls_to_pixels;

	bool physics_object_picking;
	List<Ref<InputEvent>> physics_picking_events;
	ObjectID physics_object_capture;
	ObjectID physics_object_over;
	Transform physics_last_object_transform;
	Transform physics_last_camera_transform;
	ObjectID physics_last_id;
	bool physics_has_last_mousepos;
	Vector2 physics_last_mousepos;
	struct {
		bool alt;
		bool control;
		bool shift;
		bool meta;
		int mouse_mask;

	} physics_last_mouse_state;

	void _collision_object_input_event(CollisionObject *p_object, Camera *p_camera, const Ref<InputEvent> &p_input_event, const Vector3 &p_pos, const Vector3 &p_normal, int p_shape);

	bool handle_input_locally;
	bool local_input_handled;

	Map<ObjectID, uint64_t> physics_2d_mouseover;

	Ref<World2D> world_2d;
	Ref<World> world;
	Ref<World> own_world;

	StringName input_group;
	StringName gui_input_group;
	StringName unhandled_input_group;
	StringName unhandled_key_input_group;

	void _update_listener();
	void _update_listener_2d();

	void _propagate_enter_world(Node *p_node);
	void _propagate_exit_world(Node *p_node);
	void _propagate_viewport_notification(Node *p_node, int p_what);

	void _update_stretch_transform();
	void _update_global_transform();

	bool disable_3d;
	bool keep_3d_linear;
	UpdateMode update_mode;
	RID texture_rid;
	uint32_t texture_flags;

	DebugDraw debug_draw;

	Usage usage;

	int shadow_atlas_size;
	ShadowAtlasQuadrantSubdiv shadow_atlas_quadrant_subdiv[4];

	MSAA msaa;
	bool use_fxaa;
	bool use_debanding;
	float sharpen_intensity;
	bool hdr;
	bool use_32_bpc_depth;

	Ref<ViewportTexture> default_texture;
	Set<ViewportTexture *> viewport_textures;

	struct GUI {
		// info used when this is a window

		bool key_event_accepted;
		bool mouse_in_window;
		Map<int, ObjectID> touch_focus;
		Control *mouse_focus;
		Control *last_mouse_focus;
		Control *mouse_click_grabber;
		int mouse_focus_mask;
		Control *key_focus;
		Control *mouse_over;
		Control *tooltip_control;
		Control *tooltip_popup;
		Label *tooltip_label;
		Point2 tooltip_pos;
		Point2 last_mouse_pos;
		Point2 drag_accum;
		bool drag_attempted;
		Variant drag_data;
		ObjectID drag_preview_id;
		Ref<SceneTreeTimer> tooltip_timer;
		float tooltip_delay;
		List<Control *> modal_stack;
		Transform2D focus_inv_xform;
		bool subwindow_order_dirty;
		bool subwindow_visibility_dirty;
		List<Control *> subwindows; // visible subwindows
		List<Control *> all_known_subwindows;
		bool roots_order_dirty;
		List<Control *> roots;
		int canvas_sort_index; //for sorting items with canvas as root
		bool dragging;
		bool drag_successful;

		struct CanvasParent {
			ObjectID id;
			uint32_t first_child_moved = UINT32_MAX;
			uint32_t last_child_moved_plus_one = 0;
		};

		static LocalVector<CanvasParent> canvas_parents_dirty_order;

		GUI();
	} gui;

	bool disable_input;

	void _gui_call_input(Control *p_control, const Ref<InputEvent> &p_input);
	void _gui_call_notification(Control *p_control, int p_what);

	void _gui_prepare_subwindows();
	void _gui_sort_subwindows();
	void _gui_sort_roots();
	void _gui_sort_modal_stack();
	Control *_gui_find_control(const Point2 &p_global);
	Control *_gui_find_control_at_pos(CanvasItem *p_node, const Point2 &p_global, const Transform2D &p_xform, Transform2D &r_inv_xform);

	void _gui_input_event(Ref<InputEvent> p_event);
	void _gui_cleanup_internal_state(Ref<InputEvent> p_event);

	void update_worlds();

	_FORCE_INLINE_ Transform2D _get_input_pre_xform() const;

	void _vp_input(const Ref<InputEvent> &p_ev);
	void _vp_input_text(const String &p_text);
	void _vp_unhandled_input(const Ref<InputEvent> &p_ev);
	Ref<InputEvent> _make_input_local(const Ref<InputEvent> &ev);

	friend class Control;

	List<Control *>::Element *_gui_add_root_control(Control *p_control);
	List<Control *>::Element *_gui_add_subwindow_control(Control *p_control);

	void _gui_set_subwindow_order_dirty();
	void _gui_set_root_order_dirty();

	void _gui_remove_modal_control(List<Control *>::Element *MI);
	void _gui_remove_from_modal_stack(List<Control *>::Element *MI, ObjectID p_prev_focus_owner);
	void _gui_remove_root_control(List<Control *>::Element *RI);
	void _gui_remove_subwindow_control(List<Control *>::Element *SI);

	String _gui_get_tooltip(Control *p_control, const Vector2 &p_pos, Control **r_tooltip_owner = nullptr);
	void _gui_cancel_tooltip();
	void _gui_show_tooltip();

	void _gui_remove_control(Control *p_control);
	void _gui_hid_control(Control *p_control);

	void _gui_force_drag(Control *p_base, const Variant &p_data, Control *p_control);
	void _gui_set_drag_preview(Control *p_base, Control *p_control);
	Control *_gui_get_drag_preview();

	bool _gui_is_modal_on_top(const Control *p_control);
	List<Control *>::Element *_gui_show_modal(Control *p_control);

	void _gui_unfocus_control(Control *p_control);
	bool _gui_control_has_focus(const Control *p_control);
	void _gui_control_grab_focus(Control *p_control);
	void _gui_grab_click_focus(Control *p_control);
	void _post_gui_grab_click_focus();
	void _gui_accept_event();

	Vector2 _get_window_offset() const;

	bool _gui_drop(Control *p_at_control, Point2 p_at_pos, bool p_just_check);

	friend class Listener;
	void _listener_transform_changed_notify();
	void _listener_set(Listener *p_listener);
	bool _listener_add(Listener *p_listener); //true if first
	void _listener_remove(Listener *p_listener);
	void _listener_make_next_current(Listener *p_exclude);

	friend class Camera;
	void _camera_transform_changed_notify();
	void _camera_set(Camera *p_camera);
	bool _camera_add(Camera *p_camera); //true if first
	void _camera_remove(Camera *p_camera);
	void _camera_make_next_current(Camera *p_exclude);

	friend class Listener2D;
	void _listener_2d_set(Listener2D *p_listener);
	void _listener_2d_remove(Listener2D *p_listener);

	friend class CanvasLayer;
	void _canvas_layer_add(CanvasLayer *p_canvas_layer);
	void _canvas_layer_remove(CanvasLayer *p_canvas_layer);

	void _drop_mouse_focus();
	void _drop_mouse_over();
	void _drop_physics_mouseover(bool p_paused_only = false);

	void _update_canvas_items(Node *p_node);

	void _own_world_changed();

protected:
	void _notification(int p_what);
	void _process_picking(bool p_ignore_paused);
	static void _bind_methods();
	virtual void _validate_property(PropertyInfo &property) const;

public:
	Listener *get_listener() const;
	Camera *get_camera() const;

	void enable_camera_override(bool p_enable);
	bool is_camera_override_enabled() const;

	void set_camera_override_transform(const Transform &p_transform);
	Transform get_camera_override_transform() const;

	void set_camera_override_perspective(float p_fovy_degrees, float p_z_near, float p_z_far);
	void set_camera_override_orthogonal(float p_size, float p_z_near, float p_z_far);

	void set_use_arvr(bool p_use_arvr);
	bool use_arvr();

	void set_as_audio_listener(bool p_enable);
	bool is_audio_listener() const;

	Listener2D *get_listener_2d() const;
	void set_as_audio_listener_2d(bool p_enable);
	bool is_audio_listener_2d() const;

	void set_size(const Size2 &p_size);
	void update_canvas_items();

	Size2 get_size() const;
	Rect2 get_visible_rect() const;
	RID get_viewport_rid() const;

	void set_world(const Ref<World> &p_world);
	void set_world_2d(const Ref<World2D> &p_world_2d);
	Ref<World> get_world() const;
	Ref<World> find_world() const;

	Ref<World2D> get_world_2d() const;
	Ref<World2D> find_world_2d() const;

	void enable_canvas_transform_override(bool p_enable);
	bool is_canvas_transform_override_enbled() const;

	void set_canvas_transform_override(const Transform2D &p_transform);
	Transform2D get_canvas_transform_override() const;

	void set_canvas_transform(const Transform2D &p_transform);
	Transform2D get_canvas_transform() const;

	void set_global_canvas_transform(const Transform2D &p_transform);
	Transform2D get_global_canvas_transform() const;

	Transform2D get_final_transform() const;

	void set_transparent_background(bool p_enable);
	bool has_transparent_background() const;

	void set_size_override(bool p_enable, const Size2 &p_size = Size2(-1, -1), const Vector2 &p_margin = Vector2());
	Size2 get_size_override() const;

	bool is_size_override_enabled() const;
	void set_size_override_stretch(bool p_enable);
	bool is_size_override_stretch_enabled() const;

	void set_vflip(bool p_enable);
	bool get_vflip() const;

	void set_clear_mode(ClearMode p_mode);
	ClearMode get_clear_mode() const;

	void set_update_mode(UpdateMode p_mode);
	UpdateMode get_update_mode() const;
	Ref<ViewportTexture> get_texture() const;

	void set_shadow_atlas_size(int p_size);
	int get_shadow_atlas_size() const;

	void set_shadow_atlas_quadrant_subdiv(int p_quadrant, ShadowAtlasQuadrantSubdiv p_subdiv);
	ShadowAtlasQuadrantSubdiv get_shadow_atlas_quadrant_subdiv(int p_quadrant) const;

	void set_msaa(MSAA p_msaa);
	MSAA get_msaa() const;

	void set_use_fxaa(bool p_fxaa);
	bool get_use_fxaa() const;

	void set_use_debanding(bool p_debanding);
	bool get_use_debanding() const;

	void set_sharpen_intensity(float p_intensity);
	float get_sharpen_intensity() const;

	void set_hdr(bool p_hdr);
	bool get_hdr() const;

	void set_use_32_bpc_depth(bool p_enable);
	bool is_using_32_bpc_depth() const;

	Vector2 get_camera_coords(const Vector2 &p_viewport_coords) const;
	Vector2 get_camera_rect_size() const;

	void set_use_own_world(bool p_use_own_world);
	bool is_using_own_world() const;

	void input(const Ref<InputEvent> &p_event);
	void unhandled_input(const Ref<InputEvent> &p_event);

	void set_disable_input(bool p_disable);
	bool is_input_disabled() const;

	void set_disable_3d(bool p_disable);
	bool is_3d_disabled() const;

	void set_keep_3d_linear(bool p_keep_3d_linear);
	bool get_keep_3d_linear() const;

	void set_attach_to_screen_rect(const Rect2 &p_rect);
	Rect2 get_attach_to_screen_rect() const;

	void set_use_render_direct_to_screen(bool p_render_direct_to_screen);
	bool is_using_render_direct_to_screen() const;

	Vector2 get_mouse_position() const;
	void warp_mouse(const Vector2 &p_pos);

	void set_physics_object_picking(bool p_enable);
	bool get_physics_object_picking();

	bool gui_has_modal_stack() const;

	Variant gui_get_drag_data() const;
	Control *get_modal_stack_top() const;

	void gui_reset_canvas_sort_index();
	int gui_get_canvas_sort_index();

	void gui_release_focus();
	Control *gui_get_focus_owner() const;
	Control *gui_get_hovered_control() const;

	virtual String get_configuration_warning() const;

	void set_usage(Usage p_usage);
	Usage get_usage() const;

	void set_debug_draw(DebugDraw p_debug_draw);
	DebugDraw get_debug_draw() const;

	int get_render_info(RenderInfo p_info);

	void set_snap_controls_to_pixels(bool p_enable);
	bool is_snap_controls_to_pixels_enabled() const;

	void _subwindow_visibility_changed();

	void set_input_as_handled();
	bool is_input_handled() const;

	void set_handle_input_locally(bool p_enable);
	bool is_handling_input_locally() const;

	bool gui_is_dragging() const;
	bool gui_is_drag_successful() const;

	static void notify_canvas_parent_children_moved(Node &r_parent, uint32_t p_first_child, uint32_t p_last_child_plus_one);
	static void notify_canvas_parent_child_count_reduced(const Node &p_parent);
	static void flush_canvas_parents_dirty_order();

	Viewport();
	~Viewport();
};

VARIANT_ENUM_CAST(Viewport::UpdateMode);
VARIANT_ENUM_CAST(Viewport::ShadowAtlasQuadrantSubdiv);
VARIANT_ENUM_CAST(Viewport::MSAA);
VARIANT_ENUM_CAST(Viewport::Usage);
VARIANT_ENUM_CAST(Viewport::DebugDraw);
VARIANT_ENUM_CAST(Viewport::ClearMode);
VARIANT_ENUM_CAST(Viewport::RenderInfo);

#endif // VIEWPORT_H
