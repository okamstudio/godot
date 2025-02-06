/**************************************************************************/
/*  nullable.h                                                            */
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

#ifndef NULLABLE_H
#define NULLABLE_H

#include "core/typedefs.h"

#include <type_traits>

/**
 * A template wrapper meant to allow value-types to also be assigned null. The
 * internal value should be validated as non-null before being handled as a
 * proper value. Includes null-coalescing operators `<<` and `<<=` that're meant
 * to mirror C#'s `??` and `??=` respectively.
 *
 * Despite containing a pointer, this isn't meant to be handled as a pointer type
 * in the traditional sense. The syntax is setup in such a way to enforce the idea
 * that this is the type it's wrapping, if that type could also be assigned and
 * compared against `nullptr`.
 */

template <typename T>
class [[nodiscard]] Nullable {
	T *value;
	template <typename T_Other>
	static constexpr bool is_implicitly_convertible_v = std::is_constructible_v<T, T_Other> && std::is_convertible_v<T_Other, T>;
	template <typename T_Other>
	static constexpr bool is_explicitly_convertible_v = std::is_constructible_v<T, T_Other> && !std::is_convertible_v<T_Other, T>;

public:
	_ALWAYS_INLINE_ Nullable() :
			value(nullptr) {}

	_ALWAYS_INLINE_ Nullable(std::nullptr_t) :
			value(nullptr) {}

	_ALWAYS_INLINE_ Nullable(const T &p_value) :
			value(memnew(T(p_value))) {}

	template <typename T_Other, std::enable_if_t<is_implicitly_convertible_v<T_Other>, int> = 0>
	_ALWAYS_INLINE_ Nullable(const T_Other &p_value) :
			value(memnew(T(p_value))) {}

	template <typename T_Other, std::enable_if_t<is_explicitly_convertible_v<T_Other>, int> = 0>
	_ALWAYS_INLINE_ explicit Nullable(const T_Other &p_value) :
			value(memnew(T(p_value))) {}

	_ALWAYS_INLINE_ Nullable(const Nullable &p_other) :
			value(p_other.value) {}

	template <typename T_Other, std::enable_if_t<is_implicitly_convertible_v<T_Other>, int> = 0>
	_ALWAYS_INLINE_ Nullable(const Nullable<T_Other> &p_other) {
		if (p_other.has_value()) {
			operator=(*p_other);
		}
	}

	template <typename T_Other, std::enable_if_t<is_explicitly_convertible_v<T_Other>, int> = 0>
	_ALWAYS_INLINE_ explicit Nullable(const Nullable<T_Other> &p_other) {
		if (p_other.has_value()) {
			operator=((T_Other)*p_other);
		}
	}

	_ALWAYS_INLINE_ ~Nullable() {
		if (value != nullptr) {
			memdelete(value);
		}
		value = nullptr;
	}

	_ALWAYS_INLINE_ T &operator*() const {
		DEV_ASSERT(value != nullptr);
		return *value;
	}

	_ALWAYS_INLINE_ void operator=(std::nullptr_t) {
		if (value != nullptr) {
			memdelete(value);
		}
		value = nullptr;
	}

	_ALWAYS_INLINE_ void operator=(const T &p_value) {
		if (value == nullptr) {
			value = memnew(T);
		}
		*value = p_value;
	}

	template <typename T_Other, std::enable_if_t<std::is_convertible_v<T, T_Other>, int> = 0>
	_ALWAYS_INLINE_ void operator=(const T_Other &p_value) {
		if (value == nullptr) {
			value = memnew(T);
		}
		*value = (T)p_value;
	}

	_ALWAYS_INLINE_ void operator=(const Nullable &p_other) {
		if (unlikely(this == &p_other)) {
			return;
		} else if (p_other.value == nullptr) {
			operator=(nullptr);
		} else {
			operator=(*p_other);
		}
	}

	template <typename T_Other, std::enable_if_t<std::is_convertible_v<T, T_Other>, int> = 0>
	_ALWAYS_INLINE_ void operator=(const Nullable<T_Other> &p_other) {
		if (unlikely(this == &p_other)) {
			return;
		} else if (p_other.value == nullptr) {
			operator=(nullptr);
		} else {
			operator=(*p_other);
		}
	}

	_ALWAYS_INLINE_ bool is_null() const { return value == nullptr; }
	_ALWAYS_INLINE_ bool has_value() const { return value != nullptr; }

	_ALWAYS_INLINE_ bool operator==(std::nullptr_t) const { return is_null(); }
	_ALWAYS_INLINE_ bool operator!=(std::nullptr_t) const { return has_value(); }

	_ALWAYS_INLINE_ bool operator==(const T &p_value) const { return is_null() ? false : *value == p_value; }
	_ALWAYS_INLINE_ bool operator!=(const T &p_value) const { return is_null() ? true : *value != p_value; }

	template <typename T_Other, std::enable_if_t<std::is_convertible_v<T, T_Other>, int> = 0>
	_ALWAYS_INLINE_ bool operator==(const T_Other &p_value) const { return is_null() ? false : *value == p_value; }
	template <typename T_Other, std::enable_if_t<std::is_convertible_v<T, T_Other>, int> = 0>
	_ALWAYS_INLINE_ bool operator!=(const T_Other &p_value) const { return is_null() ? true : *value != p_value; }

	_ALWAYS_INLINE_ bool operator==(const Nullable &p_other) const { return p_other.is_null() ? p_other.is_null() : (p_other.is_null() ? false : *value == *p_other.value); }
	_ALWAYS_INLINE_ bool operator!=(const Nullable &p_other) const { return p_other.is_null() ? p_other.has_value() : (p_other.is_null() ? true : *value != *p_other.value); }

	template <typename T_Other, std::enable_if_t<std::is_convertible_v<T, T_Other>, int> = 0>
	_ALWAYS_INLINE_ bool operator==(const Nullable &p_other) const { return p_other.is_null() ? p_other.is_null() : (p_other.is_null() ? false : *value == *p_other.value); }
	template <typename T_Other, std::enable_if_t<std::is_convertible_v<T, T_Other>, int> = 0>
	_ALWAYS_INLINE_ bool operator!=(const Nullable &p_other) const { return p_other.is_null() ? p_other.has_value() : (p_other.is_null() ? true : *value != *p_other.value); }

	// Null coalescing operators.

	_ALWAYS_INLINE_ T operator<<(const T &p_value) const { return has_value() ? *value : p_value; }

	template <typename T_Other, std::enable_if_t<std::is_convertible_v<T, T_Other>, int> = 0>
	_ALWAYS_INLINE_ T operator<<(const T_Other &p_value) const { return has_value() ? *value : T(p_value); }

	_ALWAYS_INLINE_ void operator<<=(const T &p_value) {
		if (is_null()) {
			value = memnew(T(p_value));
		}
	}

	template <typename T_Other, std::enable_if_t<std::is_convertible_v<T, T_Other>, int> = 0>
	_ALWAYS_INLINE_ void operator<<=(const T_Other &p_value) {
		if (is_null()) {
			value = memnew(T(p_value));
		}
	}

	_ALWAYS_INLINE_ Nullable operator<<(const Nullable &p_other) const { return has_value() ? this : p_other; }

	template <typename T_Other, std::enable_if_t<std::is_convertible_v<T, T_Other>, int> = 0>
	_ALWAYS_INLINE_ Nullable operator<<(const Nullable &p_other) const { return has_value() ? this : T(p_other); }

	_ALWAYS_INLINE_ void operator<<=(const Nullable &p_other) {
		if (is_null()) {
			operator=(p_other);
		}
	}

	template <typename T_Other, std::enable_if_t<std::is_convertible_v<T, T_Other>, int> = 0>
	_ALWAYS_INLINE_ void operator<<=(const Nullable &p_other) {
		if (is_null()) {
			operator=(p_other);
		}
	}
};

#endif // NULLABLE_H
