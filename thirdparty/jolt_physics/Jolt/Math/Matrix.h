// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Math/Vector.h>
#include <Jolt/Math/GaussianElimination.h>

JPH_NAMESPACE_BEGIN

/// Templatized matrix class
template <uint Rows, uint Cols>
class [[nodiscard]] Matrix
{
public:
	/// Constructor
	inline									Matrix() = default;
	inline									Matrix(const Matrix &inM2)								{ *this = inM2; }

	/// Dimensions
	inline uint								GetRows() const											{ return Rows; }
	inline uint								GetCols() const											{ return Cols; }

	/// Zero matrix
	inline void								SetZero()
	{
		for (uint c = 0; c < Cols; ++c)
			mCol[c].SetZero();
	}

	inline static Matrix					sZero()													{ Matrix m; m.SetZero(); return m; }

	/// Check if this matrix consists of all zeros
	inline bool								IsZero() const
	{
		for (uint c = 0; c < Cols; ++c)
			if (!mCol[c].IsZero())
				return false;

		return true;
	}

	/// Identity matrix
	inline void								SetIdentity()
	{
		// Clear matrix
		SetZero();

		// Set diagonal to 1
		for (uint rc = 0, min_rc = min(Rows, Cols); rc < min_rc; ++rc)
			mCol[rc].mF32[rc] = 1.0f;
	}

	inline static Matrix					sIdentity()												{ Matrix m; m.SetIdentity(); return m; }

	/// Check if this matrix is identity
	bool									IsIdentity() const										{ return *this == sIdentity(); }

	/// Diagonal matrix
	inline void								SetDiagonal(const Vector<Rows < Cols? Rows : Cols> &inV)
	{
		// Clear matrix
		SetZero();

		// Set diagonal
		for (uint rc = 0, min_rc = min(Rows, Cols); rc < min_rc; ++rc)
			mCol[rc].mF32[rc] = inV[rc];
	}

	inline static Matrix					sDiagonal(const Vector<Rows < Cols? Rows : Cols> &inV)
	{
		Matrix m;
		m.SetDiagonal(inV);
		return m;
	}

	/// Copy a (part) of another matrix into this matrix
	template <class OtherMatrix>
		void								CopyPart(const OtherMatrix &inM, uint inSourceRow, uint inSourceCol, uint inNumRows, uint inNumCols, uint inDestRow, uint inDestCol)
		{
			for (uint c = 0; c < inNumCols; ++c)
				for (uint r = 0; r < inNumRows; ++r)
					mCol[inDestCol + c].mF32[inDestRow + r] = inM(inSourceRow + r, inSourceCol + c);
		}

	/// Get float component by element index
	inline float							operator () (uint inRow, uint inColumn) const
	{
		JPH_ASSERT(inRow < Rows);
		JPH_ASSERT(inColumn < Cols);
		return mCol[inColumn].mF32[inRow];
	}

	inline float &							operator () (uint inRow, uint inColumn)
	{
		JPH_ASSERT(inRow < Rows);
		JPH_ASSERT(inColumn < Cols);
		return mCol[inColumn].mF32[inRow];
	}

	/// Comparison
	inline bool								operator == (const Matrix &inM2) const
	{
		for (uint c = 0; c < Cols; ++c)
			if (mCol[c] != inM2.mCol[c])
				return false;
		return true;
	}

	inline bool								operator != (const Matrix &inM2) const
	{
		for (uint c = 0; c < Cols; ++c)
			if (mCol[c] != inM2.mCol[c])
				return true;
		return false;
	}

	/// Assignment
	inline Matrix &							operator = (const Matrix &inM2)
	{
		for (uint c = 0; c < Cols; ++c)
			mCol[c] = inM2.mCol[c];
		return *this;
	}

	/// Multiply matrix by matrix
	template <uint OtherCols>
	inline Matrix<Rows, OtherCols>	operator * (const Matrix<Cols, OtherCols> &inM) const
	{
		Matrix<Rows, OtherCols> m;
		for (uint c = 0; c < OtherCols; ++c)
			for (uint r = 0; r < Rows; ++r)
			{
				float dot = 0.0f;
				for (uint i = 0; i < Cols; ++i)
					dot += mCol[i].mF32[r] * inM.mCol[c].mF32[i];
				m.mCol[c].mF32[r] = dot;
			}
		return m;
	}

	/// Multiply vector by matrix
	inline Vector<Rows>						operator * (const Vector<Cols> &inV) const
	{
		Vector<Rows> v;
		for (uint r = 0; r < Rows; ++r)
		{
			float dot = 0.0f;
			for (uint c = 0; c < Cols; ++c)
				dot += mCol[c].mF32[r] * inV.mF32[c];
			v.mF32[r] = dot;
		}
		return v;
	}

	/// Multiply matrix with float
	inline Matrix							operator * (float inV) const
	{
		Matrix m;
		for (uint c = 0; c < Cols; ++c)
			m.mCol[c] = mCol[c] * inV;
		return m;
	}

	inline friend Matrix					operator * (float inV, const Matrix &inM)
	{
		return inM * inV;
	}

	/// Per element addition of matrix
	inline Matrix							operator + (const Matrix &inM) const
	{
		Matrix m;
		for (uint c = 0; c < Cols; ++c)
			m.mCol[c] = mCol[c] + inM.mCol[c];
		return m;
	}

	/// Per element subtraction of matrix
	inline Matrix							operator - (const Matrix &inM) const
	{
		Matrix m;
		for (uint c = 0; c < Cols; ++c)
			m.mCol[c] = mCol[c] - inM.mCol[c];
		return m;
	}

	/// Transpose matrix
	inline Matrix<Cols, Rows>				Transposed() const
	{
		Matrix<Cols, Rows> m;
		for (uint r = 0; r < Rows; ++r)
			for (uint c = 0; c < Cols; ++c)
				m.mCol[r].mF32[c] = mCol[c].mF32[r];
		return m;
	}

	/// Inverse matrix
	bool									SetInversed(const Matrix &inM)
	{
		if constexpr (Rows != Cols) JPH_ASSERT(false);
		Matrix copy(inM);
		SetIdentity();
		return GaussianElimination(copy, *this);
	}

	inline Matrix							Inversed() const
	{
		Matrix m;
		m.SetInversed(*this);
		return m;
	}

	/// To String
	friend ostream &						operator << (ostream &inStream, const Matrix &inM)
	{
		for (uint i = 0; i < Cols - 1; ++i)
			inStream << inM.mCol[i] << ", ";
		inStream << inM.mCol[Cols - 1];
		return inStream;
	}

	/// Column access
	const Vector<Rows> &					GetColumn(int inIdx) const					{ return mCol[inIdx]; }
	Vector<Rows> &							GetColumn(int inIdx)						{ return mCol[inIdx]; }

	Vector<Rows>							mCol[Cols];									///< Column
};

// The template specialization doesn't sit well with Doxygen
#ifndef JPH_PLATFORM_DOXYGEN

/// Specialization of SetInversed for 2x2 matrix
template <>
inline bool Matrix<2, 2>::SetInversed(const Matrix<2, 2> &inM)
{
	// Fetch elements
	float a = inM.mCol[0].mF32[0];
	float b = inM.mCol[1].mF32[0];
	float c = inM.mCol[0].mF32[1];
	float d = inM.mCol[1].mF32[1];

	// Calculate determinant
	float det = a * d - b * c;
	if (det == 0.0f)
		return false;

	// Construct inverse
	mCol[0].mF32[0] = d / det;
	mCol[1].mF32[0] = -b / det;
	mCol[0].mF32[1] = -c / det;
	mCol[1].mF32[1] = a / det;
	return true;
}

#endif // !JPH_PLATFORM_DOXYGEN

JPH_NAMESPACE_END
