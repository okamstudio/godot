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
#include "editor_inspector.compat.inc"

#include "core/os/keyboard.h"
#include "editor/add_metadata_dialog.h"
#include "editor/doc_tools.h"
#include "editor/editor_feature_profile.h"
#include "editor/editor_main_screen.h"
#include "editor/editor_node.h"
#include "editor/editor_property_name_processor.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/editor_validation_panel.h"
#include "editor/inspector_dock.h"
#include "editor/multi_node_edit.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor/themes/editor_scale.h"
#include "editor/themes/editor_theme_manager.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/texture_rect.h"
#include "scene/property_utils.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/style_box_flat.h"
#include "scene/scene_string_names.h"

bool EditorInspector::_property_path_matches(const String &p_property_path, const String &p_filter, EditorPropertyNameProcessor::Style p_style) {
	if (p_property_path.containsn(p_filter)) {
		return true;
	}

	const Vector<String> prop_sections = p_property_path.split("/");
	for (int i = 0; i < prop_sections.size(); i++) {
		if (p_filter.is_subsequence_ofn(EditorPropertyNameProcessor::get_singleton()->process_name(prop_sections[i], p_style, p_property_path))) {
			return true;
		}
	}
	return false;
}

String EditorProperty::get_tooltip_string(const String &p_string) const {
	// Trim to 100 characters to prevent the tooltip from being too long.
	constexpr int TOOLTIP_MAX_LENGTH = 100;
	return p_string.left(TOOLTIP_MAX_LENGTH).strip_edges() + String((p_string.length() > TOOLTIP_MAX_LENGTH) ? "..." : "");
}

Size2 EditorProperty::get_minimum_size() const {
	Size2 ms;
	Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Tree"));
	int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Tree"));
	ms.height = label.is_empty() ? 0 : font->get_height(font_size) + 4 * EDSCALE;

	for (int i = 0; i < get_child_count(); i++) {
		Control *c = as_sortable_control(get_child(i));
		if (!c) {
			continue;
		}
		if (c == bottom_editor) {
			continue;
		}

		Size2 minsize = c->get_combined_minimum_size();
		ms = ms.max(minsize);
	}

	if (keying) {
		Ref<Texture2D> key = get_editor_theme_icon(SNAME("Key"));
		ms.width += key->get_width() + get_theme_constant(SNAME("h_separation"), SNAME("Tree"));
	}

	if (deletable) {
		Ref<Texture2D> key = get_editor_theme_icon(SNAME("Close"));
		ms.width += key->get_width() + get_theme_constant(SNAME("h_separation"), SNAME("Tree"));
	}

	if (checkable) {
		Ref<Texture2D> check = get_theme_icon(SNAME("checked"), SNAME("CheckBox"));
		ms.width += check->get_width() + get_theme_constant(SNAME("h_separation"), SNAME("Tree"));
	}

	if (bottom_editor != nullptr && bottom_editor->is_visible()) {
		ms.height += label.is_empty() ? 0 : get_theme_constant(SNAME("v_separation"));
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

	cache[p_property] = p_value;
	emit_signalp(SNAME("property_changed"), (const Variant **)argptrs, 4);
}

void EditorProperty::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_SORT_CHILDREN: {
			Size2 size = get_size();
			Rect2 rect;
			Rect2 bottom_rect;

			right_child_rect = Rect2();
			bottom_child_rect = Rect2();

			{
				int child_room = size.width * (1.0 - split_ratio);
				Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Tree"));
				int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Tree"));
				int height = label.is_empty() ? 0 : font->get_height(font_size) + 4 * EDSCALE;
				bool no_children = true;

				//compute room needed
				for (int i = 0; i < get_child_count(); i++) {
					Control *c = as_sortable_control(get_child(i));
					if (!c) {
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
					if (is_layout_rtl()) {
						rect = Rect2(1, 0, child_room, height);
					} else {
						rect = Rect2(size.width - child_room, 0, child_room, height);
					}
				}

				if (bottom_editor) {
					int v_offset = label.is_empty() ? 0 : get_theme_constant(SNAME("v_separation"));
					bottom_rect = Rect2(0, rect.size.height + v_offset, size.width, bottom_editor->get_combined_minimum_size().height);
				}

				if (keying) {
					Ref<Texture2D> key;

					if (use_keying_next()) {
						key = get_editor_theme_icon(SNAME("KeyNext"));
					} else {
						key = get_editor_theme_icon(SNAME("Key"));
					}

					rect.size.x -= key->get_width() + get_theme_constant(SNAME("h_separation"), SNAME("Tree"));
					if (is_layout_rtl()) {
						rect.position.x += key->get_width() + get_theme_constant(SNAME("h_separation"), SNAME("Tree"));
					}

					if (no_children) {
						text_size -= key->get_width() + 4 * EDSCALE;
					}
				}

				if (deletable) {
					Ref<Texture2D> close;

					close = get_editor_theme_icon(SNAME("Close"));

					rect.size.x -= close->get_width() + get_theme_constant(SNAME("h_separation"), SNAME("Tree"));

					if (is_layout_rtl()) {
						rect.position.x += close->get_width() + get_theme_constant(SNAME("h_separation"), SNAME("Tree"));
					}

					if (no_children) {
						text_size -= close->get_width() + 4 * EDSCALE;
					}
				}

				// Account for the space needed on the outer side
				// when any of the icons are visible.
				if (keying || deletable) {
					int separation = get_theme_constant(SNAME("h_separation"), SNAME("Tree"));
					rect.size.x -= separation;

					if (is_layout_rtl()) {
						rect.position.x += separation;
					}
				}
			}

			//set children
			for (int i = 0; i < get_child_count(); i++) {
				Control *c = as_sortable_control(get_child(i));
				if (!c) {
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

			queue_redraw(); //need to redraw text
		} break;

		case NOTIFICATION_DRAW: {
			Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Tree"));
			int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Tree"));
			bool rtl = is_layout_rtl();

			Size2 size = get_size();
			if (bottom_editor) {
				size.height = bottom_editor->get_offset(SIDE_TOP) - get_theme_constant(SNAME("v_separation"));
			} else if (label_reference) {
				size.height = label_reference->get_size().height;
			}

			// Only draw the label if it's not empty.
			if (label.is_empty()) {
				size.height = 0;
			} else {
				Ref<StyleBox> sb = get_theme_stylebox(selected ? SNAME("bg_selected") : SNAME("bg"));
				draw_style_box(sb, Rect2(Vector2(), size));
			}

			Ref<StyleBox> bg_stylebox = get_theme_stylebox(SNAME("child_bg"));
			if (draw_top_bg && right_child_rect != Rect2()) {
				draw_style_box(bg_stylebox, right_child_rect);
			}
			if (bottom_child_rect != Rect2()) {
				draw_style_box(bg_stylebox, bottom_child_rect);
			}

			Color color;
			if (draw_warning || draw_prop_warning) {
				color = get_theme_color(is_read_only() ? SNAME("readonly_warning_color") : SNAME("warning_color"));
			} else {
				color = get_theme_color(is_read_only() ? SNAME("readonly_color") : SNAME("property_color"));
			}
			if (label.contains_char('.')) {
				// FIXME: Move this to the project settings editor, as this is only used
				// for project settings feature tag overrides.
				color.a = 0.5;
			}

			int ofs = get_theme_constant(SNAME("font_offset"));
			int text_limit = text_size - ofs;

			if (checkable) {
				Ref<Texture2D> checkbox;
				if (checked) {
					checkbox = get_editor_theme_icon(SNAME("GuiChecked"));
				} else {
					checkbox = get_editor_theme_icon(SNAME("GuiUnchecked"));
				}

				Color color2(1, 1, 1);
				if (check_hover) {
					color2.r *= 1.2;
					color2.g *= 1.2;
					color2.b *= 1.2;
				}
				check_rect = Rect2(ofs, ((size.height - checkbox->get_height()) / 2), checkbox->get_width(), checkbox->get_height());
				if (rtl) {
					draw_texture(checkbox, Vector2(size.width - check_rect.position.x - checkbox->get_width(), check_rect.position.y), color2);
				} else {
					draw_texture(checkbox, check_rect.position, color2);
				}
				int check_ofs = checkbox->get_width() + get_theme_constant(SNAME("h_separation"), SNAME("Tree"));
				ofs += check_ofs;
				text_limit -= check_ofs;
			} else {
				check_rect = Rect2();
			}

			if (can_revert && !is_read_only()) {
				Ref<Texture2D> reload_icon = get_editor_theme_icon(SNAME("ReloadSmall"));
				text_limit -= reload_icon->get_width() + get_theme_constant(SNAME("h_separation"), SNAME("Tree"));
				revert_rect = Rect2(ofs + text_limit, (size.height - reload_icon->get_height()) / 2, reload_icon->get_width(), reload_icon->get_height());

				Color color2(1, 1, 1);
				if (revert_hover) {
					color2.r *= 1.2;
					color2.g *= 1.2;
					color2.b *= 1.2;
				}
				if (rtl) {
					draw_texture(reload_icon, Vector2(size.width - revert_rect.position.x - reload_icon->get_width(), revert_rect.position.y), color2);
				} else {
					draw_texture(reload_icon, revert_rect.position, color2);
				}
			} else {
				revert_rect = Rect2();
			}

			if (!pin_hidden && pinned) {
				Ref<Texture2D> pinned_icon = get_editor_theme_icon(SNAME("Pin"));
				int margin_w = get_theme_constant(SNAME("h_separation"), SNAME("Tree"));
				int total_icon_w = margin_w + pinned_icon->get_width();
				int text_w = font->get_string_size(label, rtl ? HORIZONTAL_ALIGNMENT_RIGHT : HORIZONTAL_ALIGNMENT_LEFT, text_limit - total_icon_w, font_size).x;
				int y = (size.height - pinned_icon->get_height()) / 2;
				if (rtl) {
					draw_texture(pinned_icon, Vector2(size.width - ofs - text_w - total_icon_w, y), color);
				} else {
					draw_texture(pinned_icon, Vector2(ofs + text_w + margin_w, y), color);
				}
				text_limit -= total_icon_w;
			}

			int v_ofs = (size.height - font->get_height(font_size)) / 2;
			if (rtl) {
				draw_string(font, Point2(size.width - ofs - text_limit, v_ofs + font->get_ascent(font_size)), label, HORIZONTAL_ALIGNMENT_RIGHT, text_limit, font_size, color);
			} else {
				draw_string(font, Point2(ofs, v_ofs + font->get_ascent(font_size)), label, HORIZONTAL_ALIGNMENT_LEFT, text_limit, font_size, color);
			}

			ofs = size.width;

			if (keying) {
				Ref<Texture2D> key;

				if (use_keying_next()) {
					key = get_editor_theme_icon(SNAME("KeyNext"));
				} else {
					key = get_editor_theme_icon(SNAME("Key"));
				}

				ofs -= key->get_width() + get_theme_constant(SNAME("h_separation"), SNAME("Tree"));

				Color color2(1, 1, 1);
				if (keying_hover) {
					color2.r *= 1.2;
					color2.g *= 1.2;
					color2.b *= 1.2;
				}
				keying_rect = Rect2(ofs, ((size.height - key->get_height()) / 2), key->get_width(), key->get_height());
				if (rtl) {
					draw_texture(key, Vector2(size.width - keying_rect.position.x - key->get_width(), keying_rect.position.y), color2);
				} else {
					draw_texture(key, keying_rect.position, color2);
				}

			} else {
				keying_rect = Rect2();
			}

			if (deletable) {
				Ref<Texture2D> close;

				close = get_editor_theme_icon(SNAME("Close"));

				ofs -= close->get_width() + get_theme_constant(SNAME("h_separation"), SNAME("Tree"));

				Color color2(1, 1, 1);
				if (delete_hover) {
					color2.r *= 1.2;
					color2.g *= 1.2;
					color2.b *= 1.2;
				}
				delete_rect = Rect2(ofs, ((size.height - close->get_height()) / 2), close->get_width(), close->get_height());
				if (rtl) {
					draw_texture(close, Vector2(size.width - delete_rect.position.x - close->get_width(), delete_rect.position.y), color2);
				} else {
					draw_texture(close, delete_rect.position, color2);
				}
			} else {
				delete_rect = Rect2();
			}
		} break;
		case NOTIFICATION_ENTER_TREE: {
			if (has_borders) {
				get_parent()->connect(SceneStringName(theme_changed), callable_mp(this, &EditorProperty::_update_property_bg));
				_update_property_bg();
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (has_borders) {
				get_parent()->disconnect(SceneStringName(theme_changed), callable_mp(this, &EditorProperty::_update_property_bg));
			}
		} break;
	}
}

void EditorProperty::set_label(const String &p_label) {
	label = p_label;
	queue_redraw();
}

String EditorProperty::get_label() const {
	return label;
}

Object *EditorProperty::get_edited_object() {
	return object;
}

StringName EditorProperty::get_edited_property() const {
	return property;
}

EditorInspector *EditorProperty::get_parent_inspector() const {
	Node *parent = get_parent();
	while (parent) {
		EditorInspector *ei = Object::cast_to<EditorInspector>(parent);
		if (ei) {
			return ei;
		}
		parent = parent->get_parent();
	}
	return nullptr;
}

void EditorProperty::set_doc_path(const String &p_doc_path) {
	doc_path = p_doc_path;
}

void EditorProperty::set_internal(bool p_internal) {
	internal = p_internal;
}

void EditorProperty::update_property() {
	GDVIRTUAL_CALL(_update_property);
}

void EditorProperty::_set_read_only(bool p_read_only) {
}

void EditorProperty::set_read_only(bool p_read_only) {
	read_only = p_read_only;
	if (GDVIRTUAL_CALL(_set_read_only, p_read_only)) {
		return;
	}
	_set_read_only(p_read_only);
}

bool EditorProperty::is_read_only() const {
	return read_only;
}

Variant EditorPropertyRevert::get_property_revert_value(Object *p_object, const StringName &p_property, bool *r_is_valid) {
	if (p_object->property_can_revert(p_property)) {
		if (r_is_valid) {
			*r_is_valid = true;
		}
		return p_object->property_get_revert(p_property);
	}

	return PropertyUtils::get_property_default_value(p_object, p_property, r_is_valid);
}

bool EditorPropertyRevert::can_property_revert(Object *p_object, const StringName &p_property, const Variant *p_custom_current_value) {
	bool is_valid_revert = false;
	Variant revert_value = EditorPropertyRevert::get_property_revert_value(p_object, p_property, &is_valid_revert);
	if (!is_valid_revert) {
		return false;
	}
	Variant current_value = p_custom_current_value ? *p_custom_current_value : p_object->get(p_property);
	return PropertyUtils::is_property_value_different(p_object, current_value, revert_value);
}

StringName EditorProperty::_get_revert_property() const {
	return property;
}

void EditorProperty::_update_property_bg() {
	// This function is to be called on EditorPropertyResource, EditorPropertyArray, and EditorPropertyDictionary.
	// Behavior is undetermined on any other EditorProperty.
	if (!is_inside_tree()) {
		return;
	}

	begin_bulk_theme_override();

	if (bottom_editor) {
		ColorationMode nested_color_mode = (ColorationMode)(int)EDITOR_GET("interface/inspector/nested_color_mode");
		bool delimitate_all_container_and_resources = EDITOR_GET("interface/inspector/delimitate_all_container_and_resources");
		int count_subinspectors = 0;
		if (is_colored(nested_color_mode)) {
			Node *n = this;
			while (n) {
				EditorProperty *ep = Object::cast_to<EditorProperty>(n);
				if (ep && ep->is_colored(nested_color_mode)) {
					count_subinspectors++;
				}
				n = n->get_parent();
			}
			count_subinspectors = MIN(16, count_subinspectors);
		}
		add_theme_style_override(SNAME("DictionaryAddItem"), get_theme_stylebox("DictionaryAddItem" + itos(count_subinspectors), EditorStringName(EditorStyles)));
		add_theme_constant_override("v_separation", 0);
		if (delimitate_all_container_and_resources || is_colored(nested_color_mode)) {
			add_theme_style_override("bg_selected", get_theme_stylebox("sub_inspector_property_bg" + itos(count_subinspectors), EditorStringName(EditorStyles)));
			add_theme_style_override("bg", get_theme_stylebox("sub_inspector_property_bg" + itos(count_subinspectors), EditorStringName(EditorStyles)));
			add_theme_color_override("property_color", get_theme_color(SNAME("sub_inspector_property_color"), EditorStringName(EditorStyles)));
			bottom_editor->add_theme_style_override(SceneStringName(panel), get_theme_stylebox("sub_inspector_bg" + itos(count_subinspectors), EditorStringName(EditorStyles)));
		} else {
			bottom_editor->add_theme_style_override(SceneStringName(panel), get_theme_stylebox("sub_inspector_bg_no_border", EditorStringName(EditorStyles)));
		}
	} else {
		remove_theme_style_override("bg_selected");
		remove_theme_style_override("bg");
		remove_theme_color_override("property_color");
	}
	end_bulk_theme_override();
	queue_redraw();
}

void EditorProperty::update_editor_property_status() {
	if (property == StringName()) {
		return; //no property, so nothing to do
	}

	bool new_pinned = false;
	if (can_pin) {
		Node *node = Object::cast_to<Node>(object);
		CRASH_COND(!node);
		new_pinned = node->is_property_pinned(property);
	}

	bool new_warning = false;
	if (object->has_method("_get_property_warning")) {
		new_warning = !String(object->call("_get_property_warning", property)).is_empty();
	}

	Variant current = object->get(_get_revert_property());
	bool new_can_revert = EditorPropertyRevert::can_property_revert(object, property, &current) && !is_read_only();

	bool new_checked = checked;
	if (checkable) { // for properties like theme overrides.
		bool valid = false;
		Variant value = object->get(property, &valid);
		if (valid) {
			new_checked = value.get_type() != Variant::NIL;
		}
	}

	if (new_can_revert != can_revert || new_pinned != pinned || new_checked != checked || new_warning != draw_prop_warning) {
		if (new_can_revert != can_revert) {
			emit_signal(SNAME("property_can_revert_changed"), property, new_can_revert);
		}
		draw_prop_warning = new_warning;
		can_revert = new_can_revert;
		pinned = new_pinned;
		checked = new_checked;
		queue_redraw();
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
	queue_redraw();
	queue_sort();
}

bool EditorProperty::is_checkable() const {
	return checkable;
}

void EditorProperty::set_checked(bool p_checked) {
	checked = p_checked;
	queue_redraw();
}

bool EditorProperty::is_checked() const {
	return checked;
}

void EditorProperty::set_draw_warning(bool p_draw_warning) {
	draw_warning = p_draw_warning;
	queue_redraw();
}

void EditorProperty::set_keying(bool p_keying) {
	keying = p_keying;
	queue_redraw();
	queue_sort();
}

void EditorProperty::set_deletable(bool p_deletable) {
	deletable = p_deletable;
	queue_redraw();
	queue_sort();
}

bool EditorProperty::is_deletable() const {
	return deletable;
}

bool EditorProperty::is_keying() const {
	return keying;
}

bool EditorProperty::is_draw_warning() const {
	return draw_warning;
}

void EditorProperty::_focusable_focused(int p_index) {
	if (!selectable) {
		return;
	}
	bool already_selected = selected;
	selected = true;
	selected_focusable = p_index;
	queue_redraw();
	if (!already_selected && selected) {
		emit_signal(SNAME("selected"), property, selected_focusable);
	}
}

void EditorProperty::add_focusable(Control *p_control) {
	p_control->connect(SceneStringName(focus_entered), callable_mp(this, &EditorProperty::_focusable_focused).bind(focusables.size()));
	focusables.push_back(p_control);
}

void EditorProperty::grab_focus(int p_focusable) {
	if (focusables.is_empty()) {
		return;
	}

	if (p_focusable >= 0) {
		ERR_FAIL_INDEX(p_focusable, focusables.size());
		focusables[p_focusable]->grab_focus();
	} else {
		focusables[0]->grab_focus();
	}
}

void EditorProperty::select(int p_focusable) {
	bool already_selected = selected;
	if (!selectable) {
		return;
	}

	if (p_focusable >= 0) {
		ERR_FAIL_INDEX(p_focusable, focusables.size());
		focusables[p_focusable]->grab_focus();
	} else {
		selected = true;
		queue_redraw();
	}

	if (!already_selected && selected) {
		emit_signal(SNAME("selected"), property, selected_focusable);
	}
}

void EditorProperty::deselect() {
	selected = false;
	selected_focusable = -1;
	queue_redraw();
}

bool EditorProperty::is_selected() const {
	return selected;
}

void EditorProperty::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (property == StringName()) {
		return;
	}

	Ref<InputEventMouse> me = p_event;

	if (me.is_valid()) {
		Vector2 mpos = me->get_position();
		if (is_layout_rtl()) {
			mpos.x = get_size().x - mpos.x;
		}
		bool button_left = me->get_button_mask().has_flag(MouseButtonMask::LEFT);

		bool new_keying_hover = keying_rect.has_point(mpos) && !button_left;
		if (new_keying_hover != keying_hover) {
			keying_hover = new_keying_hover;
			queue_redraw();
		}

		bool new_delete_hover = delete_rect.has_point(mpos) && !button_left;
		if (new_delete_hover != delete_hover) {
			delete_hover = new_delete_hover;
			queue_redraw();
		}

		bool new_revert_hover = revert_rect.has_point(mpos) && !button_left;
		if (new_revert_hover != revert_hover) {
			revert_hover = new_revert_hover;
			queue_redraw();
		}

		bool new_check_hover = check_rect.has_point(mpos) && !button_left;
		if (new_check_hover != check_hover) {
			check_hover = new_check_hover;
			queue_redraw();
		}
	}

	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		Vector2 mpos = mb->get_position();
		if (is_layout_rtl()) {
			mpos.x = get_size().x - mpos.x;
		}

		select();

		if (keying_rect.has_point(mpos)) {
			accept_event();
			emit_signal(SNAME("property_keyed"), property, use_keying_next());

			if (use_keying_next()) {
				if (property == "frame_coords" && (object->is_class("Sprite2D") || object->is_class("Sprite3D"))) {
					Vector2i new_coords = object->get(property);
					new_coords.x++;
					if (new_coords.x >= int64_t(object->get("hframes"))) {
						new_coords.x = 0;
						new_coords.y++;
					}
					if (new_coords.x < int64_t(object->get("hframes")) && new_coords.y < int64_t(object->get("vframes"))) {
						callable_mp(this, &EditorProperty::emit_changed).call_deferred(property, new_coords, "", false);
					}
				} else {
					if (int64_t(object->get(property)) + 1 < (int64_t(object->get("hframes")) * int64_t(object->get("vframes")))) {
						callable_mp(this, &EditorProperty::emit_changed).call_deferred(property, object->get(property).operator int64_t() + 1, "", false);
					}
				}
				callable_mp(this, &EditorProperty::update_property).call_deferred();
			}
		}
		if (delete_rect.has_point(mpos)) {
			accept_event();
			emit_signal(SNAME("property_deleted"), property);
		}

		if (revert_rect.has_point(mpos)) {
			accept_event();
			get_viewport()->gui_release_focus();
			bool is_valid_revert = false;
			Variant revert_value = EditorPropertyRevert::get_property_revert_value(object, property, &is_valid_revert);
			ERR_FAIL_COND(!is_valid_revert);
			emit_changed(_get_revert_property(), revert_value);
			update_property();
		}

		if (check_rect.has_point(mpos)) {
			accept_event();
			checked = !checked;
			queue_redraw();
			emit_signal(SNAME("property_checked"), property, checked);
		}
	} else if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::RIGHT) {
		accept_event();
		_update_popup();
		menu->set_position(get_screen_position() + get_local_mouse_position());
		menu->reset_size();
		menu->popup();
		select();
		return;
	}
}

void EditorProperty::shortcut_input(const Ref<InputEvent> &p_event) {
	if (!selected || !p_event->is_pressed()) {
		return;
	}

	const Ref<InputEventKey> k = p_event;

	if (k.is_valid() && k->is_pressed()) {
		if (ED_IS_SHORTCUT("property_editor/copy_value", p_event)) {
			menu_option(MENU_COPY_VALUE);
			accept_event();
		} else if (!is_read_only() && ED_IS_SHORTCUT("property_editor/paste_value", p_event)) {
			menu_option(MENU_PASTE_VALUE);
			accept_event();
		} else if (!internal && ED_IS_SHORTCUT("property_editor/copy_property_path", p_event)) {
			menu_option(MENU_COPY_PROPERTY_PATH);
			accept_event();
		}
	}
}

const Color *EditorProperty::_get_property_colors() {
	static Color c[4];
	c[0] = get_theme_color(SNAME("property_color_x"), EditorStringName(Editor));
	c[1] = get_theme_color(SNAME("property_color_y"), EditorStringName(Editor));
	c[2] = get_theme_color(SNAME("property_color_z"), EditorStringName(Editor));
	c[3] = get_theme_color(SNAME("property_color_w"), EditorStringName(Editor));
	return c;
}

void EditorProperty::set_label_reference(Control *p_control) {
	label_reference = p_control;
}

void EditorProperty::set_bottom_editor(Control *p_control) {
	bottom_editor = p_control;
	if (has_borders) {
		_update_property_bg();
	}
}

Variant EditorProperty::_get_cache_value(const StringName &p_prop, bool &r_valid) const {
	return object->get(p_prop, &r_valid);
}

bool EditorProperty::is_cache_valid() const {
	if (object) {
		for (const KeyValue<StringName, Variant> &E : cache) {
			bool valid;
			Variant value = _get_cache_value(E.key, valid);
			if (!valid || value != E.value) {
				return false;
			}
		}
	}
	return true;
}
void EditorProperty::update_cache() {
	cache.clear();
	if (object && property != StringName()) {
		bool valid;
		Variant value = _get_cache_value(property, valid);
		if (valid) {
			cache[property] = value;
		}
	}
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

	Label *drag_label = memnew(Label);
	drag_label->set_text(property);
	drag_label->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED); // Don't translate raw property name.
	set_drag_preview(drag_label);
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

void EditorProperty::expand_revertable() {
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

void EditorProperty::set_favoritable(bool p_favoritable) {
	can_favorite = p_favoritable;
}

bool EditorProperty::is_favoritable() const {
	return can_favorite;
}

void EditorProperty::set_object_and_property(Object *p_object, const StringName &p_property) {
	object = p_object;
	property = p_property;

	_update_flags();
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

void EditorProperty::_update_flags() {
	can_pin = false;
	pin_hidden = true;

	if (read_only) {
		return;
	}

	if (Node *node = Object::cast_to<Node>(object)) {
		// Avoid errors down the road by ignoring nodes which are not part of a scene
		if (!node->get_owner()) {
			bool is_scene_root = false;
			for (int i = 0; i < EditorNode::get_editor_data().get_edited_scene_count(); ++i) {
				if (EditorNode::get_editor_data().get_edited_scene_root(i) == node) {
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
			HashSet<StringName> storable_properties;
			node->get_storable_properties(storable_properties);
			if (storable_properties.has(node->get_property_store_alias(property))) {
				can_pin = true;
			}
		}
	}
}

Control *EditorProperty::make_custom_tooltip(const String &p_text) const {
	String symbol;
	String prologue;

	if (object->has_method("_get_property_warning")) {
		const String custom_warning = object->call("_get_property_warning", property);
		if (!custom_warning.is_empty()) {
			prologue = "[b][color=" + get_theme_color(SNAME("warning_color")).to_html(false) + "]" + custom_warning + "[/color][/b]";
		}
	}

	if (has_doc_tooltip) {
		symbol = p_text;

		const EditorInspector *inspector = get_parent_inspector();
		if (inspector) {
			const String custom_description = inspector->get_custom_property_description(p_text);
			if (!custom_description.is_empty()) {
				if (!prologue.is_empty()) {
					prologue += '\n';
				}
				prologue += custom_description;
			}
		}
	}

	if (!symbol.is_empty() || !prologue.is_empty()) {
		return EditorHelpBitTooltip::show_tooltip(const_cast<EditorProperty *>(this), symbol, prologue);
	}

	return nullptr;
}

void EditorProperty::menu_option(int p_option) {
	switch (p_option) {
		case MENU_COPY_VALUE: {
			InspectorDock::get_inspector_singleton()->set_property_clipboard(object->get(property));
		} break;
		case MENU_PASTE_VALUE: {
			emit_changed(property, InspectorDock::get_inspector_singleton()->get_property_clipboard());
		} break;
		case MENU_COPY_PROPERTY_PATH: {
			DisplayServer::get_singleton()->clipboard_set(property_path);
		} break;
		case MENU_FAVORITE_PROPERTY: {
			emit_signal(SNAME("property_favorited"), property, !favorited);
			queue_redraw();
		} break;
		case MENU_PIN_VALUE: {
			emit_signal(SNAME("property_pinned"), property, !pinned);
			queue_redraw();
		} break;
		case MENU_OPEN_DOCUMENTATION: {
			ScriptEditor::get_singleton()->goto_help(doc_path);
			EditorNode::get_singleton()->get_editor_main_screen()->select(EditorMainScreen::EDITOR_SCRIPT);
		} break;
	}
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

	ClassDB::bind_method(D_METHOD("set_draw_warning", "draw_warning"), &EditorProperty::set_draw_warning);
	ClassDB::bind_method(D_METHOD("is_draw_warning"), &EditorProperty::is_draw_warning);

	ClassDB::bind_method(D_METHOD("set_keying", "keying"), &EditorProperty::set_keying);
	ClassDB::bind_method(D_METHOD("is_keying"), &EditorProperty::is_keying);

	ClassDB::bind_method(D_METHOD("set_deletable", "deletable"), &EditorProperty::set_deletable);
	ClassDB::bind_method(D_METHOD("is_deletable"), &EditorProperty::is_deletable);

	ClassDB::bind_method(D_METHOD("get_edited_property"), &EditorProperty::get_edited_property);
	ClassDB::bind_method(D_METHOD("get_edited_object"), &EditorProperty::get_edited_object);

	ClassDB::bind_method(D_METHOD("update_property"), &EditorProperty::update_property);

	ClassDB::bind_method(D_METHOD("add_focusable", "control"), &EditorProperty::add_focusable);
	ClassDB::bind_method(D_METHOD("set_bottom_editor", "editor"), &EditorProperty::set_bottom_editor);

	ClassDB::bind_method(D_METHOD("set_selectable", "selectable"), &EditorProperty::set_selectable);
	ClassDB::bind_method(D_METHOD("is_selectable"), &EditorProperty::is_selectable);

	ClassDB::bind_method(D_METHOD("set_use_folding", "use_folding"), &EditorProperty::set_use_folding);
	ClassDB::bind_method(D_METHOD("is_using_folding"), &EditorProperty::is_using_folding);

	ClassDB::bind_method(D_METHOD("set_name_split_ratio", "ratio"), &EditorProperty::set_name_split_ratio);
	ClassDB::bind_method(D_METHOD("get_name_split_ratio"), &EditorProperty::get_name_split_ratio);

	ClassDB::bind_method(D_METHOD("deselect"), &EditorProperty::deselect);
	ClassDB::bind_method(D_METHOD("is_selected"), &EditorProperty::is_selected);
	ClassDB::bind_method(D_METHOD("select", "focusable"), &EditorProperty::select, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("set_object_and_property", "object", "property"), &EditorProperty::set_object_and_property);
	ClassDB::bind_method(D_METHOD("set_label_reference", "control"), &EditorProperty::set_label_reference);

	ClassDB::bind_method(D_METHOD("emit_changed", "property", "value", "field", "changing"), &EditorProperty::emit_changed, DEFVAL(StringName()), DEFVAL(false));

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "label"), "set_label", "get_label");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "read_only"), "set_read_only", "is_read_only");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "checkable"), "set_checkable", "is_checkable");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "checked"), "set_checked", "is_checked");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "draw_warning"), "set_draw_warning", "is_draw_warning");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "keying"), "set_keying", "is_keying");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "deletable"), "set_deletable", "is_deletable");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "selectable"), "set_selectable", "is_selectable");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_folding"), "set_use_folding", "is_using_folding");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "name_split_ratio"), "set_name_split_ratio", "get_name_split_ratio");

	ADD_SIGNAL(MethodInfo("property_changed", PropertyInfo(Variant::STRING_NAME, "property"), PropertyInfo(Variant::NIL, "value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT), PropertyInfo(Variant::STRING_NAME, "field"), PropertyInfo(Variant::BOOL, "changing")));
	ADD_SIGNAL(MethodInfo("multiple_properties_changed", PropertyInfo(Variant::PACKED_STRING_ARRAY, "properties"), PropertyInfo(Variant::ARRAY, "value")));
	ADD_SIGNAL(MethodInfo("property_keyed", PropertyInfo(Variant::STRING_NAME, "property")));
	ADD_SIGNAL(MethodInfo("property_deleted", PropertyInfo(Variant::STRING_NAME, "property")));
	ADD_SIGNAL(MethodInfo("property_keyed_with_value", PropertyInfo(Variant::STRING_NAME, "property"), PropertyInfo(Variant::NIL, "value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
	ADD_SIGNAL(MethodInfo("property_checked", PropertyInfo(Variant::STRING_NAME, "property"), PropertyInfo(Variant::BOOL, "checked")));
	ADD_SIGNAL(MethodInfo("property_favorited", PropertyInfo(Variant::STRING_NAME, "property"), PropertyInfo(Variant::BOOL, "favorited")));
	ADD_SIGNAL(MethodInfo("property_pinned", PropertyInfo(Variant::STRING_NAME, "property"), PropertyInfo(Variant::BOOL, "pinned")));
	ADD_SIGNAL(MethodInfo("property_can_revert_changed", PropertyInfo(Variant::STRING_NAME, "property"), PropertyInfo(Variant::BOOL, "can_revert")));
	ADD_SIGNAL(MethodInfo("resource_selected", PropertyInfo(Variant::STRING, "path"), PropertyInfo(Variant::OBJECT, "resource", PROPERTY_HINT_RESOURCE_TYPE, "Resource")));
	ADD_SIGNAL(MethodInfo("object_id_selected", PropertyInfo(Variant::STRING_NAME, "property"), PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("selected", PropertyInfo(Variant::STRING, "path"), PropertyInfo(Variant::INT, "focusable_idx")));

	GDVIRTUAL_BIND(_update_property)
	GDVIRTUAL_BIND(_set_read_only, "read_only")

	ClassDB::bind_method(D_METHOD("_update_editor_property_status"), &EditorProperty::update_editor_property_status);
}

EditorProperty::EditorProperty() {
	object = nullptr;
	split_ratio = 0.5;
	text_size = 0;
	property_usage = 0;
	selected_focusable = -1;
	label_reference = nullptr;
	bottom_editor = nullptr;
	menu = nullptr;
	set_process_shortcut_input(true);
}

void EditorProperty::_update_popup() {
	if (menu) {
		menu->clear();
	} else {
		menu = memnew(PopupMenu);
		add_child(menu);
		menu->connect(SceneStringName(id_pressed), callable_mp(this, &EditorProperty::menu_option));
	}
	menu->add_icon_shortcut(get_editor_theme_icon(SNAME("ActionCopy")), ED_GET_SHORTCUT("property_editor/copy_value"), MENU_COPY_VALUE);
	menu->add_icon_shortcut(get_editor_theme_icon(SNAME("ActionPaste")), ED_GET_SHORTCUT("property_editor/paste_value"), MENU_PASTE_VALUE);
	menu->add_icon_shortcut(get_editor_theme_icon(SNAME("CopyNodePath")), ED_GET_SHORTCUT("property_editor/copy_property_path"), MENU_COPY_PROPERTY_PATH);
	menu->set_item_disabled(MENU_PASTE_VALUE, is_read_only());
	menu->set_item_disabled(MENU_COPY_PROPERTY_PATH, internal);

	if (can_favorite || !pin_hidden) {
		menu->add_separator();
	}

	if (can_favorite) {
		if (favorited) {
			menu->add_icon_item(get_editor_theme_icon(SNAME("Unfavorite")), TTR("Unfavorite Property"), MENU_FAVORITE_PROPERTY);
			menu->set_item_tooltip(menu->get_item_index(MENU_FAVORITE_PROPERTY), TTR("Make this property be put back at its original place."));
		} else {
			menu->add_icon_item(get_editor_theme_icon(SNAME("Favorites")), TTR("Favorite Property"), MENU_FAVORITE_PROPERTY);
			menu->set_item_tooltip(menu->get_item_index(MENU_FAVORITE_PROPERTY), TTR("Make this property be placed at the top for all objects of this class."));
		}
	}

	if (!pin_hidden) {
		if (can_pin) {
			menu->add_icon_check_item(get_editor_theme_icon(SNAME("Pin")), TTR("Pin Value"), MENU_PIN_VALUE);
			menu->set_item_checked(menu->get_item_index(MENU_PIN_VALUE), pinned);
		} else {
			menu->add_icon_check_item(get_editor_theme_icon(SNAME("Pin")), vformat(TTR("Pin Value [Disabled because '%s' is editor-only]"), property), MENU_PIN_VALUE);
			menu->set_item_disabled(menu->get_item_index(MENU_PIN_VALUE), true);
		}
		menu->set_item_tooltip(menu->get_item_index(MENU_PIN_VALUE), TTR("Pinning a value forces it to be saved even if it's equal to the default."));
	}

	if (!doc_path.is_empty()) {
		menu->add_separator();
		menu->add_icon_item(get_editor_theme_icon(SNAME("Help")), TTR("Open Documentation"), MENU_OPEN_DOCUMENTATION);
	}
}

////////////////////////////////////////////////
////////////////////////////////////////////////

void EditorInspectorPlugin::add_custom_control(Control *control) {
	AddedEditor ae;
	ae.property_editor = control;
	added_editors.push_back(ae);
}

void EditorInspectorPlugin::add_property_editor(const String &p_for_property, Control *p_prop, bool p_add_to_end, const String &p_label) {
	AddedEditor ae;
	ae.properties.push_back(p_for_property);
	ae.property_editor = p_prop;
	ae.add_to_end = p_add_to_end;
	ae.label = p_label;
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
	bool success = false;
	GDVIRTUAL_CALL(_can_handle, p_object, success);
	return success;
}

void EditorInspectorPlugin::parse_begin(Object *p_object) {
	GDVIRTUAL_CALL(_parse_begin, p_object);
}

void EditorInspectorPlugin::parse_category(Object *p_object, const String &p_category) {
	GDVIRTUAL_CALL(_parse_category, p_object, p_category);
}

void EditorInspectorPlugin::parse_group(Object *p_object, const String &p_group) {
	GDVIRTUAL_CALL(_parse_group, p_object, p_group);
}

bool EditorInspectorPlugin::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	bool ret = false;
	GDVIRTUAL_CALL(_parse_property, p_object, p_type, p_path, p_hint, p_hint_text, p_usage, p_wide, ret);
	return ret;
}

void EditorInspectorPlugin::parse_end(Object *p_object) {
	GDVIRTUAL_CALL(_parse_end, p_object);
}

void EditorInspectorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_custom_control", "control"), &EditorInspectorPlugin::add_custom_control);
	ClassDB::bind_method(D_METHOD("add_property_editor", "property", "editor", "add_to_end", "label"), &EditorInspectorPlugin::add_property_editor, DEFVAL(false), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("add_property_editor_for_multiple_properties", "label", "properties", "editor"), &EditorInspectorPlugin::add_property_editor_for_multiple_properties);

	GDVIRTUAL_BIND(_can_handle, "object")
	GDVIRTUAL_BIND(_parse_begin, "object")
	GDVIRTUAL_BIND(_parse_category, "object", "category")
	GDVIRTUAL_BIND(_parse_group, "object", "group")
	GDVIRTUAL_BIND(_parse_property, "object", "type", "name", "hint_type", "hint_string", "usage_flags", "wide");
	GDVIRTUAL_BIND(_parse_end, "object")
}

////////////////////////////////////////////////
////////////////////////////////////////////////

void EditorInspectorCategory::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			if (menu) {
				if (is_favorite) {
					menu->set_item_icon(menu->get_item_index(EditorInspector::MENU_UNFAVORITE_ALL), get_editor_theme_icon(SNAME("Unfavorite")));
				} else {
					menu->set_item_icon(menu->get_item_index(MENU_OPEN_DOCS), get_editor_theme_icon(SNAME("Help")));
				}
			}
		} break;
		case NOTIFICATION_DRAW: {
			Ref<StyleBox> sb = get_theme_stylebox(SNAME("bg"));

			draw_style_box(sb, Rect2(Vector2(), get_size()));

			Ref<Font> font = get_theme_font(SNAME("bold"), EditorStringName(EditorFonts));
			int font_size = get_theme_font_size(SNAME("bold_size"), EditorStringName(EditorFonts));

			int hs = get_theme_constant(SNAME("h_separation"), SNAME("Tree"));
			int icon_size = get_theme_constant(SNAME("class_icon_size"), EditorStringName(Editor));

			int w = font->get_string_size(label, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).width;
			if (icon.is_valid()) {
				w += hs + icon_size;
			}
			w = MIN(w, get_size().width - sb->get_minimum_size().width);

			int ofs = (get_size().width - w) / 2;

			float v_margin_offset = sb->get_content_margin(SIDE_TOP) - sb->get_content_margin(SIDE_BOTTOM);

			if (icon.is_valid()) {
				Size2 rect_size = Size2(icon_size, icon_size);
				Point2 rect_pos = Point2(ofs, (get_size().height - icon_size) / 2 + v_margin_offset).round();
				if (is_layout_rtl()) {
					rect_pos.x = get_size().width - rect_pos.x - icon_size;
				}
				draw_texture_rect(icon, Rect2(rect_pos, rect_size));

				ofs += hs + icon_size;
				w -= hs + icon_size;
			}

			Color color = get_theme_color(SceneStringName(font_color), SNAME("Tree"));
			if (is_layout_rtl()) {
				ofs = get_size().width - ofs - w;
			}
			float text_pos_y = font->get_ascent(font_size) + (get_size().height - font->get_height(font_size)) / 2 + v_margin_offset;
			Point2 text_pos = Point2(ofs, text_pos_y).round();
			draw_string(font, text_pos, label, HORIZONTAL_ALIGNMENT_LEFT, w, font_size, color);
		} break;
	}
}

Control *EditorInspectorCategory::make_custom_tooltip(const String &p_text) const {
	// If it's not a doc tooltip, fallback to the default one.
	if (doc_class_name.is_empty()) {
		return nullptr;
	}

	return EditorHelpBitTooltip::show_tooltip(const_cast<EditorInspectorCategory *>(this), p_text);
}

void EditorInspectorCategory::set_as_favorite(EditorInspector *p_for_inspector) {
	is_favorite = true;

	menu = memnew(PopupMenu);
	menu->add_item(TTR("Unfavorite All"), EditorInspector::MENU_UNFAVORITE_ALL);
	add_child(menu);
	menu->connect(SceneStringName(id_pressed), callable_mp(p_for_inspector, &EditorInspector::_handle_menu_option));
}

Size2 EditorInspectorCategory::get_minimum_size() const {
	Ref<Font> font = get_theme_font(SNAME("bold"), EditorStringName(EditorFonts));
	int font_size = get_theme_font_size(SNAME("bold_size"), EditorStringName(EditorFonts));

	Size2 ms;
	ms.height = font->get_height(font_size);
	if (icon.is_valid()) {
		int icon_size = get_theme_constant(SNAME("class_icon_size"), EditorStringName(Editor));
		ms.height = MAX(icon_size, ms.height);
	}
	ms.height += get_theme_constant(SNAME("v_separation"), SNAME("Tree"));

	const Ref<StyleBox> &bg_style = get_theme_stylebox(SNAME("bg"));
	ms.height += bg_style->get_content_margin(SIDE_TOP) + bg_style->get_content_margin(SIDE_BOTTOM);

	return ms;
}

void EditorInspectorCategory::_handle_menu_option(int p_option) {
	switch (p_option) {
		case MENU_OPEN_DOCS:
			ScriptEditor::get_singleton()->goto_help("class:" + doc_class_name);
			EditorNode::get_singleton()->get_editor_main_screen()->select(EditorMainScreen::EDITOR_SCRIPT);
			break;
	}
}

void EditorInspectorCategory::gui_input(const Ref<InputEvent> &p_event) {
	if (!is_favorite && doc_class_name.is_empty()) {
		return;
	}

	const Ref<InputEventMouseButton> &mb_event = p_event;
	if (mb_event.is_null() || !mb_event->is_pressed() || mb_event->get_button_index() != MouseButton::RIGHT) {
		return;
	}

	if (!is_favorite) {
		if (!menu) {
			menu = memnew(PopupMenu);
			menu->add_icon_item(get_editor_theme_icon(SNAME("Help")), TTR("Open Documentation"), MENU_OPEN_DOCS);
			add_child(menu);
			menu->connect(SceneStringName(id_pressed), callable_mp(this, &EditorInspectorCategory::_handle_menu_option));
		}
		menu->set_item_disabled(menu->get_item_index(MENU_OPEN_DOCS), !EditorHelp::get_doc_data()->class_list.has(doc_class_name));
	}

	menu->set_position(get_screen_position() + mb_event->get_position());
	menu->reset_size();
	menu->popup();
}

////////////////////////////////////////////////
////////////////////////////////////////////////

void EditorInspectorSection::_test_unfold() {
	if (!vbox_added) {
		add_child(vbox);
		move_child(vbox, 0);
		vbox_added = true;
	}
}

Ref<Texture2D> EditorInspectorSection::_get_arrow() {
	Ref<Texture2D> arrow;
	if (foldable) {
		if (object->editor_is_section_unfolded(section)) {
			arrow = get_theme_icon(SNAME("arrow"), SNAME("Tree"));
		} else {
			if (is_layout_rtl()) {
				arrow = get_theme_icon(SNAME("arrow_collapsed_mirrored"), SNAME("Tree"));
			} else {
				arrow = get_theme_icon(SNAME("arrow_collapsed"), SNAME("Tree"));
			}
		}
	}
	return arrow;
}

int EditorInspectorSection::_get_header_height() {
	Ref<Font> font = get_theme_font(SNAME("bold"), EditorStringName(EditorFonts));
	int font_size = get_theme_font_size(SNAME("bold_size"), EditorStringName(EditorFonts));

	int header_height = font->get_height(font_size);
	Ref<Texture2D> arrow = _get_arrow();
	if (arrow.is_valid()) {
		header_height = MAX(header_height, arrow->get_height());
	}
	header_height += get_theme_constant(SNAME("v_separation"), SNAME("Tree"));

	return header_height;
}

void EditorInspectorSection::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			update_minimum_size();
			bg_color = get_theme_color(SNAME("prop_subsection"), EditorStringName(Editor));
			bg_color.a /= level;
		} break;

		case NOTIFICATION_SORT_CHILDREN: {
			if (!vbox_added) {
				return;
			}

			int inspector_margin = get_theme_constant(SNAME("inspector_margin"), EditorStringName(Editor));
			int section_indent_size = get_theme_constant(SNAME("indent_size"), SNAME("EditorInspectorSection"));
			if (indent_depth > 0 && section_indent_size > 0) {
				inspector_margin += indent_depth * section_indent_size;
			}
			Ref<StyleBoxFlat> section_indent_style = get_theme_stylebox(SNAME("indent_box"), SNAME("EditorInspectorSection"));
			if (indent_depth > 0 && section_indent_style.is_valid()) {
				inspector_margin += section_indent_style->get_margin(SIDE_LEFT) + section_indent_style->get_margin(SIDE_RIGHT);
			}

			Size2 size = get_size() - Vector2(inspector_margin, 0);
			int header_height = _get_header_height();
			Vector2 offset = Vector2(is_layout_rtl() ? 0 : inspector_margin, header_height);
			for (int i = 0; i < get_child_count(); i++) {
				Control *c = as_sortable_control(get_child(i));
				if (!c) {
					continue;
				}
				fit_child_in_rect(c, Rect2(offset, size));
			}
		} break;

		case NOTIFICATION_DRAW: {
			int section_indent = 0;
			int section_indent_size = get_theme_constant(SNAME("indent_size"), SNAME("EditorInspectorSection"));
			if (indent_depth > 0 && section_indent_size > 0) {
				section_indent = indent_depth * section_indent_size;
			}
			Ref<StyleBoxFlat> section_indent_style = get_theme_stylebox(SNAME("indent_box"), SNAME("EditorInspectorSection"));
			if (indent_depth > 0 && section_indent_style.is_valid()) {
				section_indent += section_indent_style->get_margin(SIDE_LEFT) + section_indent_style->get_margin(SIDE_RIGHT);
			}

			int header_width = get_size().width - section_indent;
			int header_offset_x = 0.0;
			bool rtl = is_layout_rtl();
			if (!rtl) {
				header_offset_x += section_indent;
			}

			// Draw header area.
			int header_height = _get_header_height();
			Rect2 header_rect = Rect2(Vector2(header_offset_x, 0.0), Vector2(header_width, header_height));
			Color c = bg_color;
			c.a *= 0.4;
			if (foldable && header_rect.has_point(get_local_mouse_position())) {
				c = c.lightened(Input::get_singleton()->is_mouse_button_pressed(MouseButton::LEFT) ? -0.05 : 0.2);
			}
			draw_rect(header_rect, c);

			// Draw header title, folding arrow and count of revertable properties.
			{
				int outer_margin = Math::round(2 * EDSCALE);
				int separation = get_theme_constant(SNAME("h_separation"), SNAME("EditorInspectorSection"));

				int margin_start = section_indent + outer_margin;
				int margin_end = outer_margin;

				// - Arrow.
				Ref<Texture2D> arrow = _get_arrow();
				if (arrow.is_valid()) {
					Point2 arrow_position;
					if (rtl) {
						arrow_position.x = get_size().width - (margin_start + arrow->get_width());
					} else {
						arrow_position.x = margin_start;
					}
					arrow_position.y = (header_height - arrow->get_height()) / 2;
					draw_texture(arrow, arrow_position);
					margin_start += arrow->get_width() + separation;
				}

				int available = get_size().width - (margin_start + margin_end);

				// - Count of revertable properties.
				String num_revertable_str;
				int num_revertable_width = 0;

				bool folded = foldable && !object->editor_is_section_unfolded(section);

				Ref<Font> font = get_theme_font(SNAME("bold"), EditorStringName(EditorFonts));
				int font_size = get_theme_font_size(SNAME("bold_size"), EditorStringName(EditorFonts));
				Color font_color = get_theme_color(SceneStringName(font_color), EditorStringName(Editor));

				if (folded && revertable_properties.size()) {
					int label_width = font->get_string_size(label, HORIZONTAL_ALIGNMENT_LEFT, available, font_size, TextServer::JUSTIFICATION_KASHIDA | TextServer::JUSTIFICATION_CONSTRAIN_ELLIPSIS).x;

					Ref<Font> light_font = get_theme_font(SNAME("main"), EditorStringName(EditorFonts));
					int light_font_size = get_theme_font_size(SNAME("main_size"), EditorStringName(EditorFonts));
					Color light_font_color = get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor));

					// Can we fit the long version of the revertable count text?
					num_revertable_str = vformat(TTRN("(%d change)", "(%d changes)", revertable_properties.size()), revertable_properties.size());
					num_revertable_width = light_font->get_string_size(num_revertable_str, HORIZONTAL_ALIGNMENT_LEFT, -1.0f, light_font_size, TextServer::JUSTIFICATION_NONE).x;
					if (label_width + outer_margin + num_revertable_width > available) {
						// We'll have to use the short version.
						num_revertable_str = vformat("(%d)", revertable_properties.size());
						num_revertable_width = light_font->get_string_size(num_revertable_str, HORIZONTAL_ALIGNMENT_LEFT, -1.0f, light_font_size, TextServer::JUSTIFICATION_NONE).x;
					}

					float text_offset_y = light_font->get_ascent(light_font_size) + (header_height - light_font->get_height(light_font_size)) / 2;
					Point2 text_offset = Point2(margin_end, text_offset_y).round();
					if (!rtl) {
						text_offset.x = get_size().width - (text_offset.x + num_revertable_width);
					}
					draw_string(light_font, text_offset, num_revertable_str, HORIZONTAL_ALIGNMENT_LEFT, -1.0f, light_font_size, light_font_color, TextServer::JUSTIFICATION_NONE);
					margin_end += num_revertable_width + outer_margin;
					available -= num_revertable_width + outer_margin;
				}

				// - Label.
				float text_offset_y = font->get_ascent(font_size) + (header_height - font->get_height(font_size)) / 2;
				Point2 text_offset = Point2(margin_start, text_offset_y).round();
				if (rtl) {
					text_offset.x = margin_end;
				}
				HorizontalAlignment text_align = rtl ? HORIZONTAL_ALIGNMENT_RIGHT : HORIZONTAL_ALIGNMENT_LEFT;
				draw_string(font, text_offset, label, text_align, available, font_size, font_color, TextServer::JUSTIFICATION_KASHIDA | TextServer::JUSTIFICATION_CONSTRAIN_ELLIPSIS);
			}

			// Draw section indentation.
			if (section_indent_style.is_valid() && section_indent > 0) {
				Rect2 indent_rect = Rect2(Vector2(), Vector2(indent_depth * section_indent_size, get_size().height));
				if (rtl) {
					indent_rect.position.x = get_size().width - section_indent + section_indent_style->get_margin(SIDE_RIGHT);
				} else {
					indent_rect.position.x = section_indent_style->get_margin(SIDE_LEFT);
				}
				draw_style_box(section_indent_style, indent_rect);
			}
		} break;

		case NOTIFICATION_DRAG_BEGIN: {
			dropping_for_unfold = true;
		} break;

		case NOTIFICATION_DRAG_END: {
			dropping_for_unfold = false;
		} break;

		case NOTIFICATION_MOUSE_ENTER: {
			if (dropping_for_unfold) {
				dropping_unfold_timer->start();
			}
			queue_redraw();
		} break;

		case NOTIFICATION_MOUSE_EXIT: {
			if (dropping_for_unfold) {
				dropping_unfold_timer->stop();
			}
			queue_redraw();
		} break;
	}
}

Size2 EditorInspectorSection::get_minimum_size() const {
	Size2 ms;
	for (int i = 0; i < get_child_count(); i++) {
		Control *c = as_sortable_control(get_child(i));
		if (!c) {
			continue;
		}
		Size2 minsize = c->get_combined_minimum_size();
		ms = ms.max(minsize);
	}

	Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Tree"));
	int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Tree"));
	ms.height += font->get_height(font_size) + get_theme_constant(SNAME("v_separation"), SNAME("Tree"));
	ms.width += get_theme_constant(SNAME("inspector_margin"), EditorStringName(Editor));

	int section_indent_size = get_theme_constant(SNAME("indent_size"), SNAME("EditorInspectorSection"));
	if (indent_depth > 0 && section_indent_size > 0) {
		ms.width += indent_depth * section_indent_size;
	}
	Ref<StyleBoxFlat> section_indent_style = get_theme_stylebox(SNAME("indent_box"), SNAME("EditorInspectorSection"));
	if (indent_depth > 0 && section_indent_style.is_valid()) {
		ms.width += section_indent_style->get_margin(SIDE_LEFT) + section_indent_style->get_margin(SIDE_RIGHT);
	}

	return ms;
}

void EditorInspectorSection::setup(const String &p_section, const String &p_label, Object *p_object, const Color &p_bg_color, bool p_foldable, int p_indent_depth, int p_level) {
	section = p_section;
	label = p_label;
	object = p_object;
	bg_color = p_bg_color;
	foldable = p_foldable;
	indent_depth = p_indent_depth;
	level = p_level;

	if (!foldable && !vbox_added) {
		add_child(vbox);
		move_child(vbox, 0);
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

void EditorInspectorSection::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (!foldable) {
		return;
	}

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		if (object->editor_is_section_unfolded(section)) {
			int header_height = _get_header_height();

			if (mb->get_position().y >= header_height) {
				return;
			}
		}

		accept_event();

		bool should_unfold = !object->editor_is_section_unfolded(section);
		if (should_unfold) {
			unfold();
		} else {
			fold();
		}
	} else if (mb.is_valid() && !mb->is_pressed()) {
		queue_redraw();
	}
}

String EditorInspectorSection::get_section() const {
	return section;
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
	queue_redraw();
}

void EditorInspectorSection::fold() {
	if (!foldable) {
		return;
	}

	if (!vbox_added) {
		return;
	}

	object->editor_set_section_unfold(section, false);
	vbox->hide();
	queue_redraw();
}

void EditorInspectorSection::set_bg_color(const Color &p_bg_color) {
	bg_color = p_bg_color;
	queue_redraw();
}

bool EditorInspectorSection::has_revertable_properties() const {
	return !revertable_properties.is_empty();
}

void EditorInspectorSection::property_can_revert_changed(const String &p_path, bool p_can_revert) {
	bool had_revertable_properties = has_revertable_properties();
	if (p_can_revert) {
		revertable_properties.insert(p_path);
	} else {
		revertable_properties.erase(p_path);
	}
	if (has_revertable_properties() != had_revertable_properties) {
		queue_redraw();
	}
}

void EditorInspectorSection::_bind_methods() {
	ClassDB::bind_method(D_METHOD("setup", "section", "label", "object", "bg_color", "foldable", "indent_depth", "level"), &EditorInspectorSection::setup, DEFVAL(0), DEFVAL(1));
	ClassDB::bind_method(D_METHOD("get_vbox"), &EditorInspectorSection::get_vbox);
	ClassDB::bind_method(D_METHOD("unfold"), &EditorInspectorSection::unfold);
	ClassDB::bind_method(D_METHOD("fold"), &EditorInspectorSection::fold);
}

EditorInspectorSection::EditorInspectorSection() {
	vbox = memnew(VBoxContainer);

	dropping_unfold_timer = memnew(Timer);
	dropping_unfold_timer->set_wait_time(0.6);
	dropping_unfold_timer->set_one_shot(true);
	add_child(dropping_unfold_timer);
	dropping_unfold_timer->connect("timeout", callable_mp(this, &EditorInspectorSection::unfold));
}

EditorInspectorSection::~EditorInspectorSection() {
	if (!vbox_added) {
		memdelete(vbox);
	}
}

////////////////////////////////////////////////
////////////////////////////////////////////////

int EditorInspectorArray::_get_array_count() {
	if (mode == MODE_USE_MOVE_ARRAY_ELEMENT_FUNCTION) {
		List<PropertyInfo> object_property_list;
		object->get_property_list(&object_property_list);
		return _extract_properties_as_array(object_property_list).size();
	} else if (mode == MODE_USE_COUNT_PROPERTY) {
		bool valid;
		int count_val = object->get(count_property, &valid);
		ERR_FAIL_COND_V_MSG(!valid, 0, vformat("%s is not a valid property to be used as array count.", count_property));
		return count_val;
	}
	return 0;
}

void EditorInspectorArray::_add_button_pressed() {
	_move_element(-1, -1);
}

void EditorInspectorArray::_paginator_page_changed(int p_page) {
	emit_signal("page_change_request", p_page);
}

void EditorInspectorArray::_rmb_popup_id_pressed(int p_id) {
	switch (p_id) {
		case OPTION_MOVE_UP:
			if (popup_array_index_pressed > 0) {
				_move_element(popup_array_index_pressed, popup_array_index_pressed - 1);
			}
			break;
		case OPTION_MOVE_DOWN:
			if (popup_array_index_pressed < count - 1) {
				_move_element(popup_array_index_pressed, popup_array_index_pressed + 2);
			}
			break;
		case OPTION_NEW_BEFORE:
			_move_element(-1, popup_array_index_pressed);
			break;
		case OPTION_NEW_AFTER:
			_move_element(-1, popup_array_index_pressed + 1);
			break;
		case OPTION_REMOVE:
			_move_element(popup_array_index_pressed, -1);
			break;
		case OPTION_CLEAR_ARRAY:
			_clear_array();
			break;
		case OPTION_RESIZE_ARRAY:
			new_size_spin_box->set_value(count);
			resize_dialog->get_ok_button()->set_disabled(true);
			resize_dialog->popup_centered(Size2(250, 0) * EDSCALE);
			new_size_spin_box->get_line_edit()->grab_focus();
			new_size_spin_box->get_line_edit()->select_all();
			break;
		default:
			break;
	}
}

void EditorInspectorArray::_control_dropping_draw() {
	int drop_position = _drop_position();

	if (dropping && drop_position >= 0) {
		Vector2 from;
		Vector2 to;
		if (drop_position < elements_vbox->get_child_count()) {
			Transform2D xform = Object::cast_to<Control>(elements_vbox->get_child(drop_position))->get_transform();
			from = xform.xform(Vector2());
			to = xform.xform(Vector2(elements_vbox->get_size().x, 0));
		} else {
			Control *child = Object::cast_to<Control>(elements_vbox->get_child(drop_position - 1));
			Transform2D xform = child->get_transform();
			from = xform.xform(Vector2(0, child->get_size().y));
			to = xform.xform(Vector2(elements_vbox->get_size().x, child->get_size().y));
		}
		Color color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
		control_dropping->draw_line(from, to, color, 2);
	}
}

void EditorInspectorArray::_vbox_visibility_changed() {
	control_dropping->set_visible(vbox->is_visible_in_tree());
}

void EditorInspectorArray::_panel_draw(int p_index) {
	ERR_FAIL_INDEX(p_index, (int)array_elements.size());

	Ref<StyleBox> style = get_theme_stylebox(SNAME("Focus"), EditorStringName(EditorStyles));
	if (style.is_null()) {
		return;
	}
	if (array_elements[p_index].panel->has_focus()) {
		array_elements[p_index].panel->draw_style_box(style, Rect2(Vector2(), array_elements[p_index].panel->get_size()));
	}
}

void EditorInspectorArray::_panel_gui_input(Ref<InputEvent> p_event, int p_index) {
	ERR_FAIL_INDEX(p_index, (int)array_elements.size());

	if (read_only) {
		return;
	}

	Ref<InputEventKey> key_ref = p_event;
	if (key_ref.is_valid()) {
		const InputEventKey &key = **key_ref;

		if (array_elements[p_index].panel->has_focus() && key.is_pressed() && key.get_keycode() == Key::KEY_DELETE) {
			_move_element(begin_array_index + p_index, -1);
			array_elements[p_index].panel->accept_event();
		}
	}

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		if (movable && mb->get_button_index() == MouseButton::RIGHT) {
			array_elements[p_index].panel->accept_event();
			popup_array_index_pressed = begin_array_index + p_index;
			rmb_popup->set_item_disabled(OPTION_MOVE_UP, popup_array_index_pressed == 0);
			rmb_popup->set_item_disabled(OPTION_MOVE_DOWN, popup_array_index_pressed == count - 1);
			rmb_popup->set_position(get_screen_position() + mb->get_position());
			rmb_popup->reset_size();
			rmb_popup->popup();
		}
	}
}

void EditorInspectorArray::_move_element(int p_element_index, int p_to_pos) {
	String action_name;
	if (p_element_index < 0) {
		action_name = vformat(TTR("Add element to property array with prefix %s."), array_element_prefix);
	} else if (p_to_pos < 0) {
		action_name = vformat(TTR("Remove element %d from property array with prefix %s."), p_element_index, array_element_prefix);
	} else {
		action_name = vformat(TTR("Move element %d to position %d in property array with prefix %s."), p_element_index, p_to_pos, array_element_prefix);
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(action_name);
	if (mode == MODE_USE_MOVE_ARRAY_ELEMENT_FUNCTION) {
		// Call the function.
		Callable move_function = EditorNode::get_editor_data().get_move_array_element_function(object->get_class_name());
		if (move_function.is_valid()) {
			move_function.call(undo_redo, object, array_element_prefix, p_element_index, p_to_pos);
		} else {
			WARN_PRINT(vformat("Could not find a function to move arrays elements for class %s. Register a move element function using EditorData::add_move_array_element_function", object->get_class_name()));
		}
	} else if (mode == MODE_USE_COUNT_PROPERTY) {
		ERR_FAIL_COND(p_to_pos < -1 || p_to_pos > count);

		if (!swap_method.is_empty()) {
			ERR_FAIL_COND(!object->has_method(swap_method));

			// Swap method was provided, use it.
			if (p_element_index < 0) {
				// Add an element at position
				undo_redo->add_do_property(object, count_property, count + 1);
				if (p_to_pos >= 0) {
					for (int i = count; i > p_to_pos; i--) {
						undo_redo->add_do_method(object, swap_method, i, i - 1);
					}
					for (int i = p_to_pos; i < count; i++) {
						undo_redo->add_undo_method(object, swap_method, i, i + 1);
					}
				}
				undo_redo->add_undo_property(object, count_property, count);

			} else if (p_to_pos < 0) {
				if (count > 0) {
					// Remove element at position
					undo_redo->add_undo_property(object, count_property, count);

					List<PropertyInfo> object_property_list;
					object->get_property_list(&object_property_list);

					for (int i = p_element_index; i < count - 1; i++) {
						undo_redo->add_do_method(object, swap_method, i, i + 1);
					}

					for (int i = count; i > p_element_index; i--) {
						undo_redo->add_undo_method(object, swap_method, i, i - 1);
					}

					String erase_prefix = String(array_element_prefix) + itos(p_element_index);

					for (const PropertyInfo &E : object_property_list) {
						if (E.name.begins_with(erase_prefix)) {
							undo_redo->add_undo_property(object, E.name, object->get(E.name));
						}
					}

					undo_redo->add_do_property(object, count_property, count - 1);
				}
			} else {
				if (p_to_pos > p_element_index) {
					p_to_pos--;
				}

				if (p_to_pos < p_element_index) {
					for (int i = p_element_index; i > p_to_pos; i--) {
						undo_redo->add_do_method(object, swap_method, i, i - 1);
					}
					for (int i = p_to_pos; i < p_element_index; i++) {
						undo_redo->add_undo_method(object, swap_method, i, i + 1);
					}
				} else if (p_to_pos > p_element_index) {
					for (int i = p_element_index; i < p_to_pos; i++) {
						undo_redo->add_do_method(object, swap_method, i, i + 1);
					}

					for (int i = p_to_pos; i > p_element_index; i--) {
						undo_redo->add_undo_method(object, swap_method, i, i - 1);
					}
				}
			}
		} else {
			// Use standard properties.
			List<PropertyInfo> object_property_list;
			object->get_property_list(&object_property_list);

			Array properties_as_array = _extract_properties_as_array(object_property_list);
			properties_as_array.resize(count);

			// For undoing things
			undo_redo->add_undo_property(object, count_property, properties_as_array.size());
			for (int i = 0; i < (int)properties_as_array.size(); i++) {
				Dictionary d = Dictionary(properties_as_array[i]);
				Array keys = d.keys();
				for (int j = 0; j < keys.size(); j++) {
					String key = keys[j];
					undo_redo->add_undo_property(object, vformat(key, i), d[key]);
				}
			}

			if (p_element_index < 0) {
				// Add an element.
				properties_as_array.insert(p_to_pos < 0 ? properties_as_array.size() : p_to_pos, Dictionary());
			} else if (p_to_pos < 0) {
				// Delete the element.
				properties_as_array.remove_at(p_element_index);
			} else {
				// Move the element.
				properties_as_array.insert(p_to_pos, properties_as_array[p_element_index].duplicate());
				properties_as_array.remove_at(p_to_pos < p_element_index ? p_element_index + 1 : p_element_index);
			}

			// Change the array size then set the properties.
			undo_redo->add_do_property(object, count_property, properties_as_array.size());
			for (int i = 0; i < (int)properties_as_array.size(); i++) {
				Dictionary d = properties_as_array[i];
				Array keys = d.keys();
				for (int j = 0; j < keys.size(); j++) {
					String key = keys[j];
					undo_redo->add_do_property(object, vformat(key, i), d[key]);
				}
			}
		}
	}
	undo_redo->commit_action();

	// Handle page change and update counts.
	if (p_element_index < 0) {
		int added_index = p_to_pos < 0 ? count : p_to_pos;
		emit_signal(SNAME("page_change_request"), added_index / page_length);
		count += 1;
	} else if (p_to_pos < 0) {
		count -= 1;
		if (page == max_page && (MAX(0, count - 1) / page_length != max_page)) {
			emit_signal(SNAME("page_change_request"), max_page - 1);
		}
	} else if (p_to_pos == begin_array_index - 1) {
		emit_signal(SNAME("page_change_request"), page - 1);
	} else if (p_to_pos > end_array_index) {
		emit_signal(SNAME("page_change_request"), page + 1);
	}
	begin_array_index = page * page_length;
	end_array_index = MIN(count, (page + 1) * page_length);
	max_page = MAX(0, count - 1) / page_length;
}

void EditorInspectorArray::_clear_array() {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(vformat(TTR("Clear Property Array with Prefix %s"), array_element_prefix));
	if (mode == MODE_USE_MOVE_ARRAY_ELEMENT_FUNCTION) {
		for (int i = count - 1; i >= 0; i--) {
			// Call the function.
			Callable move_function = EditorNode::get_editor_data().get_move_array_element_function(object->get_class_name());
			if (move_function.is_valid()) {
				move_function.call(undo_redo, object, array_element_prefix, i, -1);
			} else {
				WARN_PRINT(vformat("Could not find a function to move arrays elements for class %s. Register a move element function using EditorData::add_move_array_element_function", object->get_class_name()));
			}
		}
	} else if (mode == MODE_USE_COUNT_PROPERTY) {
		List<PropertyInfo> object_property_list;
		object->get_property_list(&object_property_list);

		Array properties_as_array = _extract_properties_as_array(object_property_list);
		properties_as_array.resize(count);

		// For undoing things
		undo_redo->add_undo_property(object, count_property, count);
		for (int i = 0; i < (int)properties_as_array.size(); i++) {
			Dictionary d = Dictionary(properties_as_array[i]);
			Array keys = d.keys();
			for (int j = 0; j < keys.size(); j++) {
				String key = keys[j];
				undo_redo->add_undo_property(object, vformat(key, i), d[key]);
			}
		}

		// Change the array size then set the properties.
		undo_redo->add_do_property(object, count_property, 0);
	}
	undo_redo->commit_action();

	// Handle page change and update counts.
	emit_signal(SNAME("page_change_request"), 0);
	count = 0;
	begin_array_index = 0;
	end_array_index = 0;
	max_page = 0;
}

void EditorInspectorArray::_resize_array(int p_size) {
	ERR_FAIL_COND(p_size < 0);
	if (p_size == count) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(vformat(TTR("Resize Property Array with Prefix %s"), array_element_prefix));
	if (p_size > count) {
		if (mode == MODE_USE_MOVE_ARRAY_ELEMENT_FUNCTION) {
			for (int i = count; i < p_size; i++) {
				// Call the function.
				Callable move_function = EditorNode::get_editor_data().get_move_array_element_function(object->get_class_name());
				if (move_function.is_valid()) {
					move_function.call(undo_redo, object, array_element_prefix, -1, -1);
				} else {
					WARN_PRINT(vformat("Could not find a function to move arrays elements for class %s. Register a move element function using EditorData::add_move_array_element_function", object->get_class_name()));
				}
			}
		} else if (mode == MODE_USE_COUNT_PROPERTY) {
			undo_redo->add_undo_property(object, count_property, count);
			undo_redo->add_do_property(object, count_property, p_size);
		}
	} else {
		if (mode == MODE_USE_MOVE_ARRAY_ELEMENT_FUNCTION) {
			for (int i = count - 1; i > p_size - 1; i--) {
				// Call the function.
				Callable move_function = EditorNode::get_editor_data().get_move_array_element_function(object->get_class_name());
				if (move_function.is_valid()) {
					move_function.call(undo_redo, object, array_element_prefix, i, -1);
				} else {
					WARN_PRINT(vformat("Could not find a function to move arrays elements for class %s. Register a move element function using EditorData::add_move_array_element_function", object->get_class_name()));
				}
			}
		} else if (mode == MODE_USE_COUNT_PROPERTY) {
			List<PropertyInfo> object_property_list;
			object->get_property_list(&object_property_list);

			Array properties_as_array = _extract_properties_as_array(object_property_list);
			properties_as_array.resize(count);

			// For undoing things
			undo_redo->add_undo_property(object, count_property, count);
			for (int i = count - 1; i > p_size - 1; i--) {
				Dictionary d = Dictionary(properties_as_array[i]);
				Array keys = d.keys();
				for (int j = 0; j < keys.size(); j++) {
					String key = keys[j];
					undo_redo->add_undo_property(object, vformat(key, i), d[key]);
				}
			}

			// Change the array size then set the properties.
			undo_redo->add_do_property(object, count_property, p_size);
		}
	}
	undo_redo->commit_action();

	// Handle page change and update counts.
	emit_signal(SNAME("page_change_request"), 0);
	/*
	count = 0;
	begin_array_index = 0;
	end_array_index = 0;
	max_page = 0;
	*/
}

Array EditorInspectorArray::_extract_properties_as_array(const List<PropertyInfo> &p_list) {
	Array output;

	for (const PropertyInfo &pi : p_list) {
		if (!(pi.usage & PROPERTY_USAGE_EDITOR)) {
			continue;
		}

		if (pi.name.begins_with(array_element_prefix)) {
			String str = pi.name.trim_prefix(array_element_prefix);

			int to_char_index = 0;
			while (to_char_index < str.length()) {
				if (!is_digit(str[to_char_index])) {
					break;
				}
				to_char_index++;
			}
			if (to_char_index > 0) {
				int array_index = str.left(to_char_index).to_int();
				Error error = OK;
				if (array_index >= output.size()) {
					error = output.resize(array_index + 1);
				}
				if (error == OK) {
					String format_string = String(array_element_prefix) + "%d" + str.substr(to_char_index);
					Dictionary dict = output[array_index];
					dict[format_string] = object->get(pi.name);
					output[array_index] = dict;
				} else {
					WARN_PRINT(vformat("Array element %s has an index too high. Array allocation failed.", pi.name));
				}
			}
		}
	}
	return output;
}

int EditorInspectorArray::_drop_position() const {
	for (int i = 0; i < (int)array_elements.size(); i++) {
		const ArrayElement &ae = array_elements[i];

		Size2 size = ae.panel->get_size();
		Vector2 mp = ae.panel->get_local_mouse_position();

		if (Rect2(Vector2(), size).has_point(mp)) {
			if (mp.y < size.y / 2) {
				return i;
			} else {
				return i + 1;
			}
		}
	}
	return -1;
}

void EditorInspectorArray::_resize_dialog_confirmed() {
	if (int(new_size_spin_box->get_value()) == count) {
		return;
	}

	resize_dialog->hide();
	_resize_array(int(new_size_spin_box->get_value()));
}

void EditorInspectorArray::_new_size_spin_box_value_changed(float p_value) {
	resize_dialog->get_ok_button()->set_disabled(int(p_value) == count);
}

void EditorInspectorArray::_new_size_spin_box_text_submitted(const String &p_text) {
	_resize_dialog_confirmed();
}

void EditorInspectorArray::_setup() {
	// Setup counts.
	count = _get_array_count();
	begin_array_index = page * page_length;
	end_array_index = MIN(count, (page + 1) * page_length);
	max_page = MAX(0, count - 1) / page_length;
	array_elements.resize(MAX(0, end_array_index - begin_array_index));
	if (page < 0 || page > max_page) {
		WARN_PRINT(vformat("Invalid page number %d", page));
		page = CLAMP(page, 0, max_page);
	}

	Ref<Font> numbers_font;
	int numbers_min_w = 0;

	if (numbered) {
		numbers_font = get_theme_font(SNAME("bold"), EditorStringName(EditorFonts));
		int digits_found = count;
		String test;
		while (digits_found) {
			test += "8";
			digits_found /= 10;
		}
		numbers_min_w = numbers_font->get_string_size(test).width;
	}

	for (int i = 0; i < (int)array_elements.size(); i++) {
		ArrayElement &ae = array_elements[i];

		// Panel and its hbox.
		ae.panel = memnew(PanelContainer);
		ae.panel->set_focus_mode(FOCUS_ALL);
		ae.panel->set_mouse_filter(MOUSE_FILTER_PASS);
		SET_DRAG_FORWARDING_GCD(ae.panel, EditorInspectorArray);

		int element_position = begin_array_index + i;
		ae.panel->set_meta("index", element_position);
		ae.panel->set_tooltip_text(vformat(TTR("Element %d: %s%d*"), element_position, array_element_prefix, element_position));
		ae.panel->connect(SceneStringName(focus_entered), callable_mp((CanvasItem *)ae.panel, &PanelContainer::queue_redraw));
		ae.panel->connect(SceneStringName(focus_exited), callable_mp((CanvasItem *)ae.panel, &PanelContainer::queue_redraw));
		ae.panel->connect(SceneStringName(draw), callable_mp(this, &EditorInspectorArray::_panel_draw).bind(i));
		ae.panel->connect(SceneStringName(gui_input), callable_mp(this, &EditorInspectorArray::_panel_gui_input).bind(i));
		ae.panel->add_theme_style_override(SceneStringName(panel), i % 2 ? odd_style : even_style);
		elements_vbox->add_child(ae.panel);

		ae.margin = memnew(MarginContainer);
		ae.margin->set_mouse_filter(MOUSE_FILTER_PASS);
		if (is_inside_tree()) {
			Size2 min_size = get_theme_stylebox(SNAME("Focus"), EditorStringName(EditorStyles))->get_minimum_size();
			ae.margin->begin_bulk_theme_override();
			ae.margin->add_theme_constant_override("margin_left", min_size.x / 2);
			ae.margin->add_theme_constant_override("margin_top", min_size.y / 2);
			ae.margin->add_theme_constant_override("margin_right", min_size.x / 2);
			ae.margin->add_theme_constant_override("margin_bottom", min_size.y / 2);
			ae.margin->end_bulk_theme_override();
		}
		ae.panel->add_child(ae.margin);

		ae.hbox = memnew(HBoxContainer);
		ae.hbox->set_h_size_flags(SIZE_EXPAND_FILL);
		ae.hbox->set_v_size_flags(SIZE_EXPAND_FILL);
		ae.margin->add_child(ae.hbox);

		// Move button.
		if (movable) {
			VBoxContainer *move_vbox = memnew(VBoxContainer);
			move_vbox->set_v_size_flags(SIZE_EXPAND_FILL);
			move_vbox->set_alignment(BoxContainer::ALIGNMENT_CENTER);
			ae.hbox->add_child(move_vbox);

			if (element_position > 0) {
				ae.move_up = memnew(Button);
				ae.move_up->set_button_icon(get_editor_theme_icon(SNAME("MoveUp")));
				ae.move_up->connect(SceneStringName(pressed), callable_mp(this, &EditorInspectorArray::_move_element).bind(element_position, element_position - 1));
				move_vbox->add_child(ae.move_up);
			}

			ae.move_texture_rect = memnew(TextureRect);
			ae.move_texture_rect->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
			ae.move_texture_rect->set_default_cursor_shape(Control::CURSOR_MOVE);

			if (is_inside_tree()) {
				ae.move_texture_rect->set_texture(get_editor_theme_icon(SNAME("TripleBar")));
			}
			move_vbox->add_child(ae.move_texture_rect);

			if (element_position < count - 1) {
				ae.move_down = memnew(Button);
				ae.move_down->set_button_icon(get_editor_theme_icon(SNAME("MoveDown")));
				ae.move_down->connect(SceneStringName(pressed), callable_mp(this, &EditorInspectorArray::_move_element).bind(element_position, element_position + 2));
				move_vbox->add_child(ae.move_down);
			}
		}

		if (numbered) {
			ae.number = memnew(Label);
			ae.number->add_theme_font_override(SceneStringName(font), numbers_font);
			ae.number->set_custom_minimum_size(Size2(numbers_min_w, 0));
			ae.number->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
			ae.number->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
			ae.number->set_text(itos(element_position));
			ae.hbox->add_child(ae.number);
		}

		// Right vbox.
		ae.vbox = memnew(VBoxContainer);
		ae.vbox->set_h_size_flags(SIZE_EXPAND_FILL);
		ae.vbox->set_v_size_flags(SIZE_EXPAND_FILL);
		ae.hbox->add_child(ae.vbox);

		ae.erase = memnew(Button);
		ae.erase->set_button_icon(get_editor_theme_icon(SNAME("Remove")));
		ae.erase->set_v_size_flags(SIZE_SHRINK_CENTER);
		ae.erase->connect(SceneStringName(pressed), callable_mp(this, &EditorInspectorArray::_remove_item).bind(element_position));
		ae.hbox->add_child(ae.erase);
	}

	// Hide/show the add button.
	add_button->set_visible(page == max_page);

	// Add paginator if there's more than 1 page.
	if (max_page > 0) {
		EditorPaginator *paginator = memnew(EditorPaginator);
		paginator->update(page, max_page);
		paginator->connect("page_changed", callable_mp(this, &EditorInspectorArray::_paginator_page_changed));
		vbox->add_child(paginator);
	}
}

void EditorInspectorArray::_remove_item(int p_index) {
	_move_element(p_index, -1);
}

Variant EditorInspectorArray::get_drag_data_fw(const Point2 &p_point, Control *p_from) {
	if (!movable) {
		return Variant();
	}
	int index = p_from->get_meta("index");
	Dictionary dict;
	dict["type"] = "property_array_element";
	dict["property_array_prefix"] = array_element_prefix;
	dict["index"] = index;

	return dict;
}

void EditorInspectorArray::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	Dictionary dict = p_data;

	int to_drop = dict["index"];
	int drop_position = _drop_position();
	if (drop_position < 0) {
		return;
	}
	_move_element(to_drop, begin_array_index + drop_position);
}

bool EditorInspectorArray::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	if (!movable || read_only) {
		return false;
	}
	// First, update drawing.
	control_dropping->queue_redraw();

	if (p_data.get_type() != Variant::DICTIONARY) {
		return false;
	}
	Dictionary dict = p_data;
	int drop_position = _drop_position();
	if (!dict.has("type") || dict["type"] != "property_array_element" || String(dict["property_array_prefix"]) != array_element_prefix || drop_position < 0) {
		return false;
	}

	// Check in dropping at the given index does indeed move the item.
	int moved_array_index = (int)dict["index"];
	int drop_array_index = begin_array_index + drop_position;

	return drop_array_index != moved_array_index && drop_array_index - 1 != moved_array_index;
}

void EditorInspectorArray::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			Color color = get_theme_color(SNAME("dark_color_1"), EditorStringName(Editor));
			odd_style->set_bg_color(color.darkened(-0.08));
			even_style->set_bg_color(color.darkened(0.08));

			for (ArrayElement &ae : array_elements) {
				if (ae.move_texture_rect) {
					ae.move_texture_rect->set_texture(get_editor_theme_icon(SNAME("TripleBar")));
				}
				if (ae.move_up) {
					ae.move_up->set_button_icon(get_editor_theme_icon(SNAME("MoveUp")));
				}
				if (ae.move_down) {
					ae.move_down->set_button_icon(get_editor_theme_icon(SNAME("MoveDown")));
				}
				Size2 min_size = get_theme_stylebox(SNAME("Focus"), EditorStringName(EditorStyles))->get_minimum_size();
				ae.margin->begin_bulk_theme_override();
				ae.margin->add_theme_constant_override("margin_left", min_size.x / 2);
				ae.margin->add_theme_constant_override("margin_top", min_size.y / 2);
				ae.margin->add_theme_constant_override("margin_right", min_size.x / 2);
				ae.margin->add_theme_constant_override("margin_bottom", min_size.y / 2);
				ae.margin->end_bulk_theme_override();

				if (ae.erase) {
					ae.erase->set_button_icon(get_editor_theme_icon(SNAME("Remove")));
				}
			}

			add_button->set_button_icon(get_editor_theme_icon(SNAME("Add")));
			update_minimum_size();
		} break;

		case NOTIFICATION_DRAG_BEGIN: {
			Dictionary dict = get_viewport()->gui_get_drag_data();
			if (dict.has("type") && dict["type"] == "property_array_element" && String(dict["property_array_prefix"]) == array_element_prefix) {
				dropping = true;
				control_dropping->queue_redraw();
			}
		} break;

		case NOTIFICATION_DRAG_END: {
			if (dropping) {
				dropping = false;
				control_dropping->queue_redraw();
			}
		} break;
	}
}

void EditorInspectorArray::_bind_methods() {
	ADD_SIGNAL(MethodInfo("page_change_request"));
}

void EditorInspectorArray::setup_with_move_element_function(Object *p_object, const String &p_label, const StringName &p_array_element_prefix, int p_page, const Color &p_bg_color, bool p_foldable, bool p_movable, bool p_numbered, int p_page_length, const String &p_add_item_text) {
	count_property = "";
	mode = MODE_USE_MOVE_ARRAY_ELEMENT_FUNCTION;
	array_element_prefix = p_array_element_prefix;
	page = p_page;
	movable = p_movable;
	page_length = p_page_length;
	numbered = p_numbered;

	EditorInspectorSection::setup(String(p_array_element_prefix) + "_array", p_label, p_object, p_bg_color, p_foldable, 0);

	_setup();
}

void EditorInspectorArray::setup_with_count_property(Object *p_object, const String &p_label, const StringName &p_count_property, const StringName &p_array_element_prefix, int p_page, const Color &p_bg_color, bool p_foldable, bool p_movable, bool p_numbered, int p_page_length, const String &p_add_item_text, const String &p_swap_method) {
	count_property = p_count_property;
	mode = MODE_USE_COUNT_PROPERTY;
	array_element_prefix = p_array_element_prefix;
	page = p_page;
	movable = p_movable;
	page_length = p_page_length;
	numbered = p_numbered;
	swap_method = p_swap_method;

	add_button->set_text(p_add_item_text);
	EditorInspectorSection::setup(String(count_property) + "_array", p_label, p_object, p_bg_color, p_foldable, 0);

	_setup();
}

VBoxContainer *EditorInspectorArray::get_vbox(int p_index) {
	if (p_index >= begin_array_index && p_index < end_array_index) {
		return array_elements[p_index - begin_array_index].vbox;
	} else if (p_index < 0) {
		return vbox;
	} else {
		return nullptr;
	}
}

EditorInspectorArray::EditorInspectorArray(bool p_read_only) {
	read_only = p_read_only;

	set_mouse_filter(Control::MOUSE_FILTER_STOP);

	odd_style.instantiate();
	even_style.instantiate();

	rmb_popup = memnew(PopupMenu);
	rmb_popup->add_item(TTR("Move Up"), OPTION_MOVE_UP);
	rmb_popup->add_item(TTR("Move Down"), OPTION_MOVE_DOWN);
	rmb_popup->add_separator();
	rmb_popup->add_item(TTR("Insert New Before"), OPTION_NEW_BEFORE);
	rmb_popup->add_item(TTR("Insert New After"), OPTION_NEW_AFTER);
	rmb_popup->add_separator();
	rmb_popup->add_item(TTR("Remove"), OPTION_REMOVE);
	rmb_popup->add_separator();
	rmb_popup->add_item(TTR("Clear Array"), OPTION_CLEAR_ARRAY);
	rmb_popup->add_item(TTR("Resize Array..."), OPTION_RESIZE_ARRAY);
	rmb_popup->connect(SceneStringName(id_pressed), callable_mp(this, &EditorInspectorArray::_rmb_popup_id_pressed));
	add_child(rmb_popup);

	elements_vbox = memnew(VBoxContainer);
	elements_vbox->add_theme_constant_override("separation", 0);
	vbox->add_child(elements_vbox);

	add_button = EditorInspector::create_inspector_action_button(TTR("Add Element"));
	add_button->connect(SceneStringName(pressed), callable_mp(this, &EditorInspectorArray::_add_button_pressed));
	add_button->set_disabled(read_only);
	vbox->add_child(add_button);

	control_dropping = memnew(Control);
	control_dropping->connect(SceneStringName(draw), callable_mp(this, &EditorInspectorArray::_control_dropping_draw));
	control_dropping->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	add_child(control_dropping);

	resize_dialog = memnew(AcceptDialog);
	resize_dialog->set_title(TTRC("Resize Array"));
	resize_dialog->add_cancel_button();
	resize_dialog->connect(SceneStringName(confirmed), callable_mp(this, &EditorInspectorArray::_resize_dialog_confirmed));
	add_child(resize_dialog);

	VBoxContainer *resize_dialog_vbox = memnew(VBoxContainer);
	resize_dialog->add_child(resize_dialog_vbox);

	new_size_spin_box = memnew(SpinBox);
	new_size_spin_box->set_max(16384);
	new_size_spin_box->connect(SceneStringName(value_changed), callable_mp(this, &EditorInspectorArray::_new_size_spin_box_value_changed));
	new_size_spin_box->get_line_edit()->connect(SceneStringName(text_submitted), callable_mp(this, &EditorInspectorArray::_new_size_spin_box_text_submitted));
	new_size_spin_box->set_editable(!read_only);
	resize_dialog_vbox->add_margin_child(TTRC("New Size:"), new_size_spin_box);

	vbox->connect(SceneStringName(visibility_changed), callable_mp(this, &EditorInspectorArray::_vbox_visibility_changed));
}

////////////////////////////////////////////////
////////////////////////////////////////////////

void EditorPaginator::_first_page_button_pressed() {
	emit_signal("page_changed", 0);
}

void EditorPaginator::_prev_page_button_pressed() {
	emit_signal("page_changed", MAX(0, page - 1));
}

void EditorPaginator::_page_line_edit_text_submitted(const String &p_text) {
	if (p_text.is_valid_int()) {
		int new_page = p_text.to_int() - 1;
		new_page = MIN(MAX(0, new_page), max_page);
		page_line_edit->set_text(Variant(new_page));
		emit_signal("page_changed", new_page);
	} else {
		page_line_edit->set_text(Variant(page));
	}
}

void EditorPaginator::_next_page_button_pressed() {
	emit_signal("page_changed", MIN(max_page, page + 1));
}

void EditorPaginator::_last_page_button_pressed() {
	emit_signal("page_changed", max_page);
}

void EditorPaginator::update(int p_page, int p_max_page) {
	page = p_page;
	max_page = p_max_page;

	// Update buttons.
	first_page_button->set_disabled(page == 0);
	prev_page_button->set_disabled(page == 0);
	next_page_button->set_disabled(page == max_page);
	last_page_button->set_disabled(page == max_page);

	// Update page number and page count.
	page_line_edit->set_text(vformat("%d", page + 1));
	page_count_label->set_text(vformat("/ %d", max_page + 1));
}

void EditorPaginator::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			first_page_button->set_button_icon(get_editor_theme_icon(SNAME("PageFirst")));
			prev_page_button->set_button_icon(get_editor_theme_icon(SNAME("PagePrevious")));
			next_page_button->set_button_icon(get_editor_theme_icon(SNAME("PageNext")));
			last_page_button->set_button_icon(get_editor_theme_icon(SNAME("PageLast")));
		} break;
	}
}

void EditorPaginator::_bind_methods() {
	ADD_SIGNAL(MethodInfo("page_changed", PropertyInfo(Variant::INT, "page")));
}

EditorPaginator::EditorPaginator() {
	set_h_size_flags(SIZE_EXPAND_FILL);
	set_alignment(ALIGNMENT_CENTER);

	first_page_button = memnew(Button);
	first_page_button->set_flat(true);
	first_page_button->connect(SceneStringName(pressed), callable_mp(this, &EditorPaginator::_first_page_button_pressed));
	add_child(first_page_button);

	prev_page_button = memnew(Button);
	prev_page_button->set_flat(true);
	prev_page_button->connect(SceneStringName(pressed), callable_mp(this, &EditorPaginator::_prev_page_button_pressed));
	add_child(prev_page_button);

	page_line_edit = memnew(LineEdit);
	page_line_edit->connect(SceneStringName(text_submitted), callable_mp(this, &EditorPaginator::_page_line_edit_text_submitted));
	page_line_edit->add_theme_constant_override("minimum_character_width", 2);
	add_child(page_line_edit);

	page_count_label = memnew(Label);
	add_child(page_count_label);

	next_page_button = memnew(Button);
	next_page_button->set_flat(true);
	next_page_button->connect(SceneStringName(pressed), callable_mp(this, &EditorPaginator::_next_page_button_pressed));
	add_child(next_page_button);

	last_page_button = memnew(Button);
	last_page_button->set_flat(true);
	last_page_button->connect(SceneStringName(pressed), callable_mp(this, &EditorPaginator::_last_page_button_pressed));
	add_child(last_page_button);
}

////////////////////////////////////////////////
////////////////////////////////////////////////

Ref<EditorInspectorPlugin> EditorInspector::inspector_plugins[MAX_PLUGINS];
int EditorInspector::inspector_plugin_count = 0;

EditorProperty *EditorInspector::instantiate_property_editor(Object *p_object, const Variant::Type p_type, const String &p_path, PropertyHint p_hint, const String &p_hint_text, const uint32_t p_usage, const bool p_wide) {
	for (int i = inspector_plugin_count - 1; i >= 0; i--) {
		if (!inspector_plugins[i]->can_handle(p_object)) {
			continue;
		}

		inspector_plugins[i]->parse_property(p_object, p_type, p_path, p_hint, p_hint_text, p_usage, p_wide);
		if (inspector_plugins[i]->added_editors.size()) {
			for (List<EditorInspectorPlugin::AddedEditor>::Element *E = inspector_plugins[i]->added_editors.front()->next(); E; E = E->next()) { //only keep first one
				memdelete(E->get().property_editor);
			}

			EditorProperty *prop = Object::cast_to<EditorProperty>(inspector_plugins[i]->added_editors.front()->get().property_editor);
			if (prop) {
				inspector_plugins[i]->added_editors.clear();
				return prop;
			} else {
				memdelete(inspector_plugins[i]->added_editors.front()->get().property_editor);
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

Button *EditorInspector::create_inspector_action_button(const String &p_text) {
	Button *button = memnew(Button);
	button->set_text(p_text);
	button->set_theme_type_variation(SNAME("InspectorActionButton"));
	button->set_h_size_flags(SIZE_SHRINK_CENTER);
	return button;
}

bool EditorInspector::is_main_editor_inspector() const {
	return InspectorDock::get_singleton() && InspectorDock::get_inspector_singleton() == this;
}

String EditorInspector::get_selected_path() const {
	return property_selected;
}

void EditorInspector::_parse_added_editors(VBoxContainer *current_vbox, EditorInspectorSection *p_section, Ref<EditorInspectorPlugin> ped) {
	for (const EditorInspectorPlugin::AddedEditor &F : ped->added_editors) {
		EditorProperty *ep = Object::cast_to<EditorProperty>(F.property_editor);

		if (ep && !F.properties.is_empty() && current_favorites.has(F.properties[0])) {
			ep->favorited = true;
			favorites_vbox->add_child(F.property_editor);
		} else {
			current_vbox->add_child(F.property_editor);
		}

		if (ep) {
			ep->object = object;
			ep->connect("property_changed", callable_mp(this, &EditorInspector::_property_changed).bind(false));
			ep->connect("property_keyed", callable_mp(this, &EditorInspector::_property_keyed));
			ep->connect("property_deleted", callable_mp(this, &EditorInspector::_property_deleted), CONNECT_DEFERRED);
			ep->connect("property_keyed_with_value", callable_mp(this, &EditorInspector::_property_keyed_with_value));
			ep->connect("property_checked", callable_mp(this, &EditorInspector::_property_checked));
			ep->connect("property_favorited", callable_mp(this, &EditorInspector::_set_property_favorited), CONNECT_DEFERRED);
			ep->connect("property_pinned", callable_mp(this, &EditorInspector::_property_pinned));
			ep->connect("selected", callable_mp(this, &EditorInspector::_property_selected));
			ep->connect("multiple_properties_changed", callable_mp(this, &EditorInspector::_multiple_properties_changed));
			ep->connect("resource_selected", callable_mp(this, &EditorInspector::_resource_selected), CONNECT_DEFERRED);
			ep->connect("object_id_selected", callable_mp(this, &EditorInspector::_object_id_selected), CONNECT_DEFERRED);

			if (F.properties.size()) {
				if (F.properties.size() == 1) {
					//since it's one, associate:
					ep->property = F.properties[0];
					ep->property_path = property_prefix + F.properties[0];
					ep->property_usage = 0;
				}

				if (!F.label.is_empty()) {
					ep->set_label(F.label);
				}

				for (int i = 0; i < F.properties.size(); i++) {
					String prop = F.properties[i];

					if (!editor_property_map.has(prop)) {
						editor_property_map[prop] = List<EditorProperty *>();
					}
					editor_property_map[prop].push_back(ep);
				}
			}

			Node *section_search = p_section;
			while (section_search) {
				EditorInspectorSection *section = Object::cast_to<EditorInspectorSection>(section_search);
				if (section) {
					ep->connect("property_can_revert_changed", callable_mp(section, &EditorInspectorSection::property_can_revert_changed));
				}
				section_search = section_search->get_parent();
				if (Object::cast_to<EditorInspector>(section_search)) {
					// Skip sub-resource inspectors.
					break;
				}
			}

			ep->set_read_only(read_only);
			ep->update_property();
			ep->_update_flags();
			ep->update_editor_property_status();
			ep->set_deletable(deletable_properties);
			ep->update_cache();
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
	// Store currently selected and focused elements to restore after the update.
	// TODO: Can be useful to store more context for the focusable, such as the caret position in LineEdit.
	StringName current_selected = property_selected;
	int current_focusable = -1;

	// Temporarily disable focus following on the root inspector to avoid jumping while the inspector is updating.
	get_root_inspector()->set_follow_focus(false);

	if (property_focusable != -1) {
		// Check that focusable is actually focusable.
		bool restore_focus = false;
		Control *focused = get_viewport() ? get_viewport()->gui_get_focus_owner() : nullptr;
		if (focused) {
			Node *parent = focused->get_parent();
			while (parent) {
				EditorInspector *inspector = Object::cast_to<EditorInspector>(parent);
				if (inspector) {
					restore_focus = inspector == this; // May be owned by another inspector.
					break; // Exit after the first inspector is found, since there may be nested ones.
				}
				parent = parent->get_parent();
			}
		}

		if (restore_focus) {
			current_focusable = property_focusable;
		}
	}

	// Only hide plugins if we are not editing any object.
	// This should be handled outside of the update_tree call anyway (see EditorInspector::edit), but might as well keep it safe.
	_clear(!object);

	if (!object) {
		get_root_inspector()->set_follow_focus(true);
		return;
	}

	List<Ref<EditorInspectorPlugin>> valid_plugins;

	for (int i = inspector_plugin_count - 1; i >= 0; i--) { //start by last, so lastly added can override newly added
		if (!inspector_plugins[i]->can_handle(object)) {
			continue;
		}
		valid_plugins.push_back(inspector_plugins[i]);
	}

	// Decide if properties should be drawn with the warning color (yellow),
	// or if the whole object should be considered read-only.
	bool draw_warning = false;
	bool all_read_only = false;
	if (is_inside_tree()) {
		if (object->has_method("_is_read_only")) {
			all_read_only = object->call("_is_read_only");
		}

		Node *nod = Object::cast_to<Node>(object);
		Node *es = EditorNode::get_singleton()->get_edited_scene();
		if (nod && es != nod && nod->get_owner() != es) {
			// Draw in warning color edited nodes that are not in the currently edited scene,
			// as changes may be lost in the future.
			draw_warning = true;
		} else {
			if (!all_read_only) {
				Resource *res = Object::cast_to<Resource>(object);
				if (res) {
					all_read_only = EditorNode::get_singleton()->is_resource_read_only(res);
				}
			}
		}
	}

	String filter = search_box ? search_box->get_text() : "";
	String group;
	String group_base;
	String subgroup;
	String subgroup_base;
	int section_depth = 0;
	bool disable_favorite = false;
	VBoxContainer *category_vbox = nullptr;

	List<PropertyInfo> plist;
	object->get_property_list(&plist, true);

	HashMap<VBoxContainer *, HashMap<String, VBoxContainer *>> vbox_per_path;
	HashMap<String, EditorInspectorArray *> editor_inspector_array_per_prefix;
	HashMap<String, HashMap<String, LocalVector<EditorProperty *>>> favorites_to_add;

	Color sscolor = get_theme_color(SNAME("prop_subsection"), EditorStringName(Editor));

	// Get the lists of editors to add the beginning.
	for (Ref<EditorInspectorPlugin> &ped : valid_plugins) {
		ped->parse_begin(object);
		_parse_added_editors(begin_vbox, nullptr, ped);
	}
	if (begin_vbox->get_child_count()) {
		begin_vbox->show();
	}

	StringName doc_name;

	// Get the lists of editors for properties.
	for (List<PropertyInfo>::Element *E_property = plist.front(); E_property; E_property = E_property->next()) {
		PropertyInfo &p = E_property->get();

		if (p.usage & PROPERTY_USAGE_SUBGROUP) {
			// Setup a property sub-group.
			subgroup = p.name;

			Vector<String> hint_parts = p.hint_string.split(",");
			subgroup_base = hint_parts[0];
			if (hint_parts.size() > 1) {
				section_depth = hint_parts[1].to_int();
			} else {
				section_depth = 0;
			}

			continue;

		} else if (p.usage & PROPERTY_USAGE_GROUP) {
			// Setup a property group.
			group = p.name;

			Vector<String> hint_parts = p.hint_string.split(",");
			group_base = hint_parts[0];
			if (hint_parts.size() > 1) {
				section_depth = hint_parts[1].to_int();
			} else {
				section_depth = 0;
			}

			subgroup = "";
			subgroup_base = "";

			continue;

		} else if (p.usage & PROPERTY_USAGE_CATEGORY) {
			// Setup a property category.
			group = "";
			group_base = "";
			subgroup = "";
			subgroup_base = "";
			section_depth = 0;
			disable_favorite = false;

			vbox_per_path.clear();
			editor_inspector_array_per_prefix.clear();

			// `hint_script` should contain a native class name or a script path.
			// Otherwise the category was probably added via `@export_category` or `_get_property_list()`.
			const bool is_custom_category = p.hint_string.is_empty();

			// Iterate over remaining properties. If no properties in category, skip the category.
			List<PropertyInfo>::Element *N = E_property->next();
			bool valid = true;
			while (N) {
				if (!N->get().name.begins_with("metadata/_") && N->get().usage & PROPERTY_USAGE_EDITOR &&
						(!filter.is_empty() || !restrict_to_basic || (N->get().usage & PROPERTY_USAGE_EDITOR_BASIC_SETTING))) {
					break;
				}
				// Treat custom categories as second-level ones. Do not skip a normal category if it is followed by a custom one.
				// Skip in the other 3 cases (normal -> normal, custom -> custom, custom -> normal).
				if ((N->get().usage & PROPERTY_USAGE_CATEGORY) && (is_custom_category || !N->get().hint_string.is_empty())) {
					valid = false;
					break;
				}
				N = N->next();
			}
			if (!valid) {
				continue; // Empty, ignore it.
			}

			String category_label;
			String category_tooltip;
			Ref<Texture> category_icon;

			// Do not add an icon, do not change the current class (`doc_name`) for custom categories.
			if (is_custom_category) {
				category_label = p.name;
				category_tooltip = p.name;
			} else {
				doc_name = p.name;
				category_label = p.name;

				// Use category's owner script to update some of its information.
				if (!EditorNode::get_editor_data().is_type_recognized(p.name) && ResourceLoader::exists(p.hint_string, "Script")) {
					Ref<Script> scr = ResourceLoader::load(p.hint_string, "Script");
					if (scr.is_valid()) {
						doc_name = scr->get_doc_class_name();

						StringName script_name = EditorNode::get_editor_data().script_class_get_name(scr->get_path());
						if (script_name != StringName()) {
							category_label = script_name;
							category_icon = EditorNode::get_singleton()->get_class_icon(script_name);
						} else {
							category_icon = EditorNode::get_singleton()->get_object_icon(scr.ptr(), "Object");
						}

						// Property favorites aren't compatible with built-in scripts.
						if (scr->is_built_in()) {
							disable_favorite = true;
						}
					}
				}

				if (category_icon.is_null() && !p.name.is_empty()) {
					category_icon = EditorNode::get_singleton()->get_class_icon(p.name);
				}

				if (use_doc_hints) {
					// `|` separators used in `EditorHelpBit`.
					category_tooltip = "class|" + doc_name + "|";
				}
			}

			if ((is_custom_category && !show_custom_categories) || (!is_custom_category && !show_standard_categories)) {
				continue;
			}

			// Hide the "MultiNodeEdit" category for MultiNodeEdit.
			if (Object::cast_to<MultiNodeEdit>(object) && p.name == "MultiNodeEdit") {
				continue;
			}

			// Create an EditorInspectorCategory and add it to the inspector.
			EditorInspectorCategory *category = memnew(EditorInspectorCategory);
			main_vbox->add_child(category);
			category_vbox = nullptr; // Reset.

			// Set the category info.
			category->label = category_label;
			category->set_tooltip_text(category_tooltip);
			category->icon = category_icon;
			if (!is_custom_category) {
				category->doc_class_name = doc_name;
			}

			// Add editors at the start of a category.
			for (Ref<EditorInspectorPlugin> &ped : valid_plugins) {
				ped->parse_category(object, p.name);
				_parse_added_editors(main_vbox, nullptr, ped);
			}

			continue;

		} else if (p.name.begins_with("metadata/_") || !(p.usage & PROPERTY_USAGE_EDITOR) || _is_property_disabled_by_feature_profile(p.name) ||
				(filter.is_empty() && restrict_to_basic && !(p.usage & PROPERTY_USAGE_EDITOR_BASIC_SETTING))) {
			// Ignore properties that are not supposed to be in the inspector.
			continue;
		}

		if (p.name == "script") {
			// Script should go into its own category.
			category_vbox = nullptr;
		}

		if (p.usage & PROPERTY_USAGE_HIGH_END_GFX && RS::get_singleton()->is_low_end()) {
			// Do not show this property in low end gfx.
			continue;
		}

		if (p.name == "script" && (hide_script || bool(object->call("_hide_script_from_inspector")))) {
			// Hide script variables from inspector if required.
			continue;
		}

		if (p.name.begins_with("metadata/") && bool(object->call("_hide_metadata_from_inspector"))) {
			// Hide metadata from inspector if required.
			continue;
		}

		// Get the path for property.
		String path = p.name;

		// First check if we have an array that fits the prefix.
		String array_prefix = "";
		int array_index = -1;
		for (KeyValue<String, EditorInspectorArray *> &E : editor_inspector_array_per_prefix) {
			if (p.name.begins_with(E.key) && E.key.length() > array_prefix.length()) {
				array_prefix = E.key;
			}
		}

		if (!array_prefix.is_empty()) {
			// If we have an array element, find the according index in array.
			String str = p.name.trim_prefix(array_prefix);
			int to_char_index = 0;
			while (to_char_index < str.length()) {
				if (!is_digit(str[to_char_index])) {
					break;
				}
				to_char_index++;
			}
			if (to_char_index > 0) {
				array_index = str.left(to_char_index).to_int();
			} else {
				array_prefix = "";
			}
		}

		// Don't allow to favorite array items.
		if (!disable_favorite) {
			disable_favorite = !array_prefix.is_empty();
		}

		if (!array_prefix.is_empty()) {
			path = path.trim_prefix(array_prefix);
			int char_index = path.find_char('/');
			if (char_index >= 0) {
				path = path.right(-char_index - 1);
			} else {
				path = vformat(TTR("Element %s"), array_index);
			}
		} else {
			// Check if we exit or not a subgroup. If there is a prefix, remove it from the property label string.
			if (!subgroup.is_empty() && !subgroup_base.is_empty()) {
				if (path.begins_with(subgroup_base)) {
					path = path.trim_prefix(subgroup_base);
				} else if (subgroup_base.begins_with(path)) {
					// Keep it, this is used pretty often.
				} else {
					subgroup = ""; // The prefix changed, we are no longer in the subgroup.
				}
			}

			// Check if we exit or not a group. If there is a prefix, remove it from the property label string.
			if (!group.is_empty() && !group_base.is_empty() && subgroup.is_empty()) {
				if (path.begins_with(group_base)) {
					path = path.trim_prefix(group_base);
				} else if (group_base.begins_with(path)) {
					// Keep it, this is used pretty often.
				} else {
					group = ""; // The prefix changed, we are no longer in the group.
					subgroup = "";
				}
			}

			// Add the group and subgroup to the path.
			if (!subgroup.is_empty()) {
				path = subgroup + "/" + path;
			}
			if (!group.is_empty()) {
				path = group + "/" + path;
			}
		}

		// Get the property label's string.
		String name_override = (path.contains_char('/')) ? path.substr(path.rfind_char('/') + 1) : path;
		String feature_tag;
		{
			const int dot = name_override.find_char('.');
			if (dot != -1) {
				feature_tag = name_override.substr(dot);
				name_override = name_override.substr(0, dot);
			}
		}

		// Don't localize script variables.
		EditorPropertyNameProcessor::Style name_style = property_name_style;
		if ((p.usage & PROPERTY_USAGE_SCRIPT_VARIABLE) && name_style == EditorPropertyNameProcessor::STYLE_LOCALIZED) {
			name_style = EditorPropertyNameProcessor::STYLE_CAPITALIZED;
		}
		const String property_label_string = EditorPropertyNameProcessor::get_singleton()->process_name(name_override, name_style, p.name, doc_name) + feature_tag;

		// Remove the property from the path.
		int idx = path.rfind_char('/');
		if (idx > -1) {
			path = path.left(idx);
		} else {
			path = "";
		}

		// Ignore properties that do not fit the filter.
		if (use_filter && !filter.is_empty()) {
			const String property_path = property_prefix + (path.is_empty() ? "" : path + "/") + name_override;
			if (!_property_path_matches(property_path, filter, property_name_style)) {
				continue;
			}
		}

		// Recreate the category vbox if it was reset.
		if (category_vbox == nullptr) {
			category_vbox = memnew(VBoxContainer);
			category_vbox->hide();
			main_vbox->add_child(category_vbox);
		}

		// Find the correct section/vbox to add the property editor to.
		VBoxContainer *root_vbox = array_prefix.is_empty() ? main_vbox : editor_inspector_array_per_prefix[array_prefix]->get_vbox(array_index);
		if (!root_vbox) {
			continue;
		}

		if (!vbox_per_path.has(root_vbox)) {
			vbox_per_path[root_vbox] = HashMap<String, VBoxContainer *>();
			vbox_per_path[root_vbox][""] = root_vbox;
		}

		VBoxContainer *current_vbox = root_vbox;
		String acc_path = "";
		int level = 1;

		Vector<String> components = path.split("/");
		for (int i = 0; i < components.size(); i++) {
			const String &component = components[i];
			acc_path += (i > 0) ? "/" + component : component;

			if (!vbox_per_path[root_vbox].has(acc_path)) {
				// If the section does not exists, create it.
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
				if ((i == 0 && component == group) || (i == 1 && component == subgroup)) {
					if (section_name_style == EditorPropertyNameProcessor::STYLE_LOCALIZED) {
						label = EditorPropertyNameProcessor::get_singleton()->translate_group_name(component);
						tooltip = component;
					} else {
						label = component;
						tooltip = EditorPropertyNameProcessor::get_singleton()->translate_group_name(component);
					}
				} else {
					label = EditorPropertyNameProcessor::get_singleton()->process_name(component, section_name_style, p.name, doc_name);
					tooltip = EditorPropertyNameProcessor::get_singleton()->process_name(component, EditorPropertyNameProcessor::get_tooltip_style(section_name_style), p.name, doc_name);
				}

				Color c = sscolor;
				c.a /= level;
				section->setup(acc_path, label, object, c, use_folding, section_depth, level);
				section->set_tooltip_text(tooltip);

				// Add editors at the start of a group.
				for (Ref<EditorInspectorPlugin> &ped : valid_plugins) {
					ped->parse_group(object, path);
					_parse_added_editors(section->get_vbox(), section, ped);
				}

				vbox_per_path[root_vbox][acc_path] = section->get_vbox();
			}

			current_vbox = vbox_per_path[root_vbox][acc_path];
			level = (MIN(level + 1, 4));
		}

		// If we did not find a section to add the property to, add it to the category vbox instead (the category vbox handles margins correctly).
		if (current_vbox == main_vbox) {
			category_vbox->show();
			current_vbox = category_vbox;
		}

		// Check if the property is an array counter, if so create a dedicated array editor for the array.
		if (p.usage & PROPERTY_USAGE_ARRAY) {
			EditorInspectorArray *editor_inspector_array = nullptr;
			StringName array_element_prefix;
			Color c = sscolor;
			c.a /= level;

			Vector<String> class_name_components = String(p.class_name).split(",");

			int page_size = 5;
			bool movable = true;
			bool numbered = false;
			bool foldable = use_folding;
			String add_button_text = TTR("Add Element");
			String swap_method;
			for (int i = (p.type == Variant::NIL ? 1 : 2); i < class_name_components.size(); i++) {
				if (class_name_components[i].begins_with("page_size") && class_name_components[i].get_slice_count("=") == 2) {
					page_size = class_name_components[i].get_slice("=", 1).to_int();
				} else if (class_name_components[i].begins_with("add_button_text") && class_name_components[i].get_slice_count("=") == 2) {
					add_button_text = class_name_components[i].get_slice("=", 1).strip_edges();
				} else if (class_name_components[i] == "static") {
					movable = false;
				} else if (class_name_components[i] == "numbered") {
					numbered = true;
				} else if (class_name_components[i] == "unfoldable") {
					foldable = false;
				} else if (class_name_components[i].begins_with("swap_method") && class_name_components[i].get_slice_count("=") == 2) {
					swap_method = class_name_components[i].get_slice("=", 1).strip_edges();
				}
			}

			if (p.type == Variant::NIL) {
				// Setup the array to use a method to create/move/delete elements.
				array_element_prefix = class_name_components[0];
				editor_inspector_array = memnew(EditorInspectorArray(all_read_only));

				String array_label = path.contains_char('/') ? path.substr(path.rfind_char('/') + 1) : path;
				array_label = EditorPropertyNameProcessor::get_singleton()->process_name(property_label_string, property_name_style, p.name, doc_name);
				int page = per_array_page.has(array_element_prefix) ? per_array_page[array_element_prefix] : 0;
				editor_inspector_array->setup_with_move_element_function(object, array_label, array_element_prefix, page, c, use_folding);
				editor_inspector_array->connect("page_change_request", callable_mp(this, &EditorInspector::_page_change_request).bind(array_element_prefix));
			} else if (p.type == Variant::INT) {
				// Setup the array to use the count property and built-in functions to create/move/delete elements.
				if (class_name_components.size() >= 2) {
					array_element_prefix = class_name_components[1];
					editor_inspector_array = memnew(EditorInspectorArray(all_read_only));
					int page = per_array_page.has(array_element_prefix) ? per_array_page[array_element_prefix] : 0;

					editor_inspector_array->setup_with_count_property(object, class_name_components[0], p.name, array_element_prefix, page, c, foldable, movable, numbered, page_size, add_button_text, swap_method);
					editor_inspector_array->connect("page_change_request", callable_mp(this, &EditorInspector::_page_change_request).bind(array_element_prefix));
				}
			}

			if (editor_inspector_array) {
				current_vbox->add_child(editor_inspector_array);
				editor_inspector_array_per_prefix[array_element_prefix] = editor_inspector_array;
			}

			continue;
		}

		// Checkable and checked properties.
		bool checkable = false;
		bool checked = false;
		if (p.usage & PROPERTY_USAGE_CHECKABLE) {
			checkable = true;
			checked = p.usage & PROPERTY_USAGE_CHECKED;
		}

		bool property_read_only = (p.usage & PROPERTY_USAGE_READ_ONLY) || read_only;

		// Mark properties that would require an editor restart (mostly when editing editor settings).
		if (p.usage & PROPERTY_USAGE_RESTART_IF_CHANGED) {
			restart_request_props.insert(p.name);
		}

		String doc_path;
		String theme_item_name;
		StringName classname = doc_name;

		// Build the doc hint, to use as tooltip.
		if (use_doc_hints) {
			if (!object_class.is_empty()) {
				classname = object_class;
			} else if (Object::cast_to<MultiNodeEdit>(object)) {
				classname = Object::cast_to<MultiNodeEdit>(object)->get_edited_class_name();
			} else if (classname == "") {
				classname = object->get_class_name();
				Resource *res = Object::cast_to<Resource>(object);
				if (res && !res->get_script().is_null()) {
					// Grab the script of this resource to get the evaluated script class.
					Ref<Script> scr = res->get_script();
					if (scr.is_valid()) {
						Vector<DocData::ClassDoc> docs = scr->get_documentation();
						if (!docs.is_empty()) {
							// The documentation of a GDScript's main class is at the end of the array.
							// Hacky because this isn't necessarily always guaranteed.
							classname = docs[docs.size() - 1].name;
						}
					}
				}
			}

			StringName propname = property_prefix + p.name;
			bool found = false;

			// Small hack for theme_overrides. They are listed under Control, but come from another class.
			if (classname == "Control" && p.name.begins_with("theme_override_")) {
				classname = get_edited_object()->get_class();
			}

			// Search for the doc path in the cache.
			HashMap<StringName, HashMap<StringName, DocCacheInfo>>::Iterator E = doc_cache.find(classname);
			if (E) {
				HashMap<StringName, DocCacheInfo>::Iterator F = E->value.find(propname);
				if (F) {
					found = true;
					doc_path = F->value.doc_path;
					theme_item_name = F->value.theme_item_name;
				}
			}

			if (!found) {
				DocTools *dd = EditorHelp::get_doc_data();
				// Do not cache the doc path information of scripts.
				bool is_native_class = ClassDB::class_exists(classname);

				HashMap<String, DocData::ClassDoc>::ConstIterator F = dd->class_list.find(classname);
				while (F) {
					Vector<String> slices = propname.operator String().split("/");
					// Check if it's a theme item first.
					if (slices.size() == 2 && slices[0].begins_with("theme_override_")) {
						for (int i = 0; i < F->value.theme_properties.size(); i++) {
							String doc_path_current = "class_theme_item:" + F->value.name + ":" + F->value.theme_properties[i].name;
							if (F->value.theme_properties[i].name == slices[1]) {
								doc_path = doc_path_current;
								theme_item_name = F->value.theme_properties[i].name;
							}
						}
					} else {
						for (int i = 0; i < F->value.properties.size(); i++) {
							String doc_path_current = "class_property:" + F->value.name + ":" + F->value.properties[i].name;
							if (F->value.properties[i].name == propname.operator String()) {
								doc_path = doc_path_current;
							}
						}
					}

					if (is_native_class) {
						DocCacheInfo cache_info;
						cache_info.doc_path = doc_path;
						cache_info.theme_item_name = theme_item_name;
						doc_cache[classname][propname] = cache_info;
					}

					if (!doc_path.is_empty() || F->value.inherits.is_empty()) {
						break;
					}
					// Couldn't find the doc path in the class itself, try its super class.
					F = dd->class_list.find(F->value.inherits);
				}
			}
		}

		Vector<EditorInspectorPlugin::AddedEditor> editors;
		Vector<EditorInspectorPlugin::AddedEditor> late_editors;

		// Search for the inspector plugin that will handle the properties. Then add the correct property editor to it.
		for (Ref<EditorInspectorPlugin> &ped : valid_plugins) {
			bool exclusive = ped->parse_property(object, p.type, p.name, p.hint, p.hint_string, p.usage, wide_editors);

			for (const EditorInspectorPlugin::AddedEditor &F : ped->added_editors) {
				if (F.add_to_end) {
					late_editors.push_back(F);
				} else {
					editors.push_back(F);
				}
			}

			ped->added_editors.clear();

			if (exclusive) {
				break;
			}
		}

		editors.append_array(late_editors);

		const Node *node = Object::cast_to<Node>(object);

		Vector<SceneState::PackState> sstack;
		if (node != nullptr) {
			const Node *es = EditorNode::get_singleton()->get_edited_scene();
			sstack = PropertyUtils::get_node_states_stack(node, es);
		}

		for (int i = 0; i < editors.size(); i++) {
			EditorProperty *ep = Object::cast_to<EditorProperty>(editors[i].property_editor);
			const Vector<String> &properties = editors[i].properties;

			if (ep) {
				// Set all this before the control gets the ENTER_TREE notification.
				ep->object = object;

				if (properties.size()) {
					if (properties.size() == 1) {
						// Since it's one, associate:
						ep->property = properties[0];
						ep->property_path = property_prefix + properties[0];
						ep->property_usage = p.usage;
						// And set label?
					}
					if (!editors[i].label.is_empty()) {
						ep->set_label(editors[i].label);
					} else {
						// Use the existing one.
						ep->set_label(property_label_string);
					}

					for (int j = 0; j < properties.size(); j++) {
						String prop = properties[j];

						if (!editor_property_map.has(prop)) {
							editor_property_map[prop] = List<EditorProperty *>();
						}
						editor_property_map[prop].push_back(ep);
					}
				}

				Node *section_search = current_vbox->get_parent();
				while (section_search) {
					EditorInspectorSection *section = Object::cast_to<EditorInspectorSection>(section_search);
					if (section) {
						ep->connect("property_can_revert_changed", callable_mp(section, &EditorInspectorSection::property_can_revert_changed));
					}
					section_search = section_search->get_parent();
					if (Object::cast_to<EditorInspector>(section_search)) {
						// Skip sub-resource inspectors.
						break;
					}
				}

				if (p.name.begins_with("metadata/")) {
					Variant _default = Variant();
					if (node != nullptr) {
						_default = PropertyUtils::get_property_default_value(node, p.name, nullptr, &sstack, false, nullptr, nullptr);
					}
					ep->set_deletable(_default == Variant());
				} else {
					ep->set_deletable(deletable_properties);
				}

				ep->set_draw_warning(draw_warning);
				ep->set_use_folding(use_folding);
				ep->set_favoritable(can_favorite && !disable_favorite && !ep->is_deletable());
				ep->set_checkable(checkable);
				ep->set_checked(checked);
				ep->set_keying(keying);
				ep->set_read_only(property_read_only || all_read_only);
			}

			if (ep && ep->is_favoritable() && current_favorites.has(p.name)) {
				ep->favorited = true;
				favorites_to_add[group][subgroup].push_back(ep);
			} else {
				current_vbox->add_child(editors[i].property_editor);
			}

			if (ep) {
				// Eventually, set other properties/signals after the property editor got added to the tree.
				bool update_all = (p.usage & PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED);
				ep->connect("property_changed", callable_mp(this, &EditorInspector::_property_changed).bind(update_all));
				ep->connect("property_keyed", callable_mp(this, &EditorInspector::_property_keyed));
				ep->connect("property_deleted", callable_mp(this, &EditorInspector::_property_deleted), CONNECT_DEFERRED);
				ep->connect("property_keyed_with_value", callable_mp(this, &EditorInspector::_property_keyed_with_value));
				ep->connect("property_favorited", callable_mp(this, &EditorInspector::_set_property_favorited), CONNECT_DEFERRED);
				ep->connect("property_checked", callable_mp(this, &EditorInspector::_property_checked));
				ep->connect("property_pinned", callable_mp(this, &EditorInspector::_property_pinned));
				ep->connect("selected", callable_mp(this, &EditorInspector::_property_selected));
				ep->connect("multiple_properties_changed", callable_mp(this, &EditorInspector::_multiple_properties_changed));
				ep->connect("resource_selected", callable_mp(this, &EditorInspector::_resource_selected), CONNECT_DEFERRED);
				ep->connect("object_id_selected", callable_mp(this, &EditorInspector::_object_id_selected), CONNECT_DEFERRED);

				if (use_doc_hints) {
					// `|` separators used in `EditorHelpBit`.
					if (theme_item_name.is_empty()) {
						if (p.name.contains("shader_parameter/")) {
							ShaderMaterial *shader_material = Object::cast_to<ShaderMaterial>(object);
							if (shader_material) {
								ep->set_tooltip_text("property|" + shader_material->get_shader()->get_path() + "|" + property_prefix + p.name);
							}
						} else if (p.usage & PROPERTY_USAGE_INTERNAL) {
							ep->set_tooltip_text("internal_property|" + classname + "|" + property_prefix + p.name);
						} else {
							ep->set_tooltip_text("property|" + classname + "|" + property_prefix + p.name);
						}
					} else {
						ep->set_tooltip_text("theme_item|" + classname + "|" + theme_item_name);
					}
					ep->has_doc_tooltip = true;
				}

				ep->set_doc_path(doc_path);
				ep->set_internal(p.usage & PROPERTY_USAGE_INTERNAL);

				ep->update_property();
				ep->_update_flags();
				ep->update_editor_property_status();
				ep->update_cache();

				if (current_selected && ep->property == current_selected) {
					ep->select(current_focusable);
				}
			}
		}
	}

	if (!current_favorites.is_empty()) {
		favorites_section->show();

		// Organize the favorited properties in their sections, to keep context and differentiate from others with the same name.
		bool is_localized = property_name_style == EditorPropertyNameProcessor::STYLE_LOCALIZED;
		for (const KeyValue<String, HashMap<String, LocalVector<EditorProperty *>>> &KV : favorites_to_add) {
			String section_name = KV.key;
			String label;
			String tooltip;
			VBoxContainer *parent_vbox = favorites_vbox;
			if (!section_name.is_empty()) {
				if (is_localized) {
					label = EditorPropertyNameProcessor::get_singleton()->translate_group_name(section_name);
					tooltip = section_name;
				} else {
					label = section_name;
					tooltip = EditorPropertyNameProcessor::get_singleton()->translate_group_name(section_name);
				}

				EditorInspectorSection *section = memnew(EditorInspectorSection);
				favorites_groups_vbox->add_child(section);
				parent_vbox = section->get_vbox();
				section->setup("", section_name, object, sscolor, false);
				section->set_tooltip_text(tooltip);
			}

			for (const KeyValue<String, LocalVector<EditorProperty *>> &KV2 : KV.value) {
				section_name = KV2.key;
				VBoxContainer *vbox = parent_vbox;
				if (!section_name.is_empty()) {
					if (is_localized) {
						label = EditorPropertyNameProcessor::get_singleton()->translate_group_name(section_name);
						tooltip = section_name;
					} else {
						label = section_name;
						tooltip = EditorPropertyNameProcessor::get_singleton()->translate_group_name(section_name);
					}

					EditorInspectorSection *section = memnew(EditorInspectorSection);
					vbox->add_child(section);
					vbox = section->get_vbox();
					section->setup("", section_name, object, sscolor, false);
					section->set_tooltip_text(tooltip);
				}

				for (EditorProperty *ep : KV2.value) {
					vbox->add_child(ep);
				}
			}
		}

		// Show a separator if there's no category to clearly divide the properties.
		favorites_separator->hide();
		if (main_vbox->get_child_count() > 0) {
			EditorInspectorCategory *category = Object::cast_to<EditorInspectorCategory>(main_vbox->get_child(0));
			if (!category) {
				favorites_separator->show();
			}
		}

		// Clean up empty sections.
		for (List<EditorInspectorSection *>::Element *I = sections.back(); I; I = I->prev()) {
			EditorInspectorSection *section = I->get();
			if (section->get_vbox()->get_child_count() == 0) {
				sections.erase(section);
				vbox_per_path[main_vbox].erase(section->get_section());
				memdelete(section);
			}
		}
	}

	if (!hide_metadata && !object->call("_hide_metadata_from_inspector")) {
		// Add 4px of spacing between the "Add Metadata" button and the content above it.
		Control *spacer = memnew(Control);
		spacer->set_custom_minimum_size(Size2(0, 4) * EDSCALE);
		main_vbox->add_child(spacer);

		Button *add_md = EditorInspector::create_inspector_action_button(TTR("Add Metadata"));
		add_md->set_button_icon(get_editor_theme_icon(SNAME("Add")));
		add_md->connect(SceneStringName(pressed), callable_mp(this, &EditorInspector::_show_add_meta_dialog));
		main_vbox->add_child(add_md);
		if (all_read_only) {
			add_md->set_disabled(true);
		}
	}

	// Get the lists of to add at the end.
	for (Ref<EditorInspectorPlugin> &ped : valid_plugins) {
		ped->parse_end(object);
		_parse_added_editors(main_vbox, nullptr, ped);
	}

	if (is_main_editor_inspector()) {
		// Updating inspector might invalidate some editing owners.
		EditorNode::get_singleton()->hide_unused_editors();
	}

	get_root_inspector()->set_follow_focus(true);
}

void EditorInspector::update_property(const String &p_prop) {
	if (!editor_property_map.has(p_prop)) {
		return;
	}

	for (EditorProperty *E : editor_property_map[p_prop]) {
		E->update_property();
		E->update_editor_property_status();
		E->update_cache();
	}
}

void EditorInspector::_clear(bool p_hide_plugins) {
	begin_vbox->hide();
	while (begin_vbox->get_child_count()) {
		memdelete(begin_vbox->get_child(0));
	}

	favorites_section->hide();
	while (favorites_vbox->get_child_count()) {
		memdelete(favorites_vbox->get_child(0));
	}
	while (favorites_groups_vbox->get_child_count()) {
		memdelete(favorites_groups_vbox->get_child(0));
	}

	while (main_vbox->get_child_count()) {
		memdelete(main_vbox->get_child(0));
	}

	property_selected = StringName();
	property_focusable = -1;
	editor_property_map.clear();
	sections.clear();
	pending.clear();
	restart_request_props.clear();

	if (p_hide_plugins && is_main_editor_inspector()) {
		EditorNode::get_singleton()->hide_unused_editors(this);
	}
}

Object *EditorInspector::get_edited_object() {
	return object;
}

Object *EditorInspector::get_next_edited_object() {
	return next_object;
}

void EditorInspector::edit(Object *p_object) {
	if (object == p_object) {
		return;
	}

	next_object = p_object; // Some plugins need to know the next edited object when clearing the inspector.
	if (object) {
		if (likely(Variant(object).get_validated_object())) {
			object->disconnect(CoreStringName(property_list_changed), callable_mp(this, &EditorInspector::_changed_callback));
		}
		_clear();
	}
	per_array_page.clear();

	object = p_object;

	if (object) {
		update_scroll_request = 0; //reset
		if (scroll_cache.has(object->get_instance_id())) { //if exists, set something else
			update_scroll_request = scroll_cache[object->get_instance_id()]; //done this way because wait until full size is accommodated
		}
		object->connect(CoreStringName(property_list_changed), callable_mp(this, &EditorInspector::_changed_callback));

		can_favorite = Object::cast_to<Node>(object) || Object::cast_to<Resource>(object);
		_update_current_favorites();

		update_tree();
	}

	// Keep it available until the end so it works with both main and sub inspectors.
	next_object = nullptr;

	emit_signal(SNAME("edited_object_changed"));
}

void EditorInspector::set_keying(bool p_active) {
	if (keying == p_active) {
		return;
	}
	keying = p_active;
	_keying_changed();
}

void EditorInspector::_keying_changed() {
	for (const KeyValue<StringName, List<EditorProperty *>> &F : editor_property_map) {
		for (EditorProperty *E : F.value) {
			if (E) {
				E->set_keying(keying);
			}
		}
	}
}

void EditorInspector::set_read_only(bool p_read_only) {
	if (p_read_only == read_only) {
		return;
	}
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

void EditorInspector::set_use_settings_name_style(bool p_enable) {
	if (use_settings_name_style == p_enable) {
		return;
	}
	use_settings_name_style = p_enable;
	if (use_settings_name_style) {
		set_property_name_style(EditorPropertyNameProcessor::get_singleton()->get_settings_style());
	}
}

void EditorInspector::set_autoclear(bool p_enable) {
	autoclear = p_enable;
}

void EditorInspector::set_show_categories(bool p_show_standard, bool p_show_custom) {
	show_standard_categories = p_show_standard;
	show_custom_categories = p_show_custom;
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

void EditorInspector::set_hide_metadata(bool p_hide) {
	hide_metadata = p_hide;
	update_tree();
}

void EditorInspector::set_use_filter(bool p_use) {
	use_filter = p_use;
	update_tree();
}

void EditorInspector::register_text_enter(Node *p_line_edit) {
	search_box = Object::cast_to<LineEdit>(p_line_edit);
	if (search_box) {
		search_box->connect(SceneStringName(text_changed), callable_mp(this, &EditorInspector::_filter_changed));
	}
}

void EditorInspector::_filter_changed(const String &p_text) {
	update_tree();
}

void EditorInspector::set_use_folding(bool p_use_folding, bool p_update_tree) {
	use_folding = p_use_folding;

	if (p_update_tree) {
		update_tree();
	}
}

bool EditorInspector::is_using_folding() {
	return use_folding;
}

void EditorInspector::collapse_all_folding() {
	for (EditorInspectorSection *E : sections) {
		E->fold();
	}

	for (const KeyValue<StringName, List<EditorProperty *>> &F : editor_property_map) {
		for (EditorProperty *E : F.value) {
			E->collapse_all_folding();
		}
	}
}

void EditorInspector::expand_all_folding() {
	for (EditorInspectorSection *E : sections) {
		E->unfold();
	}
	for (const KeyValue<StringName, List<EditorProperty *>> &F : editor_property_map) {
		for (EditorProperty *E : F.value) {
			E->expand_all_folding();
		}
	}
}

void EditorInspector::expand_revertable() {
	HashSet<EditorInspectorSection *> sections_to_unfold[2];
	for (EditorInspectorSection *E : sections) {
		if (E->has_revertable_properties()) {
			sections_to_unfold[0].insert(E);
		}
	}

	// Climb up the hierarchy doing double buffering with the sets.
	int a = 0;
	int b = 1;
	while (sections_to_unfold[a].size()) {
		for (EditorInspectorSection *E : sections_to_unfold[a]) {
			E->unfold();

			Node *n = E->get_parent();
			while (n) {
				if (Object::cast_to<EditorInspector>(n)) {
					break;
				}
				if (Object::cast_to<EditorInspectorSection>(n) && !sections_to_unfold[a].has((EditorInspectorSection *)n)) {
					sections_to_unfold[b].insert((EditorInspectorSection *)n);
				}
				n = n->get_parent();
			}
		}

		sections_to_unfold[a].clear();
		SWAP(a, b);
	}

	for (const KeyValue<StringName, List<EditorProperty *>> &F : editor_property_map) {
		for (EditorProperty *E : F.value) {
			E->expand_revertable();
		}
	}
}

void EditorInspector::set_scroll_offset(int p_offset) {
	set_v_scroll(p_offset);
}

int EditorInspector::get_scroll_offset() const {
	return get_v_scroll();
}

void EditorInspector::set_use_wide_editors(bool p_enable) {
	wide_editors = p_enable;
}

void EditorInspector::set_root_inspector(EditorInspector *p_root_inspector) {
	root_inspector = p_root_inspector;
	// Only the root inspector should follow focus.
	set_follow_focus(false);
}

void EditorInspector::set_use_deletable_properties(bool p_enabled) {
	deletable_properties = p_enabled;
}

void EditorInspector::_page_change_request(int p_new_page, const StringName &p_array_prefix) {
	int prev_page = per_array_page.has(p_array_prefix) ? per_array_page[p_array_prefix] : 0;
	int new_page = MAX(0, p_new_page);
	if (new_page != prev_page) {
		per_array_page[p_array_prefix] = new_page;
		update_tree_pending = true;
	}
}

void EditorInspector::_edit_request_change(Object *p_object, const String &p_property) {
	if (object != p_object) { //may be undoing/redoing for a non edited object, so ignore
		return;
	}

	if (changing) {
		return;
	}

	if (p_property.is_empty()) {
		update_tree_pending = true;
	} else {
		pending.insert(p_property);
	}
}

void EditorInspector::_edit_set(const String &p_name, const Variant &p_value, bool p_refresh_all, const String &p_changed_field) {
	if (autoclear && editor_property_map.has(p_name)) {
		for (EditorProperty *E : editor_property_map[p_name]) {
			if (E->is_checkable()) {
				E->set_checked(true);
			}
		}
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (bool(object->call("_dont_undo_redo"))) {
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
		bool valid = false;
		Variant value = object->get(p_name, &valid);
		if (valid) {
			if (Object::cast_to<Control>(object) && (p_name == "anchors_preset" || p_name == "layout_mode")) {
				undo_redo->add_undo_method(object, "_edit_set_state", Object::cast_to<Control>(object)->_edit_get_state());
			} else {
				undo_redo->add_undo_property(object, p_name, value);
			}
		}

		List<StringName> linked_properties;
		ClassDB::get_linked_properties_info(object->get_class_name(), p_name, &linked_properties);

		for (const StringName &linked_prop : linked_properties) {
			valid = false;
			Variant undo_value = object->get(linked_prop, &valid);
			if (valid) {
				undo_redo->add_undo_property(object, linked_prop, undo_value);
			}
		}

		PackedStringArray linked_properties_dynamic = object->call("_get_linked_undo_properties", p_name, p_value);
		for (int i = 0; i < linked_properties_dynamic.size(); i++) {
			valid = false;
			Variant undo_value = object->get(linked_properties_dynamic[i], &valid);
			if (valid) {
				undo_redo->add_undo_property(object, linked_properties_dynamic[i], undo_value);
			}
		}

		Variant v_undo_redo = undo_redo;
		Variant v_object = object;
		Variant v_name = p_name;
		const Vector<Callable> &callbacks = EditorNode::get_editor_data().get_undo_redo_inspector_hook_callback();
		for (int i = 0; i < callbacks.size(); i++) {
			const Callable &callback = callbacks[i];

			const Variant *p_arguments[] = { &v_undo_redo, &v_object, &v_name, &p_value };
			Variant return_value;
			Callable::CallError call_error;

			callback.callp(p_arguments, 4, return_value, call_error);
			if (call_error.error != Callable::CallError::CALL_OK) {
				ERR_PRINT("Invalid UndoRedo callback.");
			}
		}

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
		for (EditorProperty *E : editor_property_map[p_name]) {
			E->update_editor_property_status();
		}
	}
}

void EditorInspector::_property_changed(const String &p_path, const Variant &p_value, const String &p_name, bool p_changing, bool p_update_all) {
	// The "changing" variable must be true for properties that trigger events as typing occurs,
	// like "text_changed" signal. E.g. text property of Label, Button, RichTextLabel, etc.
	if (p_changing) {
		changing++;
	}

	_edit_set(p_path, p_value, p_update_all, p_name);

	if (p_changing) {
		changing--;
	}

	if (restart_request_props.has(p_path)) {
		emit_signal(SNAME("restart_requested"));
	}
}

void EditorInspector::_multiple_properties_changed(const Vector<String> &p_paths, const Array &p_values, bool p_changing) {
	ERR_FAIL_COND(p_paths.is_empty() || p_values.is_empty());
	ERR_FAIL_COND(p_paths.size() != p_values.size());
	String names;
	for (int i = 0; i < p_paths.size(); i++) {
		if (i > 0) {
			names += ",";
		}
		names += p_paths[i];
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	// TRANSLATORS: This is describing a change to multiple properties at once. The parameter is a list of property names.
	undo_redo->create_action(vformat(TTR("Set Multiple: %s"), names), UndoRedo::MERGE_ENDS);
	for (int i = 0; i < p_paths.size(); i++) {
		_edit_set(p_paths[i], p_values[i], false, "");
		if (restart_request_props.has(p_paths[i])) {
			emit_signal(SNAME("restart_requested"));
		}
	}
	if (p_changing) {
		changing++;
	}
	undo_redo->commit_action();
	if (p_changing) {
		changing--;
	}
}

void EditorInspector::_property_keyed(const String &p_path, bool p_advance) {
	if (!object) {
		return;
	}

	// The second parameter could be null, causing the event to fire with less arguments, so use the pointer call which preserves it.
	const Variant args[3] = { p_path, object->get(p_path), p_advance };
	const Variant *argp[3] = { &args[0], &args[1], &args[2] };
	emit_signalp(SNAME("property_keyed"), argp, 3);
}

void EditorInspector::_property_deleted(const String &p_path) {
	if (!object) {
		return;
	}

	if (p_path.begins_with("metadata/")) {
		String name = p_path.replace_first("metadata/", "");
		EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
		undo_redo->create_action(vformat(TTR("Remove metadata %s"), name));
		undo_redo->add_do_method(object, "remove_meta", name);
		undo_redo->add_undo_method(object, "set_meta", name, object->get_meta(name));
		undo_redo->commit_action();
	}

	emit_signal(SNAME("property_deleted"), p_path);
}

void EditorInspector::_property_keyed_with_value(const String &p_path, const Variant &p_value, bool p_advance) {
	if (!object) {
		return;
	}

	// The second parameter could be null, causing the event to fire with less arguments, so use the pointer call which preserves it.
	const Variant args[3] = { p_path, p_value, p_advance };
	const Variant *argp[3] = { &args[0], &args[1], &args[2] };
	emit_signalp(SNAME("property_keyed"), argp, 3);
}

void EditorInspector::_property_checked(const String &p_path, bool p_checked) {
	if (!object) {
		return;
	}

	//property checked
	if (autoclear) {
		if (!p_checked) {
			_edit_set(p_path, Variant(), false, "");
		} else {
			Variant to_create;
			List<PropertyInfo> pinfo;
			object->get_property_list(&pinfo);
			for (const PropertyInfo &E : pinfo) {
				if (E.name == p_path) {
					Callable::CallError ce;
					Variant::construct(E.type, to_create, nullptr, 0, ce);
					break;
				}
			}
			_edit_set(p_path, to_create, false, "");
		}

		if (editor_property_map.has(p_path)) {
			for (EditorProperty *E : editor_property_map[p_path]) {
				E->set_checked(p_checked);
				E->update_property();
				E->update_editor_property_status();
				E->update_cache();
			}
		}
	} else {
		emit_signal(SNAME("property_toggled"), p_path, p_checked);
	}
}

void EditorInspector::_property_pinned(const String &p_path, bool p_pinned) {
	if (!object) {
		return;
	}

	Node *node = Object::cast_to<Node>(object);
	ERR_FAIL_NULL(node);

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(vformat(p_pinned ? TTR("Pinned %s") : TTR("Unpinned %s"), p_path));
	undo_redo->add_do_method(node, "_set_property_pinned", p_path, p_pinned);
	undo_redo->add_undo_method(node, "_set_property_pinned", p_path, !p_pinned);
	if (editor_property_map.has(p_path)) {
		for (List<EditorProperty *>::Element *E = editor_property_map[p_path].front(); E; E = E->next()) {
			undo_redo->add_do_method(E->get(), "_update_editor_property_status");
			undo_redo->add_undo_method(E->get(), "_update_editor_property_status");
		}
	}
	undo_redo->commit_action();
}

void EditorInspector::_property_selected(const String &p_path, int p_focusable) {
	property_selected = p_path;
	property_focusable = p_focusable;
	// Deselect the others.
	for (const KeyValue<StringName, List<EditorProperty *>> &F : editor_property_map) {
		if (F.key == property_selected) {
			continue;
		}
		for (EditorProperty *E : F.value) {
			if (E->is_selected()) {
				E->deselect();
			}
		}
	}

	emit_signal(SNAME("property_selected"), p_path);
}

void EditorInspector::_object_id_selected(const String &p_path, ObjectID p_id) {
	emit_signal(SNAME("object_id_selected"), p_id);
}

void EditorInspector::_resource_selected(const String &p_path, Ref<Resource> p_resource) {
	emit_signal(SNAME("resource_selected"), p_resource, p_path);
}

void EditorInspector::_node_removed(Node *p_node) {
	if (p_node == object) {
		edit(nullptr);
	}
}

void EditorInspector::_update_current_favorites() {
	current_favorites.clear();
	if (!can_favorite) {
		return;
	}

	HashMap<String, PackedStringArray> favorites = EditorSettings::get_singleton()->get_favorite_properties();

	// Fetch script properties.
	Ref<Script> scr = object->get_script();
	if (scr.is_valid()) {
		List<PropertyInfo> plist;
		// FIXME: Only properties from a saved script will be available, unsaved ones will be ignored.
		// Can cause a little wonkiness, while nothing serious, would be nice to find a way to get
		// unsaved ones without needing to get the entire property list of an object.
		scr->get_script_property_list(&plist);

		String path;
		HashMap<String, LocalVector<String>> props;

		for (PropertyInfo &p : plist) {
			if (p.usage & PROPERTY_USAGE_CATEGORY) {
				path = favorites.has(p.hint_string) ? p.hint_string : String();
			} else if (p.usage & PROPERTY_USAGE_SCRIPT_VARIABLE && !path.is_empty()) {
				props[path].push_back(p.name);
			}
		}

		// Add favorited properties while removing invalid ones.
		bool invalid_props = false;
		for (const KeyValue<String, LocalVector<String>> &KV : props) {
			path = KV.key;
			for (int i = 0; i < favorites[path].size(); i++) {
				String prop = favorites[path][i];
				if (KV.value.has(prop)) {
					current_favorites.append(prop);
				} else {
					invalid_props = true;
					favorites[path].erase(prop);
					i--;
				}
			}

			if (favorites[path].is_empty()) {
				favorites.erase(path);
			}
		}

		if (invalid_props) {
			EditorSettings::get_singleton()->set_favorite_properties(favorites);
		}
	}

	// Fetch built-in properties.
	StringName class_name = object->get_class_name();
	for (const KeyValue<String, PackedStringArray> &KV : favorites) {
		if (ClassDB::is_parent_class(class_name, KV.key)) {
			current_favorites.append_array(KV.value);
		}
	}
}

void EditorInspector::_set_property_favorited(const String &p_path, bool p_favorited) {
	if (!object) {
		return;
	}

	StringName validate_name = object->get_class_name();
	StringName class_name;

	String theme_property;
	if (p_path.begins_with("theme_override_")) {
		theme_property = p_path.get_slice("/", 1);
	}

	while (!validate_name.is_empty()) {
		class_name = validate_name;

		if (!theme_property.is_empty()) { // Deal with theme properties.
			bool found = false;
			HashMap<String, DocData::ClassDoc>::ConstIterator F = EditorHelp::get_doc_data()->class_list.find(class_name);
			if (F) {
				for (const DocData::ThemeItemDoc &prop : F->value.theme_properties) {
					if (prop.name == theme_property) {
						found = true;
						break;
					}
				}
			}

			if (found) {
				break;
			}
		} else if (ClassDB::has_property(class_name, p_path, true)) { // Check if the property is built-in.
			break;
		}

		validate_name = ClassDB::get_parent_class_nocheck(class_name);
	}

	// "script" isn't a real property, so a hack is necessary.
	if (validate_name.is_empty() && p_path != "script") {
		class_name = "";
	}

	if (class_name.is_empty()) {
		// Check if it's part of a script.
		Ref<Script> scr = object->get_script();
		if (scr.is_valid()) {
			List<PropertyInfo> plist;
			scr->get_script_property_list(&plist);

			String path;
			for (PropertyInfo &p : plist) {
				if (p.usage & PROPERTY_USAGE_CATEGORY) {
					path = p.hint_string;
				} else if (p.usage & PROPERTY_USAGE_SCRIPT_VARIABLE && p.name == p_path) {
					class_name = path;
					break;
				}
			}
		}

		ERR_FAIL_COND_MSG(class_name.is_empty(), "Can't favorite invalid property. If said property was from a script and recently renamed, try saving it first.");
	}

	HashMap<String, PackedStringArray> favorites = EditorSettings::get_singleton()->get_favorite_properties();
	if (p_favorited) {
		current_favorites.append(p_path);
		favorites[class_name].append(p_path);
	} else {
		current_favorites.erase(p_path);

		if (favorites.has(class_name) && favorites[class_name].has(p_path)) {
			if (favorites[class_name].size() > 1) {
				favorites[class_name].erase(p_path);
			} else {
				favorites.erase(class_name);
			}
		}
	}
	EditorSettings::get_singleton()->set_favorite_properties(favorites);

	update_tree();
}

void EditorInspector::_clear_current_favorites() {
	current_favorites.clear();

	HashMap<String, PackedStringArray> favorites = EditorSettings::get_singleton()->get_favorite_properties();

	Ref<Script> scr = object->get_script();
	if (scr.is_valid()) {
		List<PropertyInfo> plist;
		scr->get_script_property_list(&plist);

		for (PropertyInfo &p : plist) {
			if (p.usage & PROPERTY_USAGE_CATEGORY && favorites.has(p.hint_string)) {
				favorites.erase(p.hint_string);
			}
		}
	}

	StringName class_name = object->get_class_name();
	while (class_name) {
		if (favorites.has(class_name)) {
			favorites.erase(class_name);
		}

		class_name = ClassDB::get_parent_class(class_name);
	}

	EditorSettings::get_singleton()->set_favorite_properties(favorites);
	update_tree();
}

void EditorInspector::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			favorites_category->icon = get_editor_theme_icon(SNAME("Favorites"));

			int separation = get_theme_constant(SNAME("v_separation"), SNAME("EditorInspector"));
			base_vbox->add_theme_constant_override("separation", separation);
			begin_vbox->add_theme_constant_override("separation", separation);
			favorites_section->add_theme_constant_override("separation", separation);
			favorites_groups_vbox->add_theme_constant_override("separation", separation);
			main_vbox->add_theme_constant_override("separation", separation);
		} break;

		case NOTIFICATION_READY: {
			ERR_FAIL_NULL(EditorFeatureProfileManager::get_singleton());
			EditorFeatureProfileManager::get_singleton()->connect("current_feature_profile_changed", callable_mp(this, &EditorInspector::_feature_profile_changed));
			set_process(is_visible_in_tree());
			add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SceneStringName(panel), SNAME("Tree")));
			if (!is_sub_inspector()) {
				get_tree()->connect("node_removed", callable_mp(this, &EditorInspector::_node_removed));
			}
		} break;

		case NOTIFICATION_PREDELETE: {
			if (!is_sub_inspector() && is_inside_tree()) {
				get_tree()->disconnect("node_removed", callable_mp(this, &EditorInspector::_node_removed));
			}
			edit(nullptr);
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			set_process(is_visible_in_tree());
		} break;

		case NOTIFICATION_PROCESS: {
			if (update_scroll_request >= 0) {
				callable_mp((Range *)get_v_scroll_bar(), &Range::set_value).call_deferred(update_scroll_request);
				update_scroll_request = -1;
			}
			if (update_tree_pending) {
				refresh_countdown = float(EDITOR_GET("docks/property_editor/auto_refresh_interval"));
			} else if (refresh_countdown > 0) {
				refresh_countdown -= get_process_delta_time();
				if (refresh_countdown <= 0) {
					for (const KeyValue<StringName, List<EditorProperty *>> &F : editor_property_map) {
						for (EditorProperty *E : F.value) {
							if (E && !E->is_cache_valid()) {
								E->update_property();
								E->update_editor_property_status();
								E->update_cache();
							}
						}
					}
					refresh_countdown = float(EDITOR_GET("docks/property_editor/auto_refresh_interval"));
				}
			}

			changing++;

			if (update_tree_pending) {
				update_tree();
				update_tree_pending = false;
				pending.clear();

			} else {
				while (pending.size()) {
					StringName prop = *pending.begin();
					if (editor_property_map.has(prop)) {
						for (EditorProperty *E : editor_property_map[prop]) {
							E->update_property();
							E->update_editor_property_status();
							E->update_cache();
						}
					}
					pending.remove(pending.begin());
				}
			}

			changing--;
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			bool needs_update = false;
			if (!is_sub_inspector() && EditorThemeManager::is_generated_theme_outdated()) {
				add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SceneStringName(panel), SNAME("Tree")));
			}

			if (use_settings_name_style && EditorSettings::get_singleton()->check_changed_settings_in_group("interface/editor/localize_settings")) {
				EditorPropertyNameProcessor::Style style = EditorPropertyNameProcessor::get_settings_style();
				if (property_name_style != style) {
					property_name_style = style;
					needs_update = true;
				}
			}

			if (EditorSettings::get_singleton()->check_changed_settings_in_group("interface/inspector")) {
				needs_update = true;
			}

			if (needs_update) {
				update_tree();
			}
		} break;
	}
}

void EditorInspector::_changed_callback() {
	//this is called when property change is notified via notify_property_list_changed()
	if (object != nullptr) {
		_update_current_favorites();
		_edit_request_change(object, String());
	}
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

void EditorInspector::add_custom_property_description(const String &p_class, const String &p_property, const String &p_description) {
	const String key = vformat("property|%s|%s", p_class, p_property);
	custom_property_descriptions[key] = p_description;
}

String EditorInspector::get_custom_property_description(const String &p_property) const {
	HashMap<String, String>::ConstIterator E = custom_property_descriptions.find(p_property);
	if (E) {
		return E->value;
	}
	return "";
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

void EditorInspector::set_restrict_to_basic_settings(bool p_restrict) {
	restrict_to_basic = p_restrict;
	update_tree();
}

void EditorInspector::set_property_clipboard(const Variant &p_value) {
	property_clipboard = p_value;
}

Variant EditorInspector::get_property_clipboard() const {
	return property_clipboard;
}

void EditorInspector::_show_add_meta_dialog() {
	if (!add_meta_dialog) {
		add_meta_dialog = memnew(AddMetadataDialog());
		add_meta_dialog->connect(SceneStringName(confirmed), callable_mp(this, &EditorInspector::_add_meta_confirm));
		add_child(add_meta_dialog);
	}

	StringName dialog_title;
	Node *node = Object::cast_to<Node>(object);
	// If object is derived from Node use node name, if derived from Resource use classname.
	dialog_title = node ? node->get_name() : StringName(object->get_class());

	List<StringName> existing_meta_keys;
	object->get_meta_list(&existing_meta_keys);
	add_meta_dialog->open(dialog_title, existing_meta_keys);
}

void EditorInspector::_add_meta_confirm() {
	// Ensure metadata is unfolded when adding a new metadata.
	object->editor_set_section_unfold("metadata", true);

	String name = add_meta_dialog->get_meta_name();
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(vformat(TTR("Add metadata %s"), name));
	undo_redo->add_do_method(object, "set_meta", name, add_meta_dialog->get_meta_defval());
	undo_redo->add_undo_method(object, "remove_meta", name);
	undo_redo->commit_action();
}

void EditorInspector::_handle_menu_option(int p_option) {
	switch (p_option) {
		case MENU_UNFAVORITE_ALL:
			_clear_current_favorites();
			break;
	}
}

void EditorInspector::_bind_methods() {
	ClassDB::bind_method(D_METHOD("edit", "object"), &EditorInspector::edit);
	ClassDB::bind_method("_edit_request_change", &EditorInspector::_edit_request_change);
	ClassDB::bind_method("get_selected_path", &EditorInspector::get_selected_path);
	ClassDB::bind_method("get_edited_object", &EditorInspector::get_edited_object);

	ClassDB::bind_static_method("EditorInspector", D_METHOD("instantiate_property_editor", "object", "type", "path", "hint", "hint_text", "usage", "wide"), &EditorInspector::instantiate_property_editor, DEFVAL(false));

	ADD_SIGNAL(MethodInfo("property_selected", PropertyInfo(Variant::STRING, "property")));
	ADD_SIGNAL(MethodInfo("property_keyed", PropertyInfo(Variant::STRING, "property"), PropertyInfo(Variant::NIL, "value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT), PropertyInfo(Variant::BOOL, "advance")));
	ADD_SIGNAL(MethodInfo("property_deleted", PropertyInfo(Variant::STRING, "property")));
	ADD_SIGNAL(MethodInfo("resource_selected", PropertyInfo(Variant::OBJECT, "resource", PROPERTY_HINT_RESOURCE_TYPE, "Resource"), PropertyInfo(Variant::STRING, "path")));
	ADD_SIGNAL(MethodInfo("object_id_selected", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("property_edited", PropertyInfo(Variant::STRING, "property")));
	ADD_SIGNAL(MethodInfo("property_toggled", PropertyInfo(Variant::STRING, "property"), PropertyInfo(Variant::BOOL, "checked")));
	ADD_SIGNAL(MethodInfo("edited_object_changed"));
	ADD_SIGNAL(MethodInfo("restart_requested"));
}

EditorInspector::EditorInspector() {
	object = nullptr;

	base_vbox = memnew(VBoxContainer);
	base_vbox->set_h_size_flags(SIZE_EXPAND_FILL);
	add_child(base_vbox);

	begin_vbox = memnew(VBoxContainer);
	base_vbox->add_child(begin_vbox);
	begin_vbox->hide();

	favorites_section = memnew(VBoxContainer);
	base_vbox->add_child(favorites_section);
	favorites_section->hide();

	favorites_category = memnew(EditorInspectorCategory);
	favorites_category->set_as_favorite(this);
	favorites_section->add_child(favorites_category);
	favorites_category->label = TTR("Favorites");

	favorites_vbox = memnew(VBoxContainer);
	favorites_section->add_child(favorites_vbox);
	favorites_groups_vbox = memnew(VBoxContainer);
	favorites_section->add_child(favorites_groups_vbox);

	favorites_separator = memnew(HSeparator);
	favorites_section->add_child(favorites_separator);
	favorites_separator->hide();

	main_vbox = memnew(VBoxContainer);
	base_vbox->add_child(main_vbox);

	set_horizontal_scroll_mode(SCROLL_MODE_DISABLED);
	set_follow_focus(true);

	changing = 0;
	search_box = nullptr;
	_prop_edited = "property_edited";
	set_process(false);
	set_focus_mode(FocusMode::FOCUS_ALL);
	property_focusable = -1;
	property_clipboard = Variant();

	get_v_scroll_bar()->connect(SceneStringName(value_changed), callable_mp(this, &EditorInspector::_vscroll_changed));
	update_scroll_request = -1;
	if (EditorSettings::get_singleton()) {
		refresh_countdown = float(EDITOR_GET("docks/property_editor/auto_refresh_interval"));
	} else {
		//used when class is created by the docgen to dump default values of everything bindable, editorsettings may not be created
		refresh_countdown = 0.33;
	}

	ED_SHORTCUT("property_editor/copy_value", TTRC("Copy Value"), KeyModifierMask::CMD_OR_CTRL | Key::C);
	ED_SHORTCUT("property_editor/paste_value", TTRC("Paste Value"), KeyModifierMask::CMD_OR_CTRL | Key::V);
	ED_SHORTCUT("property_editor/copy_property_path", TTRC("Copy Property Path"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::C);

	// `use_settings_name_style` is true by default, set the name style accordingly.
	set_property_name_style(EditorPropertyNameProcessor::get_singleton()->get_settings_style());

	set_draw_focus_border(true);
}
