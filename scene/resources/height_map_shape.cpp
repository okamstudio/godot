/**************************************************************************/
/*  height_map_shape.cpp                                                  */
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

#include "height_map_shape.h"

#include "servers/physics_server.h"

Vector<Vector3> HeightMapShape::get_debug_mesh_lines() {
	Vector<Vector3> points;
	Vector3 s(grid_scale.x, 1.0, grid_scale.y);

	if ((map_width != 0) && (map_depth != 0)) {
		// This will be slow for large maps...
		// also we'll have to figure out how well bullet centers this shape...

		Vector2 size(map_width - 1, map_depth - 1);
		Vector2 start = size * -0.5;

		PoolRealArray::Read r = map_data.read();

		// reserve some memory for our points..
		points.resize(((map_width - 1) * map_depth * 2) + (map_width * (map_depth - 1) * 2) + (map_width - 1) * (map_depth - 1) * 2);

		// now set our points
		int r_offset = 0;
		int w_offset = 0;
		for (int d = 0; d < map_depth; d++) {
			Vector3 height(start.x, 0.0, start.y);

			for (int w = 0; w < map_width; w++) {
				height.y = r[r_offset++];

				if (w != map_width - 1) {
					points.write[w_offset++] = height * s;
					points.write[w_offset++] = Vector3(height.x + 1.0, r[r_offset], height.z) * s;
				}

				if (d != map_depth - 1) {
					points.write[w_offset++] = height * s;
					points.write[w_offset++] = Vector3(height.x, r[r_offset + map_width - 1], height.z + 1.0) * s;
				}

				if ((w != map_width - 1) && (d != map_depth - 1)) {
					points.write[w_offset++] = Vector3(height.x + 1.0, r[r_offset], height.z) * s;
					points.write[w_offset++] = Vector3(height.x, r[r_offset + map_width - 1], height.z + 1.0) * s;
				}

				height.x += 1.0;
			}

			start.y += 1.0;
		}
	}

	return points;
}

real_t HeightMapShape::get_enclosing_radius() const {
	return Vector3(real_t(map_width) * grid_scale.x, max_height - min_height, real_t(map_depth) * grid_scale.y).length();
}

void HeightMapShape::_update_shape() {
	Dictionary d;
	d["width"] = map_width;
	d["depth"] = map_depth;
	d["heights"] = map_data;
	d["min_height"] = min_height;
	d["max_height"] = max_height;
	d["grid_scale"] = grid_scale;
	PhysicsServer::get_singleton()->shape_set_data(get_shape(), d);
	Shape::_update_shape();
}

void HeightMapShape::set_map_width(int p_new) {
	if (p_new < 1) {
		// ignore
	} else if (map_width != p_new) {
		int was_size = map_width * map_depth;
		map_width = p_new;

		int new_size = map_width * map_depth;
		map_data.resize(map_width * map_depth);

		PoolRealArray::Write w = map_data.write();
		while (was_size < new_size) {
			w[was_size++] = 0.0;
		}

		_update_shape();
		notify_change_to_owners();
		_change_notify("map_width");
		_change_notify("map_data");
	}
}

int HeightMapShape::get_map_width() const {
	return map_width;
}

void HeightMapShape::set_map_depth(int p_new) {
	if (p_new < 1) {
		// ignore
	} else if (map_depth != p_new) {
		int was_size = map_width * map_depth;
		map_depth = p_new;

		int new_size = map_width * map_depth;
		map_data.resize(new_size);

		PoolRealArray::Write w = map_data.write();
		while (was_size < new_size) {
			w[was_size++] = 0.0;
		}

		_update_shape();
		notify_change_to_owners();
		_change_notify("map_depth");
		_change_notify("map_data");
	}
}

int HeightMapShape::get_map_depth() const {
	return map_depth;
}

void HeightMapShape::set_map_data(PoolRealArray p_new) {
	int size = (map_width * map_depth);
	if (p_new.size() != size) {
		// fail
		return;
	}

	// copy
	PoolRealArray::Write w = map_data.write();
	PoolRealArray::Read r = p_new.read();
	for (int i = 0; i < size; i++) {
		float val = r[i];
		w[i] = val;
		if (i == 0) {
			min_height = val;
			max_height = val;
		} else {
			if (min_height > val) {
				min_height = val;
			}

			if (max_height < val) {
				max_height = val;
			}
		}
	}

	_update_shape();
	notify_change_to_owners();
	_change_notify("map_data");
}

PoolRealArray HeightMapShape::get_map_data() const {
	return map_data;
}

void HeightMapShape::set_grid_scale(Vector2 p_new) {
	if (grid_scale != p_new) {
		grid_scale = p_new;

		_update_shape();
		notify_change_to_owners();
		_change_notify("grid_scale");
	}
}

Vector2 HeightMapShape::get_grid_scale() const {
	return grid_scale;
}

Vector3 HeightMapShape::get_grid_point(int p_ix, int p_iy) const {
	// Assert invalid point indexes
	ERR_FAIL_COND_V(p_ix < 0 || p_ix >= map_width, Vector3());
	ERR_FAIL_COND_V(p_iy < 0 || p_iy >= map_depth, Vector3());

	PoolRealArray::Read r = map_data.read();

	return Vector3(
			(p_ix - (map_width - 1) * 0.5) * grid_scale.x,
			r[p_ix + p_iy * map_width],
			(p_iy - (map_depth - 1) * 0.5) * grid_scale.y);
}

void HeightMapShape::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_map_width", "width"), &HeightMapShape::set_map_width);
	ClassDB::bind_method(D_METHOD("get_map_width"), &HeightMapShape::get_map_width);
	ClassDB::bind_method(D_METHOD("set_map_depth", "height"), &HeightMapShape::set_map_depth);
	ClassDB::bind_method(D_METHOD("get_map_depth"), &HeightMapShape::get_map_depth);
	ClassDB::bind_method(D_METHOD("set_map_data", "data"), &HeightMapShape::set_map_data);
	ClassDB::bind_method(D_METHOD("get_map_data"), &HeightMapShape::get_map_data);
	ClassDB::bind_method(D_METHOD("set_grid_scale", "grid_scale"), &HeightMapShape::set_grid_scale);
	ClassDB::bind_method(D_METHOD("get_grid_scale"), &HeightMapShape::get_grid_scale);
	ClassDB::bind_method(D_METHOD("get_grid_point", "x", "y"), &HeightMapShape::get_grid_point);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "map_width", PROPERTY_HINT_RANGE, "0.001,100,0.001,or_greater"), "set_map_width", "get_map_width");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "map_depth", PROPERTY_HINT_RANGE, "0.001,100,0.001,or_greater"), "set_map_depth", "get_map_depth");
	ADD_PROPERTY(PropertyInfo(Variant::POOL_REAL_ARRAY, "map_data"), "set_map_data", "get_map_data");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "grid_scale"), "set_grid_scale", "get_grid_scale");
}

HeightMapShape::HeightMapShape() :
		Shape(RID_PRIME(PhysicsServer::get_singleton()->shape_create(PhysicsServer::SHAPE_HEIGHTMAP))) {
	map_width = 2;
	map_depth = 2;
	map_data.resize(map_width * map_depth);
	PoolRealArray::Write w = map_data.write();
	w[0] = 0.0;
	w[1] = 0.0;
	w[2] = 0.0;
	w[3] = 0.0;
	grid_scale = Vector2(1.0, 1.0);
	min_height = 0.0;
	max_height = 0.0;

	_update_shape();
}
