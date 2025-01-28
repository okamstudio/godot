/**************************************************************************/
/*  sprite_frames_editor_plugin.h                                         */
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

#ifndef SPRITE_FRAMES_EDITOR_PLUGIN_H
#define SPRITE_FRAMES_EDITOR_PLUGIN_H

#include "editor/plugins/editor_plugin.h"
#include "scene/gui/button.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/item_list.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/split_container.h"
#include "scene/gui/texture_rect.h"
#include "scene/gui/tree.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/sprite_frames.h"

class OptionButton;
class EditorFileDialog;

class ClipboardSpriteFrames : public Resource {
	GDCLASS(ClipboardSpriteFrames, Resource);

public:
	struct Frame {
		Ref<Texture2D> texture;
		float duration;
	};
	Vector<Frame> frames;
};

class SpriteFramesEditor : public HSplitContainer {
	GDCLASS(SpriteFramesEditor, HSplitContainer);

	Ref<SpriteFrames> frames;
	Node *animated_sprite = nullptr;

	enum {
		PARAM_USE_CURRENT, // Used in callbacks to indicate `dominant_param` should be not updated.
		PARAM_FRAME_COUNT, // Keep "Horizontal" & "Vertical" values.
		PARAM_SIZE, // Keep "Size" values.
	};
	int dominant_param = PARAM_FRAME_COUNT;

	enum {
		FRAME_ORDER_SELECTION, // Order frames were selected in.

		// By Row.
		FRAME_ORDER_LEFT_RIGHT_TOP_BOTTOM,
		FRAME_ORDER_LEFT_RIGHT_BOTTOM_TOP,
		FRAME_ORDER_RIGHT_LEFT_TOP_BOTTOM,
		FRAME_ORDER_RIGHT_LEFT_BOTTOM_TOP,

		// By Column.
		FRAME_ORDER_TOP_BOTTOM_LEFT_RIGHT,
		FRAME_ORDER_TOP_BOTTOM_RIGHT_LEFT,
		FRAME_ORDER_BOTTOM_TOP_LEFT_RIGHT,
		FRAME_ORDER_BOTTOM_TOP_RIGHT_LEFT,
	};

	enum {
		MENU_SHOW_IN_FILESYSTEM,
	};

	int right_clicked_frame = -1;

	bool read_only = false;

	Ref<Texture2D> autoplay_icon;
	Ref<Texture2D> stop_icon;
	Ref<Texture2D> pause_icon;
	Ref<Texture2D> empty_icon = memnew(ImageTexture);

	HBoxContainer *playback_container = nullptr;
	Button *stop = nullptr;
	Button *play = nullptr;
	Button *play_from = nullptr;
	Button *play_bw = nullptr;
	Button *play_bw_from = nullptr;

	Button *load = nullptr;
	Button *load_sheet = nullptr;
	Button *delete_frame = nullptr;
	Button *copy = nullptr;
	Button *paste = nullptr;
	Button *empty_before = nullptr;
	Button *empty_after = nullptr;
	Button *move_up = nullptr;
	Button *move_down = nullptr;
	Button *zoom_out = nullptr;
	Button *zoom_reset = nullptr;
	Button *zoom_in = nullptr;
	SpinBox *frame_duration = nullptr;
	ItemList *frame_list = nullptr;
	bool loading_scene;
	Vector<int> selection;

	Button *add_anim = nullptr;
	Button *duplicate_anim = nullptr;
	Button *delete_anim = nullptr;
	SpinBox *anim_speed = nullptr;
	Button *anim_loop = nullptr;

	HBoxContainer *autoplay_container = nullptr;
	Button *autoplay = nullptr;

	LineEdit *anim_search_box = nullptr;
	Tree *animations = nullptr;

	Label *missing_anim_label = nullptr;
	VBoxContainer *anim_frames_vb = nullptr;

	EditorFileDialog *file = nullptr;

	AcceptDialog *dialog = nullptr;

	PopupMenu *menu = nullptr;

	StringName edited_anim;

	ConfirmationDialog *delete_dialog = nullptr;

	ConfirmationDialog *split_sheet_dialog = nullptr;
	ScrollContainer *split_sheet_scroll = nullptr;
	TextureRect *split_sheet_preview = nullptr;
	VBoxContainer *split_sheet_settings_vb = nullptr;
	SpinBox *split_sheet_h = nullptr;
	SpinBox *split_sheet_v = nullptr;
	SpinBox *split_sheet_size_x = nullptr;
	SpinBox *split_sheet_size_y = nullptr;
	SpinBox *split_sheet_sep_x = nullptr;
	SpinBox *split_sheet_sep_y = nullptr;
	SpinBox *split_sheet_offset_x = nullptr;
	SpinBox *split_sheet_offset_y = nullptr;
	Button *split_sheet_zoom_out = nullptr;
	Button *split_sheet_zoom_reset = nullptr;
	Button *split_sheet_zoom_in = nullptr;
	Button *toggle_settings_button = nullptr;
	OptionButton *split_sheet_order = nullptr;
	EditorFileDialog *file_split_sheet = nullptr;
	HashMap<int, int> frames_selected; // Key is frame index. Value is selection order.
	HashSet<int> frames_toggled_by_mouse_hover;
	Vector<Pair<int, int>> frames_ordered; // First is the index to be ordered by. Second is the actual frame index.
	int selected_count = 0;
	bool frames_need_sort = false;
	int last_frame_selected = 0;

	Size2i previous_texture_size;

	float scale_ratio;
	int thumbnail_default_size;
	float thumbnail_zoom;
	float max_thumbnail_zoom;
	float min_thumbnail_zoom;
	float sheet_zoom;
	float max_sheet_zoom;
	float min_sheet_zoom;

	Size2i _get_frame_count() const;
	Size2i _get_frame_size() const;
	Size2i _get_offset() const;
	Size2i _get_separation() const;

	void _load_pressed();
	void _file_load_request(const Vector<String> &p_path, int p_at_pos = -1);
	void _copy_pressed();
	void _paste_pressed();
	void _paste_frame_array(const Ref<ClipboardSpriteFrames> &p_clipboard_frames);
	void _paste_texture(const Ref<Texture2D> &p_texture);

	void _empty_pressed();
	void _empty2_pressed();
	void _delete_pressed();
	void _up_pressed();
	void _down_pressed();
	void _frame_duration_changed(double p_value);
	void _update_library(bool p_skip_selector = false);
	void _update_library_impl();

	void _update_stop_icon();
	void _play_pressed();
	void _play_from_pressed();
	void _play_bw_pressed();
	void _play_bw_from_pressed();
	void _autoplay_pressed();
	void _stop_pressed();

	void _animation_selected();
	void _animation_name_edited();
	void _animation_add();
	void _animation_duplicate();
	void _animation_remove();
	void _animation_remove_confirmed();
	void _animation_search_text_changed(const String &p_text);
	void _animation_loop_changed();
	void _animation_speed_changed(double p_value);

	void _frame_list_gui_input(const Ref<InputEvent> &p_event);
	void _frame_list_item_selected(int p_index, bool p_selected);

	void _menu_selected(int p_index);

	void _zoom_in();
	void _zoom_out();
	void _zoom_reset();

	bool animations_dirty = false;
	bool pending_update = false;

	bool updating;
	bool updating_split_settings = false; // Skip SpinBox/Range callback when setting value by code.

	Variant get_drag_data_fw(const Point2 &p_point, Control *p_from);
	bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);

	void _open_sprite_sheet();
	void _auto_slice_sprite_sheet();
	bool _matches_background_color(const Color &p_background_color, const Color &p_pixel_color);
	Size2i _estimate_sprite_sheet_size(const Ref<Texture2D> p_texture);
	void _prepare_sprite_sheet(const String &p_file);
	int _sheet_preview_position_to_frame_index(const Vector2 &p_position);
	void _sheet_preview_draw();
	void _sheet_spin_changed(double p_value, int p_dominant_param);
	void _sheet_preview_input(const Ref<InputEvent> &p_event);
	void _sheet_scroll_input(const Ref<InputEvent> &p_event);
	void _sheet_add_frames();
	void _sheet_zoom_on_position(float p_zoom, const Vector2 &p_position);
	void _sheet_zoom_in();
	void _sheet_zoom_out();
	void _sheet_zoom_reset();
	void _sheet_order_selected(int p_option);
	void _sheet_select_all_frames();
	void _sheet_clear_all_frames();
	void _sheet_sort_frames();
	void _toggle_show_settings();
	void _update_show_settings();

	void _edit();
	void _fetch_sprite_node();
	void _remove_sprite_node();

	bool sprite_node_updating = false;
	void _sync_animation();

	void _select_animation(const String &p_name, bool p_update_node = true);
	void _rename_node_animation(EditorUndoRedoManager *undo_redo, bool is_undo, const String &p_filter, const String &p_new_animation, const String &p_new_autoplay);

protected:
	void _notification(int p_what);
	void _node_removed(Node *p_node);
	static void _bind_methods();

public:
	void edit(Ref<SpriteFrames> p_frames);
	Ref<SpriteFrames> get_sprite_frames() const;

	SpriteFramesEditor();
};

class SpriteFramesEditorPlugin : public EditorPlugin {
	GDCLASS(SpriteFramesEditorPlugin, EditorPlugin);

	SpriteFramesEditor *frames_editor = nullptr;
	Button *button = nullptr;

public:
	virtual String get_plugin_name() const override { return "SpriteFrames"; }
	bool has_main_screen() const override { return false; }
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;

	SpriteFramesEditorPlugin();
	~SpriteFramesEditorPlugin();
};

#endif // SPRITE_FRAMES_EDITOR_PLUGIN_H
