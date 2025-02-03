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

#ifndef EDITOR_CONTEXT_MENU_H
#define EDITOR_CONTEXT_MENU_H

#include "core/reference.h"
#include "core/resource.h"

class Node;
class PopupMenu;
class Texture;

class EditorContextMenuPlugin : public Reference {
	GDCLASS(EditorContextMenuPlugin, Reference);

	friend class EditorContextMenu;

protected:
	static void _bind_methods();

public:
	virtual bool can_handle(Array &p_paths);

	void add_context_menu_item(const Ref<Texture> &p_texture, const String &p_name, Object *p_handler, const String &p_callback);
};

class EditorContextMenu : public Reference {
	GDCLASS(EditorContextMenu, Reference);

	friend class EditorContextMenuPlugin;

	enum {
		MAX_PLUGINS = 32
	};

	static Ref<EditorContextMenuPlugin> scene_tree_plugins[MAX_PLUGINS];
	static Ref<EditorContextMenuPlugin> filesystem_plugins[MAX_PLUGINS];
	static Ref<EditorContextMenuPlugin> script_editor_plugins[MAX_PLUGINS];

protected:
	static void _bind_methods();

public:
	enum ContextMenuSlot {
		CONTEXT_SLOT_SCENE_TREE,
		CONTEXT_SLOT_FILESYSTEM,
		CONTEXT_SLOT_SCRIPT_EDITOR,
	};

	enum {
		CONTEXT_ITEM_ID_BASE = 200
	};

	static void add_context_menu_plugin(ContextMenuSlot slot, const Ref<EditorContextMenuPlugin> &p_plugin);

	static void remove_context_menu_plugin(ContextMenuSlot slot, const Ref<EditorContextMenuPlugin> &p_plugin);

	static void handle_plugins(ContextMenuSlot p_slot, Vector<String> &p_paths, PopupMenu *p_popup);

	static void options_pressed(ContextMenuSlot p_slot, int p_option, const Vector<String> &p_selected);
	//For scene dock
	static void options_pressed(ContextMenuSlot p_slot, int p_option, const List<Node *> &p_selected);
	//For script editor
	static void options_pressed(ContextMenuSlot p_slot, int p_option, const RES &p_script);

	static Ref<EditorContextMenuPlugin> *get_plugin_array(ContextMenuSlot p_slot);
	template <typename T>
	static void invoke_plugin_callback(ContextMenuSlot p_slot, int p_option, const T &p_arg);
};

VARIANT_ENUM_CAST(EditorContextMenu::ContextMenuSlot);

#endif //EDITOR_CONTEXT_MENU_H
