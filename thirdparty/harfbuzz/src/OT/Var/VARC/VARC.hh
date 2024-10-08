#ifndef OT_VAR_VARC_VARC_HH
#define OT_VAR_VARC_VARC_HH

#include "../../../hb-ot-layout-common.hh"
#include "../../../hb-ot-glyf-table.hh"
#include "../../../hb-ot-cff2-table.hh"
#include "../../../hb-ot-cff1-table.hh"

#include "coord-setter.hh"

namespace OT {

//namespace Var {

/*
 * VARC -- Variable Composites
 * https://github.com/harfbuzz/boring-expansion-spec/blob/main/VARC.md
 */

#ifndef HB_NO_VAR_COMPOSITES

struct VarComponent
{
  enum class flags_t : uint32_t
  {
    RESET_UNSPECIFIED_AXES	= 1u << 0,
    HAVE_AXES			= 1u << 1,
    AXIS_VALUES_HAVE_VARIATION	= 1u << 2,
    TRANSFORM_HAS_VARIATION	= 1u << 3,
    HAVE_TRANSLATE_X		= 1u << 4,
    HAVE_TRANSLATE_Y		= 1u << 5,
    HAVE_ROTATION		= 1u << 6,
    HAVE_CONDITION		= 1u << 7,
    HAVE_SCALE_X		= 1u << 8,
    HAVE_SCALE_Y		= 1u << 9,
    HAVE_TCENTER_X		= 1u << 10,
    HAVE_TCENTER_Y		= 1u << 11,
    GID_IS_24BIT		= 1u << 12,
    HAVE_SKEW_X			= 1u << 13,
    HAVE_SKEW_Y			= 1u << 14,
    RESERVED_MASK		= ~((1u << 15) - 1),
  };

  HB_INTERNAL hb_ubytes_t
  get_path_at (hb_font_t *font,
	       hb_codepoint_t parent_gid,
	       hb_draw_session_t &draw_session,
	       hb_array_t<const int> coords,
	       hb_ubytes_t record,
	       hb_set_t *visited,
	       signed *edges_left,
	       signed depth_left,
	       VarRegionList::cache_t *cache = nullptr) const;
};

struct VarCompositeGlyph
{
  static void
  get_path_at (hb_font_t *font,
	       hb_codepoint_t glyph,
	       hb_draw_session_t &draw_session,
	       hb_array_t<const int> coords,
	       hb_ubytes_t record,
	       hb_set_t *visited,
	       signed *edges_left,
	       signed depth_left,
	       VarRegionList::cache_t *cache = nullptr)
  {
    while (record)
    {
      const VarComponent &comp = * (const VarComponent *) (record.arrayZ);
      record = comp.get_path_at (font, glyph,
				 draw_session, coords,
				 record,
				 visited, edges_left, depth_left, cache);
    }
  }
};

HB_MARK_AS_FLAG_T (VarComponent::flags_t);

struct VARC
{
  friend struct VarComponent;

  static constexpr hb_tag_t tableTag = HB_TAG ('V', 'A', 'R', 'C');

  bool
  get_path_at (hb_font_t *font,
	       hb_codepoint_t glyph,
	       hb_draw_session_t &draw_session,
	       hb_array_t<const int> coords,
	       hb_codepoint_t parent_glyph = HB_CODEPOINT_INVALID,
	       hb_set_t *visited = nullptr,
	       signed *edges_left = nullptr,
	       signed depth_left = HB_MAX_NESTING_LEVEL) const
  {
    hb_set_t stack_set;
    if (visited == nullptr)
      visited = &stack_set;
    signed stack_edges = HB_MAX_GRAPH_EDGE_COUNT;
    if (edges_left == nullptr)
      edges_left = &stack_edges;

    // Don't recurse on the same glyph.
    unsigned idx = glyph == parent_glyph ?
		   NOT_COVERED :
		   (this+coverage).get_coverage (glyph);
    if (idx == NOT_COVERED)
    {
      if (!font->face->table.glyf->get_path_at (font, glyph, draw_session, coords))
#ifndef HB_NO_CFF
      if (!font->face->table.cff2->get_path_at (font, glyph, draw_session, coords))
      if (!font->face->table.cff1->get_path (font, glyph, draw_session)) // Doesn't have variations
#endif
	return false;
      return true;
    }

    if (depth_left <= 0)
      return true;

    if (*edges_left <= 0)
      return true;
    (*edges_left)--;

    if (visited->has (glyph) || visited->in_error ())
      return true;
    visited->add (glyph);

    hb_ubytes_t record = (this+glyphRecords)[idx];

    VarRegionList::cache_t *cache = record.length >= 64 ? // Heuristic
				   (this+varStore).create_cache ()
				   : nullptr;

    VarCompositeGlyph::get_path_at (font, glyph,
				    draw_session, coords,
				    record,
				    visited, edges_left, depth_left,
				    cache);

    (this+varStore).destroy_cache (cache);

    visited->del (glyph);

    return true;
  }

  bool
  get_path (hb_font_t *font, hb_codepoint_t gid, hb_draw_session_t &draw_session) const
  { return get_path_at (font, gid, draw_session, hb_array (font->coords, font->num_coords)); }

  bool paint_glyph (hb_font_t *font, hb_codepoint_t gid, hb_paint_funcs_t *funcs, void *data, hb_color_t foreground) const
  {
    funcs->push_clip_glyph (data, gid, font);
    funcs->color (data, true, foreground);
    funcs->pop_clip (data);

    return true;
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (version.sanitize (c) &&
		  hb_barrier () &&
		  version.major == 1 &&
		  coverage.sanitize (c, this) &&
		  varStore.sanitize (c, this) &&
		  conditionList.sanitize (c, this) &&
		  axisIndicesList.sanitize (c, this) &&
		  glyphRecords.sanitize (c, this));
  }

  protected:
  FixedVersion<> version; /* Version identifier */
  Offset32To<Coverage> coverage;
  Offset32To<MultiItemVariationStore> varStore;
  Offset32To<ConditionList> conditionList;
  Offset32To<TupleList> axisIndicesList;
  Offset32To<CFF2Index/*Of<VarCompositeGlyph>*/> glyphRecords;
  public:
  DEFINE_SIZE_STATIC (24);
};

#endif

//}

}

#endif  /* OT_VAR_VARC_VARC_HH */
