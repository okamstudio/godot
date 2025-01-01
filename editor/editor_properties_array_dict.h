/**************************************************************************/
/*  editor_properties_array_dict.h                                        */
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

#ifndef EDITOR_PROPERTIES_ARRAY_DICT_H
#define EDITOR_PROPERTIES_ARRAY_DICT_H

#include "editor/editor_inspector.h"
#include "editor/editor_locale_dialog.h"

class Button;
class EditorSpinSlider;
class MarginContainer;

class EditorPropertyArrayObject : public RefCounted {
	GDCLASS(EditorPropertyArrayObject, RefCounted);

	Variant array;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;

public:
	enum {
		NOT_CHANGING_TYPE = -1,
	};

	void set_array(const Variant &p_array);
	Variant get_array();

	EditorPropertyArrayObject();
};

class EditorPropertyDictionaryObject : public RefCounted {
	GDCLASS(EditorPropertyDictionaryObject, RefCounted);

	Variant new_item_key;
	Variant new_item_value;
	Dictionary dict;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;

public:
	enum {
		NOT_CHANGING_TYPE = -3,
		NEW_KEY_INDEX,
		NEW_VALUE_INDEX,
	};

	bool get_by_property_name(const String &p_name, Variant &r_ret) const;
	void set_dict(const Dictionary &p_dict);
	Dictionary get_dict();

	void set_new_item_key(const Variant &p_new_item);
	Variant get_new_item_key();

	void set_new_item_value(const Variant &p_new_item);
	Variant get_new_item_value();

	String get_label_for_index(int p_index);
	String get_property_name_for_index(int p_index);

	EditorPropertyDictionaryObject();
};

class EditorPropertyArray : public EditorProperty {
	GDCLASS(EditorPropertyArray, EditorProperty);

	struct Slot {
		Ref<EditorPropertyArrayObject> object;
		HBoxContainer *container = nullptr;
		int index = -1;
		Variant::Type type = Variant::VARIANT_MAX;
		bool as_id = false;
		EditorProperty *prop = nullptr;
		Button *reorder_button = nullptr;

		void set_index(int p_idx) {
			String prop_name = "indices/" + itos(p_idx);
			prop->set_object_and_property(object.ptr(), prop_name);
			prop->set_label(itos(p_idx));
			index = p_idx;
		}
	};

	PopupMenu *change_type = nullptr;

	int page_length = 20;
	int page_index = 0;
	int changing_type_index = EditorPropertyArrayObject::NOT_CHANGING_TYPE;
	Button *edit = nullptr;
	PanelContainer *container = nullptr;
	VBoxContainer *property_vbox = nullptr;
	EditorSpinSlider *size_slider = nullptr;
	Button *button_add_item = nullptr;
	EditorPaginator *paginator = nullptr;
	Variant::Type array_type;
	Variant::Type subtype;
	PropertyHint subtype_hint;
	String subtype_hint_string;
	LocalVector<Slot> slots;

	Slot reorder_slot;
	int reorder_to_index = -1;
	float reorder_mouse_y_delta = 0.0f;
	void initialize_array(Variant &p_array);

	void _page_changed(int p_page);

	void _reorder_button_gui_input(const Ref<InputEvent> &p_event);
	void _reorder_button_down(int p_index);
	void _reorder_button_up();
	void _create_new_property_slot();

	Node *get_base_node();

protected:
	Ref<EditorPropertyArrayObject> object;

	bool updating = false;
	bool dropping = false;

	void _notification(int p_what);

	virtual void _add_element();
	virtual void _length_changed(double p_page);
	virtual void _edit_pressed();
	virtual void _property_changed(const String &p_property, Variant p_value, const String &p_name = "", bool p_changing = false);
	virtual void _change_type(Object *p_button, int p_slot_index);
	virtual void _change_type_menu(int p_index);

	virtual void _object_id_selected(const StringName &p_property, ObjectID p_id);
	virtual void _remove_pressed(int p_index);

	virtual void _button_draw();
	virtual bool _is_drop_valid(const Dictionary &p_drag_data) const;
	virtual bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	virtual void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);

public:
	void setup(Variant::Type p_array_type, const String &p_hint_string = "");
	virtual void update_property() override;
	virtual bool is_colored(ColorationMode p_mode) override;
	EditorPropertyArray();
};

class EditorPropertyDictionary : public EditorProperty {
	GDCLASS(EditorPropertyDictionary, EditorProperty);

	struct Slot {
		Ref<EditorPropertyDictionaryObject> object;
		HBoxContainer *container = nullptr;
		int index = -1;
		Variant::Type type = Variant::VARIANT_MAX;
		bool as_id = false;
		EditorProperty *prop = nullptr;
		String prop_name;

		void set_index(int p_idx) {
			index = p_idx;
			prop_name = object->get_property_name_for_index(p_idx);
			update_prop_or_index();
		}

		void set_prop(EditorProperty *p_prop) {
			prop->add_sibling(p_prop);
			prop->queue_free();
			prop = p_prop;
			update_prop_or_index();
		}

		void update_prop_or_index() {
			prop->set_object_and_property(object.ptr(), prop_name);
			prop->set_label(object->get_label_for_index(index));
		}
	};

	PopupMenu *change_type = nullptr;
	bool updating = false;

	Ref<EditorPropertyDictionaryObject> object;
	int page_length = 20;
	int page_index = 0;
	int changing_type_index = EditorPropertyDictionaryObject::NOT_CHANGING_TYPE;
	Button *edit = nullptr;
	PanelContainer *container = nullptr;
	VBoxContainer *property_vbox = nullptr;
	PanelContainer *add_panel = nullptr;
	EditorSpinSlider *size_sliderv = nullptr;
	Button *button_add_item = nullptr;
	EditorPaginator *paginator = nullptr;
	LocalVector<Slot> slots;
	void _create_new_property_slot(int p_idx);

	void _page_changed(int p_page);
	void _edit_pressed();
	void _property_changed(const String &p_property, Variant p_value, const String &p_name = "", bool p_changing = false);
	void _change_type(Object *p_button, int p_slot_index);
	void _change_type_menu(int p_index);

	void _add_key_value();
	void _object_id_selected(const StringName &p_property, ObjectID p_id);
	void _remove_pressed(int p_slot_index);

	Variant::Type key_subtype;
	PropertyHint key_subtype_hint;
	String key_subtype_hint_string;
	Variant::Type value_subtype;
	PropertyHint value_subtype_hint;
	String value_subtype_hint_string;
	void initialize_dictionary(Variant &p_dictionary);

protected:
	void _notification(int p_what);

public:
	void setup(PropertyHint p_hint, const String &p_hint_string = "");
	virtual void update_property() override;
	virtual bool is_colored(ColorationMode p_mode) override;
	EditorPropertyDictionary();
};

class EditorPropertyLocalizableString : public EditorProperty {
	GDCLASS(EditorPropertyLocalizableString, EditorProperty);

	EditorLocaleDialog *locale_select = nullptr;

	bool updating;

	Ref<EditorPropertyDictionaryObject> object;
	int page_length = 20;
	int page_index = 0;
	Button *edit = nullptr;
	MarginContainer *container = nullptr;
	VBoxContainer *property_vbox = nullptr;
	EditorSpinSlider *size_slider = nullptr;
	Button *button_add_item = nullptr;
	EditorPaginator *paginator = nullptr;

	void _page_changed(int p_page);
	void _edit_pressed();
	void _remove_item(Object *p_button, int p_index);
	void _property_changed(const String &p_property, const Variant &p_value, const String &p_name = "", bool p_changing = false);

	void _add_locale_popup();
	void _add_locale(const String &p_locale);
	void _object_id_selected(const StringName &p_property, ObjectID p_id);

protected:
	void _notification(int p_what);

public:
	virtual void update_property() override;
	EditorPropertyLocalizableString();
};

#endif // EDITOR_PROPERTIES_ARRAY_DICT_H
