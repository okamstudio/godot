// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/NonCopyable.h>

JPH_NAMESPACE_BEGIN

/// Simple binary output stream
class JPH_EXPORT StreamOut : public NonCopyable
{
public:
	/// Virtual destructor
	virtual				~StreamOut() = default;

	/// Write a string of bytes to the binary stream
	virtual void		WriteBytes(const void *inData, size_t inNumBytes) = 0;

	/// Returns true if there was an IO failure
	virtual bool		IsFailed() const = 0;

	/// Write a primitive (e.g. float, int, etc.) to the binary stream
	template <class T, std::enable_if_t<std::is_trivially_copyable_v<T>, bool> = true>
	void				Write(const T &inT)
	{
		WriteBytes(&inT, sizeof(inT));
	}

	/// Write a vector of primitives to the binary stream
	template <class T, class A, std::enable_if_t<std::is_trivially_copyable_v<T>, bool> = true>
	void				Write(const Array<T, A> &inT)
	{
		uint32 len = uint32(inT.size());
		Write(len);
		if (!IsFailed())
		{
			if constexpr (std::is_same_v<T, Vec3> || std::is_same_v<T, DVec3> || std::is_same_v<T, DMat44>)
			{
				// These types have unused components that we don't want to write
				for (typename Array<T, A>::size_type i = 0; i < len; ++i)
					Write(inT[i]);
			}
			else
			{
				// Write all elements at once
				WriteBytes(inT.data(), len * sizeof(T));
			}
		}
	}

	/// Write a string to the binary stream (writes the number of characters and then the characters)
	template <class Type, class Traits, class Allocator>
	void				Write(const std::basic_string<Type, Traits, Allocator> &inString)
	{
		uint32 len = uint32(inString.size());
		Write(len);
		if (!IsFailed())
			WriteBytes(inString.data(), len * sizeof(Type));
	}

	/// Write a vector of primitives to the binary stream using a custom write function
	template <class T, class A, typename F>
	void				Write(const Array<T, A> &inT, const F &inWriteElement)
	{
		uint32 len = uint32(inT.size());
		Write(len);
		if (!IsFailed())
			for (typename Array<T, A>::size_type i = 0; i < len; ++i)
				inWriteElement(inT[i], *this);
	}

	/// Write a Vec3 (don't write W)
	void				Write(const Vec3 &inVec)
	{
		WriteBytes(&inVec, 3 * sizeof(float));
	}

	/// Write a DVec3 (don't write W)
	void				Write(const DVec3 &inVec)
	{
		WriteBytes(&inVec, 3 * sizeof(double));
	}

	/// Write a DMat44 (don't write W component of translation)
	void				Write(const DMat44 &inVec)
	{
		Write(inVec.GetColumn4(0));
		Write(inVec.GetColumn4(1));
		Write(inVec.GetColumn4(2));

		Write(inVec.GetTranslation());
	}
};

JPH_NAMESPACE_END
