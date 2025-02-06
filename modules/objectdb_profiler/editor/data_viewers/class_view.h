/**************************************************************************/
/*  class_view.h                                                          */
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

#ifndef CLASS_VIEW_H
#define CLASS_VIEW_H

#include "../snapshot_data.h"
#include "snapshot_view.h"

class Tree;
class TreeItem;

struct ClassData {
	ClassData() {}
	ClassData(const String &p_name, const String &p_parent) :
			class_name(p_name), parent_class_name(p_parent) {}
	String class_name;
	String parent_class_name;
	HashSet<String> child_classes;
	List<SnapshotDataObject *> instances;
	TreeItem *tree_node = nullptr;
	HashMap<GameStateSnapshot *, int> recursive_instance_count_cache;

	int instance_count(GameStateSnapshot *p_snapshot = nullptr);
	int get_recursive_instance_count(HashMap<String, ClassData> &p_all_classes, GameStateSnapshot *p_snapshot = nullptr);
};

class SnapshotClassView : public SnapshotView {
	GDCLASS(SnapshotClassView, SnapshotView);

protected:
	Tree *class_tree = nullptr;
	Tree *object_list = nullptr;
	Tree *diff_object_list = nullptr;

	void _object_selected(Tree *p_tree);
	void _class_selected();
	void _add_objects_to_class_map(HashMap<String, ClassData> &p_class_map, GameStateSnapshot *p_objects);
	void _notification(int p_what);

	Tree *_make_object_list_tree(const String &p_column_name);
	void _populate_object_list(GameStateSnapshot *p_snapshot, Tree *p_list, const String &p_name_base);

public:
	SnapshotClassView();
	virtual void show_snapshot(GameStateSnapshot *p_data, GameStateSnapshot *p_diff_data) override;
};

#endif // CLASS_VIEW_H
