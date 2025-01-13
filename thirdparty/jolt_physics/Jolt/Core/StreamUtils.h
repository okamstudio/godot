// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/Result.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/Core/UnorderedMap.h>
#include <Jolt/Core/Factory.h>

JPH_NAMESPACE_BEGIN

namespace StreamUtils {

template <class Type>
using ObjectToIDMap = UnorderedMap<const Type *, uint32>;

template <class Type>
using IDToObjectMap = Array<Ref<Type>>;

// Restore a single object by reading the hash of the type, constructing it and then calling the restore function
template <class Type>
Result<Ref<Type>>	RestoreObject(StreamIn &inStream, void (Type::*inRestoreBinaryStateFunction)(StreamIn &))
{
	Result<Ref<Type>> result;

	// Read the hash of the type
	uint32 hash;
	inStream.Read(hash);
	if (inStream.IsEOF() || inStream.IsFailed())
	{
		result.SetError("Failed to read type hash");
		return result;
	}

	// Get the RTTI for the type
	const RTTI *rtti = Factory::sInstance->Find(hash);
	if (rtti == nullptr)
	{
		result.SetError("Failed to create instance of type");
		return result;
	}

	// Construct and read the data of the type
	Ref<Type> object = reinterpret_cast<Type *>(rtti->CreateObject());
	(object->*inRestoreBinaryStateFunction)(inStream);
	if (inStream.IsEOF() || inStream.IsFailed())
	{
		result.SetError("Failed to restore object");
		return result;
	}

	result.Set(object);
	return result;
}

/// Save an object reference to a stream. Uses a map to map objects to IDs which is also used to prevent writing duplicates.
template <class Type>
void				SaveObjectReference(StreamOut &inStream, const Type *inObject, ObjectToIDMap<Type> *ioObjectToIDMap)
{
	if (ioObjectToIDMap == nullptr || inObject == nullptr)
	{
		// Write null ID
		inStream.Write(~uint32(0));
	}
	else
	{
		typename ObjectToIDMap<Type>::const_iterator id = ioObjectToIDMap->find(inObject);
		if (id != ioObjectToIDMap->end())
		{
			// Existing object, write ID
			inStream.Write(id->second);
		}
		else
		{
			// New object, write the ID
			uint32 new_id = uint32(ioObjectToIDMap->size());
			(*ioObjectToIDMap)[inObject] = new_id;
			inStream.Write(new_id);

			// Write the object
			inObject->SaveBinaryState(inStream);
		}
	}
}

/// Restore an object reference from stream.
template <class Type>
Result<Ref<Type>>	RestoreObjectReference(StreamIn &inStream, IDToObjectMap<Type> &ioIDToObjectMap)
{
	Result<Ref<Type>> result;

	// Read id
	uint32 id = ~uint32(0);
	inStream.Read(id);

	// Check null
	if (id == ~uint32(0))
	{
		result.Set(nullptr);
		return result;
	}

	// Check if it already exists
	if (id >= ioIDToObjectMap.size())
	{
		// New object, restore it
		result = Type::sRestoreFromBinaryState(inStream);
		if (result.HasError())
			return result;
		JPH_ASSERT(id == ioIDToObjectMap.size());
		ioIDToObjectMap.push_back(result.Get());
	}
	else
	{
		// Existing object filter
		result.Set(ioIDToObjectMap[id].GetPtr());
	}

	return result;
}

// Save an array of objects to a stream.
template <class ArrayType, class ValueType>
void				SaveObjectArray(StreamOut &inStream, const ArrayType &inArray, ObjectToIDMap<ValueType> *ioObjectToIDMap)
{
	uint32 len = uint32(inArray.size());
	inStream.Write(len);
	for (const ValueType *value: inArray)
		SaveObjectReference(inStream, value, ioObjectToIDMap);
}

// Restore an array of objects from a stream.
template <class ArrayType, class ValueType>
Result<ArrayType>	RestoreObjectArray(StreamIn &inStream, IDToObjectMap<ValueType> &ioIDToObjectMap)
{
	Result<ArrayType> result;

	uint32 len;
	inStream.Read(len);
	if (inStream.IsEOF() || inStream.IsFailed())
	{
		result.SetError("Failed to read stream");
		return result;
	}

	ArrayType values;
	values.reserve(len);
	for (size_t i = 0; i < len; ++i)
	{
		Result value = RestoreObjectReference(inStream, ioIDToObjectMap);
		if (value.HasError())
		{
			result.SetError(value.GetError());
			return result;
		}
		values.push_back(std::move(value.Get()));
	}

	result.Set(values);
	return result;
}

} // StreamUtils

JPH_NAMESPACE_END
