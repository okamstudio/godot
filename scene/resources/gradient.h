/**************************************************************************/
/*  gradient.h                                                            */
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

#pragma once

#include "core/io/resource.h"

#include "thirdparty/misc/ok_color.h"

class Gradient : public Resource {
	GDCLASS(Gradient, Resource);
	OBJ_SAVE_TYPE(Gradient);

public:
	enum InterpolationMode {
		GRADIENT_INTERPOLATE_LINEAR,
		GRADIENT_INTERPOLATE_CONSTANT,
		GRADIENT_INTERPOLATE_CUBIC,
	};

	enum ColorSpace {
		GRADIENT_COLOR_SPACE_SRGB,
		GRADIENT_COLOR_SPACE_LINEAR_SRGB,
		GRADIENT_COLOR_SPACE_OKLAB,
	};

	struct Point {
		float offset = 0.0;
		Color color;
		bool operator<(const Point &p_point) const {
			return offset < p_point.offset;
		}
	};

private:
	LocalVector<Point> points;
	bool is_sorted = true;
	InterpolationMode interpolation_mode = GRADIENT_INTERPOLATE_LINEAR;
	ColorSpace interpolation_color_space = GRADIENT_COLOR_SPACE_SRGB;

	_FORCE_INLINE_ void _update_sorting() {
		if (!is_sorted) {
			points.sort();
			is_sorted = true;
		}
	}

	_FORCE_INLINE_ Color transform_color_space(const Color p_color) const {
		switch (interpolation_color_space) {
			case GRADIENT_COLOR_SPACE_SRGB:
			default:
				return p_color;

			case GRADIENT_COLOR_SPACE_LINEAR_SRGB:
				return p_color.srgb_to_linear();

			case GRADIENT_COLOR_SPACE_OKLAB:
				Color linear_color = p_color.srgb_to_linear();
				ok_color::RGB rgb{};
				rgb.r = linear_color.r;
				rgb.g = linear_color.g;
				rgb.b = linear_color.b;

				ok_color ok_color;
				ok_color::Lab lab_color = ok_color.linear_srgb_to_oklab(rgb);

				// Constructs an RGB color using the Lab values directly. This allows reusing the interpolation code.
				return { lab_color.L, lab_color.a, lab_color.b, linear_color.a };
		}
	}

	_FORCE_INLINE_ Color inv_transform_color_space(const Color p_color) const {
		switch (interpolation_color_space) {
			case GRADIENT_COLOR_SPACE_SRGB:
			default:
				return p_color;

			case GRADIENT_COLOR_SPACE_LINEAR_SRGB:
				return p_color.linear_to_srgb();

			case GRADIENT_COLOR_SPACE_OKLAB:
				ok_color::Lab lab{};
				lab.L = p_color.r;
				lab.a = p_color.g;
				lab.b = p_color.b;

				ok_color new_ok_color;
				ok_color::RGB ok_rgb = new_ok_color.oklab_to_linear_srgb(lab);
				Color linear{ ok_rgb.r, ok_rgb.g, ok_rgb.b, p_color.a };
				return linear.linear_to_srgb();
		}
	}

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	Gradient();
	virtual ~Gradient();

	void add_point(float p_offset, const Color &p_color);
	void remove_point(int p_index);
	void reverse();

	void set_offset(int pos, const float offset);
	float get_offset(int pos);

	void set_color(int pos, const Color &color);
	Color get_color(int pos);

	void set_offsets(const Vector<float> &p_offsets);
	Vector<float> get_offsets() const;

	void set_colors(const Vector<Color> &p_colors);
	Vector<Color> get_colors() const;

	void set_interpolation_mode(InterpolationMode p_interp_mode);
	InterpolationMode get_interpolation_mode();

	void set_interpolation_color_space(Gradient::ColorSpace p_color_space);
	ColorSpace get_interpolation_color_space();

	_FORCE_INLINE_ Color get_color_at_offset(float p_offset) {
		if (points.is_empty()) {
			return Color(0, 0, 0, 1);
		}

		_update_sorting();

		// Binary search.
		int low = 0;
		int high = points.size() - 1;
		int middle = 0;

#ifdef DEBUG_ENABLED
		if (low > high) {
			ERR_PRINT("low > high, this may be a bug");
		}
#endif

		while (low <= high) {
			middle = (low + high) / 2;
			const Point &point = points[middle];
			if (point.offset > p_offset) {
				high = middle - 1; //search low end of array
			} else if (point.offset < p_offset) {
				low = middle + 1; //search high end of array
			} else {
				return point.color;
			}
		}

		// Return sampled value.
		if (points[middle].offset > p_offset) {
			middle--;
		}
		int first = middle;
		int second = middle + 1;
		if (second >= (int)points.size()) {
			return points[points.size() - 1].color;
		}
		if (first < 0) {
			return points[0].color;
		}
		const Point &point1 = points[first];
		const Point &point2 = points[second];
		float weight = (p_offset - point1.offset) / (point2.offset - point1.offset);

		switch (interpolation_mode) {
			case GRADIENT_INTERPOLATE_CONSTANT: {
				return point1.color;
			}
			case GRADIENT_INTERPOLATE_LINEAR:
			default: { // Fallback to linear interpolation.
				Color color1 = transform_color_space(point1.color);
				Color color2 = transform_color_space(point2.color);

				Color interpolated = color1.lerp(color2, weight);
				return inv_transform_color_space(interpolated);
			}
			case GRADIENT_INTERPOLATE_CUBIC: {
				int p0 = first - 1;
				int p3 = second + 1;
				if (p3 >= (int)points.size()) {
					p3 = second;
				}
				if (p0 < 0) {
					p0 = first;
				}
				const Point &point0 = points[p0];
				const Point &point3 = points[p3];

				Color color0 = transform_color_space(point0.color);
				Color color1 = transform_color_space(point1.color);
				Color color2 = transform_color_space(point2.color);
				Color color3 = transform_color_space(point3.color);

				Color interpolated;
				interpolated[0] = Math::cubic_interpolate(color1[0], color2[0], color0[0], color3[0], weight);
				interpolated[1] = Math::cubic_interpolate(color1[1], color2[1], color0[1], color3[1], weight);
				interpolated[2] = Math::cubic_interpolate(color1[2], color2[2], color0[2], color3[2], weight);
				interpolated[3] = Math::cubic_interpolate(color1[3], color2[3], color0[3], color3[3], weight);

				return inv_transform_color_space(interpolated);
			}
		}
	}

	int get_point_count() const;
};

VARIANT_ENUM_CAST(Gradient::InterpolationMode);
VARIANT_ENUM_CAST(Gradient::ColorSpace);
