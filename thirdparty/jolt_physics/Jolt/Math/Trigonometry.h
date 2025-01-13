// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

// Note that this file exists because std::sin etc. are not platform independent and will lead to non-deterministic simulation

/// Sine of x (input in radians)
JPH_INLINE float Sin(float inX)
{
	Vec4 s, c;
	Vec4::sReplicate(inX).SinCos(s, c);
	return s.GetX();
}

/// Cosine of x (input in radians)
JPH_INLINE float Cos(float inX)
{
	Vec4 s, c;
	Vec4::sReplicate(inX).SinCos(s, c);
	return c.GetX();
}

/// Tangent of x (input in radians)
JPH_INLINE float Tan(float inX)
{
	return Vec4::sReplicate(inX).Tan().GetX();
}

/// Arc sine of x (returns value in the range [-PI / 2, PI / 2])
/// Note that all input values will be clamped to the range [-1, 1] and this function will not return NaNs like std::asin
JPH_INLINE float ASin(float inX)
{
	return Vec4::sReplicate(inX).ASin().GetX();
}

/// Arc cosine of x (returns value in the range [0, PI])
/// Note that all input values will be clamped to the range [-1, 1] and this function will not return NaNs like std::acos
JPH_INLINE float ACos(float inX)
{
	return Vec4::sReplicate(inX).ACos().GetX();
}

/// An approximation of ACos, max error is 4.2e-3 over the entire range [-1, 1], is approximately 2.5x faster than ACos
JPH_INLINE float ACosApproximate(float inX)
{
	// See: https://www.johndcook.com/blog/2022/09/06/inverse-cosine-near-1/
	// See also: https://seblagarde.wordpress.com/2014/12/01/inverse-trigonometric-functions-gpu-optimization-for-amd-gcn-architecture/
	// Taylor of cos(x) = 1 - x^2 / 2 + ...
	// Substitute x = sqrt(2 y) we get: cos(sqrt(2 y)) = 1 - y
	// Substitute z = 1 - y we get: cos(sqrt(2 (1 - z))) = z <=> acos(z) = sqrt(2 (1 - z))
	// To avoid the discontinuity at 1, instead of using the Taylor expansion of acos(x) we use acos(x) / sqrt(2 (1 - x)) = 1 + (1 - x) / 12 + ...
	// Since the approximation was made at 1, it has quite a large error at 0 meaning that if we want to extend to the
	// range [-1, 1] by mirroring the range [0, 1], the value at 0+ is not the same as 0-.
	// So we observe that the form of the Taylor expansion is f(x) = sqrt(1 - x) * (a + b x) and we fit the function so that f(0) = pi / 2
	// this gives us a = pi / 2. f(1) = 0 regardless of b. We search for a constant b that minimizes the error in the range [0, 1].
	float abs_x = min(abs(inX), 1.0f); // Ensure that we don't get a value larger than 1
	float val = sqrt(1.0f - abs_x) * (JPH_PI / 2 - 0.175394f * abs_x);

	// Our approximation is valid in the range [0, 1], extend it to the range [-1, 1]
	return inX < 0? JPH_PI - val : val;
}

/// Arc tangent of x (returns value in the range [-PI / 2, PI / 2])
JPH_INLINE float ATan(float inX)
{
	return Vec4::sReplicate(inX).ATan().GetX();
}

/// Arc tangent of y / x using the signs of the arguments to determine the correct quadrant (returns value in the range [-PI, PI])
JPH_INLINE float ATan2(float inY, float inX)
{
	return Vec4::sATan2(Vec4::sReplicate(inY), Vec4::sReplicate(inX)).GetX();
}

JPH_NAMESPACE_END
