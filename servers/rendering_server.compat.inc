/**************************************************************************/
/*  rendering_server.compat.inc                                           */
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

#ifndef DISABLE_DEPRECATED

void RenderingServer::_multimesh_allocate_data_bind_compat_99455(RID p_multimesh, int p_instances, MultimeshTransformFormat p_transform_format, bool p_use_colors, bool p_use_custom_data) {
	multimesh_allocate_data(p_multimesh, p_instances, p_transform_format, p_use_colors, p_use_custom_data, false);
}

void RenderingServer::_environment_set_fog_bind_compat_84792(RID p_env, bool p_enable, const Color &p_light_color, float p_light_energy, float p_sun_scatter, float p_density, float p_height, float p_height_density, float p_aerial_perspective, float p_sky_affect) {
	environment_set_fog(p_env, p_enable, p_light_color, p_light_energy, p_sun_scatter, p_density, p_height, p_height_density, p_aerial_perspective, p_sky_affect, RS::EnvironmentFogMode::ENV_FOG_MODE_EXPONENTIAL);
}

void RenderingServer::_canvas_item_add_multiline_bind_compat_84523(RID p_item, const Vector<Point2> &p_points, const Vector<Color> &p_colors, float p_width) {
	canvas_item_add_multiline(p_item, p_points, p_colors, p_width, false);
}

void RenderingServer::_canvas_item_add_rect_bind_compat_84523(RID p_item, const Rect2 &p_rect, const Color &p_color) {
	canvas_item_add_rect(p_item, p_rect, p_color, false);
}

void RenderingServer::_canvas_item_add_circle_bind_compat_84523(RID p_item, const Point2 &p_pos, float p_radius, const Color &p_color) {
	canvas_item_add_circle(p_item, p_pos, p_radius, p_color, false);
}

void RenderingServer::_bind_compatibility_methods() {
	ClassDB::bind_compatibility_method(D_METHOD("multimesh_allocate_data", "multimesh", "instances", "transform_format", "color_format", "custom_data_format"), &RenderingServer::_multimesh_allocate_data_bind_compat_99455, DEFVAL(false), DEFVAL(false));
	ClassDB::bind_compatibility_method(D_METHOD("environment_set_fog", "env", "enable", "light_color", "light_energy", "sun_scatter", "density", "height", "height_density", "aerial_perspective", "sky_affect"), &RenderingServer::_environment_set_fog_bind_compat_84792);
	ClassDB::bind_compatibility_method(D_METHOD("canvas_item_add_multiline", "item", "points", "colors", "width"), &RenderingServer::_canvas_item_add_multiline_bind_compat_84523, DEFVAL(-1.0));
	ClassDB::bind_compatibility_method(D_METHOD("canvas_item_add_rect", "item", "rect", "color"), &RenderingServer::_canvas_item_add_rect_bind_compat_84523);
	ClassDB::bind_compatibility_method(D_METHOD("canvas_item_add_circle", "item", "pos", "radius", "color"), &RenderingServer::_canvas_item_add_circle_bind_compat_84523);
}

#endif
