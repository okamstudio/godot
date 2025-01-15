/**************************************************************************/
/*  refcounted_view.h                                                     */
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

#ifndef REFCOUNTED_VIEW_H
#define REFCOUNTED_VIEW_H

#include "../snapshot_data.h"
#include "shared_controls.h"
#include "snapshot_view.h"

class Tree;
class HSplitContainer;

class SnapshotRefCountedView : public SnapshotView {
	GDCLASS(SnapshotRefCountedView, SnapshotView);

protected:
	Tree *refs_list = nullptr;
	VBoxContainer *ref_details = nullptr;
	TreeSortAndFilterBar *filter_bar = nullptr;
	HSplitContainer *refs_view = nullptr;

	HashMap<TreeItem *, SnapshotDataObject *> item_data_map;
	HashMap<SnapshotDataObject *, TreeItem *> data_item_map;
	HashMap<TreeItem *, TreeItem *> reference_item_map;

	void _refcounted_selected();
	void _insert_data(GameStateSnapshot *p_snapshot, const String &p_name);
	void _ref_selected(Tree *p_source_tree);
	void _set_split_to_center();

public:
	SnapshotRefCountedView();
	virtual void show_snapshot(GameStateSnapshot *p_data, GameStateSnapshot *p_diff_data) override;
};

#endif // REFCOUNTED_VIEW_H
