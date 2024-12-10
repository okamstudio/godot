/**************************************************************************/
/*  csg_2d.h                                                              */
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

#ifndef CSG_2D_H
#define CSG_2D_H

#include "core/math/rect2.h"
#include "core/math/transform_2d.h"
#include "core/templates/local_vector.h"

#include "thirdparty/clipper2/include/clipper2/clipper.h"

struct CSGBrush2D {
	struct Outline {
		LocalVector<Vector2> vertices;
		bool is_hole = false;
		Rect2 rect;
	};

	LocalVector<Outline> outlines;

	inline void _regen_outline_rects() {
		for (uint32_t i = 0; i < outlines.size(); i++) {
			outlines[i].rect = Rect2();
			if (outlines[i].vertices.size() > 0) {
				outlines[i].rect.position = outlines[i].vertices[0];
				for (uint32_t j = 1; j < outlines[i].vertices.size(); j++) {
					outlines[i].rect.expand_to(outlines[i].vertices[j]);
				}
			}
		}
	}

	Clipper2Lib::PathsD poly_paths;

	void copy_from(const CSGBrush2D &p_brush, const Transform2D &p_xform);

	void build_from_outlines(const LocalVector<Outline> &p_outlines);
};

#endif // CSG_2D_H
