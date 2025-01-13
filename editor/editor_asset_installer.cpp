/**************************************************************************/
/*  editor_asset_installer.cpp                                            */
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

#include "editor_asset_installer.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/zip_io.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/gui/editor_toaster.h"
#include "editor/progress_dialog.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/check_box.h"
#include "scene/gui/label.h"
#include "scene/gui/link_button.h"
#include "scene/gui/separator.h"
#include "scene/gui/split_container.h"

void EditorAssetInstaller::_item_checked_cbk() {
	if (updating_source || !source_tree->get_edited()) {
		return;
	}

	updating_source = true;
	TreeItem *item = source_tree->get_edited();
	item->propagate_check(0);
	_fix_conflicted_indeterminate_state(source_tree->get_root(), 0);
	_update_confirm_button();
	_rebuild_destination_tree();
	updating_source = false;
}

// Determine parent state based on non-conflict children, to avoid indeterminate state, and allow toggle dir with conflicts.
bool EditorAssetInstaller::_fix_conflicted_indeterminate_state(TreeItem *p_item, int p_column) {
	if (p_item->get_child_count() == 0) {
		return false;
	}
	bool all_non_conflict_checked = true;
	bool all_non_conflict_unchecked = true;
	bool has_conflict_child = false;
	bool has_indeterminate_child = false;
	TreeItem *child_item = p_item->get_first_child();
	while (child_item) {
		has_conflict_child |= _fix_conflicted_indeterminate_state(child_item, p_column);
		Dictionary child_meta = child_item->get_metadata(p_column);
		bool child_conflict = child_meta.get("is_conflict", false);
		if (child_conflict) {
			child_item->set_checked(p_column, false);
			has_conflict_child = true;
		} else {
			bool child_checked = child_item->is_checked(p_column);
			bool child_indeterminate = child_item->is_indeterminate(p_column);
			all_non_conflict_checked &= (child_checked || child_indeterminate);
			all_non_conflict_unchecked &= !child_checked;
			has_indeterminate_child |= child_indeterminate;
		}
		child_item = child_item->get_next();
	}
	if (has_indeterminate_child) {
		p_item->set_indeterminate(p_column, true);
	} else if (all_non_conflict_checked) {
		p_item->set_checked(p_column, true);
	} else if (all_non_conflict_unchecked) {
		p_item->set_checked(p_column, false);
	}
	if (has_conflict_child) {
		p_item->set_custom_color(p_column, get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
	} else {
		p_item->clear_custom_color(p_column);
	}
	return has_conflict_child;
}

bool EditorAssetInstaller::_is_item_checked(const String &p_source_path) const {
	return file_item_map.has(p_source_path) && (file_item_map[p_source_path]->is_checked(0) || file_item_map[p_source_path]->is_indeterminate(0));
}

void EditorAssetInstaller::open_asset(const String &p_path, bool p_autoskip_toplevel) {
	package_path = p_path;
	asset_files.clear();

	Ref<FileAccess> io_fa;
	zlib_filefunc_def io = zipio_create_io(&io_fa);

	unzFile pkg = unzOpen2(p_path.utf8().get_data(), &io);
	if (!pkg) {
		EditorToaster::get_singleton()->popup_str(vformat(TTR("Error opening asset file for \"%s\" (not in ZIP format)."), asset_name), EditorToaster::SEVERITY_ERROR);
		return;
	}

	int ret = unzGoToFirstFile(pkg);

	while (ret == UNZ_OK) {
		//get filename
		unz_file_info info;
		char fname[16384];
		unzGetCurrentFileInfo(pkg, &info, fname, 16384, nullptr, 0, nullptr, 0);

		String source_name = String::utf8(fname);

		// Create intermediate directories if they aren't reported by unzip.
		// We are only interested in subfolders, so skip the root slash.
		int separator = source_name.find_char('/', 1);
		while (separator != -1) {
			String dir_name = source_name.substr(0, separator + 1);
			if (!dir_name.is_empty() && !asset_files.has(dir_name)) {
				asset_files.insert(dir_name);
			}

			separator = source_name.find_char('/', separator + 1);
		}

		if (!source_name.is_empty() && !asset_files.has(source_name)) {
			asset_files.insert(source_name);
		}

		ret = unzGoToNextFile(pkg);
	}

	unzClose(pkg);

	asset_title_label->set_text(asset_name);

	_check_has_toplevel();
	// Default to false, unless forced.
	skip_toplevel = p_autoskip_toplevel;
	skip_toplevel_check->set_block_signals(true);
	skip_toplevel_check->set_pressed(!skip_toplevel_check->is_disabled() && skip_toplevel);
	skip_toplevel_check->set_block_signals(false);

	_update_file_mappings();
	_rebuild_source_tree();
	_rebuild_destination_tree();

	popup_centered_clamped(Size2(620, 640) * EDSCALE);
}

void EditorAssetInstaller::_update_file_mappings() {
	mapped_files.clear();

	bool first = true;
	for (const String &E : asset_files) {
		if (first) {
			first = false;

			if (!toplevel_prefix.is_empty() && skip_toplevel) {
				continue;
			}
		}

		String path = E; // We're going to mutate it.
		if (!toplevel_prefix.is_empty() && skip_toplevel) {
			path = path.trim_prefix(toplevel_prefix);
		}

		mapped_files[E] = path;
	}
}

void EditorAssetInstaller::_rebuild_source_tree() {
	updating_source = true;
	source_tree->clear();

	TreeItem *root = source_tree->create_item();
	root->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
	root->set_checked(0, true);
	root->set_icon(0, get_theme_icon(SNAME("folder"), SNAME("FileDialog")));
	root->set_text(0, "/");
	root->set_editable(0, true);

	file_item_map.clear();
	HashMap<String, TreeItem *> directory_item_map;
	int num_file_conflicts = 0;
	first_file_conflict = nullptr;

	for (const String &E : asset_files) {
		String path = E; // We're going to mutate it.

		bool is_directory = false;
		if (path.ends_with("/")) {
			path = path.trim_suffix("/");
			is_directory = true;
		}

		TreeItem *parent_item;

		int separator = path.rfind_char('/');
		if (separator == -1) {
			parent_item = root;
		} else {
			String parent_path = path.substr(0, separator);
			HashMap<String, TreeItem *>::Iterator I = directory_item_map.find(parent_path);
			ERR_CONTINUE(!I);
			parent_item = I->value;
		}

		TreeItem *ti;
		if (is_directory) {
			ti = _create_dir_item(source_tree, parent_item, path, directory_item_map);
		} else {
			ti = _create_file_item(source_tree, parent_item, path, &num_file_conflicts);
		}
		file_item_map[E] = ti;
	}

	_update_conflict_status(num_file_conflicts);
	_update_confirm_button();

	updating_source = false;
}

void EditorAssetInstaller::_update_source_tree() {
	int num_file_conflicts = 0;
	first_file_conflict = nullptr;

	for (const KeyValue<String, TreeItem *> &E : file_item_map) {
		TreeItem *ti = E.value;
		Dictionary item_meta = ti->get_metadata(0);
		if ((bool)item_meta.get("is_dir", false)) {
			continue;
		}

		String asset_path = item_meta.get("asset_path", "");
		ERR_CONTINUE(asset_path.is_empty());

		bool target_exists = _update_source_item_status(ti, asset_path);
		if (target_exists) {
			if (first_file_conflict == nullptr) {
				first_file_conflict = ti;
			}
			num_file_conflicts += 1;
		}

		item_meta["is_conflict"] = target_exists;
		ti->set_metadata(0, item_meta);
	}

	_update_conflict_status(num_file_conflicts);
	_update_confirm_button();
}

bool EditorAssetInstaller::_update_source_item_status(TreeItem *p_item, const String &p_path) {
	ERR_FAIL_COND_V(!mapped_files.has(p_path), false);
	String target_path = target_dir_path.path_join(mapped_files[p_path]);

	bool target_exists = FileAccess::exists(target_path);
	if (target_exists) {
		p_item->set_custom_color(0, get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
		p_item->set_tooltip_text(0, vformat(TTR("%s (already exists)"), target_path));
		p_item->set_checked(0, false);
	} else {
		p_item->clear_custom_color(0);
		p_item->set_tooltip_text(0, target_path);
		p_item->set_checked(0, true);
	}

	p_item->propagate_check(0);
	_fix_conflicted_indeterminate_state(p_item->get_tree()->get_root(), 0);
	return target_exists;
}

void EditorAssetInstaller::_rebuild_destination_tree() {
	destination_tree->clear();

	TreeItem *root = destination_tree->create_item();
	root->set_icon(0, get_theme_icon(SNAME("folder"), SNAME("FileDialog")));
	root->set_text(0, target_dir_path + (target_dir_path == "res://" ? "" : "/"));

	HashMap<String, TreeItem *> directory_item_map;

	for (const KeyValue<String, String> &E : mapped_files) {
		if (!_is_item_checked(E.key)) {
			continue;
		}

		String path = E.value; // We're going to mutate it.

		bool is_directory = false;
		if (path.ends_with("/")) {
			path = path.trim_suffix("/");
			is_directory = true;
		}

		TreeItem *parent_item;

		int separator = path.rfind_char('/');
		if (separator == -1) {
			parent_item = root;
		} else {
			String parent_path = path.substr(0, separator);
			HashMap<String, TreeItem *>::Iterator I = directory_item_map.find(parent_path);
			ERR_CONTINUE(!I);
			parent_item = I->value;
		}

		if (is_directory) {
			_create_dir_item(destination_tree, parent_item, path, directory_item_map);
		} else {
			int num_file_conflicts = 0; // Don't need it, but need to pass something.
			_create_file_item(destination_tree, parent_item, path, &num_file_conflicts);
		}
	}
}

TreeItem *EditorAssetInstaller::_create_dir_item(Tree *p_tree, TreeItem *p_parent, const String &p_path, HashMap<String, TreeItem *> &p_item_map) {
	TreeItem *ti = p_tree->create_item(p_parent);

	if (p_tree == source_tree) {
		ti->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
		ti->set_editable(0, true);
		ti->set_checked(0, true);
		ti->propagate_check(0);
		_fix_conflicted_indeterminate_state(ti->get_tree()->get_root(), 0);

		Dictionary meta;
		meta["asset_path"] = p_path + "/";
		meta["is_dir"] = true;
		meta["is_conflict"] = false;
		ti->set_metadata(0, meta);
	}

	ti->set_text(0, p_path.get_file() + "/");
	ti->set_icon(0, get_theme_icon(SNAME("folder"), SNAME("FileDialog")));

	p_item_map[p_path] = ti;
	return ti;
}

TreeItem *EditorAssetInstaller::_create_file_item(Tree *p_tree, TreeItem *p_parent, const String &p_path, int *r_conflicts) {
	TreeItem *ti = p_tree->create_item(p_parent);

	if (p_tree == source_tree) {
		ti->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
		ti->set_editable(0, true);

		bool target_exists = _update_source_item_status(ti, p_path);
		if (target_exists) {
			if (first_file_conflict == nullptr) {
				first_file_conflict = ti;
			}
			*r_conflicts += 1;
		}

		Dictionary meta;
		meta["asset_path"] = p_path;
		meta["is_dir"] = false;
		meta["is_conflict"] = target_exists;
		ti->set_metadata(0, meta);
	}

	String file = p_path.get_file();
	String extension = file.get_extension().to_lower();
	if (extension_icon_map.has(extension)) {
		ti->set_icon(0, extension_icon_map[extension]);
	} else {
		ti->set_icon(0, generic_extension_icon);
	}
	ti->set_text(0, file);

	return ti;
}

void EditorAssetInstaller::_update_conflict_status(int p_conflicts) {
	if (p_conflicts >= 1) {
		asset_conflicts_link->set_text(vformat(TTRN("%d file conflicts with your project and won't be installed", "%d files conflict with your project and won't be installed", p_conflicts), p_conflicts));
		asset_conflicts_link->show();
		asset_conflicts_label->hide();
	} else {
		asset_conflicts_link->hide();
		asset_conflicts_label->show();
	}
}

void EditorAssetInstaller::_update_confirm_button() {
	TreeItem *root = source_tree->get_root();
	get_ok_button()->set_disabled(!root || (!root->is_checked(0) && !root->is_indeterminate(0)));
}

void EditorAssetInstaller::_toggle_source_tree(bool p_visible, bool p_scroll_to_error) {
	source_tree_vb->set_visible(p_visible);
	show_source_files_button->set_pressed_no_signal(p_visible); // To keep in sync if triggered by something else.

	if (p_visible) {
		show_source_files_button->set_button_icon(get_editor_theme_icon(SNAME("Back")));
	} else {
		show_source_files_button->set_button_icon(get_editor_theme_icon(SNAME("Forward")));
	}

	if (p_visible && p_scroll_to_error && first_file_conflict) {
		source_tree->scroll_to_item(first_file_conflict, true);
	}
}

void EditorAssetInstaller::_check_has_toplevel() {
	// Check if the file structure has a distinct top-level directory. This is typical
	// for archives generated by GitHub, etc, but not for manually created ZIPs.

	toplevel_prefix = "";
	skip_toplevel_check->set_pressed(false);
	skip_toplevel_check->set_disabled(true);
	skip_toplevel_check->set_tooltip_text(TTR("This asset doesn't have a root directory, so it can't be ignored."));

	if (asset_files.is_empty()) {
		return;
	}

	String first_asset;
	for (const String &E : asset_files) {
		if (first_asset.is_empty()) { // Checking the first file/directory.
			if (!E.ends_with("/")) {
				return; // No directories in this asset.
			}

			// We will match everything else against this directory.
			first_asset = E;
			continue;
		}

		if (!E.begins_with(first_asset)) {
			return; // Found a file or a directory that doesn't share the same base path.
		}
	}

	toplevel_prefix = first_asset;
	skip_toplevel_check->set_disabled(false);
	skip_toplevel_check->set_tooltip_text(TTR("Ignore the root directory when extracting files."));
}

void EditorAssetInstaller::_set_skip_toplevel(bool p_checked) {
	if (skip_toplevel == p_checked) {
		return;
	}

	skip_toplevel = p_checked;
	_update_file_mappings();
	_update_source_tree();
	_rebuild_destination_tree();
}

void EditorAssetInstaller::_open_target_dir_dialog() {
	if (!target_dir_dialog) {
		target_dir_dialog = memnew(EditorFileDialog);
		target_dir_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_DIR);
		target_dir_dialog->set_title(TTR("Select Install Folder"));
		target_dir_dialog->set_current_dir(target_dir_path);
		target_dir_dialog->connect("dir_selected", callable_mp(this, &EditorAssetInstaller::_target_dir_selected));
		add_child(target_dir_dialog);
	}

	target_dir_dialog->popup_file_dialog();
}

void EditorAssetInstaller::_target_dir_selected(const String &p_target_path) {
	if (target_dir_path == p_target_path) {
		return;
	}

	target_dir_path = p_target_path;
	_update_file_mappings();
	_update_source_tree();
	_rebuild_destination_tree();
}

void EditorAssetInstaller::ok_pressed() {
	_install_asset();
}

void EditorAssetInstaller::_install_asset() {
	Ref<FileAccess> io_fa;
	zlib_filefunc_def io = zipio_create_io(&io_fa);

	unzFile pkg = unzOpen2(package_path.utf8().get_data(), &io);
	if (!pkg) {
		EditorToaster::get_singleton()->popup_str(vformat(TTR("Error opening asset file for \"%s\" (not in ZIP format)."), asset_name), EditorToaster::SEVERITY_ERROR);
		return;
	}

	Vector<String> failed_files;
	int ret = unzGoToFirstFile(pkg);

	ProgressDialog::get_singleton()->add_task("uncompress", TTR("Uncompressing Assets"), file_item_map.size());

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	for (int idx = 0; ret == UNZ_OK; ret = unzGoToNextFile(pkg), idx++) {
		unz_file_info info;
		char fname[16384];
		ret = unzGetCurrentFileInfo(pkg, &info, fname, 16384, nullptr, 0, nullptr, 0);
		if (ret != UNZ_OK) {
			break;
		}

		String source_name = String::utf8(fname);
		if (!_is_item_checked(source_name)) {
			continue;
		}

		HashMap<String, String>::Iterator E = mapped_files.find(source_name);
		if (!E) {
			continue; // No remapped path means we don't want it; most likely the root.
		}

		String target_path = target_dir_path.path_join(E->value);

		Dictionary asset_meta = file_item_map[source_name]->get_metadata(0);
		bool is_dir = asset_meta.get("is_dir", false);
		if (is_dir) {
			if (target_path.ends_with("/")) {
				target_path = target_path.substr(0, target_path.length() - 1);
			}

			da->make_dir_recursive(target_path);
		} else {
			Vector<uint8_t> uncomp_data;
			uncomp_data.resize(info.uncompressed_size);

			unzOpenCurrentFile(pkg);
			unzReadCurrentFile(pkg, uncomp_data.ptrw(), uncomp_data.size());
			unzCloseCurrentFile(pkg);

			// Ensure that the target folder exists.
			da->make_dir_recursive(target_path.get_base_dir());

			Ref<FileAccess> f = FileAccess::open(target_path, FileAccess::WRITE);
			if (f.is_valid()) {
				f->store_buffer(uncomp_data.ptr(), uncomp_data.size());
			} else {
				failed_files.push_back(target_path);
			}

			ProgressDialog::get_singleton()->task_step("uncompress", target_path, idx);
		}
	}

	ProgressDialog::get_singleton()->end_task("uncompress");
	unzClose(pkg);

	if (failed_files.size()) {
		String msg = vformat(TTR("The following files failed extraction from asset \"%s\":"), asset_name) + "\n\n";
		for (int i = 0; i < failed_files.size(); i++) {
			if (i > 10) {
				msg += "\n" + vformat(TTR("(and %s more files)"), itos(failed_files.size() - i));
				break;
			}
			msg += "\n" + failed_files[i];
		}
		if (EditorNode::get_singleton() != nullptr) {
			EditorNode::get_singleton()->show_warning(msg);
		}
	} else {
		if (EditorNode::get_singleton() != nullptr) {
			EditorNode::get_singleton()->show_warning(vformat(TTR("Asset \"%s\" installed successfully!"), asset_name), TTR("Success!"));
		}
	}

	EditorFileSystem::get_singleton()->scan_changes();
}

void EditorAssetInstaller::set_asset_name(const String &p_asset_name) {
	asset_name = p_asset_name;
}

String EditorAssetInstaller::get_asset_name() const {
	return asset_name;
}

void EditorAssetInstaller::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			if (show_source_files_button->is_pressed()) {
				show_source_files_button->set_button_icon(get_editor_theme_icon(SNAME("Back")));
			} else {
				show_source_files_button->set_button_icon(get_editor_theme_icon(SNAME("Forward")));
			}
			asset_conflicts_link->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("error_color"), EditorStringName(Editor)));

			generic_extension_icon = get_editor_theme_icon(SNAME("Object"));

			extension_icon_map.clear();
			{
				extension_icon_map["bmp"] = get_editor_theme_icon(SNAME("ImageTexture"));
				extension_icon_map["dds"] = get_editor_theme_icon(SNAME("ImageTexture"));
				extension_icon_map["exr"] = get_editor_theme_icon(SNAME("ImageTexture"));
				extension_icon_map["hdr"] = get_editor_theme_icon(SNAME("ImageTexture"));
				extension_icon_map["jpg"] = get_editor_theme_icon(SNAME("ImageTexture"));
				extension_icon_map["jpeg"] = get_editor_theme_icon(SNAME("ImageTexture"));
				extension_icon_map["png"] = get_editor_theme_icon(SNAME("ImageTexture"));
				extension_icon_map["svg"] = get_editor_theme_icon(SNAME("ImageTexture"));
				extension_icon_map["tga"] = get_editor_theme_icon(SNAME("ImageTexture"));
				extension_icon_map["webp"] = get_editor_theme_icon(SNAME("ImageTexture"));

				extension_icon_map["wav"] = get_editor_theme_icon(SNAME("AudioStreamWAV"));
				extension_icon_map["ogg"] = get_editor_theme_icon(SNAME("AudioStreamOggVorbis"));
				extension_icon_map["mp3"] = get_editor_theme_icon(SNAME("AudioStreamMP3"));

				extension_icon_map["scn"] = get_editor_theme_icon(SNAME("PackedScene"));
				extension_icon_map["tscn"] = get_editor_theme_icon(SNAME("PackedScene"));
				extension_icon_map["escn"] = get_editor_theme_icon(SNAME("PackedScene"));
				extension_icon_map["dae"] = get_editor_theme_icon(SNAME("PackedScene"));
				extension_icon_map["gltf"] = get_editor_theme_icon(SNAME("PackedScene"));
				extension_icon_map["glb"] = get_editor_theme_icon(SNAME("PackedScene"));

				extension_icon_map["gdshader"] = get_editor_theme_icon(SNAME("Shader"));
				extension_icon_map["gdshaderinc"] = get_editor_theme_icon(SNAME("TextFile"));
				extension_icon_map["gd"] = get_editor_theme_icon(SNAME("GDScript"));
				if (ClassDB::class_exists("CSharpScript")) {
					extension_icon_map["cs"] = get_editor_theme_icon(SNAME("CSharpScript"));
				} else {
					// Mark C# support as unavailable.
					extension_icon_map["cs"] = get_editor_theme_icon(SNAME("ImportFail"));
				}

				extension_icon_map["res"] = get_editor_theme_icon(SNAME("Resource"));
				extension_icon_map["tres"] = get_editor_theme_icon(SNAME("Resource"));
				extension_icon_map["atlastex"] = get_editor_theme_icon(SNAME("AtlasTexture"));
				// By default, OBJ files are imported as Mesh resources rather than PackedScenes.
				extension_icon_map["obj"] = get_editor_theme_icon(SNAME("MeshItem"));

				extension_icon_map["txt"] = get_editor_theme_icon(SNAME("TextFile"));
				extension_icon_map["md"] = get_editor_theme_icon(SNAME("TextFile"));
				extension_icon_map["rst"] = get_editor_theme_icon(SNAME("TextFile"));
				extension_icon_map["json"] = get_editor_theme_icon(SNAME("TextFile"));
				extension_icon_map["yml"] = get_editor_theme_icon(SNAME("TextFile"));
				extension_icon_map["yaml"] = get_editor_theme_icon(SNAME("TextFile"));
				extension_icon_map["toml"] = get_editor_theme_icon(SNAME("TextFile"));
				extension_icon_map["cfg"] = get_editor_theme_icon(SNAME("TextFile"));
				extension_icon_map["ini"] = get_editor_theme_icon(SNAME("TextFile"));
			}
		} break;
	}
}

EditorAssetInstaller::EditorAssetInstaller() {
	VBoxContainer *vb = memnew(VBoxContainer);
	add_child(vb);

	// Status bar.

	HBoxContainer *asset_status = memnew(HBoxContainer);
	vb->add_child(asset_status);

	Label *asset_label = memnew(Label);
	asset_label->set_text(TTR("Asset:"));
	asset_label->set_theme_type_variation("HeaderSmall");
	asset_status->add_child(asset_label);

	asset_title_label = memnew(Label);
	asset_status->add_child(asset_title_label);

	// File remapping controls.

	HBoxContainer *remapping_tools = memnew(HBoxContainer);
	vb->add_child(remapping_tools);

	show_source_files_button = memnew(Button);
	show_source_files_button->set_toggle_mode(true);
	show_source_files_button->set_tooltip_text(TTR("Open the list of the asset contents and select which files to install."));
	remapping_tools->add_child(show_source_files_button);
	show_source_files_button->connect(SceneStringName(toggled), callable_mp(this, &EditorAssetInstaller::_toggle_source_tree).bind(false));

	Button *target_dir_button = memnew(Button);
	target_dir_button->set_text(TTR("Change Install Folder"));
	target_dir_button->set_tooltip_text(TTR("Change the folder where the contents of the asset are going to be installed."));
	remapping_tools->add_child(target_dir_button);
	target_dir_button->connect(SceneStringName(pressed), callable_mp(this, &EditorAssetInstaller::_open_target_dir_dialog));

	remapping_tools->add_child(memnew(VSeparator));

	skip_toplevel_check = memnew(CheckBox);
	skip_toplevel_check->set_text(TTR("Ignore asset root"));
	skip_toplevel_check->set_tooltip_text(TTR("Ignore the root directory when extracting files."));
	skip_toplevel_check->connect(SceneStringName(toggled), callable_mp(this, &EditorAssetInstaller::_set_skip_toplevel));
	remapping_tools->add_child(skip_toplevel_check);

	remapping_tools->add_spacer();

	asset_conflicts_label = memnew(Label);
	asset_conflicts_label->set_theme_type_variation("HeaderSmall");
	asset_conflicts_label->set_text(TTR("No files conflict with your project"));
	remapping_tools->add_child(asset_conflicts_label);
	asset_conflicts_link = memnew(LinkButton);
	asset_conflicts_link->set_theme_type_variation("HeaderSmallLink");
	asset_conflicts_link->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	asset_conflicts_link->set_tooltip_text(TTR("Show contents of the asset and conflicting files."));
	asset_conflicts_link->set_visible(false);
	remapping_tools->add_child(asset_conflicts_link);
	asset_conflicts_link->connect(SceneStringName(pressed), callable_mp(this, &EditorAssetInstaller::_toggle_source_tree).bind(true, true));

	// File hierarchy trees.

	HSplitContainer *tree_split = memnew(HSplitContainer);
	tree_split->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	vb->add_child(tree_split);

	source_tree_vb = memnew(VBoxContainer);
	source_tree_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	source_tree_vb->set_visible(show_source_files_button->is_pressed());
	tree_split->add_child(source_tree_vb);

	Label *source_tree_label = memnew(Label);
	source_tree_label->set_text(TTR("Contents of the asset:"));
	source_tree_label->set_theme_type_variation("HeaderSmall");
	source_tree_vb->add_child(source_tree_label);

	source_tree = memnew(Tree);
	source_tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	source_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	source_tree->connect("item_edited", callable_mp(this, &EditorAssetInstaller::_item_checked_cbk));
	source_tree->set_theme_type_variation("TreeSecondary");
	source_tree_vb->add_child(source_tree);

	VBoxContainer *destination_tree_vb = memnew(VBoxContainer);
	destination_tree_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tree_split->add_child(destination_tree_vb);

	Label *destination_tree_label = memnew(Label);
	destination_tree_label->set_text(TTR("Installation preview:"));
	destination_tree_label->set_theme_type_variation("HeaderSmall");
	destination_tree_vb->add_child(destination_tree_label);

	destination_tree = memnew(Tree);
	destination_tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	destination_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	destination_tree->connect("item_edited", callable_mp(this, &EditorAssetInstaller::_item_checked_cbk));
	destination_tree_vb->add_child(destination_tree);

	// Dialog configuration.

	set_title(TTR("Configure Asset Before Installing"));
	set_ok_button_text(TTR("Install"));
	set_hide_on_ok(true);
}
