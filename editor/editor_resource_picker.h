/**************************************************************************/
/*  editor_resource_picker.h                                              */
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

#ifndef EDITOR_RESOURCE_PICKER_H
#define EDITOR_RESOURCE_PICKER_H

#include "scene/gui/box_container.h"

class Button;
class ConfirmationDialog;
class EditorFileDialog;
class PopupMenu;
class TextureRect;
class Tree;
class TreeItem;

class EditorResourcePicker : public HBoxContainer {
	GDCLASS(EditorResourcePicker, HBoxContainer);

	String base_type;
	Ref<Resource> edited_resource;

	bool editable = true;
	bool dropping = false;

	Vector<String> inheritors_array;
	mutable HashSet<StringName> allowed_types_without_convert;
	mutable HashSet<StringName> allowed_types_with_convert;

	Button *assign_button = nullptr;
	TextureRect *preview_rect = nullptr;
	Button *edit_button = nullptr;
	EditorFileDialog *file_dialog = nullptr;

	ConfirmationDialog *duplicate_resources_dialog = nullptr;
	Tree *duplicate_resources_tree = nullptr;

	Size2i assign_button_min_size = Size2i(1, 1);

	enum MenuOption {
		OBJ_MENU_LOAD,
		OBJ_MENU_QUICKLOAD,
		OBJ_MENU_INSPECT,
		OBJ_MENU_CLEAR,
		OBJ_MENU_MAKE_UNIQUE,
		OBJ_MENU_MAKE_UNIQUE_RECURSIVE,
		OBJ_MENU_SAVE,
		OBJ_MENU_SAVE_AS,
		OBJ_MENU_COPY,
		OBJ_MENU_PASTE,
		OBJ_MENU_SHOW_IN_FILE_SYSTEM,

		TYPE_BASE_ID = 100,
		CONVERT_BASE_ID = 1000,
	};

	PopupMenu *edit_menu = nullptr;

	void _update_resource_preview(const String &p_path, const Ref<Texture2D> &p_preview, const Ref<Texture2D> &p_small_preview, ObjectID p_obj);

	void _resource_selected();
	void _resource_changed();
	void _file_selected(const String &p_path);

	void _resource_saved(Object *p_resource);

	void _update_menu();
	void _update_menu_items();
	void _edit_menu_cbk(int p_which);

	void _button_draw();
	void _button_input(const Ref<InputEvent> &p_event);

	String _get_resource_type(const Ref<Resource> &p_resource) const;
	void _ensure_allowed_types() const;
	bool _is_drop_valid(const Dictionary &p_drag_data) const;
	bool _is_type_valid(const String &p_type_name, const HashSet<StringName> &p_allowed_types) const;

	Variant get_drag_data_fw(const Point2 &p_point, Control *p_from);
	bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);

	void _ensure_resource_menu();
	void _gather_resources_to_duplicate(const Ref<Resource> p_resource, TreeItem *p_item, const String &p_property_name = "") const;
	void _duplicate_selected_resources();

protected:
	virtual void _update_resource();

	Button *get_assign_button() { return assign_button; }
	static void _bind_methods();
	void _notification(int p_what);

	void set_assign_button_min_size(const Size2i &p_size);

	GDVIRTUAL1(_set_create_options, Object *)
	GDVIRTUAL1R(bool, _handle_menu_selected, int)

public:
	void set_base_type(const String &p_base_type);
	String get_base_type() const;
	Vector<String> get_allowed_types() const;

	void set_edited_resource(Ref<Resource> p_resource);
	void set_edited_resource_no_check(Ref<Resource> p_resource);
	Ref<Resource> get_edited_resource();

	void set_toggle_mode(bool p_enable);
	bool is_toggle_mode() const;
	void set_toggle_pressed(bool p_pressed);
	bool is_toggle_pressed() const;

	void set_editable(bool p_editable);
	bool is_editable() const;

	virtual void set_create_options(Object *p_menu_node);
	virtual bool handle_menu_selected(int p_which);

	EditorResourcePicker(bool p_hide_assign_button_controls = false);
};

class EditorScriptPicker : public EditorResourcePicker {
	GDCLASS(EditorScriptPicker, EditorResourcePicker);

	enum ExtraMenuOption {
		OBJ_MENU_NEW_SCRIPT = 50,
		OBJ_MENU_EXTEND_SCRIPT = 51
	};

	Node *script_owner = nullptr;

protected:
	static void _bind_methods();

public:
	virtual void set_create_options(Object *p_menu_node) override;
	virtual bool handle_menu_selected(int p_which) override;

	void set_script_owner(Node *p_owner);
	Node *get_script_owner() const;

	EditorScriptPicker();
};

class EditorShaderPicker : public EditorResourcePicker {
	GDCLASS(EditorShaderPicker, EditorResourcePicker);

	enum ExtraMenuOption {
		OBJ_MENU_NEW_SHADER = 50,
	};

	ShaderMaterial *edited_material = nullptr;
	int preferred_mode = -1;

public:
	virtual void set_create_options(Object *p_menu_node) override;
	virtual bool handle_menu_selected(int p_which) override;

	void set_edited_material(ShaderMaterial *p_material);
	ShaderMaterial *get_edited_material() const;
	void set_preferred_mode(int p_preferred_mode);

	EditorShaderPicker();
};

class EditorAudioStreamPicker : public EditorResourcePicker {
	GDCLASS(EditorAudioStreamPicker, EditorResourcePicker);

	uint64_t last_preview_version = 0;
	Control *stream_preview_rect = nullptr;

	enum {
		MAX_TAGGED_FRAMES = 8
	};
	float tagged_frame_offsets[MAX_TAGGED_FRAMES];
	uint32_t tagged_frame_offset_count = 0;

	void _preview_draw();
	virtual void _update_resource() override;

protected:
	void _notification(int p_what);

public:
	EditorAudioStreamPicker();
};

#endif // EDITOR_RESOURCE_PICKER_H
