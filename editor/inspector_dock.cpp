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

#include "editor/editor_main_screen.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/filesystem_dock.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/gui/editor_object_selector.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor/themes/editor_scale.h"

InspectorDock *InspectorDock::singleton = nullptr;

void InspectorDock::_prepare_menu() {
	PopupMenu *menu = object_menu->get_popup();
	for (int i = EditorPropertyNameProcessor::STYLE_RAW; i <= EditorPropertyNameProcessor::STYLE_LOCALIZED; i++) {
		menu->set_item_checked(menu->get_item_index(PROPERTY_NAME_STYLE_RAW + i), i == property_name_style);
	}
}

void InspectorDock::_menu_option(int p_option) {
	_menu_option_confirm(p_option, false);
}

void InspectorDock::_menu_confirm_current() {
	_menu_option_confirm(current_option, true);
}

void InspectorDock::_menu_option_confirm(int p_option, bool p_confirmed) {
	if (!p_confirmed) {
		current_option = p_option;
	}

	switch (p_option) {
		case EXPAND_ALL: {
			_menu_expandall();
		} break;
		case COLLAPSE_ALL: {
			_menu_collapseall();
		} break;
		case EXPAND_REVERTABLE: {
			_menu_expand_revertable();
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
		case RESOURCE_SHOW_IN_FILESYSTEM: {
			Ref<Resource> current_res = _get_current_resource();
			ERR_FAIL_COND(current_res.is_null());
			FileSystemDock::get_singleton()->navigate_to_path(current_res->get_path());
		} break;

		case OBJECT_REQUEST_HELP: {
			if (current) {
				EditorNode::get_singleton()->get_editor_main_screen()->select(EditorMainScreen::EDITOR_SCRIPT);
				emit_signal(SNAME("request_help"), current->get_class());
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
			if (!p_confirmed) {
				Vector<String> resource_propnames;

				if (current) {
					List<PropertyInfo> props;
					current->get_property_list(&props);

					for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
						if (!(E->get().usage & PROPERTY_USAGE_STORAGE)) {
							continue;
						}

						Variant v = current->get(E->get().name);
						Ref<RefCounted> ref = v;
						Ref<Resource> res = ref;
						if (v.is_ref_counted() && ref.is_valid() && res.is_valid()) {
							// Valid resource which would be duplicated if action is confirmed.
							resource_propnames.append(E->get().name);
						}
					}
				}

				unique_resources_list_tree->clear();
				if (resource_propnames.size()) {
					const EditorPropertyNameProcessor::Style name_style = inspector->get_property_name_style();

					TreeItem *root = unique_resources_list_tree->create_item();
					for (const String &E : resource_propnames) {
						const String propname = EditorPropertyNameProcessor::get_singleton()->process_name(E, name_style);

						TreeItem *ti = unique_resources_list_tree->create_item(root);
						ti->set_text(0, propname);
					}

					unique_resources_label->set_text(TTR("The following resources will be duplicated and embedded within this resource/object."));
					unique_resources_confirmation->popup_centered();
				} else {
					current_option = -1;
					unique_resources_label->set_text(TTR("This object has no resources."));
					unique_resources_confirmation->popup_centered();
				}
			} else {
				editor_data->apply_changes_in_editors();

				if (current) {
					List<PropertyInfo> props;
					current->get_property_list(&props);
					HashMap<Ref<Resource>, Ref<Resource>> duplicates;
					for (const PropertyInfo &prop_info : props) {
						if (!(prop_info.usage & PROPERTY_USAGE_STORAGE)) {
							continue;
						}

						Variant v = current->get(prop_info.name);
						if (v.is_ref_counted()) {
							Ref<RefCounted> ref = v;
							if (ref.is_valid()) {
								Ref<Resource> res = ref;
								if (res.is_valid()) {
									if (!duplicates.has(res)) {
										duplicates[res] = res->duplicate();
									}
									res = duplicates[res];

									current->set(prop_info.name, res);
									get_inspector_singleton()->update_property(prop_info.name);
								}
							}
						}
					}
				}

				int history_id = EditorUndoRedoManager::get_singleton()->get_history_id_for_object(current);
				EditorUndoRedoManager::get_singleton()->clear_history(history_id);

				EditorNode::get_singleton()->edit_item(current, inspector);
			}

		} break;

		case PROPERTY_NAME_STYLE_RAW:
		case PROPERTY_NAME_STYLE_CAPITALIZED:
		case PROPERTY_NAME_STYLE_LOCALIZED: {
			property_name_style = (EditorPropertyNameProcessor::Style)(p_option - PROPERTY_NAME_STYLE_RAW);
			inspector->set_property_name_style(property_name_style);
		} break;

		default: {
			if (p_option >= OBJECT_METHOD_BASE) {
				ERR_FAIL_NULL(current);

				int idx = p_option - OBJECT_METHOD_BASE;

				List<MethodInfo> methods;
				current->get_method_list(&methods);

				ERR_FAIL_INDEX(idx, methods.size());
				String name = methods.get(idx).name;

				current->call(name);
			}
		}
	}
}

void InspectorDock::_new_resource() {
	new_resource_dialog->popup_create(true);
}

void InspectorDock::_load_resource(const String &p_type) {
	load_resource_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);

	List<String> extensions;
	ResourceLoader::get_recognized_extensions_for_type(p_type, &extensions);

	load_resource_dialog->clear_filters();
	for (const String &extension : extensions) {
		load_resource_dialog->add_filter("*." + extension, extension.to_upper());
	}

	const Vector<String> textfile_ext = ((String)(EDITOR_GET("docks/filesystem/textfile_extensions"))).split(",", false);
	for (int i = 0; i < textfile_ext.size(); i++) {
		load_resource_dialog->add_filter("*." + textfile_ext[i], textfile_ext[i].to_upper());
	}

	load_resource_dialog->popup_file_dialog();
}

void InspectorDock::_resource_file_selected(const String &p_file) {
	Ref<Resource> res;
	if (ResourceLoader::exists(p_file, "")) {
		res = ResourceLoader::load(p_file);
	} else {
		const Vector<String> textfile_ext = ((String)(EDITOR_GET("docks/filesystem/textfile_extensions"))).split(",", false);
		if (textfile_ext.has(p_file.get_extension())) {
			res = ScriptEditor::get_singleton()->open_file(p_file);
		}
	}

	if (res.is_null()) {
		info_dialog->set_text(TTR("Failed to load resource."));
		return;
	};

	EditorNode::get_singleton()->push_item(res.operator->());
}

void InspectorDock::_save_resource(bool save_as) {
	Ref<Resource> current_res = _get_current_resource();
	ERR_FAIL_COND(current_res.is_null());

	if (save_as) {
		EditorNode::get_singleton()->save_resource_as(current_res);
	} else {
		EditorNode::get_singleton()->save_resource(current_res);
	}
}

void InspectorDock::_unref_resource() {
	Ref<Resource> current_res = _get_current_resource();
	ERR_FAIL_COND(current_res.is_null());
	current_res->set_path("");
	EditorNode::get_singleton()->edit_current();
}

void InspectorDock::_copy_resource() {
	Ref<Resource> current_res = _get_current_resource();
	ERR_FAIL_COND(current_res.is_null());
	EditorSettings::get_singleton()->set_resource_clipboard(current_res);
}

void InspectorDock::_paste_resource() {
	Ref<Resource> r = EditorSettings::get_singleton()->get_resource_clipboard();
	if (r.is_valid()) {
		EditorNode::get_singleton()->push_item(EditorSettings::get_singleton()->get_resource_clipboard().ptr(), String());
	}
}

void InspectorDock::_prepare_resource_extra_popup() {
	Ref<Resource> r = EditorSettings::get_singleton()->get_resource_clipboard();
	PopupMenu *popup = resource_extra_button->get_popup();
	popup->set_item_disabled(popup->get_item_index(RESOURCE_EDIT_CLIPBOARD), r.is_null());

	Ref<Resource> current_res = _get_current_resource();
	popup->set_item_disabled(popup->get_item_index(RESOURCE_SHOW_IN_FILESYSTEM), current_res.is_null() || current_res->is_built_in());
}

Ref<Resource> InspectorDock::_get_current_resource() const {
	ObjectID current_id = EditorNode::get_singleton()->get_editor_selection_history()->get_current();
	Object *current_obj = current_id.is_valid() ? ObjectDB::get_instance(current_id) : nullptr;
	return Ref<Resource>(Object::cast_to<Resource>(current_obj));
}

void InspectorDock::_prepare_history() {
	EditorSelectionHistory *editor_history = EditorNode::get_singleton()->get_editor_selection_history();

	int history_to = MAX(0, editor_history->get_history_len() - 25);

	history_menu->get_popup()->clear();

	HashSet<ObjectID> already;
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

		Ref<Texture2D> icon = EditorNode::get_singleton()->get_object_icon(obj, "Object");

		String text;
		if (obj->has_method("_get_editor_name")) {
			text = obj->call("_get_editor_name");
		} else if (Object::cast_to<Resource>(obj)) {
			Resource *r = Object::cast_to<Resource>(obj);
			if (r->get_path().is_resource_file()) {
				text = r->get_path().get_file();
			} else if (!r->get_name().is_empty()) {
				text = r->get_name();
			} else {
				text = r->get_class();
			}
		} else if (Object::cast_to<Node>(obj)) {
			text = Object::cast_to<Node>(obj)->get_name();
		} else if (obj->is_class("EditorDebuggerRemoteObject")) {
			text = obj->call("get_title");
		} else {
			text = obj->get_class();
		}

		if (i == editor_history->get_history_pos() && current) {
			text += " " + TTR("(Current)");
		}
		history_menu->get_popup()->add_icon_item(icon, text, i);
	}
}

void InspectorDock::_select_history(int p_idx) {
	// Push it to the top, it is not correct, but it's more useful.
	ObjectID id = EditorNode::get_singleton()->get_editor_selection_history()->get_history_obj(p_idx);
	Object *obj = ObjectDB::get_instance(id);
	if (!obj) {
		return;
	}
	EditorNode::get_singleton()->push_item(obj);
}

void InspectorDock::_resource_created() {
	Variant c = new_resource_dialog->instantiate_selected();

	ERR_FAIL_COND(!c);
	Resource *r = Object::cast_to<Resource>(c);
	ERR_FAIL_NULL(r);

	EditorNode::get_singleton()->push_item(r);
}

void InspectorDock::_resource_selected(const Ref<Resource> &p_res, const String &p_property) {
	if (p_res.is_null()) {
		return;
	}

	Ref<Resource> r = p_res;
	EditorNode::get_singleton()->push_item(r.operator->(), p_property);
}

void InspectorDock::_edit_forward() {
	if (EditorNode::get_singleton()->get_editor_selection_history()->next()) {
		EditorNode::get_singleton()->edit_current();
	}
}

void InspectorDock::_edit_back() {
	EditorSelectionHistory *editor_history = EditorNode::get_singleton()->get_editor_selection_history();
	if ((current && editor_history->previous()) || editor_history->get_path_size() == 1) {
		EditorNode::get_singleton()->edit_current();
	}
}

void InspectorDock::_menu_collapseall() {
	inspector->collapse_all_folding();
}

void InspectorDock::_menu_expandall() {
	inspector->expand_all_folding();
}

void InspectorDock::_menu_expand_revertable() {
	inspector->expand_revertable();
}

void InspectorDock::_info_pressed() {
	info_dialog->popup_centered();
}

Container *InspectorDock::get_addon_area() {
	return this;
}

void InspectorDock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED:
		case NOTIFICATION_TRANSLATION_CHANGED:
		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED: {
			resource_new_button->set_button_icon(get_editor_theme_icon(SNAME("New")));
			resource_load_button->set_button_icon(get_editor_theme_icon(SNAME("Load")));
			resource_save_button->set_button_icon(get_editor_theme_icon(SNAME("Save")));
			resource_extra_button->set_button_icon(get_editor_theme_icon(SNAME("GuiTabMenuHl")));
			open_docs_button->set_button_icon(get_editor_theme_icon(SNAME("HelpSearch")));

			PopupMenu *resource_extra_popup = resource_extra_button->get_popup();
			resource_extra_popup->set_item_icon(resource_extra_popup->get_item_index(RESOURCE_EDIT_CLIPBOARD), get_editor_theme_icon(SNAME("ActionPaste")));
			resource_extra_popup->set_item_icon(resource_extra_popup->get_item_index(RESOURCE_COPY), get_editor_theme_icon(SNAME("ActionCopy")));
			resource_extra_popup->set_item_icon(resource_extra_popup->get_item_index(RESOURCE_SHOW_IN_FILESYSTEM), get_editor_theme_icon(SNAME("ShowInFileSystem")));

			if (is_layout_rtl()) {
				backward_button->set_button_icon(get_editor_theme_icon(SNAME("Forward")));
				forward_button->set_button_icon(get_editor_theme_icon(SNAME("Back")));
			} else {
				backward_button->set_button_icon(get_editor_theme_icon(SNAME("Back")));
				forward_button->set_button_icon(get_editor_theme_icon(SNAME("Forward")));
			}

			const int icon_width = get_theme_constant(SNAME("class_icon_size"), EditorStringName(Editor));
			history_menu->get_popup()->add_theme_constant_override("icon_max_width", icon_width);

			history_menu->set_button_icon(get_editor_theme_icon(SNAME("History")));
			object_menu->set_button_icon(get_editor_theme_icon(SNAME("Tools")));
			search->set_right_icon(get_editor_theme_icon(SNAME("Search")));
			if (info_is_warning) {
				info->set_button_icon(get_editor_theme_icon(SNAME("NodeWarning")));
				info->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
			} else {
				info->set_button_icon(get_editor_theme_icon(SNAME("NodeInfo")));
				info->add_theme_color_override(SceneStringName(font_color), get_theme_color(SceneStringName(font_color), EditorStringName(Editor)));
			}
		} break;
	}
}

void InspectorDock::_bind_methods() {
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

void InspectorDock::set_info(const String &p_button_text, const String &p_message, bool p_is_warning) {
	info->hide();
	info_is_warning = p_is_warning;

	if (info_is_warning) {
		info->set_button_icon(get_editor_theme_icon(SNAME("NodeWarning")));
		info->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
	} else {
		info->set_button_icon(get_editor_theme_icon(SNAME("NodeInfo")));
		info->add_theme_color_override(SceneStringName(font_color), get_theme_color(SceneStringName(font_color), EditorStringName(Editor)));
	}

	if (!p_button_text.is_empty() && !p_message.is_empty()) {
		info->show();
		info->set_text(p_button_text);
		info_dialog->set_text(p_message);
	}
}

void InspectorDock::clear() {
}

void InspectorDock::update(Object *p_object) {
	EditorSelectionHistory *editor_history = EditorNode::get_singleton()->get_editor_selection_history();
	backward_button->set_disabled(editor_history->is_at_beginning());
	forward_button->set_disabled(editor_history->is_at_end());

	history_menu->set_disabled(true);
	if (editor_history->get_history_len() > 0) {
		history_menu->set_disabled(false);
	}
	object_selector->update_path();

	current = p_object;

	const bool is_object = p_object != nullptr;
	const bool is_resource = is_object && p_object->is_class("Resource");
	const bool is_text_file = is_object && p_object->is_class("TextFile");
	const bool is_node = is_object && p_object->is_class("Node");

	object_menu->set_disabled(!is_object || is_text_file);
	search->set_editable(is_object && !is_text_file);
	resource_save_button->set_disabled(!is_resource || is_text_file);
	open_docs_button->set_disabled(is_text_file || (!is_resource && !is_node));

	PopupMenu *resource_extra_popup = resource_extra_button->get_popup();
	resource_extra_popup->set_item_disabled(resource_extra_popup->get_item_index(RESOURCE_COPY), !is_resource || is_text_file);
	resource_extra_popup->set_item_disabled(resource_extra_popup->get_item_index(RESOURCE_MAKE_BUILT_IN), !is_resource || is_text_file);

	if (!is_object || is_text_file) {
		info->hide();
		object_selector->clear_path();
		return;
	}

	object_selector->enable_path();

	PopupMenu *p = object_menu->get_popup();

	p->clear();
	p->add_icon_shortcut(get_editor_theme_icon(SNAME("GuiTreeArrowDown")), ED_SHORTCUT("property_editor/expand_all", TTRC("Expand All")), EXPAND_ALL);
	p->add_icon_shortcut(get_editor_theme_icon(SNAME("GuiTreeArrowRight")), ED_SHORTCUT("property_editor/collapse_all", TTRC("Collapse All")), COLLAPSE_ALL);
	// Calling it 'revertable' internally, because that's what the implementation is based on, but labeling it as 'non-default' because that's more user friendly, even if not 100% accurate.
	p->add_shortcut(ED_SHORTCUT("property_editor/expand_revertable", TTRC("Expand Non-Default")), EXPAND_REVERTABLE);

	p->add_separator(TTR("Property Name Style"));
	p->add_radio_check_item(vformat(TTR("Raw (e.g. \"%s\")"), "z_index"), PROPERTY_NAME_STYLE_RAW);
	p->add_radio_check_item(vformat(TTR("Capitalized (e.g. \"%s\")"), "Z Index"), PROPERTY_NAME_STYLE_CAPITALIZED);
	// TRANSLATORS: "Z Index" should match the existing translated CanvasItem property name in the current language you're working on.
	p->add_radio_check_item(TTR("Localized (e.g. \"Z Index\")"), PROPERTY_NAME_STYLE_LOCALIZED);

	if (!EditorPropertyNameProcessor::is_localization_available()) {
		const int index = p->get_item_index(PROPERTY_NAME_STYLE_LOCALIZED);
		p->set_item_disabled(index, true);
		p->set_item_tooltip(index, TTR("Localization not available for current language."));
	}

	p->add_separator();
	p->add_shortcut(ED_SHORTCUT("property_editor/copy_params", TTRC("Copy Properties")), OBJECT_COPY_PARAMS);
	p->add_shortcut(ED_SHORTCUT("property_editor/paste_params", TTRC("Paste Properties")), OBJECT_PASTE_PARAMS);

	if (is_resource || is_node) {
		p->add_separator();
		p->add_shortcut(ED_SHORTCUT("property_editor/make_subresources_unique", TTRC("Make Sub-Resources Unique")), OBJECT_UNIQUE_RESOURCES);
	}

	List<MethodInfo> methods;
	p_object->get_method_list(&methods);

	if (!methods.is_empty()) {
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

	for (const Pair<StringName, Variant> &E : stored_properties) {
		Variant current_prop;
		if (si->get(E.first, current_prop) && current_prop.get_type() == E.second.get_type()) {
			si->set(E.first, E.second);
		}
	}
	stored_properties.clear();
}

void InspectorDock::shortcut_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventKey> key = p_event;

	if (key.is_null() || !key->is_pressed() || key->is_echo()) {
		return;
	}

	if (!is_visible() || !inspector->get_rect().has_point(inspector->get_local_mouse_position())) {
		return;
	}

	if (ED_IS_SHORTCUT("editor/open_search", p_event)) {
		search->grab_focus();
		search->select_all();
		accept_event();
	}
}

InspectorDock::InspectorDock(EditorData &p_editor_data) {
	singleton = this;
	set_name("Inspector");

	editor_data = &p_editor_data;

	property_name_style = EditorPropertyNameProcessor::get_default_inspector_style();

	HBoxContainer *general_options_hb = memnew(HBoxContainer);
	add_child(general_options_hb);

	resource_new_button = memnew(Button);
	resource_new_button->set_theme_type_variation("FlatMenuButton");
	resource_new_button->set_tooltip_text(TTR("Create a new resource in memory and edit it."));
	general_options_hb->add_child(resource_new_button);
	resource_new_button->connect(SceneStringName(pressed), callable_mp(this, &InspectorDock::_new_resource));
	resource_new_button->set_focus_mode(Control::FOCUS_NONE);

	resource_load_button = memnew(Button);
	resource_load_button->set_theme_type_variation("FlatMenuButton");
	resource_load_button->set_tooltip_text(TTR("Load an existing resource from disk and edit it."));
	general_options_hb->add_child(resource_load_button);
	resource_load_button->connect(SceneStringName(pressed), callable_mp(this, &InspectorDock::_open_resource_selector));
	resource_load_button->set_focus_mode(Control::FOCUS_NONE);

	resource_save_button = memnew(MenuButton);
	resource_save_button->set_flat(false);
	resource_save_button->set_theme_type_variation("FlatMenuButton");
	resource_save_button->set_tooltip_text(TTR("Save the currently edited resource."));
	general_options_hb->add_child(resource_save_button);
	resource_save_button->get_popup()->add_item(TTR("Save"), RESOURCE_SAVE);
	resource_save_button->get_popup()->add_item(TTR("Save As..."), RESOURCE_SAVE_AS);
	resource_save_button->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &InspectorDock::_menu_option));
	resource_save_button->set_focus_mode(Control::FOCUS_NONE);
	resource_save_button->set_disabled(true);

	resource_extra_button = memnew(MenuButton);
	resource_extra_button->set_flat(false);
	resource_extra_button->set_theme_type_variation("FlatMenuButton");
	resource_extra_button->set_tooltip_text(TTR("Extra resource options."));
	general_options_hb->add_child(resource_extra_button);
	resource_extra_button->connect("about_to_popup", callable_mp(this, &InspectorDock::_prepare_resource_extra_popup));
	resource_extra_button->get_popup()->add_shortcut(ED_SHORTCUT("property_editor/paste_resource", TTRC("Edit Resource from Clipboard")), RESOURCE_EDIT_CLIPBOARD);
	resource_extra_button->get_popup()->add_shortcut(ED_SHORTCUT("property_editor/copy_resource", TTRC("Copy Resource")), RESOURCE_COPY);
	resource_extra_button->get_popup()->set_item_disabled(1, true);
	resource_extra_button->get_popup()->add_separator();
	resource_extra_button->get_popup()->add_shortcut(ED_SHORTCUT("property_editor/show_in_filesystem", TTRC("Show in FileSystem")), RESOURCE_SHOW_IN_FILESYSTEM);
	resource_extra_button->get_popup()->add_shortcut(ED_SHORTCUT("property_editor/unref_resource", TTRC("Make Resource Built-In")), RESOURCE_MAKE_BUILT_IN);
	resource_extra_button->get_popup()->set_item_disabled(3, true);
	resource_extra_button->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &InspectorDock::_menu_option));

	general_options_hb->add_spacer();

	backward_button = memnew(Button);
	backward_button->set_flat(true);
	general_options_hb->add_child(backward_button);
	backward_button->set_tooltip_text(TTR("Go to previous edited object in history."));
	backward_button->set_disabled(true);
	backward_button->connect(SceneStringName(pressed), callable_mp(this, &InspectorDock::_edit_back));

	forward_button = memnew(Button);
	forward_button->set_flat(true);
	general_options_hb->add_child(forward_button);
	forward_button->set_tooltip_text(TTR("Go to next edited object in history."));
	forward_button->set_disabled(true);
	forward_button->connect(SceneStringName(pressed), callable_mp(this, &InspectorDock::_edit_forward));

	history_menu = memnew(MenuButton);
	history_menu->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	history_menu->set_flat(false);
	history_menu->set_theme_type_variation("FlatMenuButton");
	history_menu->set_tooltip_text(TTR("History of recently edited objects."));
	general_options_hb->add_child(history_menu);
	history_menu->connect("about_to_popup", callable_mp(this, &InspectorDock::_prepare_history));
	history_menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &InspectorDock::_select_history));

	HBoxContainer *subresource_hb = memnew(HBoxContainer);
	add_child(subresource_hb);
	object_selector = memnew(EditorObjectSelector(EditorNode::get_singleton()->get_editor_selection_history()));
	object_selector->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	subresource_hb->add_child(object_selector);

	open_docs_button = memnew(Button);
	open_docs_button->set_theme_type_variation("FlatMenuButton");
	open_docs_button->set_disabled(true);
	open_docs_button->set_tooltip_text(TTRC("Open documentation for this object."));
	open_docs_button->set_shortcut(ED_SHORTCUT("property_editor/open_help", TTRC("Open Documentation")));
	subresource_hb->add_child(open_docs_button);
	open_docs_button->connect(SceneStringName(pressed), callable_mp(this, &InspectorDock::_menu_option).bind(OBJECT_REQUEST_HELP));

	new_resource_dialog = memnew(CreateDialog);
	EditorNode::get_singleton()->get_gui_base()->add_child(new_resource_dialog);
	new_resource_dialog->set_base_type("Resource");
	new_resource_dialog->connect("create", callable_mp(this, &InspectorDock::_resource_created));

	HBoxContainer *property_tools_hb = memnew(HBoxContainer);
	add_child(property_tools_hb);

	search = memnew(LineEdit);
	search->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	search->set_placeholder(TTR("Filter Properties"));
	search->set_clear_button_enabled(true);
	property_tools_hb->add_child(search);

	object_menu = memnew(MenuButton);
	object_menu->set_flat(false);
	object_menu->set_theme_type_variation("FlatMenuButton");
	object_menu->set_shortcut_context(this);
	property_tools_hb->add_child(object_menu);
	object_menu->set_tooltip_text(TTR("Manage object properties."));
	object_menu->get_popup()->connect("about_to_popup", callable_mp(this, &InspectorDock::_prepare_menu));
	object_menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &InspectorDock::_menu_option));

	info = memnew(Button);
	add_child(info);
	info->set_clip_text(true);
	info->hide();
	info->connect(SceneStringName(pressed), callable_mp(this, &InspectorDock::_info_pressed));

	unique_resources_confirmation = memnew(ConfirmationDialog);
	add_child(unique_resources_confirmation);

	VBoxContainer *container = memnew(VBoxContainer);
	unique_resources_confirmation->add_child(container);

	unique_resources_label = memnew(Label);
	container->add_child(unique_resources_label);

	unique_resources_list_tree = memnew(Tree);
	unique_resources_list_tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	unique_resources_list_tree->set_hide_root(true);
	unique_resources_list_tree->set_columns(1);
	unique_resources_list_tree->set_column_title(0, TTR("Property"));
	unique_resources_list_tree->set_custom_minimum_size(Size2(0, 200 * EDSCALE));
	container->add_child(unique_resources_list_tree);

	Label *bottom_label = memnew(Label);
	bottom_label->set_text(TTR("This cannot be undone. Are you sure?"));
	container->add_child(bottom_label);

	unique_resources_confirmation->connect(SceneStringName(confirmed), callable_mp(this, &InspectorDock::_menu_confirm_current));

	info_dialog = memnew(AcceptDialog);
	EditorNode::get_singleton()->get_gui_base()->add_child(info_dialog);

	load_resource_dialog = memnew(EditorFileDialog);
	add_child(load_resource_dialog);
	load_resource_dialog->set_current_dir("res://");
	load_resource_dialog->connect("file_selected", callable_mp(this, &InspectorDock::_resource_file_selected));

	inspector = memnew(EditorInspector);
	add_child(inspector);
	inspector->set_autoclear(true);
	inspector->set_show_categories(true, true);
	inspector->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	inspector->set_use_doc_hints(true);
	inspector->set_hide_script(false);
	inspector->set_hide_metadata(false);
	inspector->set_use_settings_name_style(false);
	inspector->set_property_name_style(property_name_style);
	inspector->set_use_folding(!bool(EDITOR_GET("interface/inspector/disable_folding")));
	inspector->register_text_enter(search);

	inspector->set_use_filter(true);

	inspector->connect("resource_selected", callable_mp(this, &InspectorDock::_resource_selected));

	set_process_shortcut_input(true);
}

InspectorDock::~InspectorDock() {
	singleton = nullptr;
}
