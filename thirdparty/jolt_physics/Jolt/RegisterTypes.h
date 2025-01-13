// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// Internal helper function
JPH_EXPORT extern bool VerifyJoltVersionIDInternal(uint64 inVersionID);

/// This function can be used to verify the library ABI is compatible with your
/// application.
/// Use it in this way: `assert(VerifyJoltVersionID());`.
/// Returns `false` if the library used is not compatible with your app.
JPH_INLINE bool VerifyJoltVersionID() { return VerifyJoltVersionIDInternal(JPH_VERSION_ID); }

/// Internal helper function
JPH_EXPORT extern void RegisterTypesInternal(uint64 inVersionID);

/// Register all physics types with the factory and install their collision handlers with the CollisionDispatch class.
/// If you have your own custom shape types you probably need to register their handlers with the CollisionDispatch before calling this function.
/// If you implement your own default material (PhysicsMaterial::sDefault) make sure to initialize it before this function or else this function will create one for you.
JPH_INLINE void RegisterTypes() { RegisterTypesInternal(JPH_VERSION_ID); }

/// Unregisters all types with the factory and cleans up the default material
JPH_EXPORT extern void UnregisterTypes();

JPH_NAMESPACE_END
