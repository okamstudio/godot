/**************************************************************************/
/*  navigation_path_query_parameters_2d.cpp                               */
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

#include "navigation_path_query_parameters_2d.h"

void NavigationPathQueryParameters2D::set_pathfinding_algorithm(const NavigationPathQueryParameters2D::PathfindingAlgorithm p_pathfinding_algorithm) {
	pathfinding_algorithm = p_pathfinding_algorithm;
}

NavigationPathQueryParameters2D::PathfindingAlgorithm NavigationPathQueryParameters2D::get_pathfinding_algorithm() const {
	return pathfinding_algorithm;
}

void NavigationPathQueryParameters2D::set_path_postprocessing(const NavigationPathQueryParameters2D::PathPostProcessing p_path_postprocessing) {
	path_postprocessing = p_path_postprocessing;
}

NavigationPathQueryParameters2D::PathPostProcessing NavigationPathQueryParameters2D::get_path_postprocessing() const {
	return path_postprocessing;
}

void NavigationPathQueryParameters2D::set_map(RID p_map) {
	map = p_map;
}

RID NavigationPathQueryParameters2D::get_map() const {
	return map;
}

void NavigationPathQueryParameters2D::set_start_position(Vector2 p_start_position) {
	start_position = p_start_position;
}

Vector2 NavigationPathQueryParameters2D::get_start_position() const {
	return start_position;
}

void NavigationPathQueryParameters2D::set_target_position(Vector2 p_target_position) {
	target_position = p_target_position;
}

Vector2 NavigationPathQueryParameters2D::get_target_position() const {
	return target_position;
}

void NavigationPathQueryParameters2D::set_navigation_layers(uint32_t p_navigation_layers) {
	navigation_layers = p_navigation_layers;
}

uint32_t NavigationPathQueryParameters2D::get_navigation_layers() const {
	return navigation_layers;
}

void NavigationPathQueryParameters2D::set_metadata_flags(BitField<NavigationPathQueryParameters2D::PathMetadataFlags> p_flags) {
	metadata_flags = (int64_t)p_flags;
}

BitField<NavigationPathQueryParameters2D::PathMetadataFlags> NavigationPathQueryParameters2D::get_metadata_flags() const {
	return (int64_t)metadata_flags;
}

void NavigationPathQueryParameters2D::set_simplify_path(bool p_enabled) {
	simplify_path = p_enabled;
}

bool NavigationPathQueryParameters2D::get_simplify_path() const {
	return simplify_path;
}

void NavigationPathQueryParameters2D::set_simplify_epsilon(real_t p_epsilon) {
	simplify_epsilon = MAX(0.0, p_epsilon);
}

real_t NavigationPathQueryParameters2D::get_simplify_epsilon() const {
	return simplify_epsilon;
}

void NavigationPathQueryParameters2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_pathfinding_algorithm", "pathfinding_algorithm"), &NavigationPathQueryParameters2D::set_pathfinding_algorithm);
	ClassDB::bind_method(D_METHOD("get_pathfinding_algorithm"), &NavigationPathQueryParameters2D::get_pathfinding_algorithm);

	ClassDB::bind_method(D_METHOD("set_path_postprocessing", "path_postprocessing"), &NavigationPathQueryParameters2D::set_path_postprocessing);
	ClassDB::bind_method(D_METHOD("get_path_postprocessing"), &NavigationPathQueryParameters2D::get_path_postprocessing);

	ClassDB::bind_method(D_METHOD("set_map", "map"), &NavigationPathQueryParameters2D::set_map);
	ClassDB::bind_method(D_METHOD("get_map"), &NavigationPathQueryParameters2D::get_map);

	ClassDB::bind_method(D_METHOD("set_start_position", "start_position"), &NavigationPathQueryParameters2D::set_start_position);
	ClassDB::bind_method(D_METHOD("get_start_position"), &NavigationPathQueryParameters2D::get_start_position);

	ClassDB::bind_method(D_METHOD("set_target_position", "target_position"), &NavigationPathQueryParameters2D::set_target_position);
	ClassDB::bind_method(D_METHOD("get_target_position"), &NavigationPathQueryParameters2D::get_target_position);

	ClassDB::bind_method(D_METHOD("set_navigation_layers", "navigation_layers"), &NavigationPathQueryParameters2D::set_navigation_layers);
	ClassDB::bind_method(D_METHOD("get_navigation_layers"), &NavigationPathQueryParameters2D::get_navigation_layers);

	ClassDB::bind_method(D_METHOD("set_metadata_flags", "flags"), &NavigationPathQueryParameters2D::set_metadata_flags);
	ClassDB::bind_method(D_METHOD("get_metadata_flags"), &NavigationPathQueryParameters2D::get_metadata_flags);

	ClassDB::bind_method(D_METHOD("set_simplify_path", "enabled"), &NavigationPathQueryParameters2D::set_simplify_path);
	ClassDB::bind_method(D_METHOD("get_simplify_path"), &NavigationPathQueryParameters2D::get_simplify_path);

	ClassDB::bind_method(D_METHOD("set_simplify_epsilon", "epsilon"), &NavigationPathQueryParameters2D::set_simplify_epsilon);
	ClassDB::bind_method(D_METHOD("get_simplify_epsilon"), &NavigationPathQueryParameters2D::get_simplify_epsilon);

	ADD_PROPERTY(PropertyInfo(Variant::RID, "map"), "set_map", "get_map");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "start_position"), "set_start_position", "get_start_position");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "target_position"), "set_target_position", "get_target_position");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "navigation_layers", PROPERTY_HINT_LAYERS_2D_NAVIGATION), "set_navigation_layers", "get_navigation_layers");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "pathfinding_algorithm", PROPERTY_HINT_ENUM, "AStar"), "set_pathfinding_algorithm", "get_pathfinding_algorithm");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "path_postprocessing", PROPERTY_HINT_ENUM, "Corridorfunnel,Edgecentered,None"), "set_path_postprocessing", "get_path_postprocessing");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "metadata_flags", PROPERTY_HINT_FLAGS, "Include Types,Include RIDs,Include Owners"), "set_metadata_flags", "get_metadata_flags");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "simplify_path"), "set_simplify_path", "get_simplify_path");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "simplify_epsilon"), "set_simplify_epsilon", "get_simplify_epsilon");

	BIND_ENUM_CONSTANT(PATHFINDING_ALGORITHM_ASTAR);

	BIND_ENUM_CONSTANT(PATH_POSTPROCESSING_CORRIDORFUNNEL);
	BIND_ENUM_CONSTANT(PATH_POSTPROCESSING_EDGECENTERED);
	BIND_ENUM_CONSTANT(PATH_POSTPROCESSING_NONE);

	BIND_BITFIELD_FLAG(PATH_METADATA_INCLUDE_NONE);
	BIND_BITFIELD_FLAG(PATH_METADATA_INCLUDE_TYPES);
	BIND_BITFIELD_FLAG(PATH_METADATA_INCLUDE_RIDS);
	BIND_BITFIELD_FLAG(PATH_METADATA_INCLUDE_OWNERS);
	BIND_BITFIELD_FLAG(PATH_METADATA_INCLUDE_ALL);
}
