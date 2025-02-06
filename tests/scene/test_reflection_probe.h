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

TEST_CASE("[SceneTree][ReflectionProbe] Testing reflection probe - getters/setters and get AABB") {
	ReflectionProbe *reflection_probe = memnew(ReflectionProbe);

	SUBCASE("Default values") {
		CHECK(reflection_probe->get_intensity() == 1.0f);
		CHECK(reflection_probe->get_ambient_mode() == ReflectionProbe::AMBIENT_ENVIRONMENT);
		CHECK(reflection_probe->get_ambient_color() == Color(0, 0, 0));
		CHECK(reflection_probe->get_ambient_color_energy() == 1.0f);
		CHECK(reflection_probe->get_max_distance() == 0.0f);
		CHECK(reflection_probe->get_mesh_lod_threshold() == 1.0f);
		CHECK(reflection_probe->get_size() == Vector3(20, 20, 20));
		CHECK(reflection_probe->get_origin_offset() == Vector3(0, 0, 0));
		CHECK(reflection_probe->are_shadows_enabled() == false);
		CHECK(reflection_probe->is_set_as_interior() == false);
		CHECK(reflection_probe->get_reflection_mask() == uint32_t(1 << 20) - 1);
		CHECK(reflection_probe->get_update_mode() == ReflectionProbe::UPDATE_ONCE);
		CHECK(reflection_probe->get_aabb().position == Vector3(0.0f, 0.0f, 0.0f));
		CHECK(reflection_probe->get_aabb().size == Vector3(10.0f, 10.0f, 10.0f));
	}

	SUBCASE("Intensity") {
		reflection_probe->set_intensity(0.4f);
		CHECK(reflection_probe->get_intensity() == 0.4f);
	}

	SUBCASE("Ambient mode") {
		reflection_probe->set_ambient_mode(ReflectionProbe::AMBIENT_DISABLED);
		CHECK(reflection_probe->get_ambient_mode() == ReflectionProbe::AMBIENT_DISABLED);
		reflection_probe->set_ambient_mode(ReflectionProbe::AMBIENT_COLOR);
		CHECK(reflection_probe->get_ambient_mode() == ReflectionProbe::AMBIENT_COLOR);
		reflection_probe->set_ambient_mode(ReflectionProbe::AMBIENT_ENVIRONMENT);
		CHECK(reflection_probe->get_ambient_mode() == ReflectionProbe::AMBIENT_ENVIRONMENT);
	}

	SUBCASE("Ambient color energy") {
		reflection_probe->set_ambient_color_energy(0.3f);
		CHECK(reflection_probe->get_ambient_color_energy() == 0.3f);
	}

	SUBCASE("Max distance") {
		reflection_probe->set_max_distance(0.17f);
		CHECK(reflection_probe->get_max_distance() == 0.17f);
	}

	SUBCASE("Mesh lod threshold") {
		reflection_probe->set_mesh_lod_threshold(500.3f);
		CHECK(reflection_probe->get_mesh_lod_threshold() == 500.3f);
	}

	SUBCASE("Size") {
		reflection_probe->set_size(Vector3(1.3f, 1.33f, 1.337f));
		CHECK(reflection_probe->get_size() == Vector3(1.3f, 1.33f, 1.337f));
		CHECK(reflection_probe->get_aabb().size == Vector3(1.3f / 2.0f, 1.33f / 2.0f, 1.337f / 2.0f));
	}

	SUBCASE("Origin offset") {
		reflection_probe->set_origin_offset(Vector3(0.2f, 1.2f, 1.22f));
		CHECK(reflection_probe->get_origin_offset() == Vector3(0.2f, 1.2f, 1.22f));
	}

	SUBCASE("Set as interior") {
		reflection_probe->set_as_interior(true);
		CHECK(reflection_probe->is_set_as_interior() == true);
	}

	SUBCASE("Enable box projection") {
		reflection_probe->set_enable_box_projection(true);
		CHECK(reflection_probe->is_box_projection_enabled() == true);
	}

	SUBCASE("Enable shadows") {
		reflection_probe->set_enable_shadows(true);
		CHECK(reflection_probe->are_shadows_enabled() == true);
	}

	SUBCASE("Cull mask") {
		reflection_probe->set_cull_mask((1 << 18) - 1);
		CHECK(reflection_probe->get_cull_mask() == (1 << 18) - 1);
	}

	SUBCASE("Reflection mask") {
		reflection_probe->set_reflection_mask((1 << 18) - 1);
		CHECK(reflection_probe->get_reflection_mask() == (1 << 18) - 1);
	}

	SUBCASE("Update mode") {
		reflection_probe->set_update_mode(ReflectionProbe::UPDATE_ALWAYS);
		CHECK(reflection_probe->get_update_mode() == ReflectionProbe::UPDATE_ALWAYS);
	}

	memdelete(reflection_probe);
}

} // namespace TestReflectionProbe

#endif // TEST_REFLECTION_PROBE_H
