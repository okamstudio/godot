/**************************************************************************/
/*  height_map_shape.h                                                    */
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

#ifndef HEIGHT_MAP_SHAPE_H
#define HEIGHT_MAP_SHAPE_H

#include "scene/resources/shape.h"

class HeightMapShape : public Shape {
	GDCLASS(HeightMapShape, Shape);

	int map_width;
	int map_depth;
	PoolRealArray map_data;
	Vector2 grid_scale;
	float min_height;
	float max_height;

protected:
	static void _bind_methods();
	virtual void _update_shape();

public:
	void set_map_width(int p_new);
	int get_map_width() const;
	void set_map_depth(int p_new);
	int get_map_depth() const;
	void set_map_data(PoolRealArray p_new);
	PoolRealArray get_map_data() const;
	void set_grid_scale(Vector2 p_new);
	Vector2 get_grid_scale() const;

	Vector3 get_grid_point(int p_ix, int p_iy) const;

	virtual Vector<Vector3> get_debug_mesh_lines();
	virtual real_t get_enclosing_radius() const;

	HeightMapShape();
};

#endif // HEIGHT_MAP_SHAPE_H
