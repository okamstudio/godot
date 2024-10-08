/*
 * Copyright © 1998-2004  David Turner and Werner Lemberg
 * Copyright © 2004,2007,2009,2010  Red Hat, Inc.
 * Copyright © 2011,2012  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Red Hat Author(s): Owen Taylor, Behdad Esfahbod
 * Google Author(s): Behdad Esfahbod
 */

#include "hb-buffer.hh"
#include "hb-utf.hh"


/**
 * SECTION: hb-buffer
 * @title: hb-buffer
 * @short_description: Input and output buffers
 * @include: hb.h
 *
 * Buffers serve a dual role in HarfBuzz; before shaping, they hold
 * the input characters that are passed to hb_shape(), and after
 * shaping they hold the output glyphs.
 *
 * The input buffer is a sequence of Unicode codepoints, with
 * associated attributes such as direction and script.  The output
 * buffer is a sequence of glyphs, with associated attributes such
 * as position and cluster.
 **/


/**
 * hb_segment_properties_equal:
 * @a: first #hb_segment_properties_t to compare.
 * @b: second #hb_segment_properties_t to compare.
 *
 * Checks the equality of two #hb_segment_properties_t's.
 *
 * Return value:
 * `true` if all properties of @a equal those of @b, `false` otherwise.
 *
 * Since: 0.9.7
 **/
hb_bool_t
hb_segment_properties_equal (const hb_segment_properties_t *a,
			     const hb_segment_properties_t *b)
{
  return a->direction == b->direction &&
	 a->script    == b->script    &&
	 a->language  == b->language  &&
	 a->reserved1 == b->reserved1 &&
	 a->reserved2 == b->reserved2;

}

/**
 * hb_segment_properties_hash:
 * @p: #hb_segment_properties_t to hash.
 *
 * Creates a hash representing @p.
 *
 * Return value:
 * A hash of @p.
 *
 * Since: 0.9.7
 **/
unsigned int
hb_segment_properties_hash (const hb_segment_properties_t *p)
{
  return ((unsigned int) p->direction * 31 +
	  (unsigned int) p->script) * 31 +
	 (intptr_t) (p->language);
}

/**
 * hb_segment_properties_overlay:
 * @p: #hb_segment_properties_t to fill in.
 * @src: #hb_segment_properties_t to fill in from.
 *
 * Fills in missing fields of @p from @src in a considered manner.
 *
 * First, if @p does not have direction set, direction is copied from @src.
 *
 * Next, if @p and @src have the same direction (which can be unset), if @p
 * does not have script set, script is copied from @src.
 *
 * Finally, if @p and @src have the same direction and script (which either
 * can be unset), if @p does not have language set, language is copied from
 * @src.
 *
 * Since: 3.3.0
 **/
void
hb_segment_properties_overlay (hb_segment_properties_t *p,
			       const hb_segment_properties_t *src)
{
  if (unlikely (!p || !src))
    return;

  if (!p->direction)
    p->direction = src->direction;

  if (p->direction != src->direction)
    return;

  if (!p->script)
    p->script = src->script;

  if (p->script != src->script)
    return;

  if (!p->language)
    p->language = src->language;
}

/* Here is how the buffer works internally:
 *
 * There are two info pointers: info and out_info.  They always have
 * the same allocated size, but different lengths.
 *
 * As an optimization, both info and out_info may point to the
 * same piece of memory, which is owned by info.  This remains the
 * case as long as out_len doesn't exceed i at any time.
 * In that case, sync() is mostly no-op and the glyph operations
 * operate mostly in-place.
 *
 * As soon as out_info gets longer than info, out_info is moved over
 * to an alternate buffer (which we reuse the pos buffer for), and its
 * current contents (out_len entries) are copied to the new place.
 *
 * This should all remain transparent to the user.  sync() then
 * switches info over to out_info and does housekeeping.
 */



/* Internal API */

bool
hb_buffer_t::enlarge (unsigned int size)
{
  if (unlikely (!successful))
    return false;
  if (unlikely (size > max_len))
  {
    successful = false;
    return false;
  }

  unsigned int new_allocated = allocated;
  hb_glyph_position_t *new_pos = nullptr;
  hb_glyph_info_t *new_info = nullptr;
  bool separate_out = out_info != info;

  if (unlikely (hb_unsigned_mul_overflows (size, sizeof (info[0]))))
    goto done;

  while (size >= new_allocated)
    new_allocated += (new_allocated >> 1) + 32;

  unsigned new_bytes;
  if (unlikely (hb_unsigned_mul_overflows (new_allocated, sizeof (info[0]), &new_bytes)))
    goto done;

  static_assert (sizeof (info[0]) == sizeof (pos[0]), "");
  new_pos = (hb_glyph_position_t *) hb_realloc (pos, new_bytes);
  new_info = (hb_glyph_info_t *) hb_realloc (info, new_bytes);

done:
  if (unlikely (!new_pos || !new_info))
    successful = false;

  if (likely (new_pos))
    pos = new_pos;

  if (likely (new_info))
    info = new_info;

  out_info = separate_out ? (hb_glyph_info_t *) pos : info;
  if (likely (successful))
    allocated = new_allocated;

  return likely (successful);
}

bool
hb_buffer_t::make_room_for (unsigned int num_in,
			    unsigned int num_out)
{
  if (unlikely (!ensure (out_len + num_out))) return false;

  if (out_info == info &&
      out_len + num_out > idx + num_in)
  {
    assert (have_output);

    out_info = (hb_glyph_info_t *) pos;
    hb_memcpy (out_info, info, out_len * sizeof (out_info[0]));
  }

  return true;
}

bool
hb_buffer_t::shift_forward (unsigned int count)
{
  assert (have_output);
  if (unlikely (!ensure (len + count))) return false;

  memmove (info + idx + count, info + idx, (len - idx) * sizeof (info[0]));
  if (idx + count > len)
  {
    /* Under memory failure we might expose this area.  At least
     * clean it up.  Oh well...
     *
     * Ideally, we should at least set Default_Ignorable bits on
     * these, as well as consistent cluster values.  But the former
     * is layering violation... */
    hb_memset (info + len, 0, (idx + count - len) * sizeof (info[0]));
  }
  len += count;
  idx += count;

  return true;
}

hb_buffer_t::scratch_buffer_t *
hb_buffer_t::get_scratch_buffer (unsigned int *size)
{
  have_output = false;
  have_positions = false;

  out_len = 0;
  out_info = info;

  assert ((uintptr_t) pos % sizeof (scratch_buffer_t) == 0);
  *size = allocated * sizeof (pos[0]) / sizeof (scratch_buffer_t);
  return (scratch_buffer_t *) (void *) pos;
}



/* HarfBuzz-Internal API */

void
hb_buffer_t::similar (const hb_buffer_t &src)
{
  hb_unicode_funcs_destroy (unicode);
  unicode = hb_unicode_funcs_reference (src.unicode);
  flags = src.flags;
  cluster_level = src.cluster_level;
  replacement = src.replacement;
  invisible = src.invisible;
  not_found = src.not_found;
  not_found_variation_selector = src.not_found_variation_selector;
}

void
hb_buffer_t::reset ()
{
  hb_unicode_funcs_destroy (unicode);
  unicode = hb_unicode_funcs_reference (hb_unicode_funcs_get_default ());
  flags = HB_BUFFER_FLAG_DEFAULT;
  cluster_level = HB_BUFFER_CLUSTER_LEVEL_DEFAULT;
  replacement = HB_BUFFER_REPLACEMENT_CODEPOINT_DEFAULT;
  invisible = 0;
  not_found = 0;
  not_found_variation_selector = HB_CODEPOINT_INVALID;

  clear ();
}

void
hb_buffer_t::clear ()
{
  content_type = HB_BUFFER_CONTENT_TYPE_INVALID;
  hb_segment_properties_t default_props = HB_SEGMENT_PROPERTIES_DEFAULT;
  props = default_props;

  successful = true;
  shaping_failed = false;
  have_output = false;
  have_positions = false;

  idx = 0;
  len = 0;
  out_len = 0;
  out_info = info;

  hb_memset (context, 0, sizeof context);
  hb_memset (context_len, 0, sizeof context_len);

  deallocate_var_all ();
  serial = 0;
  random_state = 1;
  scratch_flags = HB_BUFFER_SCRATCH_FLAG_DEFAULT;
}

void
hb_buffer_t::enter ()
{
  deallocate_var_all ();
  serial = 0;
  shaping_failed = false;
  scratch_flags = HB_BUFFER_SCRATCH_FLAG_DEFAULT;
  unsigned mul;
  if (likely (!hb_unsigned_mul_overflows (len, HB_BUFFER_MAX_LEN_FACTOR, &mul)))
  {
    max_len = hb_max (mul, (unsigned) HB_BUFFER_MAX_LEN_MIN);
  }
  if (likely (!hb_unsigned_mul_overflows (len, HB_BUFFER_MAX_OPS_FACTOR, &mul)))
  {
    max_ops = hb_max (mul, (unsigned) HB_BUFFER_MAX_OPS_MIN);
  }
}
void
hb_buffer_t::leave ()
{
  max_len = HB_BUFFER_MAX_LEN_DEFAULT;
  max_ops = HB_BUFFER_MAX_OPS_DEFAULT;
  deallocate_var_all ();
  serial = 0;
  // Intentionally not reseting shaping_failed, such that it can be inspected.
}


void
hb_buffer_t::add (hb_codepoint_t  codepoint,
		  unsigned int    cluster)
{
  hb_glyph_info_t *glyph;

  if (unlikely (!ensure (len + 1))) return;

  glyph = &info[len];

  hb_memset (glyph, 0, sizeof (*glyph));
  glyph->codepoint = codepoint;
  glyph->mask = 0;
  glyph->cluster = cluster;

  len++;
}

void
hb_buffer_t::add_info (const hb_glyph_info_t &glyph_info)
{
  if (unlikely (!ensure (len + 1))) return;

  info[len] = glyph_info;

  len++;
}


void
hb_buffer_t::clear_output ()
{
  have_output = true;
  have_positions = false;

  idx = 0;
  out_len = 0;
  out_info = info;
}

void
hb_buffer_t::clear_positions ()
{
  have_output = false;
  have_positions = true;

  out_len = 0;
  out_info = info;

  hb_memset (pos, 0, sizeof (pos[0]) * len);
}

bool
hb_buffer_t::sync ()
{
  bool ret = false;

  assert (have_output);

  assert (idx <= len);

  if (unlikely (!successful || !next_glyphs (len - idx)))
    goto reset;

  if (out_info != info)
  {
    pos = (hb_glyph_position_t *) info;
    info = out_info;
  }
  len = out_len;
  ret = true;

reset:
  have_output = false;
  out_len = 0;
  out_info = info;
  idx = 0;

  return ret;
}

int
hb_buffer_t::sync_so_far ()
{
  bool had_output = have_output;
  unsigned out_i = out_len;
  unsigned i = idx;
  unsigned old_idx = idx;

  if (sync ())
    idx = out_i;
  else
    idx = i;

  if (had_output)
  {
    have_output = true;
    out_len = idx;
  }

  assert (idx <= len);

  return idx - old_idx;
}

bool
hb_buffer_t::move_to (unsigned int i)
{
  if (!have_output)
  {
    assert (i <= len);
    idx = i;
    return true;
  }
  if (unlikely (!successful))
    return false;

  assert (i <= out_len + (len - idx));

  if (out_len < i)
  {
    unsigned int count = i - out_len;
    if (unlikely (!make_room_for (count, count))) return false;

    memmove (out_info + out_len, info + idx, count * sizeof (out_info[0]));
    idx += count;
    out_len += count;
  }
  else if (out_len > i)
  {
    /* Tricky part: rewinding... */
    unsigned int count = out_len - i;

    /* This will blow in our face if memory allocation fails later
     * in this same lookup...
     *
     * We used to shift with extra 32 items.
     * But that would leave empty slots in the buffer in case of allocation
     * failures.  See comments in shift_forward().  This can cause O(N^2)
     * behavior more severely than adding 32 empty slots can... */
    if (unlikely (idx < count && !shift_forward (count - idx))) return false;

    assert (idx >= count);

    idx -= count;
    out_len -= count;
    memmove (info + idx, out_info + out_len, count * sizeof (out_info[0]));
  }

  return true;
}


void
hb_buffer_t::set_masks (hb_mask_t    value,
			hb_mask_t    mask,
			unsigned int cluster_start,
			unsigned int cluster_end)
{
  if (!mask)
    return;

  hb_mask_t not_mask = ~mask;
  value &= mask;

  unsigned int count = len;
  for (unsigned int i = 0; i < count; i++)
    if (cluster_start <= info[i].cluster && info[i].cluster < cluster_end)
      info[i].mask = (info[i].mask & not_mask) | value;
}

void
hb_buffer_t::merge_clusters_impl (unsigned int start,
				  unsigned int end)
{
  if (cluster_level == HB_BUFFER_CLUSTER_LEVEL_CHARACTERS)
  {
    unsafe_to_break (start, end);
    return;
  }

  unsigned int cluster = info[start].cluster;

  for (unsigned int i = start + 1; i < end; i++)
    cluster = hb_min (cluster, info[i].cluster);

  /* Extend end */
  if (cluster != info[end - 1].cluster)
    while (end < len && info[end - 1].cluster == info[end].cluster)
      end++;

  /* Extend start */
  if (cluster != info[start].cluster)
    while (idx < start && info[start - 1].cluster == info[start].cluster)
      start--;

  /* If we hit the start of buffer, continue in out-buffer. */
  if (idx == start && info[start].cluster != cluster)
    for (unsigned int i = out_len; i && out_info[i - 1].cluster == info[start].cluster; i--)
      set_cluster (out_info[i - 1], cluster);

  for (unsigned int i = start; i < end; i++)
    set_cluster (info[i], cluster);
}
void
hb_buffer_t::merge_out_clusters (unsigned int start,
				 unsigned int end)
{
  if (cluster_level == HB_BUFFER_CLUSTER_LEVEL_CHARACTERS)
    return;

  if (unlikely (end - start < 2))
    return;

  unsigned int cluster = out_info[start].cluster;

  for (unsigned int i = start + 1; i < end; i++)
    cluster = hb_min (cluster, out_info[i].cluster);

  /* Extend start */
  while (start && out_info[start - 1].cluster == out_info[start].cluster)
    start--;

  /* Extend end */
  while (end < out_len && out_info[end - 1].cluster == out_info[end].cluster)
    end++;

  /* If we hit the end of out-buffer, continue in buffer. */
  if (end == out_len)
    for (unsigned int i = idx; i < len && info[i].cluster == out_info[end - 1].cluster; i++)
      set_cluster (info[i], cluster);

  for (unsigned int i = start; i < end; i++)
    set_cluster (out_info[i], cluster);
}
void
hb_buffer_t::delete_glyph ()
{
  /* The logic here is duplicated in hb_ot_hide_default_ignorables(). */

  unsigned int cluster = info[idx].cluster;
  if ((idx + 1 < len && cluster == info[idx + 1].cluster) ||
      (out_len && cluster == out_info[out_len - 1].cluster))
  {
    /* Cluster survives; do nothing. */
    goto done;
  }

  if (out_len)
  {
    /* Merge cluster backward. */
    if (cluster < out_info[out_len - 1].cluster)
    {
      unsigned int mask = info[idx].mask;
      unsigned int old_cluster = out_info[out_len - 1].cluster;
      for (unsigned i = out_len; i && out_info[i - 1].cluster == old_cluster; i--)
	set_cluster (out_info[i - 1], cluster, mask);
    }
    goto done;
  }

  if (idx + 1 < len)
  {
    /* Merge cluster forward. */
    merge_clusters (idx, idx + 2);
    goto done;
  }

done:
  skip_glyph ();
}

void
hb_buffer_t::delete_glyphs_inplace (bool (*filter) (const hb_glyph_info_t *info))
{
  /* Merge clusters and delete filtered glyphs.
   * NOTE! We can't use out-buffer as we have positioning data. */
  unsigned int j = 0;
  unsigned int count = len;
  for (unsigned int i = 0; i < count; i++)
  {
    if (filter (&info[i]))
    {
      /* Merge clusters.
       * Same logic as delete_glyph(), but for in-place removal. */

      unsigned int cluster = info[i].cluster;
      if (i + 1 < count && cluster == info[i + 1].cluster)
	continue; /* Cluster survives; do nothing. */

      if (j)
      {
	/* Merge cluster backward. */
	if (cluster < info[j - 1].cluster)
	{
	  unsigned int mask = info[i].mask;
	  unsigned int old_cluster = info[j - 1].cluster;
	  for (unsigned k = j; k && info[k - 1].cluster == old_cluster; k--)
	    set_cluster (info[k - 1], cluster, mask);
	}
	continue;
      }

      if (i + 1 < count)
	merge_clusters (i, i + 2); /* Merge cluster forward. */

      continue;
    }

    if (j != i)
    {
      info[j] = info[i];
      pos[j] = pos[i];
    }
    j++;
  }
  len = j;
}

void
hb_buffer_t::guess_segment_properties ()
{
  assert_unicode ();

  /* If script is set to INVALID, guess from buffer contents */
  if (props.script == HB_SCRIPT_INVALID) {
    for (unsigned int i = 0; i < len; i++) {
      hb_script_t script = unicode->script (info[i].codepoint);
      if (likely (script != HB_SCRIPT_COMMON &&
		  script != HB_SCRIPT_INHERITED &&
		  script != HB_SCRIPT_UNKNOWN)) {
	props.script = script;
	break;
      }
    }
  }

  /* If direction is set to INVALID, guess from script */
  if (props.direction == HB_DIRECTION_INVALID) {
    props.direction = hb_script_get_horizontal_direction (props.script);
    if (props.direction == HB_DIRECTION_INVALID)
      props.direction = HB_DIRECTION_LTR;
  }

  /* If language is not set, use default language from locale */
  if (props.language == HB_LANGUAGE_INVALID) {
    /* TODO get_default_for_script? using $LANGUAGE */
    props.language = hb_language_get_default ();
  }
}


/* Public API */

DEFINE_NULL_INSTANCE (hb_buffer_t) =
{
  HB_OBJECT_HEADER_STATIC,

  const_cast<hb_unicode_funcs_t *> (&_hb_Null_hb_unicode_funcs_t),
  HB_BUFFER_FLAG_DEFAULT,
  HB_BUFFER_CLUSTER_LEVEL_DEFAULT,
  HB_BUFFER_REPLACEMENT_CODEPOINT_DEFAULT,
  0, /* invisible */
  0, /* not_found */
  HB_CODEPOINT_INVALID, /* not_found_variation_selector */


  HB_BUFFER_CONTENT_TYPE_INVALID,
  HB_SEGMENT_PROPERTIES_DEFAULT,

  false, /* successful */
  true, /* shaping_failed */
  false, /* have_output */
  true  /* have_positions */

  /* Zero is good enough for everything else. */
};


/**
 * hb_buffer_create:
 *
 * Creates a new #hb_buffer_t with all properties to defaults.
 *
 * Return value: (transfer full):
 * A newly allocated #hb_buffer_t with a reference count of 1. The initial
 * reference count should be released with hb_buffer_destroy() when you are done
 * using the #hb_buffer_t. This function never returns `NULL`. If memory cannot
 * be allocated, a special #hb_buffer_t object will be returned on which
 * hb_buffer_allocation_successful() returns `false`.
 *
 * Since: 0.9.2
 **/
hb_buffer_t *
hb_buffer_create ()
{
  hb_buffer_t *buffer;

  if (!(buffer = hb_object_create<hb_buffer_t> ()))
    return hb_buffer_get_empty ();

  buffer->max_len = HB_BUFFER_MAX_LEN_DEFAULT;
  buffer->max_ops = HB_BUFFER_MAX_OPS_DEFAULT;

  buffer->reset ();

  return buffer;
}

/**
 * hb_buffer_create_similar:
 * @src: An #hb_buffer_t
 *
 * Creates a new #hb_buffer_t, similar to hb_buffer_create(). The only
 * difference is that the buffer is configured similarly to @src.
 *
 * Return value: (transfer full):
 * A newly allocated #hb_buffer_t, similar to hb_buffer_create().
 *
 * Since: 3.3.0
 **/
hb_buffer_t *
hb_buffer_create_similar (const hb_buffer_t *src)
{
  hb_buffer_t *buffer = hb_buffer_create ();

  buffer->similar (*src);

  return buffer;
}

/**
 * hb_buffer_reset:
 * @buffer: An #hb_buffer_t
 *
 * Resets the buffer to its initial status, as if it was just newly created
 * with hb_buffer_create().
 *
 * Since: 0.9.2
 **/
void
hb_buffer_reset (hb_buffer_t *buffer)
{
  if (unlikely (hb_object_is_immutable (buffer)))
    return;

  buffer->reset ();
}

/**
 * hb_buffer_get_empty:
 *
 * Fetches an empty #hb_buffer_t.
 *
 * Return value: (transfer full): The empty buffer
 *
 * Since: 0.9.2
 **/
hb_buffer_t *
hb_buffer_get_empty ()
{
  return const_cast<hb_buffer_t *> (&Null (hb_buffer_t));
}

/**
 * hb_buffer_reference: (skip)
 * @buffer: An #hb_buffer_t
 *
 * Increases the reference count on @buffer by one. This prevents @buffer from
 * being destroyed until a matching call to hb_buffer_destroy() is made.
 *
 * Return value: (transfer full):
 * The referenced #hb_buffer_t.
 *
 * Since: 0.9.2
 **/
hb_buffer_t *
hb_buffer_reference (hb_buffer_t *buffer)
{
  return hb_object_reference (buffer);
}

/**
 * hb_buffer_destroy: (skip)
 * @buffer: An #hb_buffer_t
 *
 * Deallocate the @buffer.
 * Decreases the reference count on @buffer by one. If the result is zero, then
 * @buffer and all associated resources are freed. See hb_buffer_reference().
 *
 * Since: 0.9.2
 **/
void
hb_buffer_destroy (hb_buffer_t *buffer)
{
  if (!hb_object_destroy (buffer)) return;

  hb_unicode_funcs_destroy (buffer->unicode);

  hb_free (buffer->info);
  hb_free (buffer->pos);
#ifndef HB_NO_BUFFER_MESSAGE
  if (buffer->message_destroy)
    buffer->message_destroy (buffer->message_data);
#endif

  hb_free (buffer);
}

/**
 * hb_buffer_set_user_data: (skip)
 * @buffer: An #hb_buffer_t
 * @key: The user-data key
 * @data: A pointer to the user data
 * @destroy: (nullable): A callback to call when @data is not needed anymore
 * @replace: Whether to replace an existing data with the same key
 *
 * Attaches a user-data key/data pair to the specified buffer. 
 *
 * Return value: `true` if success, `false` otherwise
 *
 * Since: 0.9.2
 **/
hb_bool_t
hb_buffer_set_user_data (hb_buffer_t        *buffer,
			 hb_user_data_key_t *key,
			 void *              data,
			 hb_destroy_func_t   destroy,
			 hb_bool_t           replace)
{
  return hb_object_set_user_data (buffer, key, data, destroy, replace);
}

/**
 * hb_buffer_get_user_data: (skip)
 * @buffer: An #hb_buffer_t
 * @key: The user-data key to query
 *
 * Fetches the user data associated with the specified key,
 * attached to the specified buffer.
 *
 * Return value: (transfer none): A pointer to the user data
 *
 * Since: 0.9.2
 **/
void *
hb_buffer_get_user_data (const hb_buffer_t  *buffer,
			 hb_user_data_key_t *key)
{
  return hb_object_get_user_data (buffer, key);
}


/**
 * hb_buffer_set_content_type:
 * @buffer: An #hb_buffer_t
 * @content_type: The type of buffer contents to set
 *
 * Sets the type of @buffer contents. Buffers are either empty, contain
 * characters (before shaping), or contain glyphs (the result of shaping).
 *
 * You rarely need to call this function, since a number of other
 * functions transition the content type for you. Namely:
 *
 * - A newly created buffer starts with content type
 *   %HB_BUFFER_CONTENT_TYPE_INVALID. Calling hb_buffer_reset(),
 *   hb_buffer_clear_contents(), as well as calling hb_buffer_set_length()
 *   with an argument of zero all set the buffer content type to invalid
 *   as well.
 *
 * - Calling hb_buffer_add_utf8(), hb_buffer_add_utf16(),
 *   hb_buffer_add_utf32(), hb_buffer_add_codepoints() and
 *   hb_buffer_add_latin1() expect that buffer is either empty and
 *   have a content type of invalid, or that buffer content type is
 *   %HB_BUFFER_CONTENT_TYPE_UNICODE, and they also set the content
 *   type to Unicode if they added anything to an empty buffer.
 *
 * - Finally hb_shape() and hb_shape_full() expect that the buffer
 *   is either empty and have content type of invalid, or that buffer
 *   content type is %HB_BUFFER_CONTENT_TYPE_UNICODE, and upon
 *   success they set the buffer content type to
 *   %HB_BUFFER_CONTENT_TYPE_GLYPHS.
 *
 * The above transitions are designed such that one can use a buffer
 * in a loop of "reset : add-text : shape" without needing to ever
 * modify the content type manually.
 *
 * Since: 0.9.5
 **/
void
hb_buffer_set_content_type (hb_buffer_t              *buffer,
			    hb_buffer_content_type_t  content_type)
{
  buffer->content_type = content_type;
}

/**
 * hb_buffer_get_content_type:
 * @buffer: An #hb_buffer_t
 *
 * Fetches the type of @buffer contents. Buffers are either empty, contain
 * characters (before shaping), or contain glyphs (the result of shaping).
 *
 * Return value:
 * The type of @buffer contents
 *
 * Since: 0.9.5
 **/
hb_buffer_content_type_t
hb_buffer_get_content_type (const hb_buffer_t *buffer)
{
  return buffer->content_type;
}


/**
 * hb_buffer_set_unicode_funcs:
 * @buffer: An #hb_buffer_t
 * @unicode_funcs: The Unicode-functions structure
 *
 * Sets the Unicode-functions structure of a buffer to
 * @unicode_funcs.
 *
 * Since: 0.9.2
 **/
void
hb_buffer_set_unicode_funcs (hb_buffer_t        *buffer,
			     hb_unicode_funcs_t *unicode_funcs)
{
  if (unlikely (hb_object_is_immutable (buffer)))
    return;

  if (!unicode_funcs)
    unicode_funcs = hb_unicode_funcs_get_default ();

  hb_unicode_funcs_reference (unicode_funcs);
  hb_unicode_funcs_destroy (buffer->unicode);
  buffer->unicode = unicode_funcs;
}

/**
 * hb_buffer_get_unicode_funcs:
 * @buffer: An #hb_buffer_t
 *
 * Fetches the Unicode-functions structure of a buffer.
 *
 * Return value: The Unicode-functions structure
 *
 * Since: 0.9.2
 **/
hb_unicode_funcs_t *
hb_buffer_get_unicode_funcs (const hb_buffer_t *buffer)
{
  return buffer->unicode;
}

/**
 * hb_buffer_set_direction:
 * @buffer: An #hb_buffer_t
 * @direction: the #hb_direction_t of the @buffer
 *
 * Set the text flow direction of the buffer. No shaping can happen without
 * setting @buffer direction, and it controls the visual direction for the
 * output glyphs; for RTL direction the glyphs will be reversed. Many layout
 * features depend on the proper setting of the direction, for example,
 * reversing RTL text before shaping, then shaping with LTR direction is not
 * the same as keeping the text in logical order and shaping with RTL
 * direction.
 *
 * Since: 0.9.2
 **/
void
hb_buffer_set_direction (hb_buffer_t    *buffer,
			 hb_direction_t  direction)
{
  if (unlikely (hb_object_is_immutable (buffer)))
    return;

  buffer->props.direction = direction;
}

/**
 * hb_buffer_get_direction:
 * @buffer: An #hb_buffer_t
 *
 * See hb_buffer_set_direction()
 *
 * Return value:
 * The direction of the @buffer.
 *
 * Since: 0.9.2
 **/
hb_direction_t
hb_buffer_get_direction (const hb_buffer_t *buffer)
{
  return buffer->props.direction;
}

/**
 * hb_buffer_set_script:
 * @buffer: An #hb_buffer_t
 * @script: An #hb_script_t to set.
 *
 * Sets the script of @buffer to @script.
 *
 * Script is crucial for choosing the proper shaping behaviour for scripts that
 * require it (e.g. Arabic) and the which OpenType features defined in the font
 * to be applied.
 *
 * You can pass one of the predefined #hb_script_t values, or use
 * hb_script_from_string() or hb_script_from_iso15924_tag() to get the
 * corresponding script from an ISO 15924 script tag.
 *
 * Since: 0.9.2
 **/
void
hb_buffer_set_script (hb_buffer_t *buffer,
		      hb_script_t  script)
{
  if (unlikely (hb_object_is_immutable (buffer)))
    return;

  buffer->props.script = script;
}

/**
 * hb_buffer_get_script:
 * @buffer: An #hb_buffer_t
 *
 * Fetches the script of @buffer.
 *
 * Return value:
 * The #hb_script_t of the @buffer
 *
 * Since: 0.9.2
 **/
hb_script_t
hb_buffer_get_script (const hb_buffer_t *buffer)
{
  return buffer->props.script;
}

/**
 * hb_buffer_set_language:
 * @buffer: An #hb_buffer_t
 * @language: An hb_language_t to set
 *
 * Sets the language of @buffer to @language.
 *
 * Languages are crucial for selecting which OpenType feature to apply to the
 * buffer which can result in applying language-specific behaviour. Languages
 * are orthogonal to the scripts, and though they are related, they are
 * different concepts and should not be confused with each other.
 *
 * Use hb_language_from_string() to convert from BCP 47 language tags to
 * #hb_language_t.
 *
 * Since: 0.9.2
 **/
void
hb_buffer_set_language (hb_buffer_t   *buffer,
			hb_language_t  language)
{
  if (unlikely (hb_object_is_immutable (buffer)))
    return;

  buffer->props.language = language;
}

/**
 * hb_buffer_get_language:
 * @buffer: An #hb_buffer_t
 *
 * See hb_buffer_set_language().
 *
 * Return value: (transfer none):
 * The #hb_language_t of the buffer. Must not be freed by the caller.
 *
 * Since: 0.9.2
 **/
hb_language_t
hb_buffer_get_language (const hb_buffer_t *buffer)
{
  return buffer->props.language;
}

/**
 * hb_buffer_set_segment_properties:
 * @buffer: An #hb_buffer_t
 * @props: An #hb_segment_properties_t to use
 *
 * Sets the segment properties of the buffer, a shortcut for calling
 * hb_buffer_set_direction(), hb_buffer_set_script() and
 * hb_buffer_set_language() individually.
 *
 * Since: 0.9.7
 **/
void
hb_buffer_set_segment_properties (hb_buffer_t *buffer,
				  const hb_segment_properties_t *props)
{
  if (unlikely (hb_object_is_immutable (buffer)))
    return;

  buffer->props = *props;
}

/**
 * hb_buffer_get_segment_properties:
 * @buffer: An #hb_buffer_t
 * @props: (out): The output #hb_segment_properties_t
 *
 * Sets @props to the #hb_segment_properties_t of @buffer.
 *
 * Since: 0.9.7
 **/
void
hb_buffer_get_segment_properties (const hb_buffer_t *buffer,
				  hb_segment_properties_t *props)
{
  *props = buffer->props;
}


/**
 * hb_buffer_set_flags:
 * @buffer: An #hb_buffer_t
 * @flags: The buffer flags to set
 *
 * Sets @buffer flags to @flags. See #hb_buffer_flags_t.
 *
 * Since: 0.9.7
 **/
void
hb_buffer_set_flags (hb_buffer_t       *buffer,
		     hb_buffer_flags_t  flags)
{
  if (unlikely (hb_object_is_immutable (buffer)))
    return;

  buffer->flags = flags;
}

/**
 * hb_buffer_get_flags:
 * @buffer: An #hb_buffer_t
 *
 * Fetches the #hb_buffer_flags_t of @buffer.
 *
 * Return value:
 * The @buffer flags
 *
 * Since: 0.9.7
 **/
hb_buffer_flags_t
hb_buffer_get_flags (const hb_buffer_t *buffer)
{
  return buffer->flags;
}

/**
 * hb_buffer_set_cluster_level:
 * @buffer: An #hb_buffer_t
 * @cluster_level: The cluster level to set on the buffer
 *
 * Sets the cluster level of a buffer. The #hb_buffer_cluster_level_t
 * dictates one aspect of how HarfBuzz will treat non-base characters 
 * during shaping.
 *
 * Since: 0.9.42
 **/
void
hb_buffer_set_cluster_level (hb_buffer_t               *buffer,
			     hb_buffer_cluster_level_t  cluster_level)
{
  if (unlikely (hb_object_is_immutable (buffer)))
    return;

  buffer->cluster_level = cluster_level;
}

/**
 * hb_buffer_get_cluster_level:
 * @buffer: An #hb_buffer_t
 *
 * Fetches the cluster level of a buffer. The #hb_buffer_cluster_level_t
 * dictates one aspect of how HarfBuzz will treat non-base characters 
 * during shaping.
 *
 * Return value: The cluster level of @buffer
 *
 * Since: 0.9.42
 **/
hb_buffer_cluster_level_t
hb_buffer_get_cluster_level (const hb_buffer_t *buffer)
{
  return buffer->cluster_level;
}


/**
 * hb_buffer_set_replacement_codepoint:
 * @buffer: An #hb_buffer_t
 * @replacement: the replacement #hb_codepoint_t
 *
 * Sets the #hb_codepoint_t that replaces invalid entries for a given encoding
 * when adding text to @buffer.
 *
 * Default is #HB_BUFFER_REPLACEMENT_CODEPOINT_DEFAULT.
 *
 * Since: 0.9.31
 **/
void
hb_buffer_set_replacement_codepoint (hb_buffer_t    *buffer,
				     hb_codepoint_t  replacement)
{
  if (unlikely (hb_object_is_immutable (buffer)))
    return;

  buffer->replacement = replacement;
}

/**
 * hb_buffer_get_replacement_codepoint:
 * @buffer: An #hb_buffer_t
 *
 * Fetches the #hb_codepoint_t that replaces invalid entries for a given encoding
 * when adding text to @buffer.
 *
 * Return value:
 * The @buffer replacement #hb_codepoint_t
 *
 * Since: 0.9.31
 **/
hb_codepoint_t
hb_buffer_get_replacement_codepoint (const hb_buffer_t *buffer)
{
  return buffer->replacement;
}


/**
 * hb_buffer_set_invisible_glyph:
 * @buffer: An #hb_buffer_t
 * @invisible: the invisible #hb_codepoint_t
 *
 * Sets the #hb_codepoint_t that replaces invisible characters in
 * the shaping result.  If set to zero (default), the glyph for the
 * U+0020 SPACE character is used.  Otherwise, this value is used
 * verbatim.
 *
 * Since: 2.0.0
 **/
void
hb_buffer_set_invisible_glyph (hb_buffer_t    *buffer,
			       hb_codepoint_t  invisible)
{
  if (unlikely (hb_object_is_immutable (buffer)))
    return;

  buffer->invisible = invisible;
}

/**
 * hb_buffer_get_invisible_glyph:
 * @buffer: An #hb_buffer_t
 *
 * See hb_buffer_set_invisible_glyph().
 *
 * Return value:
 * The @buffer invisible #hb_codepoint_t
 *
 * Since: 2.0.0
 **/
hb_codepoint_t
hb_buffer_get_invisible_glyph (const hb_buffer_t *buffer)
{
  return buffer->invisible;
}

/**
 * hb_buffer_set_not_found_glyph:
 * @buffer: An #hb_buffer_t
 * @not_found: the not-found #hb_codepoint_t
 *
 * Sets the #hb_codepoint_t that replaces characters not found in
 * the font during shaping.
 *
 * The not-found glyph defaults to zero, sometimes known as the
 * ".notdef" glyph.  This API allows for differentiating the two.
 *
 * Since: 3.1.0
 **/
void
hb_buffer_set_not_found_glyph (hb_buffer_t    *buffer,
			       hb_codepoint_t  not_found)
{
  if (unlikely (hb_object_is_immutable (buffer)))
    return;

  buffer->not_found = not_found;
}

/**
 * hb_buffer_get_not_found_glyph:
 * @buffer: An #hb_buffer_t
 *
 * See hb_buffer_set_not_found_glyph().
 *
 * Return value:
 * The @buffer not-found #hb_codepoint_t
 *
 * Since: 3.1.0
 **/
hb_codepoint_t
hb_buffer_get_not_found_glyph (const hb_buffer_t *buffer)
{
  return buffer->not_found;
}

/**
 * hb_buffer_set_not_found_variation_selector_glyph:
 * @buffer: An #hb_buffer_t
 * @not_found_variation_selector: the not-found-variation-selector #hb_codepoint_t
 *
 * Sets the #hb_codepoint_t that replaces variation-selector characters not resolved
 * in the font during shaping.
 *
 * The not-found-variation-selector glyph defaults to #HB_CODEPOINT_INVALID,
 * in which case an unresolved variation-selector will be removed from the glyph
 * string during shaping. This API allows for changing that and retaining a glyph,
 * such that the situation can be detected by the client and handled accordingly
 * (e.g. by using a different font).
 *
 * Since: 10.0.0
 **/
void
hb_buffer_set_not_found_variation_selector_glyph (hb_buffer_t    *buffer,
						  hb_codepoint_t  not_found_variation_selector)
{
  buffer->not_found_variation_selector = not_found_variation_selector;
}

/**
 * hb_buffer_get_not_found_variation_selector_glyph:
 * @buffer: An #hb_buffer_t
 *
 * See hb_buffer_set_not_found_variation_selector_glyph().
 *
 * Return value:
 * The @buffer not-found-variation-selector #hb_codepoint_t
 *
 * Since: 10.0.0
 **/
hb_codepoint_t
hb_buffer_get_not_found_variation_selector_glyph (const hb_buffer_t *buffer)
{
  return buffer->not_found_variation_selector;
}

/**
 * hb_buffer_set_random_state:
 * @buffer: An #hb_buffer_t
 * @state: the new random state
 *
 * Sets the random state of the buffer. The state changes
 * every time a glyph uses randomness (eg. the `rand`
 * OpenType feature). This function together with
 * hb_buffer_get_random_state() allow for transferring
 * the current random state to a subsequent buffer, to
 * get better randomness distribution.
 *
 * Defaults to 1 and when buffer contents are cleared.
 * A value of 0 disables randomness during shaping.
 *
 * Since: 8.4.0
 **/
void
hb_buffer_set_random_state (hb_buffer_t    *buffer,
			    unsigned        state)
{
  if (unlikely (hb_object_is_immutable (buffer)))
    return;

  buffer->random_state = state;
}

/**
 * hb_buffer_get_random_state:
 * @buffer: An #hb_buffer_t
 *
 * See hb_buffer_set_random_state().
 *
 * Return value:
 * The @buffer random state
 *
 * Since: 8.4.0
 **/
unsigned
hb_buffer_get_random_state (const hb_buffer_t *buffer)
{
  return buffer->random_state;
}

/**
 * hb_buffer_clear_contents:
 * @buffer: An #hb_buffer_t
 *
 * Similar to hb_buffer_reset(), but does not clear the Unicode functions and
 * the replacement code point.
 *
 * Since: 0.9.11
 **/
void
hb_buffer_clear_contents (hb_buffer_t *buffer)
{
  if (unlikely (hb_object_is_immutable (buffer)))
    return;

  buffer->clear ();
}

/**
 * hb_buffer_pre_allocate:
 * @buffer: An #hb_buffer_t
 * @size: Number of items to pre allocate.
 *
 * Pre allocates memory for @buffer to fit at least @size number of items.
 *
 * Return value:
 * `true` if @buffer memory allocation succeeded, `false` otherwise
 *
 * Since: 0.9.2
 **/
hb_bool_t
hb_buffer_pre_allocate (hb_buffer_t *buffer, unsigned int size)
{
  return buffer->ensure (size);
}

/**
 * hb_buffer_allocation_successful:
 * @buffer: An #hb_buffer_t
 *
 * Check if allocating memory for the buffer succeeded.
 *
 * Return value:
 * `true` if @buffer memory allocation succeeded, `false` otherwise.
 *
 * Since: 0.9.2
 **/
hb_bool_t
hb_buffer_allocation_successful (hb_buffer_t  *buffer)
{
  return buffer->successful;
}

/**
 * hb_buffer_add:
 * @buffer: An #hb_buffer_t
 * @codepoint: A Unicode code point.
 * @cluster: The cluster value of @codepoint.
 *
 * Appends a character with the Unicode value of @codepoint to @buffer, and
 * gives it the initial cluster value of @cluster. Clusters can be any thing
 * the client wants, they are usually used to refer to the index of the
 * character in the input text stream and are output in
 * #hb_glyph_info_t.cluster field.
 *
 * This function does not check the validity of @codepoint, it is up to the
 * caller to ensure it is a valid Unicode code point.
 *
 * Since: 0.9.7
 **/
void
hb_buffer_add (hb_buffer_t    *buffer,
	       hb_codepoint_t  codepoint,
	       unsigned int    cluster)
{
  buffer->add (codepoint, cluster);
  buffer->clear_context (1);
}

/**
 * hb_buffer_set_length:
 * @buffer: An #hb_buffer_t
 * @length: The new length of @buffer
 *
 * Similar to hb_buffer_pre_allocate(), but clears any new items added at the
 * end.
 *
 * Return value:
 * `true` if @buffer memory allocation succeeded, `false` otherwise.
 *
 * Since: 0.9.2
 **/
hb_bool_t
hb_buffer_set_length (hb_buffer_t  *buffer,
		      unsigned int  length)
{
  if (unlikely (hb_object_is_immutable (buffer)))
    return length == 0;

  if (unlikely (!buffer->ensure (length)))
    return false;

  /* Wipe the new space */
  if (length > buffer->len) {
    hb_memset (buffer->info + buffer->len, 0, sizeof (buffer->info[0]) * (length - buffer->len));
    if (buffer->have_positions)
      hb_memset (buffer->pos + buffer->len, 0, sizeof (buffer->pos[0]) * (length - buffer->len));
  }

  buffer->len = length;

  if (!length)
  {
    buffer->content_type = HB_BUFFER_CONTENT_TYPE_INVALID;
    buffer->clear_context (0);
  }
  buffer->clear_context (1);

  return true;
}

/**
 * hb_buffer_get_length:
 * @buffer: An #hb_buffer_t
 *
 * Returns the number of items in the buffer.
 *
 * Return value:
 * The @buffer length.
 * The value valid as long as buffer has not been modified.
 *
 * Since: 0.9.2
 **/
unsigned int
hb_buffer_get_length (const hb_buffer_t *buffer)
{
  return buffer->len;
}

/**
 * hb_buffer_get_glyph_infos:
 * @buffer: An #hb_buffer_t
 * @length: (out): The output-array length.
 *
 * Returns @buffer glyph information array.  Returned pointer
 * is valid as long as @buffer contents are not modified.
 *
 * Return value: (transfer none) (array length=length):
 * The @buffer glyph information array.
 * The value valid as long as buffer has not been modified.
 *
 * Since: 0.9.2
 **/
hb_glyph_info_t *
hb_buffer_get_glyph_infos (hb_buffer_t  *buffer,
			   unsigned int *length)
{
  if (length)
    *length = buffer->len;

  return (hb_glyph_info_t *) buffer->info;
}

/**
 * hb_buffer_get_glyph_positions:
 * @buffer: An #hb_buffer_t
 * @length: (out): The output length
 *
 * Returns @buffer glyph position array.  Returned pointer
 * is valid as long as @buffer contents are not modified.
 *
 * If buffer did not have positions before, the positions will be
 * initialized to zeros, unless this function is called from
 * within a buffer message callback (see hb_buffer_set_message_func()),
 * in which case `NULL` is returned.
 *
 * Return value: (transfer none) (array length=length):
 * The @buffer glyph position array.
 * The value valid as long as buffer has not been modified.
 *
 * Since: 0.9.2
 **/
hb_glyph_position_t *
hb_buffer_get_glyph_positions (hb_buffer_t  *buffer,
			       unsigned int *length)
{
  if (length)
    *length = buffer->len;

  if (!buffer->have_positions)
  {
    if (unlikely (buffer->message_depth))
      return nullptr;

    buffer->clear_positions ();
  }

  return (hb_glyph_position_t *) buffer->pos;
}

/**
 * hb_buffer_has_positions:
 * @buffer: an #hb_buffer_t.
 *
 * Returns whether @buffer has glyph position data.
 * A buffer gains position data when hb_buffer_get_glyph_positions() is called on it,
 * and cleared of position data when hb_buffer_clear_contents() is called.
 *
 * Return value:
 * `true` if the @buffer has position array, `false` otherwise.
 *
 * Since: 2.7.3
 **/
HB_EXTERN hb_bool_t
hb_buffer_has_positions (hb_buffer_t  *buffer)
{
  return buffer->have_positions;
}

/**
 * hb_glyph_info_get_glyph_flags:
 * @info: a #hb_glyph_info_t
 *
 * Returns glyph flags encoded within a #hb_glyph_info_t.
 *
 * Return value:
 * The #hb_glyph_flags_t encoded within @info
 *
 * Since: 1.5.0
 **/
hb_glyph_flags_t
(hb_glyph_info_get_glyph_flags) (const hb_glyph_info_t *info)
{
  return hb_glyph_info_get_glyph_flags (info);
}

/**
 * hb_buffer_reverse:
 * @buffer: An #hb_buffer_t
 *
 * Reverses buffer contents.
 *
 * Since: 0.9.2
 **/
void
hb_buffer_reverse (hb_buffer_t *buffer)
{
  buffer->reverse ();
}

/**
 * hb_buffer_reverse_range:
 * @buffer: An #hb_buffer_t
 * @start: start index
 * @end: end index
 *
 * Reverses buffer contents between @start and @end.
 *
 * Since: 0.9.41
 **/
void
hb_buffer_reverse_range (hb_buffer_t *buffer,
			 unsigned int start, unsigned int end)
{
  buffer->reverse_range (start, end);
}

/**
 * hb_buffer_reverse_clusters:
 * @buffer: An #hb_buffer_t
 *
 * Reverses buffer clusters.  That is, the buffer contents are
 * reversed, then each cluster (consecutive items having the
 * same cluster number) are reversed again.
 *
 * Since: 0.9.2
 **/
void
hb_buffer_reverse_clusters (hb_buffer_t *buffer)
{
  buffer->reverse_clusters ();
}

/**
 * hb_buffer_guess_segment_properties:
 * @buffer: An #hb_buffer_t
 *
 * Sets unset buffer segment properties based on buffer Unicode
 * contents.  If buffer is not empty, it must have content type
 * #HB_BUFFER_CONTENT_TYPE_UNICODE.
 *
 * If buffer script is not set (ie. is #HB_SCRIPT_INVALID), it
 * will be set to the Unicode script of the first character in
 * the buffer that has a script other than #HB_SCRIPT_COMMON,
 * #HB_SCRIPT_INHERITED, and #HB_SCRIPT_UNKNOWN.
 *
 * Next, if buffer direction is not set (ie. is #HB_DIRECTION_INVALID),
 * it will be set to the natural horizontal direction of the
 * buffer script as returned by hb_script_get_horizontal_direction().
 * If hb_script_get_horizontal_direction() returns #HB_DIRECTION_INVALID,
 * then #HB_DIRECTION_LTR is used.
 *
 * Finally, if buffer language is not set (ie. is #HB_LANGUAGE_INVALID),
 * it will be set to the process's default language as returned by
 * hb_language_get_default().  This may change in the future by
 * taking buffer script into consideration when choosing a language.
 * Note that hb_language_get_default() is NOT threadsafe the first time
 * it is called.  See documentation for that function for details.
 *
 * Since: 0.9.7
 **/
void
hb_buffer_guess_segment_properties (hb_buffer_t *buffer)
{
  buffer->guess_segment_properties ();
}

template <typename utf_t>
static inline void
hb_buffer_add_utf (hb_buffer_t  *buffer,
		   const typename utf_t::codepoint_t *text,
		   int           text_length,
		   unsigned int  item_offset,
		   int           item_length)
{
  typedef typename utf_t::codepoint_t T;
  const hb_codepoint_t replacement = buffer->replacement;

  buffer->assert_unicode ();

  if (unlikely (hb_object_is_immutable (buffer)))
    return;

  if (text_length == -1)
    text_length = utf_t::strlen (text);

  if (item_length == -1)
    item_length = text_length - item_offset;

  if (unlikely (item_length < 0 ||
		item_length > INT_MAX / 8 ||
		!buffer->ensure (buffer->len + item_length * sizeof (T) / 4)))
    return;

  /* If buffer is empty and pre-context provided, install it.
   * This check is written this way, to make sure people can
   * provide pre-context in one add_utf() call, then provide
   * text in a follow-up call.  See:
   *
   * https://bugzilla.mozilla.org/show_bug.cgi?id=801410#c13
   */
  if (!buffer->len && item_offset > 0)
  {
    /* Add pre-context */
    buffer->clear_context (0);
    const T *prev = text + item_offset;
    const T *start = text;
    while (start < prev && buffer->context_len[0] < buffer->CONTEXT_LENGTH)
    {
      hb_codepoint_t u;
      prev = utf_t::prev (prev, start, &u, replacement);
      buffer->context[0][buffer->context_len[0]++] = u;
    }
  }

  const T *next = text + item_offset;
  const T *end = next + item_length;
  while (next < end)
  {
    hb_codepoint_t u;
    const T *old_next = next;
    next = utf_t::next (next, end, &u, replacement);
    buffer->add (u, old_next - (const T *) text);
  }

  /* Add post-context */
  buffer->clear_context (1);
  end = text + text_length;
  while (next < end && buffer->context_len[1] < buffer->CONTEXT_LENGTH)
  {
    hb_codepoint_t u;
    next = utf_t::next (next, end, &u, replacement);
    buffer->context[1][buffer->context_len[1]++] = u;
  }

  buffer->content_type = HB_BUFFER_CONTENT_TYPE_UNICODE;
}

/**
 * hb_buffer_add_utf8:
 * @buffer: An #hb_buffer_t
 * @text: (array length=text_length) (element-type uint8_t): An array of UTF-8
 *               characters to append.
 * @text_length: The length of the @text, or -1 if it is `NULL` terminated.
 * @item_offset: The offset of the first character to add to the @buffer.
 * @item_length: The number of characters to add to the @buffer, or -1 for the
 *               end of @text (assuming it is `NULL` terminated).
 *
 * See hb_buffer_add_codepoints().
 *
 * Replaces invalid UTF-8 characters with the @buffer replacement code point,
 * see hb_buffer_set_replacement_codepoint().
 *
 * Since: 0.9.2
 **/
void
hb_buffer_add_utf8 (hb_buffer_t  *buffer,
		    const char   *text,
		    int           text_length,
		    unsigned int  item_offset,
		    int           item_length)
{
  hb_buffer_add_utf<hb_utf8_t> (buffer, (const uint8_t *) text, text_length, item_offset, item_length);
}

/**
 * hb_buffer_add_utf16:
 * @buffer: An #hb_buffer_t
 * @text: (array length=text_length): An array of UTF-16 characters to append
 * @text_length: The length of the @text, or -1 if it is `NULL` terminated
 * @item_offset: The offset of the first character to add to the @buffer
 * @item_length: The number of characters to add to the @buffer, or -1 for the
 *               end of @text (assuming it is `NULL` terminated)
 *
 * See hb_buffer_add_codepoints().
 *
 * Replaces invalid UTF-16 characters with the @buffer replacement code point,
 * see hb_buffer_set_replacement_codepoint().
 *
 * Since: 0.9.2
 **/
void
hb_buffer_add_utf16 (hb_buffer_t    *buffer,
		     const uint16_t *text,
		     int             text_length,
		     unsigned int    item_offset,
		     int             item_length)
{
  hb_buffer_add_utf<hb_utf16_t> (buffer, text, text_length, item_offset, item_length);
}

/**
 * hb_buffer_add_utf32:
 * @buffer: An #hb_buffer_t
 * @text: (array length=text_length): An array of UTF-32 characters to append
 * @text_length: The length of the @text, or -1 if it is `NULL` terminated
 * @item_offset: The offset of the first character to add to the @buffer
 * @item_length: The number of characters to add to the @buffer, or -1 for the
 *               end of @text (assuming it is `NULL` terminated)
 *
 * See hb_buffer_add_codepoints().
 *
 * Replaces invalid UTF-32 characters with the @buffer replacement code point,
 * see hb_buffer_set_replacement_codepoint().
 *
 * Since: 0.9.2
 **/
void
hb_buffer_add_utf32 (hb_buffer_t    *buffer,
		     const uint32_t *text,
		     int             text_length,
		     unsigned int    item_offset,
		     int             item_length)
{
  hb_buffer_add_utf<hb_utf32_t> (buffer, text, text_length, item_offset, item_length);
}

/**
 * hb_buffer_add_latin1:
 * @buffer: An #hb_buffer_t
 * @text: (array length=text_length) (element-type uint8_t): an array of UTF-8
 *               characters to append
 * @text_length: the length of the @text, or -1 if it is `NULL` terminated
 * @item_offset: the offset of the first character to add to the @buffer
 * @item_length: the number of characters to add to the @buffer, or -1 for the
 *               end of @text (assuming it is `NULL` terminated)
 *
 * Similar to hb_buffer_add_codepoints(), but allows only access to first 256
 * Unicode code points that can fit in 8-bit strings.
 *
 * <note>Has nothing to do with non-Unicode Latin-1 encoding.</note>
 *
 * Since: 0.9.39
 **/
void
hb_buffer_add_latin1 (hb_buffer_t   *buffer,
		      const uint8_t *text,
		      int            text_length,
		      unsigned int   item_offset,
		      int            item_length)
{
  hb_buffer_add_utf<hb_latin1_t> (buffer, text, text_length, item_offset, item_length);
}

/**
 * hb_buffer_add_codepoints:
 * @buffer: a #hb_buffer_t to append characters to.
 * @text: (array length=text_length): an array of Unicode code points to append.
 * @text_length: the length of the @text, or -1 if it is `NULL` terminated.
 * @item_offset: the offset of the first code point to add to the @buffer.
 * @item_length: the number of code points to add to the @buffer, or -1 for the
 *               end of @text (assuming it is `NULL` terminated).
 *
 * Appends characters from @text array to @buffer. The @item_offset is the
 * position of the first character from @text that will be appended, and
 * @item_length is the number of character. When shaping part of a larger text
 * (e.g. a run of text from a paragraph), instead of passing just the substring
 * corresponding to the run, it is preferable to pass the whole
 * paragraph and specify the run start and length as @item_offset and
 * @item_length, respectively, to give HarfBuzz the full context to be able,
 * for example, to do cross-run Arabic shaping or properly handle combining
 * marks at stat of run.
 *
 * This function does not check the validity of @text, it is up to the caller
 * to ensure it contains a valid Unicode scalar values.  In contrast,
 * hb_buffer_add_utf32() can be used that takes similar input but performs
 * sanity-check on the input.
 *
 * Since: 0.9.31
 **/
void
hb_buffer_add_codepoints (hb_buffer_t          *buffer,
			  const hb_codepoint_t *text,
			  int                   text_length,
			  unsigned int          item_offset,
			  int                   item_length)
{
  hb_buffer_add_utf<hb_utf32_novalidate_t> (buffer, text, text_length, item_offset, item_length);
}


/**
 * hb_buffer_append:
 * @buffer: An #hb_buffer_t
 * @source: source #hb_buffer_t
 * @start: start index into source buffer to copy.  Use 0 to copy from start of buffer.
 * @end: end index into source buffer to copy.  Use @HB_FEATURE_GLOBAL_END to copy to end of buffer.
 *
 * Append (part of) contents of another buffer to this buffer.
 *
 * Since: 1.5.0
 **/
HB_EXTERN void
hb_buffer_append (hb_buffer_t *buffer,
		  const hb_buffer_t *source,
		  unsigned int start,
		  unsigned int end)
{
  assert (!buffer->have_output && !source->have_output);
  assert (buffer->have_positions == source->have_positions ||
	  !buffer->len || !source->len);
  assert (buffer->content_type == source->content_type ||
	  !buffer->len || !source->len);

  if (end > source->len)
    end = source->len;
  if (start > end)
    start = end;
  if (start == end)
    return;

  if (buffer->len + (end - start) < buffer->len) /* Overflows. */
  {
    buffer->successful = false;
    return;
  }

  unsigned int orig_len = buffer->len;
  hb_buffer_set_length (buffer, buffer->len + (end - start));
  if (unlikely (!buffer->successful))
    return;

  if (!orig_len)
    buffer->content_type = source->content_type;
  if (!buffer->have_positions && source->have_positions)
    buffer->clear_positions ();

  hb_segment_properties_overlay (&buffer->props, &source->props);

  hb_memcpy (buffer->info + orig_len, source->info + start, (end - start) * sizeof (buffer->info[0]));
  if (buffer->have_positions)
    hb_memcpy (buffer->pos + orig_len, source->pos + start, (end - start) * sizeof (buffer->pos[0]));

  if (source->content_type == HB_BUFFER_CONTENT_TYPE_UNICODE)
  {
    /* See similar logic in add_utf. */

    /* pre-context */
    if (!orig_len && start + source->context_len[0] > 0)
    {
      buffer->clear_context (0);
      while (start > 0 && buffer->context_len[0] < buffer->CONTEXT_LENGTH)
	buffer->context[0][buffer->context_len[0]++] = source->info[--start].codepoint;
      for (auto i = 0u; i < source->context_len[0] && buffer->context_len[0] < buffer->CONTEXT_LENGTH; i++)
	buffer->context[0][buffer->context_len[0]++] = source->context[0][i];
    }

    /* post-context */
    buffer->clear_context (1);
    while (end < source->len && buffer->context_len[1] < buffer->CONTEXT_LENGTH)
      buffer->context[1][buffer->context_len[1]++] = source->info[end++].codepoint;
    for (auto i = 0u; i < source->context_len[1] && buffer->context_len[1] < buffer->CONTEXT_LENGTH; i++)
      buffer->context[1][buffer->context_len[1]++] = source->context[1][i];
  }
}


static int
compare_info_codepoint (const hb_glyph_info_t *pa,
			const hb_glyph_info_t *pb)
{
  return (int) pb->codepoint - (int) pa->codepoint;
}

static inline void
normalize_glyphs_cluster (hb_buffer_t *buffer,
			  unsigned int start,
			  unsigned int end,
			  bool backward)
{
  hb_glyph_position_t *pos = buffer->pos;

  /* Total cluster advance */
  hb_position_t total_x_advance = 0, total_y_advance = 0;
  for (unsigned int i = start; i < end; i++)
  {
    total_x_advance += pos[i].x_advance;
    total_y_advance += pos[i].y_advance;
  }

  hb_position_t x_advance = 0, y_advance = 0;
  for (unsigned int i = start; i < end; i++)
  {
    pos[i].x_offset += x_advance;
    pos[i].y_offset += y_advance;

    x_advance += pos[i].x_advance;
    y_advance += pos[i].y_advance;

    pos[i].x_advance = 0;
    pos[i].y_advance = 0;
  }

  if (backward)
  {
    /* Transfer all cluster advance to the last glyph. */
    pos[end - 1].x_advance = total_x_advance;
    pos[end - 1].y_advance = total_y_advance;

    hb_stable_sort (buffer->info + start, end - start - 1, compare_info_codepoint, buffer->pos + start);
  } else {
    /* Transfer all cluster advance to the first glyph. */
    pos[start].x_advance += total_x_advance;
    pos[start].y_advance += total_y_advance;
    for (unsigned int i = start + 1; i < end; i++) {
      pos[i].x_offset -= total_x_advance;
      pos[i].y_offset -= total_y_advance;
    }
    hb_stable_sort (buffer->info + start + 1, end - start - 1, compare_info_codepoint, buffer->pos + start + 1);
  }
}

/**
 * hb_buffer_normalize_glyphs:
 * @buffer: An #hb_buffer_t
 *
 * Reorders a glyph buffer to have canonical in-cluster glyph order / position.
 * The resulting clusters should behave identical to pre-reordering clusters.
 *
 * <note>This has nothing to do with Unicode normalization.</note>
 *
 * Since: 0.9.2
 **/
void
hb_buffer_normalize_glyphs (hb_buffer_t *buffer)
{
  assert (buffer->have_positions);

  buffer->assert_glyphs ();

  bool backward = HB_DIRECTION_IS_BACKWARD (buffer->props.direction);

  foreach_cluster (buffer, start, end)
    normalize_glyphs_cluster (buffer, start, end, backward);
}

void
hb_buffer_t::sort (unsigned int start, unsigned int end, int(*compar)(const hb_glyph_info_t *, const hb_glyph_info_t *))
{
  assert (!have_positions);
  for (unsigned int i = start + 1; i < end; i++)
  {
    unsigned int j = i;
    while (j > start && compar (&info[j - 1], &info[i]) > 0)
      j--;
    if (i == j)
      continue;
    /* Move item i to occupy place for item j, shift what's in between. */
    merge_clusters (j, i + 1);
    {
      hb_glyph_info_t t = info[i];
      memmove (&info[j + 1], &info[j], (i - j) * sizeof (hb_glyph_info_t));
      info[j] = t;
    }
  }
}


/*
 * Comparing buffers.
 */

/**
 * hb_buffer_diff:
 * @buffer: a buffer.
 * @reference: other buffer to compare to.
 * @dottedcircle_glyph: glyph id of U+25CC DOTTED CIRCLE, or (hb_codepoint_t) -1.
 * @position_fuzz: allowed absolute difference in position values.
 *
 * If dottedcircle_glyph is (hb_codepoint_t) -1 then #HB_BUFFER_DIFF_FLAG_DOTTED_CIRCLE_PRESENT
 * and #HB_BUFFER_DIFF_FLAG_NOTDEF_PRESENT are never returned.  This should be used by most
 * callers if just comparing two buffers is needed.
 *
 * Since: 1.5.0
 **/
hb_buffer_diff_flags_t
hb_buffer_diff (hb_buffer_t *buffer,
		hb_buffer_t *reference,
		hb_codepoint_t dottedcircle_glyph,
		unsigned int position_fuzz)
{
  if (buffer->content_type != reference->content_type && buffer->len && reference->len)
    return HB_BUFFER_DIFF_FLAG_CONTENT_TYPE_MISMATCH;

  hb_buffer_diff_flags_t result = HB_BUFFER_DIFF_FLAG_EQUAL;
  bool contains = dottedcircle_glyph != (hb_codepoint_t) -1;

  unsigned int count = reference->len;

  if (buffer->len != count)
  {
    /*
     * we can't compare glyph-by-glyph, but we do want to know if there
     * are .notdef or dottedcircle glyphs present in the reference buffer
     */
    const hb_glyph_info_t *info = reference->info;
    unsigned int i;
    for (i = 0; i < count; i++)
    {
      if (contains && info[i].codepoint == dottedcircle_glyph)
	result |= HB_BUFFER_DIFF_FLAG_DOTTED_CIRCLE_PRESENT;
      if (contains && info[i].codepoint == 0)
	result |= HB_BUFFER_DIFF_FLAG_NOTDEF_PRESENT;
    }
    result |= HB_BUFFER_DIFF_FLAG_LENGTH_MISMATCH;
    return hb_buffer_diff_flags_t (result);
  }

  if (!count)
    return hb_buffer_diff_flags_t (result);

  const hb_glyph_info_t *buf_info = buffer->info;
  const hb_glyph_info_t *ref_info = reference->info;
  for (unsigned int i = 0; i < count; i++)
  {
    if (buf_info->codepoint != ref_info->codepoint)
      result |= HB_BUFFER_DIFF_FLAG_CODEPOINT_MISMATCH;
    if (buf_info->cluster != ref_info->cluster)
      result |= HB_BUFFER_DIFF_FLAG_CLUSTER_MISMATCH;
    if ((buf_info->mask ^ ref_info->mask) & HB_GLYPH_FLAG_DEFINED)
      result |= HB_BUFFER_DIFF_FLAG_GLYPH_FLAGS_MISMATCH;
    if (contains && ref_info->codepoint == dottedcircle_glyph)
      result |= HB_BUFFER_DIFF_FLAG_DOTTED_CIRCLE_PRESENT;
    if (contains && ref_info->codepoint == 0)
      result |= HB_BUFFER_DIFF_FLAG_NOTDEF_PRESENT;
    buf_info++;
    ref_info++;
  }

  if (buffer->content_type == HB_BUFFER_CONTENT_TYPE_GLYPHS)
  {
    assert (buffer->have_positions);
    const hb_glyph_position_t *buf_pos = buffer->pos;
    const hb_glyph_position_t *ref_pos = reference->pos;
    for (unsigned int i = 0; i < count; i++)
    {
      if ((unsigned int) abs (buf_pos->x_advance - ref_pos->x_advance) > position_fuzz ||
	  (unsigned int) abs (buf_pos->y_advance - ref_pos->y_advance) > position_fuzz ||
	  (unsigned int) abs (buf_pos->x_offset - ref_pos->x_offset) > position_fuzz ||
	  (unsigned int) abs (buf_pos->y_offset - ref_pos->y_offset) > position_fuzz)
      {
	result |= HB_BUFFER_DIFF_FLAG_POSITION_MISMATCH;
	break;
      }
      buf_pos++;
      ref_pos++;
    }
  }

  return result;
}


/*
 * Debugging.
 */

#ifndef HB_NO_BUFFER_MESSAGE
/**
 * hb_buffer_set_message_func:
 * @buffer: An #hb_buffer_t
 * @func: (closure user_data) (destroy destroy) (scope notified): Callback function
 * @user_data: (nullable): Data to pass to @func
 * @destroy: (nullable): The function to call when @user_data is not needed anymore
 *
 * Sets the implementation function for #hb_buffer_message_func_t.
 *
 * Since: 1.1.3
 **/
void
hb_buffer_set_message_func (hb_buffer_t *buffer,
			    hb_buffer_message_func_t func,
			    void *user_data, hb_destroy_func_t destroy)
{
  if (unlikely (hb_object_is_immutable (buffer)))
  {
    if (destroy)
      destroy (user_data);
    return;
  }

  if (buffer->message_destroy)
    buffer->message_destroy (buffer->message_data);

  if (func) {
    buffer->message_func = func;
    buffer->message_data = user_data;
    buffer->message_destroy = destroy;
  } else {
    buffer->message_func = nullptr;
    buffer->message_data = nullptr;
    buffer->message_destroy = nullptr;
  }
}
bool
hb_buffer_t::message_impl (hb_font_t *font, const char *fmt, va_list ap)
{
  assert (!have_output || (out_info == info && out_len == idx));

  message_depth++;

  char buf[100];
  vsnprintf (buf, sizeof (buf), fmt, ap);
  bool ret = (bool) this->message_func (this, font, buf, this->message_data);

  message_depth--;

  return ret;
}
#endif
