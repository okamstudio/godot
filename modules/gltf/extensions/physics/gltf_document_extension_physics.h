/**************************************************************************/
/*  gltf_document_extension_physics.h                                     */
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

#include "../gltf_document_extension.h"
#include "gltf_physics_body.h"
#include "gltf_physics_shape.h"

class GLTFDocumentExtensionPhysics : public GLTFDocumentExtension {
	GDCLASS(GLTFDocumentExtensionPhysics, GLTFDocumentExtension);

public:
	// Import process.
	Error import_preflight(Ref<GLTFState> p_state, Vector<String> p_extensions) override;
	Vector<String> get_supported_extensions() override;
	Error parse_node_extensions(Ref<GLTFState> p_state, Ref<GLTFNode> p_gltf_node, Dictionary &p_extensions) override;
	Ref<GLTFObjectModelProperty> import_object_model_property(Ref<GLTFState> p_state, const PackedStringArray &p_split_json_pointer, const TypedArray<NodePath> &p_partial_paths) override;
	Node3D *generate_scene_node(Ref<GLTFState> p_state, Ref<GLTFNode> p_gltf_node, Node *p_scene_parent) override;
	// Export process.
	void convert_scene_node(Ref<GLTFState> p_state, Ref<GLTFNode> p_gltf_node, Node *p_scene_node) override;
	Error export_preserialize(Ref<GLTFState> p_state) override;
	Ref<GLTFObjectModelProperty> export_object_model_property(Ref<GLTFState> p_state, const NodePath &p_node_path, const Node *p_godot_node, GLTFNodeIndex p_gltf_node_index, const Object *p_target_object, int p_target_depth) override;
	Error export_node(Ref<GLTFState> p_state, Ref<GLTFNode> p_gltf_node, Dictionary &r_node_json, Node *p_scene_node) override;
};
