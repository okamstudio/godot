/**************************************************************************/
/*  jolt_globals.cpp                                                      */
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

#include "jolt_globals.h"

#include "objects/jolt_group_filter.h"
#include "shapes/jolt_custom_double_sided_shape.h"
#include "shapes/jolt_custom_ray_shape.h"
#include "shapes/jolt_custom_user_data_shape.h"

#include "core/os/memory.h"
#include "core/string/print_string.h"

#include "Jolt/Jolt.h"

#include "Jolt/RegisterTypes.h"

#include <stdarg.h>

void *jolt_alloc(size_t p_size) {
	return Memory::alloc_static(p_size);
}

void *jolt_realloc(void *p_mem, size_t p_old_size, size_t p_new_size) {
	return Memory::realloc_static(p_mem, p_new_size);
}

void jolt_free(void *p_mem) {
	Memory::free_static(p_mem);
}

void *jolt_aligned_alloc(size_t p_size, size_t p_alignment) {
	return Memory::alloc_aligned_static(p_size, p_alignment);
}

void jolt_aligned_free(void *p_mem) {
	Memory::free_aligned_static(p_mem);
}

#ifdef JPH_ENABLE_ASSERTS

void jolt_trace(const char *p_format, ...) {
	va_list args;
	va_start(args, p_format);
	char buffer[1024] = { '\0' };
	vsnprintf(buffer, sizeof(buffer), p_format, args);
	va_end(args);
	print_verbose(buffer);
}

bool jolt_assert(const char *p_expr, const char *p_msg, const char *p_file, uint32_t p_line) {
	ERR_PRINT(vformat("Jolt Physics assertion '%s' failed with message '%s' at '%s:%d'", p_expr, p_msg != nullptr ? p_msg : "", p_file, p_line));
	return false;
}

#endif

void jolt_initialize() {
	JPH::Allocate = &jolt_alloc;
	JPH::Reallocate = &jolt_realloc;
	JPH::Free = &jolt_free;
	JPH::AlignedAllocate = &jolt_aligned_alloc;
	JPH::AlignedFree = &jolt_aligned_free;

#ifdef JPH_ENABLE_ASSERTS
	JPH::Trace = &jolt_trace;
	JPH::AssertFailed = &jolt_assert;
#endif

	JPH::Factory::sInstance = new JPH::Factory();

	JPH::RegisterTypes();

	JoltCustomRayShape::register_type();
	JoltCustomUserDataShape::register_type();
	JoltCustomDoubleSidedShape::register_type();

	JoltGroupFilter::instance = new JoltGroupFilter();
	JoltGroupFilter::instance->SetEmbedded();
}

void jolt_deinitialize() {
	if (JoltGroupFilter::instance != nullptr) {
		delete JoltGroupFilter::instance;
		JoltGroupFilter::instance = nullptr;
	}

	JPH::UnregisterTypes();

	if (JPH::Factory::sInstance != nullptr) {
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;
	}
}
