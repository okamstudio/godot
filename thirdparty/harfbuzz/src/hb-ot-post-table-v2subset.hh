/*
 * Copyright © 2021  Google, Inc.
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
 */

#ifndef HB_OT_POST_TABLE_V2SUBSET_HH
#define HB_OT_POST_TABLE_V2SUBSET_HH

#include "hb-open-type.hh"
#include "hb-ot-post-table.hh"

/*
 * post -- PostScript
 * https://docs.microsoft.com/en-us/typography/opentype/spec/post
 */

namespace OT {
template<typename Iterator>
HB_INTERNAL bool postV2Tail::serialize (hb_serialize_context_t *c,
                                        Iterator it,
                                        const void* _post) const
{
  TRACE_SERIALIZE (this);
  auto *out = c->start_embed (this);
  if (unlikely (!c->check_success (out))) return_trace (false);
  if (!out->glyphNameIndex.serialize (c, + it
                                         | hb_map (hb_second)))
      return_trace (false);

  hb_set_t copied_indices;
  for (const auto& _ : + it )
  {
    unsigned glyph_id = _.first;
    unsigned new_index = _.second;

    if (new_index < 258) continue;
    if (copied_indices.has (new_index)) continue;
    copied_indices.add (new_index);

    hb_bytes_t s = reinterpret_cast<const post::accelerator_t*> (_post)->find_glyph_name (glyph_id);
    HBUINT8 *o = c->allocate_size<HBUINT8> (HBUINT8::static_size * (s.length + 1));
    if (unlikely (!o)) return_trace (false);
    if (!c->check_assign (o[0], s.length, HB_SERIALIZE_ERROR_INT_OVERFLOW)) return_trace (false);
    hb_memcpy (o+1, s.arrayZ, HBUINT8::static_size * s.length);
  }

  return_trace (true);
}

HB_INTERNAL bool postV2Tail::subset (hb_subset_context_t *c) const
{
  TRACE_SUBSET (this);

  const hb_map_t &reverse_glyph_map = *c->plan->reverse_glyph_map;
  unsigned num_glyphs = c->plan->num_output_glyphs ();
  hb_map_t old_new_index_map, old_gid_new_index_map;
  unsigned i = 0;

  post::accelerator_t _post (c->plan->source);

  hb_hashmap_t<hb_bytes_t, uint32_t, true> glyph_name_to_new_index;

  old_new_index_map.alloc (num_glyphs);
  old_gid_new_index_map.alloc (num_glyphs);
  glyph_name_to_new_index.alloc (num_glyphs);

  for (auto _ : c->plan->new_to_old_gid_list)
  {
    hb_codepoint_t old_gid = _.second;
    unsigned old_index = glyphNameIndex[old_gid];

    unsigned new_index;
    const uint32_t *new_index2;
    if (old_index <= 257)
      new_index = old_index;
    else if (old_new_index_map.has (old_index, &new_index2))
      new_index = *new_index2;
    else
    {
      hb_bytes_t s = _post.find_glyph_name (old_gid);
      new_index = glyph_name_to_new_index.get (s);
      if (new_index == (unsigned)-1)
      {
        int standard_glyph_index = -1;
        for (unsigned i = 0; i < format1_names_length; i++)
        {
          if (s == format1_names (i))
          {
            standard_glyph_index = i;
            break;
          }
        }

        if (standard_glyph_index == -1)
        {
          new_index = 258 + i;
          i++;
        }
        else
        { new_index = standard_glyph_index; }
        glyph_name_to_new_index.set (s, new_index);
      }
      old_new_index_map.set (old_index, new_index);
    }
    old_gid_new_index_map.set (old_gid, new_index);
  }

  if (old_gid_new_index_map.in_error())
    return_trace (false);

  auto index_iter =
  + hb_range (num_glyphs)
  | hb_map_retains_sorting ([&](hb_codepoint_t new_gid)
                            {
                              hb_codepoint_t *old_gid;
                              /* use 0 for retain-gid holes, which refers to the name .notdef,
                               * as the glyphNameIndex entry for that glyph ID."*/
                              unsigned new_index = 0;
                              if (reverse_glyph_map.has (new_gid, &old_gid)) {
                                new_index = old_gid_new_index_map.get (*old_gid);
                                return hb_pair_t<unsigned, unsigned> (*old_gid, new_index);
                              }
                              return hb_pair_t<unsigned, unsigned> (new_gid, new_index);
                            })
  ;

  return_trace (serialize (c->serializer, index_iter, &_post));
}

} /* namespace OT */
#endif /* HB_OT_POST_TABLE_V2SUBSET_HH */
