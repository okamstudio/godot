/**************************************************************************/
/*  editor_scene_importer_blend.cpp                                       */
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

#include "editor_scene_importer_blend.h"

#ifdef TOOLS_ENABLED

#include "../gltf_defines.h"
#include "../gltf_document.h"
#include "editor_import_blend_runner.h"

#include "core/config/project_settings.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/themes/editor_scale.h"
#include "main/main.h"
#include "scene/gui/line_edit.h"

#ifdef MINGW_ENABLED
#define near
#define far
#endif

#ifdef WINDOWS_ENABLED
#include <shlwapi.h>
#endif

static bool _get_blender_version(const String &p_path, int &r_major, int &r_minor, String *r_err = nullptr) {
	if (!FileAccess::exists(p_path)) {
		if (r_err) {
			*r_err = TTR("Path does not point to a valid executable.");
		}
		return false;
	}
	List<String> args;
	args.push_back("--version");
	String pipe;
	Error err = OS::get_singleton()->execute(p_path, args, &pipe);
	if (err != OK) {
		if (r_err) {
			*r_err = TTR("Couldn't run Blender executable.");
		}
		return false;
	}
	int bl = pipe.find("Blender ");
	if (bl == -1) {
		if (r_err) {
			*r_err = vformat(TTR("Unexpected --version output from Blender executable at: %s."), p_path);
		}
		return false;
	}
	pipe = pipe.substr(bl);
	pipe = pipe.replace_first("Blender ", "");
	int pp = pipe.find_char('.');
	if (pp == -1) {
		if (r_err) {
			*r_err = vformat(TTR("Couldn't extract version information from Blender executable at: %s."), p_path);
		}
		return false;
	}
	String v = pipe.substr(0, pp);
	r_major = v.to_int();
	if (r_major < 3) {
		if (r_err) {
			*r_err = vformat(TTR("Found Blender version %d.x, which is too old for this importer (3.0+ is required)."), r_major);
		}
		return false;
	}

	int pp2 = pipe.find_char('.', pp + 1);
	r_minor = pp2 > pp ? pipe.substr(pp + 1, pp2 - pp - 1).to_int() : 0;

	return true;
}

void EditorSceneFormatImporterBlend::get_extensions(List<String> *r_extensions) const {
	r_extensions->push_back("blend");
}

Node *EditorSceneFormatImporterBlend::import_scene(const String &p_path, uint32_t p_flags,
		const HashMap<StringName, Variant> &p_options,
		List<String> *r_missing_deps, Error *r_err) {
	String blender_path = EDITOR_GET("filesystem/import/blender/blender_path");

	ERR_FAIL_COND_V_MSG(blender_path.is_empty(), nullptr, "Blender path is empty, check your Editor Settings.");
	ERR_FAIL_COND_V_MSG(!FileAccess::exists(blender_path), nullptr, vformat("Invalid Blender path: %s, check your Editor Settings.", blender_path));

	if (blender_major_version == -1 || blender_minor_version == -1 || last_tested_blender_path != blender_path) {
		String error;
		if (!_get_blender_version(blender_path, blender_major_version, blender_minor_version, &error)) {
			ERR_FAIL_V_MSG(nullptr, error);
		}
		last_tested_blender_path = blender_path;
	}

	// Get global paths for source and sink.
	// Escape paths to be valid Python strings to embed in the script.
	String source_global = ProjectSettings::get_singleton()->globalize_path(p_path);

#ifdef WINDOWS_ENABLED
	// On Windows, when using a network share path, the above will return a path starting with "//"
	// which once handed to Blender will be treated like a relative path. So we need to replace the
	// first two characters with "\\" to make it absolute again.
	if (source_global.is_network_share_path()) {
		source_global = "\\\\" + source_global.substr(2);
	}
#endif

	const String blend_basename = p_path.get_file().get_basename();
	const String sink = ProjectSettings::get_singleton()->get_imported_files_path().path_join(
			vformat("%s-%s.gltf", blend_basename, p_path.md5_text()));
	const String sink_global = ProjectSettings::get_singleton()->globalize_path(sink);
	// If true, unpack the original images to the Godot file system and use them. Allows changing image import settings like VRAM compression.
	// If false, allow Blender to convert the original images, such as re-packing roughness and metallic into one roughness+metallic texture.
	// In most cases this is desired, but if the .blend file's images are not in the correct format, this must be disabled for correct behavior.
	const bool unpack_original_images = p_options.has(SNAME("blender/materials/unpack_enabled")) && p_options[SNAME("blender/materials/unpack_enabled")];

	// Handle configuration options.

	Dictionary request_options;
	Dictionary parameters_map;

	parameters_map["filepath"] = sink_global;
	parameters_map["export_keep_originals"] = unpack_original_images;
	parameters_map["export_format"] = "GLTF_SEPARATE";
	parameters_map["export_yup"] = true;

	if (p_options.has(SNAME("blender/nodes/custom_properties")) && p_options[SNAME("blender/nodes/custom_properties")]) {
		parameters_map["export_extras"] = true;
	} else {
		parameters_map["export_extras"] = false;
	}
	if (p_options.has(SNAME("blender/meshes/skins"))) {
		int32_t skins = p_options["blender/meshes/skins"];
		if (skins == BLEND_BONE_INFLUENCES_NONE) {
			parameters_map["export_skins"] = false;
		} else if (skins == BLEND_BONE_INFLUENCES_COMPATIBLE) {
			parameters_map["export_skins"] = true;
			parameters_map["export_all_influences"] = false;
		} else if (skins == BLEND_BONE_INFLUENCES_ALL) {
			parameters_map["export_skins"] = true;
			parameters_map["export_all_influences"] = true;
		}
	} else {
		parameters_map["export_skins"] = false;
	}
	if (p_options.has(SNAME("blender/materials/export_materials"))) {
		int32_t exports = p_options["blender/materials/export_materials"];
		if (exports == BLEND_MATERIAL_EXPORT_PLACEHOLDER) {
			parameters_map["export_materials"] = "PLACEHOLDER";
		} else if (exports == BLEND_MATERIAL_EXPORT_EXPORT) {
			parameters_map["export_materials"] = "EXPORT";
		}
	} else {
		parameters_map["export_materials"] = "PLACEHOLDER";
	}
	if (p_options.has(SNAME("blender/nodes/cameras")) && p_options[SNAME("blender/nodes/cameras")]) {
		parameters_map["export_cameras"] = true;
	} else {
		parameters_map["export_cameras"] = false;
	}
	if (p_options.has(SNAME("blender/nodes/punctual_lights")) && p_options[SNAME("blender/nodes/punctual_lights")]) {
		parameters_map["export_lights"] = true;
	} else {
		parameters_map["export_lights"] = false;
	}
	if (blender_major_version > 4 || (blender_major_version == 4 && blender_minor_version >= 2)) {
		if (p_options.has(SNAME("blender/meshes/colors")) && p_options[SNAME("blender/meshes/colors")]) {
			parameters_map["export_vertex_color"] = "MATERIAL";
		} else {
			parameters_map["export_vertex_color"] = "NONE";
		}
	} else {
		if (p_options.has(SNAME("blender/meshes/colors")) && p_options[SNAME("blender/meshes/colors")]) {
			parameters_map["export_colors"] = true;
		} else {
			parameters_map["export_colors"] = false;
		}
	}
	if (p_options.has(SNAME("blender/nodes/visible"))) {
		int32_t visible = p_options["blender/nodes/visible"];
		if (visible == BLEND_VISIBLE_VISIBLE_ONLY) {
			parameters_map["use_visible"] = true;
		} else if (visible == BLEND_VISIBLE_RENDERABLE) {
			parameters_map["use_renderable"] = true;
		} else if (visible == BLEND_VISIBLE_ALL) {
			parameters_map["use_renderable"] = false;
			parameters_map["use_visible"] = false;
		}
	} else {
		parameters_map["use_renderable"] = false;
		parameters_map["use_visible"] = false;
	}
	if (p_options.has(SNAME("blender/nodes/active_collection_only")) && p_options[SNAME("blender/nodes/active_collection_only")]) {
		parameters_map["use_active_collection"] = true;
	}

	if (p_options.has(SNAME("blender/meshes/uvs")) && p_options[SNAME("blender/meshes/uvs")]) {
		parameters_map["export_texcoords"] = true;
	} else {
		parameters_map["export_texcoords"] = false;
	}
	if (p_options.has(SNAME("blender/meshes/normals")) && p_options[SNAME("blender/meshes/normals")]) {
		parameters_map["export_normals"] = true;
	} else {
		parameters_map["export_normals"] = false;
	}

	if (blender_major_version > 4 || (blender_major_version == 4 && blender_minor_version >= 1)) {
		if (p_options.has(SNAME("blender/meshes/export_geometry_nodes_instances")) && p_options[SNAME("blender/meshes/export_geometry_nodes_instances")]) {
			parameters_map["export_gn_mesh"] = true;
			if (blender_major_version == 4 && blender_minor_version == 1) {
				// There is a bug in Blender 4.1 where it can't export lights and geometry nodes at the same time, one must be disabled.
				parameters_map["export_lights"] = false;
			}
		} else {
			parameters_map["export_gn_mesh"] = false;
		}
	}
	if (p_options.has(SNAME("blender/meshes/tangents")) && p_options[SNAME("blender/meshes/tangents")]) {
		parameters_map["export_tangents"] = true;
	} else {
		parameters_map["export_tangents"] = false;
	}
	if (p_options.has(SNAME("blender/animation/group_tracks")) && p_options[SNAME("blender/animation/group_tracks")]) {
		if (blender_major_version > 3 || (blender_major_version == 3 && blender_minor_version >= 6)) {
			parameters_map["export_animation_mode"] = "ACTIONS";
		} else {
			parameters_map["export_nla_strips"] = true;
		}
	} else {
		if (blender_major_version > 3 || (blender_major_version == 3 && blender_minor_version >= 6)) {
			parameters_map["export_animation_mode"] = "ACTIVE_ACTIONS";
		} else {
			parameters_map["export_nla_strips"] = false;
		}
	}
	if (p_options.has(SNAME("blender/animation/limit_playback")) && p_options[SNAME("blender/animation/limit_playback")]) {
		parameters_map["export_frame_range"] = true;
	} else {
		parameters_map["export_frame_range"] = false;
	}
	if (p_options.has(SNAME("blender/animation/always_sample")) && p_options[SNAME("blender/animation/always_sample")]) {
		parameters_map["export_force_sampling"] = true;
	} else {
		parameters_map["export_force_sampling"] = false;
	}
	if (p_options.has(SNAME("blender/meshes/export_bones_deforming_mesh_only")) && p_options[SNAME("blender/meshes/export_bones_deforming_mesh_only")]) {
		parameters_map["export_def_bones"] = true;
	} else {
		parameters_map["export_def_bones"] = false;
	}
	if (p_options.has(SNAME("blender/nodes/modifiers")) && p_options[SNAME("blender/nodes/modifiers")]) {
		parameters_map["export_apply"] = true;
	} else {
		parameters_map["export_apply"] = false;
	}

	request_options["unpack_all"] = unpack_original_images;
	request_options["path"] = source_global;
	request_options["gltf_options"] = parameters_map;

	// Run Blender and export glTF.
	Error err = EditorImportBlendRunner::get_singleton()->do_import(request_options);
	if (err != OK) {
		if (r_err) {
			*r_err = ERR_SCRIPT_FAILED;
		}
		return nullptr;
	}

	// Import the generated glTF.

	// Use GLTFDocument instead of glTF importer to keep image references.
	Ref<GLTFDocument> gltf;
	gltf.instantiate();
	Ref<GLTFState> state;
	state.instantiate();

	if (p_options.has(SNAME("nodes/import_as_skeleton_bones")) ? (bool)p_options[SNAME("nodes/import_as_skeleton_bones")] : false) {
		state->set_import_as_skeleton_bones(true);
	}
	state->set_scene_name(blend_basename);
	state->set_extract_path(p_path.get_base_dir());
	state->set_extract_prefix(blend_basename);
	err = gltf->append_from_file(sink.get_basename() + ".gltf", state, p_flags, sink.get_base_dir());
	if (err != OK) {
		if (r_err) {
			*r_err = FAILED;
		}
		return nullptr;
	}
	ERR_FAIL_COND_V(!p_options.has("animation/fps"), nullptr);

#ifndef DISABLE_DEPRECATED
	bool trimming = p_options.has("animation/trimming") ? (bool)p_options["animation/trimming"] : false;
	return gltf->generate_scene(state, (float)p_options["animation/fps"], trimming, false);
#else
	return gltf->generate_scene(state, (float)p_options["animation/fps"], (bool)p_options["animation/trimming"], false);
#endif
}

Variant EditorSceneFormatImporterBlend::get_option_visibility(const String &p_path, const String &p_scene_import_type, const String &p_option,
		const HashMap<StringName, Variant> &p_options) {
	if (p_path.get_extension().to_lower() != "blend") {
		return true;
	}

	if (p_option.begins_with("animation/")) {
		if (p_option != "animation/import" && !bool(p_options["animation/import"])) {
			return false;
		}
	}
	return true;
}

void EditorSceneFormatImporterBlend::get_import_options(const String &p_path, List<ResourceImporter::ImportOption> *r_options) {
	// Returns all the options when path is empty because that means it's for the Project Settings.
	if (!p_path.is_empty() && p_path.get_extension().to_lower() != "blend") {
		return;
	}
#define ADD_OPTION_BOOL(PATH, VALUE) \
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, SNAME(PATH)), VALUE));
#define ADD_OPTION_ENUM(PATH, ENUM_HINT, VALUE) \
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, SNAME(PATH), PROPERTY_HINT_ENUM, ENUM_HINT), VALUE));

	ADD_OPTION_ENUM("blender/nodes/visible", "All,Visible Only,Renderable", BLEND_VISIBLE_ALL);
	ADD_OPTION_BOOL("blender/nodes/active_collection_only", false);
	ADD_OPTION_BOOL("blender/nodes/punctual_lights", true);
	ADD_OPTION_BOOL("blender/nodes/cameras", true);
	ADD_OPTION_BOOL("blender/nodes/custom_properties", true);
	ADD_OPTION_ENUM("blender/nodes/modifiers", "No Modifiers,All Modifiers", BLEND_MODIFIERS_ALL);
	ADD_OPTION_BOOL("blender/meshes/colors", false);
	ADD_OPTION_BOOL("blender/meshes/uvs", true);
	ADD_OPTION_BOOL("blender/meshes/normals", true);
	ADD_OPTION_BOOL("blender/meshes/export_geometry_nodes_instances", false);
	ADD_OPTION_BOOL("blender/meshes/tangents", true);
	ADD_OPTION_ENUM("blender/meshes/skins", "None,4 Influences (Compatible),All Influences", BLEND_BONE_INFLUENCES_ALL);
	ADD_OPTION_BOOL("blender/meshes/export_bones_deforming_mesh_only", false);
	ADD_OPTION_BOOL("blender/materials/unpack_enabled", true);
	ADD_OPTION_ENUM("blender/materials/export_materials", "Placeholder,Export", BLEND_MATERIAL_EXPORT_EXPORT);
	ADD_OPTION_BOOL("blender/animation/limit_playback", true);
	ADD_OPTION_BOOL("blender/animation/always_sample", true);
	ADD_OPTION_BOOL("blender/animation/group_tracks", true);
}

///////////////////////////

static bool _test_blender_path(const String &p_path, String *r_err = nullptr) {
	int major, minor;
	return _get_blender_version(p_path, major, minor, r_err);
}

bool EditorFileSystemImportFormatSupportQueryBlend::is_active() const {
	bool blend_enabled = GLOBAL_GET("filesystem/import/blender/enabled");

	if (blend_enabled && !_test_blender_path(EDITOR_GET("filesystem/import/blender/blender_path").operator String())) {
		// Intending to import Blender, but blend not configured.
		return true;
	}

	return false;
}
Vector<String> EditorFileSystemImportFormatSupportQueryBlend::get_file_extensions() const {
	Vector<String> ret;
	ret.push_back("blend");
	return ret;
}

void EditorFileSystemImportFormatSupportQueryBlend::_validate_path(String p_path) {
	String error;
	bool success = false;
	if (p_path == "") {
		error = TTR("Path is empty.");
	} else {
		if (_test_blender_path(p_path, &error)) {
			success = true;
			if (auto_detected_path == p_path) {
				error = TTR("Path to Blender executable is valid (Autodetected).");
			} else {
				error = TTR("Path to Blender executable is valid.");
			}
		}
	}

	path_status->set_text(error);

	if (success) {
		path_status->add_theme_color_override(SceneStringName(font_color), path_status->get_theme_color(SNAME("success_color"), EditorStringName(Editor)));
		configure_blender_dialog->get_ok_button()->set_disabled(false);
	} else {
		path_status->add_theme_color_override(SceneStringName(font_color), path_status->get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
		configure_blender_dialog->get_ok_button()->set_disabled(true);
	}
}

bool EditorFileSystemImportFormatSupportQueryBlend::_autodetect_path() {
	// Autodetect
	auto_detected_path = "";

#if defined(MACOS_ENABLED)
	Vector<String> find_paths = {
		"/opt/homebrew/bin/blender",
		"/opt/local/bin/blender",
		"/usr/local/bin/blender",
		"/usr/local/opt/blender",
		"/Applications/Blender.app/Contents/MacOS/Blender",
	};
	{
		List<String> mdfind_args;
		mdfind_args.push_back("kMDItemCFBundleIdentifier=org.blenderfoundation.blender");

		String output;
		Error err = OS::get_singleton()->execute("mdfind", mdfind_args, &output);
		if (err == OK) {
			for (const String &find_path : output.split("\n")) {
				find_paths.push_back(find_path.path_join("Contents/MacOS/Blender"));
			}
		}
	}
#elif defined(WINDOWS_ENABLED)
	Vector<String> find_paths = {
		"C:\\Program Files\\Blender Foundation\\blender.exe",
		"C:\\Program Files (x86)\\Blender Foundation\\blender.exe",
	};
	{
		char blender_opener_path[MAX_PATH];
		DWORD path_len = MAX_PATH;
		HRESULT res = AssocQueryString(0, ASSOCSTR_EXECUTABLE, ".blend", "open", blender_opener_path, &path_len);
		if (res == S_OK) {
			find_paths.push_back(String(blender_opener_path).get_base_dir().path_join("blender.exe"));
		}
	}

#elif defined(UNIX_ENABLED)
	Vector<String> find_paths = {
		"/usr/bin/blender",
		"/usr/local/bin/blender",
		"/opt/blender/bin/blender",
	};
#endif

	for (const String &find_path : find_paths) {
		if (_test_blender_path(find_path)) {
			auto_detected_path = find_path;
			return true;
		}
	}

	return false;
}

void EditorFileSystemImportFormatSupportQueryBlend::_path_confirmed() {
	confirmed = true;
}

void EditorFileSystemImportFormatSupportQueryBlend::_select_install(String p_path) {
	blender_path->set_text(p_path);
	_validate_path(p_path);
}
void EditorFileSystemImportFormatSupportQueryBlend::_browse_install() {
	if (blender_path->get_text() != String()) {
		browse_dialog->set_current_file(blender_path->get_text());
	}

	browse_dialog->popup_centered_ratio();
}

void EditorFileSystemImportFormatSupportQueryBlend::_update_icons() {
	blender_path_browse->set_button_icon(blender_path_browse->get_editor_theme_icon(SNAME("FolderBrowse")));
}

bool EditorFileSystemImportFormatSupportQueryBlend::query() {
	if (!configure_blender_dialog) {
		configure_blender_dialog = memnew(ConfirmationDialog);
		configure_blender_dialog->set_title(TTR("Configure Blender Importer"));
		configure_blender_dialog->set_flag(Window::FLAG_BORDERLESS, true); // Avoid closing accidentally.
		configure_blender_dialog->set_close_on_escape(false);

		String select_exec_label = TTR("Blender 3.0+ is required to import '.blend' files.\nPlease provide a valid path to a Blender executable.");
#ifdef MACOS_ENABLED
		select_exec_label += "\n" + TTR("On macOS, this should be the `Contents/MacOS/blender` file within the Blender `.app` folder.");
#endif
		VBoxContainer *vb = memnew(VBoxContainer);
		vb->add_child(memnew(Label(select_exec_label)));

		HBoxContainer *hb = memnew(HBoxContainer);

		blender_path = memnew(LineEdit);
		blender_path->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		hb->add_child(blender_path);

		blender_path_browse = memnew(Button);
		blender_path_browse->set_text(TTR("Browse"));
		blender_path_browse->connect(SceneStringName(pressed), callable_mp(this, &EditorFileSystemImportFormatSupportQueryBlend::_browse_install));
		hb->add_child(blender_path_browse);

		hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		hb->set_custom_minimum_size(Size2(400 * EDSCALE, 0));

		vb->add_child(hb);

		path_status = memnew(Label);
		vb->add_child(path_status);

		configure_blender_dialog->add_child(vb);

		blender_path->connect(SceneStringName(text_changed), callable_mp(this, &EditorFileSystemImportFormatSupportQueryBlend::_validate_path));

		EditorNode::get_singleton()->get_gui_base()->add_child(configure_blender_dialog);

		configure_blender_dialog->set_ok_button_text(TTR("Confirm Path"));
		configure_blender_dialog->set_cancel_button_text(TTR("Disable '.blend' Import"));
		configure_blender_dialog->get_cancel_button()->set_tooltip_text(TTR("Disables Blender '.blend' files import for this project. Can be re-enabled in Project Settings."));
		configure_blender_dialog->connect(SceneStringName(confirmed), callable_mp(this, &EditorFileSystemImportFormatSupportQueryBlend::_path_confirmed));

		browse_dialog = memnew(EditorFileDialog);
		browse_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
		browse_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
		browse_dialog->connect("file_selected", callable_mp(this, &EditorFileSystemImportFormatSupportQueryBlend::_select_install));

		EditorNode::get_singleton()->get_gui_base()->add_child(browse_dialog);

		// Update icons.
		// This is a hack because we can't rely on notifications here as we don't receive them.
		// Usually, we only have to wait for `NOTIFICATION_THEME_CHANGED` to update the icons.
		callable_mp(this, &EditorFileSystemImportFormatSupportQueryBlend::_update_icons).call_deferred();
	}

	String path = EDITOR_GET("filesystem/import/blender/blender_path");

	if (path.is_empty() && _autodetect_path()) {
		path = auto_detected_path;
	}

	blender_path->set_text(path);

	_validate_path(path);

	configure_blender_dialog->popup_centered();
	confirmed = false;

	while (true) {
		DisplayServer::get_singleton()->process_events();
		Main::iteration();
		if (!configure_blender_dialog->is_visible() || confirmed) {
			break;
		}
	}

	if (confirmed) {
		// Can only confirm a valid path.
		EditorSettings::get_singleton()->set("filesystem/import/blender/blender_path", blender_path->get_text());
		EditorSettings::get_singleton()->save();
	} else {
		// Disable Blender import
		ProjectSettings::get_singleton()->set("filesystem/import/blender/enabled", false);
		ProjectSettings::get_singleton()->save();

		if (EditorNode::immediate_confirmation_dialog(TTR("Disabling '.blend' file import requires restarting the editor."), TTR("Save & Restart"), TTR("Restart"))) {
			EditorNode::get_singleton()->save_all_scenes();
		}
		EditorNode::get_singleton()->restart_editor();
		return true;
	}

	return false;
}

EditorFileSystemImportFormatSupportQueryBlend::EditorFileSystemImportFormatSupportQueryBlend() {
}

#endif // TOOLS_ENABLED
