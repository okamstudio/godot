/**************************************************************************/
/*  string_name.h                                                         */
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

#ifndef STRING_NAME_H
#define STRING_NAME_H

#include "core/os/mutex.h"
#include "core/string/ustring.h"
#include "core/templates/safe_refcount.h"

#define UNIQUE_NODE_PREFIX "%"

class Main;

struct StaticCString {
	const char *ptr;
	static StaticCString create(const char *p_ptr);
};

class StringName {
	enum {
		STRING_TABLE_BITS = 16,
		STRING_TABLE_LEN = 1 << STRING_TABLE_BITS,
		STRING_TABLE_MASK = STRING_TABLE_LEN - 1
	};

	struct _Data {
		SafeRefCount refcount;
		SafeNumeric<uint32_t> static_count;
		const char *cname = nullptr;
		String name;
#ifdef DEBUG_ENABLED
		uint32_t debug_references = 0;
#endif
		String get_name() const { return cname ? String(cname) : name; }
		bool operator==(const String &p_name) const;
		bool operator!=(const String &p_name) const;
		bool operator==(const char *p_name) const;
		bool operator!=(const char *p_name) const;

		int idx = 0;
		uint32_t hash = 0;
		_Data *prev = nullptr;
		_Data *next = nullptr;
		_Data() {}
	};

	static inline _Data *_table[STRING_TABLE_LEN];

	_Data *_data = nullptr;

	void unref();
	friend void register_core_types();
	friend void unregister_core_types();
	friend class Main;
	static inline Mutex mutex;
	static void setup();
	static void cleanup();
	static uint32_t get_empty_hash();
	static inline bool configured = false;
#ifdef DEBUG_ENABLED
	struct DebugSortReferences {
		bool operator()(const _Data *p_left, const _Data *p_right) const {
			return p_left->debug_references > p_right->debug_references;
		}
	};

	static inline bool debug_stringname = false;
#endif

	StringName(_Data *p_data) { _data = p_data; }

public:
	operator const void *() const { return (_data && (_data->cname || !_data->name.is_empty())) ? (void *)1 : nullptr; }

	bool operator==(const String &p_name) const;
	bool operator==(const char *p_name) const;
	bool operator!=(const String &p_name) const;
	bool operator!=(const char *p_name) const;

	char32_t operator[](int p_index) const;
	int length() const;
	bool is_empty() const;

	_FORCE_INLINE_ bool is_node_unique_name() const {
		if (!_data) {
			return false;
		}
		if (_data->cname != nullptr) {
			return (char32_t)_data->cname[0] == (char32_t)UNIQUE_NODE_PREFIX[0];
		} else {
			return (char32_t)_data->name[0] == (char32_t)UNIQUE_NODE_PREFIX[0];
		}
	}
	_FORCE_INLINE_ bool operator<(const StringName &p_name) const {
		return _data < p_name._data;
	}
	_FORCE_INLINE_ bool operator<=(const StringName &p_name) const {
		return _data <= p_name._data;
	}
	_FORCE_INLINE_ bool operator>(const StringName &p_name) const {
		return _data > p_name._data;
	}
	_FORCE_INLINE_ bool operator>=(const StringName &p_name) const {
		return _data >= p_name._data;
	}
	_FORCE_INLINE_ bool operator==(const StringName &p_name) const {
		// The real magic of all this mess happens here.
		// This is why path comparisons are very fast.
		return _data == p_name._data;
	}
	_FORCE_INLINE_ bool operator!=(const StringName &p_name) const {
		return _data != p_name._data;
	}
	_FORCE_INLINE_ uint32_t hash() const {
		if (_data) {
			return _data->hash;
		} else {
			return get_empty_hash();
		}
	}
	_FORCE_INLINE_ const void *data_unique_pointer() const {
		return (void *)_data;
	}

	_FORCE_INLINE_ operator String() const {
		if (_data) {
			if (_data->cname) {
				return String(_data->cname);
			} else {
				return _data->name;
			}
		}

		return String();
	}

	static StringName search(const char *p_name);
	static StringName search(const char32_t *p_name);
	static StringName search(const String &p_name);

	struct AlphCompare {
		_FORCE_INLINE_ bool operator()(const StringName &l, const StringName &r) const {
			const char *l_cname = l._data ? l._data->cname : "";
			const char *r_cname = r._data ? r._data->cname : "";

			if (l_cname) {
				if (r_cname) {
					return is_str_less(l_cname, r_cname);
				} else {
					return is_str_less(l_cname, r._data->name.ptr());
				}
			} else {
				if (r_cname) {
					return is_str_less(l._data->name.ptr(), r_cname);
				} else {
					return is_str_less(l._data->name.ptr(), r._data->name.ptr());
				}
			}
		}
	};

	StringName &operator=(const StringName &p_name);
	StringName &operator=(StringName &&p_name) {
		if (_data == p_name._data) {
			return *this;
		}

		unref();
		_data = p_name._data;
		p_name._data = nullptr;
		return *this;
	}
	StringName(const char *p_name, bool p_static = false);
	StringName(const StringName &p_name);
	StringName(StringName &&p_name) {
		_data = p_name._data;
		p_name._data = nullptr;
	}
	StringName(const String &p_name, bool p_static = false);
	StringName(const StaticCString &p_static_string, bool p_static = false);
	StringName() {}

	static void assign_static_unique_class_name(StringName *ptr, const char *p_name);
	_FORCE_INLINE_ ~StringName() {
		if (likely(configured) && _data) { //only free if configured
			unref();
		}
	}

#ifdef DEBUG_ENABLED
	static void set_debug_stringnames(bool p_enable) { debug_stringname = p_enable; }
#endif
};

bool operator==(const String &p_name, const StringName &p_string_name);
bool operator!=(const String &p_name, const StringName &p_string_name);
bool operator==(const char *p_name, const StringName &p_string_name);
bool operator!=(const char *p_name, const StringName &p_string_name);

StringName _scs_create(const char *p_chr, bool p_static = false);

/*
 * The SNAME macro is used to speed up StringName creation, as it allows caching it after the first usage in a very efficient way.
 * It should NOT be used everywhere, but instead in places where high performance is required and the creation of a StringName
 * can be costly. Places where it should be used are:
 * - Control::get_theme_*(<name> and Window::get_theme_*(<name> functions.
 * - emit_signal(<name>,..) function
 * - call_deferred(<name>,..) function
 * - Comparisons to a StringName in overridden _set and _get methods.
 *
 * Use in places that can be called hundreds of times per frame (or more) is recommended, but this situation is very rare. If in doubt, do not use.
 */

#define SNAME(m_arg) ([]() -> const StringName & { static StringName sname = _scs_create(m_arg, true); return sname; })()

#endif // STRING_NAME_H
