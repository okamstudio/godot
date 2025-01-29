/**************************************************************************/
/*  label_3d.cpp                                                          */
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

#include "label_3d.h"

#include "scene/main/window.h"
#include "scene/resources/theme.h"
#include "scene/theme/theme_db.h"

void Label3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_horizontal_alignment", "alignment"), &Label3D::set_horizontal_alignment);
	ClassDB::bind_method(D_METHOD("get_horizontal_alignment"), &Label3D::get_horizontal_alignment);

	ClassDB::bind_method(D_METHOD("set_vertical_alignment", "alignment"), &Label3D::set_vertical_alignment);
	ClassDB::bind_method(D_METHOD("get_vertical_alignment"), &Label3D::get_vertical_alignment);

	ClassDB::bind_method(D_METHOD("set_modulate", "modulate"), &Label3D::set_modulate);
	ClassDB::bind_method(D_METHOD("get_modulate"), &Label3D::get_modulate);

	ClassDB::bind_method(D_METHOD("set_outline_modulate", "modulate"), &Label3D::set_outline_modulate);
	ClassDB::bind_method(D_METHOD("get_outline_modulate"), &Label3D::get_outline_modulate);

	ClassDB::bind_method(D_METHOD("set_text", "text"), &Label3D::set_text);
	ClassDB::bind_method(D_METHOD("get_text"), &Label3D::get_text);

	ClassDB::bind_method(D_METHOD("set_text_direction", "direction"), &Label3D::set_text_direction);
	ClassDB::bind_method(D_METHOD("get_text_direction"), &Label3D::get_text_direction);

	ClassDB::bind_method(D_METHOD("set_language", "language"), &Label3D::set_language);
	ClassDB::bind_method(D_METHOD("get_language"), &Label3D::get_language);

	ClassDB::bind_method(D_METHOD("set_structured_text_bidi_override", "parser"), &Label3D::set_structured_text_bidi_override);
	ClassDB::bind_method(D_METHOD("get_structured_text_bidi_override"), &Label3D::get_structured_text_bidi_override);

	ClassDB::bind_method(D_METHOD("set_structured_text_bidi_override_options", "args"), &Label3D::set_structured_text_bidi_override_options);
	ClassDB::bind_method(D_METHOD("get_structured_text_bidi_override_options"), &Label3D::get_structured_text_bidi_override_options);

	ClassDB::bind_method(D_METHOD("set_uppercase", "enable"), &Label3D::set_uppercase);
	ClassDB::bind_method(D_METHOD("is_uppercase"), &Label3D::is_uppercase);

	ClassDB::bind_method(D_METHOD("set_render_priority", "priority"), &Label3D::set_render_priority);
	ClassDB::bind_method(D_METHOD("get_render_priority"), &Label3D::get_render_priority);

	ClassDB::bind_method(D_METHOD("set_outline_render_priority", "priority"), &Label3D::set_outline_render_priority);
	ClassDB::bind_method(D_METHOD("get_outline_render_priority"), &Label3D::get_outline_render_priority);

	ClassDB::bind_method(D_METHOD("set_font", "font"), &Label3D::set_font);
	ClassDB::bind_method(D_METHOD("get_font"), &Label3D::get_font);

	ClassDB::bind_method(D_METHOD("set_font_size", "size"), &Label3D::set_font_size);
	ClassDB::bind_method(D_METHOD("get_font_size"), &Label3D::get_font_size);

	ClassDB::bind_method(D_METHOD("set_outline_size", "outline_size"), &Label3D::set_outline_size);
	ClassDB::bind_method(D_METHOD("get_outline_size"), &Label3D::get_outline_size);

	ClassDB::bind_method(D_METHOD("set_line_spacing", "line_spacing"), &Label3D::set_line_spacing);
	ClassDB::bind_method(D_METHOD("get_line_spacing"), &Label3D::get_line_spacing);

	ClassDB::bind_method(D_METHOD("set_autowrap_mode", "autowrap_mode"), &Label3D::set_autowrap_mode);
	ClassDB::bind_method(D_METHOD("get_autowrap_mode"), &Label3D::get_autowrap_mode);

	ClassDB::bind_method(D_METHOD("set_justification_flags", "justification_flags"), &Label3D::set_justification_flags);
	ClassDB::bind_method(D_METHOD("get_justification_flags"), &Label3D::get_justification_flags);

	ClassDB::bind_method(D_METHOD("set_width", "width"), &Label3D::set_width);
	ClassDB::bind_method(D_METHOD("get_width"), &Label3D::get_width);

	ClassDB::bind_method(D_METHOD("set_pixel_size", "pixel_size"), &Label3D::set_pixel_size);
	ClassDB::bind_method(D_METHOD("get_pixel_size"), &Label3D::get_pixel_size);

	ClassDB::bind_method(D_METHOD("set_offset", "offset"), &Label3D::set_offset);
	ClassDB::bind_method(D_METHOD("get_offset"), &Label3D::get_offset);

	ClassDB::bind_method(D_METHOD("set_draw_flag", "flag", "enabled"), &Label3D::set_draw_flag);
	ClassDB::bind_method(D_METHOD("get_draw_flag", "flag"), &Label3D::get_draw_flag);

	ClassDB::bind_method(D_METHOD("set_billboard_mode", "mode"), &Label3D::set_billboard_mode);
	ClassDB::bind_method(D_METHOD("get_billboard_mode"), &Label3D::get_billboard_mode);

	ClassDB::bind_method(D_METHOD("set_alpha_cut_mode", "mode"), &Label3D::set_alpha_cut_mode);
	ClassDB::bind_method(D_METHOD("get_alpha_cut_mode"), &Label3D::get_alpha_cut_mode);

	ClassDB::bind_method(D_METHOD("set_alpha_scissor_threshold", "threshold"), &Label3D::set_alpha_scissor_threshold);
	ClassDB::bind_method(D_METHOD("get_alpha_scissor_threshold"), &Label3D::get_alpha_scissor_threshold);

	ClassDB::bind_method(D_METHOD("set_alpha_hash_scale", "threshold"), &Label3D::set_alpha_hash_scale);
	ClassDB::bind_method(D_METHOD("get_alpha_hash_scale"), &Label3D::get_alpha_hash_scale);

	ClassDB::bind_method(D_METHOD("set_alpha_antialiasing", "alpha_aa"), &Label3D::set_alpha_antialiasing);
	ClassDB::bind_method(D_METHOD("get_alpha_antialiasing"), &Label3D::get_alpha_antialiasing);

	ClassDB::bind_method(D_METHOD("set_alpha_antialiasing_edge", "edge"), &Label3D::set_alpha_antialiasing_edge);
	ClassDB::bind_method(D_METHOD("get_alpha_antialiasing_edge"), &Label3D::get_alpha_antialiasing_edge);

	ClassDB::bind_method(D_METHOD("set_texture_filter", "mode"), &Label3D::set_texture_filter);
	ClassDB::bind_method(D_METHOD("get_texture_filter"), &Label3D::get_texture_filter);

	ClassDB::bind_method(D_METHOD("generate_triangle_mesh"), &Label3D::generate_triangle_mesh);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pixel_size", PROPERTY_HINT_RANGE, "0.0001,128,0.0001,suffix:m"), "set_pixel_size", "get_pixel_size");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "offset", PROPERTY_HINT_NONE, "suffix:px"), "set_offset", "get_offset");

	ADD_GROUP("Flags", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "billboard", PROPERTY_HINT_ENUM, "Disabled,Enabled,Y-Billboard"), "set_billboard_mode", "get_billboard_mode");
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "shaded"), "set_draw_flag", "get_draw_flag", FLAG_SHADED);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "double_sided"), "set_draw_flag", "get_draw_flag", FLAG_DOUBLE_SIDED);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "no_depth_test"), "set_draw_flag", "get_draw_flag", FLAG_DISABLE_DEPTH_TEST);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "fixed_size"), "set_draw_flag", "get_draw_flag", FLAG_FIXED_SIZE);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "alpha_cut", PROPERTY_HINT_ENUM, "Disabled,Discard,Opaque Pre-Pass,Alpha Hash"), "set_alpha_cut_mode", "get_alpha_cut_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "alpha_scissor_threshold", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_alpha_scissor_threshold", "get_alpha_scissor_threshold");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "alpha_hash_scale", PROPERTY_HINT_RANGE, "0,2,0.01"), "set_alpha_hash_scale", "get_alpha_hash_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "alpha_antialiasing_mode", PROPERTY_HINT_ENUM, "Disabled,Alpha Edge Blend,Alpha Edge Clip"), "set_alpha_antialiasing", "get_alpha_antialiasing");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "alpha_antialiasing_edge", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_alpha_antialiasing_edge", "get_alpha_antialiasing_edge");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_filter", PROPERTY_HINT_ENUM, "Nearest,Linear,Nearest Mipmap,Linear Mipmap,Nearest Mipmap Anisotropic,Linear Mipmap Anisotropic"), "set_texture_filter", "get_texture_filter");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "render_priority", PROPERTY_HINT_RANGE, itos(RS::MATERIAL_RENDER_PRIORITY_MIN) + "," + itos(RS::MATERIAL_RENDER_PRIORITY_MAX) + ",1"), "set_render_priority", "get_render_priority");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "outline_render_priority", PROPERTY_HINT_RANGE, itos(RS::MATERIAL_RENDER_PRIORITY_MIN) + "," + itos(RS::MATERIAL_RENDER_PRIORITY_MAX) + ",1"), "set_outline_render_priority", "get_outline_render_priority");

	ADD_GROUP("Text", "");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "modulate"), "set_modulate", "get_modulate");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "outline_modulate"), "set_outline_modulate", "get_outline_modulate");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text", PROPERTY_HINT_MULTILINE_TEXT, ""), "set_text", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "font", PROPERTY_HINT_RESOURCE_TYPE, "Font"), "set_font", "get_font");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "font_size", PROPERTY_HINT_RANGE, "1,256,1,or_greater,suffix:px"), "set_font_size", "get_font_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "outline_size", PROPERTY_HINT_RANGE, "0,127,1,suffix:px"), "set_outline_size", "get_outline_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "horizontal_alignment", PROPERTY_HINT_ENUM, "Left,Center,Right,Fill"), "set_horizontal_alignment", "get_horizontal_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "vertical_alignment", PROPERTY_HINT_ENUM, "Top,Center,Bottom"), "set_vertical_alignment", "get_vertical_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "uppercase"), "set_uppercase", "is_uppercase");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "line_spacing", PROPERTY_HINT_NONE, "suffix:px"), "set_line_spacing", "get_line_spacing");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "autowrap_mode", PROPERTY_HINT_ENUM, "Off,Arbitrary,Word,Word (Smart)"), "set_autowrap_mode", "get_autowrap_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "justification_flags", PROPERTY_HINT_FLAGS, "Kashida Justification:1,Word Justification:2,Justify Only After Last Tab:8,Skip Last Line:32,Skip Last Line With Visible Characters:64,Do Not Skip Single Line:128"), "set_justification_flags", "get_justification_flags");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "width", PROPERTY_HINT_NONE, "suffix:px"), "set_width", "get_width");

	ADD_GROUP("BiDi", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "text_direction", PROPERTY_HINT_ENUM, "Auto,Left-to-Right,Right-to-Left"), "set_text_direction", "get_text_direction");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "language", PROPERTY_HINT_LOCALE_ID, ""), "set_language", "get_language");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "structured_text_bidi_override", PROPERTY_HINT_ENUM, "Default,URI,File,Email,List,None,Custom"), "set_structured_text_bidi_override", "get_structured_text_bidi_override");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "structured_text_bidi_override_options"), "set_structured_text_bidi_override_options", "get_structured_text_bidi_override_options");

	BIND_ENUM_CONSTANT(FLAG_SHADED);
	BIND_ENUM_CONSTANT(FLAG_DOUBLE_SIDED);
	BIND_ENUM_CONSTANT(FLAG_DISABLE_DEPTH_TEST);
	BIND_ENUM_CONSTANT(FLAG_FIXED_SIZE);
	BIND_ENUM_CONSTANT(FLAG_MAX);

	BIND_ENUM_CONSTANT(ALPHA_CUT_DISABLED);
	BIND_ENUM_CONSTANT(ALPHA_CUT_DISCARD);
	BIND_ENUM_CONSTANT(ALPHA_CUT_OPAQUE_PREPASS);
	BIND_ENUM_CONSTANT(ALPHA_CUT_HASH);
}

void Label3D::_validate_property(PropertyInfo &p_property) const {
	if (
			p_property.name == "material_override" ||
			p_property.name == "material_overlay" ||
			p_property.name == "lod_bias" ||
			p_property.name == "gi_mode" ||
			p_property.name == "gi_lightmap_scale") {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}

	if (p_property.name == "cast_shadow" && alpha_cut == ALPHA_CUT_DISABLED) {
		// Alpha-blended materials can't cast shadows.
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

void Label3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (!pending_update) {
				_im_update();
			}
			Window *window = get_window();
			ERR_FAIL_NULL(window);
			window->connect("size_changed", callable_mp(this, &Label3D::_font_changed));
		} break;
		case NOTIFICATION_EXIT_TREE: {
			Window *window = get_window();
			ERR_FAIL_NULL(window);
			window->disconnect("size_changed", callable_mp(this, &Label3D::_font_changed));
		} break;
		case NOTIFICATION_TRANSLATION_CHANGED: {
			String new_text = atr(text);
			if (new_text == xl_text) {
				return; // Nothing new.
			}
			xl_text = new_text;
			dirty_text = true;
			_queue_update();
		} break;
	}
}

void Label3D::_im_update() {
	_shape();

	triangle_mesh.unref();
	update_gizmos();

	pending_update = false;
}

void Label3D::_queue_update() {
	if (pending_update) {
		return;
	}

	pending_update = true;
	callable_mp(this, &Label3D::_im_update).call_deferred();
}

AABB Label3D::get_aabb() const {
	return aabb;
}

Ref<TriangleMesh> Label3D::generate_triangle_mesh() const {
	if (triangle_mesh.is_valid()) {
		return triangle_mesh;
	}

	Ref<Font> font = _get_font_or_default();
	if (font.is_null()) {
		return Ref<TriangleMesh>();
	}

	Vector<Vector3> faces;
	faces.resize(6);
	Vector3 *facesw = faces.ptrw();

	float total_h = 0.0;
	float max_line_w = 0.0;
	for (int i = 0; i < lines_rid.size(); i++) {
		total_h += TS->shaped_text_get_size(lines_rid[i]).y + line_spacing;
		max_line_w = MAX(max_line_w, TS->shaped_text_get_width(lines_rid[i]));
	}

	float vbegin = 0;
	switch (vertical_alignment) {
		case VERTICAL_ALIGNMENT_FILL:
		case VERTICAL_ALIGNMENT_TOP: {
			// Nothing.
		} break;
		case VERTICAL_ALIGNMENT_CENTER: {
			vbegin = (total_h - line_spacing) / 2.0;
		} break;
		case VERTICAL_ALIGNMENT_BOTTOM: {
			vbegin = (total_h - line_spacing);
		} break;
	}

	Vector2 offset = Vector2(0, vbegin);
	switch (horizontal_alignment) {
		case HORIZONTAL_ALIGNMENT_LEFT:
			break;
		case HORIZONTAL_ALIGNMENT_FILL:
		case HORIZONTAL_ALIGNMENT_CENTER: {
			offset.x = -max_line_w / 2.0;
		} break;
		case HORIZONTAL_ALIGNMENT_RIGHT: {
			offset.x = -max_line_w;
		} break;
	}

	Rect2 final_rect = Rect2(offset + lbl_offset, Size2(max_line_w, total_h));

	if (final_rect.size.x == 0 || final_rect.size.y == 0) {
		return Ref<TriangleMesh>();
	}

	real_t px_size = get_pixel_size();

	Vector2 vertices[4] = {

		(final_rect.position + Vector2(0, -final_rect.size.y)) * px_size,
		(final_rect.position + Vector2(final_rect.size.x, -final_rect.size.y)) * px_size,
		(final_rect.position + Vector2(final_rect.size.x, 0)) * px_size,
		final_rect.position * px_size,

	};

	static const int indices[6] = {
		0, 1, 2,
		0, 2, 3
	};

	for (int j = 0; j < 6; j++) {
		int i = indices[j];
		Vector3 vtx;
		vtx[0] = vertices[i][0];
		vtx[1] = vertices[i][1];
		facesw[j] = vtx;
	}

	triangle_mesh.instantiate();
	triangle_mesh->create(faces);

	return triangle_mesh;
}

void Label3D::_generate_glyph_surfaces(const Glyph &p_glyph, Vector2 &r_offset, const Color &p_modulate, int p_priority, int p_outline_size) {
	if (p_glyph.index == 0) {
		r_offset.x += p_glyph.advance * pixel_size * p_glyph.repeat; // Non visual character, skip.
		return;
	}

	Vector2 gl_of;
	Vector2 gl_sz;
	Rect2 gl_uv;
	Size2 texs;
	RID tex;

	if (p_glyph.font_rid.is_valid()) {
		tex = TS->font_get_glyph_texture_rid(p_glyph.font_rid, Vector2i(p_glyph.font_size, p_outline_size), p_glyph.index);
		if (tex.is_valid()) {
			gl_of = (TS->font_get_glyph_offset(p_glyph.font_rid, Vector2i(p_glyph.font_size, p_outline_size), p_glyph.index) + Vector2(p_glyph.x_off, p_glyph.y_off)) * pixel_size;
			gl_sz = TS->font_get_glyph_size(p_glyph.font_rid, Vector2i(p_glyph.font_size, p_outline_size), p_glyph.index) * pixel_size;
			gl_uv = TS->font_get_glyph_uv_rect(p_glyph.font_rid, Vector2i(p_glyph.font_size, p_outline_size), p_glyph.index);
			texs = TS->font_get_glyph_texture_size(p_glyph.font_rid, Vector2i(p_glyph.font_size, p_outline_size), p_glyph.index);
		}
	} else if (((p_glyph.flags & TextServer::GRAPHEME_IS_VIRTUAL) != TextServer::GRAPHEME_IS_VIRTUAL) && ((p_glyph.flags & TextServer::GRAPHEME_IS_EMBEDDED_OBJECT) != TextServer::GRAPHEME_IS_EMBEDDED_OBJECT)) {
		gl_sz = TS->get_hex_code_box_size(p_glyph.font_size, p_glyph.index) * pixel_size;
		gl_of = Vector2(0, -gl_sz.y);
	}

	if (gl_uv.size.x <= 2 || gl_uv.size.y <= 2) {
		r_offset.x += p_glyph.advance * pixel_size * p_glyph.repeat; // Nothing to draw.
		return;
	}

	bool msdf = TS->font_is_multichannel_signed_distance_field(p_glyph.font_rid);

	for (int j = 0; j < p_glyph.repeat; j++) {
		SurfaceKey key = SurfaceKey(tex.get_id(), p_priority, p_outline_size);
		if (!surfaces.has(key)) {
			SurfaceData surf;
			surf.material = RenderingServer::get_singleton()->material_create();
			// Set defaults for material, names need to match up those in StandardMaterial3D
			RS::get_singleton()->material_set_param(surf.material, "albedo", Color(1, 1, 1, 1));
			RS::get_singleton()->material_set_param(surf.material, "specular", 0.5);
			RS::get_singleton()->material_set_param(surf.material, "metallic", 0.0);
			RS::get_singleton()->material_set_param(surf.material, "roughness", 1.0);
			RS::get_singleton()->material_set_param(surf.material, "uv1_offset", Vector3(0, 0, 0));
			RS::get_singleton()->material_set_param(surf.material, "uv1_scale", Vector3(1, 1, 1));
			RS::get_singleton()->material_set_param(surf.material, "uv2_offset", Vector3(0, 0, 0));
			RS::get_singleton()->material_set_param(surf.material, "uv2_scale", Vector3(1, 1, 1));
			RS::get_singleton()->material_set_param(surf.material, "alpha_scissor_threshold", alpha_scissor_threshold);
			RS::get_singleton()->material_set_param(surf.material, "alpha_hash_scale", alpha_hash_scale);
			RS::get_singleton()->material_set_param(surf.material, "alpha_antialiasing_edge", alpha_antialiasing_edge);
			if (msdf) {
				RS::get_singleton()->material_set_param(surf.material, "msdf_pixel_range", TS->font_get_msdf_pixel_range(p_glyph.font_rid));
				RS::get_singleton()->material_set_param(surf.material, "msdf_outline_size", p_outline_size);
			}

			BaseMaterial3D::Transparency mat_transparency = BaseMaterial3D::Transparency::TRANSPARENCY_ALPHA;
			if (get_alpha_cut_mode() == ALPHA_CUT_DISCARD) {
				mat_transparency = BaseMaterial3D::Transparency::TRANSPARENCY_ALPHA_SCISSOR;
			} else if (get_alpha_cut_mode() == ALPHA_CUT_OPAQUE_PREPASS) {
				mat_transparency = BaseMaterial3D::Transparency::TRANSPARENCY_ALPHA_DEPTH_PRE_PASS;
			} else if (get_alpha_cut_mode() == ALPHA_CUT_HASH) {
				mat_transparency = BaseMaterial3D::Transparency::TRANSPARENCY_ALPHA_HASH;
			}

			RID shader_rid;
			StandardMaterial3D::get_material_for_2d(get_draw_flag(FLAG_SHADED), mat_transparency, get_draw_flag(FLAG_DOUBLE_SIDED), get_billboard_mode() == StandardMaterial3D::BILLBOARD_ENABLED, get_billboard_mode() == StandardMaterial3D::BILLBOARD_FIXED_Y, msdf, get_draw_flag(FLAG_DISABLE_DEPTH_TEST), get_draw_flag(FLAG_FIXED_SIZE), texture_filter, alpha_antialiasing_mode, &shader_rid);

			RS::get_singleton()->material_set_shader(surf.material, shader_rid);
			RS::get_singleton()->material_set_param(surf.material, "texture_albedo", tex);
			if (get_alpha_cut_mode() == ALPHA_CUT_DISABLED) {
				RS::get_singleton()->material_set_render_priority(surf.material, p_priority);
			} else {
				surf.z_shift = p_priority * pixel_size;
			}

			surfaces[key] = surf;
		}
		SurfaceData &s = surfaces[key];

		s.mesh_vertices.resize((s.offset + 1) * 4);
		s.mesh_normals.resize((s.offset + 1) * 4);
		s.mesh_tangents.resize((s.offset + 1) * 16);
		s.mesh_colors.resize((s.offset + 1) * 4);
		s.mesh_uvs.resize((s.offset + 1) * 4);

		s.mesh_vertices.write[(s.offset * 4) + 3] = Vector3(r_offset.x + gl_of.x, r_offset.y - gl_of.y - gl_sz.y, s.z_shift);
		s.mesh_vertices.write[(s.offset * 4) + 2] = Vector3(r_offset.x + gl_of.x + gl_sz.x, r_offset.y - gl_of.y - gl_sz.y, s.z_shift);
		s.mesh_vertices.write[(s.offset * 4) + 1] = Vector3(r_offset.x + gl_of.x + gl_sz.x, r_offset.y - gl_of.y, s.z_shift);
		s.mesh_vertices.write[(s.offset * 4) + 0] = Vector3(r_offset.x + gl_of.x, r_offset.y - gl_of.y, s.z_shift);

		for (int i = 0; i < 4; i++) {
			s.mesh_normals.write[(s.offset * 4) + i] = Vector3(0.0, 0.0, 1.0);
			s.mesh_tangents.write[(s.offset * 16) + (i * 4) + 0] = 1.0;
			s.mesh_tangents.write[(s.offset * 16) + (i * 4) + 1] = 0.0;
			s.mesh_tangents.write[(s.offset * 16) + (i * 4) + 2] = 0.0;
			s.mesh_tangents.write[(s.offset * 16) + (i * 4) + 3] = 1.0;
			s.mesh_colors.write[(s.offset * 4) + i] = p_modulate;
			s.mesh_uvs.write[(s.offset * 4) + i] = Vector2();
		}

		if (tex.is_valid()) {
			s.mesh_uvs.write[(s.offset * 4) + 3] = Vector2(gl_uv.position.x / texs.x, (gl_uv.position.y + gl_uv.size.y) / texs.y);
			s.mesh_uvs.write[(s.offset * 4) + 2] = Vector2((gl_uv.position.x + gl_uv.size.x) / texs.x, (gl_uv.position.y + gl_uv.size.y) / texs.y);
			s.mesh_uvs.write[(s.offset * 4) + 1] = Vector2((gl_uv.position.x + gl_uv.size.x) / texs.x, gl_uv.position.y / texs.y);
			s.mesh_uvs.write[(s.offset * 4) + 0] = Vector2(gl_uv.position.x / texs.x, gl_uv.position.y / texs.y);
		}

		s.indices.resize((s.offset + 1) * 6);
		s.indices.write[(s.offset * 6) + 0] = (s.offset * 4) + 0;
		s.indices.write[(s.offset * 6) + 1] = (s.offset * 4) + 1;
		s.indices.write[(s.offset * 6) + 2] = (s.offset * 4) + 2;
		s.indices.write[(s.offset * 6) + 3] = (s.offset * 4) + 0;
		s.indices.write[(s.offset * 6) + 4] = (s.offset * 4) + 2;
		s.indices.write[(s.offset * 6) + 5] = (s.offset * 4) + 3;

		s.offset++;
		r_offset.x += p_glyph.advance * pixel_size;
	}
}

void Label3D::_shape() {
	// When a shaped text is invalidated by an external source, we want to reshape it.
	if (!TS->shaped_text_is_ready(text_rid)) {
		dirty_text = true;
	}

	for (const RID &line_rid : lines_rid) {
		if (!TS->shaped_text_is_ready(line_rid)) {
			dirty_lines = true;
			break;
		}
	}

	// Clear mesh.
	RS::get_singleton()->mesh_clear(mesh);
	aabb = AABB();

	// Clear materials.
	for (const KeyValue<SurfaceKey, SurfaceData> &E : surfaces) {
		RenderingServer::get_singleton()->free(E.value.material);
	}
	surfaces.clear();

	Ref<Font> font = _get_font_or_default();
	ERR_FAIL_COND(font.is_null());

	// Update text buffer.
	if (dirty_text) {
		TS->shaped_text_clear(text_rid);
		TS->shaped_text_set_direction(text_rid, text_direction);

		String txt = (uppercase) ? TS->string_to_upper(xl_text, language) : xl_text;
		TS->shaped_text_add_string(text_rid, txt, font->get_rids(), font_size, font->get_opentype_features(), language);

		TypedArray<Vector3i> stt;
		if (st_parser == TextServer::STRUCTURED_TEXT_CUSTOM) {
			GDVIRTUAL_CALL(_structured_text_parser, st_args, txt, stt);
		} else {
			stt = TS->parse_structured_text(st_parser, st_args, txt);
		}
		TS->shaped_text_set_bidi_override(text_rid, stt);

		dirty_text = false;
		dirty_font = false;
		dirty_lines = true;
	} else if (dirty_font) {
		int spans = TS->shaped_get_span_count(text_rid);
		for (int i = 0; i < spans; i++) {
			TS->shaped_set_span_update_font(text_rid, i, font->get_rids(), font_size, font->get_opentype_features());
		}

		dirty_font = false;
		dirty_lines = true;
	}

	if (dirty_lines) {
		for (int i = 0; i < lines_rid.size(); i++) {
			TS->free_rid(lines_rid[i]);
		}
		lines_rid.clear();

		BitField<TextServer::LineBreakFlag> autowrap_flags = TextServer::BREAK_MANDATORY;
		switch (autowrap_mode) {
			case TextServer::AUTOWRAP_WORD_SMART:
				autowrap_flags = TextServer::BREAK_WORD_BOUND | TextServer::BREAK_ADAPTIVE | TextServer::BREAK_MANDATORY;
				break;
			case TextServer::AUTOWRAP_WORD:
				autowrap_flags = TextServer::BREAK_WORD_BOUND | TextServer::BREAK_MANDATORY;
				break;
			case TextServer::AUTOWRAP_ARBITRARY:
				autowrap_flags = TextServer::BREAK_GRAPHEME_BOUND | TextServer::BREAK_MANDATORY;
				break;
			case TextServer::AUTOWRAP_OFF:
				break;
		}
		autowrap_flags = autowrap_flags | TextServer::BREAK_TRIM_EDGE_SPACES;

		PackedInt32Array line_breaks = TS->shaped_text_get_line_breaks(text_rid, width, 0, autowrap_flags);
		float max_line_w = 0.0;
		for (int i = 0; i < line_breaks.size(); i = i + 2) {
			RID line = TS->shaped_text_substr(text_rid, line_breaks[i], line_breaks[i + 1] - line_breaks[i]);
			max_line_w = MAX(max_line_w, TS->shaped_text_get_width(line));
			lines_rid.push_back(line);
		}

		if (horizontal_alignment == HORIZONTAL_ALIGNMENT_FILL) {
			int jst_to_line = lines_rid.size();
			if (lines_rid.size() == 1 && jst_flags.has_flag(TextServer::JUSTIFICATION_DO_NOT_SKIP_SINGLE_LINE)) {
				jst_to_line = lines_rid.size();
			} else {
				if (jst_flags.has_flag(TextServer::JUSTIFICATION_SKIP_LAST_LINE)) {
					jst_to_line = lines_rid.size() - 1;
				}
				if (jst_flags.has_flag(TextServer::JUSTIFICATION_SKIP_LAST_LINE_WITH_VISIBLE_CHARS)) {
					for (int i = lines_rid.size() - 1; i >= 0; i--) {
						if (TS->shaped_text_has_visible_chars(lines_rid[i])) {
							jst_to_line = i;
							break;
						}
					}
				}
			}
			for (int i = 0; i < jst_to_line; i++) {
				TS->shaped_text_fit_to_width(lines_rid[i], (width > 0) ? width : max_line_w, jst_flags);
			}
		}
		dirty_lines = false;
	}

	// Generate surfaces and materials.
	float total_h = 0.0;
	for (int i = 0; i < lines_rid.size(); i++) {
		total_h += (TS->shaped_text_get_size(lines_rid[i]).y + line_spacing) * pixel_size;
	}

	float vbegin = 0.0;
	switch (vertical_alignment) {
		case VERTICAL_ALIGNMENT_FILL:
		case VERTICAL_ALIGNMENT_TOP: {
			// Nothing.
		} break;
		case VERTICAL_ALIGNMENT_CENTER: {
			vbegin = (total_h - line_spacing * pixel_size) / 2.0;
		} break;
		case VERTICAL_ALIGNMENT_BOTTOM: {
			vbegin = (total_h - line_spacing * pixel_size);
		} break;
	}

	Vector2 offset = Vector2(0, vbegin + lbl_offset.y * pixel_size);
	for (int i = 0; i < lines_rid.size(); i++) {
		const Glyph *glyphs = TS->shaped_text_get_glyphs(lines_rid[i]);
		int gl_size = TS->shaped_text_get_glyph_count(lines_rid[i]);
		float line_width = TS->shaped_text_get_width(lines_rid[i]) * pixel_size;

		switch (horizontal_alignment) {
			case HORIZONTAL_ALIGNMENT_LEFT:
				offset.x = 0.0;
				break;
			case HORIZONTAL_ALIGNMENT_FILL:
			case HORIZONTAL_ALIGNMENT_CENTER: {
				offset.x = -line_width / 2.0;
			} break;
			case HORIZONTAL_ALIGNMENT_RIGHT: {
				offset.x = -line_width;
			} break;
		}
		offset.x += lbl_offset.x * pixel_size;
		if (aabb == AABB()) {
			aabb.position = Vector3(offset.x, offset.y, 0);
			aabb.expand_to(Vector3(offset.x + line_width, offset.y - (TS->shaped_text_get_size(lines_rid[i]).y + line_spacing) * pixel_size, 0));
		} else {
			aabb.expand_to(Vector3(offset.x, offset.y, 0));
			aabb.expand_to(Vector3(offset.x + line_width, offset.y - (TS->shaped_text_get_size(lines_rid[i]).y + line_spacing) * pixel_size, 0));
		}
		offset.y -= TS->shaped_text_get_ascent(lines_rid[i]) * pixel_size;

		if (outline_modulate.a != 0.0 && outline_size > 0) {
			// Outline surfaces.
			Vector2 ol_offset = offset;
			for (int j = 0; j < gl_size; j++) {
				_generate_glyph_surfaces(glyphs[j], ol_offset, outline_modulate, outline_render_priority, outline_size);
			}
		}

		// Main text surfaces.
		for (int j = 0; j < gl_size; j++) {
			_generate_glyph_surfaces(glyphs[j], offset, modulate, render_priority);
		}
		offset.y -= (TS->shaped_text_get_descent(lines_rid[i]) + line_spacing) * pixel_size;
	}

	for (const KeyValue<SurfaceKey, SurfaceData> &E : surfaces) {
		Array mesh_array;
		mesh_array.resize(RS::ARRAY_MAX);
		mesh_array[RS::ARRAY_VERTEX] = E.value.mesh_vertices;
		mesh_array[RS::ARRAY_NORMAL] = E.value.mesh_normals;
		mesh_array[RS::ARRAY_TANGENT] = E.value.mesh_tangents;
		mesh_array[RS::ARRAY_COLOR] = E.value.mesh_colors;
		mesh_array[RS::ARRAY_TEX_UV] = E.value.mesh_uvs;
		mesh_array[RS::ARRAY_INDEX] = E.value.indices;

		RS::SurfaceData sd;
		RS::get_singleton()->mesh_create_surface_data_from_arrays(&sd, RS::PRIMITIVE_TRIANGLES, mesh_array);

		sd.material = E.value.material;

		RS::get_singleton()->mesh_add_surface(mesh, sd);
	}
}

void Label3D::set_text(const String &p_string) {
	if (text == p_string) {
		return;
	}

	text = p_string;
	xl_text = atr(p_string);
	dirty_text = true;
	_queue_update();
}

String Label3D::get_text() const {
	return text;
}

void Label3D::set_horizontal_alignment(HorizontalAlignment p_alignment) {
	ERR_FAIL_INDEX((int)p_alignment, 4);
	if (horizontal_alignment != p_alignment) {
		if (horizontal_alignment == HORIZONTAL_ALIGNMENT_FILL || p_alignment == HORIZONTAL_ALIGNMENT_FILL) {
			dirty_lines = true; // Reshape lines.
		}
		horizontal_alignment = p_alignment;
		_queue_update();
	}
}

HorizontalAlignment Label3D::get_horizontal_alignment() const {
	return horizontal_alignment;
}

void Label3D::set_vertical_alignment(VerticalAlignment p_alignment) {
	ERR_FAIL_INDEX((int)p_alignment, 4);
	if (vertical_alignment != p_alignment) {
		vertical_alignment = p_alignment;
		_queue_update();
	}
}

VerticalAlignment Label3D::get_vertical_alignment() const {
	return vertical_alignment;
}

void Label3D::set_text_direction(TextServer::Direction p_text_direction) {
	ERR_FAIL_COND((int)p_text_direction < -1 || (int)p_text_direction > 3);
	if (text_direction != p_text_direction) {
		text_direction = p_text_direction;
		dirty_text = true;
		_queue_update();
	}
}

TextServer::Direction Label3D::get_text_direction() const {
	return text_direction;
}

void Label3D::set_language(const String &p_language) {
	if (language != p_language) {
		language = p_language;
		dirty_text = true;
		_queue_update();
	}
}

String Label3D::get_language() const {
	return language;
}

void Label3D::set_structured_text_bidi_override(TextServer::StructuredTextParser p_parser) {
	if (st_parser != p_parser) {
		st_parser = p_parser;
		dirty_text = true;
		_queue_update();
	}
}

TextServer::StructuredTextParser Label3D::get_structured_text_bidi_override() const {
	return st_parser;
}

void Label3D::set_structured_text_bidi_override_options(Array p_args) {
	if (st_args != p_args) {
		st_args = p_args;
		dirty_text = true;
		_queue_update();
	}
}

Array Label3D::get_structured_text_bidi_override_options() const {
	return st_args;
}

void Label3D::set_uppercase(bool p_uppercase) {
	if (uppercase != p_uppercase) {
		uppercase = p_uppercase;
		dirty_text = true;
		_queue_update();
	}
}

bool Label3D::is_uppercase() const {
	return uppercase;
}

void Label3D::set_render_priority(int p_priority) {
	ERR_FAIL_COND(p_priority < RS::MATERIAL_RENDER_PRIORITY_MIN || p_priority > RS::MATERIAL_RENDER_PRIORITY_MAX);
	if (render_priority != p_priority) {
		render_priority = p_priority;
		_queue_update();
	}
}

int Label3D::get_render_priority() const {
	return render_priority;
}

void Label3D::set_outline_render_priority(int p_priority) {
	ERR_FAIL_COND(p_priority < RS::MATERIAL_RENDER_PRIORITY_MIN || p_priority > RS::MATERIAL_RENDER_PRIORITY_MAX);
	if (outline_render_priority != p_priority) {
		outline_render_priority = p_priority;
		_queue_update();
	}
}

int Label3D::get_outline_render_priority() const {
	return outline_render_priority;
}

void Label3D::_font_changed() {
	dirty_font = true;
	_queue_update();
}

void Label3D::set_font(const Ref<Font> &p_font) {
	if (font_override != p_font) {
		if (font_override.is_valid()) {
			font_override->disconnect_changed(callable_mp(this, &Label3D::_font_changed));
		}
		font_override = p_font;
		dirty_font = true;
		if (font_override.is_valid()) {
			font_override->connect_changed(callable_mp(this, &Label3D::_font_changed));
		}
		_queue_update();
	}
}

Ref<Font> Label3D::get_font() const {
	return font_override;
}

Ref<Font> Label3D::_get_font_or_default() const {
	// Similar code taken from `FontVariation::_get_base_font_or_default`.

	if (theme_font.is_valid()) {
		theme_font->disconnect_changed(callable_mp(const_cast<Label3D *>(this), &Label3D::_font_changed));
		theme_font.unref();
	}

	if (font_override.is_valid()) {
		return font_override;
	}

	const StringName theme_name = SceneStringName(font);
	Vector<StringName> theme_types;
	ThemeDB::get_singleton()->get_native_type_dependencies(get_class_name(), theme_types);

	ThemeContext *global_context = ThemeDB::get_singleton()->get_default_theme_context();
	Vector<Ref<Theme>> themes = global_context->get_themes();
	if (Engine::get_singleton()->is_editor_hint()) {
		themes.insert(0, ThemeDB::get_singleton()->get_project_theme());
	}

	for (const Ref<Theme> &theme : themes) {
		if (theme.is_null()) {
			continue;
		}

		for (const StringName &E : theme_types) {
			if (!theme->has_font(theme_name, E)) {
				continue;
			}

			Ref<Font> f = theme->get_font(theme_name, E);
			if (f.is_valid()) {
				theme_font = f;
				theme_font->connect_changed(callable_mp(const_cast<Label3D *>(this), &Label3D::_font_changed));
			}
			return f;
		}
	}

	Ref<Font> f = global_context->get_fallback_theme()->get_font(theme_name, StringName());
	if (f.is_valid()) {
		theme_font = f;
		theme_font->connect_changed(callable_mp(const_cast<Label3D *>(this), &Label3D::_font_changed));
	}
	return f;
}

void Label3D::set_font_size(int p_size) {
	if (font_size != p_size) {
		font_size = p_size;
		dirty_font = true;
		_queue_update();
	}
}

int Label3D::get_font_size() const {
	return font_size;
}

void Label3D::set_outline_size(int p_size) {
	if (outline_size != p_size) {
		outline_size = p_size;
		_queue_update();
	}
}

int Label3D::get_outline_size() const {
	return outline_size;
}

void Label3D::set_modulate(const Color &p_color) {
	if (modulate != p_color) {
		modulate = p_color;
		_queue_update();
	}
}

Color Label3D::get_modulate() const {
	return modulate;
}

void Label3D::set_outline_modulate(const Color &p_color) {
	if (outline_modulate != p_color) {
		outline_modulate = p_color;
		_queue_update();
	}
}

Color Label3D::get_outline_modulate() const {
	return outline_modulate;
}

void Label3D::set_autowrap_mode(TextServer::AutowrapMode p_mode) {
	if (autowrap_mode != p_mode) {
		autowrap_mode = p_mode;
		dirty_lines = true;
		_queue_update();
	}
}

TextServer::AutowrapMode Label3D::get_autowrap_mode() const {
	return autowrap_mode;
}

void Label3D::set_justification_flags(BitField<TextServer::JustificationFlag> p_flags) {
	if (jst_flags != p_flags) {
		jst_flags = p_flags;
		dirty_lines = true;
		_queue_update();
	}
}

BitField<TextServer::JustificationFlag> Label3D::get_justification_flags() const {
	return jst_flags;
}

void Label3D::set_width(float p_width) {
	if (width != p_width) {
		width = p_width;
		dirty_lines = true;
		_queue_update();
	}
}

float Label3D::get_width() const {
	return width;
}

void Label3D::set_pixel_size(real_t p_amount) {
	if (pixel_size != p_amount) {
		pixel_size = p_amount;
		_queue_update();
	}
}

real_t Label3D::get_pixel_size() const {
	return pixel_size;
}

void Label3D::set_offset(const Point2 &p_offset) {
	if (lbl_offset != p_offset) {
		lbl_offset = p_offset;
		_queue_update();
	}
}

Point2 Label3D::get_offset() const {
	return lbl_offset;
}

void Label3D::set_line_spacing(float p_line_spacing) {
	if (line_spacing != p_line_spacing) {
		line_spacing = p_line_spacing;
		_queue_update();
	}
}

float Label3D::get_line_spacing() const {
	return line_spacing;
}

void Label3D::set_draw_flag(DrawFlags p_flag, bool p_enable) {
	ERR_FAIL_INDEX(p_flag, FLAG_MAX);
	if (flags[p_flag] != p_enable) {
		flags[p_flag] = p_enable;
		_queue_update();
	}
}

bool Label3D::get_draw_flag(DrawFlags p_flag) const {
	ERR_FAIL_INDEX_V(p_flag, FLAG_MAX, false);
	return flags[p_flag];
}

void Label3D::set_billboard_mode(StandardMaterial3D::BillboardMode p_mode) {
	ERR_FAIL_INDEX(p_mode, 3);
	if (billboard_mode != p_mode) {
		billboard_mode = p_mode;
		_queue_update();
	}
}

StandardMaterial3D::BillboardMode Label3D::get_billboard_mode() const {
	return billboard_mode;
}

void Label3D::set_alpha_cut_mode(AlphaCutMode p_mode) {
	ERR_FAIL_INDEX(p_mode, ALPHA_CUT_MAX);
	if (alpha_cut != p_mode) {
		alpha_cut = p_mode;
		_queue_update();
		notify_property_list_changed();
	}
}

void Label3D::set_texture_filter(StandardMaterial3D::TextureFilter p_filter) {
	if (texture_filter != p_filter) {
		texture_filter = p_filter;
		_queue_update();
	}
}

StandardMaterial3D::TextureFilter Label3D::get_texture_filter() const {
	return texture_filter;
}

Label3D::AlphaCutMode Label3D::get_alpha_cut_mode() const {
	return alpha_cut;
}

void Label3D::set_alpha_hash_scale(float p_hash_scale) {
	if (alpha_hash_scale != p_hash_scale) {
		alpha_hash_scale = p_hash_scale;
		_queue_update();
	}
}

float Label3D::get_alpha_hash_scale() const {
	return alpha_hash_scale;
}

void Label3D::set_alpha_scissor_threshold(float p_threshold) {
	if (alpha_scissor_threshold != p_threshold) {
		alpha_scissor_threshold = p_threshold;
		_queue_update();
	}
}

float Label3D::get_alpha_scissor_threshold() const {
	return alpha_scissor_threshold;
}

void Label3D::set_alpha_antialiasing(BaseMaterial3D::AlphaAntiAliasing p_alpha_aa) {
	if (alpha_antialiasing_mode != p_alpha_aa) {
		alpha_antialiasing_mode = p_alpha_aa;
		_queue_update();
	}
}

BaseMaterial3D::AlphaAntiAliasing Label3D::get_alpha_antialiasing() const {
	return alpha_antialiasing_mode;
}

void Label3D::set_alpha_antialiasing_edge(float p_edge) {
	if (alpha_antialiasing_edge != p_edge) {
		alpha_antialiasing_edge = p_edge;
		_queue_update();
	}
}

float Label3D::get_alpha_antialiasing_edge() const {
	return alpha_antialiasing_edge;
}

Label3D::Label3D() {
	for (int i = 0; i < FLAG_MAX; i++) {
		flags[i] = (i == FLAG_DOUBLE_SIDED);
	}

	text_rid = TS->create_shaped_text();

	mesh = RenderingServer::get_singleton()->mesh_create();

	// Disable shadow casting by default to improve performance and avoid unintended visual artifacts.
	set_cast_shadows_setting(SHADOW_CASTING_SETTING_OFF);

	// Label3D can't contribute to GI in any way, so disable it to improve performance.
	set_gi_mode(GI_MODE_DISABLED);

	set_base(mesh);
}

Label3D::~Label3D() {
	for (int i = 0; i < lines_rid.size(); i++) {
		TS->free_rid(lines_rid[i]);
	}
	lines_rid.clear();

	TS->free_rid(text_rid);

	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RenderingServer::get_singleton()->free(mesh);
	for (KeyValue<SurfaceKey, SurfaceData> E : surfaces) {
		RenderingServer::get_singleton()->free(E.value.material);
	}
	surfaces.clear();
}
