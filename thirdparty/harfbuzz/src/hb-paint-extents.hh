/*
 * Copyright © 2022 Behdad Esfahbod
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
 */

#ifndef HB_PAINT_EXTENTS_HH
#define HB_PAINT_EXTENTS_HH

#include "hb.hh"
#include "hb-paint.h"

#include "hb-geometry.hh"


typedef struct  hb_paint_extents_context_t hb_paint_extents_context_t;

struct hb_paint_extents_context_t
{
  hb_paint_extents_context_t ()
  {
    transforms.push (hb_transform_t{});
    clips.push (hb_bounds_t{hb_bounds_t::UNBOUNDED});
    groups.push (hb_bounds_t{hb_bounds_t::EMPTY});
  }

  hb_extents_t get_extents ()
  {
    return groups.tail().extents;
  }

  bool is_bounded ()
  {
    return groups.tail().status != hb_bounds_t::UNBOUNDED;
  }

  void push_transform (const hb_transform_t &trans)
  {
    hb_transform_t t = transforms.tail ();
    t.multiply (trans);
    transforms.push (t);
  }

  void pop_transform ()
  {
    transforms.pop ();
  }

  void push_clip (hb_extents_t extents)
  {
    /* Transform extents and push a new clip. */
    const hb_transform_t &t = transforms.tail ();
    t.transform_extents (extents);

    auto bounds = hb_bounds_t {extents};
    bounds.intersect (clips.tail ());

    clips.push (bounds);
  }

  void pop_clip ()
  {
    clips.pop ();
  }

  void push_group ()
  {
    groups.push (hb_bounds_t {hb_bounds_t::EMPTY});
  }

  void pop_group (hb_paint_composite_mode_t mode)
  {
    const hb_bounds_t src_bounds = groups.pop ();
    hb_bounds_t &backdrop_bounds = groups.tail ();

    // https://learn.microsoft.com/en-us/typography/opentype/spec/colr#format-32-paintcomposite
    switch ((int) mode)
    {
      case HB_PAINT_COMPOSITE_MODE_CLEAR:
	backdrop_bounds.status = hb_bounds_t::EMPTY;
	break;
      case HB_PAINT_COMPOSITE_MODE_SRC:
      case HB_PAINT_COMPOSITE_MODE_SRC_OUT:
	backdrop_bounds = src_bounds;
	break;
      case HB_PAINT_COMPOSITE_MODE_DEST:
      case HB_PAINT_COMPOSITE_MODE_DEST_OUT:
	break;
      case HB_PAINT_COMPOSITE_MODE_SRC_IN:
      case HB_PAINT_COMPOSITE_MODE_DEST_IN:
	backdrop_bounds.intersect (src_bounds);
	break;
      default:
	backdrop_bounds.union_ (src_bounds);
	break;
     }
  }

  void paint ()
  {
    const hb_bounds_t &clip = clips.tail ();
    hb_bounds_t &group = groups.tail ();

    group.union_ (clip);
  }

  protected:
  hb_vector_t<hb_transform_t> transforms;
  hb_vector_t<hb_bounds_t> clips;
  hb_vector_t<hb_bounds_t> groups;
};

HB_INTERNAL hb_paint_funcs_t *
hb_paint_extents_get_funcs ();


#endif /* HB_PAINT_EXTENTS_HH */
