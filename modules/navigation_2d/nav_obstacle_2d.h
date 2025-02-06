/**************************************************************************/
/*  nav_obstacle_2d.h                                                     */
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

#ifndef NAV_OBSTACLE_2D_H
#define NAV_OBSTACLE_2D_H

#include "nav_rid_2d.h"

#include "core/object/class_db.h"
#include "core/templates/self_list.h"

class NavAgent2D;
class NavMap2D;

class NavObstacle2D : public NavRid2D {
	NavAgent2D *agent = nullptr;
	NavMap2D *map = nullptr;
	Vector2 velocity;
	Vector2 position;
	Vector<Vector2> vertices;

	real_t radius = 0.0;

	bool avoidance_enabled = false;
	uint32_t avoidance_layers = 1;

	bool obstacle_dirty = true;

	uint32_t last_map_iteration_id = 0;
	bool paused = false;

	SelfList<NavObstacle2D> sync_dirty_request_list_element;

public:
	NavObstacle2D();
	~NavObstacle2D();

	void set_avoidance_enabled(bool p_enabled);
	bool is_avoidance_enabled() { return avoidance_enabled; }

	void set_map(NavMap2D *p_map);
	NavMap2D *get_map() { return map; }

	void set_agent(NavAgent2D *p_agent);
	NavAgent2D *get_agent() { return agent; }

	void set_position(const Vector2 &p_position);
	Vector2 get_position() const { return position; }

	void set_radius(real_t p_radius);
	real_t get_radius() const { return radius; }

	void set_velocity(const Vector2 &p_velocity);
	Vector2 get_velocity() const { return velocity; }

	void set_vertices(const Vector<Vector2> &p_vertices);
	const Vector<Vector2> &get_vertices() const { return vertices; }

	bool is_map_changed();

	void set_avoidance_layers(uint32_t p_layers);
	uint32_t get_avoidance_layers() const { return avoidance_layers; }

	void set_paused(bool p_paused);
	bool get_paused() const;

	bool is_dirty() const;
	void sync();
	void request_sync();
	void cancel_sync_request();

private:
	void internal_update_agent();
};

#endif // NAV_OBSTACLE_2D_H
