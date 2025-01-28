/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "fbx_document.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_scene_importer_fbx2gltf.h"
#include "editor/editor_scene_importer_ufbx.h"

#include "core/config/project_settings.h"
#include "editor/editor_node.h"

static void _editor_init() {
	Ref<EditorSceneFormatImporterUFBX> import_fbx;
	import_fbx.instantiate();
	ResourceImporterScene::add_scene_importer(import_fbx);

	bool fbx2gltf_enabled = GLOBAL_GET("filesystem/import/fbx2gltf/enabled");
	if (fbx2gltf_enabled) {
		Ref<EditorSceneFormatImporterFBX2GLTF> importer;
		importer.instantiate();
		ResourceImporterScene::add_scene_importer(importer);
	}
}
#endif // TOOLS_ENABLED

void initialize_fbx_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(FBXDocument);
		GDREGISTER_CLASS(FBXState);
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		// Editor-specific API.
		ClassDB::APIType prev_api = ClassDB::get_current_api();
		ClassDB::set_current_api(ClassDB::API_EDITOR);

		GDREGISTER_CLASS(EditorSceneFormatImporterUFBX);

		GLOBAL_DEF_RST_BASIC("filesystem/import/fbx2gltf/enabled", true);
		GDREGISTER_CLASS(EditorSceneFormatImporterFBX2GLTF);
		GLOBAL_DEF_RST("filesystem/import/fbx2gltf/enabled.android", false);
		GLOBAL_DEF_RST("filesystem/import/fbx2gltf/enabled.web", false);

		ClassDB::set_current_api(prev_api);
		EditorNode::add_init_callback(_editor_init);
	}
#endif // TOOLS_ENABLED
}

void uninitialize_fbx_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	// TODO: 20240118 // fire
	// FBXDocument::unregister_all_gltf_document_extensions();
}
