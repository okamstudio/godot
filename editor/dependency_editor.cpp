/**************************************************************************/
/*  dependency_editor.cpp                                                 */
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

#include "dependency_editor.h"

#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/margin_container.h"

void DependencyEditor::_searched(const String &p_path) {
	HashMap<String, String> dep_rename;
	dep_rename[replacing] = p_path;

	ResourceLoader::rename_dependencies(editing, dep_rename);

	_update_list();
	_update_file();
}

void DependencyEditor::_load_pressed(Object *p_item, int p_cell, int p_button, MouseButton p_mouse_button) {
	if (p_mouse_button != MouseButton::LEFT) {
		return;
	}
	TreeItem *ti = Object::cast_to<TreeItem>(p_item);
	replacing = ti->get_text(1);

	search->set_title(TTR("Search Replacement For:") + " " + replacing.get_file());

	// Set directory to closest existing directory.
	search->set_current_dir(replacing.get_base_dir());

	search->clear_filters();
	List<String> ext;
	ResourceLoader::get_recognized_extensions_for_type(ti->get_metadata(0), &ext);
	for (const String &E : ext) {
		search->add_filter("*." + E);
	}
	search->popup_file_dialog();
}

void DependencyEditor::_fix_and_find(EditorFileSystemDirectory *efsd, HashMap<String, HashMap<String, String>> &candidates) {
	for (int i = 0; i < efsd->get_subdir_count(); i++) {
		_fix_and_find(efsd->get_subdir(i), candidates);
	}

	for (int i = 0; i < efsd->get_file_count(); i++) {
		String file = efsd->get_file(i);
		if (!candidates.has(file)) {
			continue;
		}

		String path = efsd->get_file_path(i);

		for (KeyValue<String, String> &E : candidates[file]) {
			if (E.value.is_empty()) {
				E.value = path;
				continue;
			}

			//must match the best, using subdirs
			String existing = E.value.replace_first("res://", "");
			String current = path.replace_first("res://", "");
			String lost = E.key.replace_first("res://", "");

			Vector<String> existingv = existing.split("/");
			existingv.reverse();
			Vector<String> currentv = current.split("/");
			currentv.reverse();
			Vector<String> lostv = lost.split("/");
			lostv.reverse();

			int existing_score = 0;
			int current_score = 0;

			for (int j = 0; j < lostv.size(); j++) {
				if (j < existingv.size() && lostv[j] == existingv[j]) {
					existing_score++;
				}
				if (j < currentv.size() && lostv[j] == currentv[j]) {
					current_score++;
				}
			}

			if (current_score > existing_score) {
				//if it was the same, could track distance to new path but..

				E.value = path; //replace by more accurate
			}
		}
	}
}

void DependencyEditor::_fix_all() {
	if (!EditorFileSystem::get_singleton()->get_filesystem()) {
		return;
	}

	HashMap<String, HashMap<String, String>> candidates;

	for (const String &E : missing) {
		String base = E.get_file();
		if (!candidates.has(base)) {
			candidates[base] = HashMap<String, String>();
		}

		candidates[base][E] = "";
	}

	_fix_and_find(EditorFileSystem::get_singleton()->get_filesystem(), candidates);

	HashMap<String, String> remaps;

	for (KeyValue<String, HashMap<String, String>> &E : candidates) {
		for (const KeyValue<String, String> &F : E.value) {
			if (!F.value.is_empty()) {
				remaps[F.key] = F.value;
			}
		}
	}

	if (remaps.size()) {
		ResourceLoader::rename_dependencies(editing, remaps);

		_update_list();
		_update_file();
	}
}

void DependencyEditor::_update_file() {
	EditorFileSystem::get_singleton()->update_file(editing);
}

void DependencyEditor::_update_list() {
	List<String> deps;
	ResourceLoader::get_dependencies(editing, &deps, true);

	tree->clear();
	missing.clear();

	TreeItem *root = tree->create_item();

	Ref<Texture2D> folder = tree->get_theme_icon(SNAME("folder"), SNAME("FileDialog"));

	bool broken = false;

	for (const String &n : deps) {
		TreeItem *item = tree->create_item(root);
		String path;
		String type;

		if (n.contains("::")) {
			path = n.get_slice("::", 0);
			type = n.get_slice("::", 1);
		} else {
			path = n;
			type = "Resource";
		}

		ResourceUID::ID uid = ResourceUID::get_singleton()->text_to_id(path);
		if (uid != ResourceUID::INVALID_ID) {
			// Dependency is in uid format, obtain proper path.
			if (ResourceUID::get_singleton()->has_id(uid)) {
				path = ResourceUID::get_singleton()->get_id_path(uid);
			} else if (n.get_slice_count("::") >= 3) {
				// If uid can't be found, try to use fallback path.
				path = n.get_slice("::", 2);
			} else {
				ERR_PRINT("Invalid dependency UID and fallback path.");
				continue;
			}
		}

		String name = path.get_file();

		Ref<Texture2D> icon = EditorNode::get_singleton()->get_class_icon(type);
		item->set_text(0, name);
		item->set_icon(0, icon);
		item->set_metadata(0, type);
		item->set_text(1, path);

		if (!FileAccess::exists(path)) {
			item->set_custom_color(1, Color(1, 0.4, 0.3));
			missing.push_back(path);
			broken = true;
		}

		item->add_button(1, folder, 0);
	}

	fixdeps->set_disabled(!broken);
}

void DependencyEditor::edit(const String &p_path) {
	editing = p_path;
	set_title(TTR("Dependencies For:") + " " + p_path.get_file());

	_update_list();
	popup_centered_ratio(0.4);

	if (EditorNode::get_singleton()->is_scene_open(p_path)) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("Scene '%s' is currently being edited.\nChanges will only take effect when reloaded."), p_path.get_file()));
	} else if (ResourceCache::has(p_path)) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("Resource '%s' is in use.\nChanges will only take effect when reloaded."), p_path.get_file()));
	}
}

DependencyEditor::DependencyEditor() {
	VBoxContainer *vb = memnew(VBoxContainer);
	vb->set_name(TTR("Dependencies"));
	add_child(vb);

	tree = memnew(Tree);
	tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	tree->set_columns(2);
	tree->set_column_titles_visible(true);
	tree->set_column_title(0, TTR("Resource"));
	tree->set_column_clip_content(0, true);
	tree->set_column_expand_ratio(0, 2);
	tree->set_column_title(1, TTR("Path"));
	tree->set_column_clip_content(1, true);
	tree->set_column_expand_ratio(1, 1);
	tree->set_hide_root(true);
	tree->connect("button_clicked", callable_mp(this, &DependencyEditor::_load_pressed));

	HBoxContainer *hbc = memnew(HBoxContainer);
	Label *label = memnew(Label(TTR("Dependencies:")));
	label->set_theme_type_variation("HeaderSmall");

	hbc->add_child(label);
	hbc->add_spacer();
	fixdeps = memnew(Button(TTR("Fix Broken")));
	hbc->add_child(fixdeps);
	fixdeps->connect(SceneStringName(pressed), callable_mp(this, &DependencyEditor::_fix_all));

	vb->add_child(hbc);

	MarginContainer *mc = memnew(MarginContainer);
	mc->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	mc->add_child(tree);
	vb->add_child(mc);

	set_title(TTR("Dependency Editor"));
	search = memnew(EditorFileDialog);
	search->connect("file_selected", callable_mp(this, &DependencyEditor::_searched));
	search->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	search->set_title(TTR("Search Replacement Resource:"));
	add_child(search);
}

/////////////////////////////////////
void DependencyEditorOwners::_list_rmb_clicked(int p_item, const Vector2 &p_pos, MouseButton p_mouse_button_index) {
	if (p_mouse_button_index != MouseButton::RIGHT) {
		return;
	}

	file_options->clear();
	file_options->reset_size();
	if (p_item >= 0) {
		PackedInt32Array selected_items = owners->get_selected_items();
		bool only_scenes_selected = true;

		for (int i = 0; i < selected_items.size(); i++) {
			int item_idx = selected_items[i];
			if (ResourceLoader::get_resource_type(owners->get_item_text(item_idx)) != "PackedScene") {
				only_scenes_selected = false;
				break;
			}
		}

		if (only_scenes_selected) {
			file_options->add_icon_item(get_editor_theme_icon(SNAME("Load")), TTRN("Open Scene", "Open Scenes", selected_items.size()), FILE_OPEN);
		} else if (selected_items.size() == 1) {
			file_options->add_icon_item(get_editor_theme_icon(SNAME("Load")), TTR("Open"), FILE_OPEN);
		} else {
			return;
		}
	}

	file_options->set_position(owners->get_screen_position() + p_pos);
	file_options->reset_size();
	file_options->popup();
}

void DependencyEditorOwners::_select_file(int p_idx) {
	String fpath = owners->get_item_text(p_idx);

	if (ResourceLoader::get_resource_type(fpath) == "PackedScene") {
		EditorNode::get_singleton()->open_request(fpath);
	} else {
		EditorNode::get_singleton()->load_resource(fpath);
	}
	hide();
	emit_signal(SceneStringName(confirmed));
}

void DependencyEditorOwners::_empty_clicked(const Vector2 &p_pos, MouseButton p_mouse_button_index) {
	if (p_mouse_button_index != MouseButton::LEFT) {
		return;
	}

	owners->deselect_all();
}

void DependencyEditorOwners::_file_option(int p_option) {
	switch (p_option) {
		case FILE_OPEN: {
			PackedInt32Array selected_items = owners->get_selected_items();
			for (int i = 0; i < selected_items.size(); i++) {
				int item_idx = selected_items[i];
				if (item_idx < 0 || item_idx >= owners->get_item_count()) {
					break;
				}
				_select_file(item_idx);
			}
		} break;
	}
}

void DependencyEditorOwners::_fill_owners(EditorFileSystemDirectory *efsd) {
	if (!efsd) {
		return;
	}

	for (int i = 0; i < efsd->get_subdir_count(); i++) {
		_fill_owners(efsd->get_subdir(i));
	}

	for (int i = 0; i < efsd->get_file_count(); i++) {
		Vector<String> deps = efsd->get_file_deps(i);
		bool found = false;
		for (int j = 0; j < deps.size(); j++) {
			if (deps[j] == editing) {
				found = true;
				break;
			}
		}
		if (!found) {
			continue;
		}

		Ref<Texture2D> icon = EditorNode::get_singleton()->get_class_icon(efsd->get_file_type(i));

		owners->add_item(efsd->get_file_path(i), icon);
	}
}

void DependencyEditorOwners::show(const String &p_path) {
	editing = p_path;
	owners->clear();
	_fill_owners(EditorFileSystem::get_singleton()->get_filesystem());
	popup_centered_ratio(0.3);

	set_title(vformat(TTR("Owners of: %s (Total: %d)"), p_path.get_file(), owners->get_item_count()));
}

DependencyEditorOwners::DependencyEditorOwners() {
	file_options = memnew(PopupMenu);
	add_child(file_options);
	file_options->connect(SceneStringName(id_pressed), callable_mp(this, &DependencyEditorOwners::_file_option));

	owners = memnew(ItemList);
	owners->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	owners->set_select_mode(ItemList::SELECT_MULTI);
	owners->connect("item_clicked", callable_mp(this, &DependencyEditorOwners::_list_rmb_clicked));
	owners->connect("item_activated", callable_mp(this, &DependencyEditorOwners::_select_file));
	owners->connect("empty_clicked", callable_mp(this, &DependencyEditorOwners::_empty_clicked));
	owners->set_allow_rmb_select(true);
	add_child(owners);
}

///////////////////////

void DependencyRemoveDialog::_find_files_in_removed_folder(EditorFileSystemDirectory *efsd, const String &p_folder) {
	if (!efsd) {
		return;
	}

	for (int i = 0; i < efsd->get_subdir_count(); ++i) {
		_find_files_in_removed_folder(efsd->get_subdir(i), p_folder);
	}
	for (int i = 0; i < efsd->get_file_count(); i++) {
		String file = efsd->get_file_path(i);
		ERR_FAIL_COND(all_remove_files.has(file)); //We are deleting a directory which is contained in a directory we are deleting...
		all_remove_files[file] = p_folder; //Point the file to the ancestor directory we are deleting so we know what to parent it under in the tree.
	}
}

void DependencyRemoveDialog::_find_all_removed_dependencies(EditorFileSystemDirectory *efsd, Vector<RemovedDependency> &p_removed) {
	if (!efsd) {
		return;
	}

	for (int i = 0; i < efsd->get_subdir_count(); i++) {
		_find_all_removed_dependencies(efsd->get_subdir(i), p_removed);
	}

	for (int i = 0; i < efsd->get_file_count(); i++) {
		const String path = efsd->get_file_path(i);

		//It doesn't matter if a file we are about to delete will have some of its dependencies removed too
		if (all_remove_files.has(path)) {
			continue;
		}

		Vector<String> all_deps = efsd->get_file_deps(i);
		for (int j = 0; j < all_deps.size(); ++j) {
			if (all_remove_files.has(all_deps[j])) {
				RemovedDependency dep;
				dep.file = path;
				dep.file_type = efsd->get_file_type(i);
				dep.dependency = all_deps[j];
				dep.dependency_folder = all_remove_files[all_deps[j]];
				p_removed.push_back(dep);
			}
		}
	}
}

void DependencyRemoveDialog::_find_localization_remaps_of_removed_files(Vector<RemovedDependency> &p_removed) {
	for (KeyValue<String, String> &files : all_remove_files) {
		const String &path = files.key;

		// Look for dependencies in the translation remaps.
		if (ProjectSettings::get_singleton()->has_setting("internationalization/locale/translation_remaps")) {
			Dictionary remaps = GLOBAL_GET("internationalization/locale/translation_remaps");

			if (remaps.has(path)) {
				RemovedDependency dep;
				dep.file = TTR("Localization remap");
				dep.file_type = "";
				dep.dependency = path;
				dep.dependency_folder = files.value;
				p_removed.push_back(dep);
			}

			Array remap_keys = remaps.keys();
			for (int j = 0; j < remap_keys.size(); j++) {
				PackedStringArray remapped_files = remaps[remap_keys[j]];
				for (int k = 0; k < remapped_files.size(); k++) {
					int splitter_pos = remapped_files[k].rfind_char(':');
					String res_path = remapped_files[k].substr(0, splitter_pos);
					if (res_path == path) {
						String locale_name = remapped_files[k].substr(splitter_pos + 1);

						RemovedDependency dep;
						dep.file = vformat(TTR("Localization remap for path '%s' and locale '%s'."), remap_keys[j], locale_name);
						dep.file_type = "";
						dep.dependency = path;
						dep.dependency_folder = files.value;
						p_removed.push_back(dep);
					}
				}
			}
		}
	}
}

void DependencyRemoveDialog::_build_removed_dependency_tree(const Vector<RemovedDependency> &p_removed) {
	owners->clear();
	owners->create_item(); // root

	HashMap<String, TreeItem *> tree_items;
	for (int i = 0; i < p_removed.size(); i++) {
		RemovedDependency rd = p_removed[i];

		//Ensure that the dependency is already in the tree
		if (!tree_items.has(rd.dependency)) {
			if (rd.dependency_folder.length() > 0) {
				//Ensure the ancestor folder is already in the tree
				if (!tree_items.has(rd.dependency_folder)) {
					TreeItem *folder_item = owners->create_item(owners->get_root());
					folder_item->set_text(0, rd.dependency_folder);
					folder_item->set_icon(0, owners->get_editor_theme_icon(SNAME("Folder")));
					tree_items[rd.dependency_folder] = folder_item;
				}
				TreeItem *dependency_item = owners->create_item(tree_items[rd.dependency_folder]);
				dependency_item->set_text(0, rd.dependency);
				dependency_item->set_icon(0, owners->get_editor_theme_icon(SNAME("Warning")));
				tree_items[rd.dependency] = dependency_item;
			} else {
				TreeItem *dependency_item = owners->create_item(owners->get_root());
				dependency_item->set_text(0, rd.dependency);
				dependency_item->set_icon(0, owners->get_editor_theme_icon(SNAME("Warning")));
				tree_items[rd.dependency] = dependency_item;
			}
		}

		//List this file under this dependency
		Ref<Texture2D> icon = EditorNode::get_singleton()->get_class_icon(rd.file_type);
		TreeItem *file_item = owners->create_item(tree_items[rd.dependency]);
		file_item->set_text(0, rd.file);
		file_item->set_icon(0, icon);
	}
}

void DependencyRemoveDialog::_show_files_to_delete_list() {
	files_to_delete_list->clear();

	for (const String &s : dirs_to_delete) {
		String t = s.trim_prefix("res://");
		files_to_delete_list->add_item(t, Ref<Texture2D>(), false);
	}

	for (const String &s : files_to_delete) {
		String t = s.trim_prefix("res://");
		files_to_delete_list->add_item(t, Ref<Texture2D>(), false);
	}
}

void DependencyRemoveDialog::show(const Vector<String> &p_folders, const Vector<String> &p_files) {
	all_remove_files.clear();
	dirs_to_delete.clear();
	files_to_delete.clear();
	owners->clear();

	for (int i = 0; i < p_folders.size(); ++i) {
		String folder = p_folders[i].ends_with("/") ? p_folders[i] : (p_folders[i] + "/");
		_find_files_in_removed_folder(EditorFileSystem::get_singleton()->get_filesystem_path(folder), folder);
		dirs_to_delete.push_back(folder);
	}
	for (int i = 0; i < p_files.size(); ++i) {
		all_remove_files[p_files[i]] = String();
		files_to_delete.push_back(p_files[i]);
	}

	_show_files_to_delete_list();

	Vector<RemovedDependency> removed_deps;
	_find_all_removed_dependencies(EditorFileSystem::get_singleton()->get_filesystem(), removed_deps);
	_find_localization_remaps_of_removed_files(removed_deps);
	removed_deps.sort();
	if (removed_deps.is_empty()) {
		vb_owners->hide();
		text->set_text(TTR("Remove the selected files from the project? (Cannot be undone.)\nDepending on your filesystem configuration, the files will either be moved to the system trash or deleted permanently."));
		reset_size();
		popup_centered();
	} else {
		_build_removed_dependency_tree(removed_deps);
		vb_owners->show();
		text->set_text(TTR("The files being removed are required by other resources in order for them to work.\nRemove them anyway? (Cannot be undone.)\nDepending on your filesystem configuration, the files will either be moved to the system trash or deleted permanently."));
		popup_centered(Size2(500, 350));
	}

	EditorFileSystem::get_singleton()->scan_changes();
}

void DependencyRemoveDialog::ok_pressed() {
	for (const KeyValue<String, String> &E : all_remove_files) {
		String file = E.key;

		if (ResourceCache::has(file)) {
			Ref<Resource> res = ResourceCache::get_ref(file);
			emit_signal(SNAME("resource_removed"), res);
			res->set_path("");
		}
	}

	bool project_settings_modified = false;
	for (const String &file : files_to_delete) {
		// If the file we are deleting for e.g. the main scene, default environment,
		// or audio bus layout, we must clear its definition in Project Settings.
		if (file == ResourceUID::ensure_path(GLOBAL_GET("application/config/icon"))) {
			ProjectSettings::get_singleton()->set("application/config/icon", "");
			project_settings_modified = true;
		} else if (file == ResourceUID::ensure_path(GLOBAL_GET("application/run/main_scene"))) {
			ProjectSettings::get_singleton()->set("application/run/main_scene", "");
			project_settings_modified = true;
		} else if (file == ResourceUID::ensure_path(GLOBAL_GET("application/boot_splash/image"))) {
			ProjectSettings::get_singleton()->set("application/boot_splash/image", "");
			project_settings_modified = true;
		} else if (file == ResourceUID::ensure_path(GLOBAL_GET("rendering/environment/defaults/default_environment"))) {
			ProjectSettings::get_singleton()->set("rendering/environment/defaults/default_environment", "");
			project_settings_modified = true;
		} else if (file == ResourceUID::ensure_path(GLOBAL_GET("display/mouse_cursor/custom_image"))) {
			ProjectSettings::get_singleton()->set("display/mouse_cursor/custom_image", "");
			project_settings_modified = true;
		} else if (file == ResourceUID::ensure_path(GLOBAL_GET("gui/theme/custom"))) {
			ProjectSettings::get_singleton()->set("gui/theme/custom", "");
			project_settings_modified = true;
		} else if (file == ResourceUID::ensure_path(GLOBAL_GET("gui/theme/custom_font"))) {
			ProjectSettings::get_singleton()->set("gui/theme/custom_font", "");
			project_settings_modified = true;
		} else if (file == ResourceUID::ensure_path(GLOBAL_GET("audio/buses/default_bus_layout"))) {
			ProjectSettings::get_singleton()->set("audio/buses/default_bus_layout", "");
			project_settings_modified = true;
		}

		const String path = OS::get_singleton()->get_resource_dir() + file.replace_first("res://", "/");
		print_verbose("Moving to trash: " + path);
		Error err = OS::get_singleton()->move_to_trash(path);
		if (err != OK) {
			EditorNode::get_singleton()->add_io_error(TTR("Cannot remove:") + "\n" + file + "\n");
		} else {
			emit_signal(SNAME("file_removed"), file);
		}
	}
	if (project_settings_modified) {
		ProjectSettings::get_singleton()->save();
	}

	if (dirs_to_delete.size() == 0) {
		// If we only deleted files we should only need to tell the file system about the files we touched.
		for (int i = 0; i < files_to_delete.size(); ++i) {
			EditorFileSystem::get_singleton()->update_file(files_to_delete[i]);
		}
	} else {
		for (int i = 0; i < dirs_to_delete.size(); ++i) {
			String path = OS::get_singleton()->get_resource_dir() + dirs_to_delete[i].replace_first("res://", "/");
			print_verbose("Moving to trash: " + path);
			Error err = OS::get_singleton()->move_to_trash(path);
			if (err != OK) {
				EditorNode::get_singleton()->add_io_error(TTR("Cannot remove:") + "\n" + dirs_to_delete[i] + "\n");
			} else {
				emit_signal(SNAME("folder_removed"), dirs_to_delete[i]);
			}
		}

		EditorFileSystem::get_singleton()->scan_changes();
	}

	// If some files/dirs would be deleted, favorite dirs need to be updated
	Vector<String> previous_favorites = EditorSettings::get_singleton()->get_favorites();
	Vector<String> new_favorites;

	for (int i = 0; i < previous_favorites.size(); ++i) {
		if (previous_favorites[i].ends_with("/")) {
			if (!dirs_to_delete.has(previous_favorites[i])) {
				new_favorites.push_back(previous_favorites[i]);
			}
		} else {
			if (!files_to_delete.has(previous_favorites[i])) {
				new_favorites.push_back(previous_favorites[i]);
			}
		}
	}

	if (new_favorites.size() < previous_favorites.size()) {
		EditorSettings::get_singleton()->set_favorites(new_favorites);
	}
}

void DependencyRemoveDialog::_bind_methods() {
	ADD_SIGNAL(MethodInfo("resource_removed", PropertyInfo(Variant::OBJECT, "obj")));
	ADD_SIGNAL(MethodInfo("file_removed", PropertyInfo(Variant::STRING, "file")));
	ADD_SIGNAL(MethodInfo("folder_removed", PropertyInfo(Variant::STRING, "folder")));
}

DependencyRemoveDialog::DependencyRemoveDialog() {
	set_ok_button_text(TTR("Remove"));

	VBoxContainer *vb = memnew(VBoxContainer);
	vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(vb);

	text = memnew(Label);
	vb->add_child(text);

	Label *files_to_delete_label = memnew(Label);
	files_to_delete_label->set_theme_type_variation("HeaderSmall");
	files_to_delete_label->set_text(TTR("Files to be deleted:"));
	vb->add_child(files_to_delete_label);

	files_to_delete_list = memnew(ItemList);
	files_to_delete_list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	files_to_delete_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	files_to_delete_list->set_custom_minimum_size(Size2(0, 94) * EDSCALE);
	vb->add_child(files_to_delete_list);

	vb_owners = memnew(VBoxContainer);
	vb_owners->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	vb_owners->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	vb->add_child(vb_owners);

	Label *owners_label = memnew(Label);
	owners_label->set_theme_type_variation("HeaderSmall");
	owners_label->set_text(TTR("Dependencies of files to be deleted:"));
	vb_owners->add_child(owners_label);

	owners = memnew(Tree);
	owners->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	owners->set_hide_root(true);
	owners->set_custom_minimum_size(Size2(0, 94) * EDSCALE);
	vb_owners->add_child(owners);
	owners->set_v_size_flags(Control::SIZE_EXPAND_FILL);
}

//////////////

void DependencyErrorDialog::show(Mode p_mode, const String &p_for_file, const Vector<String> &report) {
	mode = p_mode;
	for_file = p_for_file;
	set_title(TTR("Error loading:") + " " + p_for_file.get_file());
	files->clear();

	TreeItem *root = files->create_item(nullptr);
	for (int i = 0; i < report.size(); i++) {
		String dep;
		String type = "Object";
		dep = report[i].get_slice("::", 0);
		if (report[i].get_slice_count("::") > 0) {
			type = report[i].get_slice("::", 1);
		}

		Ref<Texture2D> icon = EditorNode::get_singleton()->get_class_icon(type);

		TreeItem *ti = files->create_item(root);
		ti->set_text(0, dep);
		ti->set_icon(0, icon);
	}

	popup_centered();
}

void DependencyErrorDialog::ok_pressed() {
	switch (mode) {
		case MODE_SCENE:
			EditorNode::get_singleton()->load_scene(for_file, true);
			break;
		case MODE_RESOURCE:
			EditorNode::get_singleton()->load_resource(for_file, true);
			break;
	}
}

void DependencyErrorDialog::custom_action(const String &) {
	EditorNode::get_singleton()->fix_dependencies(for_file);
}

DependencyErrorDialog::DependencyErrorDialog() {
	VBoxContainer *vb = memnew(VBoxContainer);
	add_child(vb);

	files = memnew(Tree);
	files->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	files->set_hide_root(true);
	vb->add_margin_child(TTR("Load failed due to missing dependencies:"), files, true);
	files->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	set_min_size(Size2(500, 220) * EDSCALE);
	set_ok_button_text(TTR("Open Anyway"));
	set_cancel_button_text(TTR("Close"));

	text = memnew(Label);
	vb->add_child(text);
	text->set_text(TTR("Which action should be taken?"));

	mode = Mode::MODE_RESOURCE;

	fdep = add_button(TTR("Fix Dependencies"), true, "fixdeps");

	set_title(TTR("Errors loading!"));
}

//////////////////////////////////////////////////////////////////////

void OrphanResourcesDialog::ok_pressed() {
	paths.clear();

	_find_to_delete(files->get_root(), paths);
	if (paths.is_empty()) {
		return;
	}

	delete_confirm->set_text(vformat(TTR("Permanently delete %d item(s)? (No undo!)"), paths.size()));
	delete_confirm->popup_centered();
}

bool OrphanResourcesDialog::_fill_owners(EditorFileSystemDirectory *efsd, HashMap<String, int> &refs, TreeItem *p_parent) {
	if (!efsd) {
		return false;
	}

	bool has_children = false;

	for (int i = 0; i < efsd->get_subdir_count(); i++) {
		TreeItem *dir_item = nullptr;
		if (p_parent) {
			dir_item = files->create_item(p_parent);
			dir_item->set_text(0, efsd->get_subdir(i)->get_name());
			dir_item->set_icon(0, files->get_theme_icon(SNAME("folder"), SNAME("FileDialog")));
		}
		bool children = _fill_owners(efsd->get_subdir(i), refs, dir_item);

		if (p_parent) {
			if (!children) {
				memdelete(dir_item);
			} else {
				has_children = true;
			}
		}
	}

	for (int i = 0; i < efsd->get_file_count(); i++) {
		if (!p_parent) {
			Vector<String> deps = efsd->get_file_deps(i);
			for (int j = 0; j < deps.size(); j++) {
				if (!refs.has(deps[j])) {
					refs[deps[j]] = 1;
				}
			}
		} else {
			String path = efsd->get_file_path(i);
			if (!refs.has(path)) {
				TreeItem *ti = files->create_item(p_parent);
				ti->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
				ti->set_text(0, efsd->get_file(i));
				ti->set_editable(0, true);

				String type = efsd->get_file_type(i);

				Ref<Texture2D> icon = EditorNode::get_singleton()->get_class_icon(type);
				ti->set_icon(0, icon);
				int ds = efsd->get_file_deps(i).size();
				ti->set_text(1, itos(ds));
				if (ds) {
					ti->add_button(1, files->get_editor_theme_icon(SNAME("GuiVisibilityVisible")), -1, false, TTR("Show Dependencies"));
				}
				ti->set_metadata(0, path);
				has_children = true;
			}
		}
	}

	return has_children;
}

void OrphanResourcesDialog::refresh() {
	HashMap<String, int> refs;
	_fill_owners(EditorFileSystem::get_singleton()->get_filesystem(), refs, nullptr);
	files->clear();
	TreeItem *root = files->create_item();
	_fill_owners(EditorFileSystem::get_singleton()->get_filesystem(), refs, root);
}

void OrphanResourcesDialog::show() {
	refresh();
	popup_centered_ratio(0.4);
}

void OrphanResourcesDialog::_find_to_delete(TreeItem *p_item, List<String> &r_paths) {
	while (p_item) {
		if (p_item->get_cell_mode(0) == TreeItem::CELL_MODE_CHECK && p_item->is_checked(0)) {
			r_paths.push_back(p_item->get_metadata(0));
		}

		if (p_item->get_first_child()) {
			_find_to_delete(p_item->get_first_child(), r_paths);
		}

		p_item = p_item->get_next();
	}
}

void OrphanResourcesDialog::_delete_confirm() {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	for (const String &E : paths) {
		da->remove(E);
		EditorFileSystem::get_singleton()->update_file(E);
	}
	refresh();
}

void OrphanResourcesDialog::_button_pressed(Object *p_item, int p_column, int p_id, MouseButton p_button) {
	if (p_button != MouseButton::LEFT) {
		return;
	}
	TreeItem *ti = Object::cast_to<TreeItem>(p_item);

	String path = ti->get_metadata(0);
	dep_edit->edit(path);
}

OrphanResourcesDialog::OrphanResourcesDialog() {
	set_title(TTR("Orphan Resource Explorer"));
	delete_confirm = memnew(ConfirmationDialog);
	set_ok_button_text(TTR("Delete"));
	add_child(delete_confirm);
	dep_edit = memnew(DependencyEditor);
	add_child(dep_edit);
	delete_confirm->connect(SceneStringName(confirmed), callable_mp(this, &OrphanResourcesDialog::_delete_confirm));
	set_hide_on_ok(false);

	VBoxContainer *vbc = memnew(VBoxContainer);
	add_child(vbc);

	files = memnew(Tree);
	files->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	files->set_columns(2);
	files->set_column_titles_visible(true);
	files->set_column_custom_minimum_width(1, 100 * EDSCALE);
	files->set_column_expand(0, true);
	files->set_column_clip_content(0, true);
	files->set_column_expand(1, false);
	files->set_column_clip_content(1, true);
	files->set_column_title(0, TTR("Resource"));
	files->set_column_title(1, TTR("Owns"));
	files->set_hide_root(true);
	vbc->add_margin_child(TTR("Resources Without Explicit Ownership:"), files, true);
	files->connect("button_clicked", callable_mp(this, &OrphanResourcesDialog::_button_pressed));
}
