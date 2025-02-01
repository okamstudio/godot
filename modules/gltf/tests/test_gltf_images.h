/**************************************************************************/
/*  test_gltf_images.h                                                    */
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

#include "test_gltf.h"

#ifdef TOOLS_ENABLED

#include "editor/editor_file_system.h"
#include "editor/editor_paths.h"
#include "scene/resources/image_texture.h"

namespace TestGltf {
Ref<Texture2D> _check_texture(Node *p_node) {
	MeshInstance3D *mesh_instance_3d = Object::cast_to<MeshInstance3D>(p_node->find_child("mesh_instance_3d", true, true));
	Ref<StandardMaterial3D> material = mesh_instance_3d->get_active_material(0);
	Ref<Texture2D> texture = material->get_texture(StandardMaterial3D::TextureParam::TEXTURE_ALBEDO);

	CHECK_MESSAGE(texture->get_size().x == 2, "Texture width not correct.");
	CHECK_MESSAGE(texture->get_size().y == 2, "Texture height not correct.");

	// Check if the loaded texture pixels are exactly as we expect.
	for (int x = 0; x < 2; ++x) {
		for (int y = 0; y < 2; ++y) {
			Color c = texture->get_image()->get_pixel(x, y);
			CHECK_MESSAGE(c == Color(x, y, y), "Texture content is incorrect.");
		}
	}
	return texture;
}

TEST_CASE("[SceneTree][Node] Export GLTF with external texture and import") {
	init("gltf_images_external_export_import");
	// Setup scene.
	Ref<ImageTexture> original_texture;
	original_texture.instantiate();
	Ref<Image> image;
	image.instantiate();
	image->initialize_data(2, 2, false, Image::FORMAT_RGBA8);
	for (int x = 0; x < 2; ++x) {
		for (int y = 0; y < 2; ++y) {
			image->set_pixel(x, y, Color(x, y, y));
		}
	}

	original_texture->set_image(image);

	Ref<StandardMaterial3D> original_material;
	original_material.instantiate();
	original_material->set_texture(StandardMaterial3D::TextureParam::TEXTURE_ALBEDO, original_texture);
	original_material->set_name("material");

	Ref<PlaneMesh> original_meshdata;
	original_meshdata.instantiate();
	original_meshdata->set_name("planemesh");
	original_meshdata->surface_set_material(0, original_material);

	MeshInstance3D *original_mesh_instance = memnew(MeshInstance3D);
	original_mesh_instance->set_mesh(original_meshdata);
	original_mesh_instance->set_name("mesh_instance_3d");

	Node3D *original = memnew(Node3D);
	SceneTree::get_singleton()->get_root()->add_child(original);
	original->add_child(original_mesh_instance);
	original->set_owner(SceneTree::get_singleton()->get_root());
	original_mesh_instance->set_owner(SceneTree::get_singleton()->get_root());

	// Convert to GLFT and back.
	Node *loaded = gltf_export_then_import(original, "gltf_images");
	_check_texture(loaded);

	memdelete(original_mesh_instance);
	memdelete(original);
	memdelete(loaded);
}

TEST_CASE("[SceneTree][Node][Editor] Import GLTF from .godot/imported folder with external texture") {
	init("gltf_placed_in_dot_godot_imported", "res://.godot/imported");

	EditorFileSystem *efs = memnew(EditorFileSystem);
	EditorResourcePreview *erp = memnew(EditorResourcePreview);

	Node *loaded = gltf_import("res://.godot/imported/gltf_placed_in_dot_godot_imported.gltf");
	Ref<Texture2D> texture = _check_texture(loaded);

	// In-editor imports of gltf and texture from .godot/imported folder should end up in res:// if extract_path is defined.
	CHECK_MESSAGE(texture->get_path() == "res://gltf_placed_in_dot_godot_imported_material_albedo000.png", "Texture not parsed as resource.");

	memdelete(loaded);
	memdelete(erp);
	memdelete(efs);
}

TEST_CASE("[SceneTree][Node][Editor] Import GLTF with texture outside of res:// directory") {
	init("gltf_pointing_to_texture_outside_of_res_folder", "res://");

	EditorFileSystem *efs = memnew(EditorFileSystem);
	EditorResourcePreview *erp = memnew(EditorResourcePreview);

	// Copy texture to the parent folder of res:// - i.e. to res://.. where we can't import from.
	String oneup = TestUtils::get_temp_path("texture.png");
	Error err;
	Ref<FileAccess> output = FileAccess::open(oneup, FileAccess::WRITE, &err);
	CHECK_MESSAGE(err == OK, "Unable to open texture file.");
	output->store_buffer(FileAccess::get_file_as_bytes("res://texture_source.png"));
	output->close();

	Node *loaded = gltf_import("res://gltf_pointing_to_texture_outside_of_res_folder.gltf");
	Ref<Texture2D> texture = _check_texture(loaded);

	// Imports of gltf with texture from outside of res:// folder should end up being copied to res://
	CHECK_MESSAGE(texture->get_path() == "res://gltf_pointing_to_texture_outside_of_res_folder_material_albedo000.png", "Texture not parsed as resource.");

	memdelete(loaded);
	memdelete(erp);
	memdelete(efs);
}

TEST_CASE("[SceneTree][Node][Editor] Import GLTF with embedded texture, check how it got extracted") {
	init("gltf_embedded_texture", "res://");

	EditorFileSystem *efs = memnew(EditorFileSystem);
	EditorResourcePreview *erp = memnew(EditorResourcePreview);

	Node *loaded = gltf_import("res://embedded_texture.gltf");
	Ref<Texture2D> texture = _check_texture(loaded);

	// In-editor imports of texture embedded in file should end up with a resource.
	CHECK_MESSAGE(texture->get_path() == "res://embedded_texture_material_albedo000.png", "Texture not parsed as resource.");

	memdelete(loaded);
	memdelete(erp);
	memdelete(efs);
}

} //namespace TestGltf

#endif // TOOLS_ENABLED
