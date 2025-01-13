// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/StaticArray.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Core/RTTI.h>
#include <Jolt/Core/NonCopyable.h>
#include <Jolt/ObjectStream/SerializableAttribute.h>

#ifdef JPH_OBJECT_STREAM

JPH_NAMESPACE_BEGIN

/// Base class for object stream input and output streams.
class JPH_EXPORT ObjectStream : public NonCopyable
{
public:
	/// Stream type
	enum class EStreamType
	{
		Text,
		Binary,
	};

protected:
	/// Destructor
	virtual							~ObjectStream() = default;

	/// Identifier for objects
	using Identifier = uint32;

	static constexpr int			sVersion = 1;
	static constexpr int			sRevision = 0;
	static constexpr Identifier		sNullIdentifier = 0;
};

/// Interface class for reading from an object stream
class JPH_EXPORT IObjectStreamIn : public ObjectStream
{
public:
	///@name Input type specific operations
	virtual bool				ReadDataType(EOSDataType &outType) = 0;
	virtual bool				ReadName(String &outName) = 0;
	virtual bool				ReadIdentifier(Identifier &outIdentifier) = 0;
	virtual bool				ReadCount(uint32 &outCount) = 0;

	///@name Read primitives
	virtual bool				ReadPrimitiveData(uint8 &outPrimitive) = 0;
	virtual bool				ReadPrimitiveData(uint16 &outPrimitive) = 0;
	virtual bool				ReadPrimitiveData(int &outPrimitive) = 0;
	virtual bool				ReadPrimitiveData(uint32 &outPrimitive) = 0;
	virtual bool				ReadPrimitiveData(uint64 &outPrimitive) = 0;
	virtual bool				ReadPrimitiveData(float &outPrimitive) = 0;
	virtual bool				ReadPrimitiveData(double &outPrimitive) = 0;
	virtual bool				ReadPrimitiveData(bool &outPrimitive) = 0;
	virtual bool				ReadPrimitiveData(String &outPrimitive) = 0;
	virtual bool				ReadPrimitiveData(Float3 &outPrimitive) = 0;
	virtual bool				ReadPrimitiveData(Double3 &outPrimitive) = 0;
	virtual bool				ReadPrimitiveData(Vec3 &outPrimitive) = 0;
	virtual bool				ReadPrimitiveData(DVec3 &outPrimitive) = 0;
	virtual bool				ReadPrimitiveData(Vec4 &outPrimitive) = 0;
	virtual bool				ReadPrimitiveData(Quat &outPrimitive) = 0;
	virtual bool				ReadPrimitiveData(Mat44 &outPrimitive) = 0;
	virtual bool				ReadPrimitiveData(DMat44 &outPrimitive) = 0;

	///@name Read compounds
	virtual bool				ReadClassData(const char *inClassName, void *inInstance) = 0;
	virtual bool				ReadPointerData(const RTTI *inRTTI, void **inPointer, int inRefCountOffset = -1) = 0;
};

/// Interface class for writing to an object stream
class JPH_EXPORT IObjectStreamOut : public ObjectStream
{
public:
	///@name Output type specific operations
	virtual void				WriteDataType(EOSDataType inType) = 0;
	virtual void				WriteName(const char *inName) = 0;
	virtual void				WriteIdentifier(Identifier inIdentifier) = 0;
	virtual void				WriteCount(uint32 inCount) = 0;

	///@name Write primitives
	virtual void				WritePrimitiveData(const uint8 &inPrimitive) = 0;
	virtual void				WritePrimitiveData(const uint16 &inPrimitive) = 0;
	virtual void				WritePrimitiveData(const int &inPrimitive) = 0;
	virtual void				WritePrimitiveData(const uint32 &inPrimitive) = 0;
	virtual void				WritePrimitiveData(const uint64 &inPrimitive) = 0;
	virtual void				WritePrimitiveData(const float &inPrimitive) = 0;
	virtual void				WritePrimitiveData(const double &inPrimitive) = 0;
	virtual void				WritePrimitiveData(const bool &inPrimitive) = 0;
	virtual void				WritePrimitiveData(const String &inPrimitive) = 0;
	virtual void				WritePrimitiveData(const Float3 &inPrimitive) = 0;
	virtual void				WritePrimitiveData(const Double3 &inPrimitive) = 0;
	virtual void				WritePrimitiveData(const Vec3 &inPrimitive) = 0;
	virtual void				WritePrimitiveData(const DVec3 &inPrimitive) = 0;
	virtual void				WritePrimitiveData(const Vec4 &inPrimitive) = 0;
	virtual void				WritePrimitiveData(const Quat &inPrimitive) = 0;
	virtual void				WritePrimitiveData(const Mat44 &inPrimitive) = 0;
	virtual void				WritePrimitiveData(const DMat44 &inPrimitive) = 0;

	///@name Write compounds
	virtual void				WritePointerData(const RTTI *inRTTI, const void *inPointer) = 0;
	virtual void				WriteClassData(const RTTI *inRTTI, const void *inInstance) = 0;

	///@name Layout hints (for text output)
	virtual void				HintNextItem()												{ /* Default is do nothing */ }
	virtual void				HintIndentUp()												{ /* Default is do nothing */ }
	virtual void				HintIndentDown()											{ /* Default is do nothing */ }
};

// Define macro to declare functions for a specific primitive type
#define JPH_DECLARE_PRIMITIVE(name)																			\
	JPH_EXPORT bool	OSIsType(name *, int inArrayDepth, EOSDataType inDataType, const char *inClassName);	\
	JPH_EXPORT bool	OSReadData(IObjectStreamIn &ioStream, name &outPrimitive);								\
	JPH_EXPORT void	OSWriteDataType(IObjectStreamOut &ioStream, name *);									\
	JPH_EXPORT void	OSWriteData(IObjectStreamOut &ioStream, const name &inPrimitive);

// This file uses the JPH_DECLARE_PRIMITIVE macro to define all types
#include <Jolt/ObjectStream/ObjectStreamTypes.h>

// Define serialization templates
template <class T, class A>
bool OSIsType(Array<T, A> *, int inArrayDepth, EOSDataType inDataType, const char *inClassName)
{
	return (inArrayDepth > 0 && OSIsType(static_cast<T *>(nullptr), inArrayDepth - 1, inDataType, inClassName));
}

template <class T, uint N>
bool OSIsType(StaticArray<T, N> *, int inArrayDepth, EOSDataType inDataType, const char *inClassName)
{
	return (inArrayDepth > 0 && OSIsType(static_cast<T *>(nullptr), inArrayDepth - 1, inDataType, inClassName));
}

template <class T, uint N>
bool OSIsType(T (*)[N], int inArrayDepth, EOSDataType inDataType, const char *inClassName)
{
	return (inArrayDepth > 0 && OSIsType(static_cast<T *>(nullptr), inArrayDepth - 1, inDataType, inClassName));
}

template <class T>
bool OSIsType(Ref<T> *, int inArrayDepth, EOSDataType inDataType, const char *inClassName)
{
	return OSIsType(static_cast<T *>(nullptr), inArrayDepth, inDataType, inClassName);
}

template <class T>
bool OSIsType(RefConst<T> *, int inArrayDepth, EOSDataType inDataType, const char *inClassName)
{
	return OSIsType(static_cast<T *>(nullptr), inArrayDepth, inDataType, inClassName);
}

/// Define serialization templates for dynamic arrays
template <class T, class A>
bool OSReadData(IObjectStreamIn &ioStream, Array<T, A> &inArray)
{
	bool continue_reading = true;

	// Read array length
	uint32 array_length;
	continue_reading = ioStream.ReadCount(array_length);

	// Read array items
	if (continue_reading)
	{
		inArray.clear();
		inArray.resize(array_length);
		for (uint32 el = 0; el < array_length && continue_reading; ++el)
			continue_reading = OSReadData(ioStream, inArray[el]);
	}

	return continue_reading;
}

/// Define serialization templates for static arrays
template <class T, uint N>
bool OSReadData(IObjectStreamIn &ioStream, StaticArray<T, N> &inArray)
{
	bool continue_reading = true;

	// Read array length
	uint32 array_length;
	continue_reading = ioStream.ReadCount(array_length);

	// Check if we can fit this many elements
	if (array_length > N)
		return false;

	// Read array items
	if (continue_reading)
	{
		inArray.clear();
		inArray.resize(array_length);
		for (uint32 el = 0; el < array_length && continue_reading; ++el)
			continue_reading = OSReadData(ioStream, inArray[el]);
	}

	return continue_reading;
}

/// Define serialization templates for C style arrays
template <class T, uint N>
bool OSReadData(IObjectStreamIn &ioStream, T (&inArray)[N])
{
	bool continue_reading = true;

	// Read array length
	uint32 array_length;
	continue_reading = ioStream.ReadCount(array_length);
	if (array_length != N)
		return false;

	// Read array items
	for (uint32 el = 0; el < N && continue_reading; ++el)
		continue_reading = OSReadData(ioStream, inArray[el]);

	return continue_reading;
}

/// Define serialization templates for references
template <class T>
bool OSReadData(IObjectStreamIn &ioStream, Ref<T> &inRef)
{
	return ioStream.ReadPointerData(JPH_RTTI(T), inRef.InternalGetPointer(), T::sInternalGetRefCountOffset());
}

template <class T>
bool OSReadData(IObjectStreamIn &ioStream, RefConst<T> &inRef)
{
	return ioStream.ReadPointerData(JPH_RTTI(T), inRef.InternalGetPointer(), T::sInternalGetRefCountOffset());
}

// Define serialization templates for dynamic arrays
template <class T, class A>
void OSWriteDataType(IObjectStreamOut &ioStream, Array<T, A> *)
{
	ioStream.WriteDataType(EOSDataType::Array);
	OSWriteDataType(ioStream, static_cast<T *>(nullptr));
}

template <class T, class A>
void OSWriteData(IObjectStreamOut &ioStream, const Array<T, A> &inArray)
{
	// Write size of array
	ioStream.HintNextItem();
	ioStream.WriteCount(static_cast<uint32>(inArray.size()));

	// Write data in array
	ioStream.HintIndentUp();
	for (const T &v : inArray)
		OSWriteData(ioStream, v);
	ioStream.HintIndentDown();
}

/// Define serialization templates for static arrays
template <class T, uint N>
void OSWriteDataType(IObjectStreamOut &ioStream, StaticArray<T, N> *)
{
	ioStream.WriteDataType(EOSDataType::Array);
	OSWriteDataType(ioStream, static_cast<T *>(nullptr));
}

template <class T, uint N>
void OSWriteData(IObjectStreamOut &ioStream, const StaticArray<T, N> &inArray)
{
	// Write size of array
	ioStream.HintNextItem();
	ioStream.WriteCount(inArray.size());

	// Write data in array
	ioStream.HintIndentUp();
	for (const typename StaticArray<T, N>::value_type &v : inArray)
		OSWriteData(ioStream, v);
	ioStream.HintIndentDown();
}

/// Define serialization templates for C style arrays
template <class T, uint N>
void OSWriteDataType(IObjectStreamOut &ioStream, T (*)[N])
{
	ioStream.WriteDataType(EOSDataType::Array);
	OSWriteDataType(ioStream, static_cast<T *>(nullptr));
}

template <class T, uint N>
void OSWriteData(IObjectStreamOut &ioStream, const T (&inArray)[N])
{
	// Write size of array
	ioStream.HintNextItem();
	ioStream.WriteCount(uint32(N));

	// Write data in array
	ioStream.HintIndentUp();
	for (const T &v : inArray)
		OSWriteData(ioStream, v);
	ioStream.HintIndentDown();
}

/// Define serialization templates for references
template <class T>
void OSWriteDataType(IObjectStreamOut &ioStream, Ref<T> *)
{
	OSWriteDataType(ioStream, static_cast<T *>(nullptr));
}

template <class T>
void OSWriteData(IObjectStreamOut &ioStream, const Ref<T> &inRef)
{
	if (inRef != nullptr)
		ioStream.WritePointerData(GetRTTI(inRef.GetPtr()), inRef.GetPtr());
	else
		ioStream.WritePointerData(nullptr, nullptr);
}

template <class T>
void OSWriteDataType(IObjectStreamOut &ioStream, RefConst<T> *)
{
	OSWriteDataType(ioStream, static_cast<T *>(nullptr));
}

template <class T>
void OSWriteData(IObjectStreamOut &ioStream, const RefConst<T> &inRef)
{
	if (inRef != nullptr)
		ioStream.WritePointerData(GetRTTI(inRef.GetPtr()), inRef.GetPtr());
	else
		ioStream.WritePointerData(nullptr, nullptr);
}

JPH_NAMESPACE_END

#endif // JPH_OBJECT_STREAM
