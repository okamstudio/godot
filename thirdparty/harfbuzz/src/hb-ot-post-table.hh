/*
 * Copyright © 2016  Google, Inc.
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

#ifndef HB_OT_POST_TABLE_HH
#define HB_OT_POST_TABLE_HH

#include "hb-open-type.hh"
#include "hb-ot-var-mvar-table.hh"

#define HB_STRING_ARRAY_NAME format1_names
#define HB_STRING_ARRAY_LIST "hb-ot-post-macroman.hh"
#include "hb-string-array.hh"
#undef HB_STRING_ARRAY_LIST
#undef HB_STRING_ARRAY_NAME

/*
 * post -- PostScript
 * https://docs.microsoft.com/en-us/typography/opentype/spec/post
 */
#define HB_OT_TAG_post HB_TAG('p','o','s','t')


namespace OT {


struct postV2Tail
{
  friend struct post;

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (glyphNameIndex.sanitize (c));
  }

  template<typename Iterator>
  bool serialize (hb_serialize_context_t *c,
                  Iterator it,
                  const void* _post) const;

  bool subset (hb_subset_context_t *c) const;

  protected:
  Array16Of<HBUINT16>	glyphNameIndex;	/* This is not an offset, but is the
					 * ordinal number of the glyph in 'post'
					 * string tables. */
/*UnsizedArrayOf<HBUINT8>
			namesX;*/	/* Glyph names with length bytes [variable]
					 * (a Pascal string). */

  public:
  DEFINE_SIZE_ARRAY (2, glyphNameIndex);
};

struct post
{
  static constexpr hb_tag_t tableTag = HB_OT_TAG_post;

  bool serialize (hb_serialize_context_t *c, bool glyph_names) const
  {
    TRACE_SERIALIZE (this);
    post *post_prime = c->allocate_min<post> ();
    if (unlikely (!post_prime))  return_trace (false);

    hb_memcpy (post_prime, this, post::min_size);
    if (!glyph_names)
      return_trace (c->check_assign (post_prime->version.major, 3,
                                     HB_SERIALIZE_ERROR_INT_OVERFLOW)); // Version 3 does not have any glyph names.

    return_trace (true);
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *post_prime = c->serializer->start_embed<post> ();

    bool glyph_names = c->plan->flags & HB_SUBSET_FLAGS_GLYPH_NAMES;
    if (!serialize (c->serializer, glyph_names))
      return_trace (false);

#ifndef HB_NO_VAR
    if (c->plan->normalized_coords)
    {
      auto &MVAR = *c->plan->source->table.MVAR;
      auto *table = post_prime;

      HB_ADD_MVAR_VAR (HB_OT_METRICS_TAG_UNDERLINE_SIZE,   underlineThickness);
      HB_ADD_MVAR_VAR (HB_OT_METRICS_TAG_UNDERLINE_OFFSET, underlinePosition);
    }
#endif

    Triple *axis_range;
    if (c->plan->user_axes_location.has (HB_TAG ('s','l','n','t'), &axis_range))
    {
      float italic_angle = hb_max (-90.0, hb_min (axis_range->middle, 90.0));
      if (post_prime->italicAngle.to_float () != italic_angle)
        post_prime->italicAngle.set_float (italic_angle);
    }

    if (glyph_names && version.major == 2)
    {
      hb_barrier ();
      return_trace (v2X.subset (c));
    }

    return_trace (true);
  }

  struct accelerator_t
  {
    friend struct postV2Tail;

    accelerator_t (hb_face_t *face)
    {
      table = hb_sanitize_context_t ().reference_table<post> (face);
      unsigned int table_length = table.get_length ();

      version = table->version.to_int ();
      if (version != 0x00020000) return;
      hb_barrier ();

      const postV2Tail &v2 = table->v2X;

      glyphNameIndex = &v2.glyphNameIndex;
      pool = &StructAfter<uint8_t> (v2.glyphNameIndex);

      const uint8_t *end = (const uint8_t *) (const void *) table + table_length;
      index_to_offset.alloc (hb_min (face->get_num_glyphs (), table_length / 8));
      for (const uint8_t *data = pool;
	   index_to_offset.length < 65535 && data < end && data + *data < end;
	   data += 1 + *data)
	index_to_offset.push (data - pool);
    }
    ~accelerator_t ()
    {
      hb_free (gids_sorted_by_name.get_acquire ());
      table.destroy ();
    }

    bool get_glyph_name (hb_codepoint_t glyph,
			 char *buf, unsigned int buf_len) const
    {
      hb_bytes_t s = find_glyph_name (glyph);
      if (!s.length) return false;
      if (!buf_len) return true;
      unsigned int len = hb_min (buf_len - 1, s.length);
      strncpy (buf, s.arrayZ, len);
      buf[len] = '\0';
      return true;
    }

    bool get_glyph_from_name (const char *name, int len,
			      hb_codepoint_t *glyph) const
    {
      unsigned int count = get_glyph_count ();
      if (unlikely (!count)) return false;

      if (len < 0) len = strlen (name);

      if (unlikely (!len)) return false;

    retry:
      uint16_t *gids = gids_sorted_by_name.get_acquire ();

      if (unlikely (!gids))
      {
	gids = (uint16_t *) hb_malloc (count * sizeof (gids[0]));
	if (unlikely (!gids))
	  return false; /* Anything better?! */

	for (unsigned int i = 0; i < count; i++)
	  gids[i] = i;
	hb_qsort (gids, count, sizeof (gids[0]), cmp_gids, (void *) this);

	if (unlikely (!gids_sorted_by_name.cmpexch (nullptr, gids)))
	{
	  hb_free (gids);
	  goto retry;
	}
      }

      hb_bytes_t st (name, len);
      auto* gid = hb_bsearch (st, gids, count, sizeof (gids[0]), cmp_key, (void *) this);
      if (gid)
      {
	*glyph = *gid;
	return true;
      }

      return false;
    }

    hb_blob_ptr_t<post> table;

    protected:

    unsigned int get_glyph_count () const
    {
      if (version == 0x00010000)
      {
        hb_barrier ();
	return format1_names_length;
      }

      if (version == 0x00020000)
      {
        hb_barrier ();
	return glyphNameIndex->len;
      }

      return 0;
    }

    static int cmp_gids (const void *pa, const void *pb, void *arg)
    {
      const accelerator_t *thiz = (const accelerator_t *) arg;
      uint16_t a = * (const uint16_t *) pa;
      uint16_t b = * (const uint16_t *) pb;
      return thiz->find_glyph_name (b).cmp (thiz->find_glyph_name (a));
    }

    static int cmp_key (const void *pk, const void *po, void *arg)
    {
      const accelerator_t *thiz = (const accelerator_t *) arg;
      const hb_bytes_t *key = (const hb_bytes_t *) pk;
      uint16_t o = * (const uint16_t *) po;
      return thiz->find_glyph_name (o).cmp (*key);
    }

    hb_bytes_t find_glyph_name (hb_codepoint_t glyph) const
    {
      if (version == 0x00010000)
      {
        hb_barrier ();
	if (glyph >= format1_names_length)
	  return hb_bytes_t ();

	return format1_names (glyph);
      }

      if (version != 0x00020000)
	return hb_bytes_t ();
      hb_barrier ();

      if (glyph >= glyphNameIndex->len)
	return hb_bytes_t ();

      unsigned int index = glyphNameIndex->arrayZ[glyph];
      if (index < format1_names_length)
	return format1_names (index);
      index -= format1_names_length;

      if (index >= index_to_offset.length)
	return hb_bytes_t ();
      unsigned int offset = index_to_offset[index];

      const uint8_t *data = pool + offset;
      unsigned int name_length = *data;
      data++;

      return hb_bytes_t ((const char *) data, name_length);
    }

    private:
    uint32_t version;
    const Array16Of<HBUINT16> *glyphNameIndex = nullptr;
    hb_vector_t<uint32_t> index_to_offset;
    const uint8_t *pool = nullptr;
    hb_atomic_ptr_t<uint16_t *> gids_sorted_by_name;
  };

  bool has_data () const { return version.to_int (); }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  hb_barrier () &&
		  (version.to_int () == 0x00010000 ||
		   (version.to_int () == 0x00020000 && hb_barrier () && v2X.sanitize (c)) ||
		   version.to_int () == 0x00030000));
  }

  public:
  FixedVersion<>version;		/* 0x00010000 for version 1.0
					 * 0x00020000 for version 2.0
					 * 0x00025000 for version 2.5 (deprecated)
					 * 0x00030000 for version 3.0 */
  F16DOT16	italicAngle;		/* Italic angle in counter-clockwise degrees
					 * from the vertical. Zero for upright text,
					 * negative for text that leans to the right
					 * (forward). */
  FWORD		underlinePosition;	/* This is the suggested distance of the top
					 * of the underline from the baseline
					 * (negative values indicate below baseline).
					 * The PostScript definition of this FontInfo
					 * dictionary key (the y coordinate of the
					 * center of the stroke) is not used for
					 * historical reasons. The value of the
					 * PostScript key may be calculated by
					 * subtracting half the underlineThickness
					 * from the value of this field. */
  FWORD		underlineThickness;	/* Suggested values for the underline
					   thickness. */
  HBUINT32	isFixedPitch;		/* Set to 0 if the font is proportionally
					 * spaced, non-zero if the font is not
					 * proportionally spaced (i.e. monospaced). */
  HBUINT32	minMemType42;		/* Minimum memory usage when an OpenType font
					 * is downloaded. */
  HBUINT32	maxMemType42;		/* Maximum memory usage when an OpenType font
					 * is downloaded. */
  HBUINT32	minMemType1;		/* Minimum memory usage when an OpenType font
					 * is downloaded as a Type 1 font. */
  HBUINT32	maxMemType1;		/* Maximum memory usage when an OpenType font
					 * is downloaded as a Type 1 font. */
  postV2Tail	v2X;
  DEFINE_SIZE_MIN (32);
};

struct post_accelerator_t : post::accelerator_t {
  post_accelerator_t (hb_face_t *face) : post::accelerator_t (face) {}
};


} /* namespace OT */


#endif /* HB_OT_POST_TABLE_HH */
