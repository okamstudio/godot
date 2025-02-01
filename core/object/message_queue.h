/**************************************************************************/
/*  message_queue.h                                                       */
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

#include "core/object/object_id.h"
#include "core/os/thread_safe.h"
#include "core/templates/local_vector.h"
#include "core/templates/paged_allocator.h"
#include "core/variant/variant.h"

class Object;

class CallQueue {
	friend class MessageQueue;

public:
	enum {
		PAGE_SIZE_BYTES = 4096
	};

	struct Page {
		uint8_t data[PAGE_SIZE_BYTES];
	};

	// Needs to be public to be able to define it outside the class.
	// Needs to lock because there can be multiple of these allocators in several threads.
	typedef PagedAllocator<Page, true> Allocator;

private:
	enum {
		TYPE_CALL,
		TYPE_NOTIFICATION,
		TYPE_SET,
		TYPE_END, // End marker.
		FLAG_NULL_IS_OK = 1 << 13,
		FLAG_SHOW_ERROR = 1 << 14,
		FLAG_MASK = FLAG_NULL_IS_OK - 1,
	};

	Mutex mutex;

	Allocator *allocator = nullptr;
	bool allocator_is_custom = false;

	LocalVector<Page *> pages;
	LocalVector<uint32_t> page_bytes;
	uint32_t max_pages = 0;
	uint32_t pages_used = 0;
	bool flushing = false;

#ifdef DEV_ENABLED
	bool is_current_thread_override = false;
#endif

	struct Message {
		Callable callable;
		int16_t type;
		union {
			int16_t notification;
			int16_t args;
		};
	};

	_FORCE_INLINE_ void _ensure_first_page() {
		if (unlikely(pages.is_empty())) {
			pages.push_back(allocator->alloc());
			page_bytes.push_back(0);
			pages_used = 1;
		}
	}

	void _add_page();

	void _call_function(const Callable &p_callable, const Variant *p_args, int p_argcount, bool p_show_error);

	String error_text;

public:
	Error push_callp(ObjectID p_id, const StringName &p_method, const Variant **p_args, int p_argcount, bool p_show_error = false);
	template <typename... VarArgs>
	Error push_call(ObjectID p_id, const StringName &p_method, VarArgs... p_args) {
		Variant args[sizeof...(p_args) + 1] = { p_args..., Variant() }; // +1 makes sure zero sized arrays are also supported.
		const Variant *argptrs[sizeof...(p_args) + 1];
		for (uint32_t i = 0; i < sizeof...(p_args); i++) {
			argptrs[i] = &args[i];
		}
		return push_callp(p_id, p_method, sizeof...(p_args) == 0 ? nullptr : (const Variant **)argptrs, sizeof...(p_args));
	}

	Error push_callablep(const Callable &p_callable, const Variant **p_args, int p_argcount, bool p_show_error = false);
	Error push_set(ObjectID p_id, const StringName &p_prop, const Variant &p_value);
	Error push_notification(ObjectID p_id, int p_notification);

	template <typename... VarArgs>
	Error push_callable(const Callable &p_callable, VarArgs... p_args) {
		Variant args[sizeof...(p_args) + 1] = { p_args..., Variant() }; // +1 makes sure zero sized arrays are also supported.
		const Variant *argptrs[sizeof...(p_args) + 1];
		for (uint32_t i = 0; i < sizeof...(p_args); i++) {
			argptrs[i] = &args[i];
		}
		return push_callablep(p_callable, sizeof...(p_args) == 0 ? nullptr : (const Variant **)argptrs, sizeof...(p_args));
	}

	Error push_callp(Object *p_object, const StringName &p_method, const Variant **p_args, int p_argcount, bool p_show_error = false);
	template <typename... VarArgs>
	Error push_call(Object *p_object, const StringName &p_method, VarArgs... p_args) {
		Variant args[sizeof...(p_args) + 1] = { p_args..., Variant() }; // +1 makes sure zero sized arrays are also supported.
		const Variant *argptrs[sizeof...(p_args) + 1];
		for (uint32_t i = 0; i < sizeof...(p_args); i++) {
			argptrs[i] = &args[i];
		}
		return push_callp(p_object, p_method, sizeof...(p_args) == 0 ? nullptr : (const Variant **)argptrs, sizeof...(p_args));
	}

	Error push_notification(Object *p_object, int p_notification);
	Error push_set(Object *p_object, const StringName &p_prop, const Variant &p_value);

	Error flush();
	void clear();
	void statistics();

	bool has_messages() const;

	bool is_flushing() const;
	int get_max_buffer_usage() const;

	CallQueue(Allocator *p_custom_allocator = nullptr, uint32_t p_max_pages = 8192, const String &p_error_text = String());
	virtual ~CallQueue();
};

class MessageQueue : public CallQueue {
	static CallQueue *main_singleton;
	static thread_local CallQueue *thread_singleton;
	friend class CallQueue;

public:
	_FORCE_INLINE_ static CallQueue *get_singleton() { return thread_singleton ? thread_singleton : main_singleton; }
	_FORCE_INLINE_ static CallQueue *get_main_singleton() { return main_singleton; }

	static void set_thread_singleton_override(CallQueue *p_thread_singleton);

	MessageQueue();
	~MessageQueue();
};
