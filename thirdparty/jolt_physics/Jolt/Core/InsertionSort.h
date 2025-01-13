// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2022 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// Implementation of the insertion sort algorithm.
template <typename Iterator, typename Compare>
inline void InsertionSort(Iterator inBegin, Iterator inEnd, Compare inCompare)
{
	// Empty arrays don't need to be sorted
	if (inBegin != inEnd)
	{
		// Start at the second element
		for (Iterator i = inBegin + 1; i != inEnd; ++i)
		{
			// Move this element to a temporary value
			auto x = std::move(*i);

			// Check if the element goes before inBegin (we can't decrement the iterator before inBegin so this needs to be a separate branch)
			if (inCompare(x, *inBegin))
			{
				// Move all elements to the right to make space for x
				Iterator prev;
				for (Iterator j = i; j != inBegin; j = prev)
				{
					prev = j - 1;
					*j = *prev;
				}

				// Move x to the first place
				*inBegin = std::move(x);
			}
			else
			{
				// Move elements to the right as long as they are bigger than x
				Iterator j = i;
				for (Iterator prev = j - 1; inCompare(x, *prev); j = prev, --prev)
					*j = std::move(*prev);

				// Move x into place
				*j = std::move(x);
			}
		}
	}
}

/// Implementation of insertion sort algorithm without comparator.
template <typename Iterator>
inline void InsertionSort(Iterator inBegin, Iterator inEnd)
{
	std::less<> compare;
	InsertionSort(inBegin, inEnd, compare);
}

JPH_NAMESPACE_END
