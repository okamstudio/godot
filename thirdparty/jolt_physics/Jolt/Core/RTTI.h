// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/Reference.h>
#include <Jolt/Core/StaticArray.h>
#include <Jolt/ObjectStream/SerializableAttribute.h>

JPH_NAMESPACE_BEGIN

//////////////////////////////////////////////////////////////////////////////////////////
// RTTI
//////////////////////////////////////////////////////////////////////////////////////////

/// Light weight runtime type information system. This way we don't need to turn
/// on the default RTTI system of the compiler (introducing a possible overhead for every
/// class)
///
/// Notes:
///  - An extra virtual member function is added. This adds 8 bytes to the size of
///    an instance of the class (unless you are already using virtual functions).
///
/// To use RTTI on a specific class use:
///
/// Header file:
///
///		class Foo
///		{
///			JPH_DECLARE_RTTI_VIRTUAL_BASE(Foo)
///		}
///
///		class Bar : public Foo
///		{
///			JPH_DECLARE_RTTI_VIRTUAL(Bar)
///		};
///
/// Implementation file:
///
///		JPH_IMPLEMENT_RTTI_VIRTUAL_BASE(Foo)
///		{
///		}
///
///		JPH_IMPLEMENT_RTTI_VIRTUAL(Bar)
///		{
///			JPH_ADD_BASE_CLASS(Bar, Foo) // Multiple inheritance is allowed, just do JPH_ADD_BASE_CLASS for every base class
///		}
///
/// For abstract classes use:
///
/// Header file:
///
///		class Foo
///		{
///			JPH_DECLARE_RTTI_ABSTRACT_BASE(Foo)
///
///		public:
///			virtual void AbstractFunction() = 0;
///		}
///
///		class Bar : public Foo
///		{
///			JPH_DECLARE_RTTI_VIRTUAL(Bar)
///
///		public:
///			virtual void AbstractFunction() { } // Function is now implemented so this class is no longer abstract
///		};
///
/// Implementation file:
///
///		JPH_IMPLEMENT_RTTI_ABSTRACT_BASE(Foo)
///		{
///		}
///
///		JPH_IMPLEMENT_RTTI_VIRTUAL(Bar)
///		{
///			JPH_ADD_BASE_CLASS(Bar, Foo)
///		}
///
/// Example of usage in a program:
///
///		Foo *foo_ptr = new Foo;
///		Foo *bar_ptr = new Bar;
///
///		IsType(foo_ptr, RTTI(Bar)) returns false
///		IsType(bar_ptr, RTTI(Bar)) returns true
///
///		IsKindOf(foo_ptr, RTTI(Bar)) returns false
///		IsKindOf(bar_ptr, RTTI(Foo)) returns true
///		IsKindOf(bar_ptr, RTTI(Bar)) returns true
///
///		StaticCast<Bar>(foo_ptr) asserts and returns foo_ptr casted to Bar *
///		StaticCast<Bar>(bar_ptr) returns bar_ptr casted to Bar *
///
///		DynamicCast<Bar>(foo_ptr) returns nullptr
///		DynamicCast<Bar>(bar_ptr) returns bar_ptr casted to Bar *
///
/// Other feature of DynamicCast:
///
///		class A { int data[5]; };
///		class B { int data[7]; };
///		class C : public A, public B { int data[9]; };
///
///		C *c = new C;
///		A *a = c;
///
/// Note that:
///
///		B *b = (B *)a;
///
/// generates an invalid pointer,
///
///		B *b = StaticCast<B>(a);
///
/// doesn't compile, and
///
///		B *b = DynamicCast<B>(a);
///
/// does the correct cast
class JPH_EXPORT RTTI
{
public:
	/// Function to create an object
	using pCreateObjectFunction = void *(*)();

	/// Function to destroy an object
	using pDestructObjectFunction = void (*)(void *inObject);

	/// Function to initialize the runtime type info structure
	using pCreateRTTIFunction = void (*)(RTTI &inRTTI);

	/// Constructor
								RTTI(const char *inName, int inSize, pCreateObjectFunction inCreateObject, pDestructObjectFunction inDestructObject);
								RTTI(const char *inName, int inSize, pCreateObjectFunction inCreateObject, pDestructObjectFunction inDestructObject, pCreateRTTIFunction inCreateRTTI);

	// Properties
	inline const char *			GetName() const												{ return mName; }
	void						SetName(const char *inName)									{ mName = inName; }
	inline int					GetSize() const												{ return mSize; }
	bool						IsAbstract() const											{ return mCreate == nullptr || mDestruct == nullptr; }
	int							GetBaseClassCount() const;
	const RTTI *				GetBaseClass(int inIdx) const;
	uint32						GetHash() const;

	/// Create an object of this type (returns nullptr if the object is abstract)
	void *						CreateObject() const;

	/// Destruct object of this type (does nothing if the object is abstract)
	void						DestructObject(void *inObject) const;

	/// Add base class
	void						AddBaseClass(const RTTI *inRTTI, int inOffset);

	/// Equality operators
	bool						operator == (const RTTI &inRHS) const;
	bool						operator != (const RTTI &inRHS) const						{ return !(*this == inRHS); }

	/// Test if this class is derived from class of type inRTTI
	bool						IsKindOf(const RTTI *inRTTI) const;

	/// Cast inObject of this type to object of type inRTTI, returns nullptr if the cast is unsuccessful
	const void *				CastTo(const void *inObject, const RTTI *inRTTI) const;

#ifdef JPH_OBJECT_STREAM
	/// Attribute access
	void						AddAttribute(const SerializableAttribute &inAttribute);
	int							GetAttributeCount() const;
	const SerializableAttribute & GetAttribute(int inIdx) const;
#endif // JPH_OBJECT_STREAM

protected:
	/// Base class information
	struct BaseClass
	{
		const RTTI *			mRTTI;
		int						mOffset;
	};

	const char *				mName;														///< Class name
	int							mSize;														///< Class size
	StaticArray<BaseClass, 4>	mBaseClasses;												///< Names of base classes
	pCreateObjectFunction		mCreate;													///< Pointer to a function that will create a new instance of this class
	pDestructObjectFunction		mDestruct;													///< Pointer to a function that will destruct an object of this class
#ifdef JPH_OBJECT_STREAM
	StaticArray<SerializableAttribute, 32> mAttributes;										///< All attributes of this class
#endif // JPH_OBJECT_STREAM
};

//////////////////////////////////////////////////////////////////////////////////////////
// Add run time type info to types that don't have virtual functions
//////////////////////////////////////////////////////////////////////////////////////////

// JPH_DECLARE_RTTI_NON_VIRTUAL
#define JPH_DECLARE_RTTI_NON_VIRTUAL(linkage, class_name)															\
public:																												\
	JPH_OVERRIDE_NEW_DELETE																							\
	friend linkage RTTI *		GetRTTIOfType(class_name *);														\
	friend inline const RTTI *	GetRTTI([[maybe_unused]] const class_name *inObject) { return GetRTTIOfType(static_cast<class_name *>(nullptr)); }\
	static void					sCreateRTTI(RTTI &inRTTI);															\

// JPH_IMPLEMENT_RTTI_NON_VIRTUAL
#define JPH_IMPLEMENT_RTTI_NON_VIRTUAL(class_name)																	\
	RTTI *						GetRTTIOfType(class_name *)															\
	{																												\
		static RTTI rtti(#class_name, sizeof(class_name), []() -> void * { return new class_name; }, [](void *inObject) { delete (class_name *)inObject; }, &class_name::sCreateRTTI); \
		return &rtti;																								\
	}																												\
	void						class_name::sCreateRTTI(RTTI &inRTTI)												\

//////////////////////////////////////////////////////////////////////////////////////////
// Same as above, but when you cannot insert the declaration in the class
// itself, for example for templates and third party classes
//////////////////////////////////////////////////////////////////////////////////////////

// JPH_DECLARE_RTTI_OUTSIDE_CLASS
#define JPH_DECLARE_RTTI_OUTSIDE_CLASS(linkage, class_name)															\
	linkage RTTI *				GetRTTIOfType(class_name *);														\
	inline const RTTI *			GetRTTI(const class_name *inObject) { return GetRTTIOfType((class_name *)nullptr); }\
	void						CreateRTTI##class_name(RTTI &inRTTI);												\

// JPH_IMPLEMENT_RTTI_OUTSIDE_CLASS
#define JPH_IMPLEMENT_RTTI_OUTSIDE_CLASS(class_name)																\
	RTTI *						GetRTTIOfType(class_name *)															\
	{																												\
		static RTTI rtti((const char *)#class_name, sizeof(class_name), []() -> void * { return new class_name; }, [](void *inObject) { delete (class_name *)inObject; }, &CreateRTTI##class_name); \
		return &rtti;																								\
	}																												\
	void						CreateRTTI##class_name(RTTI &inRTTI)

//////////////////////////////////////////////////////////////////////////////////////////
// Same as above, but for classes that have virtual functions
//////////////////////////////////////////////////////////////////////////////////////////

#define JPH_DECLARE_RTTI_HELPER(linkage, class_name, modifier)														\
public:																												\
	JPH_OVERRIDE_NEW_DELETE																							\
	friend linkage RTTI *		GetRTTIOfType(class_name *);														\
	friend inline const RTTI *	GetRTTI(const class_name *inObject) { return inObject->GetRTTI(); }					\
	virtual const RTTI *		GetRTTI() const modifier;															\
	virtual const void *		CastTo(const RTTI *inRTTI) const modifier;											\
	static void					sCreateRTTI(RTTI &inRTTI);															\

// JPH_DECLARE_RTTI_VIRTUAL - for derived classes with RTTI
#define JPH_DECLARE_RTTI_VIRTUAL(linkage, class_name)																\
	JPH_DECLARE_RTTI_HELPER(linkage, class_name, override)

// JPH_IMPLEMENT_RTTI_VIRTUAL
#define JPH_IMPLEMENT_RTTI_VIRTUAL(class_name)																		\
	RTTI *			GetRTTIOfType(class_name *)																		\
	{																												\
		static RTTI rtti(#class_name, sizeof(class_name), []() -> void * { return new class_name; }, [](void *inObject) { delete (class_name *)inObject; }, &class_name::sCreateRTTI); \
		return &rtti;																								\
	}																												\
	const RTTI *				class_name::GetRTTI() const															\
	{																												\
		return JPH_RTTI(class_name);																				\
	}																												\
	const void *				class_name::CastTo(const RTTI *inRTTI) const										\
	{																												\
		return JPH_RTTI(class_name)->CastTo((const void *)this, inRTTI);											\
	}																												\
	void						class_name::sCreateRTTI(RTTI &inRTTI)												\

// JPH_DECLARE_RTTI_VIRTUAL_BASE - for concrete base class that has RTTI
#define JPH_DECLARE_RTTI_VIRTUAL_BASE(linkage, class_name)															\
	JPH_DECLARE_RTTI_HELPER(linkage, class_name, )

// JPH_IMPLEMENT_RTTI_VIRTUAL_BASE
#define JPH_IMPLEMENT_RTTI_VIRTUAL_BASE(class_name)																	\
	JPH_IMPLEMENT_RTTI_VIRTUAL(class_name)

// JPH_DECLARE_RTTI_ABSTRACT - for derived abstract class that have RTTI
#define JPH_DECLARE_RTTI_ABSTRACT(linkage, class_name)																\
	JPH_DECLARE_RTTI_HELPER(linkage, class_name, override)

// JPH_IMPLEMENT_RTTI_ABSTRACT
#define JPH_IMPLEMENT_RTTI_ABSTRACT(class_name)																		\
	RTTI *						GetRTTIOfType(class_name *)															\
	{																												\
		static RTTI rtti(#class_name, sizeof(class_name), nullptr, [](void *inObject) { delete (class_name *)inObject; }, &class_name::sCreateRTTI); \
		return &rtti;																								\
	}																												\
	const RTTI *				class_name::GetRTTI() const															\
	{																												\
		return JPH_RTTI(class_name);																				\
	}																												\
	const void *				class_name::CastTo(const RTTI *inRTTI) const										\
	{																												\
		return JPH_RTTI(class_name)->CastTo((const void *)this, inRTTI);											\
	}																												\
	void						class_name::sCreateRTTI(RTTI &inRTTI)												\

// JPH_DECLARE_RTTI_ABSTRACT_BASE - for abstract base class that has RTTI
#define JPH_DECLARE_RTTI_ABSTRACT_BASE(linkage, class_name)															\
	JPH_DECLARE_RTTI_HELPER(linkage, class_name, )

// JPH_IMPLEMENT_RTTI_ABSTRACT_BASE
#define JPH_IMPLEMENT_RTTI_ABSTRACT_BASE(class_name)																\
	JPH_IMPLEMENT_RTTI_ABSTRACT(class_name)

//////////////////////////////////////////////////////////////////////////////////////////
// Declare an RTTI class for registering with the factory
//////////////////////////////////////////////////////////////////////////////////////////

#define JPH_DECLARE_RTTI_FOR_FACTORY(linkage, class_name)															\
	linkage RTTI *				GetRTTIOfType(class class_name *);

#define JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(linkage, name_space, class_name)								\
	namespace name_space {																							\
		class class_name;																							\
		linkage RTTI *			GetRTTIOfType(class class_name *);													\
	}

//////////////////////////////////////////////////////////////////////////////////////////
// Find the RTTI of a class
//////////////////////////////////////////////////////////////////////////////////////////

#define JPH_RTTI(class_name)	GetRTTIOfType(static_cast<class_name *>(nullptr))

//////////////////////////////////////////////////////////////////////////////////////////
// Macro to rename a class, useful for embedded classes:
//
// class A { class B { }; }
//
// Now use JPH_RENAME_CLASS(B, A::B) to avoid conflicts with other classes named B
//////////////////////////////////////////////////////////////////////////////////////////

// JPH_RENAME_CLASS
#define JPH_RENAME_CLASS(class_name, new_name)																		\
								inRTTI.SetName(#new_name);

//////////////////////////////////////////////////////////////////////////////////////////
// Macro to add base classes
//////////////////////////////////////////////////////////////////////////////////////////

/// Define very dirty macro to get the offset of a baseclass into a class
#define JPH_BASE_CLASS_OFFSET(inClass, inBaseClass)	((int(uint64((inBaseClass *)((inClass *)0x10000))))-0x10000)

// JPH_ADD_BASE_CLASS
#define JPH_ADD_BASE_CLASS(class_name, base_class_name)																\
								inRTTI.AddBaseClass(JPH_RTTI(base_class_name), JPH_BASE_CLASS_OFFSET(class_name, base_class_name));

//////////////////////////////////////////////////////////////////////////////////////////
// Macros and templates to identify a class
//////////////////////////////////////////////////////////////////////////////////////////

/// Check if inObject is of DstType
template <class Type>
inline bool IsType(const Type *inObject, const RTTI *inRTTI)
{
	return inObject == nullptr || *inObject->GetRTTI() == *inRTTI;
}

template <class Type>
inline bool IsType(const RefConst<Type> &inObject, const RTTI *inRTTI)
{
	return inObject == nullptr || *inObject->GetRTTI() == *inRTTI;
}

template <class Type>
inline bool IsType(const Ref<Type> &inObject, const RTTI *inRTTI)
{
	return inObject == nullptr || *inObject->GetRTTI() == *inRTTI;
}

/// Check if inObject is or is derived from DstType
template <class Type>
inline bool IsKindOf(const Type *inObject, const RTTI *inRTTI)
{
	return inObject == nullptr || inObject->GetRTTI()->IsKindOf(inRTTI);
}

template <class Type>
inline bool IsKindOf(const RefConst<Type> &inObject, const RTTI *inRTTI)
{
	return inObject == nullptr || inObject->GetRTTI()->IsKindOf(inRTTI);
}

template <class Type>
inline bool IsKindOf(const Ref<Type> &inObject, const RTTI *inRTTI)
{
	return inObject == nullptr || inObject->GetRTTI()->IsKindOf(inRTTI);
}

/// Cast inObject to DstType, asserts on failure
template <class DstType, class SrcType, std::enable_if_t<std::is_base_of_v<DstType, SrcType> || std::is_base_of_v<SrcType, DstType>, bool> = true>
inline const DstType *StaticCast(const SrcType *inObject)
{
	return static_cast<const DstType *>(inObject);
}

template <class DstType, class SrcType, std::enable_if_t<std::is_base_of_v<DstType, SrcType> || std::is_base_of_v<SrcType, DstType>, bool> = true>
inline DstType *StaticCast(SrcType *inObject)
{
	return static_cast<DstType *>(inObject);
}

template <class DstType, class SrcType, std::enable_if_t<std::is_base_of_v<DstType, SrcType> || std::is_base_of_v<SrcType, DstType>, bool> = true>
inline const DstType *StaticCast(const RefConst<SrcType> &inObject)
{
	return static_cast<const DstType *>(inObject.GetPtr());
}

template <class DstType, class SrcType, std::enable_if_t<std::is_base_of_v<DstType, SrcType> || std::is_base_of_v<SrcType, DstType>, bool> = true>
inline DstType *StaticCast(const Ref<SrcType> &inObject)
{
	return static_cast<DstType *>(inObject.GetPtr());
}

/// Cast inObject to DstType, returns nullptr on failure
template <class DstType, class SrcType>
inline const DstType *DynamicCast(const SrcType *inObject)
{
	return inObject != nullptr? reinterpret_cast<const DstType *>(inObject->CastTo(JPH_RTTI(DstType))) : nullptr;
}

template <class DstType, class SrcType>
inline DstType *DynamicCast(SrcType *inObject)
{
	return inObject != nullptr? const_cast<DstType *>(reinterpret_cast<const DstType *>(inObject->CastTo(JPH_RTTI(DstType)))) : nullptr;
}

template <class DstType, class SrcType>
inline const DstType *DynamicCast(const RefConst<SrcType> &inObject)
{
	return inObject != nullptr? reinterpret_cast<const DstType *>(inObject->CastTo(JPH_RTTI(DstType))) : nullptr;
}

template <class DstType, class SrcType>
inline DstType *DynamicCast(const Ref<SrcType> &inObject)
{
	return inObject != nullptr? const_cast<DstType *>(reinterpret_cast<const DstType *>(inObject->CastTo(JPH_RTTI(DstType)))) : nullptr;
}

JPH_NAMESPACE_END
