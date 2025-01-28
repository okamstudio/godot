/**************************************************************************/
/*  navigation_obstacle_2d_editor_plugin.cpp                              */
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

#include "navigation_obstacle_2d_editor_plugin.h"

#include "editor/editor_undo_redo_manager.h"

Node2D *NavigationObstacle2DEditor::_get_node() const {
	return node;
}

void NavigationObstacle2DEditor::_set_node(Node *p_polygon) {
	node = Object::cast_to<NavigationObstacle2D>(p_polygon);
}

Variant NavigationObstacle2DEditor::_get_polygon(int p_idx) const {
	return node->get_vertices();
}

void NavigationObstacle2DEditor::_set_polygon(int p_idx, const Variant &p_polygon) const {
	node->set_vertices(p_polygon);
}

void NavigationObstacle2DEditor::_action_add_polygon(const Variant &p_polygon) {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->add_do_method(node, "set_vertices", p_polygon);
	undo_redo->add_undo_method(node, "set_vertices", node->get_vertices());
}

void NavigationObstacle2DEditor::_action_remove_polygon(int p_idx) {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->add_do_method(node, "set_vertices", Variant(Vector<Vector2>()));
	undo_redo->add_undo_method(node, "set_vertices", node->get_vertices());
}

void NavigationObstacle2DEditor::_action_set_polygon(int p_idx, const Variant &p_previous, const Variant &p_polygon) {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->add_do_method(node, "set_vertices", p_polygon);
	undo_redo->add_undo_method(node, "set_vertices", node->get_vertices());
}

NavigationObstacle2DEditor::NavigationObstacle2DEditor() {}

NavigationObstacle2DEditorPlugin::NavigationObstacle2DEditorPlugin() :
		AbstractPolygon2DEditorPlugin(memnew(NavigationObstacle2DEditor), "NavigationObstacle2D") {
}
