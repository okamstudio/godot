/**************************************************************************/
/*  editor_build_profile.cpp                                              */
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

#include "editor_build_profile.h"

#include "core/io/json.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/editor_paths.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/separator.h"

const char *EditorBuildProfile::build_option_identifiers[BUILD_OPTION_MAX] = {
	// This maps to SCons build options.
	"disable_3d",
	"disable_2d_physics",
	"disable_3d_physics",
	"disable_navigation",
	"openxr",
	"rendering_device", // FIXME: there's no scons option to disable rendering device
	"opengl3",
	"vulkan",
	"module_text_server_fb_enabled",
	"module_text_server_adv_enabled",
	"module_freetype_enabled",
	"brotli",
	"graphite",
	"module_msdfgen_enabled"
};

const bool EditorBuildProfile::build_option_disabled_by_default[BUILD_OPTION_MAX] = {
	// This maps to SCons build options.
	false, // 3D
	false, // PHYSICS_2D
	false, // PHYSICS_3D
	false, // NAVIGATION
	false, // XR
	false, // RENDERING_DEVICE
	false, // OPENGL
	false, // VULKAN
	true, // TEXT_SERVER_FALLBACK
	false, // TEXT_SERVER_COMPLEX
	false, // DYNAMIC_FONTS
	false, // WOFF2_FONTS
	false, // GRAPHITE_FONTS
	false, // MSDFGEN
};

const bool EditorBuildProfile::build_option_disable_values[BUILD_OPTION_MAX] = {
	// This maps to SCons build options.
	true, // 3D
	true, // PHYSICS_2D
	true, // PHYSICS_3D
	true, // NAVIGATION
	false, // XR
	false, // RENDERING_DEVICE
	false, // OPENGL
	false, // VULKAN
	false, // TEXT_SERVER_FALLBACK
	false, // TEXT_SERVER_COMPLEX
	false, // DYNAMIC_FONTS
	false, // WOFF2_FONTS
	false, // GRAPHITE_FONTS
	false, // MSDFGEN
};

const EditorBuildProfile::BuildOptionCategory EditorBuildProfile::build_option_category[BUILD_OPTION_MAX] = {
	BUILD_OPTION_CATEGORY_GENERAL, // 3D
	BUILD_OPTION_CATEGORY_GENERAL, // PHYSICS_2D
	BUILD_OPTION_CATEGORY_GENERAL, // PHYSICS_3D
	BUILD_OPTION_CATEGORY_GENERAL, // NAVIGATION
	BUILD_OPTION_CATEGORY_GENERAL, // XR
	BUILD_OPTION_CATEGORY_GENERAL, // RENDERING_DEVICE
	BUILD_OPTION_CATEGORY_GENERAL, // OPENGL
	BUILD_OPTION_CATEGORY_GENERAL, // VULKAN
	BUILD_OPTION_CATEGORY_TEXT_SERVER, // TEXT_SERVER_FALLBACK
	BUILD_OPTION_CATEGORY_TEXT_SERVER, // TEXT_SERVER_COMPLEX
	BUILD_OPTION_CATEGORY_TEXT_SERVER, // DYNAMIC_FONTS
	BUILD_OPTION_CATEGORY_TEXT_SERVER, // WOFF2_FONTS
	BUILD_OPTION_CATEGORY_TEXT_SERVER, // GRAPHITE_FONTS
	BUILD_OPTION_CATEGORY_TEXT_SERVER, // MSDFGEN
};

void EditorBuildProfile::set_disable_class(const StringName &p_class, bool p_disabled) {
	if (p_disabled) {
		disabled_classes.insert(p_class);
	} else {
		disabled_classes.erase(p_class);
	}
}

bool EditorBuildProfile::is_class_disabled(const StringName &p_class) const {
	if (p_class == StringName()) {
		return false;
	}
	return disabled_classes.has(p_class) || is_class_disabled(ClassDB::get_parent_class_nocheck(p_class));
}

void EditorBuildProfile::set_item_collapsed(const StringName &p_class, bool p_collapsed) {
	if (p_collapsed) {
		collapsed_classes.insert(p_class);
	} else {
		collapsed_classes.erase(p_class);
	}
}

bool EditorBuildProfile::is_item_collapsed(const StringName &p_class) const {
	return collapsed_classes.has(p_class);
}

void EditorBuildProfile::set_disable_build_option(BuildOption p_build_option, bool p_disable) {
	ERR_FAIL_INDEX(p_build_option, BUILD_OPTION_MAX);
	build_options_disabled[p_build_option] = p_disable;
}

void EditorBuildProfile::clear_disabled_classes() {
	disabled_classes.clear();
	collapsed_classes.clear();
}

bool EditorBuildProfile::is_build_option_disabled(BuildOption p_build_option) const {
	ERR_FAIL_INDEX_V(p_build_option, BUILD_OPTION_MAX, false);
	return build_options_disabled[p_build_option];
}

bool EditorBuildProfile::get_build_option_disable_value(BuildOption p_build_option) {
	ERR_FAIL_INDEX_V(p_build_option, BUILD_OPTION_MAX, false);
	return build_option_disable_values[p_build_option];
}

void EditorBuildProfile::set_force_detect_classes(const String &p_classes) {
	force_detect_classes = p_classes;
}

String EditorBuildProfile::get_force_detect_classes() const {
	return force_detect_classes;
}

String EditorBuildProfile::get_build_option_name(BuildOption p_build_option) {
	ERR_FAIL_INDEX_V(p_build_option, BUILD_OPTION_MAX, String());
	const char *build_option_names[BUILD_OPTION_MAX] = {
		TTRC("3D Engine"),
		TTRC("2D Physics"),
		TTRC("3D Physics"),
		TTRC("Navigation"),
		TTRC("XR"),
		TTRC("RenderingDevice"),
		TTRC("OpenGL"),
		TTRC("Vulkan"),
		TTRC("Text Server: Fallback"),
		TTRC("Text Server: Advanced"),
		TTRC("TTF, OTF, Type 1, WOFF1 Fonts"),
		TTRC("WOFF2 Fonts"),
		TTRC("SIL Graphite Fonts"),
		TTRC("Multi-channel Signed Distance Field Font Rendering"),
	};
	return TTRGET(build_option_names[p_build_option]);
}

String EditorBuildProfile::get_build_option_description(BuildOption p_build_option) {
	ERR_FAIL_INDEX_V(p_build_option, BUILD_OPTION_MAX, String());

	const char *build_option_descriptions[BUILD_OPTION_MAX] = {
		TTRC("3D Nodes as well as RenderingServer access to 3D features."),
		TTRC("2D Physics nodes and PhysicsServer2D."),
		TTRC("3D Physics nodes and PhysicsServer3D."),
		TTRC("Navigation, both 2D and 3D."),
		TTRC("XR (AR and VR)."),
		TTRC("RenderingDevice based rendering (if disabled, the OpenGL back-end is required)."),
		TTRC("OpenGL back-end (if disabled, the RenderingDevice back-end is required)."),
		TTRC("Vulkan back-end of RenderingDevice."),
		TTRC("Fallback implementation of Text Server\nSupports basic text layouts."),
		TTRC("Text Server implementation powered by ICU and HarfBuzz libraries.\nSupports complex text layouts, BiDi, and contextual OpenType font features."),
		TTRC("TrueType, OpenType, Type 1, and WOFF1 font format support using FreeType library (if disabled, WOFF2 support is also disabled)."),
		TTRC("WOFF2 font format support using FreeType and Brotli libraries."),
		TTRC("SIL Graphite smart font technology support (supported by Advanced Text Server only)."),
		TTRC("Multi-channel signed distance field font rendering support using msdfgen library (pre-rendered MSDF fonts can be used even if this option disabled)."),
	};

	return TTRGET(build_option_descriptions[p_build_option]);
}

EditorBuildProfile::BuildOptionCategory EditorBuildProfile::get_build_option_category(BuildOption p_build_option) {
	ERR_FAIL_INDEX_V(p_build_option, BUILD_OPTION_MAX, BUILD_OPTION_CATEGORY_GENERAL);
	return build_option_category[p_build_option];
}

String EditorBuildProfile::get_build_option_category_name(BuildOptionCategory p_build_option_category) {
	ERR_FAIL_INDEX_V(p_build_option_category, BUILD_OPTION_CATEGORY_MAX, String());

	const char *build_option_subcategories[BUILD_OPTION_CATEGORY_MAX]{
		TTRC("General Features:"),
		TTRC("Text Rendering and Font Options:"),
	};

	return TTRGET(build_option_subcategories[p_build_option_category]);
}

Error EditorBuildProfile::save_to_file(const String &p_path) {
	Dictionary data;
	data["type"] = "build_profile";
	Array dis_classes;
	for (const StringName &E : disabled_classes) {
		dis_classes.push_back(String(E));
	}
	dis_classes.sort();
	data["disabled_classes"] = dis_classes;

	Dictionary dis_build_options;
	for (int i = 0; i < BUILD_OPTION_MAX; i++) {
		if (build_options_disabled[i] != build_option_disabled_by_default[i]) {
			if (build_options_disabled[i]) {
				dis_build_options[build_option_identifiers[i]] = build_option_disable_values[i];
			} else {
				dis_build_options[build_option_identifiers[i]] = !build_option_disable_values[i];
			}
		}
	}

	data["disabled_build_options"] = dis_build_options;

	if (!force_detect_classes.is_empty()) {
		data["force_detect_classes"] = force_detect_classes;
	}

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_CANT_CREATE, "Cannot create file '" + p_path + "'.");

	String text = JSON::stringify(data, "\t");
	f->store_string(text);
	return OK;
}

Error EditorBuildProfile::load_from_file(const String &p_path) {
	Error err;
	String text = FileAccess::get_file_as_string(p_path, &err);
	if (err != OK) {
		return err;
	}

	JSON json;
	err = json.parse(text);
	if (err != OK) {
		ERR_PRINT("Error parsing '" + p_path + "' on line " + itos(json.get_error_line()) + ": " + json.get_error_message());
		return ERR_PARSE_ERROR;
	}

	Dictionary data = json.get_data();

	if (!data.has("type") || String(data["type"]) != "build_profile") {
		ERR_PRINT("Error parsing '" + p_path + "', it's not a build profile.");
		return ERR_PARSE_ERROR;
	}

	disabled_classes.clear();

	if (data.has("disabled_classes")) {
		Array disabled_classes_arr = data["disabled_classes"];
		for (int i = 0; i < disabled_classes_arr.size(); i++) {
			disabled_classes.insert(disabled_classes_arr[i]);
		}
	}

	for (int i = 0; i < BUILD_OPTION_MAX; i++) {
		build_options_disabled[i] = build_option_disabled_by_default[i];
	}

	if (data.has("disabled_build_options")) {
		Dictionary disabled_build_options_arr = data["disabled_build_options"];
		List<Variant> keys;
		disabled_build_options_arr.get_key_list(&keys);

		for (const Variant &K : keys) {
			String key = K;

			for (int i = 0; i < BUILD_OPTION_MAX; i++) {
				String f = build_option_identifiers[i];
				if (f == key) {
					build_options_disabled[i] = true;
					break;
				}
			}
		}
	}

	if (data.has("force_detect_classes")) {
		force_detect_classes = data["force_detect_classes"];
	}

	return OK;
}

void EditorBuildProfile::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_disable_class", "class_name", "disable"), &EditorBuildProfile::set_disable_class);
	ClassDB::bind_method(D_METHOD("is_class_disabled", "class_name"), &EditorBuildProfile::is_class_disabled);

	ClassDB::bind_method(D_METHOD("set_disable_build_option", "build_option", "disable"), &EditorBuildProfile::set_disable_build_option);
	ClassDB::bind_method(D_METHOD("is_build_option_disabled", "build_option"), &EditorBuildProfile::is_build_option_disabled);

	ClassDB::bind_method(D_METHOD("get_build_option_name", "build_option"), &EditorBuildProfile::_get_build_option_name);

	ClassDB::bind_method(D_METHOD("save_to_file", "path"), &EditorBuildProfile::save_to_file);
	ClassDB::bind_method(D_METHOD("load_from_file", "path"), &EditorBuildProfile::load_from_file);

	BIND_ENUM_CONSTANT(BUILD_OPTION_3D);
	BIND_ENUM_CONSTANT(BUILD_OPTION_PHYSICS_2D);
	BIND_ENUM_CONSTANT(BUILD_OPTION_PHYSICS_3D);
	BIND_ENUM_CONSTANT(BUILD_OPTION_NAVIGATION);
	BIND_ENUM_CONSTANT(BUILD_OPTION_XR);
	BIND_ENUM_CONSTANT(BUILD_OPTION_RENDERING_DEVICE);
	BIND_ENUM_CONSTANT(BUILD_OPTION_OPENGL);
	BIND_ENUM_CONSTANT(BUILD_OPTION_VULKAN);
	BIND_ENUM_CONSTANT(BUILD_OPTION_TEXT_SERVER_FALLBACK);
	BIND_ENUM_CONSTANT(BUILD_OPTION_TEXT_SERVER_ADVANCED);
	BIND_ENUM_CONSTANT(BUILD_OPTION_DYNAMIC_FONTS);
	BIND_ENUM_CONSTANT(BUILD_OPTION_WOFF2_FONTS);
	BIND_ENUM_CONSTANT(BUILD_OPTION_GRAPHITE_FONTS);
	BIND_ENUM_CONSTANT(BUILD_OPTION_MSDFGEN);
	BIND_ENUM_CONSTANT(BUILD_OPTION_MAX);

	BIND_ENUM_CONSTANT(BUILD_OPTION_CATEGORY_GENERAL);
	BIND_ENUM_CONSTANT(BUILD_OPTION_CATEGORY_TEXT_SERVER);
	BIND_ENUM_CONSTANT(BUILD_OPTION_CATEGORY_MAX);
}

EditorBuildProfile::EditorBuildProfile() {
	for (int i = 0; i < EditorBuildProfile::BUILD_OPTION_MAX; i++) {
		build_options_disabled[i] = build_option_disabled_by_default[i];
	}
}

//////////////////////////

void EditorBuildProfileManager::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			String last_file = EditorSettings::get_singleton()->get_project_metadata("build_profile", "last_file_path", "");
			if (!last_file.is_empty()) {
				_import_profile(last_file);
			}
			if (edited.is_null()) {
				edited.instantiate();
				_update_edited_profile();
			}

		} break;
	}
}

void EditorBuildProfileManager::_profile_action(int p_action) {
	last_action = Action(p_action);

	switch (p_action) {
		case ACTION_RESET: {
			confirm_dialog->set_text(TTR("Reset the edited profile?"));
			confirm_dialog->popup_centered();
		} break;
		case ACTION_LOAD: {
			import_profile->popup_file_dialog();
		} break;
		case ACTION_SAVE: {
			if (!profile_path->get_text().is_empty()) {
				Error err = edited->save_to_file(profile_path->get_text());
				if (err != OK) {
					EditorNode::get_singleton()->show_warning(TTR("File saving failed."));
				}
				break;
			}
			[[fallthrough]];
		}
		case ACTION_SAVE_AS: {
			export_profile->popup_file_dialog();
			export_profile->set_current_file(profile_path->get_text());
		} break;
		case ACTION_NEW: {
			confirm_dialog->set_text(TTR("Create a new profile?"));
			confirm_dialog->popup_centered();
		} break;
		case ACTION_DETECT: {
			confirm_dialog->set_text(TTR("This will scan all files in the current project to detect used classes."));
			confirm_dialog->popup_centered();
		} break;
		case ACTION_MAX: {
		} break;
	}
}

void EditorBuildProfileManager::_find_files(EditorFileSystemDirectory *p_dir, const HashMap<String, DetectedFile> &p_cache, HashMap<String, DetectedFile> &r_detected) {
	if (p_dir == nullptr) {
		return;
	}

	for (int i = 0; i < p_dir->get_file_count(); i++) {
		String p = p_dir->get_file_path(i);

		uint64_t timestamp = 0;
		String md5;

		if (p_cache.has(p)) {
			const DetectedFile &cache = p_cache[p];
			// Check if timestamp and MD5 match.
			timestamp = FileAccess::get_modified_time(p);
			bool cache_valid = true;
			if (cache.timestamp != timestamp) {
				md5 = FileAccess::get_md5(p);
				if (md5 != cache.md5) {
					cache_valid = false;
				}
			}

			if (cache_valid) {
				r_detected.insert(p, cache);
				continue;
			}
		}

		// Not cached, or cache invalid.

		DetectedFile cache;

		HashSet<StringName> classes;
		ResourceLoader::get_classes_used(p, &classes);

		for (const StringName &E : classes) {
			cache.classes.push_back(E);
		}

		if (md5.is_empty()) {
			cache.timestamp = FileAccess::get_modified_time(p);
			cache.md5 = FileAccess::get_md5(p);
		} else {
			cache.timestamp = timestamp;
			cache.md5 = md5;
		}

		r_detected.insert(p, cache);
	}

	for (int i = 0; i < p_dir->get_subdir_count(); i++) {
		_find_files(p_dir->get_subdir(i), p_cache, r_detected);
	}
}

void EditorBuildProfileManager::_detect_classes() {
	HashMap<String, DetectedFile> previous_file_cache;

	Ref<FileAccess> f = FileAccess::open(EditorPaths::get_singleton()->get_project_settings_dir().path_join("used_class_cache"), FileAccess::READ);
	if (f.is_valid()) {
		while (!f->eof_reached()) {
			String l = f->get_line();
			Vector<String> fields = l.split("::");
			if (fields.size() == 4) {
				const String &path = fields[0];
				DetectedFile df;
				df.timestamp = fields[1].to_int();
				df.md5 = fields[2];
				df.classes = fields[3].split(",");
				previous_file_cache.insert(path, df);
			}
		}
		f.unref();
	}

	HashMap<String, DetectedFile> updated_file_cache;

	_find_files(EditorFileSystem::get_singleton()->get_filesystem(), previous_file_cache, updated_file_cache);

	HashSet<StringName> used_classes;

	// Find classes and update the disk cache in the process.
	f = FileAccess::open(EditorPaths::get_singleton()->get_project_settings_dir().path_join("used_class_cache"), FileAccess::WRITE);

	for (const KeyValue<String, DetectedFile> &E : updated_file_cache) {
		String l = E.key + "::" + itos(E.value.timestamp) + "::" + E.value.md5 + "::";
		for (int i = 0; i < E.value.classes.size(); i++) {
			String c = E.value.classes[i];
			if (i > 0) {
				l += ",";
			}
			l += c;
			used_classes.insert(c);
		}
		f->store_line(l);
	}

	f.unref();

	// Add forced ones.

	Vector<String> force_detect = edited->get_force_detect_classes().split(",");
	for (int i = 0; i < force_detect.size(); i++) {
		String c = force_detect[i].strip_edges();
		if (c.is_empty()) {
			continue;
		}
		used_classes.insert(c);
	}

	// Filter all classes to discard inherited ones.

	HashSet<StringName> all_used_classes;

	for (const StringName &E : used_classes) {
		StringName c = E;
		if (!ClassDB::class_exists(c)) {
			// Maybe this is an old class that got replaced? try getting compat class.
			c = ClassDB::get_compatibility_class(c);
			if (!c) {
				// No luck, skip.
				continue;
			}
		}
		while (c) {
			all_used_classes.insert(c);
			c = ClassDB::get_parent_class(c);
		}
	}

	edited->clear_disabled_classes();

	List<StringName> all_classes;
	ClassDB::get_class_list(&all_classes);

	for (const StringName &E : all_classes) {
		if (all_used_classes.has(E)) {
			// This class is valid, do nothing.
			continue;
		}

		StringName p = ClassDB::get_parent_class(E);
		if (!p || all_used_classes.has(p)) {
			// If no parent, or if the parent is enabled, then add to disabled classes.
			// This way we avoid disabling redundant classes.
			edited->set_disable_class(E, true);
		}
	}
}

void EditorBuildProfileManager::_action_confirm() {
	switch (last_action) {
		case ACTION_RESET: {
			edited.instantiate();
			_update_edited_profile();
		} break;
		case ACTION_LOAD: {
		} break;
		case ACTION_SAVE: {
		} break;
		case ACTION_SAVE_AS: {
		} break;
		case ACTION_NEW: {
			profile_path->set_text("");
			edited.instantiate();
			_update_edited_profile();
		} break;
		case ACTION_DETECT: {
			_detect_classes();
			_update_edited_profile();
		} break;
		case ACTION_MAX: {
		} break;
	}
}

void EditorBuildProfileManager::_hide_requested() {
	_cancel_pressed(); // From AcceptDialog.
}

void EditorBuildProfileManager::_fill_classes_from(TreeItem *p_parent, const String &p_class, const String &p_selected) {
	TreeItem *class_item = class_list->create_item(p_parent);
	class_item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
	class_item->set_icon(0, EditorNode::get_singleton()->get_class_icon(p_class));
	const String &text = p_class;

	bool disabled = edited->is_class_disabled(p_class);
	if (disabled) {
		class_item->set_custom_color(0, class_list->get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
	}

	class_item->set_text(0, text);
	class_item->set_editable(0, true);
	class_item->set_selectable(0, true);
	class_item->set_metadata(0, p_class);

	bool collapsed = edited->is_item_collapsed(p_class);
	class_item->set_collapsed(collapsed);

	if (p_class == p_selected) {
		class_item->select(0);
	}
	if (disabled) {
		// Class disabled, do nothing else (do not show further).
		return;
	}

	class_item->set_checked(0, true); // If it's not disabled, its checked.

	List<StringName> child_classes;
	ClassDB::get_direct_inheriters_from_class(p_class, &child_classes);
	child_classes.sort_custom<StringName::AlphCompare>();

	for (const StringName &name : child_classes) {
		if (String(name).begins_with("Editor") || ClassDB::get_api_type(name) != ClassDB::API_CORE) {
			continue;
		}
		_fill_classes_from(class_item, name, p_selected);
	}
}

void EditorBuildProfileManager::_class_list_item_selected() {
	if (updating_build_options) {
		return;
	}

	TreeItem *item = class_list->get_selected();
	if (!item) {
		return;
	}

	Variant md = item->get_metadata(0);
	if (md.is_string()) {
		description_bit->parse_symbol("class|" + md.operator String() + "|");
	} else if (md.get_type() == Variant::INT) {
		String build_option_description = EditorBuildProfile::get_build_option_description(EditorBuildProfile::BuildOption((int)md));
		description_bit->set_custom_text(TTR(item->get_text(0)), String(), TTRGET(build_option_description));
	}
}

void EditorBuildProfileManager::_class_list_item_edited() {
	if (updating_build_options) {
		return;
	}

	TreeItem *item = class_list->get_edited();
	if (!item) {
		return;
	}

	bool checked = item->is_checked(0);

	Variant md = item->get_metadata(0);
	if (md.is_string()) {
		String class_selected = md;
		edited->set_disable_class(class_selected, !checked);
		_update_edited_profile();
	} else if (md.get_type() == Variant::INT) {
		int build_option_selected = md;
		edited->set_disable_build_option(EditorBuildProfile::BuildOption(build_option_selected), !checked);
	}
}

void EditorBuildProfileManager::_class_list_item_collapsed(Object *p_item) {
	if (updating_build_options) {
		return;
	}

	TreeItem *item = Object::cast_to<TreeItem>(p_item);
	if (!item) {
		return;
	}

	Variant md = item->get_metadata(0);
	if (!md.is_string()) {
		return;
	}

	String class_name = md;
	bool collapsed = item->is_collapsed();
	edited->set_item_collapsed(class_name, collapsed);
}

void EditorBuildProfileManager::_update_edited_profile() {
	String class_selected;
	int build_option_selected = -1;

	if (class_list->get_selected()) {
		Variant md = class_list->get_selected()->get_metadata(0);
		if (md.is_string()) {
			class_selected = md;
		} else if (md.get_type() == Variant::INT) {
			build_option_selected = md;
		}
	}

	class_list->clear();

	updating_build_options = true;

	TreeItem *root = class_list->create_item();

	HashMap<EditorBuildProfile::BuildOptionCategory, TreeItem *> subcats;
	for (int i = 0; i < EditorBuildProfile::BUILD_OPTION_CATEGORY_MAX; i++) {
		TreeItem *build_cat;
		build_cat = class_list->create_item(root);

		build_cat->set_text(0, EditorBuildProfile::get_build_option_category_name(EditorBuildProfile::BuildOptionCategory(i)));
		subcats[EditorBuildProfile::BuildOptionCategory(i)] = build_cat;
	}

	for (int i = 0; i < EditorBuildProfile::BUILD_OPTION_MAX; i++) {
		TreeItem *build_option;
		build_option = class_list->create_item(subcats[EditorBuildProfile::get_build_option_category(EditorBuildProfile::BuildOption(i))]);

		build_option->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
		build_option->set_text(0, EditorBuildProfile::get_build_option_name(EditorBuildProfile::BuildOption(i)));
		build_option->set_selectable(0, true);
		build_option->set_editable(0, true);
		build_option->set_metadata(0, i);
		if (!edited->is_build_option_disabled(EditorBuildProfile::BuildOption(i))) {
			build_option->set_checked(0, true);
		}

		if (i == build_option_selected) {
			build_option->select(0);
		}
	}

	TreeItem *classes = class_list->create_item(root);
	classes->set_text(0, TTR("Nodes and Classes:"));

	_fill_classes_from(classes, "Node", class_selected);
	_fill_classes_from(classes, "Resource", class_selected);

	force_detect_classes->set_text(edited->get_force_detect_classes());

	updating_build_options = false;

	_class_list_item_selected();
}

void EditorBuildProfileManager::_force_detect_classes_changed(const String &p_text) {
	if (updating_build_options) {
		return;
	}
	edited->set_force_detect_classes(force_detect_classes->get_text());
}

void EditorBuildProfileManager::_import_profile(const String &p_path) {
	Ref<EditorBuildProfile> profile;
	profile.instantiate();
	Error err = profile->load_from_file(p_path);
	String basefile = p_path.get_file();
	if (err != OK) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("File '%s' format is invalid, import aborted."), basefile));
		return;
	}

	profile_path->set_text(p_path);
	EditorSettings::get_singleton()->set_project_metadata("build_profile", "last_file_path", p_path);

	edited = profile;
	_update_edited_profile();
}

void EditorBuildProfileManager::_export_profile(const String &p_path) {
	ERR_FAIL_COND(edited.is_null());
	Error err = edited->save_to_file(p_path);
	if (err != OK) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("Error saving profile to path: '%s'."), p_path));
	} else {
		profile_path->set_text(p_path);
		EditorSettings::get_singleton()->set_project_metadata("build_profile", "last_file_path", p_path);
	}
}

Ref<EditorBuildProfile> EditorBuildProfileManager::get_current_profile() {
	return edited;
}

EditorBuildProfileManager *EditorBuildProfileManager::singleton = nullptr;

void EditorBuildProfileManager::_bind_methods() {
	ClassDB::bind_method("_update_selected_profile", &EditorBuildProfileManager::_update_edited_profile);
}

EditorBuildProfileManager::EditorBuildProfileManager() {
	VBoxContainer *main_vbc = memnew(VBoxContainer);
	add_child(main_vbc);

	HBoxContainer *path_hbc = memnew(HBoxContainer);
	profile_path = memnew(LineEdit);
	path_hbc->add_child(profile_path);
	profile_path->set_editable(true);
	profile_path->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	profile_actions[ACTION_NEW] = memnew(Button(TTR("New")));
	path_hbc->add_child(profile_actions[ACTION_NEW]);
	profile_actions[ACTION_NEW]->connect(SceneStringName(pressed), callable_mp(this, &EditorBuildProfileManager::_profile_action).bind(ACTION_NEW));

	profile_actions[ACTION_LOAD] = memnew(Button(TTR("Load")));
	path_hbc->add_child(profile_actions[ACTION_LOAD]);
	profile_actions[ACTION_LOAD]->connect(SceneStringName(pressed), callable_mp(this, &EditorBuildProfileManager::_profile_action).bind(ACTION_LOAD));

	profile_actions[ACTION_SAVE] = memnew(Button(TTR("Save")));
	path_hbc->add_child(profile_actions[ACTION_SAVE]);
	profile_actions[ACTION_SAVE]->connect(SceneStringName(pressed), callable_mp(this, &EditorBuildProfileManager::_profile_action).bind(ACTION_SAVE));

	profile_actions[ACTION_SAVE_AS] = memnew(Button(TTR("Save As")));
	path_hbc->add_child(profile_actions[ACTION_SAVE_AS]);
	profile_actions[ACTION_SAVE_AS]->connect(SceneStringName(pressed), callable_mp(this, &EditorBuildProfileManager::_profile_action).bind(ACTION_SAVE_AS));

	main_vbc->add_margin_child(TTR("Profile:"), path_hbc);

	main_vbc->add_child(memnew(HSeparator));

	HBoxContainer *profiles_hbc = memnew(HBoxContainer);

	profile_actions[ACTION_RESET] = memnew(Button(TTR("Reset to Defaults")));
	profiles_hbc->add_child(profile_actions[ACTION_RESET]);
	profile_actions[ACTION_RESET]->connect(SceneStringName(pressed), callable_mp(this, &EditorBuildProfileManager::_profile_action).bind(ACTION_RESET));

	profile_actions[ACTION_DETECT] = memnew(Button(TTR("Detect from Project")));
	profiles_hbc->add_child(profile_actions[ACTION_DETECT]);
	profile_actions[ACTION_DETECT]->connect(SceneStringName(pressed), callable_mp(this, &EditorBuildProfileManager::_profile_action).bind(ACTION_DETECT));

	main_vbc->add_margin_child(TTR("Actions:"), profiles_hbc);

	class_list = memnew(Tree);
	class_list->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	class_list->set_hide_root(true);
	class_list->set_edit_checkbox_cell_only_when_checkbox_is_pressed(true);
	class_list->connect("cell_selected", callable_mp(this, &EditorBuildProfileManager::_class_list_item_selected));
	class_list->connect("item_edited", callable_mp(this, &EditorBuildProfileManager::_class_list_item_edited), CONNECT_DEFERRED);
	class_list->connect("item_collapsed", callable_mp(this, &EditorBuildProfileManager::_class_list_item_collapsed));
	// It will be displayed once the user creates or chooses a profile.
	main_vbc->add_margin_child(TTR("Configure Engine Compilation Profile:"), class_list, true);

	description_bit = memnew(EditorHelpBit);
	description_bit->set_content_height_limits(80 * EDSCALE, 80 * EDSCALE);
	description_bit->connect("request_hide", callable_mp(this, &EditorBuildProfileManager::_hide_requested));
	main_vbc->add_margin_child(TTR("Description:"), description_bit, false);

	confirm_dialog = memnew(ConfirmationDialog);
	add_child(confirm_dialog);
	confirm_dialog->set_title(TTR("Please Confirm:"));
	confirm_dialog->connect(SceneStringName(confirmed), callable_mp(this, &EditorBuildProfileManager::_action_confirm));

	import_profile = memnew(EditorFileDialog);
	add_child(import_profile);
	import_profile->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	import_profile->add_filter("*.build", TTR("Engine Compilation Profile"));
	import_profile->connect("files_selected", callable_mp(this, &EditorBuildProfileManager::_import_profile));
	import_profile->set_title(TTR("Load Profile"));
	import_profile->set_access(EditorFileDialog::ACCESS_FILESYSTEM);

	export_profile = memnew(EditorFileDialog);
	add_child(export_profile);
	export_profile->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	export_profile->add_filter("*.build", TTR("Engine Compilation Profile"));
	export_profile->connect("file_selected", callable_mp(this, &EditorBuildProfileManager::_export_profile));
	export_profile->set_title(TTR("Export Profile"));
	export_profile->set_access(EditorFileDialog::ACCESS_FILESYSTEM);

	force_detect_classes = memnew(LineEdit);
	main_vbc->add_margin_child(TTR("Forced Classes on Detect:"), force_detect_classes);
	force_detect_classes->connect(SceneStringName(text_changed), callable_mp(this, &EditorBuildProfileManager::_force_detect_classes_changed));

	set_title(TTR("Edit Compilation Configuration Profile"));

	singleton = this;
}
