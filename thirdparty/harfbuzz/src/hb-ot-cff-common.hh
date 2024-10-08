/*
 * Copyright © 2018 Adobe Inc.
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
 * Adobe Author(s): Michiharu Ariza
 */
#ifndef HB_OT_CFF_COMMON_HH
#define HB_OT_CFF_COMMON_HH

#include "hb-open-type.hh"
#include "hb-bimap.hh"
#include "hb-ot-layout-common.hh"
#include "hb-cff-interp-dict-common.hh"
#include "hb-subset-plan.hh"

namespace CFF {

using namespace OT;

#define CFF_UNDEF_CODE  0xFFFFFFFF

using objidx_t = hb_serialize_context_t::objidx_t;
using whence_t = hb_serialize_context_t::whence_t;

/* CFF offsets can technically be negative */
template<typename Type, typename ...Ts>
static inline const Type& StructAtOffsetOrNull (const void *P, int offset, hb_sanitize_context_t &sc, Ts&&... ds)
{
  if (!offset) return Null (Type);

  const char *p = (const char *) P + offset;
  if (!sc.check_point (p)) return Null (Type);

  const Type &obj = *reinterpret_cast<const Type *> (p);
  if (!obj.sanitize (&sc, std::forward<Ts> (ds)...)) return Null (Type);

  return obj;
}


struct code_pair_t
{
  unsigned code;
  hb_codepoint_t glyph;
};


using str_buff_t = hb_vector_t<unsigned char>;
using str_buff_vec_t = hb_vector_t<str_buff_t>;
using glyph_to_sid_map_t = hb_vector_t<code_pair_t>;

/* Top Dict, Font Dict, Private Dict */
struct Dict : UnsizedByteStr
{
  template <typename DICTVAL, typename OP_SERIALIZER, typename ...Ts>
  bool serialize (hb_serialize_context_t *c,
		  const DICTVAL &dictval,
		  OP_SERIALIZER& opszr,
		  Ts&&... ds)
  {
    TRACE_SERIALIZE (this);
    for (unsigned int i = 0; i < dictval.get_count (); i++)
      if (unlikely (!opszr.serialize (c, dictval[i], std::forward<Ts> (ds)...)))
	return_trace (false);

    return_trace (true);
  }

  template <typename T, typename V>
  static bool serialize_int_op (hb_serialize_context_t *c, op_code_t op, V value, op_code_t intOp)
  {
    if (unlikely ((!serialize_int<T, V> (c, intOp, value))))
      return false;

    TRACE_SERIALIZE (this);
    /* serialize the opcode */
    HBUINT8 *p = c->allocate_size<HBUINT8> (OpCode_Size (op), false);
    if (unlikely (!p)) return_trace (false);
    if (Is_OpCode_ESC (op))
    {
      *p = OpCode_escape;
      op = Unmake_OpCode_ESC (op);
      p++;
    }
    *p = op;
    return_trace (true);
  }

  template <typename V>
  static bool serialize_int4_op (hb_serialize_context_t *c, op_code_t op, V value)
  { return serialize_int_op<HBINT32> (c, op, value, OpCode_longintdict); }

  template <typename V>
  static bool serialize_int2_op (hb_serialize_context_t *c, op_code_t op, V value)
  { return serialize_int_op<HBINT16> (c, op, value, OpCode_shortint); }

  template <typename T, int int_op>
  static bool serialize_link_op (hb_serialize_context_t *c, op_code_t op, objidx_t link, whence_t whence)
  {
    T &ofs = *(T *) (c->head + OpCode_Size (int_op));
    if (unlikely (!serialize_int_op<T> (c, op, 0, int_op))) return false;
    c->add_link (ofs, link, whence);
    return true;
  }

  static bool serialize_link4_op (hb_serialize_context_t *c, op_code_t op, objidx_t link, whence_t whence = whence_t::Head)
  { return serialize_link_op<HBINT32, OpCode_longintdict> (c, op, link, whence); }

  static bool serialize_link2_op (hb_serialize_context_t *c, op_code_t op, objidx_t link, whence_t whence = whence_t::Head)
  { return serialize_link_op<HBINT16, OpCode_shortint> (c, op, link, whence); }
};

struct TopDict : Dict {};
struct FontDict : Dict {};
struct PrivateDict : Dict {};

struct table_info_t
{
  void init () { offset = size = 0; link = 0; }

  unsigned int    offset;
  unsigned int    size;
  objidx_t	  link;
};

template <typename COUNT>
struct FDArray : CFFIndex<COUNT>
{
  template <typename DICTVAL, typename INFO, typename Iterator, typename OP_SERIALIZER>
  bool serialize (hb_serialize_context_t *c,
		  Iterator it,
		  OP_SERIALIZER& opszr)
  {
    TRACE_SERIALIZE (this);

    /* serialize INDEX data */
    hb_vector_t<unsigned> sizes;
    if (it.is_random_access_iterator)
      sizes.alloc (hb_len (it));

    c->push ();
    char *data_base = c->head;
    + it
    | hb_map ([&] (const hb_pair_t<const DICTVAL&, const INFO&> &_)
    {
      FontDict *dict = c->start_embed<FontDict> ();
		dict->serialize (c, _.first, opszr, _.second);
		return c->head - (const char*)dict;
	      })
    | hb_sink (sizes)
    ;
    unsigned data_size = c->head - data_base;
    c->pop_pack (false);

    if (unlikely (sizes.in_error ())) return_trace (false);

    /* It just happens that the above is packed right after the header below.
     * Such a hack. */

    /* serialize INDEX header */
    return_trace (CFFIndex<COUNT>::serialize_header (c, hb_iter (sizes), data_size));
  }
};

/* FDSelect */
struct FDSelect0 {
  bool sanitize (hb_sanitize_context_t *c, unsigned int fdcount) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!(c->check_struct (this))))
      return_trace (false);
    hb_barrier ();
    if (unlikely (!c->check_array (fds, c->get_num_glyphs ())))
      return_trace (false);

    return_trace (true);
  }

  unsigned get_fd (hb_codepoint_t glyph) const
  { return fds[glyph]; }

  hb_pair_t<unsigned, hb_codepoint_t> get_fd_range (hb_codepoint_t glyph) const
  { return {fds[glyph], glyph + 1}; }

  unsigned int get_size (unsigned int num_glyphs) const
  { return HBUINT8::static_size * num_glyphs; }

  HBUINT8     fds[HB_VAR_ARRAY];

  DEFINE_SIZE_MIN (0);
};

template <typename GID_TYPE, typename FD_TYPE>
struct FDSelect3_4_Range
{
  bool sanitize (hb_sanitize_context_t *c, const void * /*nullptr*/, unsigned int fdcount) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  hb_barrier () &&
		  first < c->get_num_glyphs () && (fd < fdcount));
  }

  GID_TYPE    first;
  FD_TYPE     fd;
  public:
  DEFINE_SIZE_STATIC (GID_TYPE::static_size + FD_TYPE::static_size);
};

template <typename GID_TYPE, typename FD_TYPE>
struct FDSelect3_4
{
  unsigned int get_size () const
  { return GID_TYPE::static_size * 2 + ranges.get_size (); }

  bool sanitize (hb_sanitize_context_t *c, unsigned int fdcount) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!(c->check_struct (this) &&
		    ranges.sanitize (c, nullptr, fdcount) &&
		    hb_barrier () &&
		    (nRanges () != 0) &&
		    ranges[0].first == 0)))
      return_trace (false);

    for (unsigned int i = 1; i < nRanges (); i++)
      if (unlikely (ranges[i - 1].first >= ranges[i].first))
	return_trace (false);

    if (unlikely (!(sentinel().sanitize (c) &&
		   hb_barrier () &&
		   (sentinel() == c->get_num_glyphs ()))))
      return_trace (false);

    return_trace (true);
  }

  static int _cmp_range (const void *_key, const void *_item)
  {
    hb_codepoint_t glyph = * (hb_codepoint_t *) _key;
    FDSelect3_4_Range<GID_TYPE, FD_TYPE> *range = (FDSelect3_4_Range<GID_TYPE, FD_TYPE> *) _item;

    if (glyph < range[0].first) return -1;
    if (glyph < range[1].first) return 0;
    return +1;
  }

  unsigned get_fd (hb_codepoint_t glyph) const
  {
    auto *range = hb_bsearch (glyph, &ranges[0], nRanges () - 1, sizeof (ranges[0]), _cmp_range);
    return range ? range->fd : ranges[nRanges () - 1].fd;
  }

  hb_pair_t<unsigned, hb_codepoint_t> get_fd_range (hb_codepoint_t glyph) const
  {
    auto *range = hb_bsearch (glyph, &ranges[0], nRanges () - 1, sizeof (ranges[0]), _cmp_range);
    unsigned fd = range ? range->fd : ranges[nRanges () - 1].fd;
    hb_codepoint_t end = range ? range[1].first : ranges[nRanges () - 1].first;
    return {fd, end};
  }

  GID_TYPE        &nRanges ()       { return ranges.len; }
  GID_TYPE         nRanges () const { return ranges.len; }
  GID_TYPE       &sentinel ()       { return StructAfter<GID_TYPE> (ranges[nRanges () - 1]); }
  const GID_TYPE &sentinel () const { return StructAfter<GID_TYPE> (ranges[nRanges () - 1]); }

  ArrayOf<FDSelect3_4_Range<GID_TYPE, FD_TYPE>, GID_TYPE> ranges;
  /* GID_TYPE sentinel */

  DEFINE_SIZE_ARRAY (GID_TYPE::static_size, ranges);
};

typedef FDSelect3_4<HBUINT16, HBUINT8> FDSelect3;
typedef FDSelect3_4_Range<HBUINT16, HBUINT8> FDSelect3_Range;

struct FDSelect
{
  bool serialize (hb_serialize_context_t *c, const FDSelect &src, unsigned int num_glyphs)
  {
    TRACE_SERIALIZE (this);
    unsigned int size = src.get_size (num_glyphs);
    FDSelect *dest = c->allocate_size<FDSelect> (size, false);
    if (unlikely (!dest)) return_trace (false);
    hb_memcpy (dest, &src, size);
    return_trace (true);
  }

  unsigned int get_size (unsigned int num_glyphs) const
  {
    switch (format)
    {
    case 0: hb_barrier (); return format.static_size + u.format0.get_size (num_glyphs);
    case 3: hb_barrier (); return format.static_size + u.format3.get_size ();
    default:return 0;
    }
  }

  unsigned get_fd (hb_codepoint_t glyph) const
  {
    if (this == &Null (FDSelect)) return 0;

    switch (format)
    {
    case 0: hb_barrier (); return u.format0.get_fd (glyph);
    case 3: hb_barrier (); return u.format3.get_fd (glyph);
    default:return 0;
    }
  }
  /* Returns pair of fd and one after last glyph in range. */
  hb_pair_t<unsigned, hb_codepoint_t> get_fd_range (hb_codepoint_t glyph) const
  {
    if (this == &Null (FDSelect)) return {0, 1};

    switch (format)
    {
    case 0: hb_barrier (); return u.format0.get_fd_range (glyph);
    case 3: hb_barrier (); return u.format3.get_fd_range (glyph);
    default:return {0, 1};
    }
  }

  bool sanitize (hb_sanitize_context_t *c, unsigned int fdcount) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!c->check_struct (this)))
      return_trace (false);
    hb_barrier ();

    switch (format)
    {
    case 0: hb_barrier (); return_trace (u.format0.sanitize (c, fdcount));
    case 3: hb_barrier (); return_trace (u.format3.sanitize (c, fdcount));
    default:return_trace (false);
    }
  }

  HBUINT8	format;
  union {
  FDSelect0	format0;
  FDSelect3	format3;
  } u;
  public:
  DEFINE_SIZE_MIN (1);
};

template <typename COUNT>
struct Subrs : CFFIndex<COUNT>
{
  typedef COUNT count_type;
  typedef CFFIndex<COUNT> SUPER;
};

} /* namespace CFF */

#endif /* HB_OT_CFF_COMMON_HH */
