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

#include "hb.hh"

#ifndef HB_NO_SUBSET_CFF

#include "hb-open-type.hh"
#include "hb-ot-cff2-table.hh"
#include "hb-set.h"
#include "hb-subset-plan.hh"
#include "hb-subset-cff-common.hh"
#include "hb-cff2-interp-cs.hh"

using namespace CFF;

struct cff2_sub_table_info_t : cff_sub_table_info_t
{
  cff2_sub_table_info_t ()
    : cff_sub_table_info_t (),
      var_store_link (0)
  {}

  objidx_t  var_store_link;
};

struct cff2_top_dict_op_serializer_t : cff_top_dict_op_serializer_t<>
{
  bool serialize (hb_serialize_context_t *c,
		  const op_str_t &opstr,
		  const cff2_sub_table_info_t &info) const
  {
    TRACE_SERIALIZE (this);

    switch (opstr.op)
    {
      case OpCode_vstore:
        if (info.var_store_link)
	  return_trace (FontDict::serialize_link4_op(c, opstr.op, info.var_store_link));
	else
	  return_trace (true);

      default:
	return_trace (cff_top_dict_op_serializer_t<>::serialize (c, opstr, info));
    }
  }
};

struct cff2_cs_opset_flatten_t : cff2_cs_opset_t<cff2_cs_opset_flatten_t, flatten_param_t, blend_arg_t>
{
  static void flush_args_and_op (op_code_t op, cff2_cs_interp_env_t<blend_arg_t> &env, flatten_param_t& param)
  {
    switch (op)
    {
      case OpCode_return:
      case OpCode_endchar:
	/* dummy opcodes in CFF2. ignore */
	break;

      case OpCode_hstem:
      case OpCode_hstemhm:
      case OpCode_vstem:
      case OpCode_vstemhm:
      case OpCode_hintmask:
      case OpCode_cntrmask:
	if (param.drop_hints)
	{
	  env.clear_args ();
	  return;
	}
	HB_FALLTHROUGH;

      default:
	SUPER::flush_args_and_op (op, env, param);
	break;
    }
  }

  static void flush_args (cff2_cs_interp_env_t<blend_arg_t> &env, flatten_param_t& param)
  {
    for (unsigned int i = 0; i < env.argStack.get_count ();)
    {
      const blend_arg_t &arg = env.argStack[i];
      if (arg.blending ())
      {
	if (unlikely (!((arg.numValues > 0) && (env.argStack.get_count () >= arg.numValues))))
	{
	  env.set_error ();
	  return;
	}
	flatten_blends (arg, i, env, param);
	i += arg.numValues;
      }
      else
      {
	str_encoder_t  encoder (param.flatStr);
	encoder.encode_num_cs (arg);
	i++;
      }
    }
    SUPER::flush_args (env, param);
  }

  static void flatten_blends (const blend_arg_t &arg, unsigned int i, cff2_cs_interp_env_t<blend_arg_t> &env, flatten_param_t& param)
  {
    /* flatten the default values */
    str_encoder_t  encoder (param.flatStr);
    for (unsigned int j = 0; j < arg.numValues; j++)
    {
      const blend_arg_t &arg1 = env.argStack[i + j];
      if (unlikely (!((arg1.blending () && (arg.numValues == arg1.numValues) && (arg1.valueIndex == j) &&
	      (arg1.deltas.length == env.get_region_count ())))))
      {
	env.set_error ();
	return;
      }
      encoder.encode_num_cs (arg1);
    }
    /* flatten deltas for each value */
    for (unsigned int j = 0; j < arg.numValues; j++)
    {
      const blend_arg_t &arg1 = env.argStack[i + j];
      for (unsigned int k = 0; k < arg1.deltas.length; k++)
	encoder.encode_num_cs (arg1.deltas[k]);
    }
    /* flatten the number of values followed by blend operator */
    encoder.encode_int (arg.numValues);
    encoder.encode_op (OpCode_blendcs);
  }

  static void flush_op (op_code_t op, cff2_cs_interp_env_t<blend_arg_t> &env, flatten_param_t& param)
  {
    switch (op)
    {
      case OpCode_return:
      case OpCode_endchar:
	return;
      default:
	str_encoder_t  encoder (param.flatStr);
	encoder.encode_op (op);
    }
  }

  static void flush_hintmask (op_code_t op, cff2_cs_interp_env_t<blend_arg_t> &env, flatten_param_t& param)
  {
    SUPER::flush_hintmask (op, env, param);
    if (!param.drop_hints)
    {
      str_encoder_t  encoder (param.flatStr);
      for (unsigned int i = 0; i < env.hintmask_size; i++)
	encoder.encode_byte (env.str_ref[i]);
    }
  }

  private:
  typedef cff2_cs_opset_t<cff2_cs_opset_flatten_t, flatten_param_t, blend_arg_t> SUPER;
  typedef cs_opset_t<blend_arg_t, cff2_cs_opset_flatten_t, cff2_cs_opset_flatten_t, cff2_cs_interp_env_t<blend_arg_t>, flatten_param_t> CSOPSET;
};

struct cff2_cs_opset_subr_subset_t : cff2_cs_opset_t<cff2_cs_opset_subr_subset_t, subr_subset_param_t, blend_arg_t>
{
  static void process_op (op_code_t op, cff2_cs_interp_env_t<blend_arg_t> &env, subr_subset_param_t& param)
  {
    switch (op) {

      case OpCode_return:
	param.current_parsed_str->set_parsed ();
	env.return_from_subr ();
	param.set_current_str (env, false);
	break;

      case OpCode_endchar:
	param.current_parsed_str->set_parsed ();
	SUPER::process_op (op, env, param);
	break;

      case OpCode_callsubr:
	process_call_subr (op, CSType_LocalSubr, env, param, env.localSubrs, param.local_closure);
	break;

      case OpCode_callgsubr:
	process_call_subr (op, CSType_GlobalSubr, env, param, env.globalSubrs, param.global_closure);
	break;

      default:
	SUPER::process_op (op, env, param);
	param.current_parsed_str->add_op (op, env.str_ref);
	break;
    }
  }

  protected:
  static void process_call_subr (op_code_t op, cs_type_t type,
				 cff2_cs_interp_env_t<blend_arg_t> &env, subr_subset_param_t& param,
				 cff2_biased_subrs_t& subrs, hb_set_t *closure)
  {
    byte_str_ref_t    str_ref = env.str_ref;
    env.call_subr (subrs, type);
    param.current_parsed_str->add_call_op (op, str_ref, env.context.subr_num);
    closure->add (env.context.subr_num);
    param.set_current_str (env, true);
  }

  private:
  typedef cff2_cs_opset_t<cff2_cs_opset_subr_subset_t, subr_subset_param_t, blend_arg_t> SUPER;
};

struct cff2_subr_subsetter_t : subr_subsetter_t<cff2_subr_subsetter_t, CFF2Subrs, const OT::cff2::accelerator_subset_t, cff2_cs_interp_env_t<blend_arg_t>, cff2_cs_opset_subr_subset_t>
{
  cff2_subr_subsetter_t (const OT::cff2::accelerator_subset_t &acc_, const hb_subset_plan_t *plan_)
    : subr_subsetter_t (acc_, plan_) {}

  static void complete_parsed_str (cff2_cs_interp_env_t<blend_arg_t> &env, subr_subset_param_t& param, parsed_cs_str_t &charstring)
  {
    /* vsindex is inserted at the beginning of the charstring as necessary */
    if (env.seen_vsindex ())
    {
      number_t  ivs;
      ivs.set_int ((int)env.get_ivs ());
      charstring.set_prefix (ivs, OpCode_vsindexcs);
    }
  }
};

struct cff2_private_blend_encoder_param_t
{
  cff2_private_blend_encoder_param_t (hb_serialize_context_t *c,
				      const CFF2ItemVariationStore *varStore,
				      hb_array_t<int> normalized_coords) :
    c (c), varStore (varStore), normalized_coords (normalized_coords) {}

  void init () {}

  void process_blend ()
  {
    if (!seen_blend)
    {
      region_count = varStore->varStore.get_region_index_count (ivs);
      scalars.resize_exact (region_count);
      varStore->varStore.get_region_scalars (ivs, normalized_coords.arrayZ, normalized_coords.length,
					     &scalars[0], region_count);
      seen_blend = true;
    }
  }

  double blend_deltas (hb_array_t<const number_t> deltas) const
  {
    double v = 0;
    if (likely (scalars.length == deltas.length))
    {
      unsigned count = scalars.length;
      for (unsigned i = 0; i < count; i++)
	v += (double) scalars.arrayZ[i] * deltas.arrayZ[i].to_real ();
    }
    return v;
  }


  hb_serialize_context_t *c = nullptr;
  bool seen_blend = false;
  unsigned ivs = 0;
  unsigned region_count = 0;
  hb_vector_t<float> scalars;
  const	 CFF2ItemVariationStore *varStore = nullptr;
  hb_array_t<int> normalized_coords;
};

struct cff2_private_dict_blend_opset_t : dict_opset_t
{
  static void process_arg_blend (cff2_private_blend_encoder_param_t& param,
				 number_t &arg,
				 const hb_array_t<const number_t> blends,
				 unsigned n, unsigned i)
  {
    arg.set_int (round (arg.to_real () + param.blend_deltas (blends)));
  }

  static void process_blend (cff2_priv_dict_interp_env_t& env, cff2_private_blend_encoder_param_t& param)
  {
    unsigned int n, k;

    param.process_blend ();
    k = param.region_count;
    n = env.argStack.pop_uint ();
    /* copy the blend values into blend array of the default values */
    unsigned int start = env.argStack.get_count () - ((k+1) * n);
    /* let an obvious error case fail, but note CFF2 spec doesn't forbid n==0 */
    if (unlikely (start > env.argStack.get_count ()))
    {
      env.set_error ();
      return;
    }
    for (unsigned int i = 0; i < n; i++)
    {
      const hb_array_t<const number_t> blends = env.argStack.sub_array (start + n + (i * k), k);
      process_arg_blend (param, env.argStack[start + i], blends, n, i);
    }

    /* pop off blend values leaving default values now adorned with blend values */
    env.argStack.pop (k * n);
  }

  static void process_op (op_code_t op, cff2_priv_dict_interp_env_t& env, cff2_private_blend_encoder_param_t& param)
  {
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
	break;
      case OpCode_vsindexdict:
	env.process_vsindex ();
	param.ivs = env.get_ivs ();
	env.clear_args ();
	return;
      case OpCode_blenddict:
	process_blend (env, param);
	return;

      default:
	dict_opset_t::process_op (op, env);
	if (!env.argStack.is_empty ()) return;
	break;
    }

    if (unlikely (env.in_error ())) return;

    // Write args then op

    str_buff_t str;
    str_encoder_t encoder (str);

    unsigned count = env.argStack.get_count ();
    for (unsigned i = 0; i < count; i++)
      encoder.encode_num_tp (env.argStack[i]);

    encoder.encode_op (op);

    auto bytes = str.as_bytes ();
    param.c->embed (&bytes, bytes.length);

    env.clear_args ();
  }
};

struct cff2_private_dict_op_serializer_t : op_serializer_t
{
  cff2_private_dict_op_serializer_t (bool desubroutinize_, bool drop_hints_, bool pinned_,
				     const CFF::CFF2ItemVariationStore* varStore_,
				     hb_array_t<int> normalized_coords_)
    : desubroutinize (desubroutinize_), drop_hints (drop_hints_), pinned (pinned_),
      varStore (varStore_), normalized_coords (normalized_coords_) {}

  bool serialize (hb_serialize_context_t *c,
		  const op_str_t &opstr,
		  objidx_t subrs_link) const
  {
    TRACE_SERIALIZE (this);

    if (drop_hints && dict_opset_t::is_hint_op (opstr.op))
      return_trace (true);

    if (opstr.op == OpCode_Subrs)
    {
      if (desubroutinize || !subrs_link)
	return_trace (true);
      else
	return_trace (FontDict::serialize_link2_op (c, opstr.op, subrs_link));
    }

    if (pinned)
    {
      // Reinterpret opstr and process blends.
      cff2_priv_dict_interp_env_t env {hb_ubytes_t (opstr.ptr, opstr.length)};
      cff2_private_blend_encoder_param_t param (c, varStore, normalized_coords);
      dict_interpreter_t<cff2_private_dict_blend_opset_t, cff2_private_blend_encoder_param_t, cff2_priv_dict_interp_env_t> interp (env);
      return_trace (interp.interpret (param));
    }

    return_trace (copy_opstr (c, opstr));
  }

  protected:
  const bool desubroutinize;
  const bool drop_hints;
  const bool pinned;
  const CFF::CFF2ItemVariationStore* varStore;
  hb_array_t<int> normalized_coords;
};


namespace OT {
struct cff2_subset_plan
{
  bool create (const OT::cff2::accelerator_subset_t &acc,
	      hb_subset_plan_t *plan)
  {
    /* make sure notdef is first */
    hb_codepoint_t old_glyph;
    if (!plan->old_gid_for_new_gid (0, &old_glyph) || (old_glyph != 0)) return false;

    num_glyphs = plan->num_output_glyphs ();
    orig_fdcount = acc.fdArray->count;

    drop_hints = plan->flags & HB_SUBSET_FLAGS_NO_HINTING;
    pinned = (bool) plan->normalized_coords;
    desubroutinize = plan->flags & HB_SUBSET_FLAGS_DESUBROUTINIZE ||
		     pinned; // For instancing we need this path

 #ifdef HB_EXPERIMENTAL_API
    min_charstrings_off_size = (plan->flags & HB_SUBSET_FLAGS_IFTB_REQUIREMENTS) ? 4 : 0;
 #else
    min_charstrings_off_size = 0;
 #endif

    if (desubroutinize)
    {
      /* Flatten global & local subrs */
      subr_flattener_t<const OT::cff2::accelerator_subset_t, cff2_cs_interp_env_t<blend_arg_t>, cff2_cs_opset_flatten_t>
		    flattener(acc, plan);
      if (!flattener.flatten (subset_charstrings))
	return false;
    }
    else
    {
      cff2_subr_subsetter_t	subr_subsetter (acc, plan);

      /* Subset subrs: collect used subroutines, leaving all unused ones behind */
      if (!subr_subsetter.subset ())
	return false;

      /* encode charstrings, global subrs, local subrs with new subroutine numbers */
      if (!subr_subsetter.encode_charstrings (subset_charstrings, !pinned))
	return false;

      if (!subr_subsetter.encode_globalsubrs (subset_globalsubrs))
	return false;

      /* local subrs */
      if (!subset_localsubrs.resize (orig_fdcount))
	return false;
      for (unsigned int fd = 0; fd < orig_fdcount; fd++)
      {
	subset_localsubrs[fd].init ();
	if (!subr_subsetter.encode_localsubrs (fd, subset_localsubrs[fd]))
	  return false;
      }
    }

    /* FDSelect */
    if (acc.fdSelect != &Null (CFF2FDSelect))
    {
      if (unlikely (!hb_plan_subset_cff_fdselect (plan,
						  orig_fdcount,
						  *(const FDSelect *)acc.fdSelect,
						  subset_fdcount,
						  subset_fdselect_size,
						  subset_fdselect_format,
						  subset_fdselect_ranges,
						  fdmap)))
	return false;
    }
    else
      fdmap.identity (1);

    return true;
  }

  cff2_sub_table_info_t info;

  unsigned int    num_glyphs;
  unsigned int    orig_fdcount = 0;
  unsigned int    subset_fdcount = 1;
  unsigned int    subset_fdselect_size = 0;
  unsigned int    subset_fdselect_format = 0;
  bool            pinned = false;
  hb_vector_t<code_pair_t>   subset_fdselect_ranges;

  hb_inc_bimap_t   fdmap;

  str_buff_vec_t	    subset_charstrings;
  str_buff_vec_t	    subset_globalsubrs;
  hb_vector_t<str_buff_vec_t> subset_localsubrs;

  bool	    drop_hints = false;
  bool	    desubroutinize = false;

  unsigned  min_charstrings_off_size = 0;
};
} // namespace OT

static bool _serialize_cff2_charstrings (hb_serialize_context_t *c,
			     cff2_subset_plan &plan,
			     const OT::cff2::accelerator_subset_t  &acc)
{
  c->push ();

  unsigned data_size = 0;
  unsigned total_size = CFF2CharStrings::total_size (plan.subset_charstrings, &data_size, plan.min_charstrings_off_size);
  if (unlikely (!c->start_zerocopy (total_size)))
    return false;

  auto *cs = c->start_embed<CFF2CharStrings> ();
  if (unlikely (!cs->serialize (c, plan.subset_charstrings, &data_size, plan.min_charstrings_off_size)))
  {
    c->pop_discard ();
    return false;
  }

  plan.info.char_strings_link = c->pop_pack (false);
  return true;
}

bool
OT::cff2::accelerator_subset_t::serialize (hb_serialize_context_t *c,
					   struct cff2_subset_plan &plan,
					   hb_array_t<int> normalized_coords) const
{
  /* push charstrings onto the object stack first which will ensure it packs as the last
     object in the table. Keeping the chastrings last satisfies the requirements for patching
     via IFTB. If this ordering needs to be changed in the future, charstrings should be left
     at the end whenever HB_SUBSET_FLAGS_ITFB_REQUIREMENTS is enabled. */
  if (!_serialize_cff2_charstrings(c, plan, *this))
    return false;

  /* private dicts & local subrs */
  hb_vector_t<table_info_t>  private_dict_infos;
  if (unlikely (!private_dict_infos.resize (plan.subset_fdcount))) return false;

  for (int i = (int)privateDicts.length; --i >= 0 ;)
  {
    if (plan.fdmap.has (i))
    {
      objidx_t	subrs_link = 0;

      if (plan.subset_localsubrs[i].length > 0)
      {
	auto *dest = c->push <CFF2Subrs> ();
	if (likely (dest->serialize (c, plan.subset_localsubrs[i])))
	  subrs_link = c->pop_pack (false);
	else
	{
	  c->pop_discard ();
	  return false;
	}
      }
      auto *pd = c->push<PrivateDict> ();
      cff2_private_dict_op_serializer_t privSzr (plan.desubroutinize, plan.drop_hints, plan.pinned,
						 varStore, normalized_coords);
      if (likely (pd->serialize (c, privateDicts[i], privSzr, subrs_link)))
      {
	unsigned fd = plan.fdmap[i];
	private_dict_infos[fd].size = c->length ();
	private_dict_infos[fd].link = c->pop_pack ();
      }
      else
      {
	c->pop_discard ();
	return false;
      }
    }
  }

  /* FDSelect */
  if (fdSelect != &Null (CFF2FDSelect))
  {
    c->push ();
    if (likely (hb_serialize_cff_fdselect (c, plan.num_glyphs, *(const FDSelect *)fdSelect,
					   plan.orig_fdcount,
					   plan.subset_fdselect_format, plan.subset_fdselect_size,
					   plan.subset_fdselect_ranges)))
      plan.info.fd_select.link = c->pop_pack ();
    else
    {
      c->pop_discard ();
      return false;
    }
  }

  /* FDArray (FD Index) */
  {
    auto *fda = c->push<CFF2FDArray> ();
    cff_font_dict_op_serializer_t fontSzr;
    auto it =
    + hb_zip (+ hb_iter (fontDicts)
	      | hb_filter ([&] (const cff2_font_dict_values_t &_)
		{ return plan.fdmap.has (&_ - &fontDicts[0]); }),
	      hb_iter (private_dict_infos))
    ;
    if (unlikely (!fda->serialize (c, it, fontSzr)))
    {
      c->pop_discard ();
      return false;
    }
    plan.info.fd_array_link = c->pop_pack (false);
  }

  /* variation store */
  if (varStore != &Null (CFF2ItemVariationStore) &&
      !plan.pinned)
  {
    auto *dest = c->push<CFF2ItemVariationStore> ();
    if (unlikely (!dest->serialize (c, varStore)))
    {
      c->pop_discard ();
      return false;
    }
    plan.info.var_store_link = c->pop_pack (false);
  }

  OT::cff2 *cff2 = c->allocate_min<OT::cff2> ();
  if (unlikely (!cff2)) return false;

  /* header */
  cff2->version.major = 0x02;
  cff2->version.minor = 0x00;
  cff2->topDict = OT::cff2::static_size;

  /* top dict */
  {
    TopDict &dict = cff2 + cff2->topDict;
    cff2_top_dict_op_serializer_t topSzr;
    if (unlikely (!dict.serialize (c, topDict, topSzr, plan.info))) return false;
    cff2->topDictSize = c->head - (const char *)&dict;
  }

  /* global subrs */
  {
    auto *dest = c->start_embed <CFF2Subrs> ();
    return dest->serialize (c, plan.subset_globalsubrs);
  }
}

bool
OT::cff2::accelerator_subset_t::subset (hb_subset_context_t *c) const
{
  cff2_subset_plan cff2_plan;

  if (unlikely (!cff2_plan.create (*this, c->plan))) return false;
  return serialize (c->serializer, cff2_plan,
		    c->plan->normalized_coords.as_array ());
}

#endif
