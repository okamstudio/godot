/**************************************************************************/
/*  test_reflection_probe.h                                               */
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

#ifndef TEST_REFLECTION_PROBE_H
#define TEST_REFLECTION_PROBE_H

#include "scene/3d/reflection_probe.h"
#include "tests/test_macros.h"

namespace TestReflectionProbe {

TEST_CASE("[SceneTree][ReflectionProbe] Check Defaults") {
	ReflectionProbe *probe = memnew(ReflectionProbe);

	CHECK(probe->get_intensity() == 1.0);
	CHECK(probe->get_ambient_mode() == ReflectionProbe::AMBIENT_ENVIRONMENT);
	CHECK(probe->get_ambient_color() == Color(0, 0, 0));
	CHECK(probe->get_ambient_color_energy() == 1.0);
	CHECK(probe->get_max_distance() == 0.0);
	CHECK(probe->get_mesh_lod_threshold() == 1.0);
	CHECK(probe->get_size() == Vector3(20, 20, 20));
	CHECK(probe->get_origin_offset() == Vector3(0, 0, 0));
	CHECK(probe->is_set_as_interior() == false);
	CHECK(probe->is_box_projection_enabled() == false);
	CHECK(probe->are_shadows_enabled() == false);
	CHECK(probe->get_cull_mask() == (1 << 20) - 1);
	CHECK(probe->get_reflection_mask() == (1 << 20) - 1);
	CHECK(probe->get_update_mode() == ReflectionProbe::UPDATE_ONCE);

	memdelete(probe);
}

TEST_CASE("[SceneTree][ReflectionProbe] Getters and setters then aabb") {
	ReflectionProbe *probe = memnew(ReflectionProbe);

	probe->set_intensity(3.0);
	CHECK(probe->get_intensity() == 3.0);

	probe->set_ambient_mode(ReflectionProbe::AMBIENT_COLOR);
	CHECK(probe->get_ambient_mode() == ReflectionProbe::AMBIENT_COLOR);

	probe->set_ambient_color(Color(1, 0, 0));
	CHECK(probe->get_ambient_color() == Color(1, 0, 0));

	probe->set_ambient_color_energy(2.0);
	CHECK(probe->get_ambient_color_energy() == 2.0);

	probe->set_max_distance(1.0);
	CHECK(probe->get_max_distance() == 1.0);

	probe->set_mesh_lod_threshold(2.0);
	CHECK(probe->get_mesh_lod_threshold() == 2.0);

	Vector3 new_size = Vector3(30, 30, 30);
	probe->set_size(new_size);
	CHECK(probe->get_size() == new_size);

	Vector3 new_origin_offset = Vector3(1, 0, 0);
	probe->set_origin_offset(new_origin_offset);
	CHECK(probe->get_origin_offset() == new_origin_offset);

	probe->set_as_interior(true);
	CHECK(probe->is_set_as_interior() == true);

	probe->set_enable_box_projection(true);
	CHECK(probe->is_box_projection_enabled() == true);

	probe->set_enable_shadows(true);
	CHECK(probe->are_shadows_enabled() == true);

	probe->set_cull_mask((1 << 19) - 1);
	CHECK(probe->get_cull_mask() == (1 << 19) - 1);

	probe->set_reflection_mask((1 << 21) - 1);
	CHECK(probe->get_reflection_mask() == (1 << 21) - 1);

	probe->set_update_mode(ReflectionProbe::UPDATE_ALWAYS);
	CHECK(probe->get_update_mode() == ReflectionProbe::UPDATE_ALWAYS);

	AABB aabb = probe->get_aabb();
	CHECK(aabb.position == -new_origin_offset);
	CHECK(aabb.size == new_origin_offset + new_size / 2);

	memdelete(probe);
}

} // namespace TestReflectionProbe

#endif // TEST_REFLECTION_PROBE_H
