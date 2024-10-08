/**************************************************************************/
/*  hashfuncs.h                                                           */
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

#ifndef HASHFUNCS_H
#define HASHFUNCS_H

#include "core/math/aabb.h"
#include "core/math/basis.h"
#include "core/math/color.h"
#include "core/math/math_defs.h"
#include "core/math/math_funcs.h"
#include "core/math/plane.h"
#include "core/math/projection.h"
#include "core/math/quaternion.h"
#include "core/math/rect2.h"
#include "core/math/rect2i.h"
#include "core/math/transform_2d.h"
#include "core/math/transform_3d.h"
#include "core/math/vector2.h"
#include "core/math/vector2i.h"
#include "core/math/vector3.h"
#include "core/math/vector3i.h"
#include "core/math/vector4.h"
#include "core/math/vector4i.h"
#include "core/object/object_id.h"
#include "core/string/node_path.h"
#include "core/string/string_name.h"
#include "core/string/ustring.h"
#include "core/templates/rid.h"
#include "core/typedefs.h"

/**
 * Hashing functions
 */

/**
 * DJB2 Hash function
 * @param C String
 * @return 32-bits hashcode
 */
static _FORCE_INLINE_ uint32_t hash_djb2(const char *p_cstr) {
	const unsigned char *chr = (const unsigned char *)p_cstr;
	uint32_t hash = 5381;
	uint32_t c = *chr++;

	while (c) {
		hash = ((hash << 5) + hash) ^ c; /* hash * 33 ^ c */
		c = *chr++;
	}

	return hash;
}

static _FORCE_INLINE_ uint32_t hash_djb2_buffer(const uint8_t *p_buff, int p_len, uint32_t p_prev = 5381) {
	uint32_t hash = p_prev;

	for (int i = 0; i < p_len; i++) {
		hash = ((hash << 5) + hash) ^ p_buff[i]; /* hash * 33 + c */
	}

	return hash;
}

static _FORCE_INLINE_ uint32_t hash_djb2_one_32(uint32_t p_in, uint32_t p_prev = 5381) {
	return ((p_prev << 5) + p_prev) ^ p_in;
}

/**
 * Thomas Wang's 64-bit to 32-bit Hash function:
 * https://web.archive.org/web/20071223173210/https:/www.concentric.net/~Ttwang/tech/inthash.htm
 *
 * @param p_int - 64-bit unsigned integer key to be hashed
 * @return unsigned 32-bit value representing hashcode
 */
static _FORCE_INLINE_ uint32_t hash_one_uint64(const uint64_t p_int) {
	uint64_t v = p_int;
	v = (~v) + (v << 18); // v = (v << 18) - v - 1;
	v = v ^ (v >> 31);
	v = v * 21; // v = (v + (v << 2)) + (v << 4);
	v = v ^ (v >> 11);
	v = v + (v << 6);
	v = v ^ (v >> 22);
	return uint32_t(v);
}

#define HASH_MURMUR3_SEED 0x7F07C65
// Murmurhash3 32-bit version.
// All MurmurHash versions are public domain software, and the author disclaims all copyright to their code.

static _FORCE_INLINE_ uint32_t hash_murmur3_one_32(uint32_t p_in, uint32_t p_seed = HASH_MURMUR3_SEED) {
	p_in *= 0xcc9e2d51;
	p_in = (p_in << 15) | (p_in >> 17);
	p_in *= 0x1b873593;

	p_seed ^= p_in;
	p_seed = (p_seed << 13) | (p_seed >> 19);
	p_seed = p_seed * 5 + 0xe6546b64;

	return p_seed;
}

static _FORCE_INLINE_ uint32_t hash_murmur3_one_float(float p_in, uint32_t p_seed = HASH_MURMUR3_SEED) {
	union {
		float f;
		uint32_t i;
	} u;

	// Normalize +/- 0.0 and NaN values so they hash the same.
	if (p_in == 0.0f) {
		u.f = 0.0;
	} else if (Math::is_nan(p_in)) {
		u.f = NAN;
	} else {
		u.f = p_in;
	}

	return hash_murmur3_one_32(u.i, p_seed);
}

static _FORCE_INLINE_ uint32_t hash_murmur3_one_64(uint64_t p_in, uint32_t p_seed = HASH_MURMUR3_SEED) {
	p_seed = hash_murmur3_one_32(p_in & 0xFFFFFFFF, p_seed);
	return hash_murmur3_one_32(p_in >> 32, p_seed);
}

static _FORCE_INLINE_ uint32_t hash_murmur3_one_double(double p_in, uint32_t p_seed = HASH_MURMUR3_SEED) {
	union {
		double d;
		uint64_t i;
	} u;

	// Normalize +/- 0.0 and NaN values so they hash the same.
	if (p_in == 0.0f) {
		u.d = 0.0;
	} else if (Math::is_nan(p_in)) {
		u.d = NAN;
	} else {
		u.d = p_in;
	}

	return hash_murmur3_one_64(u.i, p_seed);
}

static _FORCE_INLINE_ uint32_t hash_murmur3_one_real(real_t p_in, uint32_t p_seed = HASH_MURMUR3_SEED) {
#ifdef REAL_T_IS_DOUBLE
	return hash_murmur3_one_double(p_in, p_seed);
#else
	return hash_murmur3_one_float(p_in, p_seed);
#endif
}

static _FORCE_INLINE_ uint32_t hash_rotl32(uint32_t x, int8_t r) {
	return (x << r) | (x >> (32 - r));
}

static _FORCE_INLINE_ uint32_t hash_fmix32(uint32_t h) {
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}

static _FORCE_INLINE_ uint32_t hash_murmur3_buffer(const void *key, int length, const uint32_t seed = HASH_MURMUR3_SEED) {
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

static _FORCE_INLINE_ uint32_t hash_djb2_one_float(double p_in, uint32_t p_prev = 5381) {
	union {
		double d;
		uint64_t i;
	} u;

	// Normalize +/- 0.0 and NaN values so they hash the same.
	if (p_in == 0.0f) {
		u.d = 0.0;
	} else if (Math::is_nan(p_in)) {
		u.d = NAN;
	} else {
		u.d = p_in;
	}

	return ((p_prev << 5) + p_prev) + hash_one_uint64(u.i);
}

template <typename T>
static _FORCE_INLINE_ uint32_t hash_make_uint32_t(T p_in) {
	union {
		T t;
		uint32_t _u32;
	} _u;
	_u._u32 = 0;
	_u.t = p_in;
	return _u._u32;
}

static _FORCE_INLINE_ uint64_t hash_djb2_one_float_64(double p_in, uint64_t p_prev = 5381) {
	union {
		double d;
		uint64_t i;
	} u;

	// Normalize +/- 0.0 and NaN values so they hash the same.
	if (p_in == 0.0f) {
		u.d = 0.0;
	} else if (Math::is_nan(p_in)) {
		u.d = NAN;
	} else {
		u.d = p_in;
	}

	return ((p_prev << 5) + p_prev) + u.i;
}

static _FORCE_INLINE_ uint64_t hash_djb2_one_64(uint64_t p_in, uint64_t p_prev = 5381) {
	return ((p_prev << 5) + p_prev) ^ p_in;
}

template <typename T>
static _FORCE_INLINE_ uint64_t hash_make_uint64_t(T p_in) {
	union {
		T t;
		uint64_t _u64;
	} _u;
	_u._u64 = 0; // in case p_in is smaller

	_u.t = p_in;
	return _u._u64;
}

template <typename T>
class Ref;

struct HashMapHasherDefault {
	// Generic hash function for any type.
	template <typename T>
	static _FORCE_INLINE_ uint32_t hash(const T *p_pointer) { return hash_one_uint64((uint64_t)p_pointer); }

	template <typename T>
	static _FORCE_INLINE_ uint32_t hash(const Ref<T> &p_ref) { return hash_one_uint64((uint64_t)p_ref.operator->()); }

	static _FORCE_INLINE_ uint32_t hash(const String &p_string) { return p_string.hash(); }
	static _FORCE_INLINE_ uint32_t hash(const char *p_cstr) { return hash_djb2(p_cstr); }
	static _FORCE_INLINE_ uint32_t hash(const wchar_t p_wchar) { return hash_fmix32(p_wchar); }
	static _FORCE_INLINE_ uint32_t hash(const char16_t p_uchar) { return hash_fmix32(p_uchar); }
	static _FORCE_INLINE_ uint32_t hash(const char32_t p_uchar) { return hash_fmix32(p_uchar); }
	static _FORCE_INLINE_ uint32_t hash(const RID &p_rid) { return hash_one_uint64(p_rid.get_id()); }
	static _FORCE_INLINE_ uint32_t hash(const CharString &p_char_string) { return hash_djb2(p_char_string.get_data()); }
	static _FORCE_INLINE_ uint32_t hash(const StringName &p_string_name) { return p_string_name.hash(); }
	static _FORCE_INLINE_ uint32_t hash(const NodePath &p_path) { return p_path.hash(); }
	static _FORCE_INLINE_ uint32_t hash(const ObjectID &p_id) { return hash_one_uint64(p_id); }

	static _FORCE_INLINE_ uint32_t hash(const uint64_t p_int) { return hash_one_uint64(p_int); }
	static _FORCE_INLINE_ uint32_t hash(const int64_t p_int) { return hash_one_uint64(p_int); }
	static _FORCE_INLINE_ uint32_t hash(const float p_float) { return hash_murmur3_one_float(p_float); }
	static _FORCE_INLINE_ uint32_t hash(const double p_double) { return hash_murmur3_one_double(p_double); }
	static _FORCE_INLINE_ uint32_t hash(const uint32_t p_int) { return hash_fmix32(p_int); }
	static _FORCE_INLINE_ uint32_t hash(const int32_t p_int) { return hash_fmix32(p_int); }
	static _FORCE_INLINE_ uint32_t hash(const uint16_t p_int) { return hash_fmix32(p_int); }
	static _FORCE_INLINE_ uint32_t hash(const int16_t p_int) { return hash_fmix32(p_int); }
	static _FORCE_INLINE_ uint32_t hash(const uint8_t p_int) { return hash_fmix32(p_int); }
	static _FORCE_INLINE_ uint32_t hash(const int8_t p_int) { return hash_fmix32(p_int); }
	static _FORCE_INLINE_ uint32_t hash(const Vector2i &p_vec) {
		uint32_t h = hash_murmur3_one_32(p_vec.x);
		h = hash_murmur3_one_32(p_vec.y, h);
		return hash_fmix32(h);
	}
	static _FORCE_INLINE_ uint32_t hash(const Vector3i &p_vec) {
		uint32_t h = hash_murmur3_one_32(p_vec.x);
		h = hash_murmur3_one_32(p_vec.y, h);
		h = hash_murmur3_one_32(p_vec.z, h);
		return hash_fmix32(h);
	}
	static _FORCE_INLINE_ uint32_t hash(const Vector4i &p_vec) {
		uint32_t h = hash_murmur3_one_32(p_vec.x);
		h = hash_murmur3_one_32(p_vec.y, h);
		h = hash_murmur3_one_32(p_vec.z, h);
		h = hash_murmur3_one_32(p_vec.w, h);
		return hash_fmix32(h);
	}
	static _FORCE_INLINE_ uint32_t hash(const Vector2 &p_vec) {
		uint32_t h = hash_murmur3_one_real(p_vec.x);
		h = hash_murmur3_one_real(p_vec.y, h);
		return hash_fmix32(h);
	}
	static _FORCE_INLINE_ uint32_t hash(const Vector3 &p_vec) {
		uint32_t h = hash_murmur3_one_real(p_vec.x);
		h = hash_murmur3_one_real(p_vec.y, h);
		h = hash_murmur3_one_real(p_vec.z, h);
		return hash_fmix32(h);
	}
	static _FORCE_INLINE_ uint32_t hash(const Vector4 &p_vec) {
		uint32_t h = hash_murmur3_one_real(p_vec.x);
		h = hash_murmur3_one_real(p_vec.y, h);
		h = hash_murmur3_one_real(p_vec.z, h);
		h = hash_murmur3_one_real(p_vec.w, h);
		return hash_fmix32(h);
	}
	static _FORCE_INLINE_ uint32_t hash(const Rect2i &p_rect) {
		uint32_t h = hash_murmur3_one_32(p_rect.position.x);
		h = hash_murmur3_one_32(p_rect.position.y, h);
		h = hash_murmur3_one_32(p_rect.size.x, h);
		h = hash_murmur3_one_32(p_rect.size.y, h);
		return hash_fmix32(h);
	}
	static _FORCE_INLINE_ uint32_t hash(const Rect2 &p_rect) {
		uint32_t h = hash_murmur3_one_real(p_rect.position.x);
		h = hash_murmur3_one_real(p_rect.position.y, h);
		h = hash_murmur3_one_real(p_rect.size.x, h);
		h = hash_murmur3_one_real(p_rect.size.y, h);
		return hash_fmix32(h);
	}
	static _FORCE_INLINE_ uint32_t hash(const AABB &p_aabb) {
		uint32_t h = hash_murmur3_one_real(p_aabb.position.x);
		h = hash_murmur3_one_real(p_aabb.position.y, h);
		h = hash_murmur3_one_real(p_aabb.position.z, h);
		h = hash_murmur3_one_real(p_aabb.size.x, h);
		h = hash_murmur3_one_real(p_aabb.size.y, h);
		h = hash_murmur3_one_real(p_aabb.size.z, h);
		return hash_fmix32(h);
	}
};

// TODO: Fold this into HashMapHasherDefault once C++20 concepts are allowed
template <typename T>
struct HashableHasher {
	static _FORCE_INLINE_ uint32_t hash(const T &hashable) { return hashable.hash(); }
};

template <typename T>
struct HashMapComparatorDefault {
	static bool compare(const T &p_lhs, const T &p_rhs) {
		return p_lhs == p_rhs;
	}
};

template <>
struct HashMapComparatorDefault<float> {
	static bool compare(const float &p_lhs, const float &p_rhs) {
		return (p_lhs == p_rhs) || (Math::is_nan(p_lhs) && Math::is_nan(p_rhs));
	}
};

template <>
struct HashMapComparatorDefault<double> {
	static bool compare(const double &p_lhs, const double &p_rhs) {
		return (p_lhs == p_rhs) || (Math::is_nan(p_lhs) && Math::is_nan(p_rhs));
	}
};

template <>
struct HashMapComparatorDefault<Color> {
	static bool compare(const Color &p_lhs, const Color &p_rhs) {
		return ((p_lhs.r == p_rhs.r) || (Math::is_nan(p_lhs.r) && Math::is_nan(p_rhs.r))) && ((p_lhs.g == p_rhs.g) || (Math::is_nan(p_lhs.g) && Math::is_nan(p_rhs.g))) && ((p_lhs.b == p_rhs.b) || (Math::is_nan(p_lhs.b) && Math::is_nan(p_rhs.b))) && ((p_lhs.a == p_rhs.a) || (Math::is_nan(p_lhs.a) && Math::is_nan(p_rhs.a)));
	}
};

template <>
struct HashMapComparatorDefault<Vector2> {
	static bool compare(const Vector2 &p_lhs, const Vector2 &p_rhs) {
		return ((p_lhs.x == p_rhs.x) || (Math::is_nan(p_lhs.x) && Math::is_nan(p_rhs.x))) && ((p_lhs.y == p_rhs.y) || (Math::is_nan(p_lhs.y) && Math::is_nan(p_rhs.y)));
	}
};

template <>
struct HashMapComparatorDefault<Vector3> {
	static bool compare(const Vector3 &p_lhs, const Vector3 &p_rhs) {
		return ((p_lhs.x == p_rhs.x) || (Math::is_nan(p_lhs.x) && Math::is_nan(p_rhs.x))) && ((p_lhs.y == p_rhs.y) || (Math::is_nan(p_lhs.y) && Math::is_nan(p_rhs.y))) && ((p_lhs.z == p_rhs.z) || (Math::is_nan(p_lhs.z) && Math::is_nan(p_rhs.z)));
	}
};

template <>
struct HashMapComparatorDefault<Vector4> {
	static bool compare(const Vector4 &p_lhs, const Vector4 &p_rhs) {
		return ((p_lhs.x == p_rhs.x) || (Math::is_nan(p_lhs.x) && Math::is_nan(p_rhs.x))) && ((p_lhs.y == p_rhs.y) || (Math::is_nan(p_lhs.y) && Math::is_nan(p_rhs.y))) && ((p_lhs.z == p_rhs.z) || (Math::is_nan(p_lhs.z) && Math::is_nan(p_rhs.z))) && ((p_lhs.w == p_rhs.w) || (Math::is_nan(p_lhs.w) && Math::is_nan(p_rhs.w)));
	}
};

template <>
struct HashMapComparatorDefault<Rect2> {
	static bool compare(const Rect2 &p_lhs, const Rect2 &p_rhs) {
		return HashMapComparatorDefault<Vector2>().compare(p_lhs.position, p_rhs.position) && HashMapComparatorDefault<Vector2>().compare(p_lhs.size, p_rhs.size);
	}
};

template <>
struct HashMapComparatorDefault<AABB> {
	static bool compare(const AABB &p_lhs, const AABB &p_rhs) {
		return HashMapComparatorDefault<Vector3>().compare(p_lhs.position, p_rhs.position) && HashMapComparatorDefault<Vector3>().compare(p_lhs.size, p_rhs.size);
	}
};

template <>
struct HashMapComparatorDefault<Plane> {
	static bool compare(const Plane &p_lhs, const Plane &p_rhs) {
		return HashMapComparatorDefault<Vector3>().compare(p_lhs.normal, p_rhs.normal) && ((p_lhs.d == p_rhs.d) || (Math::is_nan(p_lhs.d) && Math::is_nan(p_rhs.d)));
	}
};

template <>
struct HashMapComparatorDefault<Transform2D> {
	static bool compare(const Transform2D &p_lhs, const Transform2D &p_rhs) {
		for (int i = 0; i < 3; ++i) {
			if (!HashMapComparatorDefault<Vector2>().compare(p_lhs.columns[i], p_rhs.columns[i])) {
				return false;
			}
		}

		return true;
	}
};

template <>
struct HashMapComparatorDefault<Basis> {
	static bool compare(const Basis &p_lhs, const Basis &p_rhs) {
		for (int i = 0; i < 3; ++i) {
			if (!HashMapComparatorDefault<Vector3>().compare(p_lhs.rows[i], p_rhs.rows[i])) {
				return false;
			}
		}

		return true;
	}
};

template <>
struct HashMapComparatorDefault<Transform3D> {
	static bool compare(const Transform3D &p_lhs, const Transform3D &p_rhs) {
		return HashMapComparatorDefault<Basis>().compare(p_lhs.basis, p_rhs.basis) && HashMapComparatorDefault<Vector3>().compare(p_lhs.origin, p_rhs.origin);
	}
};

template <>
struct HashMapComparatorDefault<Projection> {
	static bool compare(const Projection &p_lhs, const Projection &p_rhs) {
		for (int i = 0; i < 4; ++i) {
			if (!HashMapComparatorDefault<Vector4>().compare(p_lhs.columns[i], p_rhs.columns[i])) {
				return false;
			}
		}

		return true;
	}
};

template <>
struct HashMapComparatorDefault<Quaternion> {
	static bool compare(const Quaternion &p_lhs, const Quaternion &p_rhs) {
		return ((p_lhs.x == p_rhs.x) || (Math::is_nan(p_lhs.x) && Math::is_nan(p_rhs.x))) && ((p_lhs.y == p_rhs.y) || (Math::is_nan(p_lhs.y) && Math::is_nan(p_rhs.y))) && ((p_lhs.z == p_rhs.z) || (Math::is_nan(p_lhs.z) && Math::is_nan(p_rhs.z))) && ((p_lhs.w == p_rhs.w) || (Math::is_nan(p_lhs.w) && Math::is_nan(p_rhs.w)));
	}
};

constexpr uint32_t HASH_TABLE_SIZE_MAX = 29;

inline constexpr uint32_t hash_table_size_primes[HASH_TABLE_SIZE_MAX] = {
	5,
	13,
	23,
	47,
	97,
	193,
	389,
	769,
	1543,
	3079,
	6151,
	12289,
	24593,
	49157,
	98317,
	196613,
	393241,
	786433,
	1572869,
	3145739,
	6291469,
	12582917,
	25165843,
	50331653,
	100663319,
	201326611,
	402653189,
	805306457,
	1610612741,
};

// Computed with elem_i = UINT64_C (0 x FFFFFFFF FFFFFFFF ) / d_i + 1, where d_i is the i-th element of the above array.
inline constexpr uint64_t hash_table_size_primes_inv[HASH_TABLE_SIZE_MAX] = {
	3689348814741910324,
	1418980313362273202,
	802032351030850071,
	392483916461905354,
	190172619316593316,
	95578984837873325,
	47420935922132524,
	23987963684927896,
	11955116055547344,
	5991147799191151,
	2998982941588287,
	1501077717772769,
	750081082979285,
	375261795343686,
	187625172388393,
	93822606204624,
	46909513691883,
	23456218233098,
	11728086747027,
	5864041509391,
	2932024948977,
	1466014921160,
	733007198436,
	366503839517,
	183251896093,
	91625960335,
	45812983922,
	22906489714,
	11453246088
};

/**
 * Fastmod computes ( n mod d ) given the precomputed c much faster than n % d.
 * The implementation of fastmod is based on the following paper by Daniel Lemire et al.
 * Faster Remainder by Direct Computation: Applications to Compilers and Software Libraries
 * https://arxiv.org/abs/1902.01961
 */
static _FORCE_INLINE_ uint32_t fastmod(const uint32_t n, const uint64_t c, const uint32_t d) {
#if defined(_MSC_VER)
	// Returns the upper 64 bits of the product of two 64-bit unsigned integers.
	// This intrinsic function is required since MSVC does not support unsigned 128-bit integers.
#if defined(_M_X64) || defined(_M_ARM64)
	return __umulh(c * n, d);
#else
	// Fallback to the slower method for 32-bit platforms.
	return n % d;
#endif // _M_X64 || _M_ARM64
#else
#ifdef __SIZEOF_INT128__
	// Prevent compiler warning, because we know what we are doing.
	uint64_t lowbits = c * n;
	__extension__ typedef unsigned __int128 uint128;
	return static_cast<uint64_t>(((uint128)lowbits * d) >> 64);
#else
	// Fallback to the slower method if no 128-bit unsigned integer type is available.
	return n % d;
#endif // __SIZEOF_INT128__
#endif // _MSC_VER
}

#endif // HASHFUNCS_H
