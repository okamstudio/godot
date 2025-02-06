/**************************************************************************/
/*  editor_feature_profile.cpp                                            */
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

#include "editor_feature_profile.h"

#include "core/io/dir_access.h"
#include "core/io/json.h"
#include "editor/editor_node.h"
#include "editor/editor_paths.h"
#include "editor/editor_property_name_processor.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/separator.h"

const char *EditorFeatureProfile::feature_names[FEATURE_MAX] = {
	TTRC("3D Editor"),
	TTRC("Script Editor"),
	TTRC("Asset Library"),
	TTRC("Scene Tree Editing"),
	TTRC("Node Dock"),
	TTRC("FileSystem Dock"),
	TTRC("Import Dock"),
	TTRC("History Dock"),
	TTRC("Game View"),
};

const char *EditorFeatureProfile::feature_descriptions[FEATURE_MAX] = {
	TTRC("Allows to view and edit 3D scenes."),
	TTRC("Allows to edit scripts using the integrated script editor."),
	TTRC("Provides built-in access to the Asset Library."),
	TTRC("Allows editing the node hierarchy in the Scene dock."),
	TTRC("Allows to work with signals and groups of the node selected in the Scene dock."),
	TTRC("Allows to browse the local file system via a dedicated dock."),
	TTRC("Allows to configure import settings for individual assets. Requires the FileSystem dock to function."),
	TTRC("Provides an overview of the editor's and each scene's undo history."),
	TTRC("Provides tools for selecting and debugging nodes at runtime."),
};

const char *EditorFeatureProfile::feature_identifiers[FEATURE_MAX] = {
	"3d",
	"script",
	"asset_lib",
	"scene_tree",
	"node_dock",
	"filesystem_dock",
	"import_dock",
	"history_dock",
	"game",
};

void EditorFeatureProfile::set_disable_class(const StringName &p_class, bool p_disabled) {
	if (p_disabled) {
		disabled_classes.insert(p_class);
	} else {
		disabled_classes.erase(p_class);
	}
}

bool EditorFeatureProfile::is_class_disabled(const StringName &p_class) const {
	if (p_class == StringName()) {
		return false;
	}
	return disabled_classes.has(p_class) || is_class_disabled(ClassDB::get_parent_class_nocheck(p_class));
}

void EditorFeatureProfile::set_disable_class_editor(const StringName &p_class, bool p_disabled) {
	if (p_disabled) {
		disabled_editors.insert(p_class);
	} else {
		disabled_editors.erase(p_class);
	}
}

bool EditorFeatureProfile::is_class_editor_disabled(const StringName &p_class) const {
	if (p_class == StringName()) {
		return false;
	}
	return disabled_editors.has(p_class) || is_class_editor_disabled(ClassDB::get_parent_class_nocheck(p_class));
}

void EditorFeatureProfile::set_disable_class_property(const StringName &p_class, const StringName &p_property, bool p_disabled) {
	if (p_disabled) {
		if (!disabled_properties.has(p_class)) {
			disabled_properties[p_class] = HashSet<StringName>();
		}

		disabled_properties[p_class].insert(p_property);
	} else {
		ERR_FAIL_COND(!disabled_properties.has(p_class));
		disabled_properties[p_class].erase(p_property);
		if (disabled_properties[p_class].is_empty()) {
			disabled_properties.erase(p_class);
		}
	}
}

bool EditorFeatureProfile::is_class_property_disabled(const StringName &p_class, const StringName &p_property) const {
	if (!disabled_properties.has(p_class)) {
		return false;
	}

	if (!disabled_properties[p_class].has(p_property)) {
		return false;
	}

	return true;
}

bool EditorFeatureProfile::has_class_properties_disabled(const StringName &p_class) const {
	return disabled_properties.has(p_class);
}

void EditorFeatureProfile::set_item_collapsed(const StringName &p_class, bool p_collapsed) {
	if (p_collapsed) {
		collapsed_classes.insert(p_class);
	} else {
		collapsed_classes.erase(p_class);
	}
}

bool EditorFeatureProfile::is_item_collapsed(const StringName &p_class) const {
	return collapsed_classes.has(p_class);
}

void EditorFeatureProfile::set_disable_feature(Feature p_feature, bool p_disable) {
	ERR_FAIL_INDEX(p_feature, FEATURE_MAX);
	features_disabled[p_feature] = p_disable;
}

bool EditorFeatureProfile::is_feature_disabled(Feature p_feature) const {
	ERR_FAIL_INDEX_V(p_feature, FEATURE_MAX, false);
	return features_disabled[p_feature];
}

String EditorFeatureProfile::get_feature_name(Feature p_feature) {
	ERR_FAIL_INDEX_V(p_feature, FEATURE_MAX, String());
	return feature_names[p_feature];
}

String EditorFeatureProfile::get_feature_description(Feature p_feature) {
	ERR_FAIL_INDEX_V(p_feature, FEATURE_MAX, String());
	return feature_descriptions[p_feature];
}

Error EditorFeatureProfile::save_to_file(const String &p_path) {
	Dictionary data;
	data["type"] = "feature_profile";
	Array dis_classes;
	for (const StringName &E : disabled_classes) {
		dis_classes.push_back(String(E));
	}
	dis_classes.sort();
	data["disabled_classes"] = dis_classes;

	Array dis_editors;
	for (const StringName &E : disabled_editors) {
		dis_editors.push_back(String(E));
	}
	dis_editors.sort();
	data["disabled_editors"] = dis_editors;

	Array dis_props;

	for (KeyValue<StringName, HashSet<StringName>> &E : disabled_properties) {
		for (const StringName &F : E.value) {
			dis_props.push_back(String(E.key) + ":" + String(F));
		}
	}

	data["disabled_properties"] = dis_props;

	Array dis_features;
	for (int i = 0; i < FEATURE_MAX; i++) {
		if (features_disabled[i]) {
			dis_features.push_back(feature_identifiers[i]);
		}
	}

	data["disabled_features"] = dis_features;

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_CANT_CREATE, "Cannot create file '" + p_path + "'.");

	JSON json;
	String text = json.stringify(data, "\t");
	f->store_string(text);
	return OK;
}

Error EditorFeatureProfile::load_from_file(const String &p_path) {
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

	if (!data.has("type") || String(data["type"]) != "feature_profile") {
		ERR_PRINT("Error parsing '" + p_path + "', it's not a feature profile.");
		return ERR_PARSE_ERROR;
	}

	disabled_classes.clear();

	if (data.has("disabled_classes")) {
		Array disabled_classes_arr = data["disabled_classes"];
		for (int i = 0; i < disabled_classes_arr.size(); i++) {
			disabled_classes.insert(disabled_classes_arr[i]);
		}
	}

	disabled_editors.clear();

	if (data.has("disabled_editors")) {
		Array disabled_editors_arr = data["disabled_editors"];
		for (int i = 0; i < disabled_editors_arr.size(); i++) {
			disabled_editors.insert(disabled_editors_arr[i]);
		}
	}

	disabled_properties.clear();

	if (data.has("disabled_properties")) {
		Array disabled_properties_arr = data["disabled_properties"];
		for (int i = 0; i < disabled_properties_arr.size(); i++) {
			String s = disabled_properties_arr[i];
			set_disable_class_property(s.get_slicec(':', 0), s.get_slicec(':', 1), true);
		}
	}

	if (data.has("disabled_features")) {
		Array disabled_features_arr = data["disabled_features"];
		for (int i = 0; i < FEATURE_MAX; i++) {
			bool found = false;
			String f = feature_identifiers[i];
			for (int j = 0; j < disabled_features_arr.size(); j++) {
				String fd = disabled_features_arr[j];
				if (fd == f) {
					found = true;
					break;
				}
			}

			features_disabled[i] = found;
		}
	}

	return OK;
}

void EditorFeatureProfile::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_disable_class", "class_name", "disable"), &EditorFeatureProfile::set_disable_class);
	ClassDB::bind_method(D_METHOD("is_class_disabled", "class_name"), &EditorFeatureProfile::is_class_disabled);

	ClassDB::bind_method(D_METHOD("set_disable_class_editor", "class_name", "disable"), &EditorFeatureProfile::set_disable_class_editor);
	ClassDB::bind_method(D_METHOD("is_class_editor_disabled", "class_name"), &EditorFeatureProfile::is_class_editor_disabled);

	ClassDB::bind_method(D_METHOD("set_disable_class_property", "class_name", "property", "disable"), &EditorFeatureProfile::set_disable_class_property);
	ClassDB::bind_method(D_METHOD("is_class_property_disabled", "class_name", "property"), &EditorFeatureProfile::is_class_property_disabled);

	ClassDB::bind_method(D_METHOD("set_disable_feature", "feature", "disable"), &EditorFeatureProfile::set_disable_feature);
	ClassDB::bind_method(D_METHOD("is_feature_disabled", "feature"), &EditorFeatureProfile::is_feature_disabled);

	ClassDB::bind_method(D_METHOD("get_feature_name", "feature"), &EditorFeatureProfile::_get_feature_name);

	ClassDB::bind_method(D_METHOD("save_to_file", "path"), &EditorFeatureProfile::save_to_file);
	ClassDB::bind_method(D_METHOD("load_from_file", "path"), &EditorFeatureProfile::load_from_file);

	BIND_ENUM_CONSTANT(FEATURE_3D);
	BIND_ENUM_CONSTANT(FEATURE_SCRIPT);
	BIND_ENUM_CONSTANT(FEATURE_ASSET_LIB);
	BIND_ENUM_CONSTANT(FEATURE_SCENE_TREE);
	BIND_ENUM_CONSTANT(FEATURE_NODE_DOCK);
	BIND_ENUM_CONSTANT(FEATURE_FILESYSTEM_DOCK);
	BIND_ENUM_CONSTANT(FEATURE_IMPORT_DOCK);
	BIND_ENUM_CONSTANT(FEATURE_HISTORY_DOCK);
	BIND_ENUM_CONSTANT(FEATURE_GAME);
	BIND_ENUM_CONSTANT(FEATURE_MAX);
}

EditorFeatureProfile::EditorFeatureProfile() {
	for (int i = 0; i < FEATURE_MAX; i++) {
		features_disabled[i] = false;
	}
}

//////////////////////////

void EditorFeatureProfileManager::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			current_profile = EDITOR_GET("_default_feature_profile");
			if (!current_profile.is_empty()) {
				current.instantiate();
				Error err = current->load_from_file(EditorPaths::get_singleton()->get_feature_profiles_dir().path_join(current_profile + ".profile"));
				if (err != OK) {
					ERR_PRINT("Error loading default feature profile: " + current_profile);
					current_profile = String();
					current.unref();
				}
			}
			_update_profile_list(current_profile);
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			// Make sure that the icons are correctly adjusted if the theme's lightness was switched.
			_update_selected_profile();
		} break;
	}
}

String EditorFeatureProfileManager::_get_selected_profile() {
	int idx = profile_list->get_selected();
	if (idx < 0) {
		return String();
	}

	return profile_list->get_item_metadata(idx);
}

void EditorFeatureProfileManager::_update_profile_list(const String &p_select_profile) {
	String selected_profile;
	if (p_select_profile.is_empty()) { //default, keep
		if (profile_list->get_selected() >= 0) {
			selected_profile = profile_list->get_item_metadata(profile_list->get_selected());
			if (!FileAccess::exists(EditorPaths::get_singleton()->get_feature_profiles_dir().path_join(selected_profile + ".profile"))) {
				selected_profile = String(); //does not exist
			}
		}
	} else {
		selected_profile = p_select_profile;
	}

	Vector<String> profiles;
	Ref<DirAccess> d = DirAccess::open(EditorPaths::get_singleton()->get_feature_profiles_dir());
	ERR_FAIL_COND_MSG(d.is_null(), "Cannot open directory '" + EditorPaths::get_singleton()->get_feature_profiles_dir() + "'.");

	d->list_dir_begin();
	while (true) {
		String f = d->get_next();
		if (f.is_empty()) {
			break;
		}

		if (!d->current_is_dir()) {
			int last_pos = f.rfind(".profile");
			if (last_pos != -1) {
				profiles.push_back(f.substr(0, last_pos));
			}
		}
	}

	profiles.sort();

	profile_list->clear();

	for (int i = 0; i < profiles.size(); i++) {
		String name = profiles[i];

		if (i == 0 && selected_profile.is_empty()) {
			selected_profile = name;
		}

		if (name == current_profile) {
			name += " " + TTR("(current)");
		}
		profile_list->add_item(name);
		int index = profile_list->get_item_count() - 1;
		profile_list->set_item_metadata(index, profiles[i]);
		if (profiles[i] == selected_profile) {
			profile_list->select(index);
		}
	}

	class_list_vbc->set_visible(!selected_profile.is_empty());
	property_list_vbc->set_visible(!selected_profile.is_empty());
	no_profile_selected_help->set_visible(selected_profile.is_empty());
	profile_actions[PROFILE_CLEAR]->set_disabled(current_profile.is_empty());
	profile_actions[PROFILE_ERASE]->set_disabled(selected_profile.is_empty());
	profile_actions[PROFILE_EXPORT]->set_disabled(selected_profile.is_empty());
	profile_actions[PROFILE_SET]->set_disabled(selected_profile.is_empty());

	current_profile_name->set_text(!current_profile.is_empty() ? current_profile : TTR("(none)"));

	_update_selected_profile();
}

void EditorFeatureProfileManager::_profile_action(int p_action) {
	switch (p_action) {
		case PROFILE_CLEAR: {
			set_current_profile("", false);
		} break;
		case PROFILE_SET: {
			String selected = _get_selected_profile();
			ERR_FAIL_COND(selected.is_empty());
			if (selected == current_profile) {
				return; // Nothing to do here.
			}
			set_current_profile(selected, false);
		} break;
		case PROFILE_IMPORT: {
			import_profiles->popup_file_dialog();
		} break;
		case PROFILE_EXPORT: {
			export_profile->popup_file_dialog();
			export_profile->set_current_file(_get_selected_profile() + ".profile");
		} break;
		case PROFILE_NEW: {
			new_profile_dialog->popup_centered(Size2(240, 60) * EDSCALE);
			new_profile_name->clear();
			new_profile_name->grab_focus();
		} break;
		case PROFILE_ERASE: {
			String selected = _get_selected_profile();
			ERR_FAIL_COND(selected.is_empty());

			erase_profile_dialog->set_text(vformat(TTR("Remove currently selected profile, '%s'? Cannot be undone."), selected));
			erase_profile_dialog->popup_centered(Size2(240, 60) * EDSCALE);
		} break;
	}
}

void EditorFeatureProfileManager::_erase_selected_profile() {
	String selected = _get_selected_profile();
	ERR_FAIL_COND(selected.is_empty());
	Ref<DirAccess> da = DirAccess::open(EditorPaths::get_singleton()->get_feature_profiles_dir());
	ERR_FAIL_COND_MSG(da.is_null(), "Cannot open directory '" + EditorPaths::get_singleton()->get_feature_profiles_dir() + "'.");

	da->remove(selected + ".profile");
	if (selected == current_profile) {
		_profile_action(PROFILE_CLEAR);
	} else {
		_update_profile_list();
	}
}

void EditorFeatureProfileManager::_create_new_profile() {
	String name = new_profile_name->get_text().strip_edges();
	if (!name.is_valid_filename() || name.contains_char('.')) {
		EditorNode::get_singleton()->show_warning(TTR("Profile must be a valid filename and must not contain '.'"));
		return;
	}
	String file = EditorPaths::get_singleton()->get_feature_profiles_dir().path_join(name + ".profile");
	if (FileAccess::exists(file)) {
		EditorNode::get_singleton()->show_warning(TTR("Profile with this name already exists."));
		return;
	}

	Ref<EditorFeatureProfile> new_profile;
	new_profile.instantiate();
	new_profile->save_to_file(file);

	_update_profile_list(name);
	// The newly created profile is the first one, make it the current profile automatically.
	if (profile_list->get_item_count() == 1) {
		_profile_action(PROFILE_SET);
	}
}

void EditorFeatureProfileManager::_profile_selected(int p_what) {
	_update_selected_profile();
}

void EditorFeatureProfileManager::_hide_requested() {
	_cancel_pressed(); // From AcceptDialog.
}

void EditorFeatureProfileManager::_fill_classes_from(TreeItem *p_parent, const String &p_class, const String &p_selected, int p_class_insert_index) {
	TreeItem *class_item = class_list->create_item(p_parent, p_class_insert_index);
	class_item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
	class_item->set_icon(0, EditorNode::get_singleton()->get_class_icon(p_class));
	String text = p_class;

	bool disabled = edited->is_class_disabled(p_class);
	bool disabled_editor = edited->is_class_editor_disabled(p_class);
	bool disabled_properties = edited->has_class_properties_disabled(p_class);
	if (disabled) {
		class_item->set_custom_color(0, class_list->get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
	} else if (disabled_editor && disabled_properties) {
		text += " " + TTR("(Editor Disabled, Properties Disabled)");
	} else if (disabled_properties) {
		text += " " + TTR("(Properties Disabled)");
	} else if (disabled_editor) {
		text += " " + TTR("(Editor Disabled)");
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

	class_item->set_checked(0, true); // If it's not disabled, it's checked.

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

void EditorFeatureProfileManager::_class_list_item_selected() {
	if (updating_features) {
		return;
	}

	property_list->clear();

	TreeItem *item = class_list->get_selected();
	if (!item) {
		return;
	}

	Variant md = item->get_metadata(0);
	if (md.is_string()) {
		description_bit->parse_symbol("class|" + md.operator String() + "|");
	} else if (md.get_type() == Variant::INT) {
		String feature_description = EditorFeatureProfile::get_feature_description(EditorFeatureProfile::Feature((int)md));
		description_bit->set_custom_text(TTR(item->get_text(0)), String(), TTRGET(feature_description));
		return;
	} else {
		return;
	}

	String class_name = md;
	if (edited->is_class_disabled(class_name)) {
		return;
	}

	updating_features = true;
	TreeItem *root = property_list->create_item();
	TreeItem *options = property_list->create_item(root);
	options->set_text(0, TTR("Class Options:"));

	{
		TreeItem *option = property_list->create_item(options);
		option->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
		option->set_editable(0, true);
		option->set_selectable(0, true);
		option->set_checked(0, !edited->is_class_editor_disabled(class_name));
		option->set_text(0, TTR("Enable Contextual Editor"));
		option->set_metadata(0, CLASS_OPTION_DISABLE_EDITOR);
	}

	List<PropertyInfo> props;
	ClassDB::get_property_list(class_name, &props, true);

	bool has_editor_props = false;
	for (const PropertyInfo &E : props) {
		if (E.usage & PROPERTY_USAGE_EDITOR) {
			has_editor_props = true;
			break;
		}
	}

	if (has_editor_props) {
		TreeItem *properties = property_list->create_item(root);
		properties->set_text(0, TTR("Class Properties:"));

		const EditorPropertyNameProcessor::Style text_style = EditorPropertyNameProcessor::get_settings_style();
		const EditorPropertyNameProcessor::Style tooltip_style = EditorPropertyNameProcessor::get_tooltip_style(text_style);

		for (const PropertyInfo &E : props) {
			String name = E.name;
			if (!(E.usage & PROPERTY_USAGE_EDITOR)) {
				continue;
			}
			const String text = EditorPropertyNameProcessor::get_singleton()->process_name(name, text_style, name, class_name);
			const String tooltip = EditorPropertyNameProcessor::get_singleton()->process_name(name, tooltip_style, name, class_name);

			TreeItem *property = property_list->create_item(properties);
			property->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
			property->set_editable(0, true);
			property->set_selectable(0, true);
			property->set_checked(0, !edited->is_class_property_disabled(class_name, name));
			property->set_text(0, text);
			property->set_tooltip_text(0, tooltip);
			property->set_metadata(0, name);
			String icon_type = Variant::get_type_name(E.type);
			property->set_icon(0, EditorNode::get_singleton()->get_class_icon(icon_type));
		}
	}

	updating_features = false;
}

void EditorFeatureProfileManager::_class_list_item_edited() {
	if (updating_features) {
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
		_save_and_update();
		_update_profile_tree_from(item);
	} else if (md.get_type() == Variant::INT) {
		int feature_selected = md;
		edited->set_disable_feature(EditorFeatureProfile::Feature(feature_selected), !checked);
		_save_and_update();
	}
}

void EditorFeatureProfileManager::_class_list_item_collapsed(Object *p_item) {
	if (updating_features) {
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

void EditorFeatureProfileManager::_property_item_edited() {
	if (updating_features) {
		return;
	}

	TreeItem *class_item = class_list->get_selected();
	if (!class_item) {
		return;
	}

	Variant md = class_item->get_metadata(0);
	if (!md.is_string()) {
		return;
	}

	String class_name = md;

	TreeItem *item = property_list->get_edited();
	if (!item) {
		return;
	}
	bool checked = item->is_checked(0);

	md = item->get_metadata(0);
	if (md.is_string()) {
		String property_selected = md;
		edited->set_disable_class_property(class_name, property_selected, !checked);
		_save_and_update();
		_update_profile_tree_from(class_list->get_selected());
	} else if (md.get_type() == Variant::INT) {
		int feature_selected = md;
		switch (feature_selected) {
			case CLASS_OPTION_DISABLE_EDITOR: {
				edited->set_disable_class_editor(class_name, !checked);
				_save_and_update();
				_update_profile_tree_from(class_list->get_selected());
			} break;
		}
	}
}

void EditorFeatureProfileManager::_update_profile_tree_from(TreeItem *p_edited) {
	String edited_class = p_edited->get_metadata(0);

	TreeItem *edited_parent = p_edited->get_parent();
	int class_insert_index = p_edited->get_index();
	p_edited->get_parent()->remove_child(p_edited);

	_fill_classes_from(edited_parent, edited_class, edited_class, class_insert_index);
}

void EditorFeatureProfileManager::_update_selected_profile() {
	String class_selected;
	int feature_selected = -1;

	if (class_list->get_selected()) {
		Variant md = class_list->get_selected()->get_metadata(0);
		if (md.is_string()) {
			class_selected = md;
		} else if (md.get_type() == Variant::INT) {
			feature_selected = md;
		}
	}

	class_list->clear();

	String profile = _get_selected_profile();
	profile_actions[PROFILE_SET]->set_disabled(profile == current_profile);

	if (profile.is_empty()) { //nothing selected, nothing edited
		property_list->clear();
		edited.unref();
		return;
	}

	if (profile == current_profile) {
		edited = current; //reuse current profile (which is what editor uses)
		ERR_FAIL_COND(current.is_null()); //nothing selected, current should never be null
	} else {
		//reload edited, if different from current
		edited.instantiate();
		Error err = edited->load_from_file(EditorPaths::get_singleton()->get_feature_profiles_dir().path_join(profile + ".profile"));
		ERR_FAIL_COND_MSG(err != OK, "Error when loading editor feature profile from file '" + EditorPaths::get_singleton()->get_feature_profiles_dir().path_join(profile + ".profile") + "'.");
	}

	updating_features = true;

	TreeItem *root = class_list->create_item();

	TreeItem *features = class_list->create_item(root);
	TreeItem *last_feature = nullptr;
	features->set_text(0, TTR("Main Features:"));
	for (int i = 0; i < EditorFeatureProfile::FEATURE_MAX; i++) {
		TreeItem *feature;
		if (i == EditorFeatureProfile::FEATURE_IMPORT_DOCK) {
			feature = class_list->create_item(last_feature);
		} else {
			feature = class_list->create_item(features);
			last_feature = feature;
		}
		feature->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
		feature->set_text(0, TTRGET(EditorFeatureProfile::get_feature_name(EditorFeatureProfile::Feature(i))));
		feature->set_selectable(0, true);
		feature->set_editable(0, true);
		feature->set_metadata(0, i);
		if (!edited->is_feature_disabled(EditorFeatureProfile::Feature(i))) {
			feature->set_checked(0, true);
		}

		if (i == feature_selected) {
			feature->select(0);
		}
	}

	TreeItem *classes = class_list->create_item(root);
	classes->set_text(0, TTR("Nodes and Classes:"));

	_fill_classes_from(classes, "Node", class_selected);
	_fill_classes_from(classes, "Resource", class_selected);

	updating_features = false;

	_class_list_item_selected();
}

void EditorFeatureProfileManager::_import_profiles(const Vector<String> &p_paths) {
	//test it first
	for (int i = 0; i < p_paths.size(); i++) {
		Ref<EditorFeatureProfile> profile;
		profile.instantiate();
		Error err = profile->load_from_file(p_paths[i]);
		String basefile = p_paths[i].get_file();
		if (err != OK) {
			EditorNode::get_singleton()->show_warning(vformat(TTR("File '%s' format is invalid, import aborted."), basefile));
			return;
		}

		String dst_file = EditorPaths::get_singleton()->get_feature_profiles_dir().path_join(basefile);

		if (FileAccess::exists(dst_file)) {
			EditorNode::get_singleton()->show_warning(vformat(TTR("Profile '%s' already exists. Remove it first before importing, import aborted."), basefile.get_basename()));
			return;
		}
	}

	//do it second
	for (int i = 0; i < p_paths.size(); i++) {
		Ref<EditorFeatureProfile> profile;
		profile.instantiate();
		Error err = profile->load_from_file(p_paths[i]);
		ERR_CONTINUE(err != OK);
		String basefile = p_paths[i].get_file();
		String dst_file = EditorPaths::get_singleton()->get_feature_profiles_dir().path_join(basefile);
		profile->save_to_file(dst_file);
	}

	_update_profile_list();
	// The newly imported profile is the first one, make it the current profile automatically.
	if (profile_list->get_item_count() == 1) {
		_profile_action(PROFILE_SET);
	}
}

void EditorFeatureProfileManager::_export_profile(const String &p_path) {
	ERR_FAIL_COND(edited.is_null());
	Error err = edited->save_to_file(p_path);
	if (err != OK) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("Error saving profile to path: '%s'."), p_path));
	}
}

void EditorFeatureProfileManager::_save_and_update() {
	String edited_path = _get_selected_profile();
	ERR_FAIL_COND(edited_path.is_empty());
	ERR_FAIL_COND(edited.is_null());

	edited->save_to_file(EditorPaths::get_singleton()->get_feature_profiles_dir().path_join(edited_path + ".profile"));

	if (edited == current) {
		update_timer->start();
	}
}

void EditorFeatureProfileManager::_emit_current_profile_changed() {
	emit_signal(SNAME("current_feature_profile_changed"));
}

void EditorFeatureProfileManager::notify_changed() {
	_emit_current_profile_changed();
}

Ref<EditorFeatureProfile> EditorFeatureProfileManager::get_current_profile() {
	return current;
}

String EditorFeatureProfileManager::get_current_profile_name() const {
	return current_profile;
}

void EditorFeatureProfileManager::set_current_profile(const String &p_profile_name, bool p_validate_profile) {
	if (p_validate_profile && !p_profile_name.is_empty()) {
		// Profile may not exist.
		Ref<DirAccess> da = DirAccess::open(EditorPaths::get_singleton()->get_feature_profiles_dir());
		ERR_FAIL_COND_MSG(da.is_null(), "Cannot open directory '" + EditorPaths::get_singleton()->get_feature_profiles_dir() + "'.");
		ERR_FAIL_COND_MSG(!da->file_exists(p_profile_name + ".profile"), "Feature profile '" + p_profile_name + "' does not exist.");

		// Change profile selection to emulate the UI interaction. Otherwise, the wrong profile would get activated.
		// FIXME: Ideally, _update_selected_profile() should not rely on the user interface state to function properly.
		for (int i = 0; i < profile_list->get_item_count(); i++) {
			if (profile_list->get_item_metadata(i) == p_profile_name) {
				profile_list->select(i);
				break;
			}
		}
		_update_selected_profile();
	}

	// Store in editor settings.
	EditorSettings::get_singleton()->set("_default_feature_profile", p_profile_name);
	EditorSettings::get_singleton()->save();

	current_profile = p_profile_name;
	if (p_profile_name.is_empty()) {
		current.unref();
	} else {
		current = edited;
	}
	_update_profile_list();
	_emit_current_profile_changed();
}

EditorFeatureProfileManager *EditorFeatureProfileManager::singleton = nullptr;

void EditorFeatureProfileManager::_bind_methods() {
	ADD_SIGNAL(MethodInfo("current_feature_profile_changed"));
}

EditorFeatureProfileManager::EditorFeatureProfileManager() {
	VBoxContainer *main_vbc = memnew(VBoxContainer);
	add_child(main_vbc);

	HBoxContainer *name_hbc = memnew(HBoxContainer);
	current_profile_name = memnew(LineEdit);
	name_hbc->add_child(current_profile_name);
	current_profile_name->set_text(TTR("(none)"));
	current_profile_name->set_editable(false);
	current_profile_name->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	profile_actions[PROFILE_CLEAR] = memnew(Button(TTR("Reset to Default")));
	name_hbc->add_child(profile_actions[PROFILE_CLEAR]);
	profile_actions[PROFILE_CLEAR]->set_disabled(true);
	profile_actions[PROFILE_CLEAR]->connect(SceneStringName(pressed), callable_mp(this, &EditorFeatureProfileManager::_profile_action).bind(PROFILE_CLEAR));

	main_vbc->add_margin_child(TTR("Current Profile:"), name_hbc);

	main_vbc->add_child(memnew(HSeparator));

	HBoxContainer *profiles_hbc = memnew(HBoxContainer);
	profile_list = memnew(OptionButton);
	profile_list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	profile_list->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	profiles_hbc->add_child(profile_list);
	profile_list->connect(SceneStringName(item_selected), callable_mp(this, &EditorFeatureProfileManager::_profile_selected));

	profile_actions[PROFILE_NEW] = memnew(Button(TTR("Create Profile")));
	profiles_hbc->add_child(profile_actions[PROFILE_NEW]);
	profile_actions[PROFILE_NEW]->connect(SceneStringName(pressed), callable_mp(this, &EditorFeatureProfileManager::_profile_action).bind(PROFILE_NEW));

	profile_actions[PROFILE_ERASE] = memnew(Button(TTR("Remove Profile")));
	profiles_hbc->add_child(profile_actions[PROFILE_ERASE]);
	profile_actions[PROFILE_ERASE]->set_disabled(true);
	profile_actions[PROFILE_ERASE]->connect(SceneStringName(pressed), callable_mp(this, &EditorFeatureProfileManager::_profile_action).bind(PROFILE_ERASE));

	main_vbc->add_margin_child(TTR("Available Profiles:"), profiles_hbc);

	HBoxContainer *current_profile_hbc = memnew(HBoxContainer);

	profile_actions[PROFILE_SET] = memnew(Button(TTR("Make Current")));
	current_profile_hbc->add_child(profile_actions[PROFILE_SET]);
	profile_actions[PROFILE_SET]->set_disabled(true);
	profile_actions[PROFILE_SET]->connect(SceneStringName(pressed), callable_mp(this, &EditorFeatureProfileManager::_profile_action).bind(PROFILE_SET));

	current_profile_hbc->add_child(memnew(VSeparator));

	profile_actions[PROFILE_IMPORT] = memnew(Button(TTR("Import")));
	current_profile_hbc->add_child(profile_actions[PROFILE_IMPORT]);
	profile_actions[PROFILE_IMPORT]->connect(SceneStringName(pressed), callable_mp(this, &EditorFeatureProfileManager::_profile_action).bind(PROFILE_IMPORT));

	profile_actions[PROFILE_EXPORT] = memnew(Button(TTR("Export")));
	current_profile_hbc->add_child(profile_actions[PROFILE_EXPORT]);
	profile_actions[PROFILE_EXPORT]->set_disabled(true);
	profile_actions[PROFILE_EXPORT]->connect(SceneStringName(pressed), callable_mp(this, &EditorFeatureProfileManager::_profile_action).bind(PROFILE_EXPORT));

	main_vbc->add_child(current_profile_hbc);

	h_split = memnew(HSplitContainer);
	h_split->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main_vbc->add_child(h_split);

	class_list_vbc = memnew(VBoxContainer);
	h_split->add_child(class_list_vbc);
	class_list_vbc->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	class_list = memnew(Tree);
	class_list->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	class_list_vbc->add_margin_child(TTR("Configure Selected Profile:"), class_list, true);
	class_list->set_hide_root(true);
	class_list->set_edit_checkbox_cell_only_when_checkbox_is_pressed(true);
	class_list->connect("cell_selected", callable_mp(this, &EditorFeatureProfileManager::_class_list_item_selected));
	class_list->connect("item_edited", callable_mp(this, &EditorFeatureProfileManager::_class_list_item_edited), CONNECT_DEFERRED);
	class_list->connect("item_collapsed", callable_mp(this, &EditorFeatureProfileManager::_class_list_item_collapsed));
	class_list->set_theme_type_variation("TreeSecondary");
	// It will be displayed once the user creates or chooses a profile.
	class_list_vbc->hide();

	property_list_vbc = memnew(VBoxContainer);
	h_split->add_child(property_list_vbc);
	property_list_vbc->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	description_bit = memnew(EditorHelpBit);
	description_bit->set_content_height_limits(80 * EDSCALE, 80 * EDSCALE);
	description_bit->connect("request_hide", callable_mp(this, &EditorFeatureProfileManager::_hide_requested));
	property_list_vbc->add_margin_child(TTR("Description:"), description_bit, false);

	property_list = memnew(Tree);
	property_list_vbc->add_margin_child(TTR("Extra Options:"), property_list, true);
	property_list->set_hide_root(true);
	property_list->set_hide_folding(true);
	property_list->set_edit_checkbox_cell_only_when_checkbox_is_pressed(true);
	property_list->connect("item_edited", callable_mp(this, &EditorFeatureProfileManager::_property_item_edited), CONNECT_DEFERRED);
	// It will be displayed once the user creates or chooses a profile.
	property_list_vbc->hide();

	no_profile_selected_help = memnew(Label(TTR("Create or import a profile to edit available classes and properties.")));
	// Add some spacing above the help label.
	Ref<StyleBoxEmpty> sb = memnew(StyleBoxEmpty);
	sb->set_content_margin(SIDE_TOP, 20 * EDSCALE);
	no_profile_selected_help->add_theme_style_override(CoreStringName(normal), sb);
	no_profile_selected_help->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	no_profile_selected_help->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	h_split->add_child(no_profile_selected_help);

	new_profile_dialog = memnew(ConfirmationDialog);
	new_profile_dialog->set_title(TTR("Create Profile"));
	VBoxContainer *new_profile_vb = memnew(VBoxContainer);
	new_profile_dialog->add_child(new_profile_vb);
	Label *new_profile_label = memnew(Label);
	new_profile_label->set_text(TTR("New profile name:"));
	new_profile_vb->add_child(new_profile_label);
	new_profile_name = memnew(LineEdit);
	new_profile_vb->add_child(new_profile_name);
	new_profile_name->set_custom_minimum_size(Size2(300 * EDSCALE, 1));
	add_child(new_profile_dialog);
	new_profile_dialog->connect(SceneStringName(confirmed), callable_mp(this, &EditorFeatureProfileManager::_create_new_profile));
	new_profile_dialog->register_text_enter(new_profile_name);
	new_profile_dialog->set_ok_button_text(TTR("Create"));

	erase_profile_dialog = memnew(ConfirmationDialog);
	add_child(erase_profile_dialog);
	erase_profile_dialog->set_title(TTR("Remove Profile"));
	erase_profile_dialog->connect(SceneStringName(confirmed), callable_mp(this, &EditorFeatureProfileManager::_erase_selected_profile));

	import_profiles = memnew(EditorFileDialog);
	add_child(import_profiles);
	import_profiles->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILES);
	import_profiles->add_filter("*.profile", TTR("Godot Feature Profile"));
	import_profiles->connect("files_selected", callable_mp(this, &EditorFeatureProfileManager::_import_profiles));
	import_profiles->set_title(TTR("Import Profile(s)"));
	import_profiles->set_access(EditorFileDialog::ACCESS_FILESYSTEM);

	export_profile = memnew(EditorFileDialog);
	add_child(export_profile);
	export_profile->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	export_profile->add_filter("*.profile", TTR("Godot Feature Profile"));
	export_profile->connect("file_selected", callable_mp(this, &EditorFeatureProfileManager::_export_profile));
	export_profile->set_title(TTR("Export Profile"));
	export_profile->set_access(EditorFileDialog::ACCESS_FILESYSTEM);

	set_title(TTR("Manage Editor Feature Profiles"));
	EDITOR_DEF("_default_feature_profile", "");

	update_timer = memnew(Timer);
	update_timer->set_wait_time(1); //wait a second before updating editor
	add_child(update_timer);
	update_timer->connect("timeout", callable_mp(this, &EditorFeatureProfileManager::_emit_current_profile_changed));
	update_timer->set_one_shot(true);

	singleton = this;
}
