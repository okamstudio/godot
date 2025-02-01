/**************************************************************************/
/*  navigation_path_query_parameters_2d.h                                 */
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

#include "core/object/ref_counted.h"
#include "servers/navigation/navigation_utilities.h"

class NavigationPathQueryParameters2D : public RefCounted {
	GDCLASS(NavigationPathQueryParameters2D, RefCounted);

protected:
	static void _bind_methods();

public:
	enum PathfindingAlgorithm {
		PATHFINDING_ALGORITHM_ASTAR = NavigationUtilities::PATHFINDING_ALGORITHM_ASTAR,
	};

	enum PathPostProcessing {
		PATH_POSTPROCESSING_CORRIDORFUNNEL = NavigationUtilities::PATH_POSTPROCESSING_CORRIDORFUNNEL,
		PATH_POSTPROCESSING_EDGECENTERED = NavigationUtilities::PATH_POSTPROCESSING_EDGECENTERED,
		PATH_POSTPROCESSING_NONE = NavigationUtilities::PATH_POSTPROCESSING_NONE,
	};

	enum PathMetadataFlags {
		PATH_METADATA_INCLUDE_NONE = NavigationUtilities::PathMetadataFlags::PATH_INCLUDE_NONE,
		PATH_METADATA_INCLUDE_TYPES = NavigationUtilities::PathMetadataFlags::PATH_INCLUDE_TYPES,
		PATH_METADATA_INCLUDE_RIDS = NavigationUtilities::PathMetadataFlags::PATH_INCLUDE_RIDS,
		PATH_METADATA_INCLUDE_OWNERS = NavigationUtilities::PathMetadataFlags::PATH_INCLUDE_OWNERS,
		PATH_METADATA_INCLUDE_ALL = NavigationUtilities::PathMetadataFlags::PATH_INCLUDE_ALL
	};

private:
	PathfindingAlgorithm pathfinding_algorithm = PATHFINDING_ALGORITHM_ASTAR;
	PathPostProcessing path_postprocessing = PATH_POSTPROCESSING_CORRIDORFUNNEL;
	RID map;
	Vector2 start_position;
	Vector2 target_position;
	uint32_t navigation_layers = 1;
	BitField<PathMetadataFlags> metadata_flags = PATH_METADATA_INCLUDE_ALL;
	bool simplify_path = false;
	real_t simplify_epsilon = 0.0;

public:
	void set_pathfinding_algorithm(const PathfindingAlgorithm p_pathfinding_algorithm);
	PathfindingAlgorithm get_pathfinding_algorithm() const;

	void set_path_postprocessing(const PathPostProcessing p_path_postprocessing);
	PathPostProcessing get_path_postprocessing() const;

	void set_map(RID p_map);
	RID get_map() const;

	void set_start_position(const Vector2 p_start_position);
	Vector2 get_start_position() const;

	void set_target_position(const Vector2 p_target_position);
	Vector2 get_target_position() const;

	void set_navigation_layers(uint32_t p_navigation_layers);
	uint32_t get_navigation_layers() const;

	void set_metadata_flags(BitField<NavigationPathQueryParameters2D::PathMetadataFlags> p_flags);
	BitField<NavigationPathQueryParameters2D::PathMetadataFlags> get_metadata_flags() const;

	void set_simplify_path(bool p_enabled);
	bool get_simplify_path() const;

	void set_simplify_epsilon(real_t p_epsilon);
	real_t get_simplify_epsilon() const;
};

VARIANT_ENUM_CAST(NavigationPathQueryParameters2D::PathfindingAlgorithm);
VARIANT_ENUM_CAST(NavigationPathQueryParameters2D::PathPostProcessing);
VARIANT_BITFIELD_CAST(NavigationPathQueryParameters2D::PathMetadataFlags);
