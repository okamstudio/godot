/**************************************************************************/
/*  editor_properties_array_dict.cpp                                      */
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

#include "editor_properties_array_dict.h"

#include "core/input/input.h"
#include "core/io/marshalls.h"
#include "editor/editor_properties.h"
#include "editor/editor_properties_vector.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_spin_slider.h"
#include "editor/inspector_dock.h"
#include "editor/themes/editor_scale.h"
#include "editor/themes/editor_theme_manager.h"
#include "scene/gui/button.h"
#include "scene/gui/margin_container.h"
#include "scene/resources/packed_scene.h"

bool EditorPropertyArrayObject::_set(const StringName &p_name, const Variant &p_value) {
	String name = p_name;

	if (!name.begins_with("indices")) {
		return false;
	}

	int index;
	if (name.begins_with("metadata/")) {
		index = name.get_slice("/", 2).to_int();
	} else {
		index = name.get_slice("/", 1).to_int();
	}

	array.set(index, p_value);
	return true;
}

bool EditorPropertyArrayObject::_get(const StringName &p_name, Variant &r_ret) const {
	String name = p_name;

	if (!name.begins_with("indices")) {
		return false;
	}

	int index;
	if (name.begins_with("metadata/")) {
		index = name.get_slice("/", 2).to_int();
	} else {
		index = name.get_slice("/", 1).to_int();
	}

	bool valid;
	r_ret = array.get(index, &valid);

	if (r_ret.get_type() == Variant::OBJECT && Object::cast_to<EncodedObjectAsID>(r_ret)) {
		r_ret = Object::cast_to<EncodedObjectAsID>(r_ret)->get_object_id();
	}

	return valid;
}

void EditorPropertyArrayObject::set_array(const Variant &p_array) {
	array = p_array;
}

Variant EditorPropertyArrayObject::get_array() {
	return array;
}

EditorPropertyArrayObject::EditorPropertyArrayObject() {
}

///////////////////

bool EditorPropertyDictionaryObject::_set(const StringName &p_name, const Variant &p_value) {
	String name = p_name;

	if (name == "new_item_key") {
		new_item_key = p_value;
		return true;
	}

	if (name == "new_item_value") {
		new_item_value = p_value;
		return true;
	}

	if (name.begins_with("indices")) {
		dict = dict.duplicate();
		int index = name.get_slicec('/', 1).to_int();
		Variant key = dict.get_key_at_index(index);
		dict[key] = p_value;
		return true;
	}

	return false;
}

bool EditorPropertyDictionaryObject::_get(const StringName &p_name, Variant &r_ret) const {
	if (!get_by_property_name(p_name, r_ret)) {
		return false;
	}
	if (r_ret.get_type() == Variant::OBJECT && Object::cast_to<EncodedObjectAsID>(r_ret)) {
		r_ret = Object::cast_to<EncodedObjectAsID>(r_ret)->get_object_id();
	}
	return true;
}

bool EditorPropertyDictionaryObject::get_by_property_name(const String &p_name, Variant &r_ret) const {
	String name = p_name;

	if (name == "new_item_key") {
		r_ret = new_item_key;
		return true;
	}

	if (name == "new_item_value") {
		r_ret = new_item_value;
		return true;
	}

	if (name.begins_with("indices")) {
		int index = name.get_slicec('/', 1).to_int();
		Variant key = dict.get_key_at_index(index);
		r_ret = dict[key];
		return true;
	}

	return false;
}

void EditorPropertyDictionaryObject::set_dict(const Dictionary &p_dict) {
	dict = p_dict;
}

Dictionary EditorPropertyDictionaryObject::get_dict() {
	return dict;
}

void EditorPropertyDictionaryObject::set_new_item_key(const Variant &p_new_item) {
	new_item_key = p_new_item;
}

Variant EditorPropertyDictionaryObject::get_new_item_key() {
	return new_item_key;
}

void EditorPropertyDictionaryObject::set_new_item_value(const Variant &p_new_item) {
	new_item_value = p_new_item;
}

Variant EditorPropertyDictionaryObject::get_new_item_value() {
	return new_item_value;
}

String EditorPropertyDictionaryObject::get_property_name_for_index(int p_index) {
	switch (p_index) {
		case NEW_KEY_INDEX:
			return "new_item_key";
		case NEW_VALUE_INDEX:
			return "new_item_value";
		default:
			return "indices/" + itos(p_index);
	}
}

String EditorPropertyDictionaryObject::get_label_for_index(int p_index) {
	switch (p_index) {
		case NEW_KEY_INDEX:
			return TTR("New Key:");
			break;
		case NEW_VALUE_INDEX:
			return TTR("New Value:");
			break;
		default:
			return dict.get_key_at_index(p_index).get_construct_string();
			break;
	}
}

EditorPropertyDictionaryObject::EditorPropertyDictionaryObject() {
}

///////////////////// ARRAY ///////////////////////////

void EditorPropertyArray::initialize_array(Variant &p_array) {
	if (array_type == Variant::ARRAY && subtype != Variant::NIL) {
		Array array;
		StringName subtype_class;
		Ref<Script> subtype_script;
		if (subtype == Variant::OBJECT && !subtype_hint_string.is_empty()) {
			if (ClassDB::class_exists(subtype_hint_string)) {
				subtype_class = subtype_hint_string;
			}
		}
		array.set_typed(subtype, subtype_class, subtype_script);
		p_array = array;
	} else {
		VariantInternal::initialize(&p_array, array_type);
	}
}

void EditorPropertyArray::_property_changed(const String &p_property, Variant p_value, const String &p_name, bool p_changing) {
	if (!p_property.begins_with("indices")) {
		return;
	}

	if (p_value.get_type() == Variant::OBJECT && p_value.is_null()) {
		p_value = Variant(); // `EditorResourcePicker` resets to `Ref<Resource>()`. See GH-82716.
	}

	int index = p_property.get_slice("/", 1).to_int();

	Variant array = object->get_array().duplicate();
	array.set(index, p_value);
	emit_changed(get_edited_property(), array, p_name, p_changing);
	if (p_changing) {
		object->set_array(array);
	}
}

void EditorPropertyArray::_change_type(Object *p_button, int p_slot_index) {
	Button *button = Object::cast_to<Button>(p_button);
	changing_type_index = p_slot_index;
	Rect2 rect = button->get_screen_rect();
	change_type->reset_size();
	change_type->set_position(rect.get_end() - Vector2(change_type->get_contents_minimum_size().x, 0));
	change_type->popup();
}

void EditorPropertyArray::_change_type_menu(int p_index) {
	if (p_index == Variant::VARIANT_MAX) {
		_remove_pressed(changing_type_index);
		return;
	}

	ERR_FAIL_COND_MSG(
			changing_type_index == EditorPropertyArrayObject::NOT_CHANGING_TYPE,
			"Tried to change type of an array item, but no item was selected.");

	Variant value;
	VariantInternal::initialize(&value, Variant::Type(p_index));

	Variant array = object->get_array().duplicate();
	array.set(slots[changing_type_index].index, value);

	emit_changed(get_edited_property(), array);
}

void EditorPropertyArray::_object_id_selected(const StringName &p_property, ObjectID p_id) {
	emit_signal(SNAME("object_id_selected"), p_property, p_id);
}

void EditorPropertyArray::_create_new_property_slot() {
	int idx = slots.size();
	HBoxContainer *hbox = memnew(HBoxContainer);

	Button *reorder_button = memnew(Button);
	reorder_button->set_button_icon(get_editor_theme_icon(SNAME("TripleBar")));
	reorder_button->set_default_cursor_shape(Control::CURSOR_MOVE);
	reorder_button->set_disabled(is_read_only());
	reorder_button->connect(SceneStringName(gui_input), callable_mp(this, &EditorPropertyArray::_reorder_button_gui_input));
	reorder_button->connect(SNAME("button_up"), callable_mp(this, &EditorPropertyArray::_reorder_button_up));
	reorder_button->connect(SNAME("button_down"), callable_mp(this, &EditorPropertyArray::_reorder_button_down).bind(idx));

	hbox->add_child(reorder_button);
	EditorProperty *prop = memnew(EditorPropertyNil);
	hbox->add_child(prop);

	bool is_untyped_array = object->get_array().get_type() == Variant::ARRAY && subtype == Variant::NIL;

	if (is_untyped_array) {
		Button *edit_btn = memnew(Button);
		edit_btn->set_button_icon(get_editor_theme_icon(SNAME("Edit")));
		edit_btn->set_disabled(is_read_only());
		edit_btn->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyArray::_change_type).bind(edit_btn, idx));
		hbox->add_child(edit_btn);
	} else {
		Button *remove_btn = memnew(Button);
		remove_btn->set_button_icon(get_editor_theme_icon(SNAME("Remove")));
		remove_btn->set_disabled(is_read_only());
		remove_btn->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyArray::_remove_pressed).bind(idx));
		hbox->add_child(remove_btn);
	}
	property_vbox->add_child(hbox);

	Slot slot;
	slot.prop = prop;
	slot.object = object;
	slot.container = hbox;
	slot.reorder_button = reorder_button;
	slot.set_index(idx + page_index * page_length);
	slots.push_back(slot);
}

void EditorPropertyArray::update_property() {
	Variant array = get_edited_property_value();

	String array_type_name = Variant::get_type_name(array_type);
	if (array_type == Variant::ARRAY && subtype != Variant::NIL) {
		String type_name;
		if (subtype == Variant::OBJECT && (subtype_hint == PROPERTY_HINT_RESOURCE_TYPE || subtype_hint == PROPERTY_HINT_NODE_TYPE)) {
			type_name = subtype_hint_string;
		} else {
			type_name = Variant::get_type_name(subtype);
		}

		array_type_name = vformat("%s[%s]", array_type_name, type_name);
	}

	if (!array.is_array()) {
		edit->set_text(vformat(TTR("(Nil) %s"), array_type_name));
		edit->set_pressed(false);
		if (container) {
			set_bottom_editor(nullptr);
			memdelete(container);
			button_add_item = nullptr;
			container = nullptr;
			slots.clear();
		}
		return;
	}

	object->set_array(array);

	int size = array.call("size");
	int max_page = MAX(0, size - 1) / page_length;
	if (page_index > max_page) {
		_page_changed(max_page);
	}

	edit->set_text(vformat(TTR("%s (size %s)"), array_type_name, itos(size)));

	bool unfolded = get_edited_object()->editor_is_section_unfolded(get_edited_property());
	if (edit->is_pressed() != unfolded) {
		edit->set_pressed(unfolded);
	}

	if (unfolded) {
		updating = true;

		if (!container) {
			container = memnew(PanelContainer);
			container->set_mouse_filter(MOUSE_FILTER_STOP);
			add_child(container);
			set_bottom_editor(container);

			VBoxContainer *vbox = memnew(VBoxContainer);
			container->add_child(vbox);

			HBoxContainer *hbox = memnew(HBoxContainer);
			vbox->add_child(hbox);

			Label *size_label = memnew(Label(TTR("Size:")));
			size_label->set_h_size_flags(SIZE_EXPAND_FILL);
			hbox->add_child(size_label);

			size_slider = memnew(EditorSpinSlider);
			size_slider->set_step(1);
			size_slider->set_max(INT32_MAX);
			size_slider->set_h_size_flags(SIZE_EXPAND_FILL);
			size_slider->set_read_only(is_read_only());
			size_slider->connect(SceneStringName(value_changed), callable_mp(this, &EditorPropertyArray::_length_changed));
			hbox->add_child(size_slider);

			property_vbox = memnew(VBoxContainer);
			property_vbox->set_h_size_flags(SIZE_EXPAND_FILL);
			vbox->add_child(property_vbox);

			button_add_item = EditorInspector::create_inspector_action_button(TTR("Add Element"));
			button_add_item->set_button_icon(get_editor_theme_icon(SNAME("Add")));
			button_add_item->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyArray::_add_element));
			button_add_item->set_disabled(is_read_only());
			vbox->add_child(button_add_item);

			paginator = memnew(EditorPaginator);
			paginator->connect("page_changed", callable_mp(this, &EditorPropertyArray::_page_changed));
			vbox->add_child(paginator);

			for (int i = 0; i < page_length; i++) {
				_create_new_property_slot();
			}
		}

		size_slider->set_value(size);
		property_vbox->set_visible(size > 0);
		button_add_item->set_visible(page_index == max_page);
		paginator->update(page_index, max_page);
		paginator->set_visible(max_page > 0);

		for (Slot &slot : slots) {
			bool slot_visible = &slot != &reorder_slot && slot.index < size;
			slot.container->set_visible(slot_visible);
			// If not visible no need to update it
			if (!slot_visible) {
				continue;
			}

			int idx = slot.index;
			Variant::Type value_type = subtype;

			if (value_type == Variant::NIL) {
				value_type = array.get(idx).get_type();
			}

			// Check if the editor property needs to be updated.
			bool value_as_id = Object::cast_to<EncodedObjectAsID>(array.get(idx));
			if (value_type != slot.type || (value_type == Variant::OBJECT && (value_as_id != slot.as_id))) {
				slot.as_id = value_as_id;
				slot.type = value_type;
				EditorProperty *new_prop = nullptr;
				if (value_type == Variant::OBJECT && value_as_id) {
					EditorPropertyObjectID *editor = memnew(EditorPropertyObjectID);
					editor->setup("Object");
					new_prop = editor;
				} else {
					new_prop = EditorInspector::instantiate_property_editor(this, value_type, "", subtype_hint, subtype_hint_string, PROPERTY_USAGE_NONE);
				}
				new_prop->set_selectable(false);
				new_prop->set_use_folding(is_using_folding());
				new_prop->connect(SNAME("property_changed"), callable_mp(this, &EditorPropertyArray::_property_changed));
				new_prop->connect(SNAME("object_id_selected"), callable_mp(this, &EditorPropertyArray::_object_id_selected));
				new_prop->set_h_size_flags(SIZE_EXPAND_FILL);
				new_prop->set_read_only(is_read_only());
				slot.prop->add_sibling(new_prop, false);
				slot.prop->queue_free();
				slot.prop = new_prop;
				slot.set_index(idx);
			}
			if (slot.index == changing_type_index) {
				callable_mp(slot.prop, &EditorProperty::grab_focus).call_deferred(0);
				changing_type_index = EditorPropertyArrayObject::NOT_CHANGING_TYPE;
			}
			slot.prop->update_property();
		}

		updating = false;

	} else {
		if (container) {
			set_bottom_editor(nullptr);
			memdelete(container);
			button_add_item = nullptr;
			container = nullptr;
			slots.clear();
		}
	}
}

void EditorPropertyArray::_remove_pressed(int p_slot_index) {
	Variant array = object->get_array().duplicate();
	array.call("remove_at", slots[p_slot_index].index);

	emit_changed(get_edited_property(), array);
}

void EditorPropertyArray::_button_draw() {
	if (dropping) {
		Color color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
		edit->draw_rect(Rect2(Point2(), edit->get_size()), color, false);
	}
}

bool EditorPropertyArray::_is_drop_valid(const Dictionary &p_drag_data) const {
	if (is_read_only()) {
		return false;
	}

	String allowed_type = Variant::get_type_name(subtype);

	// When the subtype is of type Object, an additional subtype may be specified in the hint string
	// (e.g. Resource, Texture2D, ShaderMaterial, etc). We want the allowed type to be that, not just "Object".
	if (subtype == Variant::OBJECT && !subtype_hint_string.is_empty()) {
		allowed_type = subtype_hint_string;
	}

	Dictionary drag_data = p_drag_data;
	const String drop_type = drag_data.get("type", "");

	if (drop_type == "files") {
		PackedStringArray files = drag_data["files"];

		for (int i = 0; i < files.size(); i++) {
			const String &file = files[i];
			String ftype = EditorFileSystem::get_singleton()->get_file_type(file);

			for (int j = 0; j < allowed_type.get_slice_count(","); j++) {
				String at = allowed_type.get_slice(",", j).strip_edges();
				// Fail if one of the files is not of allowed type.
				if (!ClassDB::is_parent_class(ftype, at)) {
					return false;
				}
			}
		}

		// If no files fail, drop is valid.
		return true;
	}

	if (drop_type == "nodes") {
		Array node_paths = drag_data["nodes"];

		PackedStringArray allowed_subtype_array;
		if (allowed_type == "NodePath") {
			if (subtype_hint_string == "NodePath") {
				return true;
			} else {
				for (int j = 0; j < subtype_hint_string.get_slice_count(","); j++) {
					String ast = subtype_hint_string.get_slice(",", j).strip_edges();
					allowed_subtype_array.append(ast);
				}
			}
		}

		bool is_drop_allowed = true;

		for (int i = 0; i < node_paths.size(); i++) {
			const Node *dropped_node = get_node_or_null(node_paths[i]);
			ERR_FAIL_NULL_V_MSG(dropped_node, false, "Could not get the dropped node by its path.");

			if (allowed_type != "NodePath") {
				if (!ClassDB::is_parent_class(dropped_node->get_class_name(), allowed_type)) {
					// Fail if one of the nodes is not of allowed type.
					return false;
				}
			}

			// The array of NodePaths is restricted to specific types using @export_node_path().
			if (allowed_type == "NodePath" && subtype_hint_string != "NodePath") {
				if (!allowed_subtype_array.has(dropped_node->get_class_name())) {
					// The dropped node type was not found in the allowed subtype array, we must check if it inherits one of them.
					for (const String &ast : allowed_subtype_array) {
						if (ClassDB::is_parent_class(dropped_node->get_class_name(), ast)) {
							is_drop_allowed = true;
							break;
						} else {
							is_drop_allowed = false;
						}
					}
					if (!is_drop_allowed) {
						break;
					}
				}
			}
		}

		return is_drop_allowed;
	}

	return false;
}

bool EditorPropertyArray::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	return _is_drop_valid(p_data);
}

void EditorPropertyArray::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	ERR_FAIL_COND(!_is_drop_valid(p_data));

	Dictionary drag_data = p_data;
	const String drop_type = drag_data.get("type", "");
	Variant array = object->get_array();

	// Handle the case where array is not initialized yet.
	if (!array.is_array()) {
		initialize_array(array);
	} else {
		array = array.duplicate();
	}

	if (drop_type == "files") {
		PackedStringArray files = drag_data["files"];

		// Loop the file array and add to existing array.
		for (int i = 0; i < files.size(); i++) {
			const String &file = files[i];

			Ref<Resource> res = ResourceLoader::load(file);
			if (res.is_valid()) {
				array.call("push_back", res);
			}
		}

		emit_changed(get_edited_property(), array);
	}

	if (drop_type == "nodes") {
		Array node_paths = drag_data["nodes"];
		Node *base_node = get_base_node();

		for (int i = 0; i < node_paths.size(); i++) {
			const NodePath &path = node_paths[i];

			if (subtype == Variant::OBJECT) {
				array.call("push_back", get_node(path));
			} else if (subtype == Variant::NODE_PATH) {
				array.call("push_back", base_node->get_path().rel_path_to(path));
			}
		}

		emit_changed(get_edited_property(), array);
	}
}

Node *EditorPropertyArray::get_base_node() {
	Node *base_node = Object::cast_to<Node>(InspectorDock::get_inspector_singleton()->get_edited_object());

	if (!base_node) {
		base_node = get_tree()->get_edited_scene_root();
	}

	return base_node;
}

void EditorPropertyArray::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED:
		case NOTIFICATION_ENTER_TREE: {
			change_type->clear();
			change_type->add_icon_item(get_editor_theme_icon(SNAME("Remove")), TTR("Remove Item"), Variant::VARIANT_MAX);
			change_type->add_separator();
			for (int i = 0; i < Variant::VARIANT_MAX; i++) {
				if (i == Variant::CALLABLE || i == Variant::SIGNAL || i == Variant::RID) {
					// These types can't be constructed or serialized properly, so skip them.
					continue;
				}

				String type = Variant::get_type_name(Variant::Type(i));
				change_type->add_icon_item(get_editor_theme_icon(type), type, i);
			}

			if (button_add_item) {
				button_add_item->set_button_icon(get_editor_theme_icon(SNAME("Add")));
			}
		} break;

		case NOTIFICATION_DRAG_BEGIN: {
			if (is_visible_in_tree()) {
				if (_is_drop_valid(get_viewport()->gui_get_drag_data())) {
					dropping = true;
					edit->queue_redraw();
				}
			}
		} break;

		case NOTIFICATION_DRAG_END: {
			if (dropping) {
				dropping = false;
				edit->queue_redraw();
			}
		} break;
	}
}

void EditorPropertyArray::_edit_pressed() {
	Variant array = get_edited_property_value();
	if (!array.is_array() && edit->is_pressed()) {
		initialize_array(array);
		emit_changed(get_edited_property(), array);
	}

	get_edited_object()->editor_set_section_unfold(get_edited_property(), edit->is_pressed());
	update_property();
}

void EditorPropertyArray::_page_changed(int p_page) {
	if (updating) {
		return;
	}
	page_index = p_page;
	int i = p_page * page_length;

	if (reorder_slot.index < 0) {
		for (Slot &slot : slots) {
			slot.set_index(i);
			i++;
		}
	} else {
		int reorder_from_page = reorder_slot.index / page_length;
		if (reorder_from_page < p_page) {
			i++;
		}
		for (Slot &slot : slots) {
			if (slot.index != reorder_slot.index) {
				slot.set_index(i);
				i++;
			} else if (i == reorder_slot.index) {
				i++;
			}
		}
	}
	update_property();
}

void EditorPropertyArray::_length_changed(double p_page) {
	if (updating) {
		return;
	}

	Variant array = object->get_array().duplicate();
	array.call("resize", int(p_page));

	emit_changed(get_edited_property(), array);
}

void EditorPropertyArray::_add_element() {
	_length_changed(double(object->get_array().call("size")) + 1.0);
}

void EditorPropertyArray::setup(Variant::Type p_array_type, const String &p_hint_string) {
	array_type = p_array_type;

	// The format of p_hint_string is:
	// subType/subTypeHint:nextSubtype ... etc.
	if (!p_hint_string.is_empty()) {
		int hint_subtype_separator = p_hint_string.find_char(':');
		if (hint_subtype_separator >= 0) {
			String subtype_string = p_hint_string.substr(0, hint_subtype_separator);
			int slash_pos = subtype_string.find_char('/');
			if (slash_pos >= 0) {
				subtype_hint = PropertyHint(subtype_string.substr(slash_pos + 1, subtype_string.size() - slash_pos - 1).to_int());
				subtype_string = subtype_string.substr(0, slash_pos);
			}

			subtype_hint_string = p_hint_string.substr(hint_subtype_separator + 1, p_hint_string.size() - hint_subtype_separator - 1);
			subtype = Variant::Type(subtype_string.to_int());
		}
	}
}

void EditorPropertyArray::_reorder_button_gui_input(const Ref<InputEvent> &p_event) {
	if (reorder_slot.index < 0 || is_read_only()) {
		return;
	}

	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		Variant array = object->get_array();
		int size = array.call("size");

		// Cumulate the mouse delta, many small changes (dragging slowly) should result in reordering at some point.
		reorder_mouse_y_delta += mm->get_relative().y;

		// Reordering is done by moving the dragged element by +1/-1 index at a time based on the cumulated mouse delta so if
		// already at the array bounds make sure to ignore the remaining out of bounds drag (by resetting the cumulated delta).
		if ((reorder_to_index == 0 && reorder_mouse_y_delta < 0.0f) || (reorder_to_index == size - 1 && reorder_mouse_y_delta > 0.0f)) {
			reorder_mouse_y_delta = 0.0f;
			return;
		}

		float required_y_distance = 20.0f * EDSCALE;
		if (ABS(reorder_mouse_y_delta) > required_y_distance) {
			int direction = reorder_mouse_y_delta > 0.0f ? 1 : -1;
			reorder_mouse_y_delta -= required_y_distance * direction;

			reorder_to_index += direction;

			property_vbox->move_child(reorder_slot.container, reorder_to_index % page_length);

			if ((direction < 0 && reorder_to_index % page_length == page_length - 1) || (direction > 0 && reorder_to_index % page_length == 0)) {
				// Automatically move to the next/previous page.
				_page_changed(page_index + direction);
			}
			// Ensure the moving element is visible.
			InspectorDock::get_inspector_singleton()->ensure_control_visible(reorder_slot.container);
		}
	}
}

void EditorPropertyArray::_reorder_button_down(int p_slot_index) {
	if (is_read_only()) {
		return;
	}
	reorder_slot = slots[p_slot_index];
	reorder_to_index = reorder_slot.index;
	// Ideally it'd to be able to show the mouse but I had issues with
	// Control's `mouse_exit()`/`mouse_entered()` signals not getting called.
	Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
}

void EditorPropertyArray::_reorder_button_up() {
	if (is_read_only()) {
		return;
	}

	if (reorder_slot.index != reorder_to_index) {
		// Move the element.
		Variant array = object->get_array().duplicate();

		property_vbox->move_child(reorder_slot.container, reorder_slot.index % page_length);
		Variant value_to_move = array.get(reorder_slot.index);
		array.call("remove_at", reorder_slot.index);
		array.call("insert", reorder_to_index, value_to_move);

		emit_changed(get_edited_property(), array);
	}

	Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);

	ERR_FAIL_NULL(reorder_slot.reorder_button);
	reorder_slot.reorder_button->warp_mouse(reorder_slot.reorder_button->get_size() / 2.0f);
	reorder_to_index = -1;
	reorder_mouse_y_delta = 0.0f;
	reorder_slot = Slot();
	_page_changed(page_index);
}

bool EditorPropertyArray::is_colored(ColorationMode p_mode) {
	return p_mode == COLORATION_CONTAINER_RESOURCE;
}

EditorPropertyArray::EditorPropertyArray() {
	object.instantiate();
	page_length = int(EDITOR_GET("interface/inspector/max_array_dictionary_items_per_page"));

	edit = memnew(Button);
	edit->set_h_size_flags(SIZE_EXPAND_FILL);
	edit->set_clip_text(true);
	edit->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyArray::_edit_pressed));
	edit->set_toggle_mode(true);
	SET_DRAG_FORWARDING_CD(edit, EditorPropertyArray);
	edit->connect(SceneStringName(draw), callable_mp(this, &EditorPropertyArray::_button_draw));
	add_child(edit);
	add_focusable(edit);

	change_type = memnew(PopupMenu);
	add_child(change_type);
	change_type->connect(SceneStringName(id_pressed), callable_mp(this, &EditorPropertyArray::_change_type_menu));
	changing_type_index = -1;

	subtype = Variant::NIL;
	subtype_hint = PROPERTY_HINT_NONE;
	subtype_hint_string = "";
	has_borders = true;
}

///////////////////// DICTIONARY ///////////////////////////

void EditorPropertyDictionary::initialize_dictionary(Variant &p_dictionary) {
	if (key_subtype != Variant::NIL || value_subtype != Variant::NIL) {
		Dictionary dict;
		StringName key_subtype_class;
		Ref<Script> key_subtype_script;
		if (key_subtype == Variant::OBJECT && !key_subtype_hint_string.is_empty() && ClassDB::class_exists(key_subtype_hint_string)) {
			key_subtype_class = key_subtype_hint_string;
		}
		StringName value_subtype_class;
		Ref<Script> value_subtype_script;
		if (value_subtype == Variant::OBJECT && !value_subtype_hint_string.is_empty() && ClassDB::class_exists(value_subtype_hint_string)) {
			value_subtype_class = value_subtype_hint_string;
		}
		dict.set_typed(key_subtype, key_subtype_class, key_subtype_script, value_subtype, value_subtype_class, value_subtype_script);
		p_dictionary = dict;
	} else {
		VariantInternal::initialize(&p_dictionary, Variant::DICTIONARY);
	}
}

void EditorPropertyDictionary::_property_changed(const String &p_property, Variant p_value, const String &p_name, bool p_changing) {
	if (p_value.get_type() == Variant::OBJECT && p_value.is_null()) {
		p_value = Variant(); // `EditorResourcePicker` resets to `Ref<Resource>()`. See GH-82716.
	}

	object->set(p_property, p_value);
	emit_changed(get_edited_property(), object->get_dict(), p_name, p_changing);
}

void EditorPropertyDictionary::_change_type(Object *p_button, int p_slot_index) {
	Button *button = Object::cast_to<Button>(p_button);
	int index = slots[p_slot_index].index;
	Rect2 rect = button->get_screen_rect();
	change_type->set_item_disabled(change_type->get_item_index(Variant::VARIANT_MAX), index < 0);
	change_type->reset_size();
	change_type->set_position(rect.get_end() - Vector2(change_type->get_contents_minimum_size().x, 0));
	change_type->popup();
	changing_type_index = index;
}

void EditorPropertyDictionary::_add_key_value() {
	// Do not allow nil as valid key. I experienced errors with this
	if (object->get_new_item_key().get_type() == Variant::NIL) {
		return;
	}

	Dictionary dict = object->get_dict().duplicate();
	Variant new_key = object->get_new_item_key();
	Variant new_value = object->get_new_item_value();
	dict[new_key] = new_value;

	Variant::Type type = new_key.get_type();
	new_key.zero();
	VariantInternal::initialize(&new_key, type);
	object->set_new_item_key(new_key);

	type = new_value.get_type();
	new_value.zero();
	VariantInternal::initialize(&new_value, type);
	object->set_new_item_value(new_value);

	object->set_dict(dict);
	slots[(dict.size() - 1) % page_length].update_prop_or_index();
	emit_changed(get_edited_property(), dict);
}

void EditorPropertyDictionary::_create_new_property_slot(int p_idx) {
	HBoxContainer *hbox = memnew(HBoxContainer);
	EditorProperty *prop = memnew(EditorPropertyNil);
	hbox->add_child(prop);

	bool use_key = p_idx == EditorPropertyDictionaryObject::NEW_KEY_INDEX;
	bool is_untyped_dict = (use_key ? key_subtype : value_subtype) == Variant::NIL;

	if (is_untyped_dict) {
		Button *edit_btn = memnew(Button);
		edit_btn->set_button_icon(get_editor_theme_icon(SNAME("Edit")));
		edit_btn->set_disabled(is_read_only());
		edit_btn->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyDictionary::_change_type).bind(edit_btn, slots.size()));
		hbox->add_child(edit_btn);
	} else if (p_idx >= 0) {
		Button *remove_btn = memnew(Button);
		remove_btn->set_button_icon(get_editor_theme_icon(SNAME("Remove")));
		remove_btn->set_disabled(is_read_only());
		remove_btn->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyDictionary::_remove_pressed).bind(slots.size()));
		hbox->add_child(remove_btn);
	}

	if (add_panel) {
		add_panel->get_child(0)->add_child(hbox);
	} else {
		property_vbox->add_child(hbox);
	}

	Slot slot;
	slot.prop = prop;
	slot.object = object;
	slot.container = hbox;
	int index = p_idx + (p_idx >= 0 ? page_index * page_length : 0);
	slot.set_index(index);
	slots.push_back(slot);
}

void EditorPropertyDictionary::_change_type_menu(int p_index) {
	ERR_FAIL_COND_MSG(
			changing_type_index == EditorPropertyDictionaryObject::NOT_CHANGING_TYPE,
			"Tried to change the type of a dict key or value, but nothing was selected.");

	Variant value;
	switch (changing_type_index) {
		case EditorPropertyDictionaryObject::NEW_KEY_INDEX:
		case EditorPropertyDictionaryObject::NEW_VALUE_INDEX:
			VariantInternal::initialize(&value, Variant::Type(p_index));
			if (changing_type_index == EditorPropertyDictionaryObject::NEW_KEY_INDEX) {
				object->set_new_item_key(value);
			} else {
				object->set_new_item_value(value);
			}
			update_property();
			break;

		default:
			Dictionary dict = object->get_dict().duplicate();
			Variant key = dict.get_key_at_index(changing_type_index);
			if (p_index < Variant::VARIANT_MAX) {
				VariantInternal::initialize(&value, Variant::Type(p_index));
				dict[key] = value;
			} else {
				dict.erase(key);
				object->set_dict(dict);
				for (Slot &slot : slots) {
					slot.update_prop_or_index();
				}
			}

			emit_changed(get_edited_property(), dict);
	}
}

void EditorPropertyDictionary::setup(PropertyHint p_hint, const String &p_hint_string) {
	PackedStringArray types = p_hint_string.split(";");
	if (types.size() > 0 && !types[0].is_empty()) {
		String key = types[0];
		int hint_key_subtype_separator = key.find_char(':');
		if (hint_key_subtype_separator >= 0) {
			String key_subtype_string = key.substr(0, hint_key_subtype_separator);
			int slash_pos = key_subtype_string.find_char('/');
			if (slash_pos >= 0) {
				key_subtype_hint = PropertyHint(key_subtype_string.substr(slash_pos + 1, key_subtype_string.size() - slash_pos - 1).to_int());
				key_subtype_string = key_subtype_string.substr(0, slash_pos);
			}

			key_subtype_hint_string = key.substr(hint_key_subtype_separator + 1, key.size() - hint_key_subtype_separator - 1);
			key_subtype = Variant::Type(key_subtype_string.to_int());

			Variant new_key = object->get_new_item_key();
			VariantInternal::initialize(&new_key, key_subtype);
			object->set_new_item_key(new_key);
		}
	}
	if (types.size() > 1 && !types[1].is_empty()) {
		String value = types[1];
		int hint_value_subtype_separator = value.find_char(':');
		if (hint_value_subtype_separator >= 0) {
			String value_subtype_string = value.substr(0, hint_value_subtype_separator);
			int slash_pos = value_subtype_string.find_char('/');
			if (slash_pos >= 0) {
				value_subtype_hint = PropertyHint(value_subtype_string.substr(slash_pos + 1, value_subtype_string.size() - slash_pos - 1).to_int());
				value_subtype_string = value_subtype_string.substr(0, slash_pos);
			}

			value_subtype_hint_string = value.substr(hint_value_subtype_separator + 1, value.size() - hint_value_subtype_separator - 1);
			value_subtype = Variant::Type(value_subtype_string.to_int());

			Variant new_value = object->get_new_item_value();
			VariantInternal::initialize(&new_value, value_subtype);
			object->set_new_item_value(new_value);
		}
	}
}

void EditorPropertyDictionary::update_property() {
	Variant updated_val = get_edited_property_value();

	String dict_type_name = "Dictionary";
	if (key_subtype != Variant::NIL || value_subtype != Variant::NIL) {
		String key_subtype_name = "Variant";
		if (key_subtype == Variant::OBJECT && (key_subtype_hint == PROPERTY_HINT_RESOURCE_TYPE || key_subtype_hint == PROPERTY_HINT_NODE_TYPE)) {
			key_subtype_name = key_subtype_hint_string;
		} else if (key_subtype != Variant::NIL) {
			key_subtype_name = Variant::get_type_name(key_subtype);
		}
		String value_subtype_name = "Variant";
		if (value_subtype == Variant::OBJECT && (value_subtype_hint == PROPERTY_HINT_RESOURCE_TYPE || value_subtype_hint == PROPERTY_HINT_NODE_TYPE)) {
			value_subtype_name = value_subtype_hint_string;
		} else if (value_subtype != Variant::NIL) {
			value_subtype_name = Variant::get_type_name(value_subtype);
		}
		dict_type_name += vformat("[%s, %s]", key_subtype_name, value_subtype_name);
	}

	if (updated_val.get_type() != Variant::DICTIONARY) {
		edit->set_text(vformat(TTR("(Nil) %s"), dict_type_name)); // This provides symmetry with the array property.
		edit->set_pressed(false);
		if (container) {
			set_bottom_editor(nullptr);
			memdelete(container);
			button_add_item = nullptr;
			container = nullptr;
			add_panel = nullptr;
			slots.clear();
		}
		return;
	}

	Dictionary dict = updated_val;
	object->set_dict(updated_val);

	edit->set_text(vformat(TTR("%s (size %d)"), dict_type_name, dict.size()));

	bool unfolded = get_edited_object()->editor_is_section_unfolded(get_edited_property());
	if (edit->is_pressed() != unfolded) {
		edit->set_pressed(unfolded);
	}

	if (unfolded) {
		updating = true;

		if (!container) {
			container = memnew(PanelContainer);
			container->set_mouse_filter(MOUSE_FILTER_STOP);
			add_child(container);
			set_bottom_editor(container);

			VBoxContainer *vbox = memnew(VBoxContainer);
			container->add_child(vbox);

			property_vbox = memnew(VBoxContainer);
			property_vbox->set_h_size_flags(SIZE_EXPAND_FILL);
			vbox->add_child(property_vbox);

			paginator = memnew(EditorPaginator);
			paginator->connect("page_changed", callable_mp(this, &EditorPropertyDictionary::_page_changed));
			vbox->add_child(paginator);

			for (int i = 0; i < page_length; i++) {
				_create_new_property_slot(slots.size());
			}

			add_panel = memnew(PanelContainer);
			property_vbox->add_child(add_panel);
			add_panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("DictionaryAddItem")));
			VBoxContainer *add_vbox = memnew(VBoxContainer);
			add_panel->add_child(add_vbox);

			_create_new_property_slot(EditorPropertyDictionaryObject::NEW_KEY_INDEX);
			_create_new_property_slot(EditorPropertyDictionaryObject::NEW_VALUE_INDEX);

			button_add_item = EditorInspector::create_inspector_action_button(TTR("Add Key/Value Pair"));
			button_add_item->set_button_icon(get_theme_icon(SNAME("Add"), EditorStringName(EditorIcons)));
			button_add_item->set_disabled(is_read_only());
			button_add_item->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyDictionary::_add_key_value));
			add_vbox->add_child(button_add_item);
		}

		int size = dict.size();

		int max_page = MAX(0, size - 1) / page_length;
		if (page_index > max_page) {
			_page_changed(max_page);
		}

		paginator->update(page_index, max_page);
		paginator->set_visible(max_page > 0);

		add_panel->set_visible(page_index == max_page);

		for (Slot &slot : slots) {
			bool slot_visible = slot.index < size;
			slot.container->set_visible(slot_visible);
			// If not visible no need to update it.
			if (!slot_visible) {
				continue;
			}
			Variant value;
			object->get_by_property_name(slot.prop_name, value);
			Variant::Type value_type = value.get_type();

			// Check if the editor property needs to be updated.
			bool value_as_id = Object::cast_to<EncodedObjectAsID>(value);
			if (value_type != slot.type || (value_type == Variant::OBJECT && value_as_id != slot.as_id)) {
				slot.as_id = value_as_id;
				slot.type = value_type;
				EditorProperty *new_prop = nullptr;
				if (value_type == Variant::OBJECT && value_as_id) {
					EditorPropertyObjectID *editor = memnew(EditorPropertyObjectID);
					editor->setup("Object");
					new_prop = editor;
				} else {
					bool use_key = slot.index == EditorPropertyDictionaryObject::NEW_KEY_INDEX;
					new_prop = EditorInspector::instantiate_property_editor(this, value_type, "", use_key ? key_subtype_hint : value_subtype_hint,
							use_key ? key_subtype_hint_string : value_subtype_hint_string, PROPERTY_USAGE_NONE);
				}
				new_prop->set_selectable(false);
				new_prop->set_use_folding(is_using_folding());
				new_prop->connect(SNAME("property_changed"), callable_mp(this, &EditorPropertyDictionary::_property_changed));
				new_prop->connect(SNAME("object_id_selected"), callable_mp(this, &EditorPropertyDictionary::_object_id_selected));
				new_prop->set_h_size_flags(SIZE_EXPAND_FILL);
				new_prop->set_read_only(is_read_only());
				slot.set_prop(new_prop);
			} else if (slot.index != EditorPropertyDictionaryObject::NEW_KEY_INDEX && slot.index != EditorPropertyDictionaryObject::NEW_VALUE_INDEX) {
				Variant key = dict.get_key_at_index(slot.index);
				String cs = key.get_construct_string();
				slot.prop->set_label(cs);
				slot.prop->set_tooltip_text(cs);
			}

			// We need to grab the focus of the property that is being changed, even if the type didn't actually changed.
			// Otherwise, focus will stay on the change type button, which is not very user friendly.
			if (changing_type_index == slot.index) {
				callable_mp(slot.prop, &EditorProperty::grab_focus).call_deferred(0);
				changing_type_index = EditorPropertyDictionaryObject::NOT_CHANGING_TYPE; // Reset to avoid grabbing focus again.
			}

			slot.prop->update_property();
		}
		updating = false;

	} else {
		if (container) {
			set_bottom_editor(nullptr);
			memdelete(container);
			button_add_item = nullptr;
			container = nullptr;
			add_panel = nullptr;
			slots.clear();
		}
	}
}

void EditorPropertyDictionary::_remove_pressed(int p_slot_index) {
	Dictionary dict = object->get_dict().duplicate();
	int index = slots[p_slot_index].index;
	dict.erase(dict.get_key_at_index(index));

	emit_changed(get_edited_property(), dict);
}

void EditorPropertyDictionary::_object_id_selected(const StringName &p_property, ObjectID p_id) {
	emit_signal(SNAME("object_id_selected"), p_property, p_id);
}

void EditorPropertyDictionary::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED:
		case NOTIFICATION_ENTER_TREE: {
			change_type->clear();
			change_type->add_icon_item(get_editor_theme_icon(SNAME("Remove")), TTR("Remove Item"), Variant::VARIANT_MAX);
			change_type->add_separator();
			for (int i = 0; i < Variant::VARIANT_MAX; i++) {
				if (i == Variant::CALLABLE || i == Variant::SIGNAL || i == Variant::RID) {
					// These types can't be constructed or serialized properly, so skip them.
					continue;
				}

				String type = Variant::get_type_name(Variant::Type(i));
				change_type->add_icon_item(get_editor_theme_icon(type), type, i);
			}

			if (button_add_item) {
				button_add_item->set_button_icon(get_editor_theme_icon(SNAME("Add")));
				add_panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("DictionaryAddItem")));
			}
		} break;
	}
}

void EditorPropertyDictionary::_edit_pressed() {
	Variant prop_val = get_edited_property_value();
	if (prop_val.get_type() == Variant::NIL && edit->is_pressed()) {
		initialize_dictionary(prop_val);
		emit_changed(get_edited_property(), prop_val);
	}

	get_edited_object()->editor_set_section_unfold(get_edited_property(), edit->is_pressed());
	update_property();
}

void EditorPropertyDictionary::_page_changed(int p_page) {
	page_index = p_page;
	int i = p_page * page_length;
	for (Slot &slot : slots) {
		if (slot.index > -1) {
			slot.set_index(i);
			i++;
		}
	}
	if (updating) {
		return;
	}
	update_property();
}

bool EditorPropertyDictionary::is_colored(ColorationMode p_mode) {
	return p_mode == COLORATION_CONTAINER_RESOURCE;
}

EditorPropertyDictionary::EditorPropertyDictionary() {
	object.instantiate();
	page_length = int(EDITOR_GET("interface/inspector/max_array_dictionary_items_per_page"));

	edit = memnew(Button);
	edit->set_h_size_flags(SIZE_EXPAND_FILL);
	edit->set_clip_text(true);
	edit->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyDictionary::_edit_pressed));
	edit->set_toggle_mode(true);
	add_child(edit);
	add_focusable(edit);

	container = nullptr;
	button_add_item = nullptr;
	paginator = nullptr;
	change_type = memnew(PopupMenu);
	add_child(change_type);
	change_type->connect(SceneStringName(id_pressed), callable_mp(this, &EditorPropertyDictionary::_change_type_menu));
	changing_type_index = -1;
	has_borders = true;

	key_subtype = Variant::NIL;
	key_subtype_hint = PROPERTY_HINT_NONE;
	key_subtype_hint_string = "";

	value_subtype = Variant::NIL;
	value_subtype_hint = PROPERTY_HINT_NONE;
	value_subtype_hint_string = "";
}

///////////////////// LOCALIZABLE STRING ///////////////////////////

void EditorPropertyLocalizableString::_property_changed(const String &p_property, const Variant &p_value, const String &p_name, bool p_changing) {
	if (p_property.begins_with("indices")) {
		int index = p_property.get_slice("/", 1).to_int();

		Dictionary dict = object->get_dict().duplicate();
		Variant key = dict.get_key_at_index(index);
		dict[key] = p_value;

		object->set_dict(dict);
		emit_changed(get_edited_property(), dict, "", true);
	}
}

void EditorPropertyLocalizableString::_add_locale_popup() {
	locale_select->popup_locale_dialog();
}

void EditorPropertyLocalizableString::_add_locale(const String &p_locale) {
	Dictionary dict = object->get_dict().duplicate();
	object->set_new_item_key(p_locale);
	object->set_new_item_value(String());
	dict[object->get_new_item_key()] = object->get_new_item_value();

	emit_changed(get_edited_property(), dict, "", false);
	update_property();
}

void EditorPropertyLocalizableString::_remove_item(Object *p_button, int p_index) {
	Dictionary dict = object->get_dict().duplicate();

	Variant key = dict.get_key_at_index(p_index);
	dict.erase(key);

	emit_changed(get_edited_property(), dict, "", false);
	update_property();
}

void EditorPropertyLocalizableString::update_property() {
	Variant updated_val = get_edited_property_value();

	if (updated_val.get_type() == Variant::NIL) {
		edit->set_text(TTR("Localizable String (Nil)")); // This provides symmetry with the array property.
		edit->set_pressed(false);
		if (container) {
			set_bottom_editor(nullptr);
			memdelete(container);
			button_add_item = nullptr;
			container = nullptr;
		}
		return;
	}

	Dictionary dict = updated_val;
	object->set_dict(dict);

	edit->set_text(vformat(TTR("Localizable String (size %d)"), dict.size()));

	bool unfolded = get_edited_object()->editor_is_section_unfolded(get_edited_property());
	if (edit->is_pressed() != unfolded) {
		edit->set_pressed(unfolded);
	}

	if (unfolded) {
		updating = true;

		if (!container) {
			container = memnew(MarginContainer);
			container->set_theme_type_variation("MarginContainer4px");
			add_child(container);
			set_bottom_editor(container);

			VBoxContainer *vbox = memnew(VBoxContainer);
			container->add_child(vbox);

			property_vbox = memnew(VBoxContainer);
			property_vbox->set_h_size_flags(SIZE_EXPAND_FILL);
			vbox->add_child(property_vbox);

			paginator = memnew(EditorPaginator);
			paginator->connect("page_changed", callable_mp(this, &EditorPropertyLocalizableString::_page_changed));
			vbox->add_child(paginator);
		} else {
			// Queue children for deletion, deleting immediately might cause errors.
			for (int i = property_vbox->get_child_count() - 1; i >= 0; i--) {
				property_vbox->get_child(i)->queue_free();
			}
		}

		int size = dict.size();

		int max_page = MAX(0, size - 1) / page_length;
		page_index = MIN(page_index, max_page);

		paginator->update(page_index, max_page);
		paginator->set_visible(max_page > 0);

		int offset = page_index * page_length;

		int amount = MIN(size - offset, page_length);

		for (int i = 0; i < amount; i++) {
			String prop_name;
			Variant key;
			Variant value;

			prop_name = "indices/" + itos(i + offset);
			key = dict.get_key_at_index(i + offset);
			value = dict.get_value_at_index(i + offset);

			EditorProperty *prop = memnew(EditorPropertyText);

			prop->set_object_and_property(object.ptr(), prop_name);
			int remove_index = 0;

			String cs = key.get_construct_string();
			prop->set_label(cs);
			prop->set_tooltip_text(cs);
			remove_index = i + offset;

			prop->set_selectable(false);
			prop->connect("property_changed", callable_mp(this, &EditorPropertyLocalizableString::_property_changed));
			prop->connect("object_id_selected", callable_mp(this, &EditorPropertyLocalizableString::_object_id_selected));

			HBoxContainer *hbox = memnew(HBoxContainer);
			property_vbox->add_child(hbox);
			hbox->add_child(prop);
			prop->set_h_size_flags(SIZE_EXPAND_FILL);
			Button *edit_btn = memnew(Button);
			edit_btn->set_button_icon(get_editor_theme_icon(SNAME("Remove")));
			hbox->add_child(edit_btn);
			edit_btn->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyLocalizableString::_remove_item).bind(edit_btn, remove_index));

			prop->update_property();
		}

		if (page_index == max_page) {
			button_add_item = EditorInspector::create_inspector_action_button(TTR("Add Translation"));
			button_add_item->set_button_icon(get_editor_theme_icon(SNAME("Add")));
			button_add_item->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyLocalizableString::_add_locale_popup));
			property_vbox->add_child(button_add_item);
		}

		updating = false;

	} else {
		if (container) {
			set_bottom_editor(nullptr);
			memdelete(container);
			button_add_item = nullptr;
			container = nullptr;
		}
	}
}

void EditorPropertyLocalizableString::_object_id_selected(const StringName &p_property, ObjectID p_id) {
	emit_signal(SNAME("object_id_selected"), p_property, p_id);
}

void EditorPropertyLocalizableString::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED:
		case NOTIFICATION_ENTER_TREE: {
			if (button_add_item) {
				button_add_item->set_button_icon(get_editor_theme_icon(SNAME("Add")));
			}
		} break;
	}
}

void EditorPropertyLocalizableString::_edit_pressed() {
	Variant prop_val = get_edited_property_value();
	if (prop_val.get_type() == Variant::NIL && edit->is_pressed()) {
		VariantInternal::initialize(&prop_val, Variant::DICTIONARY);
		get_edited_object()->set(get_edited_property(), prop_val);
	}

	get_edited_object()->editor_set_section_unfold(get_edited_property(), edit->is_pressed());
	update_property();
}

void EditorPropertyLocalizableString::_page_changed(int p_page) {
	if (updating) {
		return;
	}
	page_index = p_page;
	update_property();
}

EditorPropertyLocalizableString::EditorPropertyLocalizableString() {
	object.instantiate();
	page_length = int(EDITOR_GET("interface/inspector/max_array_dictionary_items_per_page"));

	edit = memnew(Button);
	edit->set_h_size_flags(SIZE_EXPAND_FILL);
	edit->set_clip_text(true);
	edit->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyLocalizableString::_edit_pressed));
	edit->set_toggle_mode(true);
	add_child(edit);
	add_focusable(edit);

	container = nullptr;
	button_add_item = nullptr;
	paginator = nullptr;
	updating = false;

	locale_select = memnew(EditorLocaleDialog);
	locale_select->connect("locale_selected", callable_mp(this, &EditorPropertyLocalizableString::_add_locale));
	add_child(locale_select);
}
