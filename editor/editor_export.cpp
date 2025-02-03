/**************************************************************************/
/*  editor_export.cpp                                                     */
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

#include "editor_export.h"

#include "core/crypto/crypto_core.h"
#include "core/io/config_file.h"
#include "core/io/file_access_pack.h" // PACK_HEADER_MAGIC, PACK_FORMAT_VERSION
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/io/zip_io.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/project_settings.h"
#include "core/script_language.h"
#include "core/version.h"
#include "editor/editor_file_system.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor_node.h"
#include "editor_scale.h"
#include "editor_settings.h"
#include "scene/resources/resource_format_text.h"

static int _get_pad(int p_alignment, int p_n) {
	int rest = p_n % p_alignment;
	int pad = 0;
	if (rest > 0) {
		pad = p_alignment - rest;
	};

	return pad;
}

#define PCK_PADDING 16

bool EditorExportPreset::_set(const StringName &p_name, const Variant &p_value) {
	if (values.has(p_name)) {
		values[p_name] = p_value;
		EditorExport::singleton->save_presets();
		if (update_visibility[p_name]) {
			property_list_changed_notify();
		}
		return true;
	}
#ifndef DISABLE_DEPRECATED
	// Compatibility with new `binary_format/architecture` for Linux in 3.6+.
	else if (p_name == "binary_format/64_bits" && values.has("binary_format/architecture")) {
		values["binary_format/architecture"] = (bool)p_value ? "x86_64" : "x86_32";
		EditorExport::singleton->save_presets();
		if (update_visibility["binary_format/architecture"]) {
			property_list_changed_notify();
		}
		return true;
	}
#endif

	return false;
}

bool EditorExportPreset::_get(const StringName &p_name, Variant &r_ret) const {
	if (values.has(p_name)) {
		r_ret = values[p_name];
		return true;
	}

	return false;
}

void EditorExportPreset::_get_property_list(List<PropertyInfo> *p_list) const {
	for (const List<PropertyInfo>::Element *E = properties.front(); E; E = E->next()) {
		if (platform->get_option_visibility(this, E->get().name, values)) {
			p_list->push_back(E->get());
		}
	}
}

Ref<EditorExportPlatform> EditorExportPreset::get_platform() const {
	return platform;
}

void EditorExportPreset::update_files_to_export() {
	Vector<String> to_remove;
	for (Set<String>::Element *E = selected_files.front(); E; E = E->next()) {
		if (!FileAccess::exists(E->get())) {
			to_remove.push_back(E->get());
		}
	}
	for (int i = 0; i < to_remove.size(); ++i) {
		selected_files.erase(to_remove[i]);
	}
}

Vector<String> EditorExportPreset::get_files_to_export() const {
	Vector<String> files;
	for (Set<String>::Element *E = selected_files.front(); E; E = E->next()) {
		files.push_back(E->get());
	}
	return files;
}

void EditorExportPreset::set_name(const String &p_name) {
	name = p_name;
	EditorExport::singleton->save_presets();
}

String EditorExportPreset::get_name() const {
	return name;
}

void EditorExportPreset::set_runnable(bool p_enable) {
	runnable = p_enable;
	EditorExport::singleton->save_presets();
}

bool EditorExportPreset::is_runnable() const {
	return runnable;
}

void EditorExportPreset::set_export_filter(ExportFilter p_filter) {
	export_filter = p_filter;
	EditorExport::singleton->save_presets();
}

EditorExportPreset::ExportFilter EditorExportPreset::get_export_filter() const {
	return export_filter;
}

void EditorExportPreset::set_include_filter(const String &p_include) {
	include_filter = p_include;
	EditorExport::singleton->save_presets();
}

String EditorExportPreset::get_include_filter() const {
	return include_filter;
}

void EditorExportPreset::set_export_path(const String &p_path) {
	export_path = p_path;
	/* NOTE(SonerSound): if there is a need to implement a PropertyHint that specifically indicates a relative path,
	 * this should be removed. */
	if (export_path.is_abs_path()) {
		String res_path = OS::get_singleton()->get_resource_dir();
		export_path = res_path.path_to_file(export_path);
	}
	EditorExport::singleton->save_presets();
}

String EditorExportPreset::get_export_path() const {
	return export_path;
}

void EditorExportPreset::set_exclude_filter(const String &p_exclude) {
	exclude_filter = p_exclude;
	EditorExport::singleton->save_presets();
}

String EditorExportPreset::get_exclude_filter() const {
	return exclude_filter;
}

void EditorExportPreset::add_export_file(const String &p_path) {
	selected_files.insert(p_path);
	EditorExport::singleton->save_presets();
}

void EditorExportPreset::remove_export_file(const String &p_path) {
	selected_files.erase(p_path);
	EditorExport::singleton->save_presets();
}

bool EditorExportPreset::has_export_file(const String &p_path) {
	return selected_files.has(p_path);
}

void EditorExportPreset::set_custom_features(const String &p_custom_features) {
	custom_features = p_custom_features;
	EditorExport::singleton->save_presets();
}

String EditorExportPreset::get_custom_features() const {
	return custom_features;
}

void EditorExportPreset::set_script_export_mode(int p_mode) {
	script_mode = p_mode;
	EditorExport::singleton->save_presets();
}

int EditorExportPreset::get_script_export_mode() const {
	return script_mode;
}

void EditorExportPreset::set_script_encryption_key(const String &p_key) {
	script_key = p_key;
	EditorExport::singleton->save_presets();
}

String EditorExportPreset::get_script_encryption_key() const {
	return script_key;
}

EditorExportPreset::EditorExportPreset() :
		export_filter(EXPORT_ALL_RESOURCES),
		export_path(""),
		runnable(false),
		script_mode(MODE_SCRIPT_COMPILED) {
}

///////////////////////////////////

bool EditorExportPlatform::fill_log_messages(RichTextLabel *p_log, Error p_err) {
	bool has_messages = false;

	int msg_count = get_message_count();

	p_log->add_text(TTR("Project export for platform:") + " ");
	p_log->add_image(get_logo(), 16 * EDSCALE, 16 * EDSCALE, RichTextLabel::INLINE_ALIGN_CENTER);
	p_log->add_text(" ");
	p_log->add_text(get_name());
	p_log->add_text(" - ");
	if (p_err == OK) {
		if (get_worst_message_type() >= EditorExportPlatform::EXPORT_MESSAGE_WARNING) {
			p_log->add_image(EditorNode::get_singleton()->get_gui_base()->get_icon("StatusWarning", "EditorIcons"), 16 * EDSCALE, 16 * EDSCALE, RichTextLabel::INLINE_ALIGN_CENTER);
			p_log->add_text(" ");
			p_log->add_text(TTR("Completed with warnings."));
			has_messages = true;
		} else {
			p_log->add_image(EditorNode::get_singleton()->get_gui_base()->get_icon("StatusSuccess", "EditorIcons"), 16 * EDSCALE, 16 * EDSCALE, RichTextLabel::INLINE_ALIGN_CENTER);
			p_log->add_text(" ");
			p_log->add_text(TTR("Completed successfully."));
			if (msg_count > 0) {
				has_messages = true;
			}
		}
	} else {
		p_log->add_image(EditorNode::get_singleton()->get_gui_base()->get_icon("StatusError", "EditorIcons"), 16 * EDSCALE, 16 * EDSCALE, RichTextLabel::INLINE_ALIGN_CENTER);
		p_log->add_text(" ");
		p_log->add_text(TTR("Failed."));
		has_messages = true;
	}

	if (msg_count) {
		p_log->push_table(2);
		p_log->set_table_column_expand(0, false);
		p_log->set_table_column_expand(1, true);
		for (int m = 0; m < msg_count; m++) {
			EditorExportPlatform::ExportMessage msg = get_message(m);
			Color color = EditorNode::get_singleton()->get_gui_base()->get_color("font_color", "Label");
			Ref<Texture> icon;

			switch (msg.msg_type) {
				case EditorExportPlatform::EXPORT_MESSAGE_INFO: {
					color = EditorNode::get_singleton()->get_gui_base()->get_color("font_color", "Editor") * Color(1, 1, 1, 0.6);
				} break;
				case EditorExportPlatform::EXPORT_MESSAGE_WARNING: {
					icon = EditorNode::get_singleton()->get_gui_base()->get_icon("Warning", "EditorIcons");
					color = EditorNode::get_singleton()->get_gui_base()->get_color("warning_color", "Editor");
				} break;
				case EditorExportPlatform::EXPORT_MESSAGE_ERROR: {
					icon = EditorNode::get_singleton()->get_gui_base()->get_icon("Error", "EditorIcons");
					color = EditorNode::get_singleton()->get_gui_base()->get_color("error_color", "Editor");
				} break;
				default:
					break;
			}

			p_log->push_cell();
			p_log->add_text("\t");
			if (icon.is_valid()) {
				p_log->add_image(icon);
			}
			p_log->pop();

			p_log->push_cell();
			p_log->push_color(color);
			p_log->add_text(vformat("[%s]: %s", msg.category, msg.text));
			p_log->pop();
			p_log->pop();
		}
		p_log->pop();
		p_log->add_newline();
	}
	p_log->add_newline();
	return has_messages;
}

void EditorExportPlatform::gen_debug_flags(Vector<String> &r_flags, int p_flags) {
	String host = EditorSettings::get_singleton()->get("network/debug/remote_host");
	int remote_port = (int)EditorSettings::get_singleton()->get("network/debug/remote_port");

	if (p_flags & DEBUG_FLAG_REMOTE_DEBUG_LOCALHOST) {
		host = "localhost";
	}

	if (p_flags & DEBUG_FLAG_DUMB_CLIENT) {
		int port = EditorSettings::get_singleton()->get("filesystem/file_server/port");
		String passwd = EditorSettings::get_singleton()->get("filesystem/file_server/password");
		r_flags.push_back("--remote-fs");
		r_flags.push_back(host + ":" + itos(port));
		if (passwd != "") {
			r_flags.push_back("--remote-fs-password");
			r_flags.push_back(passwd);
		}
	}

	if (p_flags & DEBUG_FLAG_REMOTE_DEBUG) {
		r_flags.push_back("--remote-debug");

		r_flags.push_back(host + ":" + String::num(remote_port));

		List<String> breakpoints;
		ScriptEditor::get_singleton()->get_breakpoints(&breakpoints);

		if (breakpoints.size()) {
			r_flags.push_back("--breakpoints");
			String bpoints;
			for (const List<String>::Element *E = breakpoints.front(); E; E = E->next()) {
				bpoints += E->get().replace(" ", "%20");
				if (E->next()) {
					bpoints += ",";
				}
			}

			r_flags.push_back(bpoints);
		}
	}

	if (p_flags & DEBUG_FLAG_VIEW_COLLISONS) {
		r_flags.push_back("--debug-collisions");
	}

	if (p_flags & DEBUG_FLAG_VIEW_NAVIGATION) {
		r_flags.push_back("--debug-navigation");
	}

	if (p_flags & DEBUG_FLAG_SHADER_FALLBACKS) {
		r_flags.push_back("--debug-shader-fallbacks");
	}
}

Error EditorExportPlatform::_save_pack_file(void *p_userdata, const String &p_path, const Vector<uint8_t> &p_data, int p_file, int p_total) {
	ERR_FAIL_COND_V_MSG(p_total < 1, ERR_PARAMETER_RANGE_ERROR, "Must select at least one file to export.");

	PackData *pd = (PackData *)p_userdata;

	SavedData sd;
	sd.path_utf8 = p_path.utf8();
	sd.ofs = pd->f->get_position();
	sd.size = p_data.size();

	pd->f->store_buffer(p_data.ptr(), p_data.size());
	int pad = _get_pad(PCK_PADDING, sd.size);
	for (int i = 0; i < pad; i++) {
		pd->f->store_8(0);
	}

	{
		unsigned char hash[16];
		CryptoCore::md5(p_data.ptr(), p_data.size(), hash);
		sd.md5.resize(16);
		for (int i = 0; i < 16; i++) {
			sd.md5.write[i] = hash[i];
		}
	}

	pd->file_ofs.push_back(sd);

	if (pd->ep->step(TTR("Storing File:") + " " + p_path, 2 + p_file * 100 / p_total, false)) {
		return ERR_SKIP;
	}

	return OK;
}

Error EditorExportPlatform::_save_zip_file(void *p_userdata, const String &p_path, const Vector<uint8_t> &p_data, int p_file, int p_total) {
	ERR_FAIL_COND_V_MSG(p_total < 1, ERR_PARAMETER_RANGE_ERROR, "Must select at least one file to export.");

	String path = p_path.replace_first("res://", "");

	ZipData *zd = (ZipData *)p_userdata;

	zipFile zip = (zipFile)zd->zip;

	zipOpenNewFileInZip(zip,
			path.utf8().get_data(),
			nullptr,
			nullptr,
			0,
			nullptr,
			0,
			nullptr,
			Z_DEFLATED,
			Z_DEFAULT_COMPRESSION);

	zipWriteInFileInZip(zip, p_data.ptr(), p_data.size());
	zipCloseFileInZip(zip);

	if (zd->ep->step(TTR("Storing File:") + " " + p_path, 2 + p_file * 100 / p_total, false)) {
		return ERR_SKIP;
	}

	return OK;
}

Ref<ImageTexture> EditorExportPlatform::get_option_icon(int p_index) const {
	Ref<Theme> theme = EditorNode::get_singleton()->get_editor_theme();
	ERR_FAIL_COND_V(theme.is_null(), Ref<ImageTexture>());
	return theme->get_icon("Play", "EditorIcons");
}

String EditorExportPlatform::find_export_template(String template_file_name, String *err) const {
	String current_version = VERSION_FULL_CONFIG;
	String template_path = EditorSettings::get_singleton()->get_templates_dir().plus_file(current_version).plus_file(template_file_name);

	if (FileAccess::exists(template_path)) {
		return template_path;
	}

	// Not found
	if (err) {
		*err += TTR("No export template found at the expected path:") + "\n" + template_path + "\n";
	}
	return String();
}

bool EditorExportPlatform::exists_export_template(String template_file_name, String *err) const {
	return find_export_template(template_file_name, err) != "";
}

Ref<EditorExportPreset> EditorExportPlatform::create_preset() {
	Ref<EditorExportPreset> preset;
	preset.instance();
	preset->platform = Ref<EditorExportPlatform>(this);

	List<ExportOption> options;
	get_export_options(&options);

	for (List<ExportOption>::Element *E = options.front(); E; E = E->next()) {
		preset->properties.push_back(E->get().option);
		preset->values[E->get().option.name] = E->get().default_value;
		preset->update_visibility[E->get().option.name] = E->get().update_visibility;
	}

	return preset;
}

void EditorExportPlatform::_export_find_resources(EditorFileSystemDirectory *p_dir, Set<String> &p_paths) {
	for (int i = 0; i < p_dir->get_subdir_count(); i++) {
		_export_find_resources(p_dir->get_subdir(i), p_paths);
	}

	for (int i = 0; i < p_dir->get_file_count(); i++) {
		p_paths.insert(p_dir->get_file_path(i));
	}
}

void EditorExportPlatform::_export_find_dependencies(const String &p_path, Set<String> &p_paths) {
	if (p_paths.has(p_path)) {
		return;
	}

	p_paths.insert(p_path);

	EditorFileSystemDirectory *dir;
	int file_idx;
	dir = EditorFileSystem::get_singleton()->find_file(p_path, &file_idx);
	if (!dir) {
		return;
	}

	Vector<String> deps = dir->get_file_deps(file_idx);

	for (int i = 0; i < deps.size(); i++) {
		_export_find_dependencies(deps[i], p_paths);
	}
}

void EditorExportPlatform::_edit_files_with_filter(DirAccess *da, const Vector<String> &p_filters, Set<String> &r_list, bool exclude) {
	da->list_dir_begin();
	String cur_dir = da->get_current_dir().replace("\\", "/");
	if (!cur_dir.ends_with("/")) {
		cur_dir += "/";
	}
	String cur_dir_no_prefix = cur_dir.replace("res://", "");

	Vector<String> dirs;
	String f;
	while ((f = da->get_next()) != "") {
		if (da->current_is_dir()) {
			dirs.push_back(f);
		} else {
			String fullpath = cur_dir + f;
			// Test also against path without res:// so that filters like `file.txt` can work.
			String fullpath_no_prefix = cur_dir_no_prefix + f;
			for (int i = 0; i < p_filters.size(); ++i) {
				if (fullpath.matchn(p_filters[i]) || fullpath_no_prefix.matchn(p_filters[i])) {
					if (!exclude) {
						r_list.insert(fullpath);
					} else {
						r_list.erase(fullpath);
					}
				}
			}
		}
	}

	da->list_dir_end();

	for (int i = 0; i < dirs.size(); ++i) {
		String dir = dirs[i];
		if (dir.begins_with(".")) {
			continue;
		}

		if (EditorFileSystem::_should_skip_directory(cur_dir + dir)) {
			continue;
		}

		da->change_dir(dir);
		_edit_files_with_filter(da, p_filters, r_list, exclude);
		da->change_dir("..");
	}
}

void EditorExportPlatform::_edit_filter_list(Set<String> &r_list, const String &p_filter, bool exclude) {
	if (p_filter == "") {
		return;
	}
	Vector<String> split = p_filter.split(",");
	Vector<String> filters;
	for (int i = 0; i < split.size(); i++) {
		String f = split[i].strip_edges();
		if (f.empty()) {
			continue;
		}
		filters.push_back(f);
	}

	DirAccess *da = DirAccess::open("res://");
	ERR_FAIL_NULL(da);
	_edit_files_with_filter(da, filters, r_list, exclude);
	memdelete(da);
}

void EditorExportPlugin::set_export_preset(const Ref<EditorExportPreset> &p_preset) {
	if (p_preset.is_valid()) {
		export_preset = p_preset;
	}
}

Ref<EditorExportPreset> EditorExportPlugin::get_export_preset() const {
	return export_preset;
}

void EditorExportPlugin::add_file(const String &p_path, const Vector<uint8_t> &p_file, bool p_remap) {
	ExtraFile ef;
	ef.data = p_file;
	ef.path = p_path;
	ef.remap = p_remap;
	extra_files.push_back(ef);
}

void EditorExportPlugin::add_shared_object(const String &p_path, const Vector<String> &tags) {
	shared_objects.push_back(SharedObject(p_path, tags));
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

void EditorExportPlugin::add_osx_plugin_file(const String &p_path) {
	osx_plugin_files.push_back(p_path);
}

const Vector<String> &EditorExportPlugin::get_osx_plugin_files() const {
	return osx_plugin_files;
}

void EditorExportPlugin::add_ios_project_static_lib(const String &p_path) {
	ios_project_static_libs.push_back(p_path);
}

Vector<String> EditorExportPlugin::get_ios_project_static_libs() const {
	return ios_project_static_libs;
}

void EditorExportPlugin::_export_file_script(const String &p_path, const String &p_type, const PoolVector<String> &p_features) {
	if (get_script_instance()) {
		get_script_instance()->call("_export_file", p_path, p_type, p_features);
	}
}

void EditorExportPlugin::_export_begin_script(const PoolVector<String> &p_features, bool p_debug, const String &p_path, int p_flags) {
	if (get_script_instance()) {
		get_script_instance()->call("_export_begin", p_features, p_debug, p_path, p_flags);
	}
}

void EditorExportPlugin::_export_end_script() {
	if (get_script_instance()) {
		get_script_instance()->call("_export_end");
	}
}

void EditorExportPlugin::_export_file(const String &p_path, const String &p_type, const Set<String> &p_features) {
}

void EditorExportPlugin::_export_begin(const Set<String> &p_features, bool p_debug, const String &p_path, int p_flags) {
}

void EditorExportPlugin::skip() {
	skipped = true;
}

void EditorExportPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_shared_object", "path", "tags"), &EditorExportPlugin::add_shared_object);
	ClassDB::bind_method(D_METHOD("add_ios_project_static_lib", "path"), &EditorExportPlugin::add_ios_project_static_lib);
	ClassDB::bind_method(D_METHOD("add_file", "path", "file", "remap"), &EditorExportPlugin::add_file);
	ClassDB::bind_method(D_METHOD("add_ios_framework", "path"), &EditorExportPlugin::add_ios_framework);
	ClassDB::bind_method(D_METHOD("add_ios_embedded_framework", "path"), &EditorExportPlugin::add_ios_embedded_framework);
	ClassDB::bind_method(D_METHOD("add_ios_plist_content", "plist_content"), &EditorExportPlugin::add_ios_plist_content);
	ClassDB::bind_method(D_METHOD("add_ios_linker_flags", "flags"), &EditorExportPlugin::add_ios_linker_flags);
	ClassDB::bind_method(D_METHOD("add_ios_bundle_file", "path"), &EditorExportPlugin::add_ios_bundle_file);
	ClassDB::bind_method(D_METHOD("add_ios_cpp_code", "code"), &EditorExportPlugin::add_ios_cpp_code);
	ClassDB::bind_method(D_METHOD("add_osx_plugin_file", "path"), &EditorExportPlugin::add_osx_plugin_file);
	ClassDB::bind_method(D_METHOD("skip"), &EditorExportPlugin::skip);

	BIND_VMETHOD(MethodInfo("_export_file", PropertyInfo(Variant::STRING, "path"), PropertyInfo(Variant::STRING, "type"), PropertyInfo(Variant::POOL_STRING_ARRAY, "features")));
	BIND_VMETHOD(MethodInfo("_export_begin", PropertyInfo(Variant::POOL_STRING_ARRAY, "features"), PropertyInfo(Variant::BOOL, "is_debug"), PropertyInfo(Variant::STRING, "path"), PropertyInfo(Variant::INT, "flags")));
	BIND_VMETHOD(MethodInfo("_export_end"));
}

EditorExportPlugin::EditorExportPlugin() {
	skipped = false;
}

EditorExportPlatform::FeatureContainers EditorExportPlatform::get_feature_containers(const Ref<EditorExportPreset> &p_preset) {
	Ref<EditorExportPlatform> platform = p_preset->get_platform();
	List<String> feature_list;
	platform->get_platform_features(&feature_list);
	platform->get_preset_features(p_preset, &feature_list);

	FeatureContainers result;
	for (List<String>::Element *E = feature_list.front(); E; E = E->next()) {
		result.features.insert(E->get());
		result.features_pv.push_back(E->get());
	}

	if (p_preset->get_custom_features() != String()) {
		Vector<String> tmp_custom_list = p_preset->get_custom_features().split(",");

		for (int i = 0; i < tmp_custom_list.size(); i++) {
			String f = tmp_custom_list[i].strip_edges();
			if (f != String()) {
				result.features.insert(f);
				result.features_pv.push_back(f);
			}
		}
	}

	return result;
}

EditorExportPlatform::ExportNotifier::ExportNotifier(EditorExportPlatform &p_platform, const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, int p_flags) {
	FeatureContainers features = p_platform.get_feature_containers(p_preset);
	Vector<Ref<EditorExportPlugin>> export_plugins = EditorExport::get_singleton()->get_export_plugins();
	//initial export plugin callback
	for (int i = 0; i < export_plugins.size(); i++) {
		if (export_plugins[i]->get_script_instance()) { //script based
			export_plugins.write[i]->_export_begin_script(features.features_pv, p_debug, p_path, p_flags);
		} else {
			export_plugins.write[i]->_export_begin(features.features, p_debug, p_path, p_flags);
		}
	}
}

EditorExportPlatform::ExportNotifier::~ExportNotifier() {
	Vector<Ref<EditorExportPlugin>> export_plugins = EditorExport::get_singleton()->get_export_plugins();
	for (int i = 0; i < export_plugins.size(); i++) {
		if (export_plugins[i]->get_script_instance()) {
			export_plugins.write[i]->_export_end_script();
		}
		export_plugins.write[i]->_export_end();
	}
}

Error EditorExportPlatform::export_project_files(const Ref<EditorExportPreset> &p_preset, EditorExportSaveFunction p_func, void *p_udata, EditorExportSaveSharedObject p_so_func) {
	//figure out paths of files that will be exported
	Set<String> paths;
	Vector<String> path_remaps;

	if (p_preset->get_export_filter() == EditorExportPreset::EXPORT_ALL_RESOURCES) {
		//find stuff
		_export_find_resources(EditorFileSystem::get_singleton()->get_filesystem(), paths);
	} else {
		bool scenes_only = p_preset->get_export_filter() == EditorExportPreset::EXPORT_SELECTED_SCENES;

		Vector<String> files = p_preset->get_files_to_export();
		for (int i = 0; i < files.size(); i++) {
			if (scenes_only && ResourceLoader::get_resource_type(files[i]) != "PackedScene") {
				continue;
			}

			_export_find_dependencies(files[i], paths);
		}

		// Add autoload resources and their dependencies
		List<PropertyInfo> props;
		ProjectSettings::get_singleton()->get_property_list(&props);

		for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
			const PropertyInfo &pi = E->get();

			if (!pi.name.begins_with("autoload/")) {
				continue;
			}

			String autoload_path = ProjectSettings::get_singleton()->get(pi.name);

			if (autoload_path.begins_with("*")) {
				autoload_path = autoload_path.substr(1);
			}

			_export_find_dependencies(autoload_path, paths);
		}
	}

	//add native icons to non-resource include list
	_edit_filter_list(paths, String("*.icns"), false);
	_edit_filter_list(paths, String("*.ico"), false);

	_edit_filter_list(paths, p_preset->get_include_filter(), false);
	_edit_filter_list(paths, p_preset->get_exclude_filter(), true);

	// Ignore import files, since these are automatically added to the jar later with the resources
	_edit_filter_list(paths, String("*.import"), true);

	Error err = OK;
	Vector<Ref<EditorExportPlugin>> export_plugins = EditorExport::get_singleton()->get_export_plugins();

	for (int i = 0; i < export_plugins.size(); i++) {
		export_plugins.write[i]->set_export_preset(p_preset);

		if (p_so_func) {
			for (int j = 0; j < export_plugins[i]->shared_objects.size(); j++) {
				err = p_so_func(p_udata, export_plugins[i]->shared_objects[j]);
				if (err != OK) {
					return err;
				}
			}
		}
		for (int j = 0; j < export_plugins[i]->extra_files.size(); j++) {
			err = p_func(p_udata, export_plugins[i]->extra_files[j].path, export_plugins[i]->extra_files[j].data, 0, paths.size());
			if (err != OK) {
				return err;
			}
		}

		export_plugins.write[i]->_clear();
	}

	FeatureContainers feature_containers = get_feature_containers(p_preset);
	Set<String> &features = feature_containers.features;
	PoolVector<String> &features_pv = feature_containers.features_pv;

	//store everything in the export medium
	int idx = 0;
	int total = paths.size();

	for (Set<String>::Element *E = paths.front(); E; E = E->next()) {
		String path = E->get();
		String type = ResourceLoader::get_resource_type(path);

		if (FileAccess::exists(path + ".import")) {
			//file is imported, replace by what it imports
			Ref<ConfigFile> config;
			config.instance();
			err = config->load(path + ".import");
			if (err != OK) {
				ERR_PRINT("Could not parse: '" + path + "', not exported.");
				continue;
			}

			String importer_type = config->get_value("remap", "importer");

			if (importer_type == "keep") {
				//just keep file as-is
				Vector<uint8_t> array = FileAccess::get_file_as_array(path);
				err = p_func(p_udata, path, array, idx, total);

				if (err != OK) {
					return err;
				}

				continue;
			}

			List<String> remaps;
			config->get_section_keys("remap", &remaps);

			Set<String> remap_features;

			for (List<String>::Element *F = remaps.front(); F; F = F->next()) {
				String remap = F->get();
				String feature = remap.get_slice(".", 1);
				if (features.has(feature)) {
					remap_features.insert(feature);
				}
			}

			if (remap_features.size() > 1) {
				this->resolve_platform_feature_priorities(p_preset, remap_features);
			}

			err = OK;

			for (List<String>::Element *F = remaps.front(); F; F = F->next()) {
				String remap = F->get();
				if (remap == "path") {
					String remapped_path = config->get_value("remap", remap);
					Vector<uint8_t> array = FileAccess::get_file_as_array(remapped_path);
					err = p_func(p_udata, remapped_path, array, idx, total);
				} else if (remap.begins_with("path.")) {
					String feature = remap.get_slice(".", 1);

					if (remap_features.has(feature)) {
						String remapped_path = config->get_value("remap", remap);
						Vector<uint8_t> array = FileAccess::get_file_as_array(remapped_path);
						err = p_func(p_udata, remapped_path, array, idx, total);
					}
				}
			}

			if (err != OK) {
				return err;
			}

			//also save the .import file
			Vector<uint8_t> array = FileAccess::get_file_as_array(path + ".import");
			err = p_func(p_udata, path + ".import", array, idx, total);

			if (err != OK) {
				return err;
			}

		} else {
			bool do_export = true;
			for (int i = 0; i < export_plugins.size(); i++) {
				if (export_plugins[i]->get_script_instance()) { //script based
					export_plugins.write[i]->_export_file_script(path, type, features_pv);
				} else {
					export_plugins.write[i]->_export_file(path, type, features);
				}
				if (p_so_func) {
					for (int j = 0; j < export_plugins[i]->shared_objects.size(); j++) {
						err = p_so_func(p_udata, export_plugins[i]->shared_objects[j]);
						if (err != OK) {
							return err;
						}
					}
				}

				for (int j = 0; j < export_plugins[i]->extra_files.size(); j++) {
					err = p_func(p_udata, export_plugins[i]->extra_files[j].path, export_plugins[i]->extra_files[j].data, idx, total);
					if (err != OK) {
						return err;
					}
					if (export_plugins[i]->extra_files[j].remap) {
						do_export = false; //if remap, do not
						path_remaps.push_back(path);
						path_remaps.push_back(export_plugins[i]->extra_files[j].path);
					}
				}

				if (export_plugins[i]->skipped) {
					do_export = false;
				}
				export_plugins.write[i]->_clear();

				if (!do_export) {
					break; //apologies, not exporting
				}
			}
			//just store it as it comes
			if (do_export) {
				Vector<uint8_t> array = FileAccess::get_file_as_array(path);
				err = p_func(p_udata, path, array, idx, total);
				if (err != OK) {
					return err;
				}
			}
		}

		idx++;
	}

	//save config!

	Vector<String> custom_list;

	if (p_preset->get_custom_features() != String()) {
		Vector<String> tmp_custom_list = p_preset->get_custom_features().split(",");

		for (int i = 0; i < tmp_custom_list.size(); i++) {
			String f = tmp_custom_list[i].strip_edges();
			if (f != String()) {
				custom_list.push_back(f);
			}
		}
	}

	ProjectSettings::CustomMap custom_map;
	if (path_remaps.size()) {
		if (true) { //new remap mode, use always as it's friendlier with multiple .pck exports
			for (int i = 0; i < path_remaps.size(); i += 2) {
				String from = path_remaps[i];
				String to = path_remaps[i + 1];
				String remap_file = "[remap]\n\npath=\"" + to.c_escape() + "\"\n";
				CharString utf8 = remap_file.utf8();
				Vector<uint8_t> new_file;
				new_file.resize(utf8.length());
				for (int j = 0; j < utf8.length(); j++) {
					new_file.write[j] = utf8[j];
				}

				err = p_func(p_udata, from + ".remap", new_file, idx, total);
				if (err != OK) {
					return err;
				}
			}
		} else {
			//old remap mode, will still work, but it's unused because it's not multiple pck export friendly
			custom_map["path_remap/remapped_paths"] = path_remaps;
		}
	}

	// Store icon and splash images directly, they need to bypass the import system and be loaded as images
	String icon = ProjectSettings::get_singleton()->get("application/config/icon");
	String splash = ProjectSettings::get_singleton()->get("application/boot_splash/image");
	if (icon != String() && FileAccess::exists(icon)) {
		Vector<uint8_t> array = FileAccess::get_file_as_array(icon);
		err = p_func(p_udata, icon, array, idx, total);
		if (err != OK) {
			return err;
		}
	}
	if (splash != String() && FileAccess::exists(splash) && icon != splash) {
		Vector<uint8_t> array = FileAccess::get_file_as_array(splash);
		err = p_func(p_udata, splash, array, idx, total);
		if (err != OK) {
			return err;
		}
	}

	String config_file = "project.binary";
	String engine_cfb = EditorSettings::get_singleton()->get_cache_dir().plus_file("tmp" + config_file);
	ProjectSettings::get_singleton()->save_custom(engine_cfb, custom_map, custom_list);
	Vector<uint8_t> data = FileAccess::get_file_as_array(engine_cfb);
	DirAccess::remove_file_or_error(engine_cfb);

	return p_func(p_udata, "res://" + config_file, data, idx, total);
}

Error EditorExportPlatform::_add_shared_object(void *p_userdata, const SharedObject &p_so) {
	PackData *pack_data = (PackData *)p_userdata;
	if (pack_data->so_files) {
		pack_data->so_files->push_back(p_so);
	}

	return OK;
}

Error EditorExportPlatform::save_pack(const Ref<EditorExportPreset> &p_preset, const String &p_path, Vector<SharedObject> *p_so_files, bool p_embed, int64_t *r_embedded_start, int64_t *r_embedded_size) {
	EditorProgress ep("savepack", TTR("Packing"), 102, true);

	// Create the temporary export directory if it doesn't exist.
	DirAccessRef da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(EditorSettings::get_singleton()->get_cache_dir());

	String tmppath = EditorSettings::get_singleton()->get_cache_dir().plus_file("packtmp");
	FileAccess *ftmp = FileAccess::open(tmppath, FileAccess::WRITE);
	if (!ftmp) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Save PCK"), vformat(TTR("Cannot create file \"%s\"."), tmppath));
		return ERR_CANT_CREATE;
	}

	PackData pd;
	pd.ep = &ep;
	pd.f = ftmp;
	pd.so_files = p_so_files;

	Error err = export_project_files(p_preset, _save_pack_file, &pd, _add_shared_object);

	memdelete(ftmp); //close tmp file

	if (err != OK) {
		DirAccess::remove_file_or_error(tmppath);
		add_message(EXPORT_MESSAGE_ERROR, TTR("Save PCK"), TTR("Failed to export project files."));
		return err;
	}

	pd.file_ofs.sort(); //do sort, so we can do binary search later

	FileAccess *f;
	int64_t embed_pos = 0;
	if (!p_embed) {
		// Regular output to separate PCK file
		f = FileAccess::open(p_path, FileAccess::WRITE);
		if (!f) {
			DirAccess::remove_file_or_error(tmppath);
			ERR_FAIL_V(ERR_CANT_CREATE);
		}
	} else {
		// Append to executable
		f = FileAccess::open(p_path, FileAccess::READ_WRITE);
		if (!f) {
			DirAccess::remove_file_or_error(tmppath);
			ERR_FAIL_V(ERR_FILE_CANT_OPEN);
		}

		f->seek_end();
		embed_pos = f->get_position();

		if (r_embedded_start) {
			*r_embedded_start = embed_pos;
		}

		// Ensure embedded PCK starts at a 64-bit multiple
		int pad = f->get_position() % 8;
		for (int i = 0; i < pad; i++) {
			f->store_8(0);
		}
	}

	int64_t pck_start_pos = f->get_position();

	f->store_32(PACK_HEADER_MAGIC);
	f->store_32(PACK_FORMAT_VERSION);
	f->store_32(VERSION_MAJOR);
	f->store_32(VERSION_MINOR);
	f->store_32(VERSION_PATCH);

	for (int i = 0; i < 16; i++) {
		//reserved
		f->store_32(0);
	}

	f->store_32(pd.file_ofs.size()); //amount of files

	int64_t header_size = f->get_position();

	//precalculate header size

	for (int i = 0; i < pd.file_ofs.size(); i++) {
		header_size += 4; // size of path string (32 bits is enough)
		int string_len = pd.file_ofs[i].path_utf8.length();
		header_size += string_len + _get_pad(4, string_len); ///size of path string
		header_size += 8; // offset to file _with_ header size included
		header_size += 8; // size of file
		header_size += 16; // md5
	}

	int header_padding = _get_pad(PCK_PADDING, header_size);

	for (int i = 0; i < pd.file_ofs.size(); i++) {
		uint32_t string_len = pd.file_ofs[i].path_utf8.length();
		uint32_t pad = _get_pad(4, string_len);

		f->store_32(string_len + pad);
		f->store_buffer((const uint8_t *)pd.file_ofs[i].path_utf8.get_data(), string_len);
		for (uint32_t j = 0; j < pad; j++) {
			f->store_8(0);
		}

		f->store_64(pd.file_ofs[i].ofs + header_padding + header_size);
		f->store_64(pd.file_ofs[i].size); // pay attention here, this is where file is
		f->store_buffer(pd.file_ofs[i].md5.ptr(), 16); //also save md5 for file
	}

	for (int i = 0; i < header_padding; i++) {
		f->store_8(0);
	}

	// Save the rest of the data.

	ftmp = FileAccess::open(tmppath, FileAccess::READ);
	if (!ftmp) {
		memdelete(f);
		DirAccess::remove_file_or_error(tmppath);
		add_message(EXPORT_MESSAGE_ERROR, TTR("Save PCK"), vformat(TTR("Can't open file to read from path \"%s\"."), tmppath));
		return ERR_CANT_CREATE;
	}

	const int bufsize = 16384;
	uint8_t buf[bufsize];

	while (true) {
		uint64_t got = ftmp->get_buffer(buf, bufsize);
		if (got == 0) {
			break;
		}
		f->store_buffer(buf, got);
	}

	memdelete(ftmp);

	if (p_embed) {
		// Ensure embedded data ends at a 64-bit multiple
		uint64_t embed_end = f->get_position() - embed_pos + 12;
		uint64_t pad = embed_end % 8;
		for (uint64_t i = 0; i < pad; i++) {
			f->store_8(0);
		}

		uint64_t pck_size = f->get_position() - pck_start_pos;
		f->store_64(pck_size);
		f->store_32(PACK_HEADER_MAGIC);

		if (r_embedded_size) {
			*r_embedded_size = f->get_position() - embed_pos;
		}
	}

	memdelete(f);
	DirAccess::remove_file_or_error(tmppath);

	return OK;
}

Error EditorExportPlatform::save_zip(const Ref<EditorExportPreset> &p_preset, const String &p_path) {
	EditorProgress ep("savezip", TTR("Packing"), 102, true);

	FileAccess *src_f;
	zlib_filefunc_def io = zipio_create_io_from_file(&src_f);
	zipFile zip = zipOpen2(p_path.utf8().get_data(), APPEND_STATUS_CREATE, nullptr, &io);

	ZipData zd;
	zd.ep = &ep;
	zd.zip = zip;

	Error err = export_project_files(p_preset, _save_zip_file, &zd);
	if (err != OK && err != ERR_SKIP) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Save ZIP"), TTR("Failed to export project files."));
	}

	zipClose(zip, nullptr);

	return OK;
}

Error EditorExportPlatform::export_pack(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, int p_flags) {
	ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags);
	return save_pack(p_preset, p_path);
}

Error EditorExportPlatform::export_zip(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, int p_flags) {
	ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags);
	return save_zip(p_preset, p_path);
}

void EditorExportPlatform::gen_export_flags(Vector<String> &r_flags, int p_flags) {
	String host = EditorSettings::get_singleton()->get("network/debug/remote_host");
	int remote_port = (int)EditorSettings::get_singleton()->get("network/debug/remote_port");

	if (p_flags & DEBUG_FLAG_REMOTE_DEBUG_LOCALHOST) {
		host = "localhost";
	}

	if (p_flags & DEBUG_FLAG_DUMB_CLIENT) {
		int port = EditorSettings::get_singleton()->get("filesystem/file_server/port");
		String passwd = EditorSettings::get_singleton()->get("filesystem/file_server/password");
		r_flags.push_back("--remote-fs");
		r_flags.push_back(host + ":" + itos(port));
		if (passwd != "") {
			r_flags.push_back("--remote-fs-password");
			r_flags.push_back(passwd);
		}
	}

	if (p_flags & DEBUG_FLAG_REMOTE_DEBUG) {
		r_flags.push_back("--remote-debug");

		r_flags.push_back(host + ":" + String::num(remote_port));

		List<String> breakpoints;
		ScriptEditor::get_singleton()->get_breakpoints(&breakpoints);

		if (breakpoints.size()) {
			r_flags.push_back("--breakpoints");
			String bpoints;
			for (const List<String>::Element *E = breakpoints.front(); E; E = E->next()) {
				bpoints += E->get().replace(" ", "%20");
				if (E->next()) {
					bpoints += ",";
				}
			}

			r_flags.push_back(bpoints);
		}
	}

	if (p_flags & DEBUG_FLAG_VIEW_COLLISONS) {
		r_flags.push_back("--debug-collisions");
	}

	if (p_flags & DEBUG_FLAG_VIEW_NAVIGATION) {
		r_flags.push_back("--debug-navigation");
	}
}

EditorExportPlatform::EditorExportPlatform() {
}

////

EditorExport *EditorExport::singleton = nullptr;

void EditorExport::_save() {
	Ref<ConfigFile> config;
	config.instance();
	for (int i = 0; i < export_presets.size(); i++) {
		Ref<EditorExportPreset> preset = export_presets[i];
		String section = "preset." + itos(i);

		config->set_value(section, "name", preset->get_name());
		config->set_value(section, "platform", preset->get_platform()->get_name());
		config->set_value(section, "runnable", preset->is_runnable());
		config->set_value(section, "custom_features", preset->get_custom_features());

		bool save_files = false;
		switch (preset->get_export_filter()) {
			case EditorExportPreset::EXPORT_ALL_RESOURCES: {
				config->set_value(section, "export_filter", "all_resources");
			} break;
			case EditorExportPreset::EXPORT_SELECTED_SCENES: {
				config->set_value(section, "export_filter", "scenes");
				save_files = true;
			} break;
			case EditorExportPreset::EXPORT_SELECTED_RESOURCES: {
				config->set_value(section, "export_filter", "resources");
				save_files = true;
			} break;
		}

		if (save_files) {
			Vector<String> export_files = preset->get_files_to_export();
			config->set_value(section, "export_files", export_files);
		}
		config->set_value(section, "include_filter", preset->get_include_filter());
		config->set_value(section, "exclude_filter", preset->get_exclude_filter());
		config->set_value(section, "export_path", preset->get_export_path());
		config->set_value(section, "script_export_mode", preset->get_script_export_mode());
		config->set_value(section, "script_encryption_key", preset->get_script_encryption_key());

		String option_section = "preset." + itos(i) + ".options";

		for (const List<PropertyInfo>::Element *E = preset->get_properties().front(); E; E = E->next()) {
			config->set_value(option_section, E->get().name, preset->get(E->get().name));
		}
	}

	config->save("res://export_presets.cfg");
}

void EditorExport::save_presets() {
	if (block_save) {
		return;
	}
	save_timer->start();
}

void EditorExport::_bind_methods() {
	ClassDB::bind_method("_save", &EditorExport::_save);

	ADD_SIGNAL(MethodInfo("export_presets_updated"));
}

void EditorExport::add_export_platform(const Ref<EditorExportPlatform> &p_platform) {
	export_platforms.push_back(p_platform);
}

int EditorExport::get_export_platform_count() {
	return export_platforms.size();
}

Ref<EditorExportPlatform> EditorExport::get_export_platform(int p_idx) {
	ERR_FAIL_INDEX_V(p_idx, export_platforms.size(), Ref<EditorExportPlatform>());

	return export_platforms[p_idx];
}

void EditorExport::add_export_preset(const Ref<EditorExportPreset> &p_preset, int p_at_pos) {
	if (p_at_pos < 0) {
		export_presets.push_back(p_preset);
	} else {
		export_presets.insert(p_at_pos, p_preset);
	}
}

String EditorExportPlatform::test_etc2() const {
	String driver = ProjectSettings::get_singleton()->get("rendering/quality/driver/driver_name");
	bool driver_fallback = ProjectSettings::get_singleton()->get("rendering/quality/driver/fallback_to_gles2");
	bool etc_supported = ProjectSettings::get_singleton()->get("rendering/vram_compression/import_etc");
	bool etc2_supported = ProjectSettings::get_singleton()->get("rendering/vram_compression/import_etc2");

	if (driver == "GLES2" && !etc_supported) {
		return TTR("Target platform requires 'ETC' texture compression for GLES2. Enable 'Import Etc' in Project Settings.");
	} else if (driver == "GLES3") {
		String err;
		if (!etc2_supported) {
			err += TTR("Target platform requires 'ETC2' texture compression for GLES3. Enable 'Import Etc 2' in Project Settings.");
		}
		if (driver_fallback && !etc_supported) {
			if (err != String()) {
				err += "\n";
			}
			err += TTR("Target platform requires 'ETC' texture compression for the driver fallback to GLES2.\nEnable 'Import Etc' in Project Settings, or disable 'Driver Fallback Enabled'.");
		}
		return err;
	}
	return String();
}

String EditorExportPlatform::test_etc2_or_pvrtc() const {
	String driver = ProjectSettings::get_singleton()->get("rendering/quality/driver/driver_name");
	bool driver_fallback = ProjectSettings::get_singleton()->get("rendering/quality/driver/fallback_to_gles2");
	bool etc2_supported = ProjectSettings::get_singleton()->get("rendering/vram_compression/import_etc2");
	bool pvrtc_supported = ProjectSettings::get_singleton()->get("rendering/vram_compression/import_pvrtc");

	if (driver == "GLES2" && !pvrtc_supported) {
		return TTR("Target platform requires 'PVRTC' texture compression for GLES2. Enable 'Import Pvrtc' in Project Settings.");
	} else if (driver == "GLES3") {
		String err;
		if (!etc2_supported && !pvrtc_supported) {
			err += TTR("Target platform requires 'ETC2' or 'PVRTC' texture compression for GLES3. Enable 'Import Etc 2' or 'Import Pvrtc' in Project Settings.");
		}
		if (driver_fallback && !pvrtc_supported) {
			if (err != String()) {
				err += "\n";
			}
			err += TTR("Target platform requires 'PVRTC' texture compression for the driver fallback to GLES2.\nEnable 'Import Pvrtc' in Project Settings, or disable 'Driver Fallback Enabled'.");
		}
		return err;
	}
	return String();
}

int EditorExport::get_export_preset_count() const {
	return export_presets.size();
}

Ref<EditorExportPreset> EditorExport::get_export_preset(int p_idx) {
	ERR_FAIL_INDEX_V(p_idx, export_presets.size(), Ref<EditorExportPreset>());
	return export_presets[p_idx];
}

void EditorExport::remove_export_preset(int p_idx) {
	export_presets.remove(p_idx);
	save_presets();
}

void EditorExport::add_export_plugin(const Ref<EditorExportPlugin> &p_plugin) {
	if (export_plugins.find(p_plugin) == -1) {
		export_plugins.push_back(p_plugin);
	}
}

void EditorExport::remove_export_plugin(const Ref<EditorExportPlugin> &p_plugin) {
	export_plugins.erase(p_plugin);
}

Vector<Ref<EditorExportPlugin>> EditorExport::get_export_plugins() {
	return export_plugins;
}

void EditorExport::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			load_config();
		} break;
		case NOTIFICATION_PROCESS: {
			update_export_presets();
		} break;
	}
}

void EditorExport::load_config() {
	Ref<ConfigFile> config;
	config.instance();
	Error err = config->load("res://export_presets.cfg");
	if (err != OK) {
		return;
	}

	block_save = true;

	int index = 0;
	while (true) {
		String section = "preset." + itos(index);
		if (!config->has_section(section)) {
			break;
		}

		String platform = config->get_value(section, "platform");

		Ref<EditorExportPreset> preset;

		for (int i = 0; i < export_platforms.size(); i++) {
			if (export_platforms[i]->get_name() == platform) {
				preset = export_platforms.write[i]->create_preset();
				break;
			}
		}

		if (!preset.is_valid()) {
			index++;
			ERR_CONTINUE(!preset.is_valid());
		}

		preset->set_name(config->get_value(section, "name"));
		preset->set_runnable(config->get_value(section, "runnable"));

		if (config->has_section_key(section, "custom_features")) {
			preset->set_custom_features(config->get_value(section, "custom_features"));
		}

		String export_filter = config->get_value(section, "export_filter");

		bool get_files = false;

		if (export_filter == "all_resources") {
			preset->set_export_filter(EditorExportPreset::EXPORT_ALL_RESOURCES);
		} else if (export_filter == "scenes") {
			preset->set_export_filter(EditorExportPreset::EXPORT_SELECTED_SCENES);
			get_files = true;
		} else if (export_filter == "resources") {
			preset->set_export_filter(EditorExportPreset::EXPORT_SELECTED_RESOURCES);
			get_files = true;
		}

		if (get_files) {
			Vector<String> files = config->get_value(section, "export_files");

			for (int i = 0; i < files.size(); i++) {
				if (!FileAccess::exists(files[i])) {
					preset->remove_export_file(files[i]);
				} else {
					preset->add_export_file(files[i]);
				}
			}
		}

		preset->set_include_filter(config->get_value(section, "include_filter"));
		preset->set_exclude_filter(config->get_value(section, "exclude_filter"));
		preset->set_export_path(config->get_value(section, "export_path", ""));

		if (config->has_section_key(section, "script_export_mode")) {
			preset->set_script_export_mode(config->get_value(section, "script_export_mode"));
		}
		if (config->has_section_key(section, "script_encryption_key")) {
			preset->set_script_encryption_key(config->get_value(section, "script_encryption_key"));
		}

		String option_section = "preset." + itos(index) + ".options";

		List<String> options;

		config->get_section_keys(option_section, &options);

		for (List<String>::Element *E = options.front(); E; E = E->next()) {
			Variant value = config->get_value(option_section, E->get());

			preset->set(E->get(), value);
		}

		add_export_preset(preset);
		index++;
	}

	block_save = false;
}

void EditorExport::update_export_presets() {
	Map<StringName, List<EditorExportPlatform::ExportOption>> platform_options;

	for (int i = 0; i < export_platforms.size(); i++) {
		Ref<EditorExportPlatform> platform = export_platforms[i];

		if (platform->should_update_export_options()) {
			List<EditorExportPlatform::ExportOption> options;
			platform->get_export_options(&options);

			platform_options[platform->get_name()] = options;
		}
	}

	bool export_presets_updated = false;
	for (int i = 0; i < export_presets.size(); i++) {
		Ref<EditorExportPreset> preset = export_presets[i];
		if (platform_options.has(preset->get_platform()->get_name())) {
			export_presets_updated = true;

			List<EditorExportPlatform::ExportOption> options = platform_options[preset->get_platform()->get_name()];

			// Copy the previous preset values
			Map<StringName, Variant> previous_values = preset->values;

			// Clear the preset properties and values prior to reloading
			preset->properties.clear();
			preset->values.clear();
			preset->update_visibility.clear();

			for (List<EditorExportPlatform::ExportOption>::Element *E = options.front(); E; E = E->next()) {
				preset->properties.push_back(E->get().option);

				StringName option_name = E->get().option.name;
				preset->values[option_name] = previous_values.has(option_name) ? previous_values[option_name] : E->get().default_value;
				preset->update_visibility[option_name] = E->get().update_visibility;
			}
		}
	}

	if (export_presets_updated) {
		emit_signal(_export_presets_updated);
	}
}

bool EditorExport::poll_export_platforms() {
	bool changed = false;
	for (int i = 0; i < export_platforms.size(); i++) {
		if (export_platforms.write[i]->poll_export()) {
			changed = true;
		}
	}

	return changed;
}

EditorExport::EditorExport() {
	save_timer = memnew(Timer);
	add_child(save_timer);
	save_timer->set_wait_time(0.8);
	save_timer->set_one_shot(true);
	save_timer->connect("timeout", this, "_save");
	block_save = false;

	_export_presets_updated = "export_presets_updated";

	singleton = this;
	set_process(true);
}

EditorExport::~EditorExport() {
}

//////////

void EditorExportPlatformPC::get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) {
	if (p_preset->get("texture_format/s3tc")) {
		r_features->push_back("s3tc");
	}
	if (p_preset->get("texture_format/etc")) {
		r_features->push_back("etc");
	}
	if (p_preset->get("texture_format/etc2")) {
		r_features->push_back("etc2");
	}

	if (get_os_name() == "X11") {
		const String &arch = get_preset_arch(p_preset);
		r_features->push_back(arch);
		if (arch == "x86_64" || arch == "arm64") {
			r_features->push_back("64");
		} else {
			r_features->push_back("32");
		}
	} else {
		if (p_preset->get("binary_format/64_bits")) {
			r_features->push_back("64");
		} else {
			r_features->push_back("32");
		}
	}
}

void EditorExportPlatformPC::get_export_options(List<ExportOption> *r_options) {
	String ext_filter = (get_os_name() == "Windows") ? "*.exe" : "";
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "custom_template/debug", PROPERTY_HINT_GLOBAL_FILE, ext_filter), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "custom_template/release", PROPERTY_HINT_GLOBAL_FILE, ext_filter), ""));

	// Linux support to export to ARM architectures was added in 3.6.
	// Given how late this arrived, we didn't refactor the whole export preset
	// interface to support more per-platform flexibility, like done in 4.0,
	// so instead we hack the few needed changes here with `get_os_name()` checks.
	if (get_os_name() == "X11") {
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "binary_format/architecture", PROPERTY_HINT_ENUM, "x86_64,x86_32,arm64,arm32"), "x86_64"));
	} else {
		r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "binary_format/64_bits"), true));
	}
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "binary_format/embed_pck"), false));

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "texture_format/bptc"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "texture_format/s3tc"), true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "texture_format/etc"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "texture_format/etc2"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "texture_format/no_bptc_fallbacks"), true));
}

String EditorExportPlatformPC::get_name() const {
	return name;
}

String EditorExportPlatformPC::get_os_name() const {
	return os_name;
}
Ref<Texture> EditorExportPlatformPC::get_logo() const {
	return logo;
}

bool EditorExportPlatformPC::has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates) const {
	String err;
	bool valid = false;
	bool dvalid = false;
	bool rvalid = false;

	// Look for export templates (first official, and if defined custom templates).

	if (get_os_name() == "X11") {
		const String &arch = get_preset_arch(p_preset);
		dvalid = exists_export_template(debug_files[arch], &err);
		rvalid = exists_export_template(release_files[arch], &err);
	} else {
		bool use64 = p_preset->get("binary_format/64_bits");
		dvalid = exists_export_template(use64 ? debug_file_64 : debug_file_32, &err);
		rvalid = exists_export_template(use64 ? release_file_64 : release_file_32, &err);
	}

	if (p_preset->get("custom_template/debug") != "") {
		dvalid = FileAccess::exists(p_preset->get("custom_template/debug"));
		if (!dvalid) {
			err += TTR("Custom debug template not found.") + "\n";
		}
	}
	if (p_preset->get("custom_template/release") != "") {
		rvalid = FileAccess::exists(p_preset->get("custom_template/release"));
		if (!rvalid) {
			err += TTR("Custom release template not found.") + "\n";
		}
	}

	valid = dvalid || rvalid;
	r_missing_templates = !valid;

	if (!err.empty()) {
		r_error = err;
	}
	return valid;
}

bool EditorExportPlatformPC::has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const {
	return true;
}

bool EditorExportPlatform::can_export(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates) const {
	bool valid = true;
#ifndef ANDROID_ENABLED
	String templates_error;
	valid = valid && has_valid_export_configuration(p_preset, templates_error, r_missing_templates);

	if (!templates_error.empty()) {
		r_error += templates_error;
	}
#endif

	String project_configuration_error;
	valid = valid && has_valid_project_configuration(p_preset, project_configuration_error);

	if (!project_configuration_error.empty()) {
		r_error += project_configuration_error;
	}

	return valid;
}

List<String> EditorExportPlatformPC::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
	List<String> list;
	for (Map<String, String>::Element *E = extensions.front(); E; E = E->next()) {
		if (get_os_name() == "X11") {
			if (get_preset_arch(p_preset) == E->key()) {
				list.push_back(extensions[E->key()]);
				return list;
			}
		} else {
			if (p_preset->get(E->key())) {
				list.push_back(extensions[E->key()]);
				return list;
			}
		}
	}

	if (extensions.has("default")) {
		list.push_back(extensions["default"]);
		return list;
	}

	return list;
}

Error EditorExportPlatformPC::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, int p_flags) {
	ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags);

	Error err = prepare_template(p_preset, p_debug, p_path, p_flags);
	if (err == OK) {
		err = modify_template(p_preset, p_debug, p_path, p_flags);
	}
	if (err == OK) {
		err = export_project_data(p_preset, p_debug, p_path, p_flags);
	}

	return err;
}

Error EditorExportPlatformPC::prepare_template(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, int p_flags) {
	if (!DirAccess::exists(p_path.get_base_dir())) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Prepare Template"), TTR("The given export path doesn't exist."));
		return ERR_FILE_BAD_PATH;
	}

	String custom_debug = p_preset->get("custom_template/debug");
	String custom_release = p_preset->get("custom_template/release");

	String template_path = p_debug ? custom_debug : custom_release;

	template_path = template_path.strip_edges();

	if (template_path == String()) {
		if (get_os_name() == "X11") {
			if (p_debug) {
				template_path = find_export_template(debug_files[get_preset_arch(p_preset)]);
			} else {
				template_path = find_export_template(release_files[get_preset_arch(p_preset)]);
			}
		} else {
			if (p_preset->get("binary_format/64_bits")) {
				if (p_debug) {
					template_path = find_export_template(debug_file_64);
				} else {
					template_path = find_export_template(release_file_64);
				}
			} else {
				if (p_debug) {
					template_path = find_export_template(debug_file_32);
				} else {
					template_path = find_export_template(release_file_32);
				}
			}
		}
	}

	if (template_path != String() && !FileAccess::exists(template_path)) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Prepare Template"), vformat(TTR("Template file not found: \"%s\"."), template_path));
		return ERR_FILE_NOT_FOUND;
	}

	DirAccessRef da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(p_path.get_base_dir());
	Error err = da->copy(template_path, p_path, get_chmod_flags());
	if (err != OK) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Prepare Template"), TTR("Failed to copy export template."));
	}

	return err;
}

Error EditorExportPlatformPC::export_project_data(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, int p_flags) {
	String pck_path;
	if (p_preset->get("binary_format/embed_pck")) {
		pck_path = p_path;
	} else {
		pck_path = p_path.get_basename() + ".pck";
	}

	Vector<SharedObject> so_files;

	int64_t embedded_pos;
	int64_t embedded_size;
	Error err = save_pack(p_preset, pck_path, &so_files, p_preset->get("binary_format/embed_pck"), &embedded_pos, &embedded_size);
	if (err == OK && p_preset->get("binary_format/embed_pck")) {
		if (embedded_size >= 0x100000000) {
			bool use64;
			if (get_os_name() == "X11") {
				const String &arch = get_preset_arch(p_preset);
				use64 = (arch == "x86_64" || arch == "arm64");
			} else {
				use64 = p_preset->get("binary_format/64_bits");
			}
			if (!use64) {
				add_message(EXPORT_MESSAGE_ERROR, TTR("PCK Embedding"), TTR("On 32-bit exports the embedded PCK cannot be bigger than 4 GiB."));
				return ERR_INVALID_PARAMETER;
			}
		}

		err = fixup_embedded_pck(p_path, embedded_pos, embedded_size);
	}

	if (err == OK && !so_files.empty()) {
		// If shared object files, copy them.
		DirAccessRef da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		for (int i = 0; i < so_files.size() && err == OK; i++) {
			err = da->copy(so_files[i].path, p_path.get_base_dir().plus_file(so_files[i].path.get_file()));
			if (err == OK) {
				err = sign_shared_object(p_preset, p_debug, p_path.get_base_dir().plus_file(so_files[i].path.get_file()));
			}
		}
	}

	return err;
}

Error EditorExportPlatformPC::sign_shared_object(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path) {
	return OK;
}

void EditorExportPlatformPC::set_extension(const String &p_extension, const String &p_feature_key) {
	extensions[p_feature_key] = p_extension;
}

void EditorExportPlatformPC::set_name(const String &p_name) {
	name = p_name;
}

void EditorExportPlatformPC::set_os_name(const String &p_name) {
	os_name = p_name;
}

void EditorExportPlatformPC::set_logo(const Ref<Texture> &p_logo) {
	logo = p_logo;
}

void EditorExportPlatformPC::set_release_64(const String &p_file) {
	release_file_64 = p_file;
}

void EditorExportPlatformPC::set_release_32(const String &p_file) {
	release_file_32 = p_file;
}

void EditorExportPlatformPC::set_debug_64(const String &p_file) {
	debug_file_64 = p_file;
}

void EditorExportPlatformPC::set_debug_32(const String &p_file) {
	debug_file_32 = p_file;
}

// For Linux only.
void EditorExportPlatformPC::set_release_files(const String &p_arch, const String &p_file) {
	release_files[p_arch] = p_file;
}

void EditorExportPlatformPC::set_debug_files(const String &p_arch, const String &p_file) {
	debug_files[p_arch] = p_file;
}

String EditorExportPlatformPC::get_preset_arch(const Ref<EditorExportPreset> &p_preset) const {
	String arch = p_preset->get("binary_format/architecture");
	if (arch != "x86_64" && arch != "x86_32" && arch != "arm64" && arch != "arm32") {
		ERR_PRINT(vformat("Invalid value \"%s\" for \"binary_format/architecture\" in export preset \"%s\". Defaulting to \"x86_64\".",
				arch, p_preset->get_name()));
		arch = "x86_64";
	}
	return arch;
}

void EditorExportPlatformPC::add_platform_feature(const String &p_feature) {
	extra_features.insert(p_feature);
}

void EditorExportPlatformPC::get_platform_features(List<String> *r_features) {
	r_features->push_back("pc"); //all pcs support "pc"
	r_features->push_back("s3tc"); //all pcs support "s3tc" compression
	r_features->push_back(get_os_name()); //OS name is a feature
	for (Set<String>::Element *E = extra_features.front(); E; E = E->next()) {
		r_features->push_back(E->get());
	}
}

void EditorExportPlatformPC::resolve_platform_feature_priorities(const Ref<EditorExportPreset> &p_preset, Set<String> &p_features) {
	if (p_features.has("bptc")) {
		if (p_preset->has("texture_format/no_bptc_fallbacks")) {
			p_features.erase("s3tc");
		}
	}
}

int EditorExportPlatformPC::get_chmod_flags() const {
	return chmod_flags;
}

void EditorExportPlatformPC::set_chmod_flags(int p_flags) {
	chmod_flags = p_flags;
}

EditorExportPlatformPC::EditorExportPlatformPC() {
	chmod_flags = -1;
}

///////////////////////

void EditorExportTextSceneToBinaryPlugin::_export_file(const String &p_path, const String &p_type, const Set<String> &p_features) {
	String extension = p_path.get_extension().to_lower();
	if (extension != "tres" && extension != "tscn") {
		return;
	}

	bool convert = GLOBAL_GET("editor/convert_text_resources_to_binary_on_export");
	if (!convert) {
		return;
	}

	int flg = 0;
	if (GLOBAL_GET("filesystem/on_save/compress_binary_resources")) {
		flg |= ResourceSaver::FLAG_COMPRESS;
	}

	String tmp_path = EditorSettings::get_singleton()->get_cache_dir().plus_file("tmpfile.res");
	Error err;
	RES res = ResourceFormatLoaderText::singleton->load(p_path, p_path, &err);
	if (err != OK) {
		DirAccess::remove_file_or_error(tmp_path);
		ERR_FAIL_MSG("Cannot load text resource from path '" + p_path + "'.");
	}

	err = ResourceSaver::save(tmp_path, res, flg);
	if (err != OK) {
		DirAccess::remove_file_or_error(tmp_path);
		ERR_FAIL_MSG("Cannot save resource to file '" + tmp_path);
	}

	Vector<uint8_t> data = FileAccess::get_file_as_array(tmp_path);
	if (data.size() == 0) {
		DirAccess::remove_file_or_error(tmp_path);
		ERR_FAIL();
	}
	DirAccess::remove_file_or_error(tmp_path);
	if (extension == "tscn") {
		add_file(p_path + ".converted.scn", data, true);
	} else {
		add_file(p_path + ".converted.res", data, true);
	}
}

EditorExportTextSceneToBinaryPlugin::EditorExportTextSceneToBinaryPlugin() {
	GLOBAL_DEF("editor/convert_text_resources_to_binary_on_export", true);
}
