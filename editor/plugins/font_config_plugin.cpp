/**************************************************************************/
/*  font_config_plugin.cpp                                                */
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

#include "font_config_plugin.h"

#include "core/string/translation_server.h"
#include "editor/editor_settings.h"
#include "editor/import/dynamic_font_import_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/margin_container.h"

/*************************************************************************/
/*  EditorPropertyFontMetaObject                                         */
/*************************************************************************/

bool EditorPropertyFontMetaObject::_set(const StringName &p_name, const Variant &p_value) {
	String name = p_name;

	if (name.begins_with("keys")) {
		String key = name.get_slicec('/', 1);
		dict[key] = p_value;
		return true;
	}

	return false;
}

bool EditorPropertyFontMetaObject::_get(const StringName &p_name, Variant &r_ret) const {
	String name = p_name;

	if (name.begins_with("keys")) {
		String key = name.get_slicec('/', 1);
		r_ret = dict[key];
		return true;
	}

	return false;
}

void EditorPropertyFontMetaObject::set_dict(const Dictionary &p_dict) {
	dict = p_dict;
}

Dictionary EditorPropertyFontMetaObject::get_dict() {
	return dict;
}

/*************************************************************************/
/*  EditorPropertyFontOTObject                                           */
/*************************************************************************/

bool EditorPropertyFontOTObject::_set(const StringName &p_name, const Variant &p_value) {
	String name = p_name;

	if (name.begins_with("keys")) {
		int key = name.get_slicec('/', 1).to_int();
		dict[key] = p_value;
		return true;
	}

	return false;
}

bool EditorPropertyFontOTObject::_get(const StringName &p_name, Variant &r_ret) const {
	String name = p_name;

	if (name.begins_with("keys")) {
		int key = name.get_slicec('/', 1).to_int();
		r_ret = dict[key];
		return true;
	}

	return false;
}

void EditorPropertyFontOTObject::set_dict(const Dictionary &p_dict) {
	dict = p_dict;
}

Dictionary EditorPropertyFontOTObject::get_dict() {
	return dict;
}

void EditorPropertyFontOTObject::set_defaults(const Dictionary &p_dict) {
	defaults_dict = p_dict;
}

Dictionary EditorPropertyFontOTObject::get_defaults() {
	return defaults_dict;
}

bool EditorPropertyFontOTObject::_property_can_revert(const StringName &p_name) const {
	String name = p_name;

	if (name.begins_with("keys")) {
		int key = name.get_slicec('/', 1).to_int();
		return defaults_dict.has(key) && dict.has(key);
	}
	return false;
}

bool EditorPropertyFontOTObject::_property_get_revert(const StringName &p_name, Variant &r_property) const {
	String name = p_name;

	if (name.begins_with("keys")) {
		int key = name.get_slicec('/', 1).to_int();
		if (defaults_dict.has(key)) {
			Vector3i range = defaults_dict[key];
			r_property = range.z;
			return true;
		}
	}
	return false;
}

/*************************************************************************/
/* EditorPropertyFontMetaOverride                                        */
/*************************************************************************/

void EditorPropertyFontMetaOverride::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			if (button_add) {
				button_add->set_button_icon(get_editor_theme_icon(SNAME("Add")));
			}
		} break;
	}
}

void EditorPropertyFontMetaOverride::_property_changed(const String &p_property, const Variant &p_value, const String &p_name, bool p_changing) {
	if (p_property.begins_with("keys")) {
		Dictionary dict = object->get_dict();
		String key = p_property.get_slicec('/', 1);
		dict[key] = (bool)p_value;

		emit_changed(get_edited_property(), dict, "", true);

		dict = dict.duplicate(); // Duplicate, so undo/redo works better.
		object->set_dict(dict);
	}
}

void EditorPropertyFontMetaOverride::_remove(Object *p_button, const String &p_key) {
	Dictionary dict = object->get_dict();

	dict.erase(p_key);

	emit_changed(get_edited_property(), dict, "", false);

	dict = dict.duplicate(); // Duplicate, so undo/redo works better.
	object->set_dict(dict);
	update_property();
}

void EditorPropertyFontMetaOverride::_add_menu() {
	if (script_editor) {
		Size2 size = get_size();
		menu->set_position(get_screen_position() + Size2(0, size.height * get_global_transform().get_scale().y));
		menu->reset_size();
		menu->popup();
	} else {
		locale_select->popup_locale_dialog();
	}
}

void EditorPropertyFontMetaOverride::_add_script(int p_option) {
	Dictionary dict = object->get_dict();

	dict[script_codes[p_option]] = true;

	emit_changed(get_edited_property(), dict, "", false);

	dict = dict.duplicate(); // Duplicate, so undo/redo works better.
	object->set_dict(dict);
	update_property();
}

void EditorPropertyFontMetaOverride::_add_lang(const String &p_locale) {
	Dictionary dict = object->get_dict();

	dict[p_locale] = true;

	emit_changed(get_edited_property(), dict, "", false);

	dict = dict.duplicate(); // Duplicate, so undo/redo works better.
	object->set_dict(dict);
	update_property();
}

void EditorPropertyFontMetaOverride::_object_id_selected(const StringName &p_property, ObjectID p_id) {
	emit_signal(SNAME("object_id_selected"), p_property, p_id);
}

void EditorPropertyFontMetaOverride::update_property() {
	Variant updated_val = get_edited_property_value();

	Dictionary dict = updated_val;

	edit->set_text(vformat(TTR("Overrides (%d)"), dict.size()));

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
			vbox->set_v_size_flags(SIZE_EXPAND_FILL);
			container->add_child(vbox);

			property_vbox = memnew(VBoxContainer);
			property_vbox->set_h_size_flags(SIZE_EXPAND_FILL);
			vbox->add_child(property_vbox);

			paginator = memnew(EditorPaginator);
			paginator->connect("page_changed", callable_mp(this, &EditorPropertyFontMetaOverride::_page_changed));
			vbox->add_child(paginator);
		} else {
			// Queue children for deletion, deleting immediately might cause errors.
			for (int i = property_vbox->get_child_count() - 1; i >= 0; i--) {
				property_vbox->get_child(i)->queue_free();
			}
			button_add = nullptr;
		}

		int size = dict.size();

		int max_page = MAX(0, size - 1) / page_length;
		page_index = MIN(page_index, max_page);

		paginator->update(page_index, max_page);
		paginator->set_visible(max_page > 0);

		int offset = page_index * page_length;

		int amount = MIN(size - offset, page_length);

		dict = dict.duplicate();
		object->set_dict(dict);

		for (int i = 0; i < amount; i++) {
			String name = dict.get_key_at_index(i);
			EditorProperty *prop = memnew(EditorPropertyCheck);
			prop->set_object_and_property(object.ptr(), "keys/" + name);

			if (script_editor) {
				prop->set_label(TranslationServer::get_singleton()->get_script_name(name));
			} else {
				prop->set_label(TranslationServer::get_singleton()->get_locale_name(name));
			}
			prop->set_tooltip_text(name);
			prop->set_selectable(false);

			prop->connect("property_changed", callable_mp(this, &EditorPropertyFontMetaOverride::_property_changed));
			prop->connect("object_id_selected", callable_mp(this, &EditorPropertyFontMetaOverride::_object_id_selected));

			HBoxContainer *hbox = memnew(HBoxContainer);
			property_vbox->add_child(hbox);
			hbox->add_child(prop);
			prop->set_h_size_flags(SIZE_EXPAND_FILL);
			Button *remove = memnew(Button);
			remove->set_button_icon(get_editor_theme_icon(SNAME("Remove")));
			hbox->add_child(remove);
			remove->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyFontMetaOverride::_remove).bind(remove, name));

			prop->update_property();
		}

		if (script_editor) {
			// TRANSLATORS: Script refers to a writing system.
			button_add = EditorInspector::create_inspector_action_button(TTR("Add Script", "Locale"));
		} else {
			button_add = EditorInspector::create_inspector_action_button(TTR("Add Locale"));
		}
		button_add->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyFontMetaOverride::_add_menu));
		property_vbox->add_child(button_add);

		updating = false;
	} else {
		if (container) {
			set_bottom_editor(nullptr);
			memdelete(container);
			button_add = nullptr;
			container = nullptr;
		}
	}
}

void EditorPropertyFontMetaOverride::_edit_pressed() {
	Variant prop_val = get_edited_property_value();
	if (prop_val.get_type() == Variant::NIL) {
		Callable::CallError ce;
		Variant::construct(Variant::DICTIONARY, prop_val, nullptr, 0, ce);
		get_edited_object()->set(get_edited_property(), prop_val);
	}

	get_edited_object()->editor_set_section_unfold(get_edited_property(), edit->is_pressed());
	update_property();
}

void EditorPropertyFontMetaOverride::_page_changed(int p_page) {
	if (updating) {
		return;
	}
	page_index = p_page;
	update_property();
}

EditorPropertyFontMetaOverride::EditorPropertyFontMetaOverride(bool p_script) {
	script_editor = p_script;

	object.instantiate();
	page_length = int(EDITOR_GET("interface/inspector/max_array_dictionary_items_per_page"));

	edit = memnew(Button);
	edit->set_h_size_flags(SIZE_EXPAND_FILL);
	edit->set_clip_text(true);
	edit->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyFontMetaOverride::_edit_pressed));
	edit->set_toggle_mode(true);
	add_child(edit);
	add_focusable(edit);

	menu = memnew(PopupMenu);
	if (script_editor) {
		script_codes = TranslationServer::get_singleton()->get_all_scripts();
		for (int i = 0; i < script_codes.size(); i++) {
			menu->add_item(TranslationServer::get_singleton()->get_script_name(script_codes[i]) + " (" + script_codes[i] + ")", i);
		}
	}
	add_child(menu);
	menu->connect(SceneStringName(id_pressed), callable_mp(this, &EditorPropertyFontMetaOverride::_add_script));

	locale_select = memnew(EditorLocaleDialog);
	locale_select->connect("locale_selected", callable_mp(this, &EditorPropertyFontMetaOverride::_add_lang));
	add_child(locale_select);
}

/*************************************************************************/
/* EditorPropertyOTVariation                                             */
/*************************************************************************/

void EditorPropertyOTVariation::_property_changed(const String &p_property, const Variant &p_value, const String &p_name, bool p_changing) {
	if (p_property.begins_with("keys")) {
		Dictionary dict = object->get_dict();
		int key = p_property.get_slicec('/', 1).to_int();
		dict[key] = (int)p_value;

		emit_changed(get_edited_property(), dict, "", true);

		dict = dict.duplicate(); // Duplicate, so undo/redo works better.
		object->set_dict(dict);
	}
}

void EditorPropertyOTVariation::_object_id_selected(const StringName &p_property, ObjectID p_id) {
	emit_signal(SNAME("object_id_selected"), p_property, p_id);
}

void EditorPropertyOTVariation::update_property() {
	Variant updated_val = get_edited_property_value();

	Dictionary dict = updated_val;

	Ref<Font> fd;
	if (Object::cast_to<Font>(get_edited_object()) != nullptr) {
		fd = get_edited_object();
	} else if (Object::cast_to<DynamicFontImportSettingsData>(get_edited_object()) != nullptr) {
		Ref<DynamicFontImportSettingsData> imp = Object::cast_to<DynamicFontImportSettingsData>(get_edited_object());
		fd = imp->get_font();
	}

	Dictionary supported = (fd.is_valid()) ? fd->get_supported_variation_list() : Dictionary();

	for (int i = 0; i < supported.size(); i++) {
		int name_tag = supported.get_key_at_index(i);
		Vector3i range = supported.get_value_at_index(i);
		if ((dict.has(name_tag) && dict[name_tag].get_type() == Variant::NIL) || !dict.has(name_tag)) {
			dict[name_tag] = range.z;
		}
	}

	edit->set_text(vformat(TTR("Variation Coordinates (%d)"), supported.size()));

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
			vbox->set_v_size_flags(SIZE_EXPAND_FILL);
			container->add_child(vbox);

			property_vbox = memnew(VBoxContainer);
			property_vbox->set_h_size_flags(SIZE_EXPAND_FILL);
			vbox->add_child(property_vbox);

			paginator = memnew(EditorPaginator);
			paginator->connect("page_changed", callable_mp(this, &EditorPropertyOTVariation::_page_changed));
			vbox->add_child(paginator);
		} else {
			// Queue children for deletion, deleting immediately might cause errors.
			for (int i = property_vbox->get_child_count() - 1; i >= 0; i--) {
				property_vbox->get_child(i)->queue_free();
			}
		}

		int size = supported.size();

		int max_page = MAX(0, size - 1) / page_length;
		page_index = MIN(page_index, max_page);

		paginator->update(page_index, max_page);
		paginator->set_visible(max_page > 0);

		int offset = page_index * page_length;

		int amount = MIN(size - offset, page_length);

		dict = dict.duplicate();
		object->set_dict(dict);
		object->set_defaults(supported);

		for (int i = 0; i < amount; i++) {
			int name_tag = supported.get_key_at_index(i);
			Vector3i range = supported.get_value_at_index(i);

			EditorPropertyInteger *prop = memnew(EditorPropertyInteger);
			prop->setup(range.x, range.y, false, true, false, false);
			prop->set_object_and_property(object.ptr(), "keys/" + itos(name_tag));

			String name = TS->tag_to_name(name_tag);
			String name_cap;
			{
				String aux = name.replace("_", " ").strip_edges();
				for (int j = 0; j < aux.get_slice_count(" "); j++) {
					String slice = aux.get_slicec(' ', j);
					if (slice.length() > 0) {
						slice[0] = String::char_uppercase(slice[0]);
						if (i > 0) {
							name_cap += " ";
						}
						name_cap += slice;
					}
				}
			}
			prop->set_label(name_cap);
			prop->set_tooltip_text(name);
			prop->set_selectable(false);

			prop->connect("property_changed", callable_mp(this, &EditorPropertyOTVariation::_property_changed));
			prop->connect("object_id_selected", callable_mp(this, &EditorPropertyOTVariation::_object_id_selected));

			property_vbox->add_child(prop);

			prop->update_property();
		}

		updating = false;
	} else {
		if (container) {
			set_bottom_editor(nullptr);
			memdelete(container);
			container = nullptr;
		}
	}
}

void EditorPropertyOTVariation::_edit_pressed() {
	Variant prop_val = get_edited_property_value();
	if (prop_val.get_type() == Variant::NIL) {
		Callable::CallError ce;
		Variant::construct(Variant::DICTIONARY, prop_val, nullptr, 0, ce);
		get_edited_object()->set(get_edited_property(), prop_val);
	}

	get_edited_object()->editor_set_section_unfold(get_edited_property(), edit->is_pressed());
	update_property();
}

void EditorPropertyOTVariation::_page_changed(int p_page) {
	if (updating) {
		return;
	}
	page_index = p_page;
	update_property();
}

EditorPropertyOTVariation::EditorPropertyOTVariation() {
	object.instantiate();
	page_length = int(EDITOR_GET("interface/inspector/max_array_dictionary_items_per_page"));

	edit = memnew(Button);
	edit->set_h_size_flags(SIZE_EXPAND_FILL);
	edit->set_clip_text(true);
	edit->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyOTVariation::_edit_pressed));
	edit->set_toggle_mode(true);
	add_child(edit);
	add_focusable(edit);
}

/*************************************************************************/
/* EditorPropertyOTFeatures                                              */
/*************************************************************************/

void EditorPropertyOTFeatures::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			if (button_add) {
				button_add->set_button_icon(get_editor_theme_icon(SNAME("Add")));
			}
		} break;
	}
}

void EditorPropertyOTFeatures::_property_changed(const String &p_property, const Variant &p_value, const String &p_name, bool p_changing) {
	if (p_property.begins_with("keys")) {
		Dictionary dict = object->get_dict();
		int key = p_property.get_slicec('/', 1).to_int();
		dict[key] = (int)p_value;

		emit_changed(get_edited_property(), dict, "", true);

		dict = dict.duplicate(); // Duplicate, so undo/redo works better.
		object->set_dict(dict);
	}
}

void EditorPropertyOTFeatures::_remove(Object *p_button, int p_key) {
	Dictionary dict = object->get_dict();

	dict.erase(p_key);

	emit_changed(get_edited_property(), dict, "", false);

	dict = dict.duplicate(); // Duplicate, so undo/redo works better.
	object->set_dict(dict);
	update_property();
}

void EditorPropertyOTFeatures::_add_menu() {
	Size2 size = get_size();
	menu->set_position(get_screen_position() + Size2(0, size.height * get_global_transform().get_scale().y));
	menu->reset_size();
	menu->popup();
}

void EditorPropertyOTFeatures::_add_feature(int p_option) {
	Dictionary dict = object->get_dict();

	dict[p_option] = 1;

	emit_changed(get_edited_property(), dict, "", false);

	dict = dict.duplicate(); // Duplicate, so undo/redo works better.
	object->set_dict(dict);
	update_property();
}

void EditorPropertyOTFeatures::_object_id_selected(const StringName &p_property, ObjectID p_id) {
	emit_signal(SNAME("object_id_selected"), p_property, p_id);
}

void EditorPropertyOTFeatures::update_property() {
	Variant updated_val = get_edited_property_value();

	Dictionary dict = updated_val;

	Ref<Font> fd;
	if (Object::cast_to<FontVariation>(get_edited_object()) != nullptr) {
		fd = get_edited_object();
	} else if (Object::cast_to<DynamicFontImportSettingsData>(get_edited_object()) != nullptr) {
		Ref<DynamicFontImportSettingsData> imp = Object::cast_to<DynamicFontImportSettingsData>(get_edited_object());
		fd = imp->get_font();
	}

	Dictionary supported;
	if (fd.is_valid()) {
		supported = fd->get_supported_feature_list();
	}

	if (supported.is_empty()) {
		edit->set_text(vformat(TTR("No supported features")));
		if (container) {
			set_bottom_editor(nullptr);
			memdelete(container);
			button_add = nullptr;
			container = nullptr;
		}
		return;
	}
	edit->set_text(vformat(TTR("Features (%d of %d set)"), dict.size(), supported.size()));

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
			vbox->set_v_size_flags(SIZE_EXPAND_FILL);
			container->add_child(vbox);

			property_vbox = memnew(VBoxContainer);
			property_vbox->set_h_size_flags(SIZE_EXPAND_FILL);
			vbox->add_child(property_vbox);

			paginator = memnew(EditorPaginator);
			paginator->connect("page_changed", callable_mp(this, &EditorPropertyOTFeatures::_page_changed));
			vbox->add_child(paginator);
		} else {
			// Queue children for deletion, deleting immediately might cause errors.
			for (int i = property_vbox->get_child_count() - 1; i >= 0; i--) {
				property_vbox->get_child(i)->queue_free();
			}
			button_add = nullptr;
		}

		// Update add menu items.
		menu->clear(false);
		bool have_sub[FGRP_MAX];
		for (int i = 0; i < FGRP_MAX; i++) {
			menu_sub[i]->clear();
			have_sub[i] = false;
		}

		bool show_hidden = EDITOR_GET("interface/inspector/show_low_level_opentype_features");

		for (int i = 0; i < supported.size(); i++) {
			int name_tag = supported.get_key_at_index(i);
			Dictionary info = supported.get_value_at_index(i);
			bool hidden = info["hidden"].operator bool();
			String name = TS->tag_to_name(name_tag);
			FeatureGroups grp = FGRP_MAX;

			if (hidden && !show_hidden) {
				continue;
			}

			if (name.begins_with("stylistic_set_")) {
				grp = FGRP_STYLISTIC_SET;
			} else if (name.begins_with("character_variant_")) {
				grp = FGRP_CHARACTER_VARIANT;
			} else if (name.ends_with("_capitals")) {
				grp = FGRP_CAPITLS;
			} else if (name.ends_with("_ligatures")) {
				grp = FGRP_LIGATURES;
			} else if (name.ends_with("_alternates")) {
				grp = FGRP_ALTERNATES;
			} else if (name.ends_with("_kanji_forms") || name.begins_with("jis") || name == "simplified_forms" || name == "traditional_name_forms" || name == "traditional_forms") {
				grp = FGRP_EAL;
			} else if (name.ends_with("_widths")) {
				grp = FGRP_EAW;
			} else if (name == "tabular_figures" || name == "proportional_figures") {
				grp = FGRP_NUMAL;
			} else if (name.begins_with("custom_")) {
				grp = FGRP_CUSTOM;
			}
			String disp_name = name.capitalize();
			if (info.has("label")) {
				disp_name = vformat("%s (%s)", disp_name, info["label"].operator String());
			}

			if (grp == FGRP_MAX) {
				menu->add_item(disp_name, name_tag);
			} else {
				menu_sub[grp]->add_item(disp_name, name_tag);
				have_sub[grp] = true;
			}
		}
		for (int i = 0; i < FGRP_MAX; i++) {
			if (have_sub[i]) {
				menu->add_submenu_node_item(TTRGET(group_names[i]), menu_sub[i]);
			}
		}

		int size = dict.size();

		int max_page = MAX(0, size - 1) / page_length;
		page_index = MIN(page_index, max_page);

		paginator->update(page_index, max_page);
		paginator->set_visible(max_page > 0);

		int offset = page_index * page_length;

		int amount = MIN(size - offset, page_length);

		dict = dict.duplicate();
		object->set_dict(dict);

		for (int i = 0; i < amount; i++) {
			int name_tag = dict.get_key_at_index(i);

			if (supported.has(name_tag)) {
				Dictionary info = supported[name_tag];
				Variant::Type vtype = Variant::Type(info["type"].operator int());
				bool hidden = info["hidden"].operator bool();
				if (hidden && !show_hidden) {
					continue;
				}

				EditorProperty *prop = nullptr;
				switch (vtype) {
					case Variant::NIL: {
						prop = memnew(EditorPropertyNil);
					} break;
					case Variant::BOOL: {
						prop = memnew(EditorPropertyCheck);
					} break;
					case Variant::INT: {
						EditorPropertyInteger *editor = memnew(EditorPropertyInteger);
						editor->setup(0, 255, 1, false, false, false);
						prop = editor;
					} break;
					default: {
						ERR_CONTINUE_MSG(true, vformat("Unsupported OT feature data type %s", Variant::get_type_name(vtype)));
					}
				}
				prop->set_object_and_property(object.ptr(), "keys/" + itos(name_tag));

				String name = TS->tag_to_name(name_tag);
				String disp_name = name.capitalize();
				if (info.has("label")) {
					disp_name = vformat("%s (%s)", disp_name, info["label"].operator String());
				}
				prop->set_label(disp_name);
				prop->set_tooltip_text(name);
				prop->set_selectable(false);

				prop->connect("property_changed", callable_mp(this, &EditorPropertyOTFeatures::_property_changed));
				prop->connect("object_id_selected", callable_mp(this, &EditorPropertyOTFeatures::_object_id_selected));

				HBoxContainer *hbox = memnew(HBoxContainer);
				property_vbox->add_child(hbox);
				hbox->add_child(prop);
				prop->set_h_size_flags(SIZE_EXPAND_FILL);
				Button *remove = memnew(Button);
				remove->set_button_icon(get_editor_theme_icon(SNAME("Remove")));
				hbox->add_child(remove);
				remove->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyOTFeatures::_remove).bind(remove, name_tag));

				prop->update_property();
			}
		}

		button_add = EditorInspector::create_inspector_action_button(TTR("Add Feature"));
		button_add->set_button_icon(get_editor_theme_icon(SNAME("Add")));
		button_add->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyOTFeatures::_add_menu));
		property_vbox->add_child(button_add);

		updating = false;
	} else {
		if (container) {
			set_bottom_editor(nullptr);
			memdelete(container);
			button_add = nullptr;
			container = nullptr;
		}
	}
}

void EditorPropertyOTFeatures::_edit_pressed() {
	Variant prop_val = get_edited_property_value();
	if (prop_val.get_type() == Variant::NIL) {
		Callable::CallError ce;
		Variant::construct(Variant::DICTIONARY, prop_val, nullptr, 0, ce);
		get_edited_object()->set(get_edited_property(), prop_val);
	}

	get_edited_object()->editor_set_section_unfold(get_edited_property(), edit->is_pressed());
	update_property();
}

void EditorPropertyOTFeatures::_page_changed(int p_page) {
	if (updating) {
		return;
	}
	page_index = p_page;
	update_property();
}

EditorPropertyOTFeatures::EditorPropertyOTFeatures() {
	object.instantiate();
	page_length = int(EDITOR_GET("interface/inspector/max_array_dictionary_items_per_page"));

	edit = memnew(Button);
	edit->set_h_size_flags(SIZE_EXPAND_FILL);
	edit->set_clip_text(true);
	edit->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyOTFeatures::_edit_pressed));
	edit->set_toggle_mode(true);
	add_child(edit);
	add_focusable(edit);

	menu = memnew(PopupMenu);
	add_child(menu);
	menu->connect(SceneStringName(id_pressed), callable_mp(this, &EditorPropertyOTFeatures::_add_feature));

	for (int i = 0; i < FGRP_MAX; i++) {
		menu_sub[i] = memnew(PopupMenu);
		menu->add_child(menu_sub[i]);
		menu_sub[i]->connect(SceneStringName(id_pressed), callable_mp(this, &EditorPropertyOTFeatures::_add_feature));
	}

	group_names[FGRP_STYLISTIC_SET] = TTRC("Stylistic Sets");
	group_names[FGRP_CHARACTER_VARIANT] = TTRC("Character Variants");
	group_names[FGRP_CAPITLS] = TTRC("Capitals");
	group_names[FGRP_LIGATURES] = TTRC("Ligatures");
	group_names[FGRP_ALTERNATES] = TTRC("Alternates");
	group_names[FGRP_EAL] = TTRC("East Asian Language");
	group_names[FGRP_EAW] = TTRC("East Asian Widths");
	group_names[FGRP_NUMAL] = TTRC("Numeral Alignment");
	group_names[FGRP_CUSTOM] = TTRC("Custom");
}

/*************************************************************************/
/* EditorInspectorPluginFontVariation                                    */
/*************************************************************************/

bool EditorInspectorPluginFontVariation::can_handle(Object *p_object) {
	return (Object::cast_to<FontVariation>(p_object) != nullptr) || (Object::cast_to<DynamicFontImportSettingsData>(p_object) != nullptr);
}

bool EditorInspectorPluginFontVariation::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	if (p_path == "variation_opentype") {
		add_property_editor(p_path, memnew(EditorPropertyOTVariation));
		return true;
	} else if (p_path == "opentype_features") {
		add_property_editor(p_path, memnew(EditorPropertyOTFeatures));
		return true;
	} else if (p_path == "language_support") {
		add_property_editor(p_path, memnew(EditorPropertyFontMetaOverride(false)));
		return true;
	} else if (p_path == "script_support") {
		add_property_editor(p_path, memnew(EditorPropertyFontMetaOverride(true)));
		return true;
	}
	return false;
}

/*************************************************************************/
/* FontPreview                                                           */
/*************************************************************************/

void FontPreview::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			// Draw font name (style).
			Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
			int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
			Color text_color = get_theme_color(SceneStringName(font_color), SNAME("Label"));

			// Draw font preview.
			bool prev_ok = true;
			if (prev_font.is_valid()) {
				if (prev_font->get_font_name().is_empty()) {
					prev_ok = false;
				} else {
					String name;
					if (prev_font->get_font_style_name().is_empty()) {
						name = prev_font->get_font_name();
					} else {
						name = vformat("%s (%s)", prev_font->get_font_name(), prev_font->get_font_style_name());
					}
					if (prev_font->is_class("FontVariation")) {
						// TRANSLATORS: This refers to variable font config, appended to the font name.
						name += " - " + TTR("Variation");
					}
					font->draw_string(get_canvas_item(), Point2(0, font->get_height(font_size) + 2 * EDSCALE), name, HORIZONTAL_ALIGNMENT_CENTER, get_size().x, font_size, text_color);

					String sample;
					static const String sample_base = U"12漢字ԱբΑαАбΑαאבابܐܒހށआআਆઆଆஆఆಆആආกิກິༀကႠა한글ሀᎣᐁᚁᚠᜀᜠᝀᝠកᠠᤁᥐAb😀";
					for (int i = 0; i < sample_base.length(); i++) {
						if (prev_font->has_char(sample_base[i])) {
							sample += sample_base[i];
						}
					}
					if (sample.is_empty()) {
						sample = prev_font->get_supported_chars().substr(0, 6);
					}
					if (sample.is_empty()) {
						prev_ok = false;
					} else {
						prev_font->draw_string(get_canvas_item(), Point2(0, font->get_height(font_size) + prev_font->get_height(25 * EDSCALE)), sample, HORIZONTAL_ALIGNMENT_CENTER, get_size().x, 25 * EDSCALE, text_color);
					}
				}
			}
			if (!prev_ok) {
				text_color.a *= 0.5;
				font->draw_string(get_canvas_item(), Point2(0, font->get_height(font_size) + 2 * EDSCALE), TTR("Unable to preview font"), HORIZONTAL_ALIGNMENT_CENTER, get_size().x, font_size, text_color);
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (prev_font.is_valid()) {
				prev_font->disconnect_changed(callable_mp(this, &FontPreview::_preview_changed));
			}
		} break;
	}
}

void FontPreview::_bind_methods() {}

Size2 FontPreview::get_minimum_size() const {
	return Vector2(64, 64) * EDSCALE;
}

void FontPreview::set_data(const Ref<Font> &p_f) {
	if (prev_font.is_valid()) {
		prev_font->disconnect_changed(callable_mp(this, &FontPreview::_preview_changed));
	}
	prev_font = p_f;
	if (prev_font.is_valid()) {
		prev_font->connect_changed(callable_mp(this, &FontPreview::_preview_changed));
	}
	queue_redraw();
}

void FontPreview::_preview_changed() {
	queue_redraw();
}

FontPreview::FontPreview() {
}

/*************************************************************************/
/* EditorInspectorPluginFontPreview                                      */
/*************************************************************************/

bool EditorInspectorPluginFontPreview::can_handle(Object *p_object) {
	return Object::cast_to<Font>(p_object) != nullptr;
}

void EditorInspectorPluginFontPreview::parse_begin(Object *p_object) {
	Font *fd = Object::cast_to<Font>(p_object);
	ERR_FAIL_NULL(fd);

	FontPreview *editor = memnew(FontPreview);
	editor->set_data(fd);
	add_custom_control(editor);
}

bool EditorInspectorPluginFontPreview::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	return false;
}

/*************************************************************************/
/* EditorPropertyFontNamesArray                                          */
/*************************************************************************/

void EditorPropertyFontNamesArray::_add_element() {
	Size2 size = get_size();
	menu->set_position(get_screen_position() + Size2(0, size.height * get_global_transform().get_scale().y));
	menu->reset_size();
	menu->popup();
}

void EditorPropertyFontNamesArray::_add_font(int p_option) {
	if (updating) {
		return;
	}

	Variant array = object->get_array();
	int previous_size = array.call("size");

	array.call("resize", previous_size + 1);
	array.set(previous_size, menu->get_item_text(p_option));

	emit_changed(get_edited_property(), array, "", false);
	object->set_array(array);
	update_property();
}

EditorPropertyFontNamesArray::EditorPropertyFontNamesArray() {
	menu = memnew(PopupMenu);
	menu->add_item("Sans-Serif", 0);
	menu->add_item("Serif", 1);
	menu->add_item("Monospace", 2);
	menu->add_item("Fantasy", 3);
	menu->add_item("Cursive", 4);

	menu->add_separator();

	if (OS::get_singleton()) {
		Vector<String> fonts = OS::get_singleton()->get_system_fonts();
		fonts.sort();
		for (int i = 0; i < fonts.size(); i++) {
			menu->add_item(fonts[i], i + 6);
		}
	}
	add_child(menu);
	menu->connect(SceneStringName(id_pressed), callable_mp(this, &EditorPropertyFontNamesArray::_add_font));
}

/*************************************************************************/
/* EditorInspectorPluginSystemFont                                       */
/*************************************************************************/

bool EditorInspectorPluginSystemFont::can_handle(Object *p_object) {
	return Object::cast_to<SystemFont>(p_object) != nullptr;
}

bool EditorInspectorPluginSystemFont::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	if (p_path == "font_names") {
		EditorPropertyFontNamesArray *editor = memnew(EditorPropertyFontNamesArray);
		editor->setup(p_type, p_hint_text);
		add_property_editor(p_path, editor);
		return true;
	}
	return false;
}

/*************************************************************************/
/* FontEditorPlugin                                                */
/*************************************************************************/

FontEditorPlugin::FontEditorPlugin() {
	Ref<EditorInspectorPluginFontVariation> fc_plugin;
	fc_plugin.instantiate();
	EditorInspector::add_inspector_plugin(fc_plugin);

	Ref<EditorInspectorPluginSystemFont> fs_plugin;
	fs_plugin.instantiate();
	EditorInspector::add_inspector_plugin(fs_plugin);

	Ref<EditorInspectorPluginFontPreview> fp_plugin;
	fp_plugin.instantiate();
	EditorInspector::add_inspector_plugin(fp_plugin);
}
