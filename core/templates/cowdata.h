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

#include "core/error/error_macros.h"
#include "core/os/memory.h"
#include "core/templates/safe_refcount.h"

#include <string.h>
#include <type_traits>

template <typename T>
class Vector;
class String;
class Char16String;
class CharString;
template <typename T, typename V>
class VMap;

static_assert(std::is_trivially_destructible_v<std::atomic<uint64_t>>);

// Silence a false positive warning (see GH-52119).
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wplacement-new"
#endif

template <typename T>
class CowData {
	template <typename TV>
	friend class Vector;
	friend class String;
	friend class Char16String;
	friend class CharString;
	template <typename TV, typename VV>
	friend class VMap;

public:
	typedef int64_t Size;
	typedef uint64_t USize;
	static constexpr USize MAX_INT = INT64_MAX;

private:
	// Function to find the next power of 2 to an integer.
	static _FORCE_INLINE_ USize next_po2(USize x) {
		if (x == 0) {
			return 0;
		}

		--x;
		x |= x >> 1;
		x |= x >> 2;
		x |= x >> 4;
		x |= x >> 8;
		x |= x >> 16;
		if (sizeof(USize) == 8) {
			x |= x >> 32;
		}

		return ++x;
	}

	// Alignment:  ↓ max_align_t           ↓ USize          ↓ max_align_t
	//             ┌────────────────────┬──┬─────────────┬──┬───────────...
	//             │ SafeNumeric<USize> │░░│ USize       │░░│ T[]
	//             │ ref. count         │░░│ data size   │░░│ data
	//             └────────────────────┴──┴─────────────┴──┴───────────...
	// Offset:     ↑ REF_COUNT_OFFSET      ↑ SIZE_OFFSET    ↑ DATA_OFFSET

	static constexpr size_t REF_COUNT_OFFSET = 0;
	static constexpr size_t SIZE_OFFSET = ((REF_COUNT_OFFSET + sizeof(SafeNumeric<USize>)) % alignof(USize) == 0) ? (REF_COUNT_OFFSET + sizeof(SafeNumeric<USize>)) : ((REF_COUNT_OFFSET + sizeof(SafeNumeric<USize>)) + alignof(USize) - ((REF_COUNT_OFFSET + sizeof(SafeNumeric<USize>)) % alignof(USize)));
	static constexpr size_t DATA_OFFSET = ((SIZE_OFFSET + sizeof(USize)) % alignof(max_align_t) == 0) ? (SIZE_OFFSET + sizeof(USize)) : ((SIZE_OFFSET + sizeof(USize)) + alignof(max_align_t) - ((SIZE_OFFSET + sizeof(USize)) % alignof(max_align_t)));

	mutable T *_ptr = nullptr;

	// internal helpers

	static _FORCE_INLINE_ SafeNumeric<USize> *_get_refcount_ptr(uint8_t *p_ptr) {
		return (SafeNumeric<USize> *)(p_ptr + REF_COUNT_OFFSET);
	}

	static _FORCE_INLINE_ USize *_get_size_ptr(uint8_t *p_ptr) {
		return (USize *)(p_ptr + SIZE_OFFSET);
	}

	static _FORCE_INLINE_ T *_get_data_ptr(uint8_t *p_ptr) {
		return (T *)(p_ptr + DATA_OFFSET);
	}

	_FORCE_INLINE_ SafeNumeric<USize> *_get_refcount() const {
		if (!_ptr) {
			return nullptr;
		}

		return (SafeNumeric<USize> *)((uint8_t *)_ptr - DATA_OFFSET + REF_COUNT_OFFSET);
	}

	_FORCE_INLINE_ USize *_get_size() const {
		if (!_ptr) {
			return nullptr;
		}

		return (USize *)((uint8_t *)_ptr - DATA_OFFSET + SIZE_OFFSET);
	}

	_FORCE_INLINE_ USize _get_alloc_size(USize p_elements) const {
		return next_po2(p_elements * sizeof(T));
	}

	_FORCE_INLINE_ bool _get_alloc_size_checked(USize p_elements, USize *out) const {
		if (unlikely(p_elements == 0)) {
			*out = 0;
			return true;
		}
#if defined(__GNUC__) && defined(IS_32_BIT)
		USize o;
		USize p;
		if (__builtin_mul_overflow(p_elements, sizeof(T), &o)) {
			*out = 0;
			return false;
		}
		*out = next_po2(o);
		if (__builtin_add_overflow(o, static_cast<USize>(32), &p)) {
			return false; // No longer allocated here.
		}
#else
		// Speed is more important than correctness here, do the operations unchecked
		// and hope for the best.
		*out = _get_alloc_size(p_elements);
#endif
		return *out;
	}

	void _unref();
	void _ref(const CowData *p_from);
	void _ref(const CowData &p_from);
	USize _copy_on_write();

public:
	void operator=(const CowData<T> &p_from) { _ref(p_from); }
	void operator=(CowData<T> &&p_from) {
		if (_ptr == p_from._ptr) {
			return;
		}

		_unref();
		_ptr = p_from._ptr;
		p_from._ptr = nullptr;
	}

	_FORCE_INLINE_ T *ptrw() {
		_copy_on_write();
		return _ptr;
	}

	_FORCE_INLINE_ const T *ptr() const {
		return _ptr;
	}

	_FORCE_INLINE_ Size size() const {
		USize *size = (USize *)_get_size();
		if (size) {
			return *size;
		} else {
			return 0;
		}
	}

	_FORCE_INLINE_ void clear() { resize(0); }
	_FORCE_INLINE_ bool is_empty() const { return _ptr == nullptr; }

	_FORCE_INLINE_ void set(Size p_index, const T &p_elem) {
		ERR_FAIL_INDEX(p_index, size());
		_copy_on_write();
		_ptr[p_index] = p_elem;
	}

	_FORCE_INLINE_ T &get_m(Size p_index) {
		CRASH_BAD_INDEX(p_index, size());
		_copy_on_write();
		return _ptr[p_index];
	}

	_FORCE_INLINE_ const T &get(Size p_index) const {
		CRASH_BAD_INDEX(p_index, size());

		return _ptr[p_index];
	}

	template <bool p_ensure_zero = false>
	Error resize(Size p_size);

	_FORCE_INLINE_ void remove_at(Size p_index) {
		ERR_FAIL_INDEX(p_index, size());
		T *p = ptrw();
		Size len = size();
		for (Size i = p_index; i < len - 1; i++) {
			p[i] = p[i + 1];
		}

		resize(len - 1);
	}

	Error insert(Size p_pos, const T &p_val) {
		Size new_size = size() + 1;
		ERR_FAIL_INDEX_V(p_pos, new_size, ERR_INVALID_PARAMETER);
		Error err = resize(new_size);
		ERR_FAIL_COND_V(err, err);
		T *p = ptrw();
		for (Size i = new_size - 1; i > p_pos; i--) {
			p[i] = p[i - 1];
		}
		p[p_pos] = p_val;

		return OK;
	}

	Size find(const T &p_val, Size p_from = 0) const;
	Size rfind(const T &p_val, Size p_from = -1) const;
	Size count(const T &p_val) const;

	_FORCE_INLINE_ CowData() {}
	_FORCE_INLINE_ ~CowData();
	_FORCE_INLINE_ CowData(const CowData<T> &p_from) { _ref(p_from); }
	_FORCE_INLINE_ CowData(CowData<T> &&p_from) {
		_ptr = p_from._ptr;
		p_from._ptr = nullptr;
	}
};

template <typename T>
void CowData<T>::_unref() {
	if (!_ptr) {
		return;
	}

	SafeNumeric<USize> *refc = _get_refcount();
	if (refc->decrement() > 0) {
		return; // still in use
	}
	// clean up

	if constexpr (!std::is_trivially_destructible_v<T>) {
		USize current_size = *_get_size();

		for (USize i = 0; i < current_size; ++i) {
			// call destructors
			T *t = &_ptr[i];
			t->~T();
		}
	}

	// free mem
	Memory::free_static(((uint8_t *)_ptr) - DATA_OFFSET, false);
}

template <typename T>
typename CowData<T>::USize CowData<T>::_copy_on_write() {
	if (!_ptr) {
		return 0;
	}

	SafeNumeric<USize> *refc = _get_refcount();

	USize rc = refc->get();
	if (unlikely(rc > 1)) {
		/* in use by more than me */
		USize current_size = *_get_size();

		uint8_t *mem_new = (uint8_t *)Memory::alloc_static(_get_alloc_size(current_size) + DATA_OFFSET, false);
		ERR_FAIL_NULL_V(mem_new, 0);

		SafeNumeric<USize> *_refc_ptr = _get_refcount_ptr(mem_new);
		USize *_size_ptr = _get_size_ptr(mem_new);
		T *_data_ptr = _get_data_ptr(mem_new);

		new (_refc_ptr) SafeNumeric<USize>(1); //refcount
		*(_size_ptr) = current_size; //size

		// initialize new elements
		if constexpr (std::is_trivially_copyable_v<T>) {
			memcpy((uint8_t *)_data_ptr, _ptr, current_size * sizeof(T));
		} else {
			for (USize i = 0; i < current_size; i++) {
				memnew_placement(&_data_ptr[i], T(_ptr[i]));
			}
		}

		_unref();
		_ptr = _data_ptr;

		rc = 1;
	}
	return rc;
}

template <typename T>
template <bool p_ensure_zero>
Error CowData<T>::resize(Size p_size) {
	ERR_FAIL_COND_V(p_size < 0, ERR_INVALID_PARAMETER);

	Size current_size = size();

	if (p_size == current_size) {
		return OK;
	}

	if (p_size == 0) {
		// wants to clean up
		_unref();
		_ptr = nullptr;
		return OK;
	}

	// possibly changing size, copy on write
	USize rc = _copy_on_write();

	USize current_alloc_size = _get_alloc_size(current_size);
	USize alloc_size;
	ERR_FAIL_COND_V(!_get_alloc_size_checked(p_size, &alloc_size), ERR_OUT_OF_MEMORY);

	if (p_size > current_size) {
		if (alloc_size != current_alloc_size) {
			if (current_size == 0) {
				// alloc from scratch
				uint8_t *mem_new = (uint8_t *)Memory::alloc_static(alloc_size + DATA_OFFSET, false);
				ERR_FAIL_NULL_V(mem_new, ERR_OUT_OF_MEMORY);

				SafeNumeric<USize> *_refc_ptr = _get_refcount_ptr(mem_new);
				USize *_size_ptr = _get_size_ptr(mem_new);
				T *_data_ptr = _get_data_ptr(mem_new);

				new (_refc_ptr) SafeNumeric<USize>(1); //refcount
				*(_size_ptr) = 0; //size, currently none

				_ptr = _data_ptr;

			} else {
				uint8_t *mem_new = (uint8_t *)Memory::realloc_static(((uint8_t *)_ptr) - DATA_OFFSET, alloc_size + DATA_OFFSET, false);
				ERR_FAIL_NULL_V(mem_new, ERR_OUT_OF_MEMORY);

				SafeNumeric<USize> *_refc_ptr = _get_refcount_ptr(mem_new);
				T *_data_ptr = _get_data_ptr(mem_new);

				new (_refc_ptr) SafeNumeric<USize>(rc); //refcount

				_ptr = _data_ptr;
			}
		}

		// construct the newly created elements

		if constexpr (!std::is_trivially_constructible_v<T>) {
			for (Size i = *_get_size(); i < p_size; i++) {
				memnew_placement(&_ptr[i], T);
			}
		} else if (p_ensure_zero) {
			memset((void *)(_ptr + current_size), 0, (p_size - current_size) * sizeof(T));
		}

		*_get_size() = p_size;

	} else if (p_size < current_size) {
		if constexpr (!std::is_trivially_destructible_v<T>) {
			// deinitialize no longer needed elements
			for (USize i = p_size; i < *_get_size(); i++) {
				T *t = &_ptr[i];
				t->~T();
			}
		}

		if (alloc_size != current_alloc_size) {
			uint8_t *mem_new = (uint8_t *)Memory::realloc_static(((uint8_t *)_ptr) - DATA_OFFSET, alloc_size + DATA_OFFSET, false);
			ERR_FAIL_NULL_V(mem_new, ERR_OUT_OF_MEMORY);

			SafeNumeric<USize> *_refc_ptr = _get_refcount_ptr(mem_new);
			T *_data_ptr = _get_data_ptr(mem_new);

			new (_refc_ptr) SafeNumeric<USize>(rc); //refcount

			_ptr = _data_ptr;
		}

		*_get_size() = p_size;
	}

	return OK;
}

template <typename T>
typename CowData<T>::Size CowData<T>::find(const T &p_val, Size p_from) const {
	Size ret = -1;

	if (p_from < 0 || size() == 0) {
		return ret;
	}

	for (Size i = p_from; i < size(); i++) {
		if (get(i) == p_val) {
			ret = i;
			break;
		}
	}

	return ret;
}

template <typename T>
typename CowData<T>::Size CowData<T>::rfind(const T &p_val, Size p_from) const {
	const Size s = size();

	if (p_from < 0) {
		p_from = s + p_from;
	}
	if (p_from < 0 || p_from >= s) {
		p_from = s - 1;
	}

	for (Size i = p_from; i >= 0; i--) {
		if (get(i) == p_val) {
			return i;
		}
	}
	return -1;
}

template <typename T>
typename CowData<T>::Size CowData<T>::count(const T &p_val) const {
	Size amount = 0;
	for (Size i = 0; i < size(); i++) {
		if (get(i) == p_val) {
			amount++;
		}
	}
	return amount;
}

template <typename T>
void CowData<T>::_ref(const CowData *p_from) {
	_ref(*p_from);
}

template <typename T>
void CowData<T>::_ref(const CowData &p_from) {
	if (_ptr == p_from._ptr) {
		return; // self assign, do nothing.
	}

	_unref();
	_ptr = nullptr;

	if (!p_from._ptr) {
		return; //nothing to do
	}

	if (p_from._get_refcount()->conditional_increment() > 0) { // could reference
		_ptr = p_from._ptr;
	}
}

template <typename T>
CowData<T>::~CowData() {
	_unref();
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif // COWDATA_H
