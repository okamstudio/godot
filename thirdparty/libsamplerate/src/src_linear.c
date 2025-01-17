/*
** Copyright (c) 2002-2021, Erik de Castro Lopo <erikd@mega-nerd.com>
** All rights reserved.
**
** This code is released under 2-clause BSD license. Please see the
** file at : https://github.com/libsndfile/libsamplerate/blob/master/COPYING
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "common.h"

static SRC_ERROR linear_vari_process (SRC_STATE *state, SRC_DATA *data) ;
static void linear_reset (SRC_STATE *state) ;
static SRC_STATE *linear_copy (SRC_STATE *state) ;
static void linear_close (SRC_STATE *state) ;

/*========================================================================================
*/

#define	LINEAR_MAGIC_MARKER	MAKE_MAGIC ('l', 'i', 'n', 'e', 'a', 'r')

#define	SRC_DEBUG	0

typedef struct
{	int		linear_magic_marker ;
	bool	dirty ;
	long	in_count, in_used ;
	long	out_count, out_gen ;
	float	*last_value ;
} LINEAR_DATA ;

static SRC_STATE_VT linear_state_vt =
{
	linear_vari_process,
	linear_vari_process,
	linear_reset,
	linear_copy,
	linear_close
} ;

/*----------------------------------------------------------------------------------------
*/

static SRC_ERROR
linear_vari_process (SRC_STATE *state, SRC_DATA *data)
{	LINEAR_DATA *priv ;
	double		src_ratio, input_index, rem ;
	int			ch ;

	if (data->input_frames <= 0)
		return SRC_ERR_NO_ERROR ;

	if (state->private_data == NULL)
		return SRC_ERR_NO_PRIVATE ;

	priv = (LINEAR_DATA*) state->private_data ;

	if (!priv->dirty)
	{	/* If we have just been reset, set the last_value data. */
		for (ch = 0 ; ch < state->channels ; ch++)
			priv->last_value [ch] = data->data_in [ch] ;
		priv->dirty = true ;
		} ;

	priv->in_count = data->input_frames * state->channels ;
	priv->out_count = data->output_frames * state->channels ;
	priv->in_used = priv->out_gen = 0 ;

	src_ratio = state->last_ratio ;

	if (is_bad_src_ratio (src_ratio))
		return SRC_ERR_BAD_INTERNAL_STATE ;

	input_index = state->last_position ;

	/* Calculate samples before first sample in input array. */
	while (input_index < 1.0 && priv->out_gen < priv->out_count)
	{
		if (priv->in_used + state->channels * (1.0 + input_index) >= priv->in_count)
			break ;

		if (priv->out_count > 0 && fabs (state->last_ratio - data->src_ratio) > SRC_MIN_RATIO_DIFF)
			src_ratio = state->last_ratio + priv->out_gen * (data->src_ratio - state->last_ratio) / priv->out_count ;

		for (ch = 0 ; ch < state->channels ; ch++)
		{	data->data_out [priv->out_gen] = (float) (priv->last_value [ch] + input_index *
										((double) data->data_in [ch] - priv->last_value [ch])) ;
			priv->out_gen ++ ;
			} ;

		/* Figure out the next index. */
		input_index += 1.0 / src_ratio ;
		} ;

	rem = fmod_one (input_index) ;
	priv->in_used += state->channels * lrint (input_index - rem) ;
	input_index = rem ;

	/* Main processing loop. */
	while (priv->out_gen < priv->out_count && priv->in_used + state->channels * input_index < priv->in_count)
	{
		if (priv->out_count > 0 && fabs (state->last_ratio - data->src_ratio) > SRC_MIN_RATIO_DIFF)
			src_ratio = state->last_ratio + priv->out_gen * (data->src_ratio - state->last_ratio) / priv->out_count ;

#if SRC_DEBUG
		if (priv->in_used < state->channels && input_index < 1.0)
		{	printf ("Whoops!!!!   in_used : %ld     channels : %d     input_index : %f\n", priv->in_used, state->channels, input_index) ;
			exit (1) ;
			} ;
#endif

		for (ch = 0 ; ch < state->channels ; ch++)
		{	data->data_out [priv->out_gen] = (float) (data->data_in [priv->in_used - state->channels + ch] + input_index *
						((double) data->data_in [priv->in_used + ch] - data->data_in [priv->in_used - state->channels + ch])) ;
			priv->out_gen ++ ;
			} ;

		/* Figure out the next index. */
		input_index += 1.0 / src_ratio ;
		rem = fmod_one (input_index) ;

		priv->in_used += state->channels * lrint (input_index - rem) ;
		input_index = rem ;
		} ;

	if (priv->in_used > priv->in_count)
	{	input_index += (priv->in_used - priv->in_count) / state->channels ;
		priv->in_used = priv->in_count ;
		} ;

	state->last_position = input_index ;

	if (priv->in_used > 0)
		for (ch = 0 ; ch < state->channels ; ch++)
			priv->last_value [ch] = data->data_in [priv->in_used - state->channels + ch] ;

	/* Save current ratio rather then target ratio. */
	state->last_ratio = src_ratio ;

	data->input_frames_used = priv->in_used / state->channels ;
	data->output_frames_gen = priv->out_gen / state->channels ;

	return SRC_ERR_NO_ERROR ;
} /* linear_vari_process */

/*------------------------------------------------------------------------------
*/

LIBSAMPLERATE_DLL_PRIVATE const char*
linear_get_name (int src_enum)
{
	if (src_enum == SRC_LINEAR)
		return "Linear Interpolator" ;

	return NULL ;
} /* linear_get_name */

LIBSAMPLERATE_DLL_PRIVATE const char*
linear_get_description (int src_enum)
{
	if (src_enum == SRC_LINEAR)
		return "Linear interpolator, very fast, poor quality." ;

	return NULL ;
} /* linear_get_descrition */

static LINEAR_DATA *
linear_data_new (int channels)
{
	assert (channels > 0) ;

	LINEAR_DATA *priv = (LINEAR_DATA *) calloc (1, sizeof (LINEAR_DATA)) ;
	if (priv)
	{
		priv->linear_magic_marker = LINEAR_MAGIC_MARKER ;
		priv->last_value = (float *) calloc (channels, sizeof (float)) ;
		if (!priv->last_value)
		{
			free (priv) ;
			priv = NULL ;
		}
	}

	return priv ;
}

LIBSAMPLERATE_DLL_PRIVATE SRC_STATE *
linear_state_new (int channels, SRC_ERROR *error)
{
	assert (channels > 0) ;
	assert (error != NULL) ;

	SRC_STATE *state = (SRC_STATE *) calloc (1, sizeof (SRC_STATE)) ;
	if (!state)
	{
		*error = SRC_ERR_MALLOC_FAILED ;
		return NULL ;
	}

	state->channels = channels ;
	state->mode = SRC_MODE_PROCESS ;

	state->private_data = linear_data_new (state->channels) ;
	if (!state->private_data)
	{
		free (state) ;
		*error = SRC_ERR_MALLOC_FAILED ;
		return NULL ;
	}

	state->vt = &linear_state_vt ;

	linear_reset (state) ;

	*error = SRC_ERR_NO_ERROR ;

	return state ;
}

/*===================================================================================
*/

static void
linear_reset (SRC_STATE *state)
{	LINEAR_DATA *priv = NULL ;

	priv = (LINEAR_DATA*) state->private_data ;
	if (priv == NULL)
		return ;

	priv->dirty = false ;
	memset (priv->last_value, 0, sizeof (priv->last_value [0]) * state->channels) ;

	return ;
} /* linear_reset */

SRC_STATE *
linear_copy (SRC_STATE *state)
{
	assert (state != NULL) ;

	if (state->private_data == NULL)
		return NULL ;

	SRC_STATE *to = (SRC_STATE *) calloc (1, sizeof (SRC_STATE)) ;
	if (!to)
		return NULL ;
	memcpy (to, state, sizeof (SRC_STATE)) ;

	LINEAR_DATA* from_priv = (LINEAR_DATA*) state->private_data ;
	LINEAR_DATA *to_priv = (LINEAR_DATA *) calloc (1, sizeof (LINEAR_DATA)) ;
	if (!to_priv)
	{
		free (to) ;
		return NULL ;
	}

	memcpy (to_priv, from_priv, sizeof (LINEAR_DATA)) ;
	to_priv->last_value = (float *) malloc (sizeof (float) * state->channels) ;
	if (!to_priv->last_value)
	{
		free (to) ;
		free (to_priv) ;
		return NULL ;
	}
	memcpy (to_priv->last_value, from_priv->last_value, sizeof (float) * state->channels) ;

	to->private_data = to_priv ;

	return to ;
} /* linear_copy */

static void
linear_close (SRC_STATE *state)
{
	if (state)
	{
		LINEAR_DATA *linear = (LINEAR_DATA *) state->private_data ;
		if (linear)
		{
			if (linear->last_value)
			{
				free (linear->last_value) ;
				linear->last_value = NULL ;
			}
			free (linear) ;
			linear = NULL ;
		}
		free (state) ;
		state = NULL ;
	}
} /* linear_close */
