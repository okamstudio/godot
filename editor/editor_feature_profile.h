/**************************************************************************/
/*  editor_feature_profile.h                                              */
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

#pragma once

#include "core/object/ref_counted.h"
#include "editor/editor_help.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/option_button.h"
#include "scene/gui/split_container.h"
#include "scene/gui/tree.h"

class EditorFileDialog;

class EditorFeatureProfile : public RefCounted {
	GDCLASS(EditorFeatureProfile, RefCounted);

public:
	enum Feature {
		FEATURE_3D,
		FEATURE_SCRIPT,
		FEATURE_ASSET_LIB,
		FEATURE_SCENE_TREE,
		FEATURE_NODE_DOCK,
		FEATURE_FILESYSTEM_DOCK,
		FEATURE_IMPORT_DOCK,
		FEATURE_HISTORY_DOCK,
		FEATURE_GAME,
		FEATURE_MAX
	};

private:
	HashSet<StringName> disabled_classes;
	HashSet<StringName> disabled_editors;
	HashMap<StringName, HashSet<StringName>> disabled_properties;

	HashSet<StringName> collapsed_classes;

	bool features_disabled[FEATURE_MAX];
	static const char *feature_names[FEATURE_MAX];
	static const char *feature_descriptions[FEATURE_MAX];
	static const char *feature_identifiers[FEATURE_MAX];

	String _get_feature_name(Feature p_feature) { return get_feature_name(p_feature); }

protected:
	static void _bind_methods();

public:
	void set_disable_class(const StringName &p_class, bool p_disabled);
	bool is_class_disabled(const StringName &p_class) const;

	void set_disable_class_editor(const StringName &p_class, bool p_disabled);
	bool is_class_editor_disabled(const StringName &p_class) const;

	void set_disable_class_property(const StringName &p_class, const StringName &p_property, bool p_disabled);
	bool is_class_property_disabled(const StringName &p_class, const StringName &p_property) const;
	bool has_class_properties_disabled(const StringName &p_class) const;

	void set_item_collapsed(const StringName &p_class, bool p_collapsed);
	bool is_item_collapsed(const StringName &p_class) const;

	void set_disable_feature(Feature p_feature, bool p_disable);
	bool is_feature_disabled(Feature p_feature) const;

	Error save_to_file(const String &p_path);
	Error load_from_file(const String &p_path);

	static String get_feature_name(Feature p_feature);
	static String get_feature_description(Feature p_feature);

	EditorFeatureProfile();
};

VARIANT_ENUM_CAST(EditorFeatureProfile::Feature)

class EditorFeatureProfileManager : public AcceptDialog {
	GDCLASS(EditorFeatureProfileManager, AcceptDialog);

	enum Action {
		PROFILE_CLEAR,
		PROFILE_SET,
		PROFILE_IMPORT,
		PROFILE_EXPORT,
		PROFILE_NEW,
		PROFILE_ERASE,
		PROFILE_MAX
	};

	enum ClassOptions {
		CLASS_OPTION_DISABLE_EDITOR
	};

	ConfirmationDialog *erase_profile_dialog = nullptr;
	ConfirmationDialog *new_profile_dialog = nullptr;
	LineEdit *new_profile_name = nullptr;

	LineEdit *current_profile_name = nullptr;
	OptionButton *profile_list = nullptr;
	Button *profile_actions[PROFILE_MAX];

	HSplitContainer *h_split = nullptr;

	VBoxContainer *class_list_vbc = nullptr;
	Tree *class_list = nullptr;
	VBoxContainer *property_list_vbc = nullptr;
	Tree *property_list = nullptr;
	EditorHelpBit *description_bit = nullptr;
	Label *no_profile_selected_help = nullptr;

	EditorFileDialog *import_profiles = nullptr;
	EditorFileDialog *export_profile = nullptr;

	void _profile_action(int p_action);
	void _profile_selected(int p_what);
	void _hide_requested();

	String current_profile;
	void _update_profile_list(const String &p_select_profile = String());
	void _update_selected_profile();
	void _update_profile_tree_from(TreeItem *p_edited);
	void _fill_classes_from(TreeItem *p_parent, const String &p_class, const String &p_selected, int p_class_insert_index = -1);

	Ref<EditorFeatureProfile> current;
	Ref<EditorFeatureProfile> edited;

	void _erase_selected_profile();
	void _create_new_profile();
	String _get_selected_profile();

	void _import_profiles(const Vector<String> &p_paths);
	void _export_profile(const String &p_path);

	bool updating_features = false;

	void _class_list_item_selected();
	void _class_list_item_edited();
	void _class_list_item_collapsed(Object *p_item);
	void _property_item_edited();
	void _save_and_update();

	Timer *update_timer = nullptr;
	void _emit_current_profile_changed();

	static EditorFeatureProfileManager *singleton;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	Ref<EditorFeatureProfile> get_current_profile();
	String get_current_profile_name() const;
	void set_current_profile(const String &p_profile_name, bool p_validate_profile);
	void notify_changed();

	static EditorFeatureProfileManager *get_singleton() { return singleton; }
	EditorFeatureProfileManager();
};
