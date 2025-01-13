/*
 * Copyright © 2019-2020  Ebrahim Byagowi
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

#if !defined(HB_H_IN) && !defined(HB_NO_SINGLE_HEADER_ERROR)
#error "Include <hb.h> instead."
#endif

#ifndef HB_DRAW_H
#define HB_DRAW_H

#include "hb.h"

HB_BEGIN_DECLS


/**
 * hb_draw_state_t
 * @path_open: Whether there is an open path
 * @path_start_x: X component of the start of current path
 * @path_start_y: Y component of the start of current path
 * @current_x: X component of current point
 * @current_y: Y component of current point
 *
 * Current drawing state.
 *
 * Since: 4.0.0
 **/
typedef struct hb_draw_state_t {
  hb_bool_t path_open;

  float path_start_x;
  float path_start_y;

  float current_x;
  float current_y;

  /*< private >*/
  hb_var_num_t   reserved1;
  hb_var_num_t   reserved2;
  hb_var_num_t   reserved3;
  hb_var_num_t   reserved4;
  hb_var_num_t   reserved5;
  hb_var_num_t   reserved6;
  hb_var_num_t   reserved7;
} hb_draw_state_t;

/**
 * HB_DRAW_STATE_DEFAULT:
 *
 * The default #hb_draw_state_t at the start of glyph drawing.
 */
#define HB_DRAW_STATE_DEFAULT {0, 0.f, 0.f, 0.f, 0.f, {0.}, {0.}, {0.}, {0.}, {0.}, {0.}, {0.}}


/**
 * hb_draw_funcs_t:
 *
 * Glyph draw callbacks.
 *
 * #hb_draw_move_to_func_t, #hb_draw_line_to_func_t and
 * #hb_draw_cubic_to_func_t calls are necessary to be defined but we translate
 * #hb_draw_quadratic_to_func_t calls to #hb_draw_cubic_to_func_t if the
 * callback isn't defined.
 *
 * Since: 4.0.0
 **/

typedef struct hb_draw_funcs_t hb_draw_funcs_t;


/**
 * hb_draw_move_to_func_t:
 * @dfuncs: draw functions object
 * @draw_data: The data accompanying the draw functions in hb_font_draw_glyph()
 * @st: current draw state
 * @to_x: X component of target point
 * @to_y: Y component of target point
 * @user_data: User data pointer passed to hb_draw_funcs_set_move_to_func()
 *
 * A virtual method for the #hb_draw_funcs_t to perform a "move-to" draw
 * operation.
 *
 * Since: 4.0.0
 *
 **/
typedef void (*hb_draw_move_to_func_t) (hb_draw_funcs_t *dfuncs, void *draw_data,
					hb_draw_state_t *st,
					float to_x, float to_y,
					void *user_data);

/**
 * hb_draw_line_to_func_t:
 * @dfuncs: draw functions object
 * @draw_data: The data accompanying the draw functions in hb_font_draw_glyph()
 * @st: current draw state
 * @to_x: X component of target point
 * @to_y: Y component of target point
 * @user_data: User data pointer passed to hb_draw_funcs_set_line_to_func()
 *
 * A virtual method for the #hb_draw_funcs_t to perform a "line-to" draw
 * operation.
 *
 * Since: 4.0.0
 *
 **/
typedef void (*hb_draw_line_to_func_t) (hb_draw_funcs_t *dfuncs, void *draw_data,
					hb_draw_state_t *st,
					float to_x, float to_y,
					void *user_data);

/**
 * hb_draw_quadratic_to_func_t:
 * @dfuncs: draw functions object
 * @draw_data: The data accompanying the draw functions in hb_font_draw_glyph()
 * @st: current draw state
 * @control_x: X component of control point
 * @control_y: Y component of control point
 * @to_x: X component of target point
 * @to_y: Y component of target point
 * @user_data: User data pointer passed to hb_draw_funcs_set_quadratic_to_func()
 *
 * A virtual method for the #hb_draw_funcs_t to perform a "quadratic-to" draw
 * operation.
 *
 * Since: 4.0.0
 *
 **/
typedef void (*hb_draw_quadratic_to_func_t) (hb_draw_funcs_t *dfuncs, void *draw_data,
					     hb_draw_state_t *st,
					     float control_x, float control_y,
					     float to_x, float to_y,
					     void *user_data);

/**
 * hb_draw_cubic_to_func_t:
 * @dfuncs: draw functions object
 * @draw_data: The data accompanying the draw functions in hb_font_draw_glyph()
 * @st: current draw state
 * @control1_x: X component of first control point
 * @control1_y: Y component of first control point
 * @control2_x: X component of second control point
 * @control2_y: Y component of second control point
 * @to_x: X component of target point
 * @to_y: Y component of target point
 * @user_data: User data pointer passed to hb_draw_funcs_set_cubic_to_func()
 *
 * A virtual method for the #hb_draw_funcs_t to perform a "cubic-to" draw
 * operation.
 *
 * Since: 4.0.0
 *
 **/
typedef void (*hb_draw_cubic_to_func_t) (hb_draw_funcs_t *dfuncs, void *draw_data,
					 hb_draw_state_t *st,
					 float control1_x, float control1_y,
					 float control2_x, float control2_y,
					 float to_x, float to_y,
					 void *user_data);

/**
 * hb_draw_close_path_func_t:
 * @dfuncs: draw functions object
 * @draw_data: The data accompanying the draw functions in hb_font_draw_glyph()
 * @st: current draw state
 * @user_data: User data pointer passed to hb_draw_funcs_set_close_path_func()
 *
 * A virtual method for the #hb_draw_funcs_t to perform a "close-path" draw
 * operation.
 *
 * Since: 4.0.0
 *
 **/
typedef void (*hb_draw_close_path_func_t) (hb_draw_funcs_t *dfuncs, void *draw_data,
					   hb_draw_state_t *st,
					   void *user_data);

/**
 * hb_draw_funcs_set_move_to_func:
 * @dfuncs: draw functions object
 * @func: (closure user_data) (destroy destroy) (scope notified): move-to callback
 * @user_data: Data to pass to @func
 * @destroy: (nullable): The function to call when @user_data is not needed anymore
 *
 * Sets move-to callback to the draw functions object.
 *
 * Since: 4.0.0
 **/
HB_EXTERN void
hb_draw_funcs_set_move_to_func (hb_draw_funcs_t        *dfuncs,
				hb_draw_move_to_func_t  func,
				void *user_data, hb_destroy_func_t destroy);

/**
 * hb_draw_funcs_set_line_to_func:
 * @dfuncs: draw functions object
 * @func: (closure user_data) (destroy destroy) (scope notified): line-to callback
 * @user_data: Data to pass to @func
 * @destroy: (nullable): The function to call when @user_data is not needed anymore
 *
 * Sets line-to callback to the draw functions object.
 *
 * Since: 4.0.0
 **/
HB_EXTERN void
hb_draw_funcs_set_line_to_func (hb_draw_funcs_t        *dfuncs,
				hb_draw_line_to_func_t  func,
				void *user_data, hb_destroy_func_t destroy);

/**
 * hb_draw_funcs_set_quadratic_to_func:
 * @dfuncs: draw functions object
 * @func: (closure user_data) (destroy destroy) (scope notified): quadratic-to callback
 * @user_data: Data to pass to @func
 * @destroy: (nullable): The function to call when @user_data is not needed anymore
 *
 * Sets quadratic-to callback to the draw functions object.
 *
 * Since: 4.0.0
 **/
HB_EXTERN void
hb_draw_funcs_set_quadratic_to_func (hb_draw_funcs_t             *dfuncs,
				     hb_draw_quadratic_to_func_t  func,
				     void *user_data, hb_destroy_func_t destroy);

/**
 * hb_draw_funcs_set_cubic_to_func:
 * @dfuncs: draw functions
 * @func: (closure user_data) (destroy destroy) (scope notified): cubic-to callback
 * @user_data: Data to pass to @func
 * @destroy: (nullable): The function to call when @user_data is not needed anymore
 *
 * Sets cubic-to callback to the draw functions object.
 *
 * Since: 4.0.0
 **/
HB_EXTERN void
hb_draw_funcs_set_cubic_to_func (hb_draw_funcs_t         *dfuncs,
				 hb_draw_cubic_to_func_t  func,
				 void *user_data, hb_destroy_func_t destroy);

/**
 * hb_draw_funcs_set_close_path_func:
 * @dfuncs: draw functions object
 * @func: (closure user_data) (destroy destroy) (scope notified): close-path callback
 * @user_data: Data to pass to @func
 * @destroy: (nullable): The function to call when @user_data is not needed anymore
 *
 * Sets close-path callback to the draw functions object.
 *
 * Since: 4.0.0
 **/
HB_EXTERN void
hb_draw_funcs_set_close_path_func (hb_draw_funcs_t           *dfuncs,
				   hb_draw_close_path_func_t  func,
				   void *user_data, hb_destroy_func_t destroy);


HB_EXTERN hb_draw_funcs_t *
hb_draw_funcs_create (void);

HB_EXTERN hb_draw_funcs_t *
hb_draw_funcs_get_empty (void);

HB_EXTERN hb_draw_funcs_t *
hb_draw_funcs_reference (hb_draw_funcs_t *dfuncs);

HB_EXTERN void
hb_draw_funcs_destroy (hb_draw_funcs_t *dfuncs);

HB_EXTERN hb_bool_t
hb_draw_funcs_set_user_data (hb_draw_funcs_t *dfuncs,
			     hb_user_data_key_t *key,
			     void *              data,
			     hb_destroy_func_t   destroy,
			     hb_bool_t           replace);


HB_EXTERN void *
hb_draw_funcs_get_user_data (const hb_draw_funcs_t *dfuncs,
			     hb_user_data_key_t       *key);

HB_EXTERN void
hb_draw_funcs_make_immutable (hb_draw_funcs_t *dfuncs);

HB_EXTERN hb_bool_t
hb_draw_funcs_is_immutable (hb_draw_funcs_t *dfuncs);


HB_EXTERN void
hb_draw_move_to (hb_draw_funcs_t *dfuncs, void *draw_data,
		 hb_draw_state_t *st,
		 float to_x, float to_y);

HB_EXTERN void
hb_draw_line_to (hb_draw_funcs_t *dfuncs, void *draw_data,
		 hb_draw_state_t *st,
		 float to_x, float to_y);

HB_EXTERN void
hb_draw_quadratic_to (hb_draw_funcs_t *dfuncs, void *draw_data,
		      hb_draw_state_t *st,
		      float control_x, float control_y,
		      float to_x, float to_y);

HB_EXTERN void
hb_draw_cubic_to (hb_draw_funcs_t *dfuncs, void *draw_data,
		  hb_draw_state_t *st,
		  float control1_x, float control1_y,
		  float control2_x, float control2_y,
		  float to_x, float to_y);

HB_EXTERN void
hb_draw_close_path (hb_draw_funcs_t *dfuncs, void *draw_data,
		    hb_draw_state_t *st);


HB_END_DECLS

#endif /* HB_DRAW_H */
