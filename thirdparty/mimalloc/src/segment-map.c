/* ----------------------------------------------------------------------------
Copyright (c) 2019-2023, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

/* -----------------------------------------------------------
  The following functions are to reliably find the segment or
  block that encompasses any pointer p (or NULL if it is not
  in any of our segments).
  We maintain a bitmap of all memory with 1 bit per MI_SEGMENT_SIZE (64MiB)
  set to 1 if it contains the segment meta data.
----------------------------------------------------------- */
#include "mimalloc.h"
#include "mimalloc/internal.h"
#include "mimalloc/atomic.h"

#if (MI_INTPTR_SIZE>=8) && MI_TRACK_ASAN
#define MI_MAX_ADDRESS    ((size_t)140 << 40) // 140TB (see issue #881)
#elif (MI_INTPTR_SIZE >= 8)
#define MI_MAX_ADDRESS    ((size_t)40 << 40)  // 40TB (to include huge page areas)
#else
#define MI_MAX_ADDRESS    ((size_t)2 << 30)   // 2Gb
#endif

#define MI_SEGMENT_MAP_BITS  (MI_MAX_ADDRESS / MI_SEGMENT_SIZE)
#define MI_SEGMENT_MAP_SIZE  (MI_SEGMENT_MAP_BITS / 8)
#define MI_SEGMENT_MAP_WSIZE (MI_SEGMENT_MAP_SIZE / MI_INTPTR_SIZE)

static _Atomic(uintptr_t) mi_segment_map[MI_SEGMENT_MAP_WSIZE + 1];  // 2KiB per TB with 64MiB segments

static size_t mi_segment_map_index_of(const mi_segment_t* segment, size_t* bitidx) {
  // note: segment can be invalid or NULL.
  mi_assert_internal(_mi_ptr_segment(segment + 1) == segment); // is it aligned on MI_SEGMENT_SIZE?
  if ((uintptr_t)segment >= MI_MAX_ADDRESS) {
    *bitidx = 0;
    return MI_SEGMENT_MAP_WSIZE;
  }
  else {
    const uintptr_t segindex = ((uintptr_t)segment) / MI_SEGMENT_SIZE;
    *bitidx = segindex % MI_INTPTR_BITS;
    const size_t mapindex = segindex / MI_INTPTR_BITS;
    mi_assert_internal(mapindex < MI_SEGMENT_MAP_WSIZE);
    return mapindex;
  }
}

void _mi_segment_map_allocated_at(const mi_segment_t* segment) {
  size_t bitidx;
  size_t index = mi_segment_map_index_of(segment, &bitidx);
  mi_assert_internal(index <= MI_SEGMENT_MAP_WSIZE);
  if (index==MI_SEGMENT_MAP_WSIZE) return;
  uintptr_t mask = mi_atomic_load_relaxed(&mi_segment_map[index]);
  uintptr_t newmask;
  do {
    newmask = (mask | ((uintptr_t)1 << bitidx));
  } while (!mi_atomic_cas_weak_release(&mi_segment_map[index], &mask, newmask));
}

void _mi_segment_map_freed_at(const mi_segment_t* segment) {
  size_t bitidx;
  size_t index = mi_segment_map_index_of(segment, &bitidx);
  mi_assert_internal(index <= MI_SEGMENT_MAP_WSIZE);
  if (index == MI_SEGMENT_MAP_WSIZE) return;
  uintptr_t mask = mi_atomic_load_relaxed(&mi_segment_map[index]);
  uintptr_t newmask;
  do {
    newmask = (mask & ~((uintptr_t)1 << bitidx));
  } while (!mi_atomic_cas_weak_release(&mi_segment_map[index], &mask, newmask));
}

// Determine the segment belonging to a pointer or NULL if it is not in a valid segment.
static mi_segment_t* _mi_segment_of(const void* p) {
  if (p == NULL) return NULL;
  mi_segment_t* segment = _mi_ptr_segment(p);  // segment can be NULL  
  size_t bitidx;
  size_t index = mi_segment_map_index_of(segment, &bitidx);
  // fast path: for any pointer to valid small/medium/large object or first MI_SEGMENT_SIZE in huge
  const uintptr_t mask = mi_atomic_load_relaxed(&mi_segment_map[index]);
  if mi_likely((mask & ((uintptr_t)1 << bitidx)) != 0) {
    return segment; // yes, allocated by us
  }
  if (index==MI_SEGMENT_MAP_WSIZE) return NULL;

  // TODO: maintain max/min allocated range for efficiency for more efficient rejection of invalid pointers?

  // search downwards for the first segment in case it is an interior pointer
  // could be slow but searches in MI_INTPTR_SIZE * MI_SEGMENT_SIZE (512MiB) steps trough
  // valid huge objects
  // note: we could maintain a lowest index to speed up the path for invalid pointers?
  size_t lobitidx;
  size_t loindex;
  uintptr_t lobits = mask & (((uintptr_t)1 << bitidx) - 1);
  if (lobits != 0) {
    loindex = index;
    lobitidx = mi_bsr(lobits);    // lobits != 0
  }
  else if (index == 0) {
    return NULL;
  }
  else {
    mi_assert_internal(index > 0);
    uintptr_t lomask = mask;
    loindex = index;
    do {
      loindex--;  
      lomask = mi_atomic_load_relaxed(&mi_segment_map[loindex]);      
    } while (lomask != 0 && loindex > 0);
    if (lomask == 0) return NULL;
    lobitidx = mi_bsr(lomask);    // lomask != 0
  }
  mi_assert_internal(loindex < MI_SEGMENT_MAP_WSIZE);
  // take difference as the addresses could be larger than the MAX_ADDRESS space.
  size_t diff = (((index - loindex) * (8*MI_INTPTR_SIZE)) + bitidx - lobitidx) * MI_SEGMENT_SIZE;
  segment = (mi_segment_t*)((uint8_t*)segment - diff);

  if (segment == NULL) return NULL;
  mi_assert_internal((void*)segment < p);
  bool cookie_ok = (_mi_ptr_cookie(segment) == segment->cookie);
  mi_assert_internal(cookie_ok);
  if mi_unlikely(!cookie_ok) return NULL;
  if (((uint8_t*)segment + mi_segment_size(segment)) <= (uint8_t*)p) return NULL; // outside the range
  mi_assert_internal(p >= (void*)segment && (uint8_t*)p < (uint8_t*)segment + mi_segment_size(segment));
  return segment;
}

// Is this a valid pointer in our heap?
static bool  mi_is_valid_pointer(const void* p) {
  return ((_mi_segment_of(p) != NULL) || (_mi_arena_contains(p)));
}

mi_decl_nodiscard mi_decl_export bool mi_is_in_heap_region(const void* p) mi_attr_noexcept {
  return mi_is_valid_pointer(p);
}

/*
// Return the full segment range belonging to a pointer
static void* mi_segment_range_of(const void* p, size_t* size) {
  mi_segment_t* segment = _mi_segment_of(p);
  if (segment == NULL) {
    if (size != NULL) *size = 0;
    return NULL;
  }
  else {
    if (size != NULL) *size = segment->segment_size;
    return segment;
  }
  mi_assert_expensive(page == NULL || mi_segment_is_valid(_mi_page_segment(page),tld));
  mi_assert_internal(page == NULL || (mi_segment_page_size(_mi_page_segment(page)) - (MI_SECURE == 0 ? 0 : _mi_os_page_size())) >= block_size);
  mi_reset_delayed(tld);
  mi_assert_internal(page == NULL || mi_page_not_in_queue(page, tld));
  return page;
}
*/
