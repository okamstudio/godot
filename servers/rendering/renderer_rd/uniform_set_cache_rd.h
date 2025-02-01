/**************************************************************************/
/*  uniform_set_cache_rd.h                                                */
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

class UniformSetCacheRD : public Object {
	GDCLASS(UniformSetCacheRD, Object)

	struct Cache {
		Cache *prev = nullptr;
		Cache *next = nullptr;
		uint32_t hash = 0;
		RID shader;
		uint32_t set = 0;
		RID cache;
		LocalVector<RD::Uniform> uniforms;
	};

	PagedAllocator<Cache> cache_allocator;

	enum {
		HASH_TABLE_SIZE = 16381 // Prime
	};

	Cache *hash_table[HASH_TABLE_SIZE] = {};

	static _FORCE_INLINE_ uint32_t _hash_uniform(const RD::Uniform &u, uint32_t h) {
		h = hash_murmur3_one_32(u.uniform_type, h);
		h = hash_murmur3_one_32(u.binding, h);
		uint32_t rsize = u.get_id_count();
		for (uint32_t j = 0; j < rsize; j++) {
			h = hash_murmur3_one_64(u.get_id(j).get_id(), h);
		}
		return hash_fmix32(h);
	}

	static _FORCE_INLINE_ bool _compare_uniform(const RD::Uniform &a, const RD::Uniform &b) {
		if (a.binding != b.binding) {
			return false;
		}
		if (a.uniform_type != b.uniform_type) {
			return false;
		}
		uint32_t rsize = a.get_id_count();
		if (rsize != b.get_id_count()) {
			return false;
		}
		for (uint32_t j = 0; j < rsize; j++) {
			if (a.get_id(j) != b.get_id(j)) {
				return false;
			}
		}
		return true;
	}

	_FORCE_INLINE_ uint32_t _hash_args(uint32_t h, const RD::Uniform &arg) {
		return _hash_uniform(arg, h);
	}

	template <typename... Args>
	uint32_t _hash_args(uint32_t h, const RD::Uniform &arg, Args... args) {
		h = _hash_uniform(arg, h);
		return _hash_args(h, args...);
	}

	_FORCE_INLINE_ bool _compare_args(uint32_t idx, const LocalVector<RD::Uniform> &uniforms, const RD::Uniform &arg) {
		return _compare_uniform(uniforms[idx], arg);
	}

	template <typename... Args>
	_FORCE_INLINE_ bool _compare_args(uint32_t idx, const LocalVector<RD::Uniform> &uniforms, const RD::Uniform &arg, Args... args) {
		if (!_compare_uniform(uniforms[idx], arg)) {
			return false;
		}
		return _compare_args(idx + 1, uniforms, args...);
	}

	_FORCE_INLINE_ void _create_args(Vector<RD::Uniform> &uniforms, const RD::Uniform &arg) {
		uniforms.push_back(arg);
	}

	template <typename... Args>
	_FORCE_INLINE_ void _create_args(Vector<RD::Uniform> &uniforms, const RD::Uniform &arg, Args... args) {
		uniforms.push_back(arg);
		_create_args(uniforms, args...);
	}

	static UniformSetCacheRD *singleton;

	uint32_t cache_instances_used = 0;

	void _invalidate(Cache *p_cache);
	static void _uniform_set_invalidation_callback(void *p_userdata);

	template <typename Collection>
	RID _allocate_from_uniforms(RID p_shader, uint32_t p_set, uint32_t p_hash, uint32_t p_table_idx, const Collection &p_uniforms) {
		RID rid = RD::get_singleton()->uniform_set_create(p_uniforms, p_shader, p_set);
		ERR_FAIL_COND_V(rid.is_null(), rid);

		Cache *c = cache_allocator.alloc();
		c->hash = p_hash;
		c->set = p_set;
		c->shader = p_shader;
		c->cache = rid;
		c->uniforms.resize(p_uniforms.size());
		for (uint32_t i = 0; i < c->uniforms.size(); i++) {
			c->uniforms[i] = p_uniforms[i];
		}
		c->prev = nullptr;
		c->next = hash_table[p_table_idx];
		if (hash_table[p_table_idx]) {
			hash_table[p_table_idx]->prev = c;
		}
		hash_table[p_table_idx] = c;

		RD::get_singleton()->uniform_set_set_invalidation_callback(rid, _uniform_set_invalidation_callback, c);

		cache_instances_used++;

		return rid;
	}

private:
	static void _bind_methods();

public:
	template <typename... Args>
	RID get_cache(RID p_shader, uint32_t p_set, Args... args) {
		uint32_t h = hash_murmur3_one_64(p_shader.get_id());
		h = hash_murmur3_one_32(p_set, h);
		h = _hash_args(h, args...);

		uint32_t table_idx = h % HASH_TABLE_SIZE;
		{
			const Cache *c = hash_table[table_idx];

			while (c) {
				if (c->hash == h && c->set == p_set && c->shader == p_shader && sizeof...(Args) == c->uniforms.size() && _compare_args(0, c->uniforms, args...)) {
					return c->cache;
				}
				c = c->next;
			}
		}

		// Not in cache, create:

		Vector<RD::Uniform> uniforms;
		_create_args(uniforms, args...);

		return _allocate_from_uniforms(p_shader, p_set, h, table_idx, uniforms);
	}

	template <typename... Args>
	RID get_cache_vec(RID p_shader, uint32_t p_set, const LocalVector<RD::Uniform> &p_uniforms) {
		uint32_t h = hash_murmur3_one_64(p_shader.get_id());
		h = hash_murmur3_one_32(p_set, h);
		for (uint32_t i = 0; i < p_uniforms.size(); i++) {
			h = _hash_uniform(p_uniforms[i], h);
		}

		h = hash_fmix32(h);

		uint32_t table_idx = h % HASH_TABLE_SIZE;
		{
			const Cache *c = hash_table[table_idx];

			while (c) {
				if (c->hash == h && c->set == p_set && c->shader == p_shader && (uint32_t)p_uniforms.size() == c->uniforms.size()) {
					bool all_ok = true;
					for (uint32_t i = 0; i < p_uniforms.size(); i++) {
						if (!_compare_uniform(p_uniforms[i], c->uniforms[i])) {
							all_ok = false;
							break;
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
		return _allocate_from_uniforms(p_shader, p_set, h, table_idx, p_uniforms);
	}

	static RID get_cache_array(RID p_shader, uint32_t p_set, const TypedArray<RDUniform> &p_uniforms);

	static UniformSetCacheRD *get_singleton() { return singleton; }

	UniformSetCacheRD();
	~UniformSetCacheRD();
};
