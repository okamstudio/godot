/*
 * Copyright © 2017  Google, Inc.
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

#ifndef HB_OT_KERN_TABLE_HH
#define HB_OT_KERN_TABLE_HH

#include "hb-aat-layout-kerx-table.hh"


/*
 * kern -- Kerning
 * https://docs.microsoft.com/en-us/typography/opentype/spec/kern
 * https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6kern.html
 */
#define HB_OT_TAG_kern HB_TAG('k','e','r','n')


namespace OT {


template <typename KernSubTableHeader>
struct KernSubTableFormat3
{
  int get_kerning (hb_codepoint_t left, hb_codepoint_t right) const
  {
    hb_array_t<const FWORD> kernValue = kernValueZ.as_array (kernValueCount);
    hb_array_t<const HBUINT8> leftClass = StructAfter<const UnsizedArrayOf<HBUINT8>> (kernValue).as_array (glyphCount);
    hb_array_t<const HBUINT8> rightClass = StructAfter<const UnsizedArrayOf<HBUINT8>> (leftClass).as_array (glyphCount);
    hb_array_t<const HBUINT8> kernIndex = StructAfter<const UnsizedArrayOf<HBUINT8>> (rightClass).as_array (leftClassCount * rightClassCount);

    unsigned int leftC = leftClass[left];
    unsigned int rightC = rightClass[right];
    if (unlikely (leftC >= leftClassCount || rightC >= rightClassCount))
      return 0;
    unsigned int i = leftC * rightClassCount + rightC;
    return kernValue[kernIndex[i]];
  }

  bool apply (AAT::hb_aat_apply_context_t *c) const
  {
    TRACE_APPLY (this);

    if (!c->plan->requested_kerning)
      return false;

    if (header.coverage & header.Backwards)
      return false;

    hb_kern_machine_t<KernSubTableFormat3> machine (*this, header.coverage & header.CrossStream);
    machine.kern (c->font, c->buffer, c->plan->kern_mask);

    return_trace (true);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  hb_barrier () &&
		  c->check_range (kernValueZ,
				  kernValueCount * sizeof (FWORD) +
				  glyphCount * 2 +
				  leftClassCount * rightClassCount));
  }

  template <typename set_t>
  void collect_glyphs (set_t &left_set, set_t &right_set, unsigned num_glyphs) const
  {
    set_t set;
    if (likely (glyphCount))
      set.add_range (0, glyphCount - 1);
    left_set.union_ (set);
    right_set.union_ (set);
  }

  protected:
  KernSubTableHeader
		header;
  HBUINT16	glyphCount;	/* The number of glyphs in this font. */
  HBUINT8	kernValueCount;	/* The number of kerning values. */
  HBUINT8	leftClassCount;	/* The number of left-hand classes. */
  HBUINT8	rightClassCount;/* The number of right-hand classes. */
  HBUINT8	flags;		/* Set to zero (reserved for future use). */
  UnsizedArrayOf<FWORD>
		kernValueZ;	/* The kerning values.
				 * Length kernValueCount. */
#if 0
  UnsizedArrayOf<HBUINT8>
		leftClass;	/* The left-hand classes.
				 * Length glyphCount. */
  UnsizedArrayOf<HBUINT8>
		rightClass;	/* The right-hand classes.
				 * Length glyphCount. */
  UnsizedArrayOf<HBUINT8>kernIndex;
				/* The indices into the kernValue array.
				 * Length leftClassCount * rightClassCount */
#endif
  public:
  DEFINE_SIZE_ARRAY (KernSubTableHeader::static_size + 6, kernValueZ);
};

template <typename KernSubTableHeader>
struct KernSubTable
{
  unsigned int get_size () const { return u.header.length; }
  unsigned int get_type () const { return u.header.format; }

  int get_kerning (hb_codepoint_t left, hb_codepoint_t right) const
  {
    switch (get_type ()) {
    /* This method hooks up to hb_font_t's get_h_kerning.  Only support Format0. */
    case 0: hb_barrier (); return u.format0.get_kerning (left, right);
    default:return 0;
    }
  }

  template <typename context_t, typename ...Ts>
  typename context_t::return_t dispatch (context_t *c, Ts&&... ds) const
  {
    unsigned int subtable_type = get_type ();
    TRACE_DISPATCH (this, subtable_type);
    switch (subtable_type) {
    case 0:	return_trace (c->dispatch (u.format0));
#ifndef HB_NO_AAT_SHAPE
    case 1:	return_trace (c->dispatch (u.format1, std::forward<Ts> (ds)...));
#endif
    case 2:	return_trace (c->dispatch (u.format2));
#ifndef HB_NO_AAT_SHAPE
    case 3:	return_trace (c->dispatch (u.format3, std::forward<Ts> (ds)...));
#endif
    default:	return_trace (c->default_return_value ());
    }
  }

  template <typename set_t>
  void collect_glyphs (set_t &left_set, set_t &right_set, unsigned num_glyphs) const
  {
    unsigned int subtable_type = get_type ();
    switch (subtable_type) {
    case 0:	u.format0.collect_glyphs (left_set, right_set, num_glyphs); return;
    case 1:	u.format1.collect_glyphs (left_set, right_set, num_glyphs); return;
    case 2:	u.format2.collect_glyphs (left_set, right_set, num_glyphs); return;
    case 3:	u.format3.collect_glyphs (left_set, right_set, num_glyphs); return;
    default:	return;
    }
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!(u.header.sanitize (c) &&
		    hb_barrier () &&
		    u.header.length >= u.header.min_size &&
		    c->check_range (this, u.header.length)))) return_trace (false);

    return_trace (dispatch (c));
  }

  public:
  union {
  KernSubTableHeader				header;
  AAT::KerxSubTableFormat0<KernSubTableHeader>	format0;
  AAT::KerxSubTableFormat1<KernSubTableHeader>	format1;
  AAT::KerxSubTableFormat2<KernSubTableHeader>	format2;
  KernSubTableFormat3<KernSubTableHeader>	format3;
  } u;
  public:
  DEFINE_SIZE_MIN (KernSubTableHeader::static_size);
};


struct KernOTSubTableHeader
{
  static constexpr bool apple = false;
  typedef AAT::ObsoleteTypes Types;

  unsigned   tuple_count () const { return 0; }
  bool     is_horizontal () const { return (coverage & Horizontal); }

  enum Coverage
  {
    Horizontal	= 0x01u,
    Minimum	= 0x02u,
    CrossStream	= 0x04u,
    Override	= 0x08u,

    /* Not supported: */
    Backwards	= 0x00u,
    Variation	= 0x00u,
  };

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  public:
  HBUINT16	versionZ;	/* Unused. */
  HBUINT16	length;		/* Length of the subtable (including this header). */
  HBUINT8	format;		/* Subtable format. */
  HBUINT8	coverage;	/* Coverage bits. */
  public:
  DEFINE_SIZE_STATIC (6);
};

struct KernOT : AAT::KerxTable<KernOT>
{
  friend struct AAT::KerxTable<KernOT>;

  static constexpr hb_tag_t tableTag = HB_OT_TAG_kern;
  static constexpr unsigned minVersion = 0u;

  typedef KernOTSubTableHeader SubTableHeader;
  typedef SubTableHeader::Types Types;
  typedef KernSubTable<SubTableHeader> SubTable;

  protected:
  HBUINT16	version;	/* Version--0x0000u */
  HBUINT16	tableCount;	/* Number of subtables in the kerning table. */
  SubTable	firstSubTable;	/* Subtables. */
  public:
  DEFINE_SIZE_MIN (4);
};


struct KernAATSubTableHeader
{
  static constexpr bool apple = true;
  typedef AAT::ObsoleteTypes Types;

  unsigned   tuple_count () const { return 0; }
  bool     is_horizontal () const { return !(coverage & Vertical); }

  enum Coverage
  {
    Vertical	= 0x80u,
    CrossStream	= 0x40u,
    Variation	= 0x20u,

    /* Not supported: */
    Backwards	= 0x00u,
  };

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  public:
  HBUINT32	length;		/* Length of the subtable (including this header). */
  HBUINT8	coverage;	/* Coverage bits. */
  HBUINT8	format;		/* Subtable format. */
  HBUINT16	tupleIndex;	/* The tuple index (used for variations fonts).
				 * This value specifies which tuple this subtable covers.
				 * Note: We don't implement. */
  public:
  DEFINE_SIZE_STATIC (8);
};

struct KernAAT : AAT::KerxTable<KernAAT>
{
  friend struct AAT::KerxTable<KernAAT>;

  static constexpr hb_tag_t tableTag = HB_OT_TAG_kern;
  static constexpr unsigned minVersion = 0x00010000u;

  typedef KernAATSubTableHeader SubTableHeader;
  typedef SubTableHeader::Types Types;
  typedef KernSubTable<SubTableHeader> SubTable;

  protected:
  HBUINT32	version;	/* Version--0x00010000u */
  HBUINT32	tableCount;	/* Number of subtables in the kerning table. */
  SubTable	firstSubTable;	/* Subtables. */
  public:
  DEFINE_SIZE_MIN (8);
};

struct kern
{
  static constexpr hb_tag_t tableTag = HB_OT_TAG_kern;

  bool     has_data () const { return u.version32; }
  unsigned get_type () const { return u.major; }

  bool has_state_machine () const
  {
    switch (get_type ()) {
    case 0: hb_barrier (); return u.ot.has_state_machine ();
#ifndef HB_NO_AAT_SHAPE
    case 1: hb_barrier (); return u.aat.has_state_machine ();
#endif
    default:return false;
    }
  }

  bool has_cross_stream () const
  {
    switch (get_type ()) {
    case 0: hb_barrier (); return u.ot.has_cross_stream ();
#ifndef HB_NO_AAT_SHAPE
    case 1: hb_barrier (); return u.aat.has_cross_stream ();
#endif
    default:return false;
    }
  }

  int get_h_kerning (hb_codepoint_t left, hb_codepoint_t right) const
  {
    switch (get_type ()) {
    case 0: hb_barrier (); return u.ot.get_h_kerning (left, right);
#ifndef HB_NO_AAT_SHAPE
    case 1: hb_barrier (); return u.aat.get_h_kerning (left, right);
#endif
    default:return 0;
    }
  }

  bool apply (AAT::hb_aat_apply_context_t *c,
	      const AAT::kern_accelerator_data_t *accel_data = nullptr) const
  { return dispatch (c, accel_data); }

  template <typename context_t, typename ...Ts>
  typename context_t::return_t dispatch (context_t *c, Ts&&... ds) const
  {
    unsigned int subtable_type = get_type ();
    TRACE_DISPATCH (this, subtable_type);
    switch (subtable_type) {
    case 0:	return_trace (c->dispatch (u.ot, std::forward<Ts> (ds)...));
#ifndef HB_NO_AAT_SHAPE
    case 1:	return_trace (c->dispatch (u.aat, std::forward<Ts> (ds)...));
#endif
    default:	return_trace (c->default_return_value ());
    }
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (!u.version32.sanitize (c)) return_trace (false);
    hb_barrier ();
    return_trace (dispatch (c));
  }

  AAT::kern_accelerator_data_t create_accelerator_data (unsigned num_glyphs) const
  {
    switch (get_type ()) {
    case 0: hb_barrier (); return u.ot.create_accelerator_data (num_glyphs);
#ifndef HB_NO_AAT_SHAPE
    case 1: hb_barrier (); return u.aat.create_accelerator_data (num_glyphs);
#endif
    default:return AAT::kern_accelerator_data_t ();
    }
  }

  struct accelerator_t
  {
    accelerator_t (hb_face_t *face)
    {
      hb_sanitize_context_t sc;
      this->table = sc.reference_table<kern> (face);
      this->accel_data = this->table->create_accelerator_data (face->get_num_glyphs ());
    }
    ~accelerator_t ()
    {
      this->table.destroy ();
    }

    hb_blob_t *get_blob () const { return table.get_blob (); }

    bool apply (AAT::hb_aat_apply_context_t *c) const
    {
      return table->apply (c, &accel_data);
    }

    hb_blob_ptr_t<kern> table;
    AAT::kern_accelerator_data_t accel_data;
  };

  protected:
  union {
  HBUINT32		version32;
  HBUINT16		major;
  KernOT		ot;
#ifndef HB_NO_AAT_SHAPE
  KernAAT		aat;
#endif
  } u;
  public:
  DEFINE_SIZE_UNION (4, version32);
};

struct kern_accelerator_t : kern::accelerator_t {
  kern_accelerator_t (hb_face_t *face) : kern::accelerator_t (face) {}
};

} /* namespace OT */


#endif /* HB_OT_KERN_TABLE_HH */
