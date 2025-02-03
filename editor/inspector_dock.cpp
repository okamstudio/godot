/**************************************************************************/
/*  inspector_dock.cpp                                                    */
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

#include "inspector_dock.h"

#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/plugins/animation_player_editor_plugin.h"

void InspectorDock::_prepare_menu() {
	PopupMenu *menu = object_menu->get_popup();
	for (int i = EditorPropertyNameProcessor::STYLE_RAW; i <= EditorPropertyNameProcessor::STYLE_LOCALIZED; i++) {
		menu->set_item_checked(menu->get_item_index(PROPERTY_NAME_STYLE_RAW + i), i == property_name_style);
	}
}

void InspectorDock::_menu_option(int p_option) {
	switch (p_option) {
		case EXPAND_ALL: {
			_menu_expandall();
		} break;
		case COLLAPSE_ALL: {
			_menu_collapseall();
		} break;

		case RESOURCE_SAVE: {
			_save_resource(false);
		} break;
		case RESOURCE_SAVE_AS: {
			_save_resource(true);
		} break;

		case RESOURCE_MAKE_BUILT_IN: {
			_unref_resource();
		} break;
		case RESOURCE_COPY: {
			_copy_resource();
		} break;
		case RESOURCE_EDIT_CLIPBOARD: {
			_paste_resource();
		} break;

		case OBJECT_REQUEST_HELP: {
			if (current) {
				editor->set_visible_editor(EditorNode::EDITOR_SCRIPT);
				emit_signal("request_help", current->get_class());
			}
		} break;

		case OBJECT_COPY_PARAMS: {
			editor_data->apply_changes_in_editors();
			if (current) {
				editor_data->copy_object_params(current);
			}
		} break;

		case OBJECT_PASTE_PARAMS: {
			editor_data->apply_changes_in_editors();
			if (current) {
				editor_data->paste_object_params(current);
			}
		} break;

		case OBJECT_UNIQUE_RESOURCES: {
			editor_data->apply_changes_in_editors();
			if (current) {
				List<PropertyInfo> props;
				current->get_property_list(&props);
				Map<RES, RES> duplicates;
				for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
					if (!(E->get().usage & PROPERTY_USAGE_STORAGE)) {
						continue;
					}

					Variant v = current->get(E->get().name);
					if (v.is_ref()) {
						REF ref = v;
						if (ref.is_valid()) {
							RES res = ref;
							if (res.is_valid()) {
								if (!duplicates.has(res)) {
									duplicates[res] = res->duplicate();
								}
								res = duplicates[res];

								current->set(E->get().name, res);
								editor->get_inspector()->update_property(E->get().name);
							}
						}
					}
				}
			}

			editor_data->get_undo_redo().clear_history();

			editor->get_editor_plugins_over()->edit(nullptr);
			editor->get_editor_plugins_over()->edit(current);

		} break;

		case PROPERTY_NAME_STYLE_RAW:
		case PROPERTY_NAME_STYLE_CAPITALIZED:
		case PROPERTY_NAME_STYLE_LOCALIZED: {
			property_name_style = (EditorPropertyNameProcessor::Style)(p_option - PROPERTY_NAME_STYLE_RAW);
			inspector->set_property_name_style(property_name_style);
		} break;

		default: {
			if (p_option >= OBJECT_METHOD_BASE) {
				ERR_FAIL_COND(!current);

				int idx = p_option - OBJECT_METHOD_BASE;

				List<MethodInfo> methods;
				current->get_method_list(&methods);

				ERR_FAIL_INDEX(idx, methods.size());
				String name = methods[idx].name;

				current->call(name);
			}
		}
	}
}

void InspectorDock::_new_resource() {
	new_resource_dialog->popup_create(true);
}

void InspectorDock::_load_resource(const String &p_type) {
	load_resource_dialog->set_mode(EditorFileDialog::MODE_OPEN_FILE);

	List<String> extensions;
	ResourceLoader::get_recognized_extensions_for_type(p_type, &extensions);

	load_resource_dialog->clear_filters();
	for (int i = 0; i < extensions.size(); i++) {
		load_resource_dialog->add_filter("*." + extensions[i], extensions[i].to_upper());
	}

	load_resource_dialog->popup_centered_ratio();
}

void InspectorDock::_resource_file_selected(String p_file) {
	RES res = ResourceLoader::load(p_file);

	if (res.is_null()) {
		warning_dialog->set_text(TTR("Failed to load resource."));
		return;
	};

	editor->push_item(res.operator->());
}

void InspectorDock::_save_resource(bool save_as) const {
	uint32_t current = EditorNode::get_singleton()->get_editor_history()->get_current();
	Object *current_obj = current > 0 ? ObjectDB::get_instance(current) : nullptr;

	ERR_FAIL_COND(!Object::cast_to<Resource>(current_obj));

	RES current_res = RES(Object::cast_to<Resource>(current_obj));

	if (save_as) {
		editor->save_resource_as(current_res);
	} else {
		editor->save_resource(current_res);
	}
}

void InspectorDock::_unref_resource() const {
	uint32_t current = EditorNode::get_singleton()->get_editor_history()->get_current();
	Object *current_obj = current > 0 ? ObjectDB::get_instance(current) : nullptr;

	ERR_FAIL_COND(!Object::cast_to<Resource>(current_obj));

	RES current_res = RES(Object::cast_to<Resource>(current_obj));
	current_res->set_path("");
	editor->edit_current();
}

void InspectorDock::_copy_resource() const {
	uint32_t current = EditorNode::get_singleton()->get_editor_history()->get_current();
	Object *current_obj = current > 0 ? ObjectDB::get_instance(current) : nullptr;

	ERR_FAIL_COND(!Object::cast_to<Resource>(current_obj));

	RES current_res = RES(Object::cast_to<Resource>(current_obj));

	EditorSettings::get_singleton()->set_resource_clipboard(current_res);
}

void InspectorDock::_paste_resource() const {
	RES r = EditorSettings::get_singleton()->get_resource_clipboard();
	if (r.is_valid()) {
		editor->push_item(EditorSettings::get_singleton()->get_resource_clipboard().ptr(), String());
	}
}

void InspectorDock::_prepare_resource_extra_popup() {
	RES r = EditorSettings::get_singleton()->get_resource_clipboard();
	PopupMenu *popup = resource_extra_button->get_popup();
	popup->set_item_disabled(popup->get_item_index(RESOURCE_EDIT_CLIPBOARD), r.is_null());
}

void InspectorDock::_prepare_history() {
	EditorHistory *editor_history = EditorNode::get_singleton()->get_editor_history();

	int history_to = MAX(0, editor_history->get_history_len() - 25);

	history_menu->get_popup()->clear();

	Ref<Texture> base_icon = get_icon("Object", "EditorIcons");
	Set<ObjectID> already;
	for (int i = editor_history->get_history_len() - 1; i >= history_to; i--) {
		ObjectID id = editor_history->get_history_obj(i);
		Object *obj = ObjectDB::get_instance(id);
		if (!obj || already.has(id)) {
			if (history_to > 0) {
				history_to--;
			}
			continue;
		}

		already.insert(id);

		Ref<Texture> icon = EditorNode::get_singleton()->get_object_icon(obj, "");
		if (icon.is_null()) {
			icon = base_icon;
		}

		String text;
		if (Object::cast_to<Resource>(obj)) {
			Resource *r = Object::cast_to<Resource>(obj);
			if (r->get_path().is_resource_file()) {
				text = r->get_path().get_file();
			} else if (r->get_name() != String()) {
				text = r->get_name();
			} else {
				text = r->get_class();
			}
		} else if (Object::cast_to<Node>(obj)) {
			text = Object::cast_to<Node>(obj)->get_name();
		} else if (obj->is_class("ScriptEditorDebuggerInspectedObject")) {
			text = obj->call("get_title");
		} else {
			text = obj->get_class();
		}

		if (i == editor_history->get_history_pos() && current) {
			text = "[" + text + "]";
		}
		history_menu->get_popup()->add_icon_item(icon, text, i);
	}
}

void InspectorDock::_select_history(int p_idx) const {
	//push it to the top, it is not correct, but it's more useful
	ObjectID id = EditorNode::get_singleton()->get_editor_history()->get_history_obj(p_idx);
	Object *obj = ObjectDB::get_instance(id);
	if (!obj) {
		return;
	}
	editor->push_item(obj);
}

void InspectorDock::_resource_created() const {
	Variant c = new_resource_dialog->instance_selected();

	ERR_FAIL_COND(!c);
	Resource *r = Object::cast_to<Resource>(c);
	ERR_FAIL_COND(!r);

	editor->push_item(r);
}

void InspectorDock::_resource_selected(const RES &p_res, const String &p_property) const {
	if (p_res.is_null()) {
		return;
	}

	RES r = p_res;
	editor->push_item(r.operator->(), p_property);
}

void InspectorDock::_edit_forward() {
	if (EditorNode::get_singleton()->get_editor_history()->next()) {
		editor->edit_current();
	}
}
void InspectorDock::_edit_back() {
	EditorHistory *editor_history = EditorNode::get_singleton()->get_editor_history();
	if ((current && editor_history->previous()) || editor_history->get_path_size() == 1) {
		editor->edit_current();
	}
}

void InspectorDock::_menu_collapseall() {
	inspector->collapse_all_folding();
}

void InspectorDock::_menu_expandall() {
	inspector->expand_all_folding();
}

void InspectorDock::_property_keyed(const String &p_keyed, const Variant &p_value, bool p_advance) {
	AnimationPlayerEditor::singleton->get_track_editor()->insert_value_key(p_keyed, p_value, p_advance);
}

void InspectorDock::_transform_keyed(Object *sp, const String &p_sub, const Transform &p_key) {
	Spatial *s = Object::cast_to<Spatial>(sp);
	if (!s) {
		return;
	}
	AnimationPlayerEditor::singleton->get_track_editor()->insert_transform_key(s, p_sub, p_key);
}

void InspectorDock::_warning_pressed() {
	warning_dialog->popup_centered_minsize();
}

Container *InspectorDock::get_addon_area() {
	return this;
}

void InspectorDock::_notification(int p_what) {
	switch (p_what) {
		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			set_theme(editor->get_gui_base()->get_theme());

			resource_new_button->set_icon(get_icon("New", "EditorIcons"));
			resource_load_button->set_icon(get_icon("Load", "EditorIcons"));
			resource_save_button->set_icon(get_icon("Save", "EditorIcons"));
			resource_extra_button->set_icon(get_icon("GuiTabMenuHl", "EditorIcons"));

			PopupMenu *resource_extra_popup = resource_extra_button->get_popup();
			resource_extra_popup->set_item_icon(resource_extra_popup->get_item_index(RESOURCE_EDIT_CLIPBOARD), get_icon("ActionPaste", "EditorIcons"));
			resource_extra_popup->set_item_icon(resource_extra_popup->get_item_index(RESOURCE_COPY), get_icon("ActionCopy", "EditorIcons"));

			backward_button->set_icon(get_icon("Back", "EditorIcons"));
			forward_button->set_icon(get_icon("Forward", "EditorIcons"));

			history_menu->set_icon(get_icon("History", "EditorIcons"));
			object_menu->set_icon(get_icon("Tools", "EditorIcons"));
			warning->set_icon(get_icon("NodeWarning", "EditorIcons"));
			warning->add_color_override("font_color", get_color("warning_color", "Editor"));
		} break;
	}
}

void InspectorDock::_bind_methods() {
	ClassDB::bind_method("_prepare_menu", &InspectorDock::_prepare_menu);
	ClassDB::bind_method("_menu_option", &InspectorDock::_menu_option);

	ClassDB::bind_method("update_keying", &InspectorDock::update_keying);
	ClassDB::bind_method("_property_keyed", &InspectorDock::_property_keyed);
	ClassDB::bind_method("_transform_keyed", &InspectorDock::_transform_keyed);

	ClassDB::bind_method("_new_resource", &InspectorDock::_new_resource);
	ClassDB::bind_method("_resource_file_selected", &InspectorDock::_resource_file_selected);
	ClassDB::bind_method("_open_resource_selector", &InspectorDock::_open_resource_selector);
	ClassDB::bind_method("_unref_resource", &InspectorDock::_unref_resource);
	ClassDB::bind_method("_paste_resource", &InspectorDock::_paste_resource);
	ClassDB::bind_method("_copy_resource", &InspectorDock::_copy_resource);
	ClassDB::bind_method("_prepare_resource_extra_popup", &InspectorDock::_prepare_resource_extra_popup);

	ClassDB::bind_method("_select_history", &InspectorDock::_select_history);
	ClassDB::bind_method("_prepare_history", &InspectorDock::_prepare_history);
	ClassDB::bind_method("_resource_created", &InspectorDock::_resource_created);
	ClassDB::bind_method("_resource_selected", &InspectorDock::_resource_selected, DEFVAL(""));
	ClassDB::bind_method("_menu_collapseall", &InspectorDock::_menu_collapseall);
	ClassDB::bind_method("_menu_expandall", &InspectorDock::_menu_expandall);
	ClassDB::bind_method("_warning_pressed", &InspectorDock::_warning_pressed);
	ClassDB::bind_method("_edit_forward", &InspectorDock::_edit_forward);
	ClassDB::bind_method("_edit_back", &InspectorDock::_edit_back);

	ClassDB::bind_method("store_script_properties", &InspectorDock::store_script_properties);
	ClassDB::bind_method("apply_script_properties", &InspectorDock::apply_script_properties);

	ADD_SIGNAL(MethodInfo("request_help"));
}

void InspectorDock::edit_resource(const Ref<Resource> &p_resource) {
	_resource_selected(p_resource, "");
}

void InspectorDock::open_resource(const String &p_type) {
	_load_resource(p_type);
}

void InspectorDock::set_warning(const String &p_message) {
	warning->hide();
	if (p_message != String()) {
		warning->show();
		warning_dialog->set_text(p_message);
	}
}

void InspectorDock::clear() {
}

void InspectorDock::update(Object *p_object) {
	EditorHistory *editor_history = EditorNode::get_singleton()->get_editor_history();
	backward_button->set_disabled(editor_history->is_at_beginning());
	forward_button->set_disabled(editor_history->is_at_end());

	history_menu->set_disabled(true);
	if (editor_history->get_history_len() > 0) {
		history_menu->set_disabled(false);
	}
	editor_path->update_path();

	current = p_object;

	const bool is_object = p_object != nullptr;
	const bool is_resource = is_object && p_object->is_class("Resource");
	const bool is_node = is_object && p_object->is_class("Node");

	object_menu->set_disabled(!is_object);
	search->set_editable(is_object);
	resource_save_button->set_disabled(!is_resource);
	open_docs_button->set_disabled(!is_resource && !is_node);

	PopupMenu *resource_extra_popup = resource_extra_button->get_popup();
	resource_extra_popup->set_item_disabled(resource_extra_popup->get_item_index(RESOURCE_COPY), !is_resource);
	resource_extra_popup->set_item_disabled(resource_extra_popup->get_item_index(RESOURCE_MAKE_BUILT_IN), !is_resource);

	if (!is_object) {
		warning->hide();
		editor_path->clear_path();
		return;
	}

	editor_path->enable_path();

	PopupMenu *p = object_menu->get_popup();

	p->clear();
	p->add_icon_shortcut(get_icon("GuiTreeArrowDown", "EditorIcons"), ED_SHORTCUT("property_editor/expand_all", TTR("Expand All")), EXPAND_ALL);
	p->add_icon_shortcut(get_icon("GuiTreeArrowRight", "EditorIcons"), ED_SHORTCUT("property_editor/collapse_all", TTR("Collapse All")), COLLAPSE_ALL);

	p->add_separator(TTR("Property Name Style"));
	p->add_radio_check_item(TTR("Raw"), PROPERTY_NAME_STYLE_RAW);
	p->add_radio_check_item(TTR("Capitalized"), PROPERTY_NAME_STYLE_CAPITALIZED);
	p->add_radio_check_item(TTR("Localized"), PROPERTY_NAME_STYLE_LOCALIZED);

	if (!EditorPropertyNameProcessor::is_localization_available()) {
		const int index = p->get_item_index(PROPERTY_NAME_STYLE_LOCALIZED);
		p->set_item_disabled(index, true);
		p->set_item_tooltip(index, TTR("Localization not available for current language."));
	}

	p->add_separator();
	p->add_shortcut(ED_SHORTCUT("property_editor/copy_params", TTR("Copy Properties")), OBJECT_COPY_PARAMS);
	p->add_shortcut(ED_SHORTCUT("property_editor/paste_params", TTR("Paste Properties")), OBJECT_PASTE_PARAMS);

	if (is_resource || is_node) {
		p->add_separator();
		p->add_shortcut(ED_SHORTCUT("property_editor/make_subresources_unique", TTR("Make Sub-Resources Unique")), OBJECT_UNIQUE_RESOURCES);
	}

	List<MethodInfo> methods;
	p_object->get_method_list(&methods);

	if (!methods.empty()) {
		bool found = false;
		List<MethodInfo>::Element *I = methods.front();
		int i = 0;
		while (I) {
			if (I->get().flags & METHOD_FLAG_EDITOR) {
				if (!found) {
					p->add_separator();
					found = true;
				}
				p->add_item(I->get().name.capitalize(), OBJECT_METHOD_BASE + i);
			}
			i++;
			I = I->next();
		}
	}
}

void InspectorDock::go_back() {
	_edit_back();
}

void InspectorDock::update_keying() {
	bool valid = false;

	if (AnimationPlayerEditor::singleton->get_track_editor()->has_keying()) {
		EditorHistory *editor_history = EditorNode::get_singleton()->get_editor_history();
		if (editor_history->get_path_size() >= 1) {
			Object *obj = ObjectDB::get_instance(editor_history->get_path_object(0));
			if (Object::cast_to<Node>(obj)) {
				valid = true;
			}
		}
	}

	inspector->set_keying(valid);
}

EditorPropertyNameProcessor::Style InspectorDock::get_property_name_style() const {
	return property_name_style;
}

void InspectorDock::store_script_properties(Object *p_object) {
	ERR_FAIL_NULL(p_object);
	ScriptInstance *si = p_object->get_script_instance();
	if (!si) {
		return;
	}
	si->get_property_state(stored_properties);
}

void InspectorDock::apply_script_properties(Object *p_object) {
	ERR_FAIL_NULL(p_object);
	ScriptInstance *si = p_object->get_script_instance();
	if (!si) {
		return;
	}

	for (List<Pair<StringName, Variant>>::Element *E = stored_properties.front(); E; E = E->next()) {
		const Pair<StringName, Variant> &p = E->get();
		Variant current;
		if (si->get(p.first, current) && current.get_type() == p.second.get_type()) {
			si->set(p.first, p.second);
		}
	}
	stored_properties.clear();
}

InspectorDock::InspectorDock(EditorNode *p_editor, EditorData &p_editor_data) {
	set_name("Inspector");
	set_theme(p_editor->get_gui_base()->get_theme());

	editor = p_editor;
	editor_data = &p_editor_data;

	property_name_style = EditorPropertyNameProcessor::get_default_inspector_style();

	HBoxContainer *general_options_hb = memnew(HBoxContainer);
	add_child(general_options_hb);

	resource_new_button = memnew(ToolButton);
	resource_new_button->set_tooltip(TTR("Create a new resource in memory and edit it."));
	resource_new_button->set_icon(get_icon("New", "EditorIcons"));
	general_options_hb->add_child(resource_new_button);
	resource_new_button->connect("pressed", this, "_new_resource");
	resource_new_button->set_focus_mode(Control::FOCUS_NONE);

	resource_load_button = memnew(ToolButton);
	resource_load_button->set_tooltip(TTR("Load an existing resource from disk and edit it."));
	resource_load_button->set_icon(get_icon("Load", "EditorIcons"));
	general_options_hb->add_child(resource_load_button);
	resource_load_button->connect("pressed", this, "_open_resource_selector");
	resource_load_button->set_focus_mode(Control::FOCUS_NONE);

	resource_save_button = memnew(MenuButton);
	resource_save_button->set_tooltip(TTR("Save the currently edited resource."));
	resource_save_button->set_icon(get_icon("Save", "EditorIcons"));
	general_options_hb->add_child(resource_save_button);
	resource_save_button->get_popup()->add_item(TTR("Save"), RESOURCE_SAVE);
	resource_save_button->get_popup()->add_item(TTR("Save As..."), RESOURCE_SAVE_AS);
	resource_save_button->get_popup()->connect("id_pressed", this, "_menu_option");
	resource_save_button->set_focus_mode(Control::FOCUS_NONE);
	resource_save_button->set_disabled(true);

	resource_extra_button = memnew(MenuButton);
	resource_extra_button->set_icon(get_icon("GuiTabMenuHl", "EditorIcons"));
	resource_extra_button->set_tooltip(TTR("Extra resource options."));
	general_options_hb->add_child(resource_extra_button);
	resource_extra_button->connect("about_to_show", this, "_prepare_resource_extra_popup");
	resource_extra_button->get_popup()->add_icon_shortcut(get_icon("ActionPaste", "EditorIcons"), ED_SHORTCUT("property_editor/paste_resource", TTR("Edit Resource from Clipboard")), RESOURCE_EDIT_CLIPBOARD);
	resource_extra_button->get_popup()->add_icon_shortcut(get_icon("ActionCopy", "EditorIcons"), ED_SHORTCUT("property_editor/copy_resource", TTR("Copy Resource")), RESOURCE_COPY);
	resource_extra_button->get_popup()->set_item_disabled(1, true);
	resource_extra_button->get_popup()->add_separator();
	resource_extra_button->get_popup()->add_shortcut(ED_SHORTCUT("property_editor/unref_resource", TTR("Make Resource Built-In")), RESOURCE_MAKE_BUILT_IN);
	resource_extra_button->get_popup()->set_item_disabled(3, true);
	resource_extra_button->get_popup()->connect("id_pressed", this, "_menu_option");

	general_options_hb->add_spacer();

	backward_button = memnew(ToolButton);
	general_options_hb->add_child(backward_button);
	backward_button->set_icon(get_icon("Back", "EditorIcons"));
	backward_button->set_flat(true);
	backward_button->set_tooltip(TTR("Go to the previous edited object in history."));
	backward_button->set_disabled(true);
	backward_button->connect("pressed", this, "_edit_back");

	forward_button = memnew(ToolButton);
	general_options_hb->add_child(forward_button);
	forward_button->set_icon(get_icon("Forward", "EditorIcons"));
	forward_button->set_flat(true);
	forward_button->set_tooltip(TTR("Go to the next edited object in history."));
	forward_button->set_disabled(true);
	forward_button->connect("pressed", this, "_edit_forward");

	history_menu = memnew(MenuButton);
	history_menu->set_tooltip(TTR("History of recently edited objects."));
	history_menu->set_icon(get_icon("History", "EditorIcons"));
	general_options_hb->add_child(history_menu);
	history_menu->connect("about_to_show", this, "_prepare_history");
	history_menu->get_popup()->connect("id_pressed", this, "_select_history");

	HBoxContainer *subresource_hb = memnew(HBoxContainer);
	add_child(subresource_hb);
	editor_path = memnew(EditorPath(editor->get_editor_history()));
	editor_path->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	subresource_hb->add_child(editor_path);

	open_docs_button = memnew(Button);
	open_docs_button->set_flat(true);
	open_docs_button->set_disabled(true);
	open_docs_button->set_tooltip(TTR("Open documentation for this object."));
	open_docs_button->set_icon(get_icon("HelpSearch", "EditorIcons"));
	open_docs_button->set_shortcut(ED_SHORTCUT("property_editor/open_help", TTR("Open Documentation")));
	subresource_hb->add_child(open_docs_button);
	open_docs_button->connect("pressed", this, "_menu_option", varray(OBJECT_REQUEST_HELP));

	new_resource_dialog = memnew(CreateDialog);
	editor->get_gui_base()->add_child(new_resource_dialog);
	new_resource_dialog->set_base_type("Resource");
	new_resource_dialog->connect("create", this, "_resource_created");

	HBoxContainer *property_tools_hb = memnew(HBoxContainer);
	add_child(property_tools_hb);

	search = memnew(LineEdit);
	search->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	search->set_placeholder(TTR("Filter properties"));
	search->set_right_icon(get_icon("Search", "EditorIcons"));
	search->set_clear_button_enabled(true);
	property_tools_hb->add_child(search);

	object_menu = memnew(MenuButton);
	object_menu->set_icon(get_icon("Tools", "EditorIcons"));
	property_tools_hb->add_child(object_menu);
	object_menu->set_tooltip(TTR("Manage object properties."));
	object_menu->get_popup()->connect("about_to_show", this, "_prepare_menu");
	object_menu->get_popup()->connect("id_pressed", this, "_menu_option");

	warning = memnew(Button);
	add_child(warning);
	warning->set_text(TTR("Changes may be lost!"));
	warning->set_icon(get_icon("NodeWarning", "EditorIcons"));
	warning->add_color_override("font_color", get_color("warning_color", "Editor"));
	warning->set_clip_text(true);
	warning->hide();
	warning->connect("pressed", this, "_warning_pressed");

	warning_dialog = memnew(AcceptDialog);
	editor->get_gui_base()->add_child(warning_dialog);

	load_resource_dialog = memnew(EditorFileDialog);
	add_child(load_resource_dialog);
	load_resource_dialog->set_current_dir("res://");
	load_resource_dialog->connect("file_selected", this, "_resource_file_selected");

	inspector = memnew(EditorInspector);
	add_child(inspector);
	inspector->set_autoclear(true);
	inspector->set_show_categories(true);
	inspector->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	inspector->set_use_doc_hints(true);
	inspector->set_hide_script(false);
	inspector->set_property_name_style(EditorPropertyNameProcessor::get_default_inspector_style());
	inspector->set_use_folding(!bool(EDITOR_GET("interface/inspector/disable_folding")));
	inspector->register_text_enter(search);
	inspector->set_undo_redo(&editor_data->get_undo_redo());

	inspector->set_use_filter(true); // TODO: check me

	inspector->connect("resource_selected", this, "_resource_selected");
	inspector->connect("property_keyed", this, "_property_keyed");
}

InspectorDock::~InspectorDock() {
}
