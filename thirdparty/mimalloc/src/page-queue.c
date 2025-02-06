/*----------------------------------------------------------------------------
Copyright (c) 2018-2024, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

/* -----------------------------------------------------------
  Definition of page queues for each block size
----------------------------------------------------------- */

#ifndef MI_IN_PAGE_C
#error "this file should be included from 'page.c'"
// include to help an IDE
#include "mimalloc.h"
#include "mimalloc/internal.h"
#include "mimalloc/atomic.h"
#endif

/* -----------------------------------------------------------
  Minimal alignment in machine words (i.e. `sizeof(void*)`)
----------------------------------------------------------- */

#if (MI_MAX_ALIGN_SIZE > 4*MI_INTPTR_SIZE)
  #error "define alignment for more than 4x word size for this platform"
#elif (MI_MAX_ALIGN_SIZE > 2*MI_INTPTR_SIZE)
  #define MI_ALIGN4W   // 4 machine words minimal alignment
#elif (MI_MAX_ALIGN_SIZE > MI_INTPTR_SIZE)
  #define MI_ALIGN2W   // 2 machine words minimal alignment
#else
  // ok, default alignment is 1 word
#endif


/* -----------------------------------------------------------
  Queue query
----------------------------------------------------------- */


static inline bool mi_page_queue_is_huge(const mi_page_queue_t* pq) {
  return (pq->block_size == (MI_MEDIUM_OBJ_SIZE_MAX+sizeof(uintptr_t)));
}

static inline bool mi_page_queue_is_full(const mi_page_queue_t* pq) {
  return (pq->block_size == (MI_MEDIUM_OBJ_SIZE_MAX+(2*sizeof(uintptr_t))));
}

static inline bool mi_page_queue_is_special(const mi_page_queue_t* pq) {
  return (pq->block_size > MI_MEDIUM_OBJ_SIZE_MAX);
}

/* -----------------------------------------------------------
  Bins
----------------------------------------------------------- */

// Return the bin for a given field size.
// Returns MI_BIN_HUGE if the size is too large.
// We use `wsize` for the size in "machine word sizes",
// i.e. byte size == `wsize*sizeof(void*)`.
static inline uint8_t mi_bin(size_t size) {
  size_t wsize = _mi_wsize_from_size(size);
  uint8_t bin;
  if (wsize <= 1) {
    bin = 1;
  }
  #if defined(MI_ALIGN4W)
  else if (wsize <= 4) {
    bin = (uint8_t)((wsize+1)&~1); // round to double word sizes
  }
  #elif defined(MI_ALIGN2W)
  else if (wsize <= 8) {
    bin = (uint8_t)((wsize+1)&~1); // round to double word sizes
  }
  #else
  else if (wsize <= 8) {
    bin = (uint8_t)wsize;
  }
  #endif
  else if (wsize > MI_MEDIUM_OBJ_WSIZE_MAX) {
    bin = MI_BIN_HUGE;
  }
  else {
    #if defined(MI_ALIGN4W)
    if (wsize <= 16) { wsize = (wsize+3)&~3; } // round to 4x word sizes
    #endif
    wsize--;
    // find the highest bit
    uint8_t b = (uint8_t)mi_bsr(wsize);  // note: wsize != 0
    // and use the top 3 bits to determine the bin (~12.5% worst internal fragmentation).
    // - adjust with 3 because we use do not round the first 8 sizes
    //   which each get an exact bin
    bin = ((b << 2) + (uint8_t)((wsize >> (b - 2)) & 0x03)) - 3;
    mi_assert_internal(bin < MI_BIN_HUGE);
  }
  mi_assert_internal(bin > 0 && bin <= MI_BIN_HUGE);
  return bin;
}



/* -----------------------------------------------------------
  Queue of pages with free blocks
----------------------------------------------------------- */

uint8_t _mi_bin(size_t size) {
  return mi_bin(size);
}

size_t _mi_bin_size(uint8_t bin) {
  return _mi_heap_empty.pages[bin].block_size;
}

// Good size for allocation
size_t mi_good_size(size_t size) mi_attr_noexcept {
  if (size <= MI_MEDIUM_OBJ_SIZE_MAX) {
    return _mi_bin_size(mi_bin(size + MI_PADDING_SIZE));
  }
  else {
    return _mi_align_up(size + MI_PADDING_SIZE,_mi_os_page_size());
  }
}

#if (MI_DEBUG>1)
static bool mi_page_queue_contains(mi_page_queue_t* queue, const mi_page_t* page) {
  mi_assert_internal(page != NULL);
  mi_page_t* list = queue->first;
  while (list != NULL) {
    mi_assert_internal(list->next == NULL || list->next->prev == list);
    mi_assert_internal(list->prev == NULL || list->prev->next == list);
    if (list == page) break;
    list = list->next;
  }
  return (list == page);
}

#endif

#if (MI_DEBUG>1)
static bool mi_heap_contains_queue(const mi_heap_t* heap, const mi_page_queue_t* pq) {
  return (pq >= &heap->pages[0] && pq <= &heap->pages[MI_BIN_FULL]);
}
#endif

static inline bool mi_page_is_large_or_huge(const mi_page_t* page) {
  return (mi_page_block_size(page) > MI_MEDIUM_OBJ_SIZE_MAX || mi_page_is_huge(page));
}

static mi_page_queue_t* mi_heap_page_queue_of(mi_heap_t* heap, const mi_page_t* page) {
  mi_assert_internal(heap!=NULL);
  uint8_t bin = (mi_page_is_in_full(page) ? MI_BIN_FULL : (mi_page_is_huge(page) ? MI_BIN_HUGE : mi_bin(mi_page_block_size(page))));
  mi_assert_internal(bin <= MI_BIN_FULL);
  mi_page_queue_t* pq = &heap->pages[bin];
  mi_assert_internal((mi_page_block_size(page) == pq->block_size) ||
                       (mi_page_is_large_or_huge(page) && mi_page_queue_is_huge(pq)) ||
                         (mi_page_is_in_full(page) && mi_page_queue_is_full(pq)));
  return pq;
}

static mi_page_queue_t* mi_page_queue_of(const mi_page_t* page) {
  mi_heap_t* heap = mi_page_heap(page);
  mi_page_queue_t* pq = mi_heap_page_queue_of(heap, page);
  mi_assert_expensive(mi_page_queue_contains(pq, page));
  return pq;
}

// The current small page array is for efficiency and for each
// small size (up to 256) it points directly to the page for that
// size without having to compute the bin. This means when the
// current free page queue is updated for a small bin, we need to update a
// range of entries in `_mi_page_small_free`.
static inline void mi_heap_queue_first_update(mi_heap_t* heap, const mi_page_queue_t* pq) {
  mi_assert_internal(mi_heap_contains_queue(heap,pq));
  size_t size = pq->block_size;
  if (size > MI_SMALL_SIZE_MAX) return;

  mi_page_t* page = pq->first;
  if (pq->first == NULL) page = (mi_page_t*)&_mi_page_empty;

  // find index in the right direct page array
  size_t start;
  size_t idx = _mi_wsize_from_size(size);
  mi_page_t** pages_free = heap->pages_free_direct;

  if (pages_free[idx] == page) return;  // already set

  // find start slot
  if (idx<=1) {
    start = 0;
  }
  else {
    // find previous size; due to minimal alignment upto 3 previous bins may need to be skipped
    uint8_t bin = mi_bin(size);
    const mi_page_queue_t* prev = pq - 1;
    while( bin == mi_bin(prev->block_size) && prev > &heap->pages[0]) {
      prev--;
    }
    start = 1 + _mi_wsize_from_size(prev->block_size);
    if (start > idx) start = idx;
  }

  // set size range to the right page
  mi_assert(start <= idx);
  for (size_t sz = start; sz <= idx; sz++) {
    pages_free[sz] = page;
  }
}

/*
static bool mi_page_queue_is_empty(mi_page_queue_t* queue) {
  return (queue->first == NULL);
}
*/

static void mi_page_queue_remove(mi_page_queue_t* queue, mi_page_t* page) {
  mi_assert_internal(page != NULL);
  mi_assert_expensive(mi_page_queue_contains(queue, page));
  mi_assert_internal(mi_page_block_size(page) == queue->block_size ||
                      (mi_page_is_large_or_huge(page) && mi_page_queue_is_huge(queue)) ||
                        (mi_page_is_in_full(page) && mi_page_queue_is_full(queue)));
  mi_heap_t* heap = mi_page_heap(page);

  if (page->prev != NULL) page->prev->next = page->next;
  if (page->next != NULL) page->next->prev = page->prev;
  if (page == queue->last)  queue->last = page->prev;
  if (page == queue->first) {
    queue->first = page->next;
    // update first
    mi_assert_internal(mi_heap_contains_queue(heap, queue));
    mi_heap_queue_first_update(heap,queue);
  }
  heap->page_count--;
  page->next = NULL;
  page->prev = NULL;
  // mi_atomic_store_ptr_release(mi_atomic_cast(void*, &page->heap), NULL);
  mi_page_set_in_full(page,false);
}


static void mi_page_queue_push(mi_heap_t* heap, mi_page_queue_t* queue, mi_page_t* page) {
  mi_assert_internal(mi_page_heap(page) == heap);
  mi_assert_internal(!mi_page_queue_contains(queue, page));
  #if MI_HUGE_PAGE_ABANDON
  mi_assert_internal(_mi_page_segment(page)->kind != MI_SEGMENT_HUGE);
  #endif
  mi_assert_internal(mi_page_block_size(page) == queue->block_size ||
                      (mi_page_is_large_or_huge(page) && mi_page_queue_is_huge(queue)) ||
                        (mi_page_is_in_full(page) && mi_page_queue_is_full(queue)));

  mi_page_set_in_full(page, mi_page_queue_is_full(queue));
  // mi_atomic_store_ptr_release(mi_atomic_cast(void*, &page->heap), heap);
  page->next = queue->first;
  page->prev = NULL;
  if (queue->first != NULL) {
    mi_assert_internal(queue->first->prev == NULL);
    queue->first->prev = page;
    queue->first = page;
  }
  else {
    queue->first = queue->last = page;
  }

  // update direct
  mi_heap_queue_first_update(heap, queue);
  heap->page_count++;
}


static void mi_page_queue_enqueue_from(mi_page_queue_t* to, mi_page_queue_t* from, mi_page_t* page) {
  mi_assert_internal(page != NULL);
  mi_assert_expensive(mi_page_queue_contains(from, page));
  mi_assert_expensive(!mi_page_queue_contains(to, page));
  const size_t bsize = mi_page_block_size(page);
  MI_UNUSED(bsize);
  mi_assert_internal((bsize == to->block_size && bsize == from->block_size) ||
                     (bsize == to->block_size && mi_page_queue_is_full(from)) ||
                     (bsize == from->block_size && mi_page_queue_is_full(to)) ||
                     (mi_page_is_large_or_huge(page) && mi_page_queue_is_huge(to)) ||
                     (mi_page_is_large_or_huge(page) && mi_page_queue_is_full(to)));

  mi_heap_t* heap = mi_page_heap(page);
  if (page->prev != NULL) page->prev->next = page->next;
  if (page->next != NULL) page->next->prev = page->prev;
  if (page == from->last)  from->last = page->prev;
  if (page == from->first) {
    from->first = page->next;
    // update first
    mi_assert_internal(mi_heap_contains_queue(heap, from));
    mi_heap_queue_first_update(heap, from);
  }

  page->prev = to->last;
  page->next = NULL;
  if (to->last != NULL) {
    mi_assert_internal(heap == mi_page_heap(to->last));
    to->last->next = page;
    to->last = page;
  }
  else {
    to->first = page;
    to->last = page;
    mi_heap_queue_first_update(heap, to);
  }

  mi_page_set_in_full(page, mi_page_queue_is_full(to));
}

// Only called from `mi_heap_absorb`.
size_t _mi_page_queue_append(mi_heap_t* heap, mi_page_queue_t* pq, mi_page_queue_t* append) {
  mi_assert_internal(mi_heap_contains_queue(heap,pq));
  mi_assert_internal(pq->block_size == append->block_size);

  if (append->first==NULL) return 0;

  // set append pages to new heap and count
  size_t count = 0;
  for (mi_page_t* page = append->first; page != NULL; page = page->next) {
    // inline `mi_page_set_heap` to avoid wrong assertion during absorption;
    // in this case it is ok to be delayed freeing since both "to" and "from" heap are still alive.
    mi_atomic_store_release(&page->xheap, (uintptr_t)heap);
    // set the flag to delayed free (not overriding NEVER_DELAYED_FREE) which has as a
    // side effect that it spins until any DELAYED_FREEING is finished. This ensures
    // that after appending only the new heap will be used for delayed free operations.
    _mi_page_use_delayed_free(page, MI_USE_DELAYED_FREE, false);
    count++;
  }

  if (pq->last==NULL) {
    // take over afresh
    mi_assert_internal(pq->first==NULL);
    pq->first = append->first;
    pq->last = append->last;
    mi_heap_queue_first_update(heap, pq);
  }
  else {
    // append to end
    mi_assert_internal(pq->last!=NULL);
    mi_assert_internal(append->first!=NULL);
    pq->last->next = append->first;
    append->first->prev = pq->last;
    pq->last = append->last;
  }
  return count;
}
