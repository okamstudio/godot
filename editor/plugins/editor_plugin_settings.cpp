/**************************************************************************/
/*  editor_plugin_settings.cpp                                            */
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

#include "editor_plugin_settings.h"

#include "core/config/project_settings.h"
#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/tree.h"

void EditorPluginSettings::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_WM_WINDOW_FOCUS_IN: {
			update_plugins();
		} break;

		case Node::NOTIFICATION_READY: {
			plugin_config_dialog->connect("plugin_ready", callable_mp(EditorNode::get_singleton(), &EditorNode::_on_plugin_ready));
			plugin_list->connect("button_clicked", callable_mp(this, &EditorPluginSettings::_cell_button_pressed));
		} break;
	}
}

void EditorPluginSettings::update_plugins() {
	plugin_list->clear();
	updating = true;
	TreeItem *root = plugin_list->create_item();

	Vector<String> plugins = _get_plugins("res://addons");
	plugins.sort();

	for (int i = 0; i < plugins.size(); i++) {
		Ref<ConfigFile> cfg;
		cfg.instantiate();
		const String &path = plugins[i];

		Error err = cfg->load(path);

		if (err != OK) {
			WARN_PRINT("Can't load plugin config at: " + path);
		} else {
			Vector<String> missing_keys;
			for (const String required_key : { "name", "author", "version", "description", "script" }) {
				if (!cfg->has_section_key("plugin", required_key)) {
					missing_keys.append("\"plugin/" + required_key + "\"");
				}
			}

			if (!missing_keys.is_empty()) {
				WARN_PRINT(vformat("Plugin config at \"%s\" is missing the following keys: %s", path, String(",").join(missing_keys)));
			} else {
				String name = cfg->get_value("plugin", "name");
				String author = cfg->get_value("plugin", "author");
				String version = cfg->get_value("plugin", "version");
				String description = cfg->get_value("plugin", "description");
				String scr = cfg->get_value("plugin", "script");

				bool is_enabled = EditorNode::get_singleton()->is_addon_plugin_enabled(path);
				Color disabled_color = get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor));

				const PackedInt32Array boundaries = TS->string_get_word_breaks(description, "", 80);
				String wrapped_description;

				for (int j = 0; j < boundaries.size(); j += 2) {
					const int start = boundaries[j];
					const int end = boundaries[j + 1];
					wrapped_description += "\n" + description.substr(start, end - start + 1).rstrip("\n");
				}

				TreeItem *item = plugin_list->create_item(root);
				item->set_text(COLUMN_NAME, name);
				if (!is_enabled) {
					item->set_custom_color(COLUMN_NAME, disabled_color);
				}
				item->set_tooltip_text(COLUMN_NAME, vformat(TTR("Name: %s\nPath: %s\nMain Script: %s\n\n%s"), name, path, scr, wrapped_description));
				item->set_metadata(COLUMN_NAME, path);
				item->set_text(COLUMN_VERSION, version);
				item->set_custom_font(COLUMN_VERSION, get_theme_font("source", EditorStringName(EditorFonts)));
				item->set_metadata(COLUMN_VERSION, scr);
				item->set_text(COLUMN_AUTHOR, author);
				item->set_metadata(COLUMN_AUTHOR, description);
				item->set_cell_mode(COLUMN_STATUS, TreeItem::CELL_MODE_CHECK);
				item->set_text(COLUMN_STATUS, TTR("On"));
				item->set_checked(COLUMN_STATUS, is_enabled);
				item->set_editable(COLUMN_STATUS, true);
				item->add_button(COLUMN_EDIT, get_editor_theme_icon(SNAME("Edit")), BUTTON_PLUGIN_EDIT, false, TTR("Edit Plugin"));
			}
		}
	}

	updating = false;
}

void EditorPluginSettings::_plugin_activity_changed() {
	if (updating) {
		return;
	}

	TreeItem *ti = plugin_list->get_edited();
	ERR_FAIL_NULL(ti);
	bool checked = ti->is_checked(COLUMN_STATUS);
	String name = ti->get_metadata(COLUMN_NAME);

	EditorNode::get_singleton()->set_addon_plugin_enabled(name, checked, true);

	bool is_enabled = EditorNode::get_singleton()->is_addon_plugin_enabled(name);

	if (is_enabled != checked) {
		updating = true;
		ti->set_checked(COLUMN_STATUS, is_enabled);
		updating = false;
	}
	if (is_enabled) {
		ti->clear_custom_color(COLUMN_NAME);
	} else {
		ti->set_custom_color(COLUMN_NAME, get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
	}
}

void EditorPluginSettings::_create_clicked() {
	plugin_config_dialog->config("");
	plugin_config_dialog->popup_centered();
}

void EditorPluginSettings::_cell_button_pressed(Object *p_item, int p_column, int p_id, MouseButton p_button) {
	if (p_button != MouseButton::LEFT) {
		return;
	}
	TreeItem *item = Object::cast_to<TreeItem>(p_item);
	if (!item) {
		return;
	}
	if (p_id == BUTTON_PLUGIN_EDIT) {
		if (p_column == COLUMN_EDIT) {
			String dir = item->get_metadata(COLUMN_NAME);
			plugin_config_dialog->config(dir);
			plugin_config_dialog->popup_centered();
		}
	}
}

Vector<String> EditorPluginSettings::_get_plugins(const String &p_dir) {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	Error err = da->change_dir(p_dir);
	if (err != OK) {
		return Vector<String>();
	}

	Vector<String> plugins;
	da->list_dir_begin();
	for (String path = da->get_next(); !path.is_empty(); path = da->get_next()) {
		if (path[0] == '.' || !da->current_is_dir()) {
			continue;
		}

		const String full_path = p_dir.path_join(path);
		const String plugin_config = full_path.path_join("plugin.cfg");
		if (FileAccess::exists(plugin_config)) {
			plugins.push_back(plugin_config);
		} else {
			plugins.append_array(_get_plugins(full_path));
		}
	}

	da->list_dir_end();
	return plugins;
}

EditorPluginSettings::EditorPluginSettings() {
	ProjectSettings::get_singleton()->add_hidden_prefix("editor_plugins/");

	plugin_config_dialog = memnew(PluginConfigDialog);
	plugin_config_dialog->config("");
	add_child(plugin_config_dialog);

	HBoxContainer *title_hb = memnew(HBoxContainer);
	Label *label = memnew(Label(TTR("Installed Plugins:")));
	label->set_theme_type_variation("HeaderSmall");
	title_hb->add_child(label);
	title_hb->add_spacer();
	Button *create_plugin_button = memnew(Button(TTR("Create New Plugin")));
	create_plugin_button->connect(SceneStringName(pressed), callable_mp(this, &EditorPluginSettings::_create_clicked));
	title_hb->add_child(create_plugin_button);
	add_child(title_hb);

	plugin_list = memnew(Tree);
	plugin_list->set_v_size_flags(SIZE_EXPAND_FILL);
	plugin_list->set_columns(COLUMN_MAX);
	plugin_list->set_column_titles_visible(true);
	plugin_list->set_column_title(COLUMN_STATUS, TTR("Enabled"));
	plugin_list->set_column_title(COLUMN_NAME, TTR("Name"));
	plugin_list->set_column_title(COLUMN_VERSION, TTR("Version"));
	plugin_list->set_column_title(COLUMN_AUTHOR, TTR("Author"));
	plugin_list->set_column_title(COLUMN_EDIT, TTR("Edit"));
	plugin_list->set_column_title_alignment(COLUMN_STATUS, HORIZONTAL_ALIGNMENT_LEFT);
	plugin_list->set_column_title_alignment(COLUMN_NAME, HORIZONTAL_ALIGNMENT_LEFT);
	plugin_list->set_column_title_alignment(COLUMN_VERSION, HORIZONTAL_ALIGNMENT_LEFT);
	plugin_list->set_column_title_alignment(COLUMN_AUTHOR, HORIZONTAL_ALIGNMENT_LEFT);
	plugin_list->set_column_title_alignment(COLUMN_EDIT, HORIZONTAL_ALIGNMENT_LEFT);
	plugin_list->set_column_expand(COLUMN_PADDING_LEFT, false);
	plugin_list->set_column_expand(COLUMN_STATUS, false);
	plugin_list->set_column_expand(COLUMN_NAME, true);
	plugin_list->set_column_expand(COLUMN_VERSION, false);
	plugin_list->set_column_expand(COLUMN_AUTHOR, false);
	plugin_list->set_column_expand(COLUMN_EDIT, false);
	plugin_list->set_column_expand(COLUMN_PADDING_RIGHT, false);
	plugin_list->set_column_clip_content(COLUMN_STATUS, true);
	plugin_list->set_column_clip_content(COLUMN_NAME, true);
	plugin_list->set_column_clip_content(COLUMN_VERSION, true);
	plugin_list->set_column_clip_content(COLUMN_AUTHOR, true);
	plugin_list->set_column_clip_content(COLUMN_EDIT, true);
	plugin_list->set_column_custom_minimum_width(COLUMN_PADDING_LEFT, 10 * EDSCALE);
	plugin_list->set_column_custom_minimum_width(COLUMN_STATUS, 80 * EDSCALE);
	plugin_list->set_column_custom_minimum_width(COLUMN_VERSION, 100 * EDSCALE);
	plugin_list->set_column_custom_minimum_width(COLUMN_AUTHOR, 250 * EDSCALE);
	plugin_list->set_column_custom_minimum_width(COLUMN_EDIT, 40 * EDSCALE);
	plugin_list->set_column_custom_minimum_width(COLUMN_PADDING_RIGHT, 10 * EDSCALE);
	plugin_list->set_hide_root(true);
	plugin_list->connect("item_edited", callable_mp(this, &EditorPluginSettings::_plugin_activity_changed), CONNECT_DEFERRED);

	VBoxContainer *mc = memnew(VBoxContainer);
	mc->add_child(plugin_list);
	mc->set_v_size_flags(SIZE_EXPAND_FILL);
	mc->set_h_size_flags(SIZE_EXPAND_FILL);

	add_child(mc);
}
