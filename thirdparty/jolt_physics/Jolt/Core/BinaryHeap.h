// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2024 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// Push a new element into a binary max-heap.
/// [inBegin, inEnd - 1) must be a a valid heap. Element inEnd - 1 will be inserted into the heap. The heap will be [inBegin, inEnd) after this call.
/// inPred is a function that returns true if the first element is less or equal than the second element.
/// See: https://en.wikipedia.org/wiki/Binary_heap
template <typename Iterator, typename Pred>
void BinaryHeapPush(Iterator inBegin, Iterator inEnd, Pred inPred)
{
	using diff_t = typename std::iterator_traits<Iterator>::difference_type;
	using elem_t = typename std::iterator_traits<Iterator>::value_type;

	// New heap size
	diff_t count = std::distance(inBegin, inEnd);

	// Start from the last element
	diff_t current = count - 1;
	while (current > 0)
	{
		// Get current element
		elem_t &current_elem = *(inBegin + current);

		// Get parent element
		diff_t parent = (current - 1) >> 1;
		elem_t &parent_elem = *(inBegin + parent);

		// Sort them so that the parent is larger than the child
		if (inPred(parent_elem, current_elem))
		{
			std::swap(parent_elem, current_elem);
			current = parent;
		}
		else
		{
			// When there's no change, we're done
			break;
		}
	}
}

/// Pop an element from a binary max-heap.
/// [inBegin, inEnd) must be a valid heap. The largest element will be removed from the heap. The heap will be [inBegin, inEnd - 1) after this call.
/// inPred is a function that returns true if the first element is less or equal than the second element.
/// See: https://en.wikipedia.org/wiki/Binary_heap
template <typename Iterator, typename Pred>
void BinaryHeapPop(Iterator inBegin, Iterator inEnd, Pred inPred)
{
	using diff_t = typename std::iterator_traits<Iterator>::difference_type;

	// Begin by moving the highest element to the end, this is the popped element
	std::swap(*(inEnd - 1), *inBegin);

	// New heap size
	diff_t count = std::distance(inBegin, inEnd) - 1;

	// Start from the root
	diff_t largest = 0;
	for (;;)
	{
		// Get first child
		diff_t child = (largest << 1) + 1;

		// Check if we're beyond the end of the heap, if so the 2nd child is also beyond the end
		if (child >= count)
			break;

		// Remember the largest element from the previous iteration
		diff_t prev_largest = largest;

		// Check if first child is bigger, if so select it
		if (inPred(*(inBegin + largest), *(inBegin + child)))
			largest = child;

		// Switch to the second child
		++child;

		// Check if second child is bigger, if so select it
		if (child < count && inPred(*(inBegin + largest), *(inBegin + child)))
			largest = child;

		// If there was no change, we're done
		if (prev_largest == largest)
			break;

		// Swap element
		std::swap(*(inBegin + prev_largest), *(inBegin + largest));
	}
}

JPH_NAMESPACE_END
