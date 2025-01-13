/*

Copyright (c) 2023, Dominic Szablewski - https://phoboslab.org
SPDX-License-Identifier: MIT

QOA - The "Quite OK Audio" format for fast, lossy audio compression


-- Data Format

QOA encodes pulse-code modulated (PCM) audio data with up to 255 channels, 
sample rates from 1 up to 16777215 hertz and a bit depth of 16 bits.

The compression method employed in QOA is lossy; it discards some information
from the uncompressed PCM data. For many types of audio signals this compression
is "transparent", i.e. the difference from the original file is often not
audible.

QOA encodes 20 samples of 16 bit PCM data into slices of 64 bits. A single
sample therefore requires 3.2 bits of storage space, resulting in a 5x
compression (16 / 3.2).

A QOA file consists of an 8 byte file header, followed by a number of frames.
Each frame contains an 8 byte frame header, the current 16 byte en-/decoder
state per channel and 256 slices per channel. Each slice is 8 bytes wide and
encodes 20 samples of audio data.

All values, including the slices, are big endian. The file layout is as follows:

struct {
	struct {
		char     magic[4];         // magic bytes "qoaf"
		uint32_t samples;          // samples per channel in this file
	} file_header;             

	struct {
		struct {
			uint8_t  num_channels; // no. of channels
			uint24_t samplerate;   // samplerate in hz
			uint16_t fsamples;     // samples per channel in this frame
			uint16_t fsize;        // frame size (includes this header)
		} frame_header;          

		struct {
			int16_t history[4];    // most recent last
			int16_t weights[4];    // most recent last
		} lms_state[num_channels]; 

		qoa_slice_t slices[256][num_channels];

	} frames[ceil(samples / (256 * 20))];
} qoa_file_t;

Each `qoa_slice_t` contains a quantized scalefactor `sf_quant` and 20 quantized
residuals `qrNN`:

.- QOA_SLICE -- 64 bits, 20 samples --------------------------/  /------------.
|        Byte[0]         |        Byte[1]         |  Byte[2]  \  \  Byte[7]   |
| 7  6  5  4  3  2  1  0 | 7  6  5  4  3  2  1  0 | 7  6  5   /  /    2  1  0 |
|------------+--------+--------+--------+---------+---------+-\  \--+---------|
|  sf_quant  |  qr00  |  qr01  |  qr02  |  qr03   |  qr04   | /  /  |  qr19   |
`-------------------------------------------------------------\  \------------`

Each frame except the last must contain exactly 256 slices per channel. The last
frame may contain between 1 .. 256 (inclusive) slices per channel. The last
slice (for each channel) in the last frame may contain less than 20 samples; the
slice still must be 8 bytes wide, with the unused samples zeroed out.

Channels are interleaved per slice. E.g. for 2 channel stereo: 
slice[0] = L, slice[1] = R, slice[2] = L, slice[3] = R ...

A valid QOA file or stream must have at least one frame. Each frame must contain
at least one channel and one sample with a samplerate between 1 .. 16777215
(inclusive).

If the total number of samples is not known by the encoder, the samples in the
file header may be set to 0x00000000 to indicate that the encoder is 
"streaming". In a streaming context, the samplerate and number of channels may
differ from frame to frame. For static files (those with samples set to a
non-zero value), each frame must have the same number of channels and same
samplerate.

Note that this implementation of QOA only handles files with a known total
number of samples.

A decoder should support at least 8 channels. The channel layout for channel
counts 1 .. 8 is:

	1. Mono
	2. L, R
	3. L, R, C 
	4. FL, FR, B/SL, B/SR 
	5. FL, FR, C, B/SL, B/SR 
	6. FL, FR, C, LFE, B/SL, B/SR
	7. FL, FR, C, LFE, B, SL, SR 
	8. FL, FR, C, LFE, BL, BR, SL, SR

QOA predicts each audio sample based on the previously decoded ones using a
"Sign-Sign Least Mean Squares Filter" (LMS). This prediction plus the 
dequantized residual forms the final output sample.

*/



/* -----------------------------------------------------------------------------
	Header - Public functions */

#ifndef QOA_H
#define QOA_H

#ifdef __cplusplus
extern "C" {
#endif

#define QOA_MIN_FILESIZE 16
#define QOA_MAX_CHANNELS 8

#define QOA_SLICE_LEN 20
#define QOA_SLICES_PER_FRAME 256
#define QOA_FRAME_LEN (QOA_SLICES_PER_FRAME * QOA_SLICE_LEN)
#define QOA_LMS_LEN 4
#define QOA_MAGIC 0x716f6166 /* 'qoaf' */

#define QOA_FRAME_SIZE(channels, slices) \
	(8 + QOA_LMS_LEN * 4 * channels + 8 * slices * channels)

typedef struct {
	int history[QOA_LMS_LEN];
	int weights[QOA_LMS_LEN];
} qoa_lms_t;

typedef struct {
	unsigned int channels;
	unsigned int samplerate;
	unsigned int samples;
	qoa_lms_t lms[QOA_MAX_CHANNELS];
	#ifdef QOA_RECORD_TOTAL_ERROR
		double error;
	#endif
} qoa_desc;

unsigned int qoa_encode_header(qoa_desc *qoa, unsigned char *bytes);
unsigned int qoa_encode_frame(const short *sample_data, qoa_desc *qoa, unsigned int frame_len, unsigned char *bytes);
void *qoa_encode(const short *sample_data, qoa_desc *qoa, unsigned int *out_len);

unsigned int qoa_max_frame_size(qoa_desc *qoa);
unsigned int qoa_decode_header(const unsigned char *bytes, int size, qoa_desc *qoa);
unsigned int qoa_decode_frame(const unsigned char *bytes, unsigned int size, qoa_desc *qoa, short *sample_data, unsigned int *frame_len);
short *qoa_decode(const unsigned char *bytes, int size, qoa_desc *file);

#ifndef QOA_NO_STDIO

int qoa_write(const char *filename, const short *sample_data, qoa_desc *qoa);
void *qoa_read(const char *filename, qoa_desc *qoa);

#endif /* QOA_NO_STDIO */


#ifdef __cplusplus
}
#endif
#endif /* QOA_H */


/* -----------------------------------------------------------------------------
	Implementation */

#ifdef QOA_IMPLEMENTATION
#include <stdlib.h>

#ifndef QOA_MALLOC
	#define QOA_MALLOC(sz) malloc(sz)
	#define QOA_FREE(p) free(p)
#endif

typedef unsigned long long qoa_uint64_t;


/* The quant_tab provides an index into the dequant_tab for residuals in the
range of -8 .. 8. It maps this range to just 3bits and becomes less accurate at 
the higher end. Note that the residual zero is identical to the lowest positive 
value. This is mostly fine, since the qoa_div() function always rounds away 
from zero. */

static const int qoa_quant_tab[17] = {
	7, 7, 7, 5, 5, 3, 3, 1, /* -8..-1 */
	0,                      /*  0     */
	0, 2, 2, 4, 4, 6, 6, 6  /*  1.. 8 */
};


/* We have 16 different scalefactors. Like the quantized residuals these become
less accurate at the higher end. In theory, the highest scalefactor that we
would need to encode the highest 16bit residual is (2**16)/8 = 8192. However we
rely on the LMS filter to predict samples accurately enough that a maximum 
residual of one quarter of the 16 bit range is sufficient. I.e. with the 
scalefactor 2048 times the quant range of 8 we can encode residuals up to 2**14.

The scalefactor values are computed as:
scalefactor_tab[s] <- round(pow(s + 1, 2.75)) */

static const int qoa_scalefactor_tab[16] = {
	1, 7, 21, 45, 84, 138, 211, 304, 421, 562, 731, 928, 1157, 1419, 1715, 2048
};


/* The reciprocal_tab maps each of the 16 scalefactors to their rounded 
reciprocals 1/scalefactor. This allows us to calculate the scaled residuals in 
the encoder with just one multiplication instead of an expensive division. We 
do this in .16 fixed point with integers, instead of floats.

The reciprocal_tab is computed as:
reciprocal_tab[s] <- ((1<<16) + scalefactor_tab[s] - 1) / scalefactor_tab[s] */

static const int qoa_reciprocal_tab[16] = {
	65536, 9363, 3121, 1457, 781, 475, 311, 216, 156, 117, 90, 71, 57, 47, 39, 32
};


/* The dequant_tab maps each of the scalefactors and quantized residuals to 
their unscaled & dequantized version.

Since qoa_div rounds away from the zero, the smallest entries are mapped to 3/4
instead of 1. The dequant_tab assumes the following dequantized values for each 
of the quant_tab indices and is computed as:
float dqt[8] = {0.75, -0.75, 2.5, -2.5, 4.5, -4.5, 7, -7};
dequant_tab[s][q] <- round_ties_away_from_zero(scalefactor_tab[s] * dqt[q])

The rounding employed here is "to nearest, ties away from zero",  i.e. positive
and negative values are treated symmetrically.
*/

static const int qoa_dequant_tab[16][8] = {
	{   1,    -1,    3,    -3,    5,    -5,     7,     -7},
	{   5,    -5,   18,   -18,   32,   -32,    49,    -49},
	{  16,   -16,   53,   -53,   95,   -95,   147,   -147},
	{  34,   -34,  113,  -113,  203,  -203,   315,   -315},
	{  63,   -63,  210,  -210,  378,  -378,   588,   -588},
	{ 104,  -104,  345,  -345,  621,  -621,   966,   -966},
	{ 158,  -158,  528,  -528,  950,  -950,  1477,  -1477},
	{ 228,  -228,  760,  -760, 1368, -1368,  2128,  -2128},
	{ 316,  -316, 1053, -1053, 1895, -1895,  2947,  -2947},
	{ 422,  -422, 1405, -1405, 2529, -2529,  3934,  -3934},
	{ 548,  -548, 1828, -1828, 3290, -3290,  5117,  -5117},
	{ 696,  -696, 2320, -2320, 4176, -4176,  6496,  -6496},
	{ 868,  -868, 2893, -2893, 5207, -5207,  8099,  -8099},
	{1064, -1064, 3548, -3548, 6386, -6386,  9933,  -9933},
	{1286, -1286, 4288, -4288, 7718, -7718, 12005, -12005},
	{1536, -1536, 5120, -5120, 9216, -9216, 14336, -14336},
};


/* The Least Mean Squares Filter is the heart of QOA. It predicts the next
sample based on the previous 4 reconstructed samples. It does so by continuously
adjusting 4 weights based on the residual of the previous prediction.

The next sample is predicted as the sum of (weight[i] * history[i]).

The adjustment of the weights is done with a "Sign-Sign-LMS" that adds or
subtracts the residual to each weight, based on the corresponding sample from 
the history. This, surprisingly, is sufficient to get worthwhile predictions.

This is all done with fixed point integers. Hence the right-shifts when updating
the weights and calculating the prediction. */

static int qoa_lms_predict(qoa_lms_t *lms) {
	int prediction = 0;
	for (int i = 0; i < QOA_LMS_LEN; i++) {
		prediction += lms->weights[i] * lms->history[i];
	}
	return prediction >> 13;
}

static void qoa_lms_update(qoa_lms_t *lms, int sample, int residual) {
	int delta = residual >> 4;
	for (int i = 0; i < QOA_LMS_LEN; i++) {
		lms->weights[i] += lms->history[i] < 0 ? -delta : delta;
	}

	for (int i = 0; i < QOA_LMS_LEN-1; i++) {
		lms->history[i] = lms->history[i+1];
	}
	lms->history[QOA_LMS_LEN-1] = sample;
}


/* qoa_div() implements a rounding division, but avoids rounding to zero for 
small numbers. E.g. 0.1 will be rounded to 1. Note that 0 itself still 
returns as 0, which is handled in the qoa_quant_tab[].
qoa_div() takes an index into the .16 fixed point qoa_reciprocal_tab as an
argument, so it can do the division with a cheaper integer multiplication. */

static inline int qoa_div(int v, int scalefactor) {
	int reciprocal = qoa_reciprocal_tab[scalefactor];
	int n = (v * reciprocal + (1 << 15)) >> 16;
	n = n + ((v > 0) - (v < 0)) - ((n > 0) - (n < 0)); /* round away from 0 */
	return n;
}

static inline int qoa_clamp(int v, int min, int max) {
	if (v < min) { return min; }
	if (v > max) { return max; }
	return v;
}

/* This specialized clamp function for the signed 16 bit range improves decode
performance quite a bit. The extra if() statement works nicely with the CPUs
branch prediction as this branch is rarely taken. */

static inline int qoa_clamp_s16(int v) {
	if ((unsigned int)(v + 32768) > 65535) {
		if (v < -32768) { return -32768; }
		if (v >  32767) { return  32767; }
	}
	return v;
}

static inline qoa_uint64_t qoa_read_u64(const unsigned char *bytes, unsigned int *p) {
	bytes += *p;
	*p += 8;
	return 
		((qoa_uint64_t)(bytes[0]) << 56) | ((qoa_uint64_t)(bytes[1]) << 48) |
		((qoa_uint64_t)(bytes[2]) << 40) | ((qoa_uint64_t)(bytes[3]) << 32) |
		((qoa_uint64_t)(bytes[4]) << 24) | ((qoa_uint64_t)(bytes[5]) << 16) |
		((qoa_uint64_t)(bytes[6]) <<  8) | ((qoa_uint64_t)(bytes[7]) <<  0);
}

static inline void qoa_write_u64(qoa_uint64_t v, unsigned char *bytes, unsigned int *p) {
	bytes += *p;
	*p += 8;
	bytes[0] = (v >> 56) & 0xff;
	bytes[1] = (v >> 48) & 0xff;
	bytes[2] = (v >> 40) & 0xff;
	bytes[3] = (v >> 32) & 0xff;
	bytes[4] = (v >> 24) & 0xff;
	bytes[5] = (v >> 16) & 0xff;
	bytes[6] = (v >>  8) & 0xff;
	bytes[7] = (v >>  0) & 0xff;
}


/* -----------------------------------------------------------------------------
	Encoder */

unsigned int qoa_encode_header(qoa_desc *qoa, unsigned char *bytes) {
	unsigned int p = 0;
	qoa_write_u64(((qoa_uint64_t)QOA_MAGIC << 32) | qoa->samples, bytes, &p);
	return p;
}

unsigned int qoa_encode_frame(const short *sample_data, qoa_desc *qoa, unsigned int frame_len, unsigned char *bytes) {
	unsigned int channels = qoa->channels;

	unsigned int p = 0;
	unsigned int slices = (frame_len + QOA_SLICE_LEN - 1) / QOA_SLICE_LEN;
	unsigned int frame_size = QOA_FRAME_SIZE(channels, slices);
	int prev_scalefactor[QOA_MAX_CHANNELS] = {0};

	/* Write the frame header */
	qoa_write_u64((
		(qoa_uint64_t)qoa->channels   << 56 |
		(qoa_uint64_t)qoa->samplerate << 32 |
		(qoa_uint64_t)frame_len       << 16 |
		(qoa_uint64_t)frame_size
	), bytes, &p);

	
	for (unsigned int c = 0; c < channels; c++) {
		/* Write the current LMS state */
		qoa_uint64_t weights = 0;
		qoa_uint64_t history = 0;
		for (int i = 0; i < QOA_LMS_LEN; i++) {
			history = (history << 16) | (qoa->lms[c].history[i] & 0xffff);
			weights = (weights << 16) | (qoa->lms[c].weights[i] & 0xffff);
		}
		qoa_write_u64(history, bytes, &p);
		qoa_write_u64(weights, bytes, &p);
	}

	/* We encode all samples with the channels interleaved on a slice level.
	E.g. for stereo: (ch-0, slice 0), (ch 1, slice 0), (ch 0, slice 1), ...*/
	for (unsigned int sample_index = 0; sample_index < frame_len; sample_index += QOA_SLICE_LEN) {

		for (unsigned int c = 0; c < channels; c++) {
			int slice_len = qoa_clamp(QOA_SLICE_LEN, 0, frame_len - sample_index);
			int slice_start = sample_index * channels + c;
			int slice_end = (sample_index + slice_len) * channels + c;			

			/* Brute for search for the best scalefactor. Just go through all
			16 scalefactors, encode all samples for the current slice and 
			meassure the total squared error. */
			qoa_uint64_t best_rank = -1;
			#ifdef QOA_RECORD_TOTAL_ERROR
				qoa_uint64_t best_error = -1;
			#endif
			qoa_uint64_t best_slice = 0;
			qoa_lms_t best_lms;
			int best_scalefactor = 0;

			for (int sfi = 0; sfi < 16; sfi++) {
				/* There is a strong correlation between the scalefactors of
				neighboring slices. As an optimization, start testing
				the best scalefactor of the previous slice first. */
				int scalefactor = (sfi + prev_scalefactor[c]) % 16;

				/* We have to reset the LMS state to the last known good one
				before trying each scalefactor, as each pass updates the LMS
				state when encoding. */
				qoa_lms_t lms = qoa->lms[c];
				qoa_uint64_t slice = scalefactor;
				qoa_uint64_t current_rank = 0;
				#ifdef QOA_RECORD_TOTAL_ERROR
					qoa_uint64_t current_error = 0;
				#endif

				for (int si = slice_start; si < slice_end; si += channels) {
					int sample = sample_data[si];
					int predicted = qoa_lms_predict(&lms);

					int residual = sample - predicted;
					int scaled = qoa_div(residual, scalefactor);
					int clamped = qoa_clamp(scaled, -8, 8);
					int quantized = qoa_quant_tab[clamped + 8];
					int dequantized = qoa_dequant_tab[scalefactor][quantized];
					int reconstructed = qoa_clamp_s16(predicted + dequantized);


					/* If the weights have grown too large, we introduce a penalty
					here. This prevents pops/clicks in certain problem cases */
					int weights_penalty = ((
						lms.weights[0] * lms.weights[0] + 
						lms.weights[1] * lms.weights[1] + 
						lms.weights[2] * lms.weights[2] + 
						lms.weights[3] * lms.weights[3]
					) >> 18) - 0x8ff;
					if (weights_penalty < 0) {
						weights_penalty = 0;
					}

					long long error = (sample - reconstructed);
					qoa_uint64_t error_sq = error * error;

					current_rank += error_sq + weights_penalty * weights_penalty;
					#ifdef QOA_RECORD_TOTAL_ERROR
						current_error += error_sq;
					#endif
					if (current_rank > best_rank) {
						break;
					}

					qoa_lms_update(&lms, reconstructed, dequantized);
					slice = (slice << 3) | quantized;
				}

				if (current_rank < best_rank) {
					best_rank = current_rank;
					#ifdef QOA_RECORD_TOTAL_ERROR
						best_error = current_error;
					#endif
					best_slice = slice;
					best_lms = lms;
					best_scalefactor = scalefactor;
				}
			}

			prev_scalefactor[c] = best_scalefactor;

			qoa->lms[c] = best_lms;
			#ifdef QOA_RECORD_TOTAL_ERROR
				qoa->error += best_error;
			#endif

			/* If this slice was shorter than QOA_SLICE_LEN, we have to left-
			shift all encoded data, to ensure the rightmost bits are the empty
			ones. This should only happen in the last frame of a file as all
			slices are completely filled otherwise. */
			best_slice <<= (QOA_SLICE_LEN - slice_len) * 3;
			qoa_write_u64(best_slice, bytes, &p);
		}
	}
	
	return p;
}

void *qoa_encode(const short *sample_data, qoa_desc *qoa, unsigned int *out_len) {
	if (
		qoa->samples == 0 || 
		qoa->samplerate == 0 || qoa->samplerate > 0xffffff ||
		qoa->channels == 0 || qoa->channels > QOA_MAX_CHANNELS
	) {
		return NULL;
	}

	/* Calculate the encoded size and allocate */
	unsigned int num_frames = (qoa->samples + QOA_FRAME_LEN-1) / QOA_FRAME_LEN;
	unsigned int num_slices = (qoa->samples + QOA_SLICE_LEN-1) / QOA_SLICE_LEN;
	unsigned int encoded_size = 8 +                    /* 8 byte file header */
		num_frames * 8 +                               /* 8 byte frame headers */
		num_frames * QOA_LMS_LEN * 4 * qoa->channels + /* 4 * 4 bytes lms state per channel */
		num_slices * 8 * qoa->channels;                /* 8 byte slices */

	unsigned char *bytes = QOA_MALLOC(encoded_size);

	for (unsigned int c = 0; c < qoa->channels; c++) {
		/* Set the initial LMS weights to {0, 0, -1, 2}. This helps with the 
		prediction of the first few ms of a file. */
		qoa->lms[c].weights[0] = 0;
		qoa->lms[c].weights[1] = 0;
		qoa->lms[c].weights[2] = -(1<<13);
		qoa->lms[c].weights[3] =  (1<<14);

		/* Explicitly set the history samples to 0, as we might have some
		garbage in there. */
		for (int i = 0; i < QOA_LMS_LEN; i++) {
			qoa->lms[c].history[i] = 0;
		}
	}


	/* Encode the header and go through all frames */
	unsigned int p = qoa_encode_header(qoa, bytes);
	#ifdef QOA_RECORD_TOTAL_ERROR
		qoa->error = 0;
	#endif

	int frame_len = QOA_FRAME_LEN;
	for (unsigned int sample_index = 0; sample_index < qoa->samples; sample_index += frame_len) {
		frame_len = qoa_clamp(QOA_FRAME_LEN, 0, qoa->samples - sample_index);		
		const short *frame_samples = sample_data + sample_index * qoa->channels;
		unsigned int frame_size = qoa_encode_frame(frame_samples, qoa, frame_len, bytes + p);
		p += frame_size;
	}

	*out_len = p;
	return bytes;
}



/* -----------------------------------------------------------------------------
	Decoder */

unsigned int qoa_max_frame_size(qoa_desc *qoa) {
	return QOA_FRAME_SIZE(qoa->channels, QOA_SLICES_PER_FRAME);
}

unsigned int qoa_decode_header(const unsigned char *bytes, int size, qoa_desc *qoa) {
	unsigned int p = 0;
	if (size < QOA_MIN_FILESIZE) {
		return 0;
	}


	/* Read the file header, verify the magic number ('qoaf') and read the 
	total number of samples. */
	qoa_uint64_t file_header = qoa_read_u64(bytes, &p);

	if ((file_header >> 32) != QOA_MAGIC) {
		return 0;
	}

	qoa->samples = file_header & 0xffffffff;
	if (!qoa->samples) {
		return 0;
	}

	/* Peek into the first frame header to get the number of channels and
	the samplerate. */
	qoa_uint64_t frame_header = qoa_read_u64(bytes, &p);
	qoa->channels   = (frame_header >> 56) & 0x0000ff;
	qoa->samplerate = (frame_header >> 32) & 0xffffff;

	if (qoa->channels == 0 || qoa->samples == 0 || qoa->samplerate == 0) {
		return 0;
	}

	return 8;
}

unsigned int qoa_decode_frame(const unsigned char *bytes, unsigned int size, qoa_desc *qoa, short *sample_data, unsigned int *frame_len) {
	unsigned int p = 0;
	*frame_len = 0;

	if (size < 8 + QOA_LMS_LEN * 4 * qoa->channels) {
		return 0;
	}

	/* Read and verify the frame header */
	qoa_uint64_t frame_header = qoa_read_u64(bytes, &p);
	unsigned int channels   = (frame_header >> 56) & 0x0000ff;
	unsigned int samplerate = (frame_header >> 32) & 0xffffff;
	unsigned int samples    = (frame_header >> 16) & 0x00ffff;
	unsigned int frame_size = (frame_header      ) & 0x00ffff;

	unsigned int data_size = frame_size - 8 - QOA_LMS_LEN * 4 * channels;
	unsigned int num_slices = data_size / 8;
	unsigned int max_total_samples = num_slices * QOA_SLICE_LEN;

	if (
		channels != qoa->channels || 
		samplerate != qoa->samplerate ||
		frame_size > size ||
		samples * channels > max_total_samples
	) {
		return 0;
	}


	/* Read the LMS state: 4 x 2 bytes history, 4 x 2 bytes weights per channel */
	for (unsigned int c = 0; c < channels; c++) {
		qoa_uint64_t history = qoa_read_u64(bytes, &p);
		qoa_uint64_t weights = qoa_read_u64(bytes, &p);
		for (int i = 0; i < QOA_LMS_LEN; i++) {
			qoa->lms[c].history[i] = ((signed short)(history >> 48));
			history <<= 16;
			qoa->lms[c].weights[i] = ((signed short)(weights >> 48));
			weights <<= 16;
		}
	}


	/* Decode all slices for all channels in this frame */
	for (unsigned int sample_index = 0; sample_index < samples; sample_index += QOA_SLICE_LEN) {
		for (unsigned int c = 0; c < channels; c++) {
			qoa_uint64_t slice = qoa_read_u64(bytes, &p);

			int scalefactor = (slice >> 60) & 0xf;
			slice <<= 4;

			int slice_start = sample_index * channels + c;
			int slice_end = qoa_clamp(sample_index + QOA_SLICE_LEN, 0, samples) * channels + c;

			for (int si = slice_start; si < slice_end; si += channels) {
				int predicted = qoa_lms_predict(&qoa->lms[c]);
				int quantized = (slice >> 61) & 0x7;
				int dequantized = qoa_dequant_tab[scalefactor][quantized];
				int reconstructed = qoa_clamp_s16(predicted + dequantized);
				
				sample_data[si] = reconstructed;
				slice <<= 3;

				qoa_lms_update(&qoa->lms[c], reconstructed, dequantized);
			}
		}
	}

	*frame_len = samples;
	return p;
}

short *qoa_decode(const unsigned char *bytes, int size, qoa_desc *qoa) {
	unsigned int p = qoa_decode_header(bytes, size, qoa);
	if (!p) {
		return NULL;
	}

	/* Calculate the required size of the sample buffer and allocate */
	int total_samples = qoa->samples * qoa->channels;
	short *sample_data = QOA_MALLOC(total_samples * sizeof(short));

	unsigned int sample_index = 0;
	unsigned int frame_len;
	unsigned int frame_size;

	/* Decode all frames */
	do {
		short *sample_ptr = sample_data + sample_index * qoa->channels;
		frame_size = qoa_decode_frame(bytes + p, size - p, qoa, sample_ptr, &frame_len);

		p += frame_size;
		sample_index += frame_len;
	} while (frame_size && sample_index < qoa->samples);

	qoa->samples = sample_index;
	return sample_data;
}



/* -----------------------------------------------------------------------------
	File read/write convenience functions */

#ifndef QOA_NO_STDIO
#include <stdio.h>

int qoa_write(const char *filename, const short *sample_data, qoa_desc *qoa) {
	FILE *f = fopen(filename, "wb");
	unsigned int size;
	void *encoded;

	if (!f) {
		return 0;
	}

	encoded = qoa_encode(sample_data, qoa, &size);
	if (!encoded) {
		fclose(f);
		return 0;
	}

	fwrite(encoded, 1, size, f);
	fclose(f);

	QOA_FREE(encoded);
	return size;
}

void *qoa_read(const char *filename, qoa_desc *qoa) {
	FILE *f = fopen(filename, "rb");
	int size, bytes_read;
	void *data;
	short *sample_data;

	if (!f) {
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	size = ftell(f);
	if (size <= 0) {
		fclose(f);
		return NULL;
	}
	fseek(f, 0, SEEK_SET);

	data = QOA_MALLOC(size);
	if (!data) {
		fclose(f);
		return NULL;
	}

	bytes_read = fread(data, 1, size, f);
	fclose(f);

	sample_data = qoa_decode(data, bytes_read, qoa);
	QOA_FREE(data);
	return sample_data;
}

#endif /* QOA_NO_STDIO */
#endif /* QOA_IMPLEMENTATION */
