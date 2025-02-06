/**************************************************************************/
/*  navigation_globals.h                                                  */
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

namespace NavigationDefaults3D {

// Rasterization.

// To find the polygons edges the vertices are displaced in a grid where
// each cell has the following cell_size and cell_height.
constexpr float navmesh_cell_size{ 0.25f }; // Must match ProjectSettings default 3D cell_size and NavigationMesh cell_size.
constexpr float navmesh_cell_height{ 0.25f }; // Must match ProjectSettings default 3D cell_height and NavigationMesh cell_height.
constexpr float navmesh_cell_size_min{ 0.01f };
constexpr auto navmesh_cell_size_hint{ "0.001,100,0.001,or_greater" };

// Map.

constexpr float edge_connection_margin{ 0.25f };
constexpr float link_connection_radius{ 1.0f };

} //namespace NavigationDefaults3D

namespace NavigationDefaults2D {

// Rasterization.

// Same as in 3D but larger since 1px is treated as 1m.
constexpr float navmesh_cell_size{ 1.0f }; // Must match ProjectSettings default 2D cell_size.
constexpr auto navmesh_cell_size_hint{ "0.001,100,0.001,or_greater" };

// Map.

constexpr float edge_connection_margin{ 1.0f };
constexpr float link_connection_radius{ 4.0f };

} //namespace NavigationDefaults2D
