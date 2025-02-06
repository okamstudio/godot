/**************************************************************************/
/*  editor_color_map.cpp                                                  */
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

#include "editor_color_map.h"

HashMap<Color, Color> EditorColorMap::color_conversion_map;
HashSet<StringName> EditorColorMap::color_conversion_exceptions;

void EditorColorMap::add_conversion_color_pair(const String &p_from_color, const String &p_to_color) {
	color_conversion_map[Color::html(p_from_color)] = Color::html(p_to_color);
}

void EditorColorMap::add_conversion_exception(const StringName &p_icon_name) {
	color_conversion_exceptions.insert(p_icon_name);
}

void EditorColorMap::create() {
	// Some of the colors below are listed for completeness sake.
	// This can be a basis for proper palette validation later.

	// Convert:               FROM       TO
	add_conversion_color_pair("#478cbf", "#478cbf"); // Godot Blue
	add_conversion_color_pair("#414042", "#414042"); // Godot Gray

	add_conversion_color_pair("#ffffff", "#414141"); // Pure white
	add_conversion_color_pair("#fefefe", "#fefefe"); // Forced light color
	add_conversion_color_pair("#000000", "#bfbfbf"); // Pure black
	add_conversion_color_pair("#010101", "#010101"); // Forced dark color

	// Keep pure RGB colors as is, but list them for explicitness.
	add_conversion_color_pair("#ff0000", "#ff0000"); // Pure red
	add_conversion_color_pair("#00ff00", "#00ff00"); // Pure green
	add_conversion_color_pair("#0000ff", "#0000ff"); // Pure blue

	// GUI Colors
	add_conversion_color_pair("#e0e0e0", "#5a5a5a"); // Common icon color
	add_conversion_color_pair("#808080", "#808080"); // GUI disabled color
	add_conversion_color_pair("#b3b3b3", "#363636"); // GUI disabled light color
	add_conversion_color_pair("#699ce8", "#699ce8"); // GUI highlight color
	add_conversion_color_pair("#f9f9f9", "#606060"); // Scrollbar grabber highlight color

	add_conversion_color_pair("#5fb2ff", "#0079f0"); // Selection (blue)
	add_conversion_color_pair("#003e7a", "#2b74bb"); // Selection (darker blue)
	add_conversion_color_pair("#f7f5cf", "#615f3a"); // Gizmo (yellow)

	add_conversion_color_pair("#c38ef1", "#a85de9"); // Animation
	add_conversion_color_pair("#8da5f3", "#3d64dd"); // 2D
	add_conversion_color_pair("#7582a8", "#6d83c8"); // 2D Abstract
	add_conversion_color_pair("#fc7f7f", "#cd3838"); // 3D
	add_conversion_color_pair("#b56d6d", "#be6a6a"); // 3D Abstract
	add_conversion_color_pair("#8eef97", "#2fa139"); // GUI Control
	add_conversion_color_pair("#76ad7b", "#64a66a"); // GUI Control Abstract
	add_conversion_color_pair("#f0caa0", "#844b0e"); // Editor-only

	// Rainbow
	add_conversion_color_pair("#ff4545", "#ff2929"); // Red
	add_conversion_color_pair("#ffe345", "#ffe337"); // Yellow
	add_conversion_color_pair("#80ff45", "#74ff34"); // Green
	add_conversion_color_pair("#45ffa2", "#2cff98"); // Aqua
	add_conversion_color_pair("#45d7ff", "#22ccff"); // Blue
	add_conversion_color_pair("#8045ff", "#702aff"); // Purple
	add_conversion_color_pair("#ff4596", "#ff2781"); // Pink

	// Audio gradients
	add_conversion_color_pair("#e1da5b", "#d6cf4b"); // Yellow

	add_conversion_color_pair("#62aeff", "#1678e0"); // Frozen gradient top
	add_conversion_color_pair("#75d1e6", "#41acc5"); // Frozen gradient middle
	add_conversion_color_pair("#84ffee", "#49ccba"); // Frozen gradient bottom

	add_conversion_color_pair("#f70000", "#c91616"); // Color track red
	add_conversion_color_pair("#eec315", "#d58c0b"); // Color track orange
	add_conversion_color_pair("#dbee15", "#b7d10a"); // Color track yellow
	add_conversion_color_pair("#288027", "#218309"); // Color track green

	// Other objects
	add_conversion_color_pair("#ffca5f", "#fea900"); // Mesh resource (orange)
	add_conversion_color_pair("#2998ff", "#68b6ff"); // Shape resource (blue)
	add_conversion_color_pair("#a2d2ff", "#4998e3"); // Shape resource (light blue)
	add_conversion_color_pair("#69c4d4", "#29a3cc"); // Input event highlight (light blue)

	// Animation editor tracks
	// The property track icon color is set by the common icon color.
	add_conversion_color_pair("#ea7940", "#bd5e2c"); // 3D Position track
	add_conversion_color_pair("#ff2b88", "#bd165f"); // 3D Rotation track
	add_conversion_color_pair("#eac840", "#bd9d1f"); // 3D Scale track
	add_conversion_color_pair("#3cf34e", "#16a827"); // Call Method track
	add_conversion_color_pair("#2877f6", "#236be6"); // Bezier Curve track
	add_conversion_color_pair("#eae440", "#9f9722"); // Audio Playback track
	add_conversion_color_pair("#a448f0", "#9853ce"); // Animation Playback track
	add_conversion_color_pair("#5ad5c4", "#0a9c88"); // Blend Shape track

	// Control layouts
	add_conversion_color_pair("#d6d6d6", "#474747"); // Highlighted part
	add_conversion_color_pair("#474747", "#d6d6d6"); // Background part
	add_conversion_color_pair("#919191", "#6e6e6e"); // Border part

	// TileSet editor icons
	add_conversion_color_pair("#fce00e", "#aa8d24"); // New Single Tile
	add_conversion_color_pair("#0e71fc", "#0350bd"); // New Autotile
	add_conversion_color_pair("#c6ced4", "#828f9b"); // New Atlas

	// Variant types
	add_conversion_color_pair("#41ecad", "#25e3a0"); // Variant
	add_conversion_color_pair("#6f91f0", "#6d8eeb"); // bool
	add_conversion_color_pair("#5abbef", "#4fb2e9"); // int/uint
	add_conversion_color_pair("#35d4f4", "#27ccf0"); // float
	add_conversion_color_pair("#4593ec", "#4690e7"); // String
	add_conversion_color_pair("#ee5677", "#ee7991"); // AABB
	add_conversion_color_pair("#e0e0e0", "#5a5a5a"); // Array
	add_conversion_color_pair("#e1ec41", "#b2bb19"); // Basis
	add_conversion_color_pair("#54ed9e", "#57e99f"); // Dictionary
	add_conversion_color_pair("#417aec", "#6993ec"); // NodePath
	add_conversion_color_pair("#55f3e3", "#12d5c3"); // Object
	add_conversion_color_pair("#f74949", "#f77070"); // Plane
	add_conversion_color_pair("#44bd44", "#46b946"); // Projection
	add_conversion_color_pair("#ec418e", "#ec69a3"); // Quaternion
	add_conversion_color_pair("#f1738f", "#ee758e"); // Rect2
	add_conversion_color_pair("#41ec80", "#2ce573"); // RID
	add_conversion_color_pair("#b9ec41", "#96ce1a"); // Transform2D
	add_conversion_color_pair("#f68f45", "#f49047"); // Transform3D
	add_conversion_color_pair("#ac73f1", "#ad76ee"); // Vector2
	add_conversion_color_pair("#de66f0", "#dc6aed"); // Vector3
	add_conversion_color_pair("#f066bd", "#ed6abd"); // Vector4

	// Visual shaders
	add_conversion_color_pair("#77ce57", "#67c046"); // Vector funcs
	add_conversion_color_pair("#ea686c", "#d95256"); // Vector transforms
	add_conversion_color_pair("#eac968", "#d9b64f"); // Textures and cubemaps
	add_conversion_color_pair("#cf68ea", "#c050dd"); // Functions and expressions

	// These icons should not be converted.
	add_conversion_exception("EditorPivot");
	add_conversion_exception("EditorHandle");
	add_conversion_exception("Editor3DHandle");
	add_conversion_exception("EditorBoneHandle");
	add_conversion_exception("Godot");
	add_conversion_exception("Sky");
	add_conversion_exception("EditorControlAnchor");
	add_conversion_exception("DefaultProjectIcon");
	add_conversion_exception("ZoomMore");
	add_conversion_exception("ZoomLess");
	add_conversion_exception("ZoomReset");
	add_conversion_exception("LockViewport");
	add_conversion_exception("GroupViewport");
	add_conversion_exception("StatusSuccess");
	add_conversion_exception("OverbrightIndicator");
	add_conversion_exception("MaterialPreviewCube");
	add_conversion_exception("MaterialPreviewSphere");
	add_conversion_exception("MaterialPreviewQuad");

	add_conversion_exception("MaterialPreviewLight1");
	add_conversion_exception("MaterialPreviewLight2");

	// GUI
	add_conversion_exception("GuiChecked");
	add_conversion_exception("GuiRadioChecked");
	add_conversion_exception("GuiIndeterminate");
	add_conversion_exception("GuiCloseCustomizable");
	add_conversion_exception("GuiGraphNodePort");
	add_conversion_exception("GuiResizer");
	add_conversion_exception("GuiMiniCheckerboard");

	/// Code Editor.
	add_conversion_exception("GuiTab");
	add_conversion_exception("GuiSpace");
	add_conversion_exception("CodeFoldedRightArrow");
	add_conversion_exception("CodeFoldDownArrow");
	add_conversion_exception("CodeRegionFoldedRightArrow");
	add_conversion_exception("CodeRegionFoldDownArrow");
	add_conversion_exception("TextEditorPlay");
	add_conversion_exception("Breakpoint");
}

void EditorColorMap::finish() {
	color_conversion_map.clear();
	color_conversion_exceptions.clear();
}
