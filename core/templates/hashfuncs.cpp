/**************************************************************************/
/*  hashfuncs.cpp                                                         */
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

#include "hashfuncs.h"

uint32_t hash_djb2(const char *p_cstr) {
	const unsigned char *chr = (const unsigned char *)p_cstr;
	uint32_t hash = 5381;
	uint32_t c = *chr++;

	while (c) {
		hash = ((hash << 5) + hash) ^ c; /* hash * 33 ^ c */
		c = *chr++;
	}

	return hash;
}

uint32_t hash_djb2_buffer(const uint8_t *p_buff, int p_len, uint32_t p_prev) {
	uint32_t hash = p_prev;

	for (int i = 0; i < p_len; i++) {
		hash = ((hash << 5) + hash) ^ p_buff[i]; /* hash * 33 + c */
	}

	return hash;
}

uint32_t hash_murmur3_buffer(const void *key, int length, const uint32_t seed) {
	// Although not required, this is a random prime number.
	const uint8_t *data = (const uint8_t *)key;
	const int nblocks = length / 4;

	uint32_t h1 = seed;

	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;

	const uint32_t *blocks = (const uint32_t *)(data + nblocks * 4);

	for (int i = -nblocks; i; i++) {
		uint32_t k1 = blocks[i];

		k1 *= c1;
		k1 = hash_rotl32(k1, 15);
		k1 *= c2;

		h1 ^= k1;
		h1 = hash_rotl32(h1, 13);
		h1 = h1 * 5 + 0xe6546b64;
	}

	const uint8_t *tail = (const uint8_t *)(data + nblocks * 4);

	uint32_t k1 = 0;

	switch (length & 3) {
		case 3:
			k1 ^= tail[2] << 16;
			[[fallthrough]];
		case 2:
			k1 ^= tail[1] << 8;
			[[fallthrough]];
		case 1:
			k1 ^= tail[0];
			k1 *= c1;
			k1 = hash_rotl32(k1, 15);
			k1 *= c2;
			h1 ^= k1;
	};

	// Finalize with additional bit mixing.
	h1 ^= length;
	return hash_fmix32(h1);
}

uint32_t HashMapHasherDefault::hash(const String &p_string) {
	return hash_fmix32(p_string.hash());
}

uint32_t HashMapHasherDefault::hash(const char *p_cstr) {
	return hash_fmix32(hash_djb2(p_cstr));
}

uint32_t HashMapHasherDefault::hash(const wchar_t p_wchar) {
	return hash_fmix32(uint32_t(p_wchar));
}

uint32_t HashMapHasherDefault::hash(const char16_t p_uchar) {
	return hash_fmix32(uint32_t(p_uchar));
}

uint32_t HashMapHasherDefault::hash(const char32_t p_uchar) {
	return hash_fmix32(uint32_t(p_uchar));
}

uint32_t HashMapHasherDefault::hash(const RID &p_rid) {
	return hash_one_uint64(p_rid.get_id());
}

uint32_t HashMapHasherDefault::hash(const CharString &p_char_string) {
	return hash_murmur3_buffer(p_char_string.get_data(), sizeof(char) * p_char_string.size());
}

uint32_t HashMapHasherDefault::hash(const StringName &p_string_name) {
	return p_string_name.hash_mixed();
}

uint32_t HashMapHasherDefault::hash(const NodePath &p_path) {
	return p_path.hash_mixed();
}

uint32_t HashMapHasherDefault::hash(const ObjectID &p_id) {
	return hash_one_uint64(p_id);
}

uint32_t HashMapHasherDefault::hash(const uint64_t p_int) {
	return hash_one_uint64(p_int);
}

uint32_t HashMapHasherDefault::hash(const int64_t p_int) {
	return hash_one_uint64(p_int);
}

uint32_t HashMapHasherDefault::hash(const float p_float) {
	return hash_murmur3_one_float(p_float);
}

uint32_t HashMapHasherDefault::hash(const double p_double) {
	return hash_murmur3_one_double(p_double);
}

uint32_t HashMapHasherDefault::hash(const uint32_t p_int) {
	return hash_fmix32(p_int);
}

uint32_t HashMapHasherDefault::hash(const int32_t p_int) {
	return hash_fmix32(uint32_t(p_int));
}

uint32_t HashMapHasherDefault::hash(const uint16_t p_int) {
	return hash_fmix32(uint32_t(p_int));
}

uint32_t HashMapHasherDefault::hash(const int16_t p_int) {
	return hash_fmix32(uint32_t(p_int));
}

uint32_t HashMapHasherDefault::hash(const uint8_t p_int) {
	return hash_fmix32(uint32_t(p_int));
}

uint32_t HashMapHasherDefault::hash(const int8_t p_int) {
	return hash_fmix32(uint32_t(p_int));
}

uint32_t HashMapHasherDefault::hash(const Vector2i &p_vec) {
	uint32_t h = hash_murmur3_one_32(uint32_t(p_vec.x));
	h = hash_murmur3_one_32(uint32_t(p_vec.y), h);
	return hash_fmix32(h);
}

uint32_t HashMapHasherDefault::hash(const Vector3i &p_vec) {
	uint32_t h = hash_murmur3_one_32(uint32_t(p_vec.x));
	h = hash_murmur3_one_32(uint32_t(p_vec.y), h);
	h = hash_murmur3_one_32(uint32_t(p_vec.z), h);
	return hash_fmix32(h);
}

uint32_t HashMapHasherDefault::hash(const Vector4i &p_vec) {
	uint32_t h = hash_murmur3_one_32(uint32_t(p_vec.x));
	h = hash_murmur3_one_32(uint32_t(p_vec.y), h);
	h = hash_murmur3_one_32(uint32_t(p_vec.z), h);
	h = hash_murmur3_one_32(uint32_t(p_vec.w), h);
	return hash_fmix32(h);
}

uint32_t HashMapHasherDefault::hash(const Vector2 &p_vec) {
	uint32_t h = hash_murmur3_one_real(p_vec.x);
	h = hash_murmur3_one_real(p_vec.y, h);
	return hash_fmix32(h);
}

uint32_t HashMapHasherDefault::hash(const Vector3 &p_vec) {
	uint32_t h = hash_murmur3_one_real(p_vec.x);
	h = hash_murmur3_one_real(p_vec.y, h);
	h = hash_murmur3_one_real(p_vec.z, h);
	return hash_fmix32(h);
}

uint32_t HashMapHasherDefault::hash(const Vector4 &p_vec) {
	uint32_t h = hash_murmur3_one_real(p_vec.x);
	h = hash_murmur3_one_real(p_vec.y, h);
	h = hash_murmur3_one_real(p_vec.z, h);
	h = hash_murmur3_one_real(p_vec.w, h);
	return hash_fmix32(h);
}

uint32_t HashMapHasherDefault::hash(const Rect2i &p_rect) {
	uint32_t h = hash_murmur3_one_32(uint32_t(p_rect.position.x));
	h = hash_murmur3_one_32(uint32_t(p_rect.position.y), h);
	h = hash_murmur3_one_32(uint32_t(p_rect.size.x), h);
	h = hash_murmur3_one_32(uint32_t(p_rect.size.y), h);
	return hash_fmix32(h);
}

uint32_t HashMapHasherDefault::hash(const Rect2 &p_rect) {
	uint32_t h = hash_murmur3_one_real(p_rect.position.x);
	h = hash_murmur3_one_real(p_rect.position.y, h);
	h = hash_murmur3_one_real(p_rect.size.x, h);
	h = hash_murmur3_one_real(p_rect.size.y, h);
	return hash_fmix32(h);
}

uint32_t HashMapHasherDefault::hash(const AABB &p_aabb) {
	uint32_t h = hash_murmur3_one_real(p_aabb.position.x);
	h = hash_murmur3_one_real(p_aabb.position.y, h);
	h = hash_murmur3_one_real(p_aabb.position.z, h);
	h = hash_murmur3_one_real(p_aabb.size.x, h);
	h = hash_murmur3_one_real(p_aabb.size.y, h);
	h = hash_murmur3_one_real(p_aabb.size.z, h);
	return hash_fmix32(h);
}
