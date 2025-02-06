/**************************************************************************/
/*  safe_list.h                                                           */
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

#include "core/os/memory.h"
#include "core/typedefs.h"

#include <atomic>
#include <functional>
#include <initializer_list>
#include <type_traits>

// Design goals for these classes:
// - Accessing this list with an iterator will never result in a use-after free,
//   even if the element being accessed has been logically removed from the list on
//   another thread.
// - Logical deletion from the list will not result in deallocation at that time,
//   instead the node will be deallocated at a later time when it is safe to do so.
// - No blocking synchronization primitives will be used.

// This is used in very specific areas of the engine where it's critical that these guarantees are held.

template <typename T, typename A = DefaultAllocator>
class SafeList {
	struct SafeListNode {
		std::atomic<SafeListNode *> next = nullptr;

		// If the node is logically deleted, this pointer will typically point
		// to the previous list item in time that was also logically deleted.
		std::atomic<SafeListNode *> graveyard_next = nullptr;

		std::function<void(T)> deletion_fn = [](T t) { return; };

		T val;
	};

	static_assert(std::atomic<T>::is_always_lock_free);

	std::atomic<SafeListNode *> head = nullptr;
	std::atomic<SafeListNode *> graveyard_head = nullptr;

	std::atomic_uint active_iterator_count = 0;

public:
	class Iterator {
		friend class SafeList;

		SafeListNode *cursor = nullptr;
		SafeList *list = nullptr;

		Iterator(SafeListNode *p_cursor, SafeList *p_list) :
				cursor(p_cursor), list(p_list) {
			list->active_iterator_count++;
		}

	public:
		Iterator(const Iterator &p_other) :
				cursor(p_other.cursor), list(p_other.list) {
			list->active_iterator_count++;
		}

		~Iterator() {
			list->active_iterator_count--;
		}

	public:
		T &operator*() {
			return cursor->val;
		}

		Iterator &operator++() {
			cursor = cursor->next;
			return *this;
		}

		// These two operators are mostly useful for comparisons to nullptr.
		bool operator==(const void *p_other) const {
			return cursor == p_other;
		}

		bool operator!=(const void *p_other) const {
			return cursor != p_other;
		}

		// These two allow easy range-based for loops.
		bool operator==(const Iterator &p_other) const {
			return cursor == p_other.cursor;
		}

		bool operator!=(const Iterator &p_other) const {
			return cursor != p_other.cursor;
		}
	};

public:
	// Calling this will cause an allocation.
	void insert(T p_value) {
		SafeListNode *new_node = memnew_allocator(SafeListNode, A);
		new_node->val = p_value;
		SafeListNode *expected_head = nullptr;
		do {
			expected_head = head.load();
			new_node->next.store(expected_head);
		} while (!head.compare_exchange_strong(/* expected= */ expected_head, /* new= */ new_node));
	}

	Iterator find(T p_value) {
		for (Iterator it = begin(); it != end(); ++it) {
			if (*it == p_value) {
				return it;
			}
		}
		return end();
	}

	void erase(T p_value, std::function<void(T)> p_deletion_fn) {
		Iterator tmp = find(p_value);
		erase(tmp, p_deletion_fn);
	}

	void erase(T p_value) {
		Iterator tmp = find(p_value);
		erase(tmp, [](T t) { return; });
	}

	void erase(Iterator &p_iterator, std::function<void(T)> p_deletion_fn) {
		p_iterator.cursor->deletion_fn = p_deletion_fn;
		erase(p_iterator);
	}

	void erase(Iterator &p_iterator) {
		if (find(p_iterator.cursor->val) == nullptr) {
			// Not in the list, nothing to do.
			return;
		}
		// First, remove the node from the list.
		while (true) {
			Iterator prev = begin();
			SafeListNode *expected_head = prev.cursor;
			for (; prev != end(); ++prev) {
				if (prev.cursor && prev.cursor->next == p_iterator.cursor) {
					break;
				}
			}
			if (prev != end()) {
				// There exists a node before this.
				prev.cursor->next.store(p_iterator.cursor->next.load());
				// Done.
				break;
			} else {
				if (head.compare_exchange_strong(/* expected= */ expected_head, /* new= */ p_iterator.cursor->next.load())) {
					// Successfully reassigned the head pointer before another thread changed it to something else.
					break;
				}
				// Fall through upon failure, try again.
			}
		}
		// Then queue it for deletion by putting it in the node graveyard.
		// Don't touch `next` because an iterator might still be pointing at this node.
		SafeListNode *expected_head = nullptr;
		do {
			expected_head = graveyard_head.load();
			p_iterator.cursor->graveyard_next.store(expected_head);
		} while (!graveyard_head.compare_exchange_strong(/* expected= */ expected_head, /* new= */ p_iterator.cursor));
	}

	Iterator begin() {
		return Iterator(head.load(), this);
	}

	Iterator end() {
		return Iterator(nullptr, this);
	}

	// Calling this will cause zero to many deallocations.
	bool maybe_cleanup() {
		SafeListNode *cursor = nullptr;
		SafeListNode *new_graveyard_head = nullptr;
		do {
			// The access order here is theoretically important.
			cursor = graveyard_head.load();
			if (active_iterator_count.load() != 0) {
				// It's not safe to clean up with an active iterator, because that iterator
				// could be pointing to an element that we want to delete.
				return false;
			}
			// Any iterator created after this point will never point to a deleted node.
			// Swap it out with the current graveyard head.
		} while (!graveyard_head.compare_exchange_strong(/* expected= */ cursor, /* new= */ new_graveyard_head));
		// Our graveyard list is now unreachable by any active iterators,
		// detached from the main graveyard head and ready for deletion.
		while (cursor) {
			SafeListNode *tmp = cursor;
			cursor = cursor->graveyard_next;
			tmp->deletion_fn(tmp->val);
			memdelete_allocator<SafeListNode, A>(tmp);
		}
		return true;
	}

	_FORCE_INLINE_ SafeList() {}
	_FORCE_INLINE_ SafeList(std::initializer_list<T> p_init) {
		for (const T &E : p_init) {
			insert(E);
		}
	}

	~SafeList() {
#ifdef DEBUG_ENABLED
		if (!maybe_cleanup()) {
			ERR_PRINT("There are still iterators around when destructing a SafeList. Memory will be leaked. This is a bug.");
		}
#else
		maybe_cleanup();
#endif
	}
};
