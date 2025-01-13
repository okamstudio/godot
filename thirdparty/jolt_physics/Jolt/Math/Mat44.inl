// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Math/Vec3.h>
#include <Jolt/Math/Vec4.h>
#include <Jolt/Math/Quat.h>

JPH_NAMESPACE_BEGIN

#define JPH_EL(r, c) mCol[c].mF32[r]

Mat44::Mat44(Vec4Arg inC1, Vec4Arg inC2, Vec4Arg inC3, Vec4Arg inC4) :
	mCol { inC1, inC2, inC3, inC4 }
{
}

Mat44::Mat44(Vec4Arg inC1, Vec4Arg inC2, Vec4Arg inC3, Vec3Arg inC4) :
	mCol { inC1, inC2, inC3, Vec4(inC4, 1.0f) }
{
}

Mat44::Mat44(Type inC1, Type inC2, Type inC3, Type inC4) :
	mCol { inC1, inC2, inC3, inC4 }
{
}

Mat44 Mat44::sZero()
{
	return Mat44(Vec4::sZero(), Vec4::sZero(), Vec4::sZero(), Vec4::sZero());
}

Mat44 Mat44::sIdentity()
{
	return Mat44(Vec4(1, 0, 0, 0), Vec4(0, 1, 0, 0), Vec4(0, 0, 1, 0), Vec4(0, 0, 0, 1));
}

Mat44 Mat44::sNaN()
{
	return Mat44(Vec4::sNaN(), Vec4::sNaN(), Vec4::sNaN(), Vec4::sNaN());
}

Mat44 Mat44::sLoadFloat4x4(const Float4 *inV)
{
	Mat44 result;
	for (int c = 0; c < 4; ++c)
		result.mCol[c] = Vec4::sLoadFloat4(inV + c);
	return result;
}

Mat44 Mat44::sLoadFloat4x4Aligned(const Float4 *inV)
{
	Mat44 result;
	for (int c = 0; c < 4; ++c)
		result.mCol[c] = Vec4::sLoadFloat4Aligned(inV + c);
	return result;
}

Mat44 Mat44::sRotationX(float inX)
{
	Vec4 sv, cv;
	Vec4::sReplicate(inX).SinCos(sv, cv);
	float s = sv.GetX(), c = cv.GetX();
	return Mat44(Vec4(1, 0, 0, 0), Vec4(0, c, s, 0), Vec4(0, -s, c, 0), Vec4(0, 0, 0, 1));
}

Mat44 Mat44::sRotationY(float inY)
{
	Vec4 sv, cv;
	Vec4::sReplicate(inY).SinCos(sv, cv);
	float s = sv.GetX(), c = cv.GetX();
	return Mat44(Vec4(c, 0, -s, 0), Vec4(0, 1, 0, 0), Vec4(s, 0, c, 0), Vec4(0, 0, 0, 1));
}

Mat44 Mat44::sRotationZ(float inZ)
{
	Vec4 sv, cv;
	Vec4::sReplicate(inZ).SinCos(sv, cv);
	float s = sv.GetX(), c = cv.GetX();
	return Mat44(Vec4(c, s, 0, 0), Vec4(-s, c, 0, 0), Vec4(0, 0, 1, 0), Vec4(0, 0, 0, 1));
}

Mat44 Mat44::sRotation(QuatArg inQuat)
{
	JPH_ASSERT(inQuat.IsNormalized());

	// See: https://en.wikipedia.org/wiki/Quaternions_and_spatial_rotation section 'Quaternion-derived rotation matrix'
#ifdef JPH_USE_SSE4_1
	__m128 xyzw = inQuat.mValue.mValue;
	__m128 two_xyzw = _mm_add_ps(xyzw, xyzw);
	__m128 yzxw = _mm_shuffle_ps(xyzw, xyzw, _MM_SHUFFLE(3, 0, 2, 1));
	__m128 two_yzxw = _mm_add_ps(yzxw, yzxw);
	__m128 zxyw = _mm_shuffle_ps(xyzw, xyzw, _MM_SHUFFLE(3, 1, 0, 2));
	__m128 two_zxyw = _mm_add_ps(zxyw, zxyw);
	__m128 wwww = _mm_shuffle_ps(xyzw, xyzw, _MM_SHUFFLE(3, 3, 3, 3));
	__m128 diagonal = _mm_sub_ps(_mm_sub_ps(_mm_set1_ps(1.0f), _mm_mul_ps(two_yzxw, yzxw)), _mm_mul_ps(two_zxyw, zxyw));	// (1 - 2 y^2 - 2 z^2, 1 - 2 x^2 - 2 z^2, 1 - 2 x^2 - 2 y^2, 1 - 4 w^2)
	__m128 plus = _mm_add_ps(_mm_mul_ps(two_xyzw, zxyw), _mm_mul_ps(two_yzxw, wwww));										// 2 * (xz + yw, xy + zw, yz + xw, ww)
	__m128 minus = _mm_sub_ps(_mm_mul_ps(two_yzxw, xyzw), _mm_mul_ps(two_zxyw, wwww));										// 2 * (xy - zw, yz - xw, xz - yw, 0)

	// Workaround for compiler changing _mm_sub_ps(_mm_mul_ps(...), ...) into a fused multiply sub instruction, resulting in w not being 0
	// There doesn't appear to be a reliable way to turn this off in Clang
	minus = _mm_insert_ps(minus, minus, 0b1000);

	__m128 col0 = _mm_blend_ps(_mm_blend_ps(plus, diagonal, 0b0001), minus, 0b1100);	// (1 - 2 y^2 - 2 z^2, 2 xy + 2 zw, 2 xz - 2 yw, 0)
	__m128 col1 = _mm_blend_ps(_mm_blend_ps(diagonal, minus, 0b1001), plus, 0b0100);	// (2 xy - 2 zw, 1 - 2 x^2 - 2 z^2, 2 yz + 2 xw, 0)
	__m128 col2 = _mm_blend_ps(_mm_blend_ps(minus, plus, 0b0001), diagonal, 0b0100);	// (2 xz + 2 yw, 2 yz - 2 xw, 1 - 2 x^2 - 2 y^2, 0)
	__m128 col3 = _mm_set_ps(1, 0, 0, 0);

	return Mat44(col0, col1, col2, col3);
#else
	float x = inQuat.GetX();
	float y = inQuat.GetY();
	float z = inQuat.GetZ();
	float w = inQuat.GetW();

	float tx = x + x; // Note: Using x + x instead of 2.0f * x to force this function to return the same value as the SSE4.1 version across platforms.
	float ty = y + y;
	float tz = z + z;

	float xx = tx * x;
	float yy = ty * y;
	float zz = tz * z;
	float xy = tx * y;
	float xz = tx * z;
	float xw = tx * w;
	float yz = ty * z;
	float yw = ty * w;
	float zw = tz * w;

	return Mat44(Vec4((1.0f - yy) - zz, xy + zw, xz - yw, 0.0f), // Note: Added extra brackets to force this function to return the same value as the SSE4.1 version across platforms.
				 Vec4(xy - zw, (1.0f - zz) - xx, yz + xw, 0.0f),
				 Vec4(xz + yw, yz - xw, (1.0f - xx) - yy, 0.0f),
				 Vec4(0.0f, 0.0f, 0.0f, 1.0f));
#endif
}

Mat44 Mat44::sRotation(Vec3Arg inAxis, float inAngle)
{
	return sRotation(Quat::sRotation(inAxis, inAngle));
}

Mat44 Mat44::sTranslation(Vec3Arg inV)
{
	return Mat44(Vec4(1, 0, 0, 0), Vec4(0, 1, 0, 0), Vec4(0, 0, 1, 0), Vec4(inV, 1));
}

Mat44 Mat44::sRotationTranslation(QuatArg inR, Vec3Arg inT)
{
	Mat44 m = sRotation(inR);
	m.SetTranslation(inT);
	return m;
}

Mat44 Mat44::sInverseRotationTranslation(QuatArg inR, Vec3Arg inT)
{
	Mat44 m = sRotation(inR.Conjugated());
	m.SetTranslation(-m.Multiply3x3(inT));
	return m;
}

Mat44 Mat44::sScale(float inScale)
{
	return Mat44(Vec4(inScale, 0, 0, 0), Vec4(0, inScale, 0, 0), Vec4(0, 0, inScale, 0), Vec4(0, 0, 0, 1));
}

Mat44 Mat44::sScale(Vec3Arg inV)
{
	return Mat44(Vec4(inV.GetX(), 0, 0, 0), Vec4(0, inV.GetY(), 0, 0), Vec4(0, 0, inV.GetZ(), 0), Vec4(0, 0, 0, 1));
}

Mat44 Mat44::sOuterProduct(Vec3Arg inV1, Vec3Arg inV2)
{
	Vec4 v1(inV1, 0);
	return Mat44(v1 * inV2.SplatX(), v1 * inV2.SplatY(), v1 * inV2.SplatZ(), Vec4(0, 0, 0, 1));
}

Mat44 Mat44::sCrossProduct(Vec3Arg inV)
{
#ifdef JPH_USE_SSE4_1
	// Zero out the W component
	__m128 zero = _mm_setzero_ps();
	__m128 v = _mm_blend_ps(inV.mValue, zero, 0b1000);

	// Negate
	__m128 min_v = _mm_sub_ps(zero, v);

	return Mat44(
		_mm_shuffle_ps(v, min_v, _MM_SHUFFLE(3, 1, 2, 3)), // [0, z, -y, 0]
		_mm_shuffle_ps(min_v, v, _MM_SHUFFLE(3, 0, 3, 2)), // [-z, 0, x, 0]
		_mm_blend_ps(_mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 3, 3, 1)), _mm_shuffle_ps(min_v, min_v, _MM_SHUFFLE(3, 3, 0, 3)), 0b0010), // [y, -x, 0, 0]
		Vec4(0, 0, 0, 1));
#else
	float x = inV.GetX();
	float y = inV.GetY();
	float z = inV.GetZ();

	return Mat44(
		Vec4(0, z, -y, 0),
		Vec4(-z, 0, x, 0),
		Vec4(y, -x, 0, 0),
		Vec4(0, 0, 0, 1));
#endif
}

Mat44 Mat44::sLookAt(Vec3Arg inPos, Vec3Arg inTarget, Vec3Arg inUp)
{
	Vec3 direction = (inTarget - inPos).NormalizedOr(-Vec3::sAxisZ());
	Vec3 right = direction.Cross(inUp).NormalizedOr(Vec3::sAxisX());
	Vec3 up = right.Cross(direction);

	return Mat44(Vec4(right, 0), Vec4(up, 0), Vec4(-direction, 0), Vec4(inPos, 1)).InversedRotationTranslation();
}

Mat44 Mat44::sPerspective(float inFovY, float inAspect, float inNear, float inFar)
{
	float height = 1.0f / Tan(0.5f * inFovY);
	float width = height / inAspect;
	float range = inFar / (inNear - inFar);

	return Mat44(Vec4(width, 0.0f, 0.0f, 0.0f), Vec4(0.0f, height, 0.0f, 0.0f), Vec4(0.0f, 0.0f, range, -1.0f), Vec4(0.0f, 0.0f, range * inNear, 0.0f));
}

bool Mat44::operator == (Mat44Arg inM2) const
{
	return UVec4::sAnd(
		UVec4::sAnd(Vec4::sEquals(mCol[0], inM2.mCol[0]), Vec4::sEquals(mCol[1], inM2.mCol[1])),
		UVec4::sAnd(Vec4::sEquals(mCol[2], inM2.mCol[2]), Vec4::sEquals(mCol[3], inM2.mCol[3]))
	).TestAllTrue();
}

bool Mat44::IsClose(Mat44Arg inM2, float inMaxDistSq) const
{
	for (int i = 0; i < 4; ++i)
		if (!mCol[i].IsClose(inM2.mCol[i], inMaxDistSq))
			return false;
	return true;
}

Mat44 Mat44::operator * (Mat44Arg inM) const
{
	Mat44 result;
#if defined(JPH_USE_SSE)
	for (int i = 0; i < 4; ++i)
	{
		__m128 c = inM.mCol[i].mValue;
		__m128 t = _mm_mul_ps(mCol[0].mValue, _mm_shuffle_ps(c, c, _MM_SHUFFLE(0, 0, 0, 0)));
		t = _mm_add_ps(t, _mm_mul_ps(mCol[1].mValue, _mm_shuffle_ps(c, c, _MM_SHUFFLE(1, 1, 1, 1))));
		t = _mm_add_ps(t, _mm_mul_ps(mCol[2].mValue, _mm_shuffle_ps(c, c, _MM_SHUFFLE(2, 2, 2, 2))));
		t = _mm_add_ps(t, _mm_mul_ps(mCol[3].mValue, _mm_shuffle_ps(c, c, _MM_SHUFFLE(3, 3, 3, 3))));
		result.mCol[i].mValue = t;
	}
#elif defined(JPH_USE_NEON)
	for (int i = 0; i < 4; ++i)
	{
		Type c = inM.mCol[i].mValue;
		Type t = vmulq_f32(mCol[0].mValue, vdupq_laneq_f32(c, 0));
		t = vmlaq_f32(t, mCol[1].mValue, vdupq_laneq_f32(c, 1));
		t = vmlaq_f32(t, mCol[2].mValue, vdupq_laneq_f32(c, 2));
		t = vmlaq_f32(t, mCol[3].mValue, vdupq_laneq_f32(c, 3));
		result.mCol[i].mValue = t;
	}
#else
	for (int i = 0; i < 4; ++i)
		result.mCol[i] = mCol[0] * inM.mCol[i].mF32[0] + mCol[1] * inM.mCol[i].mF32[1] + mCol[2] * inM.mCol[i].mF32[2] + mCol[3] * inM.mCol[i].mF32[3];
#endif
	return result;
}

Vec3 Mat44::operator * (Vec3Arg inV) const
{
#if defined(JPH_USE_SSE)
	__m128 t = _mm_mul_ps(mCol[0].mValue, _mm_shuffle_ps(inV.mValue, inV.mValue, _MM_SHUFFLE(0, 0, 0, 0)));
	t = _mm_add_ps(t, _mm_mul_ps(mCol[1].mValue, _mm_shuffle_ps(inV.mValue, inV.mValue, _MM_SHUFFLE(1, 1, 1, 1))));
	t = _mm_add_ps(t, _mm_mul_ps(mCol[2].mValue, _mm_shuffle_ps(inV.mValue, inV.mValue, _MM_SHUFFLE(2, 2, 2, 2))));
	t = _mm_add_ps(t, mCol[3].mValue);
	return Vec3::sFixW(t);
#elif defined(JPH_USE_NEON)
	Type t = vmulq_f32(mCol[0].mValue, vdupq_laneq_f32(inV.mValue, 0));
	t = vmlaq_f32(t, mCol[1].mValue, vdupq_laneq_f32(inV.mValue, 1));
	t = vmlaq_f32(t, mCol[2].mValue, vdupq_laneq_f32(inV.mValue, 2));
	t = vaddq_f32(t, mCol[3].mValue); // Don't combine this with the first mul into a fused multiply add, causes precision issues
	return Vec3::sFixW(t);
#else
	return Vec3(
		mCol[0].mF32[0] * inV.mF32[0] + mCol[1].mF32[0] * inV.mF32[1] + mCol[2].mF32[0] * inV.mF32[2] + mCol[3].mF32[0],
		mCol[0].mF32[1] * inV.mF32[0] + mCol[1].mF32[1] * inV.mF32[1] + mCol[2].mF32[1] * inV.mF32[2] + mCol[3].mF32[1],
		mCol[0].mF32[2] * inV.mF32[0] + mCol[1].mF32[2] * inV.mF32[1] + mCol[2].mF32[2] * inV.mF32[2] + mCol[3].mF32[2]);
#endif
}

Vec4 Mat44::operator * (Vec4Arg inV) const
{
#if defined(JPH_USE_SSE)
	__m128 t = _mm_mul_ps(mCol[0].mValue, _mm_shuffle_ps(inV.mValue, inV.mValue, _MM_SHUFFLE(0, 0, 0, 0)));
	t = _mm_add_ps(t, _mm_mul_ps(mCol[1].mValue, _mm_shuffle_ps(inV.mValue, inV.mValue, _MM_SHUFFLE(1, 1, 1, 1))));
	t = _mm_add_ps(t, _mm_mul_ps(mCol[2].mValue, _mm_shuffle_ps(inV.mValue, inV.mValue, _MM_SHUFFLE(2, 2, 2, 2))));
	t = _mm_add_ps(t, _mm_mul_ps(mCol[3].mValue, _mm_shuffle_ps(inV.mValue, inV.mValue, _MM_SHUFFLE(3, 3, 3, 3))));
	return t;
#elif defined(JPH_USE_NEON)
	Type t = vmulq_f32(mCol[0].mValue, vdupq_laneq_f32(inV.mValue, 0));
	t = vmlaq_f32(t, mCol[1].mValue, vdupq_laneq_f32(inV.mValue, 1));
	t = vmlaq_f32(t, mCol[2].mValue, vdupq_laneq_f32(inV.mValue, 2));
	t = vmlaq_f32(t, mCol[3].mValue, vdupq_laneq_f32(inV.mValue, 3));
	return t;
#else
	return Vec4(
		mCol[0].mF32[0] * inV.mF32[0] + mCol[1].mF32[0] * inV.mF32[1] + mCol[2].mF32[0] * inV.mF32[2] + mCol[3].mF32[0] * inV.mF32[3],
		mCol[0].mF32[1] * inV.mF32[0] + mCol[1].mF32[1] * inV.mF32[1] + mCol[2].mF32[1] * inV.mF32[2] + mCol[3].mF32[1] * inV.mF32[3],
		mCol[0].mF32[2] * inV.mF32[0] + mCol[1].mF32[2] * inV.mF32[1] + mCol[2].mF32[2] * inV.mF32[2] + mCol[3].mF32[2] * inV.mF32[3],
		mCol[0].mF32[3] * inV.mF32[0] + mCol[1].mF32[3] * inV.mF32[1] + mCol[2].mF32[3] * inV.mF32[2] + mCol[3].mF32[3] * inV.mF32[3]);
#endif
}

Vec3 Mat44::Multiply3x3(Vec3Arg inV) const
{
#if defined(JPH_USE_SSE)
	__m128 t = _mm_mul_ps(mCol[0].mValue, _mm_shuffle_ps(inV.mValue, inV.mValue, _MM_SHUFFLE(0, 0, 0, 0)));
	t = _mm_add_ps(t, _mm_mul_ps(mCol[1].mValue, _mm_shuffle_ps(inV.mValue, inV.mValue, _MM_SHUFFLE(1, 1, 1, 1))));
	t = _mm_add_ps(t, _mm_mul_ps(mCol[2].mValue, _mm_shuffle_ps(inV.mValue, inV.mValue, _MM_SHUFFLE(2, 2, 2, 2))));
	return Vec3::sFixW(t);
#elif defined(JPH_USE_NEON)
	Type t = vmulq_f32(mCol[0].mValue, vdupq_laneq_f32(inV.mValue, 0));
	t = vmlaq_f32(t, mCol[1].mValue, vdupq_laneq_f32(inV.mValue, 1));
	t = vmlaq_f32(t, mCol[2].mValue, vdupq_laneq_f32(inV.mValue, 2));
	return Vec3::sFixW(t);
#else
	return Vec3(
		mCol[0].mF32[0] * inV.mF32[0] + mCol[1].mF32[0] * inV.mF32[1] + mCol[2].mF32[0] * inV.mF32[2],
		mCol[0].mF32[1] * inV.mF32[0] + mCol[1].mF32[1] * inV.mF32[1] + mCol[2].mF32[1] * inV.mF32[2],
		mCol[0].mF32[2] * inV.mF32[0] + mCol[1].mF32[2] * inV.mF32[1] + mCol[2].mF32[2] * inV.mF32[2]);
#endif
}

Vec3 Mat44::Multiply3x3Transposed(Vec3Arg inV) const
{
#if defined(JPH_USE_SSE4_1)
	__m128 x = _mm_dp_ps(mCol[0].mValue, inV.mValue, 0x7f);
	__m128 y = _mm_dp_ps(mCol[1].mValue, inV.mValue, 0x7f);
	__m128 xy = _mm_blend_ps(x, y, 0b0010);
	__m128 z = _mm_dp_ps(mCol[2].mValue, inV.mValue, 0x7f);
	__m128 xyzz = _mm_blend_ps(xy, z, 0b1100);
	return xyzz;
#else
	return Transposed3x3().Multiply3x3(inV);
#endif
}

Mat44 Mat44::Multiply3x3(Mat44Arg inM) const
{
	JPH_ASSERT(mCol[0][3] == 0.0f);
	JPH_ASSERT(mCol[1][3] == 0.0f);
	JPH_ASSERT(mCol[2][3] == 0.0f);

	Mat44 result;
#if defined(JPH_USE_SSE)
	for (int i = 0; i < 3; ++i)
	{
		__m128 c = inM.mCol[i].mValue;
		__m128 t = _mm_mul_ps(mCol[0].mValue, _mm_shuffle_ps(c, c, _MM_SHUFFLE(0, 0, 0, 0)));
		t = _mm_add_ps(t, _mm_mul_ps(mCol[1].mValue, _mm_shuffle_ps(c, c, _MM_SHUFFLE(1, 1, 1, 1))));
		t = _mm_add_ps(t, _mm_mul_ps(mCol[2].mValue, _mm_shuffle_ps(c, c, _MM_SHUFFLE(2, 2, 2, 2))));
		result.mCol[i].mValue = t;
	}
#elif defined(JPH_USE_NEON)
	for (int i = 0; i < 3; ++i)
	{
		Type c = inM.mCol[i].mValue;
		Type t = vmulq_f32(mCol[0].mValue, vdupq_laneq_f32(c, 0));
		t = vmlaq_f32(t, mCol[1].mValue, vdupq_laneq_f32(c, 1));
		t = vmlaq_f32(t, mCol[2].mValue, vdupq_laneq_f32(c, 2));
		result.mCol[i].mValue = t;
	}
#else
	for (int i = 0; i < 3; ++i)
		result.mCol[i] = mCol[0] * inM.mCol[i].mF32[0] + mCol[1] * inM.mCol[i].mF32[1] + mCol[2] * inM.mCol[i].mF32[2];
#endif
	result.mCol[3] = Vec4(0, 0, 0, 1);
	return result;
}

Mat44 Mat44::Multiply3x3LeftTransposed(Mat44Arg inM) const
{
	// Transpose left hand side
	Mat44 trans = Transposed3x3();

	// Do 3x3 matrix multiply
	Mat44 result;
	result.mCol[0] = trans.mCol[0] * inM.mCol[0].SplatX() + trans.mCol[1] * inM.mCol[0].SplatY() + trans.mCol[2] * inM.mCol[0].SplatZ();
	result.mCol[1] = trans.mCol[0] * inM.mCol[1].SplatX() + trans.mCol[1] * inM.mCol[1].SplatY() + trans.mCol[2] * inM.mCol[1].SplatZ();
	result.mCol[2] = trans.mCol[0] * inM.mCol[2].SplatX() + trans.mCol[1] * inM.mCol[2].SplatY() + trans.mCol[2] * inM.mCol[2].SplatZ();
	result.mCol[3] = Vec4(0, 0, 0, 1);
	return result;
}

Mat44 Mat44::Multiply3x3RightTransposed(Mat44Arg inM) const
{
	JPH_ASSERT(mCol[0][3] == 0.0f);
	JPH_ASSERT(mCol[1][3] == 0.0f);
	JPH_ASSERT(mCol[2][3] == 0.0f);

	Mat44 result;
	result.mCol[0] = mCol[0] * inM.mCol[0].SplatX() + mCol[1] * inM.mCol[1].SplatX() + mCol[2] * inM.mCol[2].SplatX();
	result.mCol[1] = mCol[0] * inM.mCol[0].SplatY() + mCol[1] * inM.mCol[1].SplatY() + mCol[2] * inM.mCol[2].SplatY();
	result.mCol[2] = mCol[0] * inM.mCol[0].SplatZ() + mCol[1] * inM.mCol[1].SplatZ() + mCol[2] * inM.mCol[2].SplatZ();
	result.mCol[3] = Vec4(0, 0, 0, 1);
	return result;
}

Mat44 Mat44::operator * (float inV) const
{
	Vec4 multiplier = Vec4::sReplicate(inV);

	Mat44 result;
	for (int c = 0; c < 4; ++c)
		result.mCol[c] = mCol[c] * multiplier;
	return result;
}

Mat44 &Mat44::operator *= (float inV)
{
	for (int c = 0; c < 4; ++c)
		mCol[c] *= inV;

	return *this;
}

Mat44 Mat44::operator + (Mat44Arg inM) const
{
	Mat44 result;
	for (int i = 0; i < 4; ++i)
		result.mCol[i] = mCol[i] + inM.mCol[i];
	return result;
}

Mat44 Mat44::operator - () const
{
	Mat44 result;
	for (int i = 0; i < 4; ++i)
		result.mCol[i] = -mCol[i];
	return result;
}

Mat44 Mat44::operator - (Mat44Arg inM) const
{
	Mat44 result;
	for (int i = 0; i < 4; ++i)
		result.mCol[i] = mCol[i] - inM.mCol[i];
	return result;
}

Mat44 &Mat44::operator += (Mat44Arg inM)
{
	for (int c = 0; c < 4; ++c)
		mCol[c] += inM.mCol[c];

	return *this;
}

void Mat44::StoreFloat4x4(Float4 *outV) const
{
	for (int c = 0; c < 4; ++c)
		mCol[c].StoreFloat4(outV + c);
}

Mat44 Mat44::Transposed() const
{
#if defined(JPH_USE_SSE)
	__m128 tmp1 = _mm_shuffle_ps(mCol[0].mValue, mCol[1].mValue, _MM_SHUFFLE(1, 0, 1, 0));
	__m128 tmp3 = _mm_shuffle_ps(mCol[0].mValue, mCol[1].mValue, _MM_SHUFFLE(3, 2, 3, 2));
	__m128 tmp2 = _mm_shuffle_ps(mCol[2].mValue, mCol[3].mValue, _MM_SHUFFLE(1, 0, 1, 0));
	__m128 tmp4 = _mm_shuffle_ps(mCol[2].mValue, mCol[3].mValue, _MM_SHUFFLE(3, 2, 3, 2));

	Mat44 result;
	result.mCol[0].mValue = _mm_shuffle_ps(tmp1, tmp2, _MM_SHUFFLE(2, 0, 2, 0));
	result.mCol[1].mValue = _mm_shuffle_ps(tmp1, tmp2, _MM_SHUFFLE(3, 1, 3, 1));
	result.mCol[2].mValue = _mm_shuffle_ps(tmp3, tmp4, _MM_SHUFFLE(2, 0, 2, 0));
	result.mCol[3].mValue = _mm_shuffle_ps(tmp3, tmp4, _MM_SHUFFLE(3, 1, 3, 1));
	return result;
#elif defined(JPH_USE_NEON)
	float32x4x2_t tmp1 = vzipq_f32(mCol[0].mValue, mCol[2].mValue);
	float32x4x2_t tmp2 = vzipq_f32(mCol[1].mValue, mCol[3].mValue);
	float32x4x2_t tmp3 = vzipq_f32(tmp1.val[0], tmp2.val[0]);
	float32x4x2_t tmp4 = vzipq_f32(tmp1.val[1], tmp2.val[1]);

	Mat44 result;
	result.mCol[0].mValue = tmp3.val[0];
	result.mCol[1].mValue = tmp3.val[1];
	result.mCol[2].mValue = tmp4.val[0];
	result.mCol[3].mValue = tmp4.val[1];
	return result;
#else
	Mat44 result;
	for (int c = 0; c < 4; ++c)
		for (int r = 0; r < 4; ++r)
			result.mCol[r].mF32[c] = mCol[c].mF32[r];
	return result;
#endif
}

Mat44 Mat44::Transposed3x3() const
{
#if defined(JPH_USE_SSE)
	__m128 zero = _mm_setzero_ps();
	__m128 tmp1 = _mm_shuffle_ps(mCol[0].mValue, mCol[1].mValue, _MM_SHUFFLE(1, 0, 1, 0));
	__m128 tmp3 = _mm_shuffle_ps(mCol[0].mValue, mCol[1].mValue, _MM_SHUFFLE(3, 2, 3, 2));
	__m128 tmp2 = _mm_shuffle_ps(mCol[2].mValue, zero, _MM_SHUFFLE(1, 0, 1, 0));
	__m128 tmp4 = _mm_shuffle_ps(mCol[2].mValue, zero, _MM_SHUFFLE(3, 2, 3, 2));

	Mat44 result;
	result.mCol[0].mValue = _mm_shuffle_ps(tmp1, tmp2, _MM_SHUFFLE(2, 0, 2, 0));
	result.mCol[1].mValue = _mm_shuffle_ps(tmp1, tmp2, _MM_SHUFFLE(3, 1, 3, 1));
	result.mCol[2].mValue = _mm_shuffle_ps(tmp3, tmp4, _MM_SHUFFLE(2, 0, 2, 0));
#elif defined(JPH_USE_NEON)
	float32x4x2_t tmp1 = vzipq_f32(mCol[0].mValue, mCol[2].mValue);
	float32x4x2_t tmp2 = vzipq_f32(mCol[1].mValue, vdupq_n_f32(0));
	float32x4x2_t tmp3 = vzipq_f32(tmp1.val[0], tmp2.val[0]);
	float32x4x2_t tmp4 = vzipq_f32(tmp1.val[1], tmp2.val[1]);

	Mat44 result;
	result.mCol[0].mValue = tmp3.val[0];
	result.mCol[1].mValue = tmp3.val[1];
	result.mCol[2].mValue = tmp4.val[0];
#else
	Mat44 result;
	for (int c = 0; c < 3; ++c)
	{
		for (int r = 0; r < 3; ++r)
			result.mCol[c].mF32[r] = mCol[r].mF32[c];
		result.mCol[c].mF32[3] = 0;
	}
#endif
	result.mCol[3] = Vec4(0, 0, 0, 1);
	return result;
}

Mat44 Mat44::Inversed() const
{
#if defined(JPH_USE_SSE)
	// Algorithm from: http://download.intel.com/design/PentiumIII/sml/24504301.pdf
	// Streaming SIMD Extensions - Inverse of 4x4 Matrix
	// Adapted to load data using _mm_shuffle_ps instead of loading from memory
	// Replaced _mm_rcp_ps with _mm_div_ps for better accuracy

	__m128 tmp1 = _mm_shuffle_ps(mCol[0].mValue, mCol[1].mValue, _MM_SHUFFLE(1, 0, 1, 0));
	__m128 row1 = _mm_shuffle_ps(mCol[2].mValue, mCol[3].mValue, _MM_SHUFFLE(1, 0, 1, 0));
	__m128 row0 = _mm_shuffle_ps(tmp1, row1, _MM_SHUFFLE(2, 0, 2, 0));
	row1 = _mm_shuffle_ps(row1, tmp1, _MM_SHUFFLE(3, 1, 3, 1));
	tmp1 = _mm_shuffle_ps(mCol[0].mValue, mCol[1].mValue, _MM_SHUFFLE(3, 2, 3, 2));
	__m128 row3 = _mm_shuffle_ps(mCol[2].mValue, mCol[3].mValue, _MM_SHUFFLE(3, 2, 3, 2));
	__m128 row2 = _mm_shuffle_ps(tmp1, row3, _MM_SHUFFLE(2, 0, 2, 0));
	row3 = _mm_shuffle_ps(row3, tmp1, _MM_SHUFFLE(3, 1, 3, 1));

	tmp1 = _mm_mul_ps(row2, row3);
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(2, 3, 0, 1));
	__m128 minor0 = _mm_mul_ps(row1, tmp1);
	__m128 minor1 = _mm_mul_ps(row0, tmp1);
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(1, 0, 3, 2));
	minor0 = _mm_sub_ps(_mm_mul_ps(row1, tmp1), minor0);
	minor1 = _mm_sub_ps(_mm_mul_ps(row0, tmp1), minor1);
	minor1 = _mm_shuffle_ps(minor1, minor1, _MM_SHUFFLE(1, 0, 3, 2));

	tmp1 = _mm_mul_ps(row1, row2);
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(2, 3, 0, 1));
	minor0 = _mm_add_ps(_mm_mul_ps(row3, tmp1), minor0);
	__m128 minor3 = _mm_mul_ps(row0, tmp1);
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(1, 0, 3, 2));
	minor0 = _mm_sub_ps(minor0, _mm_mul_ps(row3, tmp1));
	minor3 = _mm_sub_ps(_mm_mul_ps(row0, tmp1), minor3);
	minor3 = _mm_shuffle_ps(minor3, minor3, _MM_SHUFFLE(1, 0, 3, 2));

	tmp1 = _mm_mul_ps(_mm_shuffle_ps(row1, row1, _MM_SHUFFLE(1, 0, 3, 2)), row3);
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(2, 3, 0, 1));
	row2 = _mm_shuffle_ps(row2, row2, _MM_SHUFFLE(1, 0, 3, 2));
	minor0 = _mm_add_ps(_mm_mul_ps(row2, tmp1), minor0);
	__m128 minor2 = _mm_mul_ps(row0, tmp1);
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(1, 0, 3, 2));
	minor0 = _mm_sub_ps(minor0, _mm_mul_ps(row2, tmp1));
	minor2 = _mm_sub_ps(_mm_mul_ps(row0, tmp1), minor2);
	minor2 = _mm_shuffle_ps(minor2, minor2, _MM_SHUFFLE(1, 0, 3, 2));

	tmp1 = _mm_mul_ps(row0, row1);
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(2, 3, 0, 1));
	minor2 = _mm_add_ps(_mm_mul_ps(row3, tmp1), minor2);
	minor3 = _mm_sub_ps(_mm_mul_ps(row2, tmp1), minor3);
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(1, 0, 3, 2));
	minor2 = _mm_sub_ps(_mm_mul_ps(row3, tmp1), minor2);
	minor3 = _mm_sub_ps(minor3, _mm_mul_ps(row2, tmp1));

	tmp1 = _mm_mul_ps(row0, row3);
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(2, 3, 0, 1));
	minor1 = _mm_sub_ps(minor1, _mm_mul_ps(row2, tmp1));
	minor2 = _mm_add_ps(_mm_mul_ps(row1, tmp1), minor2);
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(1, 0, 3, 2));
	minor1 = _mm_add_ps(_mm_mul_ps(row2, tmp1), minor1);
	minor2 = _mm_sub_ps(minor2, _mm_mul_ps(row1, tmp1));

	tmp1 = _mm_mul_ps(row0, row2);
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(2, 3, 0, 1));
	minor1 = _mm_add_ps(_mm_mul_ps(row3, tmp1), minor1);
	minor3 = _mm_sub_ps(minor3, _mm_mul_ps(row1, tmp1));
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(1, 0, 3, 2));
	minor1 = _mm_sub_ps(minor1, _mm_mul_ps(row3, tmp1));
	minor3 = _mm_add_ps(_mm_mul_ps(row1, tmp1), minor3);

	__m128 det = _mm_mul_ps(row0, minor0);
	det = _mm_add_ps(_mm_shuffle_ps(det, det, _MM_SHUFFLE(2, 3, 0, 1)), det); // Original code did (x + z) + (y + w), changed to (x + y) + (z + w) to match the ARM code below and make the result cross platform deterministic
	det = _mm_add_ss(_mm_shuffle_ps(det, det, _MM_SHUFFLE(1, 0, 3, 2)), det);
	det = _mm_div_ss(_mm_set_ss(1.0f), det);
	det = _mm_shuffle_ps(det, det, _MM_SHUFFLE(0, 0, 0, 0));

	Mat44 result;
	result.mCol[0].mValue = _mm_mul_ps(det, minor0);
	result.mCol[1].mValue = _mm_mul_ps(det, minor1);
	result.mCol[2].mValue = _mm_mul_ps(det, minor2);
	result.mCol[3].mValue = _mm_mul_ps(det, minor3);
	return result;
#elif defined(JPH_USE_NEON)
	// Adapted from the SSE version, there's surprising few articles about efficient ways of calculating an inverse for ARM on the internet
	Type tmp1 = JPH_NEON_SHUFFLE_F32x4(mCol[0].mValue, mCol[1].mValue, 0, 1, 4, 5);
	Type row1 = JPH_NEON_SHUFFLE_F32x4(mCol[2].mValue, mCol[3].mValue, 0, 1, 4, 5);
	Type row0 = JPH_NEON_SHUFFLE_F32x4(tmp1, row1, 0, 2, 4, 6);
	row1 = JPH_NEON_SHUFFLE_F32x4(row1, tmp1, 1, 3, 5, 7);
	tmp1 = JPH_NEON_SHUFFLE_F32x4(mCol[0].mValue, mCol[1].mValue, 2, 3, 6, 7);
	Type row3 = JPH_NEON_SHUFFLE_F32x4(mCol[2].mValue, mCol[3].mValue, 2, 3, 6, 7);
	Type row2 = JPH_NEON_SHUFFLE_F32x4(tmp1, row3, 0, 2, 4, 6);
	row3 = JPH_NEON_SHUFFLE_F32x4(row3, tmp1, 1, 3, 5, 7);

	tmp1 = vmulq_f32(row2, row3);
	tmp1 = JPH_NEON_SHUFFLE_F32x4(tmp1, tmp1, 1, 0, 3, 2);
	Type minor0 = vmulq_f32(row1, tmp1);
	Type minor1 = vmulq_f32(row0, tmp1);
	tmp1 = JPH_NEON_SHUFFLE_F32x4(tmp1, tmp1, 2, 3, 0, 1);
	minor0 = vsubq_f32(vmulq_f32(row1, tmp1), minor0);
	minor1 = vsubq_f32(vmulq_f32(row0, tmp1), minor1);
	minor1 = JPH_NEON_SHUFFLE_F32x4(minor1, minor1, 2, 3, 0, 1);

	tmp1 = vmulq_f32(row1, row2);
	tmp1 = JPH_NEON_SHUFFLE_F32x4(tmp1, tmp1, 1, 0, 3, 2);
	minor0 = vaddq_f32(vmulq_f32(row3, tmp1), minor0);
	Type minor3 = vmulq_f32(row0, tmp1);
	tmp1 = JPH_NEON_SHUFFLE_F32x4(tmp1, tmp1, 2, 3, 0, 1);
	minor0 = vsubq_f32(minor0, vmulq_f32(row3, tmp1));
	minor3 = vsubq_f32(vmulq_f32(row0, tmp1), minor3);
	minor3 = JPH_NEON_SHUFFLE_F32x4(minor3, minor3, 2, 3, 0, 1);

	tmp1 = JPH_NEON_SHUFFLE_F32x4(row1, row1, 2, 3, 0, 1);
	tmp1 = vmulq_f32(tmp1, row3);
	tmp1 = JPH_NEON_SHUFFLE_F32x4(tmp1, tmp1, 1, 0, 3, 2);
	row2 = JPH_NEON_SHUFFLE_F32x4(row2, row2, 2, 3, 0, 1);
	minor0 = vaddq_f32(vmulq_f32(row2, tmp1), minor0);
	Type minor2 = vmulq_f32(row0, tmp1);
	tmp1 = JPH_NEON_SHUFFLE_F32x4(tmp1, tmp1, 2, 3, 0, 1);
	minor0 = vsubq_f32(minor0, vmulq_f32(row2, tmp1));
	minor2 = vsubq_f32(vmulq_f32(row0, tmp1), minor2);
	minor2 = JPH_NEON_SHUFFLE_F32x4(minor2, minor2, 2, 3, 0, 1);

	tmp1 = vmulq_f32(row0, row1);
	tmp1 = JPH_NEON_SHUFFLE_F32x4(tmp1, tmp1, 1, 0, 3, 2);
	minor2 = vaddq_f32(vmulq_f32(row3, tmp1), minor2);
	minor3 = vsubq_f32(vmulq_f32(row2, tmp1), minor3);
	tmp1 = JPH_NEON_SHUFFLE_F32x4(tmp1, tmp1, 2, 3, 0, 1);
	minor2 = vsubq_f32(vmulq_f32(row3, tmp1), minor2);
	minor3 = vsubq_f32(minor3, vmulq_f32(row2, tmp1));

	tmp1 = vmulq_f32(row0, row3);
	tmp1 = JPH_NEON_SHUFFLE_F32x4(tmp1, tmp1, 1, 0, 3, 2);
	minor1 = vsubq_f32(minor1, vmulq_f32(row2, tmp1));
	minor2 = vaddq_f32(vmulq_f32(row1, tmp1), minor2);
	tmp1 = JPH_NEON_SHUFFLE_F32x4(tmp1, tmp1, 2, 3, 0, 1);
	minor1 = vaddq_f32(vmulq_f32(row2, tmp1), minor1);
	minor2 = vsubq_f32(minor2, vmulq_f32(row1, tmp1));

	tmp1 = vmulq_f32(row0, row2);
	tmp1 = JPH_NEON_SHUFFLE_F32x4(tmp1, tmp1, 1, 0, 3, 2);
	minor1 = vaddq_f32(vmulq_f32(row3, tmp1), minor1);
	minor3 = vsubq_f32(minor3, vmulq_f32(row1, tmp1));
	tmp1 = JPH_NEON_SHUFFLE_F32x4(tmp1, tmp1, 2, 3, 0, 1);
	minor1 = vsubq_f32(minor1, vmulq_f32(row3, tmp1));
	minor3 = vaddq_f32(vmulq_f32(row1, tmp1), minor3);

	Type det = vmulq_f32(row0, minor0);
	det = vdupq_n_f32(vaddvq_f32(det));
	det = vdivq_f32(vdupq_n_f32(1.0f), det);

	Mat44 result;
	result.mCol[0].mValue = vmulq_f32(det, minor0);
	result.mCol[1].mValue = vmulq_f32(det, minor1);
	result.mCol[2].mValue = vmulq_f32(det, minor2);
	result.mCol[3].mValue = vmulq_f32(det, minor3);
	return result;
#else
	float m00 = JPH_EL(0, 0), m10 = JPH_EL(1, 0), m20 = JPH_EL(2, 0), m30 = JPH_EL(3, 0);
	float m01 = JPH_EL(0, 1), m11 = JPH_EL(1, 1), m21 = JPH_EL(2, 1), m31 = JPH_EL(3, 1);
	float m02 = JPH_EL(0, 2), m12 = JPH_EL(1, 2), m22 = JPH_EL(2, 2), m32 = JPH_EL(3, 2);
	float m03 = JPH_EL(0, 3), m13 = JPH_EL(1, 3), m23 = JPH_EL(2, 3), m33 = JPH_EL(3, 3);

	float m10211120 = m10 * m21 - m11 * m20;
	float m10221220 = m10 * m22 - m12 * m20;
	float m10231320 = m10 * m23 - m13 * m20;
	float m10311130 = m10 * m31 - m11 * m30;
	float m10321230 = m10 * m32 - m12 * m30;
	float m10331330 = m10 * m33 - m13 * m30;
	float m11221221 = m11 * m22 - m12 * m21;
	float m11231321 = m11 * m23 - m13 * m21;
	float m11321231 = m11 * m32 - m12 * m31;
	float m11331331 = m11 * m33 - m13 * m31;
	float m12231322 = m12 * m23 - m13 * m22;
	float m12331332 = m12 * m33 - m13 * m32;
	float m20312130 = m20 * m31 - m21 * m30;
	float m20322230 = m20 * m32 - m22 * m30;
	float m20332330 = m20 * m33 - m23 * m30;
	float m21322231 = m21 * m32 - m22 * m31;
	float m21332331 = m21 * m33 - m23 * m31;
	float m22332332 = m22 * m33 - m23 * m32;

	Vec4 col0(m11 * m22332332 - m12 * m21332331 + m13 * m21322231,		-m10 * m22332332 + m12 * m20332330 - m13 * m20322230,		m10 * m21332331 - m11 * m20332330 + m13 * m20312130,		-m10 * m21322231 + m11 * m20322230 - m12 * m20312130);
	Vec4 col1(-m01 * m22332332 + m02 * m21332331 - m03 * m21322231,		m00 * m22332332 - m02 * m20332330 + m03 * m20322230,		-m00 * m21332331 + m01 * m20332330 - m03 * m20312130,		m00 * m21322231 - m01 * m20322230 + m02 * m20312130);
	Vec4 col2(m01 * m12331332 - m02 * m11331331 + m03 * m11321231,		-m00 * m12331332 + m02 * m10331330 - m03 * m10321230,		m00 * m11331331 - m01 * m10331330 + m03 * m10311130,		-m00 * m11321231 + m01 * m10321230 - m02 * m10311130);
	Vec4 col3(-m01 * m12231322 + m02 * m11231321 - m03 * m11221221,		m00 * m12231322 - m02 * m10231320 + m03 * m10221220,		-m00 * m11231321 + m01 * m10231320 - m03 * m10211120,		m00 * m11221221 - m01 * m10221220 + m02 * m10211120);

	float det = m00 * col0.mF32[0] + m01 * col0.mF32[1] + m02 * col0.mF32[2] + m03 * col0.mF32[3];

	return Mat44(col0 / det, col1 / det, col2 / det, col3 / det);
#endif
}

Mat44 Mat44::InversedRotationTranslation() const
{
	Mat44 m = Transposed3x3();
	m.SetTranslation(-m.Multiply3x3(GetTranslation()));
	return m;
}

float Mat44::GetDeterminant3x3() const
{
	return GetAxisX().Dot(GetAxisY().Cross(GetAxisZ()));
}

Mat44 Mat44::Adjointed3x3() const
{
	return Mat44(
		Vec4(JPH_EL(1, 1), JPH_EL(1, 2), JPH_EL(1, 0), 0) * Vec4(JPH_EL(2, 2), JPH_EL(2, 0), JPH_EL(2, 1), 0)
			- Vec4(JPH_EL(1, 2), JPH_EL(1, 0), JPH_EL(1, 1), 0) * Vec4(JPH_EL(2, 1), JPH_EL(2, 2), JPH_EL(2, 0), 0),
		Vec4(JPH_EL(0, 2), JPH_EL(0, 0), JPH_EL(0, 1), 0) * Vec4(JPH_EL(2, 1), JPH_EL(2, 2), JPH_EL(2, 0), 0)
			- Vec4(JPH_EL(0, 1), JPH_EL(0, 2), JPH_EL(0, 0), 0) * Vec4(JPH_EL(2, 2), JPH_EL(2, 0), JPH_EL(2, 1), 0),
		Vec4(JPH_EL(0, 1), JPH_EL(0, 2), JPH_EL(0, 0), 0) * Vec4(JPH_EL(1, 2), JPH_EL(1, 0), JPH_EL(1, 1), 0)
			- Vec4(JPH_EL(0, 2), JPH_EL(0, 0), JPH_EL(0, 1), 0) * Vec4(JPH_EL(1, 1), JPH_EL(1, 2), JPH_EL(1, 0), 0),
		Vec4(0, 0, 0, 1));
}

Mat44 Mat44::Inversed3x3() const
{
	float det = GetDeterminant3x3();

	return Mat44(
		(Vec4(JPH_EL(1, 1), JPH_EL(1, 2), JPH_EL(1, 0), 0) * Vec4(JPH_EL(2, 2), JPH_EL(2, 0), JPH_EL(2, 1), 0)
			- Vec4(JPH_EL(1, 2), JPH_EL(1, 0), JPH_EL(1, 1), 0) * Vec4(JPH_EL(2, 1), JPH_EL(2, 2), JPH_EL(2, 0), 0)) / det,
		(Vec4(JPH_EL(0, 2), JPH_EL(0, 0), JPH_EL(0, 1), 0) * Vec4(JPH_EL(2, 1), JPH_EL(2, 2), JPH_EL(2, 0), 0)
			- Vec4(JPH_EL(0, 1), JPH_EL(0, 2), JPH_EL(0, 0), 0) * Vec4(JPH_EL(2, 2), JPH_EL(2, 0), JPH_EL(2, 1), 0)) / det,
		(Vec4(JPH_EL(0, 1), JPH_EL(0, 2), JPH_EL(0, 0), 0) * Vec4(JPH_EL(1, 2), JPH_EL(1, 0), JPH_EL(1, 1), 0)
			- Vec4(JPH_EL(0, 2), JPH_EL(0, 0), JPH_EL(0, 1), 0) * Vec4(JPH_EL(1, 1), JPH_EL(1, 2), JPH_EL(1, 0), 0)) / det,
		Vec4(0, 0, 0, 1));
}

bool Mat44::SetInversed3x3(Mat44Arg inM)
{
	float det = inM.GetDeterminant3x3();

	// If the determinant is zero the matrix is singular and we return false
	if (det == 0.0f)
		return false;

	// Finish calculating the inverse
	*this = inM.Adjointed3x3();
	mCol[0] /= det;
	mCol[1] /= det;
	mCol[2] /= det;
	return true;
}

Quat Mat44::GetQuaternion() const
{
	float tr = mCol[0].mF32[0] + mCol[1].mF32[1] + mCol[2].mF32[2];

	if (tr >= 0.0f)
	{
		float s = sqrt(tr + 1.0f);
		float is = 0.5f / s;
		return Quat(
			(mCol[1].mF32[2] - mCol[2].mF32[1]) * is,
			(mCol[2].mF32[0] - mCol[0].mF32[2]) * is,
			(mCol[0].mF32[1] - mCol[1].mF32[0]) * is,
			0.5f * s);
	}
	else
	{
		int i = 0;
		if (mCol[1].mF32[1] > mCol[0].mF32[0]) i = 1;
		if (mCol[2].mF32[2] > mCol[i].mF32[i]) i = 2;

		if (i == 0)
		{
			float s = sqrt(mCol[0].mF32[0] - (mCol[1].mF32[1] + mCol[2].mF32[2]) + 1);
			float is = 0.5f / s;
			return Quat(
				0.5f * s,
				(mCol[1].mF32[0] + mCol[0].mF32[1]) * is,
				(mCol[0].mF32[2] + mCol[2].mF32[0]) * is,
				(mCol[1].mF32[2] - mCol[2].mF32[1]) * is);
		}
		else if (i == 1)
		{
			float s = sqrt(mCol[1].mF32[1] - (mCol[2].mF32[2] + mCol[0].mF32[0]) + 1);
			float is = 0.5f / s;
			return Quat(
				(mCol[1].mF32[0] + mCol[0].mF32[1]) * is,
				0.5f * s,
				(mCol[2].mF32[1] + mCol[1].mF32[2]) * is,
				(mCol[2].mF32[0] - mCol[0].mF32[2]) * is);
		}
		else
		{
			JPH_ASSERT(i == 2);

			float s = sqrt(mCol[2].mF32[2] - (mCol[0].mF32[0] + mCol[1].mF32[1]) + 1);
			float is = 0.5f / s;
			return Quat(
				(mCol[0].mF32[2] + mCol[2].mF32[0]) * is,
				(mCol[2].mF32[1] + mCol[1].mF32[2]) * is,
				0.5f * s,
				(mCol[0].mF32[1] - mCol[1].mF32[0]) * is);
		}
	}
}

Mat44 Mat44::sQuatLeftMultiply(QuatArg inQ)
{
	return Mat44(
		Vec4(1, 1, -1, -1) * inQ.mValue.Swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X>(),
		Vec4(-1, 1, 1, -1) * inQ.mValue.Swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X, SWIZZLE_Y>(),
		Vec4(1, -1, 1, -1) * inQ.mValue.Swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z>(),
		inQ.mValue);
}

Mat44 Mat44::sQuatRightMultiply(QuatArg inQ)
{
	return Mat44(
		Vec4(1, -1, 1, -1) * inQ.mValue.Swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X>(),
		Vec4(1, 1, -1, -1) * inQ.mValue.Swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X, SWIZZLE_Y>(),
		Vec4(-1, 1, 1, -1) * inQ.mValue.Swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z>(),
		inQ.mValue);
}

Mat44 Mat44::GetRotation() const
{
	JPH_ASSERT(mCol[0][3] == 0.0f);
	JPH_ASSERT(mCol[1][3] == 0.0f);
	JPH_ASSERT(mCol[2][3] == 0.0f);

	return Mat44(mCol[0], mCol[1], mCol[2], Vec4(0, 0, 0, 1));
}

Mat44 Mat44::GetRotationSafe() const
{
#if defined(JPH_USE_AVX512)
	return Mat44(_mm_maskz_mov_ps(0b0111, mCol[0].mValue),
				 _mm_maskz_mov_ps(0b0111, mCol[1].mValue),
				 _mm_maskz_mov_ps(0b0111, mCol[2].mValue),
				 Vec4(0, 0, 0, 1));
#elif defined(JPH_USE_SSE4_1)
	__m128 zero = _mm_setzero_ps();
	return Mat44(_mm_blend_ps(mCol[0].mValue, zero, 8),
				 _mm_blend_ps(mCol[1].mValue, zero, 8),
				 _mm_blend_ps(mCol[2].mValue, zero, 8),
				 Vec4(0, 0, 0, 1));
#elif defined(JPH_USE_NEON)
	return Mat44(vsetq_lane_f32(0, mCol[0].mValue, 3),
				 vsetq_lane_f32(0, mCol[1].mValue, 3),
				 vsetq_lane_f32(0, mCol[2].mValue, 3),
				 Vec4(0, 0, 0, 1));
#else
	return Mat44(Vec4(mCol[0].mF32[0], mCol[0].mF32[1], mCol[0].mF32[2], 0),
				 Vec4(mCol[1].mF32[0], mCol[1].mF32[1], mCol[1].mF32[2], 0),
				 Vec4(mCol[2].mF32[0], mCol[2].mF32[1], mCol[2].mF32[2], 0),
				 Vec4(0, 0, 0, 1));
#endif
}

void Mat44::SetRotation(Mat44Arg inRotation)
{
	mCol[0] = inRotation.mCol[0];
	mCol[1] = inRotation.mCol[1];
	mCol[2] = inRotation.mCol[2];
}

Mat44 Mat44::PreTranslated(Vec3Arg inTranslation) const
{
	return Mat44(mCol[0], mCol[1], mCol[2], Vec4(GetTranslation() + Multiply3x3(inTranslation), 1));
}

Mat44 Mat44::PostTranslated(Vec3Arg inTranslation) const
{
	return Mat44(mCol[0], mCol[1], mCol[2], Vec4(GetTranslation() + inTranslation, 1));
}

Mat44 Mat44::PreScaled(Vec3Arg inScale) const
{
	return Mat44(inScale.GetX() * mCol[0], inScale.GetY() * mCol[1], inScale.GetZ() * mCol[2], mCol[3]);
}

Mat44 Mat44::PostScaled(Vec3Arg inScale) const
{
	Vec4 scale(inScale, 1);
	return Mat44(scale * mCol[0], scale * mCol[1], scale * mCol[2], scale * mCol[3]);
}

Mat44 Mat44::Decompose(Vec3 &outScale) const
{
	// Start the modified Gram-Schmidt algorithm
	// X axis will just be normalized
	Vec3 x = GetAxisX();

	// Make Y axis perpendicular to X
	Vec3 y = GetAxisY();
	float x_dot_x = x.LengthSq();
	y -= (x.Dot(y) / x_dot_x) * x;

	// Make Z axis perpendicular to X
	Vec3 z = GetAxisZ();
	z -= (x.Dot(z) / x_dot_x) * x;

	// Make Z axis perpendicular to Y
	float y_dot_y = y.LengthSq();
	z -= (y.Dot(z) / y_dot_y) * y;

	// Determine the scale
	float z_dot_z = z.LengthSq();
	outScale = Vec3(x_dot_x, y_dot_y, z_dot_z).Sqrt();

	// If the resulting x, y and z vectors don't form a right handed matrix, flip the z axis.
	if (x.Cross(y).Dot(z) < 0.0f)
		outScale.SetZ(-outScale.GetZ());

	// Determine the rotation and translation
	return Mat44(Vec4(x / outScale.GetX(), 0), Vec4(y / outScale.GetY(), 0), Vec4(z / outScale.GetZ(), 0), GetColumn4(3));
}

#undef JPH_EL

JPH_NAMESPACE_END
