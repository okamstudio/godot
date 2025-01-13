// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

#ifndef JPH_DISABLE_CUSTOM_ALLOCATOR

// Normal memory allocation, must be at least 8 byte aligned on 32 bit platform and 16 byte aligned on 64 bit platform
using AllocateFunction = void *(*)(size_t inSize);
using ReallocateFunction = void *(*)(void *inBlock, size_t inOldSize, size_t inNewSize);
using FreeFunction = void (*)(void *inBlock);

// Aligned memory allocation
using AlignedAllocateFunction = void *(*)(size_t inSize, size_t inAlignment);
using AlignedFreeFunction = void (*)(void *inBlock);

// User defined allocation / free functions
JPH_EXPORT extern AllocateFunction Allocate;
JPH_EXPORT extern ReallocateFunction Reallocate;
JPH_EXPORT extern FreeFunction Free;
JPH_EXPORT extern AlignedAllocateFunction AlignedAllocate;
JPH_EXPORT extern AlignedFreeFunction AlignedFree;

/// Register platform default allocation / free functions
JPH_EXPORT void RegisterDefaultAllocator();

/// Macro to override the new and delete functions
#define JPH_OVERRIDE_NEW_DELETE \
	JPH_INLINE void *operator new (size_t inCount)												{ return JPH::Allocate(inCount); } \
	JPH_INLINE void operator delete (void *inPointer) noexcept									{ JPH::Free(inPointer); } \
	JPH_INLINE void *operator new[] (size_t inCount)											{ return JPH::Allocate(inCount); } \
	JPH_INLINE void operator delete[] (void *inPointer) noexcept								{ JPH::Free(inPointer); } \
	JPH_INLINE void *operator new (size_t inCount, std::align_val_t inAlignment)				{ return JPH::AlignedAllocate(inCount, static_cast<size_t>(inAlignment)); } \
	JPH_INLINE void operator delete (void *inPointer, [[maybe_unused]] std::align_val_t inAlignment) noexcept	{ JPH::AlignedFree(inPointer); } \
	JPH_INLINE void *operator new[] (size_t inCount, std::align_val_t inAlignment)				{ return JPH::AlignedAllocate(inCount, static_cast<size_t>(inAlignment)); } \
	JPH_INLINE void operator delete[] (void *inPointer, [[maybe_unused]] std::align_val_t inAlignment) noexcept	{ JPH::AlignedFree(inPointer); } \
	JPH_INLINE void *operator new ([[maybe_unused]] size_t inCount, void *inPointer) noexcept	{ return inPointer; } \
	JPH_INLINE void operator delete ([[maybe_unused]] void *inPointer, [[maybe_unused]] void *inPlace) noexcept { /* Do nothing */ } \
	JPH_INLINE void *operator new[] ([[maybe_unused]] size_t inCount, void *inPointer) noexcept	{ return inPointer; } \
	JPH_INLINE void operator delete[] ([[maybe_unused]] void *inPointer, [[maybe_unused]] void *inPlace) noexcept { /* Do nothing */ }

#else

// Directly define the allocation functions
JPH_EXPORT void *Allocate(size_t inSize);
JPH_EXPORT void *Reallocate(void *inBlock, size_t inOldSize, size_t inNewSize);
JPH_EXPORT void Free(void *inBlock);
JPH_EXPORT void *AlignedAllocate(size_t inSize, size_t inAlignment);
JPH_EXPORT void AlignedFree(void *inBlock);

// Don't implement allocator registering
inline void RegisterDefaultAllocator() { }

// Don't override new/delete
#define JPH_OVERRIDE_NEW_DELETE

#endif // !JPH_DISABLE_CUSTOM_ALLOCATOR

JPH_NAMESPACE_END
