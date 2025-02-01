/**************************************************************************/
/*  optimized_translation.h                                               */
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

#pragma once

#include "core/string/translation.h"

class OptimizedTranslation : public Translation {
	GDCLASS(OptimizedTranslation, Translation);

	//this translation uses a sort of modified perfect hash algorithm
	//it requires hashing strings twice and then does a binary search,
	//so it's slower, but at the same time it has an extremely high chance
	//of catching untranslated strings

	//load/store friendly types
	Vector<int> hash_table;
	Vector<int> bucket_table;
	Vector<uint8_t> strings;

	struct Bucket {
		int size;
		uint32_t func;

		struct Elem {
			uint32_t key;
			uint32_t str_offset;
			uint32_t comp_size;
			uint32_t uncomp_size;
		};

		Elem elem[1];
	};

	_FORCE_INLINE_ uint32_t hash(uint32_t d, const char *p_str) const {
		if (d == 0) {
			d = 0x1000193;
		}
		while (*p_str) {
			d = (d * 0x1000193) ^ uint32_t(*p_str);
			p_str++;
		}

		return d;
	}

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	static void _bind_methods();

public:
	virtual StringName get_message(const StringName &p_src_text, const StringName &p_context = "") const override; //overridable for other implementations
	virtual StringName get_plural_message(const StringName &p_src_text, const StringName &p_plural_text, int p_n, const StringName &p_context = "") const override;
	virtual Vector<String> get_translated_message_list() const override;
	void generate(const Ref<Translation> &p_from);

	OptimizedTranslation() {}
};
