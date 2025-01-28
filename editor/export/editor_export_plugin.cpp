/**************************************************************************/
/*  editor_export_plugin.cpp                                              */
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

#include "editor_export_plugin.h"

#include "core/config/project_settings.h"
#include "editor/export/editor_export_platform.h"

void EditorExportPlugin::set_export_preset(const Ref<EditorExportPreset> &p_preset) {
	if (p_preset.is_valid()) {
		export_preset = p_preset;
	}
}

Ref<EditorExportPreset> EditorExportPlugin::get_export_preset() const {
	return export_preset;
}

Ref<EditorExportPlatform> EditorExportPlugin::get_export_platform() const {
	if (export_preset.is_valid()) {
		return export_preset->get_platform();
	} else {
		return Ref<EditorExportPlatform>();
	}
}

void EditorExportPlugin::add_file(const String &p_path, const Vector<uint8_t> &p_file, bool p_remap) {
	ExtraFile ef;
	ef.data = p_file;
	ef.path = p_path;
	ef.remap = p_remap;
	extra_files.push_back(ef);
}

void EditorExportPlugin::add_shared_object(const String &p_path, const Vector<String> &p_tags, const String &p_target) {
	shared_objects.push_back(SharedObject(p_path, p_tags, p_target));
}

void EditorExportPlugin::_add_shared_object(const SharedObject &p_shared_object) {
	shared_objects.push_back(p_shared_object);
}

void EditorExportPlugin::add_ios_framework(const String &p_path) {
	ios_frameworks.push_back(p_path);
}

void EditorExportPlugin::add_ios_embedded_framework(const String &p_path) {
	ios_embedded_frameworks.push_back(p_path);
}

Vector<String> EditorExportPlugin::get_ios_frameworks() const {
	return ios_frameworks;
}

Vector<String> EditorExportPlugin::get_ios_embedded_frameworks() const {
	return ios_embedded_frameworks;
}

void EditorExportPlugin::add_ios_plist_content(const String &p_plist_content) {
	ios_plist_content += p_plist_content + "\n";
}

String EditorExportPlugin::get_ios_plist_content() const {
	return ios_plist_content;
}

void EditorExportPlugin::add_ios_linker_flags(const String &p_flags) {
	if (ios_linker_flags.length() > 0) {
		ios_linker_flags += ' ';
	}
	ios_linker_flags += p_flags;
}

String EditorExportPlugin::get_ios_linker_flags() const {
	return ios_linker_flags;
}

void EditorExportPlugin::add_ios_bundle_file(const String &p_path) {
	ios_bundle_files.push_back(p_path);
}

Vector<String> EditorExportPlugin::get_ios_bundle_files() const {
	return ios_bundle_files;
}

void EditorExportPlugin::add_ios_cpp_code(const String &p_code) {
	ios_cpp_code += p_code;
}

String EditorExportPlugin::get_ios_cpp_code() const {
	return ios_cpp_code;
}

void EditorExportPlugin::add_macos_plugin_file(const String &p_path) {
	macos_plugin_files.push_back(p_path);
}

const Vector<String> &EditorExportPlugin::get_macos_plugin_files() const {
	return macos_plugin_files;
}

void EditorExportPlugin::add_ios_project_static_lib(const String &p_path) {
	ios_project_static_libs.push_back(p_path);
}

Vector<String> EditorExportPlugin::get_ios_project_static_libs() const {
	return ios_project_static_libs;
}

Variant EditorExportPlugin::get_option(const StringName &p_name) const {
	ERR_FAIL_COND_V(export_preset.is_null(), Variant());
	return export_preset->get(p_name);
}

String EditorExportPlugin::_has_valid_export_configuration(const Ref<EditorExportPlatform> &p_export_platform, const Ref<EditorExportPreset> &p_preset) {
	String warning;
	if (!supports_platform(p_export_platform)) {
		warning += vformat(TTR("Plugin \"%s\" is not supported on \"%s\""), get_name(), p_export_platform->get_name());
		warning += "\n";
		return warning;
	}

	set_export_preset(p_preset);
	List<EditorExportPlatform::ExportOption> options;
	_get_export_options(p_export_platform, &options);
	for (const EditorExportPlatform::ExportOption &E : options) {
		String option_warning = _get_export_option_warning(p_export_platform, E.option.name);
		if (!option_warning.is_empty()) {
			warning += option_warning + "\n";
		}
	}

	return warning;
}

void EditorExportPlugin::_export_file_script(const String &p_path, const String &p_type, const Vector<String> &p_features) {
	GDVIRTUAL_CALL(_export_file, p_path, p_type, p_features);
}

void EditorExportPlugin::_export_begin_script(const Vector<String> &p_features, bool p_debug, const String &p_path, int p_flags) {
	GDVIRTUAL_CALL(_export_begin, p_features, p_debug, p_path, p_flags);
}

void EditorExportPlugin::_export_end_script() {
	GDVIRTUAL_CALL(_export_end);
}

// Customization

bool EditorExportPlugin::_begin_customize_resources(const Ref<EditorExportPlatform> &p_platform, const Vector<String> &p_features) {
	bool ret = false;
	GDVIRTUAL_CALL(_begin_customize_resources, p_platform, p_features, ret);
	return ret;
}

Ref<Resource> EditorExportPlugin::_customize_resource(const Ref<Resource> &p_resource, const String &p_path) {
	Ref<Resource> ret;
	GDVIRTUAL_CALL(_customize_resource, p_resource, p_path, ret);
	return ret;
}

bool EditorExportPlugin::_begin_customize_scenes(const Ref<EditorExportPlatform> &p_platform, const Vector<String> &p_features) {
	bool ret = false;
	GDVIRTUAL_CALL(_begin_customize_scenes, p_platform, p_features, ret);
	return ret;
}

Node *EditorExportPlugin::_customize_scene(Node *p_root, const String &p_path) {
	Node *ret = nullptr;
	GDVIRTUAL_CALL(_customize_scene, p_root, p_path, ret);
	return ret;
}

uint64_t EditorExportPlugin::_get_customization_configuration_hash() const {
	uint64_t ret = 0;
	GDVIRTUAL_CALL(_get_customization_configuration_hash, ret);
	return ret;
}

void EditorExportPlugin::_end_customize_scenes() {
	GDVIRTUAL_CALL(_end_customize_scenes);
}

void EditorExportPlugin::_end_customize_resources() {
	GDVIRTUAL_CALL(_end_customize_resources);
}

String EditorExportPlugin::get_name() const {
	String ret;
	GDVIRTUAL_CALL(_get_name, ret);
	return ret;
}

bool EditorExportPlugin::supports_platform(const Ref<EditorExportPlatform> &p_export_platform) const {
	bool ret = false;
	GDVIRTUAL_CALL(_supports_platform, p_export_platform, ret);
	return ret;
}

PackedStringArray EditorExportPlugin::get_export_features(const Ref<EditorExportPlatform> &p_export_platform, bool p_debug) const {
	return _get_export_features(p_export_platform, p_debug);
}

PackedStringArray EditorExportPlugin::get_android_dependencies(const Ref<EditorExportPlatform> &p_export_platform, bool p_debug) const {
	PackedStringArray ret;
	GDVIRTUAL_CALL(_get_android_dependencies, p_export_platform, p_debug, ret);
	return ret;
}

PackedStringArray EditorExportPlugin::get_android_dependencies_maven_repos(const Ref<EditorExportPlatform> &p_export_platform, bool p_debug) const {
	PackedStringArray ret;
	GDVIRTUAL_CALL(_get_android_dependencies_maven_repos, p_export_platform, p_debug, ret);
	return ret;
}

PackedStringArray EditorExportPlugin::get_android_libraries(const Ref<EditorExportPlatform> &p_export_platform, bool p_debug) const {
	PackedStringArray ret;
	GDVIRTUAL_CALL(_get_android_libraries, p_export_platform, p_debug, ret);
	return ret;
}

String EditorExportPlugin::get_android_manifest_activity_element_contents(const Ref<EditorExportPlatform> &p_export_platform, bool p_debug) const {
	String ret;
	GDVIRTUAL_CALL(_get_android_manifest_activity_element_contents, p_export_platform, p_debug, ret);
	return ret;
}

String EditorExportPlugin::get_android_manifest_application_element_contents(const Ref<EditorExportPlatform> &p_export_platform, bool p_debug) const {
	String ret;
	GDVIRTUAL_CALL(_get_android_manifest_application_element_contents, p_export_platform, p_debug, ret);
	return ret;
}

String EditorExportPlugin::get_android_manifest_element_contents(const Ref<EditorExportPlatform> &p_export_platform, bool p_debug) const {
	String ret;
	GDVIRTUAL_CALL(_get_android_manifest_element_contents, p_export_platform, p_debug, ret);
	return ret;
}

PackedStringArray EditorExportPlugin::_get_export_features(const Ref<EditorExportPlatform> &p_platform, bool p_debug) const {
	PackedStringArray ret;
	GDVIRTUAL_CALL(_get_export_features, p_platform, p_debug, ret);
	return ret;
}

void EditorExportPlugin::_get_export_options(const Ref<EditorExportPlatform> &p_platform, List<EditorExportPlatform::ExportOption> *r_options) const {
	TypedArray<Dictionary> ret;
	GDVIRTUAL_CALL(_get_export_options, p_platform, ret);
	for (int i = 0; i < ret.size(); i++) {
		Dictionary option = ret[i];
		ERR_CONTINUE_MSG(!option.has("option"), "Missing required element 'option'");
		ERR_CONTINUE_MSG(!option.has("default_value"), "Missing required element 'default_value'");
		PropertyInfo property_info = PropertyInfo::from_dict(option["option"]);
		Variant default_value = option["default_value"];
		bool update_visibility = option.has("update_visibility") && option["update_visibility"];
		r_options->push_back(EditorExportPlatform::ExportOption(property_info, default_value, update_visibility));
	}
}

bool EditorExportPlugin::_should_update_export_options(const Ref<EditorExportPlatform> &p_platform) const {
	bool ret = false;
	GDVIRTUAL_CALL(_should_update_export_options, p_platform, ret);
	return ret;
}

bool EditorExportPlugin::_get_export_option_visibility(const Ref<EditorExportPlatform> &p_export_platform, const String &p_option_name) const {
	bool ret = true;
	GDVIRTUAL_CALL(_get_export_option_visibility, p_export_platform, p_option_name, ret);
	return ret;
}

String EditorExportPlugin::_get_export_option_warning(const Ref<EditorExportPlatform> &p_export_platform, const String &p_option_name) const {
	String ret;
	GDVIRTUAL_CALL(_get_export_option_warning, p_export_platform, p_option_name, ret);
	return ret;
}

Dictionary EditorExportPlugin::_get_export_options_overrides(const Ref<EditorExportPlatform> &p_platform) const {
	Dictionary ret;
	GDVIRTUAL_CALL(_get_export_options_overrides, p_platform, ret);
	return ret;
}

void EditorExportPlugin::_export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) {
}

void EditorExportPlugin::_export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) {
}

void EditorExportPlugin::_export_end() {}

void EditorExportPlugin::skip() {
	skipped = true;
}

void EditorExportPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_shared_object", "path", "tags", "target"), &EditorExportPlugin::add_shared_object);
	ClassDB::bind_method(D_METHOD("add_ios_project_static_lib", "path"), &EditorExportPlugin::add_ios_project_static_lib);
	ClassDB::bind_method(D_METHOD("add_file", "path", "file", "remap"), &EditorExportPlugin::add_file);
	ClassDB::bind_method(D_METHOD("add_ios_framework", "path"), &EditorExportPlugin::add_ios_framework);
	ClassDB::bind_method(D_METHOD("add_ios_embedded_framework", "path"), &EditorExportPlugin::add_ios_embedded_framework);
	ClassDB::bind_method(D_METHOD("add_ios_plist_content", "plist_content"), &EditorExportPlugin::add_ios_plist_content);
	ClassDB::bind_method(D_METHOD("add_ios_linker_flags", "flags"), &EditorExportPlugin::add_ios_linker_flags);
	ClassDB::bind_method(D_METHOD("add_ios_bundle_file", "path"), &EditorExportPlugin::add_ios_bundle_file);
	ClassDB::bind_method(D_METHOD("add_ios_cpp_code", "code"), &EditorExportPlugin::add_ios_cpp_code);
	ClassDB::bind_method(D_METHOD("add_macos_plugin_file", "path"), &EditorExportPlugin::add_macos_plugin_file);
	ClassDB::bind_method(D_METHOD("skip"), &EditorExportPlugin::skip);
	ClassDB::bind_method(D_METHOD("get_option", "name"), &EditorExportPlugin::get_option);

	ClassDB::bind_method(D_METHOD("get_export_preset"), &EditorExportPlugin::get_export_preset);
	ClassDB::bind_method(D_METHOD("get_export_platform"), &EditorExportPlugin::get_export_platform);

	GDVIRTUAL_BIND(_export_file, "path", "type", "features");
	GDVIRTUAL_BIND(_export_begin, "features", "is_debug", "path", "flags");
	GDVIRTUAL_BIND(_export_end);

	GDVIRTUAL_BIND(_begin_customize_resources, "platform", "features");
	GDVIRTUAL_BIND(_customize_resource, "resource", "path");

	GDVIRTUAL_BIND(_begin_customize_scenes, "platform", "features");
	GDVIRTUAL_BIND(_customize_scene, "scene", "path");

	GDVIRTUAL_BIND(_get_customization_configuration_hash);

	GDVIRTUAL_BIND(_end_customize_scenes);
	GDVIRTUAL_BIND(_end_customize_resources);

	GDVIRTUAL_BIND(_get_export_options, "platform");
	GDVIRTUAL_BIND(_get_export_options_overrides, "platform");
	GDVIRTUAL_BIND(_should_update_export_options, "platform");
	GDVIRTUAL_BIND(_get_export_option_visibility, "platform", "option");
	GDVIRTUAL_BIND(_get_export_option_warning, "platform", "option");

	GDVIRTUAL_BIND(_get_export_features, "platform", "debug");
	GDVIRTUAL_BIND(_get_name);

	GDVIRTUAL_BIND(_supports_platform, "platform");

	GDVIRTUAL_BIND(_get_android_dependencies, "platform", "debug");
	GDVIRTUAL_BIND(_get_android_dependencies_maven_repos, "platform", "debug");
	GDVIRTUAL_BIND(_get_android_libraries, "platform", "debug");
	GDVIRTUAL_BIND(_get_android_manifest_activity_element_contents, "platform", "debug");
	GDVIRTUAL_BIND(_get_android_manifest_application_element_contents, "platform", "debug");
	GDVIRTUAL_BIND(_get_android_manifest_element_contents, "platform", "debug");
}
