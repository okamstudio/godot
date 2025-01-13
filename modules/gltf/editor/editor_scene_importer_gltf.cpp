/**************************************************************************/
/*  editor_scene_importer_gltf.cpp                                        */
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

#include "editor_scene_importer_gltf.h"

#ifdef TOOLS_ENABLED

#include "../gltf_defines.h"
#include "../gltf_document.h"

uint32_t EditorSceneFormatImporterGLTF::get_import_flags() const {
	return ImportFlags::IMPORT_SCENE | ImportFlags::IMPORT_ANIMATION;
}

void EditorSceneFormatImporterGLTF::get_extensions(List<String> *r_extensions) const {
	r_extensions->push_back("gltf");
	r_extensions->push_back("glb");
}

Node *EditorSceneFormatImporterGLTF::import_scene(const String &p_path, uint32_t p_flags,
		const HashMap<StringName, Variant> &p_options,
		List<String> *r_missing_deps, Error *r_err) {
	Ref<GLTFDocument> gltf;
	gltf.instantiate();
	Ref<GLTFState> state;
	state.instantiate();
	if (p_options.has("gltf/naming_version")) {
		int naming_version = p_options["gltf/naming_version"];
		gltf->set_naming_version(naming_version);
	}
	if (p_options.has("gltf/embedded_image_handling")) {
		int32_t enum_option = p_options["gltf/embedded_image_handling"];
		state->set_handle_binary_image(enum_option);
	}
	if (p_options.has(SNAME("nodes/import_as_skeleton_bones")) ? (bool)p_options[SNAME("nodes/import_as_skeleton_bones")] : false) {
		state->set_import_as_skeleton_bones(true);
	}
	if (p_options.has(SNAME("extract_path"))) {
		state->set_extract_path(p_options["extract_path"]);
	}
	state->set_bake_fps(p_options["animation/fps"]);
	Error err = gltf->append_from_file(p_path, state, p_flags);
	if (err != OK) {
		if (r_err) {
			*r_err = err;
		}
		return nullptr;
	}
	if (p_options.has("animation/import")) {
		state->set_create_animations(bool(p_options["animation/import"]));
	}

#ifndef DISABLE_DEPRECATED
	bool trimming = p_options.has("animation/trimming") ? (bool)p_options["animation/trimming"] : false;
	return gltf->generate_scene(state, state->get_bake_fps(), trimming, false);
#else
	return gltf->generate_scene(state, state->get_bake_fps(), (bool)p_options["animation/trimming"], false);
#endif
}

void EditorSceneFormatImporterGLTF::get_import_options(const String &p_path,
		List<ResourceImporter::ImportOption> *r_options) {
	String file_extension = p_path.get_extension().to_lower();
	// Returns all the options when path is empty because that means it's for the Project Settings.
	if (p_path.is_empty() || file_extension == "gltf" || file_extension == "glb") {
		r_options->push_back(ResourceImporterScene::ImportOption(PropertyInfo(Variant::INT, "gltf/naming_version", PROPERTY_HINT_ENUM, "Godot 4.1 or 4.0,Godot 4.2 or later"), 1));
		r_options->push_back(ResourceImporterScene::ImportOption(PropertyInfo(Variant::INT, "gltf/embedded_image_handling", PROPERTY_HINT_ENUM, "Discard All Textures,Extract Textures,Embed as Basis Universal,Embed as Uncompressed", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), GLTFState::HANDLE_BINARY_EXTRACT_TEXTURES));
	}
}

void EditorSceneFormatImporterGLTF::handle_compatibility_options(HashMap<StringName, Variant> &p_import_params) const {
	if (!p_import_params.has("gltf/naming_version")) {
		// If an existing import file is missing the glTF
		// compatibility version, we need to use version 0.
		p_import_params["gltf/naming_version"] = 0;
	}
}

Variant EditorSceneFormatImporterGLTF::get_option_visibility(const String &p_path, const String &p_scene_import_type,
		const String &p_option, const HashMap<StringName, Variant> &p_options) {
	return true;
}

#endif // TOOLS_ENABLED
