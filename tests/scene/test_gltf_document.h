/**************************************************************************/
/*  test_gltf_document.h                                                  */
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

#include "modules/gltf/extensions/gltf_document_extension_convert_importer_mesh.h"
#include "modules/gltf/gltf_document.h"

#include "tests/test_macros.h"
#include "tests/test_utils.h"

namespace TestGLTFDocument {

struct GLTFArraySize {
	String key;
	int val;
};

struct GLTFKeyValue {
	String key;
	Variant val;
};

struct GLTFTestCase {
	String filename;
	String copyright;
	String generator;
	String version;
	Vector<GLTFArraySize> array_sizes;
	Vector<GLTFArraySize> json_array_sizes;
	Vector<GLTFKeyValue> keyvalues;
};

const GLTFTestCase glTF_test_cases[] = {
	{ "models/cube.gltf",
			"",
			"Khronos glTF Blender I/O v4.3.47",
			"2.0",
			// Here are the array sizes.
			{
					{ "nodes", 1 },
					{ "buffers", 1 },
					{ "buffer_views", 13 },
					{ "accessors", 13 },
					{ "meshes", 1 },
					{ "materials", 2 },
					{ "root_nodes", 1 },
					{ "textures", 0 },
					{ "texture_samplers", 0 },
					{ "images", 0 },
					{ "skins", 0 },
					{ "cameras", 0 },
					{ "lights", 0 },
					{ "skeletons", 0 },
					{ "animations", 1 },
			},
			// Here are the json array sizes.
			{
					{ "scenes", 1 },
					{ "nodes", 1 },
					{ "animations", 1 },
					{ "meshes", 1 },
					{ "accessors", 13 },
					{ "bufferViews", 13 },
					{ "buffers", 1 },
			},
			// Here are the key-value pairs.
			{
					{ "major_version", 2 },
					{ "minor_version", 0 },
					{ "scene_name", "cube" },
					{ "filename", "cube" } } },
	{ "models/suzanne.glb",
			"this is example text",
			"Khronos glTF Blender I/O v4.3.47",
			"2.0",
			// Here are the array sizes.
			{
					{ "glb_data", 68908 },
					{ "nodes", 2 },
					{ "buffers", 1 },
					{ "buffer_views", 5 },
					{ "accessors", 4 },
					{ "meshes", 1 },
					{ "materials", 1 },
					{ "root_nodes", 2 },
					{ "textures", 1 },
					{ "texture_samplers", 1 },
					{ "images", 1 },
					{ "skins", 0 },
					{ "cameras", 1 },
					{ "lights", 0 },
					{ "unique_names", 4 },
					{ "skeletons", 0 },
					{ "animations", 0 },
			},
			// Here are the json array sizes.
			{
					{ "scenes", 1 },
					{ "nodes", 2 },
					{ "cameras", 1 },
					{ "materials", 1 },
					{ "meshes", 1 },
					{ "textures", 1 },
					{ "images", 1 },
					{ "accessors", 4 },
					{ "bufferViews", 5 },
					{ "buffers", 1 },
			},
			// Here are the key-value pairs.
			{
					{ "major_version", 2 },
					{ "minor_version", 0 },
					{ "scene_name", "suzanne" },
					{ "filename", "suzanne" } } },
};

void register_gltf_extension() {
	GLTFDocument::unregister_all_gltf_document_extensions();

	// Ensures meshes become a MeshInstance3D and not an ImporterMeshInstance3D.
	Ref<GLTFDocumentExtensionConvertImporterMesh> extension_GLTFDocumentExtensionConvertImporterMesh;
	extension_GLTFDocumentExtensionConvertImporterMesh.instantiate();
	GLTFDocument::register_gltf_document_extension(extension_GLTFDocumentExtensionConvertImporterMesh);
}

void test_gltf_document_values(Ref<GLTFDocument> &p_gltf_document, Ref<GLTFState> &p_gltf_state, const GLTFTestCase &p_test_case) {
	const Error err = p_gltf_document->append_from_file(TestUtils::get_data_path(p_test_case.filename), p_gltf_state);
	REQUIRE(err == OK);

	for (GLTFArraySize array_size : p_test_case.array_sizes) {
		CHECK_MESSAGE(((Array)(p_gltf_state->getvar(array_size.key))).size() == array_size.val, "Expected \"", array_size.key, "\" to have ", array_size.val, " elements.");
	}

	for (GLTFArraySize array_size : p_test_case.json_array_sizes) {
		CHECK(p_gltf_state->get_json().has(array_size.key));
		CHECK_MESSAGE(((Array)(p_gltf_state->get_json()[array_size.key])).size() == array_size.val, "Expected \"", array_size.key, "\" to have ", array_size.val, " elements.");
	}

	for (GLTFKeyValue key_value : p_test_case.keyvalues) {
		CHECK_MESSAGE(p_gltf_state->getvar(key_value.key) == key_value.val, "Expected \"", key_value.key, "\" to be \"", key_value.val, "\".");
	}

	CHECK(p_gltf_state->get_copyright() == p_test_case.copyright);
	CHECK(((Dictionary)p_gltf_state->get_json()["asset"])["generator"] == p_test_case.generator);
	CHECK(((Dictionary)p_gltf_state->get_json()["asset"])["version"] == p_test_case.version);
}

void test_gltf_save(Node *p_node) {
	Ref<GLTFDocument> gltf_document_save;
	gltf_document_save.instantiate();
	Ref<GLTFState> gltf_state_save;
	gltf_state_save.instantiate();

	gltf_document_save->append_from_scene(p_node, gltf_state_save);

	// Check saving the scene to gltf and glb.
	const Error err_save_gltf = gltf_document_save->write_to_filesystem(gltf_state_save, TestUtils::get_temp_path("cube.gltf"));
	const Error err_save_glb = gltf_document_save->write_to_filesystem(gltf_state_save, TestUtils::get_temp_path("cube.glb"));
	CHECK(err_save_gltf == OK);
	CHECK(err_save_glb == OK);
}

TEST_CASE("[SceneTree][GLTFDocument] Load cube.gltf") {
	register_gltf_extension();

	Ref<GLTFDocument> gltf_document;
	gltf_document.instantiate();
	Ref<GLTFState> gltf_state;
	gltf_state.instantiate();

	test_gltf_document_values(gltf_document, gltf_state, glTF_test_cases[0]);

	Node *node = gltf_document->generate_scene(gltf_state);

	// Check the loaded scene.
	CHECK(node->is_class("Node3D"));
	CHECK(node->get_name() == "cube");

	CHECK(node->get_child(0)->is_class("MeshInstance3D"));
	CHECK(node->get_child(0)->get_name() == "Cube");

	CHECK(node->get_child(1)->is_class("AnimationPlayer"));
	CHECK(node->get_child(1)->get_name() == "AnimationPlayer");

	test_gltf_save(node);

	// Clean up the node.
	memdelete(node);
}

TEST_CASE("[SceneTree][GLTFDocument] Load suzanne.glb") {
	register_gltf_extension();

	Ref<GLTFDocument> gltf_document;
	gltf_document.instantiate();
	Ref<GLTFState> gltf_state;
	gltf_state.instantiate();

	test_gltf_document_values(gltf_document, gltf_state, glTF_test_cases[1]);

	Node *node = gltf_document->generate_scene(gltf_state);

	// Check the loaded scene.
	CHECK(node->is_class("Node3D"));
	CHECK(node->get_name() == "suzanne");

	CHECK(node->get_child(0)->is_class("MeshInstance3D"));
	CHECK(node->get_child(0)->get_name() == "Suzanne");

	CHECK(node->get_child(1)->is_class("Camera3D"));
	CHECK(node->get_child(1)->get_name() == "Camera");

	test_gltf_save(node);

	// Clean up the node.
	memdelete(node);
}

} // namespace TestGLTFDocument
