/**************************************************************************/
/*  framebuffer_cache_rd.h                                                */
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

#include "core/templates/local_vector.h"
#include "core/templates/paged_allocator.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_device_binds.h"

class FramebufferCacheRD : public Object {
	GDCLASS(FramebufferCacheRD, Object)

	struct Cache {
		Cache *prev = nullptr;
		Cache *next = nullptr;
		uint32_t hash = 0;
		RID cache;
		LocalVector<RID> textures;
		LocalVector<RD::FramebufferPass> passes;
		uint32_t views = 0;
	};

	PagedAllocator<Cache> cache_allocator;

	enum {
		HASH_TABLE_SIZE = 16381 // Prime
	};

	Cache *hash_table[HASH_TABLE_SIZE] = {};

	static _FORCE_INLINE_ uint32_t _hash_pass(const RD::FramebufferPass &p, uint32_t h) {
		h = hash_murmur3_one_32(p.depth_attachment, h);
		h = hash_murmur3_one_32(p.vrs_attachment, h);

		h = hash_murmur3_one_32(p.color_attachments.size(), h);
		for (int i = 0; i < p.color_attachments.size(); i++) {
			h = hash_murmur3_one_32(p.color_attachments[i], h);
		}

		h = hash_murmur3_one_32(p.resolve_attachments.size(), h);
		for (int i = 0; i < p.resolve_attachments.size(); i++) {
			h = hash_murmur3_one_32(p.resolve_attachments[i], h);
		}

		h = hash_murmur3_one_32(p.preserve_attachments.size(), h);
		for (int i = 0; i < p.preserve_attachments.size(); i++) {
			h = hash_murmur3_one_32(p.preserve_attachments[i], h);
		}

		return h;
	}

	static _FORCE_INLINE_ bool _compare_pass(const RD::FramebufferPass &a, const RD::FramebufferPass &b) {
		if (a.depth_attachment != b.depth_attachment) {
			return false;
		}

		if (a.vrs_attachment != b.vrs_attachment) {
			return false;
		}

		if (a.color_attachments.size() != b.color_attachments.size()) {
			return false;
		}

		for (int i = 0; i < a.color_attachments.size(); i++) {
			if (a.color_attachments[i] != b.color_attachments[i]) {
				return false;
			}
		}

		if (a.resolve_attachments.size() != b.resolve_attachments.size()) {
			return false;
		}

		for (int i = 0; i < a.resolve_attachments.size(); i++) {
			if (a.resolve_attachments[i] != b.resolve_attachments[i]) {
				return false;
			}
		}

		if (a.preserve_attachments.size() != b.preserve_attachments.size()) {
			return false;
		}

		for (int i = 0; i < a.preserve_attachments.size(); i++) {
			if (a.preserve_attachments[i] != b.preserve_attachments[i]) {
				return false;
			}
		}

		return true;
	}

	_FORCE_INLINE_ uint32_t _hash_rids(uint32_t h, const RID &arg) {
		return hash_murmur3_one_64(arg.get_id(), h);
	}

	template <typename... Args>
	uint32_t _hash_rids(uint32_t h, const RID &arg, Args... args) {
		h = hash_murmur3_one_64(arg.get_id(), h);
		return _hash_rids(h, args...);
	}

	_FORCE_INLINE_ bool _compare_args(uint32_t idx, const LocalVector<RID> &textures, const RID &arg) {
		return textures[idx] == arg;
	}

	template <typename... Args>
	_FORCE_INLINE_ bool _compare_args(uint32_t idx, const LocalVector<RID> &textures, const RID &arg, Args... args) {
		if (textures[idx] != arg) {
			return false;
		}
		return _compare_args(idx + 1, textures, args...);
	}

	_FORCE_INLINE_ void _create_args(Vector<RID> &textures, const RID &arg) {
		textures.push_back(arg);
	}

	template <typename... Args>
	_FORCE_INLINE_ void _create_args(Vector<RID> &textures, const RID &arg, Args... args) {
		textures.push_back(arg);
		_create_args(textures, args...);
	}

	static FramebufferCacheRD *singleton;

	uint32_t cache_instances_used = 0;

	void _invalidate(Cache *p_cache);
	static void _framebuffer_invalidation_callback(void *p_userdata);

	RID _allocate_from_data(uint32_t p_views, uint32_t p_hash, uint32_t p_table_idx, const Vector<RID> &p_textures, const Vector<RD::FramebufferPass> &p_passes) {
		RID rid;
		if (p_passes.size()) {
			rid = RD::get_singleton()->framebuffer_create_multipass(p_textures, p_passes, RD::INVALID_ID, p_views);
		} else {
			rid = RD::get_singleton()->framebuffer_create(p_textures, RD::INVALID_ID, p_views);
		}

		ERR_FAIL_COND_V(rid.is_null(), rid);

		Cache *c = cache_allocator.alloc();
		c->views = p_views;
		c->cache = rid;
		c->hash = p_hash;
		c->textures.resize(p_textures.size());
		for (uint32_t i = 0; i < c->textures.size(); i++) {
			c->textures[i] = p_textures[i];
		}
		c->passes.resize(p_passes.size());
		for (uint32_t i = 0; i < c->passes.size(); i++) {
			c->passes[i] = p_passes[i];
		}
		c->prev = nullptr;
		c->next = hash_table[p_table_idx];
		if (hash_table[p_table_idx]) {
			hash_table[p_table_idx]->prev = c;
		}
		hash_table[p_table_idx] = c;

		RD::get_singleton()->framebuffer_set_invalidation_callback(rid, _framebuffer_invalidation_callback, c);

		cache_instances_used++;

		return rid;
	}

private:
	static void _bind_methods();

public:
	template <typename... Args>
	RID get_cache(Args... args) {
		uint32_t h = hash_murmur3_one_32(1); //1 view
		h = hash_murmur3_one_32(sizeof...(Args), h);
		h = _hash_rids(h, args...);
		h = hash_murmur3_one_32(0, h); // 0 passes
		h = hash_fmix32(h);

		uint32_t table_idx = h % HASH_TABLE_SIZE;
		{
			const Cache *c = hash_table[table_idx];

			while (c) {
				if (c->hash == h && c->passes.size() == 0 && c->textures.size() == sizeof...(Args) && c->views == 1 && _compare_args(0, c->textures, args...)) {
					return c->cache;
				}
				c = c->next;
			}
		}

		// Not in cache, create:

		Vector<RID> textures;
		_create_args(textures, args...);

		return _allocate_from_data(1, h, table_idx, textures, Vector<RD::FramebufferPass>());
	}

	template <typename... Args>
	RID get_cache_multiview(uint32_t p_views, Args... args) {
		uint32_t h = hash_murmur3_one_32(p_views);
		h = hash_murmur3_one_32(sizeof...(Args), h);
		h = _hash_rids(h, args...);
		h = hash_murmur3_one_32(0, h); // 0 passes
		h = hash_fmix32(h);

		uint32_t table_idx = h % HASH_TABLE_SIZE;
		{
			const Cache *c = hash_table[table_idx];

			while (c) {
				if (c->hash == h && c->passes.size() == 0 && c->textures.size() == sizeof...(Args) && c->views == p_views && _compare_args(0, c->textures, args...)) {
					return c->cache;
				}
				c = c->next;
			}
		}

		// Not in cache, create:

		Vector<RID> textures;
		_create_args(textures, args...);

		return _allocate_from_data(p_views, h, table_idx, textures, Vector<RD::FramebufferPass>());
	}

	RID get_cache_multipass(const Vector<RID> &p_textures, const Vector<RD::FramebufferPass> &p_passes, uint32_t p_views = 1) {
		uint32_t h = hash_murmur3_one_32(p_views);
		h = hash_murmur3_one_32(p_textures.size(), h);
		for (int i = 0; i < p_textures.size(); i++) {
			h = hash_murmur3_one_64(p_textures[i].get_id(), h);
		}
		h = hash_murmur3_one_32(p_passes.size(), h);
		for (int i = 0; i < p_passes.size(); i++) {
			h = _hash_pass(p_passes[i], h);
		}

		h = hash_fmix32(h);

		uint32_t table_idx = h % HASH_TABLE_SIZE;
		{
			const Cache *c = hash_table[table_idx];

			while (c) {
				if (c->hash == h && c->views == p_views && c->textures.size() == (uint32_t)p_textures.size() && c->passes.size() == (uint32_t)p_passes.size()) {
					bool all_ok = true;

					for (int i = 0; i < p_textures.size(); i++) {
						if (p_textures[i] != c->textures[i]) {
							all_ok = false;
							break;
						}
					}

					if (all_ok) {
						for (int i = 0; i < p_passes.size(); i++) {
							if (!_compare_pass(p_passes[i], c->passes[i])) {
								all_ok = false;
								break;
							}
						}
					}

					if (all_ok) {
						return c->cache;
					}
				}
				c = c->next;
			}
		}

		// Not in cache, create:
		return _allocate_from_data(p_views, h, table_idx, p_textures, p_passes);
	}

	static RID get_cache_multipass_array(const TypedArray<RID> &p_textures, const TypedArray<RDFramebufferPass> &p_passes, uint32_t p_views = 1);

	static FramebufferCacheRD *get_singleton() { return singleton; }

	FramebufferCacheRD();
	~FramebufferCacheRD();
};
