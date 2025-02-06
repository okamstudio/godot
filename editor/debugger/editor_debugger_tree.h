/**************************************************************************/
/*  editor_debugger_tree.h                                                */
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

#pragma once

#include "scene/gui/tree.h"

class SceneDebuggerTree;
class EditorFileDialog;

class EditorDebuggerTree : public Tree {
	GDCLASS(EditorDebuggerTree, Tree);

private:
	struct ParentItem {
		TreeItem *tree_item;
		int child_count;
		bool matches_filter;

		ParentItem(TreeItem *p_tree_item = nullptr, int p_child_count = 0, bool p_matches_filter = false) {
			tree_item = p_tree_item;
			child_count = p_child_count;
			matches_filter = p_matches_filter;
		}
	};

	enum ItemMenu {
		ITEM_MENU_SAVE_REMOTE_NODE,
		ITEM_MENU_COPY_NODE_PATH,
		ITEM_MENU_EXPAND_COLLAPSE,
	};

	ObjectID inspected_object_id;
	int debugger_id = 0;
	bool updating_scene_tree = false;
	bool scrolling_to_item = false;
	bool selection_uncollapse_all = false;
	HashSet<ObjectID> unfold_cache;
	PopupMenu *item_menu = nullptr;
	EditorFileDialog *file_dialog = nullptr;
	String last_filter;

	void _scene_tree_folded(Object *p_obj);
	void _scene_tree_selected();
	void _scene_tree_rmb_selected(const Vector2 &p_position, MouseButton p_button);
	void _item_menu_id_pressed(int p_option);
	void _file_selected(const String &p_file);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	enum Button {
		BUTTON_SUBSCENE = 0,
		BUTTON_VISIBILITY = 1,
	};

	virtual Variant get_drag_data(const Point2 &p_point) override;

	void update_icon_max_width();
	String get_selected_path();
	ObjectID get_selected_object();
	int get_current_debugger(); // Would love to have one tree for every debugger.
	void update_scene_tree(const SceneDebuggerTree *p_tree, int p_debugger);
	void select_node(ObjectID p_id);
	EditorDebuggerTree();
};
