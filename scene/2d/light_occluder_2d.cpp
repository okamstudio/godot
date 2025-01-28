/**************************************************************************/
/*  light_occluder_2d.cpp                                                 */
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

#include "light_occluder_2d.h"

#include "core/config/engine.h"
#include "core/math/geometry_2d.h"

#define LINE_GRAB_WIDTH 8

#ifdef DEBUG_ENABLED
Rect2 OccluderPolygon2D::_edit_get_rect() const {
	if (rect_cache_dirty) {
		if (closed) {
			const Vector2 *r = polygon.ptr();
			item_rect = Rect2();
			for (int i = 0; i < polygon.size(); i++) {
				Vector2 pos = r[i];
				if (i == 0) {
					item_rect.position = pos;
				} else {
					item_rect.expand_to(pos);
				}
			}
			rect_cache_dirty = false;
		} else {
			if (polygon.size() == 0) {
				item_rect = Rect2();
			} else {
				Vector2 d = Vector2(LINE_GRAB_WIDTH, LINE_GRAB_WIDTH);
				item_rect = Rect2(polygon[0] - d, 2 * d);
				for (int i = 1; i < polygon.size(); i++) {
					item_rect.expand_to(polygon[i] - d);
					item_rect.expand_to(polygon[i] + d);
				}
			}
		}
	}

	return item_rect;
}

bool OccluderPolygon2D::_edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const {
	if (closed) {
		return Geometry2D::is_point_in_polygon(p_point, Variant(polygon));
	} else {
		const real_t d = LINE_GRAB_WIDTH / 2 + p_tolerance;
		const Vector2 *points = polygon.ptr();
		for (int i = 0; i < polygon.size() - 1; i++) {
			Vector2 p = Geometry2D::get_closest_point_to_segment(p_point, &points[i]);
			if (p.distance_to(p_point) <= d) {
				return true;
			}
		}

		return false;
	}
}
#endif // DEBUG_ENABLED

void OccluderPolygon2D::set_polygon(const Vector<Vector2> &p_polygon) {
	polygon = p_polygon;
	rect_cache_dirty = true;
	RS::get_singleton()->canvas_occluder_polygon_set_shape(occ_polygon, p_polygon, closed);
	emit_changed();
	update_configuration_warning();
}

Vector<Vector2> OccluderPolygon2D::get_polygon() const {
	return polygon;
}

void OccluderPolygon2D::set_closed(bool p_closed) {
	if (closed == p_closed) {
		return;
	}
	closed = p_closed;
	if (polygon.size()) {
		RS::get_singleton()->canvas_occluder_polygon_set_shape(occ_polygon, polygon, closed);
	}
	emit_changed();
}

bool OccluderPolygon2D::is_closed() const {
	return closed;
}

void OccluderPolygon2D::set_cull_mode(CullMode p_mode) {
	cull = p_mode;
	RS::get_singleton()->canvas_occluder_polygon_set_cull_mode(occ_polygon, RS::CanvasOccluderPolygonCullMode(p_mode));
}

OccluderPolygon2D::CullMode OccluderPolygon2D::get_cull_mode() const {
	return cull;
}

RID OccluderPolygon2D::get_rid() const {
	return occ_polygon;
}

void OccluderPolygon2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_closed", "closed"), &OccluderPolygon2D::set_closed);
	ClassDB::bind_method(D_METHOD("is_closed"), &OccluderPolygon2D::is_closed);

	ClassDB::bind_method(D_METHOD("set_cull_mode", "cull_mode"), &OccluderPolygon2D::set_cull_mode);
	ClassDB::bind_method(D_METHOD("get_cull_mode"), &OccluderPolygon2D::get_cull_mode);

	ClassDB::bind_method(D_METHOD("set_polygon", "polygon"), &OccluderPolygon2D::set_polygon);
	ClassDB::bind_method(D_METHOD("get_polygon"), &OccluderPolygon2D::get_polygon);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "closed"), "set_closed", "is_closed");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cull_mode", PROPERTY_HINT_ENUM, "Disabled,ClockWise,CounterClockWise"), "set_cull_mode", "get_cull_mode");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "polygon"), "set_polygon", "get_polygon");

	BIND_ENUM_CONSTANT(CULL_DISABLED);
	BIND_ENUM_CONSTANT(CULL_CLOCKWISE);
	BIND_ENUM_CONSTANT(CULL_COUNTER_CLOCKWISE);
}

OccluderPolygon2D::OccluderPolygon2D() {
	occ_polygon = RS::get_singleton()->canvas_occluder_polygon_create();
}

OccluderPolygon2D::~OccluderPolygon2D() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RS::get_singleton()->free(occ_polygon);
}

void LightOccluder2D::_poly_changed() {
#ifdef DEBUG_ENABLED
	queue_redraw();
#endif // DEBUG_ENABLED
}

void LightOccluder2D::_physics_interpolated_changed() {
	RenderingServer::get_singleton()->canvas_light_occluder_set_interpolated(occluder, is_physics_interpolated());
}

void LightOccluder2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_CANVAS: {
			RS::get_singleton()->canvas_light_occluder_attach_to_canvas(occluder, get_canvas());
			RS::get_singleton()->canvas_light_occluder_set_transform(occluder, get_global_transform());
			RS::get_singleton()->canvas_light_occluder_set_enabled(occluder, is_visible_in_tree());
		} break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			RS::get_singleton()->canvas_light_occluder_set_transform(occluder, get_global_transform());
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			RS::get_singleton()->canvas_light_occluder_set_enabled(occluder, is_visible_in_tree());
		} break;

		case NOTIFICATION_DRAW: {
			if (Engine::get_singleton()->is_editor_hint()) {
				if (occluder_polygon.is_valid()) {
					Vector<Vector2> poly = occluder_polygon->get_polygon();

					if (poly.size()) {
						if (occluder_polygon->is_closed()) {
							Vector<Color> color;
							color.push_back(Color(0, 0, 0, 0.6));
							draw_polygon(Variant(poly), color);
						} else {
							int ps = poly.size();
							const Vector2 *r = poly.ptr();
							for (int i = 0; i < ps - 1; i++) {
								draw_line(r[i], r[i + 1], Color(0, 0, 0, 0.6), 3);
							}
						}
					}
				}
			}
		} break;

		case NOTIFICATION_EXIT_CANVAS: {
			RS::get_singleton()->canvas_light_occluder_attach_to_canvas(occluder, RID());
		} break;

		case NOTIFICATION_RESET_PHYSICS_INTERPOLATION: {
			if (is_visible_in_tree() && is_physics_interpolated()) {
				// Explicitly make sure the transform is up to date in RenderingServer before
				// resetting. This is necessary because NOTIFICATION_TRANSFORM_CHANGED
				// is normally deferred, and a client change to transform will not always be sent
				// before the reset, so we need to guarantee this.
				RS::get_singleton()->canvas_light_occluder_set_transform(occluder, get_global_transform());
				RS::get_singleton()->canvas_light_occluder_reset_physics_interpolation(occluder);
			}
		} break;
	}
}

#ifdef DEBUG_ENABLED
Rect2 LightOccluder2D::_edit_get_rect() const {
	return occluder_polygon.is_valid() ? occluder_polygon->_edit_get_rect() : Rect2();
}

bool LightOccluder2D::_edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const {
	return occluder_polygon.is_valid() ? occluder_polygon->_edit_is_selected_on_click(p_point, p_tolerance) : false;
}
#endif // DEBUG_ENABLED

void LightOccluder2D::set_occluder_polygon(const Ref<OccluderPolygon2D> &p_polygon) {
#ifdef DEBUG_ENABLED
	if (occluder_polygon.is_valid()) {
		occluder_polygon->disconnect_changed(callable_mp(this, &LightOccluder2D::_poly_changed));
	}
#endif // DEBUG_ENABLED
	occluder_polygon = p_polygon;

	if (occluder_polygon.is_valid()) {
		RS::get_singleton()->canvas_light_occluder_set_polygon(occluder, occluder_polygon->get_rid());
	} else {
		RS::get_singleton()->canvas_light_occluder_set_polygon(occluder, RID());
	}

#ifdef DEBUG_ENABLED
	if (occluder_polygon.is_valid()) {
		occluder_polygon->connect_changed(callable_mp(this, &LightOccluder2D::_poly_changed));
	}
	queue_redraw();
#endif // DEBUG_ENABLED
}

Ref<OccluderPolygon2D> LightOccluder2D::get_occluder_polygon() const {
	return occluder_polygon;
}

void LightOccluder2D::set_occluder_light_mask(int p_mask) {
	mask = p_mask;
	RS::get_singleton()->canvas_light_occluder_set_light_mask(occluder, p_mask);
}

int LightOccluder2D::get_occluder_light_mask() const {
	return mask;
}

PackedStringArray LightOccluder2D::get_configuration_warnings() const {
	PackedStringArray warnings = Node2D::get_configuration_warnings();

	if (occluder_polygon.is_null()) {
		warnings.push_back(RTR("An occluder polygon must be set (or drawn) for this occluder to take effect."));
	}

	if (occluder_polygon.is_valid() && occluder_polygon->get_polygon().size() == 0) {
		warnings.push_back(RTR("The occluder polygon for this occluder is empty. Please draw a polygon."));
	}

	return warnings;
}

void LightOccluder2D::set_as_sdf_collision(bool p_enable) {
	sdf_collision = p_enable;
	RS::get_singleton()->canvas_light_occluder_set_as_sdf_collision(occluder, sdf_collision);
}
bool LightOccluder2D::is_set_as_sdf_collision() const {
	return sdf_collision;
}

void LightOccluder2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_occluder_polygon", "polygon"), &LightOccluder2D::set_occluder_polygon);
	ClassDB::bind_method(D_METHOD("get_occluder_polygon"), &LightOccluder2D::get_occluder_polygon);

	ClassDB::bind_method(D_METHOD("set_occluder_light_mask", "mask"), &LightOccluder2D::set_occluder_light_mask);
	ClassDB::bind_method(D_METHOD("get_occluder_light_mask"), &LightOccluder2D::get_occluder_light_mask);

	ClassDB::bind_method(D_METHOD("set_as_sdf_collision", "enable"), &LightOccluder2D::set_as_sdf_collision);
	ClassDB::bind_method(D_METHOD("is_set_as_sdf_collision"), &LightOccluder2D::is_set_as_sdf_collision);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "occluder", PROPERTY_HINT_RESOURCE_TYPE, "OccluderPolygon2D"), "set_occluder_polygon", "get_occluder_polygon");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "sdf_collision"), "set_as_sdf_collision", "is_set_as_sdf_collision");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "occluder_light_mask", PROPERTY_HINT_LAYERS_2D_RENDER), "set_occluder_light_mask", "get_occluder_light_mask");
}

LightOccluder2D::LightOccluder2D() {
	occluder = RS::get_singleton()->canvas_light_occluder_create();

	set_notify_transform(true);
	set_as_sdf_collision(true);
}

LightOccluder2D::~LightOccluder2D() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());

	RS::get_singleton()->free(occluder);
}
