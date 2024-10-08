/*
 * Copyright © 2020  Ebrahim Byagowi
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

#ifndef HB_DRAW_HH
#define HB_DRAW_HH

#include "hb.hh"


/*
 * hb_draw_funcs_t
 */

#define HB_DRAW_FUNCS_IMPLEMENT_CALLBACKS \
  HB_DRAW_FUNC_IMPLEMENT (move_to) \
  HB_DRAW_FUNC_IMPLEMENT (line_to) \
  HB_DRAW_FUNC_IMPLEMENT (quadratic_to) \
  HB_DRAW_FUNC_IMPLEMENT (cubic_to) \
  HB_DRAW_FUNC_IMPLEMENT (close_path) \
  /* ^--- Add new callbacks here */

struct hb_draw_funcs_t
{
  hb_object_header_t header;

  struct {
#define HB_DRAW_FUNC_IMPLEMENT(name) hb_draw_##name##_func_t name;
    HB_DRAW_FUNCS_IMPLEMENT_CALLBACKS
#undef HB_DRAW_FUNC_IMPLEMENT
  } func;

  struct {
#define HB_DRAW_FUNC_IMPLEMENT(name) void *name;
    HB_DRAW_FUNCS_IMPLEMENT_CALLBACKS
#undef HB_DRAW_FUNC_IMPLEMENT
  } *user_data;

  struct {
#define HB_DRAW_FUNC_IMPLEMENT(name) hb_destroy_func_t name;
    HB_DRAW_FUNCS_IMPLEMENT_CALLBACKS
#undef HB_DRAW_FUNC_IMPLEMENT
  } *destroy;

  void emit_move_to (void *draw_data, hb_draw_state_t &st,
		     float to_x, float to_y)
  { func.move_to (this, draw_data, &st,
		  to_x, to_y,
		  !user_data ? nullptr : user_data->move_to); }
  void emit_line_to (void *draw_data, hb_draw_state_t &st,
		     float to_x, float to_y)
  { func.line_to (this, draw_data, &st,
		  to_x, to_y,
		  !user_data ? nullptr : user_data->line_to); }
  void emit_quadratic_to (void *draw_data, hb_draw_state_t &st,
			  float control_x, float control_y,
			  float to_x, float to_y)
  { func.quadratic_to (this, draw_data, &st,
		       control_x, control_y,
		       to_x, to_y,
		       !user_data ? nullptr : user_data->quadratic_to); }
  void emit_cubic_to (void *draw_data, hb_draw_state_t &st,
		      float control1_x, float control1_y,
		      float control2_x, float control2_y,
		      float to_x, float to_y)
  { func.cubic_to (this, draw_data, &st,
		   control1_x, control1_y,
		   control2_x, control2_y,
		   to_x, to_y,
		   !user_data ? nullptr : user_data->cubic_to); }
  void emit_close_path (void *draw_data, hb_draw_state_t &st)
  { func.close_path (this, draw_data, &st,
		     !user_data ? nullptr : user_data->close_path); }


  void
  HB_ALWAYS_INLINE
  move_to (void *draw_data, hb_draw_state_t &st,
	   float to_x, float to_y)
  {
    if (unlikely (st.path_open)) close_path (draw_data, st);
    st.current_x = to_x;
    st.current_y = to_y;
  }

  void
  HB_ALWAYS_INLINE
  line_to (void *draw_data, hb_draw_state_t &st,
	   float to_x, float to_y)
  {
    if (unlikely (!st.path_open)) start_path (draw_data, st);
    emit_line_to (draw_data, st, to_x, to_y);
    st.current_x = to_x;
    st.current_y = to_y;
  }

  void
  HB_ALWAYS_INLINE
  quadratic_to (void *draw_data, hb_draw_state_t &st,
		float control_x, float control_y,
		float to_x, float to_y)
  {
    if (unlikely (!st.path_open)) start_path (draw_data, st);
    emit_quadratic_to (draw_data, st, control_x, control_y, to_x, to_y);
    st.current_x = to_x;
    st.current_y = to_y;
  }

  void
  HB_ALWAYS_INLINE
  cubic_to (void *draw_data, hb_draw_state_t &st,
	    float control1_x, float control1_y,
	    float control2_x, float control2_y,
	    float to_x, float to_y)
  {
    if (unlikely (!st.path_open)) start_path (draw_data, st);
    emit_cubic_to (draw_data, st, control1_x, control1_y, control2_x, control2_y, to_x, to_y);
    st.current_x = to_x;
    st.current_y = to_y;
  }

  void
  HB_ALWAYS_INLINE
  close_path (void *draw_data, hb_draw_state_t &st)
  {
    if (likely (st.path_open))
    {
      if ((st.path_start_x != st.current_x) || (st.path_start_y != st.current_y))
	emit_line_to (draw_data, st, st.path_start_x, st.path_start_y);
      emit_close_path (draw_data, st);
    }
    st.path_open = false;
    st.path_start_x = st.current_x = st.path_start_y = st.current_y = 0;
  }

  protected:

  void start_path (void *draw_data, hb_draw_state_t &st)
  {
    assert (!st.path_open);
    emit_move_to (draw_data, st, st.current_x, st.current_y);
    st.path_open = true;
    st.path_start_x = st.current_x;
    st.path_start_y = st.current_y;
  }
};
DECLARE_NULL_INSTANCE (hb_draw_funcs_t);

struct hb_draw_session_t
{
  hb_draw_session_t (hb_draw_funcs_t *funcs_, void *draw_data_, float slant_ = 0.f)
    : slant {slant_}, not_slanted {slant == 0.f},
      funcs {funcs_}, draw_data {draw_data_}, st HB_DRAW_STATE_DEFAULT
  {}

  ~hb_draw_session_t () { close_path (); }

  HB_ALWAYS_INLINE
  void move_to (float to_x, float to_y)
  {
    if (likely (not_slanted))
      funcs->move_to (draw_data, st,
		      to_x, to_y);
    else
      funcs->move_to (draw_data, st,
		      to_x + to_y * slant, to_y);
  }
  HB_ALWAYS_INLINE
  void line_to (float to_x, float to_y)
  {
    if (likely (not_slanted))
      funcs->line_to (draw_data, st,
		      to_x, to_y);
    else
      funcs->line_to (draw_data, st,
		      to_x + to_y * slant, to_y);
  }
  void
  HB_ALWAYS_INLINE
  quadratic_to (float control_x, float control_y,
		float to_x, float to_y)
  {
    if (likely (not_slanted))
      funcs->quadratic_to (draw_data, st,
			   control_x, control_y,
			   to_x, to_y);
    else
      funcs->quadratic_to (draw_data, st,
			   control_x + control_y * slant, control_y,
			   to_x + to_y * slant, to_y);
  }
  void
  HB_ALWAYS_INLINE
  cubic_to (float control1_x, float control1_y,
	    float control2_x, float control2_y,
	    float to_x, float to_y)
  {
    if (likely (not_slanted))
      funcs->cubic_to (draw_data, st,
		       control1_x, control1_y,
		       control2_x, control2_y,
		       to_x, to_y);
    else
      funcs->cubic_to (draw_data, st,
		       control1_x + control1_y * slant, control1_y,
		       control2_x + control2_y * slant, control2_y,
		       to_x + to_y * slant, to_y);
  }
  HB_ALWAYS_INLINE
  void close_path ()
  {
    funcs->close_path (draw_data, st);
  }

  public:
  float slant;
  bool not_slanted;
  hb_draw_funcs_t *funcs;
  void *draw_data;
  hb_draw_state_t st;
};

#endif /* HB_DRAW_HH */
