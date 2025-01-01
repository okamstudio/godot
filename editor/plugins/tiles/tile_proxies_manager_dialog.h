/**************************************************************************/
/*  tile_proxies_manager_dialog.h                                         */
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

#ifndef TILE_PROXIES_MANAGER_DIALOG_H
#define TILE_PROXIES_MANAGER_DIALOG_H

#include "editor/editor_properties.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/item_list.h"
#include "scene/resources/2d/tile_set.h"

class EditorPropertyVector2i;
class EditorUndoRedoManager;

class TileProxiesManagerDialog : public ConfirmationDialog {
	GDCLASS(TileProxiesManagerDialog, ConfirmationDialog);

private:
	int commited_actions_count = 0;
	Ref<TileSet> tile_set;

	TileMapCell from;
	TileMapCell to;

	// GUI
	ItemList *source_level_list = nullptr;
	ItemList *coords_level_list = nullptr;
	ItemList *alternative_level_list = nullptr;

	EditorPropertyInteger *source_from_property_editor = nullptr;
	EditorPropertyVector2i *coords_from_property_editor = nullptr;
	EditorPropertyInteger *alternative_from_property_editor = nullptr;
	EditorPropertyInteger *source_to_property_editor = nullptr;
	EditorPropertyVector2i *coords_to_property_editor = nullptr;
	EditorPropertyInteger *alternative_to_property_editor = nullptr;

	PopupMenu *popup_menu = nullptr;
	void _right_clicked(int p_item, Vector2 p_local_mouse_pos, MouseButton p_mouse_button_index, Object *p_item_list);
	void _menu_id_pressed(int p_id);
	void _delete_selected_bindings();
	void _update_lists();
	void _update_enabled_property_editors();
	void _property_changed(const String &p_path, const Variant &p_value, const String &p_name, bool p_changing);
	void _add_button_pressed();

	void _clear_invalid_button_pressed();
	void _clear_all_button_pressed();

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _unhandled_key_input(Ref<InputEvent> p_event);
	virtual void cancel_pressed() override;
	static void _bind_methods();

public:
	void update_tile_set(Ref<TileSet> p_tile_set);

	TileProxiesManagerDialog();
};

#endif // TILE_PROXIES_MANAGER_DIALOG_H
