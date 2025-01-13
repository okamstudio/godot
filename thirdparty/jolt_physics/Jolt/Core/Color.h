// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

class Color;

/// Type to use for passing arguments to a function
using ColorArg = Color;

/// Class that holds an RGBA color with 8-bits per component
class [[nodiscard]] JPH_EXPORT_GCC_BUG_WORKAROUND Color
{
public:
	/// Constructors
							Color() = default; ///< Intentionally not initialized for performance reasons
							Color(const Color &inRHS) = default;
	Color &					operator = (const Color &inRHS) = default;
	explicit constexpr		Color(uint32 inColor)													: mU32(inColor) { }
	constexpr				Color(uint8 inRed, uint8 inGreen, uint8 inBlue, uint8 inAlpha = 255)	: r(inRed), g(inGreen), b(inBlue), a(inAlpha) { }
	constexpr				Color(ColorArg inRHS, uint8 inAlpha)									: r(inRHS.r), g(inRHS.g), b(inRHS.b), a(inAlpha) { }

	/// Comparison
	inline bool				operator == (ColorArg inRHS) const										{ return mU32 == inRHS.mU32; }
	inline bool				operator != (ColorArg inRHS) const										{ return mU32 != inRHS.mU32; }

	/// Convert to uint32
	uint32					GetUInt32() const														{ return mU32; }

	/// Element access, 0 = red, 1 = green, 2 = blue, 3 = alpha
	inline uint8			operator () (uint inIdx) const											{ JPH_ASSERT(inIdx < 4); return (&r)[inIdx]; }
	inline uint8 &			operator () (uint inIdx)												{ JPH_ASSERT(inIdx < 4); return (&r)[inIdx]; }

	/// Multiply two colors
	inline Color			operator * (const Color &inRHS) const									{ return Color(uint8((uint32(r) * inRHS.r) >> 8), uint8((uint32(g) * inRHS.g) >> 8), uint8((uint32(b) * inRHS.b) >> 8), uint8((uint32(a) * inRHS.a) >> 8)); }

	/// Multiply color with intensity in the range [0, 1]
	inline Color			operator * (float inIntensity) const									{ return Color(uint8(r * inIntensity), uint8(g * inIntensity), uint8(b * inIntensity), a); }

	/// Convert to Vec4 with range [0, 1]
	inline Vec4				ToVec4() const															{ return Vec4(r, g, b, a) / 255.0f; }

	/// Get grayscale intensity of color
	inline uint8			GetIntensity() const													{ return uint8((uint32(r) * 54 + g * 183 + b * 19) >> 8); }

	/// Get a visually distinct color
	static Color			sGetDistinctColor(int inIndex);

	/// Predefined colors
	static const Color		sBlack;
	static const Color		sDarkRed;
	static const Color		sRed;
	static const Color		sDarkGreen;
	static const Color		sGreen;
	static const Color		sDarkBlue;
	static const Color		sBlue;
	static const Color		sYellow;
	static const Color		sPurple;
	static const Color		sCyan;
	static const Color		sOrange;
	static const Color		sDarkOrange;
	static const Color		sGrey;
	static const Color		sLightGrey;
	static const Color		sWhite;

	union
	{
		uint32				mU32;																	///< Combined value for red, green, blue and alpha
		struct
		{
			uint8			r;																		///< Red channel
			uint8			g;																		///< Green channel
			uint8			b;																		///< Blue channel
			uint8			a;																		///< Alpha channel
		};
	};
};

static_assert(std::is_trivial<Color>(), "Is supposed to be a trivial type!");

JPH_NAMESPACE_END
