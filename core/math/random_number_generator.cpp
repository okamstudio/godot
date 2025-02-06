/**************************************************************************/
/*  random_number_generator.cpp                                           */
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

#include "random_number_generator.h"

void RandomNumberGenerator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_seed", "seed"), &RandomNumberGenerator::set_seed);
	ClassDB::bind_method(D_METHOD("get_seed"), &RandomNumberGenerator::get_seed);

	ClassDB::bind_method(D_METHOD("set_state", "state"), &RandomNumberGenerator::set_state);
	ClassDB::bind_method(D_METHOD("get_state"), &RandomNumberGenerator::get_state);

	ClassDB::bind_method(D_METHOD("randi"), &RandomNumberGenerator::randi);
	ClassDB::bind_method(D_METHOD("rand_bool", "true_chance"), &RandomNumberGenerator::rand_bool, DEFVAL(0.5));
	ClassDB::bind_method(D_METHOD("randf"), &RandomNumberGenerator::randf);
	ClassDB::bind_method(D_METHOD("randfn", "mean", "deviation"), &RandomNumberGenerator::randfn, DEFVAL(0.0), DEFVAL(1.0));
	ClassDB::bind_method(D_METHOD("randf_range", "from", "to"), &RandomNumberGenerator::randf_range);
	ClassDB::bind_method(D_METHOD("randi_range", "from", "to"), &RandomNumberGenerator::randi_range);
	ClassDB::bind_method(D_METHOD("rand_weighted", "weights"), &RandomNumberGenerator::rand_weighted);
	ClassDB::bind_method(D_METHOD("randomize"), &RandomNumberGenerator::randomize);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "state"), "set_state", "get_state");
	// Default values are non-deterministic, override for doc generation purposes.
	ADD_PROPERTY_DEFAULT("seed", 0);
	ADD_PROPERTY_DEFAULT("state", 0);
}
