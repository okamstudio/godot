// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// Whether or not to collect faces, used by CastShape and CollideShape
enum class ECollectFacesMode : uint8
{
	CollectFaces,										///< mShape1/2Face is desired
	NoFaces												///< mShape1/2Face is not desired
};

JPH_NAMESPACE_END
