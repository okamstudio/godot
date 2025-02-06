/* ----------------------------------------------------------------------------
Copyright (c) 2018-2023, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/
#pragma once
#ifndef MIMALLOC_INTERNAL_H
#define MIMALLOC_INTERNAL_H


// --------------------------------------------------------------------------
// This file contains the interal API's of mimalloc and various utility
// functions and macros.
// --------------------------------------------------------------------------

#include "types.h"
#include "track.h"

#if (MI_DEBUG>0)
#define mi_trace_message(...)  _mi_trace_message(__VA_ARGS__)
#else
#define mi_trace_message(...)
#endif

#define MI_CACHE_LINE          64
#if defined(_MSC_VER)
#pragma warning(disable:4127)   // suppress constant conditional warning (due to MI_SECURE paths)
#pragma warning(disable:26812)  // unscoped enum warning
#define mi_decl_noinline        __declspec(noinline)
#define mi_decl_thread          __declspec(thread)
#define mi_decl_cache_align     __declspec(align(MI_CACHE_LINE))
#define mi_decl_weak
#elif (defined(__GNUC__) && (__GNUC__ >= 3)) || defined(__clang__) // includes clang and icc
#define mi_decl_noinline        __attribute__((noinline))
#define mi_decl_thread          __thread
#define mi_decl_cache_align     __attribute__((aligned(MI_CACHE_LINE)))
#define mi_decl_weak            __attribute__((weak))
#else
#define mi_decl_noinline
#define mi_decl_thread          __thread        // hope for the best :-)
#define mi_decl_cache_align
#define mi_decl_weak
#endif

#if defined(__EMSCRIPTEN__) && !defined(__wasi__)
#define __wasi__
#endif

#if defined(__cplusplus)
#define mi_decl_externc       extern "C"
#else
#define mi_decl_externc
#endif

// pthreads
#if !defined(_WIN32) && !defined(__wasi__)
#define  MI_USE_PTHREADS
#include <pthread.h>
#endif

// "options.c"
void       _mi_fputs(mi_output_fun* out, void* arg, const char* prefix, const char* message);
void       _mi_fprintf(mi_output_fun* out, void* arg, const char* fmt, ...);
void       _mi_warning_message(const char* fmt, ...);
void       _mi_verbose_message(const char* fmt, ...);
void       _mi_trace_message(const char* fmt, ...);
void       _mi_options_init(void);
void       _mi_error_message(int err, const char* fmt, ...);

// random.c
void       _mi_random_init(mi_random_ctx_t* ctx);
void       _mi_random_init_weak(mi_random_ctx_t* ctx);
void       _mi_random_reinit_if_weak(mi_random_ctx_t * ctx);
void       _mi_random_split(mi_random_ctx_t* ctx, mi_random_ctx_t* new_ctx);
uintptr_t  _mi_random_next(mi_random_ctx_t* ctx);
uintptr_t  _mi_heap_random_next(mi_heap_t* heap);
uintptr_t  _mi_os_random_weak(uintptr_t extra_seed);
static inline uintptr_t _mi_random_shuffle(uintptr_t x);

// init.c
extern mi_decl_cache_align mi_stats_t       _mi_stats_main;
extern mi_decl_cache_align const mi_page_t  _mi_page_empty;
bool       _mi_is_main_thread(void);
size_t     _mi_current_thread_count(void);
bool       _mi_preloading(void);           // true while the C runtime is not initialized yet
mi_threadid_t _mi_thread_id(void) mi_attr_noexcept;
mi_heap_t*    _mi_heap_main_get(void);     // statically allocated main backing heap
void       _mi_thread_done(mi_heap_t* heap);
void       _mi_thread_data_collect(void);
void       _mi_tld_init(mi_tld_t* tld, mi_heap_t* bheap);

// os.c
void       _mi_os_init(void);                                            // called from process init
void*      _mi_os_alloc(size_t size, mi_memid_t* memid, mi_stats_t* stats);
void       _mi_os_free(void* p, size_t size, mi_memid_t memid, mi_stats_t* stats);
void       _mi_os_free_ex(void* p, size_t size, bool still_committed, mi_memid_t memid, mi_stats_t* stats);

size_t     _mi_os_page_size(void);
size_t     _mi_os_good_alloc_size(size_t size);
bool       _mi_os_has_overcommit(void);
bool       _mi_os_has_virtual_reserve(void);

bool       _mi_os_purge(void* p, size_t size, mi_stats_t* stats);
bool       _mi_os_reset(void* addr, size_t size, mi_stats_t* tld_stats);
bool       _mi_os_commit(void* p, size_t size, bool* is_zero, mi_stats_t* stats);
bool       _mi_os_decommit(void* addr, size_t size, mi_stats_t* stats);
bool       _mi_os_protect(void* addr, size_t size);
bool       _mi_os_unprotect(void* addr, size_t size);
bool       _mi_os_purge(void* p, size_t size, mi_stats_t* stats);
bool       _mi_os_purge_ex(void* p, size_t size, bool allow_reset, mi_stats_t* stats);

void*      _mi_os_alloc_aligned(size_t size, size_t alignment, bool commit, bool allow_large, mi_memid_t* memid, mi_stats_t* stats);
void*      _mi_os_alloc_aligned_at_offset(size_t size, size_t alignment, size_t align_offset, bool commit, bool allow_large, mi_memid_t* memid, mi_stats_t* tld_stats);

void*      _mi_os_get_aligned_hint(size_t try_alignment, size_t size);
bool       _mi_os_use_large_page(size_t size, size_t alignment);
size_t     _mi_os_large_page_size(void);

void*      _mi_os_alloc_huge_os_pages(size_t pages, int numa_node, mi_msecs_t max_secs, size_t* pages_reserved, size_t* psize, mi_memid_t* memid);

// arena.c
mi_arena_id_t _mi_arena_id_none(void);
void       _mi_arena_free(void* p, size_t size, size_t still_committed_size, mi_memid_t memid, mi_stats_t* stats);
void*      _mi_arena_alloc(size_t size, bool commit, bool allow_large, mi_arena_id_t req_arena_id, mi_memid_t* memid, mi_os_tld_t* tld);
void*      _mi_arena_alloc_aligned(size_t size, size_t alignment, size_t align_offset, bool commit, bool allow_large, mi_arena_id_t req_arena_id, mi_memid_t* memid, mi_os_tld_t* tld);
bool       _mi_arena_memid_is_suitable(mi_memid_t memid, mi_arena_id_t request_arena_id);
bool       _mi_arena_contains(const void* p);
void       _mi_arenas_collect(bool force_purge, mi_stats_t* stats);
void       _mi_arena_unsafe_destroy_all(mi_stats_t* stats);

bool       _mi_arena_segment_clear_abandoned(mi_segment_t* segment);
void       _mi_arena_segment_mark_abandoned(mi_segment_t* segment);
size_t     _mi_arena_segment_abandoned_count(void);

typedef struct mi_arena_field_cursor_s { // abstract
  mi_arena_id_t  start;
  int            count;
  size_t         bitmap_idx;
} mi_arena_field_cursor_t;
void          _mi_arena_field_cursor_init(mi_heap_t* heap, mi_arena_field_cursor_t* current);
mi_segment_t* _mi_arena_segment_clear_abandoned_next(mi_arena_field_cursor_t* previous);

// "segment-map.c"
void       _mi_segment_map_allocated_at(const mi_segment_t* segment);
void       _mi_segment_map_freed_at(const mi_segment_t* segment);

// "segment.c"
mi_page_t* _mi_segment_page_alloc(mi_heap_t* heap, size_t block_size, size_t page_alignment, mi_segments_tld_t* tld, mi_os_tld_t* os_tld);
void       _mi_segment_page_free(mi_page_t* page, bool force, mi_segments_tld_t* tld);
void       _mi_segment_page_abandon(mi_page_t* page, mi_segments_tld_t* tld);
bool       _mi_segment_try_reclaim_abandoned( mi_heap_t* heap, bool try_all, mi_segments_tld_t* tld);
void       _mi_segment_collect(mi_segment_t* segment, bool force, mi_segments_tld_t* tld);

#if MI_HUGE_PAGE_ABANDON
void       _mi_segment_huge_page_free(mi_segment_t* segment, mi_page_t* page, mi_block_t* block);
#else
void       _mi_segment_huge_page_reset(mi_segment_t* segment, mi_page_t* page, mi_block_t* block);
#endif

uint8_t*   _mi_segment_page_start(const mi_segment_t* segment, const mi_page_t* page, size_t* page_size); // page start for any page
void       _mi_abandoned_reclaim_all(mi_heap_t* heap, mi_segments_tld_t* tld);
void       _mi_abandoned_await_readers(void);
void       _mi_abandoned_collect(mi_heap_t* heap, bool force, mi_segments_tld_t* tld);
bool       _mi_segment_attempt_reclaim(mi_heap_t* heap, mi_segment_t* segment);

// "page.c"
void*      _mi_malloc_generic(mi_heap_t* heap, size_t size, bool zero, size_t huge_alignment)  mi_attr_noexcept mi_attr_malloc;

void       _mi_page_retire(mi_page_t* page) mi_attr_noexcept;                  // free the page if there are no other pages with many free blocks
void       _mi_page_unfull(mi_page_t* page);
void       _mi_page_free(mi_page_t* page, mi_page_queue_t* pq, bool force);   // free the page
void       _mi_page_abandon(mi_page_t* page, mi_page_queue_t* pq);            // abandon the page, to be picked up by another thread...
void       _mi_heap_delayed_free_all(mi_heap_t* heap);
bool       _mi_heap_delayed_free_partial(mi_heap_t* heap);
void       _mi_heap_collect_retired(mi_heap_t* heap, bool force);

void       _mi_page_use_delayed_free(mi_page_t* page, mi_delayed_t delay, bool override_never);
bool       _mi_page_try_use_delayed_free(mi_page_t* page, mi_delayed_t delay, bool override_never);
size_t     _mi_page_queue_append(mi_heap_t* heap, mi_page_queue_t* pq, mi_page_queue_t* append);
void       _mi_deferred_free(mi_heap_t* heap, bool force);

void       _mi_page_free_collect(mi_page_t* page,bool force);
void       _mi_page_reclaim(mi_heap_t* heap, mi_page_t* page);   // callback from segments

size_t     _mi_bin_size(uint8_t bin);           // for stats
uint8_t    _mi_bin(size_t size);                // for stats

// "heap.c"
void       _mi_heap_init(mi_heap_t* heap, mi_tld_t* tld, mi_arena_id_t arena_id, bool noreclaim, uint8_t tag);
void       _mi_heap_destroy_pages(mi_heap_t* heap);
void       _mi_heap_collect_abandon(mi_heap_t* heap);
void       _mi_heap_set_default_direct(mi_heap_t* heap);
bool       _mi_heap_memid_is_suitable(mi_heap_t* heap, mi_memid_t memid);
void       _mi_heap_unsafe_destroy_all(void);
mi_heap_t* _mi_heap_by_tag(mi_heap_t* heap, uint8_t tag);

// "stats.c"
void       _mi_stats_done(mi_stats_t* stats);
mi_msecs_t  _mi_clock_now(void);
mi_msecs_t  _mi_clock_end(mi_msecs_t start);
mi_msecs_t  _mi_clock_start(void);

// "alloc.c"
void*       _mi_page_malloc_zero(mi_heap_t* heap, mi_page_t* page, size_t size, bool zero) mi_attr_noexcept;  // called from `_mi_malloc_generic`
void*       _mi_page_malloc(mi_heap_t* heap, mi_page_t* page, size_t size) mi_attr_noexcept;                  // called from `_mi_heap_malloc_aligned`
void*       _mi_page_malloc_zeroed(mi_heap_t* heap, mi_page_t* page, size_t size) mi_attr_noexcept;           // called from `_mi_heap_malloc_aligned`
void*       _mi_heap_malloc_zero(mi_heap_t* heap, size_t size, bool zero) mi_attr_noexcept;
void*       _mi_heap_malloc_zero_ex(mi_heap_t* heap, size_t size, bool zero, size_t huge_alignment) mi_attr_noexcept;     // called from `_mi_heap_malloc_aligned`
void*       _mi_heap_realloc_zero(mi_heap_t* heap, void* p, size_t newsize, bool zero) mi_attr_noexcept;
mi_block_t* _mi_page_ptr_unalign(const mi_page_t* page, const void* p);
bool        _mi_free_delayed_block(mi_block_t* block);
void        _mi_free_generic(mi_segment_t* segment, mi_page_t* page, bool is_local, void* p) mi_attr_noexcept;  // for runtime integration
void        _mi_padding_shrink(const mi_page_t* page, const mi_block_t* block, const size_t min_size);

// "libc.c"
#include    <stdarg.h>
void        _mi_vsnprintf(char* buf, size_t bufsize, const char* fmt, va_list args);
void        _mi_snprintf(char* buf, size_t buflen, const char* fmt, ...);
char        _mi_toupper(char c);
int         _mi_strnicmp(const char* s, const char* t, size_t n);
void        _mi_strlcpy(char* dest, const char* src, size_t dest_size);
void        _mi_strlcat(char* dest, const char* src, size_t dest_size);
size_t      _mi_strlen(const char* s);
size_t      _mi_strnlen(const char* s, size_t max_len);
bool        _mi_getenv(const char* name, char* result, size_t result_size);

#if MI_DEBUG>1
bool        _mi_page_is_valid(mi_page_t* page);
#endif


// ------------------------------------------------------
// Branches
// ------------------------------------------------------

#if defined(__GNUC__) || defined(__clang__)
#define mi_unlikely(x)     (__builtin_expect(!!(x),false))
#define mi_likely(x)       (__builtin_expect(!!(x),true))
#elif (defined(__cplusplus) && (__cplusplus >= 202002L)) || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
#define mi_unlikely(x)     (x) [[unlikely]]
#define mi_likely(x)       (x) [[likely]]
#else
#define mi_unlikely(x)     (x)
#define mi_likely(x)       (x)
#endif

#ifndef __has_builtin
#define __has_builtin(x)  0
#endif


/* -----------------------------------------------------------
  Error codes passed to `_mi_fatal_error`
  All are recoverable but EFAULT is a serious error and aborts by default in secure mode.
  For portability define undefined error codes using common Unix codes:
  <https://www-numi.fnal.gov/offline_software/srt_public_context/WebDocs/Errors/unix_system_errors.html>
----------------------------------------------------------- */
#include <errno.h>
#ifndef EAGAIN         // double free
#define EAGAIN (11)
#endif
#ifndef ENOMEM         // out of memory
#define ENOMEM (12)
#endif
#ifndef EFAULT         // corrupted free-list or meta-data
#define EFAULT (14)
#endif
#ifndef EINVAL         // trying to free an invalid pointer
#define EINVAL (22)
#endif
#ifndef EOVERFLOW      // count*size overflow
#define EOVERFLOW (75)
#endif


/* -----------------------------------------------------------
  Inlined definitions
----------------------------------------------------------- */
#define MI_UNUSED(x)     (void)(x)
#if (MI_DEBUG>0)
#define MI_UNUSED_RELEASE(x)
#else
#define MI_UNUSED_RELEASE(x)  MI_UNUSED(x)
#endif

#define MI_INIT4(x)   x(),x(),x(),x()
#define MI_INIT8(x)   MI_INIT4(x),MI_INIT4(x)
#define MI_INIT16(x)  MI_INIT8(x),MI_INIT8(x)
#define MI_INIT32(x)  MI_INIT16(x),MI_INIT16(x)
#define MI_INIT64(x)  MI_INIT32(x),MI_INIT32(x)
#define MI_INIT128(x) MI_INIT64(x),MI_INIT64(x)
#define MI_INIT256(x) MI_INIT128(x),MI_INIT128(x)


#include <string.h>
// initialize a local variable to zero; use memset as compilers optimize constant sized memset's
#define _mi_memzero_var(x)  memset(&x,0,sizeof(x))

// Is `x` a power of two? (0 is considered a power of two)
static inline bool _mi_is_power_of_two(uintptr_t x) {
  return ((x & (x - 1)) == 0);
}

// Is a pointer aligned?
static inline bool _mi_is_aligned(void* p, size_t alignment) {
  mi_assert_internal(alignment != 0);
  return (((uintptr_t)p % alignment) == 0);
}

// Align upwards
static inline uintptr_t _mi_align_up(uintptr_t sz, size_t alignment) {
  mi_assert_internal(alignment != 0);
  uintptr_t mask = alignment - 1;
  if ((alignment & mask) == 0) {  // power of two?
    return ((sz + mask) & ~mask);
  }
  else {
    return (((sz + mask)/alignment)*alignment);
  }
}

// Align downwards
static inline uintptr_t _mi_align_down(uintptr_t sz, size_t alignment) {
  mi_assert_internal(alignment != 0);
  uintptr_t mask = alignment - 1;
  if ((alignment & mask) == 0) { // power of two?
    return (sz & ~mask);
  }
  else {
    return ((sz / alignment) * alignment);
  }
}

// Align a pointer upwards
static inline void* mi_align_up_ptr(void* p, size_t alignment) {
  return (void*)_mi_align_up((uintptr_t)p, alignment);
}

// Align a pointer downwards
static inline void* mi_align_down_ptr(void* p, size_t alignment) {
  return (void*)_mi_align_down((uintptr_t)p, alignment);
}


// Divide upwards: `s <= _mi_divide_up(s,d)*d < s+d`.
static inline uintptr_t _mi_divide_up(uintptr_t size, size_t divider) {
  mi_assert_internal(divider != 0);
  return (divider == 0 ? size : ((size + divider - 1) / divider));
}

// Is memory zero initialized?
static inline bool mi_mem_is_zero(const void* p, size_t size) {
  for (size_t i = 0; i < size; i++) {
    if (((uint8_t*)p)[i] != 0) return false;
  }
  return true;
}


// Align a byte size to a size in _machine words_,
// i.e. byte size == `wsize*sizeof(void*)`.
static inline size_t _mi_wsize_from_size(size_t size) {
  mi_assert_internal(size <= SIZE_MAX - sizeof(uintptr_t));
  return (size + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);
}

// Overflow detecting multiply
#if __has_builtin(__builtin_umul_overflow) || (defined(__GNUC__) && (__GNUC__ >= 5))
#include <limits.h>      // UINT_MAX, ULONG_MAX
#if defined(_CLOCK_T)    // for Illumos
#undef _CLOCK_T
#endif
static inline bool mi_mul_overflow(size_t count, size_t size, size_t* total) {
  #if (SIZE_MAX == ULONG_MAX)
    return __builtin_umull_overflow(count, size, (unsigned long *)total);
  #elif (SIZE_MAX == UINT_MAX)
    return __builtin_umul_overflow(count, size, (unsigned int *)total);
  #else
    return __builtin_umulll_overflow(count, size, (unsigned long long *)total);
  #endif
}
#else /* __builtin_umul_overflow is unavailable */
static inline bool mi_mul_overflow(size_t count, size_t size, size_t* total) {
  #define MI_MUL_COULD_OVERFLOW ((size_t)1 << (4*sizeof(size_t)))  // sqrt(SIZE_MAX)
  *total = count * size;
  // note: gcc/clang optimize this to directly check the overflow flag
  return ((size >= MI_MUL_COULD_OVERFLOW || count >= MI_MUL_COULD_OVERFLOW) && size > 0 && (SIZE_MAX / size) < count);
}
#endif

// Safe multiply `count*size` into `total`; return `true` on overflow.
static inline bool mi_count_size_overflow(size_t count, size_t size, size_t* total) {
  if (count==1) {  // quick check for the case where count is one (common for C++ allocators)
    *total = size;
    return false;
  }
  else if mi_unlikely(mi_mul_overflow(count, size, total)) {
    #if MI_DEBUG > 0
    _mi_error_message(EOVERFLOW, "allocation request is too large (%zu * %zu bytes)\n", count, size);
    #endif
    *total = SIZE_MAX;
    return true;
  }
  else return false;
}


/*----------------------------------------------------------------------------------------
  Heap functions
------------------------------------------------------------------------------------------- */

extern const mi_heap_t _mi_heap_empty;  // read-only empty heap, initial value of the thread local default heap

static inline bool mi_heap_is_backing(const mi_heap_t* heap) {
  return (heap->tld->heap_backing == heap);
}

static inline bool mi_heap_is_initialized(mi_heap_t* heap) {
  mi_assert_internal(heap != NULL);
  return (heap != &_mi_heap_empty);
}

static inline uintptr_t _mi_ptr_cookie(const void* p) {
  extern mi_heap_t _mi_heap_main;
  mi_assert_internal(_mi_heap_main.cookie != 0);
  return ((uintptr_t)p ^ _mi_heap_main.cookie);
}

/* -----------------------------------------------------------
  Pages
----------------------------------------------------------- */

static inline mi_page_t* _mi_heap_get_free_small_page(mi_heap_t* heap, size_t size) {
  mi_assert_internal(size <= (MI_SMALL_SIZE_MAX + MI_PADDING_SIZE));
  const size_t idx = _mi_wsize_from_size(size);
  mi_assert_internal(idx < MI_PAGES_DIRECT);
  return heap->pages_free_direct[idx];
}

// Segment that contains the pointer
// Large aligned blocks may be aligned at N*MI_SEGMENT_SIZE (inside a huge segment > MI_SEGMENT_SIZE),
// and we need align "down" to the segment info which is `MI_SEGMENT_SIZE` bytes before it;
// therefore we align one byte before `p`.
// We check for NULL afterwards on 64-bit systems to improve codegen for `mi_free`.
static inline mi_segment_t* _mi_ptr_segment(const void* p) {
  mi_segment_t* const segment = (mi_segment_t*)(((uintptr_t)p - 1) & ~MI_SEGMENT_MASK);
  #if MI_INTPTR_SIZE <= 4
  return (p==NULL ? NULL : segment);
  #else
  return ((intptr_t)segment <= 0 ? NULL : segment);
  #endif
}

static inline mi_page_t* mi_slice_to_page(mi_slice_t* s) {
  mi_assert_internal(s->slice_offset== 0 && s->slice_count > 0);
  return (mi_page_t*)(s);
}

static inline mi_slice_t* mi_page_to_slice(mi_page_t* p) {
  mi_assert_internal(p->slice_offset== 0 && p->slice_count > 0);
  return (mi_slice_t*)(p);
}

// Segment belonging to a page
static inline mi_segment_t* _mi_page_segment(const mi_page_t* page) {
  mi_assert_internal(page!=NULL);
  mi_segment_t* segment = _mi_ptr_segment(page);
  mi_assert_internal(segment == NULL || ((mi_slice_t*)page >= segment->slices && (mi_slice_t*)page < segment->slices + segment->slice_entries));
  return segment;
}

static inline mi_slice_t* mi_slice_first(const mi_slice_t* slice) {
  mi_slice_t* start = (mi_slice_t*)((uint8_t*)slice - slice->slice_offset);
  mi_assert_internal(start >= _mi_ptr_segment(slice)->slices);
  mi_assert_internal(start->slice_offset == 0);
  mi_assert_internal(start + start->slice_count > slice);
  return start;
}

// Get the page containing the pointer (performance critical as it is called in mi_free)
static inline mi_page_t* _mi_segment_page_of(const mi_segment_t* segment, const void* p) {
  mi_assert_internal(p > (void*)segment);
  ptrdiff_t diff = (uint8_t*)p - (uint8_t*)segment;
  mi_assert_internal(diff > 0 && diff <= (ptrdiff_t)MI_SEGMENT_SIZE);
  size_t idx = (size_t)diff >> MI_SEGMENT_SLICE_SHIFT;
  mi_assert_internal(idx <= segment->slice_entries);
  mi_slice_t* slice0 = (mi_slice_t*)&segment->slices[idx];
  mi_slice_t* slice = mi_slice_first(slice0);  // adjust to the block that holds the page data
  mi_assert_internal(slice->slice_offset == 0);
  mi_assert_internal(slice >= segment->slices && slice < segment->slices + segment->slice_entries);
  return mi_slice_to_page(slice);
}

// Quick page start for initialized pages
static inline uint8_t* mi_page_start(const mi_page_t* page) {
  mi_assert_internal(page->page_start != NULL);
  mi_assert_expensive(_mi_segment_page_start(_mi_page_segment(page),page,NULL) == page->page_start);
  return page->page_start;
}

// Get the page containing the pointer
static inline mi_page_t* _mi_ptr_page(void* p) {
  mi_assert_internal(p!=NULL);
  return _mi_segment_page_of(_mi_ptr_segment(p), p);
}

// Get the block size of a page (special case for huge objects)
static inline size_t mi_page_block_size(const mi_page_t* page) {
  mi_assert_internal(page->block_size > 0);
  return page->block_size;
}

static inline bool mi_page_is_huge(const mi_page_t* page) {
  mi_assert_internal((page->is_huge && _mi_page_segment(page)->kind == MI_SEGMENT_HUGE) ||
                     (!page->is_huge && _mi_page_segment(page)->kind != MI_SEGMENT_HUGE));
  return page->is_huge;
}

// Get the usable block size of a page without fixed padding.
// This may still include internal padding due to alignment and rounding up size classes.
static inline size_t mi_page_usable_block_size(const mi_page_t* page) {
  return mi_page_block_size(page) - MI_PADDING_SIZE;
}

// size of a segment
static inline size_t mi_segment_size(mi_segment_t* segment) {
  return segment->segment_slices * MI_SEGMENT_SLICE_SIZE;
}

static inline uint8_t* mi_segment_end(mi_segment_t* segment) {
  return (uint8_t*)segment + mi_segment_size(segment);
}

// Thread free access
static inline mi_block_t* mi_page_thread_free(const mi_page_t* page) {
  return (mi_block_t*)(mi_atomic_load_relaxed(&((mi_page_t*)page)->xthread_free) & ~3);
}

static inline mi_delayed_t mi_page_thread_free_flag(const mi_page_t* page) {
  return (mi_delayed_t)(mi_atomic_load_relaxed(&((mi_page_t*)page)->xthread_free) & 3);
}

// Heap access
static inline mi_heap_t* mi_page_heap(const mi_page_t* page) {
  return (mi_heap_t*)(mi_atomic_load_relaxed(&((mi_page_t*)page)->xheap));
}

static inline void mi_page_set_heap(mi_page_t* page, mi_heap_t* heap) {
  mi_assert_internal(mi_page_thread_free_flag(page) != MI_DELAYED_FREEING);
  mi_atomic_store_release(&page->xheap,(uintptr_t)heap);
  if (heap != NULL) { page->heap_tag = heap->tag; }
}

// Thread free flag helpers
static inline mi_block_t* mi_tf_block(mi_thread_free_t tf) {
  return (mi_block_t*)(tf & ~0x03);
}
static inline mi_delayed_t mi_tf_delayed(mi_thread_free_t tf) {
  return (mi_delayed_t)(tf & 0x03);
}
static inline mi_thread_free_t mi_tf_make(mi_block_t* block, mi_delayed_t delayed) {
  return (mi_thread_free_t)((uintptr_t)block | (uintptr_t)delayed);
}
static inline mi_thread_free_t mi_tf_set_delayed(mi_thread_free_t tf, mi_delayed_t delayed) {
  return mi_tf_make(mi_tf_block(tf),delayed);
}
static inline mi_thread_free_t mi_tf_set_block(mi_thread_free_t tf, mi_block_t* block) {
  return mi_tf_make(block, mi_tf_delayed(tf));
}

// are all blocks in a page freed?
// note: needs up-to-date used count, (as the `xthread_free` list may not be empty). see `_mi_page_collect_free`.
static inline bool mi_page_all_free(const mi_page_t* page) {
  mi_assert_internal(page != NULL);
  return (page->used == 0);
}

// are there any available blocks?
static inline bool mi_page_has_any_available(const mi_page_t* page) {
  mi_assert_internal(page != NULL && page->reserved > 0);
  return (page->used < page->reserved || (mi_page_thread_free(page) != NULL));
}

// are there immediately available blocks, i.e. blocks available on the free list.
static inline bool mi_page_immediate_available(const mi_page_t* page) {
  mi_assert_internal(page != NULL);
  return (page->free != NULL);
}

// is more than 7/8th of a page in use?
static inline bool mi_page_mostly_used(const mi_page_t* page) {
  if (page==NULL) return true;
  uint16_t frac = page->reserved / 8U;
  return (page->reserved - page->used <= frac);
}

static inline mi_page_queue_t* mi_page_queue(const mi_heap_t* heap, size_t size) {
  return &((mi_heap_t*)heap)->pages[_mi_bin(size)];
}



//-----------------------------------------------------------
// Page flags
//-----------------------------------------------------------
static inline bool mi_page_is_in_full(const mi_page_t* page) {
  return page->flags.x.in_full;
}

static inline void mi_page_set_in_full(mi_page_t* page, bool in_full) {
  page->flags.x.in_full = in_full;
}

static inline bool mi_page_has_aligned(const mi_page_t* page) {
  return page->flags.x.has_aligned;
}

static inline void mi_page_set_has_aligned(mi_page_t* page, bool has_aligned) {
  page->flags.x.has_aligned = has_aligned;
}


/* -------------------------------------------------------------------
Encoding/Decoding the free list next pointers

This is to protect against buffer overflow exploits where the
free list is mutated. Many hardened allocators xor the next pointer `p`
with a secret key `k1`, as `p^k1`. This prevents overwriting with known
values but might be still too weak: if the attacker can guess
the pointer `p` this  can reveal `k1` (since `p^k1^p == k1`).
Moreover, if multiple blocks can be read as well, the attacker can
xor both as `(p1^k1) ^ (p2^k1) == p1^p2` which may reveal a lot
about the pointers (and subsequently `k1`).

Instead mimalloc uses an extra key `k2` and encodes as `((p^k2)<<<k1)+k1`.
Since these operations are not associative, the above approaches do not
work so well any more even if the `p` can be guesstimated. For example,
for the read case we can subtract two entries to discard the `+k1` term,
but that leads to `((p1^k2)<<<k1) - ((p2^k2)<<<k1)` at best.
We include the left-rotation since xor and addition are otherwise linear
in the lowest bit. Finally, both keys are unique per page which reduces
the re-use of keys by a large factor.

We also pass a separate `null` value to be used as `NULL` or otherwise
`(k2<<<k1)+k1` would appear (too) often as a sentinel value.
------------------------------------------------------------------- */

static inline bool mi_is_in_same_segment(const void* p, const void* q) {
  return (_mi_ptr_segment(p) == _mi_ptr_segment(q));
}

static inline bool mi_is_in_same_page(const void* p, const void* q) {
  mi_segment_t* segment = _mi_ptr_segment(p);
  if (_mi_ptr_segment(q) != segment) return false;
  // assume q may be invalid // return (_mi_segment_page_of(segment, p) == _mi_segment_page_of(segment, q));
  mi_page_t* page = _mi_segment_page_of(segment, p);
  size_t psize;
  uint8_t* start = _mi_segment_page_start(segment, page, &psize);
  return (start <= (uint8_t*)q && (uint8_t*)q < start + psize);
}

static inline uintptr_t mi_rotl(uintptr_t x, uintptr_t shift) {
  shift %= MI_INTPTR_BITS;
  return (shift==0 ? x : ((x << shift) | (x >> (MI_INTPTR_BITS - shift))));
}
static inline uintptr_t mi_rotr(uintptr_t x, uintptr_t shift) {
  shift %= MI_INTPTR_BITS;
  return (shift==0 ? x : ((x >> shift) | (x << (MI_INTPTR_BITS - shift))));
}

static inline void* mi_ptr_decode(const void* null, const mi_encoded_t x, const uintptr_t* keys) {
  void* p = (void*)(mi_rotr(x - keys[0], keys[0]) ^ keys[1]);
  return (p==null ? NULL : p);
}

static inline mi_encoded_t mi_ptr_encode(const void* null, const void* p, const uintptr_t* keys) {
  uintptr_t x = (uintptr_t)(p==NULL ? null : p);
  return mi_rotl(x ^ keys[1], keys[0]) + keys[0];
}

static inline mi_block_t* mi_block_nextx( const void* null, const mi_block_t* block, const uintptr_t* keys ) {
  mi_track_mem_defined(block,sizeof(mi_block_t));
  mi_block_t* next;
  #ifdef MI_ENCODE_FREELIST
  next = (mi_block_t*)mi_ptr_decode(null, block->next, keys);
  #else
  MI_UNUSED(keys); MI_UNUSED(null);
  next = (mi_block_t*)block->next;
  #endif
  mi_track_mem_noaccess(block,sizeof(mi_block_t));
  return next;
}

static inline void mi_block_set_nextx(const void* null, mi_block_t* block, const mi_block_t* next, const uintptr_t* keys) {
  mi_track_mem_undefined(block,sizeof(mi_block_t));
  #ifdef MI_ENCODE_FREELIST
  block->next = mi_ptr_encode(null, next, keys);
  #else
  MI_UNUSED(keys); MI_UNUSED(null);
  block->next = (mi_encoded_t)next;
  #endif
  mi_track_mem_noaccess(block,sizeof(mi_block_t));
}

static inline mi_block_t* mi_block_next(const mi_page_t* page, const mi_block_t* block) {
  #ifdef MI_ENCODE_FREELIST
  mi_block_t* next = mi_block_nextx(page,block,page->keys);
  // check for free list corruption: is `next` at least in the same page?
  // TODO: check if `next` is `page->block_size` aligned?
  if mi_unlikely(next!=NULL && !mi_is_in_same_page(block, next)) {
    _mi_error_message(EFAULT, "corrupted free list entry of size %zub at %p: value 0x%zx\n", mi_page_block_size(page), block, (uintptr_t)next);
    next = NULL;
  }
  return next;
  #else
  MI_UNUSED(page);
  return mi_block_nextx(page,block,NULL);
  #endif
}

static inline void mi_block_set_next(const mi_page_t* page, mi_block_t* block, const mi_block_t* next) {
  #ifdef MI_ENCODE_FREELIST
  mi_block_set_nextx(page,block,next, page->keys);
  #else
  MI_UNUSED(page);
  mi_block_set_nextx(page,block,next,NULL);
  #endif
}


// -------------------------------------------------------------------
// commit mask
// -------------------------------------------------------------------

static inline void mi_commit_mask_create_empty(mi_commit_mask_t* cm) {
  for (size_t i = 0; i < MI_COMMIT_MASK_FIELD_COUNT; i++) {
    cm->mask[i] = 0;
  }
}

static inline void mi_commit_mask_create_full(mi_commit_mask_t* cm) {
  for (size_t i = 0; i < MI_COMMIT_MASK_FIELD_COUNT; i++) {
    cm->mask[i] = ~((size_t)0);
  }
}

static inline bool mi_commit_mask_is_empty(const mi_commit_mask_t* cm) {
  for (size_t i = 0; i < MI_COMMIT_MASK_FIELD_COUNT; i++) {
    if (cm->mask[i] != 0) return false;
  }
  return true;
}

static inline bool mi_commit_mask_is_full(const mi_commit_mask_t* cm) {
  for (size_t i = 0; i < MI_COMMIT_MASK_FIELD_COUNT; i++) {
    if (cm->mask[i] != ~((size_t)0)) return false;
  }
  return true;
}

// defined in `segment.c`:
size_t _mi_commit_mask_committed_size(const mi_commit_mask_t* cm, size_t total);
size_t _mi_commit_mask_next_run(const mi_commit_mask_t* cm, size_t* idx);

#define mi_commit_mask_foreach(cm,idx,count) \
  idx = 0; \
  while ((count = _mi_commit_mask_next_run(cm,&idx)) > 0) {

#define mi_commit_mask_foreach_end() \
    idx += count; \
  }



/* -----------------------------------------------------------
  memory id's
----------------------------------------------------------- */

static inline mi_memid_t _mi_memid_create(mi_memkind_t memkind) {
  mi_memid_t memid;
  _mi_memzero_var(memid);
  memid.memkind = memkind;
  return memid;
}

static inline mi_memid_t _mi_memid_none(void) {
  return _mi_memid_create(MI_MEM_NONE);
}

static inline mi_memid_t _mi_memid_create_os(bool committed, bool is_zero, bool is_large) {
  mi_memid_t memid = _mi_memid_create(MI_MEM_OS);
  memid.initially_committed = committed;
  memid.initially_zero = is_zero;
  memid.is_pinned = is_large;
  return memid;
}


// -------------------------------------------------------------------
// Fast "random" shuffle
// -------------------------------------------------------------------

static inline uintptr_t _mi_random_shuffle(uintptr_t x) {
  if (x==0) { x = 17; }   // ensure we don't get stuck in generating zeros
#if (MI_INTPTR_SIZE==8)
  // by Sebastiano Vigna, see: <http://xoshiro.di.unimi.it/splitmix64.c>
  x ^= x >> 30;
  x *= 0xbf58476d1ce4e5b9UL;
  x ^= x >> 27;
  x *= 0x94d049bb133111ebUL;
  x ^= x >> 31;
#elif (MI_INTPTR_SIZE==4)
  // by Chris Wellons, see: <https://nullprogram.com/blog/2018/07/31/>
  x ^= x >> 16;
  x *= 0x7feb352dUL;
  x ^= x >> 15;
  x *= 0x846ca68bUL;
  x ^= x >> 16;
#endif
  return x;
}

// -------------------------------------------------------------------
// Optimize numa node access for the common case (= one node)
// -------------------------------------------------------------------

int    _mi_os_numa_node_get(mi_os_tld_t* tld);
size_t _mi_os_numa_node_count_get(void);

extern _Atomic(size_t) _mi_numa_node_count;
static inline int _mi_os_numa_node(mi_os_tld_t* tld) {
  if mi_likely(mi_atomic_load_relaxed(&_mi_numa_node_count) == 1) { return 0; }
  else return _mi_os_numa_node_get(tld);
}
static inline size_t _mi_os_numa_node_count(void) {
  const size_t count = mi_atomic_load_relaxed(&_mi_numa_node_count);
  if mi_likely(count > 0) { return count; }
  else return _mi_os_numa_node_count_get();
}



// -----------------------------------------------------------------------
// Count bits: trailing or leading zeros (with MI_INTPTR_BITS on all zero)
// -----------------------------------------------------------------------

#if defined(__GNUC__)

#include <limits.h>       // LONG_MAX
#define MI_HAVE_FAST_BITSCAN
static inline size_t mi_clz(uintptr_t x) {
  if (x==0) return MI_INTPTR_BITS;
#if (INTPTR_MAX == LONG_MAX)
  return __builtin_clzl(x);
#else
  return __builtin_clzll(x);
#endif
}
static inline size_t mi_ctz(uintptr_t x) {
  if (x==0) return MI_INTPTR_BITS;
#if (INTPTR_MAX == LONG_MAX)
  return __builtin_ctzl(x);
#else
  return __builtin_ctzll(x);
#endif
}

#elif defined(_MSC_VER)

#include <limits.h>       // LONG_MAX
#include <intrin.h>       // BitScanReverse64
#define MI_HAVE_FAST_BITSCAN
static inline size_t mi_clz(uintptr_t x) {
  if (x==0) return MI_INTPTR_BITS;
  unsigned long idx;
#if (INTPTR_MAX == LONG_MAX)
  _BitScanReverse(&idx, x);
#else
  _BitScanReverse64(&idx, x);
#endif
  return ((MI_INTPTR_BITS - 1) - idx);
}
static inline size_t mi_ctz(uintptr_t x) {
  if (x==0) return MI_INTPTR_BITS;
  unsigned long idx;
#if (INTPTR_MAX == LONG_MAX)
  _BitScanForward(&idx, x);
#else
  _BitScanForward64(&idx, x);
#endif
  return idx;
}

#else
static inline size_t mi_ctz32(uint32_t x) {
  // de Bruijn multiplication, see <http://supertech.csail.mit.edu/papers/debruijn.pdf>
  static const unsigned char debruijn[32] = {
    0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
    31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
  };
  if (x==0) return 32;
  return debruijn[((x & -(int32_t)x) * 0x077CB531UL) >> 27];
}
static inline size_t mi_clz32(uint32_t x) {
  // de Bruijn multiplication, see <http://supertech.csail.mit.edu/papers/debruijn.pdf>
  static const uint8_t debruijn[32] = {
    31, 22, 30, 21, 18, 10, 29, 2, 20, 17, 15, 13, 9, 6, 28, 1,
    23, 19, 11, 3, 16, 14, 7, 24, 12, 4, 8, 25, 5, 26, 27, 0
  };
  if (x==0) return 32;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return debruijn[(uint32_t)(x * 0x07C4ACDDUL) >> 27];
}

static inline size_t mi_clz(uintptr_t x) {
  if (x==0) return MI_INTPTR_BITS;
#if (MI_INTPTR_BITS <= 32)
  return mi_clz32((uint32_t)x);
#else
  size_t count = mi_clz32((uint32_t)(x >> 32));
  if (count < 32) return count;
  return (32 + mi_clz32((uint32_t)x));
#endif
}
static inline size_t mi_ctz(uintptr_t x) {
  if (x==0) return MI_INTPTR_BITS;
#if (MI_INTPTR_BITS <= 32)
  return mi_ctz32((uint32_t)x);
#else
  size_t count = mi_ctz32((uint32_t)x);
  if (count < 32) return count;
  return (32 + mi_ctz32((uint32_t)(x>>32)));
#endif
}

#endif

// "bit scan reverse": Return index of the highest bit (or MI_INTPTR_BITS if `x` is zero)
static inline size_t mi_bsr(uintptr_t x) {
  return (x==0 ? MI_INTPTR_BITS : MI_INTPTR_BITS - 1 - mi_clz(x));
}


// ---------------------------------------------------------------------------------
// Provide our own `_mi_memcpy` for potential performance optimizations.
//
// For now, only on Windows with msvc/clang-cl we optimize to `rep movsb` if
// we happen to run on x86/x64 cpu's that have "fast short rep movsb" (FSRM) support
// (AMD Zen3+ (~2020) or Intel Ice Lake+ (~2017). See also issue #201 and pr #253.
// ---------------------------------------------------------------------------------

#if !MI_TRACK_ENABLED && defined(_WIN32) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
extern bool _mi_cpu_has_fsrm;
static inline void _mi_memcpy(void* dst, const void* src, size_t n) {
  if (_mi_cpu_has_fsrm) {
    __movsb((unsigned char*)dst, (const unsigned char*)src, n);
  }
  else {
    memcpy(dst, src, n);
  }
}
static inline void _mi_memzero(void* dst, size_t n) {
  if (_mi_cpu_has_fsrm) {
    __stosb((unsigned char*)dst, 0, n);
  }
  else {
    memset(dst, 0, n);
  }
}
#else
static inline void _mi_memcpy(void* dst, const void* src, size_t n) {
  memcpy(dst, src, n);
}
static inline void _mi_memzero(void* dst, size_t n) {
  memset(dst, 0, n);
}
#endif

// -------------------------------------------------------------------------------
// The `_mi_memcpy_aligned` can be used if the pointers are machine-word aligned
// This is used for example in `mi_realloc`.
// -------------------------------------------------------------------------------

#if (defined(__GNUC__) && (__GNUC__ >= 4)) || defined(__clang__)
// On GCC/CLang we provide a hint that the pointers are word aligned.
static inline void _mi_memcpy_aligned(void* dst, const void* src, size_t n) {
  mi_assert_internal(((uintptr_t)dst % MI_INTPTR_SIZE == 0) && ((uintptr_t)src % MI_INTPTR_SIZE == 0));
  void* adst = __builtin_assume_aligned(dst, MI_INTPTR_SIZE);
  const void* asrc = __builtin_assume_aligned(src, MI_INTPTR_SIZE);
  _mi_memcpy(adst, asrc, n);
}

static inline void _mi_memzero_aligned(void* dst, size_t n) {
  mi_assert_internal((uintptr_t)dst % MI_INTPTR_SIZE == 0);
  void* adst = __builtin_assume_aligned(dst, MI_INTPTR_SIZE);
  _mi_memzero(adst, n);
}
#else
// Default fallback on `_mi_memcpy`
static inline void _mi_memcpy_aligned(void* dst, const void* src, size_t n) {
  mi_assert_internal(((uintptr_t)dst % MI_INTPTR_SIZE == 0) && ((uintptr_t)src % MI_INTPTR_SIZE == 0));
  _mi_memcpy(dst, src, n);
}

static inline void _mi_memzero_aligned(void* dst, size_t n) {
  mi_assert_internal((uintptr_t)dst % MI_INTPTR_SIZE == 0);
  _mi_memzero(dst, n);
}
#endif


#endif
