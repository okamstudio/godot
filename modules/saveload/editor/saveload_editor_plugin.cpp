/**************************************************************************/
/*  saveload_editor_plugin.cpp                                            */
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

#include "saveload_editor_plugin.h"

#include "../saveload_synchronizer.h"
#include "saveload_editor.h"

#include "editor/editor_interface.h"
#include "editor/editor_node.h"

/// SaveloadEditorPlugin

SaveloadEditorPlugin::SaveloadEditorPlugin() {
	saveload_editor = memnew(SaveloadEditor);
	button = EditorNode::get_singleton()->add_bottom_panel_item(TTR("Save Load"), saveload_editor);
	button->hide();
	saveload_editor->get_pin()->connect("pressed", callable_mp(this, &SaveloadEditorPlugin::_pinned));
}

void SaveloadEditorPlugin::_open_request(const String &p_path) {
	EditorInterface::get_singleton()->open_scene_from_path(p_path);
}

void SaveloadEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			get_tree()->connect("node_removed", callable_mp(this, &SaveloadEditorPlugin::_node_removed));
		} break;
	}
}

void SaveloadEditorPlugin::_node_removed(Node *p_node) {
	if (p_node && p_node == saveload_editor->get_current()) {
		saveload_editor->edit(nullptr);
		if (saveload_editor->is_visible_in_tree()) {
			EditorNode::get_singleton()->hide_bottom_panel();
		}
		button->hide();
		saveload_editor->get_pin()->set_pressed(false);
	}
}

void SaveloadEditorPlugin::_pinned() {
	if (!saveload_editor->get_pin()->is_pressed()) {
		if (saveload_editor->is_visible_in_tree()) {
			EditorNode::get_singleton()->hide_bottom_panel();
		}
		button->hide();
	}
}

void SaveloadEditorPlugin::edit(Object *p_object) {
	saveload_editor->edit(Object::cast_to<SaveloadSynchronizer>(p_object));
}

bool SaveloadEditorPlugin::handles(Object *p_object) const {
	return p_object->is_class("SaveloadSynchronizer");
}

void SaveloadEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		button->show();
		EditorNode::get_singleton()->make_bottom_panel_item_visible(saveload_editor);
	} else if (!saveload_editor->get_pin()->is_pressed()) {
		if (saveload_editor->is_visible_in_tree()) {
			EditorNode::get_singleton()->hide_bottom_panel();
		}
		button->hide();
	}
}
