/*----------------------------------------------------------------------------
Copyright (c) 2018-2024, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

/* -----------------------------------------------------------
  The core of the allocator. Every segment contains
  pages of a certain block size. The main function
  exported is `mi_malloc_generic`.
----------------------------------------------------------- */

#include "mimalloc.h"
#include "mimalloc/internal.h"
#include "mimalloc/atomic.h"

/* -----------------------------------------------------------
  Definition of page queues for each block size
----------------------------------------------------------- */

#define MI_IN_PAGE_C
#include "page-queue.c"
#undef MI_IN_PAGE_C


/* -----------------------------------------------------------
  Page helpers
----------------------------------------------------------- */

// Index a block in a page
static inline mi_block_t* mi_page_block_at(const mi_page_t* page, void* page_start, size_t block_size, size_t i) {
  MI_UNUSED(page);
  mi_assert_internal(page != NULL);
  mi_assert_internal(i <= page->reserved);
  return (mi_block_t*)((uint8_t*)page_start + (i * block_size));
}

static void mi_page_init(mi_heap_t* heap, mi_page_t* page, size_t size, mi_tld_t* tld);
static void mi_page_extend_free(mi_heap_t* heap, mi_page_t* page, mi_tld_t* tld);

#if (MI_DEBUG>=3)
static size_t mi_page_list_count(mi_page_t* page, mi_block_t* head) {
  size_t count = 0;
  while (head != NULL) {
    mi_assert_internal(page == _mi_ptr_page(head));
    count++;
    head = mi_block_next(page, head);
  }
  return count;
}

/*
// Start of the page available memory
static inline uint8_t* mi_page_area(const mi_page_t* page) {
  return _mi_page_start(_mi_page_segment(page), page, NULL);
}
*/

static bool mi_page_list_is_valid(mi_page_t* page, mi_block_t* p) {
  size_t psize;
  uint8_t* page_area = _mi_segment_page_start(_mi_page_segment(page), page, &psize);
  mi_block_t* start = (mi_block_t*)page_area;
  mi_block_t* end   = (mi_block_t*)(page_area + psize);
  while(p != NULL) {
    if (p < start || p >= end) return false;
    p = mi_block_next(page, p);
  }
#if MI_DEBUG>3 // generally too expensive to check this
  if (page->free_is_zero) {
    const size_t ubsize = mi_page_usable_block_size(page);
    for (mi_block_t* block = page->free; block != NULL; block = mi_block_next(page, block)) {
      mi_assert_expensive(mi_mem_is_zero(block + 1, ubsize - sizeof(mi_block_t)));
    }
  }
#endif
  return true;
}

static bool mi_page_is_valid_init(mi_page_t* page) {
  mi_assert_internal(mi_page_block_size(page) > 0);
  mi_assert_internal(page->used <= page->capacity);
  mi_assert_internal(page->capacity <= page->reserved);

  uint8_t* start = mi_page_start(page);
  mi_assert_internal(start == _mi_segment_page_start(_mi_page_segment(page), page, NULL));
  mi_assert_internal(page->is_huge == (_mi_page_segment(page)->kind == MI_SEGMENT_HUGE));
  //mi_assert_internal(start + page->capacity*page->block_size == page->top);

  mi_assert_internal(mi_page_list_is_valid(page,page->free));
  mi_assert_internal(mi_page_list_is_valid(page,page->local_free));

  #if MI_DEBUG>3 // generally too expensive to check this
  if (page->free_is_zero) {
    const size_t ubsize = mi_page_usable_block_size(page);
    for(mi_block_t* block = page->free; block != NULL; block = mi_block_next(page,block)) {
      mi_assert_expensive(mi_mem_is_zero(block + 1, ubsize - sizeof(mi_block_t)));
    }
  }
  #endif

  #if !MI_TRACK_ENABLED && !MI_TSAN
  mi_block_t* tfree = mi_page_thread_free(page);
  mi_assert_internal(mi_page_list_is_valid(page, tfree));
  //size_t tfree_count = mi_page_list_count(page, tfree);
  //mi_assert_internal(tfree_count <= page->thread_freed + 1);
  #endif

  size_t free_count = mi_page_list_count(page, page->free) + mi_page_list_count(page, page->local_free);
  mi_assert_internal(page->used + free_count == page->capacity);

  return true;
}

extern bool _mi_process_is_initialized;             // has mi_process_init been called?

bool _mi_page_is_valid(mi_page_t* page) {
  mi_assert_internal(mi_page_is_valid_init(page));
  #if MI_SECURE
  mi_assert_internal(page->keys[0] != 0);
  #endif
  if (mi_page_heap(page)!=NULL) {
    mi_segment_t* segment = _mi_page_segment(page);

    mi_assert_internal(!_mi_process_is_initialized || segment->thread_id==0 || segment->thread_id == mi_page_heap(page)->thread_id);
    #if MI_HUGE_PAGE_ABANDON
    if (segment->kind != MI_SEGMENT_HUGE)
    #endif
    {
      mi_page_queue_t* pq = mi_page_queue_of(page);
      mi_assert_internal(mi_page_queue_contains(pq, page));
      mi_assert_internal(pq->block_size==mi_page_block_size(page) || mi_page_block_size(page) > MI_MEDIUM_OBJ_SIZE_MAX || mi_page_is_in_full(page));
      mi_assert_internal(mi_heap_contains_queue(mi_page_heap(page),pq));
    }
  }
  return true;
}
#endif

void _mi_page_use_delayed_free(mi_page_t* page, mi_delayed_t delay, bool override_never) {
  while (!_mi_page_try_use_delayed_free(page, delay, override_never)) {
    mi_atomic_yield();
  }
}

bool _mi_page_try_use_delayed_free(mi_page_t* page, mi_delayed_t delay, bool override_never) {
  mi_thread_free_t tfreex;
  mi_delayed_t     old_delay;
  mi_thread_free_t tfree;
  size_t yield_count = 0;
  do {
    tfree = mi_atomic_load_acquire(&page->xthread_free); // note: must acquire as we can break/repeat this loop and not do a CAS;
    tfreex = mi_tf_set_delayed(tfree, delay);
    old_delay = mi_tf_delayed(tfree);
    if mi_unlikely(old_delay == MI_DELAYED_FREEING) {
      if (yield_count >= 4) return false;  // give up after 4 tries
      yield_count++;
      mi_atomic_yield(); // delay until outstanding MI_DELAYED_FREEING are done.
      // tfree = mi_tf_set_delayed(tfree, MI_NO_DELAYED_FREE); // will cause CAS to busy fail
    }
    else if (delay == old_delay) {
      break; // avoid atomic operation if already equal
    }
    else if (!override_never && old_delay == MI_NEVER_DELAYED_FREE) {
      break; // leave never-delayed flag set
    }
  } while ((old_delay == MI_DELAYED_FREEING) ||
           !mi_atomic_cas_weak_release(&page->xthread_free, &tfree, tfreex));

  return true; // success
}

/* -----------------------------------------------------------
  Page collect the `local_free` and `thread_free` lists
----------------------------------------------------------- */

// Collect the local `thread_free` list using an atomic exchange.
// Note: The exchange must be done atomically as this is used right after
// moving to the full list in `mi_page_collect_ex` and we need to
// ensure that there was no race where the page became unfull just before the move.
static void _mi_page_thread_free_collect(mi_page_t* page)
{
  mi_block_t* head;
  mi_thread_free_t tfreex;
  mi_thread_free_t tfree = mi_atomic_load_relaxed(&page->xthread_free);
  do {
    head = mi_tf_block(tfree);
    tfreex = mi_tf_set_block(tfree,NULL);
  } while (!mi_atomic_cas_weak_acq_rel(&page->xthread_free, &tfree, tfreex));

  // return if the list is empty
  if (head == NULL) return;

  // find the tail -- also to get a proper count (without data races)
  size_t max_count = page->capacity; // cannot collect more than capacity
  size_t count = 1;
  mi_block_t* tail = head;
  mi_block_t* next;
  while ((next = mi_block_next(page,tail)) != NULL && count <= max_count) {
    count++;
    tail = next;
  }
  // if `count > max_count` there was a memory corruption (possibly infinite list due to double multi-threaded free)
  if (count > max_count) {
    _mi_error_message(EFAULT, "corrupted thread-free list\n");
    return; // the thread-free items cannot be freed
  }

  // and append the current local free list
  mi_block_set_next(page,tail, page->local_free);
  page->local_free = head;

  // update counts now
  page->used -= (uint16_t)count;
}

void _mi_page_free_collect(mi_page_t* page, bool force) {
  mi_assert_internal(page!=NULL);

  // collect the thread free list
  if (force || mi_page_thread_free(page) != NULL) {  // quick test to avoid an atomic operation
    _mi_page_thread_free_collect(page);
  }

  // and the local free list
  if (page->local_free != NULL) {
    if mi_likely(page->free == NULL) {
      // usual case
      page->free = page->local_free;
      page->local_free = NULL;
      page->free_is_zero = false;
    }
    else if (force) {
      // append -- only on shutdown (force) as this is a linear operation
      mi_block_t* tail = page->local_free;
      mi_block_t* next;
      while ((next = mi_block_next(page, tail)) != NULL) {
        tail = next;
      }
      mi_block_set_next(page, tail, page->free);
      page->free = page->local_free;
      page->local_free = NULL;
      page->free_is_zero = false;
    }
  }

  mi_assert_internal(!force || page->local_free == NULL);
}



/* -----------------------------------------------------------
  Page fresh and retire
----------------------------------------------------------- */

// called from segments when reclaiming abandoned pages
void _mi_page_reclaim(mi_heap_t* heap, mi_page_t* page) {
  mi_assert_expensive(mi_page_is_valid_init(page));

  mi_assert_internal(mi_page_heap(page) == heap);
  mi_assert_internal(mi_page_thread_free_flag(page) != MI_NEVER_DELAYED_FREE);
  #if MI_HUGE_PAGE_ABANDON
  mi_assert_internal(_mi_page_segment(page)->kind != MI_SEGMENT_HUGE);
  #endif

  // TODO: push on full queue immediately if it is full?
  mi_page_queue_t* pq = mi_page_queue(heap, mi_page_block_size(page));
  mi_page_queue_push(heap, pq, page);
  mi_assert_expensive(_mi_page_is_valid(page));
}

// allocate a fresh page from a segment
static mi_page_t* mi_page_fresh_alloc(mi_heap_t* heap, mi_page_queue_t* pq, size_t block_size, size_t page_alignment) {
  #if !MI_HUGE_PAGE_ABANDON
  mi_assert_internal(pq != NULL);
  mi_assert_internal(mi_heap_contains_queue(heap, pq));
  mi_assert_internal(page_alignment > 0 || block_size > MI_MEDIUM_OBJ_SIZE_MAX || block_size == pq->block_size);
  #endif
  mi_page_t* page = _mi_segment_page_alloc(heap, block_size, page_alignment, &heap->tld->segments, &heap->tld->os);
  if (page == NULL) {
    // this may be out-of-memory, or an abandoned page was reclaimed (and in our queue)
    return NULL;
  }
  #if MI_HUGE_PAGE_ABANDON
  mi_assert_internal(pq==NULL || _mi_page_segment(page)->page_kind != MI_PAGE_HUGE);
  #endif
  mi_assert_internal(page_alignment >0 || block_size > MI_MEDIUM_OBJ_SIZE_MAX || _mi_page_segment(page)->kind != MI_SEGMENT_HUGE);
  mi_assert_internal(pq!=NULL || mi_page_block_size(page) >= block_size);
  // a fresh page was found, initialize it
  const size_t full_block_size = (pq == NULL || mi_page_is_huge(page) ? mi_page_block_size(page) : block_size); // see also: mi_segment_huge_page_alloc
  mi_assert_internal(full_block_size >= block_size);
  mi_page_init(heap, page, full_block_size, heap->tld);
  mi_heap_stat_increase(heap, pages, 1);
  if (pq != NULL) { mi_page_queue_push(heap, pq, page); }
  mi_assert_expensive(_mi_page_is_valid(page));
  return page;
}

// Get a fresh page to use
static mi_page_t* mi_page_fresh(mi_heap_t* heap, mi_page_queue_t* pq) {
  mi_assert_internal(mi_heap_contains_queue(heap, pq));
  mi_page_t* page = mi_page_fresh_alloc(heap, pq, pq->block_size, 0);
  if (page==NULL) return NULL;
  mi_assert_internal(pq->block_size==mi_page_block_size(page));
  mi_assert_internal(pq==mi_page_queue(heap, mi_page_block_size(page)));
  return page;
}

/* -----------------------------------------------------------
   Do any delayed frees
   (put there by other threads if they deallocated in a full page)
----------------------------------------------------------- */
void _mi_heap_delayed_free_all(mi_heap_t* heap) {
  while (!_mi_heap_delayed_free_partial(heap)) {
    mi_atomic_yield();
  }
}

// returns true if all delayed frees were processed
bool _mi_heap_delayed_free_partial(mi_heap_t* heap) {
  // take over the list (note: no atomic exchange since it is often NULL)
  mi_block_t* block = mi_atomic_load_ptr_relaxed(mi_block_t, &heap->thread_delayed_free);
  while (block != NULL && !mi_atomic_cas_ptr_weak_acq_rel(mi_block_t, &heap->thread_delayed_free, &block, NULL)) { /* nothing */ };
  bool all_freed = true;

  // and free them all
  while(block != NULL) {
    mi_block_t* next = mi_block_nextx(heap,block, heap->keys);
    // use internal free instead of regular one to keep stats etc correct
    if (!_mi_free_delayed_block(block)) {
      // we might already start delayed freeing while another thread has not yet
      // reset the delayed_freeing flag; in that case delay it further by reinserting the current block
      // into the delayed free list
      all_freed = false;
      mi_block_t* dfree = mi_atomic_load_ptr_relaxed(mi_block_t, &heap->thread_delayed_free);
      do {
        mi_block_set_nextx(heap, block, dfree, heap->keys);
      } while (!mi_atomic_cas_ptr_weak_release(mi_block_t,&heap->thread_delayed_free, &dfree, block));
    }
    block = next;
  }
  return all_freed;
}

/* -----------------------------------------------------------
  Unfull, abandon, free and retire
----------------------------------------------------------- */

// Move a page from the full list back to a regular list
void _mi_page_unfull(mi_page_t* page) {
  mi_assert_internal(page != NULL);
  mi_assert_expensive(_mi_page_is_valid(page));
  mi_assert_internal(mi_page_is_in_full(page));
  if (!mi_page_is_in_full(page)) return;

  mi_heap_t* heap = mi_page_heap(page);
  mi_page_queue_t* pqfull = &heap->pages[MI_BIN_FULL];
  mi_page_set_in_full(page, false); // to get the right queue
  mi_page_queue_t* pq = mi_heap_page_queue_of(heap, page);
  mi_page_set_in_full(page, true);
  mi_page_queue_enqueue_from(pq, pqfull, page);
}

static void mi_page_to_full(mi_page_t* page, mi_page_queue_t* pq) {
  mi_assert_internal(pq == mi_page_queue_of(page));
  mi_assert_internal(!mi_page_immediate_available(page));
  mi_assert_internal(!mi_page_is_in_full(page));

  if (mi_page_is_in_full(page)) return;
  mi_page_queue_enqueue_from(&mi_page_heap(page)->pages[MI_BIN_FULL], pq, page);
  _mi_page_free_collect(page,false);  // try to collect right away in case another thread freed just before MI_USE_DELAYED_FREE was set
}


// Abandon a page with used blocks at the end of a thread.
// Note: only call if it is ensured that no references exist from
// the `page->heap->thread_delayed_free` into this page.
// Currently only called through `mi_heap_collect_ex` which ensures this.
void _mi_page_abandon(mi_page_t* page, mi_page_queue_t* pq) {
  mi_assert_internal(page != NULL);
  mi_assert_expensive(_mi_page_is_valid(page));
  mi_assert_internal(pq == mi_page_queue_of(page));
  mi_assert_internal(mi_page_heap(page) != NULL);

  mi_heap_t* pheap = mi_page_heap(page);

  // remove from our page list
  mi_segments_tld_t* segments_tld = &pheap->tld->segments;
  mi_page_queue_remove(pq, page);

  // page is no longer associated with our heap
  mi_assert_internal(mi_page_thread_free_flag(page)==MI_NEVER_DELAYED_FREE);
  mi_page_set_heap(page, NULL);

#if (MI_DEBUG>1) && !MI_TRACK_ENABLED
  // check there are no references left..
  for (mi_block_t* block = (mi_block_t*)pheap->thread_delayed_free; block != NULL; block = mi_block_nextx(pheap, block, pheap->keys)) {
    mi_assert_internal(_mi_ptr_page(block) != page);
  }
#endif

  // and abandon it
  mi_assert_internal(mi_page_heap(page) == NULL);
  _mi_segment_page_abandon(page,segments_tld);
}


// Free a page with no more free blocks
void _mi_page_free(mi_page_t* page, mi_page_queue_t* pq, bool force) {
  mi_assert_internal(page != NULL);
  mi_assert_expensive(_mi_page_is_valid(page));
  mi_assert_internal(pq == mi_page_queue_of(page));
  mi_assert_internal(mi_page_all_free(page));
  mi_assert_internal(mi_page_thread_free_flag(page)!=MI_DELAYED_FREEING);

  // no more aligned blocks in here
  mi_page_set_has_aligned(page, false);

  mi_heap_t* heap = mi_page_heap(page);

  // remove from the page list
  // (no need to do _mi_heap_delayed_free first as all blocks are already free)
  mi_segments_tld_t* segments_tld = &heap->tld->segments;
  mi_page_queue_remove(pq, page);

  // and free it
  mi_page_set_heap(page,NULL);
  _mi_segment_page_free(page, force, segments_tld);
}

#define MI_MAX_RETIRE_SIZE    MI_MEDIUM_OBJ_SIZE_MAX   // should be less than size for MI_BIN_HUGE
#define MI_RETIRE_CYCLES      (16)

// Retire a page with no more used blocks
// Important to not retire too quickly though as new
// allocations might coming.
// Note: called from `mi_free` and benchmarks often
// trigger this due to freeing everything and then
// allocating again so careful when changing this.
void _mi_page_retire(mi_page_t* page) mi_attr_noexcept {
  mi_assert_internal(page != NULL);
  mi_assert_expensive(_mi_page_is_valid(page));
  mi_assert_internal(mi_page_all_free(page));

  mi_page_set_has_aligned(page, false);

  // don't retire too often..
  // (or we end up retiring and re-allocating most of the time)
  // NOTE: refine this more: we should not retire if this
  // is the only page left with free blocks. It is not clear
  // how to check this efficiently though...
  // for now, we don't retire if it is the only page left of this size class.
  mi_page_queue_t* pq = mi_page_queue_of(page);
  const size_t bsize = mi_page_block_size(page);
  if mi_likely( /* bsize < MI_MAX_RETIRE_SIZE && */ !mi_page_queue_is_special(pq)) {  // not full or huge queue?
    if (pq->last==page && pq->first==page) { // the only page in the queue?
      mi_stat_counter_increase(_mi_stats_main.page_no_retire,1);
      page->retire_expire = (bsize <= MI_SMALL_OBJ_SIZE_MAX ? MI_RETIRE_CYCLES : MI_RETIRE_CYCLES/4);
      mi_heap_t* heap = mi_page_heap(page);
      mi_assert_internal(pq >= heap->pages);
      const size_t index = pq - heap->pages;
      mi_assert_internal(index < MI_BIN_FULL && index < MI_BIN_HUGE);
      if (index < heap->page_retired_min) heap->page_retired_min = index;
      if (index > heap->page_retired_max) heap->page_retired_max = index;
      mi_assert_internal(mi_page_all_free(page));
      return; // don't free after all
    }
  }
  _mi_page_free(page, pq, false);
}

// free retired pages: we don't need to look at the entire queues
// since we only retire pages that are at the head position in a queue.
void _mi_heap_collect_retired(mi_heap_t* heap, bool force) {
  size_t min = MI_BIN_FULL;
  size_t max = 0;
  for(size_t bin = heap->page_retired_min; bin <= heap->page_retired_max; bin++) {
    mi_page_queue_t* pq   = &heap->pages[bin];
    mi_page_t*       page = pq->first;
    if (page != NULL && page->retire_expire != 0) {
      if (mi_page_all_free(page)) {
        page->retire_expire--;
        if (force || page->retire_expire == 0) {
          _mi_page_free(pq->first, pq, force);
        }
        else {
          // keep retired, update min/max
          if (bin < min) min = bin;
          if (bin > max) max = bin;
        }
      }
      else {
        page->retire_expire = 0;
      }
    }
  }
  heap->page_retired_min = min;
  heap->page_retired_max = max;
}


/* -----------------------------------------------------------
  Initialize the initial free list in a page.
  In secure mode we initialize a randomized list by
  alternating between slices.
----------------------------------------------------------- */

#define MI_MAX_SLICE_SHIFT  (6)   // at most 64 slices
#define MI_MAX_SLICES       (1UL << MI_MAX_SLICE_SHIFT)
#define MI_MIN_SLICES       (2)

static void mi_page_free_list_extend_secure(mi_heap_t* const heap, mi_page_t* const page, const size_t bsize, const size_t extend, mi_stats_t* const stats) {
  MI_UNUSED(stats);
  #if (MI_SECURE<=2)
  mi_assert_internal(page->free == NULL);
  mi_assert_internal(page->local_free == NULL);
  #endif
  mi_assert_internal(page->capacity + extend <= page->reserved);
  mi_assert_internal(bsize == mi_page_block_size(page));
  void* const page_area = mi_page_start(page);

  // initialize a randomized free list
  // set up `slice_count` slices to alternate between
  size_t shift = MI_MAX_SLICE_SHIFT;
  while ((extend >> shift) == 0) {
    shift--;
  }
  const size_t slice_count = (size_t)1U << shift;
  const size_t slice_extend = extend / slice_count;
  mi_assert_internal(slice_extend >= 1);
  mi_block_t* blocks[MI_MAX_SLICES];   // current start of the slice
  size_t      counts[MI_MAX_SLICES];   // available objects in the slice
  for (size_t i = 0; i < slice_count; i++) {
    blocks[i] = mi_page_block_at(page, page_area, bsize, page->capacity + i*slice_extend);
    counts[i] = slice_extend;
  }
  counts[slice_count-1] += (extend % slice_count);  // final slice holds the modulus too (todo: distribute evenly?)

  // and initialize the free list by randomly threading through them
  // set up first element
  const uintptr_t r = _mi_heap_random_next(heap);
  size_t current = r % slice_count;
  counts[current]--;
  mi_block_t* const free_start = blocks[current];
  // and iterate through the rest; use `random_shuffle` for performance
  uintptr_t rnd = _mi_random_shuffle(r|1); // ensure not 0
  for (size_t i = 1; i < extend; i++) {
    // call random_shuffle only every INTPTR_SIZE rounds
    const size_t round = i%MI_INTPTR_SIZE;
    if (round == 0) rnd = _mi_random_shuffle(rnd);
    // select a random next slice index
    size_t next = ((rnd >> 8*round) & (slice_count-1));
    while (counts[next]==0) {                            // ensure it still has space
      next++;
      if (next==slice_count) next = 0;
    }
    // and link the current block to it
    counts[next]--;
    mi_block_t* const block = blocks[current];
    blocks[current] = (mi_block_t*)((uint8_t*)block + bsize);  // bump to the following block
    mi_block_set_next(page, block, blocks[next]);   // and set next; note: we may have `current == next`
    current = next;
  }
  // prepend to the free list (usually NULL)
  mi_block_set_next(page, blocks[current], page->free);  // end of the list
  page->free = free_start;
}

static mi_decl_noinline void mi_page_free_list_extend( mi_page_t* const page, const size_t bsize, const size_t extend, mi_stats_t* const stats)
{
  MI_UNUSED(stats);
  #if (MI_SECURE <= 2)
  mi_assert_internal(page->free == NULL);
  mi_assert_internal(page->local_free == NULL);
  #endif
  mi_assert_internal(page->capacity + extend <= page->reserved);
  mi_assert_internal(bsize == mi_page_block_size(page));
  void* const page_area = mi_page_start(page);

  mi_block_t* const start = mi_page_block_at(page, page_area, bsize, page->capacity);

  // initialize a sequential free list
  mi_block_t* const last = mi_page_block_at(page, page_area, bsize, page->capacity + extend - 1);
  mi_block_t* block = start;
  while(block <= last) {
    mi_block_t* next = (mi_block_t*)((uint8_t*)block + bsize);
    mi_block_set_next(page,block,next);
    block = next;
  }
  // prepend to free list (usually `NULL`)
  mi_block_set_next(page, last, page->free);
  page->free = start;
}

/* -----------------------------------------------------------
  Page initialize and extend the capacity
----------------------------------------------------------- */

#define MI_MAX_EXTEND_SIZE    (4*1024)      // heuristic, one OS page seems to work well.
#if (MI_SECURE>0)
#define MI_MIN_EXTEND         (8*MI_SECURE) // extend at least by this many
#else
#define MI_MIN_EXTEND         (4)
#endif

// Extend the capacity (up to reserved) by initializing a free list
// We do at most `MI_MAX_EXTEND` to avoid touching too much memory
// Note: we also experimented with "bump" allocation on the first
// allocations but this did not speed up any benchmark (due to an
// extra test in malloc? or cache effects?)
static void mi_page_extend_free(mi_heap_t* heap, mi_page_t* page, mi_tld_t* tld) {
  MI_UNUSED(tld);
  mi_assert_expensive(mi_page_is_valid_init(page));
  #if (MI_SECURE<=2)
  mi_assert(page->free == NULL);
  mi_assert(page->local_free == NULL);
  if (page->free != NULL) return;
  #endif
  if (page->capacity >= page->reserved) return;

  mi_stat_counter_increase(tld->stats.pages_extended, 1);

  // calculate the extend count
  const size_t bsize = mi_page_block_size(page);
  size_t extend = page->reserved - page->capacity;
  mi_assert_internal(extend > 0);

  size_t max_extend = (bsize >= MI_MAX_EXTEND_SIZE ? MI_MIN_EXTEND : MI_MAX_EXTEND_SIZE/bsize);
  if (max_extend < MI_MIN_EXTEND) { max_extend = MI_MIN_EXTEND; }
  mi_assert_internal(max_extend > 0);

  if (extend > max_extend) {
    // ensure we don't touch memory beyond the page to reduce page commit.
    // the `lean` benchmark tests this. Going from 1 to 8 increases rss by 50%.
    extend = max_extend;
  }

  mi_assert_internal(extend > 0 && extend + page->capacity <= page->reserved);
  mi_assert_internal(extend < (1UL<<16));

  // and append the extend the free list
  if (extend < MI_MIN_SLICES || MI_SECURE==0) { //!mi_option_is_enabled(mi_option_secure)) {
    mi_page_free_list_extend(page, bsize, extend, &tld->stats );
  }
  else {
    mi_page_free_list_extend_secure(heap, page, bsize, extend, &tld->stats);
  }
  // enable the new free list
  page->capacity += (uint16_t)extend;
  mi_stat_increase(tld->stats.page_committed, extend * bsize);
  mi_assert_expensive(mi_page_is_valid_init(page));
}

// Initialize a fresh page
static void mi_page_init(mi_heap_t* heap, mi_page_t* page, size_t block_size, mi_tld_t* tld) {
  mi_assert(page != NULL);
  mi_segment_t* segment = _mi_page_segment(page);
  mi_assert(segment != NULL);
  mi_assert_internal(block_size > 0);
  // set fields
  mi_page_set_heap(page, heap);
  page->block_size = block_size;
  size_t page_size;
  page->page_start = _mi_segment_page_start(segment, page, &page_size);
  mi_track_mem_noaccess(page->page_start,page_size);
  mi_assert_internal(mi_page_block_size(page) <= page_size);
  mi_assert_internal(page_size <= page->slice_count*MI_SEGMENT_SLICE_SIZE);
  mi_assert_internal(page_size / block_size < (1L<<16));
  page->reserved = (uint16_t)(page_size / block_size);
  mi_assert_internal(page->reserved > 0);
  #if (MI_PADDING || MI_ENCODE_FREELIST)
  page->keys[0] = _mi_heap_random_next(heap);
  page->keys[1] = _mi_heap_random_next(heap);
  #endif
  page->free_is_zero = page->is_zero_init;
  #if MI_DEBUG>2
  if (page->is_zero_init) {
    mi_track_mem_defined(page->page_start, page_size);
    mi_assert_expensive(mi_mem_is_zero(page->page_start, page_size));
  }
  #endif
  mi_assert_internal(page->is_committed);
  if (block_size > 0 && _mi_is_power_of_two(block_size)) {
    page->block_size_shift = (uint8_t)(mi_ctz((uintptr_t)block_size));
  }
  else {
    page->block_size_shift = 0;
  }

  mi_assert_internal(page->capacity == 0);
  mi_assert_internal(page->free == NULL);
  mi_assert_internal(page->used == 0);
  mi_assert_internal(page->xthread_free == 0);
  mi_assert_internal(page->next == NULL);
  mi_assert_internal(page->prev == NULL);
  mi_assert_internal(page->retire_expire == 0);
  mi_assert_internal(!mi_page_has_aligned(page));
  #if (MI_PADDING || MI_ENCODE_FREELIST)
  mi_assert_internal(page->keys[0] != 0);
  mi_assert_internal(page->keys[1] != 0);
  #endif
  mi_assert_internal(page->block_size_shift == 0 || (block_size == ((size_t)1 << page->block_size_shift)));
  mi_assert_expensive(mi_page_is_valid_init(page));

  // initialize an initial free list
  mi_page_extend_free(heap,page,tld);
  mi_assert(mi_page_immediate_available(page));
}


/* -----------------------------------------------------------
  Find pages with free blocks
-------------------------------------------------------------*/

// Find a page with free blocks of `page->block_size`.
static mi_page_t* mi_page_queue_find_free_ex(mi_heap_t* heap, mi_page_queue_t* pq, bool first_try)
{
  // search through the pages in "next fit" order
  #if MI_STAT
  size_t count = 0;
  #endif
  mi_page_t* page = pq->first;
  while (page != NULL)
  {
    mi_page_t* next = page->next; // remember next
    #if MI_STAT
    count++;
    #endif

    // 0. collect freed blocks by us and other threads
    _mi_page_free_collect(page, false);

    // 1. if the page contains free blocks, we are done
    if (mi_page_immediate_available(page)) {
      break;  // pick this one
    }

    // 2. Try to extend
    if (page->capacity < page->reserved) {
      mi_page_extend_free(heap, page, heap->tld);
      mi_assert_internal(mi_page_immediate_available(page));
      break;
    }

    // 3. If the page is completely full, move it to the `mi_pages_full`
    // queue so we don't visit long-lived pages too often.
    mi_assert_internal(!mi_page_is_in_full(page) && !mi_page_immediate_available(page));
    mi_page_to_full(page, pq);

    page = next;
  } // for each page

  mi_heap_stat_counter_increase(heap, searches, count);

  if (page == NULL) {
    _mi_heap_collect_retired(heap, false); // perhaps make a page available?
    page = mi_page_fresh(heap, pq);
    if (page == NULL && first_try) {
      // out-of-memory _or_ an abandoned page with free blocks was reclaimed, try once again
      page = mi_page_queue_find_free_ex(heap, pq, false);
    }
  }
  else {
    mi_assert(pq->first == page);
    page->retire_expire = 0;
  }
  mi_assert_internal(page == NULL || mi_page_immediate_available(page));
  return page;
}



// Find a page with free blocks of `size`.
static inline mi_page_t* mi_find_free_page(mi_heap_t* heap, size_t size) {
  mi_page_queue_t* pq = mi_page_queue(heap,size);
  mi_page_t* page = pq->first;
  if (page != NULL) {
   #if (MI_SECURE>=3) // in secure mode, we extend half the time to increase randomness
    if (page->capacity < page->reserved && ((_mi_heap_random_next(heap) & 1) == 1)) {
      mi_page_extend_free(heap, page, heap->tld);
      mi_assert_internal(mi_page_immediate_available(page));
    }
    else
   #endif
    {
      _mi_page_free_collect(page,false);
    }

    if (mi_page_immediate_available(page)) {
      page->retire_expire = 0;
      return page; // fast path
    }
  }
  return mi_page_queue_find_free_ex(heap, pq, true);
}


/* -----------------------------------------------------------
  Users can register a deferred free function called
  when the `free` list is empty. Since the `local_free`
  is separate this is deterministically called after
  a certain number of allocations.
----------------------------------------------------------- */

static mi_deferred_free_fun* volatile deferred_free = NULL;
static _Atomic(void*) deferred_arg; // = NULL

void _mi_deferred_free(mi_heap_t* heap, bool force) {
  heap->tld->heartbeat++;
  if (deferred_free != NULL && !heap->tld->recurse) {
    heap->tld->recurse = true;
    deferred_free(force, heap->tld->heartbeat, mi_atomic_load_ptr_relaxed(void,&deferred_arg));
    heap->tld->recurse = false;
  }
}

void mi_register_deferred_free(mi_deferred_free_fun* fn, void* arg) mi_attr_noexcept {
  deferred_free = fn;
  mi_atomic_store_ptr_release(void,&deferred_arg, arg);
}


/* -----------------------------------------------------------
  General allocation
----------------------------------------------------------- */

// Large and huge page allocation.
// Huge pages contain just one block, and the segment contains just that page (as `MI_SEGMENT_HUGE`).
// Huge pages are also use if the requested alignment is very large (> MI_BLOCK_ALIGNMENT_MAX)
// so their size is not always `> MI_LARGE_OBJ_SIZE_MAX`.
static mi_page_t* mi_large_huge_page_alloc(mi_heap_t* heap, size_t size, size_t page_alignment) {
  size_t block_size = _mi_os_good_alloc_size(size);
  mi_assert_internal(mi_bin(block_size) == MI_BIN_HUGE || page_alignment > 0);
  bool is_huge = (block_size > MI_LARGE_OBJ_SIZE_MAX || page_alignment > 0);
  #if MI_HUGE_PAGE_ABANDON
  mi_page_queue_t* pq = (is_huge ? NULL : mi_page_queue(heap, block_size));
  #else
  mi_page_queue_t* pq = mi_page_queue(heap, is_huge ? MI_LARGE_OBJ_SIZE_MAX+1 : block_size);
  mi_assert_internal(!is_huge || mi_page_queue_is_huge(pq));
  #endif
  mi_page_t* page = mi_page_fresh_alloc(heap, pq, block_size, page_alignment);
  if (page != NULL) {
    mi_assert_internal(mi_page_immediate_available(page));

    if (is_huge) {
      mi_assert_internal(mi_page_is_huge(page));
      mi_assert_internal(_mi_page_segment(page)->kind == MI_SEGMENT_HUGE);
      mi_assert_internal(_mi_page_segment(page)->used==1);
      #if MI_HUGE_PAGE_ABANDON
      mi_assert_internal(_mi_page_segment(page)->thread_id==0); // abandoned, not in the huge queue
      mi_page_set_heap(page, NULL);
      #endif
    }
    else {
      mi_assert_internal(!mi_page_is_huge(page));
    }

    const size_t bsize = mi_page_usable_block_size(page);  // note: not `mi_page_block_size` to account for padding
    if (bsize <= MI_LARGE_OBJ_SIZE_MAX) {
      mi_heap_stat_increase(heap, large, bsize);
      mi_heap_stat_counter_increase(heap, large_count, 1);
    }
    else {
      mi_heap_stat_increase(heap, huge, bsize);
      mi_heap_stat_counter_increase(heap, huge_count, 1);
    }
  }
  return page;
}


// Allocate a page
// Note: in debug mode the size includes MI_PADDING_SIZE and might have overflowed.
static mi_page_t* mi_find_page(mi_heap_t* heap, size_t size, size_t huge_alignment) mi_attr_noexcept {
  // huge allocation?
  const size_t req_size = size - MI_PADDING_SIZE;  // correct for padding_size in case of an overflow on `size`
  if mi_unlikely(req_size > (MI_MEDIUM_OBJ_SIZE_MAX - MI_PADDING_SIZE) || huge_alignment > 0) {
    if mi_unlikely(req_size > MI_MAX_ALLOC_SIZE) {
      _mi_error_message(EOVERFLOW, "allocation request is too large (%zu bytes)\n", req_size);
      return NULL;
    }
    else {
      return mi_large_huge_page_alloc(heap,size,huge_alignment);
    }
  }
  else {
    // otherwise find a page with free blocks in our size segregated queues
    #if MI_PADDING
    mi_assert_internal(size >= MI_PADDING_SIZE);
    #endif
    return mi_find_free_page(heap, size);
  }
}

// Generic allocation routine if the fast path (`alloc.c:mi_page_malloc`) does not succeed.
// Note: in debug mode the size includes MI_PADDING_SIZE and might have overflowed.
// The `huge_alignment` is normally 0 but is set to a multiple of MI_SEGMENT_SIZE for
// very large requested alignments in which case we use a huge segment.
void* _mi_malloc_generic(mi_heap_t* heap, size_t size, bool zero, size_t huge_alignment) mi_attr_noexcept
{
  mi_assert_internal(heap != NULL);

  // initialize if necessary
  if mi_unlikely(!mi_heap_is_initialized(heap)) {
    heap = mi_heap_get_default(); // calls mi_thread_init
    if mi_unlikely(!mi_heap_is_initialized(heap)) { return NULL; }
  }
  mi_assert_internal(mi_heap_is_initialized(heap));

  // call potential deferred free routines
  _mi_deferred_free(heap, false);

  // free delayed frees from other threads (but skip contended ones)
  _mi_heap_delayed_free_partial(heap);

  // find (or allocate) a page of the right size
  mi_page_t* page = mi_find_page(heap, size, huge_alignment);
  if mi_unlikely(page == NULL) { // first time out of memory, try to collect and retry the allocation once more
    mi_heap_collect(heap, true /* force */);
    page = mi_find_page(heap, size, huge_alignment);
  }

  if mi_unlikely(page == NULL) { // out of memory
    const size_t req_size = size - MI_PADDING_SIZE;  // correct for padding_size in case of an overflow on `size`
    _mi_error_message(ENOMEM, "unable to allocate memory (%zu bytes)\n", req_size);
    return NULL;
  }

  mi_assert_internal(mi_page_immediate_available(page));
  mi_assert_internal(mi_page_block_size(page) >= size);

  // and try again, this time succeeding! (i.e. this should never recurse through _mi_page_malloc)
  if mi_unlikely(zero && page->block_size == 0) {
    // note: we cannot call _mi_page_malloc with zeroing for huge blocks; we zero it afterwards in that case.
    void* p = _mi_page_malloc(heap, page, size);
    mi_assert_internal(p != NULL);
    _mi_memzero_aligned(p, mi_page_usable_block_size(page));
    return p;
  }
  else {
    return _mi_page_malloc_zero(heap, page, size, zero);
  }
}
