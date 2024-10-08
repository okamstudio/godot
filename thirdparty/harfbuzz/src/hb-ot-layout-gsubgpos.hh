/*
 * Copyright © 2007,2008,2009,2010  Red Hat, Inc.
 * Copyright © 2010,2012  Google, Inc.
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
 * Red Hat Author(s): Behdad Esfahbod
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_OT_LAYOUT_GSUBGPOS_HH
#define HB_OT_LAYOUT_GSUBGPOS_HH

#include "hb.hh"
#include "hb-buffer.hh"
#include "hb-map.hh"
#include "hb-set.hh"
#include "hb-ot-map.hh"
#include "hb-ot-layout-common.hh"
#include "hb-ot-layout-gdef-table.hh"


namespace OT {


struct hb_intersects_context_t :
       hb_dispatch_context_t<hb_intersects_context_t, bool>
{
  template <typename T>
  return_t dispatch (const T &obj) { return obj.intersects (this->glyphs); }
  static return_t default_return_value () { return false; }
  bool stop_sublookup_iteration (return_t r) const { return r; }

  const hb_set_t *glyphs;

  hb_intersects_context_t (const hb_set_t *glyphs_) :
                            glyphs (glyphs_) {}
};

struct hb_have_non_1to1_context_t :
       hb_dispatch_context_t<hb_have_non_1to1_context_t, bool>
{
  template <typename T>
  return_t dispatch (const T &obj) { return obj.may_have_non_1to1 (); }
  static return_t default_return_value () { return false; }
  bool stop_sublookup_iteration (return_t r) const { return r; }
};

struct hb_closure_context_t :
       hb_dispatch_context_t<hb_closure_context_t>
{
  typedef return_t (*recurse_func_t) (hb_closure_context_t *c, unsigned lookup_index, hb_set_t *covered_seq_indicies, unsigned seq_index, unsigned end_index);
  template <typename T>
  return_t dispatch (const T &obj) { obj.closure (this); return hb_empty_t (); }
  static return_t default_return_value () { return hb_empty_t (); }
  void recurse (unsigned lookup_index, hb_set_t *covered_seq_indicies, unsigned seq_index, unsigned end_index)
  {
    if (unlikely (nesting_level_left == 0 || !recurse_func))
      return;

    nesting_level_left--;
    recurse_func (this, lookup_index, covered_seq_indicies, seq_index, end_index);
    nesting_level_left++;
  }

  void reset_lookup_visit_count ()
  { lookup_count = 0; }

  bool lookup_limit_exceeded ()
  { return lookup_count > HB_MAX_LOOKUP_VISIT_COUNT; }

  bool should_visit_lookup (unsigned int lookup_index)
  {
    if (lookup_count++ > HB_MAX_LOOKUP_VISIT_COUNT)
      return false;

    if (is_lookup_done (lookup_index))
      return false;

    return true;
  }

  bool is_lookup_done (unsigned int lookup_index)
  {
    if (unlikely (done_lookups_glyph_count->in_error () ||
		  done_lookups_glyph_set->in_error ()))
      return true;

    /* Have we visited this lookup with the current set of glyphs? */
    if (done_lookups_glyph_count->get (lookup_index) != glyphs->get_population ())
    {
      done_lookups_glyph_count->set (lookup_index, glyphs->get_population ());

      if (!done_lookups_glyph_set->has (lookup_index))
      {
	if (unlikely (!done_lookups_glyph_set->set (lookup_index, hb::unique_ptr<hb_set_t> {hb_set_create ()})))
	  return true;
      }

      done_lookups_glyph_set->get (lookup_index)->clear ();
    }

    hb_set_t *covered_glyph_set = done_lookups_glyph_set->get (lookup_index);
    if (unlikely (covered_glyph_set->in_error ()))
      return true;
    if (parent_active_glyphs ().is_subset (*covered_glyph_set))
      return true;

    covered_glyph_set->union_ (parent_active_glyphs ());
    return false;
  }

  const hb_set_t& previous_parent_active_glyphs () {
    if (active_glyphs_stack.length <= 1)
      return *glyphs;

    return active_glyphs_stack[active_glyphs_stack.length - 2];
  }

  const hb_set_t& parent_active_glyphs ()
  {
    if (!active_glyphs_stack)
      return *glyphs;

    return active_glyphs_stack.tail ();
  }

  hb_set_t* push_cur_active_glyphs ()
  {
    hb_set_t *s = active_glyphs_stack.push ();
    if (unlikely (active_glyphs_stack.in_error ()))
      return nullptr;
    return s;
  }

  bool pop_cur_done_glyphs ()
  {
    if (!active_glyphs_stack)
      return false;

    active_glyphs_stack.pop ();
    return true;
  }

  hb_face_t *face;
  hb_set_t *glyphs;
  hb_set_t output[1];
  hb_vector_t<hb_set_t> active_glyphs_stack;
  recurse_func_t recurse_func = nullptr;
  unsigned int nesting_level_left;

  hb_closure_context_t (hb_face_t *face_,
			hb_set_t *glyphs_,
			hb_map_t *done_lookups_glyph_count_,
			hb_hashmap_t<unsigned, hb::unique_ptr<hb_set_t>> *done_lookups_glyph_set_,
			unsigned int nesting_level_left_ = HB_MAX_NESTING_LEVEL) :
			  face (face_),
			  glyphs (glyphs_),
			  nesting_level_left (nesting_level_left_),
			  done_lookups_glyph_count (done_lookups_glyph_count_),
			  done_lookups_glyph_set (done_lookups_glyph_set_)
  {}

  ~hb_closure_context_t () { flush (); }

  void set_recurse_func (recurse_func_t func) { recurse_func = func; }

  void flush ()
  {
    output->del_range (face->get_num_glyphs (), HB_SET_VALUE_INVALID);	/* Remove invalid glyphs. */
    glyphs->union_ (*output);
    output->clear ();
    active_glyphs_stack.pop ();
    active_glyphs_stack.reset ();
  }

  private:
  hb_map_t *done_lookups_glyph_count;
  hb_hashmap_t<unsigned, hb::unique_ptr<hb_set_t>> *done_lookups_glyph_set;
  unsigned int lookup_count = 0;
};



struct hb_closure_lookups_context_t :
       hb_dispatch_context_t<hb_closure_lookups_context_t>
{
  typedef return_t (*recurse_func_t) (hb_closure_lookups_context_t *c, unsigned lookup_index);
  template <typename T>
  return_t dispatch (const T &obj) { obj.closure_lookups (this); return hb_empty_t (); }
  static return_t default_return_value () { return hb_empty_t (); }
  void recurse (unsigned lookup_index)
  {
    if (unlikely (nesting_level_left == 0 || !recurse_func))
      return;

    /* Return if new lookup was recursed to before. */
    if (lookup_limit_exceeded ()
        || visited_lookups->in_error ()
        || visited_lookups->has (lookup_index))
      // Don't increment lookup count here, that will be done in the call to closure_lookups()
      // made by recurse_func.
      return;

    nesting_level_left--;
    recurse_func (this, lookup_index);
    nesting_level_left++;
  }

  void set_lookup_visited (unsigned lookup_index)
  { visited_lookups->add (lookup_index); }

  void set_lookup_inactive (unsigned lookup_index)
  { inactive_lookups->add (lookup_index); }

  bool lookup_limit_exceeded ()
  {
    bool ret = lookup_count > HB_MAX_LOOKUP_VISIT_COUNT;
    if (ret)
      DEBUG_MSG (SUBSET, nullptr, "lookup visit count limit exceeded in lookup closure!");
    return ret; }

  bool is_lookup_visited (unsigned lookup_index)
  {
    if (unlikely (lookup_count++ > HB_MAX_LOOKUP_VISIT_COUNT))
    {
      DEBUG_MSG (SUBSET, nullptr, "total visited lookup count %u exceeds max limit, lookup %u is dropped.",
                 lookup_count, lookup_index);
      return true;
    }

    if (unlikely (visited_lookups->in_error ()))
      return true;

    return visited_lookups->has (lookup_index);
  }

  hb_face_t *face;
  const hb_set_t *glyphs;
  recurse_func_t recurse_func;
  unsigned int nesting_level_left;

  hb_closure_lookups_context_t (hb_face_t *face_,
				const hb_set_t *glyphs_,
				hb_set_t *visited_lookups_,
				hb_set_t *inactive_lookups_,
				unsigned nesting_level_left_ = HB_MAX_NESTING_LEVEL) :
				face (face_),
				glyphs (glyphs_),
				recurse_func (nullptr),
				nesting_level_left (nesting_level_left_),
				visited_lookups (visited_lookups_),
				inactive_lookups (inactive_lookups_),
				lookup_count (0) {}

  void set_recurse_func (recurse_func_t func) { recurse_func = func; }

  private:
  hb_set_t *visited_lookups;
  hb_set_t *inactive_lookups;
  unsigned int lookup_count;
};

struct hb_would_apply_context_t :
       hb_dispatch_context_t<hb_would_apply_context_t, bool>
{
  template <typename T>
  return_t dispatch (const T &obj) { return obj.would_apply (this); }
  static return_t default_return_value () { return false; }
  bool stop_sublookup_iteration (return_t r) const { return r; }

  hb_face_t *face;
  const hb_codepoint_t *glyphs;
  unsigned int len;
  bool zero_context;

  hb_would_apply_context_t (hb_face_t *face_,
			    const hb_codepoint_t *glyphs_,
			    unsigned int len_,
			    bool zero_context_) :
			      face (face_),
			      glyphs (glyphs_),
			      len (len_),
			      zero_context (zero_context_) {}
};

struct hb_collect_glyphs_context_t :
       hb_dispatch_context_t<hb_collect_glyphs_context_t>
{
  typedef return_t (*recurse_func_t) (hb_collect_glyphs_context_t *c, unsigned int lookup_index);
  template <typename T>
  return_t dispatch (const T &obj) { obj.collect_glyphs (this); return hb_empty_t (); }
  static return_t default_return_value () { return hb_empty_t (); }
  void recurse (unsigned int lookup_index)
  {
    if (unlikely (nesting_level_left == 0 || !recurse_func))
      return;

    /* Note that GPOS sets recurse_func to nullptr already, so it doesn't get
     * past the previous check.  For GSUB, we only want to collect the output
     * glyphs in the recursion.  If output is not requested, we can go home now.
     *
     * Note further, that the above is not exactly correct.  A recursed lookup
     * is allowed to match input that is not matched in the context, but that's
     * not how most fonts are built.  It's possible to relax that and recurse
     * with all sets here if it proves to be an issue.
     */

    if (output == hb_set_get_empty ())
      return;

    /* Return if new lookup was recursed to before. */
    if (recursed_lookups->has (lookup_index))
      return;

    hb_set_t *old_before = before;
    hb_set_t *old_input  = input;
    hb_set_t *old_after  = after;
    before = input = after = hb_set_get_empty ();

    nesting_level_left--;
    recurse_func (this, lookup_index);
    nesting_level_left++;

    before = old_before;
    input  = old_input;
    after  = old_after;

    recursed_lookups->add (lookup_index);
  }

  hb_face_t *face;
  hb_set_t *before;
  hb_set_t *input;
  hb_set_t *after;
  hb_set_t *output;
  recurse_func_t recurse_func;
  hb_set_t *recursed_lookups;
  unsigned int nesting_level_left;

  hb_collect_glyphs_context_t (hb_face_t *face_,
			       hb_set_t  *glyphs_before, /* OUT.  May be NULL */
			       hb_set_t  *glyphs_input,  /* OUT.  May be NULL */
			       hb_set_t  *glyphs_after,  /* OUT.  May be NULL */
			       hb_set_t  *glyphs_output, /* OUT.  May be NULL */
			       unsigned int nesting_level_left_ = HB_MAX_NESTING_LEVEL) :
			      face (face_),
			      before (glyphs_before ? glyphs_before : hb_set_get_empty ()),
			      input  (glyphs_input  ? glyphs_input  : hb_set_get_empty ()),
			      after  (glyphs_after  ? glyphs_after  : hb_set_get_empty ()),
			      output (glyphs_output ? glyphs_output : hb_set_get_empty ()),
			      recurse_func (nullptr),
			      recursed_lookups (hb_set_create ()),
			      nesting_level_left (nesting_level_left_) {}
  ~hb_collect_glyphs_context_t () { hb_set_destroy (recursed_lookups); }

  void set_recurse_func (recurse_func_t func) { recurse_func = func; }
};



template <typename set_t>
struct hb_collect_coverage_context_t :
       hb_dispatch_context_t<hb_collect_coverage_context_t<set_t>, const Coverage &>
{
  typedef const Coverage &return_t; // Stoopid that we have to dupe this here.
  template <typename T>
  return_t dispatch (const T &obj) { return obj.get_coverage (); }
  static return_t default_return_value () { return Null (Coverage); }
  bool stop_sublookup_iteration (return_t r) const
  {
    r.collect_coverage (set);
    return false;
  }

  hb_collect_coverage_context_t (set_t *set_) :
				   set (set_) {}

  set_t *set;
};

struct hb_ot_apply_context_t :
       hb_dispatch_context_t<hb_ot_apply_context_t, bool, HB_DEBUG_APPLY>
{
  struct matcher_t
  {
    typedef bool (*match_func_t) (hb_glyph_info_t &info, unsigned value, const void *data);

    void set_ignore_zwnj (bool ignore_zwnj_) { ignore_zwnj = ignore_zwnj_; }
    void set_ignore_zwj (bool ignore_zwj_) { ignore_zwj = ignore_zwj_; }
    void set_ignore_hidden (bool ignore_hidden_) { ignore_hidden = ignore_hidden_; }
    void set_lookup_props (unsigned int lookup_props_) { lookup_props = lookup_props_; }
    void set_mask (hb_mask_t mask_) { mask = mask_; }
    void set_per_syllable (bool per_syllable_) { per_syllable = per_syllable_; }
    void set_syllable (uint8_t syllable_)  { syllable = per_syllable ? syllable_ : 0; }
    void set_match_func (match_func_t match_func_,
			 const void *match_data_)
    { match_func = match_func_; match_data = match_data_; }

    enum may_match_t {
      MATCH_NO,
      MATCH_YES,
      MATCH_MAYBE
    };

#ifndef HB_OPTIMIZE_SIZE
    HB_ALWAYS_INLINE
#endif
    may_match_t may_match (hb_glyph_info_t &info,
			   hb_codepoint_t glyph_data) const
    {
      if (!(info.mask & mask) ||
	  (syllable && syllable != info.syllable ()))
	return MATCH_NO;

      if (match_func)
	return match_func (info, glyph_data, match_data) ? MATCH_YES : MATCH_NO;

      return MATCH_MAYBE;
    }

    enum may_skip_t {
      SKIP_NO,
      SKIP_YES,
      SKIP_MAYBE
    };

#ifndef HB_OPTIMIZE_SIZE
    HB_ALWAYS_INLINE
#endif
    may_skip_t may_skip (const hb_ot_apply_context_t *c,
			 const hb_glyph_info_t       &info) const
    {
      if (!c->check_glyph_property (&info, lookup_props))
	return SKIP_YES;

      if (unlikely (_hb_glyph_info_is_default_ignorable (&info) &&
		    (ignore_zwnj || !_hb_glyph_info_is_zwnj (&info)) &&
		    (ignore_zwj || !_hb_glyph_info_is_zwj (&info)) &&
		    (ignore_hidden || !_hb_glyph_info_is_hidden (&info))))
	return SKIP_MAYBE;

      return SKIP_NO;
    }

    protected:
    unsigned int lookup_props = 0;
    hb_mask_t mask = -1;
    bool ignore_zwnj = false;
    bool ignore_zwj = false;
    bool ignore_hidden = false;
    bool per_syllable = false;
    uint8_t syllable = 0;
    match_func_t match_func = nullptr;
    const void *match_data = nullptr;
  };

  struct skipping_iterator_t
  {
    void init (hb_ot_apply_context_t *c_, bool context_match = false)
    {
      c = c_;
      end = c->buffer->len;
      match_glyph_data16 = nullptr;
#ifndef HB_NO_BEYOND_64K
      match_glyph_data24 = nullptr;
#endif
      matcher.set_match_func (nullptr, nullptr);
      matcher.set_lookup_props (c->lookup_props);
      /* Ignore ZWNJ if we are matching GPOS, or matching GSUB context and asked to. */
      matcher.set_ignore_zwnj (c->table_index == 1 || (context_match && c->auto_zwnj));
      /* Ignore ZWJ if we are matching context, or asked to. */
      matcher.set_ignore_zwj  (context_match || c->auto_zwj);
      /* Ignore hidden glyphs (like CGJ) during GPOS. */
      matcher.set_ignore_hidden (c->table_index == 1);
      matcher.set_mask (context_match ? -1 : c->lookup_mask);
      /* Per syllable matching is only for GSUB. */
      matcher.set_per_syllable (c->table_index == 0 && c->per_syllable);
      matcher.set_syllable (0);
    }
    void set_lookup_props (unsigned int lookup_props)
    {
      matcher.set_lookup_props (lookup_props);
    }
    void set_match_func (matcher_t::match_func_t match_func_,
			 const void *match_data_)
    {
      matcher.set_match_func (match_func_, match_data_);
    }
    void set_glyph_data (const HBUINT16 glyph_data[])
    {
      match_glyph_data16 = glyph_data;
#ifndef HB_NO_BEYOND_64K
      match_glyph_data24 = nullptr;
#endif
    }
#ifndef HB_NO_BEYOND_64K
    void set_glyph_data (const HBUINT24 glyph_data[])
    {
      match_glyph_data16 = nullptr;
      match_glyph_data24 = glyph_data;
    }
#endif

#ifndef HB_OPTIMIZE_SIZE
    HB_ALWAYS_INLINE
#endif
    void reset (unsigned int start_index_)
    {
      idx = start_index_;
      end = c->buffer->len;
      matcher.set_syllable (start_index_ == c->buffer->idx ? c->buffer->cur().syllable () : 0);
    }

#ifndef HB_OPTIMIZE_SIZE
    HB_ALWAYS_INLINE
#endif
    void reset_fast (unsigned int start_index_)
    {
      // Doesn't set end or syllable. Used by GPOS which doesn't care / change.
      idx = start_index_;
    }

    void reject ()
    {
      backup_glyph_data ();
    }

    matcher_t::may_skip_t
#ifndef HB_OPTIMIZE_SIZE
    HB_ALWAYS_INLINE
#endif
    may_skip (const hb_glyph_info_t &info) const
    { return matcher.may_skip (c, info); }

    enum match_t {
      MATCH,
      NOT_MATCH,
      SKIP
    };

#ifndef HB_OPTIMIZE_SIZE
    HB_ALWAYS_INLINE
#endif
    match_t match (hb_glyph_info_t &info)
    {
      matcher_t::may_skip_t skip = matcher.may_skip (c, info);
      if (unlikely (skip == matcher_t::SKIP_YES))
	return SKIP;

      matcher_t::may_match_t match = matcher.may_match (info, get_glyph_data ());
      if (match == matcher_t::MATCH_YES ||
	  (match == matcher_t::MATCH_MAYBE &&
	   skip == matcher_t::SKIP_NO))
	return MATCH;

      if (skip == matcher_t::SKIP_NO)
        return NOT_MATCH;

      return SKIP;
  }

#ifndef HB_OPTIMIZE_SIZE
    HB_ALWAYS_INLINE
#endif
    bool next (unsigned *unsafe_to = nullptr)
    {
      const signed stop = (signed) end - 1;
      while ((signed) idx < stop)
      {
	idx++;
	switch (match (c->buffer->info[idx]))
	{
	  case MATCH:
	  {
	    advance_glyph_data ();
	    return true;
	  }
	  case NOT_MATCH:
	  {
	    if (unsafe_to)
	      *unsafe_to = idx + 1;
	    return false;
	  }
	  case SKIP:
	    continue;
	}
      }
      if (unsafe_to)
        *unsafe_to = end;
      return false;
    }
#ifndef HB_OPTIMIZE_SIZE
    HB_ALWAYS_INLINE
#endif
    bool prev (unsigned *unsafe_from = nullptr)
    {
      const unsigned stop = 0;
      while (idx > stop)
      {
	idx--;
	switch (match (c->buffer->out_info[idx]))
	{
	  case MATCH:
	  {
	    advance_glyph_data ();
	    return true;
	  }
	  case NOT_MATCH:
	  {
	    if (unsafe_from)
	      *unsafe_from = hb_max (1u, idx) - 1u;
	    return false;
	  }
	  case SKIP:
	    continue;
	}
      }
      if (unsafe_from)
        *unsafe_from = 0;
      return false;
    }

    HB_ALWAYS_INLINE
    hb_codepoint_t
    get_glyph_data ()
    {
      if (match_glyph_data16) return *match_glyph_data16;
#ifndef HB_NO_BEYOND_64K
      else
      if (match_glyph_data24) return *match_glyph_data24;
#endif
      return 0;
    }
    HB_ALWAYS_INLINE
    void
    advance_glyph_data ()
    {
      if (match_glyph_data16) match_glyph_data16++;
#ifndef HB_NO_BEYOND_64K
      else
      if (match_glyph_data24) match_glyph_data24++;
#endif
    }
    void
    backup_glyph_data ()
    {
      if (match_glyph_data16) match_glyph_data16--;
#ifndef HB_NO_BEYOND_64K
      else
      if (match_glyph_data24) match_glyph_data24--;
#endif
    }

    unsigned int idx;
    protected:
    hb_ot_apply_context_t *c;
    matcher_t matcher;
    const HBUINT16 *match_glyph_data16;
#ifndef HB_NO_BEYOND_64K
    const HBUINT24 *match_glyph_data24;
#endif

    unsigned int end;
  };


  const char *get_name () { return "APPLY"; }
  typedef return_t (*recurse_func_t) (hb_ot_apply_context_t *c, unsigned int lookup_index);
  template <typename T>
  return_t dispatch (const T &obj) { return obj.apply (this); }
  static return_t default_return_value () { return false; }
  bool stop_sublookup_iteration (return_t r) const { return r; }
  return_t recurse (unsigned int sub_lookup_index)
  {
    if (unlikely (nesting_level_left == 0 || !recurse_func || buffer->max_ops-- <= 0))
    {
      buffer->shaping_failed = true;
      return default_return_value ();
    }

    nesting_level_left--;
    bool ret = recurse_func (this, sub_lookup_index);
    nesting_level_left++;
    return ret;
  }

  skipping_iterator_t iter_input, iter_context;

  unsigned int table_index; /* GSUB/GPOS */
  hb_font_t *font;
  hb_face_t *face;
  hb_buffer_t *buffer;
  hb_sanitize_context_t sanitizer;
  recurse_func_t recurse_func = nullptr;
  const GDEF &gdef;
  const GDEF::accelerator_t &gdef_accel;
  const ItemVariationStore &var_store;
  ItemVariationStore::cache_t *var_store_cache;
  hb_set_digest_t digest;

  hb_direction_t direction;
  hb_mask_t lookup_mask = 1;
  unsigned int lookup_index = (unsigned) -1;
  unsigned int lookup_props = 0;
  unsigned int nesting_level_left = HB_MAX_NESTING_LEVEL;

  bool has_glyph_classes;
  bool auto_zwnj = true;
  bool auto_zwj = true;
  bool per_syllable = false;
  bool random = false;
  unsigned new_syllables = (unsigned) -1;

  signed last_base = -1; // GPOS uses
  unsigned last_base_until = 0; // GPOS uses

  hb_ot_apply_context_t (unsigned int table_index_,
			 hb_font_t *font_,
			 hb_buffer_t *buffer_,
			 hb_blob_t *table_blob_) :
			table_index (table_index_),
			font (font_), face (font->face), buffer (buffer_),
			sanitizer (table_blob_),
			gdef (
#ifndef HB_NO_OT_LAYOUT
			      *face->table.GDEF->table
#else
			      Null (GDEF)
#endif
			     ),
			gdef_accel (
#ifndef HB_NO_OT_LAYOUT
			      *face->table.GDEF
#else
			      Null (GDEF::accelerator_t)
#endif
			     ),
			var_store (gdef.get_var_store ()),
			var_store_cache (
#ifndef HB_NO_VAR
					 table_index == 1 && font->num_coords ? var_store.create_cache () : nullptr
#else
					 nullptr
#endif
					),
			digest (buffer_->digest ()),
			direction (buffer_->props.direction),
			has_glyph_classes (gdef.has_glyph_classes ())
  { init_iters (); }

  ~hb_ot_apply_context_t ()
  {
#ifndef HB_NO_VAR
    ItemVariationStore::destroy_cache (var_store_cache);
#endif
  }

  void init_iters ()
  {
    iter_input.init (this, false);
    iter_context.init (this, true);
  }

  void set_lookup_mask (hb_mask_t mask, bool init = true) { lookup_mask = mask; last_base = -1; last_base_until = 0; if (init) init_iters (); }
  void set_auto_zwj (bool auto_zwj_, bool init = true) { auto_zwj = auto_zwj_; if (init) init_iters (); }
  void set_auto_zwnj (bool auto_zwnj_, bool init = true) { auto_zwnj = auto_zwnj_; if (init) init_iters (); }
  void set_per_syllable (bool per_syllable_, bool init = true) { per_syllable = per_syllable_; if (init) init_iters (); }
  void set_random (bool random_) { random = random_; }
  void set_recurse_func (recurse_func_t func) { recurse_func = func; }
  void set_lookup_index (unsigned int lookup_index_) { lookup_index = lookup_index_; }
  void set_lookup_props (unsigned int lookup_props_) { lookup_props = lookup_props_; init_iters (); }

  uint32_t random_number ()
  {
    /* http://www.cplusplus.com/reference/random/minstd_rand/ */
    buffer->random_state = buffer->random_state * 48271 % 2147483647;
    return buffer->random_state;
  }

  bool match_properties_mark (hb_codepoint_t  glyph,
			      unsigned int    glyph_props,
			      unsigned int    match_props) const
  {
    /* If using mark filtering sets, the high short of
     * match_props has the set index.
     */
    if (match_props & LookupFlag::UseMarkFilteringSet)
      return gdef_accel.mark_set_covers (match_props >> 16, glyph);

    /* The second byte of match_props has the meaning
     * "ignore marks of attachment type different than
     * the attachment type specified."
     */
    if (match_props & LookupFlag::MarkAttachmentType)
      return (match_props & LookupFlag::MarkAttachmentType) == (glyph_props & LookupFlag::MarkAttachmentType);

    return true;
  }

#ifndef HB_OPTIMIZE_SIZE
  HB_ALWAYS_INLINE
#endif
  bool check_glyph_property (const hb_glyph_info_t *info,
			     unsigned int  match_props) const
  {
    unsigned int glyph_props = _hb_glyph_info_get_glyph_props (info);

    /* Not covered, if, for example, glyph class is ligature and
     * match_props includes LookupFlags::IgnoreLigatures
     */
    if (glyph_props & match_props & LookupFlag::IgnoreFlags)
      return false;

    if (unlikely (glyph_props & HB_OT_LAYOUT_GLYPH_PROPS_MARK))
      return match_properties_mark (info->codepoint, glyph_props, match_props);

    return true;
  }

  void _set_glyph_class (hb_codepoint_t glyph_index,
			  unsigned int class_guess = 0,
			  bool ligature = false,
			  bool component = false)
  {
    digest.add (glyph_index);

    if (new_syllables != (unsigned) -1)
      buffer->cur().syllable() = new_syllables;

    unsigned int props = _hb_glyph_info_get_glyph_props (&buffer->cur());
    props |= HB_OT_LAYOUT_GLYPH_PROPS_SUBSTITUTED;
    if (ligature)
    {
      props |= HB_OT_LAYOUT_GLYPH_PROPS_LIGATED;
      /* In the only place that the MULTIPLIED bit is used, Uniscribe
       * seems to only care about the "last" transformation between
       * Ligature and Multiple substitutions.  Ie. if you ligate, expand,
       * and ligate again, it forgives the multiplication and acts as
       * if only ligation happened.  As such, clear MULTIPLIED bit.
       */
      props &= ~HB_OT_LAYOUT_GLYPH_PROPS_MULTIPLIED;
    }
    if (component)
      props |= HB_OT_LAYOUT_GLYPH_PROPS_MULTIPLIED;
    if (likely (has_glyph_classes))
    {
      props &= HB_OT_LAYOUT_GLYPH_PROPS_PRESERVE;
      _hb_glyph_info_set_glyph_props (&buffer->cur(), props | gdef_accel.get_glyph_props (glyph_index));
    }
    else if (class_guess)
    {
      props &= HB_OT_LAYOUT_GLYPH_PROPS_PRESERVE;
      _hb_glyph_info_set_glyph_props (&buffer->cur(), props | class_guess);
    }
    else
      _hb_glyph_info_set_glyph_props (&buffer->cur(), props);
  }

  void replace_glyph (hb_codepoint_t glyph_index)
  {
    _set_glyph_class (glyph_index);
    (void) buffer->replace_glyph (glyph_index);
  }
  void replace_glyph_inplace (hb_codepoint_t glyph_index)
  {
    _set_glyph_class (glyph_index);
    buffer->cur().codepoint = glyph_index;
  }
  void replace_glyph_with_ligature (hb_codepoint_t glyph_index,
				    unsigned int class_guess)
  {
    _set_glyph_class (glyph_index, class_guess, true);
    (void) buffer->replace_glyph (glyph_index);
  }
  void output_glyph_for_component (hb_codepoint_t glyph_index,
				   unsigned int class_guess)
  {
    _set_glyph_class (glyph_index, class_guess, false, true);
    (void) buffer->output_glyph (glyph_index);
  }
};


struct hb_accelerate_subtables_context_t :
       hb_dispatch_context_t<hb_accelerate_subtables_context_t>
{
  template <typename Type>
  static inline bool apply_to (const void *obj, hb_ot_apply_context_t *c)
  {
    const Type *typed_obj = (const Type *) obj;
    return typed_obj->apply (c);
  }

#ifndef HB_NO_OT_LAYOUT_LOOKUP_CACHE
  template <typename T>
  static inline auto apply_cached_ (const T *obj, hb_ot_apply_context_t *c, hb_priority<1>) HB_RETURN (bool, obj->apply_cached (c) )
  template <typename T>
  static inline auto apply_cached_ (const T *obj, hb_ot_apply_context_t *c, hb_priority<0>) HB_RETURN (bool, obj->apply (c) )
  template <typename Type>
  static inline bool apply_cached_to (const void *obj, hb_ot_apply_context_t *c)
  {
    const Type *typed_obj = (const Type *) obj;
    return apply_cached_ (typed_obj, c, hb_prioritize);
  }

  template <typename T>
  static inline auto cache_func_ (const T *obj, hb_ot_apply_context_t *c, bool enter, hb_priority<1>) HB_RETURN (bool, obj->cache_func (c, enter) )
  template <typename T>
  static inline bool cache_func_ (const T *obj, hb_ot_apply_context_t *c, bool enter, hb_priority<0>) { return false; }
  template <typename Type>
  static inline bool cache_func_to (const void *obj, hb_ot_apply_context_t *c, bool enter)
  {
    const Type *typed_obj = (const Type *) obj;
    return cache_func_ (typed_obj, c, enter, hb_prioritize);
  }
#endif

  typedef bool (*hb_apply_func_t) (const void *obj, hb_ot_apply_context_t *c);
  typedef bool (*hb_cache_func_t) (const void *obj, hb_ot_apply_context_t *c, bool enter);

  struct hb_applicable_t
  {
    friend struct hb_accelerate_subtables_context_t;
    friend struct hb_ot_layout_lookup_accelerator_t;

    template <typename T>
    void init (const T &obj_,
	       hb_apply_func_t apply_func_
#ifndef HB_NO_OT_LAYOUT_LOOKUP_CACHE
	       , hb_apply_func_t apply_cached_func_
	       , hb_cache_func_t cache_func_
#endif
		)
    {
      obj = &obj_;
      apply_func = apply_func_;
#ifndef HB_NO_OT_LAYOUT_LOOKUP_CACHE
      apply_cached_func = apply_cached_func_;
      cache_func = cache_func_;
#endif
      digest.init ();
      obj_.get_coverage ().collect_coverage (&digest);
    }

    bool apply (hb_ot_apply_context_t *c) const
    {
      return digest.may_have (c->buffer->cur().codepoint) && apply_func (obj, c);
    }
#ifndef HB_NO_OT_LAYOUT_LOOKUP_CACHE
    bool apply_cached (hb_ot_apply_context_t *c) const
    {
      return digest.may_have (c->buffer->cur().codepoint) &&  apply_cached_func (obj, c);
    }
    bool cache_enter (hb_ot_apply_context_t *c) const
    {
      return cache_func (obj, c, true);
    }
    void cache_leave (hb_ot_apply_context_t *c) const
    {
      cache_func (obj, c, false);
    }
#endif

    private:
    const void *obj;
    hb_apply_func_t apply_func;
#ifndef HB_NO_OT_LAYOUT_LOOKUP_CACHE
    hb_apply_func_t apply_cached_func;
    hb_cache_func_t cache_func;
#endif
    hb_set_digest_t digest;
  };

#ifndef HB_NO_OT_LAYOUT_LOOKUP_CACHE
  template <typename T>
  auto cache_cost (const T &obj, hb_priority<1>) HB_AUTO_RETURN ( obj.cache_cost () )
  template <typename T>
  auto cache_cost (const T &obj, hb_priority<0>) HB_AUTO_RETURN ( 0u )
#endif

  /* Dispatch interface. */
  template <typename T>
  return_t dispatch (const T &obj)
  {
    hb_applicable_t *entry = &array[i++];

    entry->init (obj,
		 apply_to<T>
#ifndef HB_NO_OT_LAYOUT_LOOKUP_CACHE
		 , apply_cached_to<T>
		 , cache_func_to<T>
#endif
		 );

#ifndef HB_NO_OT_LAYOUT_LOOKUP_CACHE
    /* Cache handling
     *
     * We allow one subtable from each lookup to use a cache. The assumption
     * being that multiple subtables of the same lookup cannot use a cache
     * because the resources they would use will collide.  As such, we ask
     * each subtable to tell us how much it costs (which a cache would avoid),
     * and we allocate the cache opportunity to the costliest subtable.
     */
    unsigned cost = cache_cost (obj, hb_prioritize);
    if (cost > cache_user_cost)
    {
      cache_user_idx = i - 1;
      cache_user_cost = cost;
    }
#endif

    return hb_empty_t ();
  }
  static return_t default_return_value () { return hb_empty_t (); }

  hb_accelerate_subtables_context_t (hb_applicable_t *array_) :
				     array (array_) {}

  hb_applicable_t *array;
  unsigned i = 0;

#ifndef HB_NO_OT_LAYOUT_LOOKUP_CACHE
  unsigned cache_user_idx = (unsigned) -1;
  unsigned cache_user_cost = 0;
#endif
};


typedef bool (*intersects_func_t) (const hb_set_t *glyphs, unsigned value, const void *data, void *cache);
typedef void (*intersected_glyphs_func_t) (const hb_set_t *glyphs, const void *data, unsigned value, hb_set_t *intersected_glyphs, void *cache);
typedef void (*collect_glyphs_func_t) (hb_set_t *glyphs, unsigned value, const void *data);
typedef bool (*match_func_t) (hb_glyph_info_t &info, unsigned value, const void *data);

struct ContextClosureFuncs
{
  intersects_func_t intersects;
  intersected_glyphs_func_t intersected_glyphs;
};
struct ContextCollectGlyphsFuncs
{
  collect_glyphs_func_t collect;
};
struct ContextApplyFuncs
{
  match_func_t match;
};
struct ChainContextApplyFuncs
{
  match_func_t match[3];
};


static inline bool intersects_glyph (const hb_set_t *glyphs, unsigned value, const void *data HB_UNUSED, void *cache HB_UNUSED)
{
  return glyphs->has (value);
}
static inline bool intersects_class (const hb_set_t *glyphs, unsigned value, const void *data, void *cache)
{
  const ClassDef &class_def = *reinterpret_cast<const ClassDef *>(data);
  hb_map_t *map = (hb_map_t *) cache;

  hb_codepoint_t *cached_v;
  if (map->has (value, &cached_v))
    return *cached_v;

  bool v = class_def.intersects_class (glyphs, value);
  map->set (value, v);

  return v;
}
static inline bool intersects_coverage (const hb_set_t *glyphs, unsigned value, const void *data, void *cache HB_UNUSED)
{
  Offset16To<Coverage> coverage;
  coverage = value;
  return (data+coverage).intersects (glyphs);
}


static inline void intersected_glyph (const hb_set_t *glyphs HB_UNUSED, const void *data, unsigned value, hb_set_t *intersected_glyphs, HB_UNUSED void *cache)
{
  unsigned g = reinterpret_cast<const HBUINT16 *>(data)[value];
  intersected_glyphs->add (g);
}

using intersected_class_cache_t = hb_hashmap_t<unsigned, hb_set_t>;

static inline void intersected_class_glyphs (const hb_set_t *glyphs, const void *data, unsigned value, hb_set_t *intersected_glyphs, void *cache)
{
  const ClassDef &class_def = *reinterpret_cast<const ClassDef *>(data);

  intersected_class_cache_t *map = (intersected_class_cache_t *) cache;

  hb_set_t *cached_v;
  if (map->has (value, &cached_v))
  {
    intersected_glyphs->union_ (*cached_v);
    return;
  }

  hb_set_t v;
  class_def.intersected_class_glyphs (glyphs, value, &v);

  intersected_glyphs->union_ (v);

  map->set (value, std::move (v));
}

static inline void intersected_coverage_glyphs (const hb_set_t *glyphs, const void *data, unsigned value, hb_set_t *intersected_glyphs, HB_UNUSED void *cache)
{
  Offset16To<Coverage> coverage;
  coverage = value;
  (data+coverage).intersect_set (*glyphs, *intersected_glyphs);
}


template <typename HBUINT>
static inline bool array_is_subset_of (const hb_set_t *glyphs,
				       unsigned int count,
				       const HBUINT values[],
				       intersects_func_t intersects_func,
				       const void *intersects_data,
				       void *cache)
{
  for (const auto &_ : + hb_iter (values, count))
    if (!intersects_func (glyphs, _, intersects_data, cache)) return false;
  return true;
}


static inline void collect_glyph (hb_set_t *glyphs, unsigned value, const void *data HB_UNUSED)
{
  glyphs->add (value);
}
static inline void collect_class (hb_set_t *glyphs, unsigned value, const void *data)
{
  const ClassDef &class_def = *reinterpret_cast<const ClassDef *>(data);
  class_def.collect_class (glyphs, value);
}
static inline void collect_coverage (hb_set_t *glyphs, unsigned value, const void *data)
{
  Offset16To<Coverage> coverage;
  coverage = value;
  (data+coverage).collect_coverage (glyphs);
}
template <typename HBUINT>
static inline void collect_array (hb_collect_glyphs_context_t *c HB_UNUSED,
				  hb_set_t *glyphs,
				  unsigned int count,
				  const HBUINT values[],
				  collect_glyphs_func_t collect_func,
				  const void *collect_data)
{
  return
  + hb_iter (values, count)
  | hb_apply ([&] (const HBUINT &_) { collect_func (glyphs, _, collect_data); })
  ;
}


static inline bool match_always (hb_glyph_info_t &info HB_UNUSED, unsigned value HB_UNUSED, const void *data HB_UNUSED)
{
  return true;
}
static inline bool match_glyph (hb_glyph_info_t &info, unsigned value, const void *data HB_UNUSED)
{
  return info.codepoint == value;
}
static inline bool match_class (hb_glyph_info_t &info, unsigned value, const void *data)
{
  const ClassDef &class_def = *reinterpret_cast<const ClassDef *>(data);
  return class_def.get_class (info.codepoint) == value;
}
static inline bool match_class_cached (hb_glyph_info_t &info, unsigned value, const void *data)
{
  unsigned klass = info.syllable();
  if (klass < 255)
    return klass == value;
  const ClassDef &class_def = *reinterpret_cast<const ClassDef *>(data);
  klass = class_def.get_class (info.codepoint);
  if (likely (klass < 255))
    info.syllable() = klass;
  return klass == value;
}
static inline bool match_class_cached1 (hb_glyph_info_t &info, unsigned value, const void *data)
{
  unsigned klass = info.syllable() & 0x0F;
  if (klass < 15)
    return klass == value;
  const ClassDef &class_def = *reinterpret_cast<const ClassDef *>(data);
  klass = class_def.get_class (info.codepoint);
  if (likely (klass < 15))
    info.syllable() = (info.syllable() & 0xF0) | klass;
  return klass == value;
}
static inline bool match_class_cached2 (hb_glyph_info_t &info, unsigned value, const void *data)
{
  unsigned klass = (info.syllable() & 0xF0) >> 4;
  if (klass < 15)
    return klass == value;
  const ClassDef &class_def = *reinterpret_cast<const ClassDef *>(data);
  klass = class_def.get_class (info.codepoint);
  if (likely (klass < 15))
    info.syllable() = (info.syllable() & 0x0F) | (klass << 4);
  return klass == value;
}
static inline bool match_coverage (hb_glyph_info_t &info, unsigned value, const void *data)
{
  Offset16To<Coverage> coverage;
  coverage = value;
  return (data+coverage).get_coverage (info.codepoint) != NOT_COVERED;
}

template <typename HBUINT>
static inline bool would_match_input (hb_would_apply_context_t *c,
				      unsigned int count, /* Including the first glyph (not matched) */
				      const HBUINT input[], /* Array of input values--start with second glyph */
				      match_func_t match_func,
				      const void *match_data)
{
  if (count != c->len)
    return false;

  for (unsigned int i = 1; i < count; i++)
  {
    hb_glyph_info_t info;
    info.codepoint = c->glyphs[i];
    if (likely (!match_func (info, input[i - 1], match_data)))
      return false;
  }

  return true;
}
template <typename HBUINT>
#ifndef HB_OPTIMIZE_SIZE
HB_ALWAYS_INLINE
#endif
static bool match_input (hb_ot_apply_context_t *c,
			 unsigned int count, /* Including the first glyph (not matched) */
			 const HBUINT input[], /* Array of input values--start with second glyph */
			 match_func_t match_func,
			 const void *match_data,
			 unsigned int *end_position,
			 unsigned int *match_positions,
			 unsigned int *p_total_component_count = nullptr)
{
  TRACE_APPLY (nullptr);

  if (unlikely (count > HB_MAX_CONTEXT_LENGTH)) return_trace (false);

  hb_buffer_t *buffer = c->buffer;

  hb_ot_apply_context_t::skipping_iterator_t &skippy_iter = c->iter_input;
  skippy_iter.reset (buffer->idx);
  skippy_iter.set_match_func (match_func, match_data);
  skippy_iter.set_glyph_data (input);

  /*
   * This is perhaps the trickiest part of OpenType...  Remarks:
   *
   * - If all components of the ligature were marks, we call this a mark ligature.
   *
   * - If there is no GDEF, and the ligature is NOT a mark ligature, we categorize
   *   it as a ligature glyph.
   *
   * - Ligatures cannot be formed across glyphs attached to different components
   *   of previous ligatures.  Eg. the sequence is LAM,SHADDA,LAM,FATHA,HEH, and
   *   LAM,LAM,HEH form a ligature, leaving SHADDA,FATHA next to eachother.
   *   However, it would be wrong to ligate that SHADDA,FATHA sequence.
   *   There are a couple of exceptions to this:
   *
   *   o If a ligature tries ligating with marks that belong to it itself, go ahead,
   *     assuming that the font designer knows what they are doing (otherwise it can
   *     break Indic stuff when a matra wants to ligate with a conjunct,
   *
   *   o If two marks want to ligate and they belong to different components of the
   *     same ligature glyph, and said ligature glyph is to be ignored according to
   *     mark-filtering rules, then allow.
   *     https://github.com/harfbuzz/harfbuzz/issues/545
   */

  unsigned int total_component_count = 0;

  unsigned int first_lig_id = _hb_glyph_info_get_lig_id (&buffer->cur());
  unsigned int first_lig_comp = _hb_glyph_info_get_lig_comp (&buffer->cur());

  enum {
    LIGBASE_NOT_CHECKED,
    LIGBASE_MAY_NOT_SKIP,
    LIGBASE_MAY_SKIP
  } ligbase = LIGBASE_NOT_CHECKED;

  for (unsigned int i = 1; i < count; i++)
  {
    unsigned unsafe_to;
    if (!skippy_iter.next (&unsafe_to))
    {
      *end_position = unsafe_to;
      return_trace (false);
    }

    match_positions[i] = skippy_iter.idx;

    unsigned int this_lig_id = _hb_glyph_info_get_lig_id (&buffer->info[skippy_iter.idx]);
    unsigned int this_lig_comp = _hb_glyph_info_get_lig_comp (&buffer->info[skippy_iter.idx]);

    if (first_lig_id && first_lig_comp)
    {
      /* If first component was attached to a previous ligature component,
       * all subsequent components should be attached to the same ligature
       * component, otherwise we shouldn't ligate them... */
      if (first_lig_id != this_lig_id || first_lig_comp != this_lig_comp)
      {
	/* ...unless, we are attached to a base ligature and that base
	 * ligature is ignorable. */
	if (ligbase == LIGBASE_NOT_CHECKED)
	{
	  bool found = false;
	  const auto *out = buffer->out_info;
	  unsigned int j = buffer->out_len;
	  while (j && _hb_glyph_info_get_lig_id (&out[j - 1]) == first_lig_id)
	  {
	    if (_hb_glyph_info_get_lig_comp (&out[j - 1]) == 0)
	    {
	      j--;
	      found = true;
	      break;
	    }
	    j--;
	  }

	  if (found && skippy_iter.may_skip (out[j]) == hb_ot_apply_context_t::matcher_t::SKIP_YES)
	    ligbase = LIGBASE_MAY_SKIP;
	  else
	    ligbase = LIGBASE_MAY_NOT_SKIP;
	}

	if (ligbase == LIGBASE_MAY_NOT_SKIP)
	  return_trace (false);
      }
    }
    else
    {
      /* If first component was NOT attached to a previous ligature component,
       * all subsequent components should also NOT be attached to any ligature
       * component, unless they are attached to the first component itself! */
      if (this_lig_id && this_lig_comp && (this_lig_id != first_lig_id))
	return_trace (false);
    }

    total_component_count += _hb_glyph_info_get_lig_num_comps (&buffer->info[skippy_iter.idx]);
  }

  *end_position = skippy_iter.idx + 1;

  if (p_total_component_count)
  {
    total_component_count += _hb_glyph_info_get_lig_num_comps (&buffer->cur());
    *p_total_component_count = total_component_count;
  }

  match_positions[0] = buffer->idx;

  return_trace (true);
}
static inline bool ligate_input (hb_ot_apply_context_t *c,
				 unsigned int count, /* Including the first glyph */
				 const unsigned int *match_positions, /* Including the first glyph */
				 unsigned int match_end,
				 hb_codepoint_t lig_glyph,
				 unsigned int total_component_count)
{
  TRACE_APPLY (nullptr);

  hb_buffer_t *buffer = c->buffer;

  buffer->merge_clusters (buffer->idx, match_end);

  /* - If a base and one or more marks ligate, consider that as a base, NOT
   *   ligature, such that all following marks can still attach to it.
   *   https://github.com/harfbuzz/harfbuzz/issues/1109
   *
   * - If all components of the ligature were marks, we call this a mark ligature.
   *   If it *is* a mark ligature, we don't allocate a new ligature id, and leave
   *   the ligature to keep its old ligature id.  This will allow it to attach to
   *   a base ligature in GPOS.  Eg. if the sequence is: LAM,LAM,SHADDA,FATHA,HEH,
   *   and LAM,LAM,HEH for a ligature, they will leave SHADDA and FATHA with a
   *   ligature id and component value of 2.  Then if SHADDA,FATHA form a ligature
   *   later, we don't want them to lose their ligature id/component, otherwise
   *   GPOS will fail to correctly position the mark ligature on top of the
   *   LAM,LAM,HEH ligature.  See:
   *     https://bugzilla.gnome.org/show_bug.cgi?id=676343
   *
   * - If a ligature is formed of components that some of which are also ligatures
   *   themselves, and those ligature components had marks attached to *their*
   *   components, we have to attach the marks to the new ligature component
   *   positions!  Now *that*'s tricky!  And these marks may be following the
   *   last component of the whole sequence, so we should loop forward looking
   *   for them and update them.
   *
   *   Eg. the sequence is LAM,LAM,SHADDA,FATHA,HEH, and the font first forms a
   *   'calt' ligature of LAM,HEH, leaving the SHADDA and FATHA with a ligature
   *   id and component == 1.  Now, during 'liga', the LAM and the LAM-HEH ligature
   *   form a LAM-LAM-HEH ligature.  We need to reassign the SHADDA and FATHA to
   *   the new ligature with a component value of 2.
   *
   *   This in fact happened to a font...  See:
   *   https://bugzilla.gnome.org/show_bug.cgi?id=437633
   */

  bool is_base_ligature = _hb_glyph_info_is_base_glyph (&buffer->info[match_positions[0]]);
  bool is_mark_ligature = _hb_glyph_info_is_mark (&buffer->info[match_positions[0]]);
  for (unsigned int i = 1; i < count; i++)
    if (!_hb_glyph_info_is_mark (&buffer->info[match_positions[i]]))
    {
      is_base_ligature = false;
      is_mark_ligature = false;
      break;
    }
  bool is_ligature = !is_base_ligature && !is_mark_ligature;

  unsigned int klass = is_ligature ? HB_OT_LAYOUT_GLYPH_PROPS_LIGATURE : 0;
  unsigned int lig_id = is_ligature ? _hb_allocate_lig_id (buffer) : 0;
  unsigned int last_lig_id = _hb_glyph_info_get_lig_id (&buffer->cur());
  unsigned int last_num_components = _hb_glyph_info_get_lig_num_comps (&buffer->cur());
  unsigned int components_so_far = last_num_components;

  if (is_ligature)
  {
    _hb_glyph_info_set_lig_props_for_ligature (&buffer->cur(), lig_id, total_component_count);
    if (_hb_glyph_info_get_general_category (&buffer->cur()) == HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK)
    {
      _hb_glyph_info_set_general_category (&buffer->cur(), HB_UNICODE_GENERAL_CATEGORY_OTHER_LETTER);
    }
  }
  c->replace_glyph_with_ligature (lig_glyph, klass);

  for (unsigned int i = 1; i < count; i++)
  {
    while (buffer->idx < match_positions[i] && buffer->successful)
    {
      if (is_ligature)
      {
	unsigned int this_comp = _hb_glyph_info_get_lig_comp (&buffer->cur());
	if (this_comp == 0)
	  this_comp = last_num_components;
	unsigned int new_lig_comp = components_so_far - last_num_components +
				    hb_min (this_comp, last_num_components);
	  _hb_glyph_info_set_lig_props_for_mark (&buffer->cur(), lig_id, new_lig_comp);
      }
      (void) buffer->next_glyph ();
    }

    last_lig_id = _hb_glyph_info_get_lig_id (&buffer->cur());
    last_num_components = _hb_glyph_info_get_lig_num_comps (&buffer->cur());
    components_so_far += last_num_components;

    /* Skip the base glyph */
    buffer->idx++;
  }

  if (!is_mark_ligature && last_lig_id)
  {
    /* Re-adjust components for any marks following. */
    for (unsigned i = buffer->idx; i < buffer->len; ++i)
    {
      if (last_lig_id != _hb_glyph_info_get_lig_id (&buffer->info[i])) break;

      unsigned this_comp = _hb_glyph_info_get_lig_comp (&buffer->info[i]);
      if (!this_comp) break;

      unsigned new_lig_comp = components_so_far - last_num_components +
			      hb_min (this_comp, last_num_components);
      _hb_glyph_info_set_lig_props_for_mark (&buffer->info[i], lig_id, new_lig_comp);
    }
  }
  return_trace (true);
}

template <typename HBUINT>
#ifndef HB_OPTIMIZE_SIZE
HB_ALWAYS_INLINE
#endif
static bool match_backtrack (hb_ot_apply_context_t *c,
			     unsigned int count,
			     const HBUINT backtrack[],
			     match_func_t match_func,
			     const void *match_data,
			     unsigned int *match_start)
{
  TRACE_APPLY (nullptr);

  hb_ot_apply_context_t::skipping_iterator_t &skippy_iter = c->iter_context;
  skippy_iter.reset (c->buffer->backtrack_len ());
  skippy_iter.set_match_func (match_func, match_data);
  skippy_iter.set_glyph_data (backtrack);

  for (unsigned int i = 0; i < count; i++)
  {
    unsigned unsafe_from;
    if (!skippy_iter.prev (&unsafe_from))
    {
      *match_start = unsafe_from;
      return_trace (false);
    }
  }

  *match_start = skippy_iter.idx;
  return_trace (true);
}

template <typename HBUINT>
#ifndef HB_OPTIMIZE_SIZE
HB_ALWAYS_INLINE
#endif
static bool match_lookahead (hb_ot_apply_context_t *c,
			     unsigned int count,
			     const HBUINT lookahead[],
			     match_func_t match_func,
			     const void *match_data,
			     unsigned int start_index,
			     unsigned int *end_index)
{
  TRACE_APPLY (nullptr);

  hb_ot_apply_context_t::skipping_iterator_t &skippy_iter = c->iter_context;
  skippy_iter.reset (start_index - 1);
  skippy_iter.set_match_func (match_func, match_data);
  skippy_iter.set_glyph_data (lookahead);

  for (unsigned int i = 0; i < count; i++)
  {
    unsigned unsafe_to;
    if (!skippy_iter.next (&unsafe_to))
    {
      *end_index = unsafe_to;
      return_trace (false);
    }
  }

  *end_index = skippy_iter.idx + 1;
  return_trace (true);
}



struct LookupRecord
{
  bool serialize (hb_serialize_context_t *c,
		  const hb_map_t         *lookup_map) const
  {
    TRACE_SERIALIZE (this);
    auto *out = c->embed (*this);
    if (unlikely (!out)) return_trace (false);

    return_trace (c->check_assign (out->lookupListIndex, lookup_map->get (lookupListIndex), HB_SERIALIZE_ERROR_INT_OVERFLOW));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  HBUINT16	sequenceIndex;		/* Index into current glyph
					 * sequence--first glyph = 0 */
  HBUINT16	lookupListIndex;	/* Lookup to apply to that
					 * position--zero--based */
  public:
  DEFINE_SIZE_STATIC (4);
};

static unsigned serialize_lookuprecord_array (hb_serialize_context_t *c,
					      const hb_array_t<const LookupRecord> lookupRecords,
					      const hb_map_t *lookup_map)
{
  unsigned count = 0;
  for (const LookupRecord& r : lookupRecords)
  {
    if (!lookup_map->has (r.lookupListIndex))
      continue;

    if (!r.serialize (c, lookup_map))
      return 0;

    count++;
  }
  return count;
}

enum ContextFormat { SimpleContext = 1, ClassBasedContext = 2, CoverageBasedContext = 3 };

template <typename HBUINT>
static void context_closure_recurse_lookups (hb_closure_context_t *c,
					     unsigned inputCount, const HBUINT input[],
					     unsigned lookupCount,
					     const LookupRecord lookupRecord[] /* Array of LookupRecords--in design order */,
					     unsigned value,
					     ContextFormat context_format,
					     const void *data,
					     intersected_glyphs_func_t intersected_glyphs_func,
					     void *cache)
{
  hb_set_t covered_seq_indicies;
  hb_set_t pos_glyphs;
  for (unsigned int i = 0; i < lookupCount; i++)
  {
    unsigned seqIndex = lookupRecord[i].sequenceIndex;
    if (seqIndex >= inputCount) continue;

    bool has_pos_glyphs = false;

    if (!covered_seq_indicies.has (seqIndex))
    {
      has_pos_glyphs = true;
      pos_glyphs.clear ();
      if (seqIndex == 0)
      {
        switch (context_format) {
        case ContextFormat::SimpleContext:
          pos_glyphs.add (value);
          break;
        case ContextFormat::ClassBasedContext:
          intersected_glyphs_func (&c->parent_active_glyphs (), data, value, &pos_glyphs, cache);
          break;
        case ContextFormat::CoverageBasedContext:
          pos_glyphs.set (c->parent_active_glyphs ());
          break;
        }
      }
      else
      {
        const void *input_data = input;
        unsigned input_value = seqIndex - 1;
        if (context_format != ContextFormat::SimpleContext)
        {
          input_data = data;
          input_value = input[seqIndex - 1];
        }

        intersected_glyphs_func (c->glyphs, input_data, input_value, &pos_glyphs, cache);
      }
    }

    covered_seq_indicies.add (seqIndex);
    hb_set_t *cur_active_glyphs = c->push_cur_active_glyphs ();
    if (unlikely (!cur_active_glyphs))
      return;
    if (has_pos_glyphs) {
      *cur_active_glyphs = std::move (pos_glyphs);
    } else {
      *cur_active_glyphs = *c->glyphs;
    }

    unsigned endIndex = inputCount;
    if (context_format == ContextFormat::CoverageBasedContext)
      endIndex += 1;

    c->recurse (lookupRecord[i].lookupListIndex, &covered_seq_indicies, seqIndex, endIndex);

    c->pop_cur_done_glyphs ();
  }
}

template <typename context_t>
static inline void recurse_lookups (context_t *c,
                                    unsigned int lookupCount,
                                    const LookupRecord lookupRecord[] /* Array of LookupRecords--in design order */)
{
  for (unsigned int i = 0; i < lookupCount; i++)
    c->recurse (lookupRecord[i].lookupListIndex);
}

static inline void apply_lookup (hb_ot_apply_context_t *c,
				 unsigned int count, /* Including the first glyph */
				 unsigned int *match_positions, /* Including the first glyph */
				 unsigned int lookupCount,
				 const LookupRecord lookupRecord[], /* Array of LookupRecords--in design order */
				 unsigned int match_end)
{
  hb_buffer_t *buffer = c->buffer;
  int end;

  unsigned int *match_positions_input = match_positions;
  unsigned int match_positions_count = count;

  /* All positions are distance from beginning of *output* buffer.
   * Adjust. */
  {
    unsigned int bl = buffer->backtrack_len ();
    end = bl + match_end - buffer->idx;

    int delta = bl - buffer->idx;
    /* Convert positions to new indexing. */
    for (unsigned int j = 0; j < count; j++)
      match_positions[j] += delta;
  }

  for (unsigned int i = 0; i < lookupCount && buffer->successful; i++)
  {
    unsigned int idx = lookupRecord[i].sequenceIndex;
    if (idx >= count)
      continue;

    unsigned int orig_len = buffer->backtrack_len () + buffer->lookahead_len ();

    /* This can happen if earlier recursed lookups deleted many entries. */
    if (unlikely (match_positions[idx] >= orig_len))
      continue;

    if (unlikely (!buffer->move_to (match_positions[idx])))
      break;

    if (unlikely (buffer->max_ops <= 0))
      break;

    if (HB_BUFFER_MESSAGE_MORE && c->buffer->messaging ())
    {
      if (buffer->have_output)
        c->buffer->sync_so_far ();
      c->buffer->message (c->font,
			  "recursing to lookup %u at %u",
			  (unsigned) lookupRecord[i].lookupListIndex,
			  buffer->idx);
    }

    if (!c->recurse (lookupRecord[i].lookupListIndex))
      continue;

    if (HB_BUFFER_MESSAGE_MORE && c->buffer->messaging ())
    {
      if (buffer->have_output)
        c->buffer->sync_so_far ();
      c->buffer->message (c->font,
			  "recursed to lookup %u",
			  (unsigned) lookupRecord[i].lookupListIndex);
    }

    unsigned int new_len = buffer->backtrack_len () + buffer->lookahead_len ();
    int delta = new_len - orig_len;

    if (!delta)
      continue;

    /* Recursed lookup changed buffer len.  Adjust.
     *
     * TODO:
     *
     * Right now, if buffer length increased by n, we assume n new glyphs
     * were added right after the current position, and if buffer length
     * was decreased by n, we assume n match positions after the current
     * one where removed.  The former (buffer length increased) case is
     * fine, but the decrease case can be improved in at least two ways,
     * both of which are significant:
     *
     *   - If recursed-to lookup is MultipleSubst and buffer length
     *     decreased, then it's current match position that was deleted,
     *     NOT the one after it.
     *
     *   - If buffer length was decreased by n, it does not necessarily
     *     mean that n match positions where removed, as there recursed-to
     *     lookup might had a different LookupFlag.  Here's a constructed
     *     case of that:
     *     https://github.com/harfbuzz/harfbuzz/discussions/3538
     *
     * It should be possible to construct tests for both of these cases.
     */

    end += delta;
    if (end < int (match_positions[idx]))
    {
      /* End might end up being smaller than match_positions[idx] if the recursed
       * lookup ended up removing many items.
       * Just never rewind end beyond start of current position, since that is
       * not possible in the recursed lookup.  Also adjust delta as such.
       *
       * https://bugs.chromium.org/p/chromium/issues/detail?id=659496
       * https://github.com/harfbuzz/harfbuzz/issues/1611
       */
      delta += match_positions[idx] - end;
      end = match_positions[idx];
    }

    unsigned int next = idx + 1; /* next now is the position after the recursed lookup. */

    if (delta > 0)
    {
      if (unlikely (delta + count > HB_MAX_CONTEXT_LENGTH))
	break;
      if (unlikely (delta + count > match_positions_count))
      {
        unsigned new_match_positions_count = hb_max (delta + count, hb_max(match_positions_count, 4u) * 1.5);
        if (match_positions == match_positions_input)
	{
	  match_positions = (unsigned int *) hb_malloc (new_match_positions_count * sizeof (match_positions[0]));
	  if (unlikely (!match_positions))
	    break;
	  memcpy (match_positions, match_positions_input, count * sizeof (match_positions[0]));
	  match_positions_count = new_match_positions_count;
	}
	else
	{
	  unsigned int *new_match_positions = (unsigned int *) hb_realloc (match_positions, new_match_positions_count * sizeof (match_positions[0]));
	  if (unlikely (!new_match_positions))
	    break;
	  match_positions = new_match_positions;
	  match_positions_count = new_match_positions_count;
	}
      }

    }
    else
    {
      /* NOTE: delta is non-positive. */
      delta = hb_max (delta, (int) next - (int) count);
      next -= delta;
    }

    /* Shift! */
    memmove (match_positions + next + delta, match_positions + next,
	     (count - next) * sizeof (match_positions[0]));
    next += delta;
    count += delta;

    /* Fill in new entries. */
    for (unsigned int j = idx + 1; j < next; j++)
      match_positions[j] = match_positions[j - 1] + 1;

    /* And fixup the rest. */
    for (; next < count; next++)
      match_positions[next] += delta;
  }

  if (match_positions != match_positions_input)
    hb_free (match_positions);

  (void) buffer->move_to (end);
}



/* Contextual lookups */

struct ContextClosureLookupContext
{
  ContextClosureFuncs funcs;
  ContextFormat context_format;
  const void *intersects_data;
  void *intersects_cache;
  void *intersected_glyphs_cache;
};

struct ContextCollectGlyphsLookupContext
{
  ContextCollectGlyphsFuncs funcs;
  const void *collect_data;
};

struct ContextApplyLookupContext
{
  ContextApplyFuncs funcs;
  const void *match_data;
};

template <typename HBUINT>
static inline bool context_intersects (const hb_set_t *glyphs,
				       unsigned int inputCount, /* Including the first glyph (not matched) */
				       const HBUINT input[], /* Array of input values--start with second glyph */
				       ContextClosureLookupContext &lookup_context)
{
  return array_is_subset_of (glyphs,
			     inputCount ? inputCount - 1 : 0, input,
			     lookup_context.funcs.intersects,
			     lookup_context.intersects_data,
			     lookup_context.intersects_cache);
}

template <typename HBUINT>
static inline void context_closure_lookup (hb_closure_context_t *c,
					   unsigned int inputCount, /* Including the first glyph (not matched) */
					   const HBUINT input[], /* Array of input values--start with second glyph */
					   unsigned int lookupCount,
					   const LookupRecord lookupRecord[],
					   unsigned value, /* Index of first glyph in Coverage or Class value in ClassDef table */
					   ContextClosureLookupContext &lookup_context)
{
  if (context_intersects (c->glyphs,
			  inputCount, input,
			  lookup_context))
    context_closure_recurse_lookups (c,
				     inputCount, input,
				     lookupCount, lookupRecord,
				     value,
				     lookup_context.context_format,
				     lookup_context.intersects_data,
				     lookup_context.funcs.intersected_glyphs,
				     lookup_context.intersected_glyphs_cache);
}

template <typename HBUINT>
static inline void context_collect_glyphs_lookup (hb_collect_glyphs_context_t *c,
						  unsigned int inputCount, /* Including the first glyph (not matched) */
						  const HBUINT input[], /* Array of input values--start with second glyph */
						  unsigned int lookupCount,
						  const LookupRecord lookupRecord[],
						  ContextCollectGlyphsLookupContext &lookup_context)
{
  collect_array (c, c->input,
		 inputCount ? inputCount - 1 : 0, input,
		 lookup_context.funcs.collect, lookup_context.collect_data);
  recurse_lookups (c,
		   lookupCount, lookupRecord);
}

template <typename HBUINT>
static inline bool context_would_apply_lookup (hb_would_apply_context_t *c,
					       unsigned int inputCount, /* Including the first glyph (not matched) */
					       const HBUINT input[], /* Array of input values--start with second glyph */
					       unsigned int lookupCount HB_UNUSED,
					       const LookupRecord lookupRecord[] HB_UNUSED,
					       const ContextApplyLookupContext &lookup_context)
{
  return would_match_input (c,
			    inputCount, input,
			    lookup_context.funcs.match, lookup_context.match_data);
}

template <typename HBUINT>
HB_ALWAYS_INLINE
static bool context_apply_lookup (hb_ot_apply_context_t *c,
				  unsigned int inputCount, /* Including the first glyph (not matched) */
				  const HBUINT input[], /* Array of input values--start with second glyph */
				  unsigned int lookupCount,
				  const LookupRecord lookupRecord[],
				  const ContextApplyLookupContext &lookup_context)
{
  if (unlikely (inputCount > HB_MAX_CONTEXT_LENGTH)) return false;
  unsigned match_positions_stack[4];
  unsigned *match_positions = match_positions_stack;
  if (unlikely (inputCount > ARRAY_LENGTH (match_positions_stack)))
  {
    match_positions = (unsigned *) hb_malloc (hb_max (inputCount, 1u) * sizeof (match_positions[0]));
    if (unlikely (!match_positions))
      return false;
  }

  unsigned match_end = 0;
  bool ret = false;
  if (match_input (c,
		   inputCount, input,
		   lookup_context.funcs.match, lookup_context.match_data,
		   &match_end, match_positions))
  {
    c->buffer->unsafe_to_break (c->buffer->idx, match_end);
    apply_lookup (c,
		  inputCount, match_positions,
		  lookupCount, lookupRecord,
		  match_end);
    ret = true;
  }
  else
  {
    c->buffer->unsafe_to_concat (c->buffer->idx, match_end);
    ret = false;
  }

  if (unlikely (match_positions != match_positions_stack))
    hb_free (match_positions);

  return ret;
}

template <typename Types>
struct Rule
{
  template <typename T>
  friend struct RuleSet;

  bool intersects (const hb_set_t *glyphs, ContextClosureLookupContext &lookup_context) const
  {
    return context_intersects (glyphs,
			       inputCount, inputZ.arrayZ,
			       lookup_context);
  }

  void closure (hb_closure_context_t *c, unsigned value, ContextClosureLookupContext &lookup_context) const
  {
    if (unlikely (c->lookup_limit_exceeded ())) return;

    const auto &lookupRecord = StructAfter<UnsizedArrayOf<LookupRecord>>
					   (inputZ.as_array ((inputCount ? inputCount - 1 : 0)));
    context_closure_lookup (c,
			    inputCount, inputZ.arrayZ,
			    lookupCount, lookupRecord.arrayZ,
			    value, lookup_context);
  }

  void closure_lookups (hb_closure_lookups_context_t *c,
                        ContextClosureLookupContext &lookup_context) const
  {
    if (unlikely (c->lookup_limit_exceeded ())) return;
    if (!intersects (c->glyphs, lookup_context)) return;

    const auto &lookupRecord = StructAfter<UnsizedArrayOf<LookupRecord>>
					   (inputZ.as_array (inputCount ? inputCount - 1 : 0));
    recurse_lookups (c, lookupCount, lookupRecord.arrayZ);
  }

  void collect_glyphs (hb_collect_glyphs_context_t *c,
		       ContextCollectGlyphsLookupContext &lookup_context) const
  {
    const auto &lookupRecord = StructAfter<UnsizedArrayOf<LookupRecord>>
					   (inputZ.as_array (inputCount ? inputCount - 1 : 0));
    context_collect_glyphs_lookup (c,
				   inputCount, inputZ.arrayZ,
				   lookupCount, lookupRecord.arrayZ,
				   lookup_context);
  }

  bool would_apply (hb_would_apply_context_t *c,
		    const ContextApplyLookupContext &lookup_context) const
  {
    const auto &lookupRecord = StructAfter<UnsizedArrayOf<LookupRecord>>
					   (inputZ.as_array (inputCount ? inputCount - 1 : 0));
    return context_would_apply_lookup (c,
				       inputCount, inputZ.arrayZ,
				       lookupCount, lookupRecord.arrayZ,
				       lookup_context);
  }

  bool apply (hb_ot_apply_context_t *c,
	      const ContextApplyLookupContext &lookup_context) const
  {
    TRACE_APPLY (this);
    const auto &lookupRecord = StructAfter<UnsizedArrayOf<LookupRecord>>
					   (inputZ.as_array (inputCount ? inputCount - 1 : 0));
    return_trace (context_apply_lookup (c, inputCount, inputZ.arrayZ, lookupCount, lookupRecord.arrayZ, lookup_context));
  }

  bool serialize (hb_serialize_context_t *c,
		  const hb_map_t *input_mapping, /* old->new glyphid or class mapping */
		  const hb_map_t *lookup_map) const
  {
    TRACE_SERIALIZE (this);
    auto *out = c->start_embed (this);
    if (unlikely (!c->extend_min (out))) return_trace (false);

    out->inputCount = inputCount;
    const auto input = inputZ.as_array (inputCount - 1);
    for (const auto org : input)
    {
      HBUINT16 d;
      d = input_mapping->get (org);
      c->copy (d);
    }

    const auto &lookupRecord = StructAfter<UnsizedArrayOf<LookupRecord>>
					   (inputZ.as_array ((inputCount ? inputCount - 1 : 0)));

    unsigned count = serialize_lookuprecord_array (c, lookupRecord.as_array (lookupCount), lookup_map);
    return_trace (c->check_assign (out->lookupCount, count, HB_SERIALIZE_ERROR_INT_OVERFLOW));
  }

  bool subset (hb_subset_context_t *c,
	       const hb_map_t *lookup_map,
	       const hb_map_t *klass_map = nullptr) const
  {
    TRACE_SUBSET (this);
    if (unlikely (!inputCount)) return_trace (false);
    const auto input = inputZ.as_array (inputCount - 1);

    const hb_map_t *mapping = klass_map == nullptr ? c->plan->glyph_map : klass_map;
    if (!hb_all (input, mapping)) return_trace (false);
    return_trace (serialize (c->serializer, mapping, lookup_map));
  }

  public:
  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  hb_barrier () &&
		  c->check_range (inputZ.arrayZ,
				  inputZ.item_size * (inputCount ? inputCount - 1 : 0) +
				  LookupRecord::static_size * lookupCount));
  }

  protected:
  HBUINT16	inputCount;		/* Total number of glyphs in input
					 * glyph sequence--includes the first
					 * glyph */
  HBUINT16	lookupCount;		/* Number of LookupRecords */
  UnsizedArrayOf<typename Types::HBUINT>
		inputZ;			/* Array of match inputs--start with
					 * second glyph */
/*UnsizedArrayOf<LookupRecord>
		lookupRecordX;*/	/* Array of LookupRecords--in
					 * design order */
  public:
  DEFINE_SIZE_ARRAY (4, inputZ);
};

template <typename Types>
struct RuleSet
{
  using Rule = OT::Rule<Types>;

  bool intersects (const hb_set_t *glyphs,
		   ContextClosureLookupContext &lookup_context) const
  {
    return
    + hb_iter (rule)
    | hb_map (hb_add (this))
    | hb_map ([&] (const Rule &_) { return _.intersects (glyphs, lookup_context); })
    | hb_any
    ;
  }

  void closure (hb_closure_context_t *c, unsigned value,
		ContextClosureLookupContext &lookup_context) const
  {
    if (unlikely (c->lookup_limit_exceeded ())) return;

    return
    + hb_iter (rule)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const Rule &_) { _.closure (c, value, lookup_context); })
    ;
  }

  void closure_lookups (hb_closure_lookups_context_t *c,
                        ContextClosureLookupContext &lookup_context) const
  {
    if (unlikely (c->lookup_limit_exceeded ())) return;
    + hb_iter (rule)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const Rule &_) { _.closure_lookups (c, lookup_context); })
    ;
  }

  void collect_glyphs (hb_collect_glyphs_context_t *c,
		       ContextCollectGlyphsLookupContext &lookup_context) const
  {
    return
    + hb_iter (rule)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const Rule &_) { _.collect_glyphs (c, lookup_context); })
    ;
  }

  bool would_apply (hb_would_apply_context_t *c,
		    const ContextApplyLookupContext &lookup_context) const
  {
    return
    + hb_iter (rule)
    | hb_map (hb_add (this))
    | hb_map ([&] (const Rule &_) { return _.would_apply (c, lookup_context); })
    | hb_any
    ;
  }

  bool apply (hb_ot_apply_context_t *c,
	      const ContextApplyLookupContext &lookup_context) const
  {
    TRACE_APPLY (this);

    unsigned num_rules = rule.len;

#ifndef HB_NO_OT_RULESETS_FAST_PATH
    if (HB_OPTIMIZE_SIZE_VAL || num_rules <= 4)
#endif
    {
    slow:
      return_trace (
      + hb_iter (rule)
      | hb_map (hb_add (this))
      | hb_map ([&] (const Rule &_) { return _.apply (c, lookup_context); })
      | hb_any
      )
      ;
    }

    /* This version is optimized for speed by matching the first & second
     * components of the rule here, instead of calling into the matching code.
     *
     * Replicated from LigatureSet::apply(). */

    hb_ot_apply_context_t::skipping_iterator_t &skippy_iter = c->iter_input;
    skippy_iter.reset (c->buffer->idx);
    skippy_iter.set_match_func (match_always, nullptr);
    skippy_iter.set_glyph_data ((HBUINT16 *) nullptr);
    unsigned unsafe_to = (unsigned) -1, unsafe_to1 = 0, unsafe_to2 = 0;
    hb_glyph_info_t *first = nullptr, *second = nullptr;
    bool matched = skippy_iter.next ();
    if (likely (matched))
    {
      first = &c->buffer->info[skippy_iter.idx];
      unsafe_to = skippy_iter.idx + 1;

      if (skippy_iter.may_skip (c->buffer->info[skippy_iter.idx]))
      {
	/* Can't use the fast path if eg. the next char is a default-ignorable
	 * or other skippable. */
        goto slow;
      }
    }
    else
    {
      /* Failed to match a next glyph. Only try applying rules that have
       * no further input. */
      return_trace (
      + hb_iter (rule)
      | hb_map (hb_add (this))
      | hb_filter ([&] (const Rule &_) { return _.inputCount <= 1; })
      | hb_map ([&] (const Rule &_) { return _.apply (c, lookup_context); })
      | hb_any
      )
      ;
    }
    matched = skippy_iter.next ();
    if (likely (matched && !skippy_iter.may_skip (c->buffer->info[skippy_iter.idx])))
    {
      second = &c->buffer->info[skippy_iter.idx];
      unsafe_to2 = skippy_iter.idx + 1;
    }

    auto match_input = lookup_context.funcs.match;
    auto *input_data = lookup_context.match_data;
    for (unsigned int i = 0; i < num_rules; i++)
    {
      const auto &r = this+rule.arrayZ[i];

      const auto &input = r.inputZ;

      if (r.inputCount <= 1 ||
	  (!match_input ||
	   match_input (*first, input.arrayZ[0], input_data)))
      {
        if (!second ||
	    (r.inputCount <= 2 ||
	     (!match_input ||
	      match_input (*second, input.arrayZ[1], input_data)))
	   )
	{
	  if (r.apply (c, lookup_context))
	  {
	    if (unsafe_to != (unsigned) -1)
	      c->buffer->unsafe_to_concat (c->buffer->idx, unsafe_to);
	    return_trace (true);
	  }
	}
	else
	  unsafe_to = unsafe_to2;
      }
      else
      {
	if (unsafe_to == (unsigned) -1)
	  unsafe_to = unsafe_to1;
      }
    }
    if (likely (unsafe_to != (unsigned) -1))
      c->buffer->unsafe_to_concat (c->buffer->idx, unsafe_to);

    return_trace (false);
  }

  bool subset (hb_subset_context_t *c,
	       const hb_map_t *lookup_map,
	       const hb_map_t *klass_map = nullptr) const
  {
    TRACE_SUBSET (this);

    auto snap = c->serializer->snapshot ();
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);

    for (const Offset16To<Rule>& _ : rule)
    {
      if (!_) continue;
      auto o_snap = c->serializer->snapshot ();
      auto *o = out->rule.serialize_append (c->serializer);
      if (unlikely (!o)) continue;

      if (!o->serialize_subset (c, _, this, lookup_map, klass_map))
      {
	out->rule.pop ();
	c->serializer->revert (o_snap);
      }
    }

    bool ret = bool (out->rule);
    if (!ret) c->serializer->revert (snap);

    return_trace (ret);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (rule.sanitize (c, this));
  }

  protected:
  Array16OfOffset16To<Rule>
		rule;			/* Array of Rule tables
					 * ordered by preference */
  public:
  DEFINE_SIZE_ARRAY (2, rule);
};


template <typename Types>
struct ContextFormat1_4
{
  using RuleSet = OT::RuleSet<Types>;

  bool intersects (const hb_set_t *glyphs) const
  {
    struct ContextClosureLookupContext lookup_context = {
      {intersects_glyph, intersected_glyph},
      ContextFormat::SimpleContext,
      nullptr
    };

    return
    + hb_zip (this+coverage, ruleSet)
    | hb_filter (*glyphs, hb_first)
    | hb_map (hb_second)
    | hb_map (hb_add (this))
    | hb_map ([&] (const RuleSet &_) { return _.intersects (glyphs, lookup_context); })
    | hb_any
    ;
  }

  bool may_have_non_1to1 () const
  { return true; }

  void closure (hb_closure_context_t *c) const
  {
    hb_set_t* cur_active_glyphs = c->push_cur_active_glyphs ();
    if (unlikely (!cur_active_glyphs)) return;
    get_coverage ().intersect_set (c->previous_parent_active_glyphs (), *cur_active_glyphs);

    struct ContextClosureLookupContext lookup_context = {
      {intersects_glyph, intersected_glyph},
      ContextFormat::SimpleContext,
      nullptr
    };

    + hb_zip (this+coverage, hb_range ((unsigned) ruleSet.len))
    | hb_filter ([&] (hb_codepoint_t _) {
      return c->previous_parent_active_glyphs ().has (_);
    }, hb_first)
    | hb_map ([&](const hb_pair_t<hb_codepoint_t, unsigned> _) { return hb_pair_t<unsigned, const RuleSet&> (_.first, this+ruleSet[_.second]); })
    | hb_apply ([&] (const hb_pair_t<unsigned, const RuleSet&>& _) { _.second.closure (c, _.first, lookup_context); })
    ;

    c->pop_cur_done_glyphs ();
  }

  void closure_lookups (hb_closure_lookups_context_t *c) const
  {
    struct ContextClosureLookupContext lookup_context = {
      {intersects_glyph, nullptr},
      ContextFormat::SimpleContext,
      nullptr
    };

    + hb_zip (this+coverage, ruleSet)
    | hb_filter (*c->glyphs, hb_first)
    | hb_map (hb_second)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const RuleSet &_) { _.closure_lookups (c, lookup_context); })
    ;
  }

  void collect_variation_indices (hb_collect_variation_indices_context_t *c) const {}

  void collect_glyphs (hb_collect_glyphs_context_t *c) const
  {
    (this+coverage).collect_coverage (c->input);

    struct ContextCollectGlyphsLookupContext lookup_context = {
      {collect_glyph},
      nullptr
    };

    + hb_iter (ruleSet)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const RuleSet &_) { _.collect_glyphs (c, lookup_context); })
    ;
  }

  bool would_apply (hb_would_apply_context_t *c) const
  {
    const RuleSet &rule_set = this+ruleSet[(this+coverage).get_coverage (c->glyphs[0])];
    struct ContextApplyLookupContext lookup_context = {
      {match_glyph},
      nullptr
    };
    return rule_set.would_apply (c, lookup_context);
  }

  const Coverage &get_coverage () const { return this+coverage; }

  bool apply (hb_ot_apply_context_t *c) const
  {
    TRACE_APPLY (this);
    unsigned int index = (this+coverage).get_coverage (c->buffer->cur().codepoint);
    if (likely (index == NOT_COVERED))
      return_trace (false);

    const RuleSet &rule_set = this+ruleSet[index];
    struct ContextApplyLookupContext lookup_context = {
      {match_glyph},
      nullptr
    };
    return_trace (rule_set.apply (c, lookup_context));
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    const hb_set_t &glyphset = *c->plan->glyphset_gsub ();
    const hb_map_t &glyph_map = *c->plan->glyph_map;

    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);
    out->format = format;

    const hb_map_t *lookup_map = c->table_tag == HB_OT_TAG_GSUB ? &c->plan->gsub_lookups : &c->plan->gpos_lookups;
    hb_sorted_vector_t<hb_codepoint_t> new_coverage;
    + hb_zip (this+coverage, ruleSet)
    | hb_filter (glyphset, hb_first)
    | hb_filter (subset_offset_array (c, out->ruleSet, this, lookup_map), hb_second)
    | hb_map (hb_first)
    | hb_map (glyph_map)
    | hb_sink (new_coverage)
    ;

    out->coverage.serialize_serialize (c->serializer, new_coverage.iter ());
    return_trace (bool (new_coverage));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (coverage.sanitize (c, this) && ruleSet.sanitize (c, this));
  }

  protected:
  HBUINT16	format;			/* Format identifier--format = 1 */
  typename Types::template OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of table */
  Array16Of<typename Types::template OffsetTo<RuleSet>>
		ruleSet;		/* Array of RuleSet tables
					 * ordered by Coverage Index */
  public:
  DEFINE_SIZE_ARRAY (2 + 2 * Types::size, ruleSet);
};


template <typename Types>
struct ContextFormat2_5
{
  using RuleSet = OT::RuleSet<SmallTypes>;

  bool intersects (const hb_set_t *glyphs) const
  {
    if (!(this+coverage).intersects (glyphs))
      return false;

    const ClassDef &class_def = this+classDef;

    hb_map_t cache;
    struct ContextClosureLookupContext lookup_context = {
      {intersects_class, nullptr},
      ContextFormat::ClassBasedContext,
      &class_def,
      &cache
    };

    hb_set_t retained_coverage_glyphs;
    (this+coverage).intersect_set (*glyphs, retained_coverage_glyphs);

    hb_set_t coverage_glyph_classes;
    class_def.intersected_classes (&retained_coverage_glyphs, &coverage_glyph_classes);


    return
    + hb_iter (ruleSet)
    | hb_map (hb_add (this))
    | hb_enumerate
    | hb_map ([&] (const hb_pair_t<unsigned, const RuleSet &> p)
	      { return class_def.intersects_class (glyphs, p.first) &&
		       coverage_glyph_classes.has (p.first) &&
		       p.second.intersects (glyphs, lookup_context); })
    | hb_any
    ;
  }

  bool may_have_non_1to1 () const
  { return true; }

  void closure (hb_closure_context_t *c) const
  {
    if (!(this+coverage).intersects (c->glyphs))
      return;

    hb_set_t* cur_active_glyphs = c->push_cur_active_glyphs ();
    if (unlikely (!cur_active_glyphs)) return;
    get_coverage ().intersect_set (c->previous_parent_active_glyphs (),
				   *cur_active_glyphs);

    const ClassDef &class_def = this+classDef;

    hb_map_t cache;
    intersected_class_cache_t intersected_cache;
    struct ContextClosureLookupContext lookup_context = {
      {intersects_class, intersected_class_glyphs},
      ContextFormat::ClassBasedContext,
      &class_def,
      &cache,
      &intersected_cache
    };

    + hb_enumerate (ruleSet)
    | hb_filter ([&] (unsigned _)
    { return class_def.intersects_class (&c->parent_active_glyphs (), _); },
		 hb_first)
    | hb_apply ([&] (const hb_pair_t<unsigned, const typename Types::template OffsetTo<RuleSet>&> _)
                {
                  const RuleSet& rule_set = this+_.second;
                  rule_set.closure (c, _.first, lookup_context);
                })
    ;

    c->pop_cur_done_glyphs ();
  }

  void closure_lookups (hb_closure_lookups_context_t *c) const
  {
    if (!(this+coverage).intersects (c->glyphs))
      return;

    const ClassDef &class_def = this+classDef;

    hb_map_t cache;
    struct ContextClosureLookupContext lookup_context = {
      {intersects_class, nullptr},
      ContextFormat::ClassBasedContext,
      &class_def,
      &cache
    };

    + hb_iter (ruleSet)
    | hb_map (hb_add (this))
    | hb_enumerate
    | hb_filter ([&] (const hb_pair_t<unsigned, const RuleSet &> p)
    { return class_def.intersects_class (c->glyphs, p.first); })
    | hb_map (hb_second)
    | hb_apply ([&] (const RuleSet & _)
    { _.closure_lookups (c, lookup_context); });
  }

  void collect_variation_indices (hb_collect_variation_indices_context_t *c) const {}

  void collect_glyphs (hb_collect_glyphs_context_t *c) const
  {
    (this+coverage).collect_coverage (c->input);

    const ClassDef &class_def = this+classDef;
    struct ContextCollectGlyphsLookupContext lookup_context = {
      {collect_class},
      &class_def
    };

    + hb_iter (ruleSet)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const RuleSet &_) { _.collect_glyphs (c, lookup_context); })
    ;
  }

  bool would_apply (hb_would_apply_context_t *c) const
  {
    const ClassDef &class_def = this+classDef;
    unsigned int index = class_def.get_class (c->glyphs[0]);
    const RuleSet &rule_set = this+ruleSet[index];
    struct ContextApplyLookupContext lookup_context = {
      {match_class},
      &class_def
    };
    return rule_set.would_apply (c, lookup_context);
  }

  const Coverage &get_coverage () const { return this+coverage; }

  unsigned cache_cost () const
  {
    unsigned c = (this+classDef).cost () * ruleSet.len;
    return c >= 4 ? c : 0;
  }
  bool cache_func (hb_ot_apply_context_t *c, bool enter) const
  {
    if (enter)
    {
      if (!HB_BUFFER_TRY_ALLOCATE_VAR (c->buffer, syllable))
	return false;
      auto &info = c->buffer->info;
      unsigned count = c->buffer->len;
      for (unsigned i = 0; i < count; i++)
	info[i].syllable() = 255;
      c->new_syllables = 255;
      return true;
    }
    else
    {
      c->new_syllables = (unsigned) -1;
      HB_BUFFER_DEALLOCATE_VAR (c->buffer, syllable);
      return true;
    }
  }

  bool apply_cached (hb_ot_apply_context_t *c) const { return _apply (c, true); }
  bool apply (hb_ot_apply_context_t *c) const { return _apply (c, false); }
  bool _apply (hb_ot_apply_context_t *c, bool cached) const
  {
    TRACE_APPLY (this);
    unsigned int index = (this+coverage).get_coverage (c->buffer->cur().codepoint);
    if (likely (index == NOT_COVERED)) return_trace (false);

    const ClassDef &class_def = this+classDef;

    struct ContextApplyLookupContext lookup_context = {
      {cached ? match_class_cached : match_class},
      &class_def
    };

    if (cached && c->buffer->cur().syllable() < 255)
      index = c->buffer->cur().syllable ();
    else
      index = class_def.get_class (c->buffer->cur().codepoint);
    const RuleSet &rule_set = this+ruleSet[index];
    return_trace (rule_set.apply (c, lookup_context));
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);
    out->format = format;
    if (unlikely (!out->coverage.serialize_subset (c, coverage, this)))
      return_trace (false);

    hb_map_t klass_map;
    out->classDef.serialize_subset (c, classDef, this, &klass_map);

    const hb_set_t* glyphset = c->plan->glyphset_gsub ();
    hb_set_t retained_coverage_glyphs;
    (this+coverage).intersect_set (*glyphset, retained_coverage_glyphs);

    hb_set_t coverage_glyph_classes;
    (this+classDef).intersected_classes (&retained_coverage_glyphs, &coverage_glyph_classes);

    const hb_map_t *lookup_map = c->table_tag == HB_OT_TAG_GSUB ? &c->plan->gsub_lookups : &c->plan->gpos_lookups;
    bool ret = true;
    int non_zero_index = -1, index = 0;
    auto snapshot = c->serializer->snapshot();
    for (const auto& _ : + hb_enumerate (ruleSet)
			 | hb_filter (klass_map, hb_first))
    {
      auto *o = out->ruleSet.serialize_append (c->serializer);
      if (unlikely (!o))
      {
	ret = false;
	break;
      }

      if (coverage_glyph_classes.has (_.first) &&
	  o->serialize_subset (c, _.second, this, lookup_map, &klass_map)) {
	non_zero_index = index;
        snapshot = c->serializer->snapshot();
      }

      index++;
    }

    if (!ret || non_zero_index == -1) return_trace (false);

    //prune empty trailing ruleSets
    --index;
    while (index > non_zero_index)
    {
      out->ruleSet.pop ();
      index--;
    }
    c->serializer->revert (snapshot);

    return_trace (bool (out->ruleSet));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (coverage.sanitize (c, this) && classDef.sanitize (c, this) && ruleSet.sanitize (c, this));
  }

  protected:
  HBUINT16	format;			/* Format identifier--format = 2 */
  typename Types::template OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of table */
  typename Types::template OffsetTo<ClassDef>
		classDef;		/* Offset to glyph ClassDef table--from
					 * beginning of table */
  Array16Of<typename Types::template OffsetTo<RuleSet>>
		ruleSet;		/* Array of RuleSet tables
					 * ordered by class */
  public:
  DEFINE_SIZE_ARRAY (4 + 2 * Types::size, ruleSet);
};


struct ContextFormat3
{
  using RuleSet = OT::RuleSet<SmallTypes>;

  bool intersects (const hb_set_t *glyphs) const
  {
    if (!(this+coverageZ[0]).intersects (glyphs))
      return false;

    struct ContextClosureLookupContext lookup_context = {
      {intersects_coverage, nullptr},
      ContextFormat::CoverageBasedContext,
      this
    };
    return context_intersects (glyphs,
			       glyphCount, (const HBUINT16 *) (coverageZ.arrayZ + 1),
			       lookup_context);
  }

  bool may_have_non_1to1 () const
  { return true; }

  void closure (hb_closure_context_t *c) const
  {
    if (!(this+coverageZ[0]).intersects (c->glyphs))
      return;

    hb_set_t* cur_active_glyphs = c->push_cur_active_glyphs ();
    if (unlikely (!cur_active_glyphs)) return;
    get_coverage ().intersect_set (c->previous_parent_active_glyphs (),
				   *cur_active_glyphs);

    const LookupRecord *lookupRecord = &StructAfter<LookupRecord> (coverageZ.as_array (glyphCount));
    struct ContextClosureLookupContext lookup_context = {
      {intersects_coverage, intersected_coverage_glyphs},
      ContextFormat::CoverageBasedContext,
      this
    };
    context_closure_lookup (c,
			    glyphCount, (const HBUINT16 *) (coverageZ.arrayZ + 1),
			    lookupCount, lookupRecord,
			    0, lookup_context);

    c->pop_cur_done_glyphs ();
  }

  void closure_lookups (hb_closure_lookups_context_t *c) const
  {
    if (!intersects (c->glyphs))
      return;
    const LookupRecord *lookupRecord = &StructAfter<LookupRecord> (coverageZ.as_array (glyphCount));
    recurse_lookups (c, lookupCount, lookupRecord);
  }

  void collect_variation_indices (hb_collect_variation_indices_context_t *c) const {}

  void collect_glyphs (hb_collect_glyphs_context_t *c) const
  {
    (this+coverageZ[0]).collect_coverage (c->input);

    const LookupRecord *lookupRecord = &StructAfter<LookupRecord> (coverageZ.as_array (glyphCount));
    struct ContextCollectGlyphsLookupContext lookup_context = {
      {collect_coverage},
      this
    };

    context_collect_glyphs_lookup (c,
				   glyphCount, (const HBUINT16 *) (coverageZ.arrayZ + 1),
				   lookupCount, lookupRecord,
				   lookup_context);
  }

  bool would_apply (hb_would_apply_context_t *c) const
  {
    const LookupRecord *lookupRecord = &StructAfter<LookupRecord> (coverageZ.as_array (glyphCount));
    struct ContextApplyLookupContext lookup_context = {
      {match_coverage},
      this
    };
    return context_would_apply_lookup (c,
				       glyphCount, (const HBUINT16 *) (coverageZ.arrayZ + 1),
				       lookupCount, lookupRecord,
				       lookup_context);
  }

  const Coverage &get_coverage () const { return this+coverageZ[0]; }

  bool apply (hb_ot_apply_context_t *c) const
  {
    TRACE_APPLY (this);
    unsigned int index = (this+coverageZ[0]).get_coverage (c->buffer->cur().codepoint);
    if (likely (index == NOT_COVERED)) return_trace (false);

    const LookupRecord *lookupRecord = &StructAfter<LookupRecord> (coverageZ.as_array (glyphCount));
    struct ContextApplyLookupContext lookup_context = {
      {match_coverage},
      this
    };
    return_trace (context_apply_lookup (c, glyphCount, (const HBUINT16 *) (coverageZ.arrayZ + 1), lookupCount, lookupRecord, lookup_context));
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);

    out->format = format;
    out->glyphCount = glyphCount;

    auto coverages = coverageZ.as_array (glyphCount);

    for (const Offset16To<Coverage>& offset : coverages)
    {
      /* TODO(subset) This looks like should not be necessary to write this way. */
      auto *o = c->serializer->allocate_size<Offset16To<Coverage>> (Offset16To<Coverage>::static_size);
      if (unlikely (!o)) return_trace (false);
      if (!o->serialize_subset (c, offset, this)) return_trace (false);
    }

    const auto& lookupRecord = StructAfter<UnsizedArrayOf<LookupRecord>> (coverageZ.as_array (glyphCount));
    const hb_map_t *lookup_map = c->table_tag == HB_OT_TAG_GSUB ? &c->plan->gsub_lookups : &c->plan->gpos_lookups;


    unsigned count = serialize_lookuprecord_array (c->serializer, lookupRecord.as_array (lookupCount), lookup_map);
    return_trace (c->serializer->check_assign (out->lookupCount, count, HB_SERIALIZE_ERROR_INT_OVERFLOW));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!c->check_struct (this))) return_trace (false);
    hb_barrier ();
    unsigned int count = glyphCount;
    if (unlikely (!count)) return_trace (false); /* We want to access coverageZ[0] freely. */
    if (unlikely (!c->check_array (coverageZ.arrayZ, count))) return_trace (false);
    for (unsigned int i = 0; i < count; i++)
      if (unlikely (!coverageZ[i].sanitize (c, this))) return_trace (false);
    const LookupRecord *lookupRecord = &StructAfter<LookupRecord> (coverageZ.as_array (glyphCount));
    return_trace (likely (c->check_array (lookupRecord, lookupCount)));
  }

  protected:
  HBUINT16	format;			/* Format identifier--format = 3 */
  HBUINT16	glyphCount;		/* Number of glyphs in the input glyph
					 * sequence */
  HBUINT16	lookupCount;		/* Number of LookupRecords */
  UnsizedArrayOf<Offset16To<Coverage>>
		coverageZ;		/* Array of offsets to Coverage
					 * table in glyph sequence order */
/*UnsizedArrayOf<LookupRecord>
		lookupRecordX;*/	/* Array of LookupRecords--in
					 * design order */
  public:
  DEFINE_SIZE_ARRAY (6, coverageZ);
};

struct Context
{
  template <typename context_t, typename ...Ts>
  typename context_t::return_t dispatch (context_t *c, Ts&&... ds) const
  {
    if (unlikely (!c->may_dispatch (this, &u.format))) return c->no_dispatch_return_value ();
    TRACE_DISPATCH (this, u.format);
    switch (u.format) {
    case 1: hb_barrier (); return_trace (c->dispatch (u.format1, std::forward<Ts> (ds)...));
    case 2: hb_barrier (); return_trace (c->dispatch (u.format2, std::forward<Ts> (ds)...));
    case 3: hb_barrier (); return_trace (c->dispatch (u.format3, std::forward<Ts> (ds)...));
#ifndef HB_NO_BEYOND_64K
    case 4: hb_barrier (); return_trace (c->dispatch (u.format4, std::forward<Ts> (ds)...));
    case 5: hb_barrier (); return_trace (c->dispatch (u.format5, std::forward<Ts> (ds)...));
#endif
    default:return_trace (c->default_return_value ());
    }
  }

  protected:
  union {
  HBUINT16			format;		/* Format identifier */
  ContextFormat1_4<SmallTypes>	format1;
  ContextFormat2_5<SmallTypes>	format2;
  ContextFormat3		format3;
#ifndef HB_NO_BEYOND_64K
  ContextFormat1_4<MediumTypes>	format4;
  ContextFormat2_5<MediumTypes>	format5;
#endif
  } u;
};


/* Chaining Contextual lookups */

struct ChainContextClosureLookupContext
{
  ContextClosureFuncs funcs;
  ContextFormat context_format;
  const void *intersects_data[3];
  void *intersects_cache[3];
  void *intersected_glyphs_cache;
};

struct ChainContextCollectGlyphsLookupContext
{
  ContextCollectGlyphsFuncs funcs;
  const void *collect_data[3];
};

struct ChainContextApplyLookupContext
{
  ChainContextApplyFuncs funcs;
  const void *match_data[3];
};

template <typename HBUINT>
static inline bool chain_context_intersects (const hb_set_t *glyphs,
					     unsigned int backtrackCount,
					     const HBUINT backtrack[],
					     unsigned int inputCount, /* Including the first glyph (not matched) */
					     const HBUINT input[], /* Array of input values--start with second glyph */
					     unsigned int lookaheadCount,
					     const HBUINT lookahead[],
					     ChainContextClosureLookupContext &lookup_context)
{
  return array_is_subset_of (glyphs,
			     backtrackCount, backtrack,
			     lookup_context.funcs.intersects,
			     lookup_context.intersects_data[0],
			     lookup_context.intersects_cache[0])
      && array_is_subset_of (glyphs,
			     inputCount ? inputCount - 1 : 0, input,
			     lookup_context.funcs.intersects,
			     lookup_context.intersects_data[1],
			     lookup_context.intersects_cache[1])
      && array_is_subset_of (glyphs,
			     lookaheadCount, lookahead,
			     lookup_context.funcs.intersects,
			     lookup_context.intersects_data[2],
			     lookup_context.intersects_cache[2]);
}

template <typename HBUINT>
static inline void chain_context_closure_lookup (hb_closure_context_t *c,
						 unsigned int backtrackCount,
						 const HBUINT backtrack[],
						 unsigned int inputCount, /* Including the first glyph (not matched) */
						 const HBUINT input[], /* Array of input values--start with second glyph */
						 unsigned int lookaheadCount,
						 const HBUINT lookahead[],
						 unsigned int lookupCount,
						 const LookupRecord lookupRecord[],
						 unsigned value,
						 ChainContextClosureLookupContext &lookup_context)
{
  if (chain_context_intersects (c->glyphs,
				backtrackCount, backtrack,
				inputCount, input,
				lookaheadCount, lookahead,
				lookup_context))
    context_closure_recurse_lookups (c,
		     inputCount, input,
		     lookupCount, lookupRecord,
		     value,
		     lookup_context.context_format,
		     lookup_context.intersects_data[1],
		     lookup_context.funcs.intersected_glyphs,
		     lookup_context.intersected_glyphs_cache);
}

template <typename HBUINT>
static inline void chain_context_collect_glyphs_lookup (hb_collect_glyphs_context_t *c,
							unsigned int backtrackCount,
							const HBUINT backtrack[],
							unsigned int inputCount, /* Including the first glyph (not matched) */
							const HBUINT input[], /* Array of input values--start with second glyph */
							unsigned int lookaheadCount,
							const HBUINT lookahead[],
							unsigned int lookupCount,
							const LookupRecord lookupRecord[],
							ChainContextCollectGlyphsLookupContext &lookup_context)
{
  collect_array (c, c->before,
		 backtrackCount, backtrack,
		 lookup_context.funcs.collect, lookup_context.collect_data[0]);
  collect_array (c, c->input,
		 inputCount ? inputCount - 1 : 0, input,
		 lookup_context.funcs.collect, lookup_context.collect_data[1]);
  collect_array (c, c->after,
		 lookaheadCount, lookahead,
		 lookup_context.funcs.collect, lookup_context.collect_data[2]);
  recurse_lookups (c,
		   lookupCount, lookupRecord);
}

template <typename HBUINT>
static inline bool chain_context_would_apply_lookup (hb_would_apply_context_t *c,
						     unsigned int backtrackCount,
						     const HBUINT backtrack[] HB_UNUSED,
						     unsigned int inputCount, /* Including the first glyph (not matched) */
						     const HBUINT input[], /* Array of input values--start with second glyph */
						     unsigned int lookaheadCount,
						     const HBUINT lookahead[] HB_UNUSED,
						     unsigned int lookupCount HB_UNUSED,
						     const LookupRecord lookupRecord[] HB_UNUSED,
						     const ChainContextApplyLookupContext &lookup_context)
{
  return (c->zero_context ? !backtrackCount && !lookaheadCount : true)
      && would_match_input (c,
			    inputCount, input,
			    lookup_context.funcs.match[1], lookup_context.match_data[1]);
}

template <typename HBUINT>
HB_ALWAYS_INLINE
static bool chain_context_apply_lookup (hb_ot_apply_context_t *c,
					unsigned int backtrackCount,
					const HBUINT backtrack[],
					unsigned int inputCount, /* Including the first glyph (not matched) */
					const HBUINT input[], /* Array of input values--start with second glyph */
					unsigned int lookaheadCount,
					const HBUINT lookahead[],
					unsigned int lookupCount,
					const LookupRecord lookupRecord[],
					const ChainContextApplyLookupContext &lookup_context)
{
  if (unlikely (inputCount > HB_MAX_CONTEXT_LENGTH)) return false;
  unsigned match_positions_stack[4];
  unsigned *match_positions = match_positions_stack;
  if (unlikely (inputCount > ARRAY_LENGTH (match_positions_stack)))
  {
    match_positions = (unsigned *) hb_malloc (hb_max (inputCount, 1u) * sizeof (match_positions[0]));
    if (unlikely (!match_positions))
      return false;
  }

  unsigned start_index = c->buffer->out_len;
  unsigned end_index = c->buffer->idx;
  unsigned match_end = 0;
  bool ret = true;
  if (!(match_input (c,
		     inputCount, input,
		     lookup_context.funcs.match[1], lookup_context.match_data[1],
		     &match_end, match_positions) && (end_index = match_end)
       && match_lookahead (c,
			   lookaheadCount, lookahead,
			   lookup_context.funcs.match[2], lookup_context.match_data[2],
			   match_end, &end_index)))
  {
    c->buffer->unsafe_to_concat (c->buffer->idx, end_index);
    ret = false;
    goto done;
  }

  if (!match_backtrack (c,
			backtrackCount, backtrack,
			lookup_context.funcs.match[0], lookup_context.match_data[0],
			&start_index))
  {
    c->buffer->unsafe_to_concat_from_outbuffer (start_index, end_index);
    ret = false;
    goto done;
  }

  c->buffer->unsafe_to_break_from_outbuffer (start_index, end_index);
  apply_lookup (c,
		inputCount, match_positions,
		lookupCount, lookupRecord,
		match_end);
  done:

  if (unlikely (match_positions != match_positions_stack))
    hb_free (match_positions);

  return ret;
}

template <typename Types>
struct ChainRule
{
  template <typename T>
  friend struct ChainRuleSet;

  bool intersects (const hb_set_t *glyphs, ChainContextClosureLookupContext &lookup_context) const
  {
    const auto &input = StructAfter<decltype (inputX)> (backtrack);
    const auto &lookahead = StructAfter<decltype (lookaheadX)> (input);
    return chain_context_intersects (glyphs,
				     backtrack.len, backtrack.arrayZ,
				     input.lenP1, input.arrayZ,
				     lookahead.len, lookahead.arrayZ,
				     lookup_context);
  }

  void closure (hb_closure_context_t *c, unsigned value,
		ChainContextClosureLookupContext &lookup_context) const
  {
    if (unlikely (c->lookup_limit_exceeded ())) return;

    const auto &input = StructAfter<decltype (inputX)> (backtrack);
    const auto &lookahead = StructAfter<decltype (lookaheadX)> (input);
    const auto &lookup = StructAfter<decltype (lookupX)> (lookahead);
    chain_context_closure_lookup (c,
				  backtrack.len, backtrack.arrayZ,
				  input.lenP1, input.arrayZ,
				  lookahead.len, lookahead.arrayZ,
				  lookup.len, lookup.arrayZ,
				  value,
				  lookup_context);
  }

  void closure_lookups (hb_closure_lookups_context_t *c,
                        ChainContextClosureLookupContext &lookup_context) const
  {
    if (unlikely (c->lookup_limit_exceeded ())) return;
    if (!intersects (c->glyphs, lookup_context)) return;

    const auto &input = StructAfter<decltype (inputX)> (backtrack);
    const auto &lookahead = StructAfter<decltype (lookaheadX)> (input);
    const auto &lookup = StructAfter<decltype (lookupX)> (lookahead);
    recurse_lookups (c, lookup.len, lookup.arrayZ);
  }

  void collect_glyphs (hb_collect_glyphs_context_t *c,
		       ChainContextCollectGlyphsLookupContext &lookup_context) const
  {
    const auto &input = StructAfter<decltype (inputX)> (backtrack);
    const auto &lookahead = StructAfter<decltype (lookaheadX)> (input);
    const auto &lookup = StructAfter<decltype (lookupX)> (lookahead);
    chain_context_collect_glyphs_lookup (c,
					 backtrack.len, backtrack.arrayZ,
					 input.lenP1, input.arrayZ,
					 lookahead.len, lookahead.arrayZ,
					 lookup.len, lookup.arrayZ,
					 lookup_context);
  }

  bool would_apply (hb_would_apply_context_t *c,
		    const ChainContextApplyLookupContext &lookup_context) const
  {
    const auto &input = StructAfter<decltype (inputX)> (backtrack);
    const auto &lookahead = StructAfter<decltype (lookaheadX)> (input);
    const auto &lookup = StructAfter<decltype (lookupX)> (lookahead);
    return chain_context_would_apply_lookup (c,
					     backtrack.len, backtrack.arrayZ,
					     input.lenP1, input.arrayZ,
					     lookahead.len, lookahead.arrayZ, lookup.len,
					     lookup.arrayZ, lookup_context);
  }

  bool apply (hb_ot_apply_context_t *c,
	      const ChainContextApplyLookupContext &lookup_context) const
  {
    TRACE_APPLY (this);
    const auto &input = StructAfter<decltype (inputX)> (backtrack);
    const auto &lookahead = StructAfter<decltype (lookaheadX)> (input);
    const auto &lookup = StructAfter<decltype (lookupX)> (lookahead);
    return_trace (chain_context_apply_lookup (c,
					      backtrack.len, backtrack.arrayZ,
					      input.lenP1, input.arrayZ,
					      lookahead.len, lookahead.arrayZ, lookup.len,
					      lookup.arrayZ, lookup_context));
  }

  template<typename Iterator,
	   hb_requires (hb_is_iterator (Iterator))>
  void serialize_array (hb_serialize_context_t *c,
			HBUINT16 len,
			Iterator it) const
  {
    c->copy (len);
    for (const auto g : it)
      c->copy ((HBUINT16) g);
  }

  bool serialize (hb_serialize_context_t *c,
		  const hb_map_t *lookup_map,
		  const hb_map_t *backtrack_map,
		  const hb_map_t *input_map = nullptr,
		  const hb_map_t *lookahead_map = nullptr) const
  {
    TRACE_SERIALIZE (this);

    const hb_map_t *mapping = backtrack_map;
    serialize_array (c, backtrack.len, + backtrack.iter ()
				       | hb_map (mapping));

    const auto &input = StructAfter<decltype (inputX)> (backtrack);
    if (input_map) mapping = input_map;
    serialize_array (c, input.lenP1, + input.iter ()
				     | hb_map (mapping));

    const auto &lookahead = StructAfter<decltype (lookaheadX)> (input);
    if (lookahead_map) mapping = lookahead_map;
    serialize_array (c, lookahead.len, + lookahead.iter ()
				       | hb_map (mapping));

    const auto &lookup = StructAfter<decltype (lookupX)> (lookahead);

    HBUINT16* lookupCount = c->embed (&(lookup.len));
    if (!lookupCount) return_trace (false);

    unsigned count = serialize_lookuprecord_array (c, lookup.as_array (), lookup_map);
    return_trace (c->check_assign (*lookupCount, count, HB_SERIALIZE_ERROR_INT_OVERFLOW));
  }

  bool subset (hb_subset_context_t *c,
	       const hb_map_t *lookup_map,
	       const hb_map_t *backtrack_map = nullptr,
	       const hb_map_t *input_map = nullptr,
	       const hb_map_t *lookahead_map = nullptr) const
  {
    TRACE_SUBSET (this);

    const auto &input = StructAfter<decltype (inputX)> (backtrack);
    const auto &lookahead = StructAfter<decltype (lookaheadX)> (input);

    if (!backtrack_map)
    {
      const hb_set_t &glyphset = *c->plan->glyphset_gsub ();
      if (!hb_all (backtrack, glyphset) ||
	  !hb_all (input, glyphset) ||
	  !hb_all (lookahead, glyphset))
	return_trace (false);

      serialize (c->serializer, lookup_map, c->plan->glyph_map);
    }
    else
    {
      if (!hb_all (backtrack, backtrack_map) ||
	  !hb_all (input, input_map) ||
	  !hb_all (lookahead, lookahead_map))
	return_trace (false);

      serialize (c->serializer, lookup_map, backtrack_map, input_map, lookahead_map);
    }

    return_trace (true);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    /* Hyper-optimized sanitized because this is really hot. */
    if (unlikely (!backtrack.len.sanitize (c))) return_trace (false);
    hb_barrier ();
    const auto &input = StructAfter<decltype (inputX)> (backtrack);
    if (unlikely (!input.lenP1.sanitize (c))) return_trace (false);
    hb_barrier ();
    const auto &lookahead = StructAfter<decltype (lookaheadX)> (input);
    if (unlikely (!lookahead.len.sanitize (c))) return_trace (false);
    hb_barrier ();
    const auto &lookup = StructAfter<decltype (lookupX)> (lookahead);
    return_trace (likely (lookup.sanitize (c)));
  }

  protected:
  Array16Of<typename Types::HBUINT>
		backtrack;		/* Array of backtracking values
					 * (to be matched before the input
					 * sequence) */
  HeadlessArray16Of<typename Types::HBUINT>
		inputX;			/* Array of input values (start with
					 * second glyph) */
  Array16Of<typename Types::HBUINT>
		lookaheadX;		/* Array of lookahead values's (to be
					 * matched after the input sequence) */
  Array16Of<LookupRecord>
		lookupX;		/* Array of LookupRecords--in
					 * design order) */
  public:
  DEFINE_SIZE_MIN (8);
};

template <typename Types>
struct ChainRuleSet
{
  using ChainRule = OT::ChainRule<Types>;

  bool intersects (const hb_set_t *glyphs, ChainContextClosureLookupContext &lookup_context) const
  {
    return
    + hb_iter (rule)
    | hb_map (hb_add (this))
    | hb_map ([&] (const ChainRule &_) { return _.intersects (glyphs, lookup_context); })
    | hb_any
    ;
  }
  void closure (hb_closure_context_t *c, unsigned value, ChainContextClosureLookupContext &lookup_context) const
  {
    if (unlikely (c->lookup_limit_exceeded ())) return;

    return
    + hb_iter (rule)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const ChainRule &_) { _.closure (c, value, lookup_context); })
    ;
  }

  void closure_lookups (hb_closure_lookups_context_t *c,
                        ChainContextClosureLookupContext &lookup_context) const
  {
    if (unlikely (c->lookup_limit_exceeded ())) return;

    + hb_iter (rule)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const ChainRule &_) { _.closure_lookups (c, lookup_context); })
    ;
  }

  void collect_glyphs (hb_collect_glyphs_context_t *c, ChainContextCollectGlyphsLookupContext &lookup_context) const
  {
    return
    + hb_iter (rule)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const ChainRule &_) { _.collect_glyphs (c, lookup_context); })
    ;
  }

  bool would_apply (hb_would_apply_context_t *c,
		    const ChainContextApplyLookupContext &lookup_context) const
  {
    return
    + hb_iter (rule)
    | hb_map (hb_add (this))
    | hb_map ([&] (const ChainRule &_) { return _.would_apply (c, lookup_context); })
    | hb_any
    ;
  }

  bool apply (hb_ot_apply_context_t *c,
	      const ChainContextApplyLookupContext &lookup_context) const
  {
    TRACE_APPLY (this);

    unsigned num_rules = rule.len;

#ifndef HB_NO_OT_RULESETS_FAST_PATH
    if (HB_OPTIMIZE_SIZE_VAL || num_rules <= 4)
#endif
    {
    slow:
      return_trace (
      + hb_iter (rule)
      | hb_map (hb_add (this))
      | hb_map ([&] (const ChainRule &_) { return _.apply (c, lookup_context); })
      | hb_any
      )
      ;
    }

    /* This version is optimized for speed by matching the first & second
     * components of the rule here, instead of calling into the matching code.
     *
     * Replicated from LigatureSet::apply(). */

    /* If the input skippy has non-auto joiners behavior (as in Indic shapers),
     * skip this fast path, as we don't distinguish between input & lookahead
     * matching in the fast path.
     *
     * https://github.com/harfbuzz/harfbuzz/issues/4813
     */
    if (!c->auto_zwnj || !c->auto_zwj)
      goto slow;

    hb_ot_apply_context_t::skipping_iterator_t &skippy_iter = c->iter_input;
    skippy_iter.reset (c->buffer->idx);
    skippy_iter.set_match_func (match_always, nullptr);
    skippy_iter.set_glyph_data ((HBUINT16 *) nullptr);
    unsigned unsafe_to = (unsigned) -1, unsafe_to1 = 0, unsafe_to2 = 0;
    hb_glyph_info_t *first = nullptr, *second = nullptr;
    bool matched = skippy_iter.next ();
    if (likely (matched))
    {
      first = &c->buffer->info[skippy_iter.idx];
      unsafe_to1 = skippy_iter.idx + 1;

      if (skippy_iter.may_skip (c->buffer->info[skippy_iter.idx]))
      {
	/* Can't use the fast path if eg. the next char is a default-ignorable
	 * or other skippable. */
        goto slow;
      }
    }
    else
    {
      /* Failed to match a next glyph. Only try applying rules that have
       * no further input and lookahead. */
      return_trace (
      + hb_iter (rule)
      | hb_map (hb_add (this))
      | hb_filter ([&] (const ChainRule &_)
		   {
		     const auto &input = StructAfter<decltype (_.inputX)> (_.backtrack);
		     const auto &lookahead = StructAfter<decltype (_.lookaheadX)> (input);
		     return input.lenP1 <= 1 && lookahead.len == 0;
		   })
      | hb_map ([&] (const ChainRule &_) { return _.apply (c, lookup_context); })
      | hb_any
      )
      ;
    }
    matched = skippy_iter.next ();
    if (likely (matched && !skippy_iter.may_skip (c->buffer->info[skippy_iter.idx])))
    {
      second = &c->buffer->info[skippy_iter.idx];
      unsafe_to2 = skippy_iter.idx + 1;
    }

    auto match_input = lookup_context.funcs.match[1];
    auto match_lookahead = lookup_context.funcs.match[2];
    auto *input_data = lookup_context.match_data[1];
    auto *lookahead_data = lookup_context.match_data[2];
    for (unsigned int i = 0; i < num_rules; i++)
    {
      const auto &r = this+rule.arrayZ[i];

      const auto &input = StructAfter<decltype (r.inputX)> (r.backtrack);
      const auto &lookahead = StructAfter<decltype (r.lookaheadX)> (input);

      unsigned lenP1 = hb_max ((unsigned) input.lenP1, 1u);
      if (lenP1 > 1 ?
	   (!match_input ||
	    match_input (*first, input.arrayZ[0], input_data))
	  :
	   (!lookahead.len || !match_lookahead ||
	    match_lookahead (*first, lookahead.arrayZ[0], lookahead_data)))
      {
        if (!second ||
	    (lenP1 > 2 ?
	     (!match_input ||
	      match_input (*second, input.arrayZ[1], input_data))
	     :
	     (lookahead.len <= 2 - lenP1 || !match_lookahead ||
	      match_lookahead (*second, lookahead.arrayZ[2 - lenP1], lookahead_data))))
	{
	  if (r.apply (c, lookup_context))
	  {
	    if (unsafe_to != (unsigned) -1)
	      c->buffer->unsafe_to_concat (c->buffer->idx, unsafe_to);
	    return_trace (true);
	  }
	}
	else
	  unsafe_to = unsafe_to2;
      }
      else
      {
	if (unsafe_to == (unsigned) -1)
	  unsafe_to = unsafe_to1;
      }
    }
    if (likely (unsafe_to != (unsigned) -1))
      c->buffer->unsafe_to_concat (c->buffer->idx, unsafe_to);

    return_trace (false);
  }

  bool subset (hb_subset_context_t *c,
	       const hb_map_t *lookup_map,
	       const hb_map_t *backtrack_klass_map = nullptr,
	       const hb_map_t *input_klass_map = nullptr,
	       const hb_map_t *lookahead_klass_map = nullptr) const
  {
    TRACE_SUBSET (this);

    auto snap = c->serializer->snapshot ();
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);

    for (const Offset16To<ChainRule>& _ : rule)
    {
      if (!_) continue;
      auto o_snap = c->serializer->snapshot ();
      auto *o = out->rule.serialize_append (c->serializer);
      if (unlikely (!o)) continue;

      if (!o->serialize_subset (c, _, this,
				lookup_map,
				backtrack_klass_map,
				input_klass_map,
				lookahead_klass_map))
      {
	out->rule.pop ();
	c->serializer->revert (o_snap);
      }
    }

    bool ret = bool (out->rule);
    if (!ret) c->serializer->revert (snap);

    return_trace (ret);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (rule.sanitize (c, this));
  }

  protected:
  Array16OfOffset16To<ChainRule>
		rule;			/* Array of ChainRule tables
					 * ordered by preference */
  public:
  DEFINE_SIZE_ARRAY (2, rule);
};

template <typename Types>
struct ChainContextFormat1_4
{
  using ChainRuleSet = OT::ChainRuleSet<Types>;

  bool intersects (const hb_set_t *glyphs) const
  {
    struct ChainContextClosureLookupContext lookup_context = {
      {intersects_glyph, intersected_glyph},
      ContextFormat::SimpleContext,
      {nullptr, nullptr, nullptr}
    };

    return
    + hb_zip (this+coverage, ruleSet)
    | hb_filter (*glyphs, hb_first)
    | hb_map (hb_second)
    | hb_map (hb_add (this))
    | hb_map ([&] (const ChainRuleSet &_) { return _.intersects (glyphs, lookup_context); })
    | hb_any
    ;
  }

  bool may_have_non_1to1 () const
  { return true; }

  void closure (hb_closure_context_t *c) const
  {
    hb_set_t* cur_active_glyphs = c->push_cur_active_glyphs ();
    if (unlikely (!cur_active_glyphs)) return;
    get_coverage ().intersect_set (c->previous_parent_active_glyphs (),
				   *cur_active_glyphs);

    struct ChainContextClosureLookupContext lookup_context = {
      {intersects_glyph, intersected_glyph},
      ContextFormat::SimpleContext,
      {nullptr, nullptr, nullptr}
    };

    + hb_zip (this+coverage, hb_range ((unsigned) ruleSet.len))
    | hb_filter ([&] (hb_codepoint_t _) {
      return c->previous_parent_active_glyphs ().has (_);
    }, hb_first)
    | hb_map ([&](const hb_pair_t<hb_codepoint_t, unsigned> _) { return hb_pair_t<unsigned, const ChainRuleSet&> (_.first, this+ruleSet[_.second]); })
    | hb_apply ([&] (const hb_pair_t<unsigned, const ChainRuleSet&>& _) { _.second.closure (c, _.first, lookup_context); })
    ;

    c->pop_cur_done_glyphs ();
  }

  void closure_lookups (hb_closure_lookups_context_t *c) const
  {
    struct ChainContextClosureLookupContext lookup_context = {
      {intersects_glyph, nullptr},
      ContextFormat::SimpleContext,
      {nullptr, nullptr, nullptr}
    };

    + hb_zip (this+coverage, ruleSet)
    | hb_filter (*c->glyphs, hb_first)
    | hb_map (hb_second)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const ChainRuleSet &_) { _.closure_lookups (c, lookup_context); })
    ;
  }

  void collect_variation_indices (hb_collect_variation_indices_context_t *c) const {}

  void collect_glyphs (hb_collect_glyphs_context_t *c) const
  {
    (this+coverage).collect_coverage (c->input);

    struct ChainContextCollectGlyphsLookupContext lookup_context = {
      {collect_glyph},
      {nullptr, nullptr, nullptr}
    };

    + hb_iter (ruleSet)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const ChainRuleSet &_) { _.collect_glyphs (c, lookup_context); })
    ;
  }

  bool would_apply (hb_would_apply_context_t *c) const
  {
    const ChainRuleSet &rule_set = this+ruleSet[(this+coverage).get_coverage (c->glyphs[0])];
    struct ChainContextApplyLookupContext lookup_context = {
      {{match_glyph, match_glyph, match_glyph}},
      {nullptr, nullptr, nullptr}
    };
    return rule_set.would_apply (c, lookup_context);
  }

  const Coverage &get_coverage () const { return this+coverage; }

  bool apply (hb_ot_apply_context_t *c) const
  {
    TRACE_APPLY (this);
    unsigned int index = (this+coverage).get_coverage (c->buffer->cur().codepoint);
    if (likely (index == NOT_COVERED)) return_trace (false);

    const ChainRuleSet &rule_set = this+ruleSet[index];
    struct ChainContextApplyLookupContext lookup_context = {
      {{match_glyph, match_glyph, match_glyph}},
      {nullptr, nullptr, nullptr}
    };
    return_trace (rule_set.apply (c, lookup_context));
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    const hb_set_t &glyphset = *c->plan->glyphset_gsub ();
    const hb_map_t &glyph_map = *c->plan->glyph_map;

    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);
    out->format = format;

    const hb_map_t *lookup_map = c->table_tag == HB_OT_TAG_GSUB ? &c->plan->gsub_lookups : &c->plan->gpos_lookups;
    hb_sorted_vector_t<hb_codepoint_t> new_coverage;
    + hb_zip (this+coverage, ruleSet)
    | hb_filter (glyphset, hb_first)
    | hb_filter (subset_offset_array (c, out->ruleSet, this, lookup_map), hb_second)
    | hb_map (hb_first)
    | hb_map (glyph_map)
    | hb_sink (new_coverage)
    ;

    out->coverage.serialize_serialize (c->serializer, new_coverage.iter ());
    return_trace (bool (new_coverage));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (coverage.sanitize (c, this) && ruleSet.sanitize (c, this));
  }

  protected:
  HBUINT16	format;			/* Format identifier--format = 1 */
  typename Types::template OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of table */
  Array16Of<typename Types::template OffsetTo<ChainRuleSet>>
		ruleSet;		/* Array of ChainRuleSet tables
					 * ordered by Coverage Index */
  public:
  DEFINE_SIZE_ARRAY (2 + 2 * Types::size, ruleSet);
};

template <typename Types>
struct ChainContextFormat2_5
{
  using ChainRuleSet = OT::ChainRuleSet<SmallTypes>;

  bool intersects (const hb_set_t *glyphs) const
  {
    if (!(this+coverage).intersects (glyphs))
      return false;

    const ClassDef &backtrack_class_def = this+backtrackClassDef;
    const ClassDef &input_class_def = this+inputClassDef;
    const ClassDef &lookahead_class_def = this+lookaheadClassDef;

    hb_map_t caches[3] = {};
    struct ChainContextClosureLookupContext lookup_context = {
      {intersects_class, nullptr},
      ContextFormat::ClassBasedContext,
      {&backtrack_class_def,
       &input_class_def,
       &lookahead_class_def},
      {&caches[0], &caches[1], &caches[2]}
    };

    hb_set_t retained_coverage_glyphs;
    (this+coverage).intersect_set (*glyphs, retained_coverage_glyphs);

    hb_set_t coverage_glyph_classes;
    input_class_def.intersected_classes (&retained_coverage_glyphs, &coverage_glyph_classes);

    return
    + hb_iter (ruleSet)
    | hb_map (hb_add (this))
    | hb_enumerate
    | hb_map ([&] (const hb_pair_t<unsigned, const ChainRuleSet &> p)
	      { return input_class_def.intersects_class (glyphs, p.first) &&
		       coverage_glyph_classes.has (p.first) &&
		       p.second.intersects (glyphs, lookup_context); })
    | hb_any
    ;
  }

  bool may_have_non_1to1 () const
  { return true; }

  void closure (hb_closure_context_t *c) const
  {
    if (!(this+coverage).intersects (c->glyphs))
      return;

    hb_set_t* cur_active_glyphs = c->push_cur_active_glyphs ();
    if (unlikely (!cur_active_glyphs)) return;
    get_coverage ().intersect_set (c->previous_parent_active_glyphs (),
				   *cur_active_glyphs);

    const ClassDef &backtrack_class_def = this+backtrackClassDef;
    const ClassDef &input_class_def = this+inputClassDef;
    const ClassDef &lookahead_class_def = this+lookaheadClassDef;

    hb_map_t caches[3] = {};
    intersected_class_cache_t intersected_cache;
    struct ChainContextClosureLookupContext lookup_context = {
      {intersects_class, intersected_class_glyphs},
      ContextFormat::ClassBasedContext,
      {&backtrack_class_def,
       &input_class_def,
       &lookahead_class_def},
      {&caches[0], &caches[1], &caches[2]},
      &intersected_cache
    };

    + hb_enumerate (ruleSet)
    | hb_filter ([&] (unsigned _)
    { return input_class_def.intersects_class (&c->parent_active_glyphs (), _); },
		 hb_first)
    | hb_apply ([&] (const hb_pair_t<unsigned, const typename Types::template OffsetTo<ChainRuleSet>&> _)
                {
                  const ChainRuleSet& chainrule_set = this+_.second;
                  chainrule_set.closure (c, _.first, lookup_context);
                })
    ;

    c->pop_cur_done_glyphs ();
  }

  void closure_lookups (hb_closure_lookups_context_t *c) const
  {
    if (!(this+coverage).intersects (c->glyphs))
      return;

    const ClassDef &backtrack_class_def = this+backtrackClassDef;
    const ClassDef &input_class_def = this+inputClassDef;
    const ClassDef &lookahead_class_def = this+lookaheadClassDef;

    hb_map_t caches[3] = {};
    struct ChainContextClosureLookupContext lookup_context = {
      {intersects_class, nullptr},
      ContextFormat::ClassBasedContext,
      {&backtrack_class_def,
       &input_class_def,
       &lookahead_class_def},
      {&caches[0], &caches[1], &caches[2]}
    };

    + hb_iter (ruleSet)
    | hb_map (hb_add (this))
    | hb_enumerate
    | hb_filter([&] (unsigned klass)
    { return input_class_def.intersects_class (c->glyphs, klass); }, hb_first)
    | hb_map (hb_second)
    | hb_apply ([&] (const ChainRuleSet &_)
    { _.closure_lookups (c, lookup_context); })
    ;
  }

  void collect_variation_indices (hb_collect_variation_indices_context_t *c) const {}

  void collect_glyphs (hb_collect_glyphs_context_t *c) const
  {
    (this+coverage).collect_coverage (c->input);

    const ClassDef &backtrack_class_def = this+backtrackClassDef;
    const ClassDef &input_class_def = this+inputClassDef;
    const ClassDef &lookahead_class_def = this+lookaheadClassDef;

    struct ChainContextCollectGlyphsLookupContext lookup_context = {
      {collect_class},
      {&backtrack_class_def,
       &input_class_def,
       &lookahead_class_def}
    };

    + hb_iter (ruleSet)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const ChainRuleSet &_) { _.collect_glyphs (c, lookup_context); })
    ;
  }

  bool would_apply (hb_would_apply_context_t *c) const
  {
    const ClassDef &backtrack_class_def = this+backtrackClassDef;
    const ClassDef &input_class_def = this+inputClassDef;
    const ClassDef &lookahead_class_def = this+lookaheadClassDef;

    unsigned int index = input_class_def.get_class (c->glyphs[0]);
    const ChainRuleSet &rule_set = this+ruleSet[index];
    struct ChainContextApplyLookupContext lookup_context = {
      {{match_class, match_class, match_class}},
      {&backtrack_class_def,
       &input_class_def,
       &lookahead_class_def}
    };
    return rule_set.would_apply (c, lookup_context);
  }

  const Coverage &get_coverage () const { return this+coverage; }

  unsigned cache_cost () const
  {
    unsigned c = (this+lookaheadClassDef).cost () * ruleSet.len;
    return c >= 4 ? c : 0;
  }
  bool cache_func (hb_ot_apply_context_t *c, bool enter) const
  {
    if (enter)
    {
      if (!HB_BUFFER_TRY_ALLOCATE_VAR (c->buffer, syllable))
	return false;
      auto &info = c->buffer->info;
      unsigned count = c->buffer->len;
      for (unsigned i = 0; i < count; i++)
	info[i].syllable() = 255;
      c->new_syllables = 255;
      return true;
    }
    else
    {
      c->new_syllables = (unsigned) -1;
      HB_BUFFER_DEALLOCATE_VAR (c->buffer, syllable);
      return true;
    }
  }

  bool apply_cached (hb_ot_apply_context_t *c) const { return _apply (c, true); }
  bool apply (hb_ot_apply_context_t *c) const { return _apply (c, false); }
  bool _apply (hb_ot_apply_context_t *c, bool cached) const
  {
    TRACE_APPLY (this);
    unsigned int index = (this+coverage).get_coverage (c->buffer->cur().codepoint);
    if (likely (index == NOT_COVERED)) return_trace (false);

    const ClassDef &backtrack_class_def = this+backtrackClassDef;
    const ClassDef &input_class_def = this+inputClassDef;
    const ClassDef &lookahead_class_def = this+lookaheadClassDef;

    /* match_class_caches1 is slightly faster. Use it for lookahead,
     * which is typically longer. */
    struct ChainContextApplyLookupContext lookup_context = {
      {{cached && &backtrack_class_def == &lookahead_class_def ? match_class_cached1 : match_class,
        cached ? match_class_cached2 : match_class,
        cached ? match_class_cached1 : match_class}},
      {&backtrack_class_def,
       &input_class_def,
       &lookahead_class_def}
    };

    // Note: Corresponds to match_class_cached2
    if (cached && ((c->buffer->cur().syllable() & 0xF0) >> 4) < 15)
      index = (c->buffer->cur().syllable () & 0xF0) >> 4;
    else
      index = input_class_def.get_class (c->buffer->cur().codepoint);
    const ChainRuleSet &rule_set = this+ruleSet[index];
    return_trace (rule_set.apply (c, lookup_context));
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);
    out->format = format;
    out->coverage.serialize_subset (c, coverage, this);

    hb_map_t backtrack_klass_map;
    hb_map_t input_klass_map;
    hb_map_t lookahead_klass_map;

    out->backtrackClassDef.serialize_subset (c, backtrackClassDef, this, &backtrack_klass_map);
    // TODO: subset inputClassDef based on glyphs survived in Coverage subsetting
    out->inputClassDef.serialize_subset (c, inputClassDef, this, &input_klass_map);
    out->lookaheadClassDef.serialize_subset (c, lookaheadClassDef, this, &lookahead_klass_map);

    if (unlikely (!c->serializer->propagate_error (backtrack_klass_map,
						   input_klass_map,
						   lookahead_klass_map)))
      return_trace (false);

    const hb_set_t* glyphset = c->plan->glyphset_gsub ();
    hb_set_t retained_coverage_glyphs;
    (this+coverage).intersect_set (*glyphset, retained_coverage_glyphs);

    hb_set_t coverage_glyph_classes;
    (this+inputClassDef).intersected_classes (&retained_coverage_glyphs, &coverage_glyph_classes);

    int non_zero_index = -1, index = 0;
    bool ret = true;
    const hb_map_t *lookup_map = c->table_tag == HB_OT_TAG_GSUB ? &c->plan->gsub_lookups : &c->plan->gpos_lookups;
    auto last_non_zero = c->serializer->snapshot ();
    for (const auto& _ : + hb_enumerate (ruleSet)
			 | hb_filter (input_klass_map, hb_first))
    {
      auto *o = out->ruleSet.serialize_append (c->serializer);
      if (unlikely (!o))
      {
	ret = false;
	break;
      }
      if (coverage_glyph_classes.has (_.first) &&
          o->serialize_subset (c, _.second, this,
			       lookup_map,
			       &backtrack_klass_map,
			       &input_klass_map,
			       &lookahead_klass_map))
      {
        last_non_zero = c->serializer->snapshot ();
	non_zero_index = index;
      }

      index++;
    }

    if (!ret || non_zero_index == -1) return_trace (false);

    // prune empty trailing ruleSets
    if (index > non_zero_index) {
      c->serializer->revert (last_non_zero);
      out->ruleSet.len = non_zero_index + 1;
    }

    return_trace (bool (out->ruleSet));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (coverage.sanitize (c, this) &&
		  backtrackClassDef.sanitize (c, this) &&
		  inputClassDef.sanitize (c, this) &&
		  lookaheadClassDef.sanitize (c, this) &&
		  ruleSet.sanitize (c, this));
  }

  protected:
  HBUINT16	format;			/* Format identifier--format = 2 */
  typename Types::template OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of table */
  typename Types::template OffsetTo<ClassDef>
		backtrackClassDef;	/* Offset to glyph ClassDef table
					 * containing backtrack sequence
					 * data--from beginning of table */
  typename Types::template OffsetTo<ClassDef>
		inputClassDef;		/* Offset to glyph ClassDef
					 * table containing input sequence
					 * data--from beginning of table */
  typename Types::template OffsetTo<ClassDef>
		lookaheadClassDef;	/* Offset to glyph ClassDef table
					 * containing lookahead sequence
					 * data--from beginning of table */
  Array16Of<typename Types::template OffsetTo<ChainRuleSet>>
		ruleSet;		/* Array of ChainRuleSet tables
					 * ordered by class */
  public:
  DEFINE_SIZE_ARRAY (4 + 4 * Types::size, ruleSet);
};

struct ChainContextFormat3
{
  using RuleSet = OT::RuleSet<SmallTypes>;

  bool intersects (const hb_set_t *glyphs) const
  {
    const auto &input = StructAfter<decltype (inputX)> (backtrack);

    if (!(this+input[0]).intersects (glyphs))
      return false;

    const auto &lookahead = StructAfter<decltype (lookaheadX)> (input);
    struct ChainContextClosureLookupContext lookup_context = {
      {intersects_coverage, nullptr},
      ContextFormat::CoverageBasedContext,
      {this, this, this}
    };
    return chain_context_intersects (glyphs,
				     backtrack.len, (const HBUINT16 *) backtrack.arrayZ,
				     input.len, (const HBUINT16 *) input.arrayZ + 1,
				     lookahead.len, (const HBUINT16 *) lookahead.arrayZ,
				     lookup_context);
  }

  bool may_have_non_1to1 () const
  { return true; }

  void closure (hb_closure_context_t *c) const
  {
    const auto &input = StructAfter<decltype (inputX)> (backtrack);

    if (!(this+input[0]).intersects (c->glyphs))
      return;

    hb_set_t* cur_active_glyphs = c->push_cur_active_glyphs ();
    if (unlikely (!cur_active_glyphs))
      return;
    get_coverage ().intersect_set (c->previous_parent_active_glyphs (),
				   *cur_active_glyphs);

    const auto &lookahead = StructAfter<decltype (lookaheadX)> (input);
    const auto &lookup = StructAfter<decltype (lookupX)> (lookahead);
    struct ChainContextClosureLookupContext lookup_context = {
      {intersects_coverage, intersected_coverage_glyphs},
      ContextFormat::CoverageBasedContext,
      {this, this, this}
    };
    chain_context_closure_lookup (c,
				  backtrack.len, (const HBUINT16 *) backtrack.arrayZ,
				  input.len, (const HBUINT16 *) input.arrayZ + 1,
				  lookahead.len, (const HBUINT16 *) lookahead.arrayZ,
				  lookup.len, lookup.arrayZ,
				  0, lookup_context);

    c->pop_cur_done_glyphs ();
  }

  void closure_lookups (hb_closure_lookups_context_t *c) const
  {
    if (!intersects (c->glyphs))
      return;

    const auto &input = StructAfter<decltype (inputX)> (backtrack);
    const auto &lookahead = StructAfter<decltype (lookaheadX)> (input);
    const auto &lookup = StructAfter<decltype (lookupX)> (lookahead);
    recurse_lookups (c, lookup.len, lookup.arrayZ);
  }

  void collect_variation_indices (hb_collect_variation_indices_context_t *c) const {}

  void collect_glyphs (hb_collect_glyphs_context_t *c) const
  {
    const auto &input = StructAfter<decltype (inputX)> (backtrack);

    (this+input[0]).collect_coverage (c->input);

    const auto &lookahead = StructAfter<decltype (lookaheadX)> (input);
    const auto &lookup = StructAfter<decltype (lookupX)> (lookahead);

    struct ChainContextCollectGlyphsLookupContext lookup_context = {
      {collect_coverage},
      {this, this, this}
    };
    chain_context_collect_glyphs_lookup (c,
					 backtrack.len, (const HBUINT16 *) backtrack.arrayZ,
					 input.len, (const HBUINT16 *) input.arrayZ + 1,
					 lookahead.len, (const HBUINT16 *) lookahead.arrayZ,
					 lookup.len, lookup.arrayZ,
					 lookup_context);
  }

  bool would_apply (hb_would_apply_context_t *c) const
  {
    const auto &input = StructAfter<decltype (inputX)> (backtrack);
    const auto &lookahead = StructAfter<decltype (lookaheadX)> (input);
    const auto &lookup = StructAfter<decltype (lookupX)> (lookahead);
    struct ChainContextApplyLookupContext lookup_context = {
      {{match_coverage, match_coverage, match_coverage}},
      {this, this, this}
    };
    return chain_context_would_apply_lookup (c,
					     backtrack.len, (const HBUINT16 *) backtrack.arrayZ,
					     input.len, (const HBUINT16 *) input.arrayZ + 1,
					     lookahead.len, (const HBUINT16 *) lookahead.arrayZ,
					     lookup.len, lookup.arrayZ, lookup_context);
  }

  const Coverage &get_coverage () const
  {
    const auto &input = StructAfter<decltype (inputX)> (backtrack);
    return this+input[0];
  }

  bool apply (hb_ot_apply_context_t *c) const
  {
    TRACE_APPLY (this);
    const auto &input = StructAfter<decltype (inputX)> (backtrack);

    unsigned int index = (this+input[0]).get_coverage (c->buffer->cur().codepoint);
    if (likely (index == NOT_COVERED)) return_trace (false);

    const auto &lookahead = StructAfter<decltype (lookaheadX)> (input);
    const auto &lookup = StructAfter<decltype (lookupX)> (lookahead);
    struct ChainContextApplyLookupContext lookup_context = {
      {{match_coverage, match_coverage, match_coverage}},
      {this, this, this}
    };
    return_trace (chain_context_apply_lookup (c,
					      backtrack.len, (const HBUINT16 *) backtrack.arrayZ,
					      input.len, (const HBUINT16 *) input.arrayZ + 1,
					      lookahead.len, (const HBUINT16 *) lookahead.arrayZ,
					      lookup.len, lookup.arrayZ, lookup_context));
  }

  template<typename Iterator,
	   hb_requires (hb_is_iterator (Iterator))>
  bool serialize_coverage_offsets (hb_subset_context_t *c, Iterator it, const void* base) const
  {
    TRACE_SERIALIZE (this);
    auto *out = c->serializer->start_embed<Array16OfOffset16To<Coverage>> ();

    if (unlikely (!c->serializer->allocate_size<HBUINT16> (HBUINT16::static_size)))
      return_trace (false);

    for (auto& offset : it) {
      auto *o = out->serialize_append (c->serializer);
      if (unlikely (!o) || !o->serialize_subset (c, offset, base))
        return_trace (false);
    }

    return_trace (true);
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);

    if (unlikely (!c->serializer->embed (this->format))) return_trace (false);

    if (!serialize_coverage_offsets (c, backtrack.iter (), this))
      return_trace (false);

    const auto &input = StructAfter<decltype (inputX)> (backtrack);
    if (!serialize_coverage_offsets (c, input.iter (), this))
      return_trace (false);

    const auto &lookahead = StructAfter<decltype (lookaheadX)> (input);
    if (!serialize_coverage_offsets (c, lookahead.iter (), this))
      return_trace (false);

    const auto &lookup = StructAfter<decltype (lookupX)> (lookahead);
    const hb_map_t *lookup_map = c->table_tag == HB_OT_TAG_GSUB ? &c->plan->gsub_lookups : &c->plan->gpos_lookups;

    HBUINT16 *lookupCount = c->serializer->copy<HBUINT16> (lookup.len);
    if (!lookupCount) return_trace (false);

    unsigned count = serialize_lookuprecord_array (c->serializer, lookup.as_array (), lookup_map);
    return_trace (c->serializer->check_assign (*lookupCount, count, HB_SERIALIZE_ERROR_INT_OVERFLOW));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!backtrack.sanitize (c, this))) return_trace (false);
    hb_barrier ();
    const auto &input = StructAfter<decltype (inputX)> (backtrack);
    if (unlikely (!input.sanitize (c, this))) return_trace (false);
    hb_barrier ();
    if (unlikely (!input.len)) return_trace (false); /* To be consistent with Context. */
    const auto &lookahead = StructAfter<decltype (lookaheadX)> (input);
    if (unlikely (!lookahead.sanitize (c, this))) return_trace (false);
    hb_barrier ();
    const auto &lookup = StructAfter<decltype (lookupX)> (lookahead);
    return_trace (likely (lookup.sanitize (c)));
  }

  protected:
  HBUINT16	format;			/* Format identifier--format = 3 */
  Array16OfOffset16To<Coverage>
		backtrack;		/* Array of coverage tables
					 * in backtracking sequence, in  glyph
					 * sequence order */
  Array16OfOffset16To<Coverage>
		inputX		;	/* Array of coverage
					 * tables in input sequence, in glyph
					 * sequence order */
  Array16OfOffset16To<Coverage>
		lookaheadX;		/* Array of coverage tables
					 * in lookahead sequence, in glyph
					 * sequence order */
  Array16Of<LookupRecord>
		lookupX;		/* Array of LookupRecords--in
					 * design order) */
  public:
  DEFINE_SIZE_MIN (10);
};

struct ChainContext
{
  template <typename context_t, typename ...Ts>
  typename context_t::return_t dispatch (context_t *c, Ts&&... ds) const
  {
    if (unlikely (!c->may_dispatch (this, &u.format))) return c->no_dispatch_return_value ();
    TRACE_DISPATCH (this, u.format);
    switch (u.format) {
    case 1: hb_barrier (); return_trace (c->dispatch (u.format1, std::forward<Ts> (ds)...));
    case 2: hb_barrier (); return_trace (c->dispatch (u.format2, std::forward<Ts> (ds)...));
    case 3: hb_barrier (); return_trace (c->dispatch (u.format3, std::forward<Ts> (ds)...));
#ifndef HB_NO_BEYOND_64K
    case 4: hb_barrier (); return_trace (c->dispatch (u.format4, std::forward<Ts> (ds)...));
    case 5: hb_barrier (); return_trace (c->dispatch (u.format5, std::forward<Ts> (ds)...));
#endif
    default:return_trace (c->default_return_value ());
    }
  }

  protected:
  union {
  HBUINT16				format;	/* Format identifier */
  ChainContextFormat1_4<SmallTypes>	format1;
  ChainContextFormat2_5<SmallTypes>	format2;
  ChainContextFormat3			format3;
#ifndef HB_NO_BEYOND_64K
  ChainContextFormat1_4<MediumTypes>	format4;
  ChainContextFormat2_5<MediumTypes>	format5;
#endif
  } u;
};


template <typename T>
struct ExtensionFormat1
{
  unsigned int get_type () const { return extensionLookupType; }

  template <typename X>
  const X& get_subtable () const
  { return this + reinterpret_cast<const Offset32To<typename T::SubTable> &> (extensionOffset); }

  template <typename context_t, typename ...Ts>
  typename context_t::return_t dispatch (context_t *c, Ts&&... ds) const
  {
    if (unlikely (!c->may_dispatch (this, this))) return c->no_dispatch_return_value ();
    TRACE_DISPATCH (this, format);
    return_trace (get_subtable<typename T::SubTable> ().dispatch (c, get_type (), std::forward<Ts> (ds)...));
  }

  void collect_variation_indices (hb_collect_variation_indices_context_t *c) const
  { dispatch (c); }

  /* This is called from may_dispatch() above with hb_sanitize_context_t. */
  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  hb_barrier () &&
		  extensionLookupType != T::SubTable::Extension);
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);

    auto *out = c->serializer->start_embed (this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);

    out->format = format;
    out->extensionLookupType = extensionLookupType;

    const auto& src_offset =
        reinterpret_cast<const Offset32To<typename T::SubTable> &> (extensionOffset);
    auto& dest_offset =
        reinterpret_cast<Offset32To<typename T::SubTable> &> (out->extensionOffset);

    return_trace (dest_offset.serialize_subset (c, src_offset, this, get_type ()));
  }

  protected:
  HBUINT16	format;			/* Format identifier. Set to 1. */
  HBUINT16	extensionLookupType;	/* Lookup type of subtable referenced
					 * by ExtensionOffset (i.e. the
					 * extension subtable). */
  Offset32	extensionOffset;	/* Offset to the extension subtable,
					 * of lookup type subtable. */
  public:
  DEFINE_SIZE_STATIC (8);
};

template <typename T>
struct Extension
{
  unsigned int get_type () const
  {
    switch (u.format) {
    case 1: hb_barrier (); return u.format1.get_type ();
    default:return 0;
    }
  }
  template <typename X>
  const X& get_subtable () const
  {
    switch (u.format) {
    case 1: hb_barrier (); return u.format1.template get_subtable<typename T::SubTable> ();
    default:return Null (typename T::SubTable);
    }
  }

  // Specialization of dispatch for subset. dispatch() normally just
  // dispatches to the sub table this points too, but for subset
  // we need to run subset on this subtable too.
  template <typename ...Ts>
  typename hb_subset_context_t::return_t dispatch (hb_subset_context_t *c, Ts&&... ds) const
  {
    switch (u.format) {
    case 1: hb_barrier (); return u.format1.subset (c);
    default: return c->default_return_value ();
    }
  }

  template <typename context_t, typename ...Ts>
  typename context_t::return_t dispatch (context_t *c, Ts&&... ds) const
  {
    if (unlikely (!c->may_dispatch (this, &u.format))) return c->no_dispatch_return_value ();
    TRACE_DISPATCH (this, u.format);
    switch (u.format) {
    case 1: hb_barrier (); return_trace (u.format1.dispatch (c, std::forward<Ts> (ds)...));
    default:return_trace (c->default_return_value ());
    }
  }

  protected:
  union {
  HBUINT16		format;		/* Format identifier */
  ExtensionFormat1<T>	format1;
  } u;
};


/*
 * GSUB/GPOS Common
 */

struct hb_ot_layout_lookup_accelerator_t
{
  template <typename TLookup>
  static hb_ot_layout_lookup_accelerator_t *create (const TLookup &lookup)
  {
    unsigned count = lookup.get_subtable_count ();

    unsigned size = sizeof (hb_ot_layout_lookup_accelerator_t) -
		    HB_VAR_ARRAY * sizeof (hb_accelerate_subtables_context_t::hb_applicable_t) +
		    count * sizeof (hb_accelerate_subtables_context_t::hb_applicable_t);

    /* The following is a calloc because when we are collecting subtables,
     * some of them might be invalid and hence not collect; as a result,
     * we might not fill in all the count entries of the subtables array.
     * Zeroing it allows the set digest to gatekeep it without having to
     * initialize it further. */
    auto *thiz = (hb_ot_layout_lookup_accelerator_t *) hb_calloc (1, size);
    if (unlikely (!thiz))
      return nullptr;

    hb_accelerate_subtables_context_t c_accelerate_subtables (thiz->subtables);
    lookup.dispatch (&c_accelerate_subtables);

    thiz->digest.init ();
    for (auto& subtable : hb_iter (thiz->subtables, count))
      thiz->digest.union_ (subtable.digest);

#ifndef HB_NO_OT_LAYOUT_LOOKUP_CACHE
    thiz->cache_user_idx = c_accelerate_subtables.cache_user_idx;
    for (unsigned i = 0; i < count; i++)
      if (i != thiz->cache_user_idx)
	thiz->subtables[i].apply_cached_func = thiz->subtables[i].apply_func;
#endif

    return thiz;
  }

  bool may_have (hb_codepoint_t g) const
  { return digest.may_have (g); }

#ifndef HB_OPTIMIZE_SIZE
  HB_ALWAYS_INLINE
#endif
  bool apply (hb_ot_apply_context_t *c, unsigned subtables_count, bool use_cache) const
  {
#ifndef HB_NO_OT_LAYOUT_LOOKUP_CACHE
    if (use_cache)
    {
      return
      + hb_iter (hb_iter (subtables, subtables_count))
      | hb_map ([&c] (const hb_accelerate_subtables_context_t::hb_applicable_t &_) { return _.apply_cached (c); })
      | hb_any
      ;
    }
    else
#endif
    {
      return
      + hb_iter (hb_iter (subtables, subtables_count))
      | hb_map ([&c] (const hb_accelerate_subtables_context_t::hb_applicable_t &_) { return _.apply (c); })
      | hb_any
      ;
    }
    return false;
  }

  bool cache_enter (hb_ot_apply_context_t *c) const
  {
#ifndef HB_NO_OT_LAYOUT_LOOKUP_CACHE
    return cache_user_idx != (unsigned) -1 &&
	   subtables[cache_user_idx].cache_enter (c);
#else
    return false;
#endif
  }
  void cache_leave (hb_ot_apply_context_t *c) const
  {
#ifndef HB_NO_OT_LAYOUT_LOOKUP_CACHE
    subtables[cache_user_idx].cache_leave (c);
#endif
  }


  hb_set_digest_t digest;
  private:
#ifndef HB_NO_OT_LAYOUT_LOOKUP_CACHE
  unsigned cache_user_idx = (unsigned) -1;
#endif
  hb_accelerate_subtables_context_t::hb_applicable_t subtables[HB_VAR_ARRAY];
};

template <typename Types>
struct GSUBGPOSVersion1_2
{
  friend struct GSUBGPOS;

  protected:
  FixedVersion<>version;	/* Version of the GSUB/GPOS table--initially set
				 * to 0x00010000u */
  typename Types:: template OffsetTo<ScriptList>
		scriptList;	/* ScriptList table */
  typename Types::template OffsetTo<FeatureList>
		featureList;	/* FeatureList table */
  typename Types::template OffsetTo<LookupList<Types>>
		lookupList;	/* LookupList table */
  Offset32To<FeatureVariations>
		featureVars;	/* Offset to Feature Variations
				   table--from beginning of table
				 * (may be NULL).  Introduced
				 * in version 0x00010001. */
  public:
  DEFINE_SIZE_MIN (4 + 3 * Types::size);

  unsigned int get_size () const
  {
    return min_size +
	   (version.to_int () >= 0x00010001u ? featureVars.static_size : 0);
  }

  const typename Types::template OffsetTo<LookupList<Types>>* get_lookup_list_offset () const
  {
    return &lookupList;
  }

  template <typename TLookup>
  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    typedef List16OfOffsetTo<TLookup, typename Types::HBUINT> TLookupList;
    if (unlikely (!(scriptList.sanitize (c, this) &&
		    featureList.sanitize (c, this) &&
		    reinterpret_cast<const typename Types::template OffsetTo<TLookupList> &> (lookupList).sanitize (c, this))))
      return_trace (false);

#ifndef HB_NO_VAR
    if (unlikely (!(version.to_int () < 0x00010001u || featureVars.sanitize (c, this))))
      return_trace (false);
#endif

    return_trace (true);
  }

  template <typename TLookup>
  bool subset (hb_subset_layout_context_t *c) const
  {
    TRACE_SUBSET (this);

    auto *out = c->subset_context->serializer->start_embed (this);
    if (unlikely (!c->subset_context->serializer->extend_min (out))) return_trace (false);

    out->version = version;

    typedef LookupOffsetList<TLookup, typename Types::HBUINT> TLookupList;
    reinterpret_cast<typename Types::template OffsetTo<TLookupList> &> (out->lookupList)
	.serialize_subset (c->subset_context,
			   reinterpret_cast<const typename Types::template OffsetTo<TLookupList> &> (lookupList),
			   this,
			   c);

    reinterpret_cast<typename Types::template OffsetTo<RecordListOfFeature> &> (out->featureList)
	.serialize_subset (c->subset_context,
			   reinterpret_cast<const typename Types::template OffsetTo<RecordListOfFeature> &> (featureList),
			   this,
			   c);

    out->scriptList.serialize_subset (c->subset_context,
				      scriptList,
				      this,
				      c);

#ifndef HB_NO_VAR
    if (version.to_int () >= 0x00010001u)
    {
      auto snapshot = c->subset_context->serializer->snapshot ();
      if (!c->subset_context->serializer->extend_min (&out->featureVars))
        return_trace (false);

      // if all axes are pinned all feature vars are dropped.
      bool ret = !c->subset_context->plan->all_axes_pinned
                 && out->featureVars.serialize_subset (c->subset_context, featureVars, this, c);
      if (!ret && version.major == 1)
      {
        c->subset_context->serializer->revert (snapshot);
	out->version.major = 1;
	out->version.minor = 0;
      }
    }
#endif

    return_trace (true);
  }
};

struct GSUBGPOS
{
  unsigned int get_size () const
  {
    switch (u.version.major) {
    case 1: hb_barrier (); return u.version1.get_size ();
#ifndef HB_NO_BEYOND_64K
    case 2: hb_barrier (); return u.version2.get_size ();
#endif
    default: return u.version.static_size;
    }
  }

  template <typename TLookup>
  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!u.version.sanitize (c))) return_trace (false);
    hb_barrier ();
    switch (u.version.major) {
    case 1: hb_barrier (); return_trace (u.version1.sanitize<TLookup> (c));
#ifndef HB_NO_BEYOND_64K
    case 2: hb_barrier (); return_trace (u.version2.sanitize<TLookup> (c));
#endif
    default: return_trace (true);
    }
  }

  template <typename TLookup>
  bool subset (hb_subset_layout_context_t *c) const
  {
    switch (u.version.major) {
    case 1: hb_barrier (); return u.version1.subset<TLookup> (c);
#ifndef HB_NO_BEYOND_64K
    case 2: hb_barrier (); return u.version2.subset<TLookup> (c);
#endif
    default: return false;
    }
  }

  const ScriptList &get_script_list () const
  {
    switch (u.version.major) {
    case 1: hb_barrier (); return this+u.version1.scriptList;
#ifndef HB_NO_BEYOND_64K
    case 2: hb_barrier (); return this+u.version2.scriptList;
#endif
    default: return Null (ScriptList);
    }
  }
  const FeatureList &get_feature_list () const
  {
    switch (u.version.major) {
    case 1: hb_barrier (); return this+u.version1.featureList;
#ifndef HB_NO_BEYOND_64K
    case 2: hb_barrier (); return this+u.version2.featureList;
#endif
    default: return Null (FeatureList);
    }
  }
  unsigned int get_lookup_count () const
  {
    switch (u.version.major) {
    case 1: hb_barrier (); return (this+u.version1.lookupList).len;
#ifndef HB_NO_BEYOND_64K
    case 2: hb_barrier (); return (this+u.version2.lookupList).len;
#endif
    default: return 0;
    }
  }
  const Lookup& get_lookup (unsigned int i) const
  {
    switch (u.version.major) {
    case 1: hb_barrier (); return (this+u.version1.lookupList)[i];
#ifndef HB_NO_BEYOND_64K
    case 2: hb_barrier (); return (this+u.version2.lookupList)[i];
#endif
    default: return Null (Lookup);
    }
  }
  const FeatureVariations &get_feature_variations () const
  {
    switch (u.version.major) {
    case 1: hb_barrier (); return (u.version.to_int () >= 0x00010001u && hb_barrier () ? this+u.version1.featureVars : Null (FeatureVariations));
#ifndef HB_NO_BEYOND_64K
    case 2: hb_barrier (); return this+u.version2.featureVars;
#endif
    default: return Null (FeatureVariations);
    }
  }

  bool has_data () const { return u.version.to_int (); }
  unsigned int get_script_count () const
  { return get_script_list ().len; }
  const Tag& get_script_tag (unsigned int i) const
  { return get_script_list ().get_tag (i); }
  unsigned int get_script_tags (unsigned int start_offset,
				unsigned int *script_count /* IN/OUT */,
				hb_tag_t     *script_tags /* OUT */) const
  { return get_script_list ().get_tags (start_offset, script_count, script_tags); }
  const Script& get_script (unsigned int i) const
  { return get_script_list ()[i]; }
  bool find_script_index (hb_tag_t tag, unsigned int *index) const
  { return get_script_list ().find_index (tag, index); }

  unsigned int get_feature_count () const
  { return get_feature_list ().len; }
  hb_tag_t get_feature_tag (unsigned int i) const
  { return i == Index::NOT_FOUND_INDEX ? HB_TAG_NONE : get_feature_list ().get_tag (i); }
  unsigned int get_feature_tags (unsigned int start_offset,
				 unsigned int *feature_count /* IN/OUT */,
				 hb_tag_t     *feature_tags /* OUT */) const
  { return get_feature_list ().get_tags (start_offset, feature_count, feature_tags); }
  const Feature& get_feature (unsigned int i) const
  { return get_feature_list ()[i]; }
  bool find_feature_index (hb_tag_t tag, unsigned int *index) const
  { return get_feature_list ().find_index (tag, index); }

  bool find_variations_index (const int *coords, unsigned int num_coords,
			      unsigned int *index,
			      ItemVarStoreInstancer *instancer) const
  {
#ifdef HB_NO_VAR
    *index = FeatureVariations::NOT_FOUND_INDEX;
    return false;
#endif
    return get_feature_variations ().find_index (coords, num_coords, index, instancer);
  }
  const Feature& get_feature_variation (unsigned int feature_index,
					unsigned int variations_index) const
  {
#ifndef HB_NO_VAR
    if (FeatureVariations::NOT_FOUND_INDEX != variations_index &&
	u.version.to_int () >= 0x00010001u)
    {
      const Feature *feature = get_feature_variations ().find_substitute (variations_index,
									  feature_index);
      if (feature)
	return *feature;
    }
#endif
    return get_feature (feature_index);
  }

  void feature_variation_collect_lookups (const hb_set_t *feature_indexes,
					  const hb_hashmap_t<unsigned, hb::shared_ptr<hb_set_t>> *feature_record_cond_idx_map,
					  hb_set_t       *lookup_indexes /* OUT */) const
  {
#ifndef HB_NO_VAR
    get_feature_variations ().collect_lookups (feature_indexes, feature_record_cond_idx_map, lookup_indexes);
#endif
  }

#ifndef HB_NO_VAR
  void collect_feature_substitutes_with_variations (hb_collect_feature_substitutes_with_var_context_t *c) const
  { get_feature_variations ().collect_feature_substitutes_with_variations (c); }
#endif

  template <typename TLookup>
  void closure_lookups (hb_face_t      *face,
			const hb_set_t *glyphs,
			hb_set_t       *lookup_indexes /* IN/OUT */) const
  {
    hb_set_t visited_lookups, inactive_lookups;
    hb_closure_lookups_context_t c (face, glyphs, &visited_lookups, &inactive_lookups);

    c.set_recurse_func (TLookup::template dispatch_recurse_func<hb_closure_lookups_context_t>);

    for (unsigned lookup_index : *lookup_indexes)
      reinterpret_cast<const TLookup &> (get_lookup (lookup_index)).closure_lookups (&c, lookup_index);

    hb_set_union (lookup_indexes, &visited_lookups);
    hb_set_subtract (lookup_indexes, &inactive_lookups);
  }

  void prune_langsys (const hb_map_t *duplicate_feature_map,
                      const hb_set_t *layout_scripts,
                      hb_hashmap_t<unsigned, hb::unique_ptr<hb_set_t>> *script_langsys_map,
                      hb_set_t       *new_feature_indexes /* OUT */) const
  {
    hb_prune_langsys_context_t c (this, script_langsys_map, duplicate_feature_map, new_feature_indexes);

    unsigned count = get_script_count ();
    for (unsigned script_index = 0; script_index < count; script_index++)
    {
      const Tag& tag = get_script_tag (script_index);
      if (!layout_scripts->has (tag)) continue;
      const Script& s = get_script (script_index);
      s.prune_langsys (&c, script_index);
    }
  }

  void prune_features (const hb_map_t *lookup_indices, /* IN */
		       const hb_hashmap_t<unsigned, hb::shared_ptr<hb_set_t>> *feature_record_cond_idx_map, /* IN */
		       const hb_hashmap_t<unsigned, const Feature*> *feature_substitutes_map, /* IN */
		       hb_set_t       *feature_indices /* IN/OUT */) const
  {
#ifndef HB_NO_VAR
    // This is the set of feature indices which have alternate versions defined
    // if the FeatureVariation's table and the alternate version(s) intersect the
    // set of lookup indices.
    hb_set_t alternate_feature_indices;
    get_feature_variations ().closure_features (lookup_indices, feature_record_cond_idx_map, &alternate_feature_indices);
    if (unlikely (alternate_feature_indices.in_error()))
    {
      feature_indices->err ();
      return;
    }
#endif

    for (unsigned i : hb_iter (feature_indices))
    {
      hb_tag_t tag =  get_feature_tag (i);
      if (tag == HB_TAG ('p', 'r', 'e', 'f'))
        // Note: Never ever drop feature 'pref', even if it's empty.
        // HarfBuzz chooses shaper for Khmer based on presence of this
        // feature.	See thread at:
	// http://lists.freedesktop.org/archives/harfbuzz/2012-November/002660.html
        continue;


      const Feature *f = &(get_feature (i));
      const Feature** p = nullptr;
      if (feature_substitutes_map->has (i, &p))
        f = *p;

      if (!f->featureParams.is_null () &&
          tag == HB_TAG ('s', 'i', 'z', 'e'))
        continue;

      if (!f->intersects_lookup_indexes (lookup_indices)
#ifndef HB_NO_VAR
          && !alternate_feature_indices.has (i)
#endif
	  )
	feature_indices->del (i);
    }
  }

  void collect_name_ids (const hb_map_t *feature_index_map,
                         hb_set_t *nameids_to_retain /* OUT */) const
  {
    unsigned count = get_feature_count ();
    for (unsigned i = 0 ; i < count; i++)
    {
      if (!feature_index_map->has (i)) continue;
      hb_tag_t tag = get_feature_tag (i);
      get_feature (i).collect_name_ids (tag, nameids_to_retain);
    }
  }

  template <typename T>
  struct accelerator_t
  {
    accelerator_t (hb_face_t *face)
    {
      hb_sanitize_context_t sc;
      sc.lazy_some_gpos = true;
      this->table = sc.reference_table<T> (face);

      if (unlikely (this->table->is_blocklisted (this->table.get_blob (), face)))
      {
	hb_blob_destroy (this->table.get_blob ());
	this->table = hb_blob_get_empty ();
      }

      this->lookup_count = table->get_lookup_count ();

      this->accels = (hb_atomic_ptr_t<hb_ot_layout_lookup_accelerator_t> *) hb_calloc (this->lookup_count, sizeof (*accels));
      if (unlikely (!this->accels))
      {
	this->lookup_count = 0;
	this->table.destroy ();
	this->table = hb_blob_get_empty ();
      }
    }
    ~accelerator_t ()
    {
      for (unsigned int i = 0; i < this->lookup_count; i++)
	hb_free (this->accels[i]);
      hb_free (this->accels);
      this->table.destroy ();
    }

    hb_blob_t *get_blob () const { return table.get_blob (); }

    hb_ot_layout_lookup_accelerator_t *get_accel (unsigned lookup_index) const
    {
      if (unlikely (lookup_index >= lookup_count)) return nullptr;

    retry:
      auto *accel = accels[lookup_index].get_acquire ();
      if (unlikely (!accel))
      {
	accel = hb_ot_layout_lookup_accelerator_t::create (table->get_lookup (lookup_index));
	if (unlikely (!accel))
	  return nullptr;

	if (unlikely (!accels[lookup_index].cmpexch (nullptr, accel)))
	{
	  hb_free (accel);
	  goto retry;
	}
      }

      return accel;
    }

    hb_blob_ptr_t<T> table;
    unsigned int lookup_count;
    hb_atomic_ptr_t<hb_ot_layout_lookup_accelerator_t> *accels;
  };

  protected:
  union {
  FixedVersion<>			version;	/* Version identifier */
  GSUBGPOSVersion1_2<SmallTypes>	version1;
#ifndef HB_NO_BEYOND_64K
  GSUBGPOSVersion1_2<MediumTypes>	version2;
#endif
  } u;
  public:
  DEFINE_SIZE_MIN (4);
};


} /* namespace OT */


#endif /* HB_OT_LAYOUT_GSUBGPOS_HH */
