// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2024 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// A strided pointer behaves exactly like a normal pointer except that the
/// elements that the pointer points to can be part of a larger structure.
/// The stride gives the number of bytes from one element to the next.
template <class T>
class JPH_EXPORT StridedPtr
{
public:
	using value_type = T;

	/// Constructors
							StridedPtr() = default;
							StridedPtr(const StridedPtr &inRHS) = default;
							StridedPtr(T *inPtr, int inStride = sizeof(T))			: mPtr(const_cast<uint8 *>(reinterpret_cast<const uint8 *>(inPtr))), mStride(inStride) { }

	/// Assignment
	inline StridedPtr &		operator = (const StridedPtr &inRHS) = default;

	/// Incrementing / decrementing
	inline StridedPtr &		operator ++ ()											{ mPtr += mStride; return *this; }
	inline StridedPtr &		operator -- ()											{ mPtr -= mStride; return *this; }
	inline StridedPtr		operator ++ (int)										{ StridedPtr old_ptr(*this); mPtr += mStride; return old_ptr; }
	inline StridedPtr		operator -- (int)										{ StridedPtr old_ptr(*this); mPtr -= mStride; return old_ptr; }
	inline StridedPtr		operator + (int inOffset) const							{ StridedPtr new_ptr(*this); new_ptr.mPtr += inOffset * mStride; return new_ptr; }
	inline StridedPtr		operator - (int inOffset) const							{ StridedPtr new_ptr(*this); new_ptr.mPtr -= inOffset * mStride; return new_ptr; }
	inline void				operator += (int inOffset)								{ mPtr += inOffset * mStride; }
	inline void				operator -= (int inOffset)								{ mPtr -= inOffset * mStride; }

	/// Distance between two pointers in elements
	inline int				operator - (const StridedPtr &inRHS) const				{ JPH_ASSERT(inRHS.mStride == mStride); return (mPtr - inRHS.mPtr) / mStride; }

	/// Comparison operators
	inline bool				operator == (const StridedPtr &inRHS) const				{ return mPtr == inRHS.mPtr; }
	inline bool				operator != (const StridedPtr &inRHS) const				{ return mPtr != inRHS.mPtr; }
	inline bool				operator <= (const StridedPtr &inRHS) const				{ return mPtr <= inRHS.mPtr; }
	inline bool				operator >= (const StridedPtr &inRHS) const				{ return mPtr >= inRHS.mPtr; }
	inline bool				operator <  (const StridedPtr &inRHS) const				{ return mPtr <  inRHS.mPtr; }
	inline bool				operator >  (const StridedPtr &inRHS) const				{ return mPtr >  inRHS.mPtr; }

	/// Access value
	inline T &				operator * () const										{ return *reinterpret_cast<T *>(mPtr); }
	inline T *				operator -> () const									{ return reinterpret_cast<T *>(mPtr); }
	inline T &				operator [] (int inOffset) const						{ uint8 *ptr = mPtr + inOffset * mStride; return *reinterpret_cast<T *>(ptr); }

	/// Explicit conversion
	inline T *				GetPtr() const											{ return reinterpret_cast<T *>(mPtr); }

	/// Get stride in bytes
	inline int				GetStride() const										{ return mStride; }

private:
	uint8 *					mPtr = nullptr;											/// Pointer to element
	int						mStride = 0;											/// Stride (number of bytes) between elements
};

JPH_NAMESPACE_END
