/**************************************************************************/
/*  property_selector.cpp                                                 */
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

#include "property_selector.h"

#include "editor/editor_help.h"
#include "editor/editor_node.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/tree.h"

void PropertySelector::_text_changed(const String &p_newtext) {
	_update_search();
}

void PropertySelector::_sbox_input(const Ref<InputEvent> &p_event) {
	// Redirect navigational key events to the tree.
	Ref<InputEventKey> key = p_event;
	if (key.is_valid()) {
		if (key->is_action("ui_up", true) || key->is_action("ui_down", true) || key->is_action("ui_page_up") || key->is_action("ui_page_down")) {
			search_options->gui_input(key);
			search_box->accept_event();

			TreeItem *root = search_options->get_root();
			if (!root->get_first_child()) {
				return;
			}

			TreeItem *current = search_options->get_selected();

			TreeItem *item = search_options->get_next_selected(root);
			while (item) {
				item->deselect(0);
				item = search_options->get_next_selected(item);
			}

			current->select(0);
		}
	}
}

void PropertySelector::_update_search() {
	if (properties) {
		set_title(TTR("Select Property"));
	} else if (virtuals_only) {
		set_title(TTR("Select Virtual Method"));
	} else {
		set_title(TTR("Select Method"));
	}

	search_options->clear();
	help_bit->set_custom_text(String(), String(), String());

	TreeItem *root = search_options->create_item();

	// Allow using spaces in place of underscores in the search string (makes the search more fault-tolerant).
	const String search_text = search_box->get_text().replace(" ", "_");

	if (properties) {
		List<PropertyInfo> props;

		if (instance) {
			instance->get_property_list(&props, true);
		} else if (type != Variant::NIL) {
			Variant v;
			Callable::CallError ce;
			Variant::construct(type, v, nullptr, 0, ce);

			v.get_property_list(&props);
		} else {
			Object *obj = ObjectDB::get_instance(script);
			if (Object::cast_to<Script>(obj)) {
				props.push_back(PropertyInfo(Variant::NIL, "Script Variables", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_CATEGORY));
				Object::cast_to<Script>(obj)->get_script_property_list(&props);
			}

			StringName base = base_type;
			while (base) {
				props.push_back(PropertyInfo(Variant::NIL, base, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_CATEGORY));
				ClassDB::get_property_list(base, &props, true);
				base = ClassDB::get_parent_class(base);
			}
		}

		TreeItem *category = nullptr;

		bool found = false;
		for (const PropertyInfo &E : props) {
			if (E.usage == PROPERTY_USAGE_CATEGORY) {
				if (category && category->get_first_child() == nullptr) {
					memdelete(category); //old category was unused
				}
				category = search_options->create_item(root);
				category->set_text(0, E.name);
				category->set_selectable(0, false);

				Ref<Texture2D> icon;
				if (E.name == "Script Variables") {
					icon = search_options->get_editor_theme_icon(SNAME("Script"));
				} else {
					icon = EditorNode::get_singleton()->get_class_icon(E.name);
				}
				category->set_icon(0, icon);
				continue;
			}

			if (!(E.usage & PROPERTY_USAGE_EDITOR) && !(E.usage & PROPERTY_USAGE_SCRIPT_VARIABLE)) {
				continue;
			}

			if (!search_box->get_text().is_empty() && !E.name.containsn(search_text)) {
				continue;
			}

			if (!type_filter.is_empty() && !type_filter.has(E.type)) {
				continue;
			}

			TreeItem *item = search_options->create_item(category ? category : root);
			item->set_text(0, E.name);
			item->set_metadata(0, E.name);
			item->set_icon(0, search_options->get_editor_theme_icon(Variant::get_type_name(E.type)));

			if (!found && !search_box->get_text().is_empty() && E.name.containsn(search_text)) {
				item->select(0);
				found = true;
			} else if (!found && search_box->get_text().is_empty() && E.name == selected) {
				item->select(0);
				found = true;
			}

			item->set_selectable(0, true);

			_create_subproperties(item, E.type);
			item->set_collapsed(true);
		}

		if (category && category->get_first_child() == nullptr) {
			memdelete(category); //old category was unused
		}

		if (found) {
			// As we call this while adding items, defer until list is completely populated.
			callable_mp(search_options, &Tree::scroll_to_item).call_deferred(search_options->get_selected(), true);
		}

	} else {
		List<MethodInfo> methods;

		if (type != Variant::NIL) {
			Variant v;
			Callable::CallError ce;
			Variant::construct(type, v, nullptr, 0, ce);
			v.get_method_list(&methods);
		} else {
			Ref<Script> script_ref = Object::cast_to<Script>(ObjectDB::get_instance(script));
			if (script_ref.is_valid()) {
				if (script_ref->is_built_in()) {
					script_ref->reload(true);
				}

				List<MethodInfo> script_methods;
				script_ref->get_script_method_list(&script_methods);

				methods.push_back(MethodInfo("*Script Methods")); // TODO: Split by inheritance.

				for (const MethodInfo &mi : script_methods) {
					if (mi.name.begins_with("@")) {
						// GH-92782. GDScript inline setters/getters are historically present in `get_method_list()`
						// and can be called using `Object.call()`. However, these functions are meant to be internal
						// and their names are not valid identifiers, so let's hide them from the user.
						continue;
					}
					methods.push_back(mi);
				}
			}

			StringName base = base_type;
			while (base) {
				methods.push_back(MethodInfo("*" + String(base)));
				ClassDB::get_method_list(base, &methods, true, true);
				base = ClassDB::get_parent_class(base);
			}
		}

		TreeItem *category = nullptr;

		bool found = false;
		bool script_methods = false;

		for (MethodInfo &mi : methods) {
			if (mi.name.begins_with("*")) {
				if (category && category->get_first_child() == nullptr) {
					memdelete(category); //old category was unused
				}
				category = search_options->create_item(root);
				category->set_text(0, mi.name.replace_first("*", ""));
				category->set_selectable(0, false);

				Ref<Texture2D> icon;
				script_methods = false;
				String rep = mi.name.replace("*", "");
				if (mi.name == "*Script Methods") {
					icon = search_options->get_editor_theme_icon(SNAME("Script"));
					script_methods = true;
				} else {
					icon = EditorNode::get_singleton()->get_class_icon(rep);
				}
				category->set_icon(0, icon);

				continue;
			}

			String name = mi.name.get_slicec(':', 0);
			if (!script_methods && name.begins_with("_") && !(mi.flags & METHOD_FLAG_VIRTUAL)) {
				continue;
			}

			if (virtuals_only && !(mi.flags & METHOD_FLAG_VIRTUAL)) {
				continue;
			}

			if (!virtuals_only && (mi.flags & METHOD_FLAG_VIRTUAL)) {
				continue;
			}

			if (!search_box->get_text().is_empty() && !name.containsn(search_text)) {
				continue;
			}

			TreeItem *item = search_options->create_item(category ? category : root);

			String desc;
			if (mi.name.contains_char(':')) {
				desc = mi.name.get_slicec(':', 1) + " ";
				mi.name = mi.name.get_slicec(':', 0);
			} else if (mi.return_val.type != Variant::NIL) {
				desc = Variant::get_type_name(mi.return_val.type);
			} else {
				desc = "void";
			}

			desc += vformat(" %s(", mi.name);

			for (List<PropertyInfo>::Iterator arg_itr = mi.arguments.begin(); arg_itr != mi.arguments.end(); ++arg_itr) {
				if (arg_itr != mi.arguments.begin()) {
					desc += ", ";
				}

				desc += arg_itr->name;

				if (arg_itr->type == Variant::NIL) {
					desc += ": Variant";
				} else if (arg_itr->name.contains_char(':')) {
					desc += vformat(": %s", arg_itr->name.get_slicec(':', 1));
					arg_itr->name = arg_itr->name.get_slicec(':', 0);
				} else {
					desc += vformat(": %s", Variant::get_type_name(arg_itr->type));
				}
			}

			desc += ")";

			if (mi.flags & METHOD_FLAG_CONST) {
				desc += " const";
			}

			if (mi.flags & METHOD_FLAG_VIRTUAL) {
				desc += " virtual";
			}

			item->set_text(0, desc);
			item->set_metadata(0, name);
			item->set_selectable(0, true);

			if (!found && !search_box->get_text().is_empty() && name.containsn(search_text)) {
				item->select(0);
				found = true;
			} else if (!found && search_box->get_text().is_empty() && name == selected) {
				item->select(0);
				found = true;
			}
		}

		if (category && category->get_first_child() == nullptr) {
			memdelete(category); //old category was unused
		}

		if (found) {
			// As we call this while adding items, defer until list is completely populated.
			callable_mp(search_options, &Tree::scroll_to_item).call_deferred(search_options->get_selected(), true);
		}
	}

	get_ok_button()->set_disabled(search_options->get_selected() == nullptr);
}

void PropertySelector::_confirmed() {
	TreeItem *ti = search_options->get_selected();
	if (!ti) {
		return;
	}
	emit_signal(SNAME("selected"), ti->get_metadata(0));
	hide();
}

void PropertySelector::_item_selected() {
	help_bit->set_custom_text(String(), String(), String());

	TreeItem *item = search_options->get_selected();
	get_ok_button()->set_disabled(item == nullptr);

	if (!item) {
		return;
	}
	String name = item->get_metadata(0);

	String class_type;
	if (type != Variant::NIL) {
		class_type = Variant::get_type_name(type);
	} else if (!base_type.is_empty()) {
		class_type = base_type;
	} else if (instance) {
		class_type = instance->get_class();
	}

	String text;
	while (!class_type.is_empty()) {
		if (properties) {
			if (ClassDB::has_property(class_type, name, true)) {
				help_bit->parse_symbol("property|" + class_type + "|" + name);
				break;
			}
		} else {
			if (ClassDB::has_method(class_type, name, true)) {
				help_bit->parse_symbol("method|" + class_type + "|" + name);
				break;
			}
		}

		// It may be from a parent class, keep looking.
		class_type = ClassDB::get_parent_class(class_type);
	}
}

void PropertySelector::_hide_requested() {
	_cancel_pressed(); // From AcceptDialog.
}

void PropertySelector::_create_subproperties(TreeItem *p_parent_item, Variant::Type p_type) {
	switch (p_type) {
		case Variant::VECTOR2: {
			_create_subproperty(p_parent_item, "x", Variant::FLOAT);
			_create_subproperty(p_parent_item, "y", Variant::FLOAT);
		} break;

		case Variant::VECTOR2I: {
			_create_subproperty(p_parent_item, "x", Variant::INT);
			_create_subproperty(p_parent_item, "y", Variant::INT);
		} break;

		case Variant::RECT2: {
			_create_subproperty(p_parent_item, "position", Variant::VECTOR2);
			_create_subproperty(p_parent_item, "size", Variant::VECTOR2);
			_create_subproperty(p_parent_item, "end", Variant::VECTOR2);
		} break;

		case Variant::RECT2I: {
			_create_subproperty(p_parent_item, "position", Variant::VECTOR2I);
			_create_subproperty(p_parent_item, "size", Variant::VECTOR2I);
			_create_subproperty(p_parent_item, "end", Variant::VECTOR2I);
		} break;

		case Variant::VECTOR3: {
			_create_subproperty(p_parent_item, "x", Variant::FLOAT);
			_create_subproperty(p_parent_item, "y", Variant::FLOAT);
			_create_subproperty(p_parent_item, "z", Variant::FLOAT);
		} break;

		case Variant::VECTOR3I: {
			_create_subproperty(p_parent_item, "x", Variant::INT);
			_create_subproperty(p_parent_item, "y", Variant::INT);
			_create_subproperty(p_parent_item, "z", Variant::INT);
		} break;

		case Variant::TRANSFORM2D: {
			_create_subproperty(p_parent_item, "origin", Variant::VECTOR2);
			_create_subproperty(p_parent_item, "x", Variant::VECTOR2);
			_create_subproperty(p_parent_item, "y", Variant::VECTOR2);
		} break;

		case Variant::VECTOR4: {
			_create_subproperty(p_parent_item, "x", Variant::FLOAT);
			_create_subproperty(p_parent_item, "y", Variant::FLOAT);
			_create_subproperty(p_parent_item, "z", Variant::FLOAT);
			_create_subproperty(p_parent_item, "w", Variant::FLOAT);
		} break;

		case Variant::VECTOR4I: {
			_create_subproperty(p_parent_item, "x", Variant::INT);
			_create_subproperty(p_parent_item, "y", Variant::INT);
			_create_subproperty(p_parent_item, "z", Variant::INT);
			_create_subproperty(p_parent_item, "w", Variant::INT);
		} break;

		case Variant::PLANE: {
			_create_subproperty(p_parent_item, "x", Variant::FLOAT);
			_create_subproperty(p_parent_item, "y", Variant::FLOAT);
			_create_subproperty(p_parent_item, "z", Variant::FLOAT);
			_create_subproperty(p_parent_item, "normal", Variant::VECTOR3);
			_create_subproperty(p_parent_item, "d", Variant::FLOAT);
		} break;

		case Variant::QUATERNION: {
			_create_subproperty(p_parent_item, "x", Variant::FLOAT);
			_create_subproperty(p_parent_item, "y", Variant::FLOAT);
			_create_subproperty(p_parent_item, "z", Variant::FLOAT);
			_create_subproperty(p_parent_item, "w", Variant::FLOAT);
		} break;

		case Variant::AABB: {
			_create_subproperty(p_parent_item, "position", Variant::VECTOR3);
			_create_subproperty(p_parent_item, "size", Variant::VECTOR3);
			_create_subproperty(p_parent_item, "end", Variant::VECTOR3);
		} break;

		case Variant::BASIS: {
			_create_subproperty(p_parent_item, "x", Variant::VECTOR3);
			_create_subproperty(p_parent_item, "y", Variant::VECTOR3);
			_create_subproperty(p_parent_item, "z", Variant::VECTOR3);
		} break;

		case Variant::TRANSFORM3D: {
			_create_subproperty(p_parent_item, "basis", Variant::BASIS);
			_create_subproperty(p_parent_item, "origin", Variant::VECTOR3);
		} break;

		case Variant::PROJECTION: {
			_create_subproperty(p_parent_item, "x", Variant::VECTOR4);
			_create_subproperty(p_parent_item, "y", Variant::VECTOR4);
			_create_subproperty(p_parent_item, "z", Variant::VECTOR4);
			_create_subproperty(p_parent_item, "w", Variant::VECTOR4);
		} break;

		case Variant::COLOR: {
			_create_subproperty(p_parent_item, "r", Variant::FLOAT);
			_create_subproperty(p_parent_item, "g", Variant::FLOAT);
			_create_subproperty(p_parent_item, "b", Variant::FLOAT);
			_create_subproperty(p_parent_item, "a", Variant::FLOAT);
			_create_subproperty(p_parent_item, "r8", Variant::INT);
			_create_subproperty(p_parent_item, "g8", Variant::INT);
			_create_subproperty(p_parent_item, "b8", Variant::INT);
			_create_subproperty(p_parent_item, "a8", Variant::INT);
			_create_subproperty(p_parent_item, "h", Variant::FLOAT);
			_create_subproperty(p_parent_item, "s", Variant::FLOAT);
			_create_subproperty(p_parent_item, "v", Variant::FLOAT);
		} break;

		default: {
		}
	}
}

void PropertySelector::_create_subproperty(TreeItem *p_parent_item, const String &p_name, Variant::Type p_type) {
	if (!type_filter.is_empty() && !type_filter.has(p_type)) {
		return;
	}

	TreeItem *item = search_options->create_item(p_parent_item);
	item->set_text(0, p_name);
	item->set_metadata(0, String(p_parent_item->get_metadata(0)) + ":" + p_name);
	item->set_icon(0, search_options->get_editor_theme_icon(Variant::get_type_name(p_type)));

	_create_subproperties(item, p_type);
}

void PropertySelector::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			connect(SceneStringName(confirmed), callable_mp(this, &PropertySelector::_confirmed));
		} break;

		case NOTIFICATION_EXIT_TREE: {
			disconnect(SceneStringName(confirmed), callable_mp(this, &PropertySelector::_confirmed));
		} break;
	}
}

void PropertySelector::select_method_from_base_type(const String &p_base, const String &p_current, bool p_virtuals_only) {
	base_type = p_base;
	selected = p_current;
	type = Variant::NIL;
	script = ObjectID();
	properties = false;
	instance = nullptr;
	virtuals_only = p_virtuals_only;

	popup_centered_ratio(0.6);
	search_box->set_text("");
	search_box->grab_focus();
	_update_search();
}

void PropertySelector::select_method_from_script(const Ref<Script> &p_script, const String &p_current) {
	ERR_FAIL_COND(p_script.is_null());
	base_type = p_script->get_instance_base_type();
	selected = p_current;
	type = Variant::NIL;
	script = p_script->get_instance_id();
	properties = false;
	instance = nullptr;
	virtuals_only = false;

	popup_centered_ratio(0.6);
	search_box->set_text("");
	search_box->grab_focus();
	_update_search();
}

void PropertySelector::select_method_from_basic_type(Variant::Type p_type, const String &p_current) {
	ERR_FAIL_COND(p_type == Variant::NIL);
	base_type = "";
	selected = p_current;
	type = p_type;
	script = ObjectID();
	properties = false;
	instance = nullptr;
	virtuals_only = false;

	popup_centered_ratio(0.6);
	search_box->set_text("");
	search_box->grab_focus();
	_update_search();
}

void PropertySelector::select_method_from_instance(Object *p_instance, const String &p_current) {
	base_type = p_instance->get_class();
	selected = p_current;
	type = Variant::NIL;
	script = ObjectID();
	{
		Ref<Script> scr = p_instance->get_script();
		if (scr.is_valid()) {
			script = scr->get_instance_id();
		}
	}
	properties = false;
	instance = nullptr;
	virtuals_only = false;

	popup_centered_ratio(0.6);
	search_box->set_text("");
	search_box->grab_focus();
	_update_search();
}

void PropertySelector::select_property_from_base_type(const String &p_base, const String &p_current) {
	base_type = p_base;
	selected = p_current;
	type = Variant::NIL;
	script = ObjectID();
	properties = true;
	instance = nullptr;
	virtuals_only = false;

	popup_centered_ratio(0.6);
	search_box->set_text("");
	search_box->grab_focus();
	_update_search();
}

void PropertySelector::select_property_from_script(const Ref<Script> &p_script, const String &p_current) {
	ERR_FAIL_COND(p_script.is_null());

	base_type = p_script->get_instance_base_type();
	selected = p_current;
	type = Variant::NIL;
	script = p_script->get_instance_id();
	properties = true;
	instance = nullptr;
	virtuals_only = false;

	popup_centered_ratio(0.6);
	search_box->set_text("");
	search_box->grab_focus();
	_update_search();
}

void PropertySelector::select_property_from_basic_type(Variant::Type p_type, const String &p_current) {
	ERR_FAIL_COND(p_type == Variant::NIL);
	base_type = "";
	selected = p_current;
	type = p_type;
	script = ObjectID();
	properties = true;
	instance = nullptr;
	virtuals_only = false;

	popup_centered_ratio(0.6);
	search_box->set_text("");
	search_box->grab_focus();
	_update_search();
}

void PropertySelector::select_property_from_instance(Object *p_instance, const String &p_current) {
	base_type = "";
	selected = p_current;
	type = Variant::NIL;
	script = ObjectID();
	properties = true;
	instance = p_instance;
	virtuals_only = false;

	popup_centered_ratio(0.6);
	search_box->set_text("");
	search_box->grab_focus();
	_update_search();
}

void PropertySelector::set_type_filter(const Vector<Variant::Type> &p_type_filter) {
	type_filter = p_type_filter;
}

void PropertySelector::_bind_methods() {
	ADD_SIGNAL(MethodInfo("selected", PropertyInfo(Variant::STRING, "name")));
}

PropertySelector::PropertySelector() {
	VBoxContainer *vbc = memnew(VBoxContainer);
	add_child(vbc);
	//set_child_rect(vbc);
	search_box = memnew(LineEdit);
	vbc->add_margin_child(TTR("Search:"), search_box);
	search_box->connect(SceneStringName(text_changed), callable_mp(this, &PropertySelector::_text_changed));
	search_box->connect(SceneStringName(gui_input), callable_mp(this, &PropertySelector::_sbox_input));
	search_options = memnew(Tree);
	search_options->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	vbc->add_margin_child(TTR("Matches:"), search_options, true);
	set_ok_button_text(TTR("Open"));
	get_ok_button()->set_disabled(true);
	register_text_enter(search_box);
	set_hide_on_ok(false);
	search_options->connect("item_activated", callable_mp(this, &PropertySelector::_confirmed));
	search_options->connect("cell_selected", callable_mp(this, &PropertySelector::_item_selected));
	search_options->set_hide_root(true);

	help_bit = memnew(EditorHelpBit);
	help_bit->set_content_height_limits(80 * EDSCALE, 80 * EDSCALE);
	help_bit->connect("request_hide", callable_mp(this, &PropertySelector::_hide_requested));
	vbc->add_margin_child(TTR("Description:"), help_bit);
}
