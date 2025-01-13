/**************************************************************************/
/*  marshalls.h                                                           */
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

#ifndef MARSHALLS_H
#define MARSHALLS_H

#include "core/math/math_defs.h"
#include "core/object/ref_counted.h"
#include "core/typedefs.h"
#include "core/variant/variant.h"

// uintr_t is only for pairing with real_t, and we only need it in here.
#ifdef REAL_T_IS_DOUBLE
typedef uint64_t uintr_t;
#else
typedef uint32_t uintr_t;
#endif

/**
 * Miscellaneous helpers for marshaling data types, and encoding
 * in an endian independent way
 */

union MarshallFloat {
	uint32_t i; ///< int
	float f; ///< float
};

union MarshallDouble {
	uint64_t l; ///< long long
	double d; ///< double
};

// Behaves like one of the above, depending on compilation setting.
union MarshallReal {
	uintr_t i;
	real_t r;
};

static inline unsigned int encode_uint16(uint16_t p_uint, uint8_t *p_arr) {
	for (int i = 0; i < 2; i++) {
		*p_arr = p_uint & 0xFF;
		p_arr++;
		p_uint >>= 8;
	}

	return sizeof(uint16_t);
}

static inline unsigned int encode_uint32(uint32_t p_uint, uint8_t *p_arr) {
	for (int i = 0; i < 4; i++) {
		*p_arr = p_uint & 0xFF;
		p_arr++;
		p_uint >>= 8;
	}

	return sizeof(uint32_t);
}

static inline unsigned int encode_half(float p_float, uint8_t *p_arr) {
	encode_uint16(Math::make_half_float(p_float), p_arr);

	return sizeof(uint16_t);
}

static inline unsigned int encode_float(float p_float, uint8_t *p_arr) {
	MarshallFloat mf;
	mf.f = p_float;
	encode_uint32(mf.i, p_arr);

	return sizeof(uint32_t);
}

static inline unsigned int encode_uint64(uint64_t p_uint, uint8_t *p_arr) {
	for (int i = 0; i < 8; i++) {
		*p_arr = p_uint & 0xFF;
		p_arr++;
		p_uint >>= 8;
	}

	return sizeof(uint64_t);
}

static inline unsigned int encode_double(double p_double, uint8_t *p_arr) {
	MarshallDouble md;
	md.d = p_double;
	encode_uint64(md.l, p_arr);

	return sizeof(uint64_t);
}

static inline unsigned int encode_uintr(uintr_t p_uint, uint8_t *p_arr) {
	for (size_t i = 0; i < sizeof(uintr_t); i++) {
		*p_arr = p_uint & 0xFF;
		p_arr++;
		p_uint >>= 8;
	}

	return sizeof(uintr_t);
}

static inline unsigned int encode_real(real_t p_real, uint8_t *p_arr) {
	MarshallReal mr;
	mr.r = p_real;
	encode_uintr(mr.i, p_arr);

	return sizeof(uintr_t);
}

static inline int encode_cstring(const char *p_string, uint8_t *p_data) {
	int len = 0;

	while (*p_string) {
		if (p_data) {
			*p_data = (uint8_t)*p_string;
			p_data++;
		}
		p_string++;
		len++;
	}

	if (p_data) {
		*p_data = 0;
	}
	return len + 1;
}

static inline uint16_t decode_uint16(const uint8_t *p_arr) {
	uint16_t u = 0;

	for (int i = 0; i < 2; i++) {
		uint16_t b = *p_arr;
		b <<= (i * 8);
		u |= b;
		p_arr++;
	}

	return u;
}

static inline uint32_t decode_uint32(const uint8_t *p_arr) {
	uint32_t u = 0;

	for (int i = 0; i < 4; i++) {
		uint32_t b = *p_arr;
		b <<= (i * 8);
		u |= b;
		p_arr++;
	}

	return u;
}

static inline float decode_half(const uint8_t *p_arr) {
	return Math::half_to_float(decode_uint16(p_arr));
}

static inline float decode_float(const uint8_t *p_arr) {
	MarshallFloat mf;
	mf.i = decode_uint32(p_arr);
	return mf.f;
}

static inline uint64_t decode_uint64(const uint8_t *p_arr) {
	uint64_t u = 0;

	for (int i = 0; i < 8; i++) {
		uint64_t b = (*p_arr) & 0xFF;
		b <<= (i * 8);
		u |= b;
		p_arr++;
	}

	return u;
}

static inline double decode_double(const uint8_t *p_arr) {
	MarshallDouble md;
	md.l = decode_uint64(p_arr);
	return md.d;
}

class EncodedObjectAsID : public RefCounted {
	GDCLASS(EncodedObjectAsID, RefCounted);

	ObjectID id;

protected:
	static void _bind_methods();

public:
	void set_object_id(ObjectID p_id);
	ObjectID get_object_id() const;

	EncodedObjectAsID() {}
};

Error decode_variant(Variant &r_variant, const uint8_t *p_buffer, int p_len, int *r_len = nullptr, bool p_allow_objects = false, int p_depth = 0);
Error encode_variant(const Variant &p_variant, uint8_t *r_buffer, int &r_len, bool p_full_objects = false, int p_depth = 0);

Vector<float> vector3_to_float32_array(const Vector3 *vecs, size_t count);

#endif // MARSHALLS_H
