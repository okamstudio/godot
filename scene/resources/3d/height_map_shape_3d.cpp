/**************************************************************************/
/*  height_map_shape_3d.cpp                                               */
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

#include "height_map_shape_3d.h"

#include "core/io/image.h"
#include "scene/resources/mesh.h"
#include "servers/physics_server_3d.h"

Vector<Vector3> HeightMapShape3D::get_debug_mesh_lines() const {
	Vector<Vector3> points;

	if ((map_width != 0) && (map_depth != 0)) {
		// This will be slow for large maps...
		// also we'll have to figure out how well bullet centers this shape...

		Vector2 size(map_width - 1, map_depth - 1);
		Vector2 start = size * -0.5;

		const real_t *r = map_data.ptr();

		// reserve some memory for our points..
		points.resize(((map_width - 1) * map_depth * 2) + (map_width * (map_depth - 1) * 2) + ((map_width - 1) * (map_depth - 1) * 2));

		// now set our points
		int r_offset = 0;
		int w_offset = 0;
		for (int d = 0; d < map_depth; d++) {
			Vector3 height(start.x, 0.0, start.y);

			for (int w = 0; w < map_width; w++) {
				height.y = r[r_offset++];

				if (w != map_width - 1) {
					points.write[w_offset++] = height;
					points.write[w_offset++] = Vector3(height.x + 1.0, r[r_offset], height.z);
				}

				if (d != map_depth - 1) {
					points.write[w_offset++] = height;
					points.write[w_offset++] = Vector3(height.x, r[r_offset + map_width - 1], height.z + 1.0);
				}

				if ((w != map_width - 1) && (d != map_depth - 1)) {
					points.write[w_offset++] = Vector3(height.x + 1.0, r[r_offset], height.z);
					points.write[w_offset++] = Vector3(height.x, r[r_offset + map_width - 1], height.z + 1.0);
				}

				height.x += 1.0;
			}

			start.y += 1.0;
		}
	}

	return points;
}

Ref<ArrayMesh> HeightMapShape3D::get_debug_arraymesh_faces(const Color &p_modulate) const {
	Vector<Vector3> verts;
	Vector<Color> colors;
	Vector<int> indices;

	// This will be slow for large maps...

	if ((map_width != 0) && (map_depth != 0)) {
		Vector2 size = Vector2(map_width - 1, map_depth - 1) * -0.5;
		const real_t *r = map_data.ptr();

		for (int d = 0; d <= map_depth - 2; d++) {
			const int this_row_offset = map_width * d;
			const int next_row_offset = this_row_offset + map_width;

			for (int w = 0; w <= map_width - 2; w++) {
				const float height_tl = r[next_row_offset + w];
				const float height_bl = r[this_row_offset + w];
				const float height_br = r[this_row_offset + w + 1];
				const float height_tr = r[next_row_offset + w + 1];

				const int index_offset = verts.size();

				verts.push_back(Vector3(size.x + w, height_tl, size.y + d + 1));
				verts.push_back(Vector3(size.x + w, height_bl, size.y + d));
				verts.push_back(Vector3(size.x + w + 1, height_br, size.y + d));
				verts.push_back(Vector3(size.x + w + 1, height_tr, size.y + d + 1));

				colors.push_back(p_modulate);
				colors.push_back(p_modulate);
				colors.push_back(p_modulate);
				colors.push_back(p_modulate);

				indices.push_back(index_offset);
				indices.push_back(index_offset + 1);
				indices.push_back(index_offset + 2);
				indices.push_back(index_offset);
				indices.push_back(index_offset + 2);
				indices.push_back(index_offset + 3);
			}
		}
	}

	Ref<ArrayMesh> mesh = memnew(ArrayMesh);
	Array a;
	a.resize(Mesh::ARRAY_MAX);
	a[RS::ARRAY_VERTEX] = verts;
	a[RS::ARRAY_COLOR] = colors;
	a[RS::ARRAY_INDEX] = indices;
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, a);

	return mesh;
}

real_t HeightMapShape3D::get_enclosing_radius() const {
	return Vector3(real_t(map_width), max_height - min_height, real_t(map_depth)).length();
}

void HeightMapShape3D::_update_shape() {
	Dictionary d;
	d["width"] = map_width;
	d["depth"] = map_depth;
	d["heights"] = map_data;
	d["min_height"] = min_height;
	d["max_height"] = max_height;
	PhysicsServer3D::get_singleton()->shape_set_data(get_shape(), d);
	Shape3D::_update_shape();
}

void HeightMapShape3D::set_map_width(int p_new) {
	if (p_new < 1) {
		// ignore
	} else if (map_width != p_new) {
		int was_size = map_width * map_depth;
		map_width = p_new;

		int new_size = map_width * map_depth;
		map_data.resize(map_width * map_depth);

		real_t *w = map_data.ptrw();
		while (was_size < new_size) {
			w[was_size++] = 0.0;
		}

		_update_shape();
		emit_changed();
	}
}

int HeightMapShape3D::get_map_width() const {
	return map_width;
}

void HeightMapShape3D::set_map_depth(int p_new) {
	if (p_new < 1) {
		// ignore
	} else if (map_depth != p_new) {
		int was_size = map_width * map_depth;
		map_depth = p_new;

		int new_size = map_width * map_depth;
		map_data.resize(new_size);

		real_t *w = map_data.ptrw();
		while (was_size < new_size) {
			w[was_size++] = 0.0;
		}

		_update_shape();
		emit_changed();
	}
}

int HeightMapShape3D::get_map_depth() const {
	return map_depth;
}

void HeightMapShape3D::set_map_data(Vector<real_t> p_new) {
	int size = (map_width * map_depth);
	if (p_new.size() != size) {
		// fail
		return;
	}

	// copy
	real_t *w = map_data.ptrw();
	const real_t *r = p_new.ptr();
	for (int i = 0; i < size; i++) {
		real_t val = r[i];
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
	emit_changed();
}

Vector<real_t> HeightMapShape3D::get_map_data() const {
	return map_data;
}

real_t HeightMapShape3D::get_min_height() const {
	return min_height;
}

real_t HeightMapShape3D::get_max_height() const {
	return max_height;
}

void HeightMapShape3D::update_map_data_from_image(const Ref<Image> &p_image, real_t p_height_min, real_t p_height_max) {
	ERR_FAIL_COND_MSG(p_image.is_null(), "Heightmap update image requires a valid Image reference.");
	ERR_FAIL_COND_MSG(p_image->get_format() != Image::FORMAT_RF && p_image->get_format() != Image::FORMAT_RH && p_image->get_format() != Image::FORMAT_R8, "Heightmap update image requires Image in format FORMAT_RF (32 bit), FORMAT_RH (16 bit), or FORMAT_R8 (8 bit).");
	ERR_FAIL_COND_MSG(p_image->get_width() < 2, "Heightmap update image requires a minimum Image width of 2.");
	ERR_FAIL_COND_MSG(p_image->get_height() < 2, "Heightmap update image requires a minimum Image height of 2.");
	ERR_FAIL_COND_MSG(p_height_min > p_height_max, "Heightmap update image requires height_max to be greater than height_min.");

	map_width = p_image->get_width();
	map_depth = p_image->get_height();
	map_data.resize(map_width * map_depth);

	real_t new_min_height = FLT_MAX;
	real_t new_max_height = -FLT_MAX;

	float remap_height_min = float(p_height_min);
	float remap_height_max = float(p_height_max);

	real_t *map_data_ptrw = map_data.ptrw();

	switch (p_image->get_format()) {
		case Image::FORMAT_RF: {
			const float *image_data_ptr = (float *)p_image->get_data().ptr();

			for (int i = 0; i < map_data.size(); i++) {
				float pixel_value = image_data_ptr[i];

				DEV_ASSERT(pixel_value >= 0.0 && pixel_value <= 1.0);

				real_t height_value = Math::remap(pixel_value, 0.0f, 1.0f, remap_height_min, remap_height_max);

				if (height_value < new_min_height) {
					new_min_height = height_value;
				}
				if (height_value > new_max_height) {
					new_max_height = height_value;
				}

				map_data_ptrw[i] = height_value;
			}

		} break;

		case Image::FORMAT_RH: {
			const uint16_t *image_data_ptr = (uint16_t *)p_image->get_data().ptr();

			for (int i = 0; i < map_data.size(); i++) {
				float pixel_value = Math::half_to_float(image_data_ptr[i]);

				DEV_ASSERT(pixel_value >= 0.0 && pixel_value <= 1.0);

				real_t height_value = Math::remap(pixel_value, 0.0f, 1.0f, remap_height_min, remap_height_max);

				if (height_value < new_min_height) {
					new_min_height = height_value;
				}
				if (height_value > new_max_height) {
					new_max_height = height_value;
				}

				map_data_ptrw[i] = height_value;
			}

		} break;

		case Image::FORMAT_R8: {
			const uint8_t *image_data_ptr = (uint8_t *)p_image->get_data().ptr();

			for (int i = 0; i < map_data.size(); i++) {
				float pixel_value = float(image_data_ptr[i] / 255.0);

				DEV_ASSERT(pixel_value >= 0.0 && pixel_value <= 1.0);

				real_t height_value = Math::remap(pixel_value, 0.0f, 1.0f, remap_height_min, remap_height_max);

				if (height_value < new_min_height) {
					new_min_height = height_value;
				}
				if (height_value > new_max_height) {
					new_max_height = height_value;
				}

				map_data_ptrw[i] = height_value;
			}

		} break;

		default: {
			return;
		}
	}

	min_height = new_min_height;
	max_height = new_max_height;

	_update_shape();
	emit_changed();
}

void HeightMapShape3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_map_width", "width"), &HeightMapShape3D::set_map_width);
	ClassDB::bind_method(D_METHOD("get_map_width"), &HeightMapShape3D::get_map_width);
	ClassDB::bind_method(D_METHOD("set_map_depth", "height"), &HeightMapShape3D::set_map_depth);
	ClassDB::bind_method(D_METHOD("get_map_depth"), &HeightMapShape3D::get_map_depth);
	ClassDB::bind_method(D_METHOD("set_map_data", "data"), &HeightMapShape3D::set_map_data);
	ClassDB::bind_method(D_METHOD("get_map_data"), &HeightMapShape3D::get_map_data);
	ClassDB::bind_method(D_METHOD("get_min_height"), &HeightMapShape3D::get_min_height);
	ClassDB::bind_method(D_METHOD("get_max_height"), &HeightMapShape3D::get_max_height);

	ClassDB::bind_method(D_METHOD("update_map_data_from_image", "image", "height_min", "height_max"), &HeightMapShape3D::update_map_data_from_image);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "map_width", PROPERTY_HINT_RANGE, "1,100,1,or_greater"), "set_map_width", "get_map_width");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "map_depth", PROPERTY_HINT_RANGE, "1,100,1,or_greater"), "set_map_depth", "get_map_depth");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "map_data"), "set_map_data", "get_map_data");
}

HeightMapShape3D::HeightMapShape3D() :
		Shape3D(PhysicsServer3D::get_singleton()->shape_create(PhysicsServer3D::SHAPE_HEIGHTMAP)) {
	map_data.resize(map_width * map_depth);
	real_t *w = map_data.ptrw();
	w[0] = 0.0;
	w[1] = 0.0;
	w[2] = 0.0;
	w[3] = 0.0;

	_update_shape();
}
