/**************************************************************************/
/*  surface_upgrade_tool.cpp                                              */
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

#include "surface_upgrade_tool.h"

#include "editor/editor_file_system.h"
#include "editor/editor_log.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/gui/editor_toaster.h"
#include "editor/themes/editor_scale.h"
#include "servers/rendering_server.h"

SurfaceUpgradeTool *SurfaceUpgradeTool::singleton = nullptr;

void SurfaceUpgradeTool::_add_files(EditorFileSystemDirectory *p_dir, Vector<String> &r_reimport_paths, Vector<String> &r_resave_paths) {
	for (int i = 0; i < p_dir->get_subdir_count(); i++) {
		_add_files(p_dir->get_subdir(i), r_reimport_paths, r_resave_paths);
	}

	for (int i = 0; i < p_dir->get_file_count(); i++) {
		if (p_dir->get_file_type(i) == "Mesh" ||
				p_dir->get_file_type(i) == "ArrayMesh" ||
				p_dir->get_file_type(i) == "PackedScene") {
			if (FileAccess::exists(p_dir->get_file_path(i) + ".import")) {
				r_reimport_paths.append(p_dir->get_file_path(i) + ".import");
			} else {
				r_resave_paths.append(p_dir->get_file_path(i));
			}
		}
	}
}

void SurfaceUpgradeTool::_try_show_popup() {
	if (singleton->show_requested || singleton->updating) {
		return;
	}

	singleton->show_requested = true;

	if (!EditorNode::get_singleton()->is_editor_ready()) {
		// EditorNode may not be ready yet. It will call this tool when it is.
		return;
	}

	if (EditorFileSystem::get_singleton()->is_importing()) {
		EditorFileSystem::get_singleton()->connect("resources_reimported", callable_mp(singleton, &SurfaceUpgradeTool::_show_popup), CONNECT_ONE_SHOT);
	} else {
		singleton->_show_popup();
	}
	RS::get_singleton()->set_warn_on_surface_upgrade(false);
}

void SurfaceUpgradeTool::_show_popup() {
	MutexLock lock(mutex);
	if (!show_requested) {
		return; // We only show the dialog if it was previously requested.
	}
	show_requested = false;

	// These messages are supposed to be translated as they are critical to users migrating their projects.

	const String confirmation_message = TTR("This project uses meshes with an outdated mesh format from previous Godot versions. The engine needs to update the format in order to use those meshes. Please use the 'Upgrade Mesh Surfaces' tool from the 'Project > Tools' menu. You can ignore this message and keep using outdated meshes, but keep in mind that this leads to increased load times every time you load the project.");
	EditorNode::get_log()->add_message(confirmation_message, EditorLog::MSG_TYPE_WARNING);

	const String toast_message = TTR("This project uses meshes with an outdated mesh format. Check the output log.");
	EditorToaster::get_singleton()->popup_str(toast_message, EditorToaster::SEVERITY_WARNING);
}

void SurfaceUpgradeTool::prepare_upgrade() {
	EditorSettings::get_singleton()->set_project_metadata("surface_upgrade_tool", "run_on_restart", true);

	Vector<String> reimport_paths;
	Vector<String> resave_paths;
	_add_files(EditorFileSystem::get_singleton()->get_filesystem(), reimport_paths, resave_paths);

	EditorSettings::get_singleton()->set_project_metadata("surface_upgrade_tool", "reimport_paths", reimport_paths);
	EditorSettings::get_singleton()->set_project_metadata("surface_upgrade_tool", "resave_paths", resave_paths);

	// Delay to avoid deadlocks, since this dialog can be triggered by loading a scene.
	callable_mp(EditorNode::get_singleton(), &EditorNode::restart_editor).call_deferred(false);
}

// Ensure that the warnings and popups are skipped.
void SurfaceUpgradeTool::begin_upgrade() {
	updating = true;

	EditorSettings::get_singleton()->set_project_metadata("surface_upgrade_tool", "run_on_restart", false);
	RS::get_singleton()->set_surface_upgrade_callback(nullptr);
	RS::get_singleton()->set_warn_on_surface_upgrade(false);
}

void SurfaceUpgradeTool::finish_upgrade() {
	EditorNode::get_singleton()->trigger_menu_option(EditorNode::FILE_CLOSE_ALL, true);

	// Update all meshes here.
	Vector<String> resave_paths = EditorSettings::get_singleton()->get_project_metadata("surface_upgrade_tool", "resave_paths", Vector<String>());
	Vector<String> reimport_paths = EditorSettings::get_singleton()->get_project_metadata("surface_upgrade_tool", "reimport_paths", Vector<String>());
	EditorProgress ep("surface_upgrade_resave", TTR("Upgrading All Meshes in Project"), resave_paths.size() + reimport_paths.size());

	int step = 0;
	for (const String &file_path : resave_paths) {
		Ref<Resource> res = ResourceLoader::load(file_path);
		ep.step(TTR("Attempting to re-save ") + file_path, step++);
		if (res.is_valid()) {
			// Ignore things that fail to load.
			ResourceSaver::save(res);
		}
	}
	EditorSettings::get_singleton()->set_project_metadata("surface_upgrade_tool", "resave_paths", Vector<String>());

	// Remove the imported scenes/meshes from .import so they will be reimported automatically after this.
	for (const String &file_path : reimport_paths) {
		Ref<ConfigFile> config;
		config.instantiate();
		Error err = config->load(file_path);
		if (err != OK) {
			ERR_PRINT("Could not open " + file_path + " for upgrade.");
			continue;
		}

		String remap_path = config->get_value("remap", "path", "");
		if (remap_path.is_empty()) {
			continue;
		}

		ep.step(TTR("Attempting to remove ") + remap_path, step++);

		String path = OS::get_singleton()->get_resource_dir() + remap_path.replace_first("res://", "/");
		print_verbose("Moving to trash: " + path);
		err = OS::get_singleton()->move_to_trash(path);
		if (err != OK) {
			EditorNode::get_singleton()->add_io_error(TTR("Cannot remove:") + "\n" + remap_path + "\n");
		}
	}
	EditorSettings::get_singleton()->set_project_metadata("surface_upgrade_tool", "reimport_paths", Vector<String>());

	emit_signal(SNAME("upgrade_finished"));
}

void SurfaceUpgradeTool::_bind_methods() {
	ADD_SIGNAL(MethodInfo("upgrade_finished"));
}

SurfaceUpgradeTool::SurfaceUpgradeTool() {
	singleton = this;
	RS::get_singleton()->set_surface_upgrade_callback(_try_show_popup);
}

SurfaceUpgradeTool::~SurfaceUpgradeTool() {
	singleton = nullptr;
}

void SurfaceUpgradeDialog::popup_on_demand() {
	const String confirmation_message = TTR("The mesh format has changed in Godot 4.2, which affects both imported meshes and meshes authored inside of Godot. The engine needs to update the format in order to use those meshes.\n\nIf your project predates Godot 4.2 and contains meshes, we recommend you run this one time conversion tool. This update will restart the editor and may take several minutes. Upgrading will make the meshes incompatible with previous versions of Godot.\n\nYou can still use your existing meshes as is. The engine will update each mesh in memory, but the update will not be saved. Choosing this option will lead to slower load times every time this project is loaded.");
	set_text(confirmation_message);
	get_ok_button()->set_text(TTR("Restart & Upgrade"));

	popup_centered(Size2(750 * EDSCALE, 0));
}

void SurfaceUpgradeDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			// Can't do it in the constructor because it doesn't know that the signal exists.
			connect(SceneStringName(confirmed), callable_mp(SurfaceUpgradeTool::get_singleton(), &SurfaceUpgradeTool::prepare_upgrade));
			break;
	}
}

SurfaceUpgradeDialog::SurfaceUpgradeDialog() {
	set_autowrap(true);
	get_label()->set_custom_minimum_size(Size2(750 * EDSCALE, 0));
}
