/**************************************************************************/
/*  editor_scene_importer_ufbx.cpp                                        */
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

#include "editor_scene_importer_ufbx.h"

#ifdef TOOLS_ENABLED

#include "../fbx_document.h"
#include "editor_scene_importer_fbx2gltf.h"

#include "core/config/project_settings.h"

void EditorSceneFormatImporterUFBX::get_extensions(List<String> *r_extensions) const {
	r_extensions->push_back("fbx");
}

Node *EditorSceneFormatImporterUFBX::import_scene(const String &p_path, uint32_t p_flags,
		const HashMap<StringName, Variant> &p_options,
		List<String> *r_missing_deps, Error *r_err) {
	// FIXME: Hack to work around GH-86309.
	if (p_options.has("fbx/importer") && int(p_options["fbx/importer"]) == FBX_IMPORTER_FBX2GLTF && GLOBAL_GET("filesystem/import/fbx2gltf/enabled")) {
		Ref<EditorSceneFormatImporterFBX2GLTF> fbx2gltf_importer;
		fbx2gltf_importer.instantiate();
		Node *scene = fbx2gltf_importer->import_scene(p_path, p_flags, p_options, r_missing_deps, r_err);
		if (r_err && *r_err == OK) {
			return scene;
		} else {
			return nullptr;
		}
	}
	Ref<FBXDocument> fbx;
	fbx.instantiate();
	Ref<FBXState> state;
	state.instantiate();
	print_verbose(vformat("FBX path: %s", p_path));
	String path = ProjectSettings::get_singleton()->globalize_path(p_path);
	bool allow_geometry_helper_nodes = p_options.has("fbx/allow_geometry_helper_nodes") ? (bool)p_options["fbx/allow_geometry_helper_nodes"] : false;
	if (allow_geometry_helper_nodes) {
		state->set_allow_geometry_helper_nodes(allow_geometry_helper_nodes);
	}
	if (p_options.has("fbx/embedded_image_handling")) {
		int32_t enum_option = p_options["fbx/embedded_image_handling"];
		state->set_handle_binary_image(enum_option);
	}
	if (p_options.has(SNAME("nodes/import_as_skeleton_bones")) ? (bool)p_options[SNAME("nodes/import_as_skeleton_bones")] : false) {
		state->set_import_as_skeleton_bones(true);
	}
	p_flags |= EditorSceneFormatImporter::IMPORT_USE_NAMED_SKIN_BINDS;
	state->set_bake_fps(p_options["animation/fps"]);
	Error err = fbx->append_from_file(path, state, p_flags, p_path.get_base_dir());
	if (err != OK) {
		if (r_err) {
			*r_err = FAILED;
		}
		return nullptr;
	}
	return fbx->generate_scene(state, state->get_bake_fps(), (bool)p_options["animation/trimming"], false);
}

Variant EditorSceneFormatImporterUFBX::get_option_visibility(const String &p_path, const String &p_scene_import_type,
		const String &p_option, const HashMap<StringName, Variant> &p_options) {
	return true;
}

void EditorSceneFormatImporterUFBX::get_import_options(const String &p_path,
		List<ResourceImporter::ImportOption> *r_options) {
	// Returns all the options when path is empty because that means it's for the Project Settings.
	if (p_path.is_empty() || p_path.get_extension().to_lower() == "fbx") {
		r_options->push_back(ResourceImporterScene::ImportOption(PropertyInfo(Variant::INT, "fbx/importer", PROPERTY_HINT_ENUM, "ufbx,FBX2glTF"), FBX_IMPORTER_UFBX));
		r_options->push_back(ResourceImporterScene::ImportOption(PropertyInfo(Variant::BOOL, "fbx/allow_geometry_helper_nodes"), false));
		r_options->push_back(ResourceImporterScene::ImportOption(PropertyInfo(Variant::INT, "fbx/embedded_image_handling", PROPERTY_HINT_ENUM, "Discard All Textures,Extract Textures,Embed as Basis Universal,Embed as Uncompressed", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), FBXState::HANDLE_BINARY_EXTRACT_TEXTURES));
	}
}

void EditorSceneFormatImporterUFBX::handle_compatibility_options(HashMap<StringName, Variant> &p_import_params) const {
	if (!p_import_params.has("fbx/importer")) {
		p_import_params["fbx/importer"] = EditorSceneFormatImporterUFBX::FBX_IMPORTER_FBX2GLTF;
	}
}

#endif // TOOLS_ENABLED
