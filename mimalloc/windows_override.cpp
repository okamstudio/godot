/**************************************************************************/
/*  windows_override.cpp                                                  */
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

#include "thirdparty/mimalloc/include/mimalloc.h"

extern "C" {

#ifdef _MSC_VER
// Dummy symbol to ensure linkage.
void mimalloc_godot_force_override() {}
#endif

// ---

// Since the mechanism for overriding the C memory functions on Windows
// that mimalloc provides only works when building as a DLL, we have to
// provide these manual overrides and hope they cover everything.

// This is based in mimallloc's mimalloc-override.h.
// Differences:
// - Instead of macros, they are actual functions.
// - __declspec(restrict) is added if the underlying mimalloc function has it.
// - A few functions seem not to be overridable (duplicate symbol link error) and so are disabled,
//   for MSVC or MinGW, according to which complains about which function.

#include <stddef.h> // for size_t
#include <wchar.h> // for wchar_t

__declspec(restrict) void *malloc(size_t n) {
	return mi_malloc(n);
}

__declspec(restrict) void *calloc(size_t n, size_t c) {
	return mi_calloc(n, c);
}

__declspec(restrict) void *realloc(void *p, size_t n) {
	return mi_realloc(p, n);
}

void free(void *p) {
	mi_free(p);
}

__declspec(restrict) char *strdup(const char *s) {
	return mi_strdup(s);
}

__declspec(restrict) char *strndup(const char *s, size_t n) {
	return mi_strndup(s, n);
}

__declspec(restrict) char *realpath(const char *f, char *n) {
	return mi_realpath(f, n);
}

void *_expand(void *p, size_t n) {
	return mi_expand(p, n);
}

#if !defined(_MSC_VER)
size_t _msize(void *p) {
	return mi_usable_size(p);
}

void *_recalloc(void *p, size_t n, size_t c) {
	return mi_recalloc(p, n, c);
}
#endif

#if defined(_MSC_VER)
__declspec(restrict) char *_strdup(const char *s) {
	return mi_strdup(s);
}
#endif

__declspec(restrict) char *_strndup(const char *s, size_t n) {
	return mi_strndup(s, n);
}

__declspec(restrict) wchar_t *_wcsdup(const wchar_t *s) {
	return (wchar_t *)mi_wcsdup((const unsigned short *)s);
}

__declspec(restrict) unsigned char *_mbsdup(const unsigned char *s) {
	return mi_mbsdup(s);
}

#if !defined(_MSC_VER)
errno_t _dupenv_s(char **b, size_t *n, const char *v) {
	return (errno_t)mi_dupenv_s(b, n, v);
}

errno_t _wdupenv_s(wchar_t **b, size_t *n, const wchar_t *v) {
	return (errno_t)mi_wdupenv_s((unsigned short **)b, n, (const unsigned short *)v);
}
#endif

void *reallocf(void *p, size_t n) {
	return mi_reallocf(p, n);
}

size_t malloc_size(void *p) {
	return mi_usable_size(p);
}

size_t malloc_usable_size(void *p) {
	return mi_usable_size(p);
}

size_t malloc_good_size(size_t sz) {
	return mi_malloc_good_size(sz);
}

void cfree(void *p) {
	mi_free(p);
}

__declspec(restrict) void *valloc(size_t n) {
	return mi_valloc(n);
}

__declspec(restrict) void *pvalloc(size_t n) {
	return mi_pvalloc(n);
}

void *reallocarray(void *p, size_t s, size_t n) {
	return mi_reallocarray(p, s, n);
}

int reallocarr(void *p, size_t s, size_t n) {
	return mi_reallocarr(p, s, n);
}

__declspec(restrict) void *memalign(size_t a, size_t n) {
	return mi_memalign(a, n);
}

__declspec(restrict) void *aligned_alloc(size_t a, size_t n) {
	return mi_aligned_alloc(a, n);
}

int posix_memalign(void **p, size_t a, size_t n) {
	return mi_posix_memalign(p, a, n);
}

int _posix_memalign(void **p, size_t a, size_t n) {
	return mi_posix_memalign(p, a, n);
}

#if defined(_MSC_VER)
__declspec(restrict) void *_aligned_malloc(size_t n, size_t a) {
	return mi_malloc_aligned(n, a);
}
#endif

__declspec(restrict) void *_aligned_realloc(void *p, size_t n, size_t a) {
	return mi_realloc_aligned(p, n, a);
}

void *_aligned_recalloc(void *p, size_t s, size_t n, size_t a) {
	return mi_aligned_recalloc(p, s, n, a);
}

size_t _aligned_msize(void *p, size_t a, size_t o) {
	return mi_usable_size(p);
}

#if defined(_MSC_VER)
void _aligned_free(void *p) {
	mi_free(p);
}
#endif

__declspec(restrict) void *_aligned_offset_malloc(size_t n, size_t a, size_t o) {
	return mi_malloc_aligned_at(n, a, o);
}

void *_aligned_offset_realloc(void *p, size_t n, size_t a, size_t o) {
	return mi_realloc_aligned_at(p, n, a, o);
}

void *_aligned_offset_recalloc(void *p, size_t s, size_t n, size_t a, size_t o) {
	return mi_recalloc_aligned_at(p, s, n, a, o);
}

} // extern "C"

// ---

// At least, mimalloc provides replacements for C++'s new, etc.,
// in this header, that has to be included in a single translation unit.
#include "thirdparty/mimalloc/include/mimalloc-new-delete.h"
