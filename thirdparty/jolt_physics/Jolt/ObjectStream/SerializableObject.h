// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/ObjectStream/ObjectStream.h>

JPH_NAMESPACE_BEGIN

//////////////////////////////////////////////////////////////////////////////////////////
// Helper macros
//////////////////////////////////////////////////////////////////////////////////////////

#ifdef JPH_OBJECT_STREAM

// JPH_DECLARE_SERIALIZATION_FUNCTIONS
#define JPH_DECLARE_SERIALIZATION_FUNCTIONS(linkage, prefix, class_name)											\
	linkage prefix bool	OSReadData(IObjectStreamIn &ioStream, class_name &inInstance);								\
	linkage prefix bool	OSReadData(IObjectStreamIn &ioStream, class_name *&inPointer);								\
	linkage prefix bool	OSIsType(class_name *, int inArrayDepth, EOSDataType inDataType, const char *inClassName);	\
	linkage prefix bool	OSIsType(class_name **, int inArrayDepth, EOSDataType inDataType, const char *inClassName);	\
	linkage prefix void	OSWriteData(IObjectStreamOut &ioStream, const class_name &inInstance);						\
	linkage prefix void	OSWriteData(IObjectStreamOut &ioStream, class_name *const &inPointer);						\
	linkage prefix void	OSWriteDataType(IObjectStreamOut &ioStream, class_name *);									\
	linkage prefix void	OSWriteDataType(IObjectStreamOut &ioStream, class_name **);

// JPH_IMPLEMENT_SERIALIZATION_FUNCTIONS
#define JPH_IMPLEMENT_SERIALIZATION_FUNCTIONS(class_name)															\
	bool		OSReadData(IObjectStreamIn &ioStream, class_name &inInstance)										\
	{																												\
		return ioStream.ReadClassData(#class_name, (void *)&inInstance);											\
	}																												\
	bool		OSReadData(IObjectStreamIn &ioStream, class_name *&inPointer)										\
	{																												\
		return ioStream.ReadPointerData(JPH_RTTI(class_name), (void **)&inPointer);									\
	}																												\
	bool		OSIsType(class_name *, int inArrayDepth, EOSDataType inDataType, const char *inClassName)			\
	{																												\
		return inArrayDepth == 0 && inDataType == EOSDataType::Instance && strcmp(inClassName, #class_name) == 0;	\
	}																												\
	bool		OSIsType(class_name **, int inArrayDepth, EOSDataType inDataType, const char *inClassName)			\
	{																												\
		return inArrayDepth == 0 && inDataType == EOSDataType::Pointer && strcmp(inClassName, #class_name) == 0;	\
	}																												\
	void		OSWriteData(IObjectStreamOut &ioStream, const class_name &inInstance)								\
	{																												\
		ioStream.WriteClassData(JPH_RTTI(class_name), (void *)&inInstance);											\
	}																												\
	void		OSWriteData(IObjectStreamOut &ioStream, class_name *const &inPointer)								\
	{																												\
		if (inPointer)																								\
			ioStream.WritePointerData(GetRTTI(inPointer), (void *)inPointer);										\
		else																										\
			ioStream.WritePointerData(nullptr, nullptr);															\
	}																												\
	void		OSWriteDataType(IObjectStreamOut &ioStream, class_name *)											\
	{																												\
		ioStream.WriteDataType(EOSDataType::Instance);																\
		ioStream.WriteName(#class_name);																			\
	}																												\
	void		OSWriteDataType(IObjectStreamOut &ioStream, class_name **)											\
	{																												\
		ioStream.WriteDataType(EOSDataType::Pointer);																\
		ioStream.WriteName(#class_name);																			\
	}

#else

#define JPH_DECLARE_SERIALIZATION_FUNCTIONS(...)
#define JPH_IMPLEMENT_SERIALIZATION_FUNCTIONS(...)

#endif // JPH_OBJECT_STREAM

//////////////////////////////////////////////////////////////////////////////////////////
// Use these macros on non-virtual objects to make them serializable
//////////////////////////////////////////////////////////////////////////////////////////

// JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL
#define JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(linkage, class_name)													\
public:																												\
	JPH_DECLARE_RTTI_NON_VIRTUAL(linkage, class_name)																\
	JPH_DECLARE_SERIALIZATION_FUNCTIONS(linkage, friend, class_name)												\

// JPH_IMPLEMENT_SERIALIZABLE_NON_VIRTUAL
#define JPH_IMPLEMENT_SERIALIZABLE_NON_VIRTUAL(class_name)															\
	JPH_IMPLEMENT_SERIALIZATION_FUNCTIONS(class_name)																\
	JPH_IMPLEMENT_RTTI_NON_VIRTUAL(class_name)																		\

//////////////////////////////////////////////////////////////////////////////////////////
// Same as above, but when you cannot insert the declaration in the class itself
//////////////////////////////////////////////////////////////////////////////////////////

// JPH_DECLARE_SERIALIZABLE_OUTSIDE_CLASS
#define JPH_DECLARE_SERIALIZABLE_OUTSIDE_CLASS(linkage, class_name)													\
	JPH_DECLARE_RTTI_OUTSIDE_CLASS(linkage, class_name)																\
	JPH_DECLARE_SERIALIZATION_FUNCTIONS(linkage, extern, class_name)												\

// JPH_IMPLEMENT_SERIALIZABLE_OUTSIDE_CLASS
#define JPH_IMPLEMENT_SERIALIZABLE_OUTSIDE_CLASS(class_name)														\
	JPH_IMPLEMENT_SERIALIZATION_FUNCTIONS(class_name)																\
	JPH_IMPLEMENT_RTTI_OUTSIDE_CLASS(class_name)																	\

//////////////////////////////////////////////////////////////////////////////////////////
// Same as above, but for classes that have virtual functions
//////////////////////////////////////////////////////////////////////////////////////////

// JPH_DECLARE_SERIALIZABLE_VIRTUAL - Use for concrete, non-base classes
#define JPH_DECLARE_SERIALIZABLE_VIRTUAL(linkage, class_name)														\
public:																												\
	JPH_DECLARE_RTTI_VIRTUAL(linkage, class_name)																	\
	JPH_DECLARE_SERIALIZATION_FUNCTIONS(linkage, friend, class_name)												\

// JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL
#define JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(class_name)																\
	JPH_IMPLEMENT_SERIALIZATION_FUNCTIONS(class_name)																\
	JPH_IMPLEMENT_RTTI_VIRTUAL(class_name)																			\

// JPH_DECLARE_SERIALIZABLE_ABSTRACT - Use for abstract, non-base classes
#define JPH_DECLARE_SERIALIZABLE_ABSTRACT(linkage, class_name)														\
public:																												\
	JPH_DECLARE_RTTI_ABSTRACT(linkage, class_name)																	\
	JPH_DECLARE_SERIALIZATION_FUNCTIONS(linkage, friend, class_name)												\

// JPH_IMPLEMENT_SERIALIZABLE_ABSTRACT
#define JPH_IMPLEMENT_SERIALIZABLE_ABSTRACT(class_name)																\
	JPH_IMPLEMENT_SERIALIZATION_FUNCTIONS(class_name)																\
	JPH_IMPLEMENT_RTTI_ABSTRACT(class_name)																			\

// JPH_DECLARE_SERIALIZABLE_VIRTUAL_BASE - Use for concrete base classes
#define JPH_DECLARE_SERIALIZABLE_VIRTUAL_BASE(linkage, class_name)													\
public:																												\
	JPH_DECLARE_RTTI_VIRTUAL_BASE(linkage, class_name)																\
	JPH_DECLARE_SERIALIZATION_FUNCTIONS(linkage, friend, class_name)												\

// JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL_BASE
#define JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL_BASE(class_name)															\
	JPH_IMPLEMENT_SERIALIZATION_FUNCTIONS(class_name)																\
	JPH_IMPLEMENT_RTTI_VIRTUAL_BASE(class_name)																		\

// JPH_DECLARE_SERIALIZABLE_ABSTRACT_BASE - Use for abstract base class
#define JPH_DECLARE_SERIALIZABLE_ABSTRACT_BASE(linkage, class_name)													\
public:																												\
	JPH_DECLARE_RTTI_ABSTRACT_BASE(linkage, class_name)																\
	JPH_DECLARE_SERIALIZATION_FUNCTIONS(linkage, friend, class_name)												\

// JPH_IMPLEMENT_SERIALIZABLE_ABSTRACT_BASE
#define JPH_IMPLEMENT_SERIALIZABLE_ABSTRACT_BASE(class_name)														\
	JPH_IMPLEMENT_SERIALIZATION_FUNCTIONS(class_name)																\
	JPH_IMPLEMENT_RTTI_ABSTRACT_BASE(class_name)

/// Classes must be derived from SerializableObject if you want to be able to save pointers or
/// reference counting pointers to objects of this or derived classes. The type will automatically
/// be determined during serialization and upon deserialization it will be restored correctly.
class JPH_EXPORT SerializableObject : public NonCopyable
{
	JPH_DECLARE_SERIALIZABLE_ABSTRACT_BASE(JPH_EXPORT, SerializableObject)

public:
	/// Constructor
	virtual						~SerializableObject() = default;
};

JPH_NAMESPACE_END
