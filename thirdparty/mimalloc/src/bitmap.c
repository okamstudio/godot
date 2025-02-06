/* ----------------------------------------------------------------------------
Copyright (c) 2019-2023 Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

/* ----------------------------------------------------------------------------
Concurrent bitmap that can set/reset sequences of bits atomically,
represented as an array of fields where each field is a machine word (`size_t`)

There are two api's; the standard one cannot have sequences that cross
between the bitmap fields (and a sequence must be <= MI_BITMAP_FIELD_BITS).

The `_across` postfixed functions do allow sequences that can cross over
between the fields. (This is used in arena allocation)
---------------------------------------------------------------------------- */

#include "mimalloc.h"
#include "mimalloc/internal.h"
#include "bitmap.h"

/* -----------------------------------------------------------
  Bitmap definition
----------------------------------------------------------- */

// The bit mask for a given number of blocks at a specified bit index.
static inline size_t mi_bitmap_mask_(size_t count, size_t bitidx) {
  mi_assert_internal(count + bitidx <= MI_BITMAP_FIELD_BITS);
  mi_assert_internal(count > 0);
  if (count >= MI_BITMAP_FIELD_BITS) return MI_BITMAP_FIELD_FULL;
  if (count == 0) return 0;
  return ((((size_t)1 << count) - 1) << bitidx);
}


/* -----------------------------------------------------------
  Claim a bit sequence atomically
----------------------------------------------------------- */

// Try to atomically claim a sequence of `count` bits in a single
// field at `idx` in `bitmap`. Returns `true` on success.
inline bool _mi_bitmap_try_find_claim_field(mi_bitmap_t bitmap, size_t idx, const size_t count, mi_bitmap_index_t* bitmap_idx)
{
  mi_assert_internal(bitmap_idx != NULL);
  mi_assert_internal(count <= MI_BITMAP_FIELD_BITS);
  mi_assert_internal(count > 0);
  mi_bitmap_field_t* field = &bitmap[idx];
  size_t map  = mi_atomic_load_relaxed(field);
  if (map==MI_BITMAP_FIELD_FULL) return false; // short cut

  // search for 0-bit sequence of length count
  const size_t mask = mi_bitmap_mask_(count, 0);
  const size_t bitidx_max = MI_BITMAP_FIELD_BITS - count;

#ifdef MI_HAVE_FAST_BITSCAN
  size_t bitidx = mi_ctz(~map);    // quickly find the first zero bit if possible
#else
  size_t bitidx = 0;               // otherwise start at 0
#endif
  size_t m = (mask << bitidx);     // invariant: m == mask shifted by bitidx

  // scan linearly for a free range of zero bits
  while (bitidx <= bitidx_max) {
    const size_t mapm = (map & m);
    if (mapm == 0) {  // are the mask bits free at bitidx?
      mi_assert_internal((m >> bitidx) == mask); // no overflow?
      const size_t newmap = (map | m);
      mi_assert_internal((newmap^map) >> bitidx == mask);
      if (!mi_atomic_cas_strong_acq_rel(field, &map, newmap)) {  // TODO: use weak cas here?
        // no success, another thread claimed concurrently.. keep going (with updated `map`)
        continue;
      }
      else {
        // success, we claimed the bits!
        *bitmap_idx = mi_bitmap_index_create(idx, bitidx);
        return true;
      }
    }
    else {
      // on to the next bit range
#ifdef MI_HAVE_FAST_BITSCAN
      mi_assert_internal(mapm != 0);
      const size_t shift = (count == 1 ? 1 : (MI_INTPTR_BITS - mi_clz(mapm) - bitidx));
      mi_assert_internal(shift > 0 && shift <= count);
#else
      const size_t shift = 1;
#endif
      bitidx += shift;
      m <<= shift;
    }
  }
  // no bits found
  return false;
}

// Find `count` bits of 0 and set them to 1 atomically; returns `true` on success.
// Starts at idx, and wraps around to search in all `bitmap_fields` fields.
// `count` can be at most MI_BITMAP_FIELD_BITS and will never cross fields.
bool _mi_bitmap_try_find_from_claim(mi_bitmap_t bitmap, const size_t bitmap_fields, const size_t start_field_idx, const size_t count, mi_bitmap_index_t* bitmap_idx) {
  size_t idx = start_field_idx;
  for (size_t visited = 0; visited < bitmap_fields; visited++, idx++) {
    if (idx >= bitmap_fields) { idx = 0; } // wrap
    if (_mi_bitmap_try_find_claim_field(bitmap, idx, count, bitmap_idx)) {
      return true;
    }
  }
  return false;
}

// Like _mi_bitmap_try_find_from_claim but with an extra predicate that must be fullfilled
bool _mi_bitmap_try_find_from_claim_pred(mi_bitmap_t bitmap, const size_t bitmap_fields, 
            const size_t start_field_idx, const size_t count, 
            mi_bitmap_pred_fun_t pred_fun, void* pred_arg,            
            mi_bitmap_index_t* bitmap_idx) {
  size_t idx = start_field_idx;
  for (size_t visited = 0; visited < bitmap_fields; visited++, idx++) {
    if (idx >= bitmap_fields) idx = 0; // wrap
    if (_mi_bitmap_try_find_claim_field(bitmap, idx, count, bitmap_idx)) {
      if (pred_fun == NULL || pred_fun(*bitmap_idx, pred_arg)) { 
        return true;
      }
      // predicate returned false, unclaim and look further
      _mi_bitmap_unclaim(bitmap, bitmap_fields, count, *bitmap_idx);
    }
  }
  return false;
}

// Set `count` bits at `bitmap_idx` to 0 atomically
// Returns `true` if all `count` bits were 1 previously.
bool _mi_bitmap_unclaim(mi_bitmap_t bitmap, size_t bitmap_fields, size_t count, mi_bitmap_index_t bitmap_idx) {
  const size_t idx = mi_bitmap_index_field(bitmap_idx);
  const size_t bitidx = mi_bitmap_index_bit_in_field(bitmap_idx);
  const size_t mask = mi_bitmap_mask_(count, bitidx);
  mi_assert_internal(bitmap_fields > idx); MI_UNUSED(bitmap_fields);
  // mi_assert_internal((bitmap[idx] & mask) == mask);
  const size_t prev = mi_atomic_and_acq_rel(&bitmap[idx], ~mask);
  return ((prev & mask) == mask);
}


// Set `count` bits at `bitmap_idx` to 1 atomically
// Returns `true` if all `count` bits were 0 previously. `any_zero` is `true` if there was at least one zero bit.
bool _mi_bitmap_claim(mi_bitmap_t bitmap, size_t bitmap_fields, size_t count, mi_bitmap_index_t bitmap_idx, bool* any_zero) {
  const size_t idx = mi_bitmap_index_field(bitmap_idx);
  const size_t bitidx = mi_bitmap_index_bit_in_field(bitmap_idx);
  const size_t mask = mi_bitmap_mask_(count, bitidx);
  mi_assert_internal(bitmap_fields > idx); MI_UNUSED(bitmap_fields);
  //mi_assert_internal(any_zero != NULL || (bitmap[idx] & mask) == 0);
  size_t prev = mi_atomic_or_acq_rel(&bitmap[idx], mask);
  if (any_zero != NULL) { *any_zero = ((prev & mask) != mask); }
  return ((prev & mask) == 0);
}

// Returns `true` if all `count` bits were 1. `any_ones` is `true` if there was at least one bit set to one.
static bool mi_bitmap_is_claimedx(mi_bitmap_t bitmap, size_t bitmap_fields, size_t count, mi_bitmap_index_t bitmap_idx, bool* any_ones) {
  const size_t idx = mi_bitmap_index_field(bitmap_idx);
  const size_t bitidx = mi_bitmap_index_bit_in_field(bitmap_idx);
  const size_t mask = mi_bitmap_mask_(count, bitidx);
  mi_assert_internal(bitmap_fields > idx); MI_UNUSED(bitmap_fields);
  const size_t field = mi_atomic_load_relaxed(&bitmap[idx]);
  if (any_ones != NULL) { *any_ones = ((field & mask) != 0); }
  return ((field & mask) == mask);
}

// Try to set `count` bits at `bitmap_idx` from 0 to 1 atomically. 
// Returns `true` if successful when all previous `count` bits were 0.
bool _mi_bitmap_try_claim(mi_bitmap_t bitmap, size_t bitmap_fields, size_t count, mi_bitmap_index_t bitmap_idx) {
  const size_t idx = mi_bitmap_index_field(bitmap_idx);
  const size_t bitidx = mi_bitmap_index_bit_in_field(bitmap_idx);
  const size_t mask = mi_bitmap_mask_(count, bitidx);
  mi_assert_internal(bitmap_fields > idx); MI_UNUSED(bitmap_fields);
  size_t expected = mi_atomic_load_relaxed(&bitmap[idx]);
  do  {    
    if ((expected & mask) != 0) return false;
  } 
  while (!mi_atomic_cas_strong_acq_rel(&bitmap[idx], &expected, expected | mask));
  mi_assert_internal((expected & mask) == 0);
  return true;
}


bool _mi_bitmap_is_claimed(mi_bitmap_t bitmap, size_t bitmap_fields, size_t count, mi_bitmap_index_t bitmap_idx) {
  return mi_bitmap_is_claimedx(bitmap, bitmap_fields, count, bitmap_idx, NULL);
}

bool _mi_bitmap_is_any_claimed(mi_bitmap_t bitmap, size_t bitmap_fields, size_t count, mi_bitmap_index_t bitmap_idx) {
  bool any_ones;
  mi_bitmap_is_claimedx(bitmap, bitmap_fields, count, bitmap_idx, &any_ones);
  return any_ones;
}


//--------------------------------------------------------------------------
// the `_across` functions work on bitmaps where sequences can cross over
// between the fields. This is used in arena allocation
//--------------------------------------------------------------------------

// Try to atomically claim a sequence of `count` bits starting from the field
// at `idx` in `bitmap` and crossing into subsequent fields. Returns `true` on success.
// Only needs to consider crossing into the next fields (see `mi_bitmap_try_find_from_claim_across`)
static bool mi_bitmap_try_find_claim_field_across(mi_bitmap_t bitmap, size_t bitmap_fields, size_t idx, const size_t count, const size_t retries, mi_bitmap_index_t* bitmap_idx, mi_stats_t* stats)
{
  mi_assert_internal(bitmap_idx != NULL);

  // check initial trailing zeros
  mi_bitmap_field_t* field = &bitmap[idx];
  size_t map = mi_atomic_load_relaxed(field);
  const size_t initial = mi_clz(map);  // count of initial zeros starting at idx
  mi_assert_internal(initial <= MI_BITMAP_FIELD_BITS);
  if (initial == 0)     return false;
  if (initial >= count) return _mi_bitmap_try_find_claim_field(bitmap, idx, count, bitmap_idx);    // no need to cross fields (this case won't happen for us)
  if (_mi_divide_up(count - initial, MI_BITMAP_FIELD_BITS) >= (bitmap_fields - idx)) return false; // not enough entries
  
  // scan ahead
  size_t found = initial;
  size_t mask = 0;     // mask bits for the final field
  while(found < count) {
    field++;
    map = mi_atomic_load_relaxed(field);
    const size_t mask_bits = (found + MI_BITMAP_FIELD_BITS <= count ? MI_BITMAP_FIELD_BITS : (count - found));
    mi_assert_internal(mask_bits > 0 && mask_bits <= MI_BITMAP_FIELD_BITS);
    mask = mi_bitmap_mask_(mask_bits, 0);
    if ((map & mask) != 0) return false;  // some part is already claimed
    found += mask_bits;
  }
  mi_assert_internal(field < &bitmap[bitmap_fields]);

  // we found a range of contiguous zeros up to the final field; mask contains mask in the final field
  // now try to claim the range atomically
  mi_bitmap_field_t* const final_field = field;
  const size_t final_mask = mask;
  mi_bitmap_field_t* const initial_field = &bitmap[idx];
  const size_t initial_idx = MI_BITMAP_FIELD_BITS - initial;
  const size_t initial_mask = mi_bitmap_mask_(initial, initial_idx);

  // initial field
  size_t newmap;
  field = initial_field;
  map = mi_atomic_load_relaxed(field);
  do {
    newmap = (map | initial_mask);
    if ((map & initial_mask) != 0) { goto rollback; };
  } while (!mi_atomic_cas_strong_acq_rel(field, &map, newmap));

  // intermediate fields
  while (++field < final_field) {
    newmap = MI_BITMAP_FIELD_FULL;
    map = 0;
    if (!mi_atomic_cas_strong_acq_rel(field, &map, newmap)) { goto rollback; }
  }

  // final field
  mi_assert_internal(field == final_field);
  map = mi_atomic_load_relaxed(field);
  do {
    newmap = (map | final_mask);
    if ((map & final_mask) != 0) { goto rollback; }
  } while (!mi_atomic_cas_strong_acq_rel(field, &map, newmap));

  // claimed!
  mi_stat_counter_increase(stats->arena_crossover_count,1);
  *bitmap_idx = mi_bitmap_index_create(idx, initial_idx);
  return true;

rollback:
  // roll back intermediate fields
  // (we just failed to claim `field` so decrement first)
  while (--field > initial_field) {
    newmap = 0;
    map = MI_BITMAP_FIELD_FULL;
    mi_assert_internal(mi_atomic_load_relaxed(field) == map);
    mi_atomic_store_release(field, newmap);
  }
  if (field == initial_field) {               // (if we failed on the initial field, `field + 1 == initial_field`)
    map = mi_atomic_load_relaxed(field);
    do {
      mi_assert_internal((map & initial_mask) == initial_mask);
      newmap = (map & ~initial_mask);
    } while (!mi_atomic_cas_strong_acq_rel(field, &map, newmap));
  }
  mi_stat_counter_increase(stats->arena_rollback_count,1);
  // retry? (we make a recursive call instead of goto to be able to use const declarations)
  if (retries <= 2) {
    return mi_bitmap_try_find_claim_field_across(bitmap, bitmap_fields, idx, count, retries+1, bitmap_idx, stats);
  }
  else {
    return false;
  }
}


// Find `count` bits of zeros and set them to 1 atomically; returns `true` on success.
// Starts at idx, and wraps around to search in all `bitmap_fields` fields.
bool _mi_bitmap_try_find_from_claim_across(mi_bitmap_t bitmap, const size_t bitmap_fields, const size_t start_field_idx, const size_t count, mi_bitmap_index_t* bitmap_idx, mi_stats_t* stats) {
  mi_assert_internal(count > 0);
  if (count <= 2) {
    // we don't bother with crossover fields for small counts
    return _mi_bitmap_try_find_from_claim(bitmap, bitmap_fields, start_field_idx, count, bitmap_idx);
  }

  // visit the fields
  size_t idx = start_field_idx;
  for (size_t visited = 0; visited < bitmap_fields; visited++, idx++) {
    if (idx >= bitmap_fields) { idx = 0; } // wrap
    // first try to claim inside a field
    /*
    if (count <= MI_BITMAP_FIELD_BITS) {
      if (_mi_bitmap_try_find_claim_field(bitmap, idx, count, bitmap_idx)) {
        return true;
      }
    }
    */
    // if that fails, then try to claim across fields
    if (mi_bitmap_try_find_claim_field_across(bitmap, bitmap_fields, idx, count, 0, bitmap_idx, stats)) {
      return true;
    }
  }
  return false;
}

// Helper for masks across fields; returns the mid count, post_mask may be 0
static size_t mi_bitmap_mask_across(mi_bitmap_index_t bitmap_idx, size_t bitmap_fields, size_t count, size_t* pre_mask, size_t* mid_mask, size_t* post_mask) {
  MI_UNUSED(bitmap_fields);
  const size_t bitidx = mi_bitmap_index_bit_in_field(bitmap_idx);
  if mi_likely(bitidx + count <= MI_BITMAP_FIELD_BITS) {
    *pre_mask = mi_bitmap_mask_(count, bitidx);
    *mid_mask = 0;
    *post_mask = 0;
    mi_assert_internal(mi_bitmap_index_field(bitmap_idx) < bitmap_fields);
    return 0;
  }
  else {
    const size_t pre_bits = MI_BITMAP_FIELD_BITS - bitidx;
    mi_assert_internal(pre_bits < count);
    *pre_mask = mi_bitmap_mask_(pre_bits, bitidx);
    count -= pre_bits;
    const size_t mid_count = (count / MI_BITMAP_FIELD_BITS);
    *mid_mask = MI_BITMAP_FIELD_FULL;
    count %= MI_BITMAP_FIELD_BITS;
    *post_mask = (count==0 ? 0 : mi_bitmap_mask_(count, 0));
    mi_assert_internal(mi_bitmap_index_field(bitmap_idx) + mid_count + (count==0 ? 0 : 1) < bitmap_fields);
    return mid_count;
  }
}

// Set `count` bits at `bitmap_idx` to 0 atomically
// Returns `true` if all `count` bits were 1 previously.
bool _mi_bitmap_unclaim_across(mi_bitmap_t bitmap, size_t bitmap_fields, size_t count, mi_bitmap_index_t bitmap_idx) {
  size_t idx = mi_bitmap_index_field(bitmap_idx);
  size_t pre_mask;
  size_t mid_mask;
  size_t post_mask;
  size_t mid_count = mi_bitmap_mask_across(bitmap_idx, bitmap_fields, count, &pre_mask, &mid_mask, &post_mask);
  bool all_one = true;
  mi_bitmap_field_t* field = &bitmap[idx];
  size_t prev = mi_atomic_and_acq_rel(field++, ~pre_mask);   // clear first part
  if ((prev & pre_mask) != pre_mask) all_one = false;
  while(mid_count-- > 0) {
    prev = mi_atomic_and_acq_rel(field++, ~mid_mask);        // clear mid part
    if ((prev & mid_mask) != mid_mask) all_one = false;
  }
  if (post_mask!=0) {
    prev = mi_atomic_and_acq_rel(field, ~post_mask);         // clear end part
    if ((prev & post_mask) != post_mask) all_one = false;
  }
  return all_one;
}

// Set `count` bits at `bitmap_idx` to 1 atomically
// Returns `true` if all `count` bits were 0 previously. `any_zero` is `true` if there was at least one zero bit.
bool _mi_bitmap_claim_across(mi_bitmap_t bitmap, size_t bitmap_fields, size_t count, mi_bitmap_index_t bitmap_idx, bool* pany_zero) {
  size_t idx = mi_bitmap_index_field(bitmap_idx);
  size_t pre_mask;
  size_t mid_mask;
  size_t post_mask;
  size_t mid_count = mi_bitmap_mask_across(bitmap_idx, bitmap_fields, count, &pre_mask, &mid_mask, &post_mask);
  bool all_zero = true;
  bool any_zero = false;
  _Atomic(size_t)*field = &bitmap[idx];
  size_t prev = mi_atomic_or_acq_rel(field++, pre_mask);
  if ((prev & pre_mask) != 0) all_zero = false;
  if ((prev & pre_mask) != pre_mask) any_zero = true;
  while (mid_count-- > 0) {
    prev = mi_atomic_or_acq_rel(field++, mid_mask);
    if ((prev & mid_mask) != 0) all_zero = false;
    if ((prev & mid_mask) != mid_mask) any_zero = true;
  }
  if (post_mask!=0) {
    prev = mi_atomic_or_acq_rel(field, post_mask);
    if ((prev & post_mask) != 0) all_zero = false;
    if ((prev & post_mask) != post_mask) any_zero = true;
  }
  if (pany_zero != NULL) { *pany_zero = any_zero; }
  return all_zero;
}


// Returns `true` if all `count` bits were 1.
// `any_ones` is `true` if there was at least one bit set to one.
static bool mi_bitmap_is_claimedx_across(mi_bitmap_t bitmap, size_t bitmap_fields, size_t count, mi_bitmap_index_t bitmap_idx, bool* pany_ones) {
  size_t idx = mi_bitmap_index_field(bitmap_idx);
  size_t pre_mask;
  size_t mid_mask;
  size_t post_mask;
  size_t mid_count = mi_bitmap_mask_across(bitmap_idx, bitmap_fields, count, &pre_mask, &mid_mask, &post_mask);
  bool all_ones = true;
  bool any_ones = false;
  mi_bitmap_field_t* field = &bitmap[idx];
  size_t prev = mi_atomic_load_relaxed(field++);
  if ((prev & pre_mask) != pre_mask) all_ones = false;
  if ((prev & pre_mask) != 0) any_ones = true;
  while (mid_count-- > 0) {
    prev = mi_atomic_load_relaxed(field++);
    if ((prev & mid_mask) != mid_mask) all_ones = false;
    if ((prev & mid_mask) != 0) any_ones = true;
  }
  if (post_mask!=0) {
    prev = mi_atomic_load_relaxed(field);
    if ((prev & post_mask) != post_mask) all_ones = false;
    if ((prev & post_mask) != 0) any_ones = true;
  }
  if (pany_ones != NULL) { *pany_ones = any_ones; }
  return all_ones;
}

bool _mi_bitmap_is_claimed_across(mi_bitmap_t bitmap, size_t bitmap_fields, size_t count, mi_bitmap_index_t bitmap_idx) {
  return mi_bitmap_is_claimedx_across(bitmap, bitmap_fields, count, bitmap_idx, NULL);
}

bool _mi_bitmap_is_any_claimed_across(mi_bitmap_t bitmap, size_t bitmap_fields, size_t count, mi_bitmap_index_t bitmap_idx) {
  bool any_ones;
  mi_bitmap_is_claimedx_across(bitmap, bitmap_fields, count, bitmap_idx, &any_ones);
  return any_ones;
}
