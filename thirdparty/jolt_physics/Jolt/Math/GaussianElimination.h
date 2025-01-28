// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// This function performs Gauss-Jordan elimination to solve a matrix equation.
/// A must be an NxN matrix and B must be an NxM matrix forming the equation A * x = B
/// on output B will contain x and A will be destroyed.
///
/// This code can be used for example to compute the inverse of a matrix.
/// Set A to the matrix to invert, set B to identity and let GaussianElimination solve
/// the equation, on return B will be the inverse of A. And A is destroyed.
///
/// Taken and adapted from Numerical Recipes in C paragraph 2.1
template <class MatrixA, class MatrixB>
bool GaussianElimination(MatrixA &ioA, MatrixB &ioB, float inTolerance = 1.0e-16f)
{
	// Get problem dimensions
	const uint n = ioA.GetCols();
	const uint m = ioB.GetCols();

	// Check matrix requirement
	JPH_ASSERT(ioA.GetRows() == n);
	JPH_ASSERT(ioB.GetRows() == n);

	// Create array for bookkeeping on pivoting
	int *ipiv = (int *)JPH_STACK_ALLOC(n * sizeof(int));
	memset(ipiv, 0, n * sizeof(int));

	for (uint i = 0; i < n; ++i)
	{
		// Initialize pivot element as the diagonal
		uint pivot_row = i, pivot_col = i;

		// Determine pivot element
		float largest_element = 0.0f;
		for (uint j = 0; j < n; ++j)
			if (ipiv[j] != 1)
				for (uint k = 0; k < n; ++k)
				{
					if (ipiv[k] == 0)
					{
						float element = abs(ioA(j, k));
						if (element >= largest_element)
						{
							largest_element = element;
							pivot_row = j;
							pivot_col = k;
						}
					}
					else if (ipiv[k] > 1)
					{
						return false;
					}
				}

		// Mark this column as used
		++ipiv[pivot_col];

		// Exchange rows when needed so that the pivot element is at ioA(pivot_col, pivot_col) instead of at ioA(pivot_row, pivot_col)
		if (pivot_row != pivot_col)
		{
			for (uint j = 0; j < n; ++j)
				std::swap(ioA(pivot_row, j), ioA(pivot_col, j));
			for (uint j = 0; j < m; ++j)
				std::swap(ioB(pivot_row, j), ioB(pivot_col, j));
		}

		// Get diagonal element that we are about to set to 1
		float diagonal_element = ioA(pivot_col, pivot_col);
		if (abs(diagonal_element) < inTolerance)
			return false;

		// Divide the whole row by the pivot element, making ioA(pivot_col, pivot_col) = 1
		for (uint j = 0; j < n; ++j)
			ioA(pivot_col, j) /= diagonal_element;
		for (uint j = 0; j < m; ++j)
			ioB(pivot_col, j) /= diagonal_element;
		ioA(pivot_col, pivot_col) = 1.0f;

		// Next reduce the rows, except for the pivot one,
		// after this step the pivot_col column is zero except for the pivot element which is 1
		for (uint j = 0; j < n; ++j)
			if (j != pivot_col)
			{
				float element = ioA(j, pivot_col);
				for (uint k = 0; k < n; ++k)
					ioA(j, k) -= ioA(pivot_col, k) * element;
				for (uint k = 0; k < m; ++k)
					ioB(j, k) -= ioB(pivot_col, k) * element;
				ioA(j, pivot_col) = 0.0f;
			}
	}

	// Success
	return true;
}

JPH_NAMESPACE_END
