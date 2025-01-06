/**************************************************************************/
/*  light_occluder_2d_editor_plugin.cpp                                   */
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

#include "light_occluder_2d_editor_plugin.h"

#include "editor/editor_undo_redo_manager.h"

Ref<OccluderPolygon2D> LightOccluder2DEditor::_ensure_occluder() const {
	Ref<OccluderPolygon2D> occluder = node->get_occluder_polygon();
	if (occluder.is_null()) {
		occluder.instantiate();
		node->set_occluder_polygon(occluder);
	}
	return occluder;
}

Node2D *LightOccluder2DEditor::_get_node() const {
	return node;
}

void LightOccluder2DEditor::_set_node(Node *p_polygon) {
	node = Object::cast_to<LightOccluder2D>(p_polygon);
}

bool LightOccluder2DEditor::_is_line() const {
	Ref<OccluderPolygon2D> occluder = node->get_occluder_polygon();
	if (occluder.is_valid()) {
		return !occluder->is_closed();
	} else {
		return false;
	}
}

int LightOccluder2DEditor::_get_polygon_count() const {
	Ref<OccluderPolygon2D> occluder = node->get_occluder_polygon();
	if (occluder.is_valid()) {
		return occluder->get_polygon().size();
	} else {
		return 0;
	}
}

Variant LightOccluder2DEditor::_get_polygon(int p_idx) const {
	Ref<OccluderPolygon2D> occluder = node->get_occluder_polygon();
	if (occluder.is_valid()) {
		return occluder->get_polygon();
	} else {
		return Variant(Vector<Vector2>());
	}
}

void LightOccluder2DEditor::_set_polygon(int p_idx, const Variant &p_polygon) const {
	Ref<OccluderPolygon2D> occluder = _ensure_occluder();
	occluder->set_polygon(p_polygon);
}

void LightOccluder2DEditor::_action_set_polygon(int p_idx, const Variant &p_previous, const Variant &p_polygon) {
	Ref<OccluderPolygon2D> occluder = _ensure_occluder();
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->add_do_method(occluder.ptr(), "set_polygon", p_polygon);
	undo_redo->add_undo_method(occluder.ptr(), "set_polygon", p_previous);
}

bool LightOccluder2DEditor::_has_resource() const {
	return node && node->get_occluder_polygon().is_valid();
}

void LightOccluder2DEditor::_create_resource() {
	if (!node) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Create Occluder Polygon"));
	undo_redo->add_do_method(node, "set_occluder_polygon", Ref<OccluderPolygon2D>(memnew(OccluderPolygon2D)));
	undo_redo->add_undo_method(node, "set_occluder_polygon", Variant(Ref<RefCounted>()));
	undo_redo->commit_action();

	_menu_option(MODE_CREATE);
}

LightOccluder2DEditor::LightOccluder2DEditor() {}

LightOccluder2DEditorPlugin::LightOccluder2DEditorPlugin() :
		AbstractPolygon2DEditorPlugin(memnew(LightOccluder2DEditor), "LightOccluder2D") {
}
