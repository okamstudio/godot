/**************************************************************************/
/*  project_settings_gdextension.cpp                                      */
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

#include "project_settings_gdextension.h"

#include "cpp_scons/cpp_scons_gdext_creator.h"
#include "gdextension_create_dialog.h"
#include "gdextension_creator_plugin.h"
#include "gdextension_edit_dialog.h"

#include "core/extension/gdextension_manager.h"
#include "editor/editor_file_system.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/tree.h"

class GDExtensionPluginCreatorBase;

const int BUTTON_EDIT = 0;

void ProjectSettingsGDExtension::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_WM_WINDOW_FOCUS_IN: {
			_update_extension_tree();
		} break;
		case Node::NOTIFICATION_READY: {
			extension_list->connect("button_clicked", callable_mp(this, &ProjectSettingsGDExtension::_cell_button_pressed));
		} break;
	}
}

void ProjectSettingsGDExtension::_cell_button_pressed(Object *p_item, int p_column, int p_id, MouseButton p_button) {
	if (p_button != MouseButton::LEFT) {
		return;
	}
	TreeItem *item = Object::cast_to<TreeItem>(p_item);
	if (!item) {
		return;
	}
	if (p_id == BUTTON_EDIT && p_column == COLUMN_EDIT) {
		String path = item->get_metadata(COLUMN_PATH);
		config_dialog->load_gdextension_config(path);
		config_dialog->popup_centered(Size2(400, 300) * EDSCALE);
	}
}

void ProjectSettingsGDExtension::_update_extension_tree() {
	extension_list->clear();
	TreeItem *root = extension_list->create_item();
	Vector<String> extensions = GDExtensionManager::get_singleton()->get_loaded_extensions();
	for (const String &extension : extensions) {
		TreeItem *item = extension_list->create_item(root);
		item->set_text(COLUMN_PATH, extension);
		item->add_button(COLUMN_EDIT, get_editor_theme_icon(SNAME("Edit")), BUTTON_EDIT, false, TTR("Edit Extension"));
		item->set_metadata(COLUMN_PATH, extension);
	}
}

ProjectSettingsGDExtension::ProjectSettingsGDExtension() {
	create_dialog = memnew(GDExtensionCreateDialog);
	add_child(create_dialog);
	create_dialog->connect("gdextension_created", callable_mp(this, &ProjectSettingsGDExtension::_on_gdextension_created));
	config_dialog = memnew(GDExtensionEditDialog);
	add_child(config_dialog);

	HBoxContainer *title_hb = memnew(HBoxContainer);
	Label *label = memnew(Label(TTR("Installed GDExtensions:")));
	label->set_theme_type_variation("HeaderSmall");
	title_hb->add_child(label);
	title_hb->add_spacer();
	Button *create_plugin_button = memnew(Button(TTR("Create GDExtension")));
	create_plugin_button->connect(SceneStringName(pressed), callable_mp(this, &ProjectSettingsGDExtension::_on_create_gdextension_pressed));
	title_hb->add_child(create_plugin_button);
	add_child(title_hb);

	extension_list = memnew(Tree);
	extension_list->set_v_size_flags(SIZE_EXPAND_FILL);
	extension_list->set_hide_root(true);
	extension_list->set_columns(COLUMN_MAX);
	extension_list->set_column_titles_visible(true);
	extension_list->set_column_title(COLUMN_PATH, TTR("Path"));
	extension_list->set_column_title(COLUMN_EDIT, TTR("Edit"));
	extension_list->set_column_title_alignment(COLUMN_PATH, HORIZONTAL_ALIGNMENT_LEFT);
	extension_list->set_column_title_alignment(COLUMN_EDIT, HORIZONTAL_ALIGNMENT_LEFT);
	extension_list->set_column_expand(COLUMN_PADDING_LEFT, false);
	extension_list->set_column_expand(COLUMN_PATH, true);
	extension_list->set_column_expand(COLUMN_EDIT, false);
	extension_list->set_column_expand(COLUMN_PADDING_RIGHT, false);
	extension_list->set_column_clip_content(COLUMN_PADDING_LEFT, true);
	extension_list->set_column_clip_content(COLUMN_PATH, true);
	extension_list->set_column_clip_content(COLUMN_EDIT, true);
	extension_list->set_column_clip_content(COLUMN_PADDING_RIGHT, true);
	extension_list->set_column_custom_minimum_width(COLUMN_PADDING_LEFT, 10 * EDSCALE);
	extension_list->set_column_custom_minimum_width(COLUMN_PATH, 300 * EDSCALE);
	extension_list->set_column_custom_minimum_width(COLUMN_EDIT, 40 * EDSCALE);
	extension_list->set_column_custom_minimum_width(COLUMN_PADDING_RIGHT, 10 * EDSCALE);

	add_child(extension_list);
}

void ProjectSettingsGDExtension::_on_create_gdextension_pressed() {
	Vector<Ref<GDExtensionCreatorPlugin>> creators;
	creators.append(memnew(CppSconsGDExtensionCreator));
	create_dialog->load_plugin_creators(creators);
	create_dialog->popup_centered(Size2(400, 300) * EDSCALE);
}

void ProjectSettingsGDExtension::_on_gdextension_created() {
	EditorFileSystem::get_singleton()->scan();
	_update_extension_tree();
}
