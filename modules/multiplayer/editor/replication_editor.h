/**************************************************************************/
/*  replication_editor.h                                                  */
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

#include "../scene_replication_config.h"

#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"

class ConfirmationDialog;
class MultiplayerSynchronizer;
class AcceptDialog;
class LineEdit;
class Tree;
class TreeItem;
class PropertySelector;
class SceneTreeDialog;

class ReplicationEditor : public VBoxContainer {
	GDCLASS(ReplicationEditor, VBoxContainer);

private:
	MultiplayerSynchronizer *current = nullptr;

	ConfirmationDialog *delete_dialog = nullptr;
	Button *add_pick_button = nullptr;
	Button *add_from_path_button = nullptr;
	LineEdit *np_line_edit = nullptr;

	Label *drop_label = nullptr;

	Ref<SceneReplicationConfig> config;
	NodePath deleting;
	Tree *tree = nullptr;

	PropertySelector *prop_selector = nullptr;
	SceneTreeDialog *pick_node = nullptr;
	NodePath adding_node_path;

	Button *pin = nullptr;

	Ref<Texture2D> _get_class_icon(const Node *p_node);

	void _add_pressed();
	void _np_text_submitted(const String &p_newtext);
	void _tree_item_edited();
	void _tree_button_pressed(Object *p_item, int p_column, int p_id, MouseButton p_button);
	void _update_value(const NodePath &p_prop, int p_column, int p_checked);
	void _update_config();
	void _dialog_closed(bool p_confirmed);
	void _add_property(const NodePath &p_property, bool p_spawn, SceneReplicationConfig::ReplicationMode p_mode);

	void _pick_node_filter_text_changed(const String &p_newtext);
	void _pick_node_select_recursive(TreeItem *p_item, const String &p_filter, Vector<Node *> &p_select_candidates);
	void _pick_node_selected(NodePath p_path);

	void _pick_new_property();
	void _pick_node_property_selected(String p_name);

	bool _can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	void _drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);

	void _add_sync_property(String p_path);

protected:
	static void _bind_methods();

	void _notification(int p_what);

public:
	void edit(MultiplayerSynchronizer *p_object);
	MultiplayerSynchronizer *get_current() const { return current; }

	Button *get_pin() { return pin; }
	ReplicationEditor();
	~ReplicationEditor() {}
};
