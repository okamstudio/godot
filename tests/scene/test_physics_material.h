/**************************************************************************/
/*  test_physics_material.h                                               */
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

#include "scene/resources/physics_material.h"
#include "tests/test_macros.h"

namespace TestPhysics_material {

TEST_CASE("[Physics_material] Defaults") {
	Ref<PhysicsMaterial> physics_material;
	physics_material.instantiate();

	CHECK(physics_material->get_friction() == 1.);
	CHECK(physics_material->is_rough() == false);
	CHECK(physics_material->get_bounce() == 0.);
	CHECK(physics_material->is_absorbent() == false);
}

TEST_CASE("[Physics_material] Friction") {
	Ref<PhysicsMaterial> physics_material;
	physics_material.instantiate();

	real_t friction = 0.314;
	physics_material->set_friction(friction);
	CHECK(physics_material->get_friction() == friction);
}

TEST_CASE("[Physics_material] Rough") {
	Ref<PhysicsMaterial> physics_material;
	physics_material.instantiate();

	bool rough = true;
	physics_material->set_rough(rough);
	CHECK(physics_material->is_rough() == rough);

	real_t friction = 0.314;
	physics_material->set_friction(friction);
	CHECK(physics_material->computed_friction() == -friction);

	rough = false;
	physics_material->set_rough(rough);
	CHECK(physics_material->is_rough() == rough);

	CHECK(physics_material->computed_friction() == friction);
}

TEST_CASE("[Physics_material] Bounce") {
	Ref<PhysicsMaterial> physics_material;
	physics_material.instantiate();

	real_t bounce = 0.271;
	physics_material->set_bounce(bounce);
	CHECK(physics_material->get_bounce() == bounce);
}

TEST_CASE("[Physics_material] Absorbent") {
	Ref<PhysicsMaterial> physics_material;
	physics_material.instantiate();

	bool absorbent = true;
	physics_material->set_absorbent(absorbent);
	CHECK(physics_material->is_absorbent() == absorbent);

	real_t bounce = 0.271;
	physics_material->set_bounce(bounce);
	CHECK(physics_material->computed_bounce() == -bounce);

	absorbent = false;
	physics_material->set_absorbent(absorbent);
	CHECK(physics_material->is_absorbent() == absorbent);

	CHECK(physics_material->computed_bounce() == bounce);
}

} // namespace TestPhysics_material
