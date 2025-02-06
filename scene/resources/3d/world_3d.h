/**************************************************************************/
/*  world_3d.h                                                            */
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
#include "scene/resources/compositor.h"
#include "scene/resources/environment.h"
#include "servers/physics_server_3d.h"
#include "servers/rendering_server.h"

class CameraAttributes;
class Camera3D;
class VisibleOnScreenNotifier3D;
struct SpatialIndexer;

class World3D : public Resource {
	GDCLASS(World3D, Resource);

private:
	RID scenario;
	mutable RID space;
	mutable RID navigation_map;

	Ref<Environment> environment;
	Ref<Environment> fallback_environment;
	Ref<CameraAttributes> camera_attributes;
	Ref<Compositor> compositor;

	HashSet<Camera3D *> cameras;

protected:
	static void _bind_methods();

	friend class Camera3D;

	void _register_camera(Camera3D *p_camera);
	void _remove_camera(Camera3D *p_camera);

public:
	RID get_space() const;
	RID get_navigation_map() const;
	RID get_scenario() const;

	void set_environment(const Ref<Environment> &p_environment);
	Ref<Environment> get_environment() const;

	void set_fallback_environment(const Ref<Environment> &p_environment);
	Ref<Environment> get_fallback_environment() const;

	void set_camera_attributes(const Ref<CameraAttributes> &p_camera_attributes);
	Ref<CameraAttributes> get_camera_attributes() const;

	void set_compositor(const Ref<Compositor> &p_compositor);
	Ref<Compositor> get_compositor() const;

	_FORCE_INLINE_ const HashSet<Camera3D *> &get_cameras() const { return cameras; }

	PhysicsDirectSpaceState3D *get_direct_space_state();

	World3D();
	~World3D();
};
