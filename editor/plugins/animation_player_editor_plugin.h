/**************************************************************************/
/*  animation_player_editor_plugin.h                                      */
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

#ifndef ANIMATION_PLAYER_EDITOR_PLUGIN_H
#define ANIMATION_PLAYER_EDITOR_PLUGIN_H

#include "editor/animation_track_editor.h"
#include "editor/plugins/animation_library_editor.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/animation/animation_player.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/slider.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/texture_button.h"
#include "scene/gui/tree.h"

class AnimationPlayerEditorPlugin;
class ImageTexture;

class AnimationPlayerEditor : public VBoxContainer {
	GDCLASS(AnimationPlayerEditor, VBoxContainer);

	friend AnimationPlayerEditorPlugin;

	AnimationPlayerEditorPlugin *plugin = nullptr;
	AnimationMixer *original_node = nullptr; // For pinned mark in SceneTree.
	AnimationPlayer *player = nullptr; // For AnimationPlayerEditor, could be dummy.
	ObjectID cached_root_node_id;
	bool is_dummy = false;

	enum {
		TOOL_NEW_ANIM,
		TOOL_ANIM_LIBRARY,
		TOOL_DUPLICATE_ANIM,
		TOOL_RENAME_ANIM,
		TOOL_EDIT_TRANSITIONS,
		TOOL_REMOVE_ANIM,
		TOOL_EDIT_RESOURCE
	};

	enum {
		ONION_SKINNING_ENABLE,
		ONION_SKINNING_PAST,
		ONION_SKINNING_FUTURE,
		ONION_SKINNING_1_STEP,
		ONION_SKINNING_2_STEPS,
		ONION_SKINNING_3_STEPS,
		ONION_SKINNING_LAST_STEPS_OPTION = ONION_SKINNING_3_STEPS,
		ONION_SKINNING_DIFFERENCES_ONLY,
		ONION_SKINNING_FORCE_WHITE_MODULATE,
		ONION_SKINNING_INCLUDE_GIZMOS,
	};

	enum {
		ANIM_OPEN,
		ANIM_SAVE,
		ANIM_SAVE_AS
	};

	enum {
		RESOURCE_LOAD,
		RESOURCE_SAVE
	};

	OptionButton *animation = nullptr;
	Button *stop = nullptr;
	Button *play = nullptr;
	Button *play_from = nullptr;
	Button *play_bw = nullptr;
	Button *play_bw_from = nullptr;
	Button *autoplay = nullptr;

	MenuButton *tool_anim = nullptr;
	Button *onion_toggle = nullptr;
	MenuButton *onion_skinning = nullptr;
	Button *pin = nullptr;
	SpinBox *frame = nullptr;
	LineEdit *scale = nullptr;
	LineEdit *name = nullptr;
	OptionButton *library = nullptr;
	Label *name_title = nullptr;

	Ref<Texture2D> stop_icon;
	Ref<Texture2D> pause_icon;
	Ref<Texture2D> autoplay_icon;
	Ref<Texture2D> reset_icon;
	Ref<ImageTexture> autoplay_reset_icon;

	bool last_active = false;
	float timeline_position = 0;

	EditorFileDialog *file = nullptr;
	ConfirmationDialog *delete_dialog = nullptr;

	AnimationLibraryEditor *library_editor = nullptr;

	struct BlendEditor {
		AcceptDialog *dialog = nullptr;
		Tree *tree = nullptr;
		OptionButton *next = nullptr;

	} blend_editor;

	ConfirmationDialog *name_dialog = nullptr;
	AcceptDialog *error_dialog = nullptr;
	int name_dialog_op = TOOL_NEW_ANIM;

	bool updating = false;
	bool updating_blends = false;

	AnimationTrackEditor *track_editor = nullptr;
	static AnimationPlayerEditor *singleton;

	// Onion skinning.
	struct {
		// Settings.
		bool enabled = false;
		bool past = true;
		bool future = false;
		uint32_t steps = 1;
		bool differences_only = false;
		bool force_white_modulate = false;
		bool include_gizmos = false;

		uint32_t get_capture_count() const {
			// 'Differences only' needs a capture of the present.
			return (past && future ? 2 * steps : steps) + (differences_only ? 1 : 0);
		}

		// Rendering.
		int64_t last_frame = 0;
		int can_overlay = 0;
		Size2 capture_size;
		LocalVector<RID> captures;
		LocalVector<bool> captures_valid;
		struct {
			RID canvas;
			RID canvas_item;
			Ref<ShaderMaterial> material;
			Ref<Shader> shader;
		} capture;

		// Cross-call state.
		struct {
			double anim_player_position = 0.0;
			Ref<AnimatedValuesBackup> anim_values_backup;
			Rect2 screen_rect;
			Dictionary canvas_edit_state;
			Dictionary spatial_edit_state;
		} temp;
	} onion;

	void _select_anim_by_name(const String &p_anim);
	float _get_editor_step() const;
	void _go_to_nearest_keyframe(bool p_backward);
	void _play_pressed();
	void _play_from_pressed();
	void _play_bw_pressed();
	void _play_bw_from_pressed();
	void _autoplay_pressed();
	void _stop_pressed();
	void _animation_selected(int p_which);
	void _animation_new();
	void _animation_rename();
	void _animation_name_edited();

	void _animation_remove();
	void _animation_remove_confirmed();
	void _animation_edit();
	void _animation_duplicate();
	Ref<Animation> _animation_clone(const Ref<Animation> p_anim);
	void _animation_resource_edit();
	void _scale_changed(const String &p_scale);
	void _seek_value_changed(float p_value, bool p_timeline_only = false);
	void _blend_editor_next_changed(const int p_idx);

	void _edit_animation_blend();
	void _update_animation_blend();

	void _list_changed();
	void _current_animation_changed(const String &p_name);
	void _update_animation();
	void _update_player();
	void _set_controls_disabled(bool p_disabled);
	void _update_animation_list_icons();
	void _update_name_dialog_library_dropdown();
	void _blend_edited();

	void _animation_player_changed(Object *p_pl);
	void _animation_libraries_updated();

	void _animation_key_editor_seek(float p_pos, bool p_timeline_only = false, bool p_update_position_only = false);
	void _animation_key_editor_anim_len_changed(float p_len);
	void _animation_update_key_frame();

	virtual void shortcut_input(const Ref<InputEvent> &p_ev) override;
	void _animation_tool_menu(int p_option);
	void _onion_skinning_menu(int p_option);

	void _editor_visibility_changed();
	bool _are_onion_layers_valid();
	void _allocate_onion_layers();
	void _free_onion_layers();
	void _prepare_onion_layers_1();
	void _prepare_onion_layers_2_prolog();
	void _prepare_onion_layers_2_step_prepare(int p_step_offset, uint32_t p_capture_idx);
	void _prepare_onion_layers_2_step_capture(int p_step_offset, uint32_t p_capture_idx);
	void _prepare_onion_layers_2_epilog();
	void _start_onion_skinning();
	void _stop_onion_skinning();

	bool _validate_tracks(const Ref<Animation> p_anim);

	void _pin_pressed();
	String _get_current() const;

	void _ensure_dummy_player();

	~AnimationPlayerEditor();

protected:
	void _notification(int p_what);
	void _node_removed(Node *p_node);
	static void _bind_methods();

public:
	AnimationMixer *get_editing_node() const;
	AnimationPlayer *get_player() const;
	AnimationMixer *fetch_mixer_for_library() const;
	Node *get_cached_root_node() const;

	static AnimationPlayerEditor *get_singleton() { return singleton; }

	bool is_pinned() const { return pin->is_pressed(); }
	void unpin() {
		pin->set_pressed(false);
		_pin_pressed();
	}
	AnimationTrackEditor *get_track_editor() { return track_editor; }
	Dictionary get_state() const;
	void set_state(const Dictionary &p_state);

	void ensure_visibility();

	void edit(AnimationMixer *p_node, AnimationPlayer *p_player, bool p_is_dummy);
	void forward_force_draw_over_viewport(Control *p_overlay);

	AnimationPlayerEditor(AnimationPlayerEditorPlugin *p_plugin);
};

class AnimationPlayerEditorPlugin : public EditorPlugin {
	GDCLASS(AnimationPlayerEditorPlugin, EditorPlugin);

	friend AnimationPlayerEditor;

	AnimationPlayerEditor *anim_editor = nullptr;
	AnimationPlayer *player = nullptr;
	AnimationPlayer *dummy_player = nullptr;
	ObjectID last_mixer;

	void _update_dummy_player(AnimationMixer *p_mixer);
	void _clear_dummy_player();

protected:
	void _notification(int p_what);

	void _property_keyed(const String &p_keyed, const Variant &p_value, bool p_advance);
	void _transform_key_request(Object *sp, const String &p_sub, const Transform3D &p_key);
	void _update_keying();

public:
	virtual Dictionary get_state() const override { return anim_editor->get_state(); }
	virtual void set_state(const Dictionary &p_state) override { anim_editor->set_state(p_state); }

	virtual String get_name() const override { return "Anim"; }
	bool has_main_screen() const override { return false; }
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;

	virtual void forward_canvas_force_draw_over_viewport(Control *p_overlay) override { anim_editor->forward_force_draw_over_viewport(p_overlay); }
	virtual void forward_3d_force_draw_over_viewport(Control *p_overlay) override { anim_editor->forward_force_draw_over_viewport(p_overlay); }

	AnimationPlayerEditorPlugin();
	~AnimationPlayerEditorPlugin();
};

// AnimationTrackKeyEditEditorPlugin

class EditorInspectorPluginAnimationTrackKeyEdit : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginAnimationTrackKeyEdit, EditorInspectorPlugin);

	AnimationTrackKeyEditEditor *atk_editor = nullptr;

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class AnimationTrackKeyEditEditorPlugin : public EditorPlugin {
	GDCLASS(AnimationTrackKeyEditEditorPlugin, EditorPlugin);

	EditorInspectorPluginAnimationTrackKeyEdit *atk_plugin = nullptr;

public:
	bool has_main_screen() const override { return false; }
	virtual bool handles(Object *p_object) const override;

	virtual String get_name() const override { return "AnimationTrackKeyEdit"; }

	AnimationTrackKeyEditEditorPlugin();
};

// AnimationMarkerKeyEditEditorPlugin

class EditorInspectorPluginAnimationMarkerKeyEdit : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginAnimationMarkerKeyEdit, EditorInspectorPlugin);

	AnimationMarkerKeyEditEditor *amk_editor = nullptr;

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class AnimationMarkerKeyEditEditorPlugin : public EditorPlugin {
	GDCLASS(AnimationMarkerKeyEditEditorPlugin, EditorPlugin);

	EditorInspectorPluginAnimationMarkerKeyEdit *amk_plugin = nullptr;

public:
	bool has_main_screen() const override { return false; }
	virtual bool handles(Object *p_object) const override;

	virtual String get_name() const override { return "AnimationMarkerKeyEdit"; }

	AnimationMarkerKeyEditEditorPlugin();
};

#endif // ANIMATION_PLAYER_EDITOR_PLUGIN_H
