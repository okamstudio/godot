/**************************************************************************/
/*  csg_2d.cpp                                                            */
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

#include "csg_2d.h"

void CSGBrush2D::copy_from(const CSGBrush2D &p_brush, const Transform2D &p_xform) {
	outlines.resize(p_brush.outlines.size());

	for (uint32_t i = 0; i < outlines.size(); i++) {
		for (uint32_t j = 0; j < outlines[i].vertices.size(); j++) {
			outlines[i].vertices[j] = p_xform.xform(p_brush.outlines[i].vertices[j]);
		}
	}

	poly_paths.clear();

	for (Clipper2Lib::PathsD::size_type i = 0; i < p_brush.poly_paths.size(); ++i) {
		const Clipper2Lib::PathD &path = p_brush.poly_paths[i];

		Clipper2Lib::PathD poly_path;

		for (Clipper2Lib::PathsD::size_type j = 0; j < path.size(); ++j) {
			Point2 vertex = Point2(static_cast<real_t>(path[j].x), static_cast<real_t>(path[j].y));
			vertex = p_xform.xform(vertex);
			poly_path.push_back(Clipper2Lib::PointD(vertex.x, vertex.y));
		}
		poly_paths.push_back(poly_path);
	}

	_regen_outline_rects();
}

void CSGBrush2D::build_from_outlines(const LocalVector<Outline> &p_outlines) {
	poly_paths.clear();

	Clipper2Lib::ClipperD clipper_D;
	clipper_D.PreserveCollinear(false);

	for (const Outline &outline : p_outlines) {
		const LocalVector<Vector2> &vertices = outline.vertices;
		Clipper2Lib::PathD subject_path;
		subject_path.reserve(vertices.size());
		for (const Vector2 &vertex : vertices) {
			Clipper2Lib::PointD point = Clipper2Lib::PointD(vertex.x, vertex.y);
			subject_path.emplace_back(point);
		}

		Clipper2Lib::PathsD subject_paths;
		subject_paths.push_back(subject_path);

		if (outline.is_hole) {
			clipper_D.AddClip(subject_paths);
		} else {
			clipper_D.AddSubject(subject_paths);
		}
	}

	clipper_D.Execute(Clipper2Lib::ClipType::Union, Clipper2Lib::FillRule::NonZero, poly_paths);
}
