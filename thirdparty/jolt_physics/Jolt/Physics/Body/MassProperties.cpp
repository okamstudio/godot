// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/MassProperties.h>
#include <Jolt/Math/Matrix.h>
#include <Jolt/Math/Vector.h>
#include <Jolt/Math/EigenValueSymmetric.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/Core/InsertionSort.h>

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_NON_VIRTUAL(MassProperties)
{
	JPH_ADD_ATTRIBUTE(MassProperties, mMass)
	JPH_ADD_ATTRIBUTE(MassProperties, mInertia)
}

bool MassProperties::DecomposePrincipalMomentsOfInertia(Mat44 &outRotation, Vec3 &outDiagonal) const
{
	// Using eigendecomposition to get the principal components of the inertia tensor
	// See: https://en.wikipedia.org/wiki/Eigendecomposition_of_a_matrix
	Matrix<3, 3> inertia;
	inertia.CopyPart(mInertia, 0, 0, 3, 3, 0, 0);
	Matrix<3, 3> eigen_vec = Matrix<3, 3>::sIdentity();
	Vector<3> eigen_val;
	if (!EigenValueSymmetric(inertia, eigen_vec, eigen_val))
		return false;

	// Sort so that the biggest value goes first
	int indices[] = { 0, 1, 2 };
	InsertionSort(indices, indices + 3, [&eigen_val](int inLeft, int inRight) { return eigen_val[inLeft] > eigen_val[inRight]; });

	// Convert to a regular Mat44 and Vec3
	outRotation = Mat44::sIdentity();
	for (int i = 0; i < 3; ++i)
	{
		outRotation.SetColumn3(i, Vec3(reinterpret_cast<Float3 &>(eigen_vec.GetColumn(indices[i]))));
		outDiagonal.SetComponent(i, eigen_val[indices[i]]);
	}

	// Make sure that the rotation matrix is a right handed matrix
	if (outRotation.GetAxisX().Cross(outRotation.GetAxisY()).Dot(outRotation.GetAxisZ()) < 0.0f)
		outRotation.SetAxisZ(-outRotation.GetAxisZ());

#ifdef JPH_ENABLE_ASSERTS
	// Validate that the solution is correct, for each axis we want to make sure that the difference in inertia is
	// smaller than some fraction of the inertia itself in that axis
	Mat44 new_inertia = outRotation * Mat44::sScale(outDiagonal) * outRotation.Inversed();
	for (int i = 0; i < 3; ++i)
		JPH_ASSERT(new_inertia.GetColumn3(i).IsClose(mInertia.GetColumn3(i), mInertia.GetColumn3(i).LengthSq() * 1.0e-10f));
#endif

	return true;
}

void MassProperties::SetMassAndInertiaOfSolidBox(Vec3Arg inBoxSize, float inDensity)
{
	// Calculate mass
	mMass = inBoxSize.GetX() * inBoxSize.GetY() * inBoxSize.GetZ() * inDensity;

	// Calculate inertia
	Vec3 size_sq = inBoxSize * inBoxSize;
	Vec3 scale = (size_sq.Swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_X>() + size_sq.Swizzle<SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_Y>()) * (mMass / 12.0f);
	mInertia = Mat44::sScale(scale);
}

void MassProperties::ScaleToMass(float inMass)
{
	if (mMass > 0.0f)
	{
		// Calculate how much we have to scale the inertia tensor
		float mass_scale = inMass / mMass;

		// Update mass
		mMass = inMass;

		// Update inertia tensor
		for (int i = 0; i < 3; ++i)
			mInertia.SetColumn4(i, mInertia.GetColumn4(i) * mass_scale);
	}
	else
	{
		// Just set the mass
		mMass = inMass;
	}
}

Vec3 MassProperties::sGetEquivalentSolidBoxSize(float inMass, Vec3Arg inInertiaDiagonal)
{
	// Moment of inertia of a solid box has diagonal:
	// mass / 12 * [size_y^2 + size_z^2, size_x^2 + size_z^2, size_x^2 + size_y^2]
	// Solving for size_x, size_y and size_y (diagonal and mass are known):
	Vec3 diagonal = inInertiaDiagonal * (12.0f / inMass);
	return Vec3(sqrt(0.5f * (-diagonal[0] + diagonal[1] + diagonal[2])), sqrt(0.5f * (diagonal[0] - diagonal[1] + diagonal[2])), sqrt(0.5f * (diagonal[0] + diagonal[1] - diagonal[2])));
}

void MassProperties::Scale(Vec3Arg inScale)
{
	// See: https://en.wikipedia.org/wiki/Moment_of_inertia#Inertia_tensor
	// The diagonal of the inertia tensor can be calculated like this:
	// Ixx = sum_{k = 1 to n}(m_k * (y_k^2 + z_k^2))
	// Iyy = sum_{k = 1 to n}(m_k * (x_k^2 + z_k^2))
	// Izz = sum_{k = 1 to n}(m_k * (x_k^2 + y_k^2))
	//
	// We want to isolate the terms x_k, y_k and z_k:
	// d = [0.5, 0.5, 0.5].[Ixx, Iyy, Izz]
	// [sum_{k = 1 to n}(m_k * x_k^2), sum_{k = 1 to n}(m_k * y_k^2), sum_{k = 1 to n}(m_k * z_k^2)] = [d, d, d] - [Ixx, Iyy, Izz]
	Vec3 diagonal = mInertia.GetDiagonal3();
	Vec3 xyz_sq = Vec3::sReplicate(Vec3::sReplicate(0.5f).Dot(diagonal)) - diagonal;

	// When scaling a shape these terms change like this:
	// sum_{k = 1 to n}(m_k * (scale_x * x_k)^2) = scale_x^2 * sum_{k = 1 to n}(m_k * x_k^2)
	// Same for y_k and z_k
	// Using these terms we can calculate the new diagonal of the inertia tensor:
	Vec3 xyz_scaled_sq = inScale * inScale * xyz_sq;
	float i_xx = xyz_scaled_sq.GetY() + xyz_scaled_sq.GetZ();
	float i_yy = xyz_scaled_sq.GetX() + xyz_scaled_sq.GetZ();
	float i_zz = xyz_scaled_sq.GetX() + xyz_scaled_sq.GetY();

	// The off diagonal elements are calculated like:
	// Ixy = -sum_{k = 1 to n}(x_k y_k)
	// Ixz = -sum_{k = 1 to n}(x_k z_k)
	// Iyz = -sum_{k = 1 to n}(y_k z_k)
	// Scaling these is simple:
	float i_xy = inScale.GetX() * inScale.GetY() * mInertia(0, 1);
	float i_xz = inScale.GetX() * inScale.GetZ() * mInertia(0, 2);
	float i_yz = inScale.GetY() * inScale.GetZ() * mInertia(1, 2);

	// Update inertia tensor
	mInertia(0, 0) = i_xx;
	mInertia(0, 1) = i_xy;
	mInertia(1, 0) = i_xy;
	mInertia(1, 1) = i_yy;
	mInertia(0, 2) = i_xz;
	mInertia(2, 0) = i_xz;
	mInertia(1, 2) = i_yz;
	mInertia(2, 1) = i_yz;
	mInertia(2, 2) = i_zz;

	// Mass scales linear with volume (note that the scaling can be negative and we don't want the mass to become negative)
	float mass_scale = abs(inScale.GetX() * inScale.GetY() * inScale.GetZ());
	mMass *= mass_scale;

	// Inertia scales linear with mass. This updates the m_k terms above.
	mInertia *= mass_scale;

	// Ensure that the bottom right element is a 1 again
	mInertia(3, 3) = 1.0f;
}

void MassProperties::Rotate(Mat44Arg inRotation)
{
	mInertia = inRotation.Multiply3x3(mInertia).Multiply3x3RightTransposed(inRotation);
}

void MassProperties::Translate(Vec3Arg inTranslation)
{
	// Transform the inertia using the parallel axis theorem: I' = I + m * (translation^2 E - translation translation^T)
	// Where I is the original body's inertia and E the identity matrix
	// See: https://en.wikipedia.org/wiki/Parallel_axis_theorem
	mInertia += mMass * (Mat44::sScale(inTranslation.Dot(inTranslation)) - Mat44::sOuterProduct(inTranslation, inTranslation));

	// Ensure that inertia is a 3x3 matrix, adding inertias causes the bottom right element to change
	mInertia.SetColumn4(3, Vec4(0, 0, 0, 1));
}

void MassProperties::SaveBinaryState(StreamOut &inStream) const
{
	inStream.Write(mMass);
	inStream.Write(mInertia);
}

void MassProperties::RestoreBinaryState(StreamIn &inStream)
{
	inStream.Read(mMass);
	inStream.Read(mInertia);
}

JPH_NAMESPACE_END
