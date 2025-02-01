/**************************************************************************/
/*  theme_editor_plugin.h                                                 */
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

#include "editor/plugins/editor_plugin.h"
#include "editor/plugins/theme_editor_preview.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/tree.h"
#include "scene/resources/theme.h"

class Button;
class CheckButton;
class EditorFileDialog;
class ItemList;
class Label;
class OptionButton;
class PanelContainer;
class TabBar;
class TabContainer;
class ThemeEditorPlugin;
class TextureRect;

class ThemeItemImportTree : public VBoxContainer {
	GDCLASS(ThemeItemImportTree, VBoxContainer);

	Ref<Theme> edited_theme;
	Ref<Theme> base_theme;

	struct ThemeItem {
		String type_name;
		Theme::DataType data_type;
		String item_name;

		bool operator<(const ThemeItem &p_item) const {
			if (type_name == p_item.type_name && data_type == p_item.data_type) {
				return item_name < p_item.item_name;
			}
			if (type_name == p_item.type_name) {
				return data_type < p_item.data_type;
			}
			return type_name < p_item.type_name;
		}
	};

	enum ItemCheckedState {
		SELECT_IMPORT_DEFINITION,
		SELECT_IMPORT_FULL,
	};

	RBMap<ThemeItem, ItemCheckedState> selected_items;

	LineEdit *import_items_filter = nullptr;

	Tree *import_items_tree = nullptr;
	List<TreeItem *> tree_color_items;
	List<TreeItem *> tree_constant_items;
	List<TreeItem *> tree_font_items;
	List<TreeItem *> tree_font_size_items;
	List<TreeItem *> tree_icon_items;
	List<TreeItem *> tree_stylebox_items;

	bool updating_tree = false;

	enum ItemActionFlag {
		IMPORT_ITEM = 1,
		IMPORT_ITEM_DATA = 2,
	};

	TextureRect *select_colors_icon = nullptr;
	Label *select_colors_label = nullptr;
	Button *select_all_colors_button = nullptr;
	Button *select_full_colors_button = nullptr;
	Button *deselect_all_colors_button = nullptr;
	Label *total_selected_colors_label = nullptr;

	TextureRect *select_constants_icon = nullptr;
	Label *select_constants_label = nullptr;
	Button *select_all_constants_button = nullptr;
	Button *select_full_constants_button = nullptr;
	Button *deselect_all_constants_button = nullptr;
	Label *total_selected_constants_label = nullptr;

	TextureRect *select_fonts_icon = nullptr;
	Label *select_fonts_label = nullptr;
	Button *select_all_fonts_button = nullptr;
	Button *select_full_fonts_button = nullptr;
	Button *deselect_all_fonts_button = nullptr;
	Label *total_selected_fonts_label = nullptr;

	TextureRect *select_font_sizes_icon = nullptr;
	Label *select_font_sizes_label = nullptr;
	Button *select_all_font_sizes_button = nullptr;
	Button *select_full_font_sizes_button = nullptr;
	Button *deselect_all_font_sizes_button = nullptr;
	Label *total_selected_font_sizes_label = nullptr;

	TextureRect *select_icons_icon = nullptr;
	Label *select_icons_label = nullptr;
	Button *select_all_icons_button = nullptr;
	Button *select_full_icons_button = nullptr;
	Button *deselect_all_icons_button = nullptr;
	Label *total_selected_icons_label = nullptr;

	TextureRect *select_styleboxes_icon = nullptr;
	Label *select_styleboxes_label = nullptr;
	Button *select_all_styleboxes_button = nullptr;
	Button *select_full_styleboxes_button = nullptr;
	Button *deselect_all_styleboxes_button = nullptr;
	Label *total_selected_styleboxes_label = nullptr;

	HBoxContainer *select_icons_warning_hb = nullptr;
	TextureRect *select_icons_warning_icon = nullptr;
	Label *select_icons_warning = nullptr;

	Button *import_collapse_types_button = nullptr;
	Button *import_expand_types_button = nullptr;
	Button *import_select_all_button = nullptr;
	Button *import_select_full_button = nullptr;
	Button *import_deselect_all_button = nullptr;

	void _update_items_tree();
	void _toggle_type_items(bool p_collapse);
	void _filter_text_changed(const String &p_value);

	void _store_selected_item(TreeItem *p_tree_item);
	void _restore_selected_item(TreeItem *p_tree_item);
	void _update_total_selected(Theme::DataType p_data_type);

	void _tree_item_edited();
	void _check_propagated_to_tree_item(Object *p_obj, int p_column);
	void _select_all_subitems(TreeItem *p_root_item, bool p_select_with_data);
	void _deselect_all_subitems(TreeItem *p_root_item, bool p_deselect_completely);

	void _select_all_items_pressed();
	void _select_full_items_pressed();
	void _deselect_all_items_pressed();

	void _select_all_data_type_pressed(int p_data_type);
	void _select_full_data_type_pressed(int p_data_type);
	void _deselect_all_data_type_pressed(int p_data_type);

	void _import_selected();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_edited_theme(const Ref<Theme> &p_theme);
	void set_base_theme(const Ref<Theme> &p_theme);
	void reset_item_tree();

	bool has_selected_items() const;

	ThemeItemImportTree();
};

class ThemeTypeEditor;

class ThemeItemEditorDialog : public AcceptDialog {
	GDCLASS(ThemeItemEditorDialog, AcceptDialog);

	ThemeTypeEditor *theme_type_editor = nullptr;

	Ref<Theme> edited_theme;

	TabContainer *tc = nullptr;

	enum TypesTreeAction {
		TYPES_TREE_REMOVE_ITEM,
	};

	Tree *edit_type_list = nullptr;
	LineEdit *edit_add_type_value = nullptr;
	Button *edit_add_type_button = nullptr;
	String edited_item_type;

	Button *edit_items_add_color = nullptr;
	Button *edit_items_add_constant = nullptr;
	Button *edit_items_add_font = nullptr;
	Button *edit_items_add_font_size = nullptr;
	Button *edit_items_add_icon = nullptr;
	Button *edit_items_add_stylebox = nullptr;
	Button *edit_items_remove_class = nullptr;
	Button *edit_items_remove_custom = nullptr;
	Button *edit_items_remove_all = nullptr;
	Tree *edit_items_tree = nullptr;
	Label *edit_items_message = nullptr;

	enum ItemsTreeAction {
		ITEMS_TREE_RENAME_ITEM,
		ITEMS_TREE_REMOVE_ITEM,
		ITEMS_TREE_REMOVE_DATA_TYPE,
	};

	ConfirmationDialog *edit_theme_item_dialog = nullptr;
	VBoxContainer *edit_theme_item_old_vb = nullptr;
	Label *theme_item_old_name = nullptr;
	LineEdit *theme_item_name = nullptr;

	enum ItemPopupMode {
		CREATE_THEME_ITEM,
		RENAME_THEME_ITEM,
		ITEM_POPUP_MODE_MAX
	};

	ItemPopupMode item_popup_mode = ITEM_POPUP_MODE_MAX;
	String edit_item_old_name;
	Theme::DataType edit_item_data_type = Theme::DATA_TYPE_MAX;

	ThemeItemImportTree *import_default_theme_items = nullptr;
	ThemeItemImportTree *import_editor_theme_items = nullptr;
	ThemeItemImportTree *import_other_theme_items = nullptr;

	LineEdit *import_another_theme_value = nullptr;
	Button *import_another_theme_button = nullptr;
	EditorFileDialog *import_another_theme_dialog = nullptr;

	ConfirmationDialog *confirm_closing_dialog = nullptr;

	void ok_pressed() override;
	void _close_dialog();

	void _dialog_about_to_show();
	void _update_edit_types();
	void _edited_type_selected();
	void _edited_type_button_pressed(Object *p_item, int p_column, int p_id, MouseButton p_button);

	void _update_edit_item_tree(String p_item_type);
	void _item_tree_button_pressed(Object *p_item, int p_column, int p_id, MouseButton p_button);

	void _add_theme_type(const String &p_new_text);
	void _add_theme_item(Theme::DataType p_data_type, String p_item_name, String p_item_type);
	void _remove_theme_type(const String &p_theme_type);
	void _remove_data_type_items(Theme::DataType p_data_type, String p_item_type);
	void _remove_class_items();
	void _remove_custom_items();
	void _remove_all_items();

	void _open_add_theme_item_dialog(int p_data_type);
	void _open_rename_theme_item_dialog(Theme::DataType p_data_type, String p_item_name);
	void _confirm_edit_theme_item();
	void _edit_theme_item_gui_input(const Ref<InputEvent> &p_event);

	void _open_select_another_theme();
	void _select_another_theme_cbk(const String &p_path);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_edited_theme(const Ref<Theme> &p_theme);

	ThemeItemEditorDialog(ThemeTypeEditor *p_theme_editor);
};

class ThemeTypeDialog : public ConfirmationDialog {
	GDCLASS(ThemeTypeDialog, ConfirmationDialog);

	Ref<Theme> edited_theme;
	bool include_own_types = false;

	String pre_submitted_value;

	LineEdit *add_type_filter = nullptr;
	ItemList *add_type_options = nullptr;
	ConfirmationDialog *add_type_confirmation = nullptr;

	void _dialog_about_to_show();
	void ok_pressed() override;

	void _update_add_type_options(const String &p_filter = "");

	void _add_type_filter_cbk(const String &p_value);
	void _type_filter_input(const Ref<InputEvent> &p_event);
	void _add_type_options_cbk(int p_index);
	void _add_type_dialog_entered(const String &p_value);
	void _add_type_dialog_activated(int p_index);

	void _add_type_selected(const String &p_type_name);
	void _add_type_confirmed();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_edited_theme(const Ref<Theme> &p_theme);
	void set_include_own_types(bool p_enable);

	ThemeTypeDialog();
};

// Custom `Label` needed to use `EditorHelpBit` to display theme item documentation.
class ThemeItemLabel : public Label {
	virtual Control *make_custom_tooltip(const String &p_text) const;
};

class ThemeTypeEditor : public MarginContainer {
	GDCLASS(ThemeTypeEditor, MarginContainer);

	Ref<Theme> edited_theme;
	String edited_type;
	bool updating = false;

	struct LeadingStylebox {
		bool pinned = false;
		StringName item_name;
		Ref<StyleBox> stylebox;
		Ref<StyleBox> ref_stylebox;
	};

	LeadingStylebox leading_stylebox;

	OptionButton *theme_type_list = nullptr;
	Button *add_type_button = nullptr;

	CheckButton *show_default_items_button = nullptr;

	TabContainer *data_type_tabs = nullptr;
	VBoxContainer *color_items_list = nullptr;
	VBoxContainer *constant_items_list = nullptr;
	VBoxContainer *font_items_list = nullptr;
	VBoxContainer *font_size_items_list = nullptr;
	VBoxContainer *icon_items_list = nullptr;
	VBoxContainer *stylebox_items_list = nullptr;

	LineEdit *type_variation_edit = nullptr;
	Button *type_variation_button = nullptr;
	Label *type_variation_locked = nullptr;

	enum TypeDialogMode {
		ADD_THEME_TYPE,
		ADD_VARIATION_BASE,
	};

	TypeDialogMode add_type_mode = ADD_THEME_TYPE;
	ThemeTypeDialog *add_type_dialog = nullptr;

	Vector<Control *> focusables;
	Timer *update_debounce_timer = nullptr;

	VBoxContainer *_create_item_list(Theme::DataType p_data_type);
	void _update_type_list();
	void _update_type_list_debounced();
	HashMap<StringName, bool> _get_type_items(String p_type_name, Theme::DataType p_type, bool p_include_default);
	HBoxContainer *_create_property_control(Theme::DataType p_data_type, String p_item_name, bool p_editable);
	void _add_focusable(Control *p_control);
	void _update_type_items();

	void _list_type_selected(int p_index);
	void _add_type_button_cbk();
	void _add_default_type_items();

	void _update_add_button(const String &p_text, LineEdit *p_for_edit);
	void _item_add_cbk(int p_data_type, Control *p_control);
	void _item_add_lineedit_cbk(String p_value, int p_data_type, Control *p_control);
	void _item_override_cbk(int p_data_type, String p_item_name);
	void _item_remove_cbk(int p_data_type, String p_item_name);
	void _item_rename_cbk(int p_data_type, String p_item_name, Control *p_control);
	void _item_rename_confirmed(int p_data_type, String p_item_name, Control *p_control);
	void _item_rename_entered(String p_value, int p_data_type, String p_item_name, Control *p_control);
	void _item_rename_canceled(int p_data_type, String p_item_name, Control *p_control);

	void _color_item_changed(Color p_value, String p_item_name);
	void _constant_item_changed(float p_value, String p_item_name);
	void _font_size_item_changed(float p_value, String p_item_name);
	void _edit_resource_item(Ref<Resource> p_resource, bool p_edit);
	void _font_item_changed(Ref<Font> p_value, String p_item_name);
	void _icon_item_changed(Ref<Texture2D> p_value, String p_item_name);
	void _stylebox_item_changed(Ref<StyleBox> p_value, String p_item_name);
	void _change_pinned_stylebox();
	void _on_pin_leader_button_pressed(Control *p_editor, String p_item_name);
	void _pin_leading_stylebox(String p_item_name, Ref<StyleBox> p_stylebox);
	void _on_unpin_leader_button_pressed();
	void _unpin_leading_stylebox();
	void _update_stylebox_from_leading();

	void _type_variation_changed(const String p_value);
	void _add_type_variation_cbk();

	void _add_type_dialog_selected(const String p_type_name);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_edited_theme(const Ref<Theme> &p_theme);
	void select_type(String p_type_name);
	bool is_stylebox_pinned(Ref<StyleBox> p_stylebox);

	ThemeTypeEditor();
};

class ThemeEditor : public VBoxContainer {
	GDCLASS(ThemeEditor, VBoxContainer);

	friend class ThemeEditorPlugin;
	ThemeEditorPlugin *plugin = nullptr;

	Ref<Theme> theme;

	TabBar *preview_tabs = nullptr;
	PanelContainer *preview_tabs_content = nullptr;
	Button *add_preview_button = nullptr;
	EditorFileDialog *preview_scene_dialog = nullptr;

	ThemeTypeEditor *theme_type_editor = nullptr;

	Label *theme_name = nullptr;
	ThemeItemEditorDialog *theme_edit_dialog = nullptr;

	void _theme_save_button_cbk(bool p_save_as);
	void _theme_edit_button_cbk();
	void _theme_close_button_cbk();
	void _scene_closed(const String &p_path);

	void _add_preview_button_cbk();
	void _preview_scene_dialog_cbk(const String &p_path);
	void _add_preview_tab(ThemeEditorPreview *p_preview_tab, const String &p_preview_name, const Ref<Texture2D> &p_icon);
	void _change_preview_tab(int p_tab);
	void _remove_preview_tab(int p_tab);
	void _remove_preview_tab_invalid(Node *p_tab_control);
	void _update_preview_tab(Node *p_tab_control);
	void _preview_control_picked(String p_class_name);

protected:
	void _notification(int p_what);

public:
	void edit(const Ref<Theme> &p_theme);
	Ref<Theme> get_edited_theme();

	bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);
	void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);

	ThemeEditor();
};

class ThemeEditorPlugin : public EditorPlugin {
	GDCLASS(ThemeEditorPlugin, EditorPlugin);

	ThemeEditor *theme_editor = nullptr;
	Button *button = nullptr;

public:
	virtual String get_plugin_name() const override { return "Theme"; }
	bool has_main_screen() const override { return false; }
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;
	virtual bool can_auto_hide() const override;

	ThemeEditorPlugin();
};
