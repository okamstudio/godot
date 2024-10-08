/*
 * Copyright © 2018  Google, Inc.
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
 * Google Author(s): Garret Rieger, Rod Sheeter, Behdad Esfahbod
 */

#include "hb.hh"
#include "hb-open-type.hh"

#include "hb-subset.hh"

#include "hb-open-file.hh"
#include "hb-ot-cmap-table.hh"
#include "hb-ot-glyf-table.hh"
#include "hb-ot-hdmx-table.hh"
#include "hb-ot-head-table.hh"
#include "hb-ot-hhea-table.hh"
#include "hb-ot-hmtx-table.hh"
#include "hb-ot-maxp-table.hh"
#include "OT/Color/CBDT/CBDT.hh"
#include "OT/Color/COLR/COLR.hh"
#include "OT/Color/CPAL/CPAL.hh"
#include "OT/Color/sbix/sbix.hh"
#include "hb-ot-os2-table.hh"
#include "hb-ot-post-table.hh"
#include "hb-ot-post-table-v2subset.hh"
#include "hb-ot-cff1-table.hh"
#include "hb-ot-cff2-table.hh"
#include "hb-ot-vorg-table.hh"
#include "hb-ot-name-table.hh"
#include "hb-ot-layout-base-table.hh"
#include "hb-ot-layout-gsub-table.hh"
#include "hb-ot-layout-gpos-table.hh"
#include "hb-ot-var-avar-table.hh"
#include "hb-ot-var-cvar-table.hh"
#include "hb-ot-var-fvar-table.hh"
#include "hb-ot-var-gvar-table.hh"
#include "hb-ot-var-hvar-table.hh"
#include "hb-ot-var-mvar-table.hh"
#include "hb-ot-math-table.hh"
#include "hb-ot-stat-table.hh"
#include "hb-repacker.hh"
#include "hb-subset-accelerator.hh"

using OT::Layout::GSUB;
using OT::Layout::GPOS;


#ifndef HB_NO_SUBSET_CFF
template<>
struct hb_subset_plan_t::source_table_loader<const OT::cff1>
{
  auto operator () (hb_subset_plan_t *plan)
  HB_AUTO_RETURN (plan->accelerator ? plan->accelerator->cff1_accel :
		  plan->inprogress_accelerator ? plan->inprogress_accelerator->cff1_accel :
		  plan->cff1_accel)
};
template<>
struct hb_subset_plan_t::source_table_loader<const OT::cff2>
{
  auto operator () (hb_subset_plan_t *plan)
  HB_AUTO_RETURN (plan->accelerator ? plan->accelerator->cff2_accel :
		  plan->inprogress_accelerator ? plan->inprogress_accelerator->cff2_accel :
		  plan->cff2_accel)
};
#endif


/**
 * SECTION:hb-subset
 * @title: hb-subset
 * @short_description: Subsets font files.
 * @include: hb-subset.h
 *
 * Subsetting reduces the codepoint coverage of font files and removes all data
 * that is no longer needed. A subset input describes the desired subset. The input is
 * provided along with a font to the subsetting operation. Output is a new font file
 * containing only the data specified in the input.
 *
 * Currently most outline and bitmap tables are supported: glyf, CFF, CFF2, sbix,
 * COLR, and CBDT/CBLC. This also includes fonts with variable outlines via OpenType
 * variations. Notably EBDT/EBLC and SVG are not supported. Layout subsetting is supported
 * only for OpenType Layout tables (GSUB, GPOS, GDEF). Notably subsetting of graphite or AAT tables
 * is not yet supported.
 *
 * Fonts with graphite or AAT tables may still be subsetted but will likely need to use the
 * retain glyph ids option and configure the subset to pass through the layout tables untouched.
 */


hb_user_data_key_t _hb_subset_accelerator_user_data_key = {};


/*
 * The list of tables in the open type spec. Used to check for tables that may need handling
 * if we are unable to list the tables in a face.
 */
static hb_tag_t known_tables[] {
  HB_TAG ('a', 'v', 'a', 'r'),
  HB_OT_TAG_BASE,
  HB_OT_TAG_CBDT,
  HB_OT_TAG_CBLC,
  HB_OT_TAG_CFF1,
  HB_OT_TAG_CFF2,
  HB_OT_TAG_cmap,
  HB_OT_TAG_COLR,
  HB_OT_TAG_CPAL,
  HB_TAG ('c', 'v', 'a', 'r'),
  HB_TAG ('c', 'v', 't', ' '),
  HB_TAG ('D', 'S', 'I', 'G'),
  HB_TAG ('E', 'B', 'D', 'T'),
  HB_TAG ('E', 'B', 'L', 'C'),
  HB_TAG ('E', 'B', 'S', 'C'),
  HB_TAG ('f', 'p', 'g', 'm'),
  HB_TAG ('f', 'v', 'a', 'r'),
  HB_TAG ('g', 'a', 's', 'p'),
  HB_OT_TAG_GDEF,
  HB_OT_TAG_glyf,
  HB_OT_TAG_GPOS,
  HB_OT_TAG_GSUB,
  HB_OT_TAG_gvar,
  HB_OT_TAG_hdmx,
  HB_OT_TAG_head,
  HB_OT_TAG_hhea,
  HB_OT_TAG_hmtx,
  HB_OT_TAG_HVAR,
  HB_OT_TAG_JSTF,
  HB_TAG ('k', 'e', 'r', 'n'),
  HB_OT_TAG_loca,
  HB_TAG ('L', 'T', 'S', 'H'),
  HB_OT_TAG_MATH,
  HB_OT_TAG_maxp,
  HB_TAG ('M', 'E', 'R', 'G'),
  HB_TAG ('m', 'e', 't', 'a'),
  HB_TAG ('M', 'V', 'A', 'R'),
  HB_TAG ('P', 'C', 'L', 'T'),
  HB_OT_TAG_post,
  HB_TAG ('p', 'r', 'e', 'p'),
  HB_OT_TAG_sbix,
  HB_TAG ('S', 'T', 'A', 'T'),
  HB_TAG ('S', 'V', 'G', ' '),
  HB_TAG ('V', 'D', 'M', 'X'),
  HB_OT_TAG_vhea,
  HB_OT_TAG_vmtx,
  HB_OT_TAG_VORG,
  HB_OT_TAG_VVAR,
  HB_OT_TAG_name,
  HB_OT_TAG_OS2
};

static bool _table_is_empty (const hb_face_t *face, hb_tag_t tag)
{
  hb_blob_t* blob = hb_face_reference_table (face, tag);
  bool result = (blob == hb_blob_get_empty ());
  hb_blob_destroy (blob);
  return result;
}

static unsigned int
_get_table_tags (const hb_subset_plan_t* plan,
                 unsigned int  start_offset,
                 unsigned int *table_count, /* IN/OUT */
                 hb_tag_t     *table_tags /* OUT */)
{
  unsigned num_tables = hb_face_get_table_tags (plan->source, 0, nullptr, nullptr);
  if (num_tables)
    return hb_face_get_table_tags (plan->source, start_offset, table_count, table_tags);

  // If face has 0 tables associated with it, assume that it was built from
  // hb_face_create_tables and thus is unable to list its tables. Fallback to
  // checking each table type we can handle for existence instead.
  auto it =
      hb_concat (
          + hb_array (known_tables)
          | hb_filter ([&] (hb_tag_t tag) {
            return !_table_is_empty (plan->source, tag) && !plan->no_subset_tables.has (tag);
          })
          | hb_map ([] (hb_tag_t tag) -> hb_tag_t { return tag; }),

          plan->no_subset_tables.iter ()
          | hb_filter([&] (hb_tag_t tag) {
            return !_table_is_empty (plan->source, tag);
          }));

  it += start_offset;

  unsigned num_written = 0;
  while (bool (it) && num_written < *table_count)
    table_tags[num_written++] = *it++;

  *table_count = num_written;
  return num_written;
}


static unsigned
_plan_estimate_subset_table_size (hb_subset_plan_t *plan,
				  unsigned table_len,
				  hb_tag_t table_tag)
{
  unsigned src_glyphs = plan->source->get_num_glyphs ();
  unsigned dst_glyphs = plan->glyphset ()->get_population ();

  unsigned bulk = 8192;
  /* Tables that we want to allocate same space as the source table. For GSUB/GPOS it's
   * because those are expensive to subset, so giving them more room is fine. */
  bool same_size = table_tag == HB_OT_TAG_GSUB ||
		   table_tag == HB_OT_TAG_GPOS ||
		   table_tag == HB_OT_TAG_name;

  if (plan->flags & HB_SUBSET_FLAGS_RETAIN_GIDS)
  {
    if (table_tag == HB_OT_TAG_CFF1)
    {
      /* Add some extra room for the CFF charset. */
      bulk += src_glyphs * 16;
    }
    else if (table_tag == HB_OT_TAG_CFF2)
    {
      /* Just extra CharString offsets. */
      bulk += src_glyphs * 4;
    }
  }

  if (unlikely (!src_glyphs) || same_size)
    return bulk + table_len;

  return bulk + (unsigned) (table_len * sqrt ((double) dst_glyphs / src_glyphs));
}

/*
 * Repack the serialization buffer if any offset overflows exist.
 */
static hb_blob_t*
_repack (hb_tag_t tag, const hb_serialize_context_t& c)
{
  if (!c.offset_overflow ())
    return c.copy_blob ();

  hb_blob_t* result = hb_resolve_overflows (c.object_graph (), tag);

  if (unlikely (!result))
  {
    DEBUG_MSG (SUBSET, nullptr, "OT::%c%c%c%c offset overflow resolution failed.",
               HB_UNTAG (tag));
    return nullptr;
  }

  return result;
}

template<typename TableType>
static
bool
_try_subset (const TableType *table,
             hb_vector_t<char>* buf,
             hb_subset_context_t* c /* OUT */)
{
  c->serializer->start_serialize ();
  if (c->serializer->in_error ()) return false;

  bool needed = table->subset (c);
  if (!c->serializer->ran_out_of_room ())
  {
    c->serializer->end_serialize ();
    return needed;
  }

  unsigned buf_size = buf->allocated;
  buf_size = buf_size * 2 + 16;




  DEBUG_MSG (SUBSET, nullptr, "OT::%c%c%c%c ran out of room; reallocating to %u bytes.",
             HB_UNTAG (c->table_tag), buf_size);

  if (unlikely (buf_size > c->source_blob->length * 16 ||
		!buf->alloc (buf_size, true)))
  {
    DEBUG_MSG (SUBSET, nullptr, "OT::%c%c%c%c failed to reallocate %u bytes.",
               HB_UNTAG (c->table_tag), buf_size);
    return needed;
  }

  c->serializer->reset (buf->arrayZ, buf->allocated);
  return _try_subset (table, buf, c);
}

template <typename T>
static auto _do_destroy (T &t, hb_priority<1>) HB_RETURN (void, t.destroy ())

template <typename T>
static void _do_destroy (T &t, hb_priority<0>) {}

template<typename TableType>
static bool
_subset (hb_subset_plan_t *plan, hb_vector_t<char> &buf)
{
  auto &&source_blob = plan->source_table<TableType> ();
  auto *table = source_blob.get ();

  hb_tag_t tag = TableType::tableTag;
  hb_blob_t *blob = source_blob.get_blob();
  if (unlikely (!blob || !blob->data))
  {
    DEBUG_MSG (SUBSET, nullptr,
               "OT::%c%c%c%c::subset sanitize failed on source table.", HB_UNTAG (tag));
    _do_destroy (source_blob, hb_prioritize);
    return false;
  }

  unsigned buf_size = _plan_estimate_subset_table_size (plan, blob->length, TableType::tableTag);
  DEBUG_MSG (SUBSET, nullptr,
             "OT::%c%c%c%c initial estimated table size: %u bytes.", HB_UNTAG (tag), buf_size);
  if (unlikely (!buf.alloc (buf_size)))
  {
    DEBUG_MSG (SUBSET, nullptr, "OT::%c%c%c%c failed to allocate %u bytes.", HB_UNTAG (tag), buf_size);
    _do_destroy (source_blob, hb_prioritize);
    return false;
  }

  bool needed = false;
  hb_serialize_context_t serializer (buf.arrayZ, buf.allocated);
  {
    hb_subset_context_t c (blob, plan, &serializer, tag);
    needed = _try_subset (table, &buf, &c);
  }
  _do_destroy (source_blob, hb_prioritize);

  if (serializer.in_error () && !serializer.only_offset_overflow ())
  {
    DEBUG_MSG (SUBSET, nullptr, "OT::%c%c%c%c::subset FAILED!", HB_UNTAG (tag));
    return false;
  }

  if (!needed)
  {
    DEBUG_MSG (SUBSET, nullptr, "OT::%c%c%c%c::subset table subsetted to empty.", HB_UNTAG (tag));
    return true;
  }

  bool result = false;
  hb_blob_t *dest_blob = _repack (tag, serializer);
  if (dest_blob)
  {
    DEBUG_MSG (SUBSET, nullptr,
               "OT::%c%c%c%c final subset table size: %u bytes.",
               HB_UNTAG (tag), dest_blob->length);
    result = plan->add_table (tag, dest_blob);
    hb_blob_destroy (dest_blob);
  }

  DEBUG_MSG (SUBSET, nullptr, "OT::%c%c%c%c::subset %s",
             HB_UNTAG (tag), result ? "success" : "FAILED!");
  return result;
}

static bool
_is_table_present (hb_face_t *source, hb_tag_t tag)
{

  if (!hb_face_get_table_tags (source, 0, nullptr, nullptr)) {
    // If face has 0 tables associated with it, assume that it was built from
    // hb_face_create_tables and thus is unable to list its tables. Fallback to
    // checking if the blob associated with tag is empty.
    return !_table_is_empty (source, tag);
  }

  hb_tag_t table_tags[32];
  unsigned offset = 0, num_tables = ARRAY_LENGTH (table_tags);
  while (((void) hb_face_get_table_tags (source, offset, &num_tables, table_tags), num_tables))
  {
    for (unsigned i = 0; i < num_tables; ++i)
      if (table_tags[i] == tag)
	return true;
    offset += num_tables;
  }
  return false;
}

static bool
_should_drop_table (hb_subset_plan_t *plan, hb_tag_t tag)
{
  if (plan->drop_tables.has (tag))
    return true;

  switch (tag)
  {
  case HB_TAG ('c','v','a','r'): /* hint table, fallthrough */
    return plan->all_axes_pinned || (plan->flags & HB_SUBSET_FLAGS_NO_HINTING);

  case HB_TAG ('c','v','t',' '): /* hint table, fallthrough */
  case HB_TAG ('f','p','g','m'): /* hint table, fallthrough */
  case HB_TAG ('p','r','e','p'): /* hint table, fallthrough */
  case HB_TAG ('h','d','m','x'): /* hint table, fallthrough */
  case HB_TAG ('V','D','M','X'): /* hint table, fallthrough */
    return plan->flags & HB_SUBSET_FLAGS_NO_HINTING;

#ifdef HB_NO_SUBSET_LAYOUT
    // Drop Layout Tables if requested.
  case HB_OT_TAG_GDEF:
  case HB_OT_TAG_GPOS:
  case HB_OT_TAG_GSUB:
  case HB_TAG ('m','o','r','x'):
  case HB_TAG ('m','o','r','t'):
  case HB_TAG ('k','e','r','x'):
  case HB_TAG ('k','e','r','n'):
    return true;
#endif

  case HB_TAG ('a','v','a','r'):
  case HB_TAG ('f','v','a','r'):
  case HB_TAG ('g','v','a','r'):
  case HB_OT_TAG_HVAR:
  case HB_OT_TAG_VVAR:
  case HB_TAG ('M','V','A','R'):
    return plan->all_axes_pinned;

  default:
    return false;
  }
}

static bool
_passthrough (hb_subset_plan_t *plan, hb_tag_t tag)
{
  hb_blob_t *source_table = hb_face_reference_table (plan->source, tag);
  bool result = plan->add_table (tag, source_table);
  hb_blob_destroy (source_table);
  return result;
}

static bool
_dependencies_satisfied (hb_subset_plan_t *plan, hb_tag_t tag,
                         const hb_set_t &subsetted_tags,
                         const hb_set_t &pending_subset_tags)
{
  switch (tag)
  {
  case HB_OT_TAG_hmtx:
  case HB_OT_TAG_vmtx:
  case HB_OT_TAG_maxp:
  case HB_OT_TAG_OS2:
    return !plan->normalized_coords || !pending_subset_tags.has (HB_OT_TAG_glyf);
  case HB_OT_TAG_GPOS:
    return plan->all_axes_pinned || !pending_subset_tags.has (HB_OT_TAG_GDEF);
  default:
    return true;
  }
}

static bool
_subset_table (hb_subset_plan_t *plan,
	       hb_vector_t<char> &buf,
	       hb_tag_t tag)
{
  if (plan->no_subset_tables.has (tag)) {
    return _passthrough (plan, tag);
  }

  DEBUG_MSG (SUBSET, nullptr, "subset %c%c%c%c", HB_UNTAG (tag));
  switch (tag)
  {
  case HB_OT_TAG_glyf: return _subset<const OT::glyf> (plan, buf);
  case HB_OT_TAG_hdmx: return _subset<const OT::hdmx> (plan, buf);
  case HB_OT_TAG_name: return _subset<const OT::name> (plan, buf);
  case HB_OT_TAG_head:
    if (_is_table_present (plan->source, HB_OT_TAG_glyf) && !_should_drop_table (plan, HB_OT_TAG_glyf))
      return true; /* skip head, handled by glyf */
    return _subset<const OT::head> (plan, buf);
  case HB_OT_TAG_hhea: return true; /* skip hhea, handled by hmtx */
  case HB_OT_TAG_hmtx: return _subset<const OT::hmtx> (plan, buf);
  case HB_OT_TAG_vhea: return true; /* skip vhea, handled by vmtx */
  case HB_OT_TAG_vmtx: return _subset<const OT::vmtx> (plan, buf);
  case HB_OT_TAG_maxp: return _subset<const OT::maxp> (plan, buf);
  case HB_OT_TAG_sbix: return _subset<const OT::sbix> (plan, buf);
  case HB_OT_TAG_loca: return true; /* skip loca, handled by glyf */
  case HB_OT_TAG_cmap: return _subset<const OT::cmap> (plan, buf);
  case HB_OT_TAG_OS2 : return _subset<const OT::OS2 > (plan, buf);
  case HB_OT_TAG_post: return _subset<const OT::post> (plan, buf);
  case HB_OT_TAG_COLR: return _subset<const OT::COLR> (plan, buf);
  case HB_OT_TAG_CPAL: return _subset<const OT::CPAL> (plan, buf);
  case HB_OT_TAG_CBLC: return _subset<const OT::CBLC> (plan, buf);
  case HB_OT_TAG_CBDT: return true; /* skip CBDT, handled by CBLC */
  case HB_OT_TAG_MATH: return _subset<const OT::MATH> (plan, buf);
  case HB_OT_TAG_BASE: return _subset<const OT::BASE> (plan, buf);

#ifndef HB_NO_SUBSET_CFF
  case HB_OT_TAG_CFF1: return _subset<const OT::cff1> (plan, buf);
  case HB_OT_TAG_CFF2: return _subset<const OT::cff2> (plan, buf);
  case HB_OT_TAG_VORG: return _subset<const OT::VORG> (plan, buf);
#endif

#ifndef HB_NO_SUBSET_LAYOUT
  case HB_OT_TAG_GDEF: return _subset<const OT::GDEF> (plan, buf);
  case HB_OT_TAG_GSUB: return _subset<const GSUB> (plan, buf);
  case HB_OT_TAG_GPOS: return _subset<const GPOS> (plan, buf);
  case HB_OT_TAG_gvar: return _subset<const OT::gvar> (plan, buf);
  case HB_OT_TAG_HVAR: return _subset<const OT::HVAR> (plan, buf);
  case HB_OT_TAG_VVAR: return _subset<const OT::VVAR> (plan, buf);
#endif

#ifndef HB_NO_VAR
  case HB_OT_TAG_fvar:
    if (plan->user_axes_location.is_empty ()) return _passthrough (plan, tag);
    return _subset<const OT::fvar> (plan, buf);
  case HB_OT_TAG_avar:
    if (plan->user_axes_location.is_empty ()) return _passthrough (plan, tag);
    return _subset<const OT::avar> (plan, buf);
  case HB_OT_TAG_cvar:
    if (plan->user_axes_location.is_empty ()) return _passthrough (plan, tag);
    return _subset<const OT::cvar> (plan, buf);
  case HB_OT_TAG_MVAR:
    if (plan->user_axes_location.is_empty ()) return _passthrough (plan, tag);
    return _subset<const OT::MVAR> (plan, buf);
#endif

  case HB_OT_TAG_STAT:
    if (!plan->user_axes_location.is_empty ()) return _subset<const OT::STAT> (plan, buf);
    else return _passthrough (plan, tag);

  case HB_TAG ('c', 'v', 't', ' '):
#ifndef HB_NO_VAR
    if (_is_table_present (plan->source, HB_OT_TAG_cvar) &&
        plan->normalized_coords && !plan->pinned_at_default)
    {
      auto &cvar = *plan->source->table.cvar;
      return OT::cvar::add_cvt_and_apply_deltas (plan, cvar.get_tuple_var_data (), &cvar);
    }
#endif
    return _passthrough (plan, tag);

  default:
    if (plan->flags & HB_SUBSET_FLAGS_PASSTHROUGH_UNRECOGNIZED)
      return _passthrough (plan, tag);

    // Drop table
    return true;
  }
}

static void _attach_accelerator_data (hb_subset_plan_t* plan,
                                      hb_face_t* face /* IN/OUT */)
{
  if (!plan->inprogress_accelerator) return;

  // Transfer the accelerator from the plan to us.
  hb_subset_accelerator_t* accel = plan->inprogress_accelerator;
  plan->inprogress_accelerator = nullptr;

  if (accel->in_error ())
  {
    hb_subset_accelerator_t::destroy (accel);
    return;
  }

  // Populate caches that need access to the final tables.
  hb_blob_ptr_t<OT::cmap> cmap_ptr (hb_sanitize_context_t ().reference_table<OT::cmap> (face));
  accel->cmap_cache = OT::cmap::create_filled_cache (cmap_ptr);
  accel->destroy_cmap_cache = OT::SubtableUnicodesCache::destroy;

  if (!hb_face_set_user_data(face,
                             hb_subset_accelerator_t::user_data_key(),
                             accel,
                             hb_subset_accelerator_t::destroy,
                             true))
    hb_subset_accelerator_t::destroy (accel);
}

/**
 * hb_subset_or_fail:
 * @source: font face data to be subset.
 * @input: input to use for the subsetting.
 *
 * Subsets a font according to provided input. Returns nullptr
 * if the subset operation fails or the face has no glyphs.
 *
 * Since: 2.9.0
 **/
hb_face_t *
hb_subset_or_fail (hb_face_t *source, const hb_subset_input_t *input)
{
  if (unlikely (!input || !source)) return nullptr;

  if (unlikely (!source->get_num_glyphs ()))
  {
    DEBUG_MSG (SUBSET, nullptr, "No glyphs in source font.");
    return nullptr;
  }

  hb_subset_plan_t *plan = hb_subset_plan_create_or_fail (source, input);
  if (unlikely (!plan)) {
    return nullptr;
  }

  hb_face_t * result = hb_subset_plan_execute_or_fail (plan);
  hb_subset_plan_destroy (plan);
  return result;
}


/**
 * hb_subset_plan_execute_or_fail:
 * @plan: a subsetting plan.
 *
 * Executes the provided subsetting @plan.
 *
 * Return value:
 * on success returns a reference to generated font subset. If the subsetting operation fails
 * returns nullptr.
 *
 * Since: 4.0.0
 **/
hb_face_t *
hb_subset_plan_execute_or_fail (hb_subset_plan_t *plan)
{
  if (unlikely (!plan || plan->in_error ())) {
    return nullptr;
  }

  hb_tag_t table_tags[32];
  unsigned offset = 0, num_tables = ARRAY_LENGTH (table_tags);

  hb_set_t subsetted_tags, pending_subset_tags;
  while (((void) _get_table_tags (plan, offset, &num_tables, table_tags), num_tables))
  {
    for (unsigned i = 0; i < num_tables; ++i)
    {
      hb_tag_t tag = table_tags[i];
      if (_should_drop_table (plan, tag)) continue;
      pending_subset_tags.add (tag);
    }

    offset += num_tables;
  }

  bool success = true;

  {
    // Grouping to deallocate buf before calling hb_face_reference (plan->dest).

    hb_vector_t<char> buf;
    buf.alloc (8192 - 16);

    while (!pending_subset_tags.is_empty ())
    {
      if (subsetted_tags.in_error ()
	  || pending_subset_tags.in_error ()) {
	success = false;
	goto end;
      }

      bool made_changes = false;
      for (hb_tag_t tag : pending_subset_tags)
      {
	if (!_dependencies_satisfied (plan, tag,
				      subsetted_tags,
				      pending_subset_tags))
	{
	  // delayed subsetting for some tables since they might have dependency on other tables
	  // in some cases: e.g: during instantiating glyf tables, hmetrics/vmetrics are updated
	  // and saved in subset plan, hmtx/vmtx subsetting need to use these updated metrics values
	  continue;
	}

	pending_subset_tags.del (tag);
	subsetted_tags.add (tag);
	made_changes = true;

	success = _subset_table (plan, buf, tag);
	if (unlikely (!success)) goto end;
      }

      if (!made_changes)
      {
	DEBUG_MSG (SUBSET, nullptr, "Table dependencies unable to be satisfied. Subset failed.");
	success = false;
	goto end;
      }
    }
  }

  if (success && plan->attach_accelerator_data) {
    _attach_accelerator_data (plan, plan->dest);
  }

end:
  return success ? hb_face_reference (plan->dest) : nullptr;
}
