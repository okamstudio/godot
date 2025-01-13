/**************************************************************************/
/*  resource_uid.h                                                        */
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

#ifndef RESOURCE_UID_H
#define RESOURCE_UID_H

#include "core/object/ref_counted.h"
#include "core/string/string_name.h"
#include "core/templates/hash_map.h"

class FileAccess;

class ResourceUID : public Object {
	GDCLASS(ResourceUID, Object)
public:
	typedef int64_t ID;
	constexpr const static ID INVALID_ID = -1;

	static String get_cache_file();

private:
	void *crypto = nullptr; // CryptoCore::RandomGenerator (avoid including crypto_core.h)
	Mutex mutex;
	struct Cache {
		CharString cs;
		bool saved_to_cache = false;
	};

	HashMap<ID, Cache> unique_ids; //unique IDs and utf8 paths (less memory used)
	static ResourceUID *singleton;

	uint32_t cache_entries = 0;
	bool changed = false;

protected:
	static void _bind_methods();

public:
	String id_to_text(ID p_id) const;
	ID text_to_id(const String &p_text) const;

	ID create_id();
	bool has_id(ID p_id) const;
	void add_id(ID p_id, const String &p_path);
	void set_id(ID p_id, const String &p_path);
	String get_id_path(ID p_id) const;
	void remove_id(ID p_id);

	static String uid_to_path(const String &p_uid);
	static String path_to_uid(const String &p_path);
	static String ensure_path(const String &p_uid_or_path);

	Error load_from_cache(bool p_reset);
	Error save_to_cache();
	Error update_cache();
	static String get_path_from_cache(Ref<FileAccess> &p_cache_file, const String &p_uid_string);

	void clear();

	static ResourceUID *get_singleton() { return singleton; }

	ResourceUID();
	~ResourceUID();
};

#endif // RESOURCE_UID_H
