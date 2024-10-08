/**************************************************************************/
/*  animation_track_editor.h                                              */
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

#ifndef ANIMATION_TRACK_EDITOR_H
#define ANIMATION_TRACK_EDITOR_H

#include "editor/editor_data.h"
#include "editor/editor_properties.h"
#include "editor/property_selector.h"
#include "scene/3d/node_3d.h"
#include "scene/gui/control.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/scroll_bar.h"
#include "scene/gui/tree.h"
#include "scene/resources/animation.h"

class AnimationMarkerEdit;
class AnimationTrackEditor;
class AnimationTrackEdit;
class CheckBox;
class ColorPickerButton;
class EditorSpinSlider;
class HSlider;
class OptionButton;
class PanelContainer;
class SceneTreeDialog;
class SpinBox;
class TextureRect;
class ViewPanner;
class EditorValidationPanel;

class AnimationTrackKeyEdit : public Object {
	GDCLASS(AnimationTrackKeyEdit, Object);

public:
	bool setting = false;
	bool animation_read_only = false;

	Ref<Animation> animation;
	int track = -1;
	float key_ofs = 0;
	Node *root_path = nullptr;

	PropertyInfo hint;
	NodePath base;
	bool use_fps = false;
	AnimationTrackEditor *editor = nullptr;

	bool _hide_script_from_inspector() { return true; }
	bool _hide_metadata_from_inspector() { return true; }
	bool _dont_undo_redo() { return true; }

	bool _is_read_only() { return animation_read_only; }

	void notify_change();
	Node *get_root_path();
	void set_use_fps(bool p_enable);

protected:
	static void _bind_methods();
	void _fix_node_path(Variant &value);
	void _update_obj(const Ref<Animation> &p_anim);
	void _key_ofs_changed(const Ref<Animation> &p_anim, float from, float to);
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
};

class AnimationMultiTrackKeyEdit : public Object {
	GDCLASS(AnimationMultiTrackKeyEdit, Object);

public:
	bool setting = false;
	bool animation_read_only = false;

	Ref<Animation> animation;

	RBMap<int, List<float>> key_ofs_map;
	RBMap<int, NodePath> base_map;
	PropertyInfo hint;

	Node *root_path = nullptr;

	bool use_fps = false;
	AnimationTrackEditor *editor = nullptr;

	bool _hide_script_from_inspector() { return true; }
	bool _hide_metadata_from_inspector() { return true; }
	bool _dont_undo_redo() { return true; }

	bool _is_read_only() { return animation_read_only; }

	void notify_change();
	Node *get_root_path();
	void set_use_fps(bool p_enable);

protected:
	static void _bind_methods();
	void _fix_node_path(Variant &value, NodePath &base);
	void _update_obj(const Ref<Animation> &p_anim);
	void _key_ofs_changed(const Ref<Animation> &p_anim, float from, float to);
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
};

class AnimationMarkerKeyEdit : public Object {
	GDCLASS(AnimationMarkerKeyEdit, Object);

public:
	bool animation_read_only = false;

	Ref<Animation> animation;
	StringName marker_name;
	bool use_fps = false;

	AnimationMarkerEdit *marker_edit = nullptr;

	bool _hide_script_from_inspector() { return true; }
	bool _hide_metadata_from_inspector() { return true; }
	bool _dont_undo_redo() { return true; }

	bool _is_read_only() { return animation_read_only; }

	float get_time() const;

protected:
	static void _bind_methods();
	void _set_marker_name(const StringName &p_name);
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
};

class AnimationMultiMarkerKeyEdit : public Object {
	GDCLASS(AnimationMultiMarkerKeyEdit, Object);

public:
	bool animation_read_only = false;

	Ref<Animation> animation;
	Vector<StringName> marker_names;

	AnimationMarkerEdit *marker_edit = nullptr;

	bool _hide_script_from_inspector() { return true; }
	bool _hide_metadata_from_inspector() { return true; }
	bool _dont_undo_redo() { return true; }

	bool _is_read_only() { return animation_read_only; }

protected:
	static void _bind_methods();
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
};

class AnimationTimelineEdit : public Range {
	GDCLASS(AnimationTimelineEdit, Range);

	friend class AnimationBezierTrackEdit;
	friend class AnimationTrackEditor;

	static constexpr float SCROLL_ZOOM_FACTOR_IN = 1.02f; // Zoom factor per mouse scroll in the animation editor when zooming in. The closer to 1.0, the finer the control.
	static constexpr float SCROLL_ZOOM_FACTOR_OUT = 0.98f; // Zoom factor when zooming out. Similar to SCROLL_ZOOM_FACTOR_IN but less than 1.0.

	Ref<Animation> animation;
	bool read_only = false;

	AnimationTrackEdit *track_edit = nullptr;
	int name_limit = 0;
	Range *zoom = nullptr;
	Range *h_scroll = nullptr;
	float play_position_pos = 0.0f;

	HBoxContainer *len_hb = nullptr;
	EditorSpinSlider *length = nullptr;
	Button *loop = nullptr;
	TextureRect *time_icon = nullptr;

	MenuButton *add_track = nullptr;
	Control *play_position = nullptr; //separate control used to draw so updates for only position changed are much faster
	HScrollBar *hscroll = nullptr;

	void _zoom_changed(double);
	void _anim_length_changed(double p_new_len);
	void _anim_loop_pressed();

	void _play_position_draw();
	Rect2 hsize_rect;

	bool editing = false;
	bool use_fps = false;

	Ref<ViewPanner> panner;
	void _pan_callback(Vector2 p_scroll_vec, Ref<InputEvent> p_event);
	void _zoom_callback(float p_zoom_factor, Vector2 p_origin, Ref<InputEvent> p_event);

	bool dragging_timeline = false;
	bool dragging_hsize = false;
	float dragging_hsize_from = 0.0f;
	float dragging_hsize_at = 0.0f;
	double last_zoom_scale = 1.0;
	double hscroll_on_zoom_buffer = -1.0;

	Vector2 zoom_scroll_origin;
	bool zoom_callback_occured = false;

	virtual void gui_input(const Ref<InputEvent> &p_event) override;
	void _track_added(int p_track);

	float _get_zoom_scale(double p_zoom_value) const;
	void _scroll_to_start();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	int get_name_limit() const;
	int get_buttons_width() const;

	float get_zoom_scale() const;

	virtual Size2 get_minimum_size() const override;
	void set_animation(const Ref<Animation> &p_animation, bool p_read_only);
	void set_track_edit(AnimationTrackEdit *p_track_edit);
	void set_zoom(Range *p_zoom);
	Range *get_zoom() const { return zoom; }
	void auto_fit();

	void set_play_position(float p_pos);
	float get_play_position() const;
	void update_play_position();

	void update_values();

	void set_use_fps(bool p_use_fps);
	bool is_using_fps() const;

	void set_hscroll(HScrollBar *p_hscroll);

	virtual CursorShape get_cursor_shape(const Point2 &p_pos) const override;

	AnimationTimelineEdit();
};

class AnimationMarkerEdit : public Control {
	GDCLASS(AnimationMarkerEdit, Control);
	friend class AnimationTimelineEdit;

	enum {
		MENU_KEY_INSERT,
		MENU_KEY_RENAME,
		MENU_KEY_DELETE,
		MENU_KEY_TOGGLE_MARKER_NAMES,
	};

	AnimationTimelineEdit *timeline = nullptr;
	Control *play_position = nullptr; // Separate control used to draw so updates for only position changed are much faster.
	float play_position_pos = 0.0f;

	HashSet<StringName> selection;

	Ref<Animation> animation;
	bool read_only = false;

	Ref<Texture2D> type_icon;
	Ref<Texture2D> selected_icon;

	PopupMenu *menu = nullptr;

	bool hovered = false;
	StringName hovering_marker;

	void _zoom_changed();

	Ref<Texture2D> icon_cache;

	void _menu_selected(int p_index);

	void _play_position_draw();
	bool _try_select_at_ui_pos(const Point2 &p_pos, bool p_aggregate, bool p_deselectable);
	bool _is_ui_pos_in_current_section(const Point2 &p_pos);

	float insert_at_pos = 0.0f;
	bool moving_selection_attempt = false;
	bool moving_selection_effective = false;
	float moving_selection_offset = 0.0f;
	float moving_selection_pivot = 0.0f;
	float moving_selection_mouse_begin_x = 0.0f;
	float moving_selection_mouse_begin_y = 0.0f;
	StringName select_single_attempt;
	bool moving_selection = false;
	void _move_selection_begin();
	void _move_selection(float p_offset);
	void _move_selection_commit();
	void _move_selection_cancel();

	void _clear_selection_for_anim(const Ref<Animation> &p_anim);
	void _select_key(const StringName &p_name, bool is_single = false);
	void _deselect_key(const StringName &p_name);

	void _insert_marker(float p_ofs);
	void _rename_marker(const StringName &p_name);
	void _delete_selected_markers();

	ConfirmationDialog *marker_insert_confirm = nullptr;
	LineEdit *marker_insert_new_name = nullptr;
	ColorPickerButton *marker_insert_color = nullptr;
	AcceptDialog *marker_insert_error_dialog = nullptr;
	float marker_insert_ofs = 0;

	ConfirmationDialog *marker_rename_confirm = nullptr;
	LineEdit *marker_rename_new_name = nullptr;
	StringName marker_rename_prev_name;

	AcceptDialog *marker_rename_error_dialog = nullptr;

	bool should_show_all_marker_names = false;

	////////////// edit menu stuff

	void _marker_insert_confirmed();
	void _marker_insert_new_name_changed(const String &p_text);
	void _marker_rename_confirmed();
	void _marker_rename_new_name_changed(const String &p_text);

	AnimationTrackEditor *editor = nullptr;

	HBoxContainer *_create_hbox_labeled_control(const String &p_text, Control *p_control) const;

	void _update_key_edit();
	void _clear_key_edit();

	AnimationMarkerKeyEdit *key_edit = nullptr;
	AnimationMultiMarkerKeyEdit *multi_key_edit = nullptr;

protected:
	static void _bind_methods();
	void _notification(int p_what);

	virtual void gui_input(const Ref<InputEvent> &p_event) override;

public:
	virtual String get_tooltip(const Point2 &p_pos) const override;

	virtual int get_key_height() const;
	virtual Rect2 get_key_rect(float p_pixels_sec) const;
	virtual bool is_key_selectable_by_distance() const;
	virtual void draw_key(const StringName &p_name, float p_pixels_sec, int p_x, bool p_selected, int p_clip_left, int p_clip_right);
	virtual void draw_bg(int p_clip_left, int p_clip_right);
	virtual void draw_fg(int p_clip_left, int p_clip_right);

	Ref<Animation> get_animation() const;
	AnimationTimelineEdit *get_timeline() const { return timeline; }
	AnimationTrackEditor *get_editor() const { return editor; }
	bool is_selection_active() const { return !selection.is_empty(); }
	bool is_moving_selection() const { return moving_selection; }
	float get_moving_selection_offset() const { return moving_selection_offset; }
	void set_animation(const Ref<Animation> &p_animation, bool p_read_only);
	virtual Size2 get_minimum_size() const override;

	void set_timeline(AnimationTimelineEdit *p_timeline);
	void set_editor(AnimationTrackEditor *p_editor);

	void set_play_position(float p_pos);
	void update_play_position();

	void set_use_fps(bool p_use_fps);

	PackedStringArray get_selected_section() const;
	bool is_marker_selected(const StringName &p_marker) const;

	// For use by AnimationTrackEditor.
	void _clear_selection(bool p_update);

	AnimationMarkerEdit();
	~AnimationMarkerEdit();
};

class AnimationTrackEdit : public Control {
	GDCLASS(AnimationTrackEdit, Control);
	friend class AnimationTimelineEdit;

	enum {
		MENU_CALL_MODE_CONTINUOUS,
		MENU_CALL_MODE_DISCRETE,
		MENU_CALL_MODE_CAPTURE,
		MENU_INTERPOLATION_NEAREST,
		MENU_INTERPOLATION_LINEAR,
		MENU_INTERPOLATION_CUBIC,
		MENU_INTERPOLATION_LINEAR_ANGLE,
		MENU_INTERPOLATION_CUBIC_ANGLE,
		MENU_LOOP_WRAP,
		MENU_LOOP_CLAMP,
		MENU_KEY_INSERT,
		MENU_KEY_DUPLICATE,
		MENU_KEY_CUT,
		MENU_KEY_COPY,
		MENU_KEY_PASTE,
		MENU_KEY_ADD_RESET,
		MENU_KEY_DELETE,
		MENU_USE_BLEND_ENABLED,
		MENU_USE_BLEND_DISABLED,
	};

	AnimationTimelineEdit *timeline = nullptr;
	Popup *path_popup = nullptr;
	LineEdit *path = nullptr;
	Node *root = nullptr;
	Control *play_position = nullptr; //separate control used to draw so updates for only position changed are much faster
	float play_position_pos = 0.0f;
	NodePath node_path;

	Ref<Animation> animation;
	bool read_only = false;
	int track = 0;

	Rect2 check_rect;
	Rect2 path_rect;

	Rect2 update_mode_rect;
	Rect2 interp_mode_rect;
	Rect2 loop_wrap_rect;
	Rect2 remove_rect;

	Ref<Texture2D> type_icon;
	Ref<Texture2D> selected_icon;

	PopupMenu *menu = nullptr;

	bool hovered = false;
	bool clicking_on_name = false;
	int hovering_key_idx = -1;

	void _zoom_changed();

	Ref<Texture2D> icon_cache;
	String path_cache;

	void _menu_selected(int p_index);

	void _path_submitted(const String &p_text);
	void _play_position_draw();
	bool _is_value_key_valid(const Variant &p_key_value, Variant::Type &r_valid_type) const;
	bool _try_select_at_ui_pos(const Point2 &p_pos, bool p_aggregate, bool p_deselectable);

	Ref<Texture2D> _get_key_type_icon() const;

	mutable int dropping_at = 0;
	float insert_at_pos = 0.0f;
	bool moving_selection_attempt = false;
	bool moving_selection_effective = false;
	float moving_selection_pivot = 0.0f;
	float moving_selection_mouse_begin_x = 0.0f;
	int select_single_attempt = -1;
	bool moving_selection = false;

	bool in_group = false;
	AnimationTrackEditor *editor = nullptr;

protected:
	static void _bind_methods();
	void _notification(int p_what);

	virtual void gui_input(const Ref<InputEvent> &p_event) override;

public:
	virtual Variant get_drag_data(const Point2 &p_point) override;
	virtual bool can_drop_data(const Point2 &p_point, const Variant &p_data) const override;
	virtual void drop_data(const Point2 &p_point, const Variant &p_data) override;

	virtual String get_tooltip(const Point2 &p_pos) const override;

	virtual int get_key_height() const;
	virtual Rect2 get_key_rect(int p_index, float p_pixels_sec);
	virtual bool is_key_selectable_by_distance() const;
	virtual void draw_key_link(int p_index, float p_pixels_sec, int p_x, int p_next_x, int p_clip_left, int p_clip_right);
	virtual void draw_key(int p_index, float p_pixels_sec, int p_x, bool p_selected, int p_clip_left, int p_clip_right);
	virtual void draw_bg(int p_clip_left, int p_clip_right);
	virtual void draw_fg(int p_clip_left, int p_clip_right);

	//helper
	void draw_texture_region_clipped(const Ref<Texture2D> &p_texture, const Rect2 &p_rect, const Rect2 &p_region);
	void draw_rect_clipped(const Rect2 &p_rect, const Color &p_color, bool p_filled = true);

	int get_track() const;
	Ref<Animation> get_animation() const;
	AnimationTimelineEdit *get_timeline() const { return timeline; }
	AnimationTrackEditor *get_editor() const { return editor; }
	NodePath get_path() const;
	void set_animation_and_track(const Ref<Animation> &p_animation, int p_track, bool p_read_only);
	virtual Size2 get_minimum_size() const override;

	void set_timeline(AnimationTimelineEdit *p_timeline);
	void set_editor(AnimationTrackEditor *p_editor);
	void set_root(Node *p_root);

	void set_play_position(float p_pos);
	void update_play_position();
	void cancel_drop();

	void set_in_group(bool p_enable);
	void append_to_selection(const Rect2 &p_box, bool p_deselection);

	AnimationTrackEdit();
};

class AnimationTrackEditPlugin : public RefCounted {
	GDCLASS(AnimationTrackEditPlugin, RefCounted);

public:
	virtual AnimationTrackEdit *create_value_track_edit(Object *p_object, Variant::Type p_type, const String &p_property, PropertyHint p_hint, const String &p_hint_string, int p_usage);
	virtual AnimationTrackEdit *create_audio_track_edit();
	virtual AnimationTrackEdit *create_animation_track_edit(Object *p_object);
};

class AnimationTrackKeyEdit;
class AnimationMultiTrackKeyEdit;
class AnimationBezierTrackEdit;

class AnimationTrackEditGroup : public Control {
	GDCLASS(AnimationTrackEditGroup, Control);
	Ref<Texture2D> icon;
	Vector2 icon_size;
	String node_name;
	NodePath node;
	Node *root = nullptr;
	AnimationTimelineEdit *timeline = nullptr;
	AnimationTrackEditor *editor = nullptr;

	void _zoom_changed();

protected:
	void _notification(int p_what);

	virtual void gui_input(const Ref<InputEvent> &p_event) override;

public:
	void set_type_and_name(const Ref<Texture2D> &p_type, const String &p_name, const NodePath &p_node);
	virtual Size2 get_minimum_size() const override;
	void set_timeline(AnimationTimelineEdit *p_timeline);
	void set_root(Node *p_root);
	void set_editor(AnimationTrackEditor *p_editor);

	AnimationTrackEditGroup();
};

class AnimationTrackEditor : public VBoxContainer {
	GDCLASS(AnimationTrackEditor, VBoxContainer);
	friend class AnimationTimelineEdit;
	friend class AnimationBezierTrackEdit;
	friend class AnimationMarkerKeyEditEditor;

	Ref<Animation> animation;
	bool read_only = false;
	Node *root = nullptr;

	MenuButton *edit = nullptr;

	PanelContainer *main_panel = nullptr;
	HScrollBar *hscroll = nullptr;
	ScrollContainer *scroll = nullptr;
	VBoxContainer *track_vbox = nullptr;
	AnimationBezierTrackEdit *bezier_edit = nullptr;
	VBoxContainer *timeline_vbox = nullptr;

	Label *info_message = nullptr;

	AnimationTimelineEdit *timeline = nullptr;
	AnimationMarkerEdit *marker_edit = nullptr;
	HSlider *zoom = nullptr;
	EditorSpinSlider *step = nullptr;
	TextureRect *zoom_icon = nullptr;
	Button *snap_keys = nullptr;
	Button *snap_timeline = nullptr;
	Button *bezier_edit_icon = nullptr;
	OptionButton *snap_mode = nullptr;
	Button *auto_fit = nullptr;
	Button *auto_fit_bezier = nullptr;

	Button *imported_anim_warning = nullptr;
	void _show_imported_anim_warning();

	Button *dummy_player_warning = nullptr;
	void _show_dummy_player_warning();

	Button *inactive_player_warning = nullptr;
	void _show_inactive_player_warning();

	void _snap_mode_changed(int p_mode);
	Vector<AnimationTrackEdit *> track_edits;
	Vector<AnimationTrackEditGroup *> groups;

	bool animation_changing_awaiting_update = false;
	void _animation_update(); // Updated by AnimationTrackEditor(this)
	int _get_track_selected();
	void _animation_changed();
	void _update_tracks();
	void _redraw_tracks();
	void _redraw_groups();
	void _check_bezier_exist();

	void _name_limit_changed();
	void _timeline_changed(float p_new_pos, bool p_timeline_only);
	void _track_remove_request(int p_track);
	void _animation_track_remove_request(int p_track, Ref<Animation> p_from_animation);
	void _track_grab_focus(int p_track);

	void _update_scroll(double);
	void _update_step(double p_new_step);
	void _update_length(double p_new_len);
	void _dropped_track(int p_from_track, int p_to_track);

	void _add_track(int p_type);
	void _new_track_node_selected(NodePath p_path);
	void _new_track_property_selected(const String &p_name);

	void _update_step_spinbox();

	PropertySelector *prop_selector = nullptr;
	PropertySelector *method_selector = nullptr;
	SceneTreeDialog *pick_track = nullptr;
	int adding_track_type = 0;
	NodePath adding_track_path;

	bool keying = false;

	struct InsertData {
		Animation::TrackType type;
		NodePath path;
		int track_idx = 0;
		float time = FLT_MAX; // Defaults to current timeline position.
		Variant value;
		String query;
		bool advance = false;
	};

	Label *insert_confirm_text = nullptr;
	CheckBox *insert_confirm_bezier = nullptr;
	CheckBox *insert_confirm_reset = nullptr;
	ConfirmationDialog *insert_confirm = nullptr;
	bool insert_queue = false;
	List<InsertData> insert_data;

	void _query_insert(const InsertData &p_id);
	Ref<Animation> _create_and_get_reset_animation();
	void _confirm_insert_list();
	struct TrackIndices {
		int normal;
		int reset;

		TrackIndices(const Animation *p_anim = nullptr, const Animation *p_reset_anim = nullptr) {
			normal = p_anim ? p_anim->get_track_count() : 0;
			reset = p_reset_anim ? p_reset_anim->get_track_count() : 0;
		}
	};
	TrackIndices _confirm_insert(InsertData p_id, TrackIndices p_next_tracks, bool p_reset_wanted, Ref<Animation> p_reset_anim, bool p_create_beziers);
	void _insert_track(bool p_reset_wanted, bool p_create_beziers);

	void _root_removed();

	PropertyInfo _find_hint_for_track(int p_idx, NodePath &r_base_path, Variant *r_current_val = nullptr);

	void _scroll_changed(const Vector2 &p_val);
	void _v_scroll_changed(float p_val);
	void _h_scroll_changed(float p_val);

	Ref<ViewPanner> panner;
	void _pan_callback(Vector2 p_scroll_vec, Ref<InputEvent> p_event);
	void _zoom_callback(float p_zoom_factor, Vector2 p_origin, Ref<InputEvent> p_event);

	void _timeline_value_changed(double);

	float insert_key_from_track_call_ofs = 0.0f;
	int insert_key_from_track_call_track = 0;
	void _insert_key_from_track(float p_ofs, int p_track);
	void _add_method_key(const String &p_method);

	void _fetch_value_track_options(const NodePath &p_path, Animation::UpdateMode *r_update_mode, Animation::InterpolationType *r_interpolation_type, bool *r_loop_wrap);

	void _clear_selection_for_anim(const Ref<Animation> &p_anim);
	void _select_at_anim(const Ref<Animation> &p_anim, int p_track, float p_pos);

	//selection

	struct SelectedKey {
		int track = 0;
		int key = 0;
		bool operator<(const SelectedKey &p_key) const { return track == p_key.track ? key < p_key.key : track < p_key.track; };
	};

	struct KeyInfo {
		float pos = 0;
	};

	RBMap<SelectedKey, KeyInfo> selection;

	bool moving_selection = false;
	float moving_selection_offset = 0.0f;
	void _move_selection_begin();
	void _move_selection(float p_offset);
	void _move_selection_commit();
	void _move_selection_cancel();

	AnimationTrackKeyEdit *key_edit = nullptr;
	AnimationMultiTrackKeyEdit *multi_key_edit = nullptr;
	void _update_key_edit();
	void _clear_key_edit();

	Control *box_selection_container = nullptr;

	Control *box_selection = nullptr;
	void _box_selection_draw();
	bool box_selecting = false;
	Vector2 box_selecting_from;
	Vector2 box_selecting_to;
	Rect2 box_select_rect;
	Vector2 prev_scroll_position;
	void _scroll_input(const Ref<InputEvent> &p_event);

	Vector<Ref<AnimationTrackEditPlugin>> track_edit_plugins;

	void _toggle_bezier_edit();
	void _cancel_bezier_edit();
	void _bezier_edit(int p_for_track);
	void _bezier_track_set_key_handle_mode(Animation *p_anim, int p_track, int p_index, Animation::HandleMode p_mode, Animation::HandleSetMode p_set_mode = Animation::HANDLE_SET_MODE_NONE);

	////////////// edit menu stuff

	ConfirmationDialog *bake_dialog = nullptr;
	CheckBox *bake_trs = nullptr;
	CheckBox *bake_blendshape = nullptr;
	CheckBox *bake_value = nullptr;
	SpinBox *bake_fps = nullptr;

	ConfirmationDialog *optimize_dialog = nullptr;
	SpinBox *optimize_velocity_error = nullptr;
	SpinBox *optimize_angular_error = nullptr;
	SpinBox *optimize_precision_error = nullptr;

	ConfirmationDialog *cleanup_dialog = nullptr;
	CheckBox *cleanup_keys_with_trimming_head = nullptr;
	CheckBox *cleanup_keys_with_trimming_end = nullptr;
	CheckBox *cleanup_keys = nullptr;
	CheckBox *cleanup_tracks = nullptr;
	CheckBox *cleanup_all = nullptr;

	ConfirmationDialog *scale_dialog = nullptr;
	SpinBox *scale = nullptr;

	ConfirmationDialog *ease_dialog = nullptr;
	OptionButton *transition_selection = nullptr;
	OptionButton *ease_selection = nullptr;
	SpinBox *ease_fps = nullptr;

	void _select_all_tracks_for_copy();

	void _edit_menu_about_to_popup();
	void _edit_menu_pressed(int p_option);
	int last_menu_track_opt = 0;

	void _cleanup_animation(Ref<Animation> p_animation);

	void _anim_duplicate_keys(float p_ofs, bool p_ofs_valid, int p_track);

	void _anim_copy_keys(bool p_cut);

	bool _is_track_compatible(int p_target_track_idx, Variant::Type p_source_value_type, Animation::TrackType p_source_track_type);

	void _anim_paste_keys(float p_ofs, bool p_ofs_valid, int p_track);

	void _view_group_toggle();
	Button *view_group = nullptr;
	Button *selected_filter = nullptr;

	void _auto_fit();
	void _auto_fit_bezier();

	void _selection_changed();

	ConfirmationDialog *track_copy_dialog = nullptr;
	Tree *track_copy_select = nullptr;

	struct TrackClipboard {
		NodePath full_path;
		NodePath base_path;
		Animation::TrackType track_type = Animation::TYPE_ANIMATION;
		Animation::InterpolationType interp_type = Animation::INTERPOLATION_CUBIC_ANGLE;
		Animation::UpdateMode update_mode = Animation::UPDATE_CAPTURE;
		Animation::LoopMode loop_mode = Animation::LOOP_PINGPONG;
		bool loop_wrap = false;
		bool enabled = false;
		bool use_blend = false;

		struct Key {
			float time = 0;
			float transition = 0;
			Variant value;
		};
		Vector<Key> keys;
	};

	struct KeyClipboard {
		int top_track;

		struct Key {
			Animation::TrackType track_type;
			int track;
			float time = 0;
			float transition = 0;
			Variant value;
		};
		Vector<Key> keys;
	};

	Vector<TrackClipboard> track_clipboard;
	KeyClipboard key_clipboard;

	void _set_key_clipboard(int p_top_track, float p_top_time, RBMap<SelectedKey, KeyInfo> &p_keymap);
	void _insert_animation_key(NodePath p_path, const Variant &p_value);

	void _pick_track_filter_text_changed(const String &p_newtext);
	void _pick_track_select_recursive(TreeItem *p_item, const String &p_filter, Vector<Node *> &p_select_candidates);

	double snap_unit;
	void _update_snap_unit();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	// Public for use with callable_mp.
	void _clear_selection(bool p_update = false);
	void _key_selected(int p_key, bool p_single, int p_track);
	void _key_deselected(int p_key, int p_track);

	enum {
		EDIT_COPY_TRACKS,
		EDIT_COPY_TRACKS_CONFIRM,
		EDIT_PASTE_TRACKS,
		EDIT_CUT_KEYS,
		EDIT_COPY_KEYS,
		EDIT_PASTE_KEYS,
		EDIT_SCALE_SELECTION,
		EDIT_SCALE_FROM_CURSOR,
		EDIT_SCALE_CONFIRM,
		EDIT_SET_START_OFFSET,
		EDIT_SET_END_OFFSET,
		EDIT_EASE_SELECTION,
		EDIT_EASE_CONFIRM,
		EDIT_DUPLICATE_SELECTED_KEYS,
		EDIT_DUPLICATE_SELECTION,
		EDIT_DUPLICATE_TRANSPOSED,
		EDIT_MOVE_FIRST_SELECTED_KEY_TO_CURSOR,
		EDIT_MOVE_LAST_SELECTED_KEY_TO_CURSOR,
		EDIT_ADD_RESET_KEY,
		EDIT_DELETE_SELECTION,
		EDIT_GOTO_NEXT_STEP,
		EDIT_GOTO_NEXT_STEP_TIMELINE_ONLY, // Next step without updating animation.
		EDIT_GOTO_PREV_STEP,
		EDIT_APPLY_RESET,
		EDIT_BAKE_ANIMATION,
		EDIT_BAKE_ANIMATION_CONFIRM,
		EDIT_OPTIMIZE_ANIMATION,
		EDIT_OPTIMIZE_ANIMATION_CONFIRM,
		EDIT_CLEAN_UP_ANIMATION,
		EDIT_CLEAN_UP_ANIMATION_CONFIRM
	};

	void add_track_edit_plugin(const Ref<AnimationTrackEditPlugin> &p_plugin);
	void remove_track_edit_plugin(const Ref<AnimationTrackEditPlugin> &p_plugin);

	void set_animation(const Ref<Animation> &p_anim, bool p_read_only);
	Ref<Animation> get_current_animation() const;
	void set_root(Node *p_root);
	Node *get_root() const;
	void update_keying();
	bool has_keying() const;

	Dictionary get_state() const;
	void set_state(const Dictionary &p_state);

	void cleanup();

	void set_anim_pos(float p_pos);
	void insert_node_value_key(Node *p_node, const String &p_property, bool p_only_if_exists = false, bool p_advance = false);
	void insert_value_key(const String &p_property, bool p_advance);
	void insert_transform_key(Node3D *p_node, const String &p_sub, const Animation::TrackType p_type, const Variant &p_value);
	bool has_track(Node3D *p_node, const String &p_sub, const Animation::TrackType p_type);
	void make_insert_queue();
	void commit_insert_queue();

	void show_select_node_warning(bool p_show);
	void show_dummy_player_warning(bool p_show);
	void show_inactive_player_warning(bool p_show);

	bool is_key_selected(int p_track, int p_key) const;
	bool is_selection_active() const;
	bool is_key_clipboard_active() const;
	bool is_moving_selection() const;
	bool is_snap_timeline_enabled() const;
	bool is_snap_keys_enabled() const;
	bool is_bezier_editor_active() const;
	bool can_add_reset_key() const;
	float get_moving_selection_offset() const;
	float snap_time(float p_value, bool p_relative = false);
	bool is_grouping_tracks();
	PackedStringArray get_selected_section() const;
	bool is_marker_selected(const StringName &p_marker) const;
	bool is_marker_moving_selection() const;
	float get_marker_moving_selection_offset() const;

	/** If `p_from_mouse_event` is `true`, handle Shift key presses for precise snapping. */
	void goto_prev_step(bool p_from_mouse_event);

	/** If `p_from_mouse_event` is `true`, handle Shift key presses for precise snapping. */
	void goto_next_step(bool p_from_mouse_event, bool p_timeline_only = false);

	MenuButton *get_edit_menu();
	AnimationTrackEditor();
	~AnimationTrackEditor();
};

// AnimationTrackKeyEditEditorPlugin

class AnimationTrackKeyEditEditor : public EditorProperty {
	GDCLASS(AnimationTrackKeyEditEditor, EditorProperty);

	Ref<Animation> animation;
	int track = -1;
	real_t key_ofs = 0.0;
	bool use_fps = false;

	EditorSpinSlider *spinner = nullptr;

	struct KeyDataCache {
		real_t time = 0.0;
		float transition = 0.0;
		Variant value;
	} key_data_cache;

	void _time_edit_entered();
	void _time_edit_exited();

public:
	AnimationTrackKeyEditEditor(Ref<Animation> p_animation, int p_track, real_t p_key_ofs, bool p_use_fps);
	~AnimationTrackKeyEditEditor();
};

// AnimationMarkerKeyEditEditorPlugin

class AnimationMarkerKeyEditEditor : public EditorProperty {
	GDCLASS(AnimationMarkerKeyEditEditor, EditorProperty);

	Ref<Animation> animation;
	StringName marker_name;
	bool use_fps = false;

	EditorSpinSlider *spinner = nullptr;

	void _time_edit_entered();
	void _time_edit_exited();

public:
	AnimationMarkerKeyEditEditor(Ref<Animation> p_animation, const StringName &p_name, bool p_use_fps);
	~AnimationMarkerKeyEditEditor();
};

#endif // ANIMATION_TRACK_EDITOR_H
