/**************************************************************************/
/*  world_2d.h                                                            */
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
#include "servers/physics_server_2d.h"

class VisibleOnScreenNotifier2D;
class Viewport;
struct SpatialIndexer2D;

class World2D : public Resource {
	GDCLASS(World2D, Resource);

	RID canvas;
	mutable RID space;
	mutable RID navigation_map;

	HashSet<Viewport *> viewports;

protected:
	static void _bind_methods();
	friend class Viewport;

public:
	RID get_canvas() const;
	RID get_space() const;
	RID get_navigation_map() const;

	PhysicsDirectSpaceState2D *get_direct_space_state();

	void register_viewport(Viewport *p_viewport);
	void remove_viewport(Viewport *p_viewport);

	_FORCE_INLINE_ const HashSet<Viewport *> &get_viewports() { return viewports; }

	World2D();
	~World2D();
};
