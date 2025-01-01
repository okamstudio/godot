/**************************************************************************/
/*  bone_map_editor_plugin.h                                              */
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

#ifndef BONE_MAP_EDITOR_PLUGIN_H
#define BONE_MAP_EDITOR_PLUGIN_H

#include "editor/editor_node.h"
#include "editor/editor_properties.h"
#include "editor/plugins/editor_plugin.h"

#include "modules/modules_enabled.gen.h" // For regex.
#ifdef MODULE_REGEX_ENABLED
#include "modules/regex/regex.h"
#endif

#include "scene/3d/skeleton_3d.h"
#include "scene/gui/box_container.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/dialogs.h"
#include "scene/resources/bone_map.h"
#include "scene/resources/texture.h"

class AspectRatioContainer;

class BoneMapperButton : public TextureButton {
	GDCLASS(BoneMapperButton, TextureButton);

public:
	enum BoneMapState {
		BONE_MAP_STATE_UNSET,
		BONE_MAP_STATE_SET,
		BONE_MAP_STATE_MISSING,
		BONE_MAP_STATE_ERROR
	};

private:
	StringName profile_bone_name;
	bool selected = false;
	bool require = false;

	TextureRect *circle = nullptr;

	void fetch_textures();

protected:
	void _notification(int p_what);

public:
	StringName get_profile_bone_name() const;
	void set_state(BoneMapState p_state);

	bool is_require() const;

	BoneMapperButton(const StringName &p_profile_bone_name, bool p_require, bool p_selected);
	~BoneMapperButton();
};

class BoneMapperItem : public VBoxContainer {
	GDCLASS(BoneMapperItem, VBoxContainer);

	int button_id = -1;
	StringName profile_bone_name;

	Ref<BoneMap> bone_map;

	EditorPropertyText *skeleton_bone_selector = nullptr;
	Button *picker_button = nullptr;

	void _update_property();
	void _open_picker();

protected:
	void _notification(int p_what);
	static void _bind_methods();
	virtual void _value_changed(const String &p_property, const Variant &p_value, const String &p_name, bool p_changing);
	virtual void create_editor();

public:
	void assign_button_id(int p_button_id);

	BoneMapperItem(Ref<BoneMap> &p_bone_map, const StringName &p_profile_bone_name = StringName());
	~BoneMapperItem();
};

class BonePicker : public AcceptDialog {
	GDCLASS(BonePicker, AcceptDialog);

	Skeleton3D *skeleton = nullptr;
	Tree *bones = nullptr;

public:
	void popup_bones_tree(const Size2i &p_minsize = Size2i());
	bool has_selected_bone();
	StringName get_selected_bone();

protected:
	void _notification(int p_what);
	void _confirm();

private:
	void create_editors();
	void create_bones_tree(Skeleton3D *p_skeleton);

public:
	BonePicker(Skeleton3D *p_skeleton);
	~BonePicker();
};

class BoneMapper : public VBoxContainer {
	GDCLASS(BoneMapper, VBoxContainer);

	Skeleton3D *skeleton = nullptr;
	Ref<BoneMap> bone_map;

	EditorPropertyResource *profile_selector = nullptr;

	Vector<BoneMapperItem *> bone_mapper_items;

	Button *clear_mapping_button = nullptr;

	VBoxContainer *mapper_item_vbox = nullptr;

	int current_group_idx = 0;
	int current_bone_idx = -1;

	AspectRatioContainer *bone_mapper_field = nullptr;
	EditorPropertyEnum *profile_group_selector = nullptr;
	ColorRect *profile_bg = nullptr;
	TextureRect *profile_texture = nullptr;
	Vector<BoneMapperButton *> bone_mapper_buttons;

	void create_editor();
	void recreate_editor();
	void clear_items();
	void recreate_items();
	void update_group_idx();
	void _update_state();

	/* Bone picker */
	BonePicker *picker = nullptr;
	StringName picker_key_name;
	void _pick_bone(const StringName &p_bone_name);
	void _apply_picker_selection();
	void _clear_mapping_current_group();

#ifdef MODULE_REGEX_ENABLED
	/* For auto mapping */
	enum BoneSegregation {
		BONE_SEGREGATION_NONE,
		BONE_SEGREGATION_LEFT,
		BONE_SEGREGATION_RIGHT
	};
	bool is_match_with_bone_name(const String &p_bone_name, const String &p_word);
	int search_bone_by_name(Skeleton3D *p_skeleton, const Vector<String> &p_picklist, BoneSegregation p_segregation = BONE_SEGREGATION_NONE, int p_parent = -1, int p_child = -1, int p_children_count = -1);
	BoneSegregation guess_bone_segregation(const String &p_bone_name);
	void auto_mapping_process(Ref<BoneMap> &p_bone_map);
	void _run_auto_mapping();
#endif // MODULE_REGEX_ENABLED

protected:
	void _notification(int p_what);
	static void _bind_methods();
	virtual void _value_changed(const String &p_property, const Variant &p_value, const String &p_name, bool p_changing);
	virtual void _profile_changed(const String &p_property, const Variant &p_value, const String &p_name, bool p_changing);

public:
	void set_current_group_idx(int p_group_idx);
	int get_current_group_idx() const;
	void set_current_bone_idx(int p_bone_idx);
	int get_current_bone_idx() const;

	BoneMapper(Skeleton3D *p_skeleton, Ref<BoneMap> &p_bone_map);
	~BoneMapper();
};

class BoneMapEditor : public VBoxContainer {
	GDCLASS(BoneMapEditor, VBoxContainer);

	Skeleton3D *skeleton = nullptr;
	Ref<BoneMap> bone_map;
	BoneMapper *bone_mapper = nullptr;

	void fetch_objects();
	void create_editors();

protected:
	void _notification(int p_what);

public:
	BoneMapEditor(Ref<BoneMap> &p_bone_map);
	~BoneMapEditor();
};

class EditorInspectorPluginBoneMap : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginBoneMap, EditorInspectorPlugin);
	BoneMapEditor *editor = nullptr;

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class BoneMapEditorPlugin : public EditorPlugin {
	GDCLASS(BoneMapEditorPlugin, EditorPlugin);

public:
	virtual String get_plugin_name() const override { return "BoneMap"; }
	BoneMapEditorPlugin();
};

#endif // BONE_MAP_EDITOR_PLUGIN_H
