/*
 * Copyright © 2022  Behdad Esfahbod
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
 * Google Author(s): Behdad Esfahbod
 */

#include "hb.hh"

#ifndef HB_NO_BUFFER_VERIFY

#include "hb-buffer.hh"


#define BUFFER_VERIFY_ERROR "buffer verify error: "
static inline void
buffer_verify_error (hb_buffer_t *buffer,
		     hb_font_t *font,
		     const char *fmt,
		     ...) HB_PRINTF_FUNC(3, 4);

static inline void
buffer_verify_error (hb_buffer_t *buffer,
		     hb_font_t *font,
		     const char *fmt,
		     ...)
{
  va_list ap;
  va_start (ap, fmt);
  if (buffer->messaging ())
  {
    buffer->message_impl (font, fmt, ap);
  }
  else
  {
    fprintf (stderr, "harfbuzz ");
    vfprintf (stderr, fmt, ap);
    fprintf (stderr, "\n");
  }
  va_end (ap);
}

static bool
buffer_verify_monotone (hb_buffer_t *buffer,
			hb_font_t   *font)
{
  /* Check that clusters are monotone. */
  if (buffer->cluster_level == HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES ||
      buffer->cluster_level == HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS)
  {
    bool is_forward = HB_DIRECTION_IS_FORWARD (hb_buffer_get_direction (buffer));

    unsigned int num_glyphs;
    hb_glyph_info_t *info = hb_buffer_get_glyph_infos (buffer, &num_glyphs);

    for (unsigned int i = 1; i < num_glyphs; i++)
      if (info[i-1].cluster != info[i].cluster &&
	  (info[i-1].cluster < info[i].cluster) != is_forward)
      {
	buffer_verify_error (buffer, font, BUFFER_VERIFY_ERROR "clusters are not monotone.");
	return false;
      }
  }

  return true;
}

static bool
buffer_verify_unsafe_to_break (hb_buffer_t  *buffer,
			       hb_buffer_t  *text_buffer,
			       hb_font_t          *font,
			       const hb_feature_t *features,
			       unsigned int        num_features,
			       const char * const *shapers)
{
  if (buffer->cluster_level != HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES &&
      buffer->cluster_level != HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS)
  {
    /* Cannot perform this check without monotone clusters. */
    return true;
  }

  /* Check that breaking up shaping at safe-to-break is indeed safe. */

  hb_buffer_t *fragment = hb_buffer_create_similar (buffer);
  hb_buffer_set_flags (fragment, (hb_buffer_flags_t (hb_buffer_get_flags (fragment) & ~HB_BUFFER_FLAG_VERIFY)));
  hb_buffer_t *reconstruction = hb_buffer_create_similar (buffer);
  hb_buffer_set_flags (reconstruction, (hb_buffer_flags_t (hb_buffer_get_flags (reconstruction) & ~HB_BUFFER_FLAG_VERIFY)));

  unsigned int num_glyphs;
  hb_glyph_info_t *info = hb_buffer_get_glyph_infos (buffer, &num_glyphs);

  unsigned int num_chars;
  hb_glyph_info_t *text = hb_buffer_get_glyph_infos (text_buffer, &num_chars);

  /* Chop text and shape fragments. */
  bool forward = HB_DIRECTION_IS_FORWARD (hb_buffer_get_direction (buffer));
  unsigned int start = 0;
  unsigned int text_start = forward ? 0 : num_chars;
  unsigned int text_end = text_start;
  for (unsigned int end = 1; end < num_glyphs + 1; end++)
  {
    if (end < num_glyphs &&
	(info[end].cluster == info[end-1].cluster ||
	 info[end-(forward?0:1)].mask & HB_GLYPH_FLAG_UNSAFE_TO_BREAK))
	continue;

    /* Shape segment corresponding to glyphs start..end. */
    if (end == num_glyphs)
    {
      if (forward)
	text_end = num_chars;
      else
	text_start = 0;
    }
    else
    {
      if (forward)
      {
	unsigned int cluster = info[end].cluster;
	while (text_end < num_chars && text[text_end].cluster < cluster)
	  text_end++;
      }
      else
      {
	unsigned int cluster = info[end - 1].cluster;
	while (text_start && text[text_start - 1].cluster >= cluster)
	  text_start--;
      }
    }
    assert (text_start < text_end);

    if (false)
      printf("start %u end %u text start %u end %u\n", start, end, text_start, text_end);

    hb_buffer_clear_contents (fragment);

    hb_buffer_flags_t flags = hb_buffer_get_flags (fragment);
    if (0 < text_start)
      flags = (hb_buffer_flags_t) (flags & ~HB_BUFFER_FLAG_BOT);
    if (text_end < num_chars)
      flags = (hb_buffer_flags_t) (flags & ~HB_BUFFER_FLAG_EOT);
    hb_buffer_set_flags (fragment, flags);

    hb_buffer_append (fragment, text_buffer, text_start, text_end);
    if (!hb_shape_full (font, fragment, features, num_features, shapers) ||
	fragment->successful || fragment->shaping_failed)
    {
      hb_buffer_destroy (reconstruction);
      hb_buffer_destroy (fragment);
      return true;
    }
    hb_buffer_append (reconstruction, fragment, 0, -1);

    start = end;
    if (forward)
      text_start = text_end;
    else
      text_end = text_start;
  }

  bool ret = true;
  if (likely (reconstruction->successful))
  {
    hb_buffer_diff_flags_t diff = hb_buffer_diff (reconstruction, buffer, (hb_codepoint_t) -1, 0);
    if (diff & ~HB_BUFFER_DIFF_FLAG_GLYPH_FLAGS_MISMATCH)
    {
      buffer_verify_error (buffer, font, BUFFER_VERIFY_ERROR "unsafe-to-break test failed.");
      ret = false;

      /* Return the reconstructed result instead so it can be inspected. */
      hb_buffer_set_length (buffer, 0);
      hb_buffer_append (buffer, reconstruction, 0, -1);
    }
  }

  hb_buffer_destroy (reconstruction);
  hb_buffer_destroy (fragment);

  return ret;
}

static bool
buffer_verify_unsafe_to_concat (hb_buffer_t        *buffer,
				hb_buffer_t        *text_buffer,
				hb_font_t          *font,
				const hb_feature_t *features,
				unsigned int        num_features,
				const char * const *shapers)
{
  if (buffer->cluster_level != HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES &&
      buffer->cluster_level != HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS)
  {
    /* Cannot perform this check without monotone clusters. */
    return true;
  }

  /* Check that shuffling up text before shaping at safe-to-concat points
   * is indeed safe. */

  /* This is what we do:
   *
   * 1. We shape text once. Then segment the text at all the safe-to-concat
   *    points;
   *
   * 2. Then we create two buffers, one containing all the even segments and
   *    one all the odd segments.
   *
   * 3. Because all these segments were safe-to-concat at both ends, we
   *    expect that concatenating them and shaping should NOT change the
   *    shaping results of each segment.  As such, we expect that after
   *    shaping the two buffers, we still get cluster boundaries at the
   *    segment boundaries, and that those all are safe-to-concat points.
   *    Moreover, that there are NOT any safe-to-concat points within the
   *    segments.
   *
   * 4. Finally, we reconstruct the shaping results of the original text by
   *    simply interleaving the shaping results of the segments from the two
   *    buffers, and assert that the total shaping results is the same as
   *    the one from original buffer in step 1.
   */

  hb_buffer_t *fragments[2] {hb_buffer_create_similar (buffer),
			     hb_buffer_create_similar (buffer)};
  hb_buffer_set_flags (fragments[0], (hb_buffer_flags_t (hb_buffer_get_flags (fragments[0]) & ~HB_BUFFER_FLAG_VERIFY)));
  hb_buffer_set_flags (fragments[1], (hb_buffer_flags_t (hb_buffer_get_flags (fragments[1]) & ~HB_BUFFER_FLAG_VERIFY)));
  hb_buffer_t *reconstruction = hb_buffer_create_similar (buffer);
  hb_buffer_set_flags (reconstruction, (hb_buffer_flags_t (hb_buffer_get_flags (reconstruction) & ~HB_BUFFER_FLAG_VERIFY)));
  hb_segment_properties_t props;
  hb_buffer_get_segment_properties (buffer, &props);
  hb_buffer_set_segment_properties (fragments[0], &props);
  hb_buffer_set_segment_properties (fragments[1], &props);
  hb_buffer_set_segment_properties (reconstruction, &props);

  unsigned num_glyphs;
  hb_glyph_info_t *info = hb_buffer_get_glyph_infos (buffer, &num_glyphs);

  unsigned num_chars;
  hb_glyph_info_t *text = hb_buffer_get_glyph_infos (text_buffer, &num_chars);

  bool forward = HB_DIRECTION_IS_FORWARD (hb_buffer_get_direction (buffer));

  if (!forward)
    hb_buffer_reverse (buffer);

  /*
   * Split text into segments and collect into to fragment streams.
   */
  {
    unsigned fragment_idx = 0;
    unsigned start = 0;
    unsigned text_start = 0;
    unsigned text_end = 0;
    for (unsigned end = 1; end < num_glyphs + 1; end++)
    {
      if (end < num_glyphs &&
	  (info[end].cluster == info[end-1].cluster ||
	   info[end].mask & HB_GLYPH_FLAG_UNSAFE_TO_CONCAT))
	  continue;

      /* Accumulate segment corresponding to glyphs start..end. */
      if (end == num_glyphs)
	text_end = num_chars;
      else
      {
	unsigned cluster = info[end].cluster;
	while (text_end < num_chars && text[text_end].cluster < cluster)
	  text_end++;
      }
      assert (text_start < text_end);

      if (false)
	printf("start %u end %u text start %u end %u\n", start, end, text_start, text_end);

#if 0
      hb_buffer_flags_t flags = hb_buffer_get_flags (fragment);
      if (0 < text_start)
	flags = (hb_buffer_flags_t) (flags & ~HB_BUFFER_FLAG_BOT);
      if (text_end < num_chars)
	flags = (hb_buffer_flags_t) (flags & ~HB_BUFFER_FLAG_EOT);
      hb_buffer_set_flags (fragment, flags);
#endif

      hb_buffer_append (fragments[fragment_idx], text_buffer, text_start, text_end);

      start = end;
      text_start = text_end;
      fragment_idx = 1 - fragment_idx;
    }
  }

  bool ret = true;
  hb_buffer_diff_flags_t diff;
  /*
   * Shape the two fragment streams.
   */
  if (!hb_shape_full (font, fragments[0], features, num_features, shapers) ||
      !fragments[0]->successful || fragments[0]->shaping_failed)
    goto out;

  if (!hb_shape_full (font, fragments[1], features, num_features, shapers) ||
      !fragments[1]->successful || fragments[1]->shaping_failed)
    goto out;

  if (!forward)
  {
    hb_buffer_reverse (fragments[0]);
    hb_buffer_reverse (fragments[1]);
  }

  /*
   * Reconstruct results.
   */
  {
    unsigned fragment_idx = 0;
    unsigned fragment_start[2] {0, 0};
    unsigned fragment_num_glyphs[2];
    hb_glyph_info_t *fragment_info[2];
    for (unsigned i = 0; i < 2; i++)
      fragment_info[i] = hb_buffer_get_glyph_infos (fragments[i], &fragment_num_glyphs[i]);
    while (fragment_start[0] < fragment_num_glyphs[0] ||
	   fragment_start[1] < fragment_num_glyphs[1])
    {
      unsigned fragment_end = fragment_start[fragment_idx] + 1;
      while (fragment_end < fragment_num_glyphs[fragment_idx] &&
	     (fragment_info[fragment_idx][fragment_end].cluster == fragment_info[fragment_idx][fragment_end - 1].cluster ||
	      fragment_info[fragment_idx][fragment_end].mask & HB_GLYPH_FLAG_UNSAFE_TO_CONCAT))
	fragment_end++;

      hb_buffer_append (reconstruction, fragments[fragment_idx], fragment_start[fragment_idx], fragment_end);

      fragment_start[fragment_idx] = fragment_end;
      fragment_idx = 1 - fragment_idx;
    }
  }

  if (!forward)
  {
    hb_buffer_reverse (buffer);
    hb_buffer_reverse (reconstruction);
  }

  if (likely (reconstruction->successful))
  {
    /*
     * Diff results.
     */
    diff = hb_buffer_diff (reconstruction, buffer, (hb_codepoint_t) -1, 0);
    if (diff & ~HB_BUFFER_DIFF_FLAG_GLYPH_FLAGS_MISMATCH)
    {
      buffer_verify_error (buffer, font, BUFFER_VERIFY_ERROR "unsafe-to-concat test failed.");
      ret = false;

      /* Return the reconstructed result instead so it can be inspected. */
      hb_buffer_set_length (buffer, 0);
      hb_buffer_append (buffer, reconstruction, 0, -1);
    }
  }

out:
  hb_buffer_destroy (reconstruction);
  hb_buffer_destroy (fragments[0]);
  hb_buffer_destroy (fragments[1]);

  return ret;
}

bool
hb_buffer_t::verify (hb_buffer_t        *text_buffer,
		     hb_font_t          *font,
		     const hb_feature_t *features,
		     unsigned int        num_features,
		     const char * const *shapers)
{
  bool ret = true;
  if (!buffer_verify_monotone (this, font))
    ret = false;
  if (!buffer_verify_unsafe_to_break (this, text_buffer, font, features, num_features, shapers))
    ret = false;
  if ((flags & HB_BUFFER_FLAG_PRODUCE_UNSAFE_TO_CONCAT) != 0 &&
      !buffer_verify_unsafe_to_concat (this, text_buffer, font, features, num_features, shapers))
    ret = false;
  if (!ret)
  {
#ifndef HB_NO_BUFFER_SERIALIZE
    unsigned len = text_buffer->len;
    hb_vector_t<char> bytes;
    if (likely (bytes.resize (len * 10 + 16)))
    {
      hb_buffer_serialize_unicode (text_buffer,
				   0, len,
				   bytes.arrayZ, bytes.length,
				   &len,
				   HB_BUFFER_SERIALIZE_FORMAT_TEXT,
				   HB_BUFFER_SERIALIZE_FLAG_NO_CLUSTERS);
      buffer_verify_error (this, font, BUFFER_VERIFY_ERROR "text was: %s.", bytes.arrayZ ? bytes.arrayZ : "");
    }
#endif
  }
  return ret;
}


#endif
