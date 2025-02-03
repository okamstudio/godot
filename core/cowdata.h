/**************************************************************************/
/*  cowdata.h                                                             */
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

#ifndef COWDATA_H
#define COWDATA_H

#include <string.h>
#include <type_traits>
#include <utility>

#include "core/error_macros.h"
#include "core/os/memory.h"
#include "core/safe_refcount.h"

template <class T>
class Vector;
class String;
class CharString;
template <class T, class V>
class VMap;

#if !defined(NO_THREADS)
SAFE_NUMERIC_TYPE_PUN_GUARANTEES(uint32_t)
#endif

template <class T>
class CowData {
	template <class TV>
	friend class Vector;
	friend class String;
	friend class CharString;
	template <class TV, class VV>
	friend class VMap;

private:
	mutable T *_ptr;

	// internal helpers

	_FORCE_INLINE_ SafeNumeric<uint32_t> *_get_refcount() const {
		if (!_ptr) {
			return nullptr;
		}

		return reinterpret_cast<SafeNumeric<uint32_t> *>(_ptr) - 2;
	}

	_FORCE_INLINE_ uint32_t *_get_size() const {
		if (!_ptr) {
			return nullptr;
		}

		return reinterpret_cast<uint32_t *>(_ptr) - 1;
	}

	_FORCE_INLINE_ size_t _get_alloc_size(size_t p_elements) const {
		//return nearest_power_of_2_templated(p_elements*sizeof(T)+sizeof(SafeRefCount)+sizeof(int));
		return next_power_of_2(p_elements * sizeof(T));
	}

	_FORCE_INLINE_ bool _get_alloc_size_checked(size_t p_elements, size_t *out) const {
#if defined(_add_overflow) && defined(_mul_overflow)
		size_t o;
		size_t p;
		if (_mul_overflow(p_elements, sizeof(T), &o)) {
			*out = 0;
			return false;
		}
		*out = next_power_of_2(o);
		if (_add_overflow(o, static_cast<size_t>(32), &p)) {
			return false; //no longer allocated here
		}
		return true;
#else
		// Speed is more important than correctness here, do the operations unchecked
		// and hope the best
		*out = _get_alloc_size(p_elements);
		return true;
#endif
	}

	void _unref(void *p_data);
	void _ref(const CowData *p_from);
	void _ref(const CowData &p_from);
	uint32_t _copy_on_write();

public:
	void operator=(const CowData<T> &p_from) { _ref(p_from); }

	_FORCE_INLINE_ T *ptrw() {
		_copy_on_write();
		return _ptr;
	}

	_FORCE_INLINE_ const T *ptr() const {
		return _ptr;
	}

	_FORCE_INLINE_ int size() const {
		uint32_t *size = (uint32_t *)_get_size();
		if (size) {
			return *size;
		} else {
			return 0;
		}
	}

	_FORCE_INLINE_ void clear() { resize(0); }
	_FORCE_INLINE_ bool empty() const { return _ptr == nullptr; }

	_FORCE_INLINE_ void set(int p_index, const T &p_elem) {
		CRASH_BAD_INDEX(p_index, size());
		_copy_on_write();
		_ptr[p_index] = p_elem;
	}

	_FORCE_INLINE_ T &get_m(int p_index) {
		CRASH_BAD_INDEX(p_index, size());
		_copy_on_write();
		return _ptr[p_index];
	}

	_FORCE_INLINE_ const T &get(int p_index) const {
		CRASH_BAD_INDEX(p_index, size());

		return _ptr[p_index];
	}

	Error resize(int p_size);

	_FORCE_INLINE_ void remove(int p_index) {
		ERR_FAIL_INDEX(p_index, size());
		T *p = ptrw();
		int len = size();
		for (int i = p_index; i < len - 1; i++) {
			p[i] = std::move(p[i + 1]);
		};

		resize(len - 1);
	}

	Error insert(int p_pos, const T &p_val) {
		int new_size = size() + 1;
		ERR_FAIL_INDEX_V(p_pos, new_size, ERR_INVALID_PARAMETER);
		Error err = resize(new_size);
		ERR_FAIL_COND_V(err, err);

		T *p = ptrw();
		for (int i = new_size - 1; i > p_pos; i--) {
			p[i] = std::move(p[i - 1]);
		}
		p[p_pos] = p_val;

		return OK;
	}

	int find(const T &p_val, int p_from = 0) const;

	_FORCE_INLINE_ CowData();
	_FORCE_INLINE_ ~CowData();
	_FORCE_INLINE_ CowData(CowData<T> &p_from) { _ref(p_from); }
};

template <class T>
void CowData<T>::_unref(void *p_data) {
	if (!p_data) {
		return;
	}

	SafeNumeric<uint32_t> *refc = _get_refcount();

	if (refc->decrement() > 0) {
		return; // still in use
	}
	// clean up

	if (!std::is_trivially_destructible<T>::value) {
		uint32_t *count = _get_size();
		T *data = (T *)(count + 1);

		for (uint32_t i = 0; i < *count; ++i) {
			// call destructors
			data[i].~T();
		}
	}

	// free mem
	Memory::free_static((uint8_t *)p_data, true);
}

template <class T>
uint32_t CowData<T>::_copy_on_write() {
	if (!_ptr) {
		return 0;
	}

	SafeNumeric<uint32_t> *refc = _get_refcount();

	uint32_t rc = refc->get();
	if (likely(rc > 1)) {
		/* in use by more than me */
		uint32_t current_size = *_get_size();

		uint32_t *mem_new = (uint32_t *)Memory::alloc_static(_get_alloc_size(current_size), true);

		new (mem_new - 2, sizeof(uint32_t), "") SafeNumeric<uint32_t>(1); //refcount
		*(mem_new - 1) = current_size; //size

		T *_data = (T *)(mem_new);

		// initialize new elements
		if (std::is_trivially_copyable<T>::value) {
			memcpy(mem_new, _ptr, current_size * sizeof(T));

		} else {
			for (uint32_t i = 0; i < current_size; i++) {
				memnew_placement(&_data[i], T(_ptr[i]));
			}
		}

		_unref(_ptr);
		_ptr = _data;

		rc = 1;
	}
	return rc;
}

template <class T>
Error CowData<T>::resize(int p_size) {
	ERR_FAIL_COND_V(p_size < 0, ERR_INVALID_PARAMETER);

	int current_size = size();

	if (p_size == current_size) {
		return OK;
	}

	if (p_size == 0) {
		// wants to clean up
		_unref(_ptr);
		_ptr = nullptr;
		return OK;
	}

	// possibly changing size, copy on write
	uint32_t rc = _copy_on_write();

	size_t current_alloc_size = _get_alloc_size(current_size);
	size_t alloc_size;
	ERR_FAIL_COND_V(!_get_alloc_size_checked(p_size, &alloc_size), ERR_OUT_OF_MEMORY);

	if (p_size > current_size) {
		if (alloc_size != current_alloc_size) {
			if (current_size == 0) {
				// alloc from scratch
				uint32_t *ptr = (uint32_t *)Memory::alloc_static(alloc_size, true);
				ERR_FAIL_COND_V(!ptr, ERR_OUT_OF_MEMORY);
				*(ptr - 1) = 0; //size, currently none
				new (ptr - 2, sizeof(uint32_t), "") SafeNumeric<uint32_t>(1); //refcount

				_ptr = (T *)ptr;

			} else {
				uint32_t *_ptrnew = (uint32_t *)Memory::realloc_static(_ptr, alloc_size, true);
				ERR_FAIL_COND_V(!_ptrnew, ERR_OUT_OF_MEMORY);
				new (_ptrnew - 2, sizeof(uint32_t), "") SafeNumeric<uint32_t>(rc); //refcount

				_ptr = (T *)(_ptrnew);
			}
		}

		// construct the newly created elements

		if (!std::is_trivially_constructible<T>::value) {
			for (int i = *_get_size(); i < p_size; i++) {
				memnew_placement(&_ptr[i], T);
			}
		}

		*_get_size() = p_size;

	} else if (p_size < current_size) {
		if (!std::is_trivially_destructible<T>::value) {
			// deinitialize no longer needed elements
			for (uint32_t i = p_size; i < *_get_size(); i++) {
				T *t = &_ptr[i];
				t->~T();
			}
		}

		if (alloc_size != current_alloc_size) {
			uint32_t *_ptrnew = (uint32_t *)Memory::realloc_static(_ptr, alloc_size, true);
			ERR_FAIL_COND_V(!_ptrnew, ERR_OUT_OF_MEMORY);
			new (_ptrnew - 2, sizeof(uint32_t), "") SafeNumeric<uint32_t>(rc); //refcount

			_ptr = (T *)(_ptrnew);
		}

		*_get_size() = p_size;
	}

	return OK;
}

template <class T>
int CowData<T>::find(const T &p_val, int p_from) const {
	int ret = -1;

	if (p_from < 0 || size() == 0) {
		return ret;
	}

	for (int i = p_from; i < size(); i++) {
		if (get(i) == p_val) {
			ret = i;
			break;
		}
	}

	return ret;
}

template <class T>
void CowData<T>::_ref(const CowData *p_from) {
	_ref(*p_from);
}

template <class T>
void CowData<T>::_ref(const CowData &p_from) {
	if (_ptr == p_from._ptr) {
		return; // self assign, do nothing.
	}

	_unref(_ptr);
	_ptr = nullptr;

	if (!p_from._ptr) {
		return; //nothing to do
	}

	if (p_from._get_refcount()->conditional_increment() > 0) { // could reference
		_ptr = p_from._ptr;
	}
}

template <class T>
CowData<T>::CowData() {
	_ptr = nullptr;
}

template <class T>
CowData<T>::~CowData() {
	_unref(_ptr);
}

#endif // COWDATA_H
