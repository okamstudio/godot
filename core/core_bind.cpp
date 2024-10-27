/**************************************************************************/
/*  core_bind.cpp                                                         */
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

#include "core_bind.h"
#include "core_bind.compat.inc"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/debugger/engine_debugger.h"
#include "core/debugger/script_debugger.h"
#include "core/io/file_access_compressed.h"
#include "core/io/file_access_encrypted.h"
#include "core/io/marshalls.h"
#include "core/math/geometry_2d.h"
#include "core/math/geometry_3d.h"
#include "core/os/keyboard.h"
#include "core/os/thread_safe.h"
#include "core/variant/typed_array.h"

namespace core_bind {

////// ResourceLoader //////

ResourceLoader *ResourceLoader::singleton = nullptr;

Error ResourceLoader::load_threaded_request(const String &p_path, const String &p_type_hint, bool p_use_sub_threads, CacheMode p_cache_mode) {
	return ::ResourceLoader::load_threaded_request(p_path, p_type_hint, p_use_sub_threads, ResourceFormatLoader::CacheMode(p_cache_mode));
}

ResourceLoader::ThreadLoadStatus ResourceLoader::load_threaded_get_status(const String &p_path, Array r_progress) {
	float progress = 0;
	::ResourceLoader::ThreadLoadStatus tls = ::ResourceLoader::load_threaded_get_status(p_path, &progress);
	// Default array should never be modified, it causes the hash of the method to change.
	if (!ClassDB::is_default_array_arg(r_progress)) {
		r_progress.resize(1);
		r_progress[0] = progress;
	}
	return (ThreadLoadStatus)tls;
}

Ref<Resource> ResourceLoader::load_threaded_get(const String &p_path) {
	Error error;
	Ref<Resource> res = ::ResourceLoader::load_threaded_get(p_path, &error);
	return res;
}

Ref<Resource> ResourceLoader::load(const String &p_path, const String &p_type_hint, CacheMode p_cache_mode) {
	Error err = OK;
	Ref<Resource> ret = ::ResourceLoader::load(p_path, p_type_hint, ResourceFormatLoader::CacheMode(p_cache_mode), &err);

	ERR_FAIL_COND_V_MSG(err != OK, ret, "Error loading resource: '" + p_path + "'.");
	return ret;
}

Vector<String> ResourceLoader::get_recognized_extensions_for_type(const String &p_type) {
	List<String> exts;
	::ResourceLoader::get_recognized_extensions_for_type(p_type, &exts);
	Vector<String> ret;
	for (const String &E : exts) {
		ret.push_back(E);
	}

	return ret;
}

void ResourceLoader::add_resource_format_loader(Ref<ResourceFormatLoader> p_format_loader, bool p_at_front) {
	::ResourceLoader::add_resource_format_loader(p_format_loader, p_at_front);
}

void ResourceLoader::remove_resource_format_loader(Ref<ResourceFormatLoader> p_format_loader) {
	::ResourceLoader::remove_resource_format_loader(p_format_loader);
}

void ResourceLoader::set_abort_on_missing_resources(bool p_abort) {
	::ResourceLoader::set_abort_on_missing_resources(p_abort);
}

PackedStringArray ResourceLoader::get_dependencies(const String &p_path) {
	List<String> deps;
	::ResourceLoader::get_dependencies(p_path, &deps);

	PackedStringArray ret;
	for (const String &E : deps) {
		ret.push_back(E);
	}

	return ret;
}

bool ResourceLoader::has_cached(const String &p_path) {
	String local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	return ResourceCache::has(local_path);
}

Ref<Resource> ResourceLoader::get_cached_ref(const String &p_path) {
	String local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	return ResourceCache::get_ref(local_path);
}

bool ResourceLoader::exists(const String &p_path, const String &p_type_hint) {
	return ::ResourceLoader::exists(p_path, p_type_hint);
}

ResourceUID::ID ResourceLoader::get_resource_uid(const String &p_path) {
	return ::ResourceLoader::get_resource_uid(p_path);
}

void ResourceLoader::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_threaded_request", "path", "type_hint", "use_sub_threads", "cache_mode"), &ResourceLoader::load_threaded_request, DEFVAL(""), DEFVAL(false), DEFVAL(CACHE_MODE_REUSE));
	ClassDB::bind_method(D_METHOD("load_threaded_get_status", "path", "progress"), &ResourceLoader::load_threaded_get_status, DEFVAL_ARRAY);
	ClassDB::bind_method(D_METHOD("load_threaded_get", "path"), &ResourceLoader::load_threaded_get);

	ClassDB::bind_method(D_METHOD("load", "path", "type_hint", "cache_mode"), &ResourceLoader::load, DEFVAL(""), DEFVAL(CACHE_MODE_REUSE));
	ClassDB::bind_method(D_METHOD("get_recognized_extensions_for_type", "type"), &ResourceLoader::get_recognized_extensions_for_type);
	ClassDB::bind_method(D_METHOD("add_resource_format_loader", "format_loader", "at_front"), &ResourceLoader::add_resource_format_loader, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("remove_resource_format_loader", "format_loader"), &ResourceLoader::remove_resource_format_loader);
	ClassDB::bind_method(D_METHOD("set_abort_on_missing_resources", "abort"), &ResourceLoader::set_abort_on_missing_resources);
	ClassDB::bind_method(D_METHOD("get_dependencies", "path"), &ResourceLoader::get_dependencies);
	ClassDB::bind_method(D_METHOD("has_cached", "path"), &ResourceLoader::has_cached);
	ClassDB::bind_method(D_METHOD("get_cached_ref", "path"), &ResourceLoader::get_cached_ref);
	ClassDB::bind_method(D_METHOD("exists", "path", "type_hint"), &ResourceLoader::exists, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("get_resource_uid", "path"), &ResourceLoader::get_resource_uid);

	BIND_ENUM_CONSTANT(THREAD_LOAD_INVALID_RESOURCE);
	BIND_ENUM_CONSTANT(THREAD_LOAD_IN_PROGRESS);
	BIND_ENUM_CONSTANT(THREAD_LOAD_FAILED);
	BIND_ENUM_CONSTANT(THREAD_LOAD_LOADED);

	BIND_ENUM_CONSTANT(CACHE_MODE_IGNORE);
	BIND_ENUM_CONSTANT(CACHE_MODE_REUSE);
	BIND_ENUM_CONSTANT(CACHE_MODE_REPLACE);
	BIND_ENUM_CONSTANT(CACHE_MODE_IGNORE_DEEP);
	BIND_ENUM_CONSTANT(CACHE_MODE_REPLACE_DEEP);
}

////// ResourceSaver //////

Error ResourceSaver::save(const Ref<Resource> &p_resource, const String &p_path, BitField<SaverFlags> p_flags) {
	return ::ResourceSaver::save(p_resource, p_path, p_flags);
}

Vector<String> ResourceSaver::get_recognized_extensions(const Ref<Resource> &p_resource) {
	List<String> exts;
	::ResourceSaver::get_recognized_extensions(p_resource, &exts);
	Vector<String> ret;
	for (const String &E : exts) {
		ret.push_back(E);
	}
	return ret;
}

void ResourceSaver::add_resource_format_saver(Ref<ResourceFormatSaver> p_format_saver, bool p_at_front) {
	::ResourceSaver::add_resource_format_saver(p_format_saver, p_at_front);
}

void ResourceSaver::remove_resource_format_saver(Ref<ResourceFormatSaver> p_format_saver) {
	::ResourceSaver::remove_resource_format_saver(p_format_saver);
}

ResourceUID::ID ResourceSaver::get_resource_id_for_path(const String &p_path, bool p_generate) {
	return ::ResourceSaver::get_resource_id_for_path(p_path, p_generate);
}

ResourceSaver *ResourceSaver::singleton = nullptr;

void ResourceSaver::_bind_methods() {
	ClassDB::bind_method(D_METHOD("save", "resource", "path", "flags"), &ResourceSaver::save, DEFVAL(""), DEFVAL((uint32_t)FLAG_NONE));
	ClassDB::bind_method(D_METHOD("get_recognized_extensions", "type"), &ResourceSaver::get_recognized_extensions);
	ClassDB::bind_method(D_METHOD("add_resource_format_saver", "format_saver", "at_front"), &ResourceSaver::add_resource_format_saver, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("remove_resource_format_saver", "format_saver"), &ResourceSaver::remove_resource_format_saver);
	ClassDB::bind_method(D_METHOD("get_resource_id_for_path", "path", "generate"), &ResourceSaver::get_resource_id_for_path, DEFVAL(false));

	BIND_BITFIELD_FLAG(FLAG_NONE);
	BIND_BITFIELD_FLAG(FLAG_RELATIVE_PATHS);
	BIND_BITFIELD_FLAG(FLAG_BUNDLE_RESOURCES);
	BIND_BITFIELD_FLAG(FLAG_CHANGE_PATH);
	BIND_BITFIELD_FLAG(FLAG_OMIT_EDITOR_PROPERTIES);
	BIND_BITFIELD_FLAG(FLAG_SAVE_BIG_ENDIAN);
	BIND_BITFIELD_FLAG(FLAG_COMPRESS);
	BIND_BITFIELD_FLAG(FLAG_REPLACE_SUBRESOURCE_PATHS);
}

////// OS //////

PackedByteArray OS::get_entropy(int p_bytes) {
	PackedByteArray pba;
	pba.resize(p_bytes);
	Error err = ::OS::get_singleton()->get_entropy(pba.ptrw(), p_bytes);
	ERR_FAIL_COND_V(err != OK, PackedByteArray());
	return pba;
}

String OS::get_system_ca_certificates() {
	return ::OS::get_singleton()->get_system_ca_certificates();
}

PackedStringArray OS::get_connected_midi_inputs() {
	return ::OS::get_singleton()->get_connected_midi_inputs();
}

void OS::open_midi_inputs() {
	::OS::get_singleton()->open_midi_inputs();
}

void OS::close_midi_inputs() {
	::OS::get_singleton()->close_midi_inputs();
}

void OS::set_use_file_access_save_and_swap(bool p_enable) {
	FileAccess::set_backup_save(p_enable);
}

void OS::set_low_processor_usage_mode(bool p_enabled) {
	::OS::get_singleton()->set_low_processor_usage_mode(p_enabled);
}

bool OS::is_in_low_processor_usage_mode() const {
	return ::OS::get_singleton()->is_in_low_processor_usage_mode();
}

void OS::set_low_processor_usage_mode_sleep_usec(int p_usec) {
	::OS::get_singleton()->set_low_processor_usage_mode_sleep_usec(p_usec);
}

int OS::get_low_processor_usage_mode_sleep_usec() const {
	return ::OS::get_singleton()->get_low_processor_usage_mode_sleep_usec();
}

void OS::set_delta_smoothing(bool p_enabled) {
	::OS::get_singleton()->set_delta_smoothing(p_enabled);
}

bool OS::is_delta_smoothing_enabled() const {
	return ::OS::get_singleton()->is_delta_smoothing_enabled();
}

void OS::alert(const String &p_alert, const String &p_title) {
	::OS::get_singleton()->alert(p_alert, p_title);
}

void OS::crash(const String &p_message) {
	CRASH_NOW_MSG(p_message);
}

Vector<String> OS::get_system_fonts() const {
	return ::OS::get_singleton()->get_system_fonts();
}

String OS::get_system_font_path(const String &p_font_name, int p_weight, int p_stretch, bool p_italic) const {
	return ::OS::get_singleton()->get_system_font_path(p_font_name, p_weight, p_stretch, p_italic);
}

Vector<String> OS::get_system_font_path_for_text(const String &p_font_name, const String &p_text, const String &p_locale, const String &p_script, int p_weight, int p_stretch, bool p_italic) const {
	return ::OS::get_singleton()->get_system_font_path_for_text(p_font_name, p_text, p_locale, p_script, p_weight, p_stretch, p_italic);
}

String OS::get_executable_path() const {
	return ::OS::get_singleton()->get_executable_path();
}

Error OS::shell_open(const String &p_uri) {
	if (p_uri.begins_with("res://")) {
		WARN_PRINT("Attempting to open an URL with the \"res://\" protocol. Use `ProjectSettings.globalize_path()` to convert a Godot-specific path to a system path before opening it with `OS.shell_open()`.");
	} else if (p_uri.begins_with("user://")) {
		WARN_PRINT("Attempting to open an URL with the \"user://\" protocol. Use `ProjectSettings.globalize_path()` to convert a Godot-specific path to a system path before opening it with `OS.shell_open()`.");
	}
	return ::OS::get_singleton()->shell_open(p_uri);
}

Error OS::shell_show_in_file_manager(const String &p_path, bool p_open_folder) {
	if (p_path.begins_with("res://")) {
		WARN_PRINT("Attempting to explore file path with the \"res://\" protocol. Use `ProjectSettings.globalize_path()` to convert a Godot-specific path to a system path before opening it with `OS.shell_show_in_file_manager()`.");
	} else if (p_path.begins_with("user://")) {
		WARN_PRINT("Attempting to explore file path with the \"user://\" protocol. Use `ProjectSettings.globalize_path()` to convert a Godot-specific path to a system path before opening it with `OS.shell_show_in_file_manager()`.");
	}
	return ::OS::get_singleton()->shell_show_in_file_manager(p_path, p_open_folder);
}

String OS::read_string_from_stdin() {
	return ::OS::get_singleton()->get_stdin_string();
}

int OS::execute(const String &p_path, const Vector<String> &p_arguments, Array r_output, bool p_read_stderr, bool p_open_console) {
	List<String> args;
	for (const String &arg : p_arguments) {
		args.push_back(arg);
	}
	String pipe;
	int exitcode = 0;
	Error err = ::OS::get_singleton()->execute(p_path, args, &pipe, &exitcode, p_read_stderr, nullptr, p_open_console);
	// Default array should never be modified, it causes the hash of the method to change.
	if (!ClassDB::is_default_array_arg(r_output)) {
		r_output.push_back(pipe);
	}
	if (err != OK) {
		return -1;
	}
	return exitcode;
}

Dictionary OS::execute_with_pipe(const String &p_path, const Vector<String> &p_arguments, bool p_blocking) {
	List<String> args;
	for (const String &arg : p_arguments) {
		args.push_back(arg);
	}
	return ::OS::get_singleton()->execute_with_pipe(p_path, args, p_blocking);
}

int OS::create_instance(const Vector<String> &p_arguments) {
	List<String> args;
	for (const String &arg : p_arguments) {
		args.push_back(arg);
	}
	::OS::ProcessID pid = 0;
	Error err = ::OS::get_singleton()->create_instance(args, &pid);
	if (err != OK) {
		return -1;
	}
	return pid;
}

int OS::create_process(const String &p_path, const Vector<String> &p_arguments, bool p_open_console) {
	List<String> args;
	for (const String &arg : p_arguments) {
		args.push_back(arg);
	}
	::OS::ProcessID pid = 0;
	Error err = ::OS::get_singleton()->create_process(p_path, args, &pid, p_open_console);
	if (err != OK) {
		return -1;
	}
	return pid;
}

Error OS::kill(int p_pid) {
	return ::OS::get_singleton()->kill(p_pid);
}

bool OS::is_process_running(int p_pid) const {
	return ::OS::get_singleton()->is_process_running(p_pid);
}

int OS::get_process_exit_code(int p_pid) const {
	return ::OS::get_singleton()->get_process_exit_code(p_pid);
}

int OS::get_process_id() const {
	return ::OS::get_singleton()->get_process_id();
}

bool OS::has_environment(const String &p_var) const {
	return ::OS::get_singleton()->has_environment(p_var);
}

String OS::get_environment(const String &p_var) const {
	return ::OS::get_singleton()->get_environment(p_var);
}

void OS::set_environment(const String &p_var, const String &p_value) const {
	::OS::get_singleton()->set_environment(p_var, p_value);
}

void OS::unset_environment(const String &p_var) const {
	::OS::get_singleton()->unset_environment(p_var);
}

String OS::get_name() const {
	return ::OS::get_singleton()->get_name();
}

String OS::get_distribution_name() const {
	return ::OS::get_singleton()->get_distribution_name();
}

String OS::get_version() const {
	return ::OS::get_singleton()->get_version();
}

Vector<String> OS::get_video_adapter_driver_info() const {
	return ::OS::get_singleton()->get_video_adapter_driver_info();
}

Vector<String> OS::get_cmdline_args() {
	List<String> cmdline = ::OS::get_singleton()->get_cmdline_args();
	Vector<String> cmdlinev;
	for (const String &E : cmdline) {
		cmdlinev.push_back(E);
	}

	return cmdlinev;
}

Vector<String> OS::get_cmdline_user_args() {
	List<String> cmdline = ::OS::get_singleton()->get_cmdline_user_args();
	Vector<String> cmdlinev;
	for (const String &E : cmdline) {
		cmdlinev.push_back(E);
	}

	return cmdlinev;
}

void OS::set_restart_on_exit(bool p_restart, const Vector<String> &p_restart_arguments) {
	List<String> args_list;
	for (const String &restart_argument : p_restart_arguments) {
		args_list.push_back(restart_argument);
	}

	::OS::get_singleton()->set_restart_on_exit(p_restart, args_list);
}

bool OS::is_restart_on_exit_set() const {
	return ::OS::get_singleton()->is_restart_on_exit_set();
}

Vector<String> OS::get_restart_on_exit_arguments() const {
	List<String> args = ::OS::get_singleton()->get_restart_on_exit_arguments();
	Vector<String> args_vector;
	for (List<String>::Element *E = args.front(); E; E = E->next()) {
		args_vector.push_back(E->get());
	}

	return args_vector;
}

String OS::get_locale() const {
	return ::OS::get_singleton()->get_locale();
}

String OS::get_locale_language() const {
	return ::OS::get_singleton()->get_locale_language();
}

String OS::get_model_name() const {
	return ::OS::get_singleton()->get_model_name();
}

Error OS::set_thread_name(const String &p_name) {
	return ::Thread::set_name(p_name);
}

::Thread::ID OS::get_thread_caller_id() const {
	return ::Thread::get_caller_id();
};

::Thread::ID OS::get_main_thread_id() const {
	return ::Thread::get_main_id();
};

bool OS::has_feature(const String &p_feature) const {
	const bool *value_ptr = feature_cache.getptr(p_feature);
	if (value_ptr) {
		return *value_ptr;
	} else {
		const bool has = ::OS::get_singleton()->has_feature(p_feature);
		feature_cache[p_feature] = has;
		return has;
	}
}

bool OS::is_sandboxed() const {
	return ::OS::get_singleton()->is_sandboxed();
}

uint64_t OS::get_static_memory_usage() const {
	return ::OS::get_singleton()->get_static_memory_usage();
}

uint64_t OS::get_static_memory_peak_usage() const {
	return ::OS::get_singleton()->get_static_memory_peak_usage();
}

Dictionary OS::get_memory_info() const {
	return ::OS::get_singleton()->get_memory_info();
}

/** This method uses a signed argument for better error reporting as it's used from the scripting API. */
void OS::delay_usec(int p_usec) const {
	ERR_FAIL_COND_MSG(
			p_usec < 0,
			vformat("Can't sleep for %d microseconds. The delay provided must be greater than or equal to 0 microseconds.", p_usec));
	::OS::get_singleton()->delay_usec(p_usec);
}

/** This method uses a signed argument for better error reporting as it's used from the scripting API. */
void OS::delay_msec(int p_msec) const {
	ERR_FAIL_COND_MSG(
			p_msec < 0,
			vformat("Can't sleep for %d milliseconds. The delay provided must be greater than or equal to 0 milliseconds.", p_msec));
	::OS::get_singleton()->delay_usec(int64_t(p_msec) * 1000);
}

bool OS::is_userfs_persistent() const {
	return ::OS::get_singleton()->is_userfs_persistent();
}

int OS::get_processor_count() const {
	return ::OS::get_singleton()->get_processor_count();
}

String OS::get_processor_name() const {
	return ::OS::get_singleton()->get_processor_name();
}

bool OS::is_stdout_verbose() const {
	return ::OS::get_singleton()->is_stdout_verbose();
}

Error OS::move_to_trash(const String &p_path) const {
	return ::OS::get_singleton()->move_to_trash(p_path);
}

String OS::get_user_data_dir() const {
	return ::OS::get_singleton()->get_user_data_dir();
}

String OS::get_config_dir() const {
	// Exposed as `get_config_dir()` instead of `get_config_path()` for consistency with other exposed OS methods.
	return ::OS::get_singleton()->get_config_path();
}

String OS::get_data_dir() const {
	// Exposed as `get_data_dir()` instead of `get_data_path()` for consistency with other exposed OS methods.
	return ::OS::get_singleton()->get_data_path();
}

String OS::get_cache_dir() const {
	// Exposed as `get_cache_dir()` instead of `get_cache_path()` for consistency with other exposed OS methods.
	return ::OS::get_singleton()->get_cache_path();
}

bool OS::is_debug_build() const {
#ifdef DEBUG_ENABLED
	return true;
#else
	return false;
#endif
}

String OS::get_system_dir(SystemDir p_dir, bool p_shared_storage) const {
	return ::OS::get_singleton()->get_system_dir(::OS::SystemDir(p_dir), p_shared_storage);
}

String OS::get_keycode_string(Key p_code) const {
	return ::keycode_get_string(p_code);
}

bool OS::is_keycode_unicode(char32_t p_unicode) const {
	return ::keycode_has_unicode((Key)p_unicode);
}

Key OS::find_keycode_from_string(const String &p_code) const {
	return find_keycode(p_code);
}

bool OS::request_permission(const String &p_name) {
	return ::OS::get_singleton()->request_permission(p_name);
}

bool OS::request_permissions() {
	return ::OS::get_singleton()->request_permissions();
}

Vector<String> OS::get_granted_permissions() const {
	return ::OS::get_singleton()->get_granted_permissions();
}

void OS::revoke_granted_permissions() {
	::OS::get_singleton()->revoke_granted_permissions();
}

String OS::get_unique_id() const {
	return ::OS::get_singleton()->get_unique_id();
}

OS *OS::singleton = nullptr;

void OS::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_entropy", "size"), &OS::get_entropy);
	ClassDB::bind_method(D_METHOD("get_system_ca_certificates"), &OS::get_system_ca_certificates);
	ClassDB::bind_method(D_METHOD("get_connected_midi_inputs"), &OS::get_connected_midi_inputs);
	ClassDB::bind_method(D_METHOD("open_midi_inputs"), &OS::open_midi_inputs);
	ClassDB::bind_method(D_METHOD("close_midi_inputs"), &OS::close_midi_inputs);

	ClassDB::bind_method(D_METHOD("alert", "text", "title"), &OS::alert, DEFVAL("Alert!"));
	ClassDB::bind_method(D_METHOD("crash", "message"), &OS::crash);

	ClassDB::bind_method(D_METHOD("set_low_processor_usage_mode", "enable"), &OS::set_low_processor_usage_mode);
	ClassDB::bind_method(D_METHOD("is_in_low_processor_usage_mode"), &OS::is_in_low_processor_usage_mode);

	ClassDB::bind_method(D_METHOD("set_low_processor_usage_mode_sleep_usec", "usec"), &OS::set_low_processor_usage_mode_sleep_usec);
	ClassDB::bind_method(D_METHOD("get_low_processor_usage_mode_sleep_usec"), &OS::get_low_processor_usage_mode_sleep_usec);

	ClassDB::bind_method(D_METHOD("set_delta_smoothing", "delta_smoothing_enabled"), &OS::set_delta_smoothing);
	ClassDB::bind_method(D_METHOD("is_delta_smoothing_enabled"), &OS::is_delta_smoothing_enabled);

	ClassDB::bind_method(D_METHOD("get_processor_count"), &OS::get_processor_count);
	ClassDB::bind_method(D_METHOD("get_processor_name"), &OS::get_processor_name);

	ClassDB::bind_method(D_METHOD("get_system_fonts"), &OS::get_system_fonts);
	ClassDB::bind_method(D_METHOD("get_system_font_path", "font_name", "weight", "stretch", "italic"), &OS::get_system_font_path, DEFVAL(400), DEFVAL(100), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_system_font_path_for_text", "font_name", "text", "locale", "script", "weight", "stretch", "italic"), &OS::get_system_font_path_for_text, DEFVAL(String()), DEFVAL(String()), DEFVAL(400), DEFVAL(100), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_executable_path"), &OS::get_executable_path);
	ClassDB::bind_method(D_METHOD("read_string_from_stdin"), &OS::read_string_from_stdin);
	ClassDB::bind_method(D_METHOD("execute", "path", "arguments", "output", "read_stderr", "open_console"), &OS::execute, DEFVAL_ARRAY, DEFVAL(false), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("execute_with_pipe", "path", "arguments", "blocking"), &OS::execute_with_pipe, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("create_process", "path", "arguments", "open_console"), &OS::create_process, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("create_instance", "arguments"), &OS::create_instance);
	ClassDB::bind_method(D_METHOD("kill", "pid"), &OS::kill);
	ClassDB::bind_method(D_METHOD("shell_open", "uri"), &OS::shell_open);
	ClassDB::bind_method(D_METHOD("shell_show_in_file_manager", "file_or_dir_path", "open_folder"), &OS::shell_show_in_file_manager, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("is_process_running", "pid"), &OS::is_process_running);
	ClassDB::bind_method(D_METHOD("get_process_exit_code", "pid"), &OS::get_process_exit_code);
	ClassDB::bind_method(D_METHOD("get_process_id"), &OS::get_process_id);

	ClassDB::bind_method(D_METHOD("has_environment", "variable"), &OS::has_environment);
	ClassDB::bind_method(D_METHOD("get_environment", "variable"), &OS::get_environment);
	ClassDB::bind_method(D_METHOD("set_environment", "variable", "value"), &OS::set_environment);
	ClassDB::bind_method(D_METHOD("unset_environment", "variable"), &OS::unset_environment);

	ClassDB::bind_method(D_METHOD("get_name"), &OS::get_name);
	ClassDB::bind_method(D_METHOD("get_distribution_name"), &OS::get_distribution_name);
	ClassDB::bind_method(D_METHOD("get_version"), &OS::get_version);
	ClassDB::bind_method(D_METHOD("get_cmdline_args"), &OS::get_cmdline_args);
	ClassDB::bind_method(D_METHOD("get_cmdline_user_args"), &OS::get_cmdline_user_args);

	ClassDB::bind_method(D_METHOD("get_video_adapter_driver_info"), &OS::get_video_adapter_driver_info);

	ClassDB::bind_method(D_METHOD("set_restart_on_exit", "restart", "arguments"), &OS::set_restart_on_exit, DEFVAL(Vector<String>()));
	ClassDB::bind_method(D_METHOD("is_restart_on_exit_set"), &OS::is_restart_on_exit_set);
	ClassDB::bind_method(D_METHOD("get_restart_on_exit_arguments"), &OS::get_restart_on_exit_arguments);

	ClassDB::bind_method(D_METHOD("delay_usec", "usec"), &OS::delay_usec);
	ClassDB::bind_method(D_METHOD("delay_msec", "msec"), &OS::delay_msec);
	ClassDB::bind_method(D_METHOD("get_locale"), &OS::get_locale);
	ClassDB::bind_method(D_METHOD("get_locale_language"), &OS::get_locale_language);
	ClassDB::bind_method(D_METHOD("get_model_name"), &OS::get_model_name);

	ClassDB::bind_method(D_METHOD("is_userfs_persistent"), &OS::is_userfs_persistent);
	ClassDB::bind_method(D_METHOD("is_stdout_verbose"), &OS::is_stdout_verbose);

	ClassDB::bind_method(D_METHOD("is_debug_build"), &OS::is_debug_build);

	ClassDB::bind_method(D_METHOD("get_static_memory_usage"), &OS::get_static_memory_usage);
	ClassDB::bind_method(D_METHOD("get_static_memory_peak_usage"), &OS::get_static_memory_peak_usage);
	ClassDB::bind_method(D_METHOD("get_memory_info"), &OS::get_memory_info);

	ClassDB::bind_method(D_METHOD("move_to_trash", "path"), &OS::move_to_trash);
	ClassDB::bind_method(D_METHOD("get_user_data_dir"), &OS::get_user_data_dir);
	ClassDB::bind_method(D_METHOD("get_system_dir", "dir", "shared_storage"), &OS::get_system_dir, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_config_dir"), &OS::get_config_dir);
	ClassDB::bind_method(D_METHOD("get_data_dir"), &OS::get_data_dir);
	ClassDB::bind_method(D_METHOD("get_cache_dir"), &OS::get_cache_dir);
	ClassDB::bind_method(D_METHOD("get_unique_id"), &OS::get_unique_id);

	ClassDB::bind_method(D_METHOD("get_keycode_string", "code"), &OS::get_keycode_string);
	ClassDB::bind_method(D_METHOD("is_keycode_unicode", "code"), &OS::is_keycode_unicode);
	ClassDB::bind_method(D_METHOD("find_keycode_from_string", "string"), &OS::find_keycode_from_string);

	ClassDB::bind_method(D_METHOD("set_use_file_access_save_and_swap", "enabled"), &OS::set_use_file_access_save_and_swap);

	ClassDB::bind_method(D_METHOD("set_thread_name", "name"), &OS::set_thread_name);
	ClassDB::bind_method(D_METHOD("get_thread_caller_id"), &OS::get_thread_caller_id);
	ClassDB::bind_method(D_METHOD("get_main_thread_id"), &OS::get_main_thread_id);

	ClassDB::bind_method(D_METHOD("has_feature", "tag_name"), &OS::has_feature);
	ClassDB::bind_method(D_METHOD("is_sandboxed"), &OS::is_sandboxed);

	ClassDB::bind_method(D_METHOD("request_permission", "name"), &OS::request_permission);
	ClassDB::bind_method(D_METHOD("request_permissions"), &OS::request_permissions);
	ClassDB::bind_method(D_METHOD("get_granted_permissions"), &OS::get_granted_permissions);
	ClassDB::bind_method(D_METHOD("revoke_granted_permissions"), &OS::revoke_granted_permissions);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "low_processor_usage_mode"), "set_low_processor_usage_mode", "is_in_low_processor_usage_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "low_processor_usage_mode_sleep_usec"), "set_low_processor_usage_mode_sleep_usec", "get_low_processor_usage_mode_sleep_usec");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "delta_smoothing"), "set_delta_smoothing", "is_delta_smoothing_enabled");

	// Those default values need to be specified for the docs generator,
	// to avoid using values from the documentation writer's own OS instance.
	ADD_PROPERTY_DEFAULT("low_processor_usage_mode", false);
	ADD_PROPERTY_DEFAULT("low_processor_usage_mode_sleep_usec", 6900);

	BIND_ENUM_CONSTANT(RENDERING_DRIVER_VULKAN);
	BIND_ENUM_CONSTANT(RENDERING_DRIVER_OPENGL3);
	BIND_ENUM_CONSTANT(RENDERING_DRIVER_D3D12);
	BIND_ENUM_CONSTANT(RENDERING_DRIVER_METAL);

	BIND_ENUM_CONSTANT(SYSTEM_DIR_DESKTOP);
	BIND_ENUM_CONSTANT(SYSTEM_DIR_DCIM);
	BIND_ENUM_CONSTANT(SYSTEM_DIR_DOCUMENTS);
	BIND_ENUM_CONSTANT(SYSTEM_DIR_DOWNLOADS);
	BIND_ENUM_CONSTANT(SYSTEM_DIR_MOVIES);
	BIND_ENUM_CONSTANT(SYSTEM_DIR_MUSIC);
	BIND_ENUM_CONSTANT(SYSTEM_DIR_PICTURES);
	BIND_ENUM_CONSTANT(SYSTEM_DIR_RINGTONES);
}

////// Geometry2D //////

Geometry2D *Geometry2D::singleton = nullptr;

Geometry2D *Geometry2D::get_singleton() {
	return singleton;
}

bool Geometry2D::is_point_in_circle(const Vector2 &p_point, const Vector2 &p_circle_pos, real_t p_circle_radius) {
	return ::Geometry2D::is_point_in_circle(p_point, p_circle_pos, p_circle_radius);
}

real_t Geometry2D::segment_intersects_circle(const Vector2 &p_from, const Vector2 &p_to, const Vector2 &p_circle_pos, real_t p_circle_radius) {
	return ::Geometry2D::segment_intersects_circle(p_from, p_to, p_circle_pos, p_circle_radius);
}

Variant Geometry2D::segment_intersects_segment(const Vector2 &p_from_a, const Vector2 &p_to_a, const Vector2 &p_from_b, const Vector2 &p_to_b) {
	Vector2 result;
	if (::Geometry2D::segment_intersects_segment(p_from_a, p_to_a, p_from_b, p_to_b, &result)) {
		return result;
	} else {
		return Variant();
	}
}

Variant Geometry2D::line_intersects_line(const Vector2 &p_from_a, const Vector2 &p_dir_a, const Vector2 &p_from_b, const Vector2 &p_dir_b) {
	Vector2 result;
	if (::Geometry2D::line_intersects_line(p_from_a, p_dir_a, p_from_b, p_dir_b, result)) {
		return result;
	} else {
		return Variant();
	}
}

Vector<Vector2> Geometry2D::get_closest_points_between_segments(const Vector2 &p1, const Vector2 &q1, const Vector2 &p2, const Vector2 &q2) {
	Vector2 r1, r2;
	::Geometry2D::get_closest_points_between_segments(p1, q1, p2, q2, r1, r2);
	Vector<Vector2> r = { r1, r2 };
	return r;
}

Vector2 Geometry2D::get_closest_point_to_segment(const Vector2 &p_point, const Vector2 &p_a, const Vector2 &p_b) {
	Vector2 s[2] = { p_a, p_b };
	return ::Geometry2D::get_closest_point_to_segment(p_point, s);
}

Vector2 Geometry2D::get_closest_point_to_segment_uncapped(const Vector2 &p_point, const Vector2 &p_a, const Vector2 &p_b) {
	Vector2 s[2] = { p_a, p_b };
	return ::Geometry2D::get_closest_point_to_segment_uncapped(p_point, s);
}

bool Geometry2D::point_is_inside_triangle(const Vector2 &s, const Vector2 &a, const Vector2 &b, const Vector2 &c) const {
	return ::Geometry2D::is_point_in_triangle(s, a, b, c);
}

bool Geometry2D::is_polygon_clockwise(const Vector<Vector2> &p_polygon) {
	return ::Geometry2D::is_polygon_clockwise(p_polygon);
}

bool Geometry2D::is_point_in_polygon(const Point2 &p_point, const Vector<Vector2> &p_polygon) {
	return ::Geometry2D::is_point_in_polygon(p_point, p_polygon);
}

Vector<int> Geometry2D::triangulate_polygon(const Vector<Vector2> &p_polygon) {
	return ::Geometry2D::triangulate_polygon(p_polygon);
}

Vector<int> Geometry2D::triangulate_delaunay(const Vector<Vector2> &p_points) {
	return ::Geometry2D::triangulate_delaunay(p_points);
}

Vector<Point2> Geometry2D::convex_hull(const Vector<Point2> &p_points) {
	return ::Geometry2D::convex_hull(p_points);
}

TypedArray<PackedVector2Array> Geometry2D::decompose_polygon_in_convex(const Vector<Vector2> &p_polygon) {
	Vector<Vector<Point2>> decomp = ::Geometry2D::decompose_polygon_in_convex(p_polygon);

	TypedArray<PackedVector2Array> ret;

	for (int i = 0; i < decomp.size(); ++i) {
		ret.push_back(decomp[i]);
	}
	return ret;
}

TypedArray<PackedVector2Array> Geometry2D::merge_polygons(const Vector<Vector2> &p_polygon_a, const Vector<Vector2> &p_polygon_b) {
	Vector<Vector<Point2>> polys = ::Geometry2D::merge_polygons(p_polygon_a, p_polygon_b);

	TypedArray<PackedVector2Array> ret;

	for (int i = 0; i < polys.size(); ++i) {
		ret.push_back(polys[i]);
	}
	return ret;
}

TypedArray<PackedVector2Array> Geometry2D::clip_polygons(const Vector<Vector2> &p_polygon_a, const Vector<Vector2> &p_polygon_b) {
	Vector<Vector<Point2>> polys = ::Geometry2D::clip_polygons(p_polygon_a, p_polygon_b);

	TypedArray<PackedVector2Array> ret;

	for (int i = 0; i < polys.size(); ++i) {
		ret.push_back(polys[i]);
	}
	return ret;
}

TypedArray<PackedVector2Array> Geometry2D::intersect_polygons(const Vector<Vector2> &p_polygon_a, const Vector<Vector2> &p_polygon_b) {
	Vector<Vector<Point2>> polys = ::Geometry2D::intersect_polygons(p_polygon_a, p_polygon_b);

	TypedArray<PackedVector2Array> ret;

	for (int i = 0; i < polys.size(); ++i) {
		ret.push_back(polys[i]);
	}
	return ret;
}

TypedArray<PackedVector2Array> Geometry2D::exclude_polygons(const Vector<Vector2> &p_polygon_a, const Vector<Vector2> &p_polygon_b) {
	Vector<Vector<Point2>> polys = ::Geometry2D::exclude_polygons(p_polygon_a, p_polygon_b);

	TypedArray<PackedVector2Array> ret;

	for (int i = 0; i < polys.size(); ++i) {
		ret.push_back(polys[i]);
	}
	return ret;
}

TypedArray<PackedVector2Array> Geometry2D::clip_polyline_with_polygon(const Vector<Vector2> &p_polyline, const Vector<Vector2> &p_polygon) {
	Vector<Vector<Point2>> polys = ::Geometry2D::clip_polyline_with_polygon(p_polyline, p_polygon);

	TypedArray<PackedVector2Array> ret;

	for (int i = 0; i < polys.size(); ++i) {
		ret.push_back(polys[i]);
	}
	return ret;
}

TypedArray<PackedVector2Array> Geometry2D::intersect_polyline_with_polygon(const Vector<Vector2> &p_polyline, const Vector<Vector2> &p_polygon) {
	Vector<Vector<Point2>> polys = ::Geometry2D::intersect_polyline_with_polygon(p_polyline, p_polygon);

	TypedArray<PackedVector2Array> ret;

	for (int i = 0; i < polys.size(); ++i) {
		ret.push_back(polys[i]);
	}
	return ret;
}

TypedArray<PackedVector2Array> Geometry2D::offset_polygon(const Vector<Vector2> &p_polygon, real_t p_delta, PolyJoinType p_join_type) {
	Vector<Vector<Point2>> polys = ::Geometry2D::offset_polygon(p_polygon, p_delta, ::Geometry2D::PolyJoinType(p_join_type));

	TypedArray<PackedVector2Array> ret;

	for (int i = 0; i < polys.size(); ++i) {
		ret.push_back(polys[i]);
	}
	return ret;
}

TypedArray<PackedVector2Array> Geometry2D::offset_polyline(const Vector<Vector2> &p_polygon, real_t p_delta, PolyJoinType p_join_type, PolyEndType p_end_type) {
	Vector<Vector<Point2>> polys = ::Geometry2D::offset_polyline(p_polygon, p_delta, ::Geometry2D::PolyJoinType(p_join_type), ::Geometry2D::PolyEndType(p_end_type));

	TypedArray<PackedVector2Array> ret;

	for (int i = 0; i < polys.size(); ++i) {
		ret.push_back(polys[i]);
	}
	return ret;
}

Dictionary Geometry2D::make_atlas(const Vector<Size2> &p_rects) {
	Dictionary ret;

	Vector<Size2i> rects;
	for (int i = 0; i < p_rects.size(); i++) {
		rects.push_back(p_rects[i]);
	}

	Vector<Point2i> result;
	Size2i size;

	::Geometry2D::make_atlas(rects, result, size);

	Vector<Point2> r_result;
	for (int i = 0; i < result.size(); i++) {
		r_result.push_back(result[i]);
	}

	ret["points"] = r_result;
	ret["size"] = size;

	return ret;
}

TypedArray<Point2i> Geometry2D::bresenham_line(const Point2i &p_from, const Point2i &p_to) {
	Vector<Point2i> points = ::Geometry2D::bresenham_line(p_from, p_to);

	TypedArray<Point2i> result;
	result.resize(points.size());

	for (int i = 0; i < points.size(); i++) {
		result[i] = points[i];
	}

	return result;
}

void Geometry2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_point_in_circle", "point", "circle_position", "circle_radius"), &Geometry2D::is_point_in_circle);
	ClassDB::bind_method(D_METHOD("segment_intersects_circle", "segment_from", "segment_to", "circle_position", "circle_radius"), &Geometry2D::segment_intersects_circle);
	ClassDB::bind_method(D_METHOD("segment_intersects_segment", "from_a", "to_a", "from_b", "to_b"), &Geometry2D::segment_intersects_segment);
	ClassDB::bind_method(D_METHOD("line_intersects_line", "from_a", "dir_a", "from_b", "dir_b"), &Geometry2D::line_intersects_line);

	ClassDB::bind_method(D_METHOD("get_closest_points_between_segments", "p1", "q1", "p2", "q2"), &Geometry2D::get_closest_points_between_segments);

	ClassDB::bind_method(D_METHOD("get_closest_point_to_segment", "point", "s1", "s2"), &Geometry2D::get_closest_point_to_segment);

	ClassDB::bind_method(D_METHOD("get_closest_point_to_segment_uncapped", "point", "s1", "s2"), &Geometry2D::get_closest_point_to_segment_uncapped);

	ClassDB::bind_method(D_METHOD("point_is_inside_triangle", "point", "a", "b", "c"), &Geometry2D::point_is_inside_triangle);

	ClassDB::bind_method(D_METHOD("is_polygon_clockwise", "polygon"), &Geometry2D::is_polygon_clockwise);
	ClassDB::bind_method(D_METHOD("is_point_in_polygon", "point", "polygon"), &Geometry2D::is_point_in_polygon);
	ClassDB::bind_method(D_METHOD("triangulate_polygon", "polygon"), &Geometry2D::triangulate_polygon);
	ClassDB::bind_method(D_METHOD("triangulate_delaunay", "points"), &Geometry2D::triangulate_delaunay);
	ClassDB::bind_method(D_METHOD("convex_hull", "points"), &Geometry2D::convex_hull);
	ClassDB::bind_method(D_METHOD("decompose_polygon_in_convex", "polygon"), &Geometry2D::decompose_polygon_in_convex);

	ClassDB::bind_method(D_METHOD("merge_polygons", "polygon_a", "polygon_b"), &Geometry2D::merge_polygons);
	ClassDB::bind_method(D_METHOD("clip_polygons", "polygon_a", "polygon_b"), &Geometry2D::clip_polygons);
	ClassDB::bind_method(D_METHOD("intersect_polygons", "polygon_a", "polygon_b"), &Geometry2D::intersect_polygons);
	ClassDB::bind_method(D_METHOD("exclude_polygons", "polygon_a", "polygon_b"), &Geometry2D::exclude_polygons);

	ClassDB::bind_method(D_METHOD("clip_polyline_with_polygon", "polyline", "polygon"), &Geometry2D::clip_polyline_with_polygon);
	ClassDB::bind_method(D_METHOD("intersect_polyline_with_polygon", "polyline", "polygon"), &Geometry2D::intersect_polyline_with_polygon);

	ClassDB::bind_method(D_METHOD("offset_polygon", "polygon", "delta", "join_type"), &Geometry2D::offset_polygon, DEFVAL(JOIN_SQUARE));
	ClassDB::bind_method(D_METHOD("offset_polyline", "polyline", "delta", "join_type", "end_type"), &Geometry2D::offset_polyline, DEFVAL(JOIN_SQUARE), DEFVAL(END_SQUARE));

	ClassDB::bind_method(D_METHOD("make_atlas", "sizes"), &Geometry2D::make_atlas);

	ClassDB::bind_method(D_METHOD("bresenham_line", "from", "to"), &Geometry2D::bresenham_line);

	BIND_ENUM_CONSTANT(OPERATION_UNION);
	BIND_ENUM_CONSTANT(OPERATION_DIFFERENCE);
	BIND_ENUM_CONSTANT(OPERATION_INTERSECTION);
	BIND_ENUM_CONSTANT(OPERATION_XOR);

	BIND_ENUM_CONSTANT(JOIN_SQUARE);
	BIND_ENUM_CONSTANT(JOIN_ROUND);
	BIND_ENUM_CONSTANT(JOIN_MITER);

	BIND_ENUM_CONSTANT(END_POLYGON);
	BIND_ENUM_CONSTANT(END_JOINED);
	BIND_ENUM_CONSTANT(END_BUTT);
	BIND_ENUM_CONSTANT(END_SQUARE);
	BIND_ENUM_CONSTANT(END_ROUND);
}

////// Geometry3D //////

Geometry3D *Geometry3D::singleton = nullptr;

Geometry3D *Geometry3D::get_singleton() {
	return singleton;
}

Vector<Vector3> Geometry3D::compute_convex_mesh_points(const TypedArray<Plane> &p_planes) {
	Vector<Plane> planes_vec;
	int size = p_planes.size();
	planes_vec.resize(size);
	for (int i = 0; i < size; ++i) {
		planes_vec.set(i, p_planes[i]);
	}
	Variant ret = ::Geometry3D::compute_convex_mesh_points(planes_vec.ptr(), size);
	return ret;
}

TypedArray<Plane> Geometry3D::build_box_planes(const Vector3 &p_extents) {
	Variant ret = ::Geometry3D::build_box_planes(p_extents);
	return ret;
}

TypedArray<Plane> Geometry3D::build_cylinder_planes(float p_radius, float p_height, int p_sides, Vector3::Axis p_axis) {
	Variant ret = ::Geometry3D::build_cylinder_planes(p_radius, p_height, p_sides, p_axis);
	return ret;
}

TypedArray<Plane> Geometry3D::build_capsule_planes(float p_radius, float p_height, int p_sides, int p_lats, Vector3::Axis p_axis) {
	Variant ret = ::Geometry3D::build_capsule_planes(p_radius, p_height, p_sides, p_lats, p_axis);
	return ret;
}

Vector<Vector3> Geometry3D::get_closest_points_between_segments(const Vector3 &p1, const Vector3 &p2, const Vector3 &q1, const Vector3 &q2) {
	Vector3 r1, r2;
	::Geometry3D::get_closest_points_between_segments(p1, p2, q1, q2, r1, r2);
	Vector<Vector3> r = { r1, r2 };
	return r;
}

Vector3 Geometry3D::get_closest_point_to_segment(const Vector3 &p_point, const Vector3 &p_a, const Vector3 &p_b) {
	Vector3 s[2] = { p_a, p_b };
	return ::Geometry3D::get_closest_point_to_segment(p_point, s);
}

Vector3 Geometry3D::get_closest_point_to_segment_uncapped(const Vector3 &p_point, const Vector3 &p_a, const Vector3 &p_b) {
	Vector3 s[2] = { p_a, p_b };
	return ::Geometry3D::get_closest_point_to_segment_uncapped(p_point, s);
}

Vector3 Geometry3D::get_triangle_barycentric_coords(const Vector3 &p_point, const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2) {
	Vector3 res = ::Geometry3D::triangle_get_barycentric_coords(p_v0, p_v1, p_v2, p_point);
	return res;
}

Variant Geometry3D::ray_intersects_triangle(const Vector3 &p_from, const Vector3 &p_dir, const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2) {
	Vector3 res;
	if (::Geometry3D::ray_intersects_triangle(p_from, p_dir, p_v0, p_v1, p_v2, &res)) {
		return res;
	} else {
		return Variant();
	}
}

Variant Geometry3D::segment_intersects_triangle(const Vector3 &p_from, const Vector3 &p_to, const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2) {
	Vector3 res;
	if (::Geometry3D::segment_intersects_triangle(p_from, p_to, p_v0, p_v1, p_v2, &res)) {
		return res;
	} else {
		return Variant();
	}
}

Vector<Vector3> Geometry3D::segment_intersects_sphere(const Vector3 &p_from, const Vector3 &p_to, const Vector3 &p_sphere_pos, real_t p_sphere_radius) {
	Vector<Vector3> r;
	Vector3 res, norm;
	if (!::Geometry3D::segment_intersects_sphere(p_from, p_to, p_sphere_pos, p_sphere_radius, &res, &norm)) {
		return r;
	}

	r.resize(2);
	r.set(0, res);
	r.set(1, norm);
	return r;
}

Vector<Vector3> Geometry3D::segment_intersects_cylinder(const Vector3 &p_from, const Vector3 &p_to, float p_height, float p_radius) {
	Vector<Vector3> r;
	Vector3 res, norm;
	if (!::Geometry3D::segment_intersects_cylinder(p_from, p_to, p_height, p_radius, &res, &norm)) {
		return r;
	}

	r.resize(2);
	r.set(0, res);
	r.set(1, norm);
	return r;
}

Vector<Vector3> Geometry3D::segment_intersects_convex(const Vector3 &p_from, const Vector3 &p_to, const TypedArray<Plane> &p_planes) {
	Vector<Vector3> r;
	Vector3 res, norm;
	Vector<Plane> planes = Variant(p_planes);
	if (!::Geometry3D::segment_intersects_convex(p_from, p_to, planes.ptr(), planes.size(), &res, &norm)) {
		return r;
	}

	r.resize(2);
	r.set(0, res);
	r.set(1, norm);
	return r;
}

Vector<Vector3> Geometry3D::clip_polygon(const Vector<Vector3> &p_points, const Plane &p_plane) {
	return ::Geometry3D::clip_polygon(p_points, p_plane);
}

Vector<int32_t> Geometry3D::tetrahedralize_delaunay(const Vector<Vector3> &p_points) {
	return ::Geometry3D::tetrahedralize_delaunay(p_points);
}

void Geometry3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("compute_convex_mesh_points", "planes"), &Geometry3D::compute_convex_mesh_points);
	ClassDB::bind_method(D_METHOD("build_box_planes", "extents"), &Geometry3D::build_box_planes);
	ClassDB::bind_method(D_METHOD("build_cylinder_planes", "radius", "height", "sides", "axis"), &Geometry3D::build_cylinder_planes, DEFVAL(Vector3::AXIS_Z));
	ClassDB::bind_method(D_METHOD("build_capsule_planes", "radius", "height", "sides", "lats", "axis"), &Geometry3D::build_capsule_planes, DEFVAL(Vector3::AXIS_Z));

	ClassDB::bind_method(D_METHOD("get_closest_points_between_segments", "p1", "p2", "q1", "q2"), &Geometry3D::get_closest_points_between_segments);

	ClassDB::bind_method(D_METHOD("get_closest_point_to_segment", "point", "s1", "s2"), &Geometry3D::get_closest_point_to_segment);

	ClassDB::bind_method(D_METHOD("get_closest_point_to_segment_uncapped", "point", "s1", "s2"), &Geometry3D::get_closest_point_to_segment_uncapped);

	ClassDB::bind_method(D_METHOD("get_triangle_barycentric_coords", "point", "a", "b", "c"), &Geometry3D::get_triangle_barycentric_coords);

	ClassDB::bind_method(D_METHOD("ray_intersects_triangle", "from", "dir", "a", "b", "c"), &Geometry3D::ray_intersects_triangle);
	ClassDB::bind_method(D_METHOD("segment_intersects_triangle", "from", "to", "a", "b", "c"), &Geometry3D::segment_intersects_triangle);
	ClassDB::bind_method(D_METHOD("segment_intersects_sphere", "from", "to", "sphere_position", "sphere_radius"), &Geometry3D::segment_intersects_sphere);
	ClassDB::bind_method(D_METHOD("segment_intersects_cylinder", "from", "to", "height", "radius"), &Geometry3D::segment_intersects_cylinder);
	ClassDB::bind_method(D_METHOD("segment_intersects_convex", "from", "to", "planes"), &Geometry3D::segment_intersects_convex);

	ClassDB::bind_method(D_METHOD("clip_polygon", "points", "plane"), &Geometry3D::clip_polygon);
	ClassDB::bind_method(D_METHOD("tetrahedralize_delaunay", "points"), &Geometry3D::tetrahedralize_delaunay);
}

////// Marshalls //////

Marshalls *Marshalls::singleton = nullptr;

Marshalls *Marshalls::get_singleton() {
	return singleton;
}

String Marshalls::variant_to_base64(const Variant &p_var, bool p_full_objects) {
	int len;
	Error err = encode_variant(p_var, nullptr, len, p_full_objects);
	ERR_FAIL_COND_V_MSG(err != OK, "", "Error when trying to encode Variant.");

	Vector<uint8_t> buff;
	buff.resize(len);
	uint8_t *w = buff.ptrw();

	err = encode_variant(p_var, &w[0], len, p_full_objects);
	ERR_FAIL_COND_V_MSG(err != OK, "", "Error when trying to encode Variant.");

	String ret = CryptoCore::b64_encode_str(&w[0], len);
	ERR_FAIL_COND_V(ret.is_empty(), ret);

	return ret;
}

Variant Marshalls::base64_to_variant(const String &p_str, bool p_allow_objects) {
	int strlen = p_str.length();
	CharString cstr = p_str.ascii();

	Vector<uint8_t> buf;
	buf.resize(strlen / 4 * 3 + 1);
	uint8_t *w = buf.ptrw();

	size_t len = 0;
	ERR_FAIL_COND_V(CryptoCore::b64_decode(&w[0], buf.size(), &len, (unsigned char *)cstr.get_data(), strlen) != OK, Variant());

	Variant v;
	Error err = decode_variant(v, &w[0], len, nullptr, p_allow_objects);
	ERR_FAIL_COND_V_MSG(err != OK, Variant(), "Error when trying to decode Variant.");

	return v;
}

String Marshalls::raw_to_base64(const Vector<uint8_t> &p_arr) {
	String ret = CryptoCore::b64_encode_str(p_arr.ptr(), p_arr.size());
	ERR_FAIL_COND_V(ret.is_empty(), ret);
	return ret;
}

Vector<uint8_t> Marshalls::base64_to_raw(const String &p_str) {
	int strlen = p_str.length();
	CharString cstr = p_str.ascii();

	size_t arr_len = 0;
	Vector<uint8_t> buf;
	{
		buf.resize(strlen / 4 * 3 + 1);
		uint8_t *w = buf.ptrw();

		ERR_FAIL_COND_V(CryptoCore::b64_decode(&w[0], buf.size(), &arr_len, (unsigned char *)cstr.get_data(), strlen) != OK, Vector<uint8_t>());
	}
	buf.resize(arr_len);

	return buf;
}

String Marshalls::utf8_to_base64(const String &p_str) {
	CharString cstr = p_str.utf8();
	String ret = CryptoCore::b64_encode_str((unsigned char *)cstr.get_data(), cstr.length());
	ERR_FAIL_COND_V(ret.is_empty(), ret);
	return ret;
}

String Marshalls::base64_to_utf8(const String &p_str) {
	int strlen = p_str.length();
	CharString cstr = p_str.ascii();

	Vector<uint8_t> buf;
	buf.resize(strlen / 4 * 3 + 1 + 1);
	uint8_t *w = buf.ptrw();

	size_t len = 0;
	ERR_FAIL_COND_V(CryptoCore::b64_decode(&w[0], buf.size(), &len, (unsigned char *)cstr.get_data(), strlen) != OK, String());

	w[len] = 0;
	String ret = String::utf8((char *)&w[0]);

	return ret;
}

void Marshalls::_bind_methods() {
	ClassDB::bind_method(D_METHOD("variant_to_base64", "variant", "full_objects"), &Marshalls::variant_to_base64, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("base64_to_variant", "base64_str", "allow_objects"), &Marshalls::base64_to_variant, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("raw_to_base64", "array"), &Marshalls::raw_to_base64);
	ClassDB::bind_method(D_METHOD("base64_to_raw", "base64_str"), &Marshalls::base64_to_raw);

	ClassDB::bind_method(D_METHOD("utf8_to_base64", "utf8_str"), &Marshalls::utf8_to_base64);
	ClassDB::bind_method(D_METHOD("base64_to_utf8", "base64_str"), &Marshalls::base64_to_utf8);
}

////// Semaphore //////

void Semaphore::wait() {
	semaphore.wait();
}

bool Semaphore::try_wait() {
	return semaphore.try_wait();
}

void Semaphore::post(int p_count) {
	ERR_FAIL_COND(p_count <= 0);
	semaphore.post(p_count);
}

void Semaphore::_bind_methods() {
	ClassDB::bind_method(D_METHOD("wait"), &Semaphore::wait);
	ClassDB::bind_method(D_METHOD("try_wait"), &Semaphore::try_wait);
	ClassDB::bind_method(D_METHOD("post", "count"), &Semaphore::post, DEFVAL(1));
}

////// Mutex //////

void Mutex::lock() {
	mutex.lock();
}

bool Mutex::try_lock() {
	return mutex.try_lock();
}

void Mutex::unlock() {
	mutex.unlock();
}

void Mutex::_bind_methods() {
	ClassDB::bind_method(D_METHOD("lock"), &Mutex::lock);
	ClassDB::bind_method(D_METHOD("try_lock"), &Mutex::try_lock);
	ClassDB::bind_method(D_METHOD("unlock"), &Mutex::unlock);
}

////// Thread //////

void Thread::_start_func(void *ud) {
	Ref<Thread> *tud = (Ref<Thread> *)ud;
	Ref<Thread> t = *tud;
	memdelete(tud);

	if (!t->target_callable.is_valid()) {
		t->running.clear();
		ERR_FAIL_MSG(vformat("Could not call function '%s' on previously freed instance to start thread %s.", t->target_callable.get_method(), t->get_id()));
	}

	// Finding out a suitable name for the thread can involve querying a node, if the target is one.
	// We know this is safe (unless the user is causing life cycle race conditions, which would be a bug on their part).
	set_current_thread_safe_for_nodes(true);
	String func_name = t->target_callable.is_custom() ? t->target_callable.get_custom()->get_as_text() : String(t->target_callable.get_method());
	set_current_thread_safe_for_nodes(false);
	::Thread::set_name(func_name);

	// To avoid a circular reference between the thread and the script which can possibly contain a reference
	// to the thread, we will do the call (keeping a reference up to that point) and then break chains with it.
	// When the call returns, we will reference the thread again if possible.
	ObjectID th_instance_id = t->get_instance_id();
	Callable target_callable = t->target_callable;
	t = Ref<Thread>();

	Callable::CallError ce;
	Variant ret;
	target_callable.callp(nullptr, 0, ret, ce);
	// If script properly kept a reference to the thread, we should be able to re-reference it now
	// (well, or if the call failed, since we had to break chains anyway because the outcome isn't known upfront).
	t = Ref<Thread>(ObjectDB::get_instance(th_instance_id));
	if (t.is_valid()) {
		t->ret = ret;
		t->running.clear();
	} else {
		// We could print a warning here, but the Thread object will be eventually destroyed
		// noticing wait_to_finish() hasn't been called on it, and it will print a warning itself.
	}

	if (ce.error != Callable::CallError::CALL_OK) {
		ERR_FAIL_MSG("Could not call function '" + func_name + "' to start thread " + t->get_id() + ": " + Variant::get_callable_error_text(t->target_callable, nullptr, 0, ce) + ".");
	}
}

Error Thread::start(const Callable &p_callable, Priority p_priority) {
	ERR_FAIL_COND_V_MSG(is_started(), ERR_ALREADY_IN_USE, "Thread already started.");
	ERR_FAIL_COND_V(!p_callable.is_valid(), ERR_INVALID_PARAMETER);
	ERR_FAIL_INDEX_V(p_priority, PRIORITY_MAX, ERR_INVALID_PARAMETER);

	ret = Variant();
	target_callable = p_callable;
	running.set();

	Ref<Thread> *ud = memnew(Ref<Thread>(this));

	::Thread::Settings s;
	s.priority = (::Thread::Priority)p_priority;
	thread.start(_start_func, ud, s);

	return OK;
}

String Thread::get_id() const {
	return itos(thread.get_id());
}

bool Thread::is_started() const {
	return thread.is_started();
}

bool Thread::is_alive() const {
	return running.is_set();
}

Variant Thread::wait_to_finish() {
	ERR_FAIL_COND_V_MSG(!is_started(), Variant(), "Thread must have been started to wait for its completion.");
	thread.wait_to_finish();
	Variant r = ret;
	target_callable = Callable();

	return r;
}

void Thread::set_thread_safety_checks_enabled(bool p_enabled) {
	ERR_FAIL_COND_MSG(::Thread::is_main_thread(), "This call is forbidden on the main thread.");
	set_current_thread_safe_for_nodes(!p_enabled);
}

void Thread::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start", "callable", "priority"), &Thread::start, DEFVAL(PRIORITY_NORMAL));
	ClassDB::bind_method(D_METHOD("get_id"), &Thread::get_id);
	ClassDB::bind_method(D_METHOD("is_started"), &Thread::is_started);
	ClassDB::bind_method(D_METHOD("is_alive"), &Thread::is_alive);
	ClassDB::bind_method(D_METHOD("wait_to_finish"), &Thread::wait_to_finish);

	ClassDB::bind_static_method("Thread", D_METHOD("set_thread_safety_checks_enabled", "enabled"), &Thread::set_thread_safety_checks_enabled);

	BIND_ENUM_CONSTANT(PRIORITY_LOW);
	BIND_ENUM_CONSTANT(PRIORITY_NORMAL);
	BIND_ENUM_CONSTANT(PRIORITY_HIGH);
}

namespace special {

////// ClassDB //////

PackedStringArray ClassDB::get_class_list() const {
	List<StringName> classes;
	::ClassDB::get_class_list(&classes);

	PackedStringArray ret;
	ret.resize(classes.size());
	int idx = 0;
	for (const StringName &E : classes) {
		ret.set(idx++, E);
	}

	return ret;
}

PackedStringArray ClassDB::get_inheriters_from_class(const StringName &p_class) const {
	List<StringName> classes;
	::ClassDB::get_inheriters_from_class(p_class, &classes);

	PackedStringArray ret;
	ret.resize(classes.size());
	int idx = 0;
	for (const StringName &E : classes) {
		ret.set(idx++, E);
	}

	return ret;
}

StringName ClassDB::get_parent_class(const StringName &p_class) const {
	return ::ClassDB::get_parent_class(p_class);
}

bool ClassDB::class_exists(const StringName &p_class) const {
	return ::ClassDB::class_exists(p_class);
}

bool ClassDB::is_parent_class(const StringName &p_class, const StringName &p_inherits) const {
	return ::ClassDB::is_parent_class(p_class, p_inherits);
}

bool ClassDB::can_instantiate(const StringName &p_class) const {
	return ::ClassDB::can_instantiate(p_class);
}

Variant ClassDB::instantiate(const StringName &p_class) const {
	Object *obj = ::ClassDB::instantiate(p_class);
	if (!obj) {
		return Variant();
	}

	RefCounted *r = Object::cast_to<RefCounted>(obj);
	if (r) {
		return Ref<RefCounted>(r);
	} else {
		return obj;
	}
}

ClassDB::APIType ClassDB::class_get_api_type(const StringName &p_class) const {
	::ClassDB::APIType api_type = ::ClassDB::get_api_type(p_class);
	return (APIType)api_type;
}

bool ClassDB::class_has_signal(const StringName &p_class, const StringName &p_signal) const {
	return ::ClassDB::has_signal(p_class, p_signal);
}

Dictionary ClassDB::class_get_signal(const StringName &p_class, const StringName &p_signal) const {
	MethodInfo signal;
	if (::ClassDB::get_signal(p_class, p_signal, &signal)) {
		return signal.operator Dictionary();
	} else {
		return Dictionary();
	}
}

TypedArray<Dictionary> ClassDB::class_get_signal_list(const StringName &p_class, bool p_no_inheritance) const {
	List<MethodInfo> signals;
	::ClassDB::get_signal_list(p_class, &signals, p_no_inheritance);
	TypedArray<Dictionary> ret;

	for (const MethodInfo &E : signals) {
		ret.push_back(E.operator Dictionary());
	}

	return ret;
}

TypedArray<Dictionary> ClassDB::class_get_property_list(const StringName &p_class, bool p_no_inheritance) const {
	List<PropertyInfo> plist;
	::ClassDB::get_property_list(p_class, &plist, p_no_inheritance);
	TypedArray<Dictionary> ret;
	for (const PropertyInfo &E : plist) {
		ret.push_back(E.operator Dictionary());
	}

	return ret;
}

StringName ClassDB::class_get_property_getter(const StringName &p_class, const StringName &p_property) {
	return ::ClassDB::get_property_getter(p_class, p_property);
}

StringName ClassDB::class_get_property_setter(const StringName &p_class, const StringName &p_property) {
	return ::ClassDB::get_property_setter(p_class, p_property);
}

Variant ClassDB::class_get_property(Object *p_object, const StringName &p_property) const {
	Variant ret;
	::ClassDB::get_property(p_object, p_property, ret);
	return ret;
}

Error ClassDB::class_set_property(Object *p_object, const StringName &p_property, const Variant &p_value) const {
	Variant ret;
	bool valid;
	if (!::ClassDB::set_property(p_object, p_property, p_value, &valid)) {
		return ERR_UNAVAILABLE;
	} else if (!valid) {
		return ERR_INVALID_DATA;
	}
	return OK;
}

Variant ClassDB::class_get_property_default_value(const StringName &p_class, const StringName &p_property) const {
	bool valid;
	Variant ret = ::ClassDB::class_get_default_property_value(p_class, p_property, &valid);
	if (valid) {
		return ret;
	}
	return Variant();
}

bool ClassDB::class_has_method(const StringName &p_class, const StringName &p_method, bool p_no_inheritance) const {
	return ::ClassDB::has_method(p_class, p_method, p_no_inheritance);
}

int ClassDB::class_get_method_argument_count(const StringName &p_class, const StringName &p_method, bool p_no_inheritance) const {
	return ::ClassDB::get_method_argument_count(p_class, p_method, nullptr, p_no_inheritance);
}

TypedArray<Dictionary> ClassDB::class_get_method_list(const StringName &p_class, bool p_no_inheritance) const {
	List<MethodInfo> methods;
	::ClassDB::get_method_list(p_class, &methods, p_no_inheritance);
	TypedArray<Dictionary> ret;

	for (const MethodInfo &E : methods) {
#ifdef DEBUG_METHODS_ENABLED
		ret.push_back(E.operator Dictionary());
#else
		Dictionary dict;
		dict["name"] = E.name;
		ret.push_back(dict);
#endif
	}

	return ret;
}

Variant ClassDB::class_call_static_method(const Variant **p_arguments, int p_argcount, Callable::CallError &r_call_error) {
	if (p_argcount < 2) {
		r_call_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		return Variant::NIL;
	}
	if (!p_arguments[0]->is_string() || !p_arguments[1]->is_string()) {
		r_call_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		return Variant::NIL;
	}
	StringName class_ = *p_arguments[0];
	StringName method = *p_arguments[1];
	const MethodBind *bind = ::ClassDB::get_method(class_, method);
	ERR_FAIL_NULL_V_MSG(bind, Variant::NIL, "Cannot find static method.");
	ERR_FAIL_COND_V_MSG(!bind->is_static(), Variant::NIL, "Method is not static.");
	return bind->call(nullptr, p_arguments + 2, p_argcount - 2, r_call_error);
}

PackedStringArray ClassDB::class_get_integer_constant_list(const StringName &p_class, bool p_no_inheritance) const {
	List<String> constants;
	::ClassDB::get_integer_constant_list(p_class, &constants, p_no_inheritance);

	PackedStringArray ret;
	ret.resize(constants.size());
	int idx = 0;
	for (const String &E : constants) {
		ret.set(idx++, E);
	}

	return ret;
}

bool ClassDB::class_has_integer_constant(const StringName &p_class, const StringName &p_name) const {
	bool success;
	::ClassDB::get_integer_constant(p_class, p_name, &success);
	return success;
}

int64_t ClassDB::class_get_integer_constant(const StringName &p_class, const StringName &p_name) const {
	bool found;
	int64_t c = ::ClassDB::get_integer_constant(p_class, p_name, &found);
	ERR_FAIL_COND_V(!found, 0);
	return c;
}

bool ClassDB::class_has_enum(const StringName &p_class, const StringName &p_name, bool p_no_inheritance) const {
	return ::ClassDB::has_enum(p_class, p_name, p_no_inheritance);
}

PackedStringArray ClassDB::class_get_enum_list(const StringName &p_class, bool p_no_inheritance) const {
	List<StringName> enums;
	::ClassDB::get_enum_list(p_class, &enums, p_no_inheritance);

	PackedStringArray ret;
	ret.resize(enums.size());
	int idx = 0;
	for (const StringName &E : enums) {
		ret.set(idx++, E);
	}

	return ret;
}

PackedStringArray ClassDB::class_get_enum_constants(const StringName &p_class, const StringName &p_enum, bool p_no_inheritance) const {
	List<StringName> constants;
	::ClassDB::get_enum_constants(p_class, p_enum, &constants, p_no_inheritance);

	PackedStringArray ret;
	ret.resize(constants.size());
	int idx = 0;
	for (const StringName &E : constants) {
		ret.set(idx++, E);
	}

	return ret;
}

StringName ClassDB::class_get_integer_constant_enum(const StringName &p_class, const StringName &p_name, bool p_no_inheritance) const {
	return ::ClassDB::get_integer_constant_enum(p_class, p_name, p_no_inheritance);
}

bool ClassDB::is_class_enum_bitfield(const StringName &p_class, const StringName &p_enum, bool p_no_inheritance) const {
	return ::ClassDB::is_enum_bitfield(p_class, p_enum, p_no_inheritance);
}

bool ClassDB::is_class_enabled(const StringName &p_class) const {
	return ::ClassDB::is_class_enabled(p_class);
}

#ifdef TOOLS_ENABLED
void ClassDB::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {
	const String pf = p_function;
	bool first_argument_is_class = false;
	if (p_idx == 0) {
		first_argument_is_class = (pf == "get_inheriters_from_class" || pf == "get_parent_class" ||
				pf == "class_exists" || pf == "can_instantiate" || pf == "instantiate" ||
				pf == "class_has_signal" || pf == "class_get_signal" || pf == "class_get_signal_list" ||
				pf == "class_get_property_list" || pf == "class_get_property" || pf == "class_set_property" ||
				pf == "class_has_method" || pf == "class_get_method_list" ||
				pf == "class_get_integer_constant_list" || pf == "class_has_integer_constant" || pf == "class_get_integer_constant" ||
				pf == "class_has_enum" || pf == "class_get_enum_list" || pf == "class_get_enum_constants" || pf == "class_get_integer_constant_enum" ||
				pf == "is_class_enabled" || pf == "is_class_enum_bitfield" || pf == "class_get_api_type");
	}
	if (first_argument_is_class || pf == "is_parent_class") {
		for (const String &E : get_class_list()) {
			r_options->push_back(E.quote());
		}
	}

	Object::get_argument_options(p_function, p_idx, r_options);
}
#endif

void ClassDB::_bind_methods() {
	::ClassDB::bind_method(D_METHOD("get_class_list"), &ClassDB::get_class_list);
	::ClassDB::bind_method(D_METHOD("get_inheriters_from_class", "class"), &ClassDB::get_inheriters_from_class);
	::ClassDB::bind_method(D_METHOD("get_parent_class", "class"), &ClassDB::get_parent_class);
	::ClassDB::bind_method(D_METHOD("class_exists", "class"), &ClassDB::class_exists);
	::ClassDB::bind_method(D_METHOD("is_parent_class", "class", "inherits"), &ClassDB::is_parent_class);
	::ClassDB::bind_method(D_METHOD("can_instantiate", "class"), &ClassDB::can_instantiate);
	::ClassDB::bind_method(D_METHOD("instantiate", "class"), &ClassDB::instantiate);

	::ClassDB::bind_method(D_METHOD("class_get_api_type", "class"), &ClassDB::class_get_api_type);

	::ClassDB::bind_method(D_METHOD("class_has_signal", "class", "signal"), &ClassDB::class_has_signal);
	::ClassDB::bind_method(D_METHOD("class_get_signal", "class", "signal"), &ClassDB::class_get_signal);
	::ClassDB::bind_method(D_METHOD("class_get_signal_list", "class", "no_inheritance"), &ClassDB::class_get_signal_list, DEFVAL(false));

	::ClassDB::bind_method(D_METHOD("class_get_property_list", "class", "no_inheritance"), &ClassDB::class_get_property_list, DEFVAL(false));
	::ClassDB::bind_method(D_METHOD("class_get_property_getter", "class", "property"), &ClassDB::class_get_property_getter);
	::ClassDB::bind_method(D_METHOD("class_get_property_setter", "class", "property"), &ClassDB::class_get_property_setter);
	::ClassDB::bind_method(D_METHOD("class_get_property", "object", "property"), &ClassDB::class_get_property);
	::ClassDB::bind_method(D_METHOD("class_set_property", "object", "property", "value"), &ClassDB::class_set_property);

	::ClassDB::bind_method(D_METHOD("class_get_property_default_value", "class", "property"), &ClassDB::class_get_property_default_value);

	::ClassDB::bind_method(D_METHOD("class_has_method", "class", "method", "no_inheritance"), &ClassDB::class_has_method, DEFVAL(false));

	::ClassDB::bind_method(D_METHOD("class_get_method_argument_count", "class", "method", "no_inheritance"), &ClassDB::class_get_method_argument_count, DEFVAL(false));

	::ClassDB::bind_method(D_METHOD("class_get_method_list", "class", "no_inheritance"), &ClassDB::class_get_method_list, DEFVAL(false));

	::ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "class_call_static_method", &ClassDB::class_call_static_method, MethodInfo("class_call_static_method", PropertyInfo(Variant::STRING_NAME, "class"), PropertyInfo(Variant::STRING_NAME, "method")));

	::ClassDB::bind_method(D_METHOD("class_get_integer_constant_list", "class", "no_inheritance"), &ClassDB::class_get_integer_constant_list, DEFVAL(false));

	::ClassDB::bind_method(D_METHOD("class_has_integer_constant", "class", "name"), &ClassDB::class_has_integer_constant);
	::ClassDB::bind_method(D_METHOD("class_get_integer_constant", "class", "name"), &ClassDB::class_get_integer_constant);

	::ClassDB::bind_method(D_METHOD("class_has_enum", "class", "name", "no_inheritance"), &ClassDB::class_has_enum, DEFVAL(false));
	::ClassDB::bind_method(D_METHOD("class_get_enum_list", "class", "no_inheritance"), &ClassDB::class_get_enum_list, DEFVAL(false));
	::ClassDB::bind_method(D_METHOD("class_get_enum_constants", "class", "enum", "no_inheritance"), &ClassDB::class_get_enum_constants, DEFVAL(false));
	::ClassDB::bind_method(D_METHOD("class_get_integer_constant_enum", "class", "name", "no_inheritance"), &ClassDB::class_get_integer_constant_enum, DEFVAL(false));

	::ClassDB::bind_method(D_METHOD("is_class_enum_bitfield", "class", "enum", "no_inheritance"), &ClassDB::is_class_enum_bitfield, DEFVAL(false));

	::ClassDB::bind_method(D_METHOD("is_class_enabled", "class"), &ClassDB::is_class_enabled);

	BIND_ENUM_CONSTANT(API_CORE);
	BIND_ENUM_CONSTANT(API_EDITOR);
	BIND_ENUM_CONSTANT(API_EXTENSION);
	BIND_ENUM_CONSTANT(API_EDITOR_EXTENSION);
	BIND_ENUM_CONSTANT(API_NONE);
}

} // namespace special

////// Engine //////

void Engine::set_physics_ticks_per_second(int p_ips) {
	::Engine::get_singleton()->set_physics_ticks_per_second(p_ips);
}

int Engine::get_physics_ticks_per_second() const {
	return ::Engine::get_singleton()->get_physics_ticks_per_second();
}

void Engine::set_max_physics_steps_per_frame(int p_max_physics_steps) {
	::Engine::get_singleton()->set_max_physics_steps_per_frame(p_max_physics_steps);
}

int Engine::get_max_physics_steps_per_frame() const {
	return ::Engine::get_singleton()->get_max_physics_steps_per_frame();
}

void Engine::set_physics_jitter_fix(double p_threshold) {
	::Engine::get_singleton()->set_physics_jitter_fix(p_threshold);
}

double Engine::get_physics_jitter_fix() const {
	return ::Engine::get_singleton()->get_physics_jitter_fix();
}

double Engine::get_physics_interpolation_fraction() const {
	return ::Engine::get_singleton()->get_physics_interpolation_fraction();
}

void Engine::set_max_fps(int p_fps) {
	::Engine::get_singleton()->set_max_fps(p_fps);
}

int Engine::get_max_fps() const {
	return ::Engine::get_singleton()->get_max_fps();
}

double Engine::get_frames_per_second() const {
	return ::Engine::get_singleton()->get_frames_per_second();
}

uint64_t Engine::get_physics_frames() const {
	return ::Engine::get_singleton()->get_physics_frames();
}

uint64_t Engine::get_process_frames() const {
	return ::Engine::get_singleton()->get_process_frames();
}

void Engine::set_time_scale(double p_scale) {
	::Engine::get_singleton()->set_time_scale(p_scale);
}

double Engine::get_time_scale() {
	return ::Engine::get_singleton()->get_time_scale();
}

int Engine::get_frames_drawn() {
	return ::Engine::get_singleton()->get_frames_drawn();
}

MainLoop *Engine::get_main_loop() const {
	// Needs to remain in OS, since it's actually OS that interacts with it, but it's better exposed here
	return ::OS::get_singleton()->get_main_loop();
}

Dictionary Engine::get_version_info() const {
	return ::Engine::get_singleton()->get_version_info();
}

Dictionary Engine::get_author_info() const {
	return ::Engine::get_singleton()->get_author_info();
}

TypedArray<Dictionary> Engine::get_copyright_info() const {
	return ::Engine::get_singleton()->get_copyright_info();
}

Dictionary Engine::get_donor_info() const {
	return ::Engine::get_singleton()->get_donor_info();
}

Dictionary Engine::get_license_info() const {
	return ::Engine::get_singleton()->get_license_info();
}

String Engine::get_license_text() const {
	return ::Engine::get_singleton()->get_license_text();
}

String Engine::get_architecture_name() const {
	return ::Engine::get_singleton()->get_architecture_name();
}

bool Engine::is_in_physics_frame() const {
	return ::Engine::get_singleton()->is_in_physics_frame();
}

bool Engine::has_singleton(const StringName &p_name) const {
	return ::Engine::get_singleton()->has_singleton(p_name);
}

Object *Engine::get_singleton_object(const StringName &p_name) const {
	return ::Engine::get_singleton()->get_singleton_object(p_name);
}

void Engine::register_singleton(const StringName &p_name, Object *p_object) {
	ERR_FAIL_COND_MSG(has_singleton(p_name), "Singleton already registered: " + String(p_name));
	ERR_FAIL_COND_MSG(!String(p_name).is_valid_ascii_identifier(), "Singleton name is not a valid identifier: " + p_name);
	::Engine::Singleton s;
	s.class_name = p_name;
	s.name = p_name;
	s.ptr = p_object;
	s.user_created = true;
	::Engine::get_singleton()->add_singleton(s);
}

void Engine::unregister_singleton(const StringName &p_name) {
	ERR_FAIL_COND_MSG(!has_singleton(p_name), "Attempt to remove unregistered singleton: " + String(p_name));
	ERR_FAIL_COND_MSG(!::Engine::get_singleton()->is_singleton_user_created(p_name), "Attempt to remove non-user created singleton: " + String(p_name));
	::Engine::get_singleton()->remove_singleton(p_name);
}

Vector<String> Engine::get_singleton_list() const {
	List<::Engine::Singleton> singletons;
	::Engine::get_singleton()->get_singletons(&singletons);
	Vector<String> ret;
	for (List<::Engine::Singleton>::Element *E = singletons.front(); E; E = E->next()) {
		ret.push_back(E->get().name);
	}
	return ret;
}

Error Engine::register_script_language(ScriptLanguage *p_language) {
	return ScriptServer::register_language(p_language);
}

Error Engine::unregister_script_language(const ScriptLanguage *p_language) {
	return ScriptServer::unregister_language(p_language);
}

int Engine::get_script_language_count() {
	return ScriptServer::get_language_count();
}

ScriptLanguage *Engine::get_script_language(int p_index) const {
	return ScriptServer::get_language(p_index);
}

void Engine::set_editor_hint(bool p_enabled) {
	::Engine::get_singleton()->set_editor_hint(p_enabled);
}

bool Engine::is_editor_hint() const {
	return ::Engine::get_singleton()->is_editor_hint();
}

String Engine::get_write_movie_path() const {
	return ::Engine::get_singleton()->get_write_movie_path();
}

void Engine::set_print_to_stdout(bool p_enabled) {
	::Engine::get_singleton()->set_print_to_stdout(p_enabled);
}

bool Engine::is_printing_to_stdout() const {
	return ::Engine::get_singleton()->is_printing_to_stdout();
}

void Engine::set_print_error_messages(bool p_enabled) {
	::Engine::get_singleton()->set_print_error_messages(p_enabled);
}

bool Engine::is_printing_error_messages() const {
	return ::Engine::get_singleton()->is_printing_error_messages();
}

#ifdef TOOLS_ENABLED
void Engine::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {
	const String pf = p_function;
	if (p_idx == 0 && (pf == "has_singleton" || pf == "get_singleton" || pf == "unregister_singleton")) {
		for (const String &E : get_singleton_list()) {
			r_options->push_back(E.quote());
		}
	}
	Object::get_argument_options(p_function, p_idx, r_options);
}
#endif

void Engine::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_physics_ticks_per_second", "physics_ticks_per_second"), &Engine::set_physics_ticks_per_second);
	ClassDB::bind_method(D_METHOD("get_physics_ticks_per_second"), &Engine::get_physics_ticks_per_second);
	ClassDB::bind_method(D_METHOD("set_max_physics_steps_per_frame", "max_physics_steps"), &Engine::set_max_physics_steps_per_frame);
	ClassDB::bind_method(D_METHOD("get_max_physics_steps_per_frame"), &Engine::get_max_physics_steps_per_frame);
	ClassDB::bind_method(D_METHOD("set_physics_jitter_fix", "physics_jitter_fix"), &Engine::set_physics_jitter_fix);
	ClassDB::bind_method(D_METHOD("get_physics_jitter_fix"), &Engine::get_physics_jitter_fix);
	ClassDB::bind_method(D_METHOD("get_physics_interpolation_fraction"), &Engine::get_physics_interpolation_fraction);
	ClassDB::bind_method(D_METHOD("set_max_fps", "max_fps"), &Engine::set_max_fps);
	ClassDB::bind_method(D_METHOD("get_max_fps"), &Engine::get_max_fps);

	ClassDB::bind_method(D_METHOD("set_time_scale", "time_scale"), &Engine::set_time_scale);
	ClassDB::bind_method(D_METHOD("get_time_scale"), &Engine::get_time_scale);

	ClassDB::bind_method(D_METHOD("get_frames_drawn"), &Engine::get_frames_drawn);
	ClassDB::bind_method(D_METHOD("get_frames_per_second"), &Engine::get_frames_per_second);
	ClassDB::bind_method(D_METHOD("get_physics_frames"), &Engine::get_physics_frames);
	ClassDB::bind_method(D_METHOD("get_process_frames"), &Engine::get_process_frames);

	ClassDB::bind_method(D_METHOD("get_main_loop"), &Engine::get_main_loop);

	ClassDB::bind_method(D_METHOD("get_version_info"), &Engine::get_version_info);
	ClassDB::bind_method(D_METHOD("get_author_info"), &Engine::get_author_info);
	ClassDB::bind_method(D_METHOD("get_copyright_info"), &Engine::get_copyright_info);
	ClassDB::bind_method(D_METHOD("get_donor_info"), &Engine::get_donor_info);
	ClassDB::bind_method(D_METHOD("get_license_info"), &Engine::get_license_info);
	ClassDB::bind_method(D_METHOD("get_license_text"), &Engine::get_license_text);

	ClassDB::bind_method(D_METHOD("get_architecture_name"), &Engine::get_architecture_name);

	ClassDB::bind_method(D_METHOD("is_in_physics_frame"), &Engine::is_in_physics_frame);

	ClassDB::bind_method(D_METHOD("has_singleton", "name"), &Engine::has_singleton);
	ClassDB::bind_method(D_METHOD("get_singleton", "name"), &Engine::get_singleton_object);

	ClassDB::bind_method(D_METHOD("register_singleton", "name", "instance"), &Engine::register_singleton);
	ClassDB::bind_method(D_METHOD("unregister_singleton", "name"), &Engine::unregister_singleton);
	ClassDB::bind_method(D_METHOD("get_singleton_list"), &Engine::get_singleton_list);

	ClassDB::bind_method(D_METHOD("register_script_language", "language"), &Engine::register_script_language);
	ClassDB::bind_method(D_METHOD("unregister_script_language", "language"), &Engine::unregister_script_language);
	ClassDB::bind_method(D_METHOD("get_script_language_count"), &Engine::get_script_language_count);
	ClassDB::bind_method(D_METHOD("get_script_language", "index"), &Engine::get_script_language);

	ClassDB::bind_method(D_METHOD("is_editor_hint"), &Engine::is_editor_hint);

	ClassDB::bind_method(D_METHOD("get_write_movie_path"), &Engine::get_write_movie_path);

	ClassDB::bind_method(D_METHOD("set_print_to_stdout", "enabled"), &Engine::set_print_to_stdout);
	ClassDB::bind_method(D_METHOD("is_printing_to_stdout"), &Engine::is_printing_to_stdout);

	ClassDB::bind_method(D_METHOD("set_print_error_messages", "enabled"), &Engine::set_print_error_messages);
	ClassDB::bind_method(D_METHOD("is_printing_error_messages"), &Engine::is_printing_error_messages);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "print_error_messages"), "set_print_error_messages", "is_printing_error_messages");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "print_to_stdout"), "set_print_to_stdout", "is_printing_to_stdout");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "physics_ticks_per_second"), "set_physics_ticks_per_second", "get_physics_ticks_per_second");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_physics_steps_per_frame"), "set_max_physics_steps_per_frame", "get_max_physics_steps_per_frame");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_fps"), "set_max_fps", "get_max_fps");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "time_scale"), "set_time_scale", "get_time_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "physics_jitter_fix"), "set_physics_jitter_fix", "get_physics_jitter_fix");
}

Engine *Engine::singleton = nullptr;

////// EngineDebugger //////

bool EngineDebugger::is_active() {
	return ::EngineDebugger::is_active();
}

void EngineDebugger::register_profiler(const StringName &p_name, Ref<EngineProfiler> p_profiler) {
	ERR_FAIL_COND(p_profiler.is_null());
	ERR_FAIL_COND_MSG(p_profiler->is_bound(), "Profiler already registered.");
	ERR_FAIL_COND_MSG(profilers.has(p_name) || has_profiler(p_name), "Profiler name already in use: " + p_name);
	Error err = p_profiler->bind(p_name);
	ERR_FAIL_COND_MSG(err != OK, "Profiler failed to register with error: " + itos(err));
	profilers.insert(p_name, p_profiler);
}

void EngineDebugger::unregister_profiler(const StringName &p_name) {
	ERR_FAIL_COND_MSG(!profilers.has(p_name), "Profiler not registered: " + p_name);
	profilers[p_name]->unbind();
	profilers.erase(p_name);
}

bool EngineDebugger::is_profiling(const StringName &p_name) {
	return ::EngineDebugger::is_profiling(p_name);
}

bool EngineDebugger::has_profiler(const StringName &p_name) {
	return ::EngineDebugger::has_profiler(p_name);
}

void EngineDebugger::profiler_add_frame_data(const StringName &p_name, const Array &p_data) {
	::EngineDebugger::profiler_add_frame_data(p_name, p_data);
}

void EngineDebugger::profiler_enable(const StringName &p_name, bool p_enabled, const Array &p_opts) {
	if (::EngineDebugger::get_singleton()) {
		::EngineDebugger::get_singleton()->profiler_enable(p_name, p_enabled, p_opts);
	}
}

void EngineDebugger::register_message_capture(const StringName &p_name, const Callable &p_callable) {
	ERR_FAIL_COND_MSG(captures.has(p_name) || has_capture(p_name), "Capture already registered: " + p_name);
	captures.insert(p_name, p_callable);
	Callable &c = captures[p_name];
	::EngineDebugger::Capture capture(&c, &EngineDebugger::call_capture);
	::EngineDebugger::register_message_capture(p_name, capture);
}

void EngineDebugger::unregister_message_capture(const StringName &p_name) {
	ERR_FAIL_COND_MSG(!captures.has(p_name), "Capture not registered: " + p_name);
	::EngineDebugger::unregister_message_capture(p_name);
	captures.erase(p_name);
}

bool EngineDebugger::has_capture(const StringName &p_name) {
	return ::EngineDebugger::has_capture(p_name);
}

void EngineDebugger::send_message(const String &p_msg, const Array &p_data) {
	ERR_FAIL_COND_MSG(!::EngineDebugger::is_active(), "Can't send message. No active debugger");
	::EngineDebugger::get_singleton()->send_message(p_msg, p_data);
}

void EngineDebugger::debug(bool p_can_continue, bool p_is_error_breakpoint) {
	ERR_FAIL_COND_MSG(!::EngineDebugger::is_active(), "Can't send debug. No active debugger");
	::EngineDebugger::get_singleton()->debug(p_can_continue, p_is_error_breakpoint);
}

void EngineDebugger::script_debug(ScriptLanguage *p_lang, bool p_can_continue, bool p_is_error_breakpoint) {
	ERR_FAIL_COND_MSG(!::EngineDebugger::get_script_debugger(), "Can't send debug. No active debugger");
	::EngineDebugger::get_script_debugger()->debug(p_lang, p_can_continue, p_is_error_breakpoint);
}

Error EngineDebugger::call_capture(void *p_user, const String &p_cmd, const Array &p_data, bool &r_captured) {
	Callable &capture = *(Callable *)p_user;
	if (!capture.is_valid()) {
		return FAILED;
	}
	Variant cmd = p_cmd, data = p_data;
	const Variant *args[2] = { &cmd, &data };
	Variant retval;
	Callable::CallError err;
	capture.callp(args, 2, retval, err);
	ERR_FAIL_COND_V_MSG(err.error != Callable::CallError::CALL_OK, FAILED, "Error calling 'capture' to callable: " + Variant::get_callable_error_text(capture, args, 2, err));
	ERR_FAIL_COND_V_MSG(retval.get_type() != Variant::BOOL, FAILED, "Error calling 'capture' to callable: " + String(capture) + ". Return type is not bool.");
	r_captured = retval;
	return OK;
}

void EngineDebugger::line_poll() {
	ERR_FAIL_COND_MSG(!::EngineDebugger::is_active(), "Can't poll. No active debugger");
	::EngineDebugger::get_singleton()->line_poll();
}

void EngineDebugger::set_lines_left(int p_lines) {
	ERR_FAIL_COND_MSG(!::EngineDebugger::get_script_debugger(), "Can't set lines left. No active debugger");
	::EngineDebugger::get_script_debugger()->set_lines_left(p_lines);
}

int EngineDebugger::get_lines_left() const {
	ERR_FAIL_COND_V_MSG(!::EngineDebugger::get_script_debugger(), 0, "Can't get lines left. No active debugger");
	return ::EngineDebugger::get_script_debugger()->get_lines_left();
}

void EngineDebugger::set_depth(int p_depth) {
	ERR_FAIL_COND_MSG(!::EngineDebugger::get_script_debugger(), "Can't set depth. No active debugger");
	::EngineDebugger::get_script_debugger()->set_depth(p_depth);
}

int EngineDebugger::get_depth() const {
	ERR_FAIL_COND_V_MSG(!::EngineDebugger::get_script_debugger(), 0, "Can't get depth. No active debugger");
	return ::EngineDebugger::get_script_debugger()->get_depth();
}

bool EngineDebugger::is_breakpoint(int p_line, const StringName &p_source) const {
	ERR_FAIL_COND_V_MSG(!::EngineDebugger::get_script_debugger(), false, "Can't check breakpoint. No active debugger");
	return ::EngineDebugger::get_script_debugger()->is_breakpoint(p_line, p_source);
}

bool EngineDebugger::is_skipping_breakpoints() const {
	ERR_FAIL_COND_V_MSG(!::EngineDebugger::get_script_debugger(), false, "Can't check skipping breakpoint. No active debugger");
	return ::EngineDebugger::get_script_debugger()->is_skipping_breakpoints();
}

void EngineDebugger::insert_breakpoint(int p_line, const StringName &p_source) {
	ERR_FAIL_COND_MSG(!::EngineDebugger::get_script_debugger(), "Can't insert breakpoint. No active debugger");
	::EngineDebugger::get_script_debugger()->insert_breakpoint(p_line, p_source);
}

void EngineDebugger::remove_breakpoint(int p_line, const StringName &p_source) {
	ERR_FAIL_COND_MSG(!::EngineDebugger::get_script_debugger(), "Can't remove breakpoint. No active debugger");
	::EngineDebugger::get_script_debugger()->remove_breakpoint(p_line, p_source);
}

void EngineDebugger::clear_breakpoints() {
	ERR_FAIL_COND_MSG(!::EngineDebugger::get_script_debugger(), "Can't clear breakpoints. No active debugger");
	::EngineDebugger::get_script_debugger()->clear_breakpoints();
}

EngineDebugger::~EngineDebugger() {
	for (const KeyValue<StringName, Callable> &E : captures) {
		::EngineDebugger::unregister_message_capture(E.key);
	}
	captures.clear();
}

EngineDebugger *EngineDebugger::singleton = nullptr;

void EngineDebugger::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_active"), &EngineDebugger::is_active);

	ClassDB::bind_method(D_METHOD("register_profiler", "name", "profiler"), &EngineDebugger::register_profiler);
	ClassDB::bind_method(D_METHOD("unregister_profiler", "name"), &EngineDebugger::unregister_profiler);

	ClassDB::bind_method(D_METHOD("is_profiling", "name"), &EngineDebugger::is_profiling);
	ClassDB::bind_method(D_METHOD("has_profiler", "name"), &EngineDebugger::has_profiler);

	ClassDB::bind_method(D_METHOD("profiler_add_frame_data", "name", "data"), &EngineDebugger::profiler_add_frame_data);
	ClassDB::bind_method(D_METHOD("profiler_enable", "name", "enable", "arguments"), &EngineDebugger::profiler_enable, DEFVAL(Array()));

	ClassDB::bind_method(D_METHOD("register_message_capture", "name", "callable"), &EngineDebugger::register_message_capture);
	ClassDB::bind_method(D_METHOD("unregister_message_capture", "name"), &EngineDebugger::unregister_message_capture);
	ClassDB::bind_method(D_METHOD("has_capture", "name"), &EngineDebugger::has_capture);

	ClassDB::bind_method(D_METHOD("line_poll"), &EngineDebugger::line_poll);

	ClassDB::bind_method(D_METHOD("send_message", "message", "data"), &EngineDebugger::send_message);
	ClassDB::bind_method(D_METHOD("debug", "can_continue", "is_error_breakpoint"), &EngineDebugger::debug, DEFVAL(true), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("script_debug", "language", "can_continue", "is_error_breakpoint"), &EngineDebugger::script_debug, DEFVAL(true), DEFVAL(false));

	ClassDB::bind_method(D_METHOD("set_lines_left", "lines"), &EngineDebugger::set_lines_left);
	ClassDB::bind_method(D_METHOD("get_lines_left"), &EngineDebugger::get_lines_left);

	ClassDB::bind_method(D_METHOD("set_depth", "depth"), &EngineDebugger::set_depth);
	ClassDB::bind_method(D_METHOD("get_depth"), &EngineDebugger::get_depth);

	ClassDB::bind_method(D_METHOD("is_breakpoint", "line", "source"), &EngineDebugger::is_breakpoint);
	ClassDB::bind_method(D_METHOD("is_skipping_breakpoints"), &EngineDebugger::is_skipping_breakpoints);
	ClassDB::bind_method(D_METHOD("insert_breakpoint", "line", "source"), &EngineDebugger::insert_breakpoint);
	ClassDB::bind_method(D_METHOD("remove_breakpoint", "line", "source"), &EngineDebugger::remove_breakpoint);
	ClassDB::bind_method(D_METHOD("clear_breakpoints"), &EngineDebugger::clear_breakpoints);
}

} // namespace core_bind
