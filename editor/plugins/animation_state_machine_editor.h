/**************************************************************************/
/*  animation_state_machine_editor.h                                      */
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

#ifndef ANIMATION_STATE_MACHINE_EDITOR_H
#define ANIMATION_STATE_MACHINE_EDITOR_H

#include "editor/plugins/animation_tree_editor_plugin.h"
#include "scene/animation/animation_node_state_machine.h"
#include "scene/gui/graph_edit.h"
#include "scene/gui/popup.h"
#include "scene/gui/tree.h"

class ConfirmationDialog;
class EditorFileDialog;
class OptionButton;
class PanelContainer;

class AnimationNodeStateMachineEditor : public AnimationTreeNodeEditorPlugin {
	GDCLASS(AnimationNodeStateMachineEditor, AnimationTreeNodeEditorPlugin);

	Ref<AnimationNodeStateMachine> state_machine;

	bool read_only = false;

	Button *tool_select = nullptr;
	Button *tool_create = nullptr;
	Button *tool_connect = nullptr;
	Popup *name_edit_popup = nullptr;
	LineEdit *name_edit = nullptr;

	HBoxContainer *selection_tools_hb = nullptr;
	Button *tool_erase = nullptr;

	HBoxContainer *transition_tools_hb = nullptr;
	OptionButton *switch_mode = nullptr;
	Button *auto_advance = nullptr;

	OptionButton *play_mode = nullptr;

	PanelContainer *panel = nullptr;

	StringName selected_node;
	HashSet<StringName> selected_nodes;

	HScrollBar *h_scroll = nullptr;
	VScrollBar *v_scroll = nullptr;

	Control *state_machine_draw = nullptr;
	Control *state_machine_play_pos = nullptr;

	PanelContainer *error_panel = nullptr;
	Label *error_label = nullptr;

	struct ThemeCache {
		Ref<StyleBox> panel_style;
		Ref<StyleBox> error_panel_style;
		Color error_color;

		Ref<Texture2D> tool_icon_select;
		Ref<Texture2D> tool_icon_create;
		Ref<Texture2D> tool_icon_connect;
		Ref<Texture2D> tool_icon_erase;

		Ref<Texture2D> transition_icon_immediate;
		Ref<Texture2D> transition_icon_sync;
		Ref<Texture2D> transition_icon_end;

		Ref<Texture2D> play_icon_start;
		Ref<Texture2D> play_icon_travel;
		Ref<Texture2D> play_icon_auto;

		Ref<Texture2D> animation_icon;

		Ref<StyleBox> node_frame;
		Ref<StyleBox> node_frame_selected;
		Ref<StyleBox> node_frame_playing;
		Ref<StyleBox> node_frame_start;
		Ref<StyleBox> node_frame_end;

		Ref<Font> node_title_font;
		int node_title_font_size = 0;
		Color node_title_font_color;

		Ref<Texture2D> play_node;
		Ref<Texture2D> edit_node;

		Color transition_color;
		Color transition_disabled_color;
		Color transition_icon_color;
		Color transition_icon_disabled_color;
		Color highlight_color;
		Color highlight_disabled_color;
		Color focus_color;
		Color guideline_color;

		Ref<Texture2D> transition_icons[6]{};

		Color playback_color;
		Color playback_background_color;
	} theme_cache;

	bool updating = false;

	static AnimationNodeStateMachineEditor *singleton;

	void _state_machine_gui_input(const Ref<InputEvent> &p_event);
	void _connection_draw(const Vector2 &p_from, const Vector2 &p_to, AnimationNodeStateMachineTransition::SwitchMode p_mode, bool p_enabled, bool p_selected, bool p_travel, float p_fade_ratio, bool p_auto_advance, bool p_is_across_group, float p_opacity = 1.0);

	void _state_machine_draw();

	void _state_machine_pos_draw_individual(const String &p_name, float p_ratio);
	void _state_machine_pos_draw_all();

	void _update_graph();

	PopupMenu *menu = nullptr;
	PopupMenu *connect_menu = nullptr;
	PopupMenu *state_machine_menu = nullptr;
	PopupMenu *end_menu = nullptr;
	PopupMenu *animations_menu = nullptr;
	Vector<String> animations_to_add;
	Vector<String> nodes_to_connect;

	Vector2 add_node_pos;

	ConfirmationDialog *delete_window = nullptr;
	Tree *delete_tree = nullptr;

	bool box_selecting = false;
	Point2 box_selecting_from;
	Point2 box_selecting_to;
	Rect2 box_selecting_rect;
	HashSet<StringName> previous_selected;

	bool dragging_selected_attempt = false;
	bool dragging_selected = false;
	Vector2 drag_from;
	Vector2 drag_ofs;
	StringName snap_x;
	StringName snap_y;

	bool connecting = false;
	bool connection_follows_cursor = false;
	StringName connecting_from;
	Vector2 connecting_to;
	StringName connecting_to_node;

	void _add_menu_type(int p_index);
	void _add_animation_type(int p_index);
	void _connect_to(int p_index);

	struct NodeRect {
		StringName node_name;
		Rect2 node;
		Rect2 play;
		Rect2 name;
		Rect2 edit;
		bool can_edit;
	};

	Vector<NodeRect> node_rects;

	struct TransitionLine {
		StringName from_node;
		StringName to_node;
		Vector2 from;
		Vector2 to;
		AnimationNodeStateMachineTransition::SwitchMode mode;
		StringName advance_condition_name;
		bool advance_condition_state = false;
		bool disabled = false;
		bool auto_advance = false;
		float width = 0;
		bool selected;
		bool travel;
		float fade_ratio;
		bool hidden;
		int transition_index;
		bool is_across_group = false;
	};

	Vector<TransitionLine> transition_lines;

	struct NodeUR {
		StringName name;
		Ref<AnimationNode> node;
		Vector2 position;
	};

	struct TransitionUR {
		StringName new_from;
		StringName new_to;
		StringName old_from;
		StringName old_to;
		Ref<AnimationNodeStateMachineTransition> transition;
	};

	StringName selected_transition_from;
	StringName selected_transition_to;
	int selected_transition_index = -1;
	void _add_transition(const bool p_nested_action = false);

	enum HoveredNodeArea {
		HOVER_NODE_NONE = -1,
		HOVER_NODE_PLAY = 0,
		HOVER_NODE_EDIT = 1,
	};

	StringName hovered_node_name;
	HoveredNodeArea hovered_node_area = HOVER_NODE_NONE;

	String prev_name;
	void _name_edited(const String &p_text);
	void _name_edited_focus_out();
	void _open_editor(const String &p_name);
	void _scroll_changed(double);

	String _get_root_playback_path(String &r_node_directory);

	void _clip_src_line_to_rect(Vector2 &r_from, const Vector2 &p_to, const Rect2 &p_rect);
	void _clip_dst_line_to_rect(const Vector2 &p_from, Vector2 &r_to, const Rect2 &p_rect);

	void _erase_selected(const bool p_nested_action = false);
	void _update_mode();
	void _open_menu(const Vector2 &p_position);
	bool _create_submenu(PopupMenu *p_menu, Ref<AnimationNodeStateMachine> p_nodesm, const StringName &p_name, const StringName &p_path);
	void _stop_connecting();

	void _delete_selected();
	void _delete_all();
	void _delete_tree_draw();

	bool last_active = false;
	StringName last_fading_from_node;
	StringName last_current_node;
	Vector<StringName> last_travel_path;

	float fade_from_last_play_pos = 0.0f;
	float fade_from_current_play_pos = 0.0f;
	float fade_from_length = 0.0f;

	float last_play_pos = 0.0f;
	float current_play_pos = 0.0f;
	float current_length = 0.0f;

	float last_fading_time = 0.0f;
	float last_fading_pos = 0.0f;
	float fading_time = 0.0f;
	float fading_pos = 0.0f;

	float error_time = 0.0f;
	String error_text;

	EditorFileDialog *open_file = nullptr;
	Ref<AnimationNode> file_loaded;
	void _file_opened(const String &p_file);

	enum {
		MENU_LOAD_FILE = 1000,
		MENU_PASTE = 1001,
		MENU_LOAD_FILE_CONFIRM = 1002
	};

	HashSet<StringName> connected_nodes;
	void _update_connected_nodes(const StringName &p_node);

	Ref<StyleBox> _adjust_stylebox_opacity(Ref<StyleBox> p_style, float p_opacity);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	static AnimationNodeStateMachineEditor *get_singleton() { return singleton; }

	virtual bool can_edit(const Ref<AnimationNode> &p_node) override;
	virtual void edit(const Ref<AnimationNode> &p_node) override;

	virtual CursorShape get_cursor_shape(const Point2 &p_pos) const override;
	virtual String get_tooltip(const Point2 &p_pos) const override;

	AnimationNodeStateMachineEditor();
};

class EditorAnimationMultiTransitionEdit : public RefCounted {
	GDCLASS(EditorAnimationMultiTransitionEdit, RefCounted);

	struct Transition {
		StringName from;
		StringName to;
		Ref<AnimationNodeStateMachineTransition> transition;
	};

	Vector<Transition> transitions;

protected:
	bool _set(const StringName &p_name, const Variant &p_property);
	bool _get(const StringName &p_name, Variant &r_property) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:
	void add_transition(const StringName &p_from, const StringName &p_to, Ref<AnimationNodeStateMachineTransition> p_transition);

	EditorAnimationMultiTransitionEdit() {}
};

#endif // ANIMATION_STATE_MACHINE_EDITOR_H
