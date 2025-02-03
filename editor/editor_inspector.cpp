/**************************************************************************/
/*  editor_inspector.cpp                                                  */
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

#include "editor_inspector.h"

#include "array_property_edit.h"
#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "dictionary_property_edit.h"
#include "editor_feature_profile.h"
#include "editor_node.h"
#include "editor_property_name_processor.h"
#include "editor_scale.h"
#include "editor_settings.h"
#include "multi_node_edit.h"
#include "scene/property_utils.h"
#include "scene/resources/packed_scene.h"

Size2 EditorProperty::get_minimum_size() const {
	Size2 ms;
	Ref<Font> font = get_font("font", "Tree");
	ms.height = font->get_height();

	for (int i = 0; i < get_child_count(); i++) {
		Control *c = Object::cast_to<Control>(get_child(i));
		if (!c) {
			continue;
		}
		if (c->is_set_as_toplevel()) {
			continue;
		}
		if (!c->is_visible()) {
			continue;
		}
		if (c == bottom_editor) {
			continue;
		}

		Size2 minsize = c->get_combined_minimum_size();
		ms.width = MAX(ms.width, minsize.width);
		ms.height = MAX(ms.height, minsize.height);
	}

	if (keying) {
		Ref<Texture> key = get_icon("Key", "EditorIcons");
		ms.width += key->get_width() + get_constant("hseparator", "Tree");
	}

	if (checkable) {
		Ref<Texture> check = get_icon("checked", "CheckBox");
		ms.width += check->get_width() + get_constant("hseparation", "CheckBox") + get_constant("hseparator", "Tree");
	}

	if (bottom_editor != nullptr && bottom_editor->is_visible()) {
		ms.height += get_constant("vseparation");
		Size2 bems = bottom_editor->get_combined_minimum_size();
		//bems.width += get_constant("item_margin", "Tree");
		ms.height += bems.height;
		ms.width = MAX(ms.width, bems.width);
	}

	return ms;
}

void EditorProperty::emit_changed(const StringName &p_property, const Variant &p_value, const StringName &p_field, bool p_changing) {
	Variant args[4] = { p_property, p_value, p_field, p_changing };
	const Variant *argptrs[4] = { &args[0], &args[1], &args[2], &args[3] };

	emit_signal("property_changed", (const Variant **)argptrs, 4);
}

void EditorProperty::_notification(int p_what) {
	if (p_what == NOTIFICATION_SORT_CHILDREN) {
		Size2 size = get_size();
		Rect2 rect;
		Rect2 bottom_rect;

		right_child_rect = Rect2();
		bottom_child_rect = Rect2();

		{
			int child_room = size.width * (1.0 - split_ratio);
			Ref<Font> font = get_font("font", "Tree");
			int height = font->get_height();
			bool no_children = true;

			//compute room needed
			for (int i = 0; i < get_child_count(); i++) {
				Control *c = Object::cast_to<Control>(get_child(i));
				if (!c) {
					continue;
				}
				if (c->is_set_as_toplevel()) {
					continue;
				}
				if (c == bottom_editor) {
					continue;
				}

				Size2 minsize = c->get_combined_minimum_size();
				child_room = MAX(child_room, minsize.width);
				height = MAX(height, minsize.height);
				no_children = false;
			}

			if (no_children) {
				text_size = size.width;
				rect = Rect2(size.width - 1, 0, 1, height);
			} else {
				text_size = MAX(0, size.width - (child_room + 4 * EDSCALE));
				rect = Rect2(size.width - child_room, 0, child_room, height);
			}

			if (bottom_editor) {
				int m = 0; //get_constant("item_margin", "Tree");

				bottom_rect = Rect2(m, rect.size.height + get_constant("vseparation"), size.width - m, bottom_editor->get_combined_minimum_size().height);
			}

			if (keying) {
				Ref<Texture> key;

				if (use_keying_next()) {
					key = get_icon("KeyNext", "EditorIcons");
				} else {
					key = get_icon("Key", "EditorIcons");
				}

				rect.size.x -= key->get_width() + get_constant("hseparator", "Tree");

				if (no_children) {
					text_size -= key->get_width() + 4 * EDSCALE;
				}
			}
		}

		//set children
		for (int i = 0; i < get_child_count(); i++) {
			Control *c = Object::cast_to<Control>(get_child(i));
			if (!c) {
				continue;
			}
			if (c->is_set_as_toplevel()) {
				continue;
			}
			if (c == bottom_editor) {
				continue;
			}

			fit_child_in_rect(c, rect);
			right_child_rect = rect;
		}

		if (bottom_editor) {
			fit_child_in_rect(bottom_editor, bottom_rect);
			bottom_child_rect = bottom_rect;
		}

		update(); //need to redraw text
	}

	if (p_what == NOTIFICATION_DRAW) {
		Ref<Font> font = get_font("font", "Tree");
		Color dark_color = get_color("dark_color_2", "Editor");

		Size2 size = get_size();
		if (bottom_editor) {
			size.height = bottom_editor->get_margin(MARGIN_TOP);
		} else if (label_reference) {
			size.height = label_reference->get_size().height;
		}

		Ref<StyleBox> sb;
		if (selected) {
			sb = get_stylebox("bg_selected");
		} else {
			sb = get_stylebox("bg");
		}

		draw_style_box(sb, Rect2(Vector2(), size));

		if (draw_top_bg && right_child_rect != Rect2()) {
			draw_rect(right_child_rect, dark_color);
		}
		if (bottom_child_rect != Rect2()) {
			draw_rect(bottom_child_rect, dark_color);
		}

		Color color;
		if (draw_red) {
			color = get_color("warning_color");
		} else {
			color = get_color("property_color");
		}
		if (label.find(".") != -1) {
			color.a = 0.5; //this should be un-hacked honestly, as it's used for editor overrides
		}

		int ofs = get_constant("font_offset");
		int text_limit = text_size - ofs;

		if (checkable) {
			Ref<Texture> checkbox;
			if (checked) {
				checkbox = get_icon("GuiChecked", "EditorIcons");
			} else {
				checkbox = get_icon("GuiUnchecked", "EditorIcons");
			}

			Color color2(1, 1, 1);
			if (check_hover) {
				color2.r *= 1.2;
				color2.g *= 1.2;
				color2.b *= 1.2;
			}
			check_rect = Rect2(ofs, ((size.height - checkbox->get_height()) / 2), checkbox->get_width(), checkbox->get_height());
			draw_texture(checkbox, check_rect.position, color2);

			int check_ofs = get_constant("hseparator", "Tree") + checkbox->get_width() + get_constant("hseparation", "CheckBox");
			ofs += check_ofs;
			text_limit -= check_ofs;
		} else {
			check_rect = Rect2();
		}

		if (can_revert) {
			Ref<Texture> reload_icon = get_icon("ReloadSmall", "EditorIcons");
			text_limit -= reload_icon->get_width() + get_constant("hseparator", "Tree") * 2;
			revert_rect = Rect2(ofs + text_limit, (size.height - reload_icon->get_height()) / 2, reload_icon->get_width(), reload_icon->get_height());

			Color color2(1, 1, 1);
			if (revert_hover) {
				color2.r *= 1.2;
				color2.g *= 1.2;
				color2.b *= 1.2;
			}

			draw_texture(reload_icon, revert_rect.position, color2);
		} else {
			revert_rect = Rect2();
		}

		if (!pin_hidden && is_pinned) {
			Ref<Texture> pinned_icon = get_icon("Pin", "EditorIcons");
			int margin_w = get_constant("hseparator", "Tree") * 2;
			text_limit -= margin_w + pinned_icon->get_width();
			int text_w = MIN(font->get_string_size(label).x, text_limit);
			draw_texture(pinned_icon, Vector2(ofs + text_w + margin_w, (size.height - pinned_icon->get_height()) / 2), color);
		}

		int v_ofs = (size.height - font->get_height()) / 2;
		draw_string(font, Point2(ofs, v_ofs + font->get_ascent()), label, color, text_limit);

		if (keying) {
			Ref<Texture> key;

			if (use_keying_next()) {
				key = get_icon("KeyNext", "EditorIcons");
			} else {
				key = get_icon("Key", "EditorIcons");
			}

			ofs = size.width - key->get_width() - get_constant("hseparator", "Tree");

			Color color2(1, 1, 1);
			if (keying_hover) {
				color2.r *= 1.2;
				color2.g *= 1.2;
				color2.b *= 1.2;
			}
			keying_rect = Rect2(ofs, ((size.height - key->get_height()) / 2), key->get_width(), key->get_height());
			draw_texture(key, keying_rect.position, color2);
		} else {
			keying_rect = Rect2();
		}
	}
}

void EditorProperty::set_label(const String &p_label) {
	label = p_label;
	update();
}

String EditorProperty::get_label() const {
	return label;
}

Object *EditorProperty::get_edited_object() {
	return object;
}

StringName EditorProperty::get_edited_property() {
	return property;
}

void EditorProperty::update_property() {
	if (get_script_instance()) {
		get_script_instance()->call("update_property");
	}
}

void EditorProperty::set_read_only(bool p_read_only) {
	read_only = p_read_only;
}

bool EditorProperty::is_read_only() const {
	return read_only;
}

Variant EditorPropertyRevert::get_property_revert_value(Object *p_object, const StringName &p_property, bool *r_is_valid) {
	if (p_object->has_method("property_can_revert") && p_object->call("property_can_revert", p_property)) {
		if (r_is_valid) {
			*r_is_valid = true;
		}
		return p_object->call("property_get_revert", p_property);
	}

	return PropertyUtils::get_property_default_value(p_object, p_property, r_is_valid);
}

bool EditorPropertyRevert::can_property_revert(Object *p_object, const StringName &p_property) {
	bool is_valid_revert = false;
	Variant revert_value = EditorPropertyRevert::get_property_revert_value(p_object, p_property, &is_valid_revert);
	if (!is_valid_revert) {
		return false;
	}
	Variant current_value = p_object->get(p_property);
	return PropertyUtils::is_property_value_different(current_value, revert_value);
}

void EditorProperty::update_revert_and_pin_status() {
	if (property == StringName()) {
		return; //no property, so nothing to do
	}

	bool new_is_pinned = false;
	if (can_pin) {
		Node *node = Object::cast_to<Node>(object);
		CRASH_COND(!node);
		new_is_pinned = node->is_property_pinned(property);
	}
	bool new_can_revert = EditorPropertyRevert::can_property_revert(object, property);

	if (new_can_revert != can_revert || new_is_pinned != is_pinned) {
		can_revert = new_can_revert;
		is_pinned = new_is_pinned;
		update();
	}
}

bool EditorProperty::use_keying_next() const {
	List<PropertyInfo> plist;
	object->get_property_list(&plist, true);

	for (List<PropertyInfo>::Element *I = plist.front(); I; I = I->next()) {
		PropertyInfo &p = I->get();

		if (p.name == property) {
			return (p.usage & PROPERTY_USAGE_KEYING_INCREMENTS);
		}
	}

	return false;
}
void EditorProperty::set_checkable(bool p_checkable) {
	checkable = p_checkable;
	update();
	queue_sort();
}

bool EditorProperty::is_checkable() const {
	return checkable;
}

void EditorProperty::set_checked(bool p_checked) {
	checked = p_checked;
	update();
}

bool EditorProperty::is_checked() const {
	return checked;
}

void EditorProperty::set_draw_red(bool p_draw_red) {
	draw_red = p_draw_red;
	update();
}

void EditorProperty::set_keying(bool p_keying) {
	keying = p_keying;
	update();
	queue_sort();
}

bool EditorProperty::is_keying() const {
	return keying;
}

bool EditorProperty::is_draw_red() const {
	return draw_red;
}

void EditorProperty::_focusable_focused(int p_index) {
	if (!selectable) {
		return;
	}
	bool already_selected = selected;
	selected = true;
	selected_focusable = p_index;
	update();
	if (!already_selected && selected) {
		emit_signal("selected", property, selected_focusable);
	}
}

void EditorProperty::add_focusable(Control *p_control) {
	p_control->connect("focus_entered", this, "_focusable_focused", varray(focusables.size()));
	focusables.push_back(p_control);
}

void EditorProperty::select(int p_focusable) {
	bool already_selected = selected;

	if (p_focusable >= 0) {
		ERR_FAIL_INDEX(p_focusable, focusables.size());
		focusables[p_focusable]->grab_focus();
	} else {
		selected = true;
		update();
	}

	if (!already_selected && selected) {
		emit_signal("selected", property, selected_focusable);
	}
}

void EditorProperty::deselect() {
	selected = false;
	selected_focusable = -1;
	update();
}

bool EditorProperty::is_selected() const {
	return selected;
}

void EditorProperty::_gui_input(const Ref<InputEvent> &p_event) {
	if (property == StringName()) {
		return;
	}

	Ref<InputEventMouse> me = p_event;

	if (me.is_valid()) {
		bool button_left = me->get_button_mask() & BUTTON_MASK_LEFT;

		bool new_keying_hover = keying_rect.has_point(me->get_position()) && !button_left;
		if (new_keying_hover != keying_hover) {
			keying_hover = new_keying_hover;
			update();
		}

		bool new_revert_hover = revert_rect.has_point(me->get_position()) && !button_left;
		if (new_revert_hover != revert_hover) {
			revert_hover = new_revert_hover;
			update();
		}

		bool new_check_hover = check_rect.has_point(me->get_position()) && !button_left;
		if (new_check_hover != check_hover) {
			check_hover = new_check_hover;
			update();
		}
	}

	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {
		if (!selected && selectable) {
			selected = true;
			emit_signal("selected", property, -1);
			update();
		}

		if (keying_rect.has_point(mb->get_position())) {
			emit_signal("property_keyed", property, use_keying_next());

			if (use_keying_next()) {
				if (property == "frame_coords" && (object->is_class("Sprite") || object->is_class("Sprite3D"))) {
					Vector2 new_coords = object->get(property);
					new_coords.x++;
					if (new_coords.x >= object->get("hframes").operator int64_t()) {
						new_coords.x = 0;
						new_coords.y++;
					}

					call_deferred("emit_changed", property, new_coords, "", false);
				} else {
					call_deferred("emit_changed", property, object->get(property).operator int64_t() + 1, "", false);
				}

				call_deferred("update_property");
			}
		}

		if (revert_rect.has_point(mb->get_position())) {
			bool is_valid_revert = false;
			Variant revert_value = EditorPropertyRevert::get_property_revert_value(object, property, &is_valid_revert);
			ERR_FAIL_COND(!is_valid_revert);
			emit_changed(property, revert_value);
			update_property();
		}

		if (check_rect.has_point(mb->get_position())) {
			checked = !checked;
			update();
			emit_signal("property_checked", property, checked);
		}
	} else if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == BUTTON_RIGHT) {
		_update_popup();
		if (menu->get_item_count()) {
			menu->set_position(get_global_mouse_position());
			menu->set_as_minsize();
			menu->popup();
			select();
		}
	}
}

void EditorProperty::_unhandled_key_input(const Ref<InputEvent> &p_event) {
	if (!selected || !is_visible_in_tree()) {
		return;
	}

	const Ref<InputEventKey> k = p_event;

	if (k.is_valid() && k->is_pressed()) {
		if (ED_IS_SHORTCUT("property_editor/copy_property", p_event)) {
			_menu_option(MENU_COPY_PROPERTY);
			accept_event();
		} else if (ED_IS_SHORTCUT("property_editor/paste_property", p_event) && !is_read_only()) {
			_menu_option(MENU_PASTE_PROPERTY);
			accept_event();
		} else if (ED_IS_SHORTCUT("property_editor/copy_property_path", p_event)) {
			_menu_option(MENU_COPY_PROPERTY_PATH);
			accept_event();
		}
	}
}

void EditorProperty::set_label_reference(Control *p_control) {
	label_reference = p_control;
}
void EditorProperty::set_bottom_editor(Control *p_control) {
	bottom_editor = p_control;
}
Variant EditorProperty::get_drag_data(const Point2 &p_point) {
	if (property == StringName()) {
		return Variant();
	}

	Dictionary dp;
	dp["type"] = "obj_property";
	dp["object"] = object;
	dp["property"] = property;
	dp["value"] = object->get(property);

	Label *label = memnew(Label);
	label->set_text(property);
	set_drag_preview(label);
	return dp;
}

void EditorProperty::set_use_folding(bool p_use_folding) {
	use_folding = p_use_folding;
}

bool EditorProperty::is_using_folding() const {
	return use_folding;
}

void EditorProperty::expand_all_folding() {
}

void EditorProperty::collapse_all_folding() {
}

void EditorProperty::set_selectable(bool p_selectable) {
	selectable = p_selectable;
}

bool EditorProperty::is_selectable() const {
	return selectable;
}

void EditorProperty::set_name_split_ratio(float p_ratio) {
	split_ratio = p_ratio;
}

float EditorProperty::get_name_split_ratio() const {
	return split_ratio;
}

void EditorProperty::set_object_and_property(Object *p_object, const StringName &p_property) {
	object = p_object;
	property = p_property;
	_update_pin_flags();
}

static bool _is_value_potential_override(Node *p_node, const String &p_property) {
	// Consider a value is potentially overriding another if either of the following is true:
	// a) The node is foreign (inheriting or an instance), so the original value may come from another scene.
	// b) The node belongs to the scene, but the original value comes from somewhere but the builtin class (i.e., a script).
	Node *edited_scene = EditorNode::get_singleton()->get_edited_scene();
	Vector<SceneState::PackState> states_stack = PropertyUtils::get_node_states_stack(p_node, edited_scene);
	if (states_stack.size()) {
		return true;
	} else {
		bool is_valid_default = false;
		bool is_class_default = false;
		PropertyUtils::get_property_default_value(p_node, p_property, &is_valid_default, &states_stack, false, nullptr, &is_class_default);
		return !is_class_default;
	}
}

void EditorProperty::_update_pin_flags() {
	can_pin = false;
	pin_hidden = true;
	if (read_only) {
		return;
	}
	if (Node *node = Object::cast_to<Node>(object)) {
		// Avoid errors down the road by ignoring nodes which are not part of a scene
		if (!node->get_owner()) {
			bool is_scene_root = false;
			for (int i = 0; i < EditorNode::get_singleton()->get_editor_data().get_edited_scene_count(); ++i) {
				if (EditorNode::get_singleton()->get_editor_data().get_edited_scene_root(i) == node) {
					is_scene_root = true;
					break;
				}
			}
			if (!is_scene_root) {
				return;
			}
		}
		if (!_is_value_potential_override(node, property)) {
			return;
		}
		pin_hidden = false;
		{
			Set<StringName> storable_properties;
			node->get_storable_properties(storable_properties);
			if (storable_properties.has(node->get_property_store_alias(property))) {
				can_pin = true;
			}
		}
	}
}

Control *EditorProperty::make_custom_tooltip(const String &p_text) const {
	EditorHelpBit *help_bit = memnew(EditorHelpBit);
	help_bit->add_style_override("panel", get_stylebox("panel", "TooltipPanel"));
	help_bit->get_rich_text()->set_fixed_size_to_width(360 * EDSCALE);

	Vector<String> slices = p_text.split("::", false);
	if (!slices.empty()) {
		String property_name = slices[0].strip_edges();
		String text = TTR("Property:") + " [u][b]" + property_name + "[/b][/u]";

		if (slices.size() > 1) {
			String property_doc = slices[1].strip_edges();
			if (property_name != property_doc) {
				text += "\n" + property_doc;
			}
		}
		help_bit->call_deferred("set_text", text); //hack so it uses proper theme once inside scene
	}

	return help_bit;
}

void EditorProperty::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_label", "text"), &EditorProperty::set_label);
	ClassDB::bind_method(D_METHOD("get_label"), &EditorProperty::get_label);

	ClassDB::bind_method(D_METHOD("set_read_only", "read_only"), &EditorProperty::set_read_only);
	ClassDB::bind_method(D_METHOD("is_read_only"), &EditorProperty::is_read_only);

	ClassDB::bind_method(D_METHOD("set_checkable", "checkable"), &EditorProperty::set_checkable);
	ClassDB::bind_method(D_METHOD("is_checkable"), &EditorProperty::is_checkable);

	ClassDB::bind_method(D_METHOD("set_checked", "checked"), &EditorProperty::set_checked);
	ClassDB::bind_method(D_METHOD("is_checked"), &EditorProperty::is_checked);

	ClassDB::bind_method(D_METHOD("set_draw_red", "draw_red"), &EditorProperty::set_draw_red);
	ClassDB::bind_method(D_METHOD("is_draw_red"), &EditorProperty::is_draw_red);

	ClassDB::bind_method(D_METHOD("set_keying", "keying"), &EditorProperty::set_keying);
	ClassDB::bind_method(D_METHOD("is_keying"), &EditorProperty::is_keying);

	ClassDB::bind_method(D_METHOD("get_edited_property"), &EditorProperty::get_edited_property);
	ClassDB::bind_method(D_METHOD("get_edited_object"), &EditorProperty::get_edited_object);

	ClassDB::bind_method(D_METHOD("_gui_input"), &EditorProperty::_gui_input);
	ClassDB::bind_method(D_METHOD("_menu_option", "option"), &EditorProperty::_menu_option);
	ClassDB::bind_method(D_METHOD("_unhandled_key_input"), &EditorProperty::_unhandled_key_input);
	ClassDB::bind_method(D_METHOD("_focusable_focused"), &EditorProperty::_focusable_focused);

	ClassDB::bind_method(D_METHOD("add_focusable", "control"), &EditorProperty::add_focusable);
	ClassDB::bind_method(D_METHOD("set_bottom_editor", "editor"), &EditorProperty::set_bottom_editor);

	ClassDB::bind_method(D_METHOD("emit_changed", "property", "value", "field", "changing"), &EditorProperty::emit_changed, DEFVAL(StringName()), DEFVAL(false));

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "label"), "set_label", "get_label");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "read_only"), "set_read_only", "is_read_only");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "checkable"), "set_checkable", "is_checkable");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "checked"), "set_checked", "is_checked");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "draw_red"), "set_draw_red", "is_draw_red");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "keying"), "set_keying", "is_keying");
	ADD_SIGNAL(MethodInfo("property_changed", PropertyInfo(Variant::STRING, "property"), PropertyInfo(Variant::NIL, "value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT), PropertyInfo(Variant::STRING, "field"), PropertyInfo(Variant::BOOL, "changing")));
	ADD_SIGNAL(MethodInfo("multiple_properties_changed", PropertyInfo(Variant::POOL_STRING_ARRAY, "properties"), PropertyInfo(Variant::ARRAY, "value")));
	ADD_SIGNAL(MethodInfo("property_keyed", PropertyInfo(Variant::STRING, "property")));
	ADD_SIGNAL(MethodInfo("property_keyed_with_value", PropertyInfo(Variant::STRING, "property"), PropertyInfo(Variant::NIL, "value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
	ADD_SIGNAL(MethodInfo("property_checked", PropertyInfo(Variant::STRING, "property"), PropertyInfo(Variant::BOOL, "checked")));
	ADD_SIGNAL(MethodInfo("property_pinned", PropertyInfo(Variant::STRING, "property"), PropertyInfo(Variant::BOOL, "pinned")));
	ADD_SIGNAL(MethodInfo("resource_selected", PropertyInfo(Variant::STRING, "path"), PropertyInfo(Variant::OBJECT, "resource", PROPERTY_HINT_RESOURCE_TYPE, "Resource")));
	ADD_SIGNAL(MethodInfo("object_id_selected", PropertyInfo(Variant::STRING, "property"), PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("selected", PropertyInfo(Variant::STRING, "path"), PropertyInfo(Variant::INT, "focusable_idx")));

	MethodInfo vm;
	vm.name = "update_property";
	BIND_VMETHOD(vm);

	ClassDB::bind_method(D_METHOD("_update_revert_and_pin_status"), &EditorProperty::update_revert_and_pin_status);
}

EditorProperty::EditorProperty() {
	draw_top_bg = true;
	object = nullptr;
	split_ratio = 0.5;
	selectable = true;
	text_size = 0;
	read_only = false;
	checkable = false;
	checked = false;
	draw_red = false;
	keying = false;
	keying_hover = false;
	revert_hover = false;
	check_hover = false;
	can_revert = false;
	can_pin = false;
	pin_hidden = false;
	is_pinned = false;
	use_folding = false;
	property_usage = 0;
	selected = false;
	selected_focusable = -1;
	label_reference = nullptr;
	bottom_editor = nullptr;
	menu = nullptr;

	set_process_unhandled_key_input(true);
}

void EditorProperty::_update_popup() {
	if (menu) {
		menu->clear();
	} else {
		menu = memnew(PopupMenu);
		add_child(menu);
		menu->connect("id_pressed", this, "_menu_option");
	}

	menu->add_shortcut(ED_GET_SHORTCUT("property_editor/copy_property"), MENU_COPY_PROPERTY);
	menu->add_shortcut(ED_GET_SHORTCUT("property_editor/paste_property"), MENU_PASTE_PROPERTY);
	menu->add_shortcut(ED_GET_SHORTCUT("property_editor/copy_property_path"), MENU_COPY_PROPERTY_PATH);
	menu->set_item_disabled(MENU_PASTE_PROPERTY, is_read_only());

	if (!pin_hidden) {
		menu->add_separator();
		if (can_pin) {
			menu->add_check_item(TTR("Pin value"), MENU_PIN_VALUE);
			menu->set_item_checked(menu->get_item_index(MENU_PIN_VALUE), is_pinned);
			menu->set_item_tooltip(menu->get_item_index(MENU_PIN_VALUE), TTR("Pinning a value forces it to be saved even if it's equal to the default."));
		} else {
			menu->add_check_item(vformat(TTR("Pin value [Disabled because '%s' is editor-only]"), property), MENU_PIN_VALUE);
			menu->set_item_disabled(menu->get_item_index(MENU_PIN_VALUE), true);
		}
	}
}

void EditorProperty::_menu_option(int p_option) {
	switch (p_option) {
		case MENU_PIN_VALUE: {
			emit_signal("property_pinned", property, !is_pinned);
			update();
		} break;
		case MENU_COPY_PROPERTY: {
			EditorNode::get_singleton()->get_inspector()->set_property_clipboard(object->get(property));
		} break;
		case MENU_PASTE_PROPERTY: {
			emit_changed(property, EditorNode::get_singleton()->get_inspector()->get_property_clipboard());
		} break;
		case MENU_COPY_PROPERTY_PATH: {
			OS::get_singleton()->set_clipboard(property_path);
		} break;
	}
}

////////////////////////////////////////////////
////////////////////////////////////////////////

void EditorInspectorPlugin::add_custom_control(Control *control) {
	AddedEditor ae;
	ae.property_editor = control;
	added_editors.push_back(ae);
}

void EditorInspectorPlugin::add_property_editor(const String &p_for_property, Control *p_prop) {
	ERR_FAIL_COND(Object::cast_to<EditorProperty>(p_prop) == nullptr);

	AddedEditor ae;
	ae.properties.push_back(p_for_property);
	ae.property_editor = p_prop;
	added_editors.push_back(ae);
}

void EditorInspectorPlugin::add_property_editor_for_multiple_properties(const String &p_label, const Vector<String> &p_properties, Control *p_prop) {
	AddedEditor ae;
	ae.properties = p_properties;
	ae.property_editor = p_prop;
	ae.label = p_label;
	added_editors.push_back(ae);
}

bool EditorInspectorPlugin::can_handle(Object *p_object) {
	if (get_script_instance()) {
		return get_script_instance()->call("can_handle", p_object);
	}
	return false;
}
void EditorInspectorPlugin::parse_begin(Object *p_object) {
	if (get_script_instance()) {
		get_script_instance()->call("parse_begin", p_object);
	}
}

void EditorInspectorPlugin::parse_category(Object *p_object, const String &p_parse_category) {
	if (get_script_instance()) {
		get_script_instance()->call("parse_category", p_object, p_parse_category);
	}
}

bool EditorInspectorPlugin::parse_property(Object *p_object, Variant::Type p_type, const String &p_path, PropertyHint p_hint, const String &p_hint_text, int p_usage) {
	if (get_script_instance()) {
		Variant arg[6] = {
			p_object, p_type, p_path, p_hint, p_hint_text, p_usage
		};
		const Variant *argptr[6] = {
			&arg[0], &arg[1], &arg[2], &arg[3], &arg[4], &arg[5]
		};

		Variant::CallError err;
		return get_script_instance()->call("parse_property", (const Variant **)&argptr, 6, err);
	}
	return false;
}
void EditorInspectorPlugin::parse_end() {
	if (get_script_instance()) {
		get_script_instance()->call("parse_end");
	}
}

void EditorInspectorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_custom_control", "control"), &EditorInspectorPlugin::add_custom_control);
	ClassDB::bind_method(D_METHOD("add_property_editor", "property", "editor"), &EditorInspectorPlugin::add_property_editor);
	ClassDB::bind_method(D_METHOD("add_property_editor_for_multiple_properties", "label", "properties", "editor"), &EditorInspectorPlugin::add_property_editor_for_multiple_properties);

	MethodInfo vm;
	vm.name = "can_handle";
	vm.return_val.type = Variant::BOOL;
	vm.arguments.push_back(PropertyInfo(Variant::OBJECT, "object"));
	BIND_VMETHOD(vm);
	vm.name = "parse_begin";
	vm.return_val.type = Variant::NIL;
	BIND_VMETHOD(vm);
	vm.name = "parse_category";
	vm.arguments.push_back(PropertyInfo(Variant::STRING, "category"));
	BIND_VMETHOD(vm);
	vm.arguments.pop_back();
	vm.name = "parse_property";
	vm.return_val.type = Variant::BOOL;
	vm.arguments.push_back(PropertyInfo(Variant::INT, "type"));
	vm.arguments.push_back(PropertyInfo(Variant::STRING, "path"));
	vm.arguments.push_back(PropertyInfo(Variant::INT, "hint"));
	vm.arguments.push_back(PropertyInfo(Variant::STRING, "hint_text"));
	vm.arguments.push_back(PropertyInfo(Variant::INT, "usage"));
	BIND_VMETHOD(vm);
	vm.arguments.clear();
	vm.name = "parse_end";
	vm.return_val.type = Variant::NIL;
	BIND_VMETHOD(vm);
}

////////////////////////////////////////////////
////////////////////////////////////////////////

void EditorInspectorCategory::_notification(int p_what) {
	if (p_what == NOTIFICATION_DRAW) {
		draw_rect(Rect2(Vector2(), get_size()), bg_color);
		Ref<Font> font = get_font("font", "Tree");

		int hs = get_constant("hseparation", "Tree");

		int w = font->get_string_size(label).width;
		if (icon.is_valid()) {
			w += hs + icon->get_width();
		}
		w = MIN(w, get_size().width - hs * 2);

		int ofs = (get_size().width - w) / 2;

		if (icon.is_valid()) {
			draw_texture(icon, Point2(ofs, (get_size().height - icon->get_height()) / 2).floor());
			ofs += hs + icon->get_width();
			w -= hs + icon->get_width();
		}

		Color color = get_color("font_color", "Tree");
		draw_string(font, Point2(ofs, font->get_ascent() + (get_size().height - font->get_height()) / 2).floor(), label, color, w);
	}
}

Control *EditorInspectorCategory::make_custom_tooltip(const String &p_text) const {
	EditorHelpBit *help_bit = memnew(EditorHelpBit);
	help_bit->add_style_override("panel", get_stylebox("panel", "TooltipPanel"));
	help_bit->get_rich_text()->set_fixed_size_to_width(360 * EDSCALE);

	Vector<String> slices = p_text.split("::", false);
	if (!slices.empty()) {
		String property_name = slices[0].strip_edges();
		String text = "[u][b]" + property_name + "[/b][/u]";

		if (slices.size() > 1) {
			String property_doc = slices[1].strip_edges();
			if (property_name != property_doc) {
				text += "\n" + property_doc;
			}
		}
		help_bit->call_deferred("set_text", text); //hack so it uses proper theme once inside scene
	}

	return help_bit;
}

Size2 EditorInspectorCategory::get_minimum_size() const {
	Ref<Font> font = get_font("font", "Tree");

	Size2 ms;
	ms.width = 1;
	ms.height = font->get_height();
	if (icon.is_valid()) {
		ms.height = MAX(icon->get_height(), ms.height);
	}
	ms.height += get_constant("vseparation", "Tree");

	return ms;
}

EditorInspectorCategory::EditorInspectorCategory() {
}

////////////////////////////////////////////////
////////////////////////////////////////////////

void EditorInspectorSection::_test_unfold() {
	if (!vbox_added) {
		add_child(vbox);
		vbox_added = true;
	}
}

Ref<Texture> EditorInspectorSection::_get_arrow() {
	Ref<Texture> arrow;
	if (foldable) {
		if (object->editor_is_section_unfolded(section)) {
			arrow = get_icon("arrow", "Tree");
		} else {
			arrow = get_icon("arrow_collapsed", "Tree");
		}
	}
	return arrow;
}

int EditorInspectorSection::_get_header_height() {
	Ref<Font> font = get_font("font", "Tree");

	int header_height = font->get_height();
	Ref<Texture> arrow = _get_arrow();
	if (arrow.is_valid()) {
		header_height = MAX(header_height, arrow->get_height());
	}
	header_height += get_constant("vseparation", "Tree");

	return header_height;
}

void EditorInspectorSection::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_SORT_CHILDREN: {
			Size2 size = get_size();
			Point2 offset;

			offset.y = _get_header_height();
			offset.x += get_constant("inspector_margin", "Editor");

			Rect2 rect(offset, size - offset);

			//set children
			for (int i = 0; i < get_child_count(); i++) {
				Control *c = Object::cast_to<Control>(get_child(i));
				if (!c) {
					continue;
				}
				if (c->is_set_as_toplevel()) {
					continue;
				}
				if (!c->is_visible_in_tree()) {
					continue;
				}

				fit_child_in_rect(c, rect);
			}

			update(); //need to redraw text
		} break;

		case NOTIFICATION_DRAW: {
			int h = _get_header_height();

			Rect2 header_rect = Rect2(Vector2(), Vector2(get_size().width, h));
			Color c = bg_color;
			c.a *= 0.4;
			if (foldable && header_rect.has_point(get_local_mouse_position())) {
				c = c.lightened(Input::get_singleton()->is_mouse_button_pressed(BUTTON_LEFT) ? -0.05 : 0.2);
			}
			draw_rect(header_rect, c);

			const int arrow_margin = 3;
			Color color = get_color("font_color", "Tree");
			Ref<Font> font = get_font("font", "Tree");
			draw_string(font, Point2(Math::round((16 + arrow_margin) * EDSCALE), font->get_ascent() + (h - font->get_height()) / 2).floor(), label, color, get_size().width);

			Ref<Texture> arrow = _get_arrow();
			if (arrow.is_valid()) {
				draw_texture(arrow, Point2(Math::round(arrow_margin * EDSCALE), (h - arrow->get_height()) / 2).floor());
			}
		} break;
		case NOTIFICATION_MOUSE_ENTER:
		case NOTIFICATION_MOUSE_EXIT: {
			update();
		} break;
	}
}

Size2 EditorInspectorSection::get_minimum_size() const {
	Size2 ms;
	for (int i = 0; i < get_child_count(); i++) {
		Control *c = Object::cast_to<Control>(get_child(i));
		if (!c) {
			continue;
		}
		if (c->is_set_as_toplevel()) {
			continue;
		}
		if (!c->is_visible()) {
			continue;
		}
		Size2 minsize = c->get_combined_minimum_size();
		ms.width = MAX(ms.width, minsize.width);
		ms.height = MAX(ms.height, minsize.height);
	}

	Ref<Font> font = get_font("font", "Tree");
	ms.height += font->get_height() + get_constant("vseparation", "Tree");
	ms.width += get_constant("inspector_margin", "Editor");

	return ms;
}

void EditorInspectorSection::setup(const String &p_section, const String &p_label, Object *p_object, const Color &p_bg_color, bool p_foldable) {
	section = p_section;
	label = p_label;
	object = p_object;
	bg_color = p_bg_color;
	foldable = p_foldable;

	if (!foldable && !vbox_added) {
		add_child(vbox);
		vbox_added = true;
	}

	if (foldable) {
		_test_unfold();
		if (object->editor_is_section_unfolded(section)) {
			vbox->show();
		} else {
			vbox->hide();
		}
	}
}

void EditorInspectorSection::_gui_input(const Ref<InputEvent> &p_event) {
	if (!foldable) {
		return;
	}

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {
		if (mb->get_position().y >= _get_header_height()) {
			return;
		}

		_test_unfold();

		bool unfold = !object->editor_is_section_unfolded(section);
		object->editor_set_section_unfold(section, unfold);
		if (unfold) {
			vbox->show();
		} else {
			vbox->hide();
		}
	} else if (mb.is_valid() && !mb->is_pressed()) {
		update();
	}
}

VBoxContainer *EditorInspectorSection::get_vbox() {
	return vbox;
}

void EditorInspectorSection::unfold() {
	if (!foldable) {
		return;
	}

	_test_unfold();

	object->editor_set_section_unfold(section, true);
	vbox->show();
	update();
}

void EditorInspectorSection::fold() {
	if (!foldable) {
		return;
	}

	if (!vbox_added) {
		return; //kinda pointless
	}

	object->editor_set_section_unfold(section, false);
	vbox->hide();
	update();
}

void EditorInspectorSection::_bind_methods() {
	ClassDB::bind_method(D_METHOD("setup", "section", "label", "object", "bg_color", "foldable"), &EditorInspectorSection::setup);
	ClassDB::bind_method(D_METHOD("get_vbox"), &EditorInspectorSection::get_vbox);
	ClassDB::bind_method(D_METHOD("unfold"), &EditorInspectorSection::unfold);
	ClassDB::bind_method(D_METHOD("fold"), &EditorInspectorSection::fold);
	ClassDB::bind_method(D_METHOD("_gui_input"), &EditorInspectorSection::_gui_input);
}

EditorInspectorSection::EditorInspectorSection() {
	object = nullptr;
	foldable = false;
	vbox = memnew(VBoxContainer);
	vbox_added = false;
}

EditorInspectorSection::~EditorInspectorSection() {
	if (!vbox_added) {
		memdelete(vbox);
	}
}

////////////////////////////////////////////////
////////////////////////////////////////////////

Ref<EditorInspectorPlugin> EditorInspector::inspector_plugins[MAX_PLUGINS];
int EditorInspector::inspector_plugin_count = 0;

bool EditorInspector::_property_path_matches(const String &p_property_path, const String &p_filter, EditorPropertyNameProcessor::Style p_style) {
	if (p_property_path.findn(p_filter) != -1) {
		return true;
	}

	const Vector<String> sections = p_property_path.split("/");
	for (int i = 0; i < sections.size(); i++) {
		if (p_filter.is_subsequence_ofi(EditorPropertyNameProcessor::get_singleton()->process_name(sections[i], p_style))) {
			return true;
		}
	}
	return false;
}

EditorProperty *EditorInspector::instantiate_property_editor(Object *p_object, Variant::Type p_type, const String &p_path, PropertyHint p_hint, const String &p_hint_text, int p_usage) {
	for (int i = inspector_plugin_count - 1; i >= 0; i--) {
		inspector_plugins[i]->parse_property(p_object, p_type, p_path, p_hint, p_hint_text, p_usage);
		if (inspector_plugins[i]->added_editors.size()) {
			for (int j = 1; j < inspector_plugins[i]->added_editors.size(); j++) { //only keep first one
				memdelete(inspector_plugins[i]->added_editors[j].property_editor);
			}

			EditorProperty *prop = Object::cast_to<EditorProperty>(inspector_plugins[i]->added_editors[0].property_editor);
			if (prop) {
				inspector_plugins[i]->added_editors.clear();
				return prop;
			} else {
				memdelete(inspector_plugins[i]->added_editors[0].property_editor);
				inspector_plugins[i]->added_editors.clear();
			}
		}
	}
	return nullptr;
}

void EditorInspector::add_inspector_plugin(const Ref<EditorInspectorPlugin> &p_plugin) {
	ERR_FAIL_COND(inspector_plugin_count == MAX_PLUGINS);

	for (int i = 0; i < inspector_plugin_count; i++) {
		if (inspector_plugins[i] == p_plugin) {
			return; //already exists
		}
	}
	inspector_plugins[inspector_plugin_count++] = p_plugin;
}

void EditorInspector::remove_inspector_plugin(const Ref<EditorInspectorPlugin> &p_plugin) {
	ERR_FAIL_COND(inspector_plugin_count == MAX_PLUGINS);

	int idx = -1;
	for (int i = 0; i < inspector_plugin_count; i++) {
		if (inspector_plugins[i] == p_plugin) {
			idx = i;
			break;
		}
	}

	ERR_FAIL_COND_MSG(idx == -1, "Trying to remove nonexistent inspector plugin.");
	for (int i = idx; i < inspector_plugin_count - 1; i++) {
		inspector_plugins[i] = inspector_plugins[i + 1];
	}
	inspector_plugins[inspector_plugin_count - 1] = Ref<EditorInspectorPlugin>();

	inspector_plugin_count--;
}

void EditorInspector::cleanup_plugins() {
	for (int i = 0; i < inspector_plugin_count; i++) {
		inspector_plugins[i].unref();
	}
	inspector_plugin_count = 0;
}

void EditorInspector::set_undo_redo(UndoRedo *p_undo_redo) {
	undo_redo = p_undo_redo;
}

String EditorInspector::get_selected_path() const {
	return property_selected;
}

void EditorInspector::_parse_added_editors(VBoxContainer *current_vbox, Ref<EditorInspectorPlugin> ped) {
	for (List<EditorInspectorPlugin::AddedEditor>::Element *F = ped->added_editors.front(); F; F = F->next()) {
		EditorProperty *ep = Object::cast_to<EditorProperty>(F->get().property_editor);
		current_vbox->add_child(F->get().property_editor);

		if (ep) {
			ep->object = object;
			ep->connect("property_changed", this, "_property_changed");
			ep->connect("property_keyed", this, "_property_keyed");
			ep->connect("property_keyed_with_value", this, "_property_keyed_with_value");
			ep->connect("property_checked", this, "_property_checked");
			ep->connect("property_pinned", this, "_property_pinned");
			ep->connect("selected", this, "_property_selected");
			ep->connect("multiple_properties_changed", this, "_multiple_properties_changed");
			ep->connect("resource_selected", this, "_resource_selected", varray(), CONNECT_DEFERRED);
			ep->connect("object_id_selected", this, "_object_id_selected", varray(), CONNECT_DEFERRED);

			if (F->get().properties.size()) {
				if (F->get().properties.size() == 1) {
					//since it's one, associate:
					ep->property = F->get().properties[0];
					ep->property_path = property_prefix + F->get().properties[0];
					ep->property_usage = 0;
				}

				if (F->get().label != String()) {
					ep->set_label(F->get().label);
				}

				for (int i = 0; i < F->get().properties.size(); i++) {
					String prop = F->get().properties[i];

					if (!editor_property_map.has(prop)) {
						editor_property_map[prop] = List<EditorProperty *>();
					}
					editor_property_map[prop].push_back(ep);
				}
			}

			ep->set_read_only(read_only);
			ep->update_property();
			ep->_update_pin_flags();
			ep->update_revert_and_pin_status();
		}
	}
	ped->added_editors.clear();
}

bool EditorInspector::_is_property_disabled_by_feature_profile(const StringName &p_property) {
	Ref<EditorFeatureProfile> profile = EditorFeatureProfileManager::get_singleton()->get_current_profile();
	if (profile.is_null()) {
		return false;
	}

	StringName class_name = object->get_class();

	while (class_name != StringName()) {
		if (profile->is_class_property_disabled(class_name, p_property)) {
			return true;
		}
		if (profile->is_class_disabled(class_name)) {
			//won't see properties of a disabled class
			return true;
		}
		class_name = ClassDB::get_parent_class(class_name);
	}

	return false;
}

void EditorInspector::update_tree() {
	//to update properly if all is refreshed
	StringName current_selected = property_selected;
	int current_focusable = -1;

	if (property_focusable != -1) {
		//check focusable is really focusable
		bool restore_focus = false;
		Control *focused = get_focus_owner();
		if (focused) {
			Node *parent = focused->get_parent();
			while (parent) {
				EditorInspector *inspector = Object::cast_to<EditorInspector>(parent);
				if (inspector) {
					restore_focus = inspector == this; //may be owned by another inspector
					break; //exit after the first inspector is found, since there may be nested ones
				}
				parent = parent->get_parent();
			}
		}

		if (restore_focus) {
			current_focusable = property_focusable;
		}
	}

	_clear();

	if (!object) {
		return;
	}

	List<Ref<EditorInspectorPlugin>> valid_plugins;

	for (int i = inspector_plugin_count - 1; i >= 0; i--) { //start by last, so lastly added can override newly added
		if (!inspector_plugins[i]->can_handle(object)) {
			continue;
		}
		valid_plugins.push_back(inspector_plugins[i]);
	}

	bool draw_red = false;

	{
		Node *nod = Object::cast_to<Node>(object);
		Node *es = EditorNode::get_singleton()->get_edited_scene();
		if (nod && es != nod && nod->get_owner() != es) {
			draw_red = true;
		}
	}

	//	TreeItem *current_category = NULL;

	String filter = search_box ? search_box->get_text() : "";
	String group;
	String group_base;
	VBoxContainer *category_vbox = nullptr;

	List<PropertyInfo> plist;
	object->get_property_list(&plist, true);

	HashMap<String, VBoxContainer *> item_path;
	Map<VBoxContainer *, EditorInspectorSection *> section_map;

	item_path[""] = main_vbox;

	Color sscolor = get_color("prop_subsection", "Editor");

	for (List<Ref<EditorInspectorPlugin>>::Element *E = valid_plugins.front(); E; E = E->next()) {
		Ref<EditorInspectorPlugin> ped = E->get();
		ped->parse_begin(object);
		_parse_added_editors(main_vbox, ped);
	}

	for (List<PropertyInfo>::Element *I = plist.front(); I; I = I->next()) {
		PropertyInfo &p = I->get();

		//make sure the property can be edited

		if (p.usage & PROPERTY_USAGE_GROUP) {
			group = p.name;
			group_base = p.hint_string;

			continue;

		} else if (p.usage & PROPERTY_USAGE_CATEGORY) {
			group = "";
			group_base = "";

			if (!show_categories) {
				continue;
			}

			List<PropertyInfo>::Element *N = I->next();
			bool valid = true;
			//if no properties in category, skip
			while (N) {
				if (N->get().usage & PROPERTY_USAGE_EDITOR) {
					break;
				}
				if (N->get().usage & PROPERTY_USAGE_CATEGORY) {
					valid = false;
					break;
				}
				N = N->next();
			}
			if (!valid) {
				continue; //empty, ignore
			}

			EditorInspectorCategory *category = memnew(EditorInspectorCategory);
			main_vbox->add_child(category);
			category_vbox = nullptr; //reset

			String type = p.name;
			category->icon = EditorNode::get_singleton()->get_class_icon(type, "Object");
			category->label = type;

			category->bg_color = get_color("prop_category", "Editor");
			if (use_doc_hints) {
				StringName type2 = p.name;
				if (!class_descr_cache.has(type2)) {
					String descr;
					DocData *dd = EditorHelp::get_doc_data();
					Map<String, DocData::ClassDoc>::Element *E = dd->class_list.find(type2);
					if (E) {
						descr = DTR(E->get().brief_description);
					}
					class_descr_cache[type2] = descr;
				}

				category->set_tooltip(p.name + "::" + (class_descr_cache[type2] == "" ? "" : class_descr_cache[type2]));
			}

			for (List<Ref<EditorInspectorPlugin>>::Element *E = valid_plugins.front(); E; E = E->next()) {
				Ref<EditorInspectorPlugin> ped = E->get();
				ped->parse_category(object, p.name);
				_parse_added_editors(main_vbox, ped);
			}

			continue;

		} else if (!(p.usage & PROPERTY_USAGE_EDITOR) || _is_property_disabled_by_feature_profile(p.name)) {
			continue;
		}

		if (p.usage & PROPERTY_USAGE_HIGH_END_GFX && VS::get_singleton()->is_low_end()) {
			continue; //do not show this property in low end gfx
		}

		if (p.name == "script" && (hide_script || bool(object->call("_hide_script_from_inspector")))) {
			continue;
		}

		String basename = p.name;
		if (group != "") {
			if (group_base != "") {
				if (basename.begins_with(group_base)) {
					basename = basename.replace_first(group_base, "");
				} else if (group_base.begins_with(basename)) {
					//keep it, this is used pretty often
				} else {
					group = ""; //no longer using group base, clear
				}
			}
		}

		if (group != "") {
			basename = group + "/" + basename;
		}

		String name = (basename.find("/") != -1) ? basename.right(basename.rfind("/") + 1) : basename;
		String name_override = name;
		String feature_tag;
		{
			const int dot = name.find(".");
			if (dot != -1) {
				name_override = name.substr(0, dot);
				feature_tag = name.right(dot);
			}
		}

		// Don't localize script variables.
		EditorPropertyNameProcessor::Style name_style = property_name_style;
		if ((p.usage & PROPERTY_USAGE_SCRIPT_VARIABLE) && name_style == EditorPropertyNameProcessor::STYLE_LOCALIZED) {
			name_style = EditorPropertyNameProcessor::STYLE_CAPITALIZED;
		}
		name = EditorPropertyNameProcessor::get_singleton()->process_name(name_override, name_style) + feature_tag;

		String path = basename.left(basename.rfind("/"));

		if (use_filter && !filter.empty()) {
			const String property_path = property_prefix + (path.empty() ? "" : path + "/") + name_override;
			if (!_property_path_matches(property_path, filter, property_name_style)) {
				continue;
			}
		}

		if (category_vbox == nullptr) {
			category_vbox = memnew(VBoxContainer);
			main_vbox->add_child(category_vbox);
		}

		VBoxContainer *current_vbox = main_vbox;

		{
			String acc_path = "";
			int level = 1;
			for (int i = 0; i < path.get_slice_count("/"); i++) {
				String path_name = path.get_slice("/", i);
				if (i > 0) {
					acc_path += "/";
				}
				acc_path += path_name;
				if (!item_path.has(acc_path)) {
					EditorInspectorSection *section = memnew(EditorInspectorSection);
					current_vbox->add_child(section);
					sections.push_back(section);

					String label;
					String tooltip;

					// Don't localize groups for script variables.
					EditorPropertyNameProcessor::Style section_name_style = property_name_style;
					if ((p.usage & PROPERTY_USAGE_SCRIPT_VARIABLE) && section_name_style == EditorPropertyNameProcessor::STYLE_LOCALIZED) {
						section_name_style = EditorPropertyNameProcessor::STYLE_CAPITALIZED;
					}

					// Only process group label if this is not the group or subgroup.
					if ((i == 0 && path_name == group)) {
						if (section_name_style == EditorPropertyNameProcessor::STYLE_LOCALIZED) {
							label = TTRGET(path_name);
							tooltip = path_name;
						} else {
							label = path_name;
							tooltip = TTRGET(path_name);
						}
					} else {
						label = EditorPropertyNameProcessor::get_singleton()->process_name(path_name, section_name_style);
						tooltip = EditorPropertyNameProcessor::get_singleton()->process_name(path_name, EditorPropertyNameProcessor::get_tooltip_style(section_name_style));
					}

					Color c = sscolor;
					c.a /= level;
					section->setup(acc_path, label, object, c, use_folding);
					section->set_tooltip(tooltip);

					VBoxContainer *vb = section->get_vbox();
					item_path[acc_path] = vb;
					section_map[vb] = section;
				}
				current_vbox = item_path[acc_path];
				level = (MIN(level + 1, 4));
			}

			if (current_vbox == main_vbox) {
				//do not add directly to the main vbox, given it has no spacing
				if (category_vbox == nullptr) {
					category_vbox = memnew(VBoxContainer);
				}
				current_vbox = category_vbox;
			}
		}

		bool checkable = false;
		bool checked = false;
		if (p.usage & PROPERTY_USAGE_CHECKABLE) {
			checkable = true;
			checked = p.usage & PROPERTY_USAGE_CHECKED;
		}

		if (p.usage & PROPERTY_USAGE_RESTART_IF_CHANGED) {
			restart_request_props.insert(p.name);
		}

		String doc_hint;

		if (use_doc_hints) {
			StringName classname = object->get_class_name();
			if (object_class != String()) {
				classname = object_class;
			} else if (Object::cast_to<MultiNodeEdit>(object)) {
				classname = Object::cast_to<MultiNodeEdit>(object)->get_edited_class_name();
			}

			StringName propname = property_prefix + p.name;
			String descr;
			bool found = false;

			Map<StringName, Map<StringName, String>>::Element *E = descr_cache.find(classname);
			if (E) {
				Map<StringName, String>::Element *F = E->get().find(propname);
				if (F) {
					found = true;
					descr = F->get();
				}
			}

			if (!found) {
				DocData *dd = EditorHelp::get_doc_data();
				Map<String, DocData::ClassDoc>::Element *F = dd->class_list.find(classname);
				while (F && descr == String()) {
					for (int i = 0; i < F->get().properties.size(); i++) {
						if (F->get().properties[i].name == propname.operator String()) {
							descr = DTR(F->get().properties[i].description);
							break;
						}
					}

					Vector<String> slices = propname.operator String().split("/");
					if (slices.size() == 2 && slices[0].begins_with("custom_")) {
						// Likely a theme property.
						for (int i = 0; i < F->get().theme_properties.size(); i++) {
							if (F->get().theme_properties[i].name == slices[1]) {
								descr = DTR(F->get().theme_properties[i].description);
								break;
							}
						}
					}

					if (!F->get().inherits.empty()) {
						F = dd->class_list.find(F->get().inherits);
					} else {
						break;
					}
				}
				descr_cache[classname][propname] = descr;
			}

			doc_hint = descr;
		}

		for (List<Ref<EditorInspectorPlugin>>::Element *E = valid_plugins.front(); E; E = E->next()) {
			Ref<EditorInspectorPlugin> ped = E->get();
			bool exclusive = ped->parse_property(object, p.type, p.name, p.hint, p.hint_string, p.usage);

			List<EditorInspectorPlugin::AddedEditor> editors = ped->added_editors; //make a copy, since plugins may be used again in a sub-inspector
			ped->added_editors.clear();

			for (List<EditorInspectorPlugin::AddedEditor>::Element *F = editors.front(); F; F = F->next()) {
				EditorProperty *ep = Object::cast_to<EditorProperty>(F->get().property_editor);

				if (ep) {
					//set all this before the control gets the ENTER_TREE notification
					ep->object = object;

					if (F->get().properties.size()) {
						if (F->get().properties.size() == 1) {
							//since it's one, associate:
							ep->property = F->get().properties[0];
							ep->property_path = property_prefix + F->get().properties[0];
							ep->property_usage = p.usage;
							//and set label?
						}

						if (F->get().label != String()) {
							ep->set_label(F->get().label);
						} else {
							//use existing one
							ep->set_label(name);
						}
						for (int i = 0; i < F->get().properties.size(); i++) {
							String prop = F->get().properties[i];

							if (!editor_property_map.has(prop)) {
								editor_property_map[prop] = List<EditorProperty *>();
							}
							editor_property_map[prop].push_back(ep);
						}
					}
					ep->set_draw_red(draw_red);
					ep->set_use_folding(use_folding);
					ep->set_checkable(checkable);
					ep->set_checked(checked);
					ep->set_keying(keying);

					ep->set_read_only(read_only);
				}

				current_vbox->add_child(F->get().property_editor);

				if (ep) {
					ep->connect("property_changed", this, "_property_changed");
					if (p.usage & PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED) {
						ep->connect("property_changed", this, "_property_changed_update_all", varray(), CONNECT_DEFERRED);
					}
					ep->connect("property_keyed", this, "_property_keyed");
					ep->connect("property_keyed_with_value", this, "_property_keyed_with_value");
					ep->connect("property_checked", this, "_property_checked");
					ep->connect("property_pinned", this, "_property_pinned");
					ep->connect("selected", this, "_property_selected");
					ep->connect("multiple_properties_changed", this, "_multiple_properties_changed");
					ep->connect("resource_selected", this, "_resource_selected", varray(), CONNECT_DEFERRED);
					ep->connect("object_id_selected", this, "_object_id_selected", varray(), CONNECT_DEFERRED);
					if (doc_hint != String()) {
						ep->set_tooltip(property_prefix + p.name + "::" + doc_hint);
					} else {
						ep->set_tooltip(property_prefix + p.name + "::" + p.tooltip);
					}
					ep->update_property();
					ep->_update_pin_flags();
					ep->update_revert_and_pin_status();

					if (current_selected && ep->property == current_selected) {
						ep->select(current_focusable);
					}
				}
			}

			if (exclusive) {
				break;
			}
		}
	}

	for (List<Ref<EditorInspectorPlugin>>::Element *E = valid_plugins.front(); E; E = E->next()) {
		Ref<EditorInspectorPlugin> ped = E->get();
		ped->parse_end();
		_parse_added_editors(main_vbox, ped);
	}

	//see if this property exists and should be kept
}
void EditorInspector::update_property(const String &p_prop) {
	if (!editor_property_map.has(p_prop)) {
		return;
	}

	for (List<EditorProperty *>::Element *E = editor_property_map[p_prop].front(); E; E = E->next()) {
		E->get()->update_property();
		E->get()->update_revert_and_pin_status();
	}
}

void EditorInspector::_clear() {
	while (main_vbox->get_child_count()) {
		memdelete(main_vbox->get_child(0));
	}
	property_selected = StringName();
	property_focusable = -1;
	editor_property_map.clear();
	sections.clear();
	pending.clear();
	restart_request_props.clear();
}

void EditorInspector::refresh() {
	if (refresh_countdown > 0 || changing) {
		return;
	}
	refresh_countdown = EditorSettings::get_singleton()->get("docks/property_editor/auto_refresh_interval");
}

Object *EditorInspector::get_edited_object() {
	return object;
}

void EditorInspector::edit(Object *p_object) {
	if (object == p_object) {
		return;
	}
	if (object) {
		_clear();
		object->remove_change_receptor(this);
	}

	object = p_object;

	if (object) {
		update_scroll_request = 0; //reset
		if (scroll_cache.has(object->get_instance_id())) { //if exists, set something else
			update_scroll_request = scroll_cache[object->get_instance_id()]; //done this way because wait until full size is accommodated
		}
		object->add_change_receptor(this);
		update_tree();
	}
}

void EditorInspector::set_keying(bool p_active) {
	if (keying == p_active) {
		return;
	}
	keying = p_active;
	update_tree();
}
void EditorInspector::set_read_only(bool p_read_only) {
	read_only = p_read_only;
	update_tree();
}

EditorPropertyNameProcessor::Style EditorInspector::get_property_name_style() const {
	return property_name_style;
}

void EditorInspector::set_property_name_style(EditorPropertyNameProcessor::Style p_style) {
	if (property_name_style == p_style) {
		return;
	}
	property_name_style = p_style;
	update_tree();
}

void EditorInspector::set_autoclear(bool p_enable) {
	autoclear = p_enable;
}

void EditorInspector::set_show_categories(bool p_show) {
	show_categories = p_show;
	update_tree();
}

void EditorInspector::set_use_doc_hints(bool p_enable) {
	use_doc_hints = p_enable;
	update_tree();
}
void EditorInspector::set_hide_script(bool p_hide) {
	hide_script = p_hide;
	update_tree();
}
void EditorInspector::set_use_filter(bool p_use) {
	use_filter = p_use;
	update_tree();
}
void EditorInspector::register_text_enter(Node *p_line_edit) {
	search_box = Object::cast_to<LineEdit>(p_line_edit);
	if (search_box) {
		search_box->connect("text_changed", this, "_filter_changed");
	}
}

void EditorInspector::_filter_changed(const String &p_text) {
	_clear();
	update_tree();
}

void EditorInspector::set_use_folding(bool p_enable) {
	use_folding = p_enable;
	update_tree();
}

bool EditorInspector::is_using_folding() {
	return use_folding;
}

void EditorInspector::collapse_all_folding() {
	for (List<EditorInspectorSection *>::Element *E = sections.front(); E; E = E->next()) {
		E->get()->fold();
	}

	for (Map<StringName, List<EditorProperty *>>::Element *F = editor_property_map.front(); F; F = F->next()) {
		for (List<EditorProperty *>::Element *E = F->get().front(); E; E = E->next()) {
			E->get()->collapse_all_folding();
		}
	}
}

void EditorInspector::expand_all_folding() {
	for (List<EditorInspectorSection *>::Element *E = sections.front(); E; E = E->next()) {
		E->get()->unfold();
	}
	for (Map<StringName, List<EditorProperty *>>::Element *F = editor_property_map.front(); F; F = F->next()) {
		for (List<EditorProperty *>::Element *E = F->get().front(); E; E = E->next()) {
			E->get()->expand_all_folding();
		}
	}
}

void EditorInspector::set_scroll_offset(int p_offset) {
	set_v_scroll(p_offset);
}

int EditorInspector::get_scroll_offset() const {
	return get_v_scroll();
}

void EditorInspector::_update_inspector_bg() {
	if (sub_inspector) {
		int count_subinspectors = 0;
		Node *n = get_parent();
		while (n) {
			EditorInspector *ei = Object::cast_to<EditorInspector>(n);
			if (ei && ei->sub_inspector) {
				count_subinspectors++;
			}
			n = n->get_parent();
		}
		count_subinspectors = MIN(15, count_subinspectors);
		add_style_override("bg", get_stylebox("sub_inspector_bg" + itos(count_subinspectors), "Editor"));
	} else {
		add_style_override("bg", get_stylebox("bg", "Tree"));
	}
}
void EditorInspector::set_sub_inspector(bool p_enable) {
	sub_inspector = p_enable;
	if (!is_inside_tree()) {
		return;
	}

	_update_inspector_bg();
}

void EditorInspector::set_property_clipboard(const Variant &p_value) {
	property_clipboard = p_value;
}

Variant EditorInspector::get_property_clipboard() const {
	return property_clipboard;
}

void EditorInspector::_edit_request_change(Object *p_object, const String &p_property) {
	if (object != p_object) { //may be undoing/redoing for a non edited object, so ignore
		return;
	}

	if (changing) {
		return;
	}

	if (p_property == String()) {
		update_tree_pending = true;
	} else {
		pending.insert(p_property);
	}
}

void EditorInspector::_edit_set(const String &p_name, const Variant &p_value, bool p_refresh_all, const String &p_changed_field) {
	if (autoclear && editor_property_map.has(p_name)) {
		for (List<EditorProperty *>::Element *E = editor_property_map[p_name].front(); E; E = E->next()) {
			if (E->get()->is_checkable()) {
				E->get()->set_checked(true);
			}
		}
	}

	if (!undo_redo || bool(object->call("_dont_undo_redo"))) {
		object->set(p_name, p_value);
		if (p_refresh_all) {
			_edit_request_change(object, "");
		} else {
			_edit_request_change(object, p_name);
		}

		emit_signal(_prop_edited, p_name);

	} else if (Object::cast_to<MultiNodeEdit>(object)) {
		Object::cast_to<MultiNodeEdit>(object)->set_property_field(p_name, p_value, p_changed_field);
		_edit_request_change(object, p_name);
		emit_signal(_prop_edited, p_name);
	} else {
		undo_redo->create_action(vformat(TTR("Set %s"), p_name), UndoRedo::MERGE_ENDS);
		undo_redo->add_do_property(object, p_name, p_value);
		undo_redo->add_undo_property(object, p_name, object->get(p_name));

		if (p_refresh_all) {
			undo_redo->add_do_method(this, "_edit_request_change", object, "");
			undo_redo->add_undo_method(this, "_edit_request_change", object, "");
		} else {
			undo_redo->add_do_method(this, "_edit_request_change", object, p_name);
			undo_redo->add_undo_method(this, "_edit_request_change", object, p_name);
		}

		Resource *r = Object::cast_to<Resource>(object);
		if (r) {
			if (String(p_name) == "resource_local_to_scene") {
				bool prev = object->get(p_name);
				bool next = p_value;
				if (next) {
					undo_redo->add_do_method(r, "setup_local_to_scene");
				}
				if (prev) {
					undo_redo->add_undo_method(r, "setup_local_to_scene");
				}
			}
		}
		undo_redo->add_do_method(this, "emit_signal", _prop_edited, p_name);
		undo_redo->add_undo_method(this, "emit_signal", _prop_edited, p_name);
		undo_redo->commit_action();
	}

	if (editor_property_map.has(p_name)) {
		for (List<EditorProperty *>::Element *E = editor_property_map[p_name].front(); E; E = E->next()) {
			E->get()->update_revert_and_pin_status();
		}
	}
}

void EditorInspector::_property_changed(const String &p_path, const Variant &p_value, const String &p_name, bool changing) {
	// The "changing" variable must be true for properties that trigger events as typing occurs,
	// like "text_changed" signal. eg: Text property of Label, Button, RichTextLabel, etc.
	if (changing) {
		this->changing++;
	}

	_edit_set(p_path, p_value, false, p_name);

	if (changing) {
		this->changing--;
	}

	if (restart_request_props.has(p_path)) {
		emit_signal("restart_requested");
	}
}

void EditorInspector::_property_changed_update_all(const String &p_path, const Variant &p_value, const String &p_name, bool p_changing) {
	update_tree();
}

void EditorInspector::_multiple_properties_changed(Vector<String> p_paths, Array p_values) {
	ERR_FAIL_COND(p_paths.size() == 0 || p_values.size() == 0);
	ERR_FAIL_COND(p_paths.size() != p_values.size());
	String names;
	for (int i = 0; i < p_paths.size(); i++) {
		if (i > 0) {
			names += ",";
		}
		names += p_paths[i];
	}
	undo_redo->create_action(TTR("Set Multiple:") + " " + names, UndoRedo::MERGE_ENDS);
	for (int i = 0; i < p_paths.size(); i++) {
		_edit_set(p_paths[i], p_values[i], false, "");
		if (restart_request_props.has(p_paths[i])) {
			emit_signal("restart_requested");
		}
	}
	changing++;
	undo_redo->commit_action();
	changing--;
}

void EditorInspector::_property_keyed(const String &p_path, bool p_advance) {
	if (!object) {
		return;
	}

	emit_signal("property_keyed", p_path, object->get(p_path), p_advance); //second param is deprecated
}

void EditorInspector::_property_keyed_with_value(const String &p_path, const Variant &p_value, bool p_advance) {
	if (!object) {
		return;
	}

	emit_signal("property_keyed", p_path, p_value, p_advance); //second param is deprecated
}

void EditorInspector::_property_checked(const String &p_path, bool p_checked) {
	if (!object) {
		return;
	}

	//property checked
	if (autoclear) {
		if (!p_checked) {
			object->set(p_path, Variant());
		} else {
			Variant to_create;
			List<PropertyInfo> pinfo;
			object->get_property_list(&pinfo);
			for (List<PropertyInfo>::Element *E = pinfo.front(); E; E = E->next()) {
				if (E->get().name == p_path) {
					Variant::CallError ce;
					to_create = Variant::construct(E->get().type, nullptr, 0, ce);
					break;
				}
			}
			object->set(p_path, to_create);
		}

		if (editor_property_map.has(p_path)) {
			for (List<EditorProperty *>::Element *E = editor_property_map[p_path].front(); E; E = E->next()) {
				E->get()->update_property();
				E->get()->update_revert_and_pin_status();
			}
		}

	} else {
		emit_signal("property_toggled", p_path, p_checked);
	}
}

void EditorInspector::_property_pinned(const String &p_path, bool p_pinned) {
	if (!object) {
		return;
	}

	Node *node = Object::cast_to<Node>(object);
	ERR_FAIL_COND(!node);

	if (undo_redo) {
		undo_redo->create_action(vformat(p_pinned ? TTR("Pinned %s") : TTR("Unpinned %s"), p_path));
		undo_redo->add_do_method(node, "_set_property_pinned", p_path, p_pinned);
		undo_redo->add_undo_method(node, "_set_property_pinned", p_path, !p_pinned);
		if (editor_property_map.has(p_path)) {
			for (List<EditorProperty *>::Element *E = editor_property_map[p_path].front(); E; E = E->next()) {
				undo_redo->add_do_method(E->get(), "_update_revert_and_pin_status");
				undo_redo->add_undo_method(E->get(), "_update_revert_and_pin_status");
			}
		}
		undo_redo->commit_action();
	} else {
		node->set_property_pinned(p_path, p_pinned);
		if (editor_property_map.has(p_path)) {
			for (List<EditorProperty *>::Element *E = editor_property_map[p_path].front(); E; E = E->next()) {
				E->get()->update_revert_and_pin_status();
			}
		}
	}
}

void EditorInspector::_property_selected(const String &p_path, int p_focusable) {
	property_selected = p_path;
	property_focusable = p_focusable;
	//deselect the others
	for (Map<StringName, List<EditorProperty *>>::Element *F = editor_property_map.front(); F; F = F->next()) {
		if (F->key() == property_selected) {
			continue;
		}
		for (List<EditorProperty *>::Element *E = F->get().front(); E; E = E->next()) {
			if (E->get()->is_selected()) {
				E->get()->deselect();
			}
		}
	}

	emit_signal("property_selected", p_path);
}

void EditorInspector::_object_id_selected(const String &p_path, ObjectID p_id) {
	emit_signal("object_id_selected", p_id);
}

void EditorInspector::_resource_selected(const String &p_path, RES p_resource) {
	emit_signal("resource_selected", p_resource, p_path);
}

void EditorInspector::_node_removed(Node *p_node) {
	if (p_node == object) {
		edit(nullptr);
	}
}

void EditorInspector::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		EditorFeatureProfileManager::get_singleton()->connect("current_feature_profile_changed", this, "_feature_profile_changed");
		_update_inspector_bg();
	}

	if (p_what == NOTIFICATION_ENTER_TREE) {
		if (!sub_inspector) {
			get_tree()->connect("node_removed", this, "_node_removed");
		}
	}
	if (p_what == NOTIFICATION_PREDELETE) {
		edit(nullptr); //just in case
	}
	if (p_what == NOTIFICATION_EXIT_TREE) {
		if (!sub_inspector) {
			get_tree()->disconnect("node_removed", this, "_node_removed");
		}
		edit(nullptr);
	}

	if (p_what == NOTIFICATION_PROCESS) {
		if (update_scroll_request >= 0) {
			get_v_scrollbar()->call_deferred("set_value", update_scroll_request);
			update_scroll_request = -1;
		}
		if (refresh_countdown > 0) {
			refresh_countdown -= get_process_delta_time();
			if (refresh_countdown <= 0) {
				for (Map<StringName, List<EditorProperty *>>::Element *F = editor_property_map.front(); F; F = F->next()) {
					for (List<EditorProperty *>::Element *E = F->get().front(); E; E = E->next()) {
						E->get()->update_property();
						E->get()->update_revert_and_pin_status();
					}
				}
			}
		}

		changing++;

		if (update_tree_pending) {
			update_tree();
			update_tree_pending = false;
			pending.clear();

		} else {
			while (pending.size()) {
				StringName prop = pending.front()->get();
				if (editor_property_map.has(prop)) {
					for (List<EditorProperty *>::Element *E = editor_property_map[prop].front(); E; E = E->next()) {
						E->get()->update_property();
						E->get()->update_revert_and_pin_status();
					}
				}
				pending.erase(pending.front());
			}
		}

		changing--;
	}

	if (p_what == EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED) {
		_update_inspector_bg();

		update_tree();
	}
}

void EditorInspector::_changed_callback(Object *p_changed, const char *p_prop) {
	//this is called when property change is notified via _change_notify()
	_edit_request_change(p_changed, p_prop);
}

void EditorInspector::_vscroll_changed(double p_offset) {
	if (update_scroll_request >= 0) { //waiting, do nothing
		return;
	}

	if (object) {
		scroll_cache[object->get_instance_id()] = p_offset;
	}
}

void EditorInspector::set_property_prefix(const String &p_prefix) {
	property_prefix = p_prefix;
}

String EditorInspector::get_property_prefix() const {
	return property_prefix;
}

void EditorInspector::set_object_class(const String &p_class) {
	object_class = p_class;
}

String EditorInspector::get_object_class() const {
	return object_class;
}

void EditorInspector::_feature_profile_changed() {
	update_tree();
}

void EditorInspector::_bind_methods() {
	ClassDB::bind_method("_property_changed", &EditorInspector::_property_changed, DEFVAL(""), DEFVAL(false));
	ClassDB::bind_method("_multiple_properties_changed", &EditorInspector::_multiple_properties_changed);
	ClassDB::bind_method("_property_changed_update_all", &EditorInspector::_property_changed_update_all);

	ClassDB::bind_method("_edit_request_change", &EditorInspector::_edit_request_change);
	ClassDB::bind_method("_node_removed", &EditorInspector::_node_removed);
	ClassDB::bind_method("_filter_changed", &EditorInspector::_filter_changed);
	ClassDB::bind_method("_property_keyed", &EditorInspector::_property_keyed);
	ClassDB::bind_method("_property_keyed_with_value", &EditorInspector::_property_keyed_with_value);
	ClassDB::bind_method("_property_checked", &EditorInspector::_property_checked);
	ClassDB::bind_method(D_METHOD("_property_pinned", "path", "pinned"), &EditorInspector::_property_pinned);
	ClassDB::bind_method("_property_selected", &EditorInspector::_property_selected);
	ClassDB::bind_method("_resource_selected", &EditorInspector::_resource_selected);
	ClassDB::bind_method("_object_id_selected", &EditorInspector::_object_id_selected);
	ClassDB::bind_method("_vscroll_changed", &EditorInspector::_vscroll_changed);
	ClassDB::bind_method("_feature_profile_changed", &EditorInspector::_feature_profile_changed);
	ClassDB::bind_method("get_edited_object", &EditorInspector::get_edited_object);

	ClassDB::bind_method("refresh", &EditorInspector::refresh);

	ADD_SIGNAL(MethodInfo("property_selected", PropertyInfo(Variant::STRING, "property")));
	ADD_SIGNAL(MethodInfo("property_keyed", PropertyInfo(Variant::STRING, "property")));
	ADD_SIGNAL(MethodInfo("resource_selected", PropertyInfo(Variant::OBJECT, "res"), PropertyInfo(Variant::STRING, "prop")));
	ADD_SIGNAL(MethodInfo("object_id_selected", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("property_edited", PropertyInfo(Variant::STRING, "property")));
	ADD_SIGNAL(MethodInfo("property_toggled", PropertyInfo(Variant::STRING, "property"), PropertyInfo(Variant::BOOL, "checked")));
	ADD_SIGNAL(MethodInfo("restart_requested"));
}

EditorInspector::EditorInspector() {
	object = nullptr;
	undo_redo = nullptr;
	main_vbox = memnew(VBoxContainer);
	main_vbox->set_h_size_flags(SIZE_EXPAND_FILL);
	main_vbox->add_constant_override("separation", 0);
	add_child(main_vbox);
	set_enable_h_scroll(false);
	set_enable_v_scroll(true);

	show_categories = false;
	hide_script = true;
	use_doc_hints = false;
	property_name_style = EditorPropertyNameProcessor::STYLE_CAPITALIZED;
	use_filter = false;
	autoclear = false;
	changing = 0;
	use_folding = false;
	update_all_pending = false;
	update_tree_pending = false;
	refresh_countdown = 0;
	read_only = false;
	search_box = nullptr;
	keying = false;
	_prop_edited = "property_edited";
	set_process(true);
	property_focusable = -1;
	sub_inspector = false;
	property_clipboard = Variant();

	get_v_scrollbar()->connect("value_changed", this, "_vscroll_changed");
	update_scroll_request = -1;

	ED_SHORTCUT("property_editor/copy_property", TTR("Copy Property"), KEY_MASK_CMD | KEY_C);
	ED_SHORTCUT("property_editor/paste_property", TTR("Paste Property"), KEY_MASK_CMD | KEY_V);
	ED_SHORTCUT("property_editor/copy_property_path", TTR("Copy Property Path"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_C);
}
