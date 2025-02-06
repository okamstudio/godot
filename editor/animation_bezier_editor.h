/**************************************************************************/
/*  animation_bezier_editor.h                                             */
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

#pragma once

#include "animation_track_editor.h"
#include "core/templates/hashfuncs.h"

class ViewPanner;

class AnimationBezierTrackEdit : public Control {
	GDCLASS(AnimationBezierTrackEdit, Control);

	enum {
		MENU_KEY_INSERT,
		MENU_KEY_DUPLICATE,
		MENU_KEY_CUT,
		MENU_KEY_COPY,
		MENU_KEY_PASTE,
		MENU_KEY_DELETE,
		MENU_KEY_SET_HANDLE_FREE,
		MENU_KEY_SET_HANDLE_LINEAR,
		MENU_KEY_SET_HANDLE_BALANCED,
		MENU_KEY_SET_HANDLE_MIRRORED,
		MENU_KEY_SET_HANDLE_AUTO_BALANCED,
		MENU_KEY_SET_HANDLE_AUTO_MIRRORED,
	};

	AnimationTimelineEdit *timeline = nullptr;
	Node *root = nullptr;
	Control *play_position = nullptr; //separate control used to draw so updates for only position changed are much faster
	real_t play_position_pos = 0;

	Ref<Animation> animation;
	bool read_only = false;
	int selected_track = 0;

	Vector<Rect2> view_rects;

	Ref<Texture2D> bezier_icon;
	Ref<Texture2D> bezier_handle_icon;
	Ref<Texture2D> selected_icon;

	RBMap<int, Rect2> subtracks;

	enum {
		REMOVE_ICON,
		LOCK_ICON,
		SOLO_ICON,
		VISIBILITY_ICON
	};

	RBMap<int, RBMap<int, Rect2>> subtrack_icons;
	HashSet<int> locked_tracks;
	HashSet<int> hidden_tracks;
	int solo_track = -1;
	bool is_filtered = false;

	float track_v_scroll = 0;
	float track_v_scroll_max = 0;

	float timeline_v_scroll = 0;
	float timeline_v_zoom = 1;

	PopupMenu *menu = nullptr;

	void _zoom_changed();

	void _update_locked_tracks_after(int p_track);
	void _update_hidden_tracks_after(int p_track);

	virtual void gui_input(const Ref<InputEvent> &p_event) override;
	void _menu_selected(int p_index);

	void _play_position_draw();
	bool _is_track_displayed(int p_track_index);
	bool _is_track_curves_displayed(int p_track_index);

	Vector2 insert_at_pos;

	typedef Pair<int, int> IntPair;

	bool moving_selection_attempt = false;
	float moving_selection_mouse_begin_x = 0.0;
	IntPair select_single_attempt;
	bool moving_selection = false;
	int moving_selection_from_key = 0;
	int moving_selection_from_track = 0;

	Vector2 moving_selection_offset;

	bool box_selecting_attempt = false;
	bool box_selecting = false;
	bool box_selecting_add = false;
	Vector2 box_selection_from;
	Vector2 box_selection_to;

	int moving_handle = 0; //0 no move -1 or +1 out, 2 both (drawing only)
	int moving_handle_key = 0;
	int moving_handle_track = 0;
	Vector2 moving_handle_left;
	Vector2 moving_handle_right;
	int moving_handle_mode = 0; // value from Animation::HandleMode

	struct PairHasher {
		static _FORCE_INLINE_ uint32_t hash(const Pair<int, int> &p_value) {
			int32_t hash = 23;
			hash = hash * 31 * hash_one_uint64(p_value.first);
			hash = hash * 31 * hash_one_uint64(p_value.second);
			return hash;
		}
	};

	HashMap<Pair<int, int>, Vector2, PairHasher> additional_moving_handle_lefts;
	HashMap<Pair<int, int>, Vector2, PairHasher> additional_moving_handle_rights;

	void _clear_selection();
	void _clear_selection_for_anim(const Ref<Animation> &p_anim);
	void _select_at_anim(const Ref<Animation> &p_anim, int p_track, real_t p_pos, bool p_single);
	bool _try_select_at_ui_pos(const Point2 &p_pos, bool p_aggregate, bool p_deselectable);
	void _change_selected_keys_handle_mode(Animation::HandleMode p_mode, bool p_auto = false);

	Vector2 menu_insert_key;

	struct AnimMoveRestore {
		int track = 0;
		double time = 0;
		Variant key;
		real_t transition = 0;
	};

	AnimationTrackEditor *editor = nullptr;

	struct EditPoint {
		Rect2 point_rect;
		Rect2 in_rect;
		Rect2 out_rect;
		int track = 0;
		int key = 0;
	};

	Vector<EditPoint> edit_points;

	struct PairCompare {
		bool operator()(const IntPair &lh, const IntPair &rh) {
			if (lh.first == rh.first) {
				return lh.second < rh.second;
			} else {
				return lh.first < rh.first;
			}
		}
	};

	typedef RBSet<IntPair, PairCompare> SelectionSet;

	SelectionSet selection;

	Ref<ViewPanner> panner;
	void _pan_callback(Vector2 p_scroll_vec, Ref<InputEvent> p_event);
	void _zoom_callback(float p_zoom_factor, Vector2 p_origin, Ref<InputEvent> p_event);

	void _draw_line_clipped(const Vector2 &p_from, const Vector2 &p_to, const Color &p_color, int p_clip_left, int p_clip_right);
	void _draw_track(int p_track, const Color &p_color);

	float _bezier_h_to_pixel(float p_h);
	void _zoom_vertically(real_t p_minimum_value, real_t p_maximum_value);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	static float get_bezier_key_value(Array p_bezier_key_array);

	virtual String get_tooltip(const Point2 &p_pos) const override;

	Ref<Animation> get_animation() const;

	void set_animation_and_track(const Ref<Animation> &p_animation, int p_track, bool p_read_only);
	virtual Size2 get_minimum_size() const override;

	void set_timeline(AnimationTimelineEdit *p_timeline);
	void set_editor(AnimationTrackEditor *p_editor);
	void set_root(Node *p_root);
	void set_filtered(bool p_filtered);
	void auto_fit_vertically();

	void set_play_position(real_t p_pos);
	void update_play_position();

	void duplicate_selected_keys(real_t p_ofs, bool p_ofs_valid);
	void copy_selected_keys(bool p_cut);
	void paste_keys(real_t p_ofs, bool p_ofs_valid);
	void delete_selection();

	void _bezier_track_insert_key_at_anim(const Ref<Animation> &p_anim, int p_track, double p_time, real_t p_value, const Vector2 &p_in_handle, const Vector2 &p_out_handle, const Animation::HandleMode p_handle_mode);

	AnimationBezierTrackEdit();
};
