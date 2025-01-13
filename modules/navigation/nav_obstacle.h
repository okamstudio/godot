/**************************************************************************/
/*  nav_obstacle.h                                                        */
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

#ifndef NAV_OBSTACLE_H
#define NAV_OBSTACLE_H

#include "nav_rid.h"

#include "core/object/class_db.h"
#include "core/templates/local_vector.h"
#include "core/templates/self_list.h"

class NavAgent;
class NavMap;

class NavObstacle : public NavRid {
	NavAgent *agent = nullptr;
	NavMap *map = nullptr;
	Vector3 velocity;
	Vector3 position;
	Vector<Vector3> vertices;

	real_t radius = 0.0;
	real_t height = 0.0;

	bool avoidance_enabled = false;
	bool use_3d_avoidance = false;
	uint32_t avoidance_layers = 1;

	bool obstacle_dirty = true;

	uint32_t last_map_iteration_id = 0;
	bool paused = false;

	SelfList<NavObstacle> sync_dirty_request_list_element;

public:
	NavObstacle();
	~NavObstacle();

	void set_avoidance_enabled(bool p_enabled);
	bool is_avoidance_enabled() { return avoidance_enabled; }

	void set_use_3d_avoidance(bool p_enabled);
	bool get_use_3d_avoidance() { return use_3d_avoidance; }

	void set_map(NavMap *p_map);
	NavMap *get_map() { return map; }

	void set_agent(NavAgent *p_agent);
	NavAgent *get_agent() { return agent; }

	void set_position(const Vector3 p_position);
	const Vector3 &get_position() const { return position; }

	void set_radius(real_t p_radius);
	real_t get_radius() const { return radius; }

	void set_height(const real_t p_height);
	real_t get_height() const { return height; }

	void set_velocity(const Vector3 p_velocity);
	const Vector3 &get_velocity() const { return velocity; }

	void set_vertices(const Vector<Vector3> &p_vertices);
	const Vector<Vector3> &get_vertices() const { return vertices; }

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

#endif // NAV_OBSTACLE_H
