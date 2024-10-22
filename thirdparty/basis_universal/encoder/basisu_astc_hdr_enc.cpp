// basisu_astc_hdr_enc.cpp
#include "basisu_astc_hdr_enc.h"
#include "../transcoder/basisu_transcoder.h"

using namespace basist;

namespace basisu
{

const float DEF_R_ERROR_SCALE = 2.0f;
const float DEF_G_ERROR_SCALE = 3.0f;

static inline uint32_t get_max_qlog(uint32_t bits)
{
	switch (bits)
	{
	case 7: return MAX_QLOG7;
	case 8: return MAX_QLOG8;
	case 9: return MAX_QLOG9;
	case 10: return MAX_QLOG10;
	case 11: return MAX_QLOG11;
	case 12: return MAX_QLOG12;
	case 16: return MAX_QLOG16;
	default: assert(0); break;
	}
	return 0;
}

#if 0
static inline float get_max_qlog_val(uint32_t bits)
{
	switch (bits)
	{
	case 7: return MAX_QLOG7_VAL;
	case 8: return MAX_QLOG8_VAL;
	case 9: return MAX_QLOG9_VAL;
	case 10: return MAX_QLOG10_VAL;
	case 11: return MAX_QLOG11_VAL;
	case 12: return MAX_QLOG12_VAL;
	case 16: return MAX_QLOG16_VAL;
	default: assert(0); break;
	}
	return 0;
}
#endif

static inline int get_bit(
	int src_val, int src_bit)
{
	assert(src_bit >= 0 && src_bit <= 31);
	int bit = (src_val >> src_bit) & 1;
	return bit;
}

static inline void pack_bit(
	int& dst, int dst_bit,
	int src_val, int src_bit = 0)
{
	assert(dst_bit >= 0 && dst_bit <= 31);
	int bit = get_bit(src_val, src_bit);
	dst |= (bit << dst_bit);
}

//--------------------------------------------------------------------------------------------------------------------------

astc_hdr_codec_options::astc_hdr_codec_options()
{
	init();
}

void astc_hdr_codec_options::init()
{
	m_bc6h_err_weight = .85f;
	m_r_err_scale = DEF_R_ERROR_SCALE;
	m_g_err_scale = DEF_G_ERROR_SCALE;

	// Disabling by default to avoid transcoding outliers (try kodim26). The quality lost is very low. TODO: Could include the uber result in the output.
	m_allow_uber_mode = false;

	// Must set best quality level first to set defaults.
	set_quality_best();

	set_quality_level(cDefaultLevel);
}

void astc_hdr_codec_options::set_quality_best()
{
	m_mode11_direct_only = false;
		
	// highest achievable quality
	m_use_solid = true;

	m_use_mode11 = true;
	m_mode11_uber_mode = true;
	m_first_mode11_weight_ise_range = MODE11_FIRST_ISE_RANGE;
	m_last_mode11_weight_ise_range = MODE11_LAST_ISE_RANGE;
	m_first_mode11_submode = -1;
	m_last_mode11_submode = 7;

	m_use_mode7_part1 = true;
	m_first_mode7_part1_weight_ise_range = MODE7_PART1_FIRST_ISE_RANGE;
	m_last_mode7_part1_weight_ise_range = MODE7_PART1_LAST_ISE_RANGE;

	m_use_mode7_part2 = true;
	m_mode7_part2_part_masks = UINT32_MAX;
	m_first_mode7_part2_weight_ise_range = MODE7_PART2_FIRST_ISE_RANGE;
	m_last_mode7_part2_weight_ise_range = MODE7_PART2_LAST_ISE_RANGE;

	m_use_mode11_part2 = true;
	m_mode11_part2_part_masks = UINT32_MAX;
	m_first_mode11_part2_weight_ise_range = MODE11_PART2_FIRST_ISE_RANGE;
	m_last_mode11_part2_weight_ise_range = MODE11_PART2_LAST_ISE_RANGE;

	m_refine_weights = true;

	m_use_estimated_partitions = false;
	m_max_estimated_partitions = 0;
}

void astc_hdr_codec_options::set_quality_normal()
{
	m_use_solid = true;

	// We'll allow uber mode in normal if the user allows it.
	m_use_mode11 = true;
	m_mode11_uber_mode = true;
	m_first_mode11_weight_ise_range = 6;
	m_last_mode11_weight_ise_range = MODE11_LAST_ISE_RANGE;

	m_use_mode7_part1 = true;
	m_first_mode7_part1_weight_ise_range = MODE7_PART1_LAST_ISE_RANGE;
	m_last_mode7_part1_weight_ise_range = MODE7_PART1_LAST_ISE_RANGE;

	m_use_mode7_part2 = true;
	m_mode7_part2_part_masks = UINT32_MAX;
	m_first_mode7_part2_weight_ise_range = MODE7_PART2_LAST_ISE_RANGE;
	m_last_mode7_part2_weight_ise_range = MODE7_PART2_LAST_ISE_RANGE;

	m_use_mode11_part2 = true;
	m_mode11_part2_part_masks = UINT32_MAX;
	m_first_mode11_part2_weight_ise_range = MODE11_PART2_LAST_ISE_RANGE;
	m_last_mode11_part2_weight_ise_range = MODE11_PART2_LAST_ISE_RANGE;

	m_refine_weights = true;
}

void astc_hdr_codec_options::set_quality_fastest()
{
	m_use_solid = true;

	m_use_mode11 = true;
	m_mode11_uber_mode = false;
	m_first_mode11_weight_ise_range = MODE11_LAST_ISE_RANGE;
	m_last_mode11_weight_ise_range = MODE11_LAST_ISE_RANGE;

	m_use_mode7_part1 = false;
	m_use_mode7_part2 = false;
	m_use_mode11_part2 = false;

	m_refine_weights = false;
}

//--------------------------------------------------------------------------------------------------------------------------

void astc_hdr_codec_options::set_quality_level(int level)
{
	level = clamp(level, cMinLevel, cMaxLevel);
	
	m_level = level;

	switch (level)
	{
	case 0:
	{
		set_quality_fastest();
		break;
	}
	case 1:
	{
		set_quality_normal();

		m_first_mode11_weight_ise_range = MODE11_LAST_ISE_RANGE - 1;
		m_last_mode11_weight_ise_range = MODE11_LAST_ISE_RANGE;

		m_use_mode7_part1 = false;
		m_use_mode7_part2 = false;

		m_use_estimated_partitions = true;
		m_max_estimated_partitions = 1;

		m_mode11_part2_part_masks = 1 | 2;
		m_mode7_part2_part_masks = 1 | 2;
		break;
	}
	case 2:
	{
		set_quality_normal();

		m_use_estimated_partitions = true;
		m_max_estimated_partitions = 2;

		m_mode11_part2_part_masks = 1 | 2;
		m_mode7_part2_part_masks = 1 | 2;

		break;
	}
	case 3:
	{
		set_quality_best();

		m_use_estimated_partitions = true;
		m_max_estimated_partitions = 2;

		m_mode11_part2_part_masks = 1 | 2 | 4 | 8;
		m_mode7_part2_part_masks = 1 | 2 | 4 | 8;

		break;
	}
	case 4:
	{
		set_quality_best();

		break;
	}
	}
}

//--------------------------------------------------------------------------------------------------------------------------

#if 0
static inline half_float qlog12_to_half_slow(uint32_t qlog12)
{
	return qlog_to_half_slow(qlog12, 12);
}
#endif

// max usable qlog8 value is 247, 248=inf, >=249 is nan
// max usable qlog7 value is 123, 124=inf, >=125 is nan

// To go from a smaller qlog to an larger one, shift left by X bits.

//const uint32_t TOTAL_USABLE_QLOG8 = 248; // 0-247 are usable, 0=0, 247=60416.0, 246=55296.0

// for qlog7's shift left by 1
//half_float g_qlog8_to_half[256];
//float g_qlog8_to_float[256];

//half_float g_qlog12_to_half[4096];
//float g_qlog12_to_float[4096];

static half_float g_qlog16_to_half[65536];

inline half_float qlog_to_half(uint32_t val, uint32_t bits)
{
	assert((bits >= 5) && (bits <= 16));
	assert(val < (1U << bits));
	return g_qlog16_to_half[val << (16 - bits)];
}

// nearest values given a positive half float value (only)
static uint16_t g_half_to_qlog7[32768], g_half_to_qlog8[32768], g_half_to_qlog9[32768], g_half_to_qlog10[32768], g_half_to_qlog11[32768], g_half_to_qlog12[32768];

const uint32_t HALF_TO_QLOG_TABS_BASE = 7;
static uint16_t* g_pHalf_to_qlog_tabs[8] =
{
	g_half_to_qlog7,
	g_half_to_qlog8,

	g_half_to_qlog9,
	g_half_to_qlog10,

	g_half_to_qlog11,
	g_half_to_qlog12
};

static inline uint32_t half_to_qlog7_12(half_float h, uint32_t bits)
{
	assert((bits >= HALF_TO_QLOG_TABS_BASE) && (bits <= 12));
	assert(h < 32768);

	return g_pHalf_to_qlog_tabs[bits - HALF_TO_QLOG_TABS_BASE][h];
}

#if 0
// Input is the low 11 bits of the qlog
// Returns the 10-bit mantissa of the half float value
static int qlog11_to_half_float_mantissa(int M)
{
	assert(M <= 0x7FF);
	int Mt;
	if (M < 512)
		Mt = 3 * M;
	else if (M >= 1536)
		Mt = 5 * M - 2048;
	else
		Mt = 4 * M - 512;
	return (Mt >> 3);
}
#endif

// Input is the 10-bit mantissa of the half float value
// Output is the 11-bit qlog value
// Inverse of qlog11_to_half_float_mantissa()
static inline int half_float_mantissa_to_qlog11(int hf)
{
	int q0 = (hf * 8 + 2) / 3;
	int q1 = (hf * 8 + 2048 + 4) / 5;

	if (q0 < 512)
		return q0;
	else if (q1 >= 1536)
		return q1;

	int q2 = (hf * 8 + 512 + 2) / 4;
	return q2;
}

static inline int half_to_qlog16(int hf)
{
	// extract 5 bits exponent, which is carried through to qlog16 unchanged
	const int exp = (hf >> 10) & 0x1F;

	// extract and invert the 10 bit mantissa to nearest qlog11 (should be lossless)
	const int mantissa = half_float_mantissa_to_qlog11(hf & 0x3FF);
	assert(mantissa <= 0x7FF);

	// Now combine to qlog16, which is what ASTC HDR interpolates using the [0-64] weights.
	uint32_t qlog16 = (exp << 11) | mantissa;

	// should be a lossless operation
	assert(qlog16_to_half_slow(qlog16) == hf);

	return qlog16;
}

static inline uint32_t quant_qlog16(uint32_t q16, uint32_t desired_bits)
{
	assert((desired_bits >= 7) && (desired_bits <= 12));
	assert(q16 <= 65535);

	const uint32_t shift = 16 - desired_bits;
	uint32_t e = (q16 + (1U << (shift - 1U)) - 1U) >> shift;

	uint32_t max_val = (1U << desired_bits) - 1U;
	e = minimum<uint32_t>(e, max_val);

	return e;
}

static void compute_half_to_qlog_table(uint32_t bits, uint16_t* pTable, const basisu::vector<float> &qlog16_to_float)
{
	assert(bits >= 5 && bits <= 12);
	const uint32_t max_val = (1 << bits) - 1;

	// For all positive half-floats
	for (uint32_t h = 0; h < 32768; h++)
	{
		// Skip invalid values
		if (is_half_inf_or_nan((half_float)h))
			continue;
		const float desired_val = half_to_float((half_float)h);

		float best_err = 1e+30f;
		uint32_t best_qlog = 0;

		// For all possible qlog's
		for (uint32_t i = 0; i <= max_val; i++)
		{
			// Skip invalid values
			float v = qlog16_to_float[i << (16 - bits)];
			if (std::isnan(v))
				continue;

			// Compute error
			float err = fabs(v - desired_val);

			// Find best
			if (err < best_err)
			{
				best_err = err;
				best_qlog = i;
			}
		}

		pTable[h] = (uint16_t)best_qlog;
	}

#if 0
	uint32_t t = 0;

	const uint32_t nb = 12;
	int nb_shift = 16 - nb;

	for (uint32_t q16 = 0; q16 < 65536; q16++)
	{
		half_float h = qlog16_to_half_slow(q16);
		if (is_half_inf_or_nan(h))
			continue;

		int q7 = half_to_qlog7_12(h, nb);

		uint32_t best_err = UINT32_MAX, best_l = 0;
		for (int l = 0; l < (1 << nb); l++)
		{
			int dec_q16 = l << nb_shift;
			int err = iabs(dec_q16 - q16);
			if (err < best_err)
			{
				best_err = err;
				best_l = l;
			}
		}

		//int e = (q16 + 253) >> 9; // 345

		int e = (q16 + (1 << (nb_shift - 1)) - 1) >> nb_shift; // 285
		if (best_l != e)
			//if (q7 != best_l)
		{
			printf("q16=%u, h=%u, q7=%u, e=%u, best_l=%u\n", q16, h, q7, e, best_l);
			t++;
		}
	}

	printf("Mismatches: %u\n", t);
	exit(0);
#endif
}

static void init_qlog_tables()
{
	basisu::vector<float> qlog16_to_float(65536);

	// for all possible qlog16, compute the corresponding half float
	for (uint32_t i = 0; i <= 65535; i++)
	{
		half_float h = qlog16_to_half_slow(i);
		g_qlog16_to_half[i] = h;

		qlog16_to_float[i] = half_to_float(h);
	}

	// for all possible half floats, find the nearest qlog5-12 float
	for (uint32_t bits = HALF_TO_QLOG_TABS_BASE; bits <= 12; bits++)
	{
		compute_half_to_qlog_table(bits, g_pHalf_to_qlog_tabs[bits - HALF_TO_QLOG_TABS_BASE], qlog16_to_float);
	}
}

// [ise_range][0] = # levels
// [ise_range][1...] = lerp value [0,64]
// in ASTC order
// Supported ISE weight ranges: 0 to 10, 11 total
const uint32_t MIN_SUPPORTED_ISE_WEIGHT_INDEX = 1; // ISE 1=3 levels
const uint32_t MAX_SUPPORTED_ISE_WEIGHT_INDEX = 10; // ISE 10=24 levels

static const uint8_t g_ise_weight_lerps[MAX_SUPPORTED_ISE_WEIGHT_INDEX + 1][32] =
{
	{ 0 }, // ise range=0 is invalid for 4x4 block sizes (<24 weight bits in the block)
	{ 3, 0, 32, 64 }, // 1
	{ 4, 0, 21, 43, 64 }, // 2
	{ 5, 0, 16, 32, 48, 64 }, // 3
	{ 6, 0, 64, 12, 52, 25, 39 }, // 4
	{ 8, 0, 9, 18, 27, 37, 46, 55, 64 }, // 5
	{ 10, 0, 64, 7, 57, 14, 50, 21, 43, 28, 36 }, // 6
	{ 12, 0, 64, 17, 47, 5, 59, 23, 41, 11, 53, 28, 36 }, // 7
	{ 16, 0, 4, 8, 12, 17, 21, 25, 29, 35, 39, 43, 47, 52, 56, 60, 64 }, // 8
	{ 20, 0, 64, 16, 48, 3, 61, 19, 45, 6, 58, 23, 41, 9, 55, 26, 38, 13, 51, 29, 35 }, // 9
	{ 24, 0, 64, 8, 56, 16, 48, 24, 40, 2, 62, 11, 53, 19, 45, 27, 37, 5, 59, 13, 51, 22, 42, 30, 34 } // 10
};

//{ 12, 0, 64, 17, 47, 5, 59, 23, 41, 11, 53, 28, 36 }, // 7
//static const uint8_t g_weight_order_7[12] = { 0, 4, 8, 2, 6, 10, 11, 7, 3, 9, 5, 1 };

static vec3F calc_mean(uint32_t num_pixels, const vec4F* pPixels)
{
	vec3F mean(0.0f);

	for (uint32_t i = 0; i < num_pixels; i++)
	{
		const vec4F& p = pPixels[i];

		mean[0] += p[0];
		mean[1] += p[1];
		mean[2] += p[2];
	}

	return mean / static_cast<float>(num_pixels);
}

static vec3F calc_rgb_pca(uint32_t num_pixels, const vec4F* pPixels, const vec3F& mean_color)
{
	float cov[6] = { 0, 0, 0, 0, 0, 0 };

	for (uint32_t i = 0; i < num_pixels; i++)
	{
		const vec4F& v = pPixels[i];

		float r = v[0] - mean_color[0];
		float g = v[1] - mean_color[1];
		float b = v[2] - mean_color[2];

		cov[0] += r * r;
		cov[1] += r * g;
		cov[2] += r * b;
		cov[3] += g * g;
		cov[4] += g * b;
		cov[5] += b * b;
	}

	float xr = .9f, xg = 1.0f, xb = .7f;
	for (uint32_t iter = 0; iter < 3; iter++)
	{
		float r = xr * cov[0] + xg * cov[1] + xb * cov[2];
		float g = xr * cov[1] + xg * cov[3] + xb * cov[4];
		float b = xr * cov[2] + xg * cov[4] + xb * cov[5];

		float m = maximumf(maximumf(fabsf(r), fabsf(g)), fabsf(b));

		if (m > 1e-10f)
		{
			m = 1.0f / m;

			r *= m;
			g *= m;
			b *= m;
		}

		xr = r;
		xg = g;
		xb = b;
	}

	float len = xr * xr + xg * xg + xb * xb;

	vec3F axis;
	if (len < 1e-10f)
		axis.set(0.0f);
	else
	{
		len = 1.0f / sqrtf(len);

		xr *= len;
		xg *= len;
		xb *= len;

		axis.set(xr, xg, xb, 0);
	}

	if (axis.dot(axis) < .5f)
	{
		axis.set(1.0f, 1.0f, 1.0f, 0.0f);
		axis.normalize_in_place();
	}

	return axis;
}

static vec3F interp_color(const vec3F& mean, const vec3F& dir, float df, const aabb3F& colorspace_box, const aabb3F& input_box, bool* pInside = nullptr)
{
#if 0
	assert(mean[0] >= input_box[0][0]);
	assert(mean[1] >= input_box[0][1]);
	assert(mean[2] >= input_box[0][2]);
	assert(mean[0] <= input_box[1][0]);
	assert(mean[1] <= input_box[1][1]);
	assert(mean[2] <= input_box[1][2]);
#endif

	if (pInside)
		*pInside = false;

	vec3F k(mean + dir * df);
	if (colorspace_box.contains(k))
	{
		if (pInside)
			*pInside = true;

		return k;
	}

	// starts inside
	vec3F s(mean);

	// ends outside
	vec3F e(mean + dir * df);

	// a ray guaranteed to go from the outside to inside
	ray3F r(e, (s - e).normalize_in_place());
	vec3F c;
	float t = 0.0f;

	intersection::result res = intersection::ray_aabb(c, t, r, input_box);
	if (res != intersection::cSuccess)
		c = k;

	return c;
}

// all in Q16 space, 0-65535
static bool compute_least_squares_endpoints_rgb(
	uint32_t N, const uint8_t* pSelectors, const vec4F* pSelector_weights,
	vec3F* pXl, vec3F* pXh, const vec4F* pColors, const aabb3F& input_box)
{
	// Least squares using normal equations: http://www.cs.cornell.edu/~bindel/class/cs3220-s12/notes/lec10.pdf 
	// https://web.archive.org/web/20150319232457/http://www.cs.cornell.edu/~bindel/class/cs3220-s12/notes/lec10.pdf
	// I did this in matrix form first, expanded out all the ops, then optimized it a bit.
	float z00 = 0.0f, z01 = 0.0f, z10 = 0.0f, z11 = 0.0f;
	float q00_r = 0.0f, q10_r = 0.0f, t_r = 0.0f;
	float q00_g = 0.0f, q10_g = 0.0f, t_g = 0.0f;
	float q00_b = 0.0f, q10_b = 0.0f, t_b = 0.0f;

	for (uint32_t i = 0; i < N; i++)
	{
		const uint32_t sel = pSelectors[i];
		z00 += pSelector_weights[sel][0];
		z10 += pSelector_weights[sel][1];
		z11 += pSelector_weights[sel][2];

		float w = pSelector_weights[sel][3];
		q00_r += w * pColors[i][0];
		t_r += pColors[i][0];

		q00_g += w * pColors[i][1];
		t_g += pColors[i][1];

		q00_b += w * pColors[i][2];
		t_b += pColors[i][2];
	}

	q10_r = t_r - q00_r;
	q10_g = t_g - q00_g;
	q10_b = t_b - q00_b;

	z01 = z10;

	float det = z00 * z11 - z01 * z10;
	if (det == 0.0f)
		return false;

	det = 1.0f / det;

	float iz00, iz01, iz10, iz11;
	iz00 = z11 * det;
	iz01 = -z01 * det;
	iz10 = -z10 * det;
	iz11 = z00 * det;

	(*pXl)[0] = (float)(iz00 * q00_r + iz01 * q10_r);
	(*pXh)[0] = (float)(iz10 * q00_r + iz11 * q10_r);

	(*pXl)[1] = (float)(iz00 * q00_g + iz01 * q10_g);
	(*pXh)[1] = (float)(iz10 * q00_g + iz11 * q10_g);

	(*pXl)[2] = (float)(iz00 * q00_b + iz01 * q10_b);
	(*pXh)[2] = (float)(iz10 * q00_b + iz11 * q10_b);

	for (uint32_t c = 0; c < 3; c++)
	{
		float l = (*pXl)[c], h = (*pXh)[c];

		if (input_box.get_dim(c) < .0000125f)
		{
			l = input_box[0][c];
			h = input_box[1][c];
		}

		(*pXl)[c] = l;
		(*pXh)[c] = h;
	}

	vec3F mean((*pXl + *pXh) * .5f);
	vec3F dir(*pXh - *pXl);

	float ln = dir.length();
	if (ln)
	{
		dir /= ln;

		float ld = (*pXl - mean).dot(dir);
		float hd = (*pXh - mean).dot(dir);

		aabb3F colorspace_box(vec3F(0.0f), vec3F(MAX_QLOG16_VAL));

		bool was_inside1 = false;

		vec3F l = interp_color(mean, dir, ld, colorspace_box, input_box, &was_inside1);
		if (!was_inside1)
			*pXl = l;

		bool was_inside2 = false;
		vec3F h = interp_color(mean, dir, hd, colorspace_box, input_box, &was_inside2);
		if (!was_inside2)
			*pXh = h;
	}

	pXl->clamp(0.0f, MAX_QLOG16_VAL);
	pXh->clamp(0.0f, MAX_QLOG16_VAL);

	return true;
}

static vec4F g_astc_ls_weights_ise[MAX_SUPPORTED_ISE_WEIGHT_INDEX + 1][24];

static uint8_t g_map_astc_to_linear_order[MAX_SUPPORTED_ISE_WEIGHT_INDEX + 1][24]; // [ise_range][astc_index] -> linear index
static uint8_t g_map_linear_to_astc_order[MAX_SUPPORTED_ISE_WEIGHT_INDEX + 1][24]; // [ise_range][linear_index] -> astc_index

static void encode_astc_hdr_init()
{
	// Precomputed weight constants used during least fit determination. For each entry: w * w, (1.0f - w) * w, (1.0f - w) * (1.0f - w), w
	for (uint32_t range = MIN_SUPPORTED_ISE_WEIGHT_INDEX; range <= MAX_SUPPORTED_ISE_WEIGHT_INDEX; range++)
	{
		const uint32_t num_levels = g_ise_weight_lerps[range][0];
		assert((num_levels >= 3) && (num_levels <= 24));

		for (uint32_t i = 0; i < num_levels; i++)
		{
			float w = g_ise_weight_lerps[range][1 + i] * (1.0f / 64.0f);

			g_astc_ls_weights_ise[range][i].set(w * w, (1.0f - w) * w, (1.0f - w) * (1.0f - w), w);
		}
	}

	for (uint32_t ise_range = MIN_SUPPORTED_ISE_WEIGHT_INDEX; ise_range <= MAX_SUPPORTED_ISE_WEIGHT_INDEX; ise_range++)
	{
		const uint32_t num_levels = g_ise_weight_lerps[ise_range][0];
		assert((num_levels >= 3) && (num_levels <= 24));

		uint32_t s[32];
		for (uint32_t i = 0; i < num_levels; i++)
			s[i] = (g_ise_weight_lerps[ise_range][1 + i] << 8) + i;

		std::sort(s, s + num_levels);

		for (uint32_t i = 0; i < num_levels; i++)
			g_map_linear_to_astc_order[ise_range][i] = (uint8_t)(s[i] & 0xFF);

		for (uint32_t i = 0; i < num_levels; i++)
			g_map_astc_to_linear_order[ise_range][g_map_linear_to_astc_order[ise_range][i]] = (uint8_t)i;
	}
}

void interpolate_qlog12_colors(
	const int e[2][3],
	half_float* pDecoded_half,
	vec3F* pDecoded_float,
	uint32_t n, uint32_t ise_weight_range)
{
	assert((ise_weight_range >= MIN_SUPPORTED_ISE_WEIGHT_INDEX) && (ise_weight_range <= MAX_SUPPORTED_ISE_WEIGHT_INDEX));

	for (uint32_t i = 0; i < 2; i++)
	{
		for (uint32_t j = 0; j < 3; j++)
		{
			assert(in_range(e[i][j], 0, 0xFFF));
		}
	}

	for (uint32_t i = 0; i < n; i++)
	{
		const int c = g_ise_weight_lerps[ise_weight_range][1 + i];
		assert(c == (int)astc_helpers::dequant_bise_weight(i, ise_weight_range));

		half_float rf, gf, bf;

		{
			uint32_t r0 = e[0][0] << 4;
			uint32_t r1 = e[1][0] << 4;
			int ri = (r0 * (64 - c) + r1 * c + 32) / 64;
			rf = qlog16_to_half_slow(ri);
		}

		{
			uint32_t g0 = e[0][1] << 4;
			uint32_t g1 = e[1][1] << 4;
			int gi = (g0 * (64 - c) + g1 * c + 32) / 64;
			gf = qlog16_to_half_slow(gi);
		}

		{
			uint32_t b0 = e[0][2] << 4;
			uint32_t b1 = e[1][2] << 4;
			int bi = (b0 * (64 - c) + b1 * c + 32) / 64;
			bf = qlog16_to_half_slow(bi);
		}

		if (pDecoded_half)
		{
			pDecoded_half[i * 3 + 0] = rf;
			pDecoded_half[i * 3 + 1] = gf;
			pDecoded_half[i * 3 + 2] = bf;
		}

		if (pDecoded_float)
		{
			pDecoded_float[i][0] = half_to_float(rf);
			pDecoded_float[i][1] = half_to_float(gf);
			pDecoded_float[i][2] = half_to_float(bf);
		}
	}
}

// decoded in ASTC order, not linear order
// return false if the ISE endpoint quantization leads to non-valid endpoints being decoded
bool get_astc_hdr_mode_11_block_colors(
	const uint8_t* pEndpoints,
	half_float* pDecoded_half,
	vec3F* pDecoded_float,
	uint32_t n, uint32_t ise_weight_range, uint32_t ise_endpoint_range)
{
	assert((ise_weight_range >= 1) && (ise_weight_range <= MAX_SUPPORTED_ISE_WEIGHT_INDEX));

	int e[2][3];
	if (!decode_mode11_to_qlog12(pEndpoints, e, ise_endpoint_range))
		return false;

	interpolate_qlog12_colors(e, pDecoded_half, pDecoded_float, n, ise_weight_range);

	return true;
}

// decoded in ASTC order, not linear order
// return false if the ISE endpoint quantization leads to non-valid endpoints being decoded
bool get_astc_hdr_mode_7_block_colors(
	const uint8_t* pEndpoints,
	half_float* pDecoded_half,
	vec3F* pDecoded_float,
	uint32_t n, uint32_t ise_weight_range, uint32_t ise_endpoint_range)
{
	assert((ise_weight_range >= 1) && (ise_weight_range <= MAX_SUPPORTED_ISE_WEIGHT_INDEX));

	int e[2][3];
	if (!decode_mode7_to_qlog12(pEndpoints, e, nullptr, ise_endpoint_range))
		return false;

	interpolate_qlog12_colors(e, pDecoded_half, pDecoded_float, n, ise_weight_range);

	return true;
}

// Fast high precision piecewise linear approximation of log2(bias+x).
// Half may be zero, positive or denormal. No NaN/Inf/negative.
static inline double q(half_float x)
{
	union { float f; int32_t i; uint32_t u; } fi;

	fi.f = fast_half_to_float_pos_not_inf_or_nan(x);

	assert(fi.f >= 0.0f);

	fi.f += .125f;

	return (double)fi.u; // approx log2f(fi.f), need to return double for the precision
}

double eval_selectors(
	uint32_t num_pixels,
	uint8_t* pWeights,
	const half_float* pBlock_pixels_half,
	uint32_t num_weight_levels,
	const half_float* pDecoded_half,
	const astc_hdr_codec_options& coptions,
	uint32_t usable_selector_bitmask)
{
	assert((num_pixels >= 1) && (num_pixels <= 16));
	assert(usable_selector_bitmask);

	const float R_WEIGHT = coptions.m_r_err_scale;
	const float G_WEIGHT = coptions.m_g_err_scale;

	double total_error = 0;

#ifdef _DEBUG
	for (uint32_t i = 0; i < num_weight_levels; i++)
	{
		assert(!is_half_inf_or_nan(pDecoded_half[i * 3 + 0]));
		assert(!is_half_inf_or_nan(pDecoded_half[i * 3 + 1]));
		assert(!is_half_inf_or_nan(pDecoded_half[i * 3 + 2]));
	}
#endif

	for (uint32_t p = 0; p < num_pixels; p++)
	{
		const half_float* pDesired_half = &pBlock_pixels_half[p * 3];

		double lowest_e = 1e+30f;

		// this is an approximation of MSLE
		for (uint32_t i = 0; i < num_weight_levels; i++)
		{
			if (((1 << i) & usable_selector_bitmask) == 0)
				continue;

			// compute piecewise linear approximation of log2(a+eps)-log2(b+eps), for each component, then MSLE
			double rd = q(pDecoded_half[i * 3 + 0]) - q(pDesired_half[0]);
			double gd = q(pDecoded_half[i * 3 + 1]) - q(pDesired_half[1]);
			double bd = q(pDecoded_half[i * 3 + 2]) - q(pDesired_half[2]);

			double e = R_WEIGHT * (rd * rd) + G_WEIGHT * (gd * gd) + bd * bd;

			if (e < lowest_e)
			{
				lowest_e = e;
				pWeights[p] = (uint8_t)i;
			}
		}

		total_error += lowest_e;

	} // p

	return total_error;
}

//--------------------------------------------------------------------------------------------------------------------------

double compute_block_error(const half_float* pOrig_block, const half_float* pPacked_block, const astc_hdr_codec_options& coptions)
{
	const float R_WEIGHT = coptions.m_r_err_scale;
	const float G_WEIGHT = coptions.m_g_err_scale;

	double total_error = 0;
		
	for (uint32_t p = 0; p < 16; p++)
	{
		double rd = q(pOrig_block[p * 3 + 0]) - q(pPacked_block[p * 3 + 0]);
		double gd = q(pOrig_block[p * 3 + 1]) - q(pPacked_block[p * 3 + 1]);
		double bd = q(pOrig_block[p * 3 + 2]) - q(pPacked_block[p * 3 + 2]);

		double e = R_WEIGHT * (rd * rd) + G_WEIGHT * (gd * gd) + bd * bd;

		total_error += e;
	}

	return total_error;
}

//--------------------------------------------------------------------------------------------------------------------------

static inline int compute_clamped_val(int v, int l, int h, bool& did_clamp, int& max_clamp_mag)
{
	assert(l < h);

	if (v < l)
	{
		max_clamp_mag = basisu::maximum<int>(max_clamp_mag, l - v);

		v = l;
		did_clamp = true;
	}
	else if (v > h)
	{
		max_clamp_mag = basisu::maximum<int>(max_clamp_mag, v - h);

		v = h;
		did_clamp = true;
	}

	return v;
}

static bool pack_astc_mode11_submode(uint32_t submode, uint8_t* pEndpoints, const vec3F& low_q16, const vec3F& high_q16, int& max_clamp_mag)
{
	assert(submode <= 7);

	const uint8_t s_b_bits[8] = { 7, 8, 6, 7,  8, 6, 7, 6 };
	const uint8_t s_c_bits[8] = { 6, 6, 7, 7,  6, 7, 7, 7 };
	const uint8_t s_d_bits[8] = { 7, 6, 7, 6,  5, 6, 5, 6 };

	const uint32_t a_bits = 9 + (submode >> 1);
	const uint32_t b_bits = s_b_bits[submode];
	const uint32_t c_bits = s_c_bits[submode];
	const uint32_t d_bits = s_d_bits[submode];

	const int max_a_val = (1 << a_bits) - 1;
	const int max_b_val = (1 << b_bits) - 1;
	const int max_c_val = (1 << c_bits) - 1;

	// The maximum usable value before it turns to NaN/Inf
	const int max_a_qlog = get_max_qlog(a_bits);

	const int min_d_val = -(1 << (d_bits - 1));
	const int max_d_val = -min_d_val - 1;
	assert((max_d_val - min_d_val + 1) == (1 << d_bits));

	int val_q[2][3];

	for (uint32_t c = 0; c < 3; c++)
	{
#if 1
		// this is better
		const half_float l = qlog16_to_half_slow((uint32_t)std::round(low_q16[c]));
		val_q[0][c] = half_to_qlog7_12(l, a_bits);
		
		const half_float h = qlog16_to_half_slow((uint32_t)std::round(high_q16[c]));
		val_q[1][c] = half_to_qlog7_12(h, a_bits);
#else
		val_q[0][c] = quant_qlog16((uint32_t)std::round(low_q16[c]), a_bits);
		val_q[1][c] = quant_qlog16((uint32_t)std::round(high_q16[c]), a_bits);
#endif
				
#if 1
		if (val_q[0][c] == val_q[1][c])
		{
#if 0
			if (l <= h)
#else
			if (low_q16[c] < high_q16[c])
#endif
			{
				if (val_q[0][c])
					val_q[0][c]--;

				if (val_q[1][c] != max_a_val)
					val_q[1][c]++;
			}
			else
			{
				if (val_q[0][c] != max_a_val)
					val_q[0][c]++;

				if (val_q[1][c])
					val_q[1][c]--;
			}
		}
#endif

		val_q[0][c] = minimum<uint32_t>(val_q[0][c], max_a_qlog);
		val_q[1][c] = minimum<uint32_t>(val_q[1][c], max_a_qlog);
	}

	int highest_q = -1, highest_val = 0, highest_comp = 0;

	for (uint32_t v = 0; v < 2; v++)
	{
		for (uint32_t c = 0; c < 3; c++)
		{
			assert(val_q[v][c] >= 0 && val_q[v][c] <= max_a_val);

			if (val_q[v][c] > highest_q)
			{
				highest_q = val_q[v][c];
				highest_val = v;
				highest_comp = c;
			}
		}
	}

	const bool had_tie = (val_q[highest_val ^ 1][highest_comp] == highest_q);

	if (highest_val != 1)
	{
		for (uint32_t c = 0; c < 3; c++)
		{
			std::swap(val_q[0][c], val_q[1][c]);
		}
	}

	if (highest_comp)
	{
		std::swap(val_q[0][0], val_q[0][highest_comp]);
		std::swap(val_q[1][0], val_q[1][highest_comp]);
	}

	int orig_q[2][3];
	memcpy(orig_q, val_q, sizeof(val_q));

	// val[1][0] is now guaranteed to be highest
	int best_va = 0, best_vb0 = 0, best_vb1 = 0, best_vc = 0, best_vd0 = 0, best_vd1 = 0;
	int best_max_clamp_mag = 0;
	bool best_did_clamp = false;
	int best_q[2][3] = { { 0, 0, 0}, { 0, 0, 0 }  };
	BASISU_NOTE_UNUSED(best_q);
	uint32_t best_dist = UINT_MAX;

	for (uint32_t pass = 0; pass < 2; pass++)
	{
		int trial_va = val_q[1][0];

		assert(trial_va <= max_a_val);
		assert(trial_va >= val_q[1][1]);
		assert(trial_va >= val_q[1][2]);

		assert(trial_va >= val_q[0][0]);
		assert(trial_va >= val_q[0][1]);
		assert(trial_va >= val_q[0][2]);

		bool did_clamp = false;
		int trial_max_clamp_mag = 0;

		int trial_vb0 = compute_clamped_val(trial_va - val_q[1][1], 0, max_b_val, did_clamp, trial_max_clamp_mag);
		int trial_vb1 = compute_clamped_val(trial_va - val_q[1][2], 0, max_b_val, did_clamp, trial_max_clamp_mag);
		int trial_vc = compute_clamped_val(trial_va - val_q[0][0], 0, max_c_val, did_clamp, trial_max_clamp_mag);
		int trial_vd0 = compute_clamped_val((trial_va - trial_vb0 - trial_vc) - val_q[0][1], min_d_val, max_d_val, did_clamp, trial_max_clamp_mag);
		int trial_vd1 = compute_clamped_val((trial_va - trial_vb1 - trial_vc) - val_q[0][2], min_d_val, max_d_val, did_clamp, trial_max_clamp_mag);

		if (!did_clamp)
		{
			// Make sure decoder gets the expected values
			assert(trial_va == val_q[1][0]);
			assert(trial_va - trial_vb0 == val_q[1][1]);
			assert(trial_va - trial_vb1 == val_q[1][2]);

			assert((trial_va - trial_vc) == val_q[0][0]);
			assert((trial_va - trial_vb0 - trial_vc - trial_vd0) == val_q[0][1]);
			assert((trial_va - trial_vb1 - trial_vc - trial_vd1) == val_q[0][2]);
		}

		const int r_e0 = clamp<int>(trial_va, 0, max_a_val);
		const int r_e1 = clamp<int>(trial_va - trial_vb0, 0, max_a_val);
		const int r_e2 = clamp<int>(trial_va - trial_vb1, 0, max_a_val);

		const int r_f0 = clamp<int>(trial_va - trial_vc, 0, max_a_val);
		const int r_f1 = clamp<int>(trial_va - trial_vb0 - trial_vc - trial_vd0, 0, max_a_val);
		const int r_f2 = clamp<int>(trial_va - trial_vb1 - trial_vc - trial_vd1, 0, max_a_val);

		assert(r_e0 <= max_a_qlog);
		assert(r_e1 <= max_a_qlog);
		assert(r_e2 <= max_a_qlog);

		assert(r_f0 <= max_a_qlog);
		assert(r_f1 <= max_a_qlog);
		assert(r_f2 <= max_a_qlog);

		if ((!did_clamp) || (!had_tie))
		{
			best_va = trial_va;
			best_vb0 = trial_vb0;
			best_vb1 = trial_vb1;
			best_vc = trial_vc;
			best_vd0 = trial_vd0;
			best_vd1 = trial_vd1;
			best_max_clamp_mag = trial_max_clamp_mag;
			best_did_clamp = did_clamp;

			best_q[1][0] = r_e0;
			best_q[1][1] = r_e1;
			best_q[1][2] = r_e2;
			best_q[0][0] = r_f0;
			best_q[0][1] = r_f1;
			best_q[0][2] = r_f2;
			break;
		}

		// we had a tie and it did clamp, try swapping L/H for a potential slight gain

		const uint32_t r_dist1 = basisu::square<int>(r_e0 - val_q[1][0]) + basisu::square<int>(r_e1 - val_q[1][1]) + basisu::square<int>(r_e2 - val_q[1][2]);
		const uint32_t r_dist0 = basisu::square<int>(r_f0 - val_q[0][0]) + basisu::square<int>(r_f1 - val_q[0][1]) + basisu::square<int>(r_f2 - val_q[0][2]);

		const uint32_t total_dist = r_dist1 + r_dist0;

		if (total_dist < best_dist)
		{
			best_dist = total_dist;

			best_va = trial_va;
			best_vb0 = trial_vb0;
			best_vb1 = trial_vb1;
			best_vc = trial_vc;
			best_vd0 = trial_vd0;
			best_vd1 = trial_vd1;
			best_did_clamp = did_clamp;

			best_q[1][0] = r_e0;
			best_q[1][1] = r_e1;
			best_q[1][2] = r_e2;
			best_q[0][0] = r_f0;
			best_q[0][1] = r_f1;
			best_q[0][2] = r_f2;
		}

		for (uint32_t c = 0; c < 3; c++)
			std::swap(val_q[0][c], val_q[1][c]);
	}

	// pack bits now
	int v0 = 0, v1 = 0, v2 = 0, v3 = 0, v4 = 0, v5 = 0;

	int x0 = 0, x1 = 0, x2 = 0, x3 = 0, x4 = 0, x5 = 0;
	switch (submode)
	{
	case 0:
		x0 = get_bit(best_vb0, 6); x1 = get_bit(best_vb1, 6); x2 = get_bit(best_vd0, 6); x3 = get_bit(best_vd1, 6); x4 = get_bit(best_vd0, 5); x5 = get_bit(best_vd1, 5);
		break;
	case 1:
		x0 = get_bit(best_vb0, 6); x1 = get_bit(best_vb1, 6); x2 = get_bit(best_vb0, 7); x3 = get_bit(best_vb1, 7); x4 = get_bit(best_vd0, 5); x5 = get_bit(best_vd1, 5);
		break;
	case 2:
		x0 = get_bit(best_va, 9); x1 = get_bit(best_vc, 6); x2 = get_bit(best_vd0, 6); x3 = get_bit(best_vd1, 6); x4 = get_bit(best_vd0, 5); x5 = get_bit(best_vd1, 5);
		break;
	case 3:
		x0 = get_bit(best_vb0, 6); x1 = get_bit(best_vb1, 6); x2 = get_bit(best_va, 9); x3 = get_bit(best_vc, 6); x4 = get_bit(best_vd0, 5); x5 = get_bit(best_vd1, 5);
		break;
	case 4:
		x0 = get_bit(best_vb0, 6); x1 = get_bit(best_vb1, 6); x2 = get_bit(best_vb0, 7); x3 = get_bit(best_vb1, 7); x4 = get_bit(best_va, 9); x5 = get_bit(best_va, 10);
		break;
	case 5:
		x0 = get_bit(best_va, 9); x1 = get_bit(best_va, 10); x2 = get_bit(best_vc, 7); x3 = get_bit(best_vc, 6); x4 = get_bit(best_vd0, 5); x5 = get_bit(best_vd1, 5);
		break;
	case 6:
		x0 = get_bit(best_vb0, 6); x1 = get_bit(best_vb1, 6); x2 = get_bit(best_va, 11); x3 = get_bit(best_vc, 6); x4 = get_bit(best_va, 9); x5 = get_bit(best_va, 10);
		break;
	case 7:
		x0 = get_bit(best_va, 9); x1 = get_bit(best_va, 10); x2 = get_bit(best_va, 11); x3 = get_bit(best_vc, 6); x4 = get_bit(best_vd0, 5); x5 = get_bit(best_vd1, 5);
		break;
	default:
		break;
	}

	// write mode
	pack_bit(v1, 7, submode, 0);
	pack_bit(v2, 7, submode, 1);
	pack_bit(v3, 7, submode, 2);

	// highest component
	pack_bit(v4, 7, highest_comp, 0);
	pack_bit(v5, 7, highest_comp, 1);

	// write bit 8 of va
	pack_bit(v1, 6, best_va, 8);

	// extra bits
	pack_bit(v2, 6, x0);
	pack_bit(v3, 6, x1);
	pack_bit(v4, 6, x2);
	pack_bit(v5, 6, x3);
	pack_bit(v4, 5, x4);
	pack_bit(v5, 5, x5);

	v0 = best_va & 0xFF;
	v1 |= (best_vc & 63);
	v2 |= (best_vb0 & 63);
	v3 |= (best_vb1 & 63);
	v4 |= (best_vd0 & 31);
	v5 |= (best_vd1 & 31);

	assert(in_range(v0, 0, 255) && in_range(v1, 0, 255) && in_range(v2, 0, 255) && in_range(v3, 0, 255) && in_range(v4, 0, 255) && in_range(v5, 0, 255));

	pEndpoints[0] = (uint8_t)v0;
	pEndpoints[1] = (uint8_t)v1;
	pEndpoints[2] = (uint8_t)v2;
	pEndpoints[3] = (uint8_t)v3;
	pEndpoints[4] = (uint8_t)v4;
	pEndpoints[5] = (uint8_t)v5;

#ifdef _DEBUG
	// Test for valid pack by unpacking
	{
		if (highest_comp)
		{
			std::swap(best_q[0][0], best_q[0][highest_comp]);
			std::swap(best_q[1][0], best_q[1][highest_comp]);

			std::swap(orig_q[0][0], orig_q[0][highest_comp]);
			std::swap(orig_q[1][0], orig_q[1][highest_comp]);
		}

		int test_e[2][3];
		decode_mode11_to_qlog12(pEndpoints, test_e, astc_helpers::BISE_256_LEVELS);
		for (uint32_t i = 0; i < 2; i++)
		{
			for (uint32_t j = 0; j < 3; j++)
			{
				assert(best_q[i][j] == test_e[i][j] >> (12 - a_bits));

				if (!best_did_clamp)
				{
					assert((orig_q[i][j] == test_e[i][j] >> (12 - a_bits)) ||
						(orig_q[1 - i][j] == test_e[i][j] >> (12 - a_bits)));
				}
			}
		}
	}
#endif

	max_clamp_mag = best_max_clamp_mag;

	return best_did_clamp;
}

//--------------------------------------------------------------------------------------------------------------------------

static void pack_astc_mode11_direct(uint8_t* pEndpoints, const vec3F& l_q16, const vec3F& h_q16)
{
	for (uint32_t i = 0; i < 3; i++)
	{
		// TODO: This goes from QLOG16->HALF->QLOG8/7
		half_float l_half = qlog16_to_half_slow(clamp((int)std::round(l_q16[i]), 0, 65535));
		half_float h_half = qlog16_to_half_slow(clamp((int)std::round(h_q16[i]), 0, 65535));

		int l_q, h_q;

		if (i == 2)
		{
			l_q = g_half_to_qlog7[bounds_check((uint32_t)l_half, 0U, 32768U)];
			h_q = g_half_to_qlog7[bounds_check((uint32_t)h_half, 0U, 32768U)];

			l_q = minimum<uint32_t>(l_q, MAX_QLOG7);
			h_q = minimum<uint32_t>(h_q, MAX_QLOG7);
		}
		else
		{
			l_q = g_half_to_qlog8[bounds_check((uint32_t)l_half, 0U, 32768U)];
			h_q = g_half_to_qlog8[bounds_check((uint32_t)h_half, 0U, 32768U)];

			l_q = minimum<uint32_t>(l_q, MAX_QLOG8);
			h_q = minimum<uint32_t>(h_q, MAX_QLOG8);
		}

#if 1
		if (l_q == h_q)
		{
			const int m = (i == 2) ? MAX_QLOG7 : MAX_QLOG8;

			if (l_q16[i] <= h_q16[i])
			{
				if (l_q)
					l_q--;

				if (h_q != m)
					h_q++;
			}
			else
			{
				if (h_q)
					h_q--;

				if (l_q != m)
					l_q++;
			}
		}
#endif
				
		if (i == 2)
		{
			assert(l_q <= (int)MAX_QLOG7 && h_q <= (int)MAX_QLOG7);
			l_q |= 128;
			h_q |= 128;
		}
		else
		{
			assert(l_q <= (int)MAX_QLOG8 && h_q <= (int)MAX_QLOG8);
		}

		pEndpoints[2 * i + 0] = (uint8_t)l_q;
		pEndpoints[2 * i + 1] = (uint8_t)h_q;
	}
}

//--------------------------------------------------------------------------------------------------------------------------

static bool pack_astc_mode7_submode(uint32_t submode, uint8_t* pEndpoints, const vec3F& rgb_q16, float s_q16, int& max_clamp_mag, uint32_t ise_weight_range)
{
	assert((ise_weight_range >= 1) && (ise_weight_range <= MAX_SUPPORTED_ISE_WEIGHT_INDEX));

	assert(submode <= 5);
	max_clamp_mag = 0;

	static const uint8_t s_r_bits[6] = { 11, 11, 10, 9, 8, 7 };
	static const uint8_t s_g_b_bits[6] = { 5, 6, 5, 6, 7, 7 };
	static const uint8_t s_s_bits[6] = { 7, 5, 8, 7, 6, 7 };

	// The precision of the components
	const uint32_t prec_bits = s_r_bits[submode];

	int qlog[4], pack_bits[4];

	for (uint32_t i = 0; i < 4; i++)
	{
		const float f = (i == 3) ? s_q16 : rgb_q16[i];

		// The # of bits the component is packed into
		if (i == 0)
			pack_bits[i] = s_r_bits[submode];
		else if (i == 3)
			pack_bits[i] = s_s_bits[submode];
		else
			pack_bits[i] = s_g_b_bits[submode];

#if 0
		// this is slightly worse
		// TODO: going from qlog16 to half loses some precision. Then going from half to qlog 7-12 will have extra error.
		half_float h = qlog_to_half(clamp((int)std::round(f), 0, MAX_QLOG16), 16);
		qlog[i] = half_to_qlog7_12((half_float)bounds_check((uint32_t)h, 0U, 32768U), prec_bits);
#else
		qlog[i] = quant_qlog16(clamp<int>((int)std::round(f), 0, MAX_QLOG16), prec_bits);

		// Only bias if there are enough texel weights, 4=6 weights
		if (ise_weight_range >= 4)
		{
			// Explictly bias the high color, and the scale up, to better exploit the weights.
			// The quantized range also then encompases the complete input range.
			const uint32_t max_val = (1 << prec_bits) - 1;
			const uint32_t K = 3;
			if (i == 3)
			{
				qlog[i] = minimum<uint32_t>(qlog[i] + K * 2, max_val);
			}
			else
			{
				qlog[i] = minimum<uint32_t>(qlog[i] + K, max_val);
			}
		}
#endif

		if (i != 3)
			qlog[i] = minimum<uint32_t>(qlog[i], get_max_qlog(prec_bits));

		// If S=0, we lose freedom for the texel weights to add any value.
		if ((i == 3) && (qlog[i] == 0))
			qlog[i] = 1;
	}

	uint32_t maj_index = 0;

	bool did_clamp = false;

	if (submode != 5)
	{
		int largest_qlog = 0;
		for (uint32_t i = 0; i < 3; i++)
		{
			if (qlog[i] > largest_qlog)
			{
				largest_qlog = qlog[i];
				maj_index = i;
			}
		}

		if (maj_index)
		{
			std::swap(qlog[0], qlog[maj_index]);
		}

		assert(qlog[0] >= qlog[1]);
		assert(qlog[0] >= qlog[2]);

		qlog[1] = qlog[0] - qlog[1];
		qlog[2] = qlog[0] - qlog[2];

		for (uint32_t i = 1; i < 4; i++)
		{
			const int max_val = (1 << pack_bits[i]) - 1;

			if (qlog[i] > max_val)
			{
				max_clamp_mag = maximum<int>(max_clamp_mag, qlog[i] - max_val);
				qlog[i] = max_val;
				did_clamp = true;
			}
		}
	}

	for (uint32_t i = 0; i < 4; i++)
	{
		const int max_val = (1 << pack_bits[i]) - 1; (void)max_val;

		assert(qlog[i] <= max_val);
	}

	int mode = 0;

	int r = qlog[0] & 63; // 6-bits
	int g = qlog[1] & 31; // 5-bits
	int b = qlog[2] & 31; // 5-bits
	int s = qlog[3] & 31; // 5-bits

	int x0 = 0, x1 = 0, x2 = 0, x3 = 0, x4 = 0, x5 = 0, x6 = 0;

	switch (submode)
	{
	case 0:
	{
		mode = (maj_index << 2) | 0;
		assert((mode & 0xC) != 0xC);

		x0 = get_bit(qlog[0], 9); // R9
		x1 = get_bit(qlog[0], 8); // R8
		x2 = get_bit(qlog[0], 7); // R7
		x3 = get_bit(qlog[0], 10); // R10
		x4 = get_bit(qlog[0], 6); // R6 
		x5 = get_bit(qlog[3], 6); // S6
		x6 = get_bit(qlog[3], 5); // S5
		break;
	}
	case 1:
	{
		mode = (maj_index << 2) | 1;
		assert((mode & 0xC) != 0xC);

		x0 = get_bit(qlog[0], 8); // R8
		x1 = get_bit(qlog[1], 5); // G5
		x2 = get_bit(qlog[0], 7); // R7
		x3 = get_bit(qlog[2], 5); // B5
		x4 = get_bit(qlog[0], 6); // R6 
		x5 = get_bit(qlog[0], 10); // R10
		x6 = get_bit(qlog[0], 9); // R9
		break;
	}
	case 2:
	{
		mode = (maj_index << 2) | 2;
		assert((mode & 0xC) != 0xC);

		x0 = get_bit(qlog[0], 9); // R9
		x1 = get_bit(qlog[0], 8); // R8
		x2 = get_bit(qlog[0], 7); // R7
		x3 = get_bit(qlog[0], 6); // R6
		x4 = get_bit(qlog[3], 7); // S7 
		x5 = get_bit(qlog[3], 6); // S6
		x6 = get_bit(qlog[3], 5); // S5
		break;
	}
	case 3:
	{
		mode = (maj_index << 2) | 3;
		assert((mode & 0xC) != 0xC);

		x0 = get_bit(qlog[0], 8); // R8
		x1 = get_bit(qlog[1], 5); // G5
		x2 = get_bit(qlog[0], 7); // R7
		x3 = get_bit(qlog[2], 5); // B5
		x4 = get_bit(qlog[0], 6); // R6 
		x5 = get_bit(qlog[3], 6); // S6
		x6 = get_bit(qlog[3], 5); // S5
		break;
	}
	case 4:
	{
		mode = maj_index | 0xC; // 0b1100
		assert((mode & 0xC) == 0xC);
		assert(mode != 0xF);

		x0 = get_bit(qlog[1], 6); // G6
		x1 = get_bit(qlog[1], 5); // G5
		x2 = get_bit(qlog[2], 6); // B6
		x3 = get_bit(qlog[2], 5); // B5
		x4 = get_bit(qlog[0], 6); // R6 
		x5 = get_bit(qlog[0], 7); // R7
		x6 = get_bit(qlog[3], 5); // S5
		break;
	}
	case 5:
	{
		mode = 0xF;

		x0 = get_bit(qlog[1], 6); // G6
		x1 = get_bit(qlog[1], 5); // G5
		x2 = get_bit(qlog[2], 6); // B6
		x3 = get_bit(qlog[2], 5); // B5
		x4 = get_bit(qlog[0], 6); // R6 
		x5 = get_bit(qlog[3], 6); // S6
		x6 = get_bit(qlog[3], 5); // S5
		break;
	}
	default:
	{
		assert(0);
		break;
	}
	}

	pEndpoints[0] = (uint8_t)((get_bit(mode, 1) << 7) | (get_bit(mode, 0) << 6) | r);
	pEndpoints[1] = (uint8_t)((get_bit(mode, 2) << 7) | (x0 << 6) | (x1 << 5) | g);
	pEndpoints[2] = (uint8_t)((get_bit(mode, 3) << 7) | (x2 << 6) | (x3 << 5) | b);
	pEndpoints[3] = (uint8_t)((x4 << 7) | (x5 << 6) | (x6 << 5) | s);

#ifdef _DEBUG
	// Test for valid pack by unpacking
	{
		const int inv_shift = 12 - prec_bits;

		int unpacked_e[2][3];
		if (submode != 5)
		{
			unpacked_e[1][0] = left_shift32(qlog[0], inv_shift);
			unpacked_e[1][1] = clamp(left_shift32((qlog[0] - qlog[1]), inv_shift), 0, 0xFFF);
			unpacked_e[1][2] = clamp(left_shift32((qlog[0] - qlog[2]), inv_shift), 0, 0xFFF);

			unpacked_e[0][0] = clamp(left_shift32((qlog[0] - qlog[3]), inv_shift), 0, 0xFFF);
			unpacked_e[0][1] = clamp(left_shift32(((qlog[0] - qlog[1]) - qlog[3]), inv_shift), 0, 0xFFF);
			unpacked_e[0][2] = clamp(left_shift32(((qlog[0] - qlog[2]) - qlog[3]), inv_shift), 0, 0xFFF);
		}
		else
		{
			unpacked_e[1][0] = left_shift32(qlog[0], inv_shift);
			unpacked_e[1][1] = left_shift32(qlog[1], inv_shift);
			unpacked_e[1][2] = left_shift32(qlog[2], inv_shift);

			unpacked_e[0][0] = clamp(left_shift32((qlog[0] - qlog[3]), inv_shift), 0, 0xFFF);
			unpacked_e[0][1] = clamp(left_shift32((qlog[1] - qlog[3]), inv_shift), 0, 0xFFF);
			unpacked_e[0][2] = clamp(left_shift32((qlog[2] - qlog[3]), inv_shift), 0, 0xFFF);
		}

		if (maj_index)
		{
			std::swap(unpacked_e[0][0], unpacked_e[0][maj_index]);
			std::swap(unpacked_e[1][0], unpacked_e[1][maj_index]);
		}

		int e[2][3];
		decode_mode7_to_qlog12_ise20(pEndpoints, e, nullptr);

		for (uint32_t i = 0; i < 3; i++)
		{
			assert(unpacked_e[0][i] == e[0][i]);
			assert(unpacked_e[1][i] == e[1][i]);
		}
	}
#endif

	return did_clamp;
}

//--------------------------------------------------------------------------------------------------------------------------

static void quantize_ise_endpoints(uint32_t ise_endpoint_range, const uint8_t* pSrc_endpoints, uint8_t *pDst_endpoints, uint32_t n)
{
	assert((ise_endpoint_range >= astc_helpers::FIRST_VALID_ENDPOINT_ISE_RANGE) && (ise_endpoint_range <= astc_helpers::LAST_VALID_ENDPOINT_ISE_RANGE));

	if (ise_endpoint_range == astc_helpers::BISE_256_LEVELS)
	{
		memcpy(pDst_endpoints, pSrc_endpoints, n);
	}
	else
	{
		for (uint32_t i = 0; i < n; i++)
		{
			uint32_t v = pSrc_endpoints[i];
			assert(v <= 255);

			pDst_endpoints[i] = astc_helpers::g_dequant_tables.get_endpoint_tab(ise_endpoint_range).m_val_to_ise[v];
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------

// Note this could fail to find any valid solution if use_endpoint_range!=20.
// Returns true if improved.
static bool try_mode11(uint32_t num_pixels,
	uint8_t* pEndpoints, uint8_t* pWeights, double& cur_block_error, uint32_t& submode_used,
	vec3F& low_color_q16, const vec3F& high_color_q16,
	half_float block_pixels_half[16][3],
	uint32_t num_weight_levels, uint32_t ise_weight_range, const astc_hdr_codec_options& coptions, bool direct_only, uint32_t ise_endpoint_range, 
	bool constrain_ise_weight8_selectors, 
	int32_t first_submode, int32_t last_submode) // -1, 7
{
	assert((ise_weight_range >= 1) && (ise_weight_range <= MAX_SUPPORTED_ISE_WEIGHT_INDEX));
	assert((num_weight_levels >= 3) && (num_weight_levels <= 32));
	assert((num_pixels >= 1) && (num_pixels <= 16));

	bool improved_flag = false;

	half_float decoded_half[32][3];
	vec3F decoded_float[32];
	uint8_t orig_trial_endpoints[NUM_MODE11_ENDPOINTS], trial_endpoints[NUM_MODE11_ENDPOINTS], trial_weights[16];

	if (direct_only)
	{
		first_submode = -1;
		last_submode = -1;
	}

	assert(first_submode <= last_submode);
	assert((first_submode >= -1) && (first_submode <= 7));
	assert((last_submode >= -1) && (last_submode <= 7));

	// TODO: First determine if a submode doesn't clamp first. If one is found, encode to that and we're done.
	for (int submode = last_submode; submode >= first_submode; submode--)
	{
		bool did_clamp = false;
		int max_clamp_mag = 0;
		if (submode == -1)
		{
			// If it had to clamp with one of the submodes, try direct which can't clamp, but has low precision.
			pack_astc_mode11_direct(orig_trial_endpoints, low_color_q16, high_color_q16);
		}
		else
		{
			did_clamp = pack_astc_mode11_submode(submode, orig_trial_endpoints, low_color_q16, high_color_q16, max_clamp_mag);

			// If it had to clamp and the clamp was too high, it'll distort the endpoint colors too much, which could lead to noticeable artifacts.
			const int MAX_CLAMP_MAG_ACCEPT_THRESH = 4;
			if ((did_clamp) && (max_clamp_mag > MAX_CLAMP_MAG_ACCEPT_THRESH))
				continue;
		}
				
		// This will distort the endpoints if the ISE endpoint range isn't 256 levels (20).
		// It could massively distort the endpoints, but still result in a valid encoding.
		quantize_ise_endpoints(ise_endpoint_range, orig_trial_endpoints, trial_endpoints, NUM_MODE11_ENDPOINTS);
		
		if (!get_astc_hdr_mode_11_block_colors(trial_endpoints, &decoded_half[0][0], decoded_float, num_weight_levels, ise_weight_range, ise_endpoint_range))
			continue;

		uint32_t usable_selector_bitmask = UINT32_MAX;
		if ((constrain_ise_weight8_selectors) && (ise_weight_range == astc_helpers::BISE_16_LEVELS))
			usable_selector_bitmask = (1 << 0) | (1 << 1) | (1 << 4) | (1 << 5) | (1 << 10) | (1 << 11) | (1 << 14) | (1 << 15);

		double trial_blk_error = eval_selectors(num_pixels, trial_weights, &block_pixels_half[0][0], num_weight_levels, &decoded_half[0][0], coptions, usable_selector_bitmask);
		if (trial_blk_error < cur_block_error)
		{
			cur_block_error = trial_blk_error;
			memcpy(pEndpoints, trial_endpoints, NUM_MODE11_ENDPOINTS);
			memcpy(pWeights, trial_weights, num_pixels);
			submode_used = submode + 1;
			improved_flag = true;
		}

		// If it didn't clamp it was a lossless encode at this precision, so we can stop early as there's probably no use trying lower precision submodes.
		// (Although it may be, because a lower precision pack could try nearby voxel coords.)
		// However, at lower levels quantization may cause the decoded endpoints to be very distorted, so we need to evaluate up to direct.
		if (ise_endpoint_range == astc_helpers::BISE_256_LEVELS) 
		{
			if (!did_clamp)
				break;
		}
	}

	return improved_flag;
}

//--------------------------------------------------------------------------------------------------------------------------

static bool try_mode7(
	uint32_t num_pixels,
	uint8_t* pEndpoints, uint8_t* pWeights, double& cur_block_error, uint32_t& submode_used,
	vec3F& high_color_q16, const float s_q16,
	half_float block_pixels_half[16][3],
	uint32_t num_weight_levels, uint32_t ise_weight_range, const astc_hdr_codec_options& coptions, 
	uint32_t ise_endpoint_range)
{
	assert((ise_weight_range >= 1) && (ise_weight_range <= MAX_SUPPORTED_ISE_WEIGHT_INDEX));
	assert((num_pixels >= 1) && (num_pixels <= 16));

	bool improved_flag = false;

	half_float decoded_half[24][3];
	vec3F decoded_float[24];

	uint8_t orig_trial_endpoints[NUM_MODE7_ENDPOINTS], trial_endpoints[NUM_MODE7_ENDPOINTS], trial_weights[16];

	// TODO: First determine if a submode doesn't clamp first. If one is found, encode to that and we're done.
	for (int submode = 0; submode <= 5; submode++)
	{
		int max_clamp_mag = 0;
		const bool did_clamp = pack_astc_mode7_submode(submode, orig_trial_endpoints, high_color_q16, s_q16, max_clamp_mag, ise_weight_range);

		if (submode < 5)
		{
			const int MAX_CLAMP_MAG_ACCEPT_THRESH = 4;
			if ((did_clamp) && (max_clamp_mag > MAX_CLAMP_MAG_ACCEPT_THRESH))
				continue;
		}

		// This will distort the endpoints if the ISE endpoint range isn't 256 levels (20).
		// It could massively distort the endpoints, but still result in a valid encoding.
		quantize_ise_endpoints(ise_endpoint_range, orig_trial_endpoints, trial_endpoints, NUM_MODE7_ENDPOINTS);

		if (!get_astc_hdr_mode_7_block_colors(trial_endpoints, &decoded_half[0][0], decoded_float, num_weight_levels, ise_weight_range, ise_endpoint_range))
			continue;

		double trial_blk_error = eval_selectors(num_pixels, trial_weights, &block_pixels_half[0][0], num_weight_levels, &decoded_half[0][0], coptions);
		if (trial_blk_error < cur_block_error)
		{
			cur_block_error = trial_blk_error;
			memcpy(pEndpoints, trial_endpoints, NUM_MODE7_ENDPOINTS);
			memcpy(pWeights, trial_weights, num_pixels);
			submode_used = submode;
			improved_flag = true;
		}

		if (ise_endpoint_range == astc_helpers::BISE_256_LEVELS)
		{
			if (!did_clamp)
				break;
		}
	}

	return improved_flag;
}

//--------------------------------------------------------------------------------------------------------------------------

static double encode_astc_hdr_block_mode_11(
	uint32_t num_pixels,
	const vec4F* pBlock_pixels,
	uint32_t ise_weight_range,
	uint32_t& best_submode,
	double cur_block_error,
	uint8_t* blk_endpoints, uint8_t* blk_weights,
	const astc_hdr_codec_options& coptions,
	bool direct_only,
	uint32_t ise_endpoint_range,
	bool uber_mode,
	bool constrain_ise_weight8_selectors,
	int32_t first_submode, int32_t last_submode)
{
	assert((ise_weight_range >= 1) && (ise_weight_range <= MAX_SUPPORTED_ISE_WEIGHT_INDEX));
	assert((ise_endpoint_range >= astc_helpers::FIRST_VALID_ENDPOINT_ISE_RANGE) && (ise_endpoint_range <= astc_helpers::LAST_VALID_ENDPOINT_ISE_RANGE));
	assert((num_pixels >= 1) && (num_pixels <= 16));

	best_submode = 0;

	half_float block_pixels_half[16][3];
	vec4F block_pixels_q16[16];
		
	// TODO: This is done redundantly.
	for (uint32_t i = 0; i < num_pixels; i++)
	{
		block_pixels_half[i][0] = float_to_half_non_neg_no_nan_inf(pBlock_pixels[i][0]);
		block_pixels_q16[i][0] = (float)half_to_qlog16(block_pixels_half[i][0]);

		block_pixels_half[i][1] = float_to_half_non_neg_no_nan_inf(pBlock_pixels[i][1]);
		block_pixels_q16[i][1] = (float)half_to_qlog16(block_pixels_half[i][1]);

		block_pixels_half[i][2] = float_to_half_non_neg_no_nan_inf(pBlock_pixels[i][2]);
		block_pixels_q16[i][2] = (float)half_to_qlog16(block_pixels_half[i][2]);

		block_pixels_q16[i][3] = 0.0f;
	}

	const uint32_t num_weight_levels = astc_helpers::get_ise_levels(ise_weight_range);
	
	// TODO: should match MAX_SUPPORTED_ISE_WEIGHT_INDEX
	const uint32_t MAX_WEIGHT_LEVELS = 32;
	(void)MAX_WEIGHT_LEVELS;
	assert(num_weight_levels <= MAX_WEIGHT_LEVELS);

	vec3F block_mean_color_q16(calc_mean(num_pixels, block_pixels_q16));
	vec3F block_axis_q16(calc_rgb_pca(num_pixels, block_pixels_q16, block_mean_color_q16));

	aabb3F color_box_q16(cInitExpand);

	float l = 1e+30f, h = -1e+30f;
	vec3F low_color_q16, high_color_q16;

	for (uint32_t i = 0; i < num_pixels; i++)
	{
		color_box_q16.expand(block_pixels_q16[i]);

		vec3F k(vec3F(block_pixels_q16[i]) - block_mean_color_q16);
		float kd = k.dot(block_axis_q16);

		if (kd < l)
		{
			l = kd;
			low_color_q16 = block_pixels_q16[i];
		}

		if (kd > h)
		{
			h = kd;
			high_color_q16 = block_pixels_q16[i];
		}
	}

	vec3F old_low_color_q16(low_color_q16), old_high_color_q16(high_color_q16);
	for (uint32_t i = 0; i < 3; i++)
	{
		low_color_q16[i] = lerp<float>(old_low_color_q16[i], old_high_color_q16[i], 1.0f / 64.0f);
		high_color_q16[i] = lerp<float>(old_low_color_q16[i], old_high_color_q16[i], 63.0f / 64.0f);
	}
		
	uint8_t trial_blk_endpoints[NUM_MODE11_ENDPOINTS];
	uint8_t trial_blk_weights[16];
	uint32_t trial_best_submode = 0;
	
	clear_obj(trial_blk_endpoints);
	clear_obj(trial_blk_weights);
	
	double trial_blk_error = 1e+30f;

	bool did_improve = try_mode11(num_pixels, trial_blk_endpoints, trial_blk_weights, trial_blk_error, trial_best_submode,
		low_color_q16, high_color_q16,
		block_pixels_half, num_weight_levels, ise_weight_range, coptions, direct_only, ise_endpoint_range, constrain_ise_weight8_selectors,
		first_submode, last_submode);
	
	// If we couldn't find ANY usable solution due to endpoint quantization, just return. There's nothing we can do.
	if (!did_improve)
		return cur_block_error;

	// Did the solution improve?
	if (trial_blk_error < cur_block_error)
	{
		cur_block_error = trial_blk_error;
		memcpy(blk_endpoints, trial_blk_endpoints, NUM_MODE11_ENDPOINTS);
		memcpy(blk_weights, trial_blk_weights, num_pixels);
		best_submode = trial_best_submode;
	}
		
#define USE_LEAST_SQUARES (1)
#if USE_LEAST_SQUARES
	// least squares on the most promising trial weight indices found
	const uint32_t NUM_LS_PASSES = 3;

	for (uint32_t pass = 0; pass < NUM_LS_PASSES; pass++)
	{
		vec3F l_q16, h_q16;
		if (!compute_least_squares_endpoints_rgb(num_pixels, trial_blk_weights, &g_astc_ls_weights_ise[ise_weight_range][0], &l_q16, &h_q16, block_pixels_q16, color_box_q16))
			break;

		bool was_improved = try_mode11(num_pixels, blk_endpoints, blk_weights, cur_block_error, best_submode,
			l_q16, h_q16,
			block_pixels_half, num_weight_levels, ise_weight_range, coptions, direct_only, ise_endpoint_range, constrain_ise_weight8_selectors,
			first_submode, last_submode);

		if (!was_improved)
			break;

		// It's improved, so let's take the new weight indices.
		memcpy(trial_blk_weights, blk_weights, num_pixels);

	} // pass
#endif
		
	if (uber_mode)
	{
		// Try varying the current best weight indices. This can be expanded/improved, but at potentially great cost.

		uint8_t temp_astc_weights[16];
		memcpy(temp_astc_weights, trial_blk_weights, num_pixels);

		uint32_t min_lin_sel = 256, max_lin_sel = 0;
		for (uint32_t i = 0; i < num_pixels; i++)
		{
			const uint32_t astc_sel = temp_astc_weights[i];

			const uint32_t lin_sel = g_map_astc_to_linear_order[ise_weight_range][astc_sel];
			assert(lin_sel < num_weight_levels);

			min_lin_sel = minimumu(min_lin_sel, lin_sel);
			max_lin_sel = maximumu(max_lin_sel, lin_sel);
		}

		bool was_improved = false;
		(void)was_improved;

		{
			bool weights_changed = false;
			uint8_t trial_weights[16];
			for (uint32_t i = 0; i < num_pixels; i++)
			{
				uint32_t astc_sel = temp_astc_weights[i];
				uint32_t lin_sel = g_map_astc_to_linear_order[ise_weight_range][astc_sel];

				if ((lin_sel == min_lin_sel) && (lin_sel < (num_weight_levels - 1)))
				{
					lin_sel++;
					weights_changed = true;
				}

				trial_weights[i] = g_map_linear_to_astc_order[ise_weight_range][lin_sel];
			}

			if (weights_changed)
			{
				vec3F l_q16, h_q16;
				if (compute_least_squares_endpoints_rgb(num_pixels, trial_weights, &g_astc_ls_weights_ise[ise_weight_range][0], &l_q16, &h_q16, block_pixels_q16, color_box_q16))
				{
					if (try_mode11(num_pixels, blk_endpoints, blk_weights, cur_block_error, best_submode,
						l_q16, h_q16,
						block_pixels_half, num_weight_levels, ise_weight_range, coptions, direct_only, ise_endpoint_range, constrain_ise_weight8_selectors, 
						first_submode, last_submode))
					{
						was_improved = true;
					}
				}
			}
		}

		{
			bool weights_changed = false;
			uint8_t trial_weights[16];
			for (uint32_t i = 0; i < num_pixels; i++)
			{
				uint32_t astc_sel = temp_astc_weights[i];
				uint32_t lin_sel = g_map_astc_to_linear_order[ise_weight_range][astc_sel];

				if ((lin_sel == max_lin_sel) && (lin_sel > 0))
				{
					lin_sel--;
					weights_changed = true;
				}

				trial_weights[i] = g_map_linear_to_astc_order[ise_weight_range][lin_sel];
			}

			if (weights_changed)
			{
				vec3F l_q16, h_q16;
				if (compute_least_squares_endpoints_rgb(num_pixels, trial_weights, &g_astc_ls_weights_ise[ise_weight_range][0], &l_q16, &h_q16, block_pixels_q16, color_box_q16))
				{
					if (try_mode11(num_pixels, blk_endpoints, blk_weights, cur_block_error, best_submode,
						l_q16, h_q16,
						block_pixels_half, num_weight_levels, ise_weight_range, coptions, direct_only, ise_endpoint_range, constrain_ise_weight8_selectors,
						first_submode, last_submode))
					{
						was_improved = true;
					}
				}
			}
		}

		{
			bool weights_changed = false;
			uint8_t trial_weights[16];
			for (uint32_t i = 0; i < num_pixels; i++)
			{
				uint32_t astc_sel = temp_astc_weights[i];
				uint32_t lin_sel = g_map_astc_to_linear_order[ise_weight_range][astc_sel];

				if ((lin_sel == max_lin_sel) && (lin_sel > 0))
				{
					lin_sel--;
					weights_changed = true;
				}
				else if ((lin_sel == min_lin_sel) && (lin_sel < (num_weight_levels - 1)))
				{
					lin_sel++;
					weights_changed = true;
				}

				trial_weights[i] = g_map_linear_to_astc_order[ise_weight_range][lin_sel];
			}

			if (weights_changed)
			{
				vec3F l_q16, h_q16;
				if (compute_least_squares_endpoints_rgb(num_pixels, trial_weights, &g_astc_ls_weights_ise[ise_weight_range][0], &l_q16, &h_q16, block_pixels_q16, color_box_q16))
				{
					if (try_mode11(num_pixels, blk_endpoints, blk_weights, cur_block_error, best_submode,
						l_q16, h_q16,
						block_pixels_half, num_weight_levels, ise_weight_range, coptions, direct_only, ise_endpoint_range, constrain_ise_weight8_selectors,
						first_submode, last_submode))
					{
						was_improved = true;
					}
				}
			}
		}
	} // uber_mode

	return cur_block_error;
}

//--------------------------------------------------------------------------------------------------------------------------

static double encode_astc_hdr_block_mode_7(
	uint32_t num_pixels, const vec4F* pBlock_pixels,
	uint32_t ise_weight_range,
	uint32_t& best_submode,
	double cur_block_error,
	uint8_t* blk_endpoints,  //[4]
	uint8_t* blk_weights, // [num_pixels]
	const astc_hdr_codec_options& coptions,
	uint32_t ise_endpoint_range)
{
	assert((num_pixels >= 1) && (num_pixels <= 16));
	assert((ise_weight_range >= 1) && (ise_weight_range <= 10));
	assert((ise_endpoint_range >= astc_helpers::FIRST_VALID_ENDPOINT_ISE_RANGE) && (ise_endpoint_range <= astc_helpers::LAST_VALID_ENDPOINT_ISE_RANGE));
	const uint32_t num_weight_levels = astc_helpers::get_ise_levels(ise_weight_range);

	const uint32_t MAX_WEIGHT_LEVELS = 24;
	assert(num_weight_levels <= MAX_WEIGHT_LEVELS);
	BASISU_NOTE_UNUSED(MAX_WEIGHT_LEVELS);

	best_submode = 0;

	half_float block_pixels_half[16][3];

	vec4F block_pixels_q16[16];
	for (uint32_t i = 0; i < num_pixels; i++)
	{
		block_pixels_half[i][0] = float_to_half_non_neg_no_nan_inf(pBlock_pixels[i][0]);
		block_pixels_q16[i][0] = (float)half_to_qlog16(block_pixels_half[i][0]);

		block_pixels_half[i][1] = float_to_half_non_neg_no_nan_inf(pBlock_pixels[i][1]);
		block_pixels_q16[i][1] = (float)half_to_qlog16(block_pixels_half[i][1]);

		block_pixels_half[i][2] = float_to_half_non_neg_no_nan_inf(pBlock_pixels[i][2]);
		block_pixels_q16[i][2] = (float)half_to_qlog16(block_pixels_half[i][2]);

		block_pixels_q16[i][3] = 0.0f;
	}

	vec3F block_mean_color_q16(calc_mean(num_pixels, block_pixels_q16));

	vec3F block_axis_q16(0.577350259f);

	aabb3F color_box_q16(cInitExpand);

	float l = 1e+30f, h = -1e+30f;
	for (uint32_t i = 0; i < num_pixels; i++)
	{
		color_box_q16.expand(block_pixels_q16[i]);

		vec3F k(vec3F(block_pixels_q16[i]) - block_mean_color_q16);
		float kd = k.dot(block_axis_q16);

		l = basisu::minimum<float>(l, kd);
		h = basisu::maximum<float>(h, kd);
	}

	vec3F low_color_q16(interp_color(block_mean_color_q16, block_axis_q16, l, color_box_q16, color_box_q16));
	vec3F high_color_q16(interp_color(block_mean_color_q16, block_axis_q16, h, color_box_q16, color_box_q16));

	low_color_q16.clamp(0.0f, MAX_QLOG16_VAL);
	high_color_q16.clamp(0.0f, MAX_QLOG16_VAL);

	vec3F diff(high_color_q16 - low_color_q16);
	float s_q16 = diff.dot(block_axis_q16) * block_axis_q16[0];

	uint8_t trial_blk_endpoints[NUM_MODE7_ENDPOINTS];
	uint8_t trial_blk_weights[16];
	uint32_t trial_best_submode = 0;

	clear_obj(trial_blk_endpoints);
	clear_obj(trial_blk_weights);

	double trial_blk_error = 1e+30f;

	bool did_improve = try_mode7(num_pixels, trial_blk_endpoints, trial_blk_weights, trial_blk_error, trial_best_submode,
		high_color_q16, ceilf(s_q16),
		block_pixels_half, num_weight_levels, ise_weight_range, coptions, ise_endpoint_range);

	// If we couldn't find ANY usable solution due to endpoint quantization, just return. There's nothing we can do.
	if (!did_improve)
	{
		return cur_block_error;
	}

	// Did the solution improve?
	if (trial_blk_error < cur_block_error)
	{
		cur_block_error = trial_blk_error;
		memcpy(blk_endpoints, trial_blk_endpoints, NUM_MODE7_ENDPOINTS);
		memcpy(blk_weights, trial_blk_weights, num_pixels);
		best_submode = trial_best_submode;
	}

	const float one_over_num_pixels = 1.0f / (float)num_pixels;

	const uint32_t NUM_TRIALS = 2;
	for (uint32_t trial = 0; trial < NUM_TRIALS; trial++)
	{
		// Given a set of selectors and S, try to compute a better high color
		vec3F new_high_color_q16(block_mean_color_q16);

		int e[2][3];
		int cur_s = 0;
		if (!decode_mode7_to_qlog12(trial_blk_endpoints, e, &cur_s, ise_endpoint_range))
			break;

		cur_s <<= 4;

		for (uint32_t i = 0; i < num_pixels; i++)
		{
			uint32_t astc_sel = trial_blk_weights[i];
			float lerp = g_ise_weight_lerps[ise_weight_range][astc_sel + 1] * (1.0f / 64.0f);

			float k = (float)cur_s * (1.0f - lerp) * one_over_num_pixels;
			new_high_color_q16[0] += k;
			new_high_color_q16[1] += k;
			new_high_color_q16[2] += k;
		}

		bool improved = try_mode7(num_pixels, blk_endpoints, blk_weights, cur_block_error, best_submode,
			new_high_color_q16, (float)cur_s,
			block_pixels_half, num_weight_levels, ise_weight_range, coptions, ise_endpoint_range);

		if (improved)
		{
			memcpy(trial_blk_endpoints, blk_endpoints, NUM_MODE7_ENDPOINTS);
			memcpy(trial_blk_weights, blk_weights, num_pixels);
		}

		// Given a set of selectors and a high color, try to compute a better S.
		float t = 0.0f;

		for (uint32_t i = 0; i < num_pixels; i++)
		{
			uint32_t astc_sel = trial_blk_weights[i];
			float lerp = g_ise_weight_lerps[ise_weight_range][astc_sel + 1] * (1.0f / 64.0f);

			t += (1.0f) - lerp;
		}

		t *= one_over_num_pixels;

		//int e[2][3];
		if (!decode_mode7_to_qlog12(trial_blk_endpoints, e, nullptr, ise_endpoint_range))
			break;

		vec3F cur_h_q16((float)(e[1][0] << 4), (float)(e[1][1] << 4), (float)(e[1][2] << 4));

		if (fabs(t) > .0000125f)
		{
			float s_r = (cur_h_q16[0] - block_mean_color_q16[0]) / t;
			float s_g = (cur_h_q16[1] - block_mean_color_q16[1]) / t;
			float s_b = (cur_h_q16[2] - block_mean_color_q16[2]) / t;

			// TODO: gather statistics on these
			if (try_mode7(num_pixels, blk_endpoints, blk_weights, cur_block_error, best_submode,
				cur_h_q16, ceilf(s_r),
				block_pixels_half, num_weight_levels, ise_weight_range, coptions, ise_endpoint_range))
			{
				improved = true;
			}

			if (try_mode7(num_pixels, blk_endpoints, blk_weights, cur_block_error, best_submode,
				cur_h_q16, ceilf(s_g),
				block_pixels_half, num_weight_levels, ise_weight_range, coptions, ise_endpoint_range))
			{
				improved = true;
			}

			if (try_mode7(num_pixels, blk_endpoints, blk_weights, cur_block_error, best_submode,
				cur_h_q16, ceilf(s_b),
				block_pixels_half, num_weight_levels, ise_weight_range, coptions, ise_endpoint_range))
			{
				improved = true;
			}

			if (try_mode7(num_pixels, blk_endpoints, blk_weights, cur_block_error, best_submode,
				cur_h_q16, ceilf((s_r + s_g + s_b) / 3.0f),
				block_pixels_half, num_weight_levels, ise_weight_range, coptions, ise_endpoint_range))
			{
				improved = true;
			}
		}

		if (!improved)
			break;

		memcpy(trial_blk_endpoints, blk_endpoints, NUM_MODE7_ENDPOINTS);
		memcpy(trial_blk_weights, blk_weights, num_pixels);

	} // trial

	return cur_block_error;
}

//--------------------------------------------------------------------------------------------------------------------------

static bool pack_solid(const vec4F* pBlock_linear_colors, basisu::vector<astc_hdr_pack_results>& all_results, const astc_hdr_codec_options& coptions)
{
	float r = 0.0f, g = 0.0f, b = 0.0f;

	const float LOG_BIAS = .125f;

	bool solid_block = true;
	for (uint32_t i = 0; i < 16; i++)
	{
		if ((pBlock_linear_colors[0][0] != pBlock_linear_colors[i][0]) ||
			(pBlock_linear_colors[0][1] != pBlock_linear_colors[i][1]) ||
			(pBlock_linear_colors[0][2] != pBlock_linear_colors[i][2]))
		{
			solid_block = false;
		}

		r += log2f(pBlock_linear_colors[i][0] + LOG_BIAS);
		g += log2f(pBlock_linear_colors[i][1] + LOG_BIAS);
		b += log2f(pBlock_linear_colors[i][2] + LOG_BIAS);
	}

	if (solid_block)
	{
		r = pBlock_linear_colors[0][0];
		g = pBlock_linear_colors[0][1];
		b = pBlock_linear_colors[0][2];
	}
	else
	{
		r = maximum<float>(0.0f, powf(2.0f, r * (1.0f / 16.0f)) - LOG_BIAS);
		g = maximum<float>(0.0f, powf(2.0f, g * (1.0f / 16.0f)) - LOG_BIAS);
		b = maximum<float>(0.0f, powf(2.0f, b * (1.0f / 16.0f)) - LOG_BIAS);

		// for safety
		r = minimum<float>(r, MAX_HALF_FLOAT);
		g = minimum<float>(g, MAX_HALF_FLOAT);
		b = minimum<float>(b, MAX_HALF_FLOAT);
	}

	half_float rh = float_to_half_non_neg_no_nan_inf(r), gh = float_to_half_non_neg_no_nan_inf(g), bh = float_to_half_non_neg_no_nan_inf(b), ah = float_to_half_non_neg_no_nan_inf(1.0f);

	astc_hdr_pack_results results;
	results.clear();

	uint8_t* packed_blk = (uint8_t*)&results.m_solid_blk;
	results.m_is_solid = true;

	packed_blk[0] = 0b11111100;
	packed_blk[1] = 255;
	packed_blk[2] = 255;
	packed_blk[3] = 255;
	packed_blk[4] = 255;
	packed_blk[5] = 255;
	packed_blk[6] = 255;
	packed_blk[7] = 255;

	packed_blk[8] = (uint8_t)rh;
	packed_blk[9] = (uint8_t)(rh >> 8);
	packed_blk[10] = (uint8_t)gh;
	packed_blk[11] = (uint8_t)(gh >> 8);
	packed_blk[12] = (uint8_t)bh;
	packed_blk[13] = (uint8_t)(bh >> 8);
	packed_blk[14] = (uint8_t)ah;
	packed_blk[15] = (uint8_t)(ah >> 8);

	results.m_best_block_error = 0;

	if (!solid_block)
	{
		const float R_WEIGHT = coptions.m_r_err_scale;
		const float G_WEIGHT = coptions.m_g_err_scale;

		// This MUST match how errors are computed in eval_selectors().
		for (uint32_t i = 0; i < 16; i++)
		{
			half_float dr = float_to_half_non_neg_no_nan_inf(pBlock_linear_colors[i][0]), dg = float_to_half_non_neg_no_nan_inf(pBlock_linear_colors[i][1]), db = float_to_half_non_neg_no_nan_inf(pBlock_linear_colors[i][2]);
			double rd = q(rh) - q(dr);
			double gd = q(gh) - q(dg);
			double bd = q(bh) - q(db);

			double e = R_WEIGHT * (rd * rd) + G_WEIGHT * (gd * gd) + bd * bd;

			results.m_best_block_error += e;
		}
	}

	const half_float hc[3] = { rh, gh, bh };

	bc6h_enc_block_solid_color(&results.m_bc6h_block, hc);

	all_results.push_back(results);

	return solid_block;
}

//--------------------------------------------------------------------------------------------------------------------------

static void pack_mode11(
	const vec4F* pBlock_linear_colors, 
	basisu::vector<astc_hdr_pack_results>& all_results, 
	const astc_hdr_codec_options& coptions, 
	uint32_t first_weight_ise_range, uint32_t last_weight_ise_range, bool constrain_ise_weight8_selectors)
{
	uint8_t trial_endpoints[NUM_MODE11_ENDPOINTS], trial_weights[16];
	uint32_t trial_submode11 = 0;

	clear_obj(trial_endpoints);
	clear_obj(trial_weights);
		
	for (uint32_t weight_ise_range = first_weight_ise_range; weight_ise_range <= last_weight_ise_range; weight_ise_range++)
	{
		const bool direct_only = coptions.m_mode11_direct_only;
		
		uint32_t endpoint_ise_range = astc_helpers::BISE_256_LEVELS;
		if (weight_ise_range == astc_helpers::BISE_16_LEVELS)
			endpoint_ise_range = astc_helpers::BISE_192_LEVELS;
		else
		{
			assert(weight_ise_range < astc_helpers::BISE_16_LEVELS);
		}
				
		double trial_error = encode_astc_hdr_block_mode_11(16, pBlock_linear_colors, weight_ise_range, trial_submode11, 1e+30f, trial_endpoints, trial_weights, coptions, direct_only, 
			endpoint_ise_range, coptions.m_mode11_uber_mode && (weight_ise_range >= astc_helpers::BISE_4_LEVELS) && coptions.m_allow_uber_mode, constrain_ise_weight8_selectors, coptions.m_first_mode11_submode, coptions.m_last_mode11_submode);

		if (trial_error < 1e+30f)
		{
			astc_hdr_pack_results results;
			results.clear();

			results.m_best_block_error = trial_error;

			results.m_best_submodes[0] = trial_submode11;
			results.m_constrained_weights = constrain_ise_weight8_selectors;
						
			results.m_best_blk.m_num_partitions = 1;
			results.m_best_blk.m_color_endpoint_modes[0] = 11;
			results.m_best_blk.m_weight_ise_range = weight_ise_range;
			results.m_best_blk.m_endpoint_ise_range = endpoint_ise_range;
			
			memcpy(results.m_best_blk.m_endpoints, trial_endpoints, NUM_MODE11_ENDPOINTS);
			memcpy(results.m_best_blk.m_weights, trial_weights, 16);

#ifdef _DEBUG
			{
				half_float block_pixels_half[16][3];

				vec4F block_pixels_q16[16];
				for (uint32_t i = 0; i < 16; i++)
				{
					block_pixels_half[i][0] = float_to_half_non_neg_no_nan_inf(pBlock_linear_colors[i][0]);
					block_pixels_half[i][1] = float_to_half_non_neg_no_nan_inf(pBlock_linear_colors[i][1]);
					block_pixels_half[i][2] = float_to_half_non_neg_no_nan_inf(pBlock_linear_colors[i][2]);
				}
				
				half_float unpacked_astc_blk_rgba[4][4][4];
				bool res = astc_helpers::decode_block(results.m_best_blk, unpacked_astc_blk_rgba, 4, 4, astc_helpers::cDecodeModeHDR16);
				assert(res);

				half_float unpacked_astc_blk_rgb[4][4][3];
				for (uint32_t y = 0; y < 4; y++)
					for (uint32_t x = 0; x < 4; x++)
						for (uint32_t c = 0; c < 3; c++)
							unpacked_astc_blk_rgb[y][x][c] = unpacked_astc_blk_rgba[y][x][c];

				double cmp_err = compute_block_error(&block_pixels_half[0][0], &unpacked_astc_blk_rgb[0][0][0], coptions);
				assert(results.m_best_block_error == cmp_err);
			}
#endif

			// transcode to BC6H
			assert(results.m_best_blk.m_color_endpoint_modes[0] == 11);
			
			// Get qlog12 endpoints
			int e[2][3];
			bool success = decode_mode11_to_qlog12(results.m_best_blk.m_endpoints, e, results.m_best_blk.m_endpoint_ise_range);
			assert(success);
			BASISU_NOTE_UNUSED(success);

			// Transform endpoints to half float
			half_float h_e[3][2] =
			{
				{ qlog_to_half(e[0][0], 12), qlog_to_half(e[1][0], 12) },
				{ qlog_to_half(e[0][1], 12), qlog_to_half(e[1][1], 12) },
				{ qlog_to_half(e[0][2], 12), qlog_to_half(e[1][2], 12) }
			};

			// Transcode to bc6h
			success = transcode_bc6h_1subset(h_e, results.m_best_blk, results.m_bc6h_block);
			assert(success);

			all_results.push_back(results);
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------

static void pack_mode7_single_part(const vec4F* pBlock_linear_colors, basisu::vector<astc_hdr_pack_results>& all_results, const astc_hdr_codec_options& coptions)
{
	uint8_t trial_endpoints[NUM_MODE7_ENDPOINTS], trial_weights[16];
	uint32_t trial_submode7 = 0;

	clear_obj(trial_endpoints);
	clear_obj(trial_weights);

	for (uint32_t weight_ise_range = coptions.m_first_mode7_part1_weight_ise_range; weight_ise_range <= coptions.m_last_mode7_part1_weight_ise_range; weight_ise_range++)
	{
		const uint32_t ise_endpoint_range = astc_helpers::BISE_256_LEVELS;

		double trial_error = encode_astc_hdr_block_mode_7(16, pBlock_linear_colors, weight_ise_range, trial_submode7, 1e+30f, trial_endpoints, trial_weights, coptions, ise_endpoint_range);

		if (trial_error < 1e+30f)
		{
			astc_hdr_pack_results results;
			results.clear();

			results.m_best_block_error = trial_error;

			results.m_best_submodes[0] = trial_submode7;
			
			results.m_best_blk.m_num_partitions = 1;
			results.m_best_blk.m_color_endpoint_modes[0] = 7;
			results.m_best_blk.m_weight_ise_range = weight_ise_range;
			results.m_best_blk.m_endpoint_ise_range = ise_endpoint_range;
			
			memcpy(results.m_best_blk.m_endpoints, trial_endpoints, NUM_MODE7_ENDPOINTS);
			memcpy(results.m_best_blk.m_weights, trial_weights, 16);

			// transcode to BC6H
			assert(results.m_best_blk.m_color_endpoint_modes[0] == 7);
			
			// Get qlog12 endpoints
			int e[2][3];
			if (!decode_mode7_to_qlog12(results.m_best_blk.m_endpoints, e, nullptr, results.m_best_blk.m_endpoint_ise_range))
				continue;

			// Transform endpoints to half float
			half_float h_e[3][2] =
			{
				{ qlog_to_half(e[0][0], 12), qlog_to_half(e[1][0], 12) },
				{ qlog_to_half(e[0][1], 12), qlog_to_half(e[1][1], 12) },
				{ qlog_to_half(e[0][2], 12), qlog_to_half(e[1][2], 12) }
			};

			// Transcode to bc6h
			bool status = transcode_bc6h_1subset(h_e, results.m_best_blk, results.m_bc6h_block);
			assert(status);
			(void)status;

			all_results.push_back(results);
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------

static bool estimate_partition2(const vec4F* pBlock_pixels, int* pBest_parts, uint32_t num_best_parts)
{
	assert(num_best_parts <= basist::TOTAL_ASTC_BC6H_COMMON_PARTITIONS2);

	vec3F training_vecs[16], mean(0.0f);

	for (uint32_t i = 0; i < 16; i++)
	{
		vec3F& v = training_vecs[i];

		v[0] = (float)float_to_half_non_neg_no_nan_inf(pBlock_pixels[i][0]);
		v[1] = (float)float_to_half_non_neg_no_nan_inf(pBlock_pixels[i][1]);
		v[2] = (float)float_to_half_non_neg_no_nan_inf(pBlock_pixels[i][2]);

		mean += v;
	}
	mean *= (1.0f / 16.0f);

	vec3F cluster_centroids[2] = { mean - vec3F(.1f), mean + vec3F(.1f) };

	uint32_t cluster_pixels[2][16];
	uint32_t num_cluster_pixels[2];
	vec3F new_cluster_means[2];

	for (uint32_t s = 0; s < 4; s++)
	{
		num_cluster_pixels[0] = 0;
		num_cluster_pixels[1] = 0;

		new_cluster_means[0].clear();
		new_cluster_means[1].clear();

		for (uint32_t i = 0; i < 16; i++)
		{
			float d0 = training_vecs[i].squared_distance(cluster_centroids[0]);
			float d1 = training_vecs[i].squared_distance(cluster_centroids[1]);

			if (d0 < d1)
			{
				cluster_pixels[0][num_cluster_pixels[0]] = i;
				new_cluster_means[0] += training_vecs[i];
				num_cluster_pixels[0]++;
			}
			else
			{
				cluster_pixels[1][num_cluster_pixels[1]] = i;
				new_cluster_means[1] += training_vecs[i];
				num_cluster_pixels[1]++;
			}
		}

		if (!num_cluster_pixels[0] || !num_cluster_pixels[1])
			return false;

		cluster_centroids[0] = new_cluster_means[0] / (float)num_cluster_pixels[0];
		cluster_centroids[1] = new_cluster_means[1] / (float)num_cluster_pixels[1];
	}

	int desired_parts[4][4]; // [y][x]
	for (uint32_t p = 0; p < 2; p++)
	{
		for (uint32_t i = 0; i < num_cluster_pixels[p]; i++)
		{
			const uint32_t pix_index = cluster_pixels[p][i];

			desired_parts[pix_index >> 2][pix_index & 3] = p;
		}
	}

	uint32_t part_similarity[basist::TOTAL_ASTC_BC6H_COMMON_PARTITIONS2];

	for (uint32_t part_index = 0; part_index < basist::TOTAL_ASTC_BC6H_COMMON_PARTITIONS2; part_index++)
	{
		const uint32_t bc7_pattern = basist::g_astc_bc7_common_partitions2[part_index].m_bc7;

		int total_sim_non_inv = 0;
		int total_sim_inv = 0;

		for (uint32_t y = 0; y < 4; y++)
		{
			for (uint32_t x = 0; x < 4; x++)
			{
				int part = basist::g_bc7_partition2[16 * bc7_pattern + x + y * 4];

				if (part == desired_parts[y][x])
					total_sim_non_inv++;

				if ((part ^ 1) == desired_parts[y][x])
					total_sim_inv++;
			}
		}

		int total_sim = maximum(total_sim_non_inv, total_sim_inv);

		part_similarity[part_index] = (total_sim << 8) | part_index;

	} // part_index;

	std::sort(part_similarity, part_similarity + basist::TOTAL_ASTC_BC6H_COMMON_PARTITIONS2);

	for (uint32_t i = 0; i < num_best_parts; i++)
		pBest_parts[i] = part_similarity[(basist::TOTAL_ASTC_BC6H_COMMON_PARTITIONS2 - 1) - i] & 0xFF;

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------

static void pack_mode7_2part(const vec4F* pBlock_linear_colors, basisu::vector<astc_hdr_pack_results>& all_results, const astc_hdr_codec_options& coptions,
	int num_estimated_partitions, const int *pEstimated_partitions,
	uint32_t first_weight_ise_range, uint32_t last_weight_ise_range)
{
	assert(coptions.m_mode7_part2_part_masks);

	astc_helpers::log_astc_block trial_blk;
	clear_obj(trial_blk);
	trial_blk.m_grid_width = 4;
	trial_blk.m_grid_height = 4;

	trial_blk.m_num_partitions = 2;
	trial_blk.m_color_endpoint_modes[0] = 7;
	trial_blk.m_color_endpoint_modes[1] = 7;

	uint32_t first_part_index = 0, last_part_index = basist::TOTAL_ASTC_BC6H_COMMON_PARTITIONS2;
		
	if (num_estimated_partitions)
	{
		first_part_index = 0;
		last_part_index = num_estimated_partitions;
	}
	
	for (uint32_t part_index_iter = first_part_index; part_index_iter < last_part_index; ++part_index_iter)
	{
		uint32_t part_index;
		if (num_estimated_partitions)
		{
			part_index = pEstimated_partitions[part_index_iter];
			assert(part_index < basist::TOTAL_ASTC_BC6H_COMMON_PARTITIONS2);
		}
		else
		{
			part_index = part_index_iter;
			if (((1U << part_index) & coptions.m_mode7_part2_part_masks) == 0)
				continue;
		}
								
		const uint32_t astc_pattern = basist::g_astc_bc7_common_partitions2[part_index].m_astc;
		const uint32_t bc7_pattern = basist::g_astc_bc7_common_partitions2[part_index].m_bc7;
		const bool invert_flag = basist::g_astc_bc7_common_partitions2[part_index].m_invert;

		vec4F part_pixels[2][16];
		uint32_t pixel_part_index[4][4]; // [y][x]
		uint32_t num_part_pixels[2] = { 0, 0 };

		// Extract each subset's texels for this partition pattern
		for (uint32_t y = 0; y < 4; y++)
		{
			for (uint32_t x = 0; x < 4; x++)
			{
				uint32_t part = basist::g_bc7_partition2[16 * bc7_pattern + x + y * 4];
				if (invert_flag)
					part = 1 - part;

				pixel_part_index[y][x] = part;
				part_pixels[part][num_part_pixels[part]] = pBlock_linear_colors[x + y * 4];

				num_part_pixels[part]++;
			}
		}

		trial_blk.m_partition_id = astc_pattern;
				
		for (uint32_t weight_ise_range = first_weight_ise_range; weight_ise_range <= last_weight_ise_range; weight_ise_range++)
		{
			assert(weight_ise_range <= astc_helpers::BISE_8_LEVELS);

			uint32_t ise_endpoint_range = astc_helpers::BISE_256_LEVELS;
			if (weight_ise_range == astc_helpers::BISE_5_LEVELS)
				ise_endpoint_range = astc_helpers::BISE_192_LEVELS;
			else if (weight_ise_range == astc_helpers::BISE_6_LEVELS)
				ise_endpoint_range = astc_helpers::BISE_128_LEVELS;
			else if (weight_ise_range == astc_helpers::BISE_8_LEVELS)
				ise_endpoint_range = astc_helpers::BISE_80_LEVELS;

			uint8_t trial_endpoints[2][NUM_MODE7_ENDPOINTS], trial_weights[2][16];
			uint32_t trial_submode7[2];

			clear_obj(trial_endpoints);
			clear_obj(trial_weights);
			clear_obj(trial_submode7);

			double total_trial_err = 0;
			for (uint32_t pack_part_index = 0; pack_part_index < 2; pack_part_index++)
			{
				total_trial_err += encode_astc_hdr_block_mode_7(
					num_part_pixels[pack_part_index], &part_pixels[pack_part_index][0],
					weight_ise_range, trial_submode7[pack_part_index], 1e+30f,
					&trial_endpoints[pack_part_index][0], &trial_weights[pack_part_index][0], coptions, ise_endpoint_range);

			} // pack_part_index

			if (total_trial_err < 1e+30f)
			{
				trial_blk.m_weight_ise_range = weight_ise_range;
				trial_blk.m_endpoint_ise_range = ise_endpoint_range;

				for (uint32_t pack_part_index = 0; pack_part_index < 2; pack_part_index++)
					memcpy(&trial_blk.m_endpoints[pack_part_index * NUM_MODE7_ENDPOINTS], &trial_endpoints[pack_part_index][0], NUM_MODE7_ENDPOINTS);

				uint32_t src_pixel_index[2] = { 0, 0 };
				for (uint32_t y = 0; y < 4; y++)
				{
					for (uint32_t x = 0; x < 4; x++)
					{
						uint32_t p = pixel_part_index[y][x];
						trial_blk.m_weights[x + y * 4] = trial_weights[p][src_pixel_index[p]++];
					}
				}
								
				astc_hdr_pack_results results;
				results.clear();

				results.m_best_block_error = total_trial_err;
				results.m_best_submodes[0] = trial_submode7[0];
				results.m_best_submodes[1] = trial_submode7[1];
				results.m_best_pat_index = part_index;

				results.m_best_blk = trial_blk;

				bool status = transcode_bc6h_2subsets(part_index, results.m_best_blk, results.m_bc6h_block);
				assert(status);
				BASISU_NOTE_UNUSED(status);

				all_results.push_back(results);
			}

		} // weight_ise_range

	} // part_index
}

//--------------------------------------------------------------------------------------------------------------------------

static void pack_mode11_2part(const vec4F* pBlock_linear_colors, basisu::vector<astc_hdr_pack_results>& all_results, const astc_hdr_codec_options& coptions,
	int num_estimated_partitions, const int* pEstimated_partitions)
{
	assert(coptions.m_mode11_part2_part_masks);

	astc_helpers::log_astc_block trial_blk;
	clear_obj(trial_blk);
	trial_blk.m_grid_width = 4;
	trial_blk.m_grid_height = 4;

	trial_blk.m_num_partitions = 2;
	trial_blk.m_color_endpoint_modes[0] = 11;
	trial_blk.m_color_endpoint_modes[1] = 11;
			
	uint32_t first_part_index = 0, last_part_index = basist::TOTAL_ASTC_BC6H_COMMON_PARTITIONS2;

	if (num_estimated_partitions)
	{
		first_part_index = 0;
		last_part_index = num_estimated_partitions;
	}

	for (uint32_t part_index_iter = first_part_index; part_index_iter < last_part_index; ++part_index_iter)
	{
		uint32_t part_index;
		if (num_estimated_partitions)
		{
			part_index = pEstimated_partitions[part_index_iter];
			assert(part_index < basist::TOTAL_ASTC_BC6H_COMMON_PARTITIONS2);
		}
		else
		{
			part_index = part_index_iter;
			if (((1U << part_index) & coptions.m_mode11_part2_part_masks) == 0)
				continue;
		}

		const uint32_t astc_pattern = basist::g_astc_bc7_common_partitions2[part_index].m_astc;
		const uint32_t bc7_pattern = basist::g_astc_bc7_common_partitions2[part_index].m_bc7;
		const bool invert_flag = basist::g_astc_bc7_common_partitions2[part_index].m_invert;

		vec4F part_pixels[2][16];
		uint32_t pixel_part_index[4][4]; // [y][x]
		uint32_t num_part_pixels[2] = { 0, 0 };

		// Extract each subset's texels for this partition pattern
		for (uint32_t y = 0; y < 4; y++)
		{
			for (uint32_t x = 0; x < 4; x++)
			{
				uint32_t part = basist::g_bc7_partition2[16 * bc7_pattern + x + y * 4];
				if (invert_flag)
					part = 1 - part;

				pixel_part_index[y][x] = part;
				part_pixels[part][num_part_pixels[part]] = pBlock_linear_colors[x + y * 4];

				num_part_pixels[part]++;
			}
		}
				
		trial_blk.m_partition_id = astc_pattern;
						
		for (uint32_t weight_ise_range = coptions.m_first_mode11_part2_weight_ise_range; weight_ise_range <= coptions.m_last_mode11_part2_weight_ise_range; weight_ise_range++)
		{
			bool direct_only = false;
			uint32_t ise_endpoint_range = astc_helpers::BISE_64_LEVELS;
			if (weight_ise_range == astc_helpers::BISE_4_LEVELS)
				ise_endpoint_range = astc_helpers::BISE_40_LEVELS;

			uint8_t trial_endpoints[2][NUM_MODE11_ENDPOINTS], trial_weights[2][16];
			uint32_t trial_submode11[2];

			clear_obj(trial_endpoints); 
			clear_obj(trial_weights);
			clear_obj(trial_submode11);

			double total_trial_err = 0;
			for (uint32_t pack_part_index = 0; pack_part_index < 2; pack_part_index++)
			{
				total_trial_err += encode_astc_hdr_block_mode_11(
					num_part_pixels[pack_part_index], &part_pixels[pack_part_index][0],
					weight_ise_range, trial_submode11[pack_part_index], 1e+30f,
					&trial_endpoints[pack_part_index][0], &trial_weights[pack_part_index][0], coptions,
					direct_only, ise_endpoint_range, coptions.m_mode11_uber_mode && (weight_ise_range >= astc_helpers::BISE_4_LEVELS) && coptions.m_allow_uber_mode, false,
					coptions.m_first_mode11_submode, coptions.m_last_mode11_submode);

			} // pack_part_index

			if (total_trial_err < 1e+30f)
			{
				trial_blk.m_weight_ise_range = weight_ise_range;
				trial_blk.m_endpoint_ise_range = ise_endpoint_range;

				for (uint32_t pack_part_index = 0; pack_part_index < 2; pack_part_index++)
					memcpy(&trial_blk.m_endpoints[pack_part_index * NUM_MODE11_ENDPOINTS], &trial_endpoints[pack_part_index][0], NUM_MODE11_ENDPOINTS);

				uint32_t src_pixel_index[2] = { 0, 0 };
				for (uint32_t y = 0; y < 4; y++)
				{
					for (uint32_t x = 0; x < 4; x++)
					{
						uint32_t p = pixel_part_index[y][x];
						trial_blk.m_weights[x + y * 4] = trial_weights[p][src_pixel_index[p]++];
					}
				}
								
				astc_hdr_pack_results results;
				results.clear();

				results.m_best_block_error = total_trial_err;
				results.m_best_submodes[0] = trial_submode11[0];
				results.m_best_submodes[1] = trial_submode11[1];
				results.m_best_pat_index = part_index;

				results.m_best_blk = trial_blk;

				bool status = transcode_bc6h_2subsets(part_index, results.m_best_blk, results.m_bc6h_block);
				assert(status);
				BASISU_NOTE_UNUSED(status);

				all_results.push_back(results);
			}

		} // weight_ise_range

	} // part_index
}

//--------------------------------------------------------------------------------------------------------------------------

bool g_astc_hdr_enc_initialized;

void astc_hdr_enc_init()
{
	if (g_astc_hdr_enc_initialized)
		return;

	astc_hdr_core_init();

	astc_helpers::init_tables(true);
			
	init_qlog_tables();

	encode_astc_hdr_init();
								
	g_astc_hdr_enc_initialized = true;
}

bool astc_hdr_enc_block(
	const float* pRGBPixels, 
	const astc_hdr_codec_options& coptions,
	basisu::vector<astc_hdr_pack_results>& all_results)
{
	assert(g_astc_hdr_enc_initialized);
	if (!g_astc_hdr_enc_initialized)
	{
		// astc_hdr_enc_init() MUST be called first.
		assert(0);
		return false;
	}

	all_results.resize(0);
				
	vec4F block_linear_colors[16];

	// Sanity check the input block.
	for (uint32_t i = 0; i < 16; i++)
	{
		for (uint32_t j = 0; j < 3; j++)
		{
			float v = pRGBPixels[i * 3 + j];

			if (std::isinf(v) || std::isnan(v))
			{
				// Input pixels cannot be NaN or +-Inf.
				assert(0);
				return false;
			}

			if (v < 0.0f)
			{
				// Input pixels cannot be signed.
				assert(0);
				return false;
			}

			if (v > MAX_HALF_FLOAT)
			{
				// Too large for half float.
				assert(0);
				return false;
			}
			
			block_linear_colors[i][j] = v;
		}
		
		block_linear_colors[i][3] = 1.0f;
	}

	assert(coptions.m_use_solid || coptions.m_use_mode11 || coptions.m_use_mode7_part2 || coptions.m_use_mode7_part1 || coptions.m_use_mode11_part2);
					
	bool is_solid = false;
	if (coptions.m_use_solid)
		is_solid = pack_solid(block_linear_colors, all_results, coptions);

	if (!is_solid)
	{
		if (coptions.m_use_mode11)
		{
			const size_t cur_num_results = all_results.size();

			pack_mode11(block_linear_colors, all_results, coptions, coptions.m_first_mode11_weight_ise_range, coptions.m_last_mode11_weight_ise_range, false);

			if (coptions.m_last_mode11_weight_ise_range == astc_helpers::BISE_16_LEVELS)
			{
				pack_mode11(block_linear_colors, all_results, coptions, astc_helpers::BISE_16_LEVELS, astc_helpers::BISE_16_LEVELS, true);
			}

			// If we couldn't get any mode 11 results at all, and we were restricted to just trying weight ISE range 8 (which required endpoint quantization) then 
			// fall back to weight ISE range 7 (which doesn't need any endpoint quantization).
			// This is to guarantee we always get at least 1 non-solid result.
			if (all_results.size() == cur_num_results)
			{
				if (coptions.m_first_mode11_weight_ise_range == astc_helpers::BISE_16_LEVELS)
				{
					pack_mode11(block_linear_colors, all_results, coptions, astc_helpers::BISE_12_LEVELS, astc_helpers::BISE_12_LEVELS, false);
				}
			}
		}
				
		if (coptions.m_use_mode7_part1)
		{
			// Mode 7 1-subset never requires endpoint quantization, so it cannot fail to find at least one usable solution.
			pack_mode7_single_part(block_linear_colors, all_results, coptions);
		}
				
		bool have_est = false;
		int best_parts[basist::TOTAL_ASTC_BC6H_COMMON_PARTITIONS2];

		if ((coptions.m_use_mode7_part2) || (coptions.m_use_mode11_part2))
		{
			if (coptions.m_use_estimated_partitions)
				have_est = estimate_partition2(block_linear_colors, best_parts, coptions.m_max_estimated_partitions);
		}

		if (coptions.m_use_mode7_part2)
		{
			const size_t cur_num_results = all_results.size();

			pack_mode7_2part(block_linear_colors, all_results, coptions, have_est ? coptions.m_max_estimated_partitions : 0, best_parts, 
				coptions.m_first_mode7_part2_weight_ise_range, coptions.m_last_mode7_part2_weight_ise_range);

			// If we couldn't find any packable 2-subset mode 7 results at weight levels >= 5 levels (which always requires endpoint quant), then try falling back to 
			// 5 levels which doesn't require endpoint quantization.
			if (all_results.size() == cur_num_results)
			{
				if (coptions.m_first_mode7_part2_weight_ise_range >= astc_helpers::BISE_5_LEVELS)
				{
					pack_mode7_2part(block_linear_colors, all_results, coptions, have_est ? coptions.m_max_estimated_partitions : 0, best_parts, 
						astc_helpers::BISE_4_LEVELS, astc_helpers::BISE_4_LEVELS);
				}
			}
		}
		
		if (coptions.m_use_mode11_part2)
		{
			// This always requires endpoint quant, so it could fail to find any usable solutions.
			pack_mode11_2part(block_linear_colors, all_results, coptions, have_est ? coptions.m_max_estimated_partitions : 0, best_parts);
		}
	}

	if (coptions.m_refine_weights)
	{
		// TODO: Move this above, do it once only.
		basist::half_float rgb_pixels_half[16 * 3];
		for (uint32_t i = 0; i < 16; i++)
		{
			rgb_pixels_half[i * 3 + 0] = float_to_half_non_neg_no_nan_inf(pRGBPixels[i * 3 + 0]);
			rgb_pixels_half[i * 3 + 1] = float_to_half_non_neg_no_nan_inf(pRGBPixels[i * 3 + 1]);
			rgb_pixels_half[i * 3 + 2] = float_to_half_non_neg_no_nan_inf(pRGBPixels[i * 3 + 2]);
		}

		for (uint32_t i = 0; i < all_results.size(); i++)
		{
			bool status = astc_hdr_refine_weights(rgb_pixels_half, all_results[i], coptions, coptions.m_bc6h_err_weight, &all_results[i].m_improved_via_refinement_flag);
			assert(status);
			BASISU_NOTE_UNUSED(status);
		}
	}

	return true;
}

bool astc_hdr_pack_results_to_block(astc_blk& dst_blk, const astc_hdr_pack_results& results)
{
	assert(g_astc_hdr_enc_initialized);
	if (!g_astc_hdr_enc_initialized)
		return false;

	if (results.m_is_solid)
	{
		memcpy(&dst_blk, &results.m_solid_blk, sizeof(results.m_solid_blk));
	}
	else
	{
		bool status = astc_helpers::pack_astc_block((astc_helpers::astc_block&)dst_blk, results.m_best_blk);
		if (!status)
		{
			assert(0);
			return false;
		}
	}

	return true;
}

// Refines a block's chosen weight indices, balancing BC6H and ASTC HDR error.
bool astc_hdr_refine_weights(const half_float *pSource_block, astc_hdr_pack_results& cur_results, const astc_hdr_codec_options& coptions, float bc6h_weight, bool *pImproved_flag)
{
	if (pImproved_flag)
		*pImproved_flag = false;

	if (cur_results.m_is_solid)
		return true;

	const uint32_t total_weights = astc_helpers::get_ise_levels(cur_results.m_best_blk.m_weight_ise_range);

	assert((total_weights >= 3) && (total_weights <= 16));

	double best_err[4][4];
	uint8_t best_weight[4][4];
	for (uint32_t y = 0; y < 4; y++)
	{
		for (uint32_t x = 0; x < 4; x++)
		{
			best_err[y][x] = 1e+30f;
			best_weight[y][x] = 0;
		}
	}

	astc_hdr_pack_results temp_results;

	const float c_weights[3] = { coptions.m_r_err_scale, coptions.m_g_err_scale, 1.0f };

	for (uint32_t weight_index = 0; weight_index < total_weights; weight_index++)
	{
		temp_results = cur_results;
		for (uint32_t i = 0; i < 16; i++)
			temp_results.m_best_blk.m_weights[i] = (uint8_t)weight_index;
		
		half_float unpacked_astc_blk_rgba[4][4][4];
		bool res = astc_helpers::decode_block(temp_results.m_best_blk, unpacked_astc_blk_rgba, 4, 4, astc_helpers::cDecodeModeHDR16);
		assert(res);

		basist::bc6h_block trial_bc6h_blk;
		res = basist::astc_hdr_transcode_to_bc6h(temp_results.m_best_blk, trial_bc6h_blk);
		assert(res);
				
		half_float unpacked_bc6h_blk[4][4][3];
		res = unpack_bc6h(&trial_bc6h_blk, unpacked_bc6h_blk, false);
		assert(res);
		BASISU_NOTE_UNUSED(res);

		for (uint32_t y = 0; y < 4; y++)
		{
			for (uint32_t x = 0; x < 4; x++)
			{
				double total_err = 0.0f;

				for (uint32_t c = 0; c < 3; c++)
				{
					const half_float orig_c = pSource_block[(x + y * 4) * 3 + c];
					const double orig_c_q = q(orig_c);
					
					const half_float astc_c = unpacked_astc_blk_rgba[y][x][c];
					const double astc_c_q = q(astc_c);
					const double astc_e = square(astc_c_q - orig_c_q) * c_weights[c];
					
					const half_float bc6h_c = unpacked_bc6h_blk[y][x][c];
					const double bc6h_c_q = q(bc6h_c);
					const double bc6h_e = square(bc6h_c_q - orig_c_q) * c_weights[c];

					const double overall_err = astc_e * (1.0f - bc6h_weight) + bc6h_e * bc6h_weight;

					total_err += overall_err;

				} //  c

				if (total_err < best_err[y][x])
				{
					best_err[y][x] = total_err;
					best_weight[y][x] = (uint8_t)weight_index;
				}

			} // x
		} // y

	} // weight_index

	bool any_changed = false;
	for (uint32_t i = 0; i < 16; i++)
	{
		if (cur_results.m_best_blk.m_weights[i] != best_weight[i >> 2][i & 3])
		{
			any_changed = true;
			break;
		}
	}

	if (any_changed)
	{
		memcpy(cur_results.m_best_blk.m_weights, best_weight, 16);

		{
			bool res = basist::astc_hdr_transcode_to_bc6h(cur_results.m_best_blk, cur_results.m_bc6h_block);
			assert(res);
			BASISU_NOTE_UNUSED(res);

			half_float unpacked_astc_blk_rgba[4][4][4];
			res = astc_helpers::decode_block(cur_results.m_best_blk, unpacked_astc_blk_rgba, 4, 4, astc_helpers::cDecodeModeHDR16);
			assert(res);

			half_float unpacked_astc_blk_rgb[4][4][3];
			for (uint32_t y = 0; y < 4; y++)
				for (uint32_t x = 0; x < 4; x++)
					for (uint32_t c = 0; c < 3; c++)
						unpacked_astc_blk_rgb[y][x][c] = unpacked_astc_blk_rgba[y][x][c];

			cur_results.m_best_block_error = compute_block_error(pSource_block, &unpacked_astc_blk_rgb[0][0][0], coptions);
		}

		if (pImproved_flag)
			*pImproved_flag = true;
	}

	return true;
}

void astc_hdr_block_stats::update(const astc_hdr_pack_results& log_blk)
{
	std::lock_guard<std::mutex> lck(m_mutex);

	m_total_blocks++;

	if (log_blk.m_improved_via_refinement_flag)
		m_total_refined++;

	if (log_blk.m_is_solid)
	{
		m_total_solid++;
	}
	else
	{
		int best_weight_range = log_blk.m_best_blk.m_weight_ise_range;

		if (log_blk.m_best_blk.m_color_endpoint_modes[0] == 7)
		{
			m_mode7_submode_hist[bounds_check(log_blk.m_best_submodes[0], 0U, 6U)]++;

			if (log_blk.m_best_blk.m_num_partitions == 2)
			{
				m_total_mode7_2part++;

				m_mode7_submode_hist[bounds_check(log_blk.m_best_submodes[1], 0U, 6U)]++;
				m_total_2part++;

				m_weight_range_hist_7_2part[bounds_check(best_weight_range, 0, 11)]++;

				m_part_hist[bounds_check(log_blk.m_best_pat_index, 0U, 32U)]++;
			}
			else
			{
				m_total_mode7_1part++;

				m_weight_range_hist_7[bounds_check(best_weight_range, 0, 11)]++;
			}
		}
		else
		{
			m_mode11_submode_hist[bounds_check(log_blk.m_best_submodes[0], 0U, 9U)]++;
			if (log_blk.m_constrained_weights)
				m_total_mode11_1part_constrained_weights++;

			if (log_blk.m_best_blk.m_num_partitions == 2)
			{
				m_total_mode11_2part++;

				m_mode11_submode_hist[bounds_check(log_blk.m_best_submodes[1], 0U, 9U)]++;
				m_total_2part++;

				m_weight_range_hist_11_2part[bounds_check(best_weight_range, 0, 11)]++;

				m_part_hist[bounds_check(log_blk.m_best_pat_index, 0U, 32U)]++;
			}
			else
			{
				m_total_mode11_1part++;

				m_weight_range_hist_11[bounds_check(best_weight_range, 0, 11)]++;
			}
		}
	}
}

void astc_hdr_block_stats::print()
{
	std::lock_guard<std::mutex> lck(m_mutex);

	assert(m_total_blocks);
	if (!m_total_blocks)
		return;

	printf("\nLow-level ASTC Encoder Statistics:\n");
	printf("Total blocks: %u\n", m_total_blocks);
	printf("Total solid: %u %3.2f%%\n", m_total_solid, (m_total_solid * 100.0f) / m_total_blocks);
	printf("Total refined: %u %3.2f%%\n", m_total_refined, (m_total_refined * 100.0f) / m_total_blocks);

	printf("Total mode 11, 1 partition: %u %3.2f%%\n", m_total_mode11_1part, (m_total_mode11_1part * 100.0f) / m_total_blocks);
	printf("Total mode 11, 1 partition, constrained weights: %u %3.2f%%\n", m_total_mode11_1part_constrained_weights, (m_total_mode11_1part_constrained_weights * 100.0f) / m_total_blocks);
	printf("Total mode 11, 2 partition: %u %3.2f%%\n", m_total_mode11_2part, (m_total_mode11_2part * 100.0f) / m_total_blocks);

	printf("Total mode 7, 1 partition: %u %3.2f%%\n", m_total_mode7_1part, (m_total_mode7_1part * 100.0f) / m_total_blocks);
	printf("Total mode 7, 2 partition: %u %3.2f%%\n", m_total_mode7_2part, (m_total_mode7_2part * 100.0f) / m_total_blocks);

	printf("Total 2 partitions: %u %3.2f%%\n", m_total_2part, (m_total_2part * 100.0f) / m_total_blocks);
	printf("\n");

	printf("ISE texel weight range histogram mode 11:\n");
	for (uint32_t i = 1; i <= MODE11_LAST_ISE_RANGE; i++)
		printf("%u %u\n", i, m_weight_range_hist_11[i]);
	printf("\n");

	printf("ISE texel weight range histogram mode 11, 2 partition:\n");
	for (uint32_t i = 1; i <= MODE11_PART2_LAST_ISE_RANGE; i++)
		printf("%u %u\n", i, m_weight_range_hist_11_2part[i]);
	printf("\n");

	printf("ISE texel weight range histogram mode 7:\n");
	for (uint32_t i = 1; i <= MODE7_PART1_LAST_ISE_RANGE; i++)
		printf("%u %u\n", i, m_weight_range_hist_7[i]);
	printf("\n");

	printf("ISE texel weight range histogram mode 7, 2 partition:\n");
	for (uint32_t i = 1; i <= MODE7_PART2_LAST_ISE_RANGE; i++)
		printf("%u %u\n", i, m_weight_range_hist_7_2part[i]);
	printf("\n");

	printf("Mode 11 submode histogram:\n");
	for (uint32_t i = 0; i <= MODE11_TOTAL_SUBMODES; i++) // +1 because of the extra direct encoding
		printf("%u %u\n", i, m_mode11_submode_hist[i]);
	printf("\n");

	printf("Mode 7 submode histogram:\n");
	for (uint32_t i = 0; i < MODE7_TOTAL_SUBMODES; i++)
		printf("%u %u\n", i, m_mode7_submode_hist[i]);
	printf("\n");

	printf("Partition pattern table usage histogram:\n");
	for (uint32_t i = 0; i < basist::TOTAL_ASTC_BC7_COMMON_PARTITIONS2; i++)
		printf("%u:%u ", i, m_part_hist[i]);
	printf("\n\n");
}

} // namespace basisu

