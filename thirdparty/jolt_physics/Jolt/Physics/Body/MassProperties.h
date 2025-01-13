// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/ObjectStream/SerializableObject.h>

JPH_NAMESPACE_BEGIN

class StreamIn;
class StreamOut;

/// Describes the mass and inertia properties of a body. Used during body construction only.
class JPH_EXPORT MassProperties
{
	JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, MassProperties)

public:
	/// Using eigendecomposition, decompose the inertia tensor into a diagonal matrix D and a right-handed rotation matrix R so that the inertia tensor is \f$R \: D \: R^{-1}\f$.
	/// @see https://en.wikipedia.org/wiki/Moment_of_inertia section 'Principal axes'
	/// @param outRotation The rotation matrix R
	/// @param outDiagonal The diagonal of the diagonal matrix D
	/// @return True if successful, false if failed
	bool					DecomposePrincipalMomentsOfInertia(Mat44 &outRotation, Vec3 &outDiagonal) const;

	/// Set the mass and inertia of a box with edge size inBoxSize and density inDensity
	void					SetMassAndInertiaOfSolidBox(Vec3Arg inBoxSize, float inDensity);

	/// Set the mass and scale the inertia tensor to match the mass
	void					ScaleToMass(float inMass);

	/// Calculates the size of the solid box that has an inertia tensor diagonal inInertiaDiagonal
	static Vec3				sGetEquivalentSolidBoxSize(float inMass, Vec3Arg inInertiaDiagonal);

	/// Rotate the inertia by 3x3 matrix inRotation
	void					Rotate(Mat44Arg inRotation);

	/// Translate the inertia by a vector inTranslation
	void					Translate(Vec3Arg inTranslation);

	/// Scale the mass and inertia by inScale, note that elements can be < 0 to flip the shape
	void					Scale(Vec3Arg inScale);

	/// Saves the state of this object in binary form to inStream.
	void					SaveBinaryState(StreamOut &inStream) const;

	/// Restore the state of this object from inStream.
	void					RestoreBinaryState(StreamIn &inStream);

	/// Mass of the shape (kg)
	float					mMass = 0.0f;

	/// Inertia tensor of the shape (kg m^2)
	Mat44					mInertia = Mat44::sZero();
};

JPH_NAMESPACE_END
