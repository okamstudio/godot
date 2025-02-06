/* ----------------------------------------------------------------------------
Copyright (c) 2018-2023, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/
#include "mimalloc.h"
#include "mimalloc/internal.h"
#include "mimalloc/atomic.h"
#include "mimalloc/prim.h"


/* -----------------------------------------------------------
  Initialization. 
----------------------------------------------------------- */

static mi_os_mem_config_t mi_os_mem_config = {
  4096,   // page size
  0,      // large page size (usually 2MiB)
  4096,   // allocation granularity
  true,   // has overcommit?  (if true we use MAP_NORESERVE on mmap systems)
  false,  // can we partially free allocated blocks? (on mmap systems we can free anywhere in a mapped range, but on Windows we must free the entire span)
  true    // has virtual reserve? (if true we can reserve virtual address space without using commit or physical memory)
};

bool _mi_os_has_overcommit(void) {
  return mi_os_mem_config.has_overcommit;
}

bool _mi_os_has_virtual_reserve(void) {
  return mi_os_mem_config.has_virtual_reserve;
}


// OS (small) page size
size_t _mi_os_page_size(void) {
  return mi_os_mem_config.page_size;
}

// if large OS pages are supported (2 or 4MiB), then return the size, otherwise return the small page size (4KiB)
size_t _mi_os_large_page_size(void) {
  return (mi_os_mem_config.large_page_size != 0 ? mi_os_mem_config.large_page_size : _mi_os_page_size());
}

bool _mi_os_use_large_page(size_t size, size_t alignment) {
  // if we have access, check the size and alignment requirements
  if (mi_os_mem_config.large_page_size == 0 || !mi_option_is_enabled(mi_option_allow_large_os_pages)) return false;
  return ((size % mi_os_mem_config.large_page_size) == 0 && (alignment % mi_os_mem_config.large_page_size) == 0);
}

// round to a good OS allocation size (bounded by max 12.5% waste)
size_t _mi_os_good_alloc_size(size_t size) {
  size_t align_size;
  if (size < 512*MI_KiB) align_size = _mi_os_page_size();
  else if (size < 2*MI_MiB) align_size = 64*MI_KiB;
  else if (size < 8*MI_MiB) align_size = 256*MI_KiB;
  else if (size < 32*MI_MiB) align_size = 1*MI_MiB;
  else align_size = 4*MI_MiB;
  if mi_unlikely(size >= (SIZE_MAX - align_size)) return size; // possible overflow?
  return _mi_align_up(size, align_size);
}

void _mi_os_init(void) {
  _mi_prim_mem_init(&mi_os_mem_config);
}


/* -----------------------------------------------------------
  Util
-------------------------------------------------------------- */
bool _mi_os_decommit(void* addr, size_t size, mi_stats_t* stats);
bool _mi_os_commit(void* addr, size_t size, bool* is_zero, mi_stats_t* tld_stats);


/* -----------------------------------------------------------
  aligned hinting
-------------------------------------------------------------- */

// On 64-bit systems, we can do efficient aligned allocation by using
// the 2TiB to 30TiB area to allocate those.
#if (MI_INTPTR_SIZE >= 8)
static mi_decl_cache_align _Atomic(uintptr_t)aligned_base;

// Return a MI_SEGMENT_SIZE aligned address that is probably available.
// If this returns NULL, the OS will determine the address but on some OS's that may not be
// properly aligned which can be more costly as it needs to be adjusted afterwards.
// For a size > 1GiB this always returns NULL in order to guarantee good ASLR randomization;
// (otherwise an initial large allocation of say 2TiB has a 50% chance to include (known) addresses
//  in the middle of the 2TiB - 6TiB address range (see issue #372))

#define MI_HINT_BASE ((uintptr_t)2 << 40)  // 2TiB start
#define MI_HINT_AREA ((uintptr_t)4 << 40)  // upto 6TiB   (since before win8 there is "only" 8TiB available to processes)
#define MI_HINT_MAX  ((uintptr_t)30 << 40) // wrap after 30TiB (area after 32TiB is used for huge OS pages)

void* _mi_os_get_aligned_hint(size_t try_alignment, size_t size)
{
  if (try_alignment <= 1 || try_alignment > MI_SEGMENT_SIZE) return NULL;
  size = _mi_align_up(size, MI_SEGMENT_SIZE);
  if (size > 1*MI_GiB) return NULL;  // guarantee the chance of fixed valid address is at most 1/(MI_HINT_AREA / 1<<30) = 1/4096.
  #if (MI_SECURE>0)
  size += MI_SEGMENT_SIZE;        // put in `MI_SEGMENT_SIZE` virtual gaps between hinted blocks; this splits VLA's but increases guarded areas.
  #endif

  uintptr_t hint = mi_atomic_add_acq_rel(&aligned_base, size);
  if (hint == 0 || hint > MI_HINT_MAX) {   // wrap or initialize
    uintptr_t init = MI_HINT_BASE;
    #if (MI_SECURE>0 || MI_DEBUG==0)       // security: randomize start of aligned allocations unless in debug mode
    uintptr_t r = _mi_heap_random_next(mi_prim_get_default_heap());
    init = init + ((MI_SEGMENT_SIZE * ((r>>17) & 0xFFFFF)) % MI_HINT_AREA);  // (randomly 20 bits)*4MiB == 0 to 4TiB
    #endif
    uintptr_t expected = hint + size;
    mi_atomic_cas_strong_acq_rel(&aligned_base, &expected, init);
    hint = mi_atomic_add_acq_rel(&aligned_base, size); // this may still give 0 or > MI_HINT_MAX but that is ok, it is a hint after all
  }
  if (hint%try_alignment != 0) return NULL;
  return (void*)hint;
}
#else
void* _mi_os_get_aligned_hint(size_t try_alignment, size_t size) {
  MI_UNUSED(try_alignment); MI_UNUSED(size);
  return NULL;
}
#endif


/* -----------------------------------------------------------
  Free memory
-------------------------------------------------------------- */

static void mi_os_free_huge_os_pages(void* p, size_t size, mi_stats_t* stats);

static void mi_os_prim_free(void* addr, size_t size, bool still_committed, mi_stats_t* tld_stats) {
  MI_UNUSED(tld_stats);
  mi_stats_t* stats = &_mi_stats_main;
  mi_assert_internal((size % _mi_os_page_size()) == 0);
  if (addr == NULL || size == 0) return; // || _mi_os_is_huge_reserved(addr)
  int err = _mi_prim_free(addr, size);
  if (err != 0) {
    _mi_warning_message("unable to free OS memory (error: %d (0x%x), size: 0x%zx bytes, address: %p)\n", err, err, size, addr);
  }
  if (still_committed) { _mi_stat_decrease(&stats->committed, size); }
  _mi_stat_decrease(&stats->reserved, size);
}

void _mi_os_free_ex(void* addr, size_t size, bool still_committed, mi_memid_t memid, mi_stats_t* tld_stats) {
  if (mi_memkind_is_os(memid.memkind)) {
    size_t csize = _mi_os_good_alloc_size(size);
    void* base = addr;
    // different base? (due to alignment)
    if (memid.mem.os.base != NULL) {
      mi_assert(memid.mem.os.base <= addr);
      mi_assert((uint8_t*)memid.mem.os.base + memid.mem.os.alignment >= (uint8_t*)addr);
      base = memid.mem.os.base;
      csize += ((uint8_t*)addr - (uint8_t*)memid.mem.os.base);
    }
    // free it
    if (memid.memkind == MI_MEM_OS_HUGE) {
      mi_assert(memid.is_pinned);
      mi_os_free_huge_os_pages(base, csize, tld_stats);
    }
    else {
      mi_os_prim_free(base, csize, still_committed, tld_stats);
    }
  }
  else {
    // nothing to do
    mi_assert(memid.memkind < MI_MEM_OS);
  }
}

void  _mi_os_free(void* p, size_t size, mi_memid_t memid, mi_stats_t* tld_stats) {
  _mi_os_free_ex(p, size, true, memid, tld_stats);
}


/* -----------------------------------------------------------
   Primitive allocation from the OS.
-------------------------------------------------------------- */

// Note: the `try_alignment` is just a hint and the returned pointer is not guaranteed to be aligned.
static void* mi_os_prim_alloc(size_t size, size_t try_alignment, bool commit, bool allow_large, bool* is_large, bool* is_zero, mi_stats_t* tld_stats) {
  mi_assert_internal(size > 0 && (size % _mi_os_page_size()) == 0);
  mi_assert_internal(is_zero != NULL);
  mi_assert_internal(is_large != NULL);
  if (size == 0) return NULL;
  if (!commit) { allow_large = false; }
  if (try_alignment == 0) { try_alignment = 1; } // avoid 0 to ensure there will be no divide by zero when aligning
  *is_zero = false;
  void* p = NULL;
  int err = _mi_prim_alloc(size, try_alignment, commit, allow_large, is_large, is_zero, &p);
  if (err != 0) {
    _mi_warning_message("unable to allocate OS memory (error: %d (0x%x), size: 0x%zx bytes, align: 0x%zx, commit: %d, allow large: %d)\n", err, err, size, try_alignment, commit, allow_large);
  }

  MI_UNUSED(tld_stats);
  mi_stats_t* stats = &_mi_stats_main;
  mi_stat_counter_increase(stats->mmap_calls, 1);
  if (p != NULL) {
    _mi_stat_increase(&stats->reserved, size);
    if (commit) {
      _mi_stat_increase(&stats->committed, size);
      // seems needed for asan (or `mimalloc-test-api` fails)
      #ifdef MI_TRACK_ASAN
      if (*is_zero) { mi_track_mem_defined(p,size); }
               else { mi_track_mem_undefined(p,size); }
      #endif
    }
  }
  return p;
}


// Primitive aligned allocation from the OS.
// This function guarantees the allocated memory is aligned.
static void* mi_os_prim_alloc_aligned(size_t size, size_t alignment, bool commit, bool allow_large, bool* is_large, bool* is_zero, void** base, mi_stats_t* stats) {
  mi_assert_internal(alignment >= _mi_os_page_size() && ((alignment & (alignment - 1)) == 0));
  mi_assert_internal(size > 0 && (size % _mi_os_page_size()) == 0);
  mi_assert_internal(is_large != NULL);
  mi_assert_internal(is_zero != NULL);
  mi_assert_internal(base != NULL);
  if (!commit) allow_large = false;
  if (!(alignment >= _mi_os_page_size() && ((alignment & (alignment - 1)) == 0))) return NULL;
  size = _mi_align_up(size, _mi_os_page_size());

  // try first with a hint (this will be aligned directly on Win 10+ or BSD)
  void* p = mi_os_prim_alloc(size, alignment, commit, allow_large, is_large, is_zero, stats);
  if (p == NULL) return NULL;

  // aligned already?
  if (((uintptr_t)p % alignment) == 0) {
    *base = p;
  }
  else {
    // if not aligned, free it, overallocate, and unmap around it
    _mi_warning_message("unable to allocate aligned OS memory directly, fall back to over-allocation (size: 0x%zx bytes, address: %p, alignment: 0x%zx, commit: %d)\n", size, p, alignment, commit);
    mi_os_prim_free(p, size, commit, stats);
    if (size >= (SIZE_MAX - alignment)) return NULL; // overflow
    const size_t over_size = size + alignment;

    if (!mi_os_mem_config.has_partial_free) {  // win32 virtualAlloc cannot free parts of an allocated block
      // over-allocate uncommitted (virtual) memory
      p = mi_os_prim_alloc(over_size, 1 /*alignment*/, false /* commit? */, false /* allow_large */, is_large, is_zero, stats);
      if (p == NULL) return NULL;

      // set p to the aligned part in the full region
      // note: this is dangerous on Windows as VirtualFree needs the actual base pointer
      // this is handled though by having the `base` field in the memid's
      *base = p; // remember the base
      p = mi_align_up_ptr(p, alignment);

      // explicitly commit only the aligned part
      if (commit) {
        _mi_os_commit(p, size, NULL, stats);
      }
    }
    else  { // mmap can free inside an allocation
      // overallocate...
      p = mi_os_prim_alloc(over_size, 1, commit, false, is_large, is_zero, stats);
      if (p == NULL) return NULL;

      // and selectively unmap parts around the over-allocated area. 
      void* aligned_p = mi_align_up_ptr(p, alignment);
      size_t pre_size = (uint8_t*)aligned_p - (uint8_t*)p;
      size_t mid_size = _mi_align_up(size, _mi_os_page_size());
      size_t post_size = over_size - pre_size - mid_size;
      mi_assert_internal(pre_size < over_size&& post_size < over_size&& mid_size >= size);
      if (pre_size > 0)  { mi_os_prim_free(p, pre_size, commit, stats); }
      if (post_size > 0) { mi_os_prim_free((uint8_t*)aligned_p + mid_size, post_size, commit, stats); }
      // we can return the aligned pointer on `mmap` systems
      p = aligned_p;
      *base = aligned_p; // since we freed the pre part, `*base == p`.
    }
  }

  mi_assert_internal(p == NULL || (p != NULL && *base != NULL && ((uintptr_t)p % alignment) == 0));
  return p;
}


/* -----------------------------------------------------------
  OS API: alloc and alloc_aligned
----------------------------------------------------------- */

void* _mi_os_alloc(size_t size, mi_memid_t* memid, mi_stats_t* stats) {
  *memid = _mi_memid_none();
  if (size == 0) return NULL;
  size = _mi_os_good_alloc_size(size);
  bool os_is_large = false;
  bool os_is_zero  = false;
  void* p = mi_os_prim_alloc(size, 0, true, false, &os_is_large, &os_is_zero, stats);
  if (p != NULL) {
    *memid = _mi_memid_create_os(true, os_is_zero, os_is_large);
  }
  return p;
}

void* _mi_os_alloc_aligned(size_t size, size_t alignment, bool commit, bool allow_large, mi_memid_t* memid, mi_stats_t* stats)
{
  MI_UNUSED(&_mi_os_get_aligned_hint); // suppress unused warnings
  *memid = _mi_memid_none();
  if (size == 0) return NULL;
  size = _mi_os_good_alloc_size(size);
  alignment = _mi_align_up(alignment, _mi_os_page_size());

  bool os_is_large = false;
  bool os_is_zero  = false;
  void* os_base = NULL;
  void* p = mi_os_prim_alloc_aligned(size, alignment, commit, allow_large, &os_is_large, &os_is_zero, &os_base, stats );
  if (p != NULL) {
    *memid = _mi_memid_create_os(commit, os_is_zero, os_is_large);
    memid->mem.os.base = os_base;
    memid->mem.os.alignment = alignment;
  }
  return p;
}

/* -----------------------------------------------------------
  OS aligned allocation with an offset. This is used
  for large alignments > MI_BLOCK_ALIGNMENT_MAX. We use a large mimalloc
  page where the object can be aligned at an offset from the start of the segment.
  As we may need to overallocate, we need to free such pointers using `mi_free_aligned`
  to use the actual start of the memory region.
----------------------------------------------------------- */

void* _mi_os_alloc_aligned_at_offset(size_t size, size_t alignment, size_t offset, bool commit, bool allow_large, mi_memid_t* memid, mi_stats_t* stats) {
  mi_assert(offset <= MI_SEGMENT_SIZE);
  mi_assert(offset <= size);
  mi_assert((alignment % _mi_os_page_size()) == 0);
  *memid = _mi_memid_none();
  if (offset > MI_SEGMENT_SIZE) return NULL;
  if (offset == 0) {
    // regular aligned allocation
    return _mi_os_alloc_aligned(size, alignment, commit, allow_large, memid, stats);
  }
  else {
    // overallocate to align at an offset
    const size_t extra = _mi_align_up(offset, alignment) - offset;
    const size_t oversize = size + extra;
    void* const start = _mi_os_alloc_aligned(oversize, alignment, commit, allow_large, memid, stats);
    if (start == NULL) return NULL;

    void* const p = (uint8_t*)start + extra;
    mi_assert(_mi_is_aligned((uint8_t*)p + offset, alignment));
    // decommit the overallocation at the start
    if (commit && extra > _mi_os_page_size()) {
      _mi_os_decommit(start, extra, stats);
    }
    return p;
  }
}

/* -----------------------------------------------------------
  OS memory API: reset, commit, decommit, protect, unprotect.
----------------------------------------------------------- */

// OS page align within a given area, either conservative (pages inside the area only),
// or not (straddling pages outside the area is possible)
static void* mi_os_page_align_areax(bool conservative, void* addr, size_t size, size_t* newsize) {
  mi_assert(addr != NULL && size > 0);
  if (newsize != NULL) *newsize = 0;
  if (size == 0 || addr == NULL) return NULL;

  // page align conservatively within the range
  void* start = (conservative ? mi_align_up_ptr(addr, _mi_os_page_size())
    : mi_align_down_ptr(addr, _mi_os_page_size()));
  void* end = (conservative ? mi_align_down_ptr((uint8_t*)addr + size, _mi_os_page_size())
    : mi_align_up_ptr((uint8_t*)addr + size, _mi_os_page_size()));
  ptrdiff_t diff = (uint8_t*)end - (uint8_t*)start;
  if (diff <= 0) return NULL;

  mi_assert_internal((conservative && (size_t)diff <= size) || (!conservative && (size_t)diff >= size));
  if (newsize != NULL) *newsize = (size_t)diff;
  return start;
}

static void* mi_os_page_align_area_conservative(void* addr, size_t size, size_t* newsize) {
  return mi_os_page_align_areax(true, addr, size, newsize);
}

bool _mi_os_commit(void* addr, size_t size, bool* is_zero, mi_stats_t* tld_stats) {
  MI_UNUSED(tld_stats);
  mi_stats_t* stats = &_mi_stats_main;
  if (is_zero != NULL) { *is_zero = false; }
  _mi_stat_increase(&stats->committed, size);  // use size for precise commit vs. decommit
  _mi_stat_counter_increase(&stats->commit_calls, 1);

  // page align range
  size_t csize;
  void* start = mi_os_page_align_areax(false /* conservative? */, addr, size, &csize);
  if (csize == 0) return true;

  // commit
  bool os_is_zero = false;
  int err = _mi_prim_commit(start, csize, &os_is_zero);
  if (err != 0) {
    _mi_warning_message("cannot commit OS memory (error: %d (0x%x), address: %p, size: 0x%zx bytes)\n", err, err, start, csize);
    return false;
  }
  if (os_is_zero && is_zero != NULL) {
    *is_zero = true;
    mi_assert_expensive(mi_mem_is_zero(start, csize));
  }
  // note: the following seems required for asan (otherwise `mimalloc-test-stress` fails)
  #ifdef MI_TRACK_ASAN
  if (os_is_zero) { mi_track_mem_defined(start,csize); }
             else { mi_track_mem_undefined(start,csize); }
  #endif
  return true;
}

static bool mi_os_decommit_ex(void* addr, size_t size, bool* needs_recommit, mi_stats_t* tld_stats) {
  MI_UNUSED(tld_stats);
  mi_stats_t* stats = &_mi_stats_main;
  mi_assert_internal(needs_recommit!=NULL);
  _mi_stat_decrease(&stats->committed, size);

  // page align
  size_t csize;
  void* start = mi_os_page_align_area_conservative(addr, size, &csize);
  if (csize == 0) return true;

  // decommit
  *needs_recommit = true;
  int err = _mi_prim_decommit(start,csize,needs_recommit);
  if (err != 0) {
    _mi_warning_message("cannot decommit OS memory (error: %d (0x%x), address: %p, size: 0x%zx bytes)\n", err, err, start, csize);
  }
  mi_assert_internal(err == 0);
  return (err == 0);
}

bool _mi_os_decommit(void* addr, size_t size, mi_stats_t* tld_stats) {
  bool needs_recommit;
  return mi_os_decommit_ex(addr, size, &needs_recommit, tld_stats);
}


// Signal to the OS that the address range is no longer in use
// but may be used later again. This will release physical memory
// pages and reduce swapping while keeping the memory committed.
// We page align to a conservative area inside the range to reset.
bool _mi_os_reset(void* addr, size_t size, mi_stats_t* stats) {
  // page align conservatively within the range
  size_t csize;
  void* start = mi_os_page_align_area_conservative(addr, size, &csize);
  if (csize == 0) return true;  // || _mi_os_is_huge_reserved(addr)
  _mi_stat_increase(&stats->reset, csize);
  _mi_stat_counter_increase(&stats->reset_calls, 1);

  #if (MI_DEBUG>1) && !MI_SECURE && !MI_TRACK_ENABLED // && !MI_TSAN
  memset(start, 0, csize); // pretend it is eagerly reset
  #endif

  int err = _mi_prim_reset(start, csize);
  if (err != 0) {
    _mi_warning_message("cannot reset OS memory (error: %d (0x%x), address: %p, size: 0x%zx bytes)\n", err, err, start, csize);
  }
  return (err == 0);
}


// either resets or decommits memory, returns true if the memory needs
// to be recommitted if it is to be re-used later on.
bool _mi_os_purge_ex(void* p, size_t size, bool allow_reset, mi_stats_t* stats)
{
  if (mi_option_get(mi_option_purge_delay) < 0) return false;  // is purging allowed?
  _mi_stat_counter_increase(&stats->purge_calls, 1);
  _mi_stat_increase(&stats->purged, size);

  if (mi_option_is_enabled(mi_option_purge_decommits) &&   // should decommit?
      !_mi_preloading())                                   // don't decommit during preloading (unsafe)
  {
    bool needs_recommit = true;
    mi_os_decommit_ex(p, size, &needs_recommit, stats);
    return needs_recommit;
  }
  else {
    if (allow_reset) {  // this can sometimes be not allowed if the range is not fully committed
      _mi_os_reset(p, size, stats);
    }
    return false;  // needs no recommit
  }
}

// either resets or decommits memory, returns true if the memory needs
// to be recommitted if it is to be re-used later on.
bool _mi_os_purge(void* p, size_t size, mi_stats_t * stats) {
  return _mi_os_purge_ex(p, size, true, stats);
}

// Protect a region in memory to be not accessible.
static  bool mi_os_protectx(void* addr, size_t size, bool protect) {
  // page align conservatively within the range
  size_t csize = 0;
  void* start = mi_os_page_align_area_conservative(addr, size, &csize);
  if (csize == 0) return false;
  /*
  if (_mi_os_is_huge_reserved(addr)) {
	  _mi_warning_message("cannot mprotect memory allocated in huge OS pages\n");
  }
  */
  int err = _mi_prim_protect(start,csize,protect);
  if (err != 0) {
    _mi_warning_message("cannot %s OS memory (error: %d (0x%x), address: %p, size: 0x%zx bytes)\n", (protect ? "protect" : "unprotect"), err, err, start, csize);
  }
  return (err == 0);
}

bool _mi_os_protect(void* addr, size_t size) {
  return mi_os_protectx(addr, size, true);
}

bool _mi_os_unprotect(void* addr, size_t size) {
  return mi_os_protectx(addr, size, false);
}



/* ----------------------------------------------------------------------------
Support for allocating huge OS pages (1Gib) that are reserved up-front
and possibly associated with a specific NUMA node. (use `numa_node>=0`)
-----------------------------------------------------------------------------*/
#define MI_HUGE_OS_PAGE_SIZE  (MI_GiB)


#if (MI_INTPTR_SIZE >= 8)
// To ensure proper alignment, use our own area for huge OS pages
static mi_decl_cache_align _Atomic(uintptr_t)  mi_huge_start; // = 0

// Claim an aligned address range for huge pages
static uint8_t* mi_os_claim_huge_pages(size_t pages, size_t* total_size) {
  if (total_size != NULL) *total_size = 0;
  const size_t size = pages * MI_HUGE_OS_PAGE_SIZE;

  uintptr_t start = 0;
  uintptr_t end = 0;
  uintptr_t huge_start = mi_atomic_load_relaxed(&mi_huge_start);
  do {
    start = huge_start;
    if (start == 0) {
      // Initialize the start address after the 32TiB area
      start = ((uintptr_t)32 << 40);  // 32TiB virtual start address
    #if (MI_SECURE>0 || MI_DEBUG==0)      // security: randomize start of huge pages unless in debug mode
      uintptr_t r = _mi_heap_random_next(mi_prim_get_default_heap());
      start = start + ((uintptr_t)MI_HUGE_OS_PAGE_SIZE * ((r>>17) & 0x0FFF));  // (randomly 12bits)*1GiB == between 0 to 4TiB
    #endif
    }
    end = start + size;
    mi_assert_internal(end % MI_SEGMENT_SIZE == 0);
  } while (!mi_atomic_cas_strong_acq_rel(&mi_huge_start, &huge_start, end));

  if (total_size != NULL) *total_size = size;
  return (uint8_t*)start;
}
#else
static uint8_t* mi_os_claim_huge_pages(size_t pages, size_t* total_size) {
  MI_UNUSED(pages);
  if (total_size != NULL) *total_size = 0;
  return NULL;
}
#endif

// Allocate MI_SEGMENT_SIZE aligned huge pages
void* _mi_os_alloc_huge_os_pages(size_t pages, int numa_node, mi_msecs_t max_msecs, size_t* pages_reserved, size_t* psize, mi_memid_t* memid) {
  *memid = _mi_memid_none();
  if (psize != NULL) *psize = 0;
  if (pages_reserved != NULL) *pages_reserved = 0;
  size_t size = 0;
  uint8_t* start = mi_os_claim_huge_pages(pages, &size);
  if (start == NULL) return NULL; // or 32-bit systems

  // Allocate one page at the time but try to place them contiguously
  // We allocate one page at the time to be able to abort if it takes too long
  // or to at least allocate as many as available on the system.
  mi_msecs_t start_t = _mi_clock_start();
  size_t page = 0;
  bool all_zero = true;
  while (page < pages) {
    // allocate a page
    bool is_zero = false;
    void* addr = start + (page * MI_HUGE_OS_PAGE_SIZE);
    void* p = NULL;
    int err = _mi_prim_alloc_huge_os_pages(addr, MI_HUGE_OS_PAGE_SIZE, numa_node, &is_zero, &p);
    if (!is_zero) { all_zero = false;  }
    if (err != 0) {
      _mi_warning_message("unable to allocate huge OS page (error: %d (0x%x), address: %p, size: %zx bytes)\n", err, err, addr, MI_HUGE_OS_PAGE_SIZE);
      break;
    }

    // Did we succeed at a contiguous address?
    if (p != addr) {
      // no success, issue a warning and break
      if (p != NULL) {
        _mi_warning_message("could not allocate contiguous huge OS page %zu at %p\n", page, addr);
        mi_os_prim_free(p, MI_HUGE_OS_PAGE_SIZE, true, &_mi_stats_main);
      }
      break;
    }

    // success, record it
    page++;  // increase before timeout check (see issue #711)
    _mi_stat_increase(&_mi_stats_main.committed, MI_HUGE_OS_PAGE_SIZE);
    _mi_stat_increase(&_mi_stats_main.reserved, MI_HUGE_OS_PAGE_SIZE);

    // check for timeout
    if (max_msecs > 0) {
      mi_msecs_t elapsed = _mi_clock_end(start_t);
      if (page >= 1) {
        mi_msecs_t estimate = ((elapsed / (page+1)) * pages);
        if (estimate > 2*max_msecs) { // seems like we are going to timeout, break
          elapsed = max_msecs + 1;
        }
      }
      if (elapsed > max_msecs) {
        _mi_warning_message("huge OS page allocation timed out (after allocating %zu page(s))\n", page);
        break;
      }
    }
  }
  mi_assert_internal(page*MI_HUGE_OS_PAGE_SIZE <= size);
  if (pages_reserved != NULL) { *pages_reserved = page; }
  if (psize != NULL) { *psize = page * MI_HUGE_OS_PAGE_SIZE; }
  if (page != 0) {
    mi_assert(start != NULL);
    *memid = _mi_memid_create_os(true /* is committed */, all_zero, true /* is_large */);
    memid->memkind = MI_MEM_OS_HUGE;
    mi_assert(memid->is_pinned);
    #ifdef MI_TRACK_ASAN
    if (all_zero) { mi_track_mem_defined(start,size); }
    #endif
  }
  return (page == 0 ? NULL : start);
}

// free every huge page in a range individually (as we allocated per page)
// note: needed with VirtualAlloc but could potentially be done in one go on mmap'd systems.
static void mi_os_free_huge_os_pages(void* p, size_t size, mi_stats_t* stats) {
  if (p==NULL || size==0) return;
  uint8_t* base = (uint8_t*)p;
  while (size >= MI_HUGE_OS_PAGE_SIZE) {
    mi_os_prim_free(base, MI_HUGE_OS_PAGE_SIZE, true, stats);
    size -= MI_HUGE_OS_PAGE_SIZE;
    base += MI_HUGE_OS_PAGE_SIZE;
  }
}

/* ----------------------------------------------------------------------------
Support NUMA aware allocation
-----------------------------------------------------------------------------*/

_Atomic(size_t)  _mi_numa_node_count; // = 0   // cache the node count

size_t _mi_os_numa_node_count_get(void) {
  size_t count = mi_atomic_load_acquire(&_mi_numa_node_count);
  if (count <= 0) {
    long ncount = mi_option_get(mi_option_use_numa_nodes); // given explicitly?
    if (ncount > 0) {
      count = (size_t)ncount;
    }
    else {
      count = _mi_prim_numa_node_count(); // or detect dynamically
      if (count == 0) count = 1;
    }
    mi_atomic_store_release(&_mi_numa_node_count, count); // save it
    _mi_verbose_message("using %zd numa regions\n", count);
  }
  return count;
}

int _mi_os_numa_node_get(mi_os_tld_t* tld) {
  MI_UNUSED(tld);
  size_t numa_count = _mi_os_numa_node_count();
  if (numa_count<=1) return 0; // optimize on single numa node systems: always node 0
  // never more than the node count and >= 0
  size_t numa_node = _mi_prim_numa_node();
  if (numa_node >= numa_count) { numa_node = numa_node % numa_count; }
  return (int)numa_node;
}
