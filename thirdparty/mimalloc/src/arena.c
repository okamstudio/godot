/* ----------------------------------------------------------------------------
Copyright (c) 2019-2023, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

/* ----------------------------------------------------------------------------
"Arenas" are fixed area's of OS memory from which we can allocate
large blocks (>= MI_ARENA_MIN_BLOCK_SIZE, 4MiB).
In contrast to the rest of mimalloc, the arenas are shared between
threads and need to be accessed using atomic operations.

Arenas are used to for huge OS page (1GiB) reservations or for reserving
OS memory upfront which can be improve performance or is sometimes needed
on embedded devices. We can also employ this with WASI or `sbrk` systems
to reserve large arenas upfront and be able to reuse the memory more effectively.

The arena allocation needs to be thread safe and we use an atomic bitmap to allocate.
-----------------------------------------------------------------------------*/
#include "mimalloc.h"
#include "mimalloc/internal.h"
#include "mimalloc/atomic.h"

#include <string.h>  // memset
#include <errno.h>   // ENOMEM

#include "bitmap.h"  // atomic bitmap

/* -----------------------------------------------------------
  Arena allocation
----------------------------------------------------------- */

// Block info: bit 0 contains the `in_use` bit, the upper bits the
// size in count of arena blocks.
typedef uintptr_t mi_block_info_t;
#define MI_ARENA_BLOCK_SIZE   (MI_SEGMENT_SIZE)        // 64MiB  (must be at least MI_SEGMENT_ALIGN)
#define MI_ARENA_MIN_OBJ_SIZE (MI_ARENA_BLOCK_SIZE/2)  // 32MiB
#define MI_MAX_ARENAS         (112)                    // not more than 126 (since we use 7 bits in the memid and an arena index + 1)

// A memory arena descriptor
typedef struct mi_arena_s {
  mi_arena_id_t id;                       // arena id; 0 for non-specific
  mi_memid_t memid;                       // memid of the memory area
  _Atomic(uint8_t*) start;                // the start of the memory area
  size_t   block_count;                   // size of the area in arena blocks (of `MI_ARENA_BLOCK_SIZE`)
  size_t   field_count;                   // number of bitmap fields (where `field_count * MI_BITMAP_FIELD_BITS >= block_count`)
  size_t   meta_size;                     // size of the arena structure itself (including its bitmaps)
  mi_memid_t meta_memid;                  // memid of the arena structure itself (OS or static allocation)
  int      numa_node;                     // associated NUMA node
  bool     exclusive;                     // only allow allocations if specifically for this arena
  bool     is_large;                      // memory area consists of large- or huge OS pages (always committed)
  _Atomic(size_t) search_idx;             // optimization to start the search for free blocks
  _Atomic(mi_msecs_t) purge_expire;       // expiration time when blocks should be decommitted from `blocks_decommit`.  
  mi_bitmap_field_t* blocks_dirty;        // are the blocks potentially non-zero?
  mi_bitmap_field_t* blocks_committed;    // are the blocks committed? (can be NULL for memory that cannot be decommitted)
  mi_bitmap_field_t* blocks_purge;        // blocks that can be (reset) decommitted. (can be NULL for memory that cannot be (reset) decommitted)
  mi_bitmap_field_t* blocks_abandoned;    // blocks that start with an abandoned segment. (This crosses API's but it is convenient to have here)
  mi_bitmap_field_t  blocks_inuse[1];     // in-place bitmap of in-use blocks (of size `field_count`)
  // do not add further fields here as the dirty, committed, purged, and abandoned bitmaps follow the inuse bitmap fields.
} mi_arena_t;


// The available arenas
static mi_decl_cache_align _Atomic(mi_arena_t*) mi_arenas[MI_MAX_ARENAS];
static mi_decl_cache_align _Atomic(size_t)      mi_arena_count; // = 0


//static bool mi_manage_os_memory_ex2(void* start, size_t size, bool is_large, int numa_node, bool exclusive, mi_memid_t memid, mi_arena_id_t* arena_id) mi_attr_noexcept;

/* -----------------------------------------------------------
  Arena id's
  id = arena_index + 1
----------------------------------------------------------- */

static size_t mi_arena_id_index(mi_arena_id_t id) {
  return (size_t)(id <= 0 ? MI_MAX_ARENAS : id - 1);
}

static mi_arena_id_t mi_arena_id_create(size_t arena_index) {
  mi_assert_internal(arena_index < MI_MAX_ARENAS);
  return (int)arena_index + 1;
}

mi_arena_id_t _mi_arena_id_none(void) {
  return 0;
}

static bool mi_arena_id_is_suitable(mi_arena_id_t arena_id, bool arena_is_exclusive, mi_arena_id_t req_arena_id) {
  return ((!arena_is_exclusive && req_arena_id == _mi_arena_id_none()) ||
          (arena_id == req_arena_id));
}

bool _mi_arena_memid_is_suitable(mi_memid_t memid, mi_arena_id_t request_arena_id) {
  if (memid.memkind == MI_MEM_ARENA) {
    return mi_arena_id_is_suitable(memid.mem.arena.id, memid.mem.arena.is_exclusive, request_arena_id);
  }
  else {
    return mi_arena_id_is_suitable(_mi_arena_id_none(), false, request_arena_id);
  }
}

bool _mi_arena_memid_is_os_allocated(mi_memid_t memid) {
  return (memid.memkind == MI_MEM_OS);
}

/* -----------------------------------------------------------
  Arena allocations get a (currently) 16-bit memory id where the
  lower 8 bits are the arena id, and the upper bits the block index.
----------------------------------------------------------- */

static size_t mi_block_count_of_size(size_t size) {
  return _mi_divide_up(size, MI_ARENA_BLOCK_SIZE);
}

static size_t mi_arena_block_size(size_t bcount) {
  return (bcount * MI_ARENA_BLOCK_SIZE);
}

static size_t mi_arena_size(mi_arena_t* arena) {
  return mi_arena_block_size(arena->block_count);
}

static mi_memid_t mi_memid_create_arena(mi_arena_id_t id, bool is_exclusive, mi_bitmap_index_t bitmap_index) {
  mi_memid_t memid = _mi_memid_create(MI_MEM_ARENA);
  memid.mem.arena.id = id;
  memid.mem.arena.block_index = bitmap_index;
  memid.mem.arena.is_exclusive = is_exclusive;
  return memid;
}

static bool mi_arena_memid_indices(mi_memid_t memid, size_t* arena_index, mi_bitmap_index_t* bitmap_index) {
  mi_assert_internal(memid.memkind == MI_MEM_ARENA);
  *arena_index = mi_arena_id_index(memid.mem.arena.id);
  *bitmap_index = memid.mem.arena.block_index;
  return memid.mem.arena.is_exclusive;
}



/* -----------------------------------------------------------
  Special static area for mimalloc internal structures
  to avoid OS calls (for example, for the arena metadata)
----------------------------------------------------------- */

#define MI_ARENA_STATIC_MAX  (MI_INTPTR_SIZE*MI_KiB)  // 8 KiB on 64-bit

static mi_decl_cache_align uint8_t mi_arena_static[MI_ARENA_STATIC_MAX];  // must be cache aligned, see issue #895
static mi_decl_cache_align _Atomic(size_t) mi_arena_static_top;

static void* mi_arena_static_zalloc(size_t size, size_t alignment, mi_memid_t* memid) {
  *memid = _mi_memid_none();
  if (size == 0 || size > MI_ARENA_STATIC_MAX) return NULL;
  const size_t toplow = mi_atomic_load_relaxed(&mi_arena_static_top);
  if ((toplow + size) > MI_ARENA_STATIC_MAX) return NULL;

  // try to claim space
  if (alignment < MI_MAX_ALIGN_SIZE) { alignment = MI_MAX_ALIGN_SIZE; }
  const size_t oversize = size + alignment - 1;
  if (toplow + oversize > MI_ARENA_STATIC_MAX) return NULL;
  const size_t oldtop = mi_atomic_add_acq_rel(&mi_arena_static_top, oversize);
  size_t top = oldtop + oversize;
  if (top > MI_ARENA_STATIC_MAX) {
    // try to roll back, ok if this fails
    mi_atomic_cas_strong_acq_rel(&mi_arena_static_top, &top, oldtop);
    return NULL;
  }

  // success
  *memid = _mi_memid_create(MI_MEM_STATIC);
  memid->initially_zero = true;
  const size_t start = _mi_align_up(oldtop, alignment);
  uint8_t* const p = &mi_arena_static[start];
  _mi_memzero_aligned(p, size);
  return p;
}

static void* mi_arena_meta_zalloc(size_t size, mi_memid_t* memid, mi_stats_t* stats) {
  *memid = _mi_memid_none();

  // try static
  void* p = mi_arena_static_zalloc(size, MI_MAX_ALIGN_SIZE, memid);
  if (p != NULL) return p;

  // or fall back to the OS
  p = _mi_os_alloc(size, memid, stats);
  if (p == NULL) return NULL;

  // zero the OS memory if needed
  if (!memid->initially_zero) {
    _mi_memzero_aligned(p, size);
    memid->initially_zero = true;
  }
  return p;
}

static void mi_arena_meta_free(void* p, mi_memid_t memid, size_t size, mi_stats_t* stats) {
  if (mi_memkind_is_os(memid.memkind)) {
    _mi_os_free(p, size, memid, stats);
  }
  else {
    mi_assert(memid.memkind == MI_MEM_STATIC);
  }
}

static void* mi_arena_block_start(mi_arena_t* arena, mi_bitmap_index_t bindex) {
  return (arena->start + mi_arena_block_size(mi_bitmap_index_bit(bindex)));
}


/* -----------------------------------------------------------
  Thread safe allocation in an arena
----------------------------------------------------------- */

// claim the `blocks_inuse` bits
static bool mi_arena_try_claim(mi_arena_t* arena, size_t blocks, mi_bitmap_index_t* bitmap_idx, mi_stats_t* stats)
{
  size_t idx = 0; // mi_atomic_load_relaxed(&arena->search_idx);  // start from last search; ok to be relaxed as the exact start does not matter
  if (_mi_bitmap_try_find_from_claim_across(arena->blocks_inuse, arena->field_count, idx, blocks, bitmap_idx, stats)) {
    mi_atomic_store_relaxed(&arena->search_idx, mi_bitmap_index_field(*bitmap_idx));  // start search from found location next time around
    return true;
  };
  return false;
}


/* -----------------------------------------------------------
  Arena Allocation
----------------------------------------------------------- */

static mi_decl_noinline void* mi_arena_try_alloc_at(mi_arena_t* arena, size_t arena_index, size_t needed_bcount,
                                                    bool commit, mi_memid_t* memid, mi_os_tld_t* tld)
{
  MI_UNUSED(arena_index);
  mi_assert_internal(mi_arena_id_index(arena->id) == arena_index);

  mi_bitmap_index_t bitmap_index;
  if (!mi_arena_try_claim(arena, needed_bcount, &bitmap_index, tld->stats)) return NULL;

  // claimed it!
  void* p = mi_arena_block_start(arena, bitmap_index);
  *memid = mi_memid_create_arena(arena->id, arena->exclusive, bitmap_index);
  memid->is_pinned = arena->memid.is_pinned;

  // none of the claimed blocks should be scheduled for a decommit
  if (arena->blocks_purge != NULL) {
    // this is thread safe as a potential purge only decommits parts that are not yet claimed as used (in `blocks_inuse`).
    _mi_bitmap_unclaim_across(arena->blocks_purge, arena->field_count, needed_bcount, bitmap_index);
  }

  // set the dirty bits (todo: no need for an atomic op here?)
  if (arena->memid.initially_zero && arena->blocks_dirty != NULL) {
    memid->initially_zero = _mi_bitmap_claim_across(arena->blocks_dirty, arena->field_count, needed_bcount, bitmap_index, NULL);
  }

  // set commit state
  if (arena->blocks_committed == NULL) {
    // always committed
    memid->initially_committed = true;
  }
  else if (commit) {
    // commit requested, but the range may not be committed as a whole: ensure it is committed now
    memid->initially_committed = true;
    bool any_uncommitted;
    _mi_bitmap_claim_across(arena->blocks_committed, arena->field_count, needed_bcount, bitmap_index, &any_uncommitted);
    if (any_uncommitted) {
      bool commit_zero = false;
      if (!_mi_os_commit(p, mi_arena_block_size(needed_bcount), &commit_zero, tld->stats)) {
        memid->initially_committed = false;
      }
      else {
        if (commit_zero) { memid->initially_zero = true; }
      }
    }
  }
  else {
    // no need to commit, but check if already fully committed
    memid->initially_committed = _mi_bitmap_is_claimed_across(arena->blocks_committed, arena->field_count, needed_bcount, bitmap_index);
  }

  return p;
}

// allocate in a speficic arena
static void* mi_arena_try_alloc_at_id(mi_arena_id_t arena_id, bool match_numa_node, int numa_node, size_t size, size_t alignment,
                                       bool commit, bool allow_large, mi_arena_id_t req_arena_id, mi_memid_t* memid, mi_os_tld_t* tld )
{
  MI_UNUSED_RELEASE(alignment);
  mi_assert_internal(alignment <= MI_SEGMENT_ALIGN);
  const size_t bcount = mi_block_count_of_size(size);
  const size_t arena_index = mi_arena_id_index(arena_id);
  mi_assert_internal(arena_index < mi_atomic_load_relaxed(&mi_arena_count));
  mi_assert_internal(size <= mi_arena_block_size(bcount));

  // Check arena suitability
  mi_arena_t* arena = mi_atomic_load_ptr_acquire(mi_arena_t, &mi_arenas[arena_index]);
  if (arena == NULL) return NULL;
  if (!allow_large && arena->is_large) return NULL;
  if (!mi_arena_id_is_suitable(arena->id, arena->exclusive, req_arena_id)) return NULL;
  if (req_arena_id == _mi_arena_id_none()) { // in not specific, check numa affinity
    const bool numa_suitable = (numa_node < 0 || arena->numa_node < 0 || arena->numa_node == numa_node);
    if (match_numa_node) { if (!numa_suitable) return NULL; }
                    else { if (numa_suitable) return NULL; }
  }

  // try to allocate
  void* p = mi_arena_try_alloc_at(arena, arena_index, bcount, commit, memid, tld);
  mi_assert_internal(p == NULL || _mi_is_aligned(p, alignment));
  return p;
}


// allocate from an arena with fallback to the OS
static mi_decl_noinline void* mi_arena_try_alloc(int numa_node, size_t size, size_t alignment,
                                                  bool commit, bool allow_large,
                                                  mi_arena_id_t req_arena_id, mi_memid_t* memid, mi_os_tld_t* tld )
{
  MI_UNUSED(alignment);
  mi_assert_internal(alignment <= MI_SEGMENT_ALIGN);
  const size_t max_arena = mi_atomic_load_relaxed(&mi_arena_count);
  if mi_likely(max_arena == 0) return NULL;

  if (req_arena_id != _mi_arena_id_none()) {
    // try a specific arena if requested
    if (mi_arena_id_index(req_arena_id) < max_arena) {
      void* p = mi_arena_try_alloc_at_id(req_arena_id, true, numa_node, size, alignment, commit, allow_large, req_arena_id, memid, tld);
      if (p != NULL) return p;
    }
  }
  else {
    // try numa affine allocation
    for (size_t i = 0; i < max_arena; i++) {
      void* p = mi_arena_try_alloc_at_id(mi_arena_id_create(i), true, numa_node, size, alignment, commit, allow_large, req_arena_id, memid, tld);
      if (p != NULL) return p;
    }

    // try from another numa node instead..
    if (numa_node >= 0) {  // if numa_node was < 0 (no specific affinity requested), all arena's have been tried already
      for (size_t i = 0; i < max_arena; i++) {
        void* p = mi_arena_try_alloc_at_id(mi_arena_id_create(i), false /* only proceed if not numa local */, numa_node, size, alignment, commit, allow_large, req_arena_id, memid, tld);
        if (p != NULL) return p;
      }
    }
  }
  return NULL;
}

// try to reserve a fresh arena space
static bool mi_arena_reserve(size_t req_size, bool allow_large, mi_arena_id_t req_arena_id, mi_arena_id_t *arena_id)
{
  if (_mi_preloading()) return false;  // use OS only while pre loading
  if (req_arena_id != _mi_arena_id_none()) return false;

  const size_t arena_count = mi_atomic_load_acquire(&mi_arena_count);
  if (arena_count > (MI_MAX_ARENAS - 4)) return false;

  size_t arena_reserve = mi_option_get_size(mi_option_arena_reserve);
  if (arena_reserve == 0) return false;

  if (!_mi_os_has_virtual_reserve()) {
    arena_reserve = arena_reserve/4;  // be conservative if virtual reserve is not supported (for WASM for example)
  }
  arena_reserve = _mi_align_up(arena_reserve, MI_ARENA_BLOCK_SIZE);
  if (arena_count >= 8 && arena_count <= 128) {
    arena_reserve = ((size_t)1<<(arena_count/8)) * arena_reserve;  // scale up the arena sizes exponentially
  }
  if (arena_reserve < req_size) return false;  // should be able to at least handle the current allocation size

  // commit eagerly?
  bool arena_commit = false;
  if (mi_option_get(mi_option_arena_eager_commit) == 2)      { arena_commit = _mi_os_has_overcommit(); }
  else if (mi_option_get(mi_option_arena_eager_commit) == 1) { arena_commit = true; }

  return (mi_reserve_os_memory_ex(arena_reserve, arena_commit, allow_large, false /* exclusive? */, arena_id) == 0);
}


void* _mi_arena_alloc_aligned(size_t size, size_t alignment, size_t align_offset, bool commit, bool allow_large,
                              mi_arena_id_t req_arena_id, mi_memid_t* memid, mi_os_tld_t* tld)
{
  mi_assert_internal(memid != NULL && tld != NULL);
  mi_assert_internal(size > 0);
  *memid = _mi_memid_none();

  const int numa_node = _mi_os_numa_node(tld); // current numa node

  // try to allocate in an arena if the alignment is small enough and the object is not too small (as for heap meta data)
  if (!mi_option_is_enabled(mi_option_disallow_arena_alloc) || req_arena_id != _mi_arena_id_none()) {  // is arena allocation allowed?
    if (size >= MI_ARENA_MIN_OBJ_SIZE && alignment <= MI_SEGMENT_ALIGN && align_offset == 0) {
      void* p = mi_arena_try_alloc(numa_node, size, alignment, commit, allow_large, req_arena_id, memid, tld);
      if (p != NULL) return p;

      // otherwise, try to first eagerly reserve a new arena
      if (req_arena_id == _mi_arena_id_none()) {
        mi_arena_id_t arena_id = 0;
        if (mi_arena_reserve(size, allow_large, req_arena_id, &arena_id)) {
          // and try allocate in there
          mi_assert_internal(req_arena_id == _mi_arena_id_none());
          p = mi_arena_try_alloc_at_id(arena_id, true, numa_node, size, alignment, commit, allow_large, req_arena_id, memid, tld);
          if (p != NULL) return p;
        }
      }
    }
  }

  // if we cannot use OS allocation, return NULL
  if (mi_option_is_enabled(mi_option_disallow_os_alloc) || req_arena_id != _mi_arena_id_none()) {
    errno = ENOMEM;
    return NULL;
  }

  // finally, fall back to the OS
  if (align_offset > 0) {
    return _mi_os_alloc_aligned_at_offset(size, alignment, align_offset, commit, allow_large, memid, tld->stats);
  }
  else {
    return _mi_os_alloc_aligned(size, alignment, commit, allow_large, memid, tld->stats);
  }
}

void* _mi_arena_alloc(size_t size, bool commit, bool allow_large, mi_arena_id_t req_arena_id, mi_memid_t* memid, mi_os_tld_t* tld)
{
  return _mi_arena_alloc_aligned(size, MI_ARENA_BLOCK_SIZE, 0, commit, allow_large, req_arena_id, memid, tld);
}


void* mi_arena_area(mi_arena_id_t arena_id, size_t* size) {
  if (size != NULL) *size = 0;
  size_t arena_index = mi_arena_id_index(arena_id);
  if (arena_index >= MI_MAX_ARENAS) return NULL;
  mi_arena_t* arena = mi_atomic_load_ptr_acquire(mi_arena_t, &mi_arenas[arena_index]);
  if (arena == NULL) return NULL;
  if (size != NULL) { *size = mi_arena_block_size(arena->block_count); }
  return arena->start;
}


/* -----------------------------------------------------------
  Arena purge
----------------------------------------------------------- */

static long mi_arena_purge_delay(void) {
  // <0 = no purging allowed, 0=immediate purging, >0=milli-second delay
  return (mi_option_get(mi_option_purge_delay) * mi_option_get(mi_option_arena_purge_mult));
}

// reset or decommit in an arena and update the committed/decommit bitmaps
// assumes we own the area (i.e. blocks_in_use is claimed by us)
static void mi_arena_purge(mi_arena_t* arena, size_t bitmap_idx, size_t blocks, mi_stats_t* stats) {
  mi_assert_internal(arena->blocks_committed != NULL);
  mi_assert_internal(arena->blocks_purge != NULL);
  mi_assert_internal(!arena->memid.is_pinned);
  const size_t size = mi_arena_block_size(blocks);
  void* const p = mi_arena_block_start(arena, bitmap_idx);
  bool needs_recommit;
  if (_mi_bitmap_is_claimed_across(arena->blocks_committed, arena->field_count, blocks, bitmap_idx)) {
    // all blocks are committed, we can purge freely
    needs_recommit = _mi_os_purge(p, size, stats);
  }
  else {
    // some blocks are not committed -- this can happen when a partially committed block is freed
    // in `_mi_arena_free` and it is conservatively marked as uncommitted but still scheduled for a purge
    // we need to ensure we do not try to reset (as that may be invalid for uncommitted memory),
    // and also undo the decommit stats (as it was already adjusted)
    mi_assert_internal(mi_option_is_enabled(mi_option_purge_decommits));
    needs_recommit = _mi_os_purge_ex(p, size, false /* allow reset? */, stats);
    if (needs_recommit) { _mi_stat_increase(&_mi_stats_main.committed, size); }
  }

  // clear the purged blocks
  _mi_bitmap_unclaim_across(arena->blocks_purge, arena->field_count, blocks, bitmap_idx);
  // update committed bitmap
  if (needs_recommit) {
    _mi_bitmap_unclaim_across(arena->blocks_committed, arena->field_count, blocks, bitmap_idx);
  }
}

// Schedule a purge. This is usually delayed to avoid repeated decommit/commit calls.
// Note: assumes we (still) own the area as we may purge immediately
static void mi_arena_schedule_purge(mi_arena_t* arena, size_t bitmap_idx, size_t blocks, mi_stats_t* stats) {
  mi_assert_internal(arena->blocks_purge != NULL);
  const long delay = mi_arena_purge_delay();
  if (delay < 0) return;  // is purging allowed at all?

  if (_mi_preloading() || delay == 0) {
    // decommit directly
    mi_arena_purge(arena, bitmap_idx, blocks, stats);
  }
  else {
    // schedule decommit
    mi_msecs_t expire = mi_atomic_loadi64_relaxed(&arena->purge_expire);
    if (expire != 0) {
      mi_atomic_addi64_acq_rel(&arena->purge_expire, (mi_msecs_t)(delay/10));  // add smallish extra delay
    }
    else {
      mi_atomic_storei64_release(&arena->purge_expire, _mi_clock_now() + delay);
    }
    _mi_bitmap_claim_across(arena->blocks_purge, arena->field_count, blocks, bitmap_idx, NULL);
  }
}

// purge a range of blocks
// return true if the full range was purged.
// assumes we own the area (i.e. blocks_in_use is claimed by us)
static bool mi_arena_purge_range(mi_arena_t* arena, size_t idx, size_t startidx, size_t bitlen, size_t purge, mi_stats_t* stats) {
  const size_t endidx = startidx + bitlen;
  size_t bitidx = startidx;
  bool all_purged = false;
  while (bitidx < endidx) {
    // count consequetive ones in the purge mask
    size_t count = 0;
    while (bitidx + count < endidx && (purge & ((size_t)1 << (bitidx + count))) != 0) {
      count++;
    }
    if (count > 0) {
      // found range to be purged
      const mi_bitmap_index_t range_idx = mi_bitmap_index_create(idx, bitidx);
      mi_arena_purge(arena, range_idx, count, stats);
      if (count == bitlen) {
        all_purged = true;
      }
    }
    bitidx += (count+1); // +1 to skip the zero bit (or end)
  }
  return all_purged;
}

// returns true if anything was purged
static bool mi_arena_try_purge(mi_arena_t* arena, mi_msecs_t now, bool force, mi_stats_t* stats)
{
  if (arena->memid.is_pinned || arena->blocks_purge == NULL) return false;
  mi_msecs_t expire = mi_atomic_loadi64_relaxed(&arena->purge_expire);
  if (expire == 0) return false;
  if (!force && expire > now) return false;

  // reset expire (if not already set concurrently)
  mi_atomic_casi64_strong_acq_rel(&arena->purge_expire, &expire, (mi_msecs_t)0);

  // potential purges scheduled, walk through the bitmap
  bool any_purged = false;
  bool full_purge = true;
  for (size_t i = 0; i < arena->field_count; i++) {
    size_t purge = mi_atomic_load_relaxed(&arena->blocks_purge[i]);
    if (purge != 0) {
      size_t bitidx = 0;
      while (bitidx < MI_BITMAP_FIELD_BITS) {
        // find consequetive range of ones in the purge mask
        size_t bitlen = 0;
        while (bitidx + bitlen < MI_BITMAP_FIELD_BITS && (purge & ((size_t)1 << (bitidx + bitlen))) != 0) {
          bitlen++;
        }
        // try to claim the longest range of corresponding in_use bits
        const mi_bitmap_index_t bitmap_index = mi_bitmap_index_create(i, bitidx);
        while( bitlen > 0 ) {
          if (_mi_bitmap_try_claim(arena->blocks_inuse, arena->field_count, bitlen, bitmap_index)) {
            break;
          }
          bitlen--;
        }
        // actual claimed bits at `in_use`
        if (bitlen > 0) {
          // read purge again now that we have the in_use bits
          purge = mi_atomic_load_acquire(&arena->blocks_purge[i]);
          if (!mi_arena_purge_range(arena, i, bitidx, bitlen, purge, stats)) {
            full_purge = false;
          }
          any_purged = true;
          // release the claimed `in_use` bits again
          _mi_bitmap_unclaim(arena->blocks_inuse, arena->field_count, bitlen, bitmap_index);
        }
        bitidx += (bitlen+1);  // +1 to skip the zero (or end)
      } // while bitidx
    } // purge != 0
  }
  // if not fully purged, make sure to purge again in the future
  if (!full_purge) {
    const long delay = mi_arena_purge_delay();
    mi_msecs_t expected = 0;
    mi_atomic_casi64_strong_acq_rel(&arena->purge_expire,&expected,_mi_clock_now() + delay);
  }
  return any_purged;
}

static void mi_arenas_try_purge( bool force, bool visit_all, mi_stats_t* stats ) {
  if (_mi_preloading() || mi_arena_purge_delay() <= 0) return;  // nothing will be scheduled

  const size_t max_arena = mi_atomic_load_acquire(&mi_arena_count);
  if (max_arena == 0) return;

  // allow only one thread to purge at a time
  static mi_atomic_guard_t purge_guard;
  mi_atomic_guard(&purge_guard)
  {
    mi_msecs_t now = _mi_clock_now();
    size_t max_purge_count = (visit_all ? max_arena : 1);
    for (size_t i = 0; i < max_arena; i++) {
      mi_arena_t* arena = mi_atomic_load_ptr_acquire(mi_arena_t, &mi_arenas[i]);
      if (arena != NULL) {
        if (mi_arena_try_purge(arena, now, force, stats)) {
          if (max_purge_count <= 1) break;
          max_purge_count--;
        }
      }
    }
  }
}


/* -----------------------------------------------------------
  Arena free
----------------------------------------------------------- */

void _mi_arena_free(void* p, size_t size, size_t committed_size, mi_memid_t memid, mi_stats_t* stats) {
  mi_assert_internal(size > 0 && stats != NULL);
  mi_assert_internal(committed_size <= size);
  if (p==NULL) return;
  if (size==0) return;
  const bool all_committed = (committed_size == size);

  if (mi_memkind_is_os(memid.memkind)) {
    // was a direct OS allocation, pass through
    if (!all_committed && committed_size > 0) {
      // if partially committed, adjust the committed stats (as `_mi_os_free` will increase decommit by the full size)
      _mi_stat_decrease(&_mi_stats_main.committed, committed_size);
    }
    _mi_os_free(p, size, memid, stats);
  }
  else if (memid.memkind == MI_MEM_ARENA) {
    // allocated in an arena
    size_t arena_idx;
    size_t bitmap_idx;
    mi_arena_memid_indices(memid, &arena_idx, &bitmap_idx);
    mi_assert_internal(arena_idx < MI_MAX_ARENAS);
    mi_arena_t* arena = mi_atomic_load_ptr_acquire(mi_arena_t,&mi_arenas[arena_idx]);
    mi_assert_internal(arena != NULL);
    const size_t blocks = mi_block_count_of_size(size);

    // checks
    if (arena == NULL) {
      _mi_error_message(EINVAL, "trying to free from an invalid arena: %p, size %zu, memid: 0x%zx\n", p, size, memid);
      return;
    }
    mi_assert_internal(arena->field_count > mi_bitmap_index_field(bitmap_idx));
    if (arena->field_count <= mi_bitmap_index_field(bitmap_idx)) {
      _mi_error_message(EINVAL, "trying to free from an invalid arena block: %p, size %zu, memid: 0x%zx\n", p, size, memid);
      return;
    }

    // need to set all memory to undefined as some parts may still be marked as no_access (like padding etc.)
    mi_track_mem_undefined(p,size);

    // potentially decommit
    if (arena->memid.is_pinned || arena->blocks_committed == NULL) {
      mi_assert_internal(all_committed);
    }
    else {
      mi_assert_internal(arena->blocks_committed != NULL);
      mi_assert_internal(arena->blocks_purge != NULL);

      if (!all_committed) {
        // mark the entire range as no longer committed (so we recommit the full range when re-using)
        _mi_bitmap_unclaim_across(arena->blocks_committed, arena->field_count, blocks, bitmap_idx);
        mi_track_mem_noaccess(p,size);
        if (committed_size > 0) {
          // if partially committed, adjust the committed stats (is it will be recommitted when re-using)
          // in the delayed purge, we now need to not count a decommit if the range is not marked as committed.
          _mi_stat_decrease(&_mi_stats_main.committed, committed_size);
        }
        // note: if not all committed, it may be that the purge will reset/decommit the entire range
        // that contains already decommitted parts. Since purge consistently uses reset or decommit that
        // works (as we should never reset decommitted parts).
      }
      // (delay) purge the entire range
      mi_arena_schedule_purge(arena, bitmap_idx, blocks, stats);
    }

    // and make it available to others again
    bool all_inuse = _mi_bitmap_unclaim_across(arena->blocks_inuse, arena->field_count, blocks, bitmap_idx);
    if (!all_inuse) {
      _mi_error_message(EAGAIN, "trying to free an already freed arena block: %p, size %zu\n", p, size);
      return;
    };
  }
  else {
    // arena was none, external, or static; nothing to do
    mi_assert_internal(memid.memkind < MI_MEM_OS);
  }

  // purge expired decommits
  mi_arenas_try_purge(false, false, stats);
}

// destroy owned arenas; this is unsafe and should only be done using `mi_option_destroy_on_exit`
// for dynamic libraries that are unloaded and need to release all their allocated memory.
static void mi_arenas_unsafe_destroy(void) {
  const size_t max_arena = mi_atomic_load_relaxed(&mi_arena_count);
  size_t new_max_arena = 0;
  for (size_t i = 0; i < max_arena; i++) {
    mi_arena_t* arena = mi_atomic_load_ptr_acquire(mi_arena_t, &mi_arenas[i]);
    if (arena != NULL) {
      if (arena->start != NULL && mi_memkind_is_os(arena->memid.memkind)) {
        mi_atomic_store_ptr_release(mi_arena_t, &mi_arenas[i], NULL);
        _mi_os_free(arena->start, mi_arena_size(arena), arena->memid, &_mi_stats_main);
      }
      else {
        new_max_arena = i;
      }
      mi_arena_meta_free(arena, arena->meta_memid, arena->meta_size, &_mi_stats_main);
    }
  }

  // try to lower the max arena.
  size_t expected = max_arena;
  mi_atomic_cas_strong_acq_rel(&mi_arena_count, &expected, new_max_arena);
}

// Purge the arenas; if `force_purge` is true, amenable parts are purged even if not yet expired
void _mi_arenas_collect(bool force_purge, mi_stats_t* stats) {
  mi_arenas_try_purge(force_purge, force_purge /* visit all? */, stats);
}

// destroy owned arenas; this is unsafe and should only be done using `mi_option_destroy_on_exit`
// for dynamic libraries that are unloaded and need to release all their allocated memory.
void _mi_arena_unsafe_destroy_all(mi_stats_t* stats) {
  mi_arenas_unsafe_destroy();
  _mi_arenas_collect(true /* force purge */, stats);  // purge non-owned arenas
}

// Is a pointer inside any of our arenas?
bool _mi_arena_contains(const void* p) {
  const size_t max_arena = mi_atomic_load_relaxed(&mi_arena_count);
  for (size_t i = 0; i < max_arena; i++) {
    mi_arena_t* arena = mi_atomic_load_ptr_acquire(mi_arena_t, &mi_arenas[i]);
    if (arena != NULL && arena->start <= (const uint8_t*)p && arena->start + mi_arena_block_size(arena->block_count) > (const uint8_t*)p) {
      return true;
    }
  }
  return false;
}

/* -----------------------------------------------------------
  Abandoned blocks/segments.
  This is used to atomically abandon/reclaim segments 
  (and crosses the arena API but it is convenient to have here).
  Abandoned segments still have live blocks; they get reclaimed
  when a thread frees a block in it, or when a thread needs a fresh
  segment; these threads scan the abandoned segments through
  the arena bitmaps.
----------------------------------------------------------- */

// Maintain a count of all abandoned segments
static mi_decl_cache_align _Atomic(size_t)abandoned_count;

size_t _mi_arena_segment_abandoned_count(void) {
  return mi_atomic_load_relaxed(&abandoned_count);
}

// reclaim a specific abandoned segment; `true` on success.
// sets the thread_id.
bool _mi_arena_segment_clear_abandoned(mi_segment_t* segment ) 
{
  if (segment->memid.memkind != MI_MEM_ARENA) {
    // not in an arena, consider it un-abandoned now.
    // but we need to still claim it atomically -- we use the thread_id for that.
    size_t expected = 0;
    if (mi_atomic_cas_strong_acq_rel(&segment->thread_id, &expected, _mi_thread_id())) {
      mi_atomic_decrement_relaxed(&abandoned_count);
      return true;
    }
    else {
      return false;
    }
  }
  // arena segment: use the blocks_abandoned bitmap.
  size_t arena_idx;
  size_t bitmap_idx;
  mi_arena_memid_indices(segment->memid, &arena_idx, &bitmap_idx);
  mi_assert_internal(arena_idx < MI_MAX_ARENAS);
  mi_arena_t* arena = mi_atomic_load_ptr_acquire(mi_arena_t, &mi_arenas[arena_idx]);
  mi_assert_internal(arena != NULL);
  bool was_marked = _mi_bitmap_unclaim(arena->blocks_abandoned, arena->field_count, 1, bitmap_idx);
  if (was_marked) { 
    mi_assert_internal(mi_atomic_load_relaxed(&segment->thread_id) == 0);
    mi_atomic_decrement_relaxed(&abandoned_count); 
    mi_atomic_store_release(&segment->thread_id, _mi_thread_id());
  }
  // mi_assert_internal(was_marked);
  mi_assert_internal(!was_marked || _mi_bitmap_is_claimed(arena->blocks_inuse, arena->field_count, 1, bitmap_idx));
  //mi_assert_internal(arena->blocks_committed == NULL || _mi_bitmap_is_claimed(arena->blocks_committed, arena->field_count, 1, bitmap_idx));
  return was_marked;
}

// mark a specific segment as abandoned
// clears the thread_id.
void _mi_arena_segment_mark_abandoned(mi_segment_t* segment) 
{
  mi_atomic_store_release(&segment->thread_id, 0);
  mi_assert_internal(segment->used == segment->abandoned);
  if (segment->memid.memkind != MI_MEM_ARENA) {
    // not in an arena; count it as abandoned and return
    mi_atomic_increment_relaxed(&abandoned_count);
    return;
  }
  size_t arena_idx;
  size_t bitmap_idx;
  mi_arena_memid_indices(segment->memid, &arena_idx, &bitmap_idx);
  mi_assert_internal(arena_idx < MI_MAX_ARENAS);
  mi_arena_t* arena = mi_atomic_load_ptr_acquire(mi_arena_t, &mi_arenas[arena_idx]);
  mi_assert_internal(arena != NULL);
  const bool was_unmarked = _mi_bitmap_claim(arena->blocks_abandoned, arena->field_count, 1, bitmap_idx, NULL);
  if (was_unmarked) { mi_atomic_increment_relaxed(&abandoned_count); }
  mi_assert_internal(was_unmarked);
  mi_assert_internal(_mi_bitmap_is_claimed(arena->blocks_inuse, arena->field_count, 1, bitmap_idx));
}

// start a cursor at a randomized arena
void _mi_arena_field_cursor_init(mi_heap_t* heap, mi_arena_field_cursor_t* current) {
  const size_t max_arena = mi_atomic_load_relaxed(&mi_arena_count);
  current->start = (max_arena == 0 ? 0 : (mi_arena_id_t)( _mi_heap_random_next(heap) % max_arena));
  current->count = 0;
  current->bitmap_idx = 0;  
}

// reclaim abandoned segments 
// this does not set the thread id (so it appears as still abandoned)
mi_segment_t* _mi_arena_segment_clear_abandoned_next(mi_arena_field_cursor_t* previous ) 
{
  const int max_arena = (int)mi_atomic_load_relaxed(&mi_arena_count);
  if (max_arena <= 0 || mi_atomic_load_relaxed(&abandoned_count) == 0) return NULL;

  int count = previous->count;
  size_t field_idx = mi_bitmap_index_field(previous->bitmap_idx);
  size_t bit_idx = mi_bitmap_index_bit_in_field(previous->bitmap_idx) + 1;
  // visit arena's (from previous)
  for (; count < max_arena; count++, field_idx = 0, bit_idx = 0) {
    mi_arena_id_t arena_idx = previous->start + count;
    if (arena_idx >= max_arena) { arena_idx = arena_idx % max_arena; } // wrap around
    mi_arena_t* arena = mi_atomic_load_ptr_acquire(mi_arena_t, &mi_arenas[arena_idx]);
    if (arena != NULL) {
      // visit the abandoned fields (starting at previous_idx)
      for ( ; field_idx < arena->field_count; field_idx++, bit_idx = 0) {
        size_t field = mi_atomic_load_relaxed(&arena->blocks_abandoned[field_idx]);
        if mi_unlikely(field != 0) { // skip zero fields quickly
          // visit each set bit in the field  (todo: maybe use `ctz` here?)
          for ( ; bit_idx < MI_BITMAP_FIELD_BITS; bit_idx++) {
            // pre-check if the bit is set
            size_t mask = ((size_t)1 << bit_idx);
            if mi_unlikely((field & mask) == mask) {
              mi_bitmap_index_t bitmap_idx = mi_bitmap_index_create(field_idx, bit_idx);
              // try to reclaim it atomically
              if (_mi_bitmap_unclaim(arena->blocks_abandoned, arena->field_count, 1, bitmap_idx)) {
                mi_atomic_decrement_relaxed(&abandoned_count);
                previous->bitmap_idx = bitmap_idx;
                previous->count = count;
                mi_assert_internal(_mi_bitmap_is_claimed(arena->blocks_inuse, arena->field_count, 1, bitmap_idx));
                mi_segment_t* segment = (mi_segment_t*)mi_arena_block_start(arena, bitmap_idx);
                mi_assert_internal(mi_atomic_load_relaxed(&segment->thread_id) == 0);
                //mi_assert_internal(arena->blocks_committed == NULL || _mi_bitmap_is_claimed(arena->blocks_committed, arena->field_count, 1, bitmap_idx));
                return segment;
              }
            }
          }
        }
      }
    }
  }
  // no more found
  previous->bitmap_idx = 0;
  previous->count = 0;
  return NULL;
}


/* -----------------------------------------------------------
  Add an arena.
----------------------------------------------------------- */

static bool mi_arena_add(mi_arena_t* arena, mi_arena_id_t* arena_id, mi_stats_t* stats) {
  mi_assert_internal(arena != NULL);
  mi_assert_internal((uintptr_t)mi_atomic_load_ptr_relaxed(uint8_t,&arena->start) % MI_SEGMENT_ALIGN == 0);
  mi_assert_internal(arena->block_count > 0);
  if (arena_id != NULL) { *arena_id = -1; }

  size_t i = mi_atomic_increment_acq_rel(&mi_arena_count);
  if (i >= MI_MAX_ARENAS) {
    mi_atomic_decrement_acq_rel(&mi_arena_count);
    return false;
  }
  _mi_stat_counter_increase(&stats->arena_count,1);
  arena->id = mi_arena_id_create(i);
  mi_atomic_store_ptr_release(mi_arena_t,&mi_arenas[i], arena);
  if (arena_id != NULL) { *arena_id = arena->id; }
  return true;
}

static bool mi_manage_os_memory_ex2(void* start, size_t size, bool is_large, int numa_node, bool exclusive, mi_memid_t memid, mi_arena_id_t* arena_id) mi_attr_noexcept
{
  if (arena_id != NULL) *arena_id = _mi_arena_id_none();
  if (size < MI_ARENA_BLOCK_SIZE) return false;

  if (is_large) {
    mi_assert_internal(memid.initially_committed && memid.is_pinned);
  }

  const size_t bcount = size / MI_ARENA_BLOCK_SIZE;
  const size_t fields = _mi_divide_up(bcount, MI_BITMAP_FIELD_BITS);
  const size_t bitmaps = (memid.is_pinned ? 3 : 5);
  const size_t asize  = sizeof(mi_arena_t) + (bitmaps*fields*sizeof(mi_bitmap_field_t));
  mi_memid_t meta_memid;
  mi_arena_t* arena   = (mi_arena_t*)mi_arena_meta_zalloc(asize, &meta_memid, &_mi_stats_main); // TODO: can we avoid allocating from the OS?
  if (arena == NULL) return false;

  // already zero'd due to zalloc
  // _mi_memzero(arena, asize);
  arena->id = _mi_arena_id_none();
  arena->memid = memid;
  arena->exclusive = exclusive;
  arena->meta_size = asize;
  arena->meta_memid = meta_memid;
  arena->block_count = bcount;
  arena->field_count = fields;
  arena->start = (uint8_t*)start;
  arena->numa_node    = numa_node; // TODO: or get the current numa node if -1? (now it allows anyone to allocate on -1)
  arena->is_large     = is_large;
  arena->purge_expire = 0;
  arena->search_idx   = 0;
  // consequetive bitmaps
  arena->blocks_dirty     = &arena->blocks_inuse[fields];     // just after inuse bitmap
  arena->blocks_abandoned = &arena->blocks_inuse[2 * fields]; // just after dirty bitmap
  arena->blocks_committed = (arena->memid.is_pinned ? NULL : &arena->blocks_inuse[3*fields]); // just after abandoned bitmap
  arena->blocks_purge     = (arena->memid.is_pinned ? NULL : &arena->blocks_inuse[4*fields]); // just after committed bitmap
  // initialize committed bitmap?
  if (arena->blocks_committed != NULL && arena->memid.initially_committed) {
    memset((void*)arena->blocks_committed, 0xFF, fields*sizeof(mi_bitmap_field_t)); // cast to void* to avoid atomic warning
  }

  // and claim leftover blocks if needed (so we never allocate there)
  ptrdiff_t post = (fields * MI_BITMAP_FIELD_BITS) - bcount;
  mi_assert_internal(post >= 0);
  if (post > 0) {
    // don't use leftover bits at the end
    mi_bitmap_index_t postidx = mi_bitmap_index_create(fields - 1, MI_BITMAP_FIELD_BITS - post);
    _mi_bitmap_claim(arena->blocks_inuse, fields, post, postidx, NULL);
  }
  return mi_arena_add(arena, arena_id, &_mi_stats_main);

}

bool mi_manage_os_memory_ex(void* start, size_t size, bool is_committed, bool is_large, bool is_zero, int numa_node, bool exclusive, mi_arena_id_t* arena_id) mi_attr_noexcept {
  mi_memid_t memid = _mi_memid_create(MI_MEM_EXTERNAL);
  memid.initially_committed = is_committed;
  memid.initially_zero = is_zero;
  memid.is_pinned = is_large;
  return mi_manage_os_memory_ex2(start,size,is_large,numa_node,exclusive,memid, arena_id);
}

// Reserve a range of regular OS memory
int mi_reserve_os_memory_ex(size_t size, bool commit, bool allow_large, bool exclusive, mi_arena_id_t* arena_id) mi_attr_noexcept {
  if (arena_id != NULL) *arena_id = _mi_arena_id_none();
  size = _mi_align_up(size, MI_ARENA_BLOCK_SIZE); // at least one block
  mi_memid_t memid;
  void* start = _mi_os_alloc_aligned(size, MI_SEGMENT_ALIGN, commit, allow_large, &memid, &_mi_stats_main);
  if (start == NULL) return ENOMEM;
  const bool is_large = memid.is_pinned; // todo: use separate is_large field?
  if (!mi_manage_os_memory_ex2(start, size, is_large, -1 /* numa node */, exclusive, memid, arena_id)) {
    _mi_os_free_ex(start, size, commit, memid, &_mi_stats_main);
    _mi_verbose_message("failed to reserve %zu KiB memory\n", _mi_divide_up(size, 1024));
    return ENOMEM;
  }
  _mi_verbose_message("reserved %zu KiB memory%s\n", _mi_divide_up(size, 1024), is_large ? " (in large os pages)" : "");
  return 0;
}


// Manage a range of regular OS memory
bool mi_manage_os_memory(void* start, size_t size, bool is_committed, bool is_large, bool is_zero, int numa_node) mi_attr_noexcept {
  return mi_manage_os_memory_ex(start, size, is_committed, is_large, is_zero, numa_node, false /* exclusive? */, NULL);
}

// Reserve a range of regular OS memory
int mi_reserve_os_memory(size_t size, bool commit, bool allow_large) mi_attr_noexcept {
  return mi_reserve_os_memory_ex(size, commit, allow_large, false, NULL);
}


/* -----------------------------------------------------------
  Debugging
----------------------------------------------------------- */

static size_t mi_debug_show_bitmap(const char* prefix, const char* header, size_t block_count, mi_bitmap_field_t* fields, size_t field_count ) {
  _mi_verbose_message("%s%s:\n", prefix, header);
  size_t bcount = 0;
  size_t inuse_count = 0;
  for (size_t i = 0; i < field_count; i++) {
    char buf[MI_BITMAP_FIELD_BITS + 1];
    uintptr_t field = mi_atomic_load_relaxed(&fields[i]);
    for (size_t bit = 0; bit < MI_BITMAP_FIELD_BITS; bit++, bcount++) {
      if (bcount < block_count) {
        bool inuse = ((((uintptr_t)1 << bit) & field) != 0);
        if (inuse) inuse_count++;
        buf[bit] = (inuse ? 'x' : '.');
      }
      else {
        buf[bit] = ' ';
      }
    }
    buf[MI_BITMAP_FIELD_BITS] = 0;
    _mi_verbose_message("%s  %s\n", prefix, buf);
  }
  _mi_verbose_message("%s  total ('x'): %zu\n", prefix, inuse_count);
  return inuse_count;
}

void mi_debug_show_arenas(bool show_inuse, bool show_abandoned, bool show_purge) mi_attr_noexcept {
  size_t max_arenas = mi_atomic_load_relaxed(&mi_arena_count);
  size_t inuse_total = 0;
  size_t abandoned_total = 0;
  size_t purge_total = 0;
  for (size_t i = 0; i < max_arenas; i++) {
    mi_arena_t* arena = mi_atomic_load_ptr_relaxed(mi_arena_t, &mi_arenas[i]);
    if (arena == NULL) break;
    _mi_verbose_message("arena %zu: %zu blocks of size %zuMiB (in %zu fields) %s\n", i, arena->block_count, MI_ARENA_BLOCK_SIZE / MI_MiB, arena->field_count, (arena->memid.is_pinned ? ", pinned" : ""));
    if (show_inuse) {
      inuse_total += mi_debug_show_bitmap("  ", "inuse blocks", arena->block_count, arena->blocks_inuse, arena->field_count);
    }
    if (arena->blocks_committed != NULL) {
      mi_debug_show_bitmap("  ", "committed blocks", arena->block_count, arena->blocks_committed, arena->field_count);
    }
    if (show_abandoned) {
      abandoned_total += mi_debug_show_bitmap("  ", "abandoned blocks", arena->block_count, arena->blocks_abandoned, arena->field_count);      
    }
    if (show_purge && arena->blocks_purge != NULL) {
      purge_total += mi_debug_show_bitmap("  ", "purgeable blocks", arena->block_count, arena->blocks_purge, arena->field_count);
    }
  }
  if (show_inuse)     _mi_verbose_message("total inuse blocks    : %zu\n", inuse_total);
  if (show_abandoned) _mi_verbose_message("total abandoned blocks: %zu\n", abandoned_total);
  if (show_purge)     _mi_verbose_message("total purgeable blocks: %zu\n", purge_total);
}


/* -----------------------------------------------------------
  Reserve a huge page arena.
----------------------------------------------------------- */
// reserve at a specific numa node
int mi_reserve_huge_os_pages_at_ex(size_t pages, int numa_node, size_t timeout_msecs, bool exclusive, mi_arena_id_t* arena_id) mi_attr_noexcept {
  if (arena_id != NULL) *arena_id = -1;
  if (pages==0) return 0;
  if (numa_node < -1) numa_node = -1;
  if (numa_node >= 0) numa_node = numa_node % _mi_os_numa_node_count();
  size_t hsize = 0;
  size_t pages_reserved = 0;
  mi_memid_t memid;
  void* p = _mi_os_alloc_huge_os_pages(pages, numa_node, timeout_msecs, &pages_reserved, &hsize, &memid);
  if (p==NULL || pages_reserved==0) {
    _mi_warning_message("failed to reserve %zu GiB huge pages\n", pages);
    return ENOMEM;
  }
  _mi_verbose_message("numa node %i: reserved %zu GiB huge pages (of the %zu GiB requested)\n", numa_node, pages_reserved, pages);

  if (!mi_manage_os_memory_ex2(p, hsize, true, numa_node, exclusive, memid, arena_id)) {
    _mi_os_free(p, hsize, memid, &_mi_stats_main);
    return ENOMEM;
  }
  return 0;
}

int mi_reserve_huge_os_pages_at(size_t pages, int numa_node, size_t timeout_msecs) mi_attr_noexcept {
  return mi_reserve_huge_os_pages_at_ex(pages, numa_node, timeout_msecs, false, NULL);
}

// reserve huge pages evenly among the given number of numa nodes (or use the available ones as detected)
int mi_reserve_huge_os_pages_interleave(size_t pages, size_t numa_nodes, size_t timeout_msecs) mi_attr_noexcept {
  if (pages == 0) return 0;

  // pages per numa node
  size_t numa_count = (numa_nodes > 0 ? numa_nodes : _mi_os_numa_node_count());
  if (numa_count <= 0) numa_count = 1;
  const size_t pages_per = pages / numa_count;
  const size_t pages_mod = pages % numa_count;
  const size_t timeout_per = (timeout_msecs==0 ? 0 : (timeout_msecs / numa_count) + 50);

  // reserve evenly among numa nodes
  for (size_t numa_node = 0; numa_node < numa_count && pages > 0; numa_node++) {
    size_t node_pages = pages_per;  // can be 0
    if (numa_node < pages_mod) node_pages++;
    int err = mi_reserve_huge_os_pages_at(node_pages, (int)numa_node, timeout_per);
    if (err) return err;
    if (pages < node_pages) {
      pages = 0;
    }
    else {
      pages -= node_pages;
    }
  }

  return 0;
}

int mi_reserve_huge_os_pages(size_t pages, double max_secs, size_t* pages_reserved) mi_attr_noexcept {
  MI_UNUSED(max_secs);
  _mi_warning_message("mi_reserve_huge_os_pages is deprecated: use mi_reserve_huge_os_pages_interleave/at instead\n");
  if (pages_reserved != NULL) *pages_reserved = 0;
  int err = mi_reserve_huge_os_pages_interleave(pages, 0, (size_t)(max_secs * 1000.0));
  if (err==0 && pages_reserved!=NULL) *pages_reserved = pages;
  return err;
}

