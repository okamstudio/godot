/* ----------------------------------------------------------------------------
Copyright (c) 2018-2024, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/
#include "mimalloc.h"
#include "mimalloc/internal.h"
#include "mimalloc/atomic.h"

#include <string.h>  // memset
#include <stdio.h>

// -------------------------------------------------------------------
// Segments
// mimalloc pages reside in segments. See `mi_segment_valid` for invariants.
// -------------------------------------------------------------------


static void mi_segment_try_purge(mi_segment_t* segment, bool force, mi_stats_t* stats);


// -------------------------------------------------------------------
// commit mask
// -------------------------------------------------------------------

static bool mi_commit_mask_all_set(const mi_commit_mask_t* commit, const mi_commit_mask_t* cm) {
  for (size_t i = 0; i < MI_COMMIT_MASK_FIELD_COUNT; i++) {
    if ((commit->mask[i] & cm->mask[i]) != cm->mask[i]) return false;
  }
  return true;
}

static bool mi_commit_mask_any_set(const mi_commit_mask_t* commit, const mi_commit_mask_t* cm) {
  for (size_t i = 0; i < MI_COMMIT_MASK_FIELD_COUNT; i++) {
    if ((commit->mask[i] & cm->mask[i]) != 0) return true;
  }
  return false;
}

static void mi_commit_mask_create_intersect(const mi_commit_mask_t* commit, const mi_commit_mask_t* cm, mi_commit_mask_t* res) {
  for (size_t i = 0; i < MI_COMMIT_MASK_FIELD_COUNT; i++) {
    res->mask[i] = (commit->mask[i] & cm->mask[i]);
  }
}

static void mi_commit_mask_clear(mi_commit_mask_t* res, const mi_commit_mask_t* cm) {
  for (size_t i = 0; i < MI_COMMIT_MASK_FIELD_COUNT; i++) {
    res->mask[i] &= ~(cm->mask[i]);
  }
}

static void mi_commit_mask_set(mi_commit_mask_t* res, const mi_commit_mask_t* cm) {
  for (size_t i = 0; i < MI_COMMIT_MASK_FIELD_COUNT; i++) {
    res->mask[i] |= cm->mask[i];
  }
}

static void mi_commit_mask_create(size_t bitidx, size_t bitcount, mi_commit_mask_t* cm) {
  mi_assert_internal(bitidx < MI_COMMIT_MASK_BITS);
  mi_assert_internal((bitidx + bitcount) <= MI_COMMIT_MASK_BITS);
  if (bitcount == MI_COMMIT_MASK_BITS) {
    mi_assert_internal(bitidx==0);
    mi_commit_mask_create_full(cm);
  }
  else if (bitcount == 0) {
    mi_commit_mask_create_empty(cm);
  }
  else {
    mi_commit_mask_create_empty(cm);
    size_t i = bitidx / MI_COMMIT_MASK_FIELD_BITS;
    size_t ofs = bitidx % MI_COMMIT_MASK_FIELD_BITS;
    while (bitcount > 0) {
      mi_assert_internal(i < MI_COMMIT_MASK_FIELD_COUNT);
      size_t avail = MI_COMMIT_MASK_FIELD_BITS - ofs;
      size_t count = (bitcount > avail ? avail : bitcount);
      size_t mask = (count >= MI_COMMIT_MASK_FIELD_BITS ? ~((size_t)0) : (((size_t)1 << count) - 1) << ofs);
      cm->mask[i] = mask;
      bitcount -= count;
      ofs = 0;
      i++;
    }
  }
}

size_t _mi_commit_mask_committed_size(const mi_commit_mask_t* cm, size_t total) {
  mi_assert_internal((total%MI_COMMIT_MASK_BITS)==0);
  size_t count = 0;
  for (size_t i = 0; i < MI_COMMIT_MASK_FIELD_COUNT; i++) {
    size_t mask = cm->mask[i];
    if (~mask == 0) {
      count += MI_COMMIT_MASK_FIELD_BITS;
    }
    else {
      for (; mask != 0; mask >>= 1) {  // todo: use popcount
        if ((mask&1)!=0) count++;
      }
    }
  }
  // we use total since for huge segments each commit bit may represent a larger size
  return ((total / MI_COMMIT_MASK_BITS) * count);
}


size_t _mi_commit_mask_next_run(const mi_commit_mask_t* cm, size_t* idx) {
  size_t i = (*idx) / MI_COMMIT_MASK_FIELD_BITS;
  size_t ofs = (*idx) % MI_COMMIT_MASK_FIELD_BITS;
  size_t mask = 0;
  // find first ones
  while (i < MI_COMMIT_MASK_FIELD_COUNT) {
    mask = cm->mask[i];
    mask >>= ofs;
    if (mask != 0) {
      while ((mask&1) == 0) {
        mask >>= 1;
        ofs++;
      }
      break;
    }
    i++;
    ofs = 0;
  }
  if (i >= MI_COMMIT_MASK_FIELD_COUNT) {
    // not found
    *idx = MI_COMMIT_MASK_BITS;
    return 0;
  }
  else {
    // found, count ones
    size_t count = 0;
    *idx = (i*MI_COMMIT_MASK_FIELD_BITS) + ofs;
    do {
      mi_assert_internal(ofs < MI_COMMIT_MASK_FIELD_BITS && (mask&1) == 1);
      do {
        count++;
        mask >>= 1;
      } while ((mask&1) == 1);
      if ((((*idx + count) % MI_COMMIT_MASK_FIELD_BITS) == 0)) {
        i++;
        if (i >= MI_COMMIT_MASK_FIELD_COUNT) break;
        mask = cm->mask[i];
        ofs = 0;
      }
    } while ((mask&1) == 1);
    mi_assert_internal(count > 0);
    return count;
  }
}


/* --------------------------------------------------------------------------------
  Segment allocation
-------------------------------------------------------------------------------- */


/* -----------------------------------------------------------
   Slices
----------------------------------------------------------- */


static const mi_slice_t* mi_segment_slices_end(const mi_segment_t* segment) {
  return &segment->slices[segment->slice_entries];
}

static uint8_t* mi_slice_start(const mi_slice_t* slice) {
  mi_segment_t* segment = _mi_ptr_segment(slice);
  mi_assert_internal(slice >= segment->slices && slice < mi_segment_slices_end(segment));
  return ((uint8_t*)segment + ((slice - segment->slices)*MI_SEGMENT_SLICE_SIZE));
}


/* -----------------------------------------------------------
   Bins
----------------------------------------------------------- */
// Use bit scan forward to quickly find the first zero bit if it is available

static inline size_t mi_slice_bin8(size_t slice_count) {
  if (slice_count<=1) return slice_count;
  mi_assert_internal(slice_count <= MI_SLICES_PER_SEGMENT);
  slice_count--;
  size_t s = mi_bsr(slice_count);  // slice_count > 1
  if (s <= 2) return slice_count + 1;
  size_t bin = ((s << 2) | ((slice_count >> (s - 2))&0x03)) - 4;
  return bin;
}

static inline size_t mi_slice_bin(size_t slice_count) {
  mi_assert_internal(slice_count*MI_SEGMENT_SLICE_SIZE <= MI_SEGMENT_SIZE);
  mi_assert_internal(mi_slice_bin8(MI_SLICES_PER_SEGMENT) <= MI_SEGMENT_BIN_MAX);
  size_t bin = mi_slice_bin8(slice_count);
  mi_assert_internal(bin <= MI_SEGMENT_BIN_MAX);
  return bin;
}

static inline size_t mi_slice_index(const mi_slice_t* slice) {
  mi_segment_t* segment = _mi_ptr_segment(slice);
  ptrdiff_t index = slice - segment->slices;
  mi_assert_internal(index >= 0 && index < (ptrdiff_t)segment->slice_entries);
  return index;
}


/* -----------------------------------------------------------
   Slice span queues
----------------------------------------------------------- */

static void mi_span_queue_push(mi_span_queue_t* sq, mi_slice_t* slice) {
  // todo: or push to the end?
  mi_assert_internal(slice->prev == NULL && slice->next==NULL);
  slice->prev = NULL; // paranoia
  slice->next = sq->first;
  sq->first = slice;
  if (slice->next != NULL) slice->next->prev = slice;
                     else sq->last = slice;
  slice->block_size = 0; // free
}

static mi_span_queue_t* mi_span_queue_for(size_t slice_count, mi_segments_tld_t* tld) {
  size_t bin = mi_slice_bin(slice_count);
  mi_span_queue_t* sq = &tld->spans[bin];
  mi_assert_internal(sq->slice_count >= slice_count);
  return sq;
}

static void mi_span_queue_delete(mi_span_queue_t* sq, mi_slice_t* slice) {
  mi_assert_internal(slice->block_size==0 && slice->slice_count>0 && slice->slice_offset==0);
  // should work too if the queue does not contain slice (which can happen during reclaim)
  if (slice->prev != NULL) slice->prev->next = slice->next;
  if (slice == sq->first) sq->first = slice->next;
  if (slice->next != NULL) slice->next->prev = slice->prev;
  if (slice == sq->last) sq->last = slice->prev;
  slice->prev = NULL;
  slice->next = NULL;
  slice->block_size = 1; // no more free
}


/* -----------------------------------------------------------
 Invariant checking
----------------------------------------------------------- */

static bool mi_slice_is_used(const mi_slice_t* slice) {
  return (slice->block_size > 0);
}


#if (MI_DEBUG>=3)
static bool mi_span_queue_contains(mi_span_queue_t* sq, mi_slice_t* slice) {
  for (mi_slice_t* s = sq->first; s != NULL; s = s->next) {
    if (s==slice) return true;
  }
  return false;
}

static bool mi_segment_is_valid(mi_segment_t* segment, mi_segments_tld_t* tld) {
  mi_assert_internal(segment != NULL);
  mi_assert_internal(_mi_ptr_cookie(segment) == segment->cookie);
  mi_assert_internal(segment->abandoned <= segment->used);
  mi_assert_internal(segment->thread_id == 0 || segment->thread_id == _mi_thread_id());
  mi_assert_internal(mi_commit_mask_all_set(&segment->commit_mask, &segment->purge_mask)); // can only decommit committed blocks
  //mi_assert_internal(segment->segment_info_size % MI_SEGMENT_SLICE_SIZE == 0);
  mi_slice_t* slice = &segment->slices[0];
  const mi_slice_t* end = mi_segment_slices_end(segment);
  size_t used_count = 0;
  mi_span_queue_t* sq;
  while(slice < end) {
    mi_assert_internal(slice->slice_count > 0);
    mi_assert_internal(slice->slice_offset == 0);
    size_t index = mi_slice_index(slice);
    size_t maxindex = (index + slice->slice_count >= segment->slice_entries ? segment->slice_entries : index + slice->slice_count) - 1;
    if (mi_slice_is_used(slice)) { // a page in use, we need at least MAX_SLICE_OFFSET_COUNT valid back offsets
      used_count++;
      mi_assert_internal(slice->is_huge == (segment->kind == MI_SEGMENT_HUGE));
      for (size_t i = 0; i <= MI_MAX_SLICE_OFFSET_COUNT && index + i <= maxindex; i++) {
        mi_assert_internal(segment->slices[index + i].slice_offset == i*sizeof(mi_slice_t));
        mi_assert_internal(i==0 || segment->slices[index + i].slice_count == 0);
        mi_assert_internal(i==0 || segment->slices[index + i].block_size == 1);
      }
      // and the last entry as well (for coalescing)
      const mi_slice_t* last = slice + slice->slice_count - 1;
      if (last > slice && last < mi_segment_slices_end(segment)) {
        mi_assert_internal(last->slice_offset == (slice->slice_count-1)*sizeof(mi_slice_t));
        mi_assert_internal(last->slice_count == 0);
        mi_assert_internal(last->block_size == 1);
      }
    }
    else {  // free range of slices; only last slice needs a valid back offset
      mi_slice_t* last = &segment->slices[maxindex];
      if (segment->kind != MI_SEGMENT_HUGE || slice->slice_count <= (segment->slice_entries - segment->segment_info_slices)) {
        mi_assert_internal((uint8_t*)slice == (uint8_t*)last - last->slice_offset);
      }
      mi_assert_internal(slice == last || last->slice_count == 0 );
      mi_assert_internal(last->block_size == 0 || (segment->kind==MI_SEGMENT_HUGE && last->block_size==1));
      if (segment->kind != MI_SEGMENT_HUGE && segment->thread_id != 0) { // segment is not huge or abandoned
        sq = mi_span_queue_for(slice->slice_count,tld);
        mi_assert_internal(mi_span_queue_contains(sq,slice));
      }
    }
    slice = &segment->slices[maxindex+1];
  }
  mi_assert_internal(slice == end);
  mi_assert_internal(used_count == segment->used + 1);
  return true;
}
#endif

/* -----------------------------------------------------------
 Segment size calculations
----------------------------------------------------------- */

static size_t mi_segment_info_size(mi_segment_t* segment) {
  return segment->segment_info_slices * MI_SEGMENT_SLICE_SIZE;
}

static uint8_t* _mi_segment_page_start_from_slice(const mi_segment_t* segment, const mi_slice_t* slice, size_t block_size, size_t* page_size)
{
  const ptrdiff_t idx = slice - segment->slices;
  const size_t psize = (size_t)slice->slice_count * MI_SEGMENT_SLICE_SIZE;
  uint8_t* const pstart = (uint8_t*)segment + (idx*MI_SEGMENT_SLICE_SIZE);
  // make the start not OS page aligned for smaller blocks to avoid page/cache effects
  // note: the offset must always be a block_size multiple since we assume small allocations
  // are aligned (see `mi_heap_malloc_aligned`).
  size_t start_offset = 0;
  if (block_size > 0 && block_size <= MI_MAX_ALIGN_GUARANTEE) {
    // for small objects, ensure the page start is aligned with the block size (PR#66 by kickunderscore)
    const size_t adjust = block_size - ((uintptr_t)pstart % block_size);
    if (adjust < block_size && psize >= block_size + adjust) {
      start_offset += adjust;
    }
  }
  if (block_size >= MI_INTPTR_SIZE) {
    if (block_size <= 64) { start_offset += 3*block_size; }
    else if (block_size <= 512) { start_offset += block_size; }
  }
  if (page_size != NULL) { *page_size = psize - start_offset; }
  return (pstart + start_offset);
}

// Start of the page available memory; can be used on uninitialized pages
uint8_t* _mi_segment_page_start(const mi_segment_t* segment, const mi_page_t* page, size_t* page_size)
{
  const mi_slice_t* slice = mi_page_to_slice((mi_page_t*)page);
  uint8_t* p = _mi_segment_page_start_from_slice(segment, slice, mi_page_block_size(page), page_size);
  mi_assert_internal(mi_page_block_size(page) > 0 || _mi_ptr_page(p) == page);
  mi_assert_internal(_mi_ptr_segment(p) == segment);
  return p;
}


static size_t mi_segment_calculate_slices(size_t required, size_t* info_slices) {
  size_t page_size = _mi_os_page_size();
  size_t isize     = _mi_align_up(sizeof(mi_segment_t), page_size);
  size_t guardsize = 0;

  if (MI_SECURE>0) {
    // in secure mode, we set up a protected page in between the segment info
    // and the page data (and one at the end of the segment)
    guardsize = page_size;
    if (required > 0) {
      required = _mi_align_up(required, MI_SEGMENT_SLICE_SIZE) + page_size;
    }
  }

  isize = _mi_align_up(isize + guardsize, MI_SEGMENT_SLICE_SIZE);
  if (info_slices != NULL) *info_slices = isize / MI_SEGMENT_SLICE_SIZE;
  size_t segment_size = (required==0 ? MI_SEGMENT_SIZE : _mi_align_up( required + isize + guardsize, MI_SEGMENT_SLICE_SIZE) );
  mi_assert_internal(segment_size % MI_SEGMENT_SLICE_SIZE == 0);
  return (segment_size / MI_SEGMENT_SLICE_SIZE);
}


/* ----------------------------------------------------------------------------
Segment caches
We keep a small segment cache per thread to increase local
reuse and avoid setting/clearing guard pages in secure mode.
------------------------------------------------------------------------------- */

static void mi_segments_track_size(long segment_size, mi_segments_tld_t* tld) {
  if (segment_size>=0) _mi_stat_increase(&tld->stats->segments,1);
                  else _mi_stat_decrease(&tld->stats->segments,1);
  tld->count += (segment_size >= 0 ? 1 : -1);
  if (tld->count > tld->peak_count) tld->peak_count = tld->count;
  tld->current_size += segment_size;
  if (tld->current_size > tld->peak_size) tld->peak_size = tld->current_size;
}

static void mi_segment_os_free(mi_segment_t* segment, mi_segments_tld_t* tld) {
  segment->thread_id = 0;
  _mi_segment_map_freed_at(segment);
  mi_segments_track_size(-((long)mi_segment_size(segment)),tld);
  if (segment->was_reclaimed) {
    tld->reclaim_count--;
    segment->was_reclaimed = false;
  }
  if (MI_SECURE>0) {
    // _mi_os_unprotect(segment, mi_segment_size(segment)); // ensure no more guard pages are set
    // unprotect the guard pages; we cannot just unprotect the whole segment size as part may be decommitted
    size_t os_pagesize = _mi_os_page_size();
    _mi_os_unprotect((uint8_t*)segment + mi_segment_info_size(segment) - os_pagesize, os_pagesize);
    uint8_t* end = (uint8_t*)segment + mi_segment_size(segment) - os_pagesize;
    _mi_os_unprotect(end, os_pagesize);
  }

  // purge delayed decommits now? (no, leave it to the arena)
  // mi_segment_try_purge(segment,true,tld->stats);

  const size_t size = mi_segment_size(segment);
  const size_t csize = _mi_commit_mask_committed_size(&segment->commit_mask, size);

  _mi_abandoned_await_readers();  // wait until safe to free
  _mi_arena_free(segment, mi_segment_size(segment), csize, segment->memid, tld->stats);
}

/* -----------------------------------------------------------
   Commit/Decommit ranges
----------------------------------------------------------- */

static void mi_segment_commit_mask(mi_segment_t* segment, bool conservative, uint8_t* p, size_t size, uint8_t** start_p, size_t* full_size, mi_commit_mask_t* cm) {
  mi_assert_internal(_mi_ptr_segment(p + 1) == segment);
  mi_assert_internal(segment->kind != MI_SEGMENT_HUGE);
  mi_commit_mask_create_empty(cm);
  if (size == 0 || size > MI_SEGMENT_SIZE || segment->kind == MI_SEGMENT_HUGE) return;
  const size_t segstart = mi_segment_info_size(segment);
  const size_t segsize = mi_segment_size(segment);
  if (p >= (uint8_t*)segment + segsize) return;

  size_t pstart = (p - (uint8_t*)segment);
  mi_assert_internal(pstart + size <= segsize);

  size_t start;
  size_t end;
  if (conservative) {
    // decommit conservative
    start = _mi_align_up(pstart, MI_COMMIT_SIZE);
    end   = _mi_align_down(pstart + size, MI_COMMIT_SIZE);
    mi_assert_internal(start >= segstart);
    mi_assert_internal(end <= segsize);
  }
  else {
    // commit liberal
    start = _mi_align_down(pstart, MI_MINIMAL_COMMIT_SIZE);
    end   = _mi_align_up(pstart + size, MI_MINIMAL_COMMIT_SIZE);
  }
  if (pstart >= segstart && start < segstart) {  // note: the mask is also calculated for an initial commit of the info area
    start = segstart;
  }
  if (end > segsize) {
    end = segsize;
  }

  mi_assert_internal(start <= pstart && (pstart + size) <= end);
  mi_assert_internal(start % MI_COMMIT_SIZE==0 && end % MI_COMMIT_SIZE == 0);
  *start_p   = (uint8_t*)segment + start;
  *full_size = (end > start ? end - start : 0);
  if (*full_size == 0) return;

  size_t bitidx = start / MI_COMMIT_SIZE;
  mi_assert_internal(bitidx < MI_COMMIT_MASK_BITS);

  size_t bitcount = *full_size / MI_COMMIT_SIZE; // can be 0
  if (bitidx + bitcount > MI_COMMIT_MASK_BITS) {
    _mi_warning_message("commit mask overflow: idx=%zu count=%zu start=%zx end=%zx p=0x%p size=%zu fullsize=%zu\n", bitidx, bitcount, start, end, p, size, *full_size);
  }
  mi_assert_internal((bitidx + bitcount) <= MI_COMMIT_MASK_BITS);
  mi_commit_mask_create(bitidx, bitcount, cm);
}

static bool mi_segment_commit(mi_segment_t* segment, uint8_t* p, size_t size, mi_stats_t* stats) {
  mi_assert_internal(mi_commit_mask_all_set(&segment->commit_mask, &segment->purge_mask));

  // commit liberal
  uint8_t* start = NULL;
  size_t   full_size = 0;
  mi_commit_mask_t mask;
  mi_segment_commit_mask(segment, false /* conservative? */, p, size, &start, &full_size, &mask);
  if (mi_commit_mask_is_empty(&mask) || full_size == 0) return true;

  if (!mi_commit_mask_all_set(&segment->commit_mask, &mask)) {
    // committing
    bool is_zero = false;
    mi_commit_mask_t cmask;
    mi_commit_mask_create_intersect(&segment->commit_mask, &mask, &cmask);
    _mi_stat_decrease(&_mi_stats_main.committed, _mi_commit_mask_committed_size(&cmask, MI_SEGMENT_SIZE)); // adjust for overlap
    if (!_mi_os_commit(start, full_size, &is_zero, stats)) return false;
    mi_commit_mask_set(&segment->commit_mask, &mask);
  }

  // increase purge expiration when using part of delayed purges -- we assume more allocations are coming soon.
  if (mi_commit_mask_any_set(&segment->purge_mask, &mask)) {
    segment->purge_expire = _mi_clock_now() + mi_option_get(mi_option_purge_delay);
  }

  // always clear any delayed purges in our range (as they are either committed now)
  mi_commit_mask_clear(&segment->purge_mask, &mask);
  return true;
}

static bool mi_segment_ensure_committed(mi_segment_t* segment, uint8_t* p, size_t size, mi_stats_t* stats) {
  mi_assert_internal(mi_commit_mask_all_set(&segment->commit_mask, &segment->purge_mask));
  // note: assumes commit_mask is always full for huge segments as otherwise the commit mask bits can overflow
  if (mi_commit_mask_is_full(&segment->commit_mask) && mi_commit_mask_is_empty(&segment->purge_mask)) return true; // fully committed
  mi_assert_internal(segment->kind != MI_SEGMENT_HUGE);
  return mi_segment_commit(segment, p, size, stats);
}

static bool mi_segment_purge(mi_segment_t* segment, uint8_t* p, size_t size, mi_stats_t* stats) {
  mi_assert_internal(mi_commit_mask_all_set(&segment->commit_mask, &segment->purge_mask));
  if (!segment->allow_purge) return true;

  // purge conservative
  uint8_t* start = NULL;
  size_t   full_size = 0;
  mi_commit_mask_t mask;
  mi_segment_commit_mask(segment, true /* conservative? */, p, size, &start, &full_size, &mask);
  if (mi_commit_mask_is_empty(&mask) || full_size==0) return true;

  if (mi_commit_mask_any_set(&segment->commit_mask, &mask)) {
    // purging
    mi_assert_internal((void*)start != (void*)segment);
    mi_assert_internal(segment->allow_decommit);
    const bool decommitted = _mi_os_purge(start, full_size, stats);  // reset or decommit
    if (decommitted) {
      mi_commit_mask_t cmask;
      mi_commit_mask_create_intersect(&segment->commit_mask, &mask, &cmask);
      _mi_stat_increase(&_mi_stats_main.committed, full_size - _mi_commit_mask_committed_size(&cmask, MI_SEGMENT_SIZE)); // adjust for double counting
      mi_commit_mask_clear(&segment->commit_mask, &mask);
    }
  }

  // always clear any scheduled purges in our range
  mi_commit_mask_clear(&segment->purge_mask, &mask);
  return true;
}

static void mi_segment_schedule_purge(mi_segment_t* segment, uint8_t* p, size_t size, mi_stats_t* stats) {
  if (!segment->allow_purge) return;

  if (mi_option_get(mi_option_purge_delay) == 0) {
    mi_segment_purge(segment, p, size, stats);
  }
  else {
    // register for future purge in the purge mask
    uint8_t* start = NULL;
    size_t   full_size = 0;
    mi_commit_mask_t mask;
    mi_segment_commit_mask(segment, true /*conservative*/, p, size, &start, &full_size, &mask);
    if (mi_commit_mask_is_empty(&mask) || full_size==0) return;

    // update delayed commit
    mi_assert_internal(segment->purge_expire > 0 || mi_commit_mask_is_empty(&segment->purge_mask));
    mi_commit_mask_t cmask;
    mi_commit_mask_create_intersect(&segment->commit_mask, &mask, &cmask);  // only purge what is committed; span_free may try to decommit more
    mi_commit_mask_set(&segment->purge_mask, &cmask);
    mi_msecs_t now = _mi_clock_now();
    if (segment->purge_expire == 0) {
      // no previous purgess, initialize now
      segment->purge_expire = now + mi_option_get(mi_option_purge_delay);
    }
    else if (segment->purge_expire <= now) {
      // previous purge mask already expired
      if (segment->purge_expire + mi_option_get(mi_option_purge_extend_delay) <= now) {
        mi_segment_try_purge(segment, true, stats);
      }
      else {
        segment->purge_expire = now + mi_option_get(mi_option_purge_extend_delay); // (mi_option_get(mi_option_purge_delay) / 8); // wait a tiny bit longer in case there is a series of free's
      }
    }
    else {
      // previous purge mask is not yet expired, increase the expiration by a bit.
      segment->purge_expire += mi_option_get(mi_option_purge_extend_delay);
    }
  }
}

static void mi_segment_try_purge(mi_segment_t* segment, bool force, mi_stats_t* stats) {
  if (!segment->allow_purge || segment->purge_expire == 0 || mi_commit_mask_is_empty(&segment->purge_mask)) return;
  mi_msecs_t now = _mi_clock_now();
  if (!force && now < segment->purge_expire) return;

  mi_commit_mask_t mask = segment->purge_mask;
  segment->purge_expire = 0;
  mi_commit_mask_create_empty(&segment->purge_mask);

  size_t idx;
  size_t count;
  mi_commit_mask_foreach(&mask, idx, count) {
    // if found, decommit that sequence
    if (count > 0) {
      uint8_t* p = (uint8_t*)segment + (idx*MI_COMMIT_SIZE);
      size_t size = count * MI_COMMIT_SIZE;
      mi_segment_purge(segment, p, size, stats);
    }
  }
  mi_commit_mask_foreach_end()
  mi_assert_internal(mi_commit_mask_is_empty(&segment->purge_mask));
}

// called from `mi_heap_collect_ex`
// this can be called per-page so it is important that try_purge has fast exit path
void _mi_segment_collect(mi_segment_t* segment, bool force, mi_segments_tld_t* tld) {
  mi_segment_try_purge(segment, force, tld->stats);
}

/* -----------------------------------------------------------
   Span free
----------------------------------------------------------- */

static bool mi_segment_is_abandoned(mi_segment_t* segment) {
  return (mi_atomic_load_relaxed(&segment->thread_id) == 0);
}

// note: can be called on abandoned segments
static void mi_segment_span_free(mi_segment_t* segment, size_t slice_index, size_t slice_count, bool allow_purge, mi_segments_tld_t* tld) {
  mi_assert_internal(slice_index < segment->slice_entries);
  mi_span_queue_t* sq = (segment->kind == MI_SEGMENT_HUGE || mi_segment_is_abandoned(segment)
                          ? NULL : mi_span_queue_for(slice_count,tld));
  if (slice_count==0) slice_count = 1;
  mi_assert_internal(slice_index + slice_count - 1 < segment->slice_entries);

  // set first and last slice (the intermediates can be undetermined)
  mi_slice_t* slice = &segment->slices[slice_index];
  slice->slice_count = (uint32_t)slice_count;
  mi_assert_internal(slice->slice_count == slice_count); // no overflow?
  slice->slice_offset = 0;
  if (slice_count > 1) {
    mi_slice_t* last = slice + slice_count - 1;
    mi_slice_t* end  = (mi_slice_t*)mi_segment_slices_end(segment);
    if (last > end) { last = end; }
    last->slice_count = 0;
    last->slice_offset = (uint32_t)(sizeof(mi_page_t)*(slice_count - 1));
    last->block_size = 0;
  }

  // perhaps decommit
  if (allow_purge) {
    mi_segment_schedule_purge(segment, mi_slice_start(slice), slice_count * MI_SEGMENT_SLICE_SIZE, tld->stats);
  }

  // and push it on the free page queue (if it was not a huge page)
  if (sq != NULL) mi_span_queue_push( sq, slice );
             else slice->block_size = 0; // mark huge page as free anyways
}

/*
// called from reclaim to add existing free spans
static void mi_segment_span_add_free(mi_slice_t* slice, mi_segments_tld_t* tld) {
  mi_segment_t* segment = _mi_ptr_segment(slice);
  mi_assert_internal(slice->xblock_size==0 && slice->slice_count>0 && slice->slice_offset==0);
  size_t slice_index = mi_slice_index(slice);
  mi_segment_span_free(segment,slice_index,slice->slice_count,tld);
}
*/

static void mi_segment_span_remove_from_queue(mi_slice_t* slice, mi_segments_tld_t* tld) {
  mi_assert_internal(slice->slice_count > 0 && slice->slice_offset==0 && slice->block_size==0);
  mi_assert_internal(_mi_ptr_segment(slice)->kind != MI_SEGMENT_HUGE);
  mi_span_queue_t* sq = mi_span_queue_for(slice->slice_count, tld);
  mi_span_queue_delete(sq, slice);
}

// note: can be called on abandoned segments
static mi_slice_t* mi_segment_span_free_coalesce(mi_slice_t* slice, mi_segments_tld_t* tld) {
  mi_assert_internal(slice != NULL && slice->slice_count > 0 && slice->slice_offset == 0);
  mi_segment_t* const segment = _mi_ptr_segment(slice);
  const bool is_abandoned = (segment->thread_id == 0); // mi_segment_is_abandoned(segment);

  // for huge pages, just mark as free but don't add to the queues
  if (segment->kind == MI_SEGMENT_HUGE) {
    // issue #691: segment->used can be 0 if the huge page block was freed while abandoned (reclaim will get here in that case)
    mi_assert_internal((segment->used==0 && slice->block_size==0) || segment->used == 1);  // decreased right after this call in `mi_segment_page_clear`
    slice->block_size = 0;  // mark as free anyways
    // we should mark the last slice `xblock_size=0` now to maintain invariants but we skip it to
    // avoid a possible cache miss (and the segment is about to be freed)
    return slice;
  }

  // otherwise coalesce the span and add to the free span queues
  size_t slice_count = slice->slice_count;
  mi_slice_t* next = slice + slice->slice_count;
  mi_assert_internal(next <= mi_segment_slices_end(segment));
  if (next < mi_segment_slices_end(segment) && next->block_size==0) {
    // free next block -- remove it from free and merge
    mi_assert_internal(next->slice_count > 0 && next->slice_offset==0);
    slice_count += next->slice_count; // extend
    if (!is_abandoned) { mi_segment_span_remove_from_queue(next, tld); }
  }
  if (slice > segment->slices) {
    mi_slice_t* prev = mi_slice_first(slice - 1);
    mi_assert_internal(prev >= segment->slices);
    if (prev->block_size==0) {
      // free previous slice -- remove it from free and merge
      mi_assert_internal(prev->slice_count > 0 && prev->slice_offset==0);
      slice_count += prev->slice_count;
      if (!is_abandoned) { mi_segment_span_remove_from_queue(prev, tld); }
      slice = prev;
    }
  }

  // and add the new free page
  mi_segment_span_free(segment, mi_slice_index(slice), slice_count, true, tld);
  return slice;
}



/* -----------------------------------------------------------
   Page allocation
----------------------------------------------------------- */

// Note: may still return NULL if committing the memory failed
static mi_page_t* mi_segment_span_allocate(mi_segment_t* segment, size_t slice_index, size_t slice_count, mi_segments_tld_t* tld) {
  mi_assert_internal(slice_index < segment->slice_entries);
  mi_slice_t* const slice = &segment->slices[slice_index];
  mi_assert_internal(slice->block_size==0 || slice->block_size==1);

  // commit before changing the slice data
  if (!mi_segment_ensure_committed(segment, _mi_segment_page_start_from_slice(segment, slice, 0, NULL), slice_count * MI_SEGMENT_SLICE_SIZE, tld->stats)) {
    return NULL;  // commit failed!
  }

  // convert the slices to a page
  slice->slice_offset = 0;
  slice->slice_count = (uint32_t)slice_count;
  mi_assert_internal(slice->slice_count == slice_count);
  const size_t bsize = slice_count * MI_SEGMENT_SLICE_SIZE;
  slice->block_size = bsize;
  mi_page_t*  page = mi_slice_to_page(slice);
  mi_assert_internal(mi_page_block_size(page) == bsize);

  // set slice back pointers for the first MI_MAX_SLICE_OFFSET_COUNT entries
  size_t extra = slice_count-1;
  if (extra > MI_MAX_SLICE_OFFSET_COUNT) extra = MI_MAX_SLICE_OFFSET_COUNT;
  if (slice_index + extra >= segment->slice_entries) extra = segment->slice_entries - slice_index - 1;  // huge objects may have more slices than avaiable entries in the segment->slices

  mi_slice_t* slice_next = slice + 1;
  for (size_t i = 1; i <= extra; i++, slice_next++) {
    slice_next->slice_offset = (uint32_t)(sizeof(mi_slice_t)*i);
    slice_next->slice_count = 0;
    slice_next->block_size = 1;
  }

  // and also for the last one (if not set already) (the last one is needed for coalescing and for large alignments)
  // note: the cast is needed for ubsan since the index can be larger than MI_SLICES_PER_SEGMENT for huge allocations (see #543)
  mi_slice_t* last = slice + slice_count - 1;
  mi_slice_t* end = (mi_slice_t*)mi_segment_slices_end(segment);
  if (last > end) last = end;
  if (last > slice) {
    last->slice_offset = (uint32_t)(sizeof(mi_slice_t) * (last - slice));
    last->slice_count = 0;
    last->block_size = 1;
  }

  // and initialize the page
  page->is_committed = true;
  page->is_huge = (segment->kind == MI_SEGMENT_HUGE);
  segment->used++;
  return page;
}

static void mi_segment_slice_split(mi_segment_t* segment, mi_slice_t* slice, size_t slice_count, mi_segments_tld_t* tld) {
  mi_assert_internal(_mi_ptr_segment(slice) == segment);
  mi_assert_internal(slice->slice_count >= slice_count);
  mi_assert_internal(slice->block_size > 0); // no more in free queue
  if (slice->slice_count <= slice_count) return;
  mi_assert_internal(segment->kind != MI_SEGMENT_HUGE);
  size_t next_index = mi_slice_index(slice) + slice_count;
  size_t next_count = slice->slice_count - slice_count;
  mi_segment_span_free(segment, next_index, next_count, false /* don't purge left-over part */, tld);
  slice->slice_count = (uint32_t)slice_count;
}

static mi_page_t* mi_segments_page_find_and_allocate(size_t slice_count, mi_arena_id_t req_arena_id, mi_segments_tld_t* tld) {
  mi_assert_internal(slice_count*MI_SEGMENT_SLICE_SIZE <= MI_LARGE_OBJ_SIZE_MAX);
  // search from best fit up
  mi_span_queue_t* sq = mi_span_queue_for(slice_count, tld);
  if (slice_count == 0) slice_count = 1;
  while (sq <= &tld->spans[MI_SEGMENT_BIN_MAX]) {
    for (mi_slice_t* slice = sq->first; slice != NULL; slice = slice->next) {
      if (slice->slice_count >= slice_count) {
        // found one
        mi_segment_t* segment = _mi_ptr_segment(slice);
        if (_mi_arena_memid_is_suitable(segment->memid, req_arena_id)) {
          // found a suitable page span
          mi_span_queue_delete(sq, slice);

          if (slice->slice_count > slice_count) {
            mi_segment_slice_split(segment, slice, slice_count, tld);
          }
          mi_assert_internal(slice != NULL && slice->slice_count == slice_count && slice->block_size > 0);
          mi_page_t* page = mi_segment_span_allocate(segment, mi_slice_index(slice), slice->slice_count, tld);
          if (page == NULL) {
            // commit failed; return NULL but first restore the slice
            mi_segment_span_free_coalesce(slice, tld);
            return NULL;
          }
          return page;
        }
      }
    }
    sq++;
  }
  // could not find a page..
  return NULL;
}


/* -----------------------------------------------------------
   Segment allocation
----------------------------------------------------------- */

static mi_segment_t* mi_segment_os_alloc( size_t required, size_t page_alignment, bool eager_delayed, mi_arena_id_t req_arena_id,
                                          size_t* psegment_slices, size_t* pinfo_slices,
                                          bool commit, mi_segments_tld_t* tld, mi_os_tld_t* os_tld)

{
  mi_memid_t memid;
  bool   allow_large = (!eager_delayed && (MI_SECURE == 0)); // only allow large OS pages once we are no longer lazy
  size_t align_offset = 0;
  size_t alignment = MI_SEGMENT_ALIGN;

  if (page_alignment > 0) {
    // mi_assert_internal(huge_page != NULL);
    mi_assert_internal(page_alignment >= MI_SEGMENT_ALIGN);
    alignment = page_alignment;
    const size_t info_size = (*pinfo_slices) * MI_SEGMENT_SLICE_SIZE;
    align_offset = _mi_align_up( info_size, MI_SEGMENT_ALIGN );
    const size_t extra = align_offset - info_size;
    // recalculate due to potential guard pages
    *psegment_slices = mi_segment_calculate_slices(required + extra, pinfo_slices);
    mi_assert_internal(*psegment_slices > 0 && *psegment_slices <= UINT32_MAX);
  }

  const size_t segment_size = (*psegment_slices) * MI_SEGMENT_SLICE_SIZE;
  mi_segment_t* segment = (mi_segment_t*)_mi_arena_alloc_aligned(segment_size, alignment, align_offset, commit, allow_large, req_arena_id, &memid, os_tld);
  if (segment == NULL) {
    return NULL;  // failed to allocate
  }

  // ensure metadata part of the segment is committed
  mi_commit_mask_t commit_mask;
  if (memid.initially_committed) {
    mi_commit_mask_create_full(&commit_mask);
  }
  else {
    // at least commit the info slices
    const size_t commit_needed = _mi_divide_up((*pinfo_slices)*MI_SEGMENT_SLICE_SIZE, MI_COMMIT_SIZE);
    mi_assert_internal(commit_needed>0);
    mi_commit_mask_create(0, commit_needed, &commit_mask);
    mi_assert_internal(commit_needed*MI_COMMIT_SIZE >= (*pinfo_slices)*MI_SEGMENT_SLICE_SIZE);
    if (!_mi_os_commit(segment, commit_needed*MI_COMMIT_SIZE, NULL, tld->stats)) {
      _mi_arena_free(segment,segment_size,0,memid,tld->stats);
      return NULL;
    }
  }
  mi_assert_internal(segment != NULL && (uintptr_t)segment % MI_SEGMENT_SIZE == 0);

  segment->memid = memid;
  segment->allow_decommit = !memid.is_pinned;
  segment->allow_purge = segment->allow_decommit && (mi_option_get(mi_option_purge_delay) >= 0);
  segment->segment_size = segment_size;
  segment->commit_mask = commit_mask;
  segment->purge_expire = 0;
  mi_commit_mask_create_empty(&segment->purge_mask);

  mi_segments_track_size((long)(segment_size), tld);
  _mi_segment_map_allocated_at(segment);
  return segment;
}


// Allocate a segment from the OS aligned to `MI_SEGMENT_SIZE` .
static mi_segment_t* mi_segment_alloc(size_t required, size_t page_alignment, mi_arena_id_t req_arena_id, mi_segments_tld_t* tld, mi_os_tld_t* os_tld, mi_page_t** huge_page)
{
  mi_assert_internal((required==0 && huge_page==NULL) || (required>0 && huge_page != NULL));

  // calculate needed sizes first
  size_t info_slices;
  size_t segment_slices = mi_segment_calculate_slices(required, &info_slices);
  mi_assert_internal(segment_slices > 0 && segment_slices <= UINT32_MAX);

  // Commit eagerly only if not the first N lazy segments (to reduce impact of many threads that allocate just a little)
  const bool eager_delay = (// !_mi_os_has_overcommit() &&             // never delay on overcommit systems
                            _mi_current_thread_count() > 1 &&       // do not delay for the first N threads
                            tld->count < (size_t)mi_option_get(mi_option_eager_commit_delay));
  const bool eager = !eager_delay && mi_option_is_enabled(mi_option_eager_commit);
  bool commit = eager || (required > 0);

  // Allocate the segment from the OS
  mi_segment_t* segment = mi_segment_os_alloc(required, page_alignment, eager_delay, req_arena_id,
                                              &segment_slices, &info_slices, commit, tld, os_tld);
  if (segment == NULL) return NULL;

  // zero the segment info? -- not always needed as it may be zero initialized from the OS
  if (!segment->memid.initially_zero) {
    ptrdiff_t ofs    = offsetof(mi_segment_t, next);
    size_t    prefix = offsetof(mi_segment_t, slices) - ofs;
    size_t    zsize  = prefix + (sizeof(mi_slice_t) * (segment_slices + 1)); // one more
    _mi_memzero((uint8_t*)segment + ofs, zsize);
  }

  // initialize the rest of the segment info
  const size_t slice_entries = (segment_slices > MI_SLICES_PER_SEGMENT ? MI_SLICES_PER_SEGMENT : segment_slices);
  segment->segment_slices = segment_slices;
  segment->segment_info_slices = info_slices;
  segment->thread_id = _mi_thread_id();
  segment->cookie = _mi_ptr_cookie(segment);
  segment->slice_entries = slice_entries;
  segment->kind = (required == 0 ? MI_SEGMENT_NORMAL : MI_SEGMENT_HUGE);

  // _mi_memzero(segment->slices, sizeof(mi_slice_t)*(info_slices+1));
  _mi_stat_increase(&tld->stats->page_committed, mi_segment_info_size(segment));

  // set up guard pages
  size_t guard_slices = 0;
  if (MI_SECURE>0) {
    // in secure mode, we set up a protected page in between the segment info
    // and the page data, and at the end of the segment.
    size_t os_pagesize = _mi_os_page_size();    
    _mi_os_protect((uint8_t*)segment + mi_segment_info_size(segment) - os_pagesize, os_pagesize);
    uint8_t* end = (uint8_t*)segment + mi_segment_size(segment) - os_pagesize;
    mi_segment_ensure_committed(segment, end, os_pagesize, tld->stats);
    _mi_os_protect(end, os_pagesize);
    if (slice_entries == segment_slices) segment->slice_entries--; // don't use the last slice :-(
    guard_slices = 1;
  }

  // reserve first slices for segment info
  mi_page_t* page0 = mi_segment_span_allocate(segment, 0, info_slices, tld);
  mi_assert_internal(page0!=NULL); if (page0==NULL) return NULL; // cannot fail as we always commit in advance
  mi_assert_internal(segment->used == 1);
  segment->used = 0; // don't count our internal slices towards usage

  // initialize initial free pages
  if (segment->kind == MI_SEGMENT_NORMAL) { // not a huge page
    mi_assert_internal(huge_page==NULL);
    mi_segment_span_free(segment, info_slices, segment->slice_entries - info_slices, false /* don't purge */, tld);
  }
  else {
    mi_assert_internal(huge_page!=NULL);
    mi_assert_internal(mi_commit_mask_is_empty(&segment->purge_mask));
    mi_assert_internal(mi_commit_mask_is_full(&segment->commit_mask));
    *huge_page = mi_segment_span_allocate(segment, info_slices, segment_slices - info_slices - guard_slices, tld);
    mi_assert_internal(*huge_page != NULL); // cannot fail as we commit in advance
  }

  mi_assert_expensive(mi_segment_is_valid(segment,tld));
  return segment;
}


static void mi_segment_free(mi_segment_t* segment, bool force, mi_segments_tld_t* tld) {
  MI_UNUSED(force);
  mi_assert_internal(segment != NULL);
  mi_assert_internal(segment->next == NULL);
  mi_assert_internal(segment->used == 0);

  // Remove the free pages
  mi_slice_t* slice = &segment->slices[0];
  const mi_slice_t* end = mi_segment_slices_end(segment);
  #if MI_DEBUG>1
  size_t page_count = 0;
  #endif
  while (slice < end) {
    mi_assert_internal(slice->slice_count > 0);
    mi_assert_internal(slice->slice_offset == 0);
    mi_assert_internal(mi_slice_index(slice)==0 || slice->block_size == 0); // no more used pages ..
    if (slice->block_size == 0 && segment->kind != MI_SEGMENT_HUGE) {
      mi_segment_span_remove_from_queue(slice, tld);
    }
    #if MI_DEBUG>1
    page_count++;
    #endif
    slice = slice + slice->slice_count;
  }
  mi_assert_internal(page_count == 2); // first page is allocated by the segment itself

  // stats
  _mi_stat_decrease(&tld->stats->page_committed, mi_segment_info_size(segment));

  // return it to the OS
  mi_segment_os_free(segment, tld);
}


/* -----------------------------------------------------------
   Page Free
----------------------------------------------------------- */

static void mi_segment_abandon(mi_segment_t* segment, mi_segments_tld_t* tld);

// note: can be called on abandoned pages
static mi_slice_t* mi_segment_page_clear(mi_page_t* page, mi_segments_tld_t* tld) {
  mi_assert_internal(page->block_size > 0);
  mi_assert_internal(mi_page_all_free(page));
  mi_segment_t* segment = _mi_ptr_segment(page);
  mi_assert_internal(segment->used > 0);

  size_t inuse = page->capacity * mi_page_block_size(page);
  _mi_stat_decrease(&tld->stats->page_committed, inuse);
  _mi_stat_decrease(&tld->stats->pages, 1);

  // reset the page memory to reduce memory pressure?
  if (segment->allow_decommit && mi_option_is_enabled(mi_option_deprecated_page_reset)) {
    size_t psize;
    uint8_t* start = _mi_segment_page_start(segment, page, &psize);
    _mi_os_reset(start, psize, tld->stats);
  }

  // zero the page data, but not the segment fields and heap tag
  page->is_zero_init = false;
  uint8_t heap_tag = page->heap_tag;
  ptrdiff_t ofs = offsetof(mi_page_t, capacity);
  _mi_memzero((uint8_t*)page + ofs, sizeof(*page) - ofs);
  page->block_size = 1;
  page->heap_tag = heap_tag;

  // and free it
  mi_slice_t* slice = mi_segment_span_free_coalesce(mi_page_to_slice(page), tld);
  segment->used--;
  // cannot assert segment valid as it is called during reclaim
  // mi_assert_expensive(mi_segment_is_valid(segment, tld));
  return slice;
}

void _mi_segment_page_free(mi_page_t* page, bool force, mi_segments_tld_t* tld)
{
  mi_assert(page != NULL);

  mi_segment_t* segment = _mi_page_segment(page);
  mi_assert_expensive(mi_segment_is_valid(segment,tld));

  // mark it as free now
  mi_segment_page_clear(page, tld);
  mi_assert_expensive(mi_segment_is_valid(segment, tld));

  if (segment->used == 0) {
    // no more used pages; remove from the free list and free the segment
    mi_segment_free(segment, force, tld);
  }
  else if (segment->used == segment->abandoned) {
    // only abandoned pages; remove from free list and abandon
    mi_segment_abandon(segment,tld);
  }
  else {
    // perform delayed purges
    mi_segment_try_purge(segment, false /* force? */, tld->stats);
  }
}


/* -----------------------------------------------------------
Abandonment

When threads terminate, they can leave segments with
live blocks (reachable through other threads). Such segments
are "abandoned" and will be reclaimed by other threads to
reuse their pages and/or free them eventually. The
`thread_id` of such segments is 0.

When a block is freed in an abandoned segment, the segment
is reclaimed into that thread.

Moreover, if threads are looking for a fresh segment, they
will first consider abondoned segments -- these can be found
by scanning the arena memory
(segments outside arena memoryare only reclaimed by a free).
----------------------------------------------------------- */

// legacy: Wait until there are no more pending reads on segments that used to be in the abandoned list
void _mi_abandoned_await_readers(void) {
  // nothing needed
}

/* -----------------------------------------------------------
   Abandon segment/page
----------------------------------------------------------- */

static void mi_segment_abandon(mi_segment_t* segment, mi_segments_tld_t* tld) {
  mi_assert_internal(segment->used == segment->abandoned);
  mi_assert_internal(segment->used > 0);
  mi_assert_internal(segment->abandoned_visits == 0);
  mi_assert_expensive(mi_segment_is_valid(segment,tld));

  // remove the free pages from the free page queues
  mi_slice_t* slice = &segment->slices[0];
  const mi_slice_t* end = mi_segment_slices_end(segment);
  while (slice < end) {
    mi_assert_internal(slice->slice_count > 0);
    mi_assert_internal(slice->slice_offset == 0);
    if (slice->block_size == 0) { // a free page
      mi_segment_span_remove_from_queue(slice,tld);
      slice->block_size = 0; // but keep it free
    }
    slice = slice + slice->slice_count;
  }

  // perform delayed decommits (forcing is much slower on mstress)
  // Only abandoned segments in arena memory can be reclaimed without a free
  // so if a segment is not from an arena we force purge here to be conservative.
  const bool force_purge = (segment->memid.memkind != MI_MEM_ARENA) || mi_option_is_enabled(mi_option_abandoned_page_purge);
  mi_segment_try_purge(segment, force_purge, tld->stats);

  // all pages in the segment are abandoned; add it to the abandoned list
  _mi_stat_increase(&tld->stats->segments_abandoned, 1);
  mi_segments_track_size(-((long)mi_segment_size(segment)), tld);
  segment->thread_id = 0;
  segment->abandoned_visits = 1;   // from 0 to 1 to signify it is abandoned
  if (segment->was_reclaimed) {
    tld->reclaim_count--;
    segment->was_reclaimed = false;
  }
  _mi_arena_segment_mark_abandoned(segment);
}

void _mi_segment_page_abandon(mi_page_t* page, mi_segments_tld_t* tld) {
  mi_assert(page != NULL);
  mi_assert_internal(mi_page_thread_free_flag(page)==MI_NEVER_DELAYED_FREE);
  mi_assert_internal(mi_page_heap(page) == NULL);
  mi_segment_t* segment = _mi_page_segment(page);

  mi_assert_expensive(mi_segment_is_valid(segment,tld));
  segment->abandoned++;

  _mi_stat_increase(&tld->stats->pages_abandoned, 1);
  mi_assert_internal(segment->abandoned <= segment->used);
  if (segment->used == segment->abandoned) {
    // all pages are abandoned, abandon the entire segment
    mi_segment_abandon(segment, tld);
  }
}

/* -----------------------------------------------------------
  Reclaim abandoned pages
----------------------------------------------------------- */

static mi_slice_t* mi_slices_start_iterate(mi_segment_t* segment, const mi_slice_t** end) {
  mi_slice_t* slice = &segment->slices[0];
  *end = mi_segment_slices_end(segment);
  mi_assert_internal(slice->slice_count>0 && slice->block_size>0); // segment allocated page
  slice = slice + slice->slice_count; // skip the first segment allocated page
  return slice;
}

// Possibly free pages and check if free space is available
static bool mi_segment_check_free(mi_segment_t* segment, size_t slices_needed, size_t block_size, mi_segments_tld_t* tld)
{
  mi_assert_internal(mi_segment_is_abandoned(segment));
  bool has_page = false;

  // for all slices
  const mi_slice_t* end;
  mi_slice_t* slice = mi_slices_start_iterate(segment, &end);
  while (slice < end) {
    mi_assert_internal(slice->slice_count > 0);
    mi_assert_internal(slice->slice_offset == 0);
    if (mi_slice_is_used(slice)) { // used page
      // ensure used count is up to date and collect potential concurrent frees
      mi_page_t* const page = mi_slice_to_page(slice);
      _mi_page_free_collect(page, false);
      if (mi_page_all_free(page)) {
        // if this page is all free now, free it without adding to any queues (yet)
        mi_assert_internal(page->next == NULL && page->prev==NULL);
        _mi_stat_decrease(&tld->stats->pages_abandoned, 1);
        segment->abandoned--;
        slice = mi_segment_page_clear(page, tld); // re-assign slice due to coalesce!
        mi_assert_internal(!mi_slice_is_used(slice));
        if (slice->slice_count >= slices_needed) {
          has_page = true;
        }
      }
      else if (mi_page_block_size(page) == block_size && mi_page_has_any_available(page)) {
        // a page has available free blocks of the right size
        has_page = true;
      }
    }
    else {
      // empty span
      if (slice->slice_count >= slices_needed) {
        has_page = true;
      }
    }
    slice = slice + slice->slice_count;
  }
  return has_page;
}

// Reclaim an abandoned segment; returns NULL if the segment was freed
// set `right_page_reclaimed` to `true` if it reclaimed a page of the right `block_size` that was not full.
static mi_segment_t* mi_segment_reclaim(mi_segment_t* segment, mi_heap_t* heap, size_t requested_block_size, bool* right_page_reclaimed, mi_segments_tld_t* tld) {
  if (right_page_reclaimed != NULL) { *right_page_reclaimed = false; }
  // can be 0 still with abandoned_next, or already a thread id for segments outside an arena that are reclaimed on a free.
  mi_assert_internal(mi_atomic_load_relaxed(&segment->thread_id) == 0 || mi_atomic_load_relaxed(&segment->thread_id) == _mi_thread_id());
  mi_atomic_store_release(&segment->thread_id, _mi_thread_id());
  segment->abandoned_visits = 0;
  segment->was_reclaimed = true;
  tld->reclaim_count++;
  mi_segments_track_size((long)mi_segment_size(segment), tld);
  mi_assert_internal(segment->next == NULL);
  _mi_stat_decrease(&tld->stats->segments_abandoned, 1);

  // for all slices
  const mi_slice_t* end;
  mi_slice_t* slice = mi_slices_start_iterate(segment, &end);
  while (slice < end) {
    mi_assert_internal(slice->slice_count > 0);
    mi_assert_internal(slice->slice_offset == 0);
    if (mi_slice_is_used(slice)) {
      // in use: reclaim the page in our heap
      mi_page_t* page = mi_slice_to_page(slice);
      mi_assert_internal(page->is_committed);
      mi_assert_internal(mi_page_thread_free_flag(page)==MI_NEVER_DELAYED_FREE);
      mi_assert_internal(mi_page_heap(page) == NULL);
      mi_assert_internal(page->next == NULL && page->prev==NULL);
      _mi_stat_decrease(&tld->stats->pages_abandoned, 1);
      segment->abandoned--;
      // set the heap again and allow heap thread delayed free again.
      mi_heap_t* target_heap = _mi_heap_by_tag(heap, page->heap_tag);  // allow custom heaps to separate objects
      if (target_heap == NULL) {
        target_heap = heap;
        _mi_error_message(EINVAL, "page with tag %u cannot be reclaimed by a heap with the same tag (using %u instead)\n", page->heap_tag, heap->tag );
      }
      mi_page_set_heap(page, target_heap);
      _mi_page_use_delayed_free(page, MI_USE_DELAYED_FREE, true); // override never (after heap is set)
      _mi_page_free_collect(page, false); // ensure used count is up to date
      if (mi_page_all_free(page)) {
        // if everything free by now, free the page
        slice = mi_segment_page_clear(page, tld);   // set slice again due to coalesceing
      }
      else {
        // otherwise reclaim it into the heap
        _mi_page_reclaim(target_heap, page);
        if (requested_block_size == mi_page_block_size(page) && mi_page_has_any_available(page) && heap == target_heap) {
          if (right_page_reclaimed != NULL) { *right_page_reclaimed = true; }
        }
      }
    }
    else {
      // the span is free, add it to our page queues
      slice = mi_segment_span_free_coalesce(slice, tld); // set slice again due to coalesceing
    }
    mi_assert_internal(slice->slice_count>0 && slice->slice_offset==0);
    slice = slice + slice->slice_count;
  }

  mi_assert(segment->abandoned == 0);
  mi_assert_expensive(mi_segment_is_valid(segment, tld));
  if (segment->used == 0) {  // due to page_clear
    mi_assert_internal(right_page_reclaimed == NULL || !(*right_page_reclaimed));
    mi_segment_free(segment, false, tld);
    return NULL;
  }
  else {
    return segment;
  }
}

// attempt to reclaim a particular segment (called from multi threaded free `alloc.c:mi_free_block_mt`)
bool _mi_segment_attempt_reclaim(mi_heap_t* heap, mi_segment_t* segment) {
  if (mi_atomic_load_relaxed(&segment->thread_id) != 0) return false;  // it is not abandoned
  // don't reclaim more from a free than half the current segments
  // this is to prevent a pure free-ing thread to start owning too many segments
  if (heap->tld->segments.reclaim_count * 2 > heap->tld->segments.count) return false;
  if (_mi_arena_segment_clear_abandoned(segment)) {  // atomically unabandon
    mi_segment_t* res = mi_segment_reclaim(segment, heap, 0, NULL, &heap->tld->segments);
    mi_assert_internal(res == segment);
    return (res != NULL);
  }
  return false;
}

void _mi_abandoned_reclaim_all(mi_heap_t* heap, mi_segments_tld_t* tld) {
  mi_segment_t* segment;
  mi_arena_field_cursor_t current; _mi_arena_field_cursor_init(heap, &current);
  while ((segment = _mi_arena_segment_clear_abandoned_next(&current)) != NULL) {
    mi_segment_reclaim(segment, heap, 0, NULL, tld);
  }
}

static long mi_segment_get_reclaim_tries(void) {
  // limit the tries to 10% (default) of the abandoned segments with at least 8 and at most 1024 tries.
  const size_t perc = (size_t)mi_option_get_clamp(mi_option_max_segment_reclaim, 0, 100);
  if (perc <= 0) return 0;
  const size_t total_count = _mi_arena_segment_abandoned_count();
  if (total_count == 0) return 0;
  const size_t relative_count = (total_count > 10000 ? (total_count / 100) * perc : (total_count * perc) / 100); // avoid overflow
  long max_tries = (long)(relative_count <= 1 ? 1 : (relative_count > 1024 ? 1024 : relative_count));
  if (max_tries < 8 && total_count > 8) { max_tries = 8;  }
  return max_tries;
}

static mi_segment_t* mi_segment_try_reclaim(mi_heap_t* heap, size_t needed_slices, size_t block_size, bool* reclaimed, mi_segments_tld_t* tld)
{
  *reclaimed = false;
  long max_tries = mi_segment_get_reclaim_tries();
  if (max_tries <= 0) return NULL;

  mi_segment_t* segment;
  mi_arena_field_cursor_t current; _mi_arena_field_cursor_init(heap, &current);
  while ((max_tries-- > 0) && ((segment = _mi_arena_segment_clear_abandoned_next(&current)) != NULL))
  {
    segment->abandoned_visits++;
    // todo: should we respect numa affinity for abondoned reclaim? perhaps only for the first visit?
    // todo: an arena exclusive heap will potentially visit many abandoned unsuitable segments and use many tries
    // Perhaps we can skip non-suitable ones in a better way?
    bool is_suitable = _mi_heap_memid_is_suitable(heap, segment->memid);
    bool has_page = mi_segment_check_free(segment,needed_slices,block_size,tld); // try to free up pages (due to concurrent frees)
    if (segment->used == 0) {
      // free the segment (by forced reclaim) to make it available to other threads.
      // note1: we prefer to free a segment as that might lead to reclaiming another
      // segment that is still partially used.
      // note2: we could in principle optimize this by skipping reclaim and directly
      // freeing but that would violate some invariants temporarily)
      mi_segment_reclaim(segment, heap, 0, NULL, tld);
    }
    else if (has_page && is_suitable) {
      // found a large enough free span, or a page of the right block_size with free space
      // we return the result of reclaim (which is usually `segment`) as it might free
      // the segment due to concurrent frees (in which case `NULL` is returned).
      return mi_segment_reclaim(segment, heap, block_size, reclaimed, tld);
    }
    else if (segment->abandoned_visits > 3 && is_suitable) {
      // always reclaim on 3rd visit to limit the abandoned queue length.
      mi_segment_reclaim(segment, heap, 0, NULL, tld);
    }
    else {
      // otherwise, push on the visited list so it gets not looked at too quickly again
      mi_segment_try_purge(segment, false /* true force? */, tld->stats); // force purge if needed as we may not visit soon again
      _mi_arena_segment_mark_abandoned(segment);
    }
  }
  return NULL;
}


void _mi_abandoned_collect(mi_heap_t* heap, bool force, mi_segments_tld_t* tld)
{
  mi_segment_t* segment;
  mi_arena_field_cursor_t current; _mi_arena_field_cursor_init(heap, &current);
  long max_tries = (force ? (long)_mi_arena_segment_abandoned_count() : 1024);  // limit latency
  while ((max_tries-- > 0) && ((segment = _mi_arena_segment_clear_abandoned_next(&current)) != NULL)) {
    mi_segment_check_free(segment,0,0,tld); // try to free up pages (due to concurrent frees)
    if (segment->used == 0) {
      // free the segment (by forced reclaim) to make it available to other threads.
      // note: we could in principle optimize this by skipping reclaim and directly
      // freeing but that would violate some invariants temporarily)
      mi_segment_reclaim(segment, heap, 0, NULL, tld);
    }
    else {
      // otherwise, purge if needed and push on the visited list
      // note: forced purge can be expensive if many threads are destroyed/created as in mstress.
      mi_segment_try_purge(segment, force, tld->stats);
      _mi_arena_segment_mark_abandoned(segment);
    }
  }
}

/* -----------------------------------------------------------
   Reclaim or allocate
----------------------------------------------------------- */

static mi_segment_t* mi_segment_reclaim_or_alloc(mi_heap_t* heap, size_t needed_slices, size_t block_size, mi_segments_tld_t* tld, mi_os_tld_t* os_tld)
{
  mi_assert_internal(block_size <= MI_LARGE_OBJ_SIZE_MAX);

  // 1. try to reclaim an abandoned segment
  bool reclaimed;
  mi_segment_t* segment = mi_segment_try_reclaim(heap, needed_slices, block_size, &reclaimed, tld);
  if (reclaimed) {
    // reclaimed the right page right into the heap
    mi_assert_internal(segment != NULL);
    return NULL; // pretend out-of-memory as the page will be in the page queue of the heap with available blocks
  }
  else if (segment != NULL) {
    // reclaimed a segment with a large enough empty span in it
    return segment;
  }
  // 2. otherwise allocate a fresh segment
  return mi_segment_alloc(0, 0, heap->arena_id, tld, os_tld, NULL);
}


/* -----------------------------------------------------------
   Page allocation
----------------------------------------------------------- */

static mi_page_t* mi_segments_page_alloc(mi_heap_t* heap, mi_page_kind_t page_kind, size_t required, size_t block_size, mi_segments_tld_t* tld, mi_os_tld_t* os_tld)
{
  mi_assert_internal(required <= MI_LARGE_OBJ_SIZE_MAX && page_kind <= MI_PAGE_LARGE);

  // find a free page
  size_t page_size = _mi_align_up(required, (required > MI_MEDIUM_PAGE_SIZE ? MI_MEDIUM_PAGE_SIZE : MI_SEGMENT_SLICE_SIZE));
  size_t slices_needed = page_size / MI_SEGMENT_SLICE_SIZE;
  mi_assert_internal(slices_needed * MI_SEGMENT_SLICE_SIZE == page_size);
  mi_page_t* page = mi_segments_page_find_and_allocate(slices_needed, heap->arena_id, tld); //(required <= MI_SMALL_SIZE_MAX ? 0 : slices_needed), tld);
  if (page==NULL) {
    // no free page, allocate a new segment and try again
    if (mi_segment_reclaim_or_alloc(heap, slices_needed, block_size, tld, os_tld) == NULL) {
      // OOM or reclaimed a good page in the heap
      return NULL;
    }
    else {
      // otherwise try again
      return mi_segments_page_alloc(heap, page_kind, required, block_size, tld, os_tld);
    }
  }
  mi_assert_internal(page != NULL && page->slice_count*MI_SEGMENT_SLICE_SIZE == page_size);
  mi_assert_internal(_mi_ptr_segment(page)->thread_id == _mi_thread_id());
  mi_segment_try_purge(_mi_ptr_segment(page), false, tld->stats);
  return page;
}



/* -----------------------------------------------------------
   Huge page allocation
----------------------------------------------------------- */

static mi_page_t* mi_segment_huge_page_alloc(size_t size, size_t page_alignment, mi_arena_id_t req_arena_id, mi_segments_tld_t* tld, mi_os_tld_t* os_tld)
{
  mi_page_t* page = NULL;
  mi_segment_t* segment = mi_segment_alloc(size,page_alignment,req_arena_id,tld,os_tld,&page);
  if (segment == NULL || page==NULL) return NULL;
  mi_assert_internal(segment->used==1);
  mi_assert_internal(mi_page_block_size(page) >= size);
  #if MI_HUGE_PAGE_ABANDON
  segment->thread_id = 0; // huge segments are immediately abandoned
  #endif

  // for huge pages we initialize the block_size as we may
  // overallocate to accommodate large alignments.
  size_t psize;
  uint8_t* start = _mi_segment_page_start(segment, page, &psize);
  page->block_size = psize;
  mi_assert_internal(page->is_huge);

  // decommit the part of the prefix of a page that will not be used; this can be quite large (close to MI_SEGMENT_SIZE)
  if (page_alignment > 0 && segment->allow_decommit) {
    uint8_t* aligned_p = (uint8_t*)_mi_align_up((uintptr_t)start, page_alignment);
    mi_assert_internal(_mi_is_aligned(aligned_p, page_alignment));
    mi_assert_internal(psize - (aligned_p - start) >= size);
    uint8_t* decommit_start = start + sizeof(mi_block_t);              // for the free list
    ptrdiff_t decommit_size = aligned_p - decommit_start;
    _mi_os_reset(decommit_start, decommit_size, &_mi_stats_main);   // note: cannot use segment_decommit on huge segments
  }

  return page;
}

#if MI_HUGE_PAGE_ABANDON
// free huge block from another thread
void _mi_segment_huge_page_free(mi_segment_t* segment, mi_page_t* page, mi_block_t* block) {
  // huge page segments are always abandoned and can be freed immediately by any thread
  mi_assert_internal(segment->kind==MI_SEGMENT_HUGE);
  mi_assert_internal(segment == _mi_page_segment(page));
  mi_assert_internal(mi_atomic_load_relaxed(&segment->thread_id)==0);

  // claim it and free
  mi_heap_t* heap = mi_heap_get_default(); // issue #221; don't use the internal get_default_heap as we need to ensure the thread is initialized.
  // paranoia: if this it the last reference, the cas should always succeed
  size_t expected_tid = 0;
  if (mi_atomic_cas_strong_acq_rel(&segment->thread_id, &expected_tid, heap->thread_id)) {
    mi_block_set_next(page, block, page->free);
    page->free = block;
    page->used--;
    page->is_zero_init = false;
    mi_assert(page->used == 0);
    mi_tld_t* tld = heap->tld;
    _mi_segment_page_free(page, true, &tld->segments);
  }
#if (MI_DEBUG!=0)
  else {
    mi_assert_internal(false);
  }
#endif
}

#else
// reset memory of a huge block from another thread
void _mi_segment_huge_page_reset(mi_segment_t* segment, mi_page_t* page, mi_block_t* block) {
  MI_UNUSED(page);
  mi_assert_internal(segment->kind == MI_SEGMENT_HUGE);
  mi_assert_internal(segment == _mi_page_segment(page));
  mi_assert_internal(page->used == 1); // this is called just before the free
  mi_assert_internal(page->free == NULL);
  if (segment->allow_decommit) {
    size_t csize = mi_usable_size(block);
    if (csize > sizeof(mi_block_t)) {
      csize = csize - sizeof(mi_block_t);
      uint8_t* p = (uint8_t*)block + sizeof(mi_block_t);
      _mi_os_reset(p, csize, &_mi_stats_main);  // note: cannot use segment_decommit on huge segments
    }
  }
}
#endif

/* -----------------------------------------------------------
   Page allocation and free
----------------------------------------------------------- */
mi_page_t* _mi_segment_page_alloc(mi_heap_t* heap, size_t block_size, size_t page_alignment, mi_segments_tld_t* tld, mi_os_tld_t* os_tld) {
  mi_page_t* page;
  if mi_unlikely(page_alignment > MI_BLOCK_ALIGNMENT_MAX) {
    mi_assert_internal(_mi_is_power_of_two(page_alignment));
    mi_assert_internal(page_alignment >= MI_SEGMENT_SIZE);
    if (page_alignment < MI_SEGMENT_SIZE) { page_alignment = MI_SEGMENT_SIZE; }
    page = mi_segment_huge_page_alloc(block_size,page_alignment,heap->arena_id,tld,os_tld);
  }
  else if (block_size <= MI_SMALL_OBJ_SIZE_MAX) {
    page = mi_segments_page_alloc(heap,MI_PAGE_SMALL,block_size,block_size,tld,os_tld);
  }
  else if (block_size <= MI_MEDIUM_OBJ_SIZE_MAX) {
    page = mi_segments_page_alloc(heap,MI_PAGE_MEDIUM,MI_MEDIUM_PAGE_SIZE,block_size,tld, os_tld);
  }
  else if (block_size <= MI_LARGE_OBJ_SIZE_MAX) {
    page = mi_segments_page_alloc(heap,MI_PAGE_LARGE,block_size,block_size,tld, os_tld);
  }
  else {
    page = mi_segment_huge_page_alloc(block_size,page_alignment,heap->arena_id,tld,os_tld);
  }
  mi_assert_internal(page == NULL || _mi_heap_memid_is_suitable(heap, _mi_page_segment(page)->memid));
  mi_assert_expensive(page == NULL || mi_segment_is_valid(_mi_page_segment(page),tld));
  return page;
}


