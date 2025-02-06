/**************************************************************************/
/*  resource.cpp                                                          */
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

#include "resource.h"

#include "core/io/resource_loader.h"
#include "core/math/math_funcs.h"
#include "core/os/os.h"
#include "scene/main/node.h" //only so casting works

void Resource::emit_changed() {
	if (ResourceLoader::is_within_load() && !Thread::is_main_thread()) {
		ResourceLoader::resource_changed_emit(this);
		return;
	}

	emit_signal(CoreStringName(changed));
}

void Resource::_resource_path_changed() {
}

void Resource::set_path(const String &p_path, bool p_take_over) {
	if (path_cache == p_path) {
		return;
	}

	if (p_path.is_empty()) {
		p_take_over = false; // Can't take over an empty path
	}

	{
		MutexLock lock(ResourceCache::lock);

		if (!path_cache.is_empty()) {
			ResourceCache::resources.erase(path_cache);
		}

		path_cache = "";

		Ref<Resource> existing = ResourceCache::get_ref(p_path);

		if (existing.is_valid()) {
			if (p_take_over) {
				existing->path_cache = String();
				ResourceCache::resources.erase(p_path);
			} else {
				ERR_FAIL_MSG(vformat("Another resource is loaded from path '%s' (possible cyclic resource inclusion).", p_path));
			}
		}

		path_cache = p_path;

		if (!path_cache.is_empty()) {
			ResourceCache::resources[path_cache] = this;
		}
	}

	_resource_path_changed();
}

String Resource::get_path() const {
	return path_cache;
}

void Resource::set_path_cache(const String &p_path) {
	path_cache = p_path;
	GDVIRTUAL_CALL(_set_path_cache, p_path);
}

static thread_local RandomPCG unique_id_gen(0, RandomPCG::DEFAULT_INC);

void Resource::seed_scene_unique_id(uint32_t p_seed) {
	unique_id_gen.seed(p_seed);
}

String Resource::generate_scene_unique_id() {
	// Generate a unique enough hash, but still user-readable.
	// If it's not unique it does not matter because the saver will try again.
	if (unique_id_gen.get_seed() == 0) {
		OS::DateTime dt = OS::get_singleton()->get_datetime();
		uint32_t hash = hash_murmur3_one_32(OS::get_singleton()->get_ticks_usec());
		hash = hash_murmur3_one_32(dt.year, hash);
		hash = hash_murmur3_one_32(dt.month, hash);
		hash = hash_murmur3_one_32(dt.day, hash);
		hash = hash_murmur3_one_32(dt.hour, hash);
		hash = hash_murmur3_one_32(dt.minute, hash);
		hash = hash_murmur3_one_32(dt.second, hash);
		hash = hash_murmur3_one_32(Math::rand(), hash);
		unique_id_gen.seed(hash);
	}

	uint32_t random_num = unique_id_gen.rand();

	static constexpr uint32_t characters = 5;
	static constexpr uint32_t char_count = ('z' - 'a');
	static constexpr uint32_t base = char_count + ('9' - '0');
	String id;
	for (uint32_t i = 0; i < characters; i++) {
		uint32_t c = random_num % base;
		if (c < char_count) {
			id += String::chr('a' + c);
		} else {
			id += String::chr('0' + (c - char_count));
		}
		random_num /= base;
	}

	return id;
}

void Resource::set_scene_unique_id(const String &p_id) {
	bool is_valid = true;
	for (int i = 0; i < p_id.length(); i++) {
		if (!is_ascii_identifier_char(p_id[i])) {
			is_valid = false;
			scene_unique_id = Resource::generate_scene_unique_id();
			break;
		}
	}

	ERR_FAIL_COND_MSG(!is_valid, "The scene unique ID must contain only letters, numbers, and underscores.");
	scene_unique_id = p_id;
}

String Resource::get_scene_unique_id() const {
	return scene_unique_id;
}

void Resource::set_name(const String &p_name) {
	name = p_name;
	emit_changed();
}

String Resource::get_name() const {
	return name;
}

void Resource::update_configuration_warning() {
	if (_update_configuration_warning) {
		_update_configuration_warning();
	}
}

bool Resource::editor_can_reload_from_file() {
	return true; //by default yes
}

void Resource::connect_changed(const Callable &p_callable, uint32_t p_flags) {
	if (ResourceLoader::is_within_load() && !Thread::is_main_thread()) {
		ResourceLoader::resource_changed_connect(this, p_callable, p_flags);
		return;
	}

	if (!is_connected(CoreStringName(changed), p_callable) || p_flags & CONNECT_REFERENCE_COUNTED) {
		connect(CoreStringName(changed), p_callable, p_flags);
	}
}

void Resource::disconnect_changed(const Callable &p_callable) {
	if (ResourceLoader::is_within_load() && !Thread::is_main_thread()) {
		ResourceLoader::resource_changed_disconnect(this, p_callable);
		return;
	}

	if (is_connected(CoreStringName(changed), p_callable)) {
		disconnect(CoreStringName(changed), p_callable);
	}
}

void Resource::reset_state() {
	GDVIRTUAL_CALL(_reset_state);
}

Error Resource::copy_from(const Ref<Resource> &p_resource) {
	ERR_FAIL_COND_V(p_resource.is_null(), ERR_INVALID_PARAMETER);
	if (get_class() != p_resource->get_class()) {
		return ERR_INVALID_PARAMETER;
	}

	reset_state(); // May want to reset state.

	List<PropertyInfo> pi;
	p_resource->get_property_list(&pi);

	for (const PropertyInfo &E : pi) {
		if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}
		if (E.name == "resource_path") {
			continue; //do not change path
		}

		set(E.name, p_resource->get(E.name));
	}
	return OK;
}

void Resource::reload_from_file() {
	String path = get_path();
	if (!path.is_resource_file()) {
		return;
	}

	Ref<Resource> s = ResourceLoader::load(ResourceLoader::path_remap(path), get_class(), ResourceFormatLoader::CACHE_MODE_IGNORE);

	if (s.is_null()) {
		return;
	}

	copy_from(s);
}

Variant Resource::_duplicate_recursive(const Variant &p_variant, const DuplicateParams &p_params, uint32_t p_usage) const {
	// Anything other than object can be simply skipped in case of a shallow copy.
	if (!p_params.deep && p_variant.get_type() != Variant::OBJECT) {
		return p_variant;
	}

	switch (p_variant.get_type()) {
		case Variant::OBJECT: {
			const Ref<Resource> &sr = p_variant;
			bool should_duplicate = false;
			if (sr.is_valid()) {
				if ((p_usage & PROPERTY_USAGE_ALWAYS_DUPLICATE)) {
					should_duplicate = true;
				} else if ((p_usage & PROPERTY_USAGE_NEVER_DUPLICATE)) {
					should_duplicate = false;
				} else if (p_params.local_scene) {
					should_duplicate = sr->is_local_to_scene();
				} else {
					switch (p_params.subres_mode) {
						case RESOURCE_DEEP_DUPLICATE_NONE: {
							should_duplicate = false;
						} break;
						case RESOURCE_DEEP_DUPLICATE_INTERNAL: {
							should_duplicate = p_params.deep && sr->is_built_in();
						} break;
						case RESOURCE_DEEP_DUPLICATE_ALL: {
							should_duplicate = p_params.deep;
						} break;
						default: {
							DEV_ASSERT(false);
						}
					}
				}
			}
			if (should_duplicate) {
				if (thread_duplicate_remap_cache->has(sr)) {
					return thread_duplicate_remap_cache->get(sr);
				} else {
					const Ref<Resource> &dupe = p_params.local_scene
							? sr->duplicate_for_local_scene(p_params.local_scene, *thread_duplicate_remap_cache)
							: sr->_duplicate(p_params);
					thread_duplicate_remap_cache->insert(sr, dupe);
					return dupe;
				}
			} else {
				return p_variant;
			}
		} break;
		case Variant::ARRAY: {
			const Array &src = p_variant;
			Array dst;
			dst.resize(src.size());
			for (int i = 0; i < src.size(); i++) {
				dst[i] = _duplicate_recursive(src[i], p_params);
			}
			return dst;
		} break;
		case Variant::DICTIONARY: {
			const Dictionary &src = p_variant;
			Dictionary dst;
			List<Variant> keys;
			src.get_key_list(&keys);
			for (const Variant &k : keys) {
				const Variant &v = src[k];
				dst.set(
						_duplicate_recursive(k, p_params),
						_duplicate_recursive(v, p_params));
			}
			return dst;
		} break;
		case Variant::PACKED_BYTE_ARRAY:
		case Variant::PACKED_INT32_ARRAY:
		case Variant::PACKED_INT64_ARRAY:
		case Variant::PACKED_FLOAT32_ARRAY:
		case Variant::PACKED_FLOAT64_ARRAY:
		case Variant::PACKED_STRING_ARRAY:
		case Variant::PACKED_VECTOR2_ARRAY:
		case Variant::PACKED_VECTOR3_ARRAY:
		case Variant::PACKED_COLOR_ARRAY:
		case Variant::PACKED_VECTOR4_ARRAY: {
			return p_variant.duplicate();
		} break;
		default: {
			return p_variant;
		}
	}
}

Ref<Resource> Resource::_duplicate(const DuplicateParams &p_params) const {
	ERR_FAIL_COND_V_MSG(p_params.local_scene && p_params.subres_mode != RESOURCE_DEEP_DUPLICATE_MAX, Ref<Resource>(), "Duplication for local-to-scene can't specify a deep duplicate mode.");

	DuplicateRemapCacheT *remap_cache_backup = thread_duplicate_remap_cache;

// These are for avoiding potential duplicates that can happen in custom code
// from participating in the same duplication session (remap cache).
#define BEFORE_USER_CODE thread_duplicate_remap_cache = nullptr;
#define AFTER_USER_CODE thread_duplicate_remap_cache = remap_cache_backup;

	List<PropertyInfo> plist;
	get_property_list(&plist);

	BEFORE_USER_CODE
	Ref<Resource> r = Object::cast_to<Resource>(ClassDB::instantiate(get_class()));
	AFTER_USER_CODE
	ERR_FAIL_COND_V(r.is_null(), Ref<Resource>());

	thread_duplicate_remap_cache->insert(Ref<Resource>(this), r);

	if (p_params.local_scene) {
		r->local_scene = p_params.local_scene;
	}

	for (const PropertyInfo &E : plist) {
		if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}

		BEFORE_USER_CODE
		Variant p = get(E.name);
		AFTER_USER_CODE

		p = _duplicate_recursive(p, p_params, E.usage);

		BEFORE_USER_CODE
		r->set(E.name, p);
		AFTER_USER_CODE
	}

	return r;

#undef BEFORE_USER_CODE
#undef AFTER_USER_CODE
}

Ref<Resource> Resource::duplicate_for_local_scene(Node *p_for_scene, DuplicateRemapCacheT &p_remap_cache) const {
#ifdef DEBUG_ENABLED
	// The only possibilities for the remap cache passed being valid are these:
	// a) It's the same already used as the one of the thread. That happens when this function
	//    is called within some recursion level within a duplication.
	// b) There's no current thread remap cache, which means this function is acting as an entry point.
	// This check failing means that this function is being called as an entry point during an ongoing
	// duplication, likely due to custom instantiation or setter code. It would be an engine bug because
	// code starting or joining a duplicate session must ensure to exit it temporarily when making calls
	// that may in turn invoke such custom code.
	if (thread_duplicate_remap_cache && &p_remap_cache != thread_duplicate_remap_cache) {
		ERR_PRINT("Resource::duplicate_for_local_scene() called during an ongoing duplication session. This is an engine bug.");
	}
#endif

	DuplicateRemapCacheT *remap_cache_backup = thread_duplicate_remap_cache;
	thread_duplicate_remap_cache = &p_remap_cache;

	DuplicateParams params;
	params.deep = true;
	params.local_scene = p_for_scene;
	const Ref<Resource> &dupe = _duplicate(params);

	thread_duplicate_remap_cache = remap_cache_backup;

	return dupe;
}

void Resource::_find_sub_resources(const Variant &p_variant, HashSet<Ref<Resource>> &p_resources_found) {
	switch (p_variant.get_type()) {
		case Variant::ARRAY: {
			Array a = p_variant;
			for (int i = 0; i < a.size(); i++) {
				_find_sub_resources(a[i], p_resources_found);
			}
		} break;
		case Variant::DICTIONARY: {
			Dictionary d = p_variant;
			List<Variant> keys;
			d.get_key_list(&keys);
			for (const Variant &k : keys) {
				_find_sub_resources(k, p_resources_found);
				_find_sub_resources(d[k], p_resources_found);
			}
		} break;
		case Variant::OBJECT: {
			Ref<Resource> r = p_variant;
			if (r.is_valid()) {
				p_resources_found.insert(r);
			}
		} break;
		default: {
		}
	}
}

void Resource::configure_for_local_scene(Node *p_for_scene, DuplicateRemapCacheT &p_remap_cache) {
	List<PropertyInfo> plist;
	get_property_list(&plist);

	reset_local_to_scene();
	local_scene = p_for_scene;

	for (const PropertyInfo &E : plist) {
		if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}
		Variant p = get(E.name);

		HashSet<Ref<Resource>> sub_resources;
		_find_sub_resources(p, sub_resources);

		for (Ref<Resource> sr : sub_resources) {
			if (sr->is_local_to_scene()) {
				if (!p_remap_cache.has(sr)) {
					sr->configure_for_local_scene(p_for_scene, p_remap_cache);
					p_remap_cache[sr] = sr;
				}
			}
		}
	}
}

Ref<Resource> Resource::duplicate(bool p_deep) const {
	DuplicateRemapCacheT remap_cache;
	bool started_session = false;
	if (!thread_duplicate_remap_cache) {
		thread_duplicate_remap_cache = &remap_cache;
		started_session = true;
	}

	DuplicateParams params;
	params.deep = p_deep;
	params.subres_mode = RESOURCE_DEEP_DUPLICATE_INTERNAL;
	const Ref<Resource> &dupe = _duplicate(params);

	if (started_session) {
		thread_duplicate_remap_cache = nullptr;
	}

	return dupe;
}

Ref<Resource> Resource::duplicate_deep(ResourceDeepDuplicateMode p_deep_subresources_mode) const {
	ERR_FAIL_INDEX_V(p_deep_subresources_mode, RESOURCE_DEEP_DUPLICATE_MAX, Ref<Resource>());

	DuplicateRemapCacheT remap_cache;
	bool started_session = false;
	if (!thread_duplicate_remap_cache) {
		thread_duplicate_remap_cache = &remap_cache;
		started_session = true;
	}

	DuplicateParams params;
	params.deep = true;
	params.subres_mode = p_deep_subresources_mode;
	const Ref<Resource> &dupe = _duplicate(params);

	if (started_session) {
		thread_duplicate_remap_cache = nullptr;
	}

	return dupe;
}

Ref<Resource> Resource::_duplicate_from_variant(bool p_deep, ResourceDeepDuplicateMode p_deep_subresources_mode, int p_recursion_count) const {
	// A call without deep duplication would have been early-rejected at Variant::duplicate() unless it's the root call.
	DEV_ASSERT(!(p_recursion_count > 0 && p_deep_subresources_mode == RESOURCE_DEEP_DUPLICATE_NONE));

	// When duplicating from Variant, this function may be called multiple times from
	// different parts of the data structure being copied. Therefore, we need to create
	// a remap cache instance in a way that can be shared among all of the calls.
	// Whatever Variant, Array or Dictionary that initiated the call chain will eventually
	// claim it, when the stack unwinds up to the root call.
	// One exception is that this is the root call.

	if (p_recursion_count == 0) {
		if (p_deep) {
			return duplicate_deep(p_deep_subresources_mode);
		} else {
			return duplicate(false);
		}
	}

	if (thread_duplicate_remap_cache) {
		Resource::DuplicateRemapCacheT::Iterator E = thread_duplicate_remap_cache->find(Ref<Resource>(this));
		if (E) {
			return E->value;
		}
	} else {
		thread_duplicate_remap_cache = memnew(DuplicateRemapCacheT);
	}

	DuplicateParams params;
	params.deep = p_deep;
	params.subres_mode = p_deep_subresources_mode;

	const Ref<Resource> dupe = _duplicate(params);

	return dupe;
}

void Resource::_teardown_duplicate_from_variant() {
	if (thread_duplicate_remap_cache) {
		memdelete(thread_duplicate_remap_cache);
		thread_duplicate_remap_cache = nullptr;
	}
}

void Resource::_set_path(const String &p_path) {
	set_path(p_path, false);
}

void Resource::_take_over_path(const String &p_path) {
	set_path(p_path, true);
}

RID Resource::get_rid() const {
	RID ret;
	if (!GDVIRTUAL_CALL(_get_rid, ret)) {
#ifndef DISABLE_DEPRECATED
		if (_get_extension() && _get_extension()->get_rid) {
			ret = RID::from_uint64(_get_extension()->get_rid(_get_extension_instance()));
		}
#endif
	}
	return ret;
}

#ifdef TOOLS_ENABLED

uint32_t Resource::hash_edited_version_for_preview() const {
	uint32_t hash = hash_murmur3_one_32(get_edited_version());

	List<PropertyInfo> plist;
	get_property_list(&plist);

	for (const PropertyInfo &E : plist) {
		if (E.usage & PROPERTY_USAGE_STORAGE && E.type == Variant::OBJECT && E.hint == PROPERTY_HINT_RESOURCE_TYPE) {
			Ref<Resource> res = get(E.name);
			if (res.is_valid()) {
				hash = hash_murmur3_one_32(res->hash_edited_version_for_preview(), hash);
			}
		}
	}

	return hash;
}

#endif

void Resource::set_local_to_scene(bool p_enable) {
	local_to_scene = p_enable;
}

bool Resource::is_local_to_scene() const {
	return local_to_scene;
}

Node *Resource::get_local_scene() const {
	if (local_scene) {
		return local_scene;
	}

	if (_get_local_scene_func) {
		return _get_local_scene_func();
	}

	return nullptr;
}

void Resource::setup_local_to_scene() {
	emit_signal(SNAME("setup_local_to_scene_requested"));
	GDVIRTUAL_CALL(_setup_local_to_scene);
}

void Resource::reset_local_to_scene() {
	// Restores the state as if setup_local_to_scene() hadn't been called.
}

Node *(*Resource::_get_local_scene_func)() = nullptr;
void (*Resource::_update_configuration_warning)() = nullptr;

void Resource::set_as_translation_remapped(bool p_remapped) {
	if (remapped_list.in_list() == p_remapped) {
		return;
	}

	MutexLock lock(ResourceCache::lock);

	if (p_remapped) {
		ResourceLoader::remapped_list.add(&remapped_list);
	} else {
		ResourceLoader::remapped_list.remove(&remapped_list);
	}
}

//helps keep IDs same number when loading/saving scenes. -1 clears ID and it Returns -1 when no id stored
void Resource::set_id_for_path(const String &p_path, const String &p_id) {
#ifdef TOOLS_ENABLED
	if (p_id.is_empty()) {
		ResourceCache::path_cache_lock.write_lock();
		ResourceCache::resource_path_cache[p_path].erase(get_path());
		ResourceCache::path_cache_lock.write_unlock();
	} else {
		ResourceCache::path_cache_lock.write_lock();
		ResourceCache::resource_path_cache[p_path][get_path()] = p_id;
		ResourceCache::path_cache_lock.write_unlock();
	}
#endif
}

String Resource::get_id_for_path(const String &p_path) const {
#ifdef TOOLS_ENABLED
	ResourceCache::path_cache_lock.read_lock();
	if (ResourceCache::resource_path_cache[p_path].has(get_path())) {
		String result = ResourceCache::resource_path_cache[p_path][get_path()];
		ResourceCache::path_cache_lock.read_unlock();
		return result;
	} else {
		ResourceCache::path_cache_lock.read_unlock();
		return "";
	}
#else
	return "";
#endif
}

void Resource::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_path", "path"), &Resource::_set_path);
	ClassDB::bind_method(D_METHOD("take_over_path", "path"), &Resource::_take_over_path);
	ClassDB::bind_method(D_METHOD("get_path"), &Resource::get_path);
	ClassDB::bind_method(D_METHOD("set_path_cache", "path"), &Resource::set_path_cache);
	ClassDB::bind_method(D_METHOD("set_name", "name"), &Resource::set_name);
	ClassDB::bind_method(D_METHOD("get_name"), &Resource::get_name);
	ClassDB::bind_method(D_METHOD("get_rid"), &Resource::get_rid);
	ClassDB::bind_method(D_METHOD("set_local_to_scene", "enable"), &Resource::set_local_to_scene);
	ClassDB::bind_method(D_METHOD("is_local_to_scene"), &Resource::is_local_to_scene);
	ClassDB::bind_method(D_METHOD("get_local_scene"), &Resource::get_local_scene);
	ClassDB::bind_method(D_METHOD("setup_local_to_scene"), &Resource::setup_local_to_scene);
	ClassDB::bind_method(D_METHOD("reset_state"), &Resource::reset_state);

	ClassDB::bind_method(D_METHOD("set_id_for_path", "path", "id"), &Resource::set_id_for_path);
	ClassDB::bind_method(D_METHOD("get_id_for_path", "path"), &Resource::get_id_for_path);

	ClassDB::bind_method(D_METHOD("is_built_in"), &Resource::is_built_in);

	ClassDB::bind_static_method("Resource", D_METHOD("generate_scene_unique_id"), &Resource::generate_scene_unique_id);
	ClassDB::bind_method(D_METHOD("set_scene_unique_id", "id"), &Resource::set_scene_unique_id);
	ClassDB::bind_method(D_METHOD("get_scene_unique_id"), &Resource::get_scene_unique_id);

	ClassDB::bind_method(D_METHOD("emit_changed"), &Resource::emit_changed);

	ClassDB::bind_method(D_METHOD("duplicate", "deep"), &Resource::duplicate, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("duplicate_deep", "deep_subresources_mode"), &Resource::duplicate_deep, DEFVAL(RESOURCE_DEEP_DUPLICATE_INTERNAL));

	// For the bindings, it's much more natural to expose this enum from the Variant realm via Resource.
	ClassDB::bind_integer_constant(get_class_static(), StringName("ResourceDeepDuplicateMode"), "RESOURCE_DEEP_DUPLICATE_NONE", RESOURCE_DEEP_DUPLICATE_NONE);
	ClassDB::bind_integer_constant(get_class_static(), StringName("ResourceDeepDuplicateMode"), "RESOURCE_DEEP_DUPLICATE_INTERNAL", RESOURCE_DEEP_DUPLICATE_INTERNAL);
	ClassDB::bind_integer_constant(get_class_static(), StringName("ResourceDeepDuplicateMode"), "RESOURCE_DEEP_DUPLICATE_ALL", RESOURCE_DEEP_DUPLICATE_ALL);

	ADD_SIGNAL(MethodInfo("changed"));
	ADD_SIGNAL(MethodInfo("setup_local_to_scene_requested"));

	ADD_GROUP("Resource", "resource_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "resource_local_to_scene"), "set_local_to_scene", "is_local_to_scene");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "resource_path", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), "set_path", "get_path");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "resource_name"), "set_name", "get_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "resource_scene_unique_id", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_scene_unique_id", "get_scene_unique_id");

	GDVIRTUAL_BIND(_setup_local_to_scene);
	GDVIRTUAL_BIND(_get_rid);
	GDVIRTUAL_BIND(_reset_state);
	GDVIRTUAL_BIND(_set_path_cache, "path");
}

Resource::Resource() :
		remapped_list(this) {}

Resource::~Resource() {
	if (unlikely(path_cache.is_empty())) {
		return;
	}

	MutexLock lock(ResourceCache::lock);
	// Only unregister from the cache if this is the actual resource listed there.
	// (Other resources can have the same value in `path_cache` if loaded with `CACHE_IGNORE`.)
	HashMap<String, Resource *>::Iterator E = ResourceCache::resources.find(path_cache);
	if (likely(E && E->value == this)) {
		ResourceCache::resources.remove(E);
	}
}

HashMap<String, Resource *> ResourceCache::resources;
#ifdef TOOLS_ENABLED
HashMap<String, HashMap<String, String>> ResourceCache::resource_path_cache;
#endif

Mutex ResourceCache::lock;
#ifdef TOOLS_ENABLED
RWLock ResourceCache::path_cache_lock;
#endif

void ResourceCache::clear() {
	if (!resources.is_empty()) {
		if (OS::get_singleton()->is_stdout_verbose()) {
			ERR_PRINT(vformat("%d resources still in use at exit.", resources.size()));
			for (const KeyValue<String, Resource *> &E : resources) {
				print_line(vformat("Resource still in use: %s (%s)", E.key, E.value->get_class()));
			}
		} else {
			ERR_PRINT(vformat("%d resources still in use at exit (run with --verbose for details).", resources.size()));
		}
	}

	resources.clear();
}

bool ResourceCache::has(const String &p_path) {
	Resource **res = nullptr;

	{
		MutexLock mutex_lock(lock);

		res = resources.getptr(p_path);

		if (res && (*res)->get_reference_count() == 0) {
			// This resource is in the process of being deleted, ignore its existence.
			(*res)->path_cache = String();
			resources.erase(p_path);
			res = nullptr;
		}
	}

	if (!res) {
		return false;
	}

	return true;
}

Ref<Resource> ResourceCache::get_ref(const String &p_path) {
	Ref<Resource> ref;
	{
		MutexLock mutex_lock(lock);
		Resource **res = resources.getptr(p_path);

		if (res) {
			ref = Ref<Resource>(*res);
		}

		if (res && ref.is_null()) {
			// This resource is in the process of being deleted, ignore its existence
			(*res)->path_cache = String();
			resources.erase(p_path);
			res = nullptr;
		}
	}

	return ref;
}

void ResourceCache::get_cached_resources(List<Ref<Resource>> *p_resources) {
	MutexLock mutex_lock(lock);

	LocalVector<String> to_remove;

	for (KeyValue<String, Resource *> &E : resources) {
		Ref<Resource> ref = Ref<Resource>(E.value);

		if (ref.is_null()) {
			// This resource is in the process of being deleted, ignore its existence
			E.value->path_cache = String();
			to_remove.push_back(E.key);
			continue;
		}

		p_resources->push_back(ref);
	}

	for (const String &E : to_remove) {
		resources.erase(E);
	}
}

int ResourceCache::get_cached_resource_count() {
	MutexLock mutex_lock(lock);
	return resources.size();
}
