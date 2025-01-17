/*
** Copyright (c) 2002-2016, Erik de Castro Lopo <erikd@mega-nerd.com>
** All rights reserved.
**
** This code is released under 2-clause BSD license. Please see the
** file at : https://github.com/libsndfile/libsamplerate/blob/master/COPYING
*/

#ifdef HAVE_CONFIG_H
#include	"config.h"
#endif

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<math.h>

#include	"samplerate.h"
#include	"common.h"

static SRC_STATE *psrc_set_converter (int converter_type, int channels, int *error) ;


SRC_STATE *
src_new (int converter_type, int channels, int *error)
{
	return psrc_set_converter (converter_type, channels, error) ;
} /* src_new */

SRC_STATE*
src_clone (SRC_STATE* orig, int *error)
{
	if (!orig)
	{
		if (error)
			*error = SRC_ERR_BAD_STATE ;
		return NULL ;
	}
	if (error)
		*error = SRC_ERR_NO_ERROR ;

	SRC_STATE *state = orig->vt->copy (orig) ;
	if (!state)
		if (error)
			*error = SRC_ERR_MALLOC_FAILED ;

	return state ;
}

SRC_STATE*
src_callback_new (src_callback_t func, int converter_type, int channels, int *error, void* cb_data)
{	SRC_STATE	*state ;

	if (func == NULL)
	{	if (error)
			*error = SRC_ERR_BAD_CALLBACK ;
		return NULL ;
		} ;

	if (error != NULL)
		*error = 0 ;

	if ((state = src_new (converter_type, channels, error)) == NULL)
		return NULL ;

	src_reset (state) ;

	state->mode = SRC_MODE_CALLBACK ;
	state->callback_func = func ;
	state->user_callback_data = cb_data ;

	return state ;
} /* src_callback_new */

SRC_STATE *
src_delete (SRC_STATE *state)
{
	if (state)
		state->vt->close (state) ;

	return NULL ;
} /* src_state */

int
src_process (SRC_STATE *state, SRC_DATA *data)
{
	int error ;

	if (state == NULL)
		return SRC_ERR_BAD_STATE ;

	if (state->mode != SRC_MODE_PROCESS)
		return SRC_ERR_BAD_MODE ;

	/* Check for valid SRC_DATA first. */
	if (data == NULL)
		return SRC_ERR_BAD_DATA ;

	/* And that data_in and data_out are valid. */
	if ((data->data_in == NULL && data->input_frames > 0)
			|| (data->data_out == NULL && data->output_frames > 0))
		return SRC_ERR_BAD_DATA_PTR ;

	/* Check src_ratio is in range. */
	if (is_bad_src_ratio (data->src_ratio))
		return SRC_ERR_BAD_SRC_RATIO ;

	if (data->input_frames < 0)
		data->input_frames = 0 ;
	if (data->output_frames < 0)
		data->output_frames = 0 ;

	if (data->data_in < data->data_out)
	{	if (data->data_in + data->input_frames * state->channels > data->data_out)
		{	/*-printf ("\n\ndata_in: %p    data_out: %p\n",
				(void*) (data->data_in + data->input_frames * psrc->channels), (void*) data->data_out) ;-*/
			return SRC_ERR_DATA_OVERLAP ;
			} ;
		}
	else if (data->data_out + data->output_frames * state->channels > data->data_in)
	{	/*-printf ("\n\ndata_in : %p   ouput frames: %ld    data_out: %p\n", (void*) data->data_in, data->output_frames, (void*) data->data_out) ;

		printf ("data_out: %p (%p)    data_in: %p\n", (void*) data->data_out,
			(void*) (data->data_out + data->input_frames * psrc->channels), (void*) data->data_in) ;-*/
		return SRC_ERR_DATA_OVERLAP ;
		} ;

	/* Set the input and output counts to zero. */
	data->input_frames_used = 0 ;
	data->output_frames_gen = 0 ;

	/* Special case for when last_ratio has not been set. */
	if (state->last_ratio < (1.0 / SRC_MAX_RATIO))
		state->last_ratio = data->src_ratio ;

	/* Now process. */
	if (fabs (state->last_ratio - data->src_ratio) < 1e-15)
		error = state->vt->const_process (state, data) ;
	else
		error = state->vt->vari_process (state, data) ;

	return error ;
} /* src_process */

long
src_callback_read (SRC_STATE *state, double src_ratio, long frames, float *data)
{
	SRC_DATA	src_data ;

	long	output_frames_gen ;
	int		error = 0 ;

	if (state == NULL)
		return 0 ;

	if (frames <= 0)
		return 0 ;

	if (state->mode != SRC_MODE_CALLBACK)
	{	state->error = SRC_ERR_BAD_MODE ;
		return 0 ;
		} ;

	if (state->callback_func == NULL)
	{	state->error = SRC_ERR_NULL_CALLBACK ;
		return 0 ;
		} ;

	memset (&src_data, 0, sizeof (src_data)) ;

	/* Check src_ratio is in range. */
	if (is_bad_src_ratio (src_ratio))
	{	state->error = SRC_ERR_BAD_SRC_RATIO ;
		return 0 ;
		} ;

	/* Switch modes temporarily. */
	src_data.src_ratio = src_ratio ;
	src_data.data_out = data ;
	src_data.output_frames = frames ;

	src_data.data_in = state->saved_data ;
	src_data.input_frames = state->saved_frames ;

	output_frames_gen = 0 ;
	while (output_frames_gen < frames)
	{	/*	Use a dummy array for the case where the callback function
		**	returns without setting the ptr.
		*/
		float dummy [1] ;

		if (src_data.input_frames == 0)
		{	float *ptr = dummy ;

			src_data.input_frames = state->callback_func (state->user_callback_data, &ptr) ;
			src_data.data_in = ptr ;

			if (src_data.input_frames == 0)
				src_data.end_of_input = 1 ;
			} ;

		/*
		** Now call process function. However, we need to set the mode
		** to SRC_MODE_PROCESS first and when we return set it back to
		** SRC_MODE_CALLBACK.
		*/
		state->mode = SRC_MODE_PROCESS ;
		error = src_process (state, &src_data) ;
		state->mode = SRC_MODE_CALLBACK ;

		if (error != 0)
			break ;

		src_data.data_in += src_data.input_frames_used * state->channels ;
		src_data.input_frames -= src_data.input_frames_used ;

		src_data.data_out += src_data.output_frames_gen * state->channels ;
		src_data.output_frames -= src_data.output_frames_gen ;

		output_frames_gen += src_data.output_frames_gen ;

		if (src_data.end_of_input == SRC_TRUE && src_data.output_frames_gen == 0)
			break ;
		} ;

	state->saved_data = src_data.data_in ;
	state->saved_frames = src_data.input_frames ;

	if (error != 0)
	{	state->error = (SRC_ERROR) error ;
		return 0 ;
		} ;

	return output_frames_gen ;
} /* src_callback_read */

/*==========================================================================
*/

int
src_set_ratio (SRC_STATE *state, double new_ratio)
{
	if (state == NULL)
		return SRC_ERR_BAD_STATE ;

	if (is_bad_src_ratio (new_ratio))
		return SRC_ERR_BAD_SRC_RATIO ;

	state->last_ratio = new_ratio ;

	return SRC_ERR_NO_ERROR ;
} /* src_set_ratio */

int
src_get_channels (SRC_STATE *state)
{
	if (state == NULL)
		return -SRC_ERR_BAD_STATE ;

	return state->channels ;
} /* src_get_channels */

int
src_reset (SRC_STATE *state)
{
	if (state == NULL)
		return SRC_ERR_BAD_STATE ;

	state->vt->reset (state) ;

	state->last_position = 0.0 ;
	state->last_ratio = 0.0 ;

	state->saved_data = NULL ;
	state->saved_frames = 0 ;

	state->error = SRC_ERR_NO_ERROR ;

	return SRC_ERR_NO_ERROR ;
} /* src_reset */

/*==============================================================================
**	Control functions.
*/

const char *
src_get_name (int converter_type)
{	const char *desc ;

	if ((desc = sinc_get_name (converter_type)) != NULL)
		return desc ;

	if ((desc = zoh_get_name (converter_type)) != NULL)
		return desc ;

	if ((desc = linear_get_name (converter_type)) != NULL)
		return desc ;

	return NULL ;
} /* src_get_name */

const char *
src_get_description (int converter_type)
{	const char *desc ;

	if ((desc = sinc_get_description (converter_type)) != NULL)
		return desc ;

	if ((desc = zoh_get_description (converter_type)) != NULL)
		return desc ;

	if ((desc = linear_get_description (converter_type)) != NULL)
		return desc ;

	return NULL ;
} /* src_get_description */

const char *
src_get_version (void)
{	return PACKAGE "-" VERSION " (c) 2002-2008 Erik de Castro Lopo" ;
} /* src_get_version */

int
src_is_valid_ratio (double ratio)
{
	if (is_bad_src_ratio (ratio))
		return SRC_FALSE ;

	return SRC_TRUE ;
} /* src_is_valid_ratio */

/*==============================================================================
**	Error reporting functions.
*/

int
src_error (SRC_STATE *state)
{	if (state)
		return state->error ;
	return SRC_ERR_NO_ERROR ;
} /* src_error */

const char*
src_strerror (int error)
{
	switch (error)
	{	case SRC_ERR_NO_ERROR :
				return "No error." ;
		case SRC_ERR_MALLOC_FAILED :
				return "Malloc failed." ;
		case SRC_ERR_BAD_STATE :
				return "SRC_STATE pointer is NULL." ;
		case SRC_ERR_BAD_DATA :
				return "SRC_DATA pointer is NULL." ;
		case SRC_ERR_BAD_DATA_PTR :
				return "SRC_DATA->data_out or SRC_DATA->data_in is NULL." ;
		case SRC_ERR_NO_PRIVATE :
				return "Internal error. No private data." ;

		case SRC_ERR_BAD_SRC_RATIO :
				return "SRC ratio outside [1/" SRC_MAX_RATIO_STR ", " SRC_MAX_RATIO_STR "] range." ;

		case SRC_ERR_BAD_SINC_STATE :
				return "src_process() called without reset after end_of_input." ;
		case SRC_ERR_BAD_PROC_PTR :
				return "Internal error. No process pointer." ;
		case SRC_ERR_SHIFT_BITS :
				return "Internal error. SHIFT_BITS too large." ;
		case SRC_ERR_FILTER_LEN :
				return "Internal error. Filter length too large." ;
		case SRC_ERR_BAD_CONVERTER :
				return "Bad converter number." ;
		case SRC_ERR_BAD_CHANNEL_COUNT :
				return "Channel count must be >= 1." ;
		case SRC_ERR_SINC_BAD_BUFFER_LEN :
				return "Internal error. Bad buffer length. Please report this." ;
		case SRC_ERR_SIZE_INCOMPATIBILITY :
				return "Internal error. Input data / internal buffer size difference. Please report this." ;
		case SRC_ERR_BAD_PRIV_PTR :
				return "Internal error. Private pointer is NULL. Please report this." ;
		case SRC_ERR_DATA_OVERLAP :
				return "Input and output data arrays overlap." ;
		case SRC_ERR_BAD_CALLBACK :
				return "Supplied callback function pointer is NULL." ;
		case SRC_ERR_BAD_MODE :
				return "Calling mode differs from initialisation mode (ie process v callback)." ;
		case SRC_ERR_NULL_CALLBACK :
				return "Callback function pointer is NULL in src_callback_read ()." ;
		case SRC_ERR_NO_VARIABLE_RATIO :
				return "This converter only allows constant conversion ratios." ;
		case SRC_ERR_SINC_PREPARE_DATA_BAD_LEN :
				return "Internal error : Bad length in prepare_data ()." ;
		case SRC_ERR_BAD_INTERNAL_STATE :
				return "Error : Someone is trampling on my internal state." ;

		case SRC_ERR_MAX_ERROR :
				return "Placeholder. No error defined for this error number." ;

		default : 						break ;
		}

	return NULL ;
} /* src_strerror */

/*==============================================================================
**	Simple interface for performing a single conversion from input buffer to
**	output buffer at a fixed conversion ratio.
*/

int
src_simple (SRC_DATA *src_data, int converter, int channels)
{	SRC_STATE	*src_state ;
	int 		error ;

	if ((src_state = src_new (converter, channels, &error)) == NULL)
		return error ;

	src_data->end_of_input = 1 ; /* Only one buffer worth of input. */

	error = src_process (src_state, src_data) ;

	src_delete (src_state) ;

	return error ;
} /* src_simple */

void
src_short_to_float_array (const short *in, float *out, int len)
{
	for (int i = 0 ; i < len ; i++)
	{	out [i] = (float) (in [i] / (1.0 * 0x8000)) ;
		} ;

	return ;
} /* src_short_to_float_array */

void
src_float_to_short_array (const float *in, short *out, int len)
{
	for (int i = 0 ; i < len ; i++)
	{	float scaled_value ;
		scaled_value = in [i] * 32768.f ;
		if (scaled_value >= 32767.f)
			out [i] = 32767 ;
		else if (scaled_value <= -32768.f)
			out [i] = -32768 ;
		else
			out [i] = (short) (lrintf (scaled_value)) ;
	}
} /* src_float_to_short_array */

void
src_int_to_float_array (const int *in, float *out, int len)
{
	for (int i = 0 ; i < len ; i++)
	{	out [i] = (float) (in [i] / (8.0 * 0x10000000)) ;
		} ;

	return ;
} /* src_int_to_float_array */

void
src_float_to_int_array (const float *in, int *out, int len)
{	double scaled_value ;

	for (int i = 0 ; i < len ; i++)
	{	scaled_value = in [i] * (8.0 * 0x10000000) ;
#if CPU_CLIPS_POSITIVE == 0
		if (scaled_value >= (1.0 * 0x7FFFFFFF))
		{	out [i] = 0x7fffffff ;
			continue ;
			} ;
#endif
#if CPU_CLIPS_NEGATIVE == 0
		if (scaled_value <= (-8.0 * 0x10000000))
		{	out [i] = -1 - 0x7fffffff ;
			continue ;
			} ;
#endif
		out [i] = (int) lrint (scaled_value) ;
		} ;

} /* src_float_to_int_array */

/*==============================================================================
**	Private functions.
*/

static SRC_STATE *
psrc_set_converter (int converter_type, int channels, int *error)
{
	SRC_ERROR temp_error;
	SRC_STATE *state ;
	switch (converter_type)
	{
#ifdef ENABLE_SINC_BEST_CONVERTER
	case SRC_SINC_BEST_QUALITY :
		state = sinc_state_new (converter_type, channels, &temp_error) ;
		break ;
#endif
#ifdef ENABLE_SINC_MEDIUM_CONVERTER
	case SRC_SINC_MEDIUM_QUALITY :
		state = sinc_state_new (converter_type, channels, &temp_error) ;
		break ;
#endif
#ifdef ENABLE_SINC_FAST_CONVERTER
	case SRC_SINC_FASTEST :
		state = sinc_state_new (converter_type, channels, &temp_error) ;
		break ;
#endif
	case SRC_ZERO_ORDER_HOLD :
		state = zoh_state_new (channels, &temp_error) ;
		break ;
	case SRC_LINEAR :
		state = linear_state_new (channels, &temp_error) ;
		break ;
	default :
		temp_error = SRC_ERR_BAD_CONVERTER ;
		state = NULL ;
		break ;
	}

	if (error)
		*error = (int) temp_error ;

	return state ;
} /* psrc_set_converter */

