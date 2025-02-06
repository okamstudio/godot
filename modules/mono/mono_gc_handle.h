/**************************************************************************/
/*  mono_gc_handle.h                                                      */
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

#include "core/object/ref_counted.h"

namespace gdmono {

enum class GCHandleType : char {
	NIL,
	STRONG_HANDLE,
	WEAK_HANDLE
};
}

extern "C" {
struct GCHandleIntPtr {
	void *value;

	_FORCE_INLINE_ bool operator==(const GCHandleIntPtr &p_other) { return value == p_other.value; }
	_FORCE_INLINE_ bool operator!=(const GCHandleIntPtr &p_other) { return value != p_other.value; }

	GCHandleIntPtr() = delete;
};
}

static_assert(sizeof(GCHandleIntPtr) == sizeof(void *));

// Manual release of the GC handle must be done when using this struct
struct MonoGCHandleData {
	GCHandleIntPtr handle = { nullptr };
	gdmono::GCHandleType type = gdmono::GCHandleType::NIL;

	_FORCE_INLINE_ bool is_released() const { return !handle.value; }
	_FORCE_INLINE_ bool is_weak() const { return type == gdmono::GCHandleType::WEAK_HANDLE; }
	_FORCE_INLINE_ GCHandleIntPtr get_intptr() const { return handle; }

	void release();

	static void free_gchandle(GCHandleIntPtr p_gchandle);

	void operator=(const MonoGCHandleData &p_other) {
#ifdef DEBUG_ENABLED
		CRASH_COND(!is_released());
#endif
		handle = p_other.handle;
		type = p_other.type;
	}

	MonoGCHandleData(const MonoGCHandleData &) = default;

	MonoGCHandleData() {}

	MonoGCHandleData(GCHandleIntPtr p_handle, gdmono::GCHandleType p_type) :
			handle(p_handle),
			type(p_type) {
	}
};
