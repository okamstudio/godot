/**************************************************************************/
/*  editor_context_menu_plugin.cpp                                        */
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

#include "editor_context_menu_plugin.h"

#include "scene/gui/popup_menu.h"

bool EditorContextMenuPlugin::can_handle(Array &p_paths) {
	if (get_script_instance()) {
		Variant arg = p_paths;
		Variant *argptr = &arg;
		Variant::CallError ce;
		return get_script_instance()->call("can_handle", (const Variant **)&argptr, 1, ce);
	}
	return false;
}

void EditorContextMenuPlugin::add_context_menu_item(const Ref<Texture> &p_texture, const String &p_name, Object *p_handler, const String &p_callback) {
	Array parameters;
	parameters.push_back(p_texture);
	parameters.push_back(p_name);
	parameters.push_back(p_handler->get_instance_id());
	parameters.push_back(p_callback);
	set_meta("parameters", parameters);
}

Ref<EditorContextMenuPlugin> EditorContextMenu::scene_tree_plugins[MAX_PLUGINS];
Ref<EditorContextMenuPlugin> EditorContextMenu::filesystem_plugins[MAX_PLUGINS];
Ref<EditorContextMenuPlugin> EditorContextMenu::script_editor_plugins[MAX_PLUGINS];

void EditorContextMenu::add_context_menu_plugin(ContextMenuSlot slot, const Ref<EditorContextMenuPlugin> &p_plugin) {
	Ref<EditorContextMenuPlugin> *plugin_list = get_plugin_array(slot);
	ERR_FAIL_COND(plugin_list[MAX_PLUGINS - 1].is_valid());
	Ref<EditorContextMenuPlugin> plugin = p_plugin;

	for (int i = 0; i < MAX_PLUGINS; ++i) {
		if (plugin_list[i].is_null()) {
			plugin_list[i] = p_plugin;
			plugin->set_meta("idx", CONTEXT_ITEM_ID_BASE + i + 1);
			break;
		}
	}
}

void EditorContextMenu::remove_context_menu_plugin(ContextMenuSlot slot, const Ref<EditorContextMenuPlugin> &p_plugin) {
	Ref<EditorContextMenuPlugin> *plugin_list = get_plugin_array(slot);
	for (int i = 0; i < MAX_PLUGINS; ++i) {
		if (plugin_list[i] == p_plugin) {
			plugin_list[i].unref();
			break;
		}
	}
}

void EditorContextMenu::_bind_methods() {
	BIND_ENUM_CONSTANT(CONTEXT_SLOT_SCENE_TREE);
	BIND_ENUM_CONSTANT(CONTEXT_SLOT_FILESYSTEM);
	BIND_ENUM_CONSTANT(CONTEXT_SLOT_SCRIPT_EDITOR);
}

void EditorContextMenu::handle_plugins(ContextMenuSlot p_slot, Vector<String> &p_paths, PopupMenu *p_popup) {
	Ref<EditorContextMenuPlugin> *plugin_list = get_plugin_array(p_slot);
	bool add_separator = false;

	Array paths;
	for (int i = 0; i < p_paths.size(); i++) {
		paths.push_back(p_paths.get(i));
	}

	for (int i = 0; i < MAX_PLUGINS; i++) {
		Ref<EditorContextMenuPlugin> plugin = plugin_list[i];
		if (plugin.is_null()) {
			break;
		}
		if (!plugin->can_handle(paths)) {
			continue;
		}
		if (!add_separator) {
			add_separator = true;
			p_popup->add_separator();
		}
		Array params = plugin->get_meta("parameters");
		Variant icon = params[0];
		int idx = plugin->get_meta("idx");
		if (icon.get_type() != Variant::NIL) {
			p_popup->add_icon_item(icon, params[1], idx);
		} else {
			p_popup->add_item(params[1], idx);
		}
	}
}

template <typename T>
void EditorContextMenu::invoke_plugin_callback(ContextMenuSlot p_slot, int p_option, const T &p_arg) {
	Ref<EditorContextMenuPlugin> *plugin_list = get_plugin_array(p_slot);
	Variant arg = p_arg;
	Variant *argptr = &arg;
	Variant::CallError ce;

	for (int i = 0; i < MAX_PLUGINS; i++) {
		Ref<EditorContextMenuPlugin> plugin = plugin_list[i];
		if (plugin.is_null()) {
			break;
		}
		int idx = plugin->get_meta("idx");
		if (p_option != idx) {
			continue;
		}
		Array params = plugin->get_meta("parameters");
		Object *handler = ObjectDB::get_instance(params[2]);
		String callback = params[3];

		handler->call(callback, (const Variant **)&argptr, 1, ce);
		if (ce.error != Variant::CallError::CALL_OK) {
			String err = Variant::get_call_error_text(handler, callback, (const Variant **)&argptr, 1, ce);
			ERR_PRINT("Error calling function from context menu: " + err);
		}
	}
}

void EditorContextMenu::options_pressed(ContextMenuSlot p_slot, int p_option, const List<Node *> &p_selected) {
	Array nodes;
	for (int i = 0; i < p_selected.size(); i++) {
		nodes.append(p_selected[i]);
	}
	invoke_plugin_callback(p_slot, p_option, nodes);
}

void EditorContextMenu::options_pressed(ContextMenuSlot p_slot, int p_option, const RES &p_script) {
	invoke_plugin_callback(p_slot, p_option, p_script);
}

void EditorContextMenu::options_pressed(ContextMenuSlot p_slot, int p_option, const Vector<String> &p_selected) {
	Array paths;
	for (int i = 0; i < p_selected.size(); i++) {
		paths.push_back(p_selected[i]);
	}
	invoke_plugin_callback(p_slot, p_option, paths);
}

Ref<EditorContextMenuPlugin> *EditorContextMenu::get_plugin_array(ContextMenuSlot p_slot) {
	switch (p_slot) {
		case CONTEXT_SLOT_SCENE_TREE:
			return scene_tree_plugins;
		case CONTEXT_SLOT_FILESYSTEM:
			return filesystem_plugins;
		case CONTEXT_SLOT_SCRIPT_EDITOR:
			return script_editor_plugins;
		default:
			return nullptr;
	}
}

void EditorContextMenuPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_context_menu_item", "icon", "name", "handler", "callback"), &EditorContextMenuPlugin::add_context_menu_item);

	MethodInfo vm;
	vm.name = "can_handle";
	vm.return_val.type = Variant::BOOL;
	vm.arguments.push_back(PropertyInfo(Variant::ARRAY, "paths"));
	BIND_VMETHOD(vm);
}
