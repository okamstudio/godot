/* ----------------------------------------------------------------------------
Copyright (c) 2018-2024, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/
#if !defined(MI_IN_ALLOC_C)
#error "this file should be included from 'alloc.c' (so aliases can work from alloc-override)"
// add includes help an IDE
#include "mimalloc.h"
#include "mimalloc/internal.h"
#include "mimalloc/atomic.h"
#include "mimalloc/prim.h"   // _mi_prim_thread_id()
#endif

// forward declarations
static void   mi_check_padding(const mi_page_t* page, const mi_block_t* block);
static bool   mi_check_is_double_free(const mi_page_t* page, const mi_block_t* block);
static size_t mi_page_usable_size_of(const mi_page_t* page, const mi_block_t* block);
static void   mi_stat_free(const mi_page_t* page, const mi_block_t* block);


// ------------------------------------------------------
// Free
// ------------------------------------------------------

// forward declaration of multi-threaded free (`_mt`) (or free in huge block if compiled with MI_HUGE_PAGE_ABANDON)
static mi_decl_noinline void mi_free_block_mt(mi_page_t* page, mi_segment_t* segment, mi_block_t* block);

// regular free of a (thread local) block pointer
// fast path written carefully to prevent spilling on the stack
static inline void mi_free_block_local(mi_page_t* page, mi_block_t* block, bool track_stats, bool check_full)
{
  // checks
  if mi_unlikely(mi_check_is_double_free(page, block)) return;
  mi_check_padding(page, block);
  if (track_stats) { mi_stat_free(page, block); }
  #if (MI_DEBUG>0) && !MI_TRACK_ENABLED  && !MI_TSAN
  if (!mi_page_is_huge(page)) {   // huge page content may be already decommitted
    memset(block, MI_DEBUG_FREED, mi_page_block_size(page));
  }
  #endif
  if (track_stats) { mi_track_free_size(block, mi_page_usable_size_of(page, block)); } // faster then mi_usable_size as we already know the page and that p is unaligned

  // actual free: push on the local free list
  mi_block_set_next(page, block, page->local_free);
  page->local_free = block;
  if mi_unlikely(--page->used == 0) {
    _mi_page_retire(page);
  }
  else if mi_unlikely(check_full && mi_page_is_in_full(page)) {
    _mi_page_unfull(page);
  }
}

// Adjust a block that was allocated aligned, to the actual start of the block in the page.
// note: this can be called from `mi_free_generic_mt` where a non-owning thread accesses the 
// `page_start` and `block_size` fields; however these are constant and the page won't be 
// deallocated (as the block we are freeing keeps it alive) and thus safe to read concurrently.
mi_block_t* _mi_page_ptr_unalign(const mi_page_t* page, const void* p) {
  mi_assert_internal(page!=NULL && p!=NULL);

  size_t diff = (uint8_t*)p - page->page_start;
  size_t adjust;
  if mi_likely(page->block_size_shift != 0) {
    adjust = diff & (((size_t)1 << page->block_size_shift) - 1);
  }
  else {
    adjust = diff % mi_page_block_size(page);
  }

  return (mi_block_t*)((uintptr_t)p - adjust);
}

// free a local pointer  (page parameter comes first for better codegen)
static void mi_decl_noinline mi_free_generic_local(mi_page_t* page, mi_segment_t* segment, void* p) mi_attr_noexcept {
  MI_UNUSED(segment);
  mi_block_t* const block = (mi_page_has_aligned(page) ? _mi_page_ptr_unalign(page, p) : (mi_block_t*)p);
  mi_free_block_local(page, block, true /* track stats */, true /* check for a full page */);
}

// free a pointer owned by another thread (page parameter comes first for better codegen)
static void mi_decl_noinline mi_free_generic_mt(mi_page_t* page, mi_segment_t* segment, void* p) mi_attr_noexcept {
  mi_block_t* const block = _mi_page_ptr_unalign(page, p); // don't check `has_aligned` flag to avoid a race (issue #865)
  mi_free_block_mt(page, segment, block);
}

// generic free (for runtime integration)
void mi_decl_noinline _mi_free_generic(mi_segment_t* segment, mi_page_t* page, bool is_local, void* p) mi_attr_noexcept {
  if (is_local) mi_free_generic_local(page,segment,p);
           else mi_free_generic_mt(page,segment,p);
}

// Get the segment data belonging to a pointer
// This is just a single `and` in release mode but does further checks in debug mode
// (and secure mode) to see if this was a valid pointer.
static inline mi_segment_t* mi_checked_ptr_segment(const void* p, const char* msg)
{
  MI_UNUSED(msg);

#if (MI_DEBUG>0)
  if mi_unlikely(((uintptr_t)p & (MI_INTPTR_SIZE - 1)) != 0) {
    _mi_error_message(EINVAL, "%s: invalid (unaligned) pointer: %p\n", msg, p);
    return NULL;
  }
#endif

  mi_segment_t* const segment = _mi_ptr_segment(p);
  if mi_unlikely(segment==NULL) return segment;

#if (MI_DEBUG>0)
  if mi_unlikely(!mi_is_in_heap_region(p)) {
  #if (MI_INTPTR_SIZE == 8 && defined(__linux__))
    if (((uintptr_t)p >> 40) != 0x7F) { // linux tends to align large blocks above 0x7F000000000 (issue #640)
  #else
    {
  #endif
      _mi_warning_message("%s: pointer might not point to a valid heap region: %p\n"
        "(this may still be a valid very large allocation (over 64MiB))\n", msg, p);
      if mi_likely(_mi_ptr_cookie(segment) == segment->cookie) {
        _mi_warning_message("(yes, the previous pointer %p was valid after all)\n", p);
      }
    }
  }
#endif
#if (MI_DEBUG>0 || MI_SECURE>=4)
  if mi_unlikely(_mi_ptr_cookie(segment) != segment->cookie) {
    _mi_error_message(EINVAL, "%s: pointer does not point to a valid heap space: %p\n", msg, p);
    return NULL;
  }
#endif

  return segment;
}

// Free a block
// Fast path written carefully to prevent register spilling on the stack
void mi_free(void* p) mi_attr_noexcept
{
  mi_segment_t* const segment = mi_checked_ptr_segment(p,"mi_free");
  if mi_unlikely(segment==NULL) return;

  const bool is_local = (_mi_prim_thread_id() == mi_atomic_load_relaxed(&segment->thread_id));
  mi_page_t* const page = _mi_segment_page_of(segment, p);

  if mi_likely(is_local) {                        // thread-local free?
    if mi_likely(page->flags.full_aligned == 0) { // and it is not a full page (full pages need to move from the full bin), nor has aligned blocks (aligned blocks need to be unaligned)
      // thread-local, aligned, and not a full page
      mi_block_t* const block = (mi_block_t*)p;
      mi_free_block_local(page, block, true /* track stats */, false /* no need to check if the page is full */);
    }
    else {
      // page is full or contains (inner) aligned blocks; use generic path
      mi_free_generic_local(page, segment, p);
    }
  }
  else {
    // not thread-local; use generic path
    mi_free_generic_mt(page, segment, p);
  }
}

// return true if successful
bool _mi_free_delayed_block(mi_block_t* block) {
  // get segment and page
  mi_assert_internal(block!=NULL);
  const mi_segment_t* const segment = _mi_ptr_segment(block);
  mi_assert_internal(_mi_ptr_cookie(segment) == segment->cookie);
  mi_assert_internal(_mi_thread_id() == segment->thread_id);
  mi_page_t* const page = _mi_segment_page_of(segment, block);

  // Clear the no-delayed flag so delayed freeing is used again for this page.
  // This must be done before collecting the free lists on this page -- otherwise
  // some blocks may end up in the page `thread_free` list with no blocks in the
  // heap `thread_delayed_free` list which may cause the page to be never freed!
  // (it would only be freed if we happen to scan it in `mi_page_queue_find_free_ex`)
  if (!_mi_page_try_use_delayed_free(page, MI_USE_DELAYED_FREE, false /* dont overwrite never delayed */)) {
    return false;
  }

  // collect all other non-local frees (move from `thread_free` to `free`) to ensure up-to-date `used` count
  _mi_page_free_collect(page, false);

  // and free the block (possibly freeing the page as well since `used` is updated)
  mi_free_block_local(page, block, false /* stats have already been adjusted */, true /* check for a full page */);
  return true;
}

// ------------------------------------------------------
// Multi-threaded Free (`_mt`)
// ------------------------------------------------------

// Push a block that is owned by another thread on its page-local thread free
// list or it's heap delayed free list. Such blocks are later collected by
// the owning thread in `_mi_free_delayed_block`.
static void mi_decl_noinline mi_free_block_delayed_mt( mi_page_t* page, mi_block_t* block )
{
  // Try to put the block on either the page-local thread free list,
  // or the heap delayed free list (if this is the first non-local free in that page)
  mi_thread_free_t tfreex;
  bool use_delayed;
  mi_thread_free_t tfree = mi_atomic_load_relaxed(&page->xthread_free);
  do {
    use_delayed = (mi_tf_delayed(tfree) == MI_USE_DELAYED_FREE);
    if mi_unlikely(use_delayed) {
      // unlikely: this only happens on the first concurrent free in a page that is in the full list
      tfreex = mi_tf_set_delayed(tfree,MI_DELAYED_FREEING);
    }
    else {
      // usual: directly add to page thread_free list
      mi_block_set_next(page, block, mi_tf_block(tfree));
      tfreex = mi_tf_set_block(tfree,block);
    }
  } while (!mi_atomic_cas_weak_release(&page->xthread_free, &tfree, tfreex));

  // If this was the first non-local free, we need to push it on the heap delayed free list instead
  if mi_unlikely(use_delayed) {
    // racy read on `heap`, but ok because MI_DELAYED_FREEING is set (see `mi_heap_delete` and `mi_heap_collect_abandon`)
    mi_heap_t* const heap = (mi_heap_t*)(mi_atomic_load_acquire(&page->xheap)); //mi_page_heap(page);
    mi_assert_internal(heap != NULL);
    if (heap != NULL) {
      // add to the delayed free list of this heap. (do this atomically as the lock only protects heap memory validity)
      mi_block_t* dfree = mi_atomic_load_ptr_relaxed(mi_block_t, &heap->thread_delayed_free);
      do {
        mi_block_set_nextx(heap,block,dfree, heap->keys);
      } while (!mi_atomic_cas_ptr_weak_release(mi_block_t,&heap->thread_delayed_free, &dfree, block));
    }

    // and reset the MI_DELAYED_FREEING flag
    tfree = mi_atomic_load_relaxed(&page->xthread_free);
    do {
      tfreex = tfree;
      mi_assert_internal(mi_tf_delayed(tfree) == MI_DELAYED_FREEING);
      tfreex = mi_tf_set_delayed(tfree,MI_NO_DELAYED_FREE);
    } while (!mi_atomic_cas_weak_release(&page->xthread_free, &tfree, tfreex));
  }
}

// Multi-threaded free (`_mt`) (or free in huge block if compiled with MI_HUGE_PAGE_ABANDON)
static void mi_decl_noinline mi_free_block_mt(mi_page_t* page, mi_segment_t* segment, mi_block_t* block)
{
  // first see if the segment was abandoned and if we can reclaim it into our thread
  if (mi_option_is_enabled(mi_option_abandoned_reclaim_on_free) &&
      #if MI_HUGE_PAGE_ABANDON
      segment->page_kind != MI_PAGE_HUGE &&
      #endif
      mi_atomic_load_relaxed(&segment->thread_id) == 0)
  {
    // the segment is abandoned, try to reclaim it into our heap
    if (_mi_segment_attempt_reclaim(mi_heap_get_default(), segment)) {
      mi_assert_internal(_mi_prim_thread_id() == mi_atomic_load_relaxed(&segment->thread_id));
      mi_free(block);  // recursively free as now it will be a local free in our heap
      return;
    }
  }

  // The padding check may access the non-thread-owned page for the key values.
  // that is safe as these are constant and the page won't be freed (as the block is not freed yet).
  mi_check_padding(page, block);

  // adjust stats (after padding check and potentially recursive `mi_free` above)
  mi_stat_free(page, block);    // stat_free may access the padding
  mi_track_free_size(block, mi_page_usable_size_of(page,block));

  // for small size, ensure we can fit the delayed thread pointers without triggering overflow detection
  _mi_padding_shrink(page, block, sizeof(mi_block_t));

  if (segment->kind == MI_SEGMENT_HUGE) {
    #if MI_HUGE_PAGE_ABANDON
    // huge page segments are always abandoned and can be freed immediately
    _mi_segment_huge_page_free(segment, page, block);
    return;
    #else
    // huge pages are special as they occupy the entire segment
    // as these are large we reset the memory occupied by the page so it is available to other threads
    // (as the owning thread needs to actually free the memory later).
    _mi_segment_huge_page_reset(segment, page, block);
    #endif
  }
  else {
    #if (MI_DEBUG>0) && !MI_TRACK_ENABLED  && !MI_TSAN       // note: when tracking, cannot use mi_usable_size with multi-threading
    memset(block, MI_DEBUG_FREED, mi_usable_size(block));
    #endif
  }

  // and finally free the actual block by pushing it on the owning heap
  // thread_delayed free list (or heap delayed free list)
  mi_free_block_delayed_mt(page,block);
}


// ------------------------------------------------------
// Usable size
// ------------------------------------------------------

// Bytes available in a block
static size_t mi_decl_noinline mi_page_usable_aligned_size_of(const mi_page_t* page, const void* p) mi_attr_noexcept {
  const mi_block_t* block = _mi_page_ptr_unalign(page, p);
  const size_t size = mi_page_usable_size_of(page, block);
  const ptrdiff_t adjust = (uint8_t*)p - (uint8_t*)block;
  mi_assert_internal(adjust >= 0 && (size_t)adjust <= size);
  return (size - adjust);
}

static inline size_t _mi_usable_size(const void* p, const char* msg) mi_attr_noexcept {
  const mi_segment_t* const segment = mi_checked_ptr_segment(p, msg);
  if mi_unlikely(segment==NULL) return 0;
  const mi_page_t* const page = _mi_segment_page_of(segment, p);
  if mi_likely(!mi_page_has_aligned(page)) {
    const mi_block_t* block = (const mi_block_t*)p;
    return mi_page_usable_size_of(page, block);
  }
  else {
    // split out to separate routine for improved code generation
    return mi_page_usable_aligned_size_of(page, p);
  }
}

mi_decl_nodiscard size_t mi_usable_size(const void* p) mi_attr_noexcept {
  return _mi_usable_size(p, "mi_usable_size");
}


// ------------------------------------------------------
// Free variants
// ------------------------------------------------------

void mi_free_size(void* p, size_t size) mi_attr_noexcept {
  MI_UNUSED_RELEASE(size);
  mi_assert(p == NULL || size <= _mi_usable_size(p,"mi_free_size"));
  mi_free(p);
}

void mi_free_size_aligned(void* p, size_t size, size_t alignment) mi_attr_noexcept {
  MI_UNUSED_RELEASE(alignment);
  mi_assert(((uintptr_t)p % alignment) == 0);
  mi_free_size(p,size);
}

void mi_free_aligned(void* p, size_t alignment) mi_attr_noexcept {
  MI_UNUSED_RELEASE(alignment);
  mi_assert(((uintptr_t)p % alignment) == 0);
  mi_free(p);
}


// ------------------------------------------------------
// Check for double free in secure and debug mode
// This is somewhat expensive so only enabled for secure mode 4
// ------------------------------------------------------

#if (MI_ENCODE_FREELIST && (MI_SECURE>=4 || MI_DEBUG!=0))
// linear check if the free list contains a specific element
static bool mi_list_contains(const mi_page_t* page, const mi_block_t* list, const mi_block_t* elem) {
  while (list != NULL) {
    if (elem==list) return true;
    list = mi_block_next(page, list);
  }
  return false;
}

static mi_decl_noinline bool mi_check_is_double_freex(const mi_page_t* page, const mi_block_t* block) {
  // The decoded value is in the same page (or NULL).
  // Walk the free lists to verify positively if it is already freed
  if (mi_list_contains(page, page->free, block) ||
      mi_list_contains(page, page->local_free, block) ||
      mi_list_contains(page, mi_page_thread_free(page), block))
  {
    _mi_error_message(EAGAIN, "double free detected of block %p with size %zu\n", block, mi_page_block_size(page));
    return true;
  }
  return false;
}

#define mi_track_page(page,access)  { size_t psize; void* pstart = _mi_page_start(_mi_page_segment(page),page,&psize); mi_track_mem_##access( pstart, psize); }

static inline bool mi_check_is_double_free(const mi_page_t* page, const mi_block_t* block) {
  bool is_double_free = false;
  mi_block_t* n = mi_block_nextx(page, block, page->keys); // pretend it is freed, and get the decoded first field
  if (((uintptr_t)n & (MI_INTPTR_SIZE-1))==0 &&  // quick check: aligned pointer?
      (n==NULL || mi_is_in_same_page(block, n))) // quick check: in same page or NULL?
  {
    // Suspicious: decoded value a in block is in the same page (or NULL) -- maybe a double free?
    // (continue in separate function to improve code generation)
    is_double_free = mi_check_is_double_freex(page, block);
  }
  return is_double_free;
}
#else
static inline bool mi_check_is_double_free(const mi_page_t* page, const mi_block_t* block) {
  MI_UNUSED(page);
  MI_UNUSED(block);
  return false;
}
#endif


// ---------------------------------------------------------------------------
// Check for heap block overflow by setting up padding at the end of the block
// ---------------------------------------------------------------------------

#if MI_PADDING // && !MI_TRACK_ENABLED
static bool mi_page_decode_padding(const mi_page_t* page, const mi_block_t* block, size_t* delta, size_t* bsize) {
  *bsize = mi_page_usable_block_size(page);
  const mi_padding_t* const padding = (mi_padding_t*)((uint8_t*)block + *bsize);
  mi_track_mem_defined(padding,sizeof(mi_padding_t));
  *delta = padding->delta;
  uint32_t canary = padding->canary;
  uintptr_t keys[2];
  keys[0] = page->keys[0];
  keys[1] = page->keys[1];
  bool ok = ((uint32_t)mi_ptr_encode(page,block,keys) == canary && *delta <= *bsize);
  mi_track_mem_noaccess(padding,sizeof(mi_padding_t));
  return ok;
}

// Return the exact usable size of a block.
static size_t mi_page_usable_size_of(const mi_page_t* page, const mi_block_t* block) {
  size_t bsize;
  size_t delta;
  bool ok = mi_page_decode_padding(page, block, &delta, &bsize);
  mi_assert_internal(ok); mi_assert_internal(delta <= bsize);
  return (ok ? bsize - delta : 0);
}

// When a non-thread-local block is freed, it becomes part of the thread delayed free
// list that is freed later by the owning heap. If the exact usable size is too small to
// contain the pointer for the delayed list, then shrink the padding (by decreasing delta)
// so it will later not trigger an overflow error in `mi_free_block`.
void _mi_padding_shrink(const mi_page_t* page, const mi_block_t* block, const size_t min_size) {
  size_t bsize;
  size_t delta;
  bool ok = mi_page_decode_padding(page, block, &delta, &bsize);
  mi_assert_internal(ok);
  if (!ok || (bsize - delta) >= min_size) return;  // usually already enough space
  mi_assert_internal(bsize >= min_size);
  if (bsize < min_size) return;  // should never happen
  size_t new_delta = (bsize - min_size);
  mi_assert_internal(new_delta < bsize);
  mi_padding_t* padding = (mi_padding_t*)((uint8_t*)block + bsize);
  mi_track_mem_defined(padding,sizeof(mi_padding_t));
  padding->delta = (uint32_t)new_delta;
  mi_track_mem_noaccess(padding,sizeof(mi_padding_t));
}
#else
static size_t mi_page_usable_size_of(const mi_page_t* page, const mi_block_t* block) {
  MI_UNUSED(block);
  return mi_page_usable_block_size(page);
}

void _mi_padding_shrink(const mi_page_t* page, const mi_block_t* block, const size_t min_size) {
  MI_UNUSED(page);
  MI_UNUSED(block);
  MI_UNUSED(min_size);
}
#endif

#if MI_PADDING && MI_PADDING_CHECK

static bool mi_verify_padding(const mi_page_t* page, const mi_block_t* block, size_t* size, size_t* wrong) {
  size_t bsize;
  size_t delta;
  bool ok = mi_page_decode_padding(page, block, &delta, &bsize);
  *size = *wrong = bsize;
  if (!ok) return false;
  mi_assert_internal(bsize >= delta);
  *size = bsize - delta;
  if (!mi_page_is_huge(page)) {
    uint8_t* fill = (uint8_t*)block + bsize - delta;
    const size_t maxpad = (delta > MI_MAX_ALIGN_SIZE ? MI_MAX_ALIGN_SIZE : delta); // check at most the first N padding bytes
    mi_track_mem_defined(fill, maxpad);
    for (size_t i = 0; i < maxpad; i++) {
      if (fill[i] != MI_DEBUG_PADDING) {
        *wrong = bsize - delta + i;
        ok = false;
        break;
      }
    }
    mi_track_mem_noaccess(fill, maxpad);
  }
  return ok;
}

static void mi_check_padding(const mi_page_t* page, const mi_block_t* block) {
  size_t size;
  size_t wrong;
  if (!mi_verify_padding(page,block,&size,&wrong)) {
    _mi_error_message(EFAULT, "buffer overflow in heap block %p of size %zu: write after %zu bytes\n", block, size, wrong );
  }
}

#else

static void mi_check_padding(const mi_page_t* page, const mi_block_t* block) {
  MI_UNUSED(page);
  MI_UNUSED(block);
}

#endif

// only maintain stats for smaller objects if requested
#if (MI_STAT>0)
static void mi_stat_free(const mi_page_t* page, const mi_block_t* block) {
  #if (MI_STAT < 2)
  MI_UNUSED(block);
  #endif
  mi_heap_t* const heap = mi_heap_get_default();
  const size_t bsize = mi_page_usable_block_size(page);
  #if (MI_STAT>1)
  const size_t usize = mi_page_usable_size_of(page, block);
  mi_heap_stat_decrease(heap, malloc, usize);
  #endif
  if (bsize <= MI_MEDIUM_OBJ_SIZE_MAX) {
    mi_heap_stat_decrease(heap, normal, bsize);
    #if (MI_STAT > 1)
    mi_heap_stat_decrease(heap, normal_bins[_mi_bin(bsize)], 1);
    #endif
  }
  else if (bsize <= MI_LARGE_OBJ_SIZE_MAX) {
    mi_heap_stat_decrease(heap, large, bsize);
  }
  else {
    mi_heap_stat_decrease(heap, huge, bsize);
  }
}
#else
static void mi_stat_free(const mi_page_t* page, const mi_block_t* block) {
  MI_UNUSED(page); MI_UNUSED(block);
}
#endif
