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
#ifndef HB_GEOMETRY_HH
#define HB_GEOMETRY_HH

#include "hb.hh"


struct hb_extents_t
{
  hb_extents_t () {}
  hb_extents_t (float xmin, float ymin, float xmax, float ymax) :
    xmin (xmin), ymin (ymin), xmax (xmax), ymax (ymax) {}

  bool is_empty () const { return xmin >= xmax || ymin >= ymax; }
  bool is_void () const { return xmin > xmax; }

  void union_ (const hb_extents_t &o)
  {
    xmin = hb_min (xmin, o.xmin);
    ymin = hb_min (ymin, o.ymin);
    xmax = hb_max (xmax, o.xmax);
    ymax = hb_max (ymax, o.ymax);
  }

  void intersect (const hb_extents_t &o)
  {
    xmin = hb_max (xmin, o.xmin);
    ymin = hb_max (ymin, o.ymin);
    xmax = hb_min (xmax, o.xmax);
    ymax = hb_min (ymax, o.ymax);
  }

  void
  add_point (float x, float y)
  {
    if (unlikely (is_void ()))
    {
      xmin = xmax = x;
      ymin = ymax = y;
    }
    else
    {
      xmin = hb_min (xmin, x);
      ymin = hb_min (ymin, y);
      xmax = hb_max (xmax, x);
      ymax = hb_max (ymax, y);
    }
  }

  float xmin = 0.f;
  float ymin = 0.f;
  float xmax = -1.f;
  float ymax = -1.f;
};

struct hb_transform_t
{
  hb_transform_t () {}
  hb_transform_t (float xx, float yx,
		  float xy, float yy,
		  float x0, float y0) :
    xx (xx), yx (yx), xy (xy), yy (yy), x0 (x0), y0 (y0) {}

  void multiply (const hb_transform_t &o)
  {
    /* Copied from cairo, with "o" being "a" there and "this" being "b" there. */
    hb_transform_t r;

    r.xx = o.xx * xx + o.yx * xy;
    r.yx = o.xx * yx + o.yx * yy;

    r.xy = o.xy * xx + o.yy * xy;
    r.yy = o.xy * yx + o.yy * yy;

    r.x0 = o.x0 * xx + o.y0 * xy + x0;
    r.y0 = o.x0 * yx + o.y0 * yy + y0;

    *this = r;
  }

  void transform_distance (float &dx, float &dy) const
  {
    float new_x = xx * dx + xy * dy;
    float new_y = yx * dx + yy * dy;
    dx = new_x;
    dy = new_y;
  }

  void transform_point (float &x, float &y) const
  {
    transform_distance (x, y);
    x += x0;
    y += y0;
  }

  void transform_extents (hb_extents_t &extents) const
  {
    float quad_x[4], quad_y[4];

    quad_x[0] = extents.xmin;
    quad_y[0] = extents.ymin;
    quad_x[1] = extents.xmin;
    quad_y[1] = extents.ymax;
    quad_x[2] = extents.xmax;
    quad_y[2] = extents.ymin;
    quad_x[3] = extents.xmax;
    quad_y[3] = extents.ymax;

    extents = hb_extents_t {};
    for (unsigned i = 0; i < 4; i++)
    {
      transform_point (quad_x[i], quad_y[i]);
      extents.add_point (quad_x[i], quad_y[i]);
    }
  }

  void transform (const hb_transform_t &o) { multiply (o); }

  void translate (float x, float y)
  {
    if (x == 0.f && y == 0.f)
      return;

    x0 += xx * x + xy * y;
    y0 += yx * x + yy * y;
  }

  void scale (float scaleX, float scaleY)
  {
    if (scaleX == 1.f && scaleY == 1.f)
      return;

    xx *= scaleX;
    yx *= scaleX;
    xy *= scaleY;
    yy *= scaleY;
  }

  void rotate (float rotation)
  {
    if (rotation == 0.f)
      return;

    // https://github.com/fonttools/fonttools/blob/f66ee05f71c8b57b5f519ee975e95edcd1466e14/Lib/fontTools/misc/transform.py#L240
    rotation = rotation * HB_PI;
    float c;
    float s;
#ifdef HAVE_SINCOSF
    sincosf (rotation, &s, &c);
#else
    c = cosf (rotation);
    s = sinf (rotation);
#endif
    auto other = hb_transform_t{c, s, -s, c, 0.f, 0.f};
    transform (other);
  }

  void skew (float skewX, float skewY)
  {
    if (skewX == 0.f && skewY == 0.f)
      return;

    // https://github.com/fonttools/fonttools/blob/f66ee05f71c8b57b5f519ee975e95edcd1466e14/Lib/fontTools/misc/transform.py#L255
    skewX = skewX * HB_PI;
    skewY = skewY * HB_PI;
    auto other = hb_transform_t{1.f,
				skewY ? tanf (skewY) : 0.f,
				skewX ? tanf (skewX) : 0.f,
				1.f,
				0.f, 0.f};
    transform (other);
  }

  float xx = 1.f;
  float yx = 0.f;
  float xy = 0.f;
  float yy = 1.f;
  float x0 = 0.f;
  float y0 = 0.f;
};

struct hb_bounds_t
{
  enum status_t {
    UNBOUNDED,
    BOUNDED,
    EMPTY,
  };

  hb_bounds_t (status_t status) : status (status) {}
  hb_bounds_t (const hb_extents_t &extents) :
    status (extents.is_empty () ? EMPTY : BOUNDED), extents (extents) {}

  void union_ (const hb_bounds_t &o)
  {
    if (o.status == UNBOUNDED)
      status = UNBOUNDED;
    else if (o.status == BOUNDED)
    {
      if (status == EMPTY)
	*this = o;
      else if (status == BOUNDED)
        extents.union_ (o.extents);
    }
  }

  void intersect (const hb_bounds_t &o)
  {
    if (o.status == EMPTY)
      status = EMPTY;
    else if (o.status == BOUNDED)
    {
      if (status == UNBOUNDED)
	*this = o;
      else if (status == BOUNDED)
      {
        extents.intersect (o.extents);
	if (extents.is_empty ())
	  status = EMPTY;
      }
    }
  }

  status_t status;
  hb_extents_t extents;
};

struct hb_transform_decomposed_t
{
  float translateX = 0;
  float translateY = 0;
  float rotation = 0;  // in degrees, counter-clockwise
  float scaleX = 1;
  float scaleY = 1;
  float skewX = 0;  // in degrees, counter-clockwise
  float skewY = 0;  // in degrees, counter-clockwise
  float tCenterX = 0;
  float tCenterY = 0;

  operator bool () const
  {
    return translateX || translateY ||
	   rotation ||
	   scaleX != 1 || scaleY != 1 ||
	   skewX || skewY ||
	   tCenterX || tCenterY;
  }

  hb_transform_t to_transform () const
  {
    hb_transform_t t;
    t.translate (translateX + tCenterX, translateY + tCenterY);
    t.rotate (rotation);
    t.scale (scaleX, scaleY);
    t.skew (-skewX, skewY);
    t.translate (-tCenterX, -tCenterY);
    return t;
  }
};


#endif /* HB_GEOMETRY_HH */
