/**************************************************************************/
/*  editor_build_profile.h                                                */
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

#ifndef EDITOR_BUILD_PROFILE_H
#define EDITOR_BUILD_PROFILE_H

#include "core/object/ref_counted.h"
#include "editor/editor_help.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/tree.h"

class EditorBuildProfile : public RefCounted {
	GDCLASS(EditorBuildProfile, RefCounted);

public:
	enum BuildOption {
		BUILD_OPTION_3D,
		BUILD_OPTION_PHYSICS_2D,
		BUILD_OPTION_PHYSICS_3D,
		BUILD_OPTION_NAVIGATION,
		BUILD_OPTION_XR,
		BUILD_OPTION_RENDERING_DEVICE,
		BUILD_OPTION_OPENGL,
		BUILD_OPTION_VULKAN,
		BUILD_OPTION_TEXT_SERVER_FALLBACK,
		BUILD_OPTION_TEXT_SERVER_ADVANCED,
		BUILD_OPTION_DYNAMIC_FONTS,
		BUILD_OPTION_WOFF2_FONTS,
		BUILD_OPTION_GRAPHITE_FONTS,
		BUILD_OPTION_MSDFGEN,
		BUILD_OPTION_MAX,
	};

	enum BuildOptionCategory {
		BUILD_OPTION_CATEGORY_GENERAL,
		BUILD_OPTION_CATEGORY_TEXT_SERVER,
		BUILD_OPTION_CATEGORY_MAX,
	};

private:
	HashSet<StringName> disabled_classes;

	HashSet<StringName> collapsed_classes;

	String force_detect_classes;

	bool build_options_disabled[BUILD_OPTION_MAX] = {};
	static const char *build_option_identifiers[BUILD_OPTION_MAX];
	static const bool build_option_disabled_by_default[BUILD_OPTION_MAX];
	static const bool build_option_disable_values[BUILD_OPTION_MAX];
	static const BuildOptionCategory build_option_category[BUILD_OPTION_MAX];

	String _get_build_option_name(BuildOption p_build_option) { return get_build_option_name(p_build_option); }

protected:
	static void _bind_methods();

public:
	void set_disable_class(const StringName &p_class, bool p_disabled);
	bool is_class_disabled(const StringName &p_class) const;

	void set_item_collapsed(const StringName &p_class, bool p_collapsed);
	bool is_item_collapsed(const StringName &p_class) const;

	void set_disable_build_option(BuildOption p_build_option, bool p_disable);
	bool is_build_option_disabled(BuildOption p_build_option) const;

	void set_force_detect_classes(const String &p_classes);
	String get_force_detect_classes() const;

	void clear_disabled_classes();

	Error save_to_file(const String &p_path);
	Error load_from_file(const String &p_path);

	static String get_build_option_name(BuildOption p_build_option);
	static String get_build_option_description(BuildOption p_build_option);
	static bool get_build_option_disable_value(BuildOption p_build_option);
	static BuildOptionCategory get_build_option_category(BuildOption p_build_option);

	static String get_build_option_category_name(BuildOptionCategory p_build_option_category);

	EditorBuildProfile();
};

VARIANT_ENUM_CAST(EditorBuildProfile::BuildOption)
VARIANT_ENUM_CAST(EditorBuildProfile::BuildOptionCategory)

class EditorFileDialog;
class EditorFileSystemDirectory;

class EditorBuildProfileManager : public AcceptDialog {
	GDCLASS(EditorBuildProfileManager, AcceptDialog);

	enum Action {
		ACTION_NEW,
		ACTION_RESET,
		ACTION_LOAD,
		ACTION_SAVE,
		ACTION_SAVE_AS,
		ACTION_DETECT,
		ACTION_MAX
	};

	Action last_action = ACTION_NEW;

	ConfirmationDialog *confirm_dialog = nullptr;
	Button *profile_actions[ACTION_MAX];

	Tree *class_list = nullptr;
	EditorHelpBit *description_bit = nullptr;

	EditorFileDialog *import_profile = nullptr;
	EditorFileDialog *export_profile = nullptr;

	LineEdit *profile_path = nullptr;

	LineEdit *force_detect_classes = nullptr;

	void _profile_action(int p_action);
	void _action_confirm();
	void _hide_requested();

	void _update_edited_profile();
	void _fill_classes_from(TreeItem *p_parent, const String &p_class, const String &p_selected);

	Ref<EditorBuildProfile> edited;

	void _import_profile(const String &p_path);
	void _export_profile(const String &p_path);

	bool updating_build_options = false;

	void _class_list_item_selected();
	void _class_list_item_edited();
	void _class_list_item_collapsed(Object *p_item);
	void _detect_classes();

	void _force_detect_classes_changed(const String &p_text);

	struct DetectedFile {
		uint32_t timestamp = 0;
		String md5;
		Vector<String> classes;
	};

	void _find_files(EditorFileSystemDirectory *p_dir, const HashMap<String, DetectedFile> &p_cache, HashMap<String, DetectedFile> &r_detected);

	static EditorBuildProfileManager *singleton;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	Ref<EditorBuildProfile> get_current_profile();

	static EditorBuildProfileManager *get_singleton() { return singleton; }
	EditorBuildProfileManager();
};

#endif // EDITOR_BUILD_PROFILE_H
