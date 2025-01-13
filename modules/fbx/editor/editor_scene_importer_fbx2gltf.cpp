/**************************************************************************/
/*  editor_scene_importer_fbx2gltf.cpp                                    */
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

#include "editor_scene_importer_fbx2gltf.h"

#ifdef TOOLS_ENABLED

#include "core/config/project_settings.h"
#include "editor/editor_settings.h"
#include "editor_scene_importer_ufbx.h"

#include "modules/gltf/gltf_document.h"

uint32_t EditorSceneFormatImporterFBX2GLTF::get_import_flags() const {
	return ImportFlags::IMPORT_SCENE | ImportFlags::IMPORT_ANIMATION;
}

void EditorSceneFormatImporterFBX2GLTF::get_extensions(List<String> *r_extensions) const {
	r_extensions->push_back("fbx");
}

Node *EditorSceneFormatImporterFBX2GLTF::import_scene(const String &p_path, uint32_t p_flags,
		const HashMap<StringName, Variant> &p_options,
		List<String> *r_missing_deps, Error *r_err) {
	// FIXME: Hack to work around GH-86309.
	if (p_options.has("fbx/importer") && int(p_options["fbx/importer"]) == EditorSceneFormatImporterUFBX::FBX_IMPORTER_UFBX) {
		Ref<EditorSceneFormatImporterUFBX> fbx2gltf_importer;
		fbx2gltf_importer.instantiate();
		Node *scene = fbx2gltf_importer->import_scene(p_path, p_flags, p_options, r_missing_deps, r_err);
		if (r_err && *r_err == OK) {
			return scene;
		} else {
			return nullptr;
		}
	}

	// Get global paths for source and sink.

	// Don't use `c_escape()` as it can generate broken paths. These paths will be
	// enclosed in double quotes by OS::execute(), so we only need to escape those.
	// `c_escape_multiline()` seems to do this (escapes `\` and `"` only).
	const String source_global = ProjectSettings::get_singleton()->globalize_path(p_path).c_escape_multiline();
	const String sink = ProjectSettings::get_singleton()->get_imported_files_path().path_join(
			vformat("%s-%s.glb", p_path.get_file().get_basename(), p_path.md5_text()));
	const String sink_global = ProjectSettings::get_singleton()->globalize_path(sink).c_escape_multiline();

	// Run fbx2gltf.

	String fbx2gltf_path = EDITOR_GET("filesystem/import/fbx/fbx2gltf_path");

	List<String> args;
	args.push_back("--pbr-metallic-roughness");
	args.push_back("--input");
	args.push_back(source_global);
	args.push_back("--output");
	args.push_back(sink_global);
	args.push_back("--binary");

	String standard_out;
	int ret;
	OS::get_singleton()->execute(fbx2gltf_path, args, &standard_out, &ret, true);
	print_verbose(fbx2gltf_path);
	print_verbose(standard_out);

	if (ret != 0) {
		if (r_err) {
			*r_err = ERR_SCRIPT_FAILED;
		}
		ERR_PRINT(vformat("FBX conversion to glTF failed with error: %d.", ret));
		return nullptr;
	}

	// Import the generated glTF.

	// Use GLTFDocument instead of glTF importer to keep image references.
	Ref<GLTFDocument> gltf;
	gltf.instantiate();
	Ref<GLTFState> state;
	state.instantiate();
	if (p_options.has(SNAME("nodes/import_as_skeleton_bones")) ? (bool)p_options[SNAME("nodes/import_as_skeleton_bones")] : false) {
		state->set_import_as_skeleton_bones(true);
	}
	print_verbose(vformat("glTF path: %s", sink));
	Error err = gltf->append_from_file(sink, state, p_flags, p_path.get_base_dir());
	if (err != OK) {
		if (r_err) {
			*r_err = FAILED;
		}
		return nullptr;
	}

#ifndef DISABLE_DEPRECATED
	bool trimming = p_options.has("animation/trimming") ? (bool)p_options["animation/trimming"] : false;
	return gltf->generate_scene(state, (float)p_options["animation/fps"], trimming, false);
#else
	return gltf->generate_scene(state, (float)p_options["animation/fps"], (bool)p_options["animation/trimming"], false);
#endif
}

Variant EditorSceneFormatImporterFBX2GLTF::get_option_visibility(const String &p_path, const String &p_scene_import_type,
		const String &p_option, const HashMap<StringName, Variant> &p_options) {
	// Remove all the FBX options except for 'fbx/importer' if the importer is fbx2gltf.
	// These options are available only for ufbx.
	if (p_option.begins_with("fbx/") && p_option != "fbx/importer" && p_options.has("fbx/importer") && int(p_options["fbx/importer"]) == EditorSceneFormatImporterUFBX::FBX_IMPORTER_FBX2GLTF) {
		return false;
	}
	return true;
}

#define ADD_OPTION_ENUM(PATH, ENUM_HINT, VALUE) \
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, SNAME(PATH), PROPERTY_HINT_ENUM, ENUM_HINT), VALUE));

void EditorSceneFormatImporterFBX2GLTF::get_import_options(const String &p_path,
		List<ResourceImporter::ImportOption> *r_options) {
}

void EditorSceneFormatImporterFBX2GLTF::handle_compatibility_options(HashMap<StringName, Variant> &p_import_params) const {
	if (!p_import_params.has("fbx/importer")) {
		p_import_params["fbx/importer"] = EditorSceneFormatImporterUFBX::FBX_IMPORTER_UFBX;
	}
}

#endif // TOOLS_ENABLED
