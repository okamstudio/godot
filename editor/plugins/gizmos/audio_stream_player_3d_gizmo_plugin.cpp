/**************************************************************************/
/*  audio_stream_player_3d_gizmo_plugin.cpp                               */
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

#include "audio_stream_player_3d_gizmo_plugin.h"

#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "scene/3d/audio_stream_player_3d.h"

AudioStreamPlayer3DGizmoPlugin::AudioStreamPlayer3DGizmoPlugin() {
	Color gizmo_color = EDITOR_GET("editors/3d_gizmos/gizmo_colors/stream_player_3d");

	create_icon_material("stream_player_3d_icon", EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("Gizmo3DSamplePlayer"), EditorStringName(EditorIcons)));
	create_material("stream_player_3d_material_primary", gizmo_color);
	create_material("stream_player_3d_material_secondary", gizmo_color * Color(1, 1, 1, 0.35));
	// Enable vertex colors for the billboard material as the gizmo color depends on the
	// AudioStreamPlayer3D attenuation type and source (Unit Size or Max Distance).
	create_material("stream_player_3d_material_billboard", Color(1, 1, 1), true, false, true);
	create_handle_material("handles");
}

bool AudioStreamPlayer3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<AudioStreamPlayer3D>(p_spatial) != nullptr;
}

String AudioStreamPlayer3DGizmoPlugin::get_gizmo_name() const {
	return "AudioStreamPlayer3D";
}

int AudioStreamPlayer3DGizmoPlugin::get_priority() const {
	return -1;
}

String AudioStreamPlayer3DGizmoPlugin::get_handle_name(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	return "Emission Radius";
}

Variant AudioStreamPlayer3DGizmoPlugin::get_handle_value(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	AudioStreamPlayer3D *player = Object::cast_to<AudioStreamPlayer3D>(p_gizmo->get_node_3d());
	return player->get_emission_angle();
}

void AudioStreamPlayer3DGizmoPlugin::set_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, Camera3D *p_camera, const Point2 &p_point) {
	AudioStreamPlayer3D *player = Object::cast_to<AudioStreamPlayer3D>(p_gizmo->get_node_3d());

	Transform3D gt = player->get_global_transform();
	Transform3D gi = gt.affine_inverse();

	Vector3 ray_from = p_camera->project_ray_origin(p_point);
	Vector3 ray_dir = p_camera->project_ray_normal(p_point);
	Vector3 ray_to = ray_from + ray_dir * 4096;

	ray_from = gi.xform(ray_from);
	ray_to = gi.xform(ray_to);

	float closest_dist = 1e20;
	float closest_angle = 1e20;

	for (int i = 0; i < 180; i++) {
		float a = Math::deg_to_rad((float)i);
		float an = Math::deg_to_rad((float)(i + 1));

		Vector3 from(Math::sin(a), 0, -Math::cos(a));
		Vector3 to(Math::sin(an), 0, -Math::cos(an));

		Vector3 r1, r2;
		Geometry3D::get_closest_points_between_segments(from, to, ray_from, ray_to, r1, r2);
		float d = r1.distance_to(r2);
		if (d < closest_dist) {
			closest_dist = d;
			closest_angle = i;
		}
	}

	if (closest_angle < 91) {
		player->set_emission_angle(closest_angle);
	}
}

void AudioStreamPlayer3DGizmoPlugin::commit_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, const Variant &p_restore, bool p_cancel) {
	AudioStreamPlayer3D *player = Object::cast_to<AudioStreamPlayer3D>(p_gizmo->get_node_3d());

	if (p_cancel) {
		player->set_emission_angle(p_restore);

	} else {
		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action(TTR("Change AudioStreamPlayer3D Emission Angle"));
		ur->add_do_method(player, "set_emission_angle", player->get_emission_angle());
		ur->add_undo_method(player, "set_emission_angle", p_restore);
		ur->commit_action();
	}
}

void AudioStreamPlayer3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	p_gizmo->clear();

	if (p_gizmo->is_selected()) {
		const AudioStreamPlayer3D *player = Object::cast_to<AudioStreamPlayer3D>(p_gizmo->get_node_3d());

		if (player->get_attenuation_model() != AudioStreamPlayer3D::ATTENUATION_DISABLED || player->get_max_distance() > CMP_EPSILON) {
			// Draw a circle to represent sound volume attenuation.
			// Use only a billboard circle to represent radius.
			// This helps distinguish AudioStreamPlayer3D gizmos from OmniLight3D gizmos.
			const Ref<Material> lines_billboard_material = get_material("stream_player_3d_material_billboard", p_gizmo);

			// Soft distance cap varies depending on attenuation model, as some will fade out more aggressively than others.
			// Multipliers were empirically determined through testing.
			float soft_multiplier;
			switch (player->get_attenuation_model()) {
				case AudioStreamPlayer3D::ATTENUATION_INVERSE_DISTANCE:
					soft_multiplier = 12.0;
					break;
				case AudioStreamPlayer3D::ATTENUATION_INVERSE_SQUARE_DISTANCE:
					soft_multiplier = 4.0;
					break;
				case AudioStreamPlayer3D::ATTENUATION_LOGARITHMIC:
					soft_multiplier = 3.25;
					break;
				default:
					// Ensures Max Distance's radius visualization is not capped by Unit Size
					// (when the attenuation mode is Disabled).
					soft_multiplier = 10000.0;
					break;
			}

			// Draw the distance at which the sound can be reasonably heard.
			// This can be either a hard distance cap with the Max Distance property (if set above 0.0),
			// or a soft distance cap with the Unit Size property (sound never reaches true zero).
			// When Max Distance is 0.0, `r` represents the distance above which the
			// sound can't be heard in *most* (but not all) scenarios.
			float r;
			if (player->get_max_distance() > CMP_EPSILON) {
				r = MIN(player->get_unit_size() * soft_multiplier, player->get_max_distance());
			} else {
				r = player->get_unit_size() * soft_multiplier;
			}
			Vector<Vector3> points_billboard;

			for (int i = 0; i < 120; i++) {
				// Create a circle.
				const float ra = Math::deg_to_rad((float)(i * 3));
				const float rb = Math::deg_to_rad((float)((i + 1) * 3));
				const Point2 a = Vector2(Math::sin(ra), Math::cos(ra)) * r;
				const Point2 b = Vector2(Math::sin(rb), Math::cos(rb)) * r;

				// Draw a billboarded circle.
				points_billboard.push_back(Vector3(a.x, a.y, 0));
				points_billboard.push_back(Vector3(b.x, b.y, 0));
			}

			Color color;
			switch (player->get_attenuation_model()) {
				// Pick cold colors for all attenuation models (except Disabled),
				// so that soft caps can be easily distinguished from hard caps
				// (which use warm colors).
				case AudioStreamPlayer3D::ATTENUATION_INVERSE_DISTANCE:
					color = Color(0.4, 0.8, 1);
					break;
				case AudioStreamPlayer3D::ATTENUATION_INVERSE_SQUARE_DISTANCE:
					color = Color(0.4, 0.5, 1);
					break;
				case AudioStreamPlayer3D::ATTENUATION_LOGARITHMIC:
					color = Color(0.4, 0.2, 1);
					break;
				default:
					// Disabled attenuation mode.
					// This is never reached when Max Distance is 0, but the
					// hue-inverted form of this color will be used if Max Distance is greater than 0.
					color = Color(1, 1, 1);
					break;
			}

			if (player->get_max_distance() > CMP_EPSILON) {
				// Sound is hard-capped by max distance. The attenuation model still matters,
				// so invert the hue of the color that was chosen above.
				color.set_h(color.get_h() + 0.5);
			}

			p_gizmo->add_lines(points_billboard, lines_billboard_material, true, color);
		}

		if (player->is_emission_angle_enabled()) {
			const float pc = player->get_emission_angle();
			const float ofs = -Math::cos(Math::deg_to_rad(pc));
			const float radius = Math::sin(Math::deg_to_rad(pc));

			Vector<Vector3> points_primary;
			points_primary.resize(200);

			real_t step = Math_TAU / 100.0;
			for (int i = 0; i < 100; i++) {
				const float a = i * step;
				const float an = (i + 1) * step;

				const Vector3 from(Math::sin(a) * radius, Math::cos(a) * radius, ofs);
				const Vector3 to(Math::sin(an) * radius, Math::cos(an) * radius, ofs);

				points_primary.write[i * 2 + 0] = from;
				points_primary.write[i * 2 + 1] = to;
			}

			const Ref<Material> material_primary = get_material("stream_player_3d_material_primary", p_gizmo);
			p_gizmo->add_lines(points_primary, material_primary);

			Vector<Vector3> points_secondary;
			points_secondary.resize(16);

			for (int i = 0; i < 8; i++) {
				const float a = i * (Math_TAU / 8.0);
				const Vector3 from(Math::sin(a) * radius, Math::cos(a) * radius, ofs);

				points_secondary.write[i * 2 + 0] = from;
				points_secondary.write[i * 2 + 1] = Vector3();
			}

			const Ref<Material> material_secondary = get_material("stream_player_3d_material_secondary", p_gizmo);
			p_gizmo->add_lines(points_secondary, material_secondary);

			Vector<Vector3> handles;
			const float ha = Math::deg_to_rad(player->get_emission_angle());
			handles.push_back(Vector3(Math::sin(ha), 0, -Math::cos(ha)));
			p_gizmo->add_handles(handles, get_material("handles"));
		}
	}

	const Ref<Material> icon = get_material("stream_player_3d_icon", p_gizmo);
	p_gizmo->add_unscaled_billboard(icon, 0.05);
}
