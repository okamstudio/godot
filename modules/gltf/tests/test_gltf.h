/**************************************************************************/
/*  test_gltf.h                                                           */
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

#include "tests/test_macros.h"

#ifdef TOOLS_ENABLED

#include "core/os/os.h"
#include "drivers/png/image_loader_png.h"
#include "editor/editor_resource_preview.h"
#include "editor/import/3d/resource_importer_scene.h"
#include "editor/import/resource_importer_texture.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/main/window.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/compressed_texture.h"
#include "scene/resources/material.h"
#include "scene/resources/packed_scene.h"
#include "tests/core/config/test_project_settings.h"

#include "modules/gltf/editor/editor_scene_importer_gltf.h"
#include "modules/gltf/gltf_document.h"
#include "modules/gltf/gltf_state.h"

namespace TestGltf {

static Node *gltf_import(const String &p_file) {
	// Setting up importers.
	Ref<ResourceImporterScene> import_scene;
	import_scene.instantiate("PackedScene", true);
	ResourceFormatImporter::get_singleton()->add_importer(import_scene);
	Ref<EditorSceneFormatImporterGLTF> import_gltf;
	import_gltf.instantiate();
	ResourceImporterScene::add_scene_importer(import_gltf);

	// Support processing png files in editor import.
	Ref<ResourceImporterTexture> import_texture;
	import_texture.instantiate(true);
	ResourceFormatImporter::get_singleton()->add_importer(import_texture);

	// Once editor import convert pngs to ctex, we will need to load it as ctex resource.
	Ref<ResourceFormatLoaderCompressedTexture2D> resource_loader_stream_texture;
	resource_loader_stream_texture.instantiate();
	ResourceLoader::add_resource_format_loader(resource_loader_stream_texture);

	HashMap<StringName, Variant> options(21);
	options["nodes/root_type"] = "";
	options["nodes/root_name"] = "";
	options["nodes/apply_root_scale"] = true;
	options["nodes/root_scale"] = 1.0;
	options["meshes/ensure_tangents"] = true;
	options["meshes/generate_lods"] = false;
	options["meshes/create_shadow_meshes"] = true;
	options["meshes/light_baking"] = 1;
	options["meshes/lightmap_texel_size"] = 0.2;
	options["meshes/force_disable_compression"] = false;
	options["skins/use_named_skins"] = true;
	options["animation/import"] = true;
	options["animation/fps"] = 30;
	options["animation/trimming"] = false;
	options["animation/remove_immutable_tracks"] = true;
	options["import_script/path"] = "";
	options["extract_path"] = "res://";
	options["_subresources"] = Dictionary();
	options["gltf/naming_version"] = 1;

	// Process gltf file, note that this generates `.scn` resource from the 2nd argument.
	String scene_file = "res://" + p_file.get_file().get_basename();
	Error err = import_scene->import(0, p_file, scene_file, options, nullptr, nullptr, nullptr);
	CHECK_MESSAGE(err == OK, "GLTF import failed.");

	Ref<PackedScene> packed_scene = ResourceLoader::load(scene_file + ".scn", "", ResourceFormatLoader::CACHE_MODE_REPLACE, &err);
	CHECK_MESSAGE(err == OK, "Loading scene failed.");
	Node *p_scene = packed_scene->instantiate();

	ResourceImporterScene::remove_scene_importer(import_gltf);
	ResourceFormatImporter::get_singleton()->remove_importer(import_texture);
	ResourceLoader::remove_resource_format_loader(resource_loader_stream_texture);
	return p_scene;
}

static Node *gltf_export_then_import(Node *p_root, const String &p_test_name) {
	String tempfile = TestUtils::get_temp_path(p_test_name);

	Ref<GLTFDocument> doc;
	doc.instantiate();
	Ref<GLTFState> state;
	state.instantiate();
	Error err = doc->append_from_scene(p_root, state, EditorSceneFormatImporter::IMPORT_USE_NAMED_SKIN_BINDS);
	CHECK_MESSAGE(err == OK, "GLTF state generation failed.");

	err = doc->write_to_filesystem(state, tempfile + ".gltf");
	CHECK_MESSAGE(err == OK, "Writing GLTF to cache dir failed.");

	return gltf_import(tempfile + ".gltf");
}

void init(const String &p_test, const String &p_copy_target = String()) {
	Error err;

	// Setup project settings since it's needed for the import process.
	String project_folder = TestUtils::get_temp_path(p_test.get_file().get_basename());
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(project_folder.path_join(".godot").path_join("imported"));
	// Initialize res:// to `project_folder`.
	TestProjectSettingsInternalsAccessor::resource_path() = project_folder;
	err = ProjectSettings::get_singleton()->setup(project_folder, String(), true);

	if (p_copy_target.is_empty()) {
		return;
	}

	// Copy all the necessary test data files to the res:// directory.
	da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	String test_data = String("modules/gltf/tests/data/").path_join(p_test);
	da = DirAccess::open(test_data);
	CHECK_MESSAGE(da.is_valid(), "Unable to open folder.");
	da->list_dir_begin();
	for (String item = da->get_next(); !item.is_empty(); item = da->get_next()) {
		if (!FileAccess::exists(test_data.path_join(item))) {
			continue;
		}
		Ref<FileAccess> output = FileAccess::open(p_copy_target.path_join(item), FileAccess::WRITE, &err);
		CHECK_MESSAGE(err == OK, "Unable to open output file.");
		output->store_buffer(FileAccess::get_file_as_bytes(test_data.path_join(item)));
		output->close();
	}
	da->list_dir_end();
}

} //namespace TestGltf

#endif // TOOLS_ENABLED
