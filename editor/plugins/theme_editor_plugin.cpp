/**************************************************************************/
/*  theme_editor_plugin.cpp                                               */
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

#include "theme_editor_plugin.h"

#include "core/os/keyboard.h"
#include "editor/editor_resource_picker.h"
#include "editor/editor_scale.h"
#include "editor/progress_dialog.h"

void ThemeItemImportTree::_update_items_tree() {
	import_items_tree->clear();
	TreeItem *root = import_items_tree->create_item();

	if (base_theme.is_null()) {
		return;
	}

	String filter_text = import_items_filter->get_text();

	List<StringName> types;
	List<StringName> names;
	List<StringName> filtered_names;
	base_theme->get_type_list(&types);
	types.sort_custom<StringName::AlphCompare>();

	int color_amount = 0;
	int constant_amount = 0;
	int font_amount = 0;
	int icon_amount = 0;
	int stylebox_amount = 0;

	tree_color_items.clear();
	tree_constant_items.clear();
	tree_font_items.clear();
	tree_icon_items.clear();
	tree_stylebox_items.clear();

	for (List<StringName>::Element *E = types.front(); E; E = E->next()) {
		String type_name = (String)E->get();

		TreeItem *type_node = import_items_tree->create_item(root);
		type_node->set_meta("_can_be_imported", false);
		type_node->set_collapsed(true);
		type_node->set_text(0, type_name);
		type_node->set_cell_mode(IMPORT_ITEM, TreeItem::CELL_MODE_CHECK);
		type_node->set_checked(IMPORT_ITEM, false);
		type_node->set_editable(IMPORT_ITEM, true);
		type_node->set_cell_mode(IMPORT_ITEM_DATA, TreeItem::CELL_MODE_CHECK);
		type_node->set_checked(IMPORT_ITEM_DATA, false);
		type_node->set_editable(IMPORT_ITEM_DATA, true);

		bool is_matching_filter = (filter_text.empty() || type_name.findn(filter_text) > -1);
		bool has_filtered_items = false;
		bool any_checked = false;
		bool any_checked_with_data = false;

		for (int i = 0; i < Theme::DATA_TYPE_MAX; i++) {
			Theme::DataType dt = (Theme::DataType)i;

			names.clear();
			filtered_names.clear();
			base_theme->get_theme_item_list(dt, E->get(), &names);

			bool data_type_has_filtered_items = false;

			for (List<StringName>::Element *F = names.front(); F; F = F->next()) {
				String item_name = (String)F->get();
				bool is_item_matching_filter = (item_name.findn(filter_text) > -1);
				if (!filter_text.empty() && !is_matching_filter && !is_item_matching_filter) {
					continue;
				}

				// Only mark this if actual items match the filter and not just the type group.
				if (!filter_text.empty() && is_item_matching_filter) {
					has_filtered_items = true;
					data_type_has_filtered_items = true;
				}
				filtered_names.push_back(F->get());
			}

			if (filtered_names.size() == 0) {
				continue;
			}

			TreeItem *data_type_node = import_items_tree->create_item(type_node);
			data_type_node->set_meta("_can_be_imported", false);
			data_type_node->set_metadata(0, i);
			data_type_node->set_collapsed(!data_type_has_filtered_items);
			data_type_node->set_cell_mode(IMPORT_ITEM, TreeItem::CELL_MODE_CHECK);
			data_type_node->set_checked(IMPORT_ITEM, false);
			data_type_node->set_editable(IMPORT_ITEM, true);
			data_type_node->set_cell_mode(IMPORT_ITEM_DATA, TreeItem::CELL_MODE_CHECK);
			data_type_node->set_checked(IMPORT_ITEM_DATA, false);
			data_type_node->set_editable(IMPORT_ITEM_DATA, true);

			List<TreeItem *> *item_list;

			switch (dt) {
				case Theme::DATA_TYPE_COLOR:
					data_type_node->set_icon(0, get_icon("Color", "EditorIcons"));
					data_type_node->set_text(0, TTR("Colors"));

					item_list = &tree_color_items;
					color_amount += filtered_names.size();
					break;

				case Theme::DATA_TYPE_CONSTANT:
					data_type_node->set_icon(0, get_icon("MemberConstant", "EditorIcons"));
					data_type_node->set_text(0, TTR("Constants"));

					item_list = &tree_constant_items;
					constant_amount += filtered_names.size();
					break;

				case Theme::DATA_TYPE_FONT:
					data_type_node->set_icon(0, get_icon("Font", "EditorIcons"));
					data_type_node->set_text(0, TTR("Fonts"));

					item_list = &tree_font_items;
					font_amount += filtered_names.size();
					break;

				case Theme::DATA_TYPE_ICON:
					data_type_node->set_icon(0, get_icon("ImageTexture", "EditorIcons"));
					data_type_node->set_text(0, TTR("Icons"));

					item_list = &tree_icon_items;
					icon_amount += filtered_names.size();
					break;

				case Theme::DATA_TYPE_STYLEBOX:
					data_type_node->set_icon(0, get_icon("StyleBoxFlat", "EditorIcons"));
					data_type_node->set_text(0, TTR("Styleboxes"));

					item_list = &tree_stylebox_items;
					stylebox_amount += filtered_names.size();
					break;

				case Theme::DATA_TYPE_MAX:
					break; // Can't happen, but silences warning.
			}

			bool data_type_any_checked = false;
			bool data_type_any_checked_with_data = false;

			filtered_names.sort_custom<StringName::AlphCompare>();
			for (List<StringName>::Element *F = filtered_names.front(); F; F = F->next()) {
				TreeItem *item_node = import_items_tree->create_item(data_type_node);
				item_node->set_meta("_can_be_imported", true);
				item_node->set_text(0, F->get());
				item_node->set_cell_mode(IMPORT_ITEM, TreeItem::CELL_MODE_CHECK);
				item_node->set_checked(IMPORT_ITEM, false);
				item_node->set_editable(IMPORT_ITEM, true);
				item_node->set_cell_mode(IMPORT_ITEM_DATA, TreeItem::CELL_MODE_CHECK);
				item_node->set_checked(IMPORT_ITEM_DATA, false);
				item_node->set_editable(IMPORT_ITEM_DATA, true);

				_restore_selected_item(item_node);
				if (item_node->is_checked(IMPORT_ITEM)) {
					data_type_any_checked = true;
					any_checked = true;
				}
				if (item_node->is_checked(IMPORT_ITEM_DATA)) {
					data_type_any_checked_with_data = true;
					any_checked_with_data = true;
				}

				item_list->push_back(item_node);
			}

			data_type_node->set_checked(IMPORT_ITEM, data_type_any_checked);
			data_type_node->set_checked(IMPORT_ITEM_DATA, data_type_any_checked && data_type_any_checked_with_data);
		}

		// Remove the item if it doesn't match the filter in any way.
		if (!is_matching_filter && !has_filtered_items) {
			root->remove_child(type_node);
			memdelete(type_node);
			continue;
		}

		// Show one level inside of a type group if there are matches in items.
		if (!filter_text.empty() && has_filtered_items) {
			type_node->set_collapsed(false);
		}

		type_node->set_checked(IMPORT_ITEM, any_checked);
		type_node->set_checked(IMPORT_ITEM_DATA, any_checked && any_checked_with_data);
	}

	if (color_amount > 0) {
		Array arr;
		arr.push_back(color_amount);
		select_colors_label->set_text(TTR("{num} color(s)").format(arr, "{num}"));
		select_all_colors_button->set_visible(true);
		select_full_colors_button->set_visible(true);
		deselect_all_colors_button->set_visible(true);
	} else {
		select_colors_label->set_text(TTR("No colors found."));
		select_all_colors_button->set_visible(false);
		select_full_colors_button->set_visible(false);
		deselect_all_colors_button->set_visible(false);
	}

	if (constant_amount > 0) {
		Array arr;
		arr.push_back(constant_amount);
		select_constants_label->set_text(TTR("{num} constant(s)").format(arr, "{num}"));
		select_all_constants_button->set_visible(true);
		select_full_constants_button->set_visible(true);
		deselect_all_constants_button->set_visible(true);
	} else {
		select_constants_label->set_text(TTR("No constants found."));
		select_all_constants_button->set_visible(false);
		select_full_constants_button->set_visible(false);
		deselect_all_constants_button->set_visible(false);
	}

	if (font_amount > 0) {
		Array arr;
		arr.push_back(font_amount);
		select_fonts_label->set_text(TTR("{num} font(s)").format(arr, "{num}"));
		select_all_fonts_button->set_visible(true);
		select_full_fonts_button->set_visible(true);
		deselect_all_fonts_button->set_visible(true);
	} else {
		select_fonts_label->set_text(TTR("No fonts found."));
		select_all_fonts_button->set_visible(false);
		select_full_fonts_button->set_visible(false);
		deselect_all_fonts_button->set_visible(false);
	}

	if (icon_amount > 0) {
		Array arr;
		arr.push_back(icon_amount);
		select_icons_label->set_text(TTR("{num} icon(s)").format(arr, "{num}"));
		select_all_icons_button->set_visible(true);
		select_full_icons_button->set_visible(true);
		deselect_all_icons_button->set_visible(true);
		select_icons_warning_hb->set_visible(true);
	} else {
		select_icons_label->set_text(TTR("No icons found."));
		select_all_icons_button->set_visible(false);
		select_full_icons_button->set_visible(false);
		deselect_all_icons_button->set_visible(false);
		select_icons_warning_hb->set_visible(false);
	}

	if (stylebox_amount > 0) {
		Array arr;
		arr.push_back(stylebox_amount);
		select_styleboxes_label->set_text(TTR("{num} stylebox(es)").format(arr, "{num}"));
		select_all_styleboxes_button->set_visible(true);
		select_full_styleboxes_button->set_visible(true);
		deselect_all_styleboxes_button->set_visible(true);
	} else {
		select_styleboxes_label->set_text(TTR("No styleboxes found."));
		select_all_styleboxes_button->set_visible(false);
		select_full_styleboxes_button->set_visible(false);
		deselect_all_styleboxes_button->set_visible(false);
	}
}

void ThemeItemImportTree::_toggle_type_items(bool p_collapse) {
	TreeItem *root = import_items_tree->get_root();
	if (!root) {
		return;
	}

	TreeItem *type_node = root->get_children();
	while (type_node) {
		type_node->set_collapsed(p_collapse);
		type_node = type_node->get_next();
	}
}

void ThemeItemImportTree::_filter_text_changed(const String &p_value) {
	_update_items_tree();
}

void ThemeItemImportTree::_store_selected_item(TreeItem *p_tree_item) {
	if (!p_tree_item->get_meta("_can_be_imported")) {
		return;
	}

	TreeItem *data_type_node = p_tree_item->get_parent();
	if (!data_type_node || data_type_node == import_items_tree->get_root()) {
		return;
	}

	TreeItem *type_node = data_type_node->get_parent();
	if (!type_node || type_node == import_items_tree->get_root()) {
		return;
	}

	ThemeItem ti;
	ti.item_name = p_tree_item->get_text(0);
	ti.data_type = (Theme::DataType)(int)data_type_node->get_metadata(0);
	ti.type_name = type_node->get_text(0);

	bool import = p_tree_item->is_checked(IMPORT_ITEM);
	bool with_data = p_tree_item->is_checked(IMPORT_ITEM_DATA);

	if (import && with_data) {
		selected_items[ti] = SELECT_IMPORT_FULL;
	} else if (import) {
		selected_items[ti] = SELECT_IMPORT_DEFINITION;
	} else {
		selected_items.erase(ti);
	}

	_update_total_selected(ti.data_type);
}

void ThemeItemImportTree::_restore_selected_item(TreeItem *p_tree_item) {
	if (!p_tree_item->get_meta("_can_be_imported")) {
		return;
	}

	TreeItem *data_type_node = p_tree_item->get_parent();
	if (!data_type_node || data_type_node == import_items_tree->get_root()) {
		return;
	}

	TreeItem *type_node = data_type_node->get_parent();
	if (!type_node || type_node == import_items_tree->get_root()) {
		return;
	}

	ThemeItem ti;
	ti.item_name = p_tree_item->get_text(0);
	ti.data_type = (Theme::DataType)(int)data_type_node->get_metadata(0);
	ti.type_name = type_node->get_text(0);

	if (!selected_items.has(ti)) {
		p_tree_item->set_checked(IMPORT_ITEM, false);
		p_tree_item->set_checked(IMPORT_ITEM_DATA, false);
		return;
	}

	if (selected_items[ti] == SELECT_IMPORT_FULL) {
		p_tree_item->set_checked(IMPORT_ITEM, true);
		p_tree_item->set_checked(IMPORT_ITEM_DATA, true);
	} else if (selected_items[ti] == SELECT_IMPORT_DEFINITION) {
		p_tree_item->set_checked(IMPORT_ITEM, true);
		p_tree_item->set_checked(IMPORT_ITEM_DATA, false);
	}
}

void ThemeItemImportTree::_update_total_selected(Theme::DataType p_data_type) {
	ERR_FAIL_INDEX_MSG(p_data_type, Theme::DATA_TYPE_MAX, "Theme item data type is out of bounds.");

	Label *total_selected_items_label;
	switch (p_data_type) {
		case Theme::DATA_TYPE_COLOR:
			total_selected_items_label = total_selected_colors_label;
			break;

		case Theme::DATA_TYPE_CONSTANT:
			total_selected_items_label = total_selected_constants_label;
			break;

		case Theme::DATA_TYPE_FONT:
			total_selected_items_label = total_selected_fonts_label;
			break;

		case Theme::DATA_TYPE_ICON:
			total_selected_items_label = total_selected_icons_label;
			break;

		case Theme::DATA_TYPE_STYLEBOX:
			total_selected_items_label = total_selected_styleboxes_label;
			break;

		case Theme::DATA_TYPE_MAX:
			return; // Can't happen, but silences warning.
	}

	if (!total_selected_items_label) {
		return;
	}

	int count = 0;
	for (Map<ThemeItem, ItemCheckedState>::Element *E = selected_items.front(); E; E = E->next()) {
		ThemeItem ti = E->key();
		if (ti.data_type == p_data_type) {
			count++;
		}
	}

	if (count == 0) {
		total_selected_items_label->hide();
	} else {
		Array arr;
		arr.push_back(count);
		total_selected_items_label->set_text(TTR("{num} currently selected").format(arr, "{num}"));
		total_selected_items_label->show();
	}
}

void ThemeItemImportTree::_tree_item_edited() {
	if (updating_tree) {
		return;
	}

	TreeItem *edited_item = import_items_tree->get_edited();
	if (!edited_item) {
		return;
	}

	updating_tree = true;

	int edited_column = import_items_tree->get_edited_column();
	bool is_checked = edited_item->is_checked(edited_column);
	if (is_checked) {
		if (edited_column == IMPORT_ITEM_DATA) {
			edited_item->set_checked(IMPORT_ITEM, true);
		}

		_select_all_subitems(edited_item, (edited_column == IMPORT_ITEM_DATA));
	} else {
		if (edited_column == IMPORT_ITEM) {
			edited_item->set_checked(IMPORT_ITEM_DATA, false);
		}

		_deselect_all_subitems(edited_item, (edited_column == IMPORT_ITEM));
	}

	_update_parent_items(edited_item);
	_store_selected_item(edited_item);

	updating_tree = false;
}

void ThemeItemImportTree::_select_all_subitems(TreeItem *p_root_item, bool p_select_with_data) {
	TreeItem *child_item = p_root_item->get_children();
	while (child_item) {
		child_item->set_checked(IMPORT_ITEM, true);
		if (p_select_with_data) {
			child_item->set_checked(IMPORT_ITEM_DATA, true);
		}
		_store_selected_item(child_item);

		_select_all_subitems(child_item, p_select_with_data);
		child_item = child_item->get_next();
	}
}

void ThemeItemImportTree::_deselect_all_subitems(TreeItem *p_root_item, bool p_deselect_completely) {
	TreeItem *child_item = p_root_item->get_children();
	while (child_item) {
		child_item->set_checked(IMPORT_ITEM_DATA, false);
		if (p_deselect_completely) {
			child_item->set_checked(IMPORT_ITEM, false);
		}
		_store_selected_item(child_item);

		_deselect_all_subitems(child_item, p_deselect_completely);
		child_item = child_item->get_next();
	}
}

void ThemeItemImportTree::_update_parent_items(TreeItem *p_root_item) {
	TreeItem *parent_item = p_root_item->get_parent();
	if (!parent_item) {
		return;
	}

	bool any_checked = false;
	bool any_checked_with_data = false;

	TreeItem *child_item = parent_item->get_children();
	while (child_item) {
		if (child_item->is_checked(IMPORT_ITEM)) {
			any_checked = true;
		}
		if (child_item->is_checked(IMPORT_ITEM_DATA)) {
			any_checked_with_data = true;
		}

		child_item = child_item->get_next();
	}

	parent_item->set_checked(IMPORT_ITEM, any_checked);
	parent_item->set_checked(IMPORT_ITEM_DATA, any_checked && any_checked_with_data);
	_update_parent_items(parent_item);
}

void ThemeItemImportTree::_select_all_items_pressed() {
	if (updating_tree) {
		return;
	}

	updating_tree = true;

	TreeItem *root = import_items_tree->get_root();
	_select_all_subitems(root, false);

	updating_tree = false;
}

void ThemeItemImportTree::_select_full_items_pressed() {
	if (updating_tree) {
		return;
	}

	updating_tree = true;

	TreeItem *root = import_items_tree->get_root();
	_select_all_subitems(root, true);

	updating_tree = false;
}

void ThemeItemImportTree::_deselect_all_items_pressed() {
	if (updating_tree) {
		return;
	}

	updating_tree = true;

	TreeItem *root = import_items_tree->get_root();
	_deselect_all_subitems(root, true);

	updating_tree = false;
}

void ThemeItemImportTree::_select_all_data_type_pressed(int p_data_type) {
	ERR_FAIL_INDEX_MSG(p_data_type, Theme::DATA_TYPE_MAX, "Theme item data type is out of bounds.");

	if (updating_tree) {
		return;
	}

	Theme::DataType data_type = (Theme::DataType)p_data_type;
	List<TreeItem *> *item_list;

	switch (data_type) {
		case Theme::DATA_TYPE_COLOR:
			item_list = &tree_color_items;
			break;

		case Theme::DATA_TYPE_CONSTANT:
			item_list = &tree_constant_items;
			break;

		case Theme::DATA_TYPE_FONT:
			item_list = &tree_font_items;
			break;

		case Theme::DATA_TYPE_ICON:
			item_list = &tree_icon_items;
			break;

		case Theme::DATA_TYPE_STYLEBOX:
			item_list = &tree_stylebox_items;
			break;

		case Theme::DATA_TYPE_MAX:
			return; // Can't happen, but silences warning.
	}

	updating_tree = true;

	for (List<TreeItem *>::Element *E = item_list->front(); E; E = E->next()) {
		TreeItem *child_item = E->get();
		if (!child_item) {
			continue;
		}

		child_item->set_checked(IMPORT_ITEM, true);
		_update_parent_items(child_item);
		_store_selected_item(child_item);
	}

	updating_tree = false;
}

void ThemeItemImportTree::_select_full_data_type_pressed(int p_data_type) {
	ERR_FAIL_INDEX_MSG(p_data_type, Theme::DATA_TYPE_MAX, "Theme item data type is out of bounds.");

	if (updating_tree) {
		return;
	}

	Theme::DataType data_type = (Theme::DataType)p_data_type;
	List<TreeItem *> *item_list;

	switch (data_type) {
		case Theme::DATA_TYPE_COLOR:
			item_list = &tree_color_items;
			break;

		case Theme::DATA_TYPE_CONSTANT:
			item_list = &tree_constant_items;
			break;

		case Theme::DATA_TYPE_FONT:
			item_list = &tree_font_items;
			break;

		case Theme::DATA_TYPE_ICON:
			item_list = &tree_icon_items;
			break;

		case Theme::DATA_TYPE_STYLEBOX:
			item_list = &tree_stylebox_items;
			break;

		case Theme::DATA_TYPE_MAX:
			return; // Can't happen, but silences warning.
	}

	updating_tree = true;

	for (List<TreeItem *>::Element *E = item_list->front(); E; E = E->next()) {
		TreeItem *child_item = E->get();
		if (!child_item) {
			continue;
		}

		child_item->set_checked(IMPORT_ITEM, true);
		child_item->set_checked(IMPORT_ITEM_DATA, true);
		_update_parent_items(child_item);
		_store_selected_item(child_item);
	}

	updating_tree = false;
}

void ThemeItemImportTree::_deselect_all_data_type_pressed(int p_data_type) {
	ERR_FAIL_INDEX_MSG(p_data_type, Theme::DATA_TYPE_MAX, "Theme item data type is out of bounds.");

	if (updating_tree) {
		return;
	}

	Theme::DataType data_type = (Theme::DataType)p_data_type;
	List<TreeItem *> *item_list;

	switch (data_type) {
		case Theme::DATA_TYPE_COLOR:
			item_list = &tree_color_items;
			break;

		case Theme::DATA_TYPE_CONSTANT:
			item_list = &tree_constant_items;
			break;

		case Theme::DATA_TYPE_FONT:
			item_list = &tree_font_items;
			break;

		case Theme::DATA_TYPE_ICON:
			item_list = &tree_icon_items;
			break;

		case Theme::DATA_TYPE_STYLEBOX:
			item_list = &tree_stylebox_items;
			break;

		case Theme::DATA_TYPE_MAX:
			return; // Can't happen, but silences warning.
	}

	updating_tree = true;

	for (List<TreeItem *>::Element *E = item_list->front(); E; E = E->next()) {
		TreeItem *child_item = E->get();
		if (!child_item) {
			continue;
		}

		child_item->set_checked(IMPORT_ITEM, false);
		child_item->set_checked(IMPORT_ITEM_DATA, false);
		_update_parent_items(child_item);
		_store_selected_item(child_item);
	}

	updating_tree = false;
}

void ThemeItemImportTree::_import_selected() {
	if (selected_items.size() == 0) {
		EditorNode::get_singleton()->show_accept(TTR("Nothing was selected for the import."), TTR("OK"));
		return;
	}

	// Prevent changes from immediately being reported while the operation is still ongoing.
	edited_theme->_freeze_change_propagation();
	ProgressDialog::get_singleton()->add_task("import_theme_items", TTR("Importing Theme Items"), selected_items.size() + 2);

	int idx = 0;
	for (Map<ThemeItem, ItemCheckedState>::Element *E = selected_items.front(); E; E = E->next()) {
		// Arbitrary number of items to skip from reporting.
		// Reduces the number of UI updates that this causes when copying large themes.
		if (idx % 10 == 0) {
			Array arr;
			arr.push_back(idx + 1);
			arr.push_back(selected_items.size());
			ProgressDialog::get_singleton()->task_step("import_theme_items", TTR("Importing items {n}/{n}").format(arr, "{n}"), idx);
		}

		ItemCheckedState cs = E->get();
		ThemeItem ti = E->key();

		if (cs == SELECT_IMPORT_DEFINITION || cs == SELECT_IMPORT_FULL) {
			Variant item_value = Variant();

			if (cs == SELECT_IMPORT_FULL) {
				item_value = base_theme->get_theme_item(ti.data_type, ti.item_name, ti.type_name);
			} else {
				switch (ti.data_type) {
					case Theme::DATA_TYPE_COLOR:
						item_value = Color();
						break;

					case Theme::DATA_TYPE_CONSTANT:
						item_value = 0;
						break;

					case Theme::DATA_TYPE_FONT:
						item_value = Ref<Font>();
						break;

					case Theme::DATA_TYPE_ICON:
						item_value = Ref<Texture>();
						break;

					case Theme::DATA_TYPE_STYLEBOX:
						item_value = Ref<StyleBox>();
						break;

					case Theme::DATA_TYPE_MAX:
						break; // Can't happen, but silences warning.
				}
			}

			edited_theme->set_theme_item(ti.data_type, ti.item_name, ti.type_name, item_value);
		}

		idx++;
	}

	// Allow changes to be reported now that the operation is finished.
	ProgressDialog::get_singleton()->task_step("import_theme_items", TTR("Updating the editor"), idx++);
	edited_theme->_unfreeze_and_propagate_changes();
	// Make sure the task is not ended before the editor freezes to update the Inspector.
	ProgressDialog::get_singleton()->task_step("import_theme_items", TTR("Finalizing"), idx++);

	ProgressDialog::get_singleton()->end_task("import_theme_items");
	emit_signal("items_imported");
}

void ThemeItemImportTree::set_edited_theme(const Ref<Theme> &p_theme) {
	edited_theme = p_theme;
}

void ThemeItemImportTree::set_base_theme(const Ref<Theme> &p_theme) {
	base_theme = p_theme;
}

void ThemeItemImportTree::reset_item_tree() {
	import_items_filter->clear();
	selected_items.clear();

	total_selected_colors_label->hide();
	total_selected_constants_label->hide();
	total_selected_fonts_label->hide();
	total_selected_icons_label->hide();
	total_selected_styleboxes_label->hide();

	_update_items_tree();
}

bool ThemeItemImportTree::has_selected_items() const {
	return (selected_items.size() > 0);
}

void ThemeItemImportTree::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			select_icons_warning_icon->set_texture(get_icon("StatusWarning", "EditorIcons"));
			select_icons_warning->add_color_override("font_color", get_color("disabled_font_color", "Editor"));

			// Bottom panel buttons.
			import_collapse_types_button->set_icon(get_icon("CollapseTree", "EditorIcons"));
			import_expand_types_button->set_icon(get_icon("ExpandTree", "EditorIcons"));

			import_select_all_button->set_icon(get_icon("ThemeSelectAll", "EditorIcons"));
			import_select_full_button->set_icon(get_icon("ThemeSelectFull", "EditorIcons"));
			import_deselect_all_button->set_icon(get_icon("ThemeDeselectAll", "EditorIcons"));

			// Side panel buttons.
			select_colors_icon->set_texture(get_icon("Color", "EditorIcons"));
			deselect_all_colors_button->set_icon(get_icon("ThemeDeselectAll", "EditorIcons"));
			select_all_colors_button->set_icon(get_icon("ThemeSelectAll", "EditorIcons"));
			select_full_colors_button->set_icon(get_icon("ThemeSelectFull", "EditorIcons"));

			select_constants_icon->set_texture(get_icon("MemberConstant", "EditorIcons"));
			deselect_all_constants_button->set_icon(get_icon("ThemeDeselectAll", "EditorIcons"));
			select_all_constants_button->set_icon(get_icon("ThemeSelectAll", "EditorIcons"));
			select_full_constants_button->set_icon(get_icon("ThemeSelectFull", "EditorIcons"));

			select_fonts_icon->set_texture(get_icon("Font", "EditorIcons"));
			deselect_all_fonts_button->set_icon(get_icon("ThemeDeselectAll", "EditorIcons"));
			select_all_fonts_button->set_icon(get_icon("ThemeSelectAll", "EditorIcons"));
			select_full_fonts_button->set_icon(get_icon("ThemeSelectFull", "EditorIcons"));

			select_icons_icon->set_texture(get_icon("ImageTexture", "EditorIcons"));
			deselect_all_icons_button->set_icon(get_icon("ThemeDeselectAll", "EditorIcons"));
			select_all_icons_button->set_icon(get_icon("ThemeSelectAll", "EditorIcons"));
			select_full_icons_button->set_icon(get_icon("ThemeSelectFull", "EditorIcons"));

			select_styleboxes_icon->set_texture(get_icon("StyleBoxFlat", "EditorIcons"));
			deselect_all_styleboxes_button->set_icon(get_icon("ThemeDeselectAll", "EditorIcons"));
			select_all_styleboxes_button->set_icon(get_icon("ThemeSelectAll", "EditorIcons"));
			select_full_styleboxes_button->set_icon(get_icon("ThemeSelectFull", "EditorIcons"));
		} break;
	}
}

void ThemeItemImportTree::_bind_methods() {
	// Internal binds.
	ClassDB::bind_method("_filter_text_changed", &ThemeItemImportTree::_filter_text_changed);
	ClassDB::bind_method("_tree_item_edited", &ThemeItemImportTree::_tree_item_edited);
	ClassDB::bind_method("_select_all_data_type_pressed", &ThemeItemImportTree::_select_all_data_type_pressed);
	ClassDB::bind_method("_select_full_data_type_pressed", &ThemeItemImportTree::_select_full_data_type_pressed);
	ClassDB::bind_method("_deselect_all_data_type_pressed", &ThemeItemImportTree::_deselect_all_data_type_pressed);
	ClassDB::bind_method("_toggle_type_items", &ThemeItemImportTree::_toggle_type_items);
	ClassDB::bind_method("_select_all_items_pressed", &ThemeItemImportTree::_select_all_items_pressed);
	ClassDB::bind_method("_select_full_items_pressed", &ThemeItemImportTree::_select_full_items_pressed);
	ClassDB::bind_method("_deselect_all_items_pressed", &ThemeItemImportTree::_deselect_all_items_pressed);
	ClassDB::bind_method("_import_selected", &ThemeItemImportTree::_import_selected);

	// Public binds.
	ADD_SIGNAL(MethodInfo("items_imported"));
}

ThemeItemImportTree::ThemeItemImportTree() {
	HBoxContainer *import_items_filter_hb = memnew(HBoxContainer);
	add_child(import_items_filter_hb);
	Label *import_items_filter_label = memnew(Label);
	import_items_filter_label->set_text(TTR("Filter:"));
	import_items_filter_hb->add_child(import_items_filter_label);
	import_items_filter = memnew(LineEdit);
	import_items_filter->set_clear_button_enabled(true);
	import_items_filter->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	import_items_filter_hb->add_child(import_items_filter);
	import_items_filter->connect("text_changed", this, "_filter_text_changed");

	HBoxContainer *import_main_hb = memnew(HBoxContainer);
	import_main_hb->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(import_main_hb);

	import_items_tree = memnew(Tree);
	import_items_tree->set_hide_root(true);
	import_items_tree->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	import_main_hb->add_child(import_items_tree);
	import_items_tree->connect("item_edited", this, "_tree_item_edited");

	import_items_tree->set_columns(3);
	import_items_tree->set_column_titles_visible(true);
	import_items_tree->set_column_title(IMPORT_ITEM, TTR("Import"));
	import_items_tree->set_column_title(IMPORT_ITEM_DATA, TTR("With Data"));
	import_items_tree->set_column_expand(0, true);
	import_items_tree->set_column_expand(IMPORT_ITEM, false);
	import_items_tree->set_column_expand(IMPORT_ITEM_DATA, false);
	import_items_tree->set_column_min_width(0, 160 * EDSCALE);
	import_items_tree->set_column_min_width(IMPORT_ITEM, 80 * EDSCALE);
	import_items_tree->set_column_min_width(IMPORT_ITEM_DATA, 80 * EDSCALE);

	ScrollContainer *import_bulk_sc = memnew(ScrollContainer);
	import_bulk_sc->set_custom_minimum_size(Size2(260.0, 0.0) * EDSCALE);
	import_bulk_sc->set_enable_h_scroll(false);
	import_main_hb->add_child(import_bulk_sc);
	VBoxContainer *import_bulk_vb = memnew(VBoxContainer);
	import_bulk_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	import_bulk_sc->add_child(import_bulk_vb);

	Label *import_bulk_label = memnew(Label);
	import_bulk_label->set_text(TTR("Select by data type:"));
	import_bulk_vb->add_child(import_bulk_label);

	select_colors_icon = memnew(TextureRect);
	select_colors_label = memnew(Label);
	deselect_all_colors_button = memnew(Button);
	select_all_colors_button = memnew(Button);
	select_full_colors_button = memnew(Button);
	total_selected_colors_label = memnew(Label);

	select_constants_icon = memnew(TextureRect);
	select_constants_label = memnew(Label);
	deselect_all_constants_button = memnew(Button);
	select_all_constants_button = memnew(Button);
	select_full_constants_button = memnew(Button);
	total_selected_constants_label = memnew(Label);

	select_fonts_icon = memnew(TextureRect);
	select_fonts_label = memnew(Label);
	deselect_all_fonts_button = memnew(Button);
	select_all_fonts_button = memnew(Button);
	select_full_fonts_button = memnew(Button);
	total_selected_fonts_label = memnew(Label);

	select_icons_icon = memnew(TextureRect);
	select_icons_label = memnew(Label);
	deselect_all_icons_button = memnew(Button);
	select_all_icons_button = memnew(Button);
	select_full_icons_button = memnew(Button);
	total_selected_icons_label = memnew(Label);

	select_styleboxes_icon = memnew(TextureRect);
	select_styleboxes_label = memnew(Label);
	deselect_all_styleboxes_button = memnew(Button);
	select_all_styleboxes_button = memnew(Button);
	select_full_styleboxes_button = memnew(Button);
	total_selected_styleboxes_label = memnew(Label);

	for (int i = 0; i < Theme::DATA_TYPE_MAX; i++) {
		Theme::DataType dt = (Theme::DataType)i;

		TextureRect *select_items_icon;
		Label *select_items_label;
		Button *deselect_all_items_button;
		Button *select_all_items_button;
		Button *select_full_items_button;
		Label *total_selected_items_label;

		String items_title = "";
		String select_all_items_tooltip = "";
		String select_full_items_tooltip = "";
		String deselect_all_items_tooltip = "";

		switch (dt) {
			case Theme::DATA_TYPE_COLOR:
				select_items_icon = select_colors_icon;
				select_items_label = select_colors_label;
				deselect_all_items_button = deselect_all_colors_button;
				select_all_items_button = select_all_colors_button;
				select_full_items_button = select_full_colors_button;
				total_selected_items_label = total_selected_colors_label;

				items_title = TTR("Colors");
				select_all_items_tooltip = TTR("Select all visible color items.");
				select_full_items_tooltip = TTR("Select all visible color items and their data.");
				deselect_all_items_tooltip = TTR("Deselect all visible color items.");
				break;

			case Theme::DATA_TYPE_CONSTANT:
				select_items_icon = select_constants_icon;
				select_items_label = select_constants_label;
				deselect_all_items_button = deselect_all_constants_button;
				select_all_items_button = select_all_constants_button;
				select_full_items_button = select_full_constants_button;
				total_selected_items_label = total_selected_constants_label;

				items_title = TTR("Constants");
				select_all_items_tooltip = TTR("Select all visible constant items.");
				select_full_items_tooltip = TTR("Select all visible constant items and their data.");
				deselect_all_items_tooltip = TTR("Deselect all visible constant items.");
				break;

			case Theme::DATA_TYPE_FONT:
				select_items_icon = select_fonts_icon;
				select_items_label = select_fonts_label;
				deselect_all_items_button = deselect_all_fonts_button;
				select_all_items_button = select_all_fonts_button;
				select_full_items_button = select_full_fonts_button;
				total_selected_items_label = total_selected_fonts_label;

				items_title = TTR("Fonts");
				select_all_items_tooltip = TTR("Select all visible font items.");
				select_full_items_tooltip = TTR("Select all visible font items and their data.");
				deselect_all_items_tooltip = TTR("Deselect all visible font items.");
				break;

			case Theme::DATA_TYPE_ICON:
				select_items_icon = select_icons_icon;
				select_items_label = select_icons_label;
				deselect_all_items_button = deselect_all_icons_button;
				select_all_items_button = select_all_icons_button;
				select_full_items_button = select_full_icons_button;
				total_selected_items_label = total_selected_icons_label;

				items_title = TTR("Icons");
				select_all_items_tooltip = TTR("Select all visible icon items.");
				select_full_items_tooltip = TTR("Select all visible icon items and their data.");
				deselect_all_items_tooltip = TTR("Deselect all visible icon items.");
				break;

			case Theme::DATA_TYPE_STYLEBOX:
				select_items_icon = select_styleboxes_icon;
				select_items_label = select_styleboxes_label;
				deselect_all_items_button = deselect_all_styleboxes_button;
				select_all_items_button = select_all_styleboxes_button;
				select_full_items_button = select_full_styleboxes_button;
				total_selected_items_label = total_selected_styleboxes_label;

				items_title = TTR("Styleboxes");
				select_all_items_tooltip = TTR("Select all visible stylebox items.");
				select_full_items_tooltip = TTR("Select all visible stylebox items and their data.");
				deselect_all_items_tooltip = TTR("Deselect all visible stylebox items.");
				break;

			case Theme::DATA_TYPE_MAX:
				continue; // Can't happen, but silences warning.
		}

		if (i > 0) {
			import_bulk_vb->add_child(memnew(HSeparator));
		}

		HBoxContainer *all_set = memnew(HBoxContainer);
		import_bulk_vb->add_child(all_set);

		HBoxContainer *label_set = memnew(HBoxContainer);
		label_set->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		all_set->add_child(label_set);
		select_items_icon->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
		label_set->add_child(select_items_icon);
		select_items_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		select_items_label->set_clip_text(true);
		select_items_label->set_text(items_title);
		label_set->add_child(select_items_label);

		HBoxContainer *button_set = memnew(HBoxContainer);
		button_set->set_alignment(BoxContainer::ALIGN_END);
		all_set->add_child(button_set);
		select_all_items_button->set_flat(true);
		select_all_items_button->set_tooltip(select_all_items_tooltip);
		button_set->add_child(select_all_items_button);
		select_all_items_button->connect("pressed", this, "_select_all_data_type_pressed", varray(i));
		select_full_items_button->set_flat(true);
		select_full_items_button->set_tooltip(select_full_items_tooltip);
		button_set->add_child(select_full_items_button);
		select_full_items_button->connect("pressed", this, "_select_full_data_type_pressed", varray(i));
		deselect_all_items_button->set_flat(true);
		deselect_all_items_button->set_tooltip(deselect_all_items_tooltip);
		button_set->add_child(deselect_all_items_button);
		deselect_all_items_button->connect("pressed", this, "_deselect_all_data_type_pressed", varray(i));

		total_selected_items_label->set_align(Label::ALIGN_RIGHT);
		total_selected_items_label->hide();
		import_bulk_vb->add_child(total_selected_items_label);

		if (dt == Theme::DATA_TYPE_ICON) {
			select_icons_warning_hb = memnew(HBoxContainer);
			import_bulk_vb->add_child(select_icons_warning_hb);

			select_icons_warning_icon = memnew(TextureRect);
			select_icons_warning_icon->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
			select_icons_warning_hb->add_child(select_icons_warning_icon);

			select_icons_warning = memnew(Label);
			select_icons_warning->set_text(TTR("Caution: Adding icon data may considerably increase the size of your Theme resource."));
			select_icons_warning->set_autowrap(true);
			select_icons_warning->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			select_icons_warning_hb->add_child(select_icons_warning);
		}
	}

	add_child(memnew(HSeparator));

	HBoxContainer *import_buttons = memnew(HBoxContainer);
	add_child(import_buttons);

	import_collapse_types_button = memnew(Button);
	import_collapse_types_button->set_flat(true);
	import_collapse_types_button->set_tooltip(TTR("Collapse types."));
	import_buttons->add_child(import_collapse_types_button);
	import_collapse_types_button->connect("pressed", this, "_toggle_type_items", varray(true));
	import_expand_types_button = memnew(Button);
	import_expand_types_button->set_flat(true);
	import_expand_types_button->set_tooltip(TTR("Expand types."));
	import_buttons->add_child(import_expand_types_button);
	import_expand_types_button->connect("pressed", this, "_toggle_type_items", varray(false));

	import_buttons->add_child(memnew(VSeparator));

	import_select_all_button = memnew(Button);
	import_select_all_button->set_flat(true);
	import_select_all_button->set_text(TTR("Select All"));
	import_select_all_button->set_tooltip(TTR("Select all Theme items."));
	import_buttons->add_child(import_select_all_button);
	import_select_all_button->connect("pressed", this, "_select_all_items_pressed");
	import_select_full_button = memnew(Button);
	import_select_full_button->set_flat(true);
	import_select_full_button->set_text(TTR("Select With Data"));
	import_select_full_button->set_tooltip(TTR("Select all Theme items with item data."));
	import_buttons->add_child(import_select_full_button);
	import_select_full_button->connect("pressed", this, "_select_full_items_pressed");
	import_deselect_all_button = memnew(Button);
	import_deselect_all_button->set_flat(true);
	import_deselect_all_button->set_text(TTR("Deselect All"));
	import_deselect_all_button->set_tooltip(TTR("Deselect all Theme items."));
	import_buttons->add_child(import_deselect_all_button);
	import_deselect_all_button->connect("pressed", this, "_deselect_all_items_pressed");

	import_buttons->add_spacer();

	Button *import_add_selected_button = memnew(Button);
	import_add_selected_button->set_text(TTR("Import Selected"));
	import_buttons->add_child(import_add_selected_button);
	import_add_selected_button->connect("pressed", this, "_import_selected");
}

void ThemeItemEditorDialog::ok_pressed() {
	if (import_default_theme_items->has_selected_items() || import_editor_theme_items->has_selected_items() || import_other_theme_items->has_selected_items()) {
		confirm_closing_dialog->set_text(TTR("Import Items tab has some items selected. Selection will be lost upon closing this window.\nClose anyway?"));
		confirm_closing_dialog->popup_centered(Size2i(380, 120) * EDSCALE);
		return;
	}

	hide();
}

void ThemeItemEditorDialog::_close_dialog() {
	hide();
}

void ThemeItemEditorDialog::_dialog_about_to_show() {
	ERR_FAIL_COND_MSG(edited_theme.is_null(), "Invalid state of the Theme Editor; the Theme resource is missing.");

	_update_edit_types();

	import_default_theme_items->set_edited_theme(edited_theme);
	import_default_theme_items->set_base_theme(Theme::get_default());
	import_default_theme_items->reset_item_tree();

	import_editor_theme_items->set_edited_theme(edited_theme);
	import_editor_theme_items->set_base_theme(EditorNode::get_singleton()->get_theme_base()->get_theme());
	import_editor_theme_items->reset_item_tree();

	import_other_theme_items->set_edited_theme(edited_theme);
	import_other_theme_items->reset_item_tree();
}

void ThemeItemEditorDialog::_update_edit_types() {
	Ref<Theme> base_theme = Theme::get_default();

	List<StringName> theme_types;
	edited_theme->get_type_list(&theme_types);
	theme_types.sort_custom<StringName::AlphCompare>();

	bool item_reselected = false;
	edit_type_list->clear();
	TreeItem *list_root = edit_type_list->create_item();

	for (List<StringName>::Element *E = theme_types.front(); E; E = E->next()) {
		Ref<Texture> item_icon;
		if (E->get() == "") {
			item_icon = get_icon("NodeDisabled", "EditorIcons");
		} else {
			item_icon = EditorNode::get_singleton()->get_class_icon(E->get(), "NodeDisabled");
		}
		TreeItem *list_item = edit_type_list->create_item(list_root);
		list_item->set_text(0, E->get());
		list_item->set_icon(0, item_icon);
		list_item->add_button(0, get_icon("Remove", "EditorIcons"), TYPES_TREE_REMOVE_ITEM, false, TTR("Remove Type"));

		if (E->get() == edited_item_type) {
			list_item->select(0);
			item_reselected = true;
		}
	}
	if (!item_reselected) {
		edited_item_type = "";

		TreeItem *ci = list_root->get_children();
		if (ci) {
			ci->select(0);
		}
	}

	List<StringName> default_types;
	base_theme->get_type_list(&default_types);
	default_types.sort_custom<StringName::AlphCompare>();

	String selected_type = "";
	TreeItem *selected_item = edit_type_list->get_selected();
	if (selected_item) {
		selected_type = selected_item->get_text(0);

		edit_items_add_color->set_disabled(false);
		edit_items_add_constant->set_disabled(false);
		edit_items_add_font->set_disabled(false);
		edit_items_add_icon->set_disabled(false);
		edit_items_add_stylebox->set_disabled(false);

		edit_items_remove_class->set_disabled(false);
		edit_items_remove_custom->set_disabled(false);
		edit_items_remove_all->set_disabled(false);

		edit_items_message->set_text("");
		edit_items_message->hide();
	} else {
		edit_items_add_color->set_disabled(true);
		edit_items_add_constant->set_disabled(true);
		edit_items_add_font->set_disabled(true);
		edit_items_add_icon->set_disabled(true);
		edit_items_add_stylebox->set_disabled(true);

		edit_items_remove_class->set_disabled(true);
		edit_items_remove_custom->set_disabled(true);
		edit_items_remove_all->set_disabled(true);

		edit_items_message->set_text(TTR("Select a theme type from the list to edit its items.\nYou can add a custom type or import a type with its items from another theme."));
		edit_items_message->show();
	}
	_update_edit_item_tree(selected_type);
}

void ThemeItemEditorDialog::_edited_type_selected() {
	TreeItem *selected_item = edit_type_list->get_selected();
	String selected_type = selected_item->get_text(0);
	_update_edit_item_tree(selected_type);
}

void ThemeItemEditorDialog::_edited_type_button_pressed(Object *p_item, int p_column, int p_id) {
	TreeItem *item = Object::cast_to<TreeItem>(p_item);
	if (!item) {
		return;
	}

	switch (p_id) {
		case TYPES_TREE_REMOVE_ITEM: {
			String type_name = item->get_text(0);
			_remove_theme_type(type_name);
		} break;
	}
}

void ThemeItemEditorDialog::_update_edit_item_tree(String p_item_type) {
	edited_item_type = p_item_type;

	edit_items_tree->clear();
	TreeItem *root = edit_items_tree->create_item();

	List<StringName> names;
	bool has_any_items = false;

	{ // Colors.
		names.clear();
		edited_theme->get_color_list(p_item_type, &names);

		if (names.size() > 0) {
			TreeItem *color_root = edit_items_tree->create_item(root);
			color_root->set_metadata(0, Theme::DATA_TYPE_COLOR);
			color_root->set_icon(0, get_icon("Color", "EditorIcons"));
			color_root->set_text(0, TTR("Colors"));
			color_root->add_button(0, get_icon("Clear", "EditorIcons"), ITEMS_TREE_REMOVE_DATA_TYPE, false, TTR("Remove All Color Items"));

			names.sort_custom<StringName::AlphCompare>();
			for (List<StringName>::Element *E = names.front(); E; E = E->next()) {
				TreeItem *item = edit_items_tree->create_item(color_root);
				item->set_text(0, E->get());
				item->add_button(0, get_icon("Edit", "EditorIcons"), ITEMS_TREE_RENAME_ITEM, false, TTR("Rename Item"));
				item->add_button(0, get_icon("Remove", "EditorIcons"), ITEMS_TREE_REMOVE_ITEM, false, TTR("Remove Item"));
			}

			has_any_items = true;
		}
	}

	{ // Constants.
		names.clear();
		edited_theme->get_constant_list(p_item_type, &names);

		if (names.size() > 0) {
			TreeItem *constant_root = edit_items_tree->create_item(root);
			constant_root->set_metadata(0, Theme::DATA_TYPE_CONSTANT);
			constant_root->set_icon(0, get_icon("MemberConstant", "EditorIcons"));
			constant_root->set_text(0, TTR("Constants"));
			constant_root->add_button(0, get_icon("Clear", "EditorIcons"), ITEMS_TREE_REMOVE_DATA_TYPE, false, TTR("Remove All Constant Items"));

			names.sort_custom<StringName::AlphCompare>();
			for (List<StringName>::Element *E = names.front(); E; E = E->next()) {
				TreeItem *item = edit_items_tree->create_item(constant_root);
				item->set_text(0, E->get());
				item->add_button(0, get_icon("Edit", "EditorIcons"), ITEMS_TREE_RENAME_ITEM, false, TTR("Rename Item"));
				item->add_button(0, get_icon("Remove", "EditorIcons"), ITEMS_TREE_REMOVE_ITEM, false, TTR("Remove Item"));
			}

			has_any_items = true;
		}
	}

	{ // Fonts.
		names.clear();
		edited_theme->get_font_list(p_item_type, &names);

		if (names.size() > 0) {
			TreeItem *font_root = edit_items_tree->create_item(root);
			font_root->set_metadata(0, Theme::DATA_TYPE_FONT);
			font_root->set_icon(0, get_icon("Font", "EditorIcons"));
			font_root->set_text(0, TTR("Fonts"));
			font_root->add_button(0, get_icon("Clear", "EditorIcons"), ITEMS_TREE_REMOVE_DATA_TYPE, false, TTR("Remove All Font Items"));

			names.sort_custom<StringName::AlphCompare>();
			for (List<StringName>::Element *E = names.front(); E; E = E->next()) {
				TreeItem *item = edit_items_tree->create_item(font_root);
				item->set_text(0, E->get());
				item->add_button(0, get_icon("Edit", "EditorIcons"), ITEMS_TREE_RENAME_ITEM, false, TTR("Rename Item"));
				item->add_button(0, get_icon("Remove", "EditorIcons"), ITEMS_TREE_REMOVE_ITEM, false, TTR("Remove Item"));
			}

			has_any_items = true;
		}
	}

	{ // Icons.
		names.clear();
		edited_theme->get_icon_list(p_item_type, &names);

		if (names.size() > 0) {
			TreeItem *icon_root = edit_items_tree->create_item(root);
			icon_root->set_metadata(0, Theme::DATA_TYPE_ICON);
			icon_root->set_icon(0, get_icon("ImageTexture", "EditorIcons"));
			icon_root->set_text(0, TTR("Icons"));
			icon_root->add_button(0, get_icon("Clear", "EditorIcons"), ITEMS_TREE_REMOVE_DATA_TYPE, false, TTR("Remove All Icon Items"));

			names.sort_custom<StringName::AlphCompare>();
			for (List<StringName>::Element *E = names.front(); E; E = E->next()) {
				TreeItem *item = edit_items_tree->create_item(icon_root);
				item->set_text(0, E->get());
				item->add_button(0, get_icon("Edit", "EditorIcons"), ITEMS_TREE_RENAME_ITEM, false, TTR("Rename Item"));
				item->add_button(0, get_icon("Remove", "EditorIcons"), ITEMS_TREE_REMOVE_ITEM, false, TTR("Remove Item"));
			}

			has_any_items = true;
		}
	}

	{ // Styleboxes.
		names.clear();
		edited_theme->get_stylebox_list(p_item_type, &names);

		if (names.size() > 0) {
			TreeItem *stylebox_root = edit_items_tree->create_item(root);
			stylebox_root->set_metadata(0, Theme::DATA_TYPE_STYLEBOX);
			stylebox_root->set_icon(0, get_icon("StyleBoxFlat", "EditorIcons"));
			stylebox_root->set_text(0, TTR("Styleboxes"));
			stylebox_root->add_button(0, get_icon("Clear", "EditorIcons"), ITEMS_TREE_REMOVE_DATA_TYPE, false, TTR("Remove All StyleBox Items"));

			names.sort_custom<StringName::AlphCompare>();
			for (List<StringName>::Element *E = names.front(); E; E = E->next()) {
				TreeItem *item = edit_items_tree->create_item(stylebox_root);
				item->set_text(0, E->get());
				item->add_button(0, get_icon("Edit", "EditorIcons"), ITEMS_TREE_RENAME_ITEM, false, TTR("Rename Item"));
				item->add_button(0, get_icon("Remove", "EditorIcons"), ITEMS_TREE_REMOVE_ITEM, false, TTR("Remove Item"));
			}

			has_any_items = true;
		}
	}

	// If some type is selected, but it doesn't seem to have any items, show a guiding message.
	TreeItem *selected_item = edit_type_list->get_selected();
	if (selected_item) {
		if (!has_any_items) {
			edit_items_message->set_text(TTR("This theme type is empty.\nAdd more items to it manually or by importing from another theme."));
			edit_items_message->show();
		} else {
			edit_items_message->set_text("");
			edit_items_message->hide();
		}
	}
}

void ThemeItemEditorDialog::_item_tree_button_pressed(Object *p_item, int p_column, int p_id) {
	TreeItem *item = Object::cast_to<TreeItem>(p_item);
	if (!item) {
		return;
	}

	switch (p_id) {
		case ITEMS_TREE_RENAME_ITEM: {
			String item_name = item->get_text(0);
			int data_type = item->get_parent()->get_metadata(0);
			_open_rename_theme_item_dialog((Theme::DataType)data_type, item_name);
		} break;
		case ITEMS_TREE_REMOVE_ITEM: {
			String item_name = item->get_text(0);
			int data_type = item->get_parent()->get_metadata(0);
			edited_theme->clear_theme_item((Theme::DataType)data_type, item_name, edited_item_type);
		} break;
		case ITEMS_TREE_REMOVE_DATA_TYPE: {
			int data_type = item->get_metadata(0);
			_remove_data_type_items((Theme::DataType)data_type, edited_item_type);
		} break;
	}

	_update_edit_item_tree(edited_item_type);
}

void ThemeItemEditorDialog::_add_theme_type(const String &p_new_text) {
	const String new_type = edit_add_type_value->get_text().strip_edges();
	edit_add_type_value->clear();

	UndoRedo *ur = EditorNode::get_singleton()->get_undo_redo();

	ur->create_action(TTR("Add Theme Type"));
	ur->add_do_method(*edited_theme, "add_type", new_type);
	ur->add_undo_method(*edited_theme, "remove_type", new_type);
	ur->add_do_method(this, "_update_edit_types");
	ur->add_undo_method(this, "_update_edit_types");

	ur->commit_action();
}

void ThemeItemEditorDialog::_remove_theme_type(const String &p_theme_type) {
	Ref<Theme> old_snapshot = edited_theme->duplicate();
	Ref<Theme> new_snapshot = edited_theme->duplicate();

	UndoRedo *ur = EditorNode::get_singleton()->get_undo_redo();
	ur->create_action(TTR("Remove Theme Type"));

	new_snapshot->remove_type(p_theme_type);

	ur->add_do_method(*edited_theme, "clear");
	ur->add_do_method(*edited_theme, "merge_with", new_snapshot);
	// If the type was empty, it cannot be restored with merge, but thankfully we can fake it.
	ur->add_undo_method(*edited_theme, "add_type", p_theme_type);
	ur->add_undo_method(*edited_theme, "merge_with", old_snapshot);

	ur->add_do_method(this, "_update_edit_types");
	ur->add_undo_method(this, "_update_edit_types");

	ur->commit_action();
}

void ThemeItemEditorDialog::_add_theme_item(Theme::DataType p_data_type, String p_item_name, String p_item_type) {
	switch (p_data_type) {
		case Theme::DATA_TYPE_ICON:
			edited_theme->set_icon(p_item_name, p_item_type, Ref<Texture>());
			break;
		case Theme::DATA_TYPE_STYLEBOX:
			edited_theme->set_stylebox(p_item_name, p_item_type, Ref<StyleBox>());
			break;
		case Theme::DATA_TYPE_FONT:
			edited_theme->set_font(p_item_name, p_item_type, Ref<Font>());
			break;
		case Theme::DATA_TYPE_COLOR:
			edited_theme->set_color(p_item_name, p_item_type, Color());
			break;
		case Theme::DATA_TYPE_CONSTANT:
			edited_theme->set_constant(p_item_name, p_item_type, 0);
			break;
		case Theme::DATA_TYPE_MAX:
			break; // Can't happen, but silences warning.
	}
}

void ThemeItemEditorDialog::_remove_data_type_items(Theme::DataType p_data_type, String p_item_type) {
	List<StringName> names;

	// Prevent changes from immediately being reported while the operation is still ongoing.
	edited_theme->_freeze_change_propagation();

	edited_theme->get_theme_item_list(p_data_type, p_item_type, &names);
	for (List<StringName>::Element *E = names.front(); E; E = E->next()) {
		edited_theme->clear_theme_item(p_data_type, E->get(), p_item_type);
	}

	// Allow changes to be reported now that the operation is finished.
	edited_theme->_unfreeze_and_propagate_changes();
}

void ThemeItemEditorDialog::_remove_class_items() {
	List<StringName> names;

	// Prevent changes from immediately being reported while the operation is still ongoing.
	edited_theme->_freeze_change_propagation();

	for (int dt = 0; dt < Theme::DATA_TYPE_MAX; dt++) {
		Theme::DataType data_type = (Theme::DataType)dt;

		names.clear();
		Theme::get_default()->get_theme_item_list(data_type, edited_item_type, &names);
		for (List<StringName>::Element *E = names.front(); E; E = E->next()) {
			if (edited_theme->has_theme_item_nocheck(data_type, E->get(), edited_item_type)) {
				edited_theme->clear_theme_item(data_type, E->get(), edited_item_type);
			}
		}
	}

	// Allow changes to be reported now that the operation is finished.
	edited_theme->_unfreeze_and_propagate_changes();

	_update_edit_item_tree(edited_item_type);
}

void ThemeItemEditorDialog::_remove_custom_items() {
	List<StringName> names;

	// Prevent changes from immediately being reported while the operation is still ongoing.
	edited_theme->_freeze_change_propagation();

	for (int dt = 0; dt < Theme::DATA_TYPE_MAX; dt++) {
		Theme::DataType data_type = (Theme::DataType)dt;

		names.clear();
		edited_theme->get_theme_item_list(data_type, edited_item_type, &names);
		for (List<StringName>::Element *E = names.front(); E; E = E->next()) {
			if (!Theme::get_default()->has_theme_item_nocheck(data_type, E->get(), edited_item_type)) {
				edited_theme->clear_theme_item(data_type, E->get(), edited_item_type);
			}
		}
	}

	// Allow changes to be reported now that the operation is finished.
	edited_theme->_unfreeze_and_propagate_changes();

	_update_edit_item_tree(edited_item_type);
}

void ThemeItemEditorDialog::_remove_all_items() {
	List<StringName> names;

	// Prevent changes from immediately being reported while the operation is still ongoing.
	edited_theme->_freeze_change_propagation();

	for (int dt = 0; dt < Theme::DATA_TYPE_MAX; dt++) {
		Theme::DataType data_type = (Theme::DataType)dt;

		names.clear();
		edited_theme->get_theme_item_list(data_type, edited_item_type, &names);
		for (List<StringName>::Element *E = names.front(); E; E = E->next()) {
			edited_theme->clear_theme_item(data_type, E->get(), edited_item_type);
		}
	}

	// Allow changes to be reported now that the operation is finished.
	edited_theme->_unfreeze_and_propagate_changes();

	_update_edit_item_tree(edited_item_type);
}

void ThemeItemEditorDialog::_open_add_theme_item_dialog(int p_data_type) {
	ERR_FAIL_INDEX_MSG(p_data_type, Theme::DATA_TYPE_MAX, "Theme item data type is out of bounds.");

	item_popup_mode = CREATE_THEME_ITEM;
	edit_item_data_type = (Theme::DataType)p_data_type;

	switch (edit_item_data_type) {
		case Theme::DATA_TYPE_COLOR:
			edit_theme_item_dialog->set_title(TTR("Add Color Item"));
			break;
		case Theme::DATA_TYPE_CONSTANT:
			edit_theme_item_dialog->set_title(TTR("Add Constant Item"));
			break;
		case Theme::DATA_TYPE_FONT:
			edit_theme_item_dialog->set_title(TTR("Add Font Item"));
			break;
		case Theme::DATA_TYPE_ICON:
			edit_theme_item_dialog->set_title(TTR("Add Icon Item"));
			break;
		case Theme::DATA_TYPE_STYLEBOX:
			edit_theme_item_dialog->set_title(TTR("Add Stylebox Item"));
			break;
		case Theme::DATA_TYPE_MAX:
			break; // Can't happen, but silences warning.
	}

	edit_theme_item_old_vb->hide();
	theme_item_name->clear();
	edit_theme_item_dialog->popup_centered(Size2(380, 110) * EDSCALE);
	theme_item_name->grab_focus();
}

void ThemeItemEditorDialog::_open_rename_theme_item_dialog(Theme::DataType p_data_type, String p_item_name) {
	ERR_FAIL_INDEX_MSG(p_data_type, Theme::DATA_TYPE_MAX, "Theme item data type is out of bounds.");

	item_popup_mode = RENAME_THEME_ITEM;
	edit_item_data_type = p_data_type;
	edit_item_old_name = p_item_name;

	switch (edit_item_data_type) {
		case Theme::DATA_TYPE_COLOR:
			edit_theme_item_dialog->set_title(TTR("Rename Color Item"));
			break;
		case Theme::DATA_TYPE_CONSTANT:
			edit_theme_item_dialog->set_title(TTR("Rename Constant Item"));
			break;
		case Theme::DATA_TYPE_FONT:
			edit_theme_item_dialog->set_title(TTR("Rename Font Item"));
			break;
		case Theme::DATA_TYPE_ICON:
			edit_theme_item_dialog->set_title(TTR("Rename Icon Item"));
			break;
		case Theme::DATA_TYPE_STYLEBOX:
			edit_theme_item_dialog->set_title(TTR("Rename Stylebox Item"));
			break;
		case Theme::DATA_TYPE_MAX:
			break; // Can't happen, but silences warning.
	}

	edit_theme_item_old_vb->show();
	theme_item_old_name->set_text(p_item_name);
	theme_item_name->set_text(p_item_name);
	edit_theme_item_dialog->popup_centered(Size2(380, 140) * EDSCALE);
	theme_item_name->grab_focus();
}

void ThemeItemEditorDialog::_confirm_edit_theme_item() {
	if (item_popup_mode == CREATE_THEME_ITEM) {
		_add_theme_item(edit_item_data_type, theme_item_name->get_text(), edited_item_type);
	} else if (item_popup_mode == RENAME_THEME_ITEM) {
		edited_theme->rename_theme_item(edit_item_data_type, edit_item_old_name, theme_item_name->get_text(), edited_item_type);
	}

	item_popup_mode = ITEM_POPUP_MODE_MAX;
	edit_item_data_type = Theme::DATA_TYPE_MAX;
	edit_item_old_name = "";

	_update_edit_item_tree(edited_item_type);
}

void ThemeItemEditorDialog::_edit_theme_item_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> k = p_event;

	if (k.is_valid()) {
		if (!k->is_pressed()) {
			return;
		}

		switch (k->get_scancode()) {
			case KEY_KP_ENTER:
			case KEY_ENTER: {
				_confirm_edit_theme_item();
				edit_theme_item_dialog->hide();
				get_tree()->set_input_as_handled();
			} break;
			case KEY_ESCAPE: {
				edit_theme_item_dialog->hide();
				get_tree()->set_input_as_handled();
			} break;
		}
	}
}

void ThemeItemEditorDialog::_open_select_another_theme() {
	import_another_theme_dialog->popup_centered_ratio();
}

void ThemeItemEditorDialog::_select_another_theme_cbk(const String &p_path) {
	Ref<Theme> loaded_theme = ResourceLoader::load(p_path);
	if (loaded_theme.is_null()) {
		EditorNode::get_singleton()->show_warning(TTR("Invalid file, not a Theme resource."));
		return;
	}
	if (loaded_theme == edited_theme) {
		EditorNode::get_singleton()->show_warning(TTR("Invalid file, same as the edited Theme resource."));
		return;
	}

	import_another_theme_value->set_text(p_path);
	import_other_theme_items->set_base_theme(loaded_theme);
	import_other_theme_items->reset_item_tree();
}

void ThemeItemEditorDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			connect("about_to_show", this, "_dialog_about_to_show");
			FALLTHROUGH;
		}
		case NOTIFICATION_THEME_CHANGED: {
			edit_items_add_color->set_icon(get_icon("Color", "EditorIcons"));
			edit_items_add_constant->set_icon(get_icon("MemberConstant", "EditorIcons"));
			edit_items_add_font->set_icon(get_icon("Font", "EditorIcons"));
			edit_items_add_icon->set_icon(get_icon("ImageTexture", "EditorIcons"));
			edit_items_add_stylebox->set_icon(get_icon("StyleBoxFlat", "EditorIcons"));

			edit_items_remove_class->set_icon(get_icon("Control", "EditorIcons"));
			edit_items_remove_custom->set_icon(get_icon("ThemeRemoveCustomItems", "EditorIcons"));
			edit_items_remove_all->set_icon(get_icon("ThemeRemoveAllItems", "EditorIcons"));

			import_another_theme_button->set_icon(get_icon("Folder", "EditorIcons"));

			tc->add_style_override("tab_selected", get_stylebox("tab_selected_odd", "TabContainer"));
			tc->add_style_override("panel", get_stylebox("panel_odd", "TabContainer"));
		} break;
	}
}

void ThemeItemEditorDialog::_bind_methods() {
	// Internal binds.
	ClassDB::bind_method("_edited_type_selected", &ThemeItemEditorDialog::_edited_type_selected);
	ClassDB::bind_method("_edited_type_button_pressed", &ThemeItemEditorDialog::_edited_type_button_pressed);
	ClassDB::bind_method("_add_theme_type", &ThemeItemEditorDialog::_add_theme_type);
	ClassDB::bind_method("_open_add_theme_item_dialog", &ThemeItemEditorDialog::_open_add_theme_item_dialog);
	ClassDB::bind_method("_remove_class_items", &ThemeItemEditorDialog::_remove_class_items);
	ClassDB::bind_method("_remove_custom_items", &ThemeItemEditorDialog::_remove_custom_items);
	ClassDB::bind_method("_remove_all_items", &ThemeItemEditorDialog::_remove_all_items);
	ClassDB::bind_method("_item_tree_button_pressed", &ThemeItemEditorDialog::_item_tree_button_pressed);
	ClassDB::bind_method("_edit_theme_item_gui_input", &ThemeItemEditorDialog::_edit_theme_item_gui_input);
	ClassDB::bind_method("_confirm_edit_theme_item", &ThemeItemEditorDialog::_confirm_edit_theme_item);
	ClassDB::bind_method("_update_edit_types", &ThemeItemEditorDialog::_update_edit_types);
	ClassDB::bind_method("_open_select_another_theme", &ThemeItemEditorDialog::_open_select_another_theme);
	ClassDB::bind_method("_select_another_theme_cbk", &ThemeItemEditorDialog::_select_another_theme_cbk);
	ClassDB::bind_method("_close_dialog", &ThemeItemEditorDialog::_close_dialog);
	ClassDB::bind_method("_dialog_about_to_show", &ThemeItemEditorDialog::_dialog_about_to_show);
}

void ThemeItemEditorDialog::set_edited_theme(const Ref<Theme> &p_theme) {
	edited_theme = p_theme;
}

ThemeItemEditorDialog::ThemeItemEditorDialog() {
	set_title(TTR("Manage Theme Items"));
	get_ok()->set_text(TTR("Close"));
	set_hide_on_ok(false); // Closing may require a confirmation in some cases.

	tc = memnew(TabContainer);
	tc->set_tab_align(TabContainer::TabAlign::ALIGN_LEFT);
	add_child(tc);

	// Edit Items tab.
	HSplitContainer *edit_dialog_hs = memnew(HSplitContainer);
	tc->add_child(edit_dialog_hs);
	tc->set_tab_title(0, TTR("Edit Items"));

	VBoxContainer *edit_dialog_side_vb = memnew(VBoxContainer);
	edit_dialog_side_vb->set_custom_minimum_size(Size2(200.0, 0.0) * EDSCALE);
	edit_dialog_hs->add_child(edit_dialog_side_vb);

	Label *edit_type_label = memnew(Label);
	edit_type_label->set_text(TTR("Types:"));
	edit_dialog_side_vb->add_child(edit_type_label);

	edit_type_list = memnew(Tree);
	edit_type_list->set_hide_root(true);
	edit_type_list->set_hide_folding(true);
	edit_type_list->set_columns(1);
	edit_type_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	edit_dialog_side_vb->add_child(edit_type_list);
	edit_type_list->connect("item_selected", this, "_edited_type_selected");
	edit_type_list->connect("button_pressed", this, "_edited_type_button_pressed");

	Label *edit_add_type_label = memnew(Label);
	edit_add_type_label->set_text(TTR("Add Type:"));
	edit_dialog_side_vb->add_child(edit_add_type_label);

	HBoxContainer *edit_add_type_hb = memnew(HBoxContainer);
	edit_dialog_side_vb->add_child(edit_add_type_hb);
	edit_add_type_value = memnew(LineEdit);
	edit_add_type_value->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	edit_add_type_value->connect("text_entered", this, "_add_theme_type");
	edit_add_type_hb->add_child(edit_add_type_value);
	Button *edit_add_type_button = memnew(Button);
	edit_add_type_button->set_text(TTR("Add"));
	edit_add_type_hb->add_child(edit_add_type_button);
	edit_add_type_button->connect("pressed", this, "_add_theme_type", varray(""));

	VBoxContainer *edit_items_vb = memnew(VBoxContainer);
	edit_items_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	edit_dialog_hs->add_child(edit_items_vb);

	HBoxContainer *edit_items_toolbar = memnew(HBoxContainer);
	edit_items_vb->add_child(edit_items_toolbar);

	Label *edit_items_toolbar_add_label = memnew(Label);
	edit_items_toolbar_add_label->set_text(TTR("Add Item:"));
	edit_items_toolbar->add_child(edit_items_toolbar_add_label);

	edit_items_add_color = memnew(Button);
	edit_items_add_color->set_tooltip(TTR("Add Color Item"));
	edit_items_add_color->set_flat(true);
	edit_items_add_color->set_disabled(true);
	edit_items_toolbar->add_child(edit_items_add_color);
	edit_items_add_color->connect("pressed", this, "_open_add_theme_item_dialog", varray(Theme::DATA_TYPE_COLOR));

	edit_items_add_constant = memnew(Button);
	edit_items_add_constant->set_tooltip(TTR("Add Constant Item"));
	edit_items_add_constant->set_flat(true);
	edit_items_add_constant->set_disabled(true);
	edit_items_toolbar->add_child(edit_items_add_constant);
	edit_items_add_constant->connect("pressed", this, "_open_add_theme_item_dialog", varray(Theme::DATA_TYPE_CONSTANT));

	edit_items_add_font = memnew(Button);
	edit_items_add_font->set_tooltip(TTR("Add Font Item"));
	edit_items_add_font->set_flat(true);
	edit_items_add_font->set_disabled(true);
	edit_items_toolbar->add_child(edit_items_add_font);
	edit_items_add_font->connect("pressed", this, "_open_add_theme_item_dialog", varray(Theme::DATA_TYPE_FONT));

	edit_items_add_icon = memnew(Button);
	edit_items_add_icon->set_tooltip(TTR("Add Icon Item"));
	edit_items_add_icon->set_flat(true);
	edit_items_add_icon->set_disabled(true);
	edit_items_toolbar->add_child(edit_items_add_icon);
	edit_items_add_icon->connect("pressed", this, "_open_add_theme_item_dialog", varray(Theme::DATA_TYPE_ICON));

	edit_items_add_stylebox = memnew(Button);
	edit_items_add_stylebox->set_tooltip(TTR("Add StyleBox Item"));
	edit_items_add_stylebox->set_flat(true);
	edit_items_add_stylebox->set_disabled(true);
	edit_items_toolbar->add_child(edit_items_add_stylebox);
	edit_items_add_stylebox->connect("pressed", this, "_open_add_theme_item_dialog", varray(Theme::DATA_TYPE_STYLEBOX));

	edit_items_toolbar->add_child(memnew(VSeparator));

	Label *edit_items_toolbar_remove_label = memnew(Label);
	edit_items_toolbar_remove_label->set_text(TTR("Remove Items:"));
	edit_items_toolbar->add_child(edit_items_toolbar_remove_label);

	edit_items_remove_class = memnew(Button);
	edit_items_remove_class->set_tooltip(TTR("Remove Class Items"));
	edit_items_remove_class->set_flat(true);
	edit_items_remove_class->set_disabled(true);
	edit_items_toolbar->add_child(edit_items_remove_class);
	edit_items_remove_class->connect("pressed", this, "_remove_class_items");

	edit_items_remove_custom = memnew(Button);
	edit_items_remove_custom->set_tooltip(TTR("Remove Custom Items"));
	edit_items_remove_custom->set_flat(true);
	edit_items_remove_custom->set_disabled(true);
	edit_items_toolbar->add_child(edit_items_remove_custom);
	edit_items_remove_custom->connect("pressed", this, "_remove_custom_items");

	edit_items_remove_all = memnew(Button);
	edit_items_remove_all->set_tooltip(TTR("Remove All Items"));
	edit_items_remove_all->set_flat(true);
	edit_items_remove_all->set_disabled(true);
	edit_items_toolbar->add_child(edit_items_remove_all);
	edit_items_remove_all->connect("pressed", this, "_remove_all_items");

	edit_items_tree = memnew(Tree);
	edit_items_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	edit_items_tree->set_hide_root(true);
	edit_items_tree->set_columns(1);
	edit_items_vb->add_child(edit_items_tree);
	edit_items_tree->connect("button_pressed", this, "_item_tree_button_pressed");

	edit_items_message = memnew(Label);
	edit_items_message->set_anchors_and_margins_preset(Control::PRESET_WIDE);
	edit_items_message->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	edit_items_message->set_align(Label::ALIGN_CENTER);
	edit_items_message->set_valign(Label::VALIGN_CENTER);
	edit_items_message->set_autowrap(true);
	edit_items_tree->add_child(edit_items_message);

	edit_theme_item_dialog = memnew(ConfirmationDialog);
	edit_theme_item_dialog->set_title(TTR("Add Theme Item"));
	add_child(edit_theme_item_dialog);
	VBoxContainer *edit_theme_item_vb = memnew(VBoxContainer);
	edit_theme_item_dialog->add_child(edit_theme_item_vb);

	edit_theme_item_old_vb = memnew(VBoxContainer);
	edit_theme_item_vb->add_child(edit_theme_item_old_vb);
	Label *edit_theme_item_old = memnew(Label);
	edit_theme_item_old->set_text(TTR("Old Name:"));
	edit_theme_item_old_vb->add_child(edit_theme_item_old);
	theme_item_old_name = memnew(Label);
	edit_theme_item_old_vb->add_child(theme_item_old_name);

	Label *edit_theme_item_label = memnew(Label);
	edit_theme_item_label->set_text(TTR("Name:"));
	edit_theme_item_vb->add_child(edit_theme_item_label);
	theme_item_name = memnew(LineEdit);
	edit_theme_item_vb->add_child(theme_item_name);
	theme_item_name->connect("gui_input", this, "_edit_theme_item_gui_input");
	edit_theme_item_dialog->connect("confirmed", this, "_confirm_edit_theme_item");

	// Import Items tab.
	TabContainer *import_tc = memnew(TabContainer);
	tc->add_child(import_tc);
	tc->set_tab_title(1, TTR("Import Items"));

	import_default_theme_items = memnew(ThemeItemImportTree);
	import_tc->add_child(import_default_theme_items);
	import_tc->set_tab_title(0, TTR("Default Theme"));
	import_default_theme_items->connect("items_imported", this, "_update_edit_types");

	import_editor_theme_items = memnew(ThemeItemImportTree);
	import_tc->add_child(import_editor_theme_items);
	import_tc->set_tab_title(1, TTR("Editor Theme"));
	import_editor_theme_items->connect("items_imported", this, "_update_edit_types");

	VBoxContainer *import_another_theme_vb = memnew(VBoxContainer);

	HBoxContainer *import_another_file_hb = memnew(HBoxContainer);
	import_another_theme_vb->add_child(import_another_file_hb);
	import_another_theme_value = memnew(LineEdit);
	import_another_theme_value->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	import_another_theme_value->set_editable(false);
	import_another_file_hb->add_child(import_another_theme_value);
	import_another_theme_button = memnew(Button);
	import_another_file_hb->add_child(import_another_theme_button);
	import_another_theme_button->connect("pressed", this, "_open_select_another_theme");

	import_another_theme_dialog = memnew(EditorFileDialog);
	import_another_theme_dialog->set_mode(EditorFileDialog::MODE_OPEN_FILE);
	import_another_theme_dialog->set_title(TTR("Select Another Theme Resource:"));
	List<String> ext;
	ResourceLoader::get_recognized_extensions_for_type("Theme", &ext);
	for (List<String>::Element *E = ext.front(); E; E = E->next()) {
		import_another_theme_dialog->add_filter("*." + E->get(), TTR("Theme Resource"));
	}
	import_another_file_hb->add_child(import_another_theme_dialog);
	import_another_theme_dialog->connect("file_selected", this, "_select_another_theme_cbk");

	import_other_theme_items = memnew(ThemeItemImportTree);
	import_other_theme_items->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	import_another_theme_vb->add_child(import_other_theme_items);

	import_tc->add_child(import_another_theme_vb);
	import_tc->set_tab_title(2, TTR("Another Theme"));
	import_other_theme_items->connect("items_imported", this, "_update_edit_types");

	confirm_closing_dialog = memnew(ConfirmationDialog);
	confirm_closing_dialog->set_autowrap(true);
	add_child(confirm_closing_dialog);
	confirm_closing_dialog->connect("confirmed", this, "_close_dialog");
}

void ThemeTypeDialog::_dialog_about_to_show() {
	add_type_filter->set_text("");

	_update_add_type_options();
}

void ThemeTypeDialog::ok_pressed() {
	_add_type_selected(add_type_filter->get_text().strip_edges());
}

void ThemeTypeDialog::_update_add_type_options(const String &p_filter) {
	add_type_options->clear();

	List<StringName> names;
	Theme::get_default()->get_type_list(&names);
	if (include_own_types) {
		edited_theme->get_type_list(&names);
	}
	names.sort_custom<StringName::AlphCompare>();

	Vector<StringName> unique_names;
	for (List<StringName>::Element *E = names.front(); E; E = E->next()) {
		// Filter out undesired values.
		if (!p_filter.is_subsequence_ofi(String(E->get()))) {
			continue;
		}

		// Skip duplicate values.
		if (unique_names.find(E->get()) >= 0) {
			continue;
		}
		unique_names.push_back(E->get());

		Ref<Texture> item_icon;
		if (E->get() == "") {
			item_icon = get_icon("NodeDisabled", "EditorIcons");
		} else {
			item_icon = EditorNode::get_singleton()->get_class_icon(E->get(), "NodeDisabled");
		}

		add_type_options->add_item(E->get(), item_icon);
	}
}

void ThemeTypeDialog::_add_type_filter_cbk(const String &p_value) {
	_update_add_type_options(p_value);
}

void ThemeTypeDialog::_add_type_options_cbk(int p_index) {
	add_type_filter->set_text(add_type_options->get_item_text(p_index));
}

void ThemeTypeDialog::_add_type_dialog_entered(const String &p_value) {
	_add_type_selected(p_value.strip_edges());
}

void ThemeTypeDialog::_add_type_dialog_activated(int p_index) {
	_add_type_selected(add_type_options->get_item_text(p_index));
}

void ThemeTypeDialog::_add_type_selected(const String &p_type_name) {
	pre_submitted_value = p_type_name;
	if (p_type_name.empty()) {
		add_type_confirmation->popup_centered();
		return;
	}

	_add_type_confirmed();
}

void ThemeTypeDialog::_add_type_confirmed() {
	emit_signal("type_selected", pre_submitted_value);
	hide();
}

void ThemeTypeDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			connect("about_to_show", this, "_dialog_about_to_show");
			FALLTHROUGH;
		}

		case NOTIFICATION_THEME_CHANGED: {
			_update_add_type_options();
		} break;

		case NOTIFICATION_POST_POPUP: {
			add_type_filter->grab_focus();
		} break;
	}
}

void ThemeTypeDialog::_bind_methods() {
	ClassDB::bind_method("_dialog_about_to_show", &ThemeTypeDialog::_dialog_about_to_show);

	ClassDB::bind_method("_add_type_filter_cbk", &ThemeTypeDialog::_add_type_filter_cbk);
	ClassDB::bind_method("_add_type_dialog_entered", &ThemeTypeDialog::_add_type_dialog_entered);
	ClassDB::bind_method("_add_type_options_cbk", &ThemeTypeDialog::_add_type_options_cbk);
	ClassDB::bind_method("_add_type_dialog_activated", &ThemeTypeDialog::_add_type_dialog_activated);
	ClassDB::bind_method("_add_type_confirmed", &ThemeTypeDialog::_add_type_confirmed);

	ADD_SIGNAL(MethodInfo("type_selected", PropertyInfo(Variant::STRING, "type_name")));
}

void ThemeTypeDialog::set_edited_theme(const Ref<Theme> &p_theme) {
	edited_theme = p_theme;
}

void ThemeTypeDialog::set_include_own_types(bool p_enable) {
	include_own_types = p_enable;
}

ThemeTypeDialog::ThemeTypeDialog() {
	get_ok()->set_text(TTR("Add Type"));
	set_hide_on_ok(false);

	VBoxContainer *add_type_vb = memnew(VBoxContainer);
	add_child(add_type_vb);

	Label *add_type_filter_label = memnew(Label);
	add_type_filter_label->set_text(TTR("Filter the list of types or create a new custom type:"));
	add_type_vb->add_child(add_type_filter_label);

	add_type_filter = memnew(LineEdit);
	add_type_vb->add_child(add_type_filter);
	add_type_filter->connect("text_changed", this, "_add_type_filter_cbk");
	add_type_filter->connect("text_entered", this, "_add_type_dialog_entered");

	Label *add_type_options_label = memnew(Label);
	add_type_options_label->set_text(TTR("Available Node-based types:"));
	add_type_vb->add_child(add_type_options_label);

	add_type_options = memnew(ItemList);
	add_type_options->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_type_vb->add_child(add_type_options);
	add_type_options->connect("item_selected", this, "_add_type_options_cbk");
	add_type_options->connect("item_activated", this, "_add_type_dialog_activated");

	add_type_confirmation = memnew(ConfirmationDialog);
	add_type_confirmation->set_title(TTR("Type name is empty!"));
	add_type_confirmation->set_text(TTR("Are you sure you want to create an empty type?"));
	add_type_confirmation->connect("confirmed", this, "_add_type_confirmed");
	add_child(add_type_confirmation);
}

VBoxContainer *ThemeTypeEditor::_create_item_list(Theme::DataType p_data_type) {
	VBoxContainer *items_tab = memnew(VBoxContainer);
	items_tab->set_custom_minimum_size(Size2(0, 160) * EDSCALE);
	data_type_tabs->add_child(items_tab);
	data_type_tabs->set_tab_title(data_type_tabs->get_tab_count() - 1, "");

	ScrollContainer *items_sc = memnew(ScrollContainer);
	items_sc->set_v_size_flags(SIZE_EXPAND_FILL);
	items_sc->set_enable_h_scroll(false);
	items_tab->add_child(items_sc);
	VBoxContainer *items_list = memnew(VBoxContainer);
	items_list->set_h_size_flags(SIZE_EXPAND_FILL);
	items_sc->add_child(items_list);

	HBoxContainer *item_add_hb = memnew(HBoxContainer);
	items_tab->add_child(item_add_hb);
	LineEdit *item_add_edit = memnew(LineEdit);
	item_add_edit->set_h_size_flags(SIZE_EXPAND_FILL);
	item_add_hb->add_child(item_add_edit);
	item_add_edit->connect("text_entered", this, "_item_add_lineedit_cbk", varray(p_data_type, item_add_edit));
	Button *item_add_button = memnew(Button);
	item_add_button->set_text(TTR("Add"));
	item_add_hb->add_child(item_add_button);
	item_add_button->connect("pressed", this, "_item_add_cbk", varray(p_data_type, item_add_edit));

	return items_list;
}

void ThemeTypeEditor::_update_type_list() {
	ERR_FAIL_COND(edited_theme.is_null());

	if (updating) {
		return;
	}
	updating = true;

	Control *focused = get_focus_owner();
	if (focused) {
		if (focusables.find(focused, 0) != -1) {
			// If focus is currently on one of the internal property editors, don't update.
			updating = false;
			return;
		}

		Node *focus_parent = focused->get_parent();
		while (focus_parent) {
			Control *c = Object::cast_to<Control>(focus_parent);
			if (c && focusables.find(c, 0) != -1) {
				// If focus is currently on one of the internal property editors, don't update.
				updating = false;
				return;
			}

			focus_parent = focus_parent->get_parent();
		}
	}

	List<StringName> theme_types;
	edited_theme->get_type_list(&theme_types);
	theme_types.sort_custom<StringName::AlphCompare>();

	theme_type_list->clear();

	if (theme_types.size() > 0) {
		theme_type_list->set_disabled(false);

		bool item_reselected = false;
		int e_idx = 0;
		for (List<StringName>::Element *E = theme_types.front(); E; E = E->next()) {
			Ref<Texture> item_icon;
			if (E->get() == "") {
				item_icon = get_icon("NodeDisabled", "EditorIcons");
			} else {
				item_icon = EditorNode::get_singleton()->get_class_icon(E->get(), "NodeDisabled");
			}
			theme_type_list->add_icon_item(item_icon, E->get());

			if (E->get() == edited_type) {
				theme_type_list->select(e_idx);
				item_reselected = true;
			}
			e_idx++;
		}

		if (!item_reselected) {
			theme_type_list->select(0);
			_list_type_selected(0);
		} else {
			_update_type_items();
		}
	} else {
		theme_type_list->set_disabled(true);
		theme_type_list->add_item(TTR("None"));

		edited_type = "";
		_update_type_items();
	}

	updating = false;
}

void ThemeTypeEditor::_update_type_list_debounced() {
	update_debounce_timer->start();
}

OrderedHashMap<StringName, bool> ThemeTypeEditor::_get_type_items(String p_type_name, void (Theme::*get_list_func)(StringName, List<StringName> *) const, bool include_default) {
	OrderedHashMap<StringName, bool> items;
	List<StringName> names;

	if (include_default) {
		names.clear();
		String default_type = p_type_name;
		if (edited_theme->get_type_variation_base(p_type_name) != StringName()) {
			default_type = edited_theme->get_type_variation_base(p_type_name);
		}

		(Theme::get_default().operator->()->*get_list_func)(default_type, &names);
		names.sort_custom<StringName::AlphCompare>();
		for (List<StringName>::Element *E = names.front(); E; E = E->next()) {
			items[E->get()] = false;
		}
	}

	{
		names.clear();
		(edited_theme.operator->()->*get_list_func)(p_type_name, &names);
		names.sort_custom<StringName::AlphCompare>();
		for (List<StringName>::Element *E = names.front(); E; E = E->next()) {
			items[E->get()] = true;
		}
	}

	List<StringName> keys;
	for (OrderedHashMap<StringName, bool>::Element E = items.front(); E; E = E.next()) {
		keys.push_back(E.key());
	}
	keys.sort_custom<StringName::AlphCompare>();

	OrderedHashMap<StringName, bool> ordered_items;
	for (List<StringName>::Element *E = keys.front(); E; E = E->next()) {
		ordered_items[E->get()] = items[E->get()];
	}

	return ordered_items;
}

HBoxContainer *ThemeTypeEditor::_create_property_control(Theme::DataType p_data_type, String p_item_name, bool p_editable) {
	HBoxContainer *item_control = memnew(HBoxContainer);

	HBoxContainer *item_name_container = memnew(HBoxContainer);
	item_name_container->set_h_size_flags(SIZE_EXPAND_FILL);
	item_name_container->set_stretch_ratio(2.0);
	item_control->add_child(item_name_container);

	Label *item_name = memnew(Label);
	item_name->set_h_size_flags(SIZE_EXPAND_FILL);
	item_name->set_clip_text(true);
	item_name->set_text(p_item_name);
	item_name->set_tooltip(p_item_name);
	item_name_container->add_child(item_name);

	if (p_editable) {
		LineEdit *item_name_edit = memnew(LineEdit);
		item_name_edit->set_h_size_flags(SIZE_EXPAND_FILL);
		item_name_edit->set_text(p_item_name);
		item_name_container->add_child(item_name_edit);
		item_name_edit->connect("text_entered", this, "_item_rename_entered", varray(p_data_type, p_item_name, item_name_container));
		item_name_edit->hide();

		Button *item_rename_button = memnew(Button);
		item_rename_button->set_icon(get_icon("Edit", "EditorIcons"));
		item_rename_button->set_tooltip(TTR("Rename Item"));
		item_rename_button->set_flat(true);
		item_name_container->add_child(item_rename_button);
		item_rename_button->connect("pressed", this, "_item_rename_cbk", varray(p_data_type, p_item_name, item_name_container));

		Button *item_remove_button = memnew(Button);
		item_remove_button->set_icon(get_icon("Remove", "EditorIcons"));
		item_remove_button->set_tooltip(TTR("Remove Item"));
		item_remove_button->set_flat(true);
		item_name_container->add_child(item_remove_button);
		item_remove_button->connect("pressed", this, "_item_remove_cbk", varray(p_data_type, p_item_name));

		Button *item_rename_confirm_button = memnew(Button);
		item_rename_confirm_button->set_icon(get_icon("ImportCheck", "EditorIcons"));
		item_rename_confirm_button->set_tooltip(TTR("Confirm Item Rename"));
		item_rename_confirm_button->set_flat(true);
		item_name_container->add_child(item_rename_confirm_button);
		item_rename_confirm_button->connect("pressed", this, "_item_rename_confirmed", varray(p_data_type, p_item_name, item_name_container));
		item_rename_confirm_button->hide();

		Button *item_rename_cancel_button = memnew(Button);
		item_rename_cancel_button->set_icon(get_icon("ImportFail", "EditorIcons"));
		item_rename_cancel_button->set_tooltip(TTR("Cancel Item Rename"));
		item_rename_cancel_button->set_flat(true);
		item_name_container->add_child(item_rename_cancel_button);
		item_rename_cancel_button->connect("pressed", this, "_item_rename_canceled", varray(p_data_type, p_item_name, item_name_container));
		item_rename_cancel_button->hide();
	} else {
		item_name->add_color_override("font_color", get_color("disabled_font_color", "Editor"));

		Button *item_override_button = memnew(Button);
		item_override_button->set_icon(get_icon("Add", "EditorIcons"));
		item_override_button->set_tooltip(TTR("Override Item"));
		item_override_button->set_flat(true);
		item_name_container->add_child(item_override_button);
		item_override_button->connect("pressed", this, "_item_override_cbk", varray(p_data_type, p_item_name));
	}

	return item_control;
}

void ThemeTypeEditor::_add_focusable(Control *p_control) {
	focusables.push_back(p_control);
}

void ThemeTypeEditor::_update_type_items() {
	bool show_default = show_default_items_button->is_pressed();
	List<StringName> names;

	focusables.clear();

	// Colors.
	{
		for (int i = color_items_list->get_child_count() - 1; i >= 0; i--) {
			Node *node = color_items_list->get_child(i);
			node->queue_delete();
			color_items_list->remove_child(node);
		}

		OrderedHashMap<StringName, bool> color_items = _get_type_items(edited_type, &Theme::get_color_list, show_default);
		for (OrderedHashMap<StringName, bool>::Element E = color_items.front(); E; E = E.next()) {
			HBoxContainer *item_control = _create_property_control(Theme::DATA_TYPE_COLOR, E.key(), E.get());
			ColorPickerButton *item_editor = memnew(ColorPickerButton);
			item_editor->set_h_size_flags(SIZE_EXPAND_FILL);
			item_control->add_child(item_editor);

			if (E.get()) {
				item_editor->set_pick_color(edited_theme->get_color(E.key(), edited_type));
				item_editor->connect("color_changed", this, "_color_item_changed", varray(E.key()));
			} else {
				item_editor->set_pick_color(Theme::get_default()->get_color(E.key(), edited_type));
				item_editor->set_disabled(true);
			}

			_add_focusable(item_editor);
			color_items_list->add_child(item_control);
		}
	}

	// Constants.
	{
		for (int i = constant_items_list->get_child_count() - 1; i >= 0; i--) {
			Node *node = constant_items_list->get_child(i);
			node->queue_delete();
			constant_items_list->remove_child(node);
		}

		OrderedHashMap<StringName, bool> constant_items = _get_type_items(edited_type, &Theme::get_constant_list, show_default);
		for (OrderedHashMap<StringName, bool>::Element E = constant_items.front(); E; E = E.next()) {
			HBoxContainer *item_control = _create_property_control(Theme::DATA_TYPE_CONSTANT, E.key(), E.get());
			SpinBox *item_editor = memnew(SpinBox);
			item_editor->set_h_size_flags(SIZE_EXPAND_FILL);
			item_editor->set_min(-100000);
			item_editor->set_max(100000);
			item_editor->set_step(1);
			item_editor->set_allow_lesser(true);
			item_editor->set_allow_greater(true);
			item_control->add_child(item_editor);

			if (E.get()) {
				item_editor->set_value(edited_theme->get_constant(E.key(), edited_type));
				item_editor->connect("value_changed", this, "_constant_item_changed", varray(E.key()));
			} else {
				item_editor->set_value(Theme::get_default()->get_constant(E.key(), edited_type));
				item_editor->set_editable(false);
			}

			_add_focusable(item_editor);
			constant_items_list->add_child(item_control);
		}
	}

	// Fonts.
	{
		for (int i = font_items_list->get_child_count() - 1; i >= 0; i--) {
			Node *node = font_items_list->get_child(i);
			node->queue_delete();
			font_items_list->remove_child(node);
		}

		OrderedHashMap<StringName, bool> font_items = _get_type_items(edited_type, &Theme::get_font_list, show_default);
		for (OrderedHashMap<StringName, bool>::Element E = font_items.front(); E; E = E.next()) {
			HBoxContainer *item_control = _create_property_control(Theme::DATA_TYPE_FONT, E.key(), E.get());
			EditorResourcePicker *item_editor = memnew(EditorResourcePicker);
			item_editor->set_h_size_flags(SIZE_EXPAND_FILL);
			item_editor->set_base_type("Font");
			item_control->add_child(item_editor);

			if (E.get()) {
				if (edited_theme->has_font(E.key(), edited_type)) {
					item_editor->set_edited_resource(edited_theme->get_font(E.key(), edited_type));
				} else {
					item_editor->set_edited_resource(RES());
				}
				item_editor->connect("resource_selected", this, "_edit_resource_item");
				item_editor->connect("resource_changed", this, "_font_item_changed", varray(E.key()));
			} else {
				if (Theme::get_default()->has_font(E.key(), edited_type)) {
					item_editor->set_edited_resource(Theme::get_default()->get_font(E.key(), edited_type));
				} else {
					item_editor->set_edited_resource(RES());
				}
				item_editor->set_editable(false);
			}

			_add_focusable(item_editor);
			font_items_list->add_child(item_control);
		}
	}

	// Icons.
	{
		for (int i = icon_items_list->get_child_count() - 1; i >= 0; i--) {
			Node *node = icon_items_list->get_child(i);
			node->queue_delete();
			icon_items_list->remove_child(node);
		}

		OrderedHashMap<StringName, bool> icon_items = _get_type_items(edited_type, &Theme::get_icon_list, show_default);
		for (OrderedHashMap<StringName, bool>::Element E = icon_items.front(); E; E = E.next()) {
			HBoxContainer *item_control = _create_property_control(Theme::DATA_TYPE_ICON, E.key(), E.get());
			EditorResourcePicker *item_editor = memnew(EditorResourcePicker);
			item_editor->set_h_size_flags(SIZE_EXPAND_FILL);
			item_editor->set_base_type("Texture");
			item_control->add_child(item_editor);

			if (E.get()) {
				if (edited_theme->has_icon(E.key(), edited_type)) {
					item_editor->set_edited_resource(edited_theme->get_icon(E.key(), edited_type));
				} else {
					item_editor->set_edited_resource(RES());
				}
				item_editor->connect("resource_selected", this, "_edit_resource_item");
				item_editor->connect("resource_changed", this, "_icon_item_changed", varray(E.key()));
			} else {
				if (Theme::get_default()->has_icon(E.key(), edited_type)) {
					item_editor->set_edited_resource(Theme::get_default()->get_icon(E.key(), edited_type));
				} else {
					item_editor->set_edited_resource(RES());
				}
				item_editor->set_editable(false);
			}

			_add_focusable(item_editor);
			icon_items_list->add_child(item_control);
		}
	}

	// Styleboxes.
	{
		for (int i = stylebox_items_list->get_child_count() - 1; i >= 0; i--) {
			Node *node = stylebox_items_list->get_child(i);
			node->queue_delete();
			stylebox_items_list->remove_child(node);
		}

		if (leading_stylebox.pinned) {
			HBoxContainer *item_control = _create_property_control(Theme::DATA_TYPE_STYLEBOX, leading_stylebox.item_name, true);
			EditorResourcePicker *item_editor = memnew(EditorResourcePicker);
			item_editor->set_h_size_flags(SIZE_EXPAND_FILL);
			item_editor->set_stretch_ratio(1.5);
			item_editor->set_base_type("StyleBox");

			Button *pin_leader_button = memnew(Button);
			pin_leader_button->set_flat(true);
			pin_leader_button->set_toggle_mode(true);
			pin_leader_button->set_pressed(true);
			pin_leader_button->set_icon(get_icon("Pin", "EditorIcons"));
			pin_leader_button->set_tooltip(TTR("Unpin this StyleBox as a main style."));
			item_control->add_child(pin_leader_button);
			pin_leader_button->connect("pressed", this, "_unpin_leading_stylebox");

			item_control->add_child(item_editor);

			if (leading_stylebox.stylebox.is_valid()) {
				item_editor->set_edited_resource(leading_stylebox.stylebox);
			} else {
				item_editor->set_edited_resource(RES());
			}
			item_editor->connect("resource_selected", this, "_edit_resource_item");
			item_editor->connect("resource_changed", this, "_stylebox_item_changed", varray(leading_stylebox.item_name));

			stylebox_items_list->add_child(item_control);
			stylebox_items_list->add_child(memnew(HSeparator));
		}

		OrderedHashMap<StringName, bool> stylebox_items = _get_type_items(edited_type, &Theme::get_stylebox_list, show_default);
		for (OrderedHashMap<StringName, bool>::Element E = stylebox_items.front(); E; E = E.next()) {
			if (leading_stylebox.pinned && leading_stylebox.item_name == E.key()) {
				continue;
			}

			HBoxContainer *item_control = _create_property_control(Theme::DATA_TYPE_STYLEBOX, E.key(), E.get());
			EditorResourcePicker *item_editor = memnew(EditorResourcePicker);
			item_editor->set_h_size_flags(SIZE_EXPAND_FILL);
			item_editor->set_stretch_ratio(1.5);
			item_editor->set_base_type("StyleBox");

			if (E.get()) {
				Ref<StyleBox> stylebox_value;
				if (edited_theme->has_stylebox(E.key(), edited_type)) {
					stylebox_value = edited_theme->get_stylebox(E.key(), edited_type);
					item_editor->set_edited_resource(stylebox_value);
				} else {
					item_editor->set_edited_resource(RES());
				}
				item_editor->connect("resource_selected", this, "_edit_resource_item");
				item_editor->connect("resource_changed", this, "_stylebox_item_changed", varray(E.key()));

				Button *pin_leader_button = memnew(Button);
				pin_leader_button->set_flat(true);
				pin_leader_button->set_toggle_mode(true);
				pin_leader_button->set_icon(get_icon("Pin", "EditorIcons"));
				pin_leader_button->set_tooltip(TTR("Pin this StyleBox as a main style. Editing its properties will update the same properties in all other StyleBoxes of this type."));
				item_control->add_child(pin_leader_button);
				pin_leader_button->connect("pressed", this, "_pin_leading_stylebox", varray(item_editor, E.key()));
			} else {
				if (Theme::get_default()->has_stylebox(E.key(), edited_type)) {
					item_editor->set_edited_resource(Theme::get_default()->get_stylebox(E.key(), edited_type));
				} else {
					item_editor->set_edited_resource(RES());
				}
				item_editor->set_editable(false);
			}

			item_control->add_child(item_editor);
			_add_focusable(item_editor);
			stylebox_items_list->add_child(item_control);
		}
	}

	// Various type settings.
	if (edited_type.empty() || ClassDB::class_exists(edited_type)) {
		type_variation_edit->set_editable(false);
		type_variation_edit->set_text("");
		type_variation_button->hide();
		type_variation_locked->set_visible(!edited_type.empty());
	} else {
		type_variation_edit->set_editable(true);
		type_variation_edit->set_text(edited_theme->get_type_variation_base(edited_type));
		_add_focusable(type_variation_edit);
		type_variation_button->show();
		type_variation_locked->hide();
	}
}

void ThemeTypeEditor::_list_type_selected(int p_index) {
	edited_type = theme_type_list->get_item_text(p_index);
	_update_type_items();
}

void ThemeTypeEditor::_add_type_button_cbk() {
	add_type_mode = ADD_THEME_TYPE;
	add_type_dialog->set_title(TTR("Add Item Type"));
	add_type_dialog->get_ok()->set_text(TTR("Add Type"));
	add_type_dialog->set_include_own_types(false);
	add_type_dialog->popup_centered(Size2(560, 420) * EDSCALE);
}

void ThemeTypeEditor::_add_default_type_items() {
	List<StringName> names;
	String default_type = edited_type;
	if (edited_theme->get_type_variation_base(edited_type) != StringName()) {
		default_type = edited_theme->get_type_variation_base(edited_type);
	}

	updating = true;
	// Prevent changes from immediately being reported while the operation is still ongoing.
	edited_theme->_freeze_change_propagation();

	{
		names.clear();
		Theme::get_default()->get_icon_list(default_type, &names);
		for (List<StringName>::Element *E = names.front(); E; E = E->next()) {
			if (!edited_theme->has_icon(E->get(), edited_type)) {
				edited_theme->set_icon(E->get(), edited_type, Ref<Texture>());
			}
		}
	}
	{
		names.clear();
		Theme::get_default()->get_stylebox_list(default_type, &names);
		for (List<StringName>::Element *E = names.front(); E; E = E->next()) {
			if (!edited_theme->has_stylebox(E->get(), edited_type)) {
				edited_theme->set_stylebox(E->get(), edited_type, Ref<StyleBox>());
			}
		}
	}
	{
		names.clear();
		Theme::get_default()->get_font_list(default_type, &names);
		for (List<StringName>::Element *E = names.front(); E; E = E->next()) {
			if (!edited_theme->has_font(E->get(), edited_type)) {
				edited_theme->set_font(E->get(), edited_type, Ref<Font>());
			}
		}
	}
	{
		names.clear();
		Theme::get_default()->get_color_list(default_type, &names);
		for (List<StringName>::Element *E = names.front(); E; E = E->next()) {
			if (!edited_theme->has_color(E->get(), edited_type)) {
				edited_theme->set_color(E->get(), edited_type, Theme::get_default()->get_color(E->get(), edited_type));
			}
		}
	}
	{
		names.clear();
		Theme::get_default()->get_constant_list(default_type, &names);
		for (List<StringName>::Element *E = names.front(); E; E = E->next()) {
			if (!edited_theme->has_constant(E->get(), edited_type)) {
				edited_theme->set_constant(E->get(), edited_type, Theme::get_default()->get_constant(E->get(), edited_type));
			}
		}
	}

	// Allow changes to be reported now that the operation is finished.
	edited_theme->_unfreeze_and_propagate_changes();
	updating = false;

	_update_type_items();
}

void ThemeTypeEditor::_item_add_cbk(int p_data_type, Control *p_control) {
	LineEdit *le = Object::cast_to<LineEdit>(p_control);
	if (le->get_text().strip_edges().empty()) {
		return;
	}

	String item_name = le->get_text().strip_edges();
	switch (p_data_type) {
		case Theme::DATA_TYPE_COLOR: {
			edited_theme->set_color(item_name, edited_type, Color());
		} break;
		case Theme::DATA_TYPE_CONSTANT: {
			edited_theme->set_constant(item_name, edited_type, 0);
		} break;
		case Theme::DATA_TYPE_FONT: {
			edited_theme->set_font(item_name, edited_type, Ref<Font>());
		} break;
		case Theme::DATA_TYPE_ICON: {
			edited_theme->set_icon(item_name, edited_type, Ref<Texture>());
		} break;
		case Theme::DATA_TYPE_STYLEBOX: {
			edited_theme->set_stylebox(item_name, edited_type, Ref<StyleBox>());
		} break;
	}

	le->set_text("");
}

void ThemeTypeEditor::_item_add_lineedit_cbk(String p_value, int p_data_type, Control *p_control) {
	_item_add_cbk(p_data_type, p_control);
}

void ThemeTypeEditor::_item_override_cbk(int p_data_type, String p_item_name) {
	switch (p_data_type) {
		case Theme::DATA_TYPE_COLOR: {
			edited_theme->set_color(p_item_name, edited_type, Theme::get_default()->get_color(p_item_name, edited_type));
		} break;
		case Theme::DATA_TYPE_CONSTANT: {
			edited_theme->set_constant(p_item_name, edited_type, Theme::get_default()->get_constant(p_item_name, edited_type));
		} break;
		case Theme::DATA_TYPE_FONT: {
			edited_theme->set_font(p_item_name, edited_type, Ref<Font>());
		} break;
		case Theme::DATA_TYPE_ICON: {
			edited_theme->set_icon(p_item_name, edited_type, Ref<Texture>());
		} break;
		case Theme::DATA_TYPE_STYLEBOX: {
			edited_theme->set_stylebox(p_item_name, edited_type, Ref<StyleBox>());
		} break;
	}
}

void ThemeTypeEditor::_item_remove_cbk(int p_data_type, String p_item_name) {
	switch (p_data_type) {
		case Theme::DATA_TYPE_COLOR: {
			edited_theme->clear_color(p_item_name, edited_type);
		} break;
		case Theme::DATA_TYPE_CONSTANT: {
			edited_theme->clear_constant(p_item_name, edited_type);
		} break;
		case Theme::DATA_TYPE_FONT: {
			edited_theme->clear_font(p_item_name, edited_type);
		} break;
		case Theme::DATA_TYPE_ICON: {
			edited_theme->clear_icon(p_item_name, edited_type);
		} break;
		case Theme::DATA_TYPE_STYLEBOX: {
			edited_theme->clear_stylebox(p_item_name, edited_type);

			if (leading_stylebox.pinned && leading_stylebox.item_name == p_item_name) {
				_unpin_leading_stylebox();
			}
		} break;
	}
}

void ThemeTypeEditor::_item_rename_cbk(int p_data_type, String p_item_name, Control *p_control) {
	// Label
	Object::cast_to<Label>(p_control->get_child(0))->hide();
	// Label buttons
	Object::cast_to<Button>(p_control->get_child(2))->hide();
	Object::cast_to<Button>(p_control->get_child(3))->hide();

	// LineEdit
	Object::cast_to<LineEdit>(p_control->get_child(1))->set_text(p_item_name);
	Object::cast_to<LineEdit>(p_control->get_child(1))->show();
	// LineEdit buttons
	Object::cast_to<Button>(p_control->get_child(4))->show();
	Object::cast_to<Button>(p_control->get_child(5))->show();
}

void ThemeTypeEditor::_item_rename_confirmed(int p_data_type, String p_item_name, Control *p_control) {
	LineEdit *le = Object::cast_to<LineEdit>(p_control->get_child(1));
	if (le->get_text().strip_edges().empty()) {
		return;
	}

	String new_name = le->get_text().strip_edges();
	if (new_name == p_item_name) {
		_item_rename_canceled(p_data_type, p_item_name, p_control);
		return;
	}

	switch (p_data_type) {
		case Theme::DATA_TYPE_COLOR: {
			edited_theme->rename_color(p_item_name, new_name, edited_type);
		} break;
		case Theme::DATA_TYPE_CONSTANT: {
			edited_theme->rename_constant(p_item_name, new_name, edited_type);
		} break;
		case Theme::DATA_TYPE_FONT: {
			edited_theme->rename_font(p_item_name, new_name, edited_type);
		} break;
		case Theme::DATA_TYPE_ICON: {
			edited_theme->rename_icon(p_item_name, new_name, edited_type);
		} break;
		case Theme::DATA_TYPE_STYLEBOX: {
			edited_theme->rename_stylebox(p_item_name, new_name, edited_type);

			if (leading_stylebox.pinned && leading_stylebox.item_name == p_item_name) {
				leading_stylebox.item_name = new_name;
			}
		} break;
	}
}

void ThemeTypeEditor::_item_rename_entered(String p_value, int p_data_type, String p_item_name, Control *p_control) {
	_item_rename_confirmed(p_data_type, p_item_name, p_control);
}

void ThemeTypeEditor::_item_rename_canceled(int p_data_type, String p_item_name, Control *p_control) {
	// LineEdit
	Object::cast_to<LineEdit>(p_control->get_child(1))->hide();
	// LineEdit buttons
	Object::cast_to<Button>(p_control->get_child(4))->hide();
	Object::cast_to<Button>(p_control->get_child(5))->hide();

	// Label
	Object::cast_to<Label>(p_control->get_child(0))->show();
	// Label buttons
	Object::cast_to<Button>(p_control->get_child(2))->show();
	Object::cast_to<Button>(p_control->get_child(3))->show();
}

void ThemeTypeEditor::_color_item_changed(Color p_value, String p_item_name) {
	edited_theme->set_color(p_item_name, edited_type, p_value);
}

void ThemeTypeEditor::_constant_item_changed(float p_value, String p_item_name) {
	edited_theme->set_constant(p_item_name, edited_type, int(p_value));
}

void ThemeTypeEditor::_edit_resource_item(RES p_resource, bool p_edit) {
	EditorNode::get_singleton()->edit_resource(p_resource);
}

void ThemeTypeEditor::_font_item_changed(Ref<Font> p_value, String p_item_name) {
	edited_theme->set_font(p_item_name, edited_type, p_value);
}

void ThemeTypeEditor::_icon_item_changed(Ref<Texture> p_value, String p_item_name) {
	edited_theme->set_icon(p_item_name, edited_type, p_value);
}

void ThemeTypeEditor::_stylebox_item_changed(Ref<StyleBox> p_value, String p_item_name) {
	edited_theme->set_stylebox(p_item_name, edited_type, p_value);

	if (leading_stylebox.pinned && leading_stylebox.item_name == p_item_name) {
		if (leading_stylebox.stylebox.is_valid()) {
			leading_stylebox.stylebox->disconnect("changed", this, "_update_stylebox_from_leading");
		}

		leading_stylebox.stylebox = p_value;
		leading_stylebox.ref_stylebox = (p_value.is_valid() ? p_value->duplicate() : RES());
		if (p_value.is_valid()) {
			leading_stylebox.stylebox->connect("changed", this, "_update_stylebox_from_leading");
		}
	}
}

void ThemeTypeEditor::_pin_leading_stylebox(Control *p_editor, String p_item_name) {
	if (leading_stylebox.stylebox.is_valid()) {
		leading_stylebox.stylebox->disconnect("changed", this, "_update_stylebox_from_leading");
	}

	Ref<StyleBox> stylebox;
	if (Object::cast_to<EditorResourcePicker>(p_editor)) {
		stylebox = Object::cast_to<EditorResourcePicker>(p_editor)->get_edited_resource();
	}

	LeadingStylebox leader;
	leader.pinned = true;
	leader.item_name = p_item_name;
	leader.stylebox = stylebox;
	leader.ref_stylebox = (stylebox.is_valid() ? stylebox->duplicate() : RES());

	leading_stylebox = leader;
	if (leading_stylebox.stylebox.is_valid()) {
		leading_stylebox.stylebox->connect("changed", this, "_update_stylebox_from_leading");
	}

	_update_type_items();
}

void ThemeTypeEditor::_unpin_leading_stylebox() {
	if (leading_stylebox.stylebox.is_valid()) {
		leading_stylebox.stylebox->disconnect("changed", this, "_update_stylebox_from_leading");
	}

	LeadingStylebox leader;
	leader.pinned = false;
	leading_stylebox = leader;

	_update_type_items();
}

void ThemeTypeEditor::_update_stylebox_from_leading() {
	if (!leading_stylebox.pinned || leading_stylebox.stylebox.is_null()) {
		return;
	}

	// Prevent changes from immediately being reported while the operation is still ongoing.
	edited_theme->_freeze_change_propagation();

	List<StringName> names;
	edited_theme->get_stylebox_list(edited_type, &names);
	List<Ref<StyleBox>> styleboxes;
	for (List<StringName>::Element *E = names.front(); E; E = E->next()) {
		Ref<StyleBox> sb = edited_theme->get_stylebox(E->get(), edited_type);

		// Avoid itself, stylebox can be shared between items.
		if (sb == leading_stylebox.stylebox) {
			continue;
		}

		if (sb->get_class() == leading_stylebox.stylebox->get_class()) {
			styleboxes.push_back(sb);
		}
	}

	List<PropertyInfo> props;
	leading_stylebox.stylebox->get_property_list(&props);
	for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
		if (!(E->get().usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}

		Variant value = leading_stylebox.stylebox->get(E->get().name);
		Variant ref_value = leading_stylebox.ref_stylebox->get(E->get().name);
		if (value == ref_value) {
			continue;
		}

		for (List<Ref<StyleBox>>::Element *F = styleboxes.front(); F; F = F->next()) {
			Ref<StyleBox> sb = F->get();
			sb->set(E->get().name, value);
		}
	}

	leading_stylebox.ref_stylebox = leading_stylebox.stylebox->duplicate();

	// Allow changes to be reported now that the operation is finished.
	edited_theme->_unfreeze_and_propagate_changes();
}

void ThemeTypeEditor::_type_variation_changed(const String p_value) {
	if (p_value.empty()) {
		edited_theme->clear_type_variation(edited_type);
	} else {
		edited_theme->set_type_variation(edited_type, StringName(p_value));
	}
}

void ThemeTypeEditor::_add_type_variation_cbk() {
	add_type_mode = ADD_VARIATION_BASE;
	add_type_dialog->set_title(TTR("Set Variation Base Type"));
	add_type_dialog->get_ok()->set_text(TTR("Set Base Type"));
	add_type_dialog->set_include_own_types(true);
	add_type_dialog->popup_centered(Size2(560, 420) * EDSCALE);
}

void ThemeTypeEditor::_add_type_dialog_selected(const String p_type_name) {
	if (add_type_mode == ADD_THEME_TYPE) {
		select_type(p_type_name);
	} else if (add_type_mode == ADD_VARIATION_BASE) {
		_type_variation_changed(p_type_name);
	}
}

void ThemeTypeEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			add_type_button->set_icon(get_icon("Add", "EditorIcons"));

			data_type_tabs->set_tab_icon(0, get_icon("Color", "EditorIcons"));
			data_type_tabs->set_tab_icon(1, get_icon("MemberConstant", "EditorIcons"));
			data_type_tabs->set_tab_icon(2, get_icon("Font", "EditorIcons"));
			data_type_tabs->set_tab_icon(3, get_icon("ImageTexture", "EditorIcons"));
			data_type_tabs->set_tab_icon(4, get_icon("StyleBoxFlat", "EditorIcons"));
			data_type_tabs->set_tab_icon(5, get_icon("Tools", "EditorIcons"));

			data_type_tabs->add_style_override("tab_selected", get_stylebox("tab_selected_odd", "TabContainer"));
			data_type_tabs->add_style_override("panel", get_stylebox("panel_odd", "TabContainer"));

			type_variation_button->set_icon(get_icon("Add", "EditorIcons"));
		} break;
	}
}

void ThemeTypeEditor::_bind_methods() {
	// Internal binds.
	ClassDB::bind_method("_update_type_list", &ThemeTypeEditor::_update_type_list);
	ClassDB::bind_method("_update_type_list_debounced", &ThemeTypeEditor::_update_type_list_debounced);
	ClassDB::bind_method("_update_type_items", &ThemeTypeEditor::_update_type_items);
	ClassDB::bind_method("_list_type_selected", &ThemeTypeEditor::_list_type_selected);

	ClassDB::bind_method("_add_type_button_cbk", &ThemeTypeEditor::_add_type_button_cbk);
	ClassDB::bind_method("_add_type_variation_cbk", &ThemeTypeEditor::_add_type_variation_cbk);
	ClassDB::bind_method("_add_type_dialog_selected", &ThemeTypeEditor::_add_type_dialog_selected);
	ClassDB::bind_method("_add_default_type_items", &ThemeTypeEditor::_add_default_type_items);

	ClassDB::bind_method("_item_add_lineedit_cbk", &ThemeTypeEditor::_item_add_lineedit_cbk);
	ClassDB::bind_method("_item_add_cbk", &ThemeTypeEditor::_item_add_cbk);
	ClassDB::bind_method("_item_rename_cbk", &ThemeTypeEditor::_item_rename_cbk);
	ClassDB::bind_method("_item_rename_entered", &ThemeTypeEditor::_item_rename_entered);
	ClassDB::bind_method("_item_rename_confirmed", &ThemeTypeEditor::_item_rename_confirmed);
	ClassDB::bind_method("_item_rename_canceled", &ThemeTypeEditor::_item_rename_canceled);
	ClassDB::bind_method("_item_remove_cbk", &ThemeTypeEditor::_item_remove_cbk);
	ClassDB::bind_method("_item_override_cbk", &ThemeTypeEditor::_item_override_cbk);

	ClassDB::bind_method("_color_item_changed", &ThemeTypeEditor::_color_item_changed);
	ClassDB::bind_method("_constant_item_changed", &ThemeTypeEditor::_constant_item_changed);
	ClassDB::bind_method("_edit_resource_item", &ThemeTypeEditor::_edit_resource_item);
	ClassDB::bind_method("_font_item_changed", &ThemeTypeEditor::_font_item_changed);
	ClassDB::bind_method("_icon_item_changed", &ThemeTypeEditor::_icon_item_changed);
	ClassDB::bind_method("_stylebox_item_changed", &ThemeTypeEditor::_stylebox_item_changed);
	ClassDB::bind_method("_pin_leading_stylebox", &ThemeTypeEditor::_pin_leading_stylebox);
	ClassDB::bind_method("_unpin_leading_stylebox", &ThemeTypeEditor::_unpin_leading_stylebox);
	ClassDB::bind_method("_update_stylebox_from_leading", &ThemeTypeEditor::_update_stylebox_from_leading);
	ClassDB::bind_method("_type_variation_changed", &ThemeTypeEditor::_type_variation_changed);
}

void ThemeTypeEditor::set_edited_theme(const Ref<Theme> &p_theme) {
	if (edited_theme.is_valid()) {
		edited_theme->disconnect("changed", this, "_update_type_list_debounced");
	}

	edited_theme = p_theme;
	edited_theme->connect("changed", this, "_update_type_list_debounced");
	_update_type_list();

	add_type_dialog->set_edited_theme(edited_theme);
}

void ThemeTypeEditor::select_type(String p_type_name) {
	edited_type = p_type_name;
	bool type_exists = false;

	for (int i = 0; i < theme_type_list->get_item_count(); i++) {
		String type_name = theme_type_list->get_item_text(i);
		if (type_name == edited_type) {
			theme_type_list->select(i);
			type_exists = true;
			break;
		}
	}

	if (type_exists) {
		_update_type_items();
	} else {
		edited_theme->add_icon_type(edited_type);
		edited_theme->add_stylebox_type(edited_type);
		edited_theme->add_font_type(edited_type);
		edited_theme->add_color_type(edited_type);
		edited_theme->add_constant_type(edited_type);

		_update_type_list();
	}
}

ThemeTypeEditor::ThemeTypeEditor() {
	VBoxContainer *main_vb = memnew(VBoxContainer);
	add_child(main_vb);

	HBoxContainer *type_list_hb = memnew(HBoxContainer);
	main_vb->add_child(type_list_hb);

	Label *type_list_label = memnew(Label);
	type_list_label->set_text(TTR("Type:"));
	type_list_hb->add_child(type_list_label);

	theme_type_list = memnew(OptionButton);
	theme_type_list->set_h_size_flags(SIZE_EXPAND_FILL);
	type_list_hb->add_child(theme_type_list);
	theme_type_list->connect("item_selected", this, "_list_type_selected");

	add_type_button = memnew(Button);
	add_type_button->set_tooltip(TTR("Add Type"));
	type_list_hb->add_child(add_type_button);
	add_type_button->connect("pressed", this, "_add_type_button_cbk");

	HBoxContainer *type_controls = memnew(HBoxContainer);
	main_vb->add_child(type_controls);

	show_default_items_button = memnew(CheckButton);
	show_default_items_button->set_h_size_flags(SIZE_EXPAND_FILL);
	show_default_items_button->set_text(TTR("Show Default"));
	show_default_items_button->set_tooltip(TTR("Show default type items alongside items that have been overridden."));
	show_default_items_button->set_pressed(true);
	type_controls->add_child(show_default_items_button);
	show_default_items_button->connect("pressed", this, "_update_type_items");

	Button *add_default_items_button = memnew(Button);
	add_default_items_button->set_h_size_flags(SIZE_EXPAND_FILL);
	add_default_items_button->set_text(TTR("Override All"));
	add_default_items_button->set_tooltip(TTR("Override all default type items."));
	type_controls->add_child(add_default_items_button);
	add_default_items_button->connect("pressed", this, "_add_default_type_items");

	data_type_tabs = memnew(TabContainer);
	main_vb->add_child(data_type_tabs);
	data_type_tabs->set_v_size_flags(SIZE_EXPAND_FILL);
	data_type_tabs->set_use_hidden_tabs_for_min_size(true);

	color_items_list = _create_item_list(Theme::DATA_TYPE_COLOR);
	constant_items_list = _create_item_list(Theme::DATA_TYPE_CONSTANT);
	font_items_list = _create_item_list(Theme::DATA_TYPE_FONT);
	icon_items_list = _create_item_list(Theme::DATA_TYPE_ICON);
	stylebox_items_list = _create_item_list(Theme::DATA_TYPE_STYLEBOX);

	VBoxContainer *type_settings_tab = memnew(VBoxContainer);
	type_settings_tab->set_custom_minimum_size(Size2(0, 160) * EDSCALE);
	data_type_tabs->add_child(type_settings_tab);
	data_type_tabs->set_tab_title(data_type_tabs->get_tab_count() - 1, "");

	ScrollContainer *type_settings_sc = memnew(ScrollContainer);
	type_settings_sc->set_v_size_flags(SIZE_EXPAND_FILL);
	type_settings_sc->set_enable_h_scroll(false);
	type_settings_tab->add_child(type_settings_sc);
	VBoxContainer *type_settings_list = memnew(VBoxContainer);
	type_settings_list->set_h_size_flags(SIZE_EXPAND_FILL);
	type_settings_sc->add_child(type_settings_list);

	VBoxContainer *type_variation_vb = memnew(VBoxContainer);
	type_settings_list->add_child(type_variation_vb);

	HBoxContainer *type_variation_hb = memnew(HBoxContainer);
	type_variation_vb->add_child(type_variation_hb);
	Label *type_variation_label = memnew(Label);
	type_variation_hb->add_child(type_variation_label);
	type_variation_label->set_text(TTR("Base Type"));
	type_variation_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	type_variation_edit = memnew(LineEdit);
	type_variation_hb->add_child(type_variation_edit);
	type_variation_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	type_variation_edit->connect("text_changed", this, "_type_variation_changed");
	type_variation_edit->connect("focus_exited", this, "_update_type_items");
	type_variation_button = memnew(Button);
	type_variation_hb->add_child(type_variation_button);
	type_variation_button->set_tooltip(TTR("Select the variation base type from a list of available types."));
	type_variation_button->connect("pressed", this, "_add_type_variation_cbk");

	type_variation_locked = memnew(Label);
	type_variation_vb->add_child(type_variation_locked);
	type_variation_locked->set_align(Label::ALIGN_CENTER);
	type_variation_locked->set_autowrap(true);
	type_variation_locked->set_text(TTR("A type associated with a built-in class cannot be marked as a variation of another type."));
	type_variation_locked->hide();

	add_type_dialog = memnew(ThemeTypeDialog);
	add_type_dialog->set_title(TTR("Add Item Type"));
	add_child(add_type_dialog);
	add_type_dialog->connect("type_selected", this, "_add_type_dialog_selected");

	update_debounce_timer = memnew(Timer);
	update_debounce_timer->set_one_shot(true);
	update_debounce_timer->set_wait_time(0.5);
	update_debounce_timer->connect("timeout", this, "_update_type_list");
	add_child(update_debounce_timer);
}

void ThemeEditor::edit(const Ref<Theme> &p_theme) {
	if (theme == p_theme) {
		return;
	}

	theme = p_theme;
	theme_type_editor->set_edited_theme(p_theme);
	theme_edit_dialog->set_edited_theme(p_theme);

	for (int i = 0; i < preview_tabs_content->get_child_count(); i++) {
		ThemeEditorPreview *preview_tab = Object::cast_to<ThemeEditorPreview>(preview_tabs_content->get_child(i));
		if (!preview_tab) {
			continue;
		}

		preview_tab->set_preview_theme(p_theme);
	}

	theme_name->set_text(TTR("Theme:") + " " + theme->get_path().get_file());
}

Ref<Theme> ThemeEditor::get_edited_theme() {
	return theme;
}

void ThemeEditor::_theme_save_button_cbk(bool p_save_as) {
	ERR_FAIL_COND_MSG(theme.is_null(), "Invalid state of the Theme Editor; the Theme resource is missing.");

	if (p_save_as) {
		EditorNode::get_singleton()->save_resource_as(theme);
	} else {
		EditorNode::get_singleton()->save_resource(theme);
	}
}

void ThemeEditor::_theme_edit_button_cbk() {
	theme_edit_dialog->popup_centered(Size2(850, 700) * EDSCALE);
}

void ThemeEditor::_add_preview_button_cbk() {
	preview_scene_dialog->popup_centered_ratio();
}

void ThemeEditor::_preview_scene_dialog_cbk(const String &p_path) {
	SceneThemeEditorPreview *preview_tab = memnew(SceneThemeEditorPreview);
	if (!preview_tab->set_preview_scene(p_path)) {
		return;
	}

	_add_preview_tab(preview_tab, p_path.get_file(), get_icon("PackedScene", "EditorIcons"));
	preview_tab->connect("scene_invalidated", this, "_remove_preview_tab_invalid", varray(preview_tab));
	preview_tab->connect("scene_reloaded", this, "_update_preview_tab", varray(preview_tab));
}

void ThemeEditor::_add_preview_tab(ThemeEditorPreview *p_preview_tab, const String &p_preview_name, const Ref<Texture> &p_icon) {
	p_preview_tab->set_preview_theme(theme);

	preview_tabs->add_tab(p_preview_name, p_icon);
	preview_tabs_content->add_child(p_preview_tab);
	preview_tabs->set_tab_right_button(preview_tabs->get_tab_count() - 1, EditorNode::get_singleton()->get_gui_base()->get_icon("close", "Tabs"));
	p_preview_tab->connect("control_picked", this, "_preview_control_picked");

	preview_tabs->set_current_tab(preview_tabs->get_tab_count() - 1);
}

void ThemeEditor::_change_preview_tab(int p_tab) {
	ERR_FAIL_INDEX_MSG(p_tab, preview_tabs_content->get_child_count(), "Attempting to open a preview tab that doesn't exist.");

	for (int i = 0; i < preview_tabs_content->get_child_count(); i++) {
		Control *c = Object::cast_to<Control>(preview_tabs_content->get_child(i));
		if (!c) {
			continue;
		}

		c->set_visible(i == p_tab);
	}
}

void ThemeEditor::_remove_preview_tab(int p_tab) {
	ERR_FAIL_INDEX_MSG(p_tab, preview_tabs_content->get_child_count(), "Attempting to remove a preview tab that doesn't exist.");

	ThemeEditorPreview *preview_tab = Object::cast_to<ThemeEditorPreview>(preview_tabs_content->get_child(p_tab));
	ERR_FAIL_COND_MSG(Object::cast_to<DefaultThemeEditorPreview>(preview_tab), "Attemptying to remove the default preview tab.");

	if (preview_tab) {
		preview_tab->disconnect("control_picked", this, "_preview_control_picked");
		if (preview_tab->is_connected("scene_invalidated", this, "_remove_preview_tab_invalid")) {
			preview_tab->disconnect("scene_invalidated", this, "_remove_preview_tab_invalid");
		}
		if (preview_tab->is_connected("scene_reloaded", this, "_update_preview_tab")) {
			preview_tab->disconnect("scene_reloaded", this, "_update_preview_tab");
		}

		preview_tabs_content->remove_child(preview_tab);
		preview_tabs->remove_tab(p_tab);
		_change_preview_tab(preview_tabs->get_current_tab());
	}
}

void ThemeEditor::_remove_preview_tab_invalid(Node *p_tab_control) {
	int tab_index = p_tab_control->get_index();
	_remove_preview_tab(tab_index);
}

void ThemeEditor::_update_preview_tab(Node *p_tab_control) {
	if (!Object::cast_to<SceneThemeEditorPreview>(p_tab_control)) {
		return;
	}

	int tab_index = p_tab_control->get_index();
	SceneThemeEditorPreview *scene_preview = Object::cast_to<SceneThemeEditorPreview>(p_tab_control);
	preview_tabs->set_tab_title(tab_index, scene_preview->get_preview_scene_path().get_file());
}

void ThemeEditor::_preview_control_picked(String p_class_name) {
	theme_type_editor->select_type(p_class_name);
}

void ThemeEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			preview_tabs->add_style_override("tab_fg", get_stylebox("ThemeEditorPreviewFG", "EditorStyles"));
			preview_tabs->add_style_override("tab_bg", get_stylebox("ThemeEditorPreviewBG", "EditorStyles"));
			preview_tabs_content->add_style_override("panel", get_stylebox("panel_odd", "TabContainer"));

			add_preview_button->set_icon(get_icon("Add", "EditorIcons"));
		} break;
	}
}

void ThemeEditor::_bind_methods() {
	// Internal binds.
	ClassDB::bind_method("_theme_save_button_cbk", &ThemeEditor::_theme_save_button_cbk);
	ClassDB::bind_method("_theme_edit_button_cbk", &ThemeEditor::_theme_edit_button_cbk);
	ClassDB::bind_method("_change_preview_tab", &ThemeEditor::_change_preview_tab);
	ClassDB::bind_method("_remove_preview_tab", &ThemeEditor::_remove_preview_tab);
	ClassDB::bind_method("_add_preview_button_cbk", &ThemeEditor::_add_preview_button_cbk);
	ClassDB::bind_method("_preview_control_picked", &ThemeEditor::_preview_control_picked);
	ClassDB::bind_method("_preview_scene_dialog_cbk", &ThemeEditor::_preview_scene_dialog_cbk);
	ClassDB::bind_method("_remove_preview_tab_invalid", &ThemeEditor::_remove_preview_tab_invalid);
	ClassDB::bind_method("_update_preview_tab", &ThemeEditor::_update_preview_tab);
}

ThemeEditor::ThemeEditor() {
	HBoxContainer *top_menu = memnew(HBoxContainer);
	add_child(top_menu);

	theme_name = memnew(Label);
	theme_name->set_text(TTR("Theme:"));
	top_menu->add_child(theme_name);

	top_menu->add_spacer(false);

	Button *theme_save_button = memnew(Button);
	theme_save_button->set_text(TTR("Save"));
	theme_save_button->set_flat(true);
	theme_save_button->connect("pressed", this, "_theme_save_button_cbk", varray(false));
	top_menu->add_child(theme_save_button);

	Button *theme_save_as_button = memnew(Button);
	theme_save_as_button->set_text(TTR("Save As..."));
	theme_save_as_button->set_flat(true);
	theme_save_as_button->connect("pressed", this, "_theme_save_button_cbk", varray(true));
	top_menu->add_child(theme_save_as_button);

	top_menu->add_child(memnew(VSeparator));

	Button *theme_edit_button = memnew(Button);
	theme_edit_button->set_text(TTR("Manage Items..."));
	theme_edit_button->set_tooltip(TTR("Add, remove, organize and import Theme items."));
	theme_edit_button->set_flat(true);
	theme_edit_button->connect("pressed", this, "_theme_edit_button_cbk");
	top_menu->add_child(theme_edit_button);

	theme_edit_dialog = memnew(ThemeItemEditorDialog);
	theme_edit_dialog->hide();
	top_menu->add_child(theme_edit_dialog);

	HSplitContainer *main_hs = memnew(HSplitContainer);
	main_hs->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(main_hs);

	VBoxContainer *preview_tabs_vb = memnew(VBoxContainer);
	preview_tabs_vb->set_h_size_flags(SIZE_EXPAND_FILL);
	preview_tabs_vb->set_custom_minimum_size(Size2(520, 0) * EDSCALE);
	preview_tabs_vb->add_constant_override("separation", 2 * EDSCALE);
	main_hs->add_child(preview_tabs_vb);
	HBoxContainer *preview_tabbar_hb = memnew(HBoxContainer);
	preview_tabs_vb->add_child(preview_tabbar_hb);
	preview_tabs_content = memnew(PanelContainer);
	preview_tabs_content->set_v_size_flags(SIZE_EXPAND_FILL);
	preview_tabs_content->set_draw_behind_parent(true);
	preview_tabs_vb->add_child(preview_tabs_content);

	preview_tabs = memnew(Tabs);
	preview_tabs->set_tab_align(Tabs::ALIGN_LEFT);
	preview_tabs->set_h_size_flags(SIZE_EXPAND_FILL);
	preview_tabbar_hb->add_child(preview_tabs);
	preview_tabs->connect("tab_changed", this, "_change_preview_tab");
	preview_tabs->connect("right_button_pressed", this, "_remove_preview_tab");

	HBoxContainer *add_preview_button_hb = memnew(HBoxContainer);
	preview_tabbar_hb->add_child(add_preview_button_hb);
	add_preview_button = memnew(Button);
	add_preview_button->set_text(TTR("Add Preview"));
	add_preview_button_hb->add_child(add_preview_button);
	add_preview_button->connect("pressed", this, "_add_preview_button_cbk");

	DefaultThemeEditorPreview *default_preview_tab = memnew(DefaultThemeEditorPreview);
	preview_tabs_content->add_child(default_preview_tab);
	default_preview_tab->connect("control_picked", this, "_preview_control_picked");
	preview_tabs->add_tab(TTR("Default Preview"));

	preview_scene_dialog = memnew(EditorFileDialog);
	preview_scene_dialog->set_mode(EditorFileDialog::MODE_OPEN_FILE);
	preview_scene_dialog->set_title(TTR("Select UI Scene:"));
	List<String> ext;
	ResourceLoader::get_recognized_extensions_for_type("PackedScene", &ext);
	for (List<String>::Element *E = ext.front(); E; E = E->next()) {
		preview_scene_dialog->add_filter("*." + E->get(), TTR("Scene"));
	}
	main_hs->add_child(preview_scene_dialog);
	preview_scene_dialog->connect("file_selected", this, "_preview_scene_dialog_cbk");

	theme_type_editor = memnew(ThemeTypeEditor);
	main_hs->add_child(theme_type_editor);
	theme_type_editor->set_custom_minimum_size(Size2(280, 0) * EDSCALE);
}

void ThemeEditorPlugin::edit(Object *p_node) {
	if (Object::cast_to<Theme>(p_node)) {
		theme_editor->edit(Object::cast_to<Theme>(p_node));
	} else if (Object::cast_to<Font>(p_node) || Object::cast_to<StyleBox>(p_node) || Object::cast_to<Texture>(p_node)) {
		// Do nothing, keep editing the existing theme.
	} else {
		theme_editor->edit(Ref<Theme>());
	}
}

bool ThemeEditorPlugin::handles(Object *p_node) const {
	if (Object::cast_to<Theme>(p_node)) {
		return true;
	}

	Ref<Theme> edited_theme = theme_editor->get_edited_theme();
	if (edited_theme.is_null()) {
		return false;
	}

	// If we are editing a theme already and this particular resource happens to belong to it,
	// then we just keep editing it, despite not being able to directly handle it.
	// This only goes one layer deep, but if required this can be extended to support, say, FontData inside of Font.
	bool belongs_to_theme = false;

	if (Object::cast_to<Font>(p_node)) {
		Ref<Font> font_item = Object::cast_to<Font>(p_node);
		List<StringName> types;
		List<StringName> names;

		edited_theme->get_font_types(&types);
		for (List<StringName>::Element *E = types.front(); E; E = E->next()) {
			names.clear();
			edited_theme->get_font_list(E->get(), &names);

			for (List<StringName>::Element *F = names.front(); F; F = F->next()) {
				if (font_item == edited_theme->get_font(F->get(), E->get())) {
					belongs_to_theme = true;
					break;
				}
			}
		}
	} else if (Object::cast_to<StyleBox>(p_node)) {
		Ref<StyleBox> stylebox_item = Object::cast_to<StyleBox>(p_node);
		List<StringName> types;
		List<StringName> names;

		edited_theme->get_stylebox_types(&types);
		for (List<StringName>::Element *E = types.front(); E; E = E->next()) {
			names.clear();
			edited_theme->get_stylebox_list(E->get(), &names);

			for (List<StringName>::Element *F = names.front(); F; F = F->next()) {
				if (stylebox_item == edited_theme->get_stylebox(F->get(), E->get())) {
					belongs_to_theme = true;
					break;
				}
			}
		}
	} else if (Object::cast_to<Texture>(p_node)) {
		Ref<Texture> icon_item = Object::cast_to<Texture>(p_node);
		List<StringName> types;
		List<StringName> names;

		edited_theme->get_icon_types(&types);
		for (List<StringName>::Element *E = types.front(); E; E = E->next()) {
			names.clear();
			edited_theme->get_icon_list(E->get(), &names);

			for (List<StringName>::Element *F = names.front(); F; F = F->next()) {
				if (icon_item == edited_theme->get_icon(F->get(), E->get())) {
					belongs_to_theme = true;
					break;
				}
			}
		}
	}

	return belongs_to_theme;
}

void ThemeEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		button->show();
		editor->make_bottom_panel_item_visible(theme_editor);
	} else {
		if (theme_editor->is_visible_in_tree()) {
			editor->hide_bottom_panel();
		}

		button->hide();
	}
}

ThemeEditorPlugin::ThemeEditorPlugin(EditorNode *p_node) {
	editor = p_node;
	theme_editor = memnew(ThemeEditor);
	theme_editor->set_custom_minimum_size(Size2(0, 200) * EDSCALE);

	button = editor->add_bottom_panel_item(TTR("Theme"), theme_editor);
	button->hide();
}
