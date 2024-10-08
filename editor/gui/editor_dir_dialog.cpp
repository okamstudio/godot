/**************************************************************************/
/*  editor_dir_dialog.cpp                                                 */
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

#include "editor_dir_dialog.h"

#include "editor/directory_create_dialog.h"
#include "editor/editor_file_system.h"
#include "editor/filesystem_dock.h"
#include "editor/themes/editor_theme_manager.h"
#include "scene/gui/box_container.h"
#include "scene/gui/tree.h"
#include "servers/display_server.h"

void EditorDirDialog::_update_dir(const Color &p_default_folder_color, const Dictionary &p_assigned_folder_colors, const HashMap<String, Color> &p_folder_colors, bool p_is_dark_theme, TreeItem *p_item, EditorFileSystemDirectory *p_dir, const String &p_select_path) {
	updating = true;

	const String path = p_dir->get_path();

	p_item->set_metadata(0, path);
	p_item->set_icon(0, tree->get_editor_theme_icon(SNAME("Folder")));

	if (!p_item->get_parent()) {
		p_item->set_text(0, "res://");
		p_item->set_icon_modulate(0, p_default_folder_color);
	} else {
		if (!opened_paths.has(path) && (p_select_path.is_empty() || !p_select_path.begins_with(path))) {
			p_item->set_collapsed(true);
		}

		p_item->set_text(0, p_dir->get_name());

		if (p_assigned_folder_colors.has(path)) {
			const Color &folder_color = p_folder_colors[p_assigned_folder_colors[path]];
			p_item->set_icon_modulate(0, p_is_dark_theme ? folder_color : folder_color * FileSystemDock::ITEM_COLOR_SCALE);
			p_item->set_custom_bg_color(0, Color(folder_color, p_is_dark_theme ? FileSystemDock::ITEM_ALPHA_MIN : FileSystemDock::ITEM_ALPHA_MAX));
		} else {
			TreeItem *parent_item = p_item->get_parent();
			Color parent_bg_color = parent_item->get_custom_bg_color(0);
			if (parent_bg_color != Color()) {
				p_item->set_custom_bg_color(0, p_assigned_folder_colors.has(parent_item->get_metadata(0)) ? parent_bg_color.darkened(FileSystemDock::ITEM_BG_DARK_SCALE) : parent_bg_color);
				p_item->set_icon_modulate(0, parent_item->get_icon_modulate(0));
			} else {
				p_item->set_icon_modulate(0, p_default_folder_color);
			}
		}
	}

	if (path == new_dir_path || !p_item->get_parent()) {
		p_item->select(0);
	}

	updating = false;
	for (int i = 0; i < p_dir->get_subdir_count(); i++) {
		TreeItem *ti = tree->create_item(p_item);
		_update_dir(p_default_folder_color, p_assigned_folder_colors, p_folder_colors, p_is_dark_theme, ti, p_dir->get_subdir(i));
	}
}

void EditorDirDialog::config(const Vector<String> &p_paths) {
	ERR_FAIL_COND(p_paths.is_empty());

	if (p_paths.size() == 1) {
		String path = p_paths[0];
		if (path.ends_with("/")) {
			path = path.substr(0, path.length() - 1);
		}
		// TRANSLATORS: %s is the file name that will be moved or duplicated.
		set_title(vformat(TTR("Move/Duplicate: %s"), path.get_file()));
	} else {
		// TRANSLATORS: %d is the number of files that will be moved or duplicated.
		set_title(vformat(TTRN("Move/Duplicate %d Item", "Move/Duplicate %d Items", p_paths.size()), p_paths.size()));
	}
}

void EditorDirDialog::reload(const String &p_path) {
	if (!is_visible()) {
		must_reload = true;
		return;
	}

	tree->clear();
	TreeItem *root = tree->create_item();
	_update_dir(tree->get_theme_color(SNAME("folder_icon_color"), SNAME("FileDialog")), FileSystemDock::get_singleton()->get_assigned_folder_colors(), FileSystemDock::get_singleton()->get_folder_colors(), EditorThemeManager::is_dark_theme(), root, EditorFileSystem::get_singleton()->get_filesystem(), p_path);
	_item_collapsed(root);
	new_dir_path.clear();
	must_reload = false;
}

void EditorDirDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			FileSystemDock::get_singleton()->connect("folder_color_changed", callable_mp(this, &EditorDirDialog::reload).bind(""));
			EditorFileSystem::get_singleton()->connect("filesystem_changed", callable_mp(this, &EditorDirDialog::reload).bind(""));
			reload();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			EditorFileSystem::get_singleton()->disconnect("filesystem_changed", callable_mp(this, &EditorDirDialog::reload));
			FileSystemDock::get_singleton()->disconnect("folder_color_changed", callable_mp(this, &EditorDirDialog::reload));
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (must_reload && is_visible()) {
				reload();
			}
		} break;
	}
}

void EditorDirDialog::_item_collapsed(Object *p_item) {
	TreeItem *item = Object::cast_to<TreeItem>(p_item);

	if (updating) {
		return;
	}

	if (item->is_collapsed()) {
		opened_paths.erase(item->get_metadata(0));
	} else {
		opened_paths.insert(item->get_metadata(0));
	}
}

void EditorDirDialog::_item_activated() {
	TreeItem *ti = tree->get_selected();
	ERR_FAIL_NULL(ti);
	if (ti->get_child_count() > 0) {
		ti->set_collapsed(!ti->is_collapsed());
	}
}

void EditorDirDialog::_copy_pressed() {
	TreeItem *ti = tree->get_selected();
	ERR_FAIL_NULL(ti);

	hide();
	emit_signal(SNAME("copy_pressed"), ti->get_metadata(0));
}

void EditorDirDialog::ok_pressed() {
	TreeItem *ti = tree->get_selected();
	ERR_FAIL_NULL(ti);

	hide();
	emit_signal(SNAME("move_pressed"), ti->get_metadata(0));
}

void EditorDirDialog::_make_dir() {
	TreeItem *ti = tree->get_selected();
	ERR_FAIL_NULL(ti);
	const String &directory = ti->get_metadata(0);
	makedialog->config(directory, callable_mp(this, &EditorDirDialog::_make_dir_confirm).bind(directory), DirectoryCreateDialog::MODE_DIRECTORY, "new folder");
	makedialog->popup_centered();
}

void EditorDirDialog::_make_dir_confirm(const String &p_path, const String &p_base_dir) {
	FileSystemDock::get_singleton()->create_directory(p_path, p_base_dir);

	// Multiple level of directories can be created at once.
	String base_dir = p_path.get_base_dir();
	while (true) {
		opened_paths.insert(base_dir + "/");
		if (base_dir == "res://") {
			break;
		}
		base_dir = base_dir.get_base_dir();
	}

	new_dir_path = p_path + "/";
}

void EditorDirDialog::_bind_methods() {
	ADD_SIGNAL(MethodInfo("copy_pressed", PropertyInfo(Variant::STRING, "dir")));
	ADD_SIGNAL(MethodInfo("move_pressed", PropertyInfo(Variant::STRING, "dir")));
}

EditorDirDialog::EditorDirDialog() {
	set_hide_on_ok(false);

	VBoxContainer *vb = memnew(VBoxContainer);
	add_child(vb);

	HBoxContainer *hb = memnew(HBoxContainer);
	vb->add_child(hb);

	hb->add_child(memnew(Label(TTR("Choose target directory:"))));
	hb->add_spacer();

	makedir = memnew(Button(TTR("Create Folder")));
	hb->add_child(makedir);
	makedir->connect(SceneStringName(pressed), callable_mp(this, &EditorDirDialog::_make_dir));

	tree = memnew(Tree);
	tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	vb->add_child(tree);
	tree->connect("item_activated", callable_mp(this, &EditorDirDialog::_item_activated));
	tree->connect("item_collapsed", callable_mp(this, &EditorDirDialog::_item_collapsed), CONNECT_DEFERRED);

	set_ok_button_text(TTR("Move"));

	copy = add_button(TTR("Copy"), !DisplayServer::get_singleton()->get_swap_cancel_ok());
	copy->connect(SceneStringName(pressed), callable_mp(this, &EditorDirDialog::_copy_pressed));

	makedialog = memnew(DirectoryCreateDialog);
	add_child(makedialog);
}
