/**************************************************************************/
/*  framebuffer_cache_rd.cpp                                              */
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

#include "framebuffer_cache_rd.h"

FramebufferCacheRD *FramebufferCacheRD::singleton = nullptr;

void FramebufferCacheRD::_bind_methods() {
	ClassDB::bind_static_method("FramebufferCacheRD", D_METHOD("get_cache_multipass", "textures", "passes", "views"), &FramebufferCacheRD::get_cache_multipass_array);
}

void FramebufferCacheRD::_invalidate(Cache *p_cache) {
	if (p_cache->prev) {
		p_cache->prev->next = p_cache->next;
	} else {
		// At beginning of table
		uint32_t table_idx = p_cache->hash % HASH_TABLE_SIZE;
		hash_table[table_idx] = p_cache->next;
	}

	if (p_cache->next) {
		p_cache->next->prev = p_cache->prev;
	}

	cache_allocator.free(p_cache);
	cache_instances_used--;
}
void FramebufferCacheRD::_framebuffer_invalidation_callback(void *p_userdata) {
	singleton->_invalidate(reinterpret_cast<Cache *>(p_userdata));
}

RID FramebufferCacheRD::get_cache_multipass_array(const TypedArray<RID> &p_textures, const TypedArray<RDFramebufferPass> &p_passes, uint32_t p_views) {
	ERR_FAIL_NULL_V(RD::get_singleton(), RID());

	Vector<RID> textures;
	Vector<RD::FramebufferPass> passes;

	for (int i = 0; i < p_textures.size(); i++) {
		RID texture = p_textures[i];
		textures.push_back(texture); // store even if NULL
	}

	for (int i = 0; i < p_passes.size(); i++) {
		Ref<RDFramebufferPass> pass = p_passes[i];
		if (pass.is_valid()) {
			passes.push_back(pass->base);
		}
	}

	return FramebufferCacheRD::get_singleton()->get_cache_multipass(textures, passes, p_views);
}

FramebufferCacheRD::FramebufferCacheRD() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = this;
}

FramebufferCacheRD::~FramebufferCacheRD() {
	if (cache_instances_used > 0) {
		ERR_PRINT("At exit: " + itos(cache_instances_used) + " framebuffer cache instance(s) still in use.");
	}
}
