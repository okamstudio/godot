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

#ifndef HB_OT_CFF2_TABLE_HH
#define HB_OT_CFF2_TABLE_HH

#include "hb-ot-cff-common.hh"
#include "hb-subset-cff-common.hh"
#include "hb-draw.hh"
#include "hb-paint.hh"

namespace CFF {

/*
 * CFF2 -- Compact Font Format (CFF) Version 2
 * https://docs.microsoft.com/en-us/typography/opentype/spec/cff2
 */
#define HB_OT_TAG_CFF2 HB_TAG('C','F','F','2')

typedef CFF2Index         CFF2CharStrings;
typedef Subrs<HBUINT32>   CFF2Subrs;

typedef FDSelect3_4<HBUINT32, HBUINT16> FDSelect4;
typedef FDSelect3_4_Range<HBUINT32, HBUINT16> FDSelect4_Range;

struct CFF2FDSelect
{
  bool serialize (hb_serialize_context_t *c, const CFF2FDSelect &src, unsigned int num_glyphs)
  {
    TRACE_SERIALIZE (this);
    unsigned int size = src.get_size (num_glyphs);
    CFF2FDSelect *dest = c->allocate_size<CFF2FDSelect> (size);
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
    case 4: hb_barrier (); return format.static_size + u.format4.get_size ();
    default:return 0;
    }
  }

  hb_codepoint_t get_fd (hb_codepoint_t glyph) const
  {
    if (this == &Null (CFF2FDSelect))
      return 0;

    switch (format)
    {
    case 0: hb_barrier (); return u.format0.get_fd (glyph);
    case 3: hb_barrier (); return u.format3.get_fd (glyph);
    case 4: hb_barrier (); return u.format4.get_fd (glyph);
    default:return 0;
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
    case 4: hb_barrier (); return_trace (u.format4.sanitize (c, fdcount));
    default:return_trace (false);
    }
  }

  HBUINT8	format;
  union {
  FDSelect0	format0;
  FDSelect3	format3;
  FDSelect4	format4;
  } u;
  public:
  DEFINE_SIZE_MIN (2);
};

struct CFF2ItemVariationStore
{
  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  hb_barrier () &&
		  c->check_range (&varStore, size) &&
		  varStore.sanitize (c));
  }

  bool serialize (hb_serialize_context_t *c, const CFF2ItemVariationStore *varStore)
  {
    TRACE_SERIALIZE (this);
    unsigned int size_ = varStore->get_size ();
    CFF2ItemVariationStore *dest = c->allocate_size<CFF2ItemVariationStore> (size_);
    if (unlikely (!dest)) return_trace (false);
    hb_memcpy (dest, varStore, size_);
    return_trace (true);
  }

  unsigned int get_size () const { return HBUINT16::static_size + size; }

  HBUINT16	size;
  ItemVariationStore  varStore;

  DEFINE_SIZE_MIN (2 + ItemVariationStore::min_size);
};

struct cff2_top_dict_values_t : top_dict_values_t<>
{
  void init ()
  {
    top_dict_values_t<>::init ();
    vstoreOffset = 0;
    FDSelectOffset = 0;
  }
  void fini () { top_dict_values_t<>::fini (); }

  int  vstoreOffset;
  int  FDSelectOffset;
};

struct cff2_top_dict_opset_t : top_dict_opset_t<>
{
  static void process_op (op_code_t op, num_interp_env_t& env, cff2_top_dict_values_t& dictval)
  {
    switch (op) {
      case OpCode_FontMatrix:
	{
	  dict_val_t val;
	  val.init ();
	  dictval.add_op (op, env.str_ref);
	  env.clear_args ();
	}
	break;

      case OpCode_vstore:
	dictval.vstoreOffset = env.argStack.pop_int ();
	env.clear_args ();
	break;
      case OpCode_FDSelect:
	dictval.FDSelectOffset = env.argStack.pop_int ();
	env.clear_args ();
	break;

      default:
	SUPER::process_op (op, env, dictval);
	/* Record this operand below if stack is empty, otherwise done */
	if (!env.argStack.is_empty ()) return;
    }

    if (unlikely (env.in_error ())) return;

    dictval.add_op (op, env.str_ref);
  }

  typedef top_dict_opset_t<> SUPER;
};

struct cff2_font_dict_values_t : dict_values_t<op_str_t>
{
  void init ()
  {
    dict_values_t<op_str_t>::init ();
    privateDictInfo.init ();
  }
  void fini () { dict_values_t<op_str_t>::fini (); }

  table_info_t    privateDictInfo;
};

struct cff2_font_dict_opset_t : dict_opset_t
{
  static void process_op (op_code_t op, num_interp_env_t& env, cff2_font_dict_values_t& dictval)
  {
    switch (op) {
      case OpCode_Private:
	dictval.privateDictInfo.offset = env.argStack.pop_uint ();
	dictval.privateDictInfo.size = env.argStack.pop_uint ();
	env.clear_args ();
	break;

      default:
	SUPER::process_op (op, env);
	if (!env.argStack.is_empty ())
	  return;
    }

    if (unlikely (env.in_error ())) return;

    dictval.add_op (op, env.str_ref);
  }

  private:
  typedef dict_opset_t SUPER;
};

template <typename VAL>
struct cff2_private_dict_values_base_t : dict_values_t<VAL>
{
  void init ()
  {
    dict_values_t<VAL>::init ();
    subrsOffset = 0;
    localSubrs = &Null (CFF2Subrs);
    ivs = 0;
  }
  void fini () { dict_values_t<VAL>::fini (); }

  int                subrsOffset;
  const CFF2Subrs   *localSubrs;
  unsigned int      ivs;
};

typedef cff2_private_dict_values_base_t<op_str_t> cff2_private_dict_values_subset_t;
typedef cff2_private_dict_values_base_t<num_dict_val_t> cff2_private_dict_values_t;

struct cff2_priv_dict_interp_env_t : num_interp_env_t
{
  cff2_priv_dict_interp_env_t (const hb_ubytes_t &str) :
    num_interp_env_t (str) {}

  void process_vsindex ()
  {
    if (likely (!seen_vsindex))
    {
      set_ivs (argStack.pop_uint ());
    }
    seen_vsindex = true;
  }

  unsigned int get_ivs () const { return ivs; }
  void	 set_ivs (unsigned int ivs_) { ivs = ivs_; }

  protected:
  unsigned int  ivs = 0;
  bool	  seen_vsindex = false;
};

struct cff2_private_dict_opset_t : dict_opset_t
{
  static void process_op (op_code_t op, cff2_priv_dict_interp_env_t& env, cff2_private_dict_values_t& dictval)
  {
    num_dict_val_t val;
    val.init ();

    switch (op) {
      case OpCode_StdHW:
      case OpCode_StdVW:
      case OpCode_BlueScale:
      case OpCode_BlueShift:
      case OpCode_BlueFuzz:
      case OpCode_ExpansionFactor:
      case OpCode_LanguageGroup:
      case OpCode_BlueValues:
      case OpCode_OtherBlues:
      case OpCode_FamilyBlues:
      case OpCode_FamilyOtherBlues:
      case OpCode_StemSnapH:
      case OpCode_StemSnapV:
	env.clear_args ();
	break;
      case OpCode_Subrs:
	dictval.subrsOffset = env.argStack.pop_int ();
	env.clear_args ();
	break;
      case OpCode_vsindexdict:
	env.process_vsindex ();
	dictval.ivs = env.get_ivs ();
	env.clear_args ();
	break;
      case OpCode_blenddict:
	break;

      default:
	dict_opset_t::process_op (op, env);
	if (!env.argStack.is_empty ()) return;
	break;
    }

    if (unlikely (env.in_error ())) return;

    dictval.add_op (op, env.str_ref, val);
  }
};

struct cff2_private_dict_opset_subset_t : dict_opset_t
{
  static void process_op (op_code_t op, cff2_priv_dict_interp_env_t& env, cff2_private_dict_values_subset_t& dictval)
  {
    switch (op) {
      case OpCode_BlueValues:
      case OpCode_OtherBlues:
      case OpCode_FamilyBlues:
      case OpCode_FamilyOtherBlues:
      case OpCode_StdHW:
      case OpCode_StdVW:
      case OpCode_BlueScale:
      case OpCode_BlueShift:
      case OpCode_BlueFuzz:
      case OpCode_StemSnapH:
      case OpCode_StemSnapV:
      case OpCode_LanguageGroup:
      case OpCode_ExpansionFactor:
	env.clear_args ();
	break;

      case OpCode_blenddict:
	env.clear_args ();
	return;

      case OpCode_Subrs:
	dictval.subrsOffset = env.argStack.pop_int ();
	env.clear_args ();
	break;

      default:
	SUPER::process_op (op, env);
	if (!env.argStack.is_empty ()) return;
	break;
    }

    if (unlikely (env.in_error ())) return;

    dictval.add_op (op, env.str_ref);
  }

  private:
  typedef dict_opset_t SUPER;
};

typedef dict_interpreter_t<cff2_top_dict_opset_t, cff2_top_dict_values_t> cff2_top_dict_interpreter_t;
typedef dict_interpreter_t<cff2_font_dict_opset_t, cff2_font_dict_values_t> cff2_font_dict_interpreter_t;

struct CFF2FDArray : FDArray<HBUINT32>
{
  /* FDArray::serialize does not compile without this partial specialization */
  template <typename ITER, typename OP_SERIALIZER>
  bool serialize (hb_serialize_context_t *c, ITER it, OP_SERIALIZER& opszr)
  { return FDArray<HBUINT32>::serialize<cff2_font_dict_values_t, table_info_t> (c, it, opszr); }
};

} /* namespace CFF */

namespace OT {

using namespace CFF;

struct cff2
{
  static constexpr hb_tag_t tableTag = HB_OT_TAG_CFF2;

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  hb_barrier () &&
		  likely (version.major == 2));
  }

  template <typename PRIVOPSET, typename PRIVDICTVAL>
  struct accelerator_templ_t
  {
    static constexpr hb_tag_t tableTag = cff2::tableTag;

    accelerator_templ_t (hb_face_t *face)
    {
      if (!face) return;

      topDict.init ();
      fontDicts.init ();
      privateDicts.init ();

      this->blob = sc.reference_table<cff2> (face);

      /* setup for run-time santization */
      sc.init (this->blob);
      sc.start_processing ();

      const OT::cff2 *cff2 = this->blob->template as<OT::cff2> ();

      if (cff2 == &Null (OT::cff2))
        goto fail;

      { /* parse top dict */
	hb_ubytes_t topDictStr = (cff2 + cff2->topDict).as_ubytes (cff2->topDictSize);
	if (unlikely (!topDictStr.sanitize (&sc))) goto fail;
	hb_barrier ();
	num_interp_env_t env (topDictStr);
	cff2_top_dict_interpreter_t top_interp (env);
	topDict.init ();
	if (unlikely (!top_interp.interpret (topDict))) goto fail;
      }

      globalSubrs = &StructAtOffsetOrNull<CFF2Subrs> (cff2, cff2->topDict + cff2->topDictSize, sc);
      varStore = &StructAtOffsetOrNull<CFF2ItemVariationStore> (cff2, topDict.vstoreOffset, sc);
      charStrings = &StructAtOffsetOrNull<CFF2CharStrings> (cff2, topDict.charStringsOffset, sc);
      fdArray = &StructAtOffsetOrNull<CFF2FDArray> (cff2, topDict.FDArrayOffset, sc);
      fdSelect = &StructAtOffsetOrNull<CFF2FDSelect> (cff2, topDict.FDSelectOffset, sc, fdArray->count);

      if (charStrings == &Null (CFF2CharStrings) ||
	  globalSubrs == &Null (CFF2Subrs) ||
	  fdArray == &Null (CFF2FDArray))
        goto fail;

      num_glyphs = charStrings->count;
      if (num_glyphs != sc.get_num_glyphs ())
        goto fail;

      fdCount = fdArray->count;
      if (!privateDicts.resize (fdCount))
        goto fail;

      /* parse font dicts and gather private dicts */
      for (unsigned int i = 0; i < fdCount; i++)
      {
	const hb_ubytes_t fontDictStr = (*fdArray)[i];
	if (unlikely (!fontDictStr.sanitize (&sc))) goto fail;
	hb_barrier ();
	cff2_font_dict_values_t  *font;
	num_interp_env_t env (fontDictStr);
	cff2_font_dict_interpreter_t font_interp (env);
	font = fontDicts.push ();
	if (unlikely (font == &Crap (cff2_font_dict_values_t))) goto fail;
	font->init ();
	if (unlikely (!font_interp.interpret (*font))) goto fail;

	const hb_ubytes_t privDictStr = StructAtOffsetOrNull<UnsizedByteStr> (cff2, font->privateDictInfo.offset, sc, font->privateDictInfo.size).as_ubytes (font->privateDictInfo.size);
	if (unlikely (privDictStr == (const unsigned char *) &Null (UnsizedByteStr))) goto fail;
	cff2_priv_dict_interp_env_t env2 (privDictStr);
	dict_interpreter_t<PRIVOPSET, PRIVDICTVAL, cff2_priv_dict_interp_env_t> priv_interp (env2);
	privateDicts[i].init ();
	if (unlikely (!priv_interp.interpret (privateDicts[i]))) goto fail;

	privateDicts[i].localSubrs = &StructAtOffsetOrNull<CFF2Subrs> (&privDictStr[0], privateDicts[i].subrsOffset, sc);
      }

      return;

      fail:
        _fini ();
    }
    ~accelerator_templ_t () { _fini (); }
    void _fini ()
    {
      sc.end_processing ();
      topDict.fini ();
      fontDicts.fini ();
      privateDicts.fini ();
      hb_blob_destroy (blob);
      blob = nullptr;
    }

    hb_vector_t<uint16_t> *create_glyph_to_sid_map () const
    {
      return nullptr;
    }

    hb_blob_t *get_blob () const { return blob; }

    bool is_valid () const { return blob; }

    protected:
    hb_sanitize_context_t	sc;

    public:
    hb_blob_t			*blob = nullptr;
    cff2_top_dict_values_t	topDict;
    const CFF2Subrs		*globalSubrs = nullptr;
    const CFF2ItemVariationStore	*varStore = nullptr;
    const CFF2CharStrings	*charStrings = nullptr;
    const CFF2FDArray		*fdArray = nullptr;
    const CFF2FDSelect		*fdSelect = nullptr;
    unsigned int		fdCount = 0;

    hb_vector_t<cff2_font_dict_values_t>     fontDicts;
    hb_vector_t<PRIVDICTVAL>  privateDicts;

    unsigned int	      num_glyphs = 0;
  };

  struct accelerator_t : accelerator_templ_t<cff2_private_dict_opset_t, cff2_private_dict_values_t>
  {
    accelerator_t (hb_face_t *face) : accelerator_templ_t (face) {}

    HB_INTERNAL bool get_extents (hb_font_t *font,
				  hb_codepoint_t glyph,
				  hb_glyph_extents_t *extents) const;
    HB_INTERNAL bool paint_glyph (hb_font_t *font, hb_codepoint_t glyph, hb_paint_funcs_t *funcs, void *data, hb_color_t foreground) const;
    HB_INTERNAL bool get_path (hb_font_t *font, hb_codepoint_t glyph, hb_draw_session_t &draw_session) const;
    HB_INTERNAL bool get_path_at (hb_font_t *font, hb_codepoint_t glyph, hb_draw_session_t &draw_session, hb_array_t<const int> coords) const;
  };

  struct accelerator_subset_t : accelerator_templ_t<cff2_private_dict_opset_subset_t, cff2_private_dict_values_subset_t>
  {
    accelerator_subset_t (hb_face_t *face) : SUPER (face) {}
    ~accelerator_subset_t ()
    {
      if (cff_accelerator)
	cff_subset_accelerator_t::destroy (cff_accelerator);
    }

    HB_INTERNAL bool subset (hb_subset_context_t *c) const;
    HB_INTERNAL bool serialize (hb_serialize_context_t *c,
				struct cff2_subset_plan &plan,
				hb_array_t<int> normalized_coords) const;

    mutable CFF::cff_subset_accelerator_t* cff_accelerator = nullptr;

    typedef accelerator_templ_t<cff2_private_dict_opset_subset_t, cff2_private_dict_values_subset_t> SUPER;
  };

  public:
  FixedVersion<HBUINT8>		version;	/* Version of CFF2 table. set to 0x0200u */
  NNOffsetTo<TopDict, HBUINT8>	topDict;	/* headerSize = Offset to Top DICT. */
  HBUINT16			topDictSize;	/* Top DICT size */

  public:
  DEFINE_SIZE_STATIC (5);
};

struct cff2_accelerator_t : cff2::accelerator_t {
  cff2_accelerator_t (hb_face_t *face) : cff2::accelerator_t (face) {}
};

struct cff2_subset_accelerator_t : cff2::accelerator_subset_t {
  cff2_subset_accelerator_t (hb_face_t *face) : cff2::accelerator_subset_t (face) {}
};

} /* namespace OT */

#endif /* HB_OT_CFF2_TABLE_HH */
