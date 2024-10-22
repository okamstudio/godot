// Do not include this header directly.
//
// Copyright 2020-2024 Binomial LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// The general goal of these vectorized estimated math functions is scalability/performance.
// There are explictly no checks NaN's/Inf's on the input arguments. There are no assertions either. 
// These are fast estimate functions - if you need more than that, use stdlib. Please do a proper 
// engineering analysis before relying on them.
// I have chosen functions written by others, ported them to CppSPMD, then measured their abs/rel errors.
// I compared each to the ones in DirectXMath and stdlib's for accuracy/performance.

CPPSPMD_FORCE_INLINE vfloat fmod_inv(const vfloat& a, const vfloat& b, const vfloat& b_inv) 
{ 
	vfloat c = frac(abs(a * b_inv)) * abs(b); 
	return spmd_ternaryf(a < 0, -c, c); 
}

CPPSPMD_FORCE_INLINE vfloat fmod_inv_p(const vfloat& a, const vfloat& b, const vfloat& b_inv) 
{ 
	return frac(a * b_inv) * b; 
}

// Avoids dividing by zero or very small values.
CPPSPMD_FORCE_INLINE vfloat safe_div(vfloat a, vfloat b, float fDivThresh = 1e-7f)
{
	return a / spmd_ternaryf( abs(b) > fDivThresh, b, spmd_ternaryf(b < 0.0f, -fDivThresh, fDivThresh) );
}

/*
	clang 9.0.0 for win /fp:precise release
	f range: 0.0000000000001250 10000000000.0000000000000000, vals: 1073741824

	log2_est():
	max abs err: 0.0000023076808731
	max rel err: 0.0000000756678881
	avg abs err: 0.0000007535452724
	avg rel err: 0.0000000235117843

	XMVectorLog2():
	max abs err: 0.0000023329709933
	max rel err: 0.0000000826961046
	avg abs err: 0.0000007564889684
	avg rel err: 0.0000000236051899

	std::log2f():
	max abs err: 0.0000020265979401
	max rel err: 0.0000000626647654
	avg abs err: 0.0000007494445227
	avg rel err: 0.0000000233800985
*/

// See https://tech.ebayinc.com/engineering/fast-approximate-logarithms-part-iii-the-formulas/
inline vfloat spmd_kernel::log2_est(vfloat v)
{
	vfloat signif, fexp;

	// Just clamp to a very small value, instead of checking for invalid inputs.
	vfloat x = max(v, 2.2e-38f);

	/*
	 * Assume IEEE representation, which is sgn(1):exp(8):frac(23)
	 * representing (1+frac)*2^(exp-127).  Call 1+frac the significand
	 */

	 // get exponent
	vint ux1_i = cast_vfloat_to_vint(x);

	vint exp = VUINT_SHIFT_RIGHT(ux1_i & 0x7F800000, 23);

	// actual exponent is exp-127, will subtract 127 later

	vint ux2_i;
	vfloat ux2_f;

	vint greater = ux1_i & 0x00400000;  // true if signif > 1.5
	SPMD_SIF(greater != 0)
	{
		// signif >= 1.5 so need to divide by 2.  Accomplish this by stuffing exp = 126 which corresponds to an exponent of -1 
		store_all(ux2_i, (ux1_i & 0x007FFFFF) | 0x3f000000);

		store_all(ux2_f, cast_vint_to_vfloat(ux2_i));

		// 126 instead of 127 compensates for division by 2
		store_all(fexp, vfloat(exp - 126));    
	}
	SPMD_SELSE(greater != 0)
	{
		// get signif by stuffing exp = 127 which corresponds to an exponent of 0
		store(ux2_i, (ux1_i & 0x007FFFFF) | 0x3f800000);

		store(ux2_f, cast_vint_to_vfloat(ux2_i));

		store(fexp, vfloat(exp - 127));
	}
	SPMD_SENDIF

	store_all(signif, ux2_f);
	store_all(signif, signif - 1.0f);

	const float a = 0.1501692f, b = 3.4226132f, c = 5.0225057f, d = 4.1130283f, e = 3.4813372f;

	vfloat xm1 = signif;
	vfloat xm1sqr = xm1 * xm1;
		
	return fexp + ((a * (xm1sqr * xm1) + b * xm1sqr + c * xm1) / (xm1sqr + d * xm1 + e));
	
	// fma lowers accuracy for SSE4.1 - no idea why (compiler reordering?)
	//return fexp + ((vfma(a, (xm1sqr * xm1), vfma(b, xm1sqr, c * xm1))) / (xm1sqr + vfma(d, xm1, e)));
}

// Uses log2_est(), so this function must be <= the precision of that.
inline vfloat spmd_kernel::log_est(vfloat v)
{
	return log2_est(v) * 0.693147181f;
}

CPPSPMD_FORCE_INLINE void spmd_kernel::reduce_expb(vfloat& arg, vfloat& two_int_a, vint& adjustment)
{
	// Assume we're using equation (2)
	store_all(adjustment, 0);
	
	// integer part of the input argument
	vint int_arg = (vint)arg;
	
	// if frac(arg) is in [0.5, 1.0]...
	SPMD_SIF((arg - int_arg) > 0.5f)   
	{
		store(adjustment, 1);
		
		// then change it to [0.0, 0.5]
		store(arg, arg - 0.5f);
	}
	SPMD_SENDIF

	// arg == just the fractional part
	store_all(arg, arg - (vfloat)int_arg);
   
	// Now compute 2** (int) arg. 
	store_all(int_arg, min(int_arg + 127, 254));
	
	store_all(two_int_a, cast_vint_to_vfloat(VINT_SHIFT_LEFT(int_arg, 23)));
}

/*
	clang 9.0.0 for win /fp:precise release
	f range : -50.0000000000000000 49.9999940395355225, vals : 16777216
	
	exp2_est():
	Total passed near - zero check : 16777216
	Total sign diffs : 0
	max abs err: 1668910609.7500000000000000
	max rel err: 0.0000015642030031
	avg abs err: 10793794.4007573910057545
	avg rel err: 0.0000003890893282
	 
	XMVectorExp2():
	Total passed near-zero check: 16777216
	Total sign diffs: 0
	max abs err: 1665552836.8750000000000000
	max rel err: 0.0000114674862370
	avg abs err: 10771868.2627860084176064
	avg rel err: 0.0000011218880770

	std::exp2f():
	Total passed near-zero check: 16777216
	Total sign diffs: 0
	max abs err: 1591636585.6250000000000000
	max rel err: 0.0000014849731018
	avg abs err: 10775800.3204844966530800
	avg rel err: 0.0000003851496422
*/

// http://www.ganssle.com/item/approximations-c-code-exponentiation-log.htm
inline vfloat spmd_kernel::exp2_est(vfloat arg)
{
	SPMD_BEGIN_CALL

	const vfloat P00 = +7.2152891521493f;
	const vfloat P01 = +0.0576900723731f;
	const vfloat Q00 = +20.8189237930062f;
	const vfloat Q01 = +1.0f;
	const vfloat sqrt2 = 1.4142135623730950488f; // sqrt(2) for scaling 

	vfloat result = 0.0f;

	// Return 0 if arg is too large. 
	// We're not introducing inf/nan's into calculations, or risk doing so by returning huge default values.
	SPMD_IF(abs(arg) > 126.0f)
	{
		spmd_return();
	}
	SPMD_END_IF

	// 2**(int(a))
	vfloat two_int_a;                
	
	// set to 1 by reduce_expb
	vint adjustment;
	
	// 0 if arg is +; 1 if negative
	vint negative = 0;                 

	// If the input is negative, invert it. At the end we'll take the reciprocal, since n**(-1) = 1/(n**x).
	SPMD_SIF(arg < 0.0f)
	{
		store(arg, -arg);
		store(negative, 1);
	}
	SPMD_SENDIF

	store_all(arg, min(arg, 126.0f));

	// reduce to [0.0, 0.5]
	reduce_expb(arg, two_int_a, adjustment);

	// The format of the polynomial is:
	//  answer=(Q(x**2) + x*P(x**2))/(Q(x**2) - x*P(x**2))
	//
	//  The following computes the polynomial in several steps:

	// Q(x**2)
	vfloat Q = vfma(Q01, (arg * arg), Q00);
	
	// x*P(x**2)
	vfloat x_P = arg * (vfma(P01, arg * arg, P00));
	
	vfloat answer = (Q + x_P) / (Q - x_P);

	// Now correct for the scaling factor of 2**(int(a))
	store_all(answer, answer * two_int_a);
			
	// If the result had a fractional part > 0.5, correct for that
	store_all(answer, spmd_ternaryf(adjustment != 0, answer * sqrt2, answer));

	// Correct for a negative input
	SPMD_SIF(negative != 0)
	{
		store(answer, 1.0f / answer);
	}
	SPMD_SENDIF

	store(result, answer);

	return result;
}

inline vfloat spmd_kernel::exp_est(vfloat arg)
{
	// e^x = exp2(x / log_base_e(2))
	// constant is 1.0/(log(2)/log(e)) or 1/log(2)
	return exp2_est(arg * 1.44269504f);
}

inline vfloat spmd_kernel::pow_est(vfloat arg1, vfloat arg2)
{
	return exp_est(log_est(arg1) * arg2);
}

/*
	clang 9.0.0 for win /fp:precise release
	Total near-zero: 144, output above near-zero tresh: 30
	Total near-zero avg: 0.0000067941016621 max: 0.0000134706497192
	Total near-zero sign diffs: 5
	Total passed near-zero check: 16777072
	Total sign diffs: 5
	max abs err: 0.0000031375306036
	max rel err: 0.1140846017075028
	avg abs err: 0.0000003026226621
	avg rel err: 0.0000033564977623
*/

// Math from this web page: http://developer.download.nvidia.com/cg/sin.html
// This is ~2x slower than sin_est() or cos_est(), and less accurate, but I'm keeping it here for comparison purposes to help validate/sanity check sin_est() and cos_est().
inline vfloat spmd_kernel::sincos_est_a(vfloat a, bool sin_flag)
{
	const float c0_x = 0.0f, c0_y = 0.5f, c0_z = 1.0f;
	const float c1_x = 0.25f, c1_y = -9.0f, c1_z = 0.75f, c1_w = 0.159154943091f;
	const float c2_x = 24.9808039603f, c2_y = -24.9808039603f, c2_z = -60.1458091736f, c2_w = 60.1458091736f;
	const float c3_x = 85.4537887573f, c3_y = -85.4537887573f, c3_z = -64.9393539429f, c3_w = 64.9393539429f;
	const float c4_x = 19.7392082214f, c4_y = -19.7392082214f, c4_z = -1.0f, c4_w = 1.0f;

	vfloat r0_x, r0_y, r0_z, r1_x, r1_y, r1_z, r2_x, r2_y, r2_z;

	store_all(r1_x, sin_flag ? vfms(c1_w, a, c1_x) : c1_w * a);

	store_all(r1_y, frac(r1_x));                   
	
	store_all(r2_x, (vfloat)(r1_y < c1_x));        

	store_all(r2_y, (vfloat)(r1_y >= c1_y));    
	store_all(r2_z, (vfloat)(r1_y >= c1_z));    

	store_all(r2_y, vfma(r2_x, c4_z, vfma(r2_y, c4_w, r2_z * c4_z)));

	store_all(r0_x, c0_x - r1_y);                
	store_all(r0_y, c0_y - r1_y);                
	store_all(r0_z, c0_z - r1_y);                
	
	store_all(r0_x, r0_x * r0_x);
	store_all(r0_y, r0_y * r0_y);
	store_all(r0_z, r0_z * r0_z);

	store_all(r1_x, vfma(c2_x, r0_x, c2_z));           
	store_all(r1_y, vfma(c2_y, r0_y, c2_w));           
	store_all(r1_z, vfma(c2_x, r0_z, c2_z));           
	
	store_all(r1_x, vfma(r1_x, r0_x, c3_x));
	store_all(r1_y, vfma(r1_y, r0_y, c3_y));
	store_all(r1_z, vfma(r1_z, r0_z, c3_x));
		
	store_all(r1_x, vfma(r1_x, r0_x, c3_z));
	store_all(r1_y, vfma(r1_y, r0_y, c3_w));
	store_all(r1_z, vfma(r1_z, r0_z, c3_z));
	
	store_all(r1_x, vfma(r1_x, r0_x, c4_x));
	store_all(r1_y, vfma(r1_y, r0_y, c4_y));
	store_all(r1_z, vfma(r1_z, r0_z, c4_x));

	store_all(r1_x, vfma(r1_x, r0_x, c4_z));
	store_all(r1_y, vfma(r1_y, r0_y, c4_w));
	store_all(r1_z, vfma(r1_z, r0_z, c4_z));

	store_all(r0_x, vfnma(r1_x, r2_x, vfnma(r1_y, r2_y, r1_z * -r2_z)));

	return r0_x;
}

// positive values only
CPPSPMD_FORCE_INLINE vfloat spmd_kernel::recip_est1(const vfloat& q)
{
	//const int mag = 0x7EF312AC; // 2 NR iters, 3 is  0x7EEEEBB3
	const int mag = 0x7EF311C3;
	const float fMinThresh = .0000125f;

	vfloat l = spmd_ternaryf(q >= fMinThresh, q, cast_vint_to_vfloat(vint(mag)));

	vint x_l = vint(mag) - cast_vfloat_to_vint(l);
	
	vfloat rcp_l = cast_vint_to_vfloat(x_l);
	
	return rcp_l * vfnma(rcp_l, q, 2.0f);
}

CPPSPMD_FORCE_INLINE vfloat spmd_kernel::recip_est1_pn(const vfloat& t)
{
	//const int mag = 0x7EF312AC; // 2 NR iters, 3 is  0x7EEEEBB3
	const int mag = 0x7EF311C3;
	const float fMinThresh = .0000125f;

	vfloat s = sign(t);
	vfloat q = abs(t);

	vfloat l = spmd_ternaryf(q >= fMinThresh, q, cast_vint_to_vfloat(vint(mag)));

	vint x_l = vint(mag) - cast_vfloat_to_vint(l);

	vfloat rcp_l = cast_vint_to_vfloat(x_l);

	return rcp_l * vfnma(rcp_l, q, 2.0f) * s;
}

// https://basesandframes.files.wordpress.com/2020/04/even_faster_math_functions_green_2020.pdf
// https://github.com/hcs0/Hackers-Delight/blob/master/rsqrt.c.txt
CPPSPMD_FORCE_INLINE vfloat spmd_kernel::rsqrt_est1(vfloat x0)
{
	vfloat xhalf = 0.5f * x0;
	vfloat x = cast_vint_to_vfloat(vint(0x5F375A82) - (VINT_SHIFT_RIGHT(cast_vfloat_to_vint(x0), 1)));
	return x * vfnma(xhalf * x, x, 1.5008909f);
}

CPPSPMD_FORCE_INLINE vfloat spmd_kernel::rsqrt_est2(vfloat x0)
{
	vfloat xhalf = 0.5f * x0;
	vfloat x = cast_vint_to_vfloat(vint(0x5F37599E) - (VINT_SHIFT_RIGHT(cast_vfloat_to_vint(x0), 1)));
	vfloat x1 = x * vfnma(xhalf * x, x, 1.5);
	vfloat x2 = x1 * vfnma(xhalf * x1, x1, 1.5);
	return x2;
}

// Math from: http://developer.download.nvidia.com/cg/atan2.html
// TODO: Needs more validation, parameter checking.
CPPSPMD_FORCE_INLINE vfloat spmd_kernel::atan2_est(vfloat y, vfloat x)
{
	vfloat t1 = abs(y);
	vfloat t3 = abs(x);
	
	vfloat t0 = max(t3, t1);
	store_all(t1, min(t3, t1));

	store_all(t3, t1 / t0);
	
	vfloat t4 = t3 * t3;
	store_all(t0, vfma(-0.013480470f, t4, 0.057477314f));
	store_all(t0, vfms(t0, t4, 0.121239071f));
	store_all(t0, vfma(t0, t4, 0.195635925f));
	store_all(t0, vfms(t0, t4, 0.332994597f));
	store_all(t0, vfma(t0, t4, 0.999995630f));
	store_all(t3, t0 * t3);

	store_all(t3, spmd_ternaryf(abs(y) > abs(x), vfloat(1.570796327f) - t3, t3));

	store_all(t3, spmd_ternaryf(x < 0.0f, vfloat(3.141592654f) - t3, t3));
	store_all(t3, spmd_ternaryf(y < 0.0f, -t3, t3));

	return t3;
}

/*
    clang 9.0.0 for win /fp:precise release
	Tested range: -25.1327412287183449 25.1327382326621169, vals : 16777216
	Skipped angles near 90/270 within +- .001 radians.
	Near-zero threshold: .0000125f
	Near-zero output above check threshold: 1e-6f

	Total near-zero: 144, output above near-zero tresh: 20
	Total near-zero avg: 0.0000067510751968 max: 0.0000133514404297
	Total near-zero sign diffs: 5
	Total passed near-zero check: 16766400
	Total sign diffs: 5
	max abs err: 1.4982600811139264
	max rel err: 0.1459155900188041
	avg rel err: 0.0000054659502568

	XMVectorTan() precise:
	Total near-zero: 144, output above near-zero tresh: 18
	Total near-zero avg: 0.0000067641216186 max: 0.0000133524126795
	Total near-zero sign diffs: 0
	Total passed near-zero check: 16766400
	Total sign diffs: 0
	max abs err: 1.9883573246424930
	max rel err: 0.1459724171926864
	avg rel err: 0.0000054965766843

	std::tanf():
	Total near-zero: 144, output above near-zero tresh: 0
	Total near-zero avg: 0.0000067116930779 max: 0.0000127713074107
	Total near-zero sign diffs: 11
	Total passed near-zero check: 16766400
	Total sign diffs: 11
	max abs err: 0.8989131818294709
	max rel err: 0.0573181403173166
	avg rel err: 0.0000030791301203
	
	Originally from:
	http://www.ganssle.com/approx.htm
*/

CPPSPMD_FORCE_INLINE vfloat spmd_kernel::tan82(vfloat x)
{
	// Original double version was 8.2 digits
	//double c1 = 211.849369664121f, c2 = -12.5288887278448f, c3 = 269.7350131214121f, c4 = -71.4145309347748f;
	// Tuned float constants for lower avg rel error (without using FMA3):
	const float c1 = 211.849350f, c2 = -12.5288887f, c3 = 269.734985f, c4 = -71.4145203f;
	vfloat x2 = x * x;
	return (x * (vfma(c2, x2, c1)) / (vfma(x2, (c4 + x2), c3)));
}

// Don't call this for angles close to 90/270!.
inline vfloat spmd_kernel::tan_est(vfloat x)
{
	const float fPi = 3.141592653589793f, fOneOverPi = 0.3183098861837907f;
	CPPSPMD_DECL(const uint8_t, s_table0[16]) =	{ 128 + 0, 128 + 2, 128 + -2, 128 + 4,    128 + 0, 128 + 2, 128 + -2, 128 + 4,	  128 + 0, 128 + 2, 128 + -2, 128 + 4,   128 + 0, 128 + 2, 128 + -2, 128 + 4 };

	vint table = init_lookup4(s_table0); // a load
	vint sgn = cast_vfloat_to_vint(x) & 0x80000000;

	store_all(x, abs(x));
	vfloat orig_x = x;

	vfloat q = x * fOneOverPi;
	store_all(x, q - floor(q));

	vfloat x4 = x * 4.0f;
	vint octant = (vint)(x4);

	vfloat x0 = spmd_ternaryf((octant & 1) != 0, -x4, x4);

	vint k = table_lookup4_8(octant, table) & 0xFF; // a shuffle

	vfloat bias = (vfloat)k + -128.0f;
	vfloat y = x0 + bias;

	vfloat z = tan82(y);

	vfloat r;
	
	vbool octant_one_or_two = (octant == 1) || (octant == 2);

	// SPMD optimization - skip costly divide if we can
	if (spmd_any(octant_one_or_two))
	{
		const float fDivThresh = .4371e-7f;
		vfloat one_over_z = 1.0f / spmd_ternaryf(abs(z) > fDivThresh, z, spmd_ternaryf(z < 0.0f, -fDivThresh, fDivThresh));
				
		vfloat b = spmd_ternaryf(octant_one_or_two, one_over_z, z);
		store_all(r, spmd_ternaryf((octant & 2) != 0, -b, b));
	}
	else
	{
		store_all(r, spmd_ternaryf(octant == 0, z, -z));
	}
		
	// Small angle approximation, to decrease the max rel error near Pi.
	SPMD_SIF(x >= (1.0f - .0003125f*4.0f))
	{
		store(r, vfnma(floor(q) + 1.0f, fPi, orig_x));
	}
	SPMD_SENDIF

	return cast_vint_to_vfloat(cast_vfloat_to_vint(r) ^ sgn);
}

inline void spmd_kernel::seed_rand(rand_context& x, vint seed)
{ 
	store(x.a, 0xf1ea5eed); 
	store(x.b, seed ^ 0xd8487b1f); 
	store(x.c, seed ^ 0xdbadef9a); 
	store(x.d, seed); 
	for (int i = 0; i < 20; ++i) 
		(void)get_randu(x); 
}

// https://burtleburtle.net/bob/rand/smallprng.html
// Returns 32-bit unsigned random numbers.
inline vint spmd_kernel::get_randu(rand_context& x)
{ 
	vint e = x.a - VINT_ROT(x.b, 27); 
	store(x.a, x.b ^ VINT_ROT(x.c, 17)); 
	store(x.b, x.c + x.d); 
	store(x.c, x.d + e); 
	store(x.d, e + x.a);	
	return x.d; 
}

// Returns random numbers between [low, high), or low if low >= high
inline vint spmd_kernel::get_randi(rand_context& x, vint low, vint high)
{
	vint rnd = get_randu(x);

	vint range = high - low;

	vint rnd_range = mulhiu(rnd, range);
	
	return spmd_ternaryi(low < high, low + rnd_range, low);
}

// Returns random numbers between [low, high), or low if low >= high
inline vfloat spmd_kernel::get_randf(rand_context& x, vfloat low, vfloat high)
{
	vint rndi = get_randu(x) & 0x7fffff;

	vfloat rnd = (vfloat)(rndi) * (1.0f / 8388608.0f);

	return spmd_ternaryf(low < high, vfma(high - low, rnd, low), low);
}

CPPSPMD_FORCE_INLINE void spmd_kernel::init_reverse_bits(vint& tab1, vint& tab2)
{
	const uint8_t tab1_bytes[16] = { 0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15 };
	const uint8_t tab2_bytes[16] = { 0, 8 << 4, 4 << 4, 12 << 4, 2 << 4, 10 << 4, 6 << 4, 14 << 4, 1 << 4, 9 << 4, 5 << 4, 13 << 4, 3 << 4, 11 << 4, 7 << 4, 15 << 4 };
	store_all(tab1, init_lookup4(tab1_bytes));
	store_all(tab2, init_lookup4(tab2_bytes));
}

CPPSPMD_FORCE_INLINE vint spmd_kernel::reverse_bits(vint k, vint tab1, vint tab2)
{
	vint r0 = table_lookup4_8(k & 0x7F7F7F7F, tab2);
	vint r1 = table_lookup4_8(VUINT_SHIFT_RIGHT(k, 4) & 0x7F7F7F7F, tab1);
	vint r3 = r0 | r1;
	return byteswap(r3);
}

CPPSPMD_FORCE_INLINE vint spmd_kernel::count_leading_zeros(vint x)
{
	CPPSPMD_DECL(const uint8_t, s_tab[16]) = { 0, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 };

	vint tab = init_lookup4(s_tab);

	//x <= 0x0000ffff
	vbool c0 = (x & 0xFFFF0000) == 0;
	vint n0 = spmd_ternaryi(c0, 16, 0);
	vint x0 = spmd_ternaryi(c0, VINT_SHIFT_LEFT(x, 16), x);

	//x <= 0x00ffffff
	vbool c1 = (x0 & 0xFF000000) == 0;
	vint n1 = spmd_ternaryi(c1, n0 + 8, n0);
	vint x1 = spmd_ternaryi(c1, VINT_SHIFT_LEFT(x0, 8), x0);

	//x <= 0x0fffffff
	vbool c2 = (x1 & 0xF0000000) == 0;
	vint n2 = spmd_ternaryi(c2, n1 + 4, n1);
	vint x2 = spmd_ternaryi(c2, VINT_SHIFT_LEFT(x1, 4), x1);

	return table_lookup4_8(VUINT_SHIFT_RIGHT(x2, 28), tab) + n2;
}

CPPSPMD_FORCE_INLINE vint spmd_kernel::count_leading_zeros_alt(vint x)
{
	//x <= 0x0000ffff
	vbool c0 = (x & 0xFFFF0000) == 0;
	vint n0 = spmd_ternaryi(c0, 16, 0);
	vint x0 = spmd_ternaryi(c0, VINT_SHIFT_LEFT(x, 16), x);

	//x <= 0x00ffffff
	vbool c1 = (x0 & 0xFF000000) == 0;
	vint n1 = spmd_ternaryi(c1, n0 + 8, n0);
	vint x1 = spmd_ternaryi(c1, VINT_SHIFT_LEFT(x0, 8), x0);

	//x <= 0x0fffffff
	vbool c2 = (x1 & 0xF0000000) == 0;
	vint n2 = spmd_ternaryi(c2, n1 + 4, n1);
	vint x2 = spmd_ternaryi(c2, VINT_SHIFT_LEFT(x1, 4), x1);

	// x <= 0x3fffffff
	vbool c3 = (x2 & 0xC0000000) == 0;
	vint n3 = spmd_ternaryi(c3, n2 + 2, n2);
	vint x3 = spmd_ternaryi(c3, VINT_SHIFT_LEFT(x2, 2), x2);

	// x <= 0x7fffffff
	vbool c4 = (x3 & 0x80000000) == 0;
	return spmd_ternaryi(c4, n3 + 1, n3);
}

CPPSPMD_FORCE_INLINE vint spmd_kernel::count_trailing_zeros(vint x)
{
	// cast the least significant bit in v to a float
	vfloat f = (vfloat)(x & -x);
	
	// extract exponent and adjust
	return VUINT_SHIFT_RIGHT(cast_vfloat_to_vint(f), 23) - 0x7F;
}

CPPSPMD_FORCE_INLINE vint spmd_kernel::count_set_bits(vint x)
{
	vint v = x - (VUINT_SHIFT_RIGHT(x, 1) & 0x55555555);                    
	vint v1 = (v & 0x33333333) + (VUINT_SHIFT_RIGHT(v, 2) & 0x33333333);     
	return VUINT_SHIFT_RIGHT(((v1 + (VUINT_SHIFT_RIGHT(v1, 4) & 0xF0F0F0F)) * 0x1010101), 24);
}

CPPSPMD_FORCE_INLINE vint cmple_epu16(const vint &a, const vint &b) 
{ 
	return cmpeq_epi16(subs_epu16(a, b), vint(0)); 
}

CPPSPMD_FORCE_INLINE vint cmpge_epu16(const vint &a, const vint &b) 
{ 
	return cmple_epu16(b, a);
}

CPPSPMD_FORCE_INLINE vint cmpgt_epu16(const vint &a, const vint &b)
{
	return andnot(cmpeq_epi16(a, b), cmple_epu16(b, a));
}

CPPSPMD_FORCE_INLINE vint cmplt_epu16(const vint &a, const vint &b)
{
	return cmpgt_epu16(b, a);
}

CPPSPMD_FORCE_INLINE vint cmpge_epi16(const vint &a, const vint &b)
{
	return cmpeq_epi16(a, b) | cmpgt_epi16(a, b);
}

CPPSPMD_FORCE_INLINE vint cmple_epi16(const vint &a, const vint &b)
{
	return cmpge_epi16(b, a);
}

void spmd_kernel::print_vint(vint v) 
{ 
	for (uint32_t i = 0; i < PROGRAM_COUNT; i++) 
		printf("%i ", extract(v, i)); 
	printf("\n"); 
}

void spmd_kernel::print_vbool(vbool v) 
{ 
	for (uint32_t i = 0; i < PROGRAM_COUNT; i++) 
		printf("%i ", extract(v, i) ? 1 : 0); 
	printf("\n"); 
}
	
void spmd_kernel::print_vint_hex(vint v) 
{ 
	for (uint32_t i = 0; i < PROGRAM_COUNT; i++) 
		printf("0x%X ", extract(v, i)); 
	printf("\n"); 
}

void spmd_kernel::print_active_lanes(const char *pPrefix) 
{ 
	CPPSPMD_DECL(int, flags[PROGRAM_COUNT]);
	memset(flags, 0, sizeof(flags));
	storeu_linear(flags, vint(1));

	if (pPrefix)
		printf("%s", pPrefix);

	for (uint32_t i = 0; i < PROGRAM_COUNT; i++) 
	{
		if (flags[i])
			printf("%u ", i);
	}
	printf("\n");
}
	
void spmd_kernel::print_vfloat(vfloat v) 
{ 
	for (uint32_t i = 0; i < PROGRAM_COUNT; i++) 
		printf("%f ", extract(v, i)); 
	printf("\n"); 
}
