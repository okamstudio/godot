/**************************************************************************/
/*  editor_scene_exporter_gltf_settings.cpp                               */
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

#include "editor_scene_exporter_gltf_settings.h"

const uint32_t PROP_EDITOR_SCRIPT_VAR = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_SCRIPT_VARIABLE;

bool EditorSceneExporterGLTFSettings::_set(const StringName &p_name, const Variant &p_value) {
	String name_str = String(p_name);
	if (name_str.contains_char('/')) {
		return _set_extension_setting(name_str, p_value);
	}
	if (p_name == StringName("image_format")) {
		_document->set_image_format(p_value);
		emit_signal(CoreStringName(property_list_changed));
		return true;
	}
	if (p_name == StringName("lossy_quality")) {
		_document->set_lossy_quality(p_value);
		return true;
	}
	if (p_name == StringName("root_node_mode")) {
		_document->set_root_node_mode((GLTFDocument::RootNodeMode)(int64_t)p_value);
		return true;
	}
	return false;
}

bool EditorSceneExporterGLTFSettings::_get(const StringName &p_name, Variant &r_ret) const {
	String name_str = String(p_name);
	if (name_str.contains_char('/')) {
		return _get_extension_setting(name_str, r_ret);
	}
	if (p_name == StringName("image_format")) {
		r_ret = _document->get_image_format();
		return true;
	}
	if (p_name == StringName("lossy_quality")) {
		r_ret = _document->get_lossy_quality();
		return true;
	}
	if (p_name == StringName("root_node_mode")) {
		r_ret = _document->get_root_node_mode();
		return true;
	}
	return false;
}

void EditorSceneExporterGLTFSettings::_get_property_list(List<PropertyInfo> *p_list) const {
	for (PropertyInfo prop : _property_list) {
		if (prop.name == "lossy_quality") {
			String image_format = get("image_format");
			bool is_image_format_lossy = image_format == "JPEG" || image_format.containsn("Lossy");
			prop.usage = is_image_format_lossy ? PROPERTY_USAGE_DEFAULT : PROPERTY_USAGE_STORAGE;
		}
		p_list->push_back(prop);
	}
}

void EditorSceneExporterGLTFSettings::_on_extension_property_list_changed() {
	generate_property_list(_document);
	emit_signal(CoreStringName(property_list_changed));
}

bool EditorSceneExporterGLTFSettings::_set_extension_setting(const String &p_name_str, const Variant &p_value) {
	PackedStringArray split = String(p_name_str).split("/", true, 1);
	if (!_config_name_to_extension_map.has(split[0])) {
		return false;
	}
	Ref<GLTFDocumentExtension> extension = _config_name_to_extension_map[split[0]];
	bool valid;
	extension->set(split[1], p_value, &valid);
	return valid;
}

bool EditorSceneExporterGLTFSettings::_get_extension_setting(const String &p_name_str, Variant &r_ret) const {
	PackedStringArray split = String(p_name_str).split("/", true, 1);
	if (!_config_name_to_extension_map.has(split[0])) {
		return false;
	}
	Ref<GLTFDocumentExtension> extension = _config_name_to_extension_map[split[0]];
	bool valid;
	r_ret = extension->get(split[1], &valid);
	return valid;
}

String get_friendly_config_prefix(Ref<GLTFDocumentExtension> p_extension) {
	String config_prefix = p_extension->get_name();
	if (!config_prefix.is_empty()) {
		return config_prefix;
	}
	const String class_name = p_extension->get_class_name();
	config_prefix = class_name.trim_prefix("GLTFDocumentExtension").capitalize();
	if (!config_prefix.is_empty()) {
		return config_prefix;
	}
	PackedStringArray supported_extensions = p_extension->get_supported_extensions();
	if (supported_extensions.size() > 0) {
		return supported_extensions[0];
	}
	return "Unknown GLTFDocumentExtension";
}

// Run this before popping up the export settings, because the extensions may have changed.
void EditorSceneExporterGLTFSettings::generate_property_list(Ref<GLTFDocument> p_document, Node *p_root) {
	_property_list.clear();
	_document = p_document;
	String image_format_hint_string = "None,PNG,JPEG";
	// Add properties from all document extensions.
	for (Ref<GLTFDocumentExtension> &extension : GLTFDocument::get_all_gltf_document_extensions()) {
		const Callable on_prop_changed = callable_mp(this, &EditorSceneExporterGLTFSettings::_on_extension_property_list_changed);
		if (!extension->is_connected(CoreStringName(property_list_changed), on_prop_changed)) {
			extension->connect(CoreStringName(property_list_changed), on_prop_changed);
		}
		const String config_prefix = get_friendly_config_prefix(extension);
		_config_name_to_extension_map[config_prefix] = extension;
		// If the extension allows saving in different image formats, add to the enum.
		PackedStringArray saveable_image_formats = extension->get_saveable_image_formats();
		for (int i = 0; i < saveable_image_formats.size(); i++) {
			image_format_hint_string += "," + saveable_image_formats[i];
		}
		// Look through the extension's properties and find the relevant ones.
		List<PropertyInfo> ext_prop_list;
		extension->get_property_list(&ext_prop_list);
		for (const PropertyInfo &prop : ext_prop_list) {
			// We only want properties that will show up in the exporter
			// settings list. Exclude Resource's properties, as they are
			// not relevant to the exporter. Include any user-defined script
			// variables exposed to the editor (PROP_EDITOR_SCRIPT_VAR).
			if ((prop.usage & PROP_EDITOR_SCRIPT_VAR) == PROP_EDITOR_SCRIPT_VAR) {
				PropertyInfo ext_prop = prop;
				ext_prop.name = config_prefix + "/" + prop.name;
				_property_list.push_back(ext_prop);
			}
		}
	}
	// Add top-level properties (in addition to what _bind_methods registers).
	PropertyInfo image_format_prop = PropertyInfo(Variant::STRING, "image_format", PROPERTY_HINT_ENUM, image_format_hint_string);
	_property_list.push_back(image_format_prop);
	PropertyInfo lossy_quality_prop = PropertyInfo(Variant::FLOAT, "lossy_quality", PROPERTY_HINT_RANGE, "0,1,0.01");
	_property_list.push_back(lossy_quality_prop);
	PropertyInfo root_node_mode_prop = PropertyInfo(Variant::INT, "root_node_mode", PROPERTY_HINT_ENUM, "Single Root,Keep Root,Multi Root");
	_property_list.push_back(root_node_mode_prop);
}

String EditorSceneExporterGLTFSettings::get_copyright() const {
	return _copyright;
}

void EditorSceneExporterGLTFSettings::set_copyright(const String &p_copyright) {
	_copyright = p_copyright;
}

void EditorSceneExporterGLTFSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_copyright"), &EditorSceneExporterGLTFSettings::get_copyright);
	ClassDB::bind_method(D_METHOD("set_copyright", "copyright"), &EditorSceneExporterGLTFSettings::set_copyright);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "copyright", PROPERTY_HINT_PLACEHOLDER_TEXT, "Example: 2014 Godette"), "set_copyright", "get_copyright");

	ClassDB::bind_method(D_METHOD("get_bake_fps"), &EditorSceneExporterGLTFSettings::get_bake_fps);
	ClassDB::bind_method(D_METHOD("set_bake_fps", "bake_fps"), &EditorSceneExporterGLTFSettings::set_bake_fps);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bake_fps"), "set_bake_fps", "get_bake_fps");
}

double EditorSceneExporterGLTFSettings::get_bake_fps() const {
	return _bake_fps;
}

void EditorSceneExporterGLTFSettings::set_bake_fps(const double p_bake_fps) {
	_bake_fps = p_bake_fps;
}
