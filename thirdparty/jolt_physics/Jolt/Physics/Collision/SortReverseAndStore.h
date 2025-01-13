// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// This function will sort values from high to low and only keep the ones that are less than inMaxValue
/// @param inValues Values to be sorted
/// @param inMaxValue Values need to be less than this to keep them
/// @param ioIdentifiers 4 identifiers that will be sorted in the same way as the values
/// @param outValues The values are stored here from high to low
/// @return The number of values that were kept
JPH_INLINE int SortReverseAndStore(Vec4Arg inValues, float inMaxValue, UVec4 &ioIdentifiers, float *outValues)
{
	// Sort so that highest values are first (we want to first process closer hits and we process stack top to bottom)
	Vec4 values = inValues;
	Vec4::sSort4Reverse(values, ioIdentifiers);

	// Count how many results are less than the max value
	UVec4 closer = Vec4::sLess(values, Vec4::sReplicate(inMaxValue));
	int num_results = closer.CountTrues();

	// Shift the values so that only the ones that are less than max are kept
	values = values.ReinterpretAsInt().ShiftComponents4Minus(num_results).ReinterpretAsFloat();
	ioIdentifiers = ioIdentifiers.ShiftComponents4Minus(num_results);

	// Store the values
	values.StoreFloat4(reinterpret_cast<Float4 *>(outValues));

	return num_results;
}

/// Shift the elements so that the identifiers that correspond with the trues in inValue come first
/// @param inValue Values to test for true or false
/// @param ioIdentifiers the identifiers that are shifted, on return they are shifted
/// @return The number of trues
JPH_INLINE int CountAndSortTrues(UVec4Arg inValue, UVec4 &ioIdentifiers)
{
	// Sort the hits
	ioIdentifiers = UVec4::sSort4True(inValue, ioIdentifiers);

	// Return the amount of hits
	return inValue.CountTrues();
}

JPH_NAMESPACE_END
