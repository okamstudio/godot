/**************************************************************************/
/*  game_view_plugin.h                                                    */
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

#ifndef GAME_VIEW_PLUGIN_H
#define GAME_VIEW_PLUGIN_H

#include "editor/debugger/editor_debugger_node.h"
#include "editor/plugins/editor_debugger_plugin.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/debugger/scene_debugger.h"
#include "scene/gui/box_container.h"

class EmbeddedProcess;
class VSeparator;
class WindowWrapper;

class GameViewDebugger : public EditorDebuggerPlugin {
	GDCLASS(GameViewDebugger, EditorDebuggerPlugin);

private:
	Vector<Ref<EditorDebuggerSession>> sessions;

	bool is_feature_enabled = true;
	int node_type = RuntimeNodeSelect::NODE_TYPE_NONE;
	bool selection_visible = true;
	int select_mode = RuntimeNodeSelect::SELECT_MODE_SINGLE;
	EditorDebuggerNode::CameraOverride camera_override_mode = EditorDebuggerNode::OVERRIDE_INGAME;

	void _session_started(Ref<EditorDebuggerSession> p_session);
	void _session_stopped();

protected:
	static void _bind_methods();

public:
	void set_is_feature_enabled(bool p_enabled);

	void set_suspend(bool p_enabled);
	void next_frame();

	void set_node_type(int p_type);
	void set_select_mode(int p_mode);

	void set_selection_visible(bool p_visible);

	void set_camera_override(bool p_enabled);
	void set_camera_manipulate_mode(EditorDebuggerNode::CameraOverride p_mode);

	void reset_camera_2d_position();
	void reset_camera_3d_position();

	virtual void setup_session(int p_session_id) override;

	GameViewDebugger() {}
};

class GameView : public VBoxContainer {
	GDCLASS(GameView, VBoxContainer);

	enum {
		CAMERA_RESET_2D,
		CAMERA_RESET_3D,
		CAMERA_MODE_INGAME,
		CAMERA_MODE_EDITORS,
		EMBED_RUN_GAME_EMBEDDED,
		EMBED_MAKE_FLOATING_ON_PLAY,
	};

	inline static GameView *singleton = nullptr;

	Ref<GameViewDebugger> debugger;
	WindowWrapper *window_wrapper = nullptr;

	bool is_feature_enabled = true;
	int active_sessions = 0;
	int screen_index_before_start = -1;

	bool embed_on_play = true;
	bool make_floating_on_play = true;

	Rect2i floating_window_rect;
	int floating_window_screen = -1;
	Rect2i floating_window_screen_rect;

	Button *suspend_button = nullptr;
	Button *next_frame_button = nullptr;

	Button *node_type_button[RuntimeNodeSelect::NODE_TYPE_MAX];
	Button *select_mode_button[RuntimeNodeSelect::SELECT_MODE_MAX];

	Button *hide_selection = nullptr;

	Button *camera_override_button = nullptr;
	MenuButton *camera_override_menu = nullptr;

	VSeparator *embedding_separator = nullptr;
	Button *keep_aspect_button = nullptr;
	MenuButton *embed_options_menu = nullptr;
	Label *game_size_label = nullptr;

	Panel *panel = nullptr;
	EmbeddedProcess *embedded_process = nullptr;
	Label *state_label = nullptr;

	void _sessions_changed();

	void _update_debugger_buttons();

	void _suspend_button_toggled(bool p_pressed);

	void _node_type_pressed(int p_option);
	void _select_mode_pressed(int p_option);
	void _embed_options_menu_menu_id_pressed(int p_id);
	void _keep_aspect_button_pressed();

	void _play_pressed();
	static void _instance_starting_static(int p_idx, List<String> &r_arguments);
	void _instance_starting(int p_idx, List<String> &r_arguments);
	void _stop_pressed();
	void _embedding_completed();
	void _embedding_failed();
	void _embedded_process_updated();
	void _embedded_process_focused();
	void _project_settings_changed();

	void _update_ui();
	void _update_embed_menu_options();
	void _update_embed_window_size();
	void _update_arguments_for_instance(int p_idx, List<String> &r_arguments);

	void _hide_selection_toggled(bool p_pressed);

	void _camera_override_button_toggled(bool p_pressed);
	void _camera_override_menu_id_pressed(int p_id);

	void _window_before_closing();
	void _update_floating_window_settings();

protected:
	void _notification(int p_what);

public:
	void set_is_feature_enabled(bool p_enabled);

	void set_state(const Dictionary &p_state);
	Dictionary get_state() const;

	void set_window_layout(Ref<ConfigFile> p_layout);
	void get_window_layout(Ref<ConfigFile> p_layout);

	GameView(Ref<GameViewDebugger> p_debugger, WindowWrapper *p_wrapper);
};

class GameViewPlugin : public EditorPlugin {
	GDCLASS(GameViewPlugin, EditorPlugin);

	GameView *game_view = nullptr;
	WindowWrapper *window_wrapper = nullptr;

	Ref<GameViewDebugger> debugger;

	String last_editor;

	void _feature_profile_changed();
	void _window_visibility_changed(bool p_visible);
	void _save_last_editor(const String &p_editor);
	void _focus_another_editor();

protected:
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "Game"; }
	bool has_main_screen() const override { return true; }
	virtual void edit(Object *p_object) override {}
	virtual bool handles(Object *p_object) const override { return false; }
	virtual void make_visible(bool p_visible) override;
	virtual void selected_notify() override;

	virtual void set_window_layout(Ref<ConfigFile> p_layout) override;
	virtual void get_window_layout(Ref<ConfigFile> p_layout) override;

	virtual void set_state(const Dictionary &p_state) override;
	virtual Dictionary get_state() const override;

	GameViewPlugin();
	~GameViewPlugin();
};

#endif // GAME_VIEW_PLUGIN_H
