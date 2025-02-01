/**************************************************************************/
/*  navigation_utilities.h                                                */
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

#include "core/math/vector3.h"
#include "core/variant/typed_array.h"

namespace NavigationUtilities {

enum PathfindingAlgorithm {
	PATHFINDING_ALGORITHM_ASTAR = 0,
};

enum PathPostProcessing {
	PATH_POSTPROCESSING_CORRIDORFUNNEL = 0,
	PATH_POSTPROCESSING_EDGECENTERED,
	PATH_POSTPROCESSING_NONE,
};

enum PathSegmentType {
	PATH_SEGMENT_TYPE_REGION = 0,
	PATH_SEGMENT_TYPE_LINK
};

enum PathMetadataFlags {
	PATH_INCLUDE_NONE = 0,
	PATH_INCLUDE_TYPES = 1,
	PATH_INCLUDE_RIDS = 2,
	PATH_INCLUDE_OWNERS = 4,
	PATH_INCLUDE_ALL = PATH_INCLUDE_TYPES | PATH_INCLUDE_RIDS | PATH_INCLUDE_OWNERS
};

} //namespace NavigationUtilities
