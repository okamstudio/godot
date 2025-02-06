/**************************************************************************/
/*  mingw_custom_tls_slot.cpp                                             */
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

#include "core/error/error_macros.h"
#include "core/typedefs.h"
#include "mimalloc/custom_tls_slot.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static DWORD tls_slot = -1;

extern "C" {
void *mi_prim_tls_slot(size_t slot) {
	DEV_ASSERT(slot == MI_TLS_SLOT);
	if (unlikely(tls_slot == -1)) {
		return nullptr;
	} else {
		return TlsGetValue(tls_slot);
	}
}

void mi_prim_tls_slot_set(size_t slot, void *value) {
	DEV_ASSERT(slot == MI_TLS_SLOT);
	if (unlikely(tls_slot == -1)) {
		tls_slot = TlsAlloc();
		CRASH_COND(tls_slot == TLS_OUT_OF_INDEXES);
	}
	TlsSetValue(tls_slot, value);
}
}
