/**************************************************************************/
/*  dedicated_server_export_plugin.cpp                                    */
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

#include "dedicated_server_export_plugin.h"

EditorExportPreset::FileExportMode DedicatedServerExportPlugin::_get_export_mode_for_path(const String &p_path) {
	Ref<EditorExportPreset> preset = get_export_preset();
	ERR_FAIL_COND_V(preset.is_null(), EditorExportPreset::MODE_FILE_NOT_CUSTOMIZED);

	EditorExportPreset::FileExportMode mode = preset->get_file_export_mode(p_path);
	if (mode != EditorExportPreset::MODE_FILE_NOT_CUSTOMIZED) {
		return mode;
	}

	String path = p_path;
	if (path.begins_with("res://")) {
		path = path.substr(6);
	}

	Vector<String> parts = path.split("/");

	while (parts.size() > 0) {
		parts.resize(parts.size() - 1);

		String test_path = "res://";
		if (parts.size() > 0) {
			test_path += String("/").join(parts) + "/";
		}

		mode = preset->get_file_export_mode(test_path);
		if (mode != EditorExportPreset::MODE_FILE_NOT_CUSTOMIZED) {
			break;
		}
	}

	return mode;
}

PackedStringArray DedicatedServerExportPlugin::_get_export_features(const Ref<EditorExportPlatform> &p_platform, bool p_debug) const {
	PackedStringArray ret;

	Ref<EditorExportPreset> preset = get_export_preset();
	ERR_FAIL_COND_V(preset.is_null(), ret);

	if (preset->is_dedicated_server()) {
		ret.append("dedicated_server");
	}
	return ret;
}

uint64_t DedicatedServerExportPlugin::_get_customization_configuration_hash() const {
	Ref<EditorExportPreset> preset = get_export_preset();
	ERR_FAIL_COND_V(preset.is_null(), 0);

	if (preset->get_export_filter() != EditorExportPreset::EXPORT_CUSTOMIZED) {
		return 0;
	}

	return preset->get_customized_files().hash();
}

bool DedicatedServerExportPlugin::_begin_customize_scenes(const Ref<EditorExportPlatform> &p_platform, const Vector<String> &p_features) {
	Ref<EditorExportPreset> preset = get_export_preset();
	ERR_FAIL_COND_V(preset.is_null(), false);

	current_export_mode = EditorExportPreset::MODE_FILE_NOT_CUSTOMIZED;

	return preset->get_export_filter() == EditorExportPreset::EXPORT_CUSTOMIZED;
}

bool DedicatedServerExportPlugin::_begin_customize_resources(const Ref<EditorExportPlatform> &p_platform, const Vector<String> &p_features) {
	Ref<EditorExportPreset> preset = get_export_preset();
	ERR_FAIL_COND_V(preset.is_null(), false);

	current_export_mode = EditorExportPreset::MODE_FILE_NOT_CUSTOMIZED;

	return preset->get_export_filter() == EditorExportPreset::EXPORT_CUSTOMIZED;
}

Node *DedicatedServerExportPlugin::_customize_scene(Node *p_root, const String &p_path) {
	// Simply set the export mode based on the scene path. All the real
	// customization happens in _customize_resource().
	current_export_mode = _get_export_mode_for_path(p_path);
	return nullptr;
}

Ref<Resource> DedicatedServerExportPlugin::_customize_resource(const Ref<Resource> &p_resource, const String &p_path) {
	// If the resource has a path, we use that to get our export mode. But if it
	// doesn't, we assume that this resource is embedded in the last resource with
	// a path.
	if (p_path != "") {
		current_export_mode = _get_export_mode_for_path(p_path);
	}

	if (p_resource.is_valid() && current_export_mode == EditorExportPreset::MODE_FILE_STRIP && p_resource->has_method("create_placeholder")) {
		Callable::CallError err;
		Ref<Resource> result = p_resource->callp("create_placeholder", nullptr, 0, err);
		if (err.error == Callable::CallError::CALL_OK) {
			return result;
		}
	}

	return Ref<Resource>();
}

void DedicatedServerExportPlugin::_end_customize_scenes() {
	current_export_mode = EditorExportPreset::MODE_FILE_NOT_CUSTOMIZED;
}

void DedicatedServerExportPlugin::_end_customize_resources() {
	current_export_mode = EditorExportPreset::MODE_FILE_NOT_CUSTOMIZED;
}
