/**************************************************************************/
/*  engine.cpp                                                            */
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

#include "engine.h"

#include "core/authors.gen.h"
#include "core/config/project_settings.h"
#include "core/donors.gen.h"
#include "core/license.gen.h"
#include "core/variant/typed_array.h"
#include "core/version.h"
#include "servers/rendering/rendering_device.h"

void Engine::set_physics_ticks_per_second(int p_ips) {
	ERR_FAIL_COND_MSG(p_ips <= 0, "Engine iterations per second must be greater than 0.");
	ips = p_ips;
}

int Engine::get_physics_ticks_per_second() const {
	return ips;
}

void Engine::set_max_physics_steps_per_frame(int p_max_physics_steps) {
	ERR_FAIL_COND_MSG(p_max_physics_steps <= 0, "Maximum number of physics steps per frame must be greater than 0.");
	max_physics_steps_per_frame = p_max_physics_steps;
}

int Engine::get_max_physics_steps_per_frame() const {
	return max_physics_steps_per_frame;
}

void Engine::set_physics_jitter_fix(double p_threshold) {
	if (p_threshold < 0) {
		p_threshold = 0;
	}
	physics_jitter_fix = p_threshold;
}

double Engine::get_physics_jitter_fix() const {
	return physics_jitter_fix;
}

void Engine::set_max_fps(int p_fps) {
	_max_fps = p_fps > 0 ? p_fps : 0;

	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (rd) {
		rd->_set_max_fps(_max_fps);
	}
}

int Engine::get_max_fps() const {
	return _max_fps;
}

void Engine::set_audio_output_latency(int p_msec) {
	_audio_output_latency = p_msec > 1 ? p_msec : 1;
}

int Engine::get_audio_output_latency() const {
	return _audio_output_latency;
}

void Engine::increment_frames_drawn() {
	if (frame_server_synced) {
		server_syncs++;
	} else {
		server_syncs = 0;
	}
	frame_server_synced = false;

	frames_drawn++;
}

uint64_t Engine::get_frames_drawn() {
	return frames_drawn;
}

void Engine::set_frame_delay(uint32_t p_msec) {
	_frame_delay = p_msec;
}

uint32_t Engine::get_frame_delay() const {
	return _frame_delay;
}

void Engine::set_time_scale(double p_scale) {
	_time_scale = p_scale;
}

double Engine::get_time_scale() const {
	return freeze_time_scale ? 0 : _time_scale;
}

double Engine::get_unfrozen_time_scale() const {
	return _time_scale;
}

Dictionary Engine::get_version_info() const {
	Dictionary dict;
	dict["major"] = VERSION_MAJOR;
	dict["minor"] = VERSION_MINOR;
	dict["patch"] = VERSION_PATCH;
	dict["hex"] = VERSION_HEX;
	dict["status"] = VERSION_STATUS;
	dict["build"] = VERSION_BUILD;

	String hash = String(VERSION_HASH);
	dict["hash"] = hash.is_empty() ? String("unknown") : hash;

	dict["timestamp"] = VERSION_TIMESTAMP;

	String stringver = String(dict["major"]) + "." + String(dict["minor"]);
	if ((int)dict["patch"] != 0) {
		stringver += "." + String(dict["patch"]);
	}
	stringver += "-" + String(dict["status"]) + " (" + String(dict["build"]) + ")";
	dict["string"] = stringver;

	return dict;
}

static Array array_from_info(const char *const *info_list) {
	Array arr;
	for (int i = 0; info_list[i] != nullptr; i++) {
		arr.push_back(String::utf8(info_list[i]));
	}
	return arr;
}

static Array array_from_info_count(const char *const *info_list, int info_count) {
	Array arr;
	for (int i = 0; i < info_count; i++) {
		arr.push_back(String::utf8(info_list[i]));
	}
	return arr;
}

Dictionary Engine::get_author_info() const {
	Dictionary dict;

	dict["lead_developers"] = array_from_info(AUTHORS_LEAD_DEVELOPERS);
	dict["project_managers"] = array_from_info(AUTHORS_PROJECT_MANAGERS);
	dict["founders"] = array_from_info(AUTHORS_FOUNDERS);
	dict["developers"] = array_from_info(AUTHORS_DEVELOPERS);

	return dict;
}

TypedArray<Dictionary> Engine::get_copyright_info() const {
	TypedArray<Dictionary> components;
	for (int component_index = 0; component_index < COPYRIGHT_INFO_COUNT; component_index++) {
		const ComponentCopyright &cp_info = COPYRIGHT_INFO[component_index];
		Dictionary component_dict;
		component_dict["name"] = String::utf8(cp_info.name);
		Array parts;
		for (int i = 0; i < cp_info.part_count; i++) {
			const ComponentCopyrightPart &cp_part = cp_info.parts[i];
			Dictionary part_dict;
			part_dict["files"] = array_from_info_count(cp_part.files, cp_part.file_count);
			part_dict["copyright"] = array_from_info_count(cp_part.copyright_statements, cp_part.copyright_count);
			part_dict["license"] = String::utf8(cp_part.license);
			parts.push_back(part_dict);
		}
		component_dict["parts"] = parts;

		components.push_back(component_dict);
	}
	return components;
}

Dictionary Engine::get_donor_info() const {
	Dictionary donors;
	donors["patrons"] = array_from_info(DONORS_PATRONS);
	donors["platinum_sponsors"] = array_from_info(DONORS_SPONSORS_PLATINUM);
	donors["gold_sponsors"] = array_from_info(DONORS_SPONSORS_GOLD);
	donors["silver_sponsors"] = array_from_info(DONORS_SPONSORS_SILVER);
	donors["diamond_members"] = array_from_info(DONORS_MEMBERS_DIAMOND);
	donors["titanium_members"] = array_from_info(DONORS_MEMBERS_TITANIUM);
	donors["platinum_members"] = array_from_info(DONORS_MEMBERS_PLATINUM);
	donors["gold_members"] = array_from_info(DONORS_MEMBERS_GOLD);
	return donors;
}

Dictionary Engine::get_license_info() const {
	Dictionary licenses;
	for (int i = 0; i < LICENSE_COUNT; i++) {
		licenses[LICENSE_NAMES[i]] = LICENSE_BODIES[i];
	}
	return licenses;
}

String Engine::get_license_text() const {
	return String(GODOT_LICENSE_TEXT);
}

String Engine::get_architecture_name() const {
#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
	return "x86_64";

#elif defined(__i386) || defined(__i386__) || defined(_M_IX86)
	return "x86_32";

#elif defined(__aarch64__) || defined(_M_ARM64) || defined(_M_ARM64EC)
	return "arm64";

#elif defined(__arm__) || defined(_M_ARM)
	return "arm32";

#elif defined(__riscv)
#if __riscv_xlen == 8
	return "rv64";
#else
	return "riscv";
#endif

#elif defined(__powerpc__)
#if defined(__powerpc64__)
	return "ppc64";
#else
	return "ppc";
#endif

#elif defined(__loongarch64)
	return "loongarch64";

#elif defined(__wasm__)
#if defined(__wasm64__)
	return "wasm64";
#elif defined(__wasm32__)
	return "wasm32";
#endif
#endif
}

bool Engine::is_abort_on_gpu_errors_enabled() const {
	return abort_on_gpu_errors;
}

int32_t Engine::get_gpu_index() const {
	return gpu_idx;
}

bool Engine::is_validation_layers_enabled() const {
	return use_validation_layers;
}

bool Engine::is_generate_spirv_debug_info_enabled() const {
	return generate_spirv_debug_info;
}

bool Engine::is_extra_gpu_memory_tracking_enabled() const {
	return extra_gpu_memory_tracking;
}

#if defined(DEBUG_ENABLED) || defined(DEV_ENABLED)
bool Engine::is_accurate_breadcrumbs_enabled() const {
	return accurate_breadcrumbs;
}
#endif

void Engine::set_print_to_stdout(bool p_enabled) {
	CoreGlobals::print_line_enabled = p_enabled;
}

bool Engine::is_printing_to_stdout() const {
	return CoreGlobals::print_line_enabled;
}

void Engine::set_print_error_messages(bool p_enabled) {
	CoreGlobals::print_error_enabled = p_enabled;
}

bool Engine::is_printing_error_messages() const {
	return CoreGlobals::print_error_enabled;
}

void Engine::print_header(const String &p_string) const {
	if (_print_header) {
		print_line(p_string);
	}
}

void Engine::print_header_rich(const String &p_string) const {
	if (_print_header) {
		print_line_rich(p_string);
	}
}

void Engine::add_singleton(const Singleton &p_singleton) {
	ERR_FAIL_COND_MSG(singleton_ptrs.has(p_singleton.name), vformat("Can't register singleton '%s' because it already exists.", p_singleton.name));
	singletons.push_back(p_singleton);
	singleton_ptrs[p_singleton.name] = p_singleton.ptr;
}

Object *Engine::get_singleton_object(const StringName &p_name) const {
	HashMap<StringName, Object *>::ConstIterator E = singleton_ptrs.find(p_name);
	ERR_FAIL_COND_V_MSG(!E, nullptr, vformat("Failed to retrieve non-existent singleton '%s'.", p_name));

#ifdef TOOLS_ENABLED
	if (!is_editor_hint() && is_singleton_editor_only(p_name)) {
		ERR_FAIL_V_MSG(nullptr, vformat("Can't retrieve singleton '%s' outside of editor.", p_name));
	}
#endif

	return E->value;
}

bool Engine::is_singleton_user_created(const StringName &p_name) const {
	ERR_FAIL_COND_V(!singleton_ptrs.has(p_name), false);

	for (const Singleton &E : singletons) {
		if (E.name == p_name && E.user_created) {
			return true;
		}
	}

	return false;
}

bool Engine::is_singleton_editor_only(const StringName &p_name) const {
	ERR_FAIL_COND_V(!singleton_ptrs.has(p_name), false);

	for (const Singleton &E : singletons) {
		if (E.name == p_name && E.editor_only) {
			return true;
		}
	}

	return false;
}

void Engine::remove_singleton(const StringName &p_name) {
	ERR_FAIL_COND(!singleton_ptrs.has(p_name));

	for (List<Singleton>::Element *E = singletons.front(); E; E = E->next()) {
		if (E->get().name == p_name) {
			singletons.erase(E);
			singleton_ptrs.erase(p_name);
			return;
		}
	}
}

bool Engine::has_singleton(const StringName &p_name) const {
	return singleton_ptrs.has(p_name);
}

void Engine::get_singletons(List<Singleton> *p_singletons) {
	for (const Singleton &E : singletons) {
#ifdef TOOLS_ENABLED
		if (!is_editor_hint() && E.editor_only) {
			continue;
		}
#endif

		p_singletons->push_back(E);
	}
}

String Engine::get_write_movie_path() const {
	return write_movie_path;
}

void Engine::set_write_movie_path(const String &p_path) {
	write_movie_path = p_path;
}

void Engine::set_shader_cache_path(const String &p_path) {
	shader_cache_path = p_path;
}
String Engine::get_shader_cache_path() const {
	return shader_cache_path;
}

Engine *Engine::singleton = nullptr;

Engine *Engine::get_singleton() {
	return singleton;
}

bool Engine::notify_frame_server_synced() {
	frame_server_synced = true;
	return server_syncs > SERVER_SYNC_FRAME_COUNT_WARNING;
}

void Engine::set_freeze_time_scale(bool p_frozen) {
	freeze_time_scale = p_frozen;
}

Engine::Engine() {
	singleton = this;
}

Engine::~Engine() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

Engine::Singleton::Singleton(const StringName &p_name, Object *p_ptr, const StringName &p_class_name) :
		name(p_name),
		ptr(p_ptr),
		class_name(p_class_name) {
#ifdef DEBUG_ENABLED
	RefCounted *rc = Object::cast_to<RefCounted>(p_ptr);
	if (rc && !rc->is_referenced()) {
		WARN_PRINT("You must use Ref<> to ensure the lifetime of a RefCounted object intended to be used as a singleton.");
	}
#endif
}
