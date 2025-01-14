/**************************************************************************/
/*  optimized_translation.cpp                                             */
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

#include "optimized_translation.h"

#include "core/templates/pair.h"

extern "C" {
#include "thirdparty/misc/smaz.h"
}

struct CompressedString {
	int orig_len = 0;
	CharString compressed;
	int offset = 0;
};

void OptimizedTranslation::generate(const Ref<Translation> &p_from) {
	// This method compresses a Translation instance.
	// Right now, it doesn't handle context or plurals, so Translation subclasses using plurals or context (i.e TranslationPO) shouldn't be compressed.
#ifdef TOOLS_ENABLED
	ERR_FAIL_COND(p_from.is_null());
	List<StringName> keys;
	p_from->get_message_list(&keys);

	int size = Math::larger_prime(keys.size());

	Vector<Vector<Pair<int, CharString>>> buckets;
	Vector<HashMap<uint32_t, int>> table;
	Vector<uint32_t> hfunc_table;
	Vector<CompressedString> compressed;

	table.resize(size);
	hfunc_table.resize(size);
	buckets.resize(size);
	compressed.resize(keys.size());

	int idx = 0;
	int total_compression_size = 0;

	for (const StringName &E : keys) {
		//hash string
		CharString cs = E.operator String().utf8();
		uint32_t h = hash(0, cs.get_data());
		Pair<int, CharString> p;
		p.first = idx;
		p.second = cs;
		buckets.write[h % size].push_back(p);

		//compress string
		CharString src_s = p_from->get_message(E).utf8();
		CompressedString ps;
		ps.orig_len = src_s.size();
		ps.offset = total_compression_size;

		if (ps.orig_len != 0) {
			CharString dst_s;
			dst_s.resize(src_s.size());
			int ret = smaz_compress(src_s.get_data(), src_s.size(), dst_s.ptrw(), src_s.size());
			if (ret >= src_s.size()) {
				//if compressed is larger than original, just use original
				ps.orig_len = src_s.size();
				ps.compressed = src_s;
			} else {
				dst_s.resize(ret);
				//ps.orig_len=;
				ps.compressed = dst_s;
			}
		} else {
			ps.orig_len = 1;
			ps.compressed.resize(1);
			ps.compressed[0] = 0;
		}

		compressed.write[idx] = ps;
		total_compression_size += ps.compressed.size();
		idx++;
	}

	int bucket_table_size = 0;

	for (int i = 0; i < size; i++) {
		const Vector<Pair<int, CharString>> &b = buckets[i];
		HashMap<uint32_t, int> &t = table.write[i];

		if (b.size() == 0) {
			continue;
		}

		int d = 1;
		int item = 0;

		while (item < b.size()) {
			uint32_t slot = hash(d, b[item].second.get_data());
			if (t.has(slot)) {
				item = 0;
				d++;
				t.clear();
			} else {
				t[slot] = b[item].first;
				item++;
			}
		}

		hfunc_table.write[i] = d;
		bucket_table_size += 2 + b.size() * 4;
	}

	ERR_FAIL_COND(bucket_table_size == 0);

	hash_table.resize(size);
	bucket_table.resize(bucket_table_size);

	int *htwb = hash_table.ptrw();
	int *btwb = bucket_table.ptrw();

	uint32_t *htw = (uint32_t *)&htwb[0];
	uint32_t *btw = (uint32_t *)&btwb[0];

	int btindex = 0;

	for (int i = 0; i < size; i++) {
		const HashMap<uint32_t, int> &t = table[i];
		if (t.size() == 0) {
			htw[i] = 0xFFFFFFFF; //nothing
			continue;
		}

		htw[i] = btindex;
		btw[btindex++] = t.size();
		btw[btindex++] = hfunc_table[i];

		for (const KeyValue<uint32_t, int> &E : t) {
			btw[btindex++] = E.key;
			btw[btindex++] = compressed[E.value].offset;
			btw[btindex++] = compressed[E.value].compressed.size();
			btw[btindex++] = compressed[E.value].orig_len;
		}
	}

	strings.resize(total_compression_size);
	uint8_t *cw = strings.ptrw();

	for (int i = 0; i < compressed.size(); i++) {
		memcpy(&cw[compressed[i].offset], compressed[i].compressed.get_data(), compressed[i].compressed.size());
	}

	ERR_FAIL_COND(btindex != bucket_table_size);
	set_locale(p_from->get_locale());

#endif
}

bool OptimizedTranslation::_set(const StringName &p_name, const Variant &p_value) {
	String prop_name = p_name.operator String();
	if (prop_name == "hash_table") {
		hash_table = p_value;
	} else if (prop_name == "bucket_table") {
		bucket_table = p_value;
	} else if (prop_name == "strings") {
		strings = p_value;
	} else if (prop_name == "load_from") {
		generate(p_value);
	} else {
		return false;
	}

	return true;
}

bool OptimizedTranslation::_get(const StringName &p_name, Variant &r_ret) const {
	String prop_name = p_name.operator String();
	if (prop_name == "hash_table") {
		r_ret = hash_table;
	} else if (prop_name == "bucket_table") {
		r_ret = bucket_table;
	} else if (prop_name == "strings") {
		r_ret = strings;
	} else {
		return false;
	}

	return true;
}

String OptimizedTranslation::get_message(const StringName &p_src_text, const StringName &p_context) const {
	// p_context passed in is ignore. The use of context is not yet supported in OptimizedTranslation.

	int htsize = hash_table.size();

	if (htsize == 0) {
		return String();
	}

	CharString str = p_src_text.operator String().utf8();
	uint32_t h = hash(0, str.get_data());

	const int *htr = hash_table.ptr();
	const uint32_t *htptr = (const uint32_t *)&htr[0];
	const int *btr = bucket_table.ptr();
	const uint32_t *btptr = (const uint32_t *)&btr[0];
	const uint8_t *sr = strings.ptr();
	const char *sptr = (const char *)&sr[0];

	uint32_t p = htptr[h % htsize];

	if (p == 0xFFFFFFFF) {
		return String(); //nothing
	}

	const Bucket &bucket = *(const Bucket *)&btptr[p];

	h = hash(bucket.func, str.get_data());

	int idx = -1;

	for (int i = 0; i < bucket.size; i++) {
		if (bucket.elem[i].key == h) {
			idx = i;
			break;
		}
	}

	if (idx == -1) {
		return String();
	}

	if (bucket.elem[idx].comp_size == bucket.elem[idx].uncomp_size) {
		String rstr;
		rstr.parse_utf8(&sptr[bucket.elem[idx].str_offset], bucket.elem[idx].uncomp_size);

		return rstr;
	} else {
		CharString uncomp;
		uncomp.resize(bucket.elem[idx].uncomp_size + 1);
		smaz_decompress(&sptr[bucket.elem[idx].str_offset], bucket.elem[idx].comp_size, uncomp.ptrw(), bucket.elem[idx].uncomp_size);
		String rstr;
		rstr.parse_utf8(uncomp.get_data());
		return rstr;
	}
}

Vector<String> OptimizedTranslation::get_translated_message_list() const {
	Vector<String> msgs;

	const int *htr = hash_table.ptr();
	const uint32_t *htptr = (const uint32_t *)&htr[0];
	const int *btr = bucket_table.ptr();
	const uint32_t *btptr = (const uint32_t *)&btr[0];
	const uint8_t *sr = strings.ptr();
	const char *sptr = (const char *)&sr[0];

	for (int i = 0; i < hash_table.size(); i++) {
		uint32_t p = htptr[i];
		if (p != 0xFFFFFFFF) {
			const Bucket &bucket = *(const Bucket *)&btptr[p];
			for (int j = 0; j < bucket.size; j++) {
				if (bucket.elem[j].comp_size == bucket.elem[j].uncomp_size) {
					String rstr;
					rstr.parse_utf8(&sptr[bucket.elem[j].str_offset], bucket.elem[j].uncomp_size);
					msgs.push_back(rstr);
				} else {
					CharString uncomp;
					uncomp.resize(bucket.elem[j].uncomp_size + 1);
					smaz_decompress(&sptr[bucket.elem[j].str_offset], bucket.elem[j].comp_size, uncomp.ptrw(), bucket.elem[j].uncomp_size);
					String rstr;
					rstr.parse_utf8(uncomp.get_data());
					msgs.push_back(rstr);
				}
			}
		}
	}
	return msgs;
}

String OptimizedTranslation::get_plural_message(const StringName &p_src_text, const StringName &p_plural_text, int p_n, const StringName &p_context) const {
	// The use of plurals translation is not yet supported in OptimizedTranslation.
	return get_message(p_src_text, p_context);
}

void OptimizedTranslation::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::PACKED_INT32_ARRAY, "hash_table"));
	p_list->push_back(PropertyInfo(Variant::PACKED_INT32_ARRAY, "bucket_table"));
	p_list->push_back(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "strings"));
	p_list->push_back(PropertyInfo(Variant::OBJECT, "load_from", PROPERTY_HINT_RESOURCE_TYPE, "Translation", PROPERTY_USAGE_EDITOR));
}

void OptimizedTranslation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("generate", "from"), &OptimizedTranslation::generate);
}
