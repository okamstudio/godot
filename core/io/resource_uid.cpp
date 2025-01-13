/**************************************************************************/
/*  resource_uid.cpp                                                      */
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

#include "resource_uid.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"

// These constants are off by 1, causing the 'z' and '9' characters never to be used.
// This cannot be fixed without breaking compatibility; see GH-83843.
static constexpr uint32_t char_count = ('z' - 'a');
static constexpr uint32_t base = char_count + ('9' - '0');

String ResourceUID::get_cache_file() {
	return ProjectSettings::get_singleton()->get_project_data_path().path_join("uid_cache.bin");
}

static constexpr uint8_t uuid_characters[] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', '0', '1', '2', '3', '4', '5', '6', '7', '8' };
static constexpr uint32_t uuid_characters_element_count = (sizeof(uuid_characters) / sizeof(*uuid_characters));
static constexpr uint8_t max_uuid_number_length = 13; // Max 0x7FFFFFFFFFFFFFFF (uid://d4n4ub6itg400) size is 13 characters.

String ResourceUID::id_to_text(ID p_id) const {
	if (p_id < 0) {
		return "uid://<invalid>";
	}

	char32_t tmp[max_uuid_number_length];
	uint32_t tmp_size = 0;
	do {
		uint32_t c = p_id % uuid_characters_element_count;
		tmp[tmp_size] = uuid_characters[c];
		p_id /= uuid_characters_element_count;
		++tmp_size;
	} while (p_id);

	// tmp_size + uid:// (6) + 1 for null.
	String txt;
	txt.resize(tmp_size + 7);

	char32_t *p = txt.ptrw();
	p[0] = 'u';
	p[1] = 'i';
	p[2] = 'd';
	p[3] = ':';
	p[4] = '/';
	p[5] = '/';
	uint32_t size = 6;

	// The above loop give the number backward, recopy it in the string in the correct order.
	for (uint32_t i = 0; i < tmp_size; ++i) {
		p[size++] = tmp[tmp_size - i - 1];
	}

	p[size] = 0;

	return txt;
}

ResourceUID::ID ResourceUID::text_to_id(const String &p_text) const {
	if (!p_text.begins_with("uid://") || p_text == "uid://<invalid>") {
		return INVALID_ID;
	}

	uint32_t l = p_text.length();
	uint64_t uid = 0;
	for (uint32_t i = 6; i < l; i++) {
		uid *= base;
		uint32_t c = p_text[i];
		if (is_ascii_lower_case(c)) {
			uid += c - 'a';
		} else if (is_digit(c)) {
			uid += c - '0' + char_count;
		} else {
			return INVALID_ID;
		}
	}
	return ID(uid & 0x7FFFFFFFFFFFFFFF);
}

ResourceUID::ID ResourceUID::create_id() {
	while (true) {
		ID id = INVALID_ID;
		MutexLock lock(mutex);
		Error err = ((CryptoCore::RandomGenerator *)crypto)->get_random_bytes((uint8_t *)&id, sizeof(id));
		ERR_FAIL_COND_V(err != OK, INVALID_ID);
		id &= 0x7FFFFFFFFFFFFFFF;
		bool exists = unique_ids.has(id);
		if (!exists) {
			return id;
		}
	}
}

bool ResourceUID::has_id(ID p_id) const {
	MutexLock l(mutex);
	return unique_ids.has(p_id);
}
void ResourceUID::add_id(ID p_id, const String &p_path) {
	MutexLock l(mutex);
	ERR_FAIL_COND(unique_ids.has(p_id));
	Cache c;
	c.cs = p_path.utf8();
	unique_ids[p_id] = c;
	changed = true;
}

void ResourceUID::set_id(ID p_id, const String &p_path) {
	MutexLock l(mutex);
	ERR_FAIL_COND(!unique_ids.has(p_id));
	CharString cs = p_path.utf8();
	const char *update_ptr = cs.ptr();
	const char *cached_ptr = unique_ids[p_id].cs.ptr();
	if (update_ptr == nullptr && cached_ptr == nullptr) {
		return; // Both are empty strings.
	}
	if ((update_ptr == nullptr) != (cached_ptr == nullptr) || strcmp(update_ptr, cached_ptr) != 0) {
		unique_ids[p_id].cs = cs;
		unique_ids[p_id].saved_to_cache = false; //changed
		changed = true;
	}
}

String ResourceUID::get_id_path(ID p_id) const {
	ERR_FAIL_COND_V_MSG(p_id == INVALID_ID, String(), "Invalid UID.");
	MutexLock l(mutex);
	const ResourceUID::Cache *cache = unique_ids.getptr(p_id);
	ERR_FAIL_COND_V_MSG(!cache, String(), vformat("Unrecognized UID: \"%s\".", id_to_text(p_id)));
	const CharString &cs = cache->cs;
	return String::utf8(cs.ptr());
}

void ResourceUID::remove_id(ID p_id) {
	MutexLock l(mutex);
	ERR_FAIL_COND(!unique_ids.has(p_id));
	unique_ids.erase(p_id);
}

String ResourceUID::uid_to_path(const String &p_uid) {
	return singleton->get_id_path(singleton->text_to_id(p_uid));
}

String ResourceUID::path_to_uid(const String &p_path) {
	const ID id = ResourceLoader::get_resource_uid(p_path);
	if (id == INVALID_ID) {
		return p_path;
	} else {
		return singleton->id_to_text(id);
	}
}

String ResourceUID::ensure_path(const String &p_uid_or_path) {
	if (p_uid_or_path.begins_with("uid://")) {
		return uid_to_path(p_uid_or_path);
	}
	return p_uid_or_path;
}

Error ResourceUID::save_to_cache() {
	String cache_file = get_cache_file();
	if (!FileAccess::exists(cache_file)) {
		Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
		d->make_dir_recursive(String(cache_file).get_base_dir()); //ensure base dir exists
	}

	Ref<FileAccess> f = FileAccess::open(cache_file, FileAccess::WRITE);
	if (f.is_null()) {
		return ERR_CANT_OPEN;
	}

	MutexLock l(mutex);
	f->store_32(unique_ids.size());

	cache_entries = 0;

	for (KeyValue<ID, Cache> &E : unique_ids) {
		f->store_64(uint64_t(E.key));
		uint32_t s = E.value.cs.length();
		f->store_32(s);
		f->store_buffer((const uint8_t *)E.value.cs.ptr(), s);
		E.value.saved_to_cache = true;
		cache_entries++;
	}

	changed = false;
	return OK;
}

Error ResourceUID::load_from_cache(bool p_reset) {
	Ref<FileAccess> f = FileAccess::open(get_cache_file(), FileAccess::READ);
	if (f.is_null()) {
		return ERR_CANT_OPEN;
	}

	MutexLock l(mutex);
	if (p_reset) {
		unique_ids.clear();
	}

	uint32_t entry_count = f->get_32();
	for (uint32_t i = 0; i < entry_count; i++) {
		int64_t id = f->get_64();
		int32_t len = f->get_32();
		Cache c;
		c.cs.resize(len + 1);
		ERR_FAIL_COND_V(c.cs.size() != len + 1, ERR_FILE_CORRUPT); // Out of memory.
		c.cs[len] = 0;
		int32_t rl = f->get_buffer((uint8_t *)c.cs.ptrw(), len);
		ERR_FAIL_COND_V(rl != len, ERR_FILE_CORRUPT);

		c.saved_to_cache = true;
		unique_ids[id] = c;
	}

	cache_entries = entry_count;
	changed = false;
	return OK;
}

Error ResourceUID::update_cache() {
	if (!changed) {
		return OK;
	}

	if (cache_entries == 0) {
		return save_to_cache();
	}
	MutexLock l(mutex);

	Ref<FileAccess> f;
	for (KeyValue<ID, Cache> &E : unique_ids) {
		if (!E.value.saved_to_cache) {
			if (f.is_null()) {
				f = FileAccess::open(get_cache_file(), FileAccess::READ_WRITE); // Append.
				if (f.is_null()) {
					return ERR_CANT_OPEN;
				}
				f->seek_end();
			}
			f->store_64(uint64_t(E.key));
			uint32_t s = E.value.cs.length();
			f->store_32(s);
			f->store_buffer((const uint8_t *)E.value.cs.ptr(), s);
			E.value.saved_to_cache = true;
			cache_entries++;
		}
	}

	if (f.is_valid()) {
		f->seek(0);
		f->store_32(cache_entries); //update amount of entries
	}

	changed = false;

	return OK;
}

String ResourceUID::get_path_from_cache(Ref<FileAccess> &p_cache_file, const String &p_uid_string) {
	const uint32_t entry_count = p_cache_file->get_32();
	CharString cs;
	for (uint32_t i = 0; i < entry_count; i++) {
		int64_t id = p_cache_file->get_64();
		int32_t len = p_cache_file->get_32();
		cs.resize(len + 1);
		ERR_FAIL_COND_V(cs.size() != len + 1, String());
		cs[len] = 0;
		int32_t rl = p_cache_file->get_buffer((uint8_t *)cs.ptrw(), len);
		ERR_FAIL_COND_V(rl != len, String());

		if (singleton->id_to_text(id) == p_uid_string) {
			return String(cs);
		}
	}
	return String();
}

void ResourceUID::clear() {
	cache_entries = 0;
	unique_ids.clear();
	changed = false;
}
void ResourceUID::_bind_methods() {
	ClassDB::bind_method(D_METHOD("id_to_text", "id"), &ResourceUID::id_to_text);
	ClassDB::bind_method(D_METHOD("text_to_id", "text_id"), &ResourceUID::text_to_id);

	ClassDB::bind_method(D_METHOD("create_id"), &ResourceUID::create_id);

	ClassDB::bind_method(D_METHOD("has_id", "id"), &ResourceUID::has_id);
	ClassDB::bind_method(D_METHOD("add_id", "id", "path"), &ResourceUID::add_id);
	ClassDB::bind_method(D_METHOD("set_id", "id", "path"), &ResourceUID::set_id);
	ClassDB::bind_method(D_METHOD("get_id_path", "id"), &ResourceUID::get_id_path);
	ClassDB::bind_method(D_METHOD("remove_id", "id"), &ResourceUID::remove_id);

	BIND_CONSTANT(INVALID_ID)
}
ResourceUID *ResourceUID::singleton = nullptr;
ResourceUID::ResourceUID() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = this;
	crypto = memnew(CryptoCore::RandomGenerator);
	((CryptoCore::RandomGenerator *)crypto)->init();
}
ResourceUID::~ResourceUID() {
	memdelete((CryptoCore::RandomGenerator *)crypto);
}
