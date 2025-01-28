/**************************************************************************/
/*  editor_quick_open_dialog.h                                            */
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

#ifndef EDITOR_QUICK_OPEN_DIALOG_H
#define EDITOR_QUICK_OPEN_DIALOG_H

#include "core/templates/oa_hash_map.h"
#include "scene/gui/dialogs.h"

class Button;
class CenterContainer;
class CheckButton;
class ConfigFile;
class EditorFileSystemDirectory;
class LineEdit;
class HFlowContainer;
class MarginContainer;
class PanelContainer;
class PopupMenu;
class ScrollContainer;
class StringName;
class Texture2D;
class TextureRect;
class VBoxContainer;

class FuzzySearchResult;

class QuickOpenResultItem;

enum class QuickOpenDisplayMode {
	GRID,
	LIST,
};

struct QuickOpenResultCandidate {
	String file_path;
	Ref<Texture2D> thumbnail;
	const FuzzySearchResult *result = nullptr;
};

class HighlightedLabel : public Label {
	GDCLASS(HighlightedLabel, Label)

	Vector<Vector2i> highlights;

	void draw_substr_rects(const Vector2i &p_substr, Vector2 p_offset, int p_line_limit, int line_spacing);

public:
	void add_highlight(const Vector2i &p_interval);
	void reset_highlights();

protected:
	void _notification(int p_notification);
};

class QuickOpenResultContainer : public VBoxContainer {
	GDCLASS(QuickOpenResultContainer, VBoxContainer)

	enum {
		FILE_SHOW_IN_FILESYSTEM,
		FILE_SHOW_IN_FILE_MANAGER
	};

public:
	void init(const Vector<StringName> &p_base_types);
	void handle_search_box_input(const Ref<InputEvent> &p_ie);
	void set_query_and_update(const String &p_query);
	void update_results();

	bool has_nothing_selected() const;
	String get_selected() const;

	void save_selected_item();
	void cleanup();

	QuickOpenResultContainer();

protected:
	void _notification(int p_what);

private:
	static constexpr int SHOW_ALL_FILES_THRESHOLD = 30;
	static constexpr int MAX_HISTORY_SIZE = 20;

	Vector<FuzzySearchResult> search_results;
	Vector<StringName> base_types;
	Vector<String> filepaths;
	OAHashMap<String, StringName> filetypes;
	Vector<QuickOpenResultCandidate> candidates;

	OAHashMap<StringName, Vector<QuickOpenResultCandidate>> selected_history;

	String query;
	int selection_index = -1;
	int num_visible_results = 0;
	int max_total_results = 0;

	bool showing_history = false;
	bool never_opened = true;
	Ref<ConfigFile> history_file;

	QuickOpenDisplayMode content_display_mode = QuickOpenDisplayMode::LIST;
	Vector<QuickOpenResultItem *> result_items;

	ScrollContainer *scroll_container = nullptr;
	VBoxContainer *list = nullptr;
	HFlowContainer *grid = nullptr;
	PopupMenu *file_context_menu = nullptr;

	PanelContainer *panel_container = nullptr;
	CenterContainer *no_results_container = nullptr;
	Label *no_results_label = nullptr;

	Label *file_details_path = nullptr;
	Button *display_mode_toggle = nullptr;
	CheckButton *include_addons_toggle = nullptr;
	CheckButton *fuzzy_search_toggle = nullptr;

	OAHashMap<StringName, Ref<Texture2D>> file_type_icons;

	static QuickOpenDisplayMode get_adaptive_display_mode(const Vector<StringName> &p_base_types);

	void _ensure_result_vector_capacity();
	void _create_initial_results();
	void _find_filepaths_in_folder(EditorFileSystemDirectory *p_directory, bool p_include_addons);

	void _setup_candidate(QuickOpenResultCandidate &p_candidate, const String &p_filepath);
	void _setup_candidate(QuickOpenResultCandidate &p_candidate, const FuzzySearchResult &p_result);
	void _update_fuzzy_search_results();
	void _use_default_candidates();
	void _score_and_sort_candidates();
	void _update_result_items(int p_new_visible_results_count, int p_new_selection_index);

	void _move_selection_index(Key p_key);
	void _select_item(int p_index);

	void _item_input(const Ref<InputEvent> &p_ev, int p_index);

	CanvasItem *_get_result_root();
	void _layout_result_item(QuickOpenResultItem *p_item);
	void _set_display_mode(QuickOpenDisplayMode p_display_mode);
	void _toggle_display_mode();
	void _toggle_include_addons(bool p_pressed);
	void _toggle_fuzzy_search(bool p_pressed);
	void _menu_option(int p_option);

	String _get_cache_file_path() const;

	static void _bind_methods();
};

class QuickOpenResultGridItem : public VBoxContainer {
	GDCLASS(QuickOpenResultGridItem, VBoxContainer)

public:
	QuickOpenResultGridItem();

	void reset();
	void set_content(const QuickOpenResultCandidate &p_candidate, bool p_highlight);
	void highlight_item(const Color &p_color);
	void remove_highlight();

private:
	TextureRect *thumbnail = nullptr;
	HighlightedLabel *name = nullptr;
};

class QuickOpenResultListItem : public HBoxContainer {
	GDCLASS(QuickOpenResultListItem, HBoxContainer)

public:
	QuickOpenResultListItem();

	void reset();
	void set_content(const QuickOpenResultCandidate &p_candidate, bool p_highlight);
	void highlight_item(const Color &p_color);
	void remove_highlight();

protected:
	void _notification(int p_what);

private:
	static const int CONTAINER_MARGIN = 8;

	MarginContainer *image_container = nullptr;
	VBoxContainer *text_container = nullptr;

	TextureRect *thumbnail = nullptr;
	HighlightedLabel *name = nullptr;
	HighlightedLabel *path = nullptr;
};

class QuickOpenResultItem : public HBoxContainer {
	GDCLASS(QuickOpenResultItem, HBoxContainer)

public:
	QuickOpenResultItem();

	bool enable_highlights = true;

	void reset();
	void set_content(const QuickOpenResultCandidate &p_candidate);
	void set_display_mode(QuickOpenDisplayMode p_display_mode);
	void highlight_item(bool p_enabled);

protected:
	void _notification(int p_what);

private:
	QuickOpenResultListItem *list_item = nullptr;
	QuickOpenResultGridItem *grid_item = nullptr;

	Ref<StyleBox> selected_stylebox;
	Ref<StyleBox> hovering_stylebox;
	Color highlighted_font_color;

	bool is_hovering = false;
	bool is_selected = false;

	void _set_enabled(bool p_enabled);
};

class EditorQuickOpenDialog : public AcceptDialog {
	GDCLASS(EditorQuickOpenDialog, AcceptDialog);

public:
	void popup_dialog(const Vector<StringName> &p_base_types, const Callable &p_item_selected_callback);
	EditorQuickOpenDialog();

protected:
	virtual void cancel_pressed() override;
	virtual void ok_pressed() override;

private:
	static String get_dialog_title(const Vector<StringName> &p_base_types);

	LineEdit *search_box = nullptr;
	QuickOpenResultContainer *container = nullptr;

	Callable item_selected_callback;

	void _search_box_text_changed(const String &p_query);
};

#endif // EDITOR_QUICK_OPEN_DIALOG_H
