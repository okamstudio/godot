/**************************************************************************/
/*  editor_export_plugin.h                                                */
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

#ifndef EDITOR_EXPORT_PLUGIN_H
#define EDITOR_EXPORT_PLUGIN_H

#include "core/os/shared_object.h"
#include "editor_export_platform.h"
#include "editor_export_preset.h"
#include "scene/main/node.h"

class EditorExportPlugin : public RefCounted {
	GDCLASS(EditorExportPlugin, RefCounted);

	friend class EditorExport;
	friend class EditorExportPlatform;
	friend class EditorExportPreset;

	Ref<EditorExportPreset> export_preset;

	Vector<SharedObject> shared_objects;
	struct ExtraFile {
		String path;
		Vector<uint8_t> data;
		bool remap = false;
	};
	Vector<ExtraFile> extra_files;
	bool skipped = false;

	Vector<String> ios_frameworks;
	Vector<String> ios_embedded_frameworks;
	Vector<String> ios_project_static_libs;
	String ios_plist_content;
	String ios_linker_flags;
	Vector<String> ios_bundle_files;
	String ios_cpp_code;

	Vector<String> macos_plugin_files;

	_FORCE_INLINE_ void _clear() {
		shared_objects.clear();
		extra_files.clear();
		skipped = false;
	}

	_FORCE_INLINE_ void _export_end_clear() {
		ios_frameworks.clear();
		ios_embedded_frameworks.clear();
		ios_bundle_files.clear();
		ios_plist_content = "";
		ios_linker_flags = "";
		ios_cpp_code = "";
		macos_plugin_files.clear();
	}

	// Export
	void _export_file_script(const String &p_path, const String &p_type, const Vector<String> &p_features);
	void _export_begin_script(const Vector<String> &p_features, bool p_debug, const String &p_path, int p_flags);
	void _export_end_script();

	String _has_valid_export_configuration(const Ref<EditorExportPlatform> &p_export_platform, const Ref<EditorExportPreset> &p_preset);

protected:
	void set_export_preset(const Ref<EditorExportPreset> &p_preset);
	Ref<EditorExportPreset> get_export_preset() const;
	Ref<EditorExportPlatform> get_export_platform() const;

	void add_file(const String &p_path, const Vector<uint8_t> &p_file, bool p_remap);
	void add_shared_object(const String &p_path, const Vector<String> &tags, const String &p_target = String());
	void _add_shared_object(const SharedObject &p_shared_object);

	void add_ios_framework(const String &p_path);
	void add_ios_embedded_framework(const String &p_path);
	void add_ios_project_static_lib(const String &p_path);
	void add_ios_plist_content(const String &p_plist_content);
	void add_ios_linker_flags(const String &p_flags);
	void add_ios_bundle_file(const String &p_path);
	void add_ios_cpp_code(const String &p_code);
	void add_macos_plugin_file(const String &p_path);

	void skip();

	virtual void _export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features);
	virtual void _export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags);
	virtual void _export_end();

	static void _bind_methods();

	GDVIRTUAL3(_export_file, String, String, Vector<String>)
	GDVIRTUAL4(_export_begin, Vector<String>, bool, String, uint32_t)
	GDVIRTUAL0(_export_end)

	GDVIRTUAL2RC(bool, _begin_customize_resources, const Ref<EditorExportPlatform> &, const Vector<String> &)
	GDVIRTUAL2R_REQUIRED(Ref<Resource>, _customize_resource, const Ref<Resource> &, String)

	GDVIRTUAL2RC(bool, _begin_customize_scenes, const Ref<EditorExportPlatform> &, const Vector<String> &)
	GDVIRTUAL2R_REQUIRED(Node *, _customize_scene, Node *, String)
	GDVIRTUAL0RC_REQUIRED(uint64_t, _get_customization_configuration_hash)

	GDVIRTUAL0(_end_customize_scenes)
	GDVIRTUAL0(_end_customize_resources)

	GDVIRTUAL2RC(PackedStringArray, _get_export_features, const Ref<EditorExportPlatform> &, bool);
	GDVIRTUAL1RC(TypedArray<Dictionary>, _get_export_options, const Ref<EditorExportPlatform> &);
	GDVIRTUAL1RC(Dictionary, _get_export_options_overrides, const Ref<EditorExportPlatform> &);
	GDVIRTUAL1RC(bool, _should_update_export_options, const Ref<EditorExportPlatform> &);
	GDVIRTUAL2RC(bool, _get_export_option_visibility, const Ref<EditorExportPlatform> &, String);
	GDVIRTUAL2RC(String, _get_export_option_warning, const Ref<EditorExportPlatform> &, String);

	GDVIRTUAL0RC_REQUIRED(String, _get_name)

	GDVIRTUAL1RC(bool, _supports_platform, const Ref<EditorExportPlatform> &);

	GDVIRTUAL2RC(PackedStringArray, _get_android_dependencies, const Ref<EditorExportPlatform> &, bool);
	GDVIRTUAL2RC(PackedStringArray, _get_android_dependencies_maven_repos, const Ref<EditorExportPlatform> &, bool);
	GDVIRTUAL2RC(PackedStringArray, _get_android_libraries, const Ref<EditorExportPlatform> &, bool);
	GDVIRTUAL2RC(String, _get_android_manifest_activity_element_contents, const Ref<EditorExportPlatform> &, bool);
	GDVIRTUAL2RC(String, _get_android_manifest_application_element_contents, const Ref<EditorExportPlatform> &, bool);
	GDVIRTUAL2RC(String, _get_android_manifest_element_contents, const Ref<EditorExportPlatform> &, bool);

	virtual bool _begin_customize_resources(const Ref<EditorExportPlatform> &p_platform, const Vector<String> &p_features); // Return true if this plugin does property export customization
	virtual Ref<Resource> _customize_resource(const Ref<Resource> &p_resource, const String &p_path); // If nothing is returned, it means do not touch (nothing changed). If something is returned (either the same or a different resource) it means changes are made.

	virtual bool _begin_customize_scenes(const Ref<EditorExportPlatform> &p_platform, const Vector<String> &p_features); // Return true if this plugin does property export customization
	virtual Node *_customize_scene(Node *p_root, const String &p_path); // Return true if a change was made

	virtual uint64_t _get_customization_configuration_hash() const; // Hash used for caching customized resources and scenes.

	virtual void _end_customize_scenes();
	virtual void _end_customize_resources();

	virtual PackedStringArray _get_export_features(const Ref<EditorExportPlatform> &p_export_platform, bool p_debug) const;
	virtual void _get_export_options(const Ref<EditorExportPlatform> &p_export_platform, List<EditorExportPlatform::ExportOption> *r_options) const;
	virtual Dictionary _get_export_options_overrides(const Ref<EditorExportPlatform> &p_export_platform) const;
	virtual bool _should_update_export_options(const Ref<EditorExportPlatform> &p_export_platform) const;
	virtual bool _get_export_option_visibility(const Ref<EditorExportPlatform> &p_export_platform, const String &p_option_name) const;
	virtual String _get_export_option_warning(const Ref<EditorExportPlatform> &p_export_platform, const String &p_option_name) const;

public:
	virtual String get_name() const;

	virtual bool supports_platform(const Ref<EditorExportPlatform> &p_export_platform) const;
	PackedStringArray get_export_features(const Ref<EditorExportPlatform> &p_export_platform, bool p_debug) const;

	virtual PackedStringArray get_android_dependencies(const Ref<EditorExportPlatform> &p_export_platform, bool p_debug) const;
	virtual PackedStringArray get_android_dependencies_maven_repos(const Ref<EditorExportPlatform> &p_export_platform, bool p_debug) const;
	virtual PackedStringArray get_android_libraries(const Ref<EditorExportPlatform> &p_export_platform, bool p_debug) const;
	virtual String get_android_manifest_activity_element_contents(const Ref<EditorExportPlatform> &p_export_platform, bool p_debug) const;
	virtual String get_android_manifest_application_element_contents(const Ref<EditorExportPlatform> &p_export_platform, bool p_debug) const;
	virtual String get_android_manifest_element_contents(const Ref<EditorExportPlatform> &p_export_platform, bool p_debug) const;

	Vector<String> get_ios_frameworks() const;
	Vector<String> get_ios_embedded_frameworks() const;
	Vector<String> get_ios_project_static_libs() const;
	String get_ios_plist_content() const;
	String get_ios_linker_flags() const;
	Vector<String> get_ios_bundle_files() const;
	String get_ios_cpp_code() const;
	const Vector<String> &get_macos_plugin_files() const;
	Variant get_option(const StringName &p_name) const;
};

#endif // EDITOR_EXPORT_PLUGIN_H
