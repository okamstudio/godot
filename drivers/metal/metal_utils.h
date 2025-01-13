/**************************************************************************/
/*  metal_utils.h                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#ifndef METAL_UTILS_H
#define METAL_UTILS_H

#import <os/log.h>

#pragma mark - Boolean flags

namespace flags {

/*! Sets the flags within the value parameter specified by the mask parameter. */
template <typename Tv, typename Tm>
void set(Tv &p_value, Tm p_mask) {
	using T = std::underlying_type_t<Tv>;
	p_value = static_cast<Tv>(static_cast<T>(p_value) | static_cast<T>(p_mask));
}

/*! Clears the flags within the value parameter specified by the mask parameter. */
template <typename Tv, typename Tm>
void clear(Tv &p_value, Tm p_mask) {
	using T = std::underlying_type_t<Tv>;
	p_value = static_cast<Tv>(static_cast<T>(p_value) & ~static_cast<T>(p_mask));
}

/*! Returns whether the specified value has any of the bits specified in mask set to 1. */
template <typename Tv, typename Tm>
static constexpr bool any(Tv p_value, const Tm p_mask) {
	return ((p_value & p_mask) != 0);
}

/*! Returns whether the specified value has all of the bits specified in mask set to 1. */
template <typename Tv, typename Tm>
static constexpr bool all(Tv p_value, const Tm p_mask) {
	return ((p_value & p_mask) == p_mask);
}

} //namespace flags

#pragma mark - Alignment and Offsets

static constexpr bool is_power_of_two(uint64_t p_value) {
	return p_value && ((p_value & (p_value - 1)) == 0);
}

static constexpr uint64_t round_up_to_alignment(uint64_t p_value, uint64_t p_alignment) {
	DEV_ASSERT(is_power_of_two(p_alignment));

	if (p_alignment == 0) {
		return p_value;
	}

	uint64_t mask = p_alignment - 1;
	uint64_t aligned_value = (p_value + mask) & ~mask;

	return aligned_value;
}

class Defer {
public:
	Defer(std::function<void()> func) :
			func_(func) {}
	~Defer() { func_(); }

private:
	std::function<void()> func_;
};

#define CONCAT_INTERNAL(x, y) x##y
#define CONCAT(x, y) CONCAT_INTERNAL(x, y)
#define DEFER const Defer &CONCAT(defer__, __LINE__) = Defer

extern os_log_t LOG_DRIVER;
// Used for dynamic tracing.
extern os_log_t LOG_INTERVALS;

#endif // METAL_UTILS_H
