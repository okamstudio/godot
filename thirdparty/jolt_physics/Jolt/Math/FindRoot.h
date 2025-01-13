// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// Find the roots of \f$inA \: x^2 + inB \: x + inC = 0\f$.
/// @return The number of roots, actual roots in outX1 and outX2.
/// If number of roots returned is 1 then outX1 == outX2.
template <typename T>
inline int FindRoot(const T inA, const T inB, const T inC, T &outX1, T &outX2)
{
	// Check if this is a linear equation
	if (inA == T(0))
	{
		// Check if this is a constant equation
		if (inB == T(0))
			return 0;

		// Linear equation with 1 solution
		outX1 = outX2 = -inC / inB;
		return 1;
	}

	// See Numerical Recipes in C, Chapter 5.6 Quadratic and Cubic Equations
	T det = Square(inB) - T(4) * inA * inC;
	if (det < T(0))
		return 0;
	T q = (inB + Sign(inB) * sqrt(det)) / T(-2);
	outX1 = q / inA;
	if (q == T(0))
	{
		outX2 = outX1;
		return 1;
	}
	outX2 = inC / q;
	return 2;
}

JPH_NAMESPACE_END
