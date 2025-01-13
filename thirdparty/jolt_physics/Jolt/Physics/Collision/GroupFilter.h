// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/Result.h>
#include <Jolt/ObjectStream/SerializableObject.h>

JPH_NAMESPACE_BEGIN

class CollisionGroup;
class StreamIn;
class StreamOut;

/// Abstract class that checks if two CollisionGroups collide
class JPH_EXPORT GroupFilter : public SerializableObject, public RefTarget<GroupFilter>
{
	JPH_DECLARE_SERIALIZABLE_ABSTRACT(JPH_EXPORT, GroupFilter)

public:
	/// Virtual destructor
	virtual						~GroupFilter() override = default;

	/// Check if two groups collide
	virtual bool				CanCollide(const CollisionGroup &inGroup1, const CollisionGroup &inGroup2) const = 0;

	/// Saves the contents of the group filter in binary form to inStream.
	virtual void				SaveBinaryState(StreamOut &inStream) const;

	using GroupFilterResult = Result<Ref<GroupFilter>>;

	/// Creates a GroupFilter of the correct type and restores its contents from the binary stream inStream.
	static GroupFilterResult	sRestoreFromBinaryState(StreamIn &inStream);

protected:
	/// This function should not be called directly, it is used by sRestoreFromBinaryState.
	virtual void				RestoreBinaryState(StreamIn &inStream);
};

JPH_NAMESPACE_END
