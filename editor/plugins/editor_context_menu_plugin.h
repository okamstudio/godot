/**************************************************************************/
/*  editor_context_menu_plugin.h                                          */
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

#ifndef EDITOR_CONTEXT_MENU_PLUGIN_H
#define EDITOR_CONTEXT_MENU_PLUGIN_H

#include "core/object/gdvirtual.gen.inc"
#include "core/object/ref_counted.h"

class InputEvent;
class PopupMenu;
class Shortcut;
class Texture2D;

class EditorContextMenuPlugin : public RefCounted {
	GDCLASS(EditorContextMenuPlugin, RefCounted);

	friend class EditorContextMenuPluginManager;

	inline static constexpr int MAX_ITEMS = 100;

public:
	enum ContextMenuSlot {
		CONTEXT_SLOT_SCENE_TREE,
		CONTEXT_SLOT_FILESYSTEM,
		CONTEXT_SLOT_SCRIPT_EDITOR,
		CONTEXT_SLOT_FILESYSTEM_CREATE,
	};
	inline static constexpr int BASE_ID = 2000;

private:
	int slot = -1;

public:
	struct ContextMenuItem {
		int id = 0;
		String item_name;
		Callable callable;
		Ref<Texture2D> icon;
		Ref<Shortcut> shortcut;
		PopupMenu *submenu = nullptr;
	};
	HashMap<String, ContextMenuItem> context_menu_items;
	HashMap<Ref<Shortcut>, Callable> context_menu_shortcuts;

protected:
	static void _bind_methods();

	GDVIRTUAL1(_popup_menu, Vector<String>);

public:
	virtual void get_options(const Vector<String> &p_paths);

	void add_menu_shortcut(const Ref<Shortcut> &p_shortcut, const Callable &p_callable);
	void add_context_menu_item(const String &p_name, const Callable &p_callable, const Ref<Texture2D> &p_texture);
	void add_context_menu_item_from_shortcut(const String &p_name, const Ref<Shortcut> &p_shortcut, const Ref<Texture2D> &p_texture);
	void add_context_submenu_item(const String &p_name, PopupMenu *p_menu, const Ref<Texture2D> &p_texture);
};

VARIANT_ENUM_CAST(EditorContextMenuPlugin::ContextMenuSlot);

class EditorContextMenuPluginManager : public Object {
	GDCLASS(EditorContextMenuPluginManager, Object);

	using ContextMenuSlot = EditorContextMenuPlugin::ContextMenuSlot;
	static inline EditorContextMenuPluginManager *singleton = nullptr;

	LocalVector<Ref<EditorContextMenuPlugin>> plugin_list;

public:
	static EditorContextMenuPluginManager *get_singleton() { return singleton; }

	void add_plugin(ContextMenuSlot p_slot, const Ref<EditorContextMenuPlugin> &p_plugin);
	void remove_plugin(const Ref<EditorContextMenuPlugin> &p_plugin);

	void add_options_from_plugins(PopupMenu *p_popup, ContextMenuSlot p_slot, const Vector<String> &p_paths);
	Callable match_custom_shortcut(ContextMenuSlot p_slot, const Ref<InputEvent> &p_event);
	bool activate_custom_option(ContextMenuSlot p_slot, int p_option, const Variant &p_arg);

	void invoke_callback(const Callable &p_callback, const Variant &p_arg);

	static void create();
	static void cleanup();
};

#endif // EDITOR_CONTEXT_MENU_PLUGIN_H
