#ifndef OT_GLYF_GLYPH_HH
#define OT_GLYF_GLYPH_HH


#include "../../hb-open-type.hh"

#include "GlyphHeader.hh"
#include "SimpleGlyph.hh"
#include "CompositeGlyph.hh"


namespace OT {

struct glyf_accelerator_t;

namespace glyf_impl {


enum phantom_point_index_t
{
  PHANTOM_LEFT   = 0,
  PHANTOM_RIGHT  = 1,
  PHANTOM_TOP    = 2,
  PHANTOM_BOTTOM = 3,
  PHANTOM_COUNT  = 4
};

struct Glyph
{
  enum glyph_type_t {
    EMPTY,
    SIMPLE,
    COMPOSITE,
  };

  public:
  composite_iter_t get_composite_iterator () const
  {
    if (type != COMPOSITE) return composite_iter_t ();
    return CompositeGlyph (*header, bytes).iter ();
  }

  const hb_bytes_t trim_padding () const
  {
    switch (type) {
    case COMPOSITE: return CompositeGlyph (*header, bytes).trim_padding ();
    case SIMPLE:    return SimpleGlyph (*header, bytes).trim_padding ();
    case EMPTY:     return bytes;
    default:        return bytes;
    }
  }

  void drop_hints ()
  {
    switch (type) {
    case COMPOSITE: CompositeGlyph (*header, bytes).drop_hints (); return;
    case SIMPLE:    SimpleGlyph (*header, bytes).drop_hints (); return;
    case EMPTY:     return;
    }
  }

  void set_overlaps_flag ()
  {
    switch (type) {
    case COMPOSITE: CompositeGlyph (*header, bytes).set_overlaps_flag (); return;
    case SIMPLE:    SimpleGlyph (*header, bytes).set_overlaps_flag (); return;
    case EMPTY:     return;
    }
  }

  void drop_hints_bytes (hb_bytes_t &dest_start, hb_bytes_t &dest_end) const
  {
    switch (type) {
    case COMPOSITE: CompositeGlyph (*header, bytes).drop_hints_bytes (dest_start); return;
    case SIMPLE:    SimpleGlyph (*header, bytes).drop_hints_bytes (dest_start, dest_end); return;
    case EMPTY:     return;
    }
  }

  bool is_composite () const
  { return type == COMPOSITE; }

  bool get_all_points_without_var (const hb_face_t *face,
                                   contour_point_vector_t &points /* OUT */) const
  {
    switch (type) {
    case SIMPLE:
      if (unlikely (!SimpleGlyph (*header, bytes).get_contour_points (points)))
        return false;
      break;
    case COMPOSITE:
    {
      for (auto &item : get_composite_iterator ())
        if (unlikely (!item.get_points (points))) return false;
      break;
    }
    case EMPTY:
      break;
    }

    /* Init phantom points */
    if (unlikely (!points.resize (points.length + PHANTOM_COUNT))) return false;
    hb_array_t<contour_point_t> phantoms = points.as_array ().sub_array (points.length - PHANTOM_COUNT, PHANTOM_COUNT);
    {
      int lsb = 0;
      int h_delta = face->table.hmtx->get_leading_bearing_without_var_unscaled (gid, &lsb) ?
                    (int) header->xMin - lsb : 0;
      HB_UNUSED int tsb = 0;
      int v_orig  = (int) header->yMax +
#ifndef HB_NO_VERTICAL
                    ((void) face->table.vmtx->get_leading_bearing_without_var_unscaled (gid, &tsb), tsb)
#else
                    0
#endif
                    ;
      unsigned h_adv = face->table.hmtx->get_advance_without_var_unscaled (gid);
      unsigned v_adv =
#ifndef HB_NO_VERTICAL
                       face->table.vmtx->get_advance_without_var_unscaled (gid)
#else
                       - face->get_upem ()
#endif
                       ;
      phantoms[PHANTOM_LEFT].x = h_delta;
      phantoms[PHANTOM_RIGHT].x = (int) h_adv + h_delta;
      phantoms[PHANTOM_TOP].y = v_orig;
      phantoms[PHANTOM_BOTTOM].y = v_orig - (int) v_adv;
    }
    return true;
  }

  void update_mtx (const hb_subset_plan_t *plan,
                   int xMin, int xMax,
                   int yMin, int yMax,
                   const contour_point_vector_t &all_points) const
  {
    hb_codepoint_t new_gid = 0;
    if (!plan->new_gid_for_old_gid (gid, &new_gid))
      return;

    if (type != EMPTY)
    {
      plan->bounds_width_vec[new_gid] = xMax - xMin;
      plan->bounds_height_vec[new_gid] = yMax - yMin;
    }

    unsigned len = all_points.length;
    float leftSideX = all_points[len - 4].x;
    float rightSideX = all_points[len - 3].x;
    float topSideY = all_points[len - 2].y;
    float bottomSideY = all_points[len - 1].y;

    uint32_t hash = hb_hash (new_gid);

    signed hori_aw = roundf (rightSideX - leftSideX);
    if (hori_aw < 0) hori_aw = 0;
    int lsb = roundf (xMin - leftSideX);
    plan->hmtx_map.set_with_hash (new_gid, hash, hb_pair ((unsigned) hori_aw, lsb));
    //flag value should be computed using non-empty glyphs
    if (type != EMPTY && lsb != xMin)
      plan->head_maxp_info.allXMinIsLsb = false;

    signed vert_aw = roundf (topSideY - bottomSideY);
    if (vert_aw < 0) vert_aw = 0;
    int tsb = roundf (topSideY - yMax);
    plan->vmtx_map.set_with_hash (new_gid, hash, hb_pair ((unsigned) vert_aw, tsb));
  }

  bool compile_header_bytes (const hb_subset_plan_t *plan,
                             const contour_point_vector_t &all_points,
                             hb_bytes_t &dest_bytes /* OUT */) const
  {
    GlyphHeader *glyph_header = nullptr;
    if (!plan->pinned_at_default && type != EMPTY && all_points.length >= 4)
    {
      glyph_header = (GlyphHeader *) hb_calloc (1, GlyphHeader::static_size);
      if (unlikely (!glyph_header)) return false;
    }

    float xMin = 0, xMax = 0;
    float yMin = 0, yMax = 0;
    if (all_points.length > 4)
    {
      xMin = xMax = all_points[0].x;
      yMin = yMax = all_points[0].y;

      unsigned count = all_points.length - 4;
      for (unsigned i = 1; i < count; i++)
      {
	float x = all_points[i].x;
	float y = all_points[i].y;
	xMin = hb_min (xMin, x);
	xMax = hb_max (xMax, x);
	yMin = hb_min (yMin, y);
	yMax = hb_max (yMax, y);
      }
    }


    // These are destined for storage in a 16 bit field to clamp the values to
    // fit into a 16 bit signed integer.
    int rounded_xMin = hb_clamp (roundf (xMin), -32768.0f, 32767.0f);
    int rounded_xMax = hb_clamp (roundf (xMax), -32768.0f, 32767.0f);
    int rounded_yMin = hb_clamp (roundf (yMin), -32768.0f, 32767.0f);
    int rounded_yMax = hb_clamp (roundf (yMax), -32768.0f, 32767.0f);

    update_mtx (plan, rounded_xMin, rounded_xMax, rounded_yMin, rounded_yMax, all_points);

    if (type != EMPTY)
    {
      plan->head_maxp_info.xMin = hb_min (plan->head_maxp_info.xMin, rounded_xMin);
      plan->head_maxp_info.yMin = hb_min (plan->head_maxp_info.yMin, rounded_yMin);
      plan->head_maxp_info.xMax = hb_max (plan->head_maxp_info.xMax, rounded_xMax);
      plan->head_maxp_info.yMax = hb_max (plan->head_maxp_info.yMax, rounded_yMax);
    }

    /* when pinned at default, no need to compile glyph header
     * and for empty glyphs: all_points only include phantom points.
     * just update metrics and then return */
    if (!glyph_header)
      return true;

    glyph_header->numberOfContours = header->numberOfContours;

    glyph_header->xMin = rounded_xMin;
    glyph_header->yMin = rounded_yMin;
    glyph_header->xMax = rounded_xMax;
    glyph_header->yMax = rounded_yMax;

    dest_bytes = hb_bytes_t ((const char *)glyph_header, GlyphHeader::static_size);
    return true;
  }

  bool compile_bytes_with_deltas (const hb_subset_plan_t *plan,
                                  hb_font_t *font,
                                  const glyf_accelerator_t &glyf,
                                  hb_bytes_t &dest_start,  /* IN/OUT */
                                  hb_bytes_t &dest_end /* OUT */)
  {
    contour_point_vector_t all_points, points_with_deltas;
    unsigned composite_contours = 0;
    head_maxp_info_t *head_maxp_info_p = &plan->head_maxp_info;
    unsigned *composite_contours_p = &composite_contours;

    // don't compute head/maxp values when glyph has no contours(type is EMPTY)
    // also ignore .notdef glyph when --notdef-outline is not enabled
    if (type == EMPTY ||
        (gid == 0 && !(plan->flags & HB_SUBSET_FLAGS_NOTDEF_OUTLINE)))
    {
      head_maxp_info_p = nullptr;
      composite_contours_p = nullptr;
    }

    if (!get_points (font, glyf, all_points, &points_with_deltas, head_maxp_info_p, composite_contours_p, false, false))
      return false;

    // .notdef, set type to empty so we only update metrics and don't compile bytes for
    // it
    if (gid == 0 &&
        !(plan->flags & HB_SUBSET_FLAGS_NOTDEF_OUTLINE))
    {
      type = EMPTY;
      dest_start = hb_bytes_t ();
      dest_end = hb_bytes_t ();
    }

    //dont compile bytes when pinned at default, just recalculate bounds
    if (!plan->pinned_at_default)
    {
      switch (type)
      {
      case COMPOSITE:
        if (!CompositeGlyph (*header, bytes).compile_bytes_with_deltas (dest_start,
                                                                        points_with_deltas,
                                                                        dest_end))
          return false;
        break;
      case SIMPLE:
        if (!SimpleGlyph (*header, bytes).compile_bytes_with_deltas (all_points,
                                                                     plan->flags & HB_SUBSET_FLAGS_NO_HINTING,
                                                                     dest_end))
          return false;
        break;
      case EMPTY:
        /* set empty bytes for empty glyph
         * do not use source glyph's pointers */
        dest_start = hb_bytes_t ();
        dest_end = hb_bytes_t ();
        break;
      }
    }

    if (!compile_header_bytes (plan, all_points, dest_start))
    {
      dest_end.fini ();
      return false;
    }
    return true;
  }


  /* Note: Recursively calls itself.
   * all_points includes phantom points
   */
  template <typename accelerator_t>
  bool get_points (hb_font_t *font, const accelerator_t &glyf_accelerator,
		   contour_point_vector_t &all_points /* OUT */,
		   contour_point_vector_t *points_with_deltas = nullptr, /* OUT */
		   head_maxp_info_t * head_maxp_info = nullptr, /* OUT */
		   unsigned *composite_contours = nullptr, /* OUT */
		   bool shift_points_hori = true,
		   bool use_my_metrics = true,
		   bool phantom_only = false,
		   hb_array_t<const int> coords = hb_array_t<const int> (),
		   hb_map_t *current_glyphs = nullptr,
		   unsigned int depth = 0,
		   unsigned *edge_count = nullptr) const
  {
    if (unlikely (depth > HB_MAX_NESTING_LEVEL)) return false;
    unsigned stack_edge_count = 0;
    if (!edge_count) edge_count = &stack_edge_count;
    if (unlikely (*edge_count > HB_MAX_GRAPH_EDGE_COUNT)) return false;
    (*edge_count)++;

    hb_map_t current_glyphs_stack;
    if (current_glyphs == nullptr)
      current_glyphs = &current_glyphs_stack;

    if (head_maxp_info)
    {
      head_maxp_info->maxComponentDepth = hb_max (head_maxp_info->maxComponentDepth, depth);
    }

    if (!coords)
      coords = hb_array (font->coords, font->num_coords);

    contour_point_vector_t stack_points;
    contour_point_vector_t &points = type == SIMPLE ? all_points : stack_points;
    unsigned old_length = points.length;

    switch (type) {
    case SIMPLE:
      if (depth == 0 && head_maxp_info)
        head_maxp_info->maxContours = hb_max (head_maxp_info->maxContours, (unsigned) header->numberOfContours);
      if (depth > 0 && composite_contours)
        *composite_contours += (unsigned) header->numberOfContours;
      if (unlikely (!SimpleGlyph (*header, bytes).get_contour_points (all_points, phantom_only)))
	return false;
      break;
    case COMPOSITE:
    {
      for (auto &item : get_composite_iterator ())
        if (unlikely (!item.get_points (points))) return false;
      break;
    }
    case EMPTY:
      break;
    }

    /* Init phantom points */
    if (unlikely (!points.resize (points.length + PHANTOM_COUNT))) return false;
    hb_array_t<contour_point_t> phantoms = points.as_array ().sub_array (points.length - PHANTOM_COUNT, PHANTOM_COUNT);
    {
      int lsb = 0;
      int h_delta = glyf_accelerator.hmtx->get_leading_bearing_without_var_unscaled (gid, &lsb) ?
		    (int) header->xMin - lsb : 0;
      HB_UNUSED int tsb = 0;
      int v_orig  = (int) header->yMax +
#ifndef HB_NO_VERTICAL
		    ((void) glyf_accelerator.vmtx->get_leading_bearing_without_var_unscaled (gid, &tsb), tsb)
#else
		    0
#endif
		    ;
      unsigned h_adv = glyf_accelerator.hmtx->get_advance_without_var_unscaled (gid);
      unsigned v_adv =
#ifndef HB_NO_VERTICAL
		       glyf_accelerator.vmtx->get_advance_without_var_unscaled (gid)
#else
		       - font->face->get_upem ()
#endif
		       ;
      phantoms[PHANTOM_LEFT].x = h_delta;
      phantoms[PHANTOM_RIGHT].x = (int) h_adv + h_delta;
      phantoms[PHANTOM_TOP].y = v_orig;
      phantoms[PHANTOM_BOTTOM].y = v_orig - (int) v_adv;
    }

#ifndef HB_NO_VAR
    if (coords)
      glyf_accelerator.gvar->apply_deltas_to_points (gid,
						     coords,
						     points.as_array ().sub_array (old_length),
						     phantom_only && type == SIMPLE);
#endif

    // mainly used by CompositeGlyph calculating new X/Y offset value so no need to extend it
    // with child glyphs' points
    if (points_with_deltas != nullptr && depth == 0 && type == COMPOSITE)
    {
      if (unlikely (!points_with_deltas->resize (points.length))) return false;
      *points_with_deltas = points;
    }

    switch (type) {
    case SIMPLE:
      if (depth == 0 && head_maxp_info)
        head_maxp_info->maxPoints = hb_max (head_maxp_info->maxPoints, all_points.length - old_length - 4);
      break;
    case COMPOSITE:
    {
      unsigned int comp_index = 0;
      for (auto &item : get_composite_iterator ())
      {
	hb_codepoint_t item_gid = item.get_gid ();

        if (unlikely (current_glyphs->has (item_gid)))
	  continue;

	current_glyphs->add (item_gid);

	unsigned old_count = all_points.length;

	if (unlikely ((!phantom_only || (use_my_metrics && item.is_use_my_metrics ())) &&
		      !glyf_accelerator.glyph_for_gid (item_gid)
				       .get_points (font,
						    glyf_accelerator,
						    all_points,
						    points_with_deltas,
						    head_maxp_info,
						    composite_contours,
						    shift_points_hori,
						    use_my_metrics,
						    phantom_only,
						    coords,
						    current_glyphs,
						    depth + 1,
						    edge_count)))
	{
	  current_glyphs->del (item_gid);
	  return false;
	}

	auto comp_points = all_points.as_array ().sub_array (old_count);

	/* Copy phantom points from component if USE_MY_METRICS flag set */
	if (use_my_metrics && item.is_use_my_metrics ())
	  for (unsigned int i = 0; i < PHANTOM_COUNT; i++)
	    phantoms[i] = comp_points[comp_points.length - PHANTOM_COUNT + i];

	if (comp_points) // Empty in case of phantom_only
	{
	  float matrix[4];
	  contour_point_t default_trans;
	  item.get_transformation (matrix, default_trans);

	  /* Apply component transformation & translation (with deltas applied) */
	  item.transform_points (comp_points, matrix, points[comp_index]);
	}

	if (item.is_anchored () && !phantom_only)
	{
	  unsigned int p1, p2;
	  item.get_anchor_points (p1, p2);
	  if (likely (p1 < all_points.length && p2 < comp_points.length))
	  {
	    contour_point_t delta;
	    delta.init (all_points[p1].x - comp_points[p2].x,
			all_points[p1].y - comp_points[p2].y);

	    item.translate (delta, comp_points);
	  }
	}

	all_points.resize (all_points.length - PHANTOM_COUNT);

	if (all_points.length > HB_GLYF_MAX_POINTS)
	{
	  current_glyphs->del (item_gid);
	  return false;
	}

	comp_index++;
        current_glyphs->del (item_gid);
      }

      if (head_maxp_info && depth == 0)
      {
        if (composite_contours)
          head_maxp_info->maxCompositeContours = hb_max (head_maxp_info->maxCompositeContours, *composite_contours);
        head_maxp_info->maxCompositePoints = hb_max (head_maxp_info->maxCompositePoints, all_points.length);
        head_maxp_info->maxComponentElements = hb_max (head_maxp_info->maxComponentElements, comp_index);
      }
      all_points.extend (phantoms);
    } break;
    case EMPTY:
      all_points.extend (phantoms);
      break;
    }

    if (depth == 0 && shift_points_hori) /* Apply at top level */
    {
      /* Undocumented rasterizer behavior:
       * Shift points horizontally by the updated left side bearing
       */
      float v = -phantoms[PHANTOM_LEFT].x;
      if (v)
        for (auto &point : all_points)
	  point.x += v;
    }

    return !all_points.in_error ();
  }

  bool get_extents_without_var_scaled (hb_font_t *font, const glyf_accelerator_t &glyf_accelerator,
				       hb_glyph_extents_t *extents) const
  {
    if (type == EMPTY) return true; /* Empty glyph; zero extents. */
    return header->get_extents_without_var_scaled (font, glyf_accelerator, gid, extents);
  }

  hb_bytes_t get_bytes () const { return bytes; }
  glyph_type_t get_type () const { return type; }
  const GlyphHeader *get_header () const { return header; }

  Glyph () : bytes (),
             header (bytes.as<GlyphHeader> ()),
             gid (-1),
             type(EMPTY)
  {}

  Glyph (hb_bytes_t bytes_,
	 hb_codepoint_t gid_ = (unsigned) -1) : bytes (bytes_),
                                                header (bytes.as<GlyphHeader> ()),
                                                gid (gid_)
  {
    int num_contours = header->numberOfContours;
    if (unlikely (num_contours == 0)) type = EMPTY;
    else if (num_contours > 0) type = SIMPLE;
    else if (num_contours <= -1) type = COMPOSITE;
    else type = EMPTY; // Spec deviation; Spec says COMPOSITE, but not seen in the wild.
  }

  protected:
  hb_bytes_t bytes;
  const GlyphHeader *header;
  hb_codepoint_t gid;
  glyph_type_t type;
};


} /* namespace glyf_impl */
} /* namespace OT */


#endif /* OT_GLYF_GLYPH_HH */
