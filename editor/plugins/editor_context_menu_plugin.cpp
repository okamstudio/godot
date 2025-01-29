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

#include "core/input/shortcut.h"
#include "editor/editor_string_names.h"
#include "scene/gui/popup_menu.h"
#include "scene/resources/texture.h"

void EditorContextMenuPlugin::get_options(const Vector<String> &p_paths) {
	GDVIRTUAL_CALL(_popup_menu, p_paths);
}

void EditorContextMenuPlugin::add_menu_shortcut(const Ref<Shortcut> &p_shortcut, const Callable &p_callable) {
	context_menu_shortcuts.insert(p_shortcut, p_callable);
}

void EditorContextMenuPlugin::add_context_menu_item(const String &p_name, const Callable &p_callable, const Ref<Texture2D> &p_texture) {
	ERR_FAIL_COND_MSG(context_menu_items.has(p_name), "Context menu item already registered.");
	ERR_FAIL_COND_MSG(context_menu_items.size() == MAX_ITEMS, "Maximum number of context menu items reached.");

	ContextMenuItem item;
	item.item_name = p_name;
	item.callable = p_callable;
	item.icon = p_texture;
	context_menu_items.insert(p_name, item);
}

void EditorContextMenuPlugin::add_context_menu_item_from_shortcut(const String &p_name, const Ref<Shortcut> &p_shortcut, const Ref<Texture2D> &p_texture) {
	Callable *callback = context_menu_shortcuts.getptr(p_shortcut);
	ERR_FAIL_NULL_MSG(callback, "Shortcut not registered. Use add_menu_shortcut() first.");

	ContextMenuItem item;
	item.item_name = p_name;
	item.callable = *callback;
	item.icon = p_texture;
	item.shortcut = p_shortcut;
	context_menu_items.insert(p_name, item);
}

void EditorContextMenuPlugin::add_context_submenu_item(const String &p_name, PopupMenu *p_menu, const Ref<Texture2D> &p_texture) {
	ERR_FAIL_NULL(p_menu);

	ContextMenuItem item;
	item.item_name = p_name;
	item.icon = p_texture;
	item.submenu = p_menu;
	context_menu_items.insert(p_name, item);
}

void EditorContextMenuPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_menu_shortcut", "shortcut", "callback"), &EditorContextMenuPlugin::add_menu_shortcut);
	ClassDB::bind_method(D_METHOD("add_context_menu_item", "name", "callback", "icon"), &EditorContextMenuPlugin::add_context_menu_item, DEFVAL(Ref<Texture2D>()));
	ClassDB::bind_method(D_METHOD("add_context_menu_item_from_shortcut", "name", "shortcut", "icon"), &EditorContextMenuPlugin::add_context_menu_item_from_shortcut, DEFVAL(Ref<Texture2D>()));
	ClassDB::bind_method(D_METHOD("add_context_submenu_item", "name", "menu", "icon"), &EditorContextMenuPlugin::add_context_submenu_item, DEFVAL(Ref<Texture2D>()));

	GDVIRTUAL_BIND(_popup_menu, "paths");

	BIND_ENUM_CONSTANT(CONTEXT_SLOT_SCENE_TREE);
	BIND_ENUM_CONSTANT(CONTEXT_SLOT_FILESYSTEM);
	BIND_ENUM_CONSTANT(CONTEXT_SLOT_SCRIPT_EDITOR);
	BIND_ENUM_CONSTANT(CONTEXT_SLOT_FILESYSTEM_CREATE);
	BIND_ENUM_CONSTANT(CONTEXT_SLOT_SCRIPT_EDITOR_CODE);
	BIND_ENUM_CONSTANT(CONTEXT_SLOT_SCENE_TABS);
	BIND_ENUM_CONSTANT(CONTEXT_SLOT_2D_EDITOR);
}

void EditorContextMenuPluginManager::add_plugin(EditorContextMenuPlugin::ContextMenuSlot p_slot, const Ref<EditorContextMenuPlugin> &p_plugin) {
	ERR_FAIL_COND(p_plugin.is_null());
	ERR_FAIL_COND(plugin_list.has(p_plugin));

	p_plugin->slot = p_slot;
	plugin_list.push_back(p_plugin);
}

void EditorContextMenuPluginManager::remove_plugin(const Ref<EditorContextMenuPlugin> &p_plugin) {
	ERR_FAIL_COND(p_plugin.is_null());
	ERR_FAIL_COND(!plugin_list.has(p_plugin));

	plugin_list.erase(p_plugin);
}

bool EditorContextMenuPluginManager::has_plugins_for_slot(ContextMenuSlot p_slot) {
	for (Ref<EditorContextMenuPlugin> &plugin : plugin_list) {
		if (plugin->slot == p_slot) {
			return true;
		}
	}
	return false;
}

void EditorContextMenuPluginManager::add_options_from_plugins(PopupMenu *p_popup, ContextMenuSlot p_slot, const Vector<String> &p_paths) {
	bool separator_added = false;
	const int icon_size = p_popup->get_theme_constant(SNAME("class_icon_size"), EditorStringName(Editor));
	int id = EditorContextMenuPlugin::BASE_ID;

	for (Ref<EditorContextMenuPlugin> &plugin : plugin_list) {
		if (plugin->slot != p_slot) {
			continue;
		}
		plugin->context_menu_items.clear();
		plugin->get_options(p_paths);

		HashMap<String, EditorContextMenuPlugin::ContextMenuItem> &items = plugin->context_menu_items;
		if (items.size() > 0 && !separator_added) {
			separator_added = true;
			p_popup->add_separator();
		}

		for (KeyValue<String, EditorContextMenuPlugin::ContextMenuItem> &E : items) {
			EditorContextMenuPlugin::ContextMenuItem &item = E.value;
			item.id = id;

			if (item.submenu) {
				p_popup->add_submenu_node_item(item.item_name, item.submenu, id);
			} else {
				p_popup->add_item(item.item_name, id);
			}

			if (item.icon.is_valid()) {
				p_popup->set_item_icon(-1, item.icon);
				p_popup->set_item_icon_max_width(-1, icon_size);
			}

			if (item.shortcut.is_valid()) {
				p_popup->set_item_shortcut(-1, item.shortcut, true);
			}
			id++;
		}
	}
}

Callable EditorContextMenuPluginManager::match_custom_shortcut(EditorContextMenuPlugin::ContextMenuSlot p_slot, const Ref<InputEvent> &p_event) {
	for (Ref<EditorContextMenuPlugin> &plugin : plugin_list) {
		if (plugin->slot != p_slot) {
			continue;
		}

		for (KeyValue<Ref<Shortcut>, Callable> &E : plugin->context_menu_shortcuts) {
			if (E.key->matches_event(p_event)) {
				return E.value;
			}
		}
	}
	return Callable();
}

bool EditorContextMenuPluginManager::activate_custom_option(ContextMenuSlot p_slot, int p_option, const Variant &p_arg) {
	for (Ref<EditorContextMenuPlugin> &plugin : plugin_list) {
		if (plugin->slot != p_slot) {
			continue;
		}

		for (KeyValue<String, EditorContextMenuPlugin::ContextMenuItem> &E : plugin->context_menu_items) {
			if (E.value.id == p_option) {
				invoke_callback(E.value.callable, p_arg);
				return true;
			}
		}
	}
	return false;
}

void EditorContextMenuPluginManager::invoke_callback(const Callable &p_callback, const Variant &p_arg) {
	const Variant *argptr = &p_arg;
	Callable::CallError ce;
	Variant result;
	p_callback.callp(&argptr, 1, result, ce);

	if (ce.error != Callable::CallError::CALL_OK) {
		ERR_FAIL_MSG("Failed to execute context menu callback: " + Variant::get_callable_error_text(p_callback, &argptr, 1, ce) + ".");
	}
}

void EditorContextMenuPluginManager::create() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = memnew(EditorContextMenuPluginManager);
}

void EditorContextMenuPluginManager::cleanup() {
	ERR_FAIL_NULL(singleton);
	memdelete(singleton);
	singleton = nullptr;
}
