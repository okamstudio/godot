/**************************************************************************/
/*  texture_region_editor_plugin.cpp                                      */
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

#include "texture_region_editor_plugin.h"

#include "core/input/input.h"
#include "core/os/keyboard.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/themes/editor_scale.h"
#include "scene/2d/sprite_2d.h"
#include "scene/3d/sprite_3d.h"
#include "scene/gui/nine_patch_rect.h"
#include "scene/gui/option_button.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/view_panner.h"
#include "scene/resources/atlas_texture.h"
#include "scene/resources/style_box_texture.h"

Transform2D TextureRegionEditor::_get_offset_transform() const {
	Transform2D mtx;
	mtx.columns[2] = -draw_ofs * draw_zoom;
	mtx.scale_basis(Vector2(draw_zoom, draw_zoom));

	return mtx;
}

void TextureRegionEditor::_texture_preview_draw() {
	const Ref<Texture2D> object_texture = _get_edited_object_texture();
	if (object_texture.is_null()) {
		return;
	}

	Transform2D mtx = _get_offset_transform();

	RS::get_singleton()->canvas_item_add_set_transform(texture_preview->get_canvas_item(), mtx);
	texture_preview->draw_rect(Rect2(Point2(), object_texture->get_size()), Color(0.5, 0.5, 0.5, 0.5), false);
	texture_preview->draw_texture(object_texture, Point2());
	RS::get_singleton()->canvas_item_add_set_transform(texture_preview->get_canvas_item(), Transform2D());
}

void TextureRegionEditor::_texture_overlay_draw() {
	const Ref<Texture2D> object_texture = _get_edited_object_texture();
	if (object_texture.is_null()) {
		return;
	}

	Transform2D mtx = _get_offset_transform();
	const Color color = get_theme_color(SNAME("mono_color"), EditorStringName(Editor));

	if (snap_mode == SNAP_GRID) {
		const Color grid_color = Color(color.r, color.g, color.b, color.a * 0.15);
		Size2 s = texture_overlay->get_size();
		int last_cell = 0;

		if (snap_step.x != 0) {
			if (snap_separation.x == 0) {
				for (int i = 0; i < s.width; i++) {
					int cell = Math::fast_ftoi(Math::floor((mtx.affine_inverse().xform(Vector2(i, 0)).x - snap_offset.x) / snap_step.x));
					if (i == 0) {
						last_cell = cell;
					}
					if (last_cell != cell) {
						texture_overlay->draw_line(Point2(i, 0), Point2(i, s.height), grid_color);
					}
					last_cell = cell;
				}
			} else {
				for (int i = 0; i < s.width + snap_separation.x; i++) {
					int cell = Math::fast_ftoi(Math::floor((mtx.affine_inverse().xform(Vector2(i, 0)).x - snap_offset.x) / (snap_step.x + snap_separation.x)));
					if (i == 0) {
						last_cell = cell;
					}
					if (last_cell != cell) {
						texture_overlay->draw_rect(Rect2(i - snap_separation.x * draw_zoom, 0, snap_separation.x * draw_zoom, s.height), grid_color);
					}
					last_cell = cell;
				}
			}
		}

		if (snap_step.y != 0) {
			if (snap_separation.y == 0) {
				for (int i = 0; i < s.height; i++) {
					int cell = Math::fast_ftoi(Math::floor((mtx.affine_inverse().xform(Vector2(0, i)).y - snap_offset.y) / snap_step.y));
					if (i == 0) {
						last_cell = cell;
					}
					if (last_cell != cell) {
						texture_overlay->draw_line(Point2(0, i), Point2(s.width, i), grid_color);
					}
					last_cell = cell;
				}
			} else {
				for (int i = 0; i < s.height + snap_separation.y; i++) {
					int cell = Math::fast_ftoi(Math::floor((mtx.affine_inverse().xform(Vector2(0, i)).y - snap_offset.y) / (snap_step.y + snap_separation.y)));
					if (i == 0) {
						last_cell = cell;
					}
					if (last_cell != cell) {
						texture_overlay->draw_rect(Rect2(0, i - snap_separation.y * draw_zoom, s.width, snap_separation.y * draw_zoom), grid_color);
					}
					last_cell = cell;
				}
			}
		}
	} else if (snap_mode == SNAP_AUTOSLICE) {
		for (const Rect2 &r : autoslice_cache) {
			const Vector2 endpoints[4] = {
				mtx.basis_xform(r.position),
				mtx.basis_xform(r.position + Vector2(r.size.x, 0)),
				mtx.basis_xform(r.position + r.size),
				mtx.basis_xform(r.position + Vector2(0, r.size.y))
			};
			for (int i = 0; i < 4; i++) {
				int next = (i + 1) % 4;
				texture_overlay->draw_line(endpoints[i] - draw_ofs * draw_zoom, endpoints[next] - draw_ofs * draw_zoom, Color(0.3, 0.7, 1, 1), 2);
			}
		}
	}

	Ref<Texture2D> select_handle = get_editor_theme_icon(SNAME("EditorHandle"));

	Rect2 scroll_rect(Point2(), object_texture->get_size());

	const Vector2 raw_endpoints[4] = {
		rect.position,
		rect.position + Vector2(rect.size.x, 0),
		rect.position + rect.size,
		rect.position + Vector2(0, rect.size.y)
	};
	const Vector2 endpoints[4] = {
		mtx.basis_xform(raw_endpoints[0]),
		mtx.basis_xform(raw_endpoints[1]),
		mtx.basis_xform(raw_endpoints[2]),
		mtx.basis_xform(raw_endpoints[3])
	};
	for (int i = 0; i < 4; i++) {
		int prev = (i + 3) % 4;
		int next = (i + 1) % 4;

		Vector2 ofs = ((endpoints[i] - endpoints[prev]).normalized() + ((endpoints[i] - endpoints[next]).normalized())).normalized();
		ofs *= Math_SQRT2 * (select_handle->get_size().width / 2);

		texture_overlay->draw_line(endpoints[i] - draw_ofs * draw_zoom, endpoints[next] - draw_ofs * draw_zoom, color, 2);

		if (snap_mode != SNAP_AUTOSLICE) {
			texture_overlay->draw_texture(select_handle, (endpoints[i] + ofs - (select_handle->get_size() / 2)).floor() - draw_ofs * draw_zoom);
		}

		ofs = (endpoints[next] - endpoints[i]) / 2;
		ofs += (endpoints[next] - endpoints[i]).orthogonal().normalized() * (select_handle->get_size().width / 2);

		if (snap_mode != SNAP_AUTOSLICE) {
			texture_overlay->draw_texture(select_handle, (endpoints[i] + ofs - (select_handle->get_size() / 2)).floor() - draw_ofs * draw_zoom);
		}

		scroll_rect.expand_to(raw_endpoints[i]);
	}

	const Size2 scroll_margin = texture_overlay->get_size() / draw_zoom;
	scroll_rect.position -= scroll_margin;
	scroll_rect.size += scroll_margin * 2;

	updating_scroll = true;

	hscroll->set_min(scroll_rect.position.x);
	hscroll->set_max(scroll_rect.position.x + scroll_rect.size.x);
	if (ABS(scroll_rect.position.x - (scroll_rect.position.x + scroll_rect.size.x)) <= scroll_margin.x) {
		hscroll->hide();
	} else {
		hscroll->show();
		hscroll->set_page(scroll_margin.x);
		hscroll->set_value(draw_ofs.x);
	}

	vscroll->set_min(scroll_rect.position.y);
	vscroll->set_max(scroll_rect.position.y + scroll_rect.size.y);
	if (ABS(scroll_rect.position.y - (scroll_rect.position.y + scroll_rect.size.y)) <= scroll_margin.y) {
		vscroll->hide();
		draw_ofs.y = scroll_rect.position.y;
	} else {
		vscroll->show();
		vscroll->set_page(scroll_margin.y);
		vscroll->set_value(draw_ofs.y);
	}

	Size2 hmin = hscroll->get_combined_minimum_size();
	Size2 vmin = vscroll->get_combined_minimum_size();

	// Avoid scrollbar overlapping.
	hscroll->set_anchor_and_offset(SIDE_RIGHT, Control::ANCHOR_END, vscroll->is_visible() ? -vmin.width : 0);
	vscroll->set_anchor_and_offset(SIDE_BOTTOM, Control::ANCHOR_END, hscroll->is_visible() ? -hmin.height : 0);

	updating_scroll = false;

	if (request_center && hscroll->get_min() < 0) {
		hscroll->set_value((hscroll->get_min() + hscroll->get_max() - hscroll->get_page()) / 2);
		vscroll->set_value((vscroll->get_min() + vscroll->get_max() - vscroll->get_page()) / 2);
		// This ensures that the view is updated correctly.
		callable_mp(this, &TextureRegionEditor::_pan_callback).call_deferred(Vector2(1, 0), Ref<InputEvent>());
		callable_mp(this, &TextureRegionEditor::_scroll_changed).call_deferred(0.0);
		request_center = false;
	}

	if (node_ninepatch || res_stylebox.is_valid()) {
		float margins[4] = { 0 };
		if (node_ninepatch) {
			margins[0] = node_ninepatch->get_patch_margin(SIDE_TOP);
			margins[1] = node_ninepatch->get_patch_margin(SIDE_BOTTOM);
			margins[2] = node_ninepatch->get_patch_margin(SIDE_LEFT);
			margins[3] = node_ninepatch->get_patch_margin(SIDE_RIGHT);
		} else if (res_stylebox.is_valid()) {
			margins[0] = res_stylebox->get_texture_margin(SIDE_TOP);
			margins[1] = res_stylebox->get_texture_margin(SIDE_BOTTOM);
			margins[2] = res_stylebox->get_texture_margin(SIDE_LEFT);
			margins[3] = res_stylebox->get_texture_margin(SIDE_RIGHT);
		}

		Vector2 pos[4] = {
			mtx.basis_xform(Vector2(0, margins[0])) + Vector2(0, endpoints[0].y - draw_ofs.y * draw_zoom),
			-mtx.basis_xform(Vector2(0, margins[1])) + Vector2(0, endpoints[2].y - draw_ofs.y * draw_zoom),
			mtx.basis_xform(Vector2(margins[2], 0)) + Vector2(endpoints[0].x - draw_ofs.x * draw_zoom, 0),
			-mtx.basis_xform(Vector2(margins[3], 0)) + Vector2(endpoints[2].x - draw_ofs.x * draw_zoom, 0)
		};

		_draw_margin_line(pos[0], pos[0] + Vector2(texture_overlay->get_size().x, 0));
		_draw_margin_line(pos[1], pos[1] + Vector2(texture_overlay->get_size().x, 0));
		_draw_margin_line(pos[2], pos[2] + Vector2(0, texture_overlay->get_size().y));
		_draw_margin_line(pos[3], pos[3] + Vector2(0, texture_overlay->get_size().y));
	}
}

void TextureRegionEditor::_draw_margin_line(Vector2 p_from, Vector2 p_to) {
	// Margin line is a dashed line with a normalized dash length. This method works
	// for both vertical and horizontal lines.

	Vector2 dash_size = (p_to - p_from).normalized() * 10;
	const int dash_thickness = Math::round(2 * EDSCALE);
	const Color dash_color = get_theme_color(SNAME("mono_color"), EditorStringName(Editor));
	const Color dash_bg_color = dash_color.inverted() * Color(1, 1, 1, 0.5);
	const int line_threshold = 200;

	// Draw a translucent background line to make the foreground line visible on any background.
	texture_overlay->draw_line(p_from, p_to, dash_bg_color, dash_thickness);

	Vector2 dash_start = p_from;
	while (dash_start.distance_squared_to(p_to) > line_threshold) {
		texture_overlay->draw_line(dash_start, dash_start + dash_size, dash_color, dash_thickness);

		// Skip two size lengths, one for the drawn dash and one for the gap.
		dash_start += dash_size * 2;
	}
}

void TextureRegionEditor::_set_grid_parameters_clamping(bool p_enabled) {
	sb_off_x->set_allow_lesser(!p_enabled);
	sb_off_x->set_allow_greater(!p_enabled);
	sb_off_y->set_allow_lesser(!p_enabled);
	sb_off_y->set_allow_greater(!p_enabled);
	sb_step_x->set_allow_greater(!p_enabled);
	sb_step_y->set_allow_greater(!p_enabled);
	sb_sep_x->set_allow_greater(!p_enabled);
	sb_sep_y->set_allow_greater(!p_enabled);
}

void TextureRegionEditor::_texture_overlay_input(const Ref<InputEvent> &p_input) {
	if (panner->gui_input(p_input, texture_overlay->get_global_rect())) {
		return;
	}

	Transform2D mtx;
	mtx.columns[2] = -draw_ofs * draw_zoom;
	mtx.scale_basis(Vector2(draw_zoom, draw_zoom));

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	Ref<InputEventMouseButton> mb = p_input;
	if (mb.is_valid()) {
		if (mb->get_button_index() == MouseButton::LEFT) {
			if (mb->is_pressed() && !panner->is_panning()) {
				// Check if we click on any handle first.
				{
					const real_t handle_radius = 16 * EDSCALE;
					const real_t handle_offset = 8 * EDSCALE;

					// Position of selection handles.
					const Vector2 endpoints[8] = {
						mtx.xform(rect.position) + Vector2(-handle_offset, -handle_offset),
						mtx.xform(rect.position + Vector2(rect.size.x / 2, 0)) + Vector2(0, -handle_offset),
						mtx.xform(rect.position + Vector2(rect.size.x, 0)) + Vector2(handle_offset, -handle_offset),
						mtx.xform(rect.position + Vector2(rect.size.x, rect.size.y / 2)) + Vector2(handle_offset, 0),
						mtx.xform(rect.position + rect.size) + Vector2(handle_offset, handle_offset),
						mtx.xform(rect.position + Vector2(rect.size.x / 2, rect.size.y)) + Vector2(0, handle_offset),
						mtx.xform(rect.position + Vector2(0, rect.size.y)) + Vector2(-handle_offset, handle_offset),
						mtx.xform(rect.position + Vector2(0, rect.size.y / 2)) + Vector2(-handle_offset, 0)
					};

					drag_from = mtx.affine_inverse().xform(mb->get_position());
					if (snap_mode == SNAP_PIXEL) {
						drag_from = drag_from.snappedf(1);
					} else if (snap_mode == SNAP_GRID) {
						drag_from = snap_point(drag_from);
					}
					drag = true;

					rect_prev = _get_edited_object_region();

					for (int i = 0; i < 8; i++) {
						Vector2 tuv = endpoints[i];
						if (tuv.distance_to(mb->get_position()) < handle_radius) {
							drag_index = i;
						}
					}
				}

				// We didn't hit any handle, try other options.
				if (drag_index < 0) {
					if (node_ninepatch || res_stylebox.is_valid()) {
						// For ninepatchable objects check if we are clicking on margin bars.

						edited_margin = -1;
						float margins[4] = { 0 };
						if (node_ninepatch) {
							margins[0] = node_ninepatch->get_patch_margin(SIDE_TOP);
							margins[1] = node_ninepatch->get_patch_margin(SIDE_BOTTOM);
							margins[2] = node_ninepatch->get_patch_margin(SIDE_LEFT);
							margins[3] = node_ninepatch->get_patch_margin(SIDE_RIGHT);
						} else if (res_stylebox.is_valid()) {
							margins[0] = res_stylebox->get_texture_margin(SIDE_TOP);
							margins[1] = res_stylebox->get_texture_margin(SIDE_BOTTOM);
							margins[2] = res_stylebox->get_texture_margin(SIDE_LEFT);
							margins[3] = res_stylebox->get_texture_margin(SIDE_RIGHT);
						}

						Vector2 pos[4] = {
							mtx.basis_xform(rect.position + Vector2(0, margins[0])) - draw_ofs * draw_zoom,
							mtx.basis_xform(rect.position + rect.size - Vector2(0, margins[1])) - draw_ofs * draw_zoom,
							mtx.basis_xform(rect.position + Vector2(margins[2], 0)) - draw_ofs * draw_zoom,
							mtx.basis_xform(rect.position + rect.size - Vector2(margins[3], 0)) - draw_ofs * draw_zoom
						};
						if (Math::abs(mb->get_position().y - pos[0].y) < 8) {
							edited_margin = 0;
							prev_margin = margins[0];
						} else if (Math::abs(mb->get_position().y - pos[1].y) < 8) {
							edited_margin = 1;
							prev_margin = margins[1];
						} else if (Math::abs(mb->get_position().x - pos[2].x) < 8) {
							edited_margin = 2;
							prev_margin = margins[2];
						} else if (Math::abs(mb->get_position().x - pos[3].x) < 8) {
							edited_margin = 3;
							prev_margin = margins[3];
						}
						if (edited_margin >= 0) {
							drag_from = mb->get_position();
							drag = true;
						}
					}

					if (edited_margin < 0 && snap_mode == SNAP_AUTOSLICE) {
						// We didn't hit anything, but we're in the autoslice mode. Handle it.

						Vector2 point = mtx.affine_inverse().xform(mb->get_position());
						for (const Rect2 &E : autoslice_cache) {
							if (E.has_point(point)) {
								rect = E;
								if (Input::get_singleton()->is_key_pressed(Key::CMD_OR_CTRL) && !(Input::get_singleton()->is_key_pressed(Key(Key::SHIFT | Key::ALT)))) {
									Rect2 r;
									if (node_sprite_2d) {
										r = node_sprite_2d->get_region_rect();
									} else if (node_sprite_3d) {
										r = node_sprite_3d->get_region_rect();
									} else if (node_ninepatch) {
										r = node_ninepatch->get_region_rect();
									} else if (res_stylebox.is_valid()) {
										r = res_stylebox->get_region_rect();
									} else if (res_atlas_texture.is_valid()) {
										r = res_atlas_texture->get_region();
									}
									rect.expand_to(r.position);
									rect.expand_to(r.get_end());
								}

								undo_redo->create_action(TTR("Set Region Rect"));
								if (node_sprite_2d) {
									undo_redo->add_do_method(node_sprite_2d, "set_region_rect", rect);
									undo_redo->add_undo_method(node_sprite_2d, "set_region_rect", node_sprite_2d->get_region_rect());
								} else if (node_sprite_3d) {
									undo_redo->add_do_method(node_sprite_3d, "set_region_rect", rect);
									undo_redo->add_undo_method(node_sprite_3d, "set_region_rect", node_sprite_3d->get_region_rect());
								} else if (node_ninepatch) {
									undo_redo->add_do_method(node_ninepatch, "set_region_rect", rect);
									undo_redo->add_undo_method(node_ninepatch, "set_region_rect", node_ninepatch->get_region_rect());
								} else if (res_stylebox.is_valid()) {
									undo_redo->add_do_method(res_stylebox.ptr(), "set_region_rect", rect);
									undo_redo->add_undo_method(res_stylebox.ptr(), "set_region_rect", res_stylebox->get_region_rect());
								} else if (res_atlas_texture.is_valid()) {
									undo_redo->add_do_method(res_atlas_texture.ptr(), "set_region", rect);
									undo_redo->add_undo_method(res_atlas_texture.ptr(), "set_region", res_atlas_texture->get_region());
								}

								undo_redo->add_do_method(this, "_update_rect");
								undo_redo->add_undo_method(this, "_update_rect");
								undo_redo->add_do_method(texture_overlay, "queue_redraw");
								undo_redo->add_undo_method(texture_overlay, "queue_redraw");
								undo_redo->commit_action();
								break;
							}
						}
					} else if (edited_margin < 0) {
						// We didn't hit anything and it's not autoslice, which means we try to create a new region.

						if (drag_index == -1) {
							creating = true;
							rect = Rect2(drag_from, Size2());
						}
					}
				}

			} else if (!mb->is_pressed() && drag) {
				if (edited_margin >= 0) {
					undo_redo->create_action(TTR("Set Margin"));
					static Side side[4] = { SIDE_TOP, SIDE_BOTTOM, SIDE_LEFT, SIDE_RIGHT };
					if (node_ninepatch) {
						undo_redo->add_do_method(node_ninepatch, "set_patch_margin", side[edited_margin], node_ninepatch->get_patch_margin(side[edited_margin]));
						undo_redo->add_undo_method(node_ninepatch, "set_patch_margin", side[edited_margin], prev_margin);
					} else if (res_stylebox.is_valid()) {
						undo_redo->add_do_method(res_stylebox.ptr(), "set_texture_margin", side[edited_margin], res_stylebox->get_texture_margin(side[edited_margin]));
						undo_redo->add_undo_method(res_stylebox.ptr(), "set_texture_margin", side[edited_margin], prev_margin);
						res_stylebox->emit_changed();
					}
					edited_margin = -1;
				} else {
					undo_redo->create_action(TTR("Set Region Rect"));
					if (node_sprite_2d) {
						undo_redo->add_do_method(node_sprite_2d, "set_region_rect", node_sprite_2d->get_region_rect());
						undo_redo->add_undo_method(node_sprite_2d, "set_region_rect", rect_prev);
					} else if (node_sprite_3d) {
						undo_redo->add_do_method(node_sprite_3d, "set_region_rect", node_sprite_3d->get_region_rect());
						undo_redo->add_undo_method(node_sprite_3d, "set_region_rect", rect_prev);
					} else if (node_ninepatch) {
						undo_redo->add_do_method(node_ninepatch, "set_region_rect", node_ninepatch->get_region_rect());
						undo_redo->add_undo_method(node_ninepatch, "set_region_rect", rect_prev);
					} else if (res_stylebox.is_valid()) {
						undo_redo->add_do_method(res_stylebox.ptr(), "set_region_rect", res_stylebox->get_region_rect());
						undo_redo->add_undo_method(res_stylebox.ptr(), "set_region_rect", rect_prev);
					} else if (res_atlas_texture.is_valid()) {
						undo_redo->add_do_method(res_atlas_texture.ptr(), "set_region", res_atlas_texture->get_region());
						undo_redo->add_undo_method(res_atlas_texture.ptr(), "set_region", rect_prev);
					}
					drag_index = -1;
				}
				undo_redo->add_do_method(this, "_update_rect");
				undo_redo->add_undo_method(this, "_update_rect");
				undo_redo->add_do_method(texture_overlay, "queue_redraw");
				undo_redo->add_undo_method(texture_overlay, "queue_redraw");
				undo_redo->commit_action();
				drag = false;
				creating = false;
			}

		} else if (mb->get_button_index() == MouseButton::RIGHT && mb->is_pressed()) {
			if (drag) {
				drag = false;
				if (edited_margin >= 0) {
					static Side side[4] = { SIDE_TOP, SIDE_BOTTOM, SIDE_LEFT, SIDE_RIGHT };
					if (node_ninepatch) {
						node_ninepatch->set_patch_margin(side[edited_margin], prev_margin);
					}
					if (res_stylebox.is_valid()) {
						res_stylebox->set_texture_margin(side[edited_margin], prev_margin);
					}
					edited_margin = -1;
				} else {
					_apply_rect(rect_prev);
					rect = rect_prev;
					texture_preview->queue_redraw();
					texture_overlay->queue_redraw();
					drag_index = -1;
				}
			}
		}
	}

	Ref<InputEventMouseMotion> mm = p_input;

	if (mm.is_valid()) {
		if (drag) {
			if (edited_margin >= 0) {
				float new_margin = 0;

				if (snap_mode != SNAP_GRID) {
					if (edited_margin == 0) {
						new_margin = prev_margin + (mm->get_position().y - drag_from.y) / draw_zoom;
					} else if (edited_margin == 1) {
						new_margin = prev_margin - (mm->get_position().y - drag_from.y) / draw_zoom;
					} else if (edited_margin == 2) {
						new_margin = prev_margin + (mm->get_position().x - drag_from.x) / draw_zoom;
					} else if (edited_margin == 3) {
						new_margin = prev_margin - (mm->get_position().x - drag_from.x) / draw_zoom;
					} else {
						ERR_PRINT("Unexpected edited_margin");
					}

					if (snap_mode == SNAP_PIXEL) {
						new_margin = Math::round(new_margin);
					}
				} else {
					Vector2 pos_snapped = snap_point(mtx.affine_inverse().xform(mm->get_position()));
					Rect2 rect_rounded = Rect2(rect.position.round(), rect.size.round());

					if (edited_margin == 0) {
						new_margin = pos_snapped.y - rect_rounded.position.y;
					} else if (edited_margin == 1) {
						new_margin = rect_rounded.size.y + rect_rounded.position.y - pos_snapped.y;
					} else if (edited_margin == 2) {
						new_margin = pos_snapped.x - rect_rounded.position.x;
					} else if (edited_margin == 3) {
						new_margin = rect_rounded.size.x + rect_rounded.position.x - pos_snapped.x;
					} else {
						ERR_PRINT("Unexpected edited_margin");
					}
				}

				if (new_margin < 0) {
					new_margin = 0;
				}
				static Side side[4] = { SIDE_TOP, SIDE_BOTTOM, SIDE_LEFT, SIDE_RIGHT };
				if (node_ninepatch) {
					node_ninepatch->set_patch_margin(side[edited_margin], new_margin);
				}
				if (res_stylebox.is_valid()) {
					res_stylebox->set_texture_margin(side[edited_margin], new_margin);
				}
			} else {
				Vector2 new_pos = mtx.affine_inverse().xform(mm->get_position());
				if (snap_mode == SNAP_PIXEL) {
					new_pos = new_pos.snappedf(1);
				} else if (snap_mode == SNAP_GRID) {
					new_pos = snap_point(new_pos);
				}

				if (creating) {
					rect = Rect2(drag_from, Size2());
					rect.expand_to(new_pos);
					_apply_rect(rect);
					texture_preview->queue_redraw();
					texture_overlay->queue_redraw();
					return;
				}

				switch (drag_index) {
					case 0: {
						Vector2 p = rect_prev.get_end();
						rect = Rect2(p, Size2());
						rect.expand_to(new_pos);
						_apply_rect(rect);
					} break;
					case 1: {
						Vector2 p = rect_prev.position + Vector2(0, rect_prev.size.y);
						rect = Rect2(p, Size2(rect_prev.size.x, 0));
						rect.expand_to(new_pos);
						_apply_rect(rect);
					} break;
					case 2: {
						Vector2 p = rect_prev.position + Vector2(0, rect_prev.size.y);
						rect = Rect2(p, Size2());
						rect.expand_to(new_pos);
						_apply_rect(rect);
					} break;
					case 3: {
						Vector2 p = rect_prev.position;
						rect = Rect2(p, Size2(0, rect_prev.size.y));
						rect.expand_to(new_pos);
						_apply_rect(rect);
					} break;
					case 4: {
						Vector2 p = rect_prev.position;
						rect = Rect2(p, Size2());
						rect.expand_to(new_pos);
						_apply_rect(rect);
					} break;
					case 5: {
						Vector2 p = rect_prev.position;
						rect = Rect2(p, Size2(rect_prev.size.x, 0));
						rect.expand_to(new_pos);
						_apply_rect(rect);
					} break;
					case 6: {
						Vector2 p = rect_prev.position + Vector2(rect_prev.size.x, 0);
						rect = Rect2(p, Size2());
						rect.expand_to(new_pos);
						_apply_rect(rect);
					} break;
					case 7: {
						Vector2 p = rect_prev.position + Vector2(rect_prev.size.x, 0);
						rect = Rect2(p, Size2(0, rect_prev.size.y));
						rect.expand_to(new_pos);
						_apply_rect(rect);
					} break;
				}
			}
			texture_preview->queue_redraw();
			texture_overlay->queue_redraw();
		}
	}

	Ref<InputEventMagnifyGesture> magnify_gesture = p_input;
	if (magnify_gesture.is_valid()) {
		_zoom_on_position(draw_zoom * magnify_gesture->get_factor(), magnify_gesture->get_position());
	}

	Ref<InputEventPanGesture> pan_gesture = p_input;
	if (pan_gesture.is_valid()) {
		hscroll->set_value(hscroll->get_value() + hscroll->get_page() * pan_gesture->get_delta().x / 8);
		vscroll->set_value(vscroll->get_value() + vscroll->get_page() * pan_gesture->get_delta().y / 8);
	}
}

void TextureRegionEditor::_pan_callback(Vector2 p_scroll_vec, Ref<InputEvent> p_event) {
	p_scroll_vec /= draw_zoom;
	hscroll->set_value(hscroll->get_value() - p_scroll_vec.x);
	vscroll->set_value(vscroll->get_value() - p_scroll_vec.y);
}

void TextureRegionEditor::_zoom_callback(float p_zoom_factor, Vector2 p_origin, Ref<InputEvent> p_event) {
	_zoom_on_position(draw_zoom * p_zoom_factor, p_origin);
}

void TextureRegionEditor::_scroll_changed(float) {
	if (updating_scroll) {
		return;
	}

	draw_ofs.x = hscroll->get_value();
	draw_ofs.y = vscroll->get_value();

	texture_preview->queue_redraw();
	texture_overlay->queue_redraw();
}

void TextureRegionEditor::_set_snap_mode(int p_mode) {
	snap_mode = (SnapMode)p_mode;

	hb_grid->set_visible(snap_mode == SNAP_GRID);
	if (snap_mode == SNAP_AUTOSLICE && is_visible() && autoslice_is_dirty) {
		_update_autoslice();
	}

	texture_overlay->queue_redraw();
}

void TextureRegionEditor::_set_snap_off_x(float p_val) {
	snap_offset.x = p_val;
	texture_overlay->queue_redraw();
}

void TextureRegionEditor::_set_snap_off_y(float p_val) {
	snap_offset.y = p_val;
	texture_overlay->queue_redraw();
}

void TextureRegionEditor::_set_snap_step_x(float p_val) {
	snap_step.x = p_val;
	texture_overlay->queue_redraw();
}

void TextureRegionEditor::_set_snap_step_y(float p_val) {
	snap_step.y = p_val;
	texture_overlay->queue_redraw();
}

void TextureRegionEditor::_set_snap_sep_x(float p_val) {
	snap_separation.x = p_val;
	texture_overlay->queue_redraw();
}

void TextureRegionEditor::_set_snap_sep_y(float p_val) {
	snap_separation.y = p_val;
	texture_overlay->queue_redraw();
}

void TextureRegionEditor::_zoom_on_position(float p_zoom, Point2 p_position) {
	if (p_zoom < min_draw_zoom || p_zoom > max_draw_zoom) {
		return;
	}

	float prev_zoom = draw_zoom;
	draw_zoom = p_zoom;
	Point2 ofs = p_position;
	ofs = ofs / prev_zoom - ofs / draw_zoom;
	draw_ofs = (draw_ofs + ofs).round();

	texture_preview->queue_redraw();
	texture_overlay->queue_redraw();
}

void TextureRegionEditor::_zoom_in() {
	_zoom_on_position(draw_zoom * 1.5, texture_overlay->get_size() / 2.0);
}

void TextureRegionEditor::_zoom_reset() {
	_zoom_on_position(1.0, texture_overlay->get_size() / 2.0);
}

void TextureRegionEditor::_zoom_out() {
	_zoom_on_position(draw_zoom / 1.5, texture_overlay->get_size() / 2.0);
}

void TextureRegionEditor::_apply_rect(const Rect2 &p_rect) {
	if (node_sprite_2d) {
		node_sprite_2d->set_region_rect(p_rect);
	} else if (node_sprite_3d) {
		node_sprite_3d->set_region_rect(p_rect);
	} else if (node_ninepatch) {
		node_ninepatch->set_region_rect(p_rect);
	} else if (res_stylebox.is_valid()) {
		res_stylebox->set_region_rect(p_rect);
	} else if (res_atlas_texture.is_valid()) {
		res_atlas_texture->set_region(p_rect);
	}
}

void TextureRegionEditor::_update_rect() {
	rect = _get_edited_object_region();
}

void TextureRegionEditor::_update_autoslice() {
	autoslice_is_dirty = false;
	autoslice_cache.clear();

	const Ref<Texture2D> object_texture = _get_edited_object_texture();
	if (object_texture.is_null()) {
		return;
	}

	for (int y = 0; y < object_texture->get_height(); y++) {
		for (int x = 0; x < object_texture->get_width(); x++) {
			if (object_texture->is_pixel_opaque(x, y)) {
				bool found = false;
				for (Rect2 &E : autoslice_cache) {
					Rect2 grown = E.grow(1.5);
					if (grown.has_point(Point2(x, y))) {
						E.expand_to(Point2(x, y));
						E.expand_to(Point2(x + 1, y + 1));
						x = E.position.x + E.size.x - 1;
						bool merged = true;
						while (merged) {
							merged = false;
							bool queue_erase = false;
							for (List<Rect2>::Element *F = autoslice_cache.front(); F; F = F->next()) {
								if (queue_erase) {
									autoslice_cache.erase(F->prev());
									queue_erase = false;
								}
								if (F->get() == E) {
									continue;
								}
								if (E.grow(1).intersects(F->get())) {
									E.expand_to(F->get().position);
									E.expand_to(F->get().position + F->get().size);
									if (F->prev()) {
										F = F->prev();
										autoslice_cache.erase(F->next());
									} else {
										queue_erase = true;
										// Can't delete the first rect in the list.
									}
									merged = true;
								}
							}
						}
						found = true;
						break;
					}
				}
				if (!found) {
					Rect2 new_rect(x, y, 1, 1);
					autoslice_cache.push_back(new_rect);
				}
			}
		}
	}
	cache_map[object_texture->get_rid()] = autoslice_cache;
}

void TextureRegionEditor::_notification(int p_what) {
	switch (p_what) {
		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (!EditorSettings::get_singleton()->check_changed_settings_in_group("editors/panning")) {
				break;
			}
			[[fallthrough]];
		}

		case NOTIFICATION_ENTER_TREE: {
			get_tree()->connect("node_removed", callable_mp(this, &TextureRegionEditor::_node_removed));

			hb_grid->set_visible(snap_mode == SNAP_GRID);
			if (snap_mode == SNAP_AUTOSLICE && is_visible() && autoslice_is_dirty) {
				_update_autoslice();
			}

			panner->setup((ViewPanner::ControlScheme)EDITOR_GET("editors/panning/sub_editors_panning_scheme").operator int(), ED_GET_SHORTCUT("canvas_item_editor/pan_view"), bool(EDITOR_GET("editors/panning/simple_panning")));
			panner->setup_warped_panning(get_viewport(), EDITOR_GET("editors/panning/warped_mouse_panning"));
		} break;

		case NOTIFICATION_EXIT_TREE: {
			get_tree()->disconnect("node_removed", callable_mp(this, &TextureRegionEditor::_node_removed));
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			texture_preview->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("TextureRegionPreviewBG"), EditorStringName(EditorStyles)));
			texture_overlay->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("TextureRegionPreviewFG"), EditorStringName(EditorStyles)));

			zoom_out->set_button_icon(get_editor_theme_icon(SNAME("ZoomLess")));
			zoom_reset->set_button_icon(get_editor_theme_icon(SNAME("ZoomReset")));
			zoom_in->set_button_icon(get_editor_theme_icon(SNAME("ZoomMore")));
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (snap_mode == SNAP_AUTOSLICE && is_visible() && autoslice_is_dirty) {
				_update_autoslice();
			}

			if (!is_visible()) {
				EditorSettings::get_singleton()->set_project_metadata("texture_region_editor", "snap_offset", snap_offset);
				EditorSettings::get_singleton()->set_project_metadata("texture_region_editor", "snap_step", snap_step);
				EditorSettings::get_singleton()->set_project_metadata("texture_region_editor", "snap_separation", snap_separation);
				EditorSettings::get_singleton()->set_project_metadata("texture_region_editor", "snap_mode", snap_mode);
			}
		} break;

		case NOTIFICATION_WM_WINDOW_FOCUS_IN: {
			// This happens when the user leaves the Editor and returns,
			// they could have changed the textures, so the cache is cleared.
			cache_map.clear();
			_edit_region();
		} break;
	}
}

void TextureRegionEditor::_node_removed(Node *p_node) {
	if (p_node == node_sprite_2d || p_node == node_sprite_3d || p_node == node_ninepatch) {
		_clear_edited_object();
		hide();
	}
}

void TextureRegionEditor::_clear_edited_object() {
	if (node_sprite_2d) {
		node_sprite_2d->disconnect(SceneStringName(texture_changed), callable_mp(this, &TextureRegionEditor::_texture_changed));
	}
	if (node_sprite_3d) {
		node_sprite_3d->disconnect(SceneStringName(texture_changed), callable_mp(this, &TextureRegionEditor::_texture_changed));
	}
	if (node_ninepatch) {
		node_ninepatch->disconnect(SceneStringName(texture_changed), callable_mp(this, &TextureRegionEditor::_texture_changed));
	}
	if (res_stylebox.is_valid()) {
		res_stylebox->disconnect_changed(callable_mp(this, &TextureRegionEditor::_texture_changed));
	}
	if (res_atlas_texture.is_valid()) {
		res_atlas_texture->disconnect_changed(callable_mp(this, &TextureRegionEditor::_texture_changed));
	}

	node_sprite_2d = nullptr;
	node_sprite_3d = nullptr;
	node_ninepatch = nullptr;
	res_stylebox = Ref<StyleBoxTexture>();
	res_atlas_texture = Ref<AtlasTexture>();
}

void TextureRegionEditor::edit(Object *p_obj) {
	_clear_edited_object();

	if (p_obj) {
		node_sprite_2d = Object::cast_to<Sprite2D>(p_obj);
		node_sprite_3d = Object::cast_to<Sprite3D>(p_obj);
		node_ninepatch = Object::cast_to<NinePatchRect>(p_obj);

		bool is_resource = false;
		if (Object::cast_to<StyleBoxTexture>(p_obj)) {
			res_stylebox = Ref<StyleBoxTexture>(p_obj);
			is_resource = true;
		}
		if (Object::cast_to<AtlasTexture>(p_obj)) {
			res_atlas_texture = Ref<AtlasTexture>(p_obj);
			is_resource = true;
		}

		if (is_resource) {
			Object::cast_to<Resource>(p_obj)->connect_changed(callable_mp(this, &TextureRegionEditor::_texture_changed));
		} else {
			p_obj->connect(SceneStringName(texture_changed), callable_mp(this, &TextureRegionEditor::_texture_changed));
		}
		_edit_region();
	}

	texture_preview->queue_redraw();
	texture_overlay->queue_redraw();
	popup_centered_ratio(0.5);
	request_center = true;
}

Ref<Texture2D> TextureRegionEditor::_get_edited_object_texture() const {
	if (node_sprite_2d) {
		return node_sprite_2d->get_texture();
	}
	if (node_sprite_3d) {
		return node_sprite_3d->get_texture();
	}
	if (node_ninepatch) {
		return node_ninepatch->get_texture();
	}
	if (res_stylebox.is_valid()) {
		return res_stylebox->get_texture();
	}
	if (res_atlas_texture.is_valid()) {
		return res_atlas_texture->get_atlas();
	}

	return Ref<Texture2D>();
}

Rect2 TextureRegionEditor::_get_edited_object_region() const {
	Rect2 region;

	if (node_sprite_2d) {
		region = node_sprite_2d->get_region_rect();
	} else if (node_sprite_3d) {
		region = node_sprite_3d->get_region_rect();
	} else if (node_ninepatch) {
		region = node_ninepatch->get_region_rect();
	} else if (res_stylebox.is_valid()) {
		region = res_stylebox->get_region_rect();
	} else if (res_atlas_texture.is_valid()) {
		region = res_atlas_texture->get_region();
	}

	const Ref<Texture2D> object_texture = _get_edited_object_texture();
	if (region == Rect2() && object_texture.is_valid()) {
		region = Rect2(Vector2(), object_texture->get_size());
	}

	return region;
}

void TextureRegionEditor::_texture_changed() {
	if (!is_visible()) {
		return;
	}
	_edit_region();
}

void TextureRegionEditor::_edit_region() {
	const Ref<Texture2D> object_texture = _get_edited_object_texture();
	if (object_texture.is_null()) {
		_set_grid_parameters_clamping(false);
		_zoom_reset();
		hscroll->hide();
		vscroll->hide();
		texture_preview->queue_redraw();
		texture_overlay->queue_redraw();
		return;
	}

	CanvasItem::TextureFilter filter = CanvasItem::TEXTURE_FILTER_NEAREST_WITH_MIPMAPS;
	if (node_sprite_2d) {
		filter = node_sprite_2d->get_texture_filter_in_tree();
	} else if (node_sprite_3d) {
		StandardMaterial3D::TextureFilter filter_3d = node_sprite_3d->get_texture_filter();

		switch (filter_3d) {
			case StandardMaterial3D::TEXTURE_FILTER_NEAREST:
				filter = CanvasItem::TEXTURE_FILTER_NEAREST;
				break;
			case StandardMaterial3D::TEXTURE_FILTER_LINEAR:
				filter = CanvasItem::TEXTURE_FILTER_LINEAR;
				break;
			case StandardMaterial3D::TEXTURE_FILTER_NEAREST_WITH_MIPMAPS:
				filter = CanvasItem::TEXTURE_FILTER_NEAREST_WITH_MIPMAPS;
				break;
			case StandardMaterial3D::TEXTURE_FILTER_LINEAR_WITH_MIPMAPS:
				filter = CanvasItem::TEXTURE_FILTER_LINEAR_WITH_MIPMAPS;
				break;
			case StandardMaterial3D::TEXTURE_FILTER_NEAREST_WITH_MIPMAPS_ANISOTROPIC:
				filter = CanvasItem::TEXTURE_FILTER_NEAREST_WITH_MIPMAPS_ANISOTROPIC;
				break;
			case StandardMaterial3D::TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC:
				filter = CanvasItem::TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC;
				break;
			default:
				// fallback to project default
				filter = CanvasItem::TEXTURE_FILTER_PARENT_NODE;
				break;
		}
	} else if (node_ninepatch) {
		filter = node_ninepatch->get_texture_filter_in_tree();
	}

	// occurs when get_texture_filter_in_tree reaches the scene root
	if (filter == CanvasItem::TEXTURE_FILTER_PARENT_NODE) {
		SubViewport *root = EditorNode::get_singleton()->get_scene_root();

		if (root != nullptr) {
			Viewport::DefaultCanvasItemTextureFilter filter_default = root->get_default_canvas_item_texture_filter();

			// depending on default filter, set filter to match, otherwise fall back on nearest w/ mipmaps
			switch (filter_default) {
				case DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_NEAREST:
					filter = CanvasItem::TEXTURE_FILTER_NEAREST;
					break;
				case DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_LINEAR:
					filter = CanvasItem::TEXTURE_FILTER_LINEAR;
					break;
				case DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS:
					filter = CanvasItem::TEXTURE_FILTER_LINEAR_WITH_MIPMAPS;
					break;
				case DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS:
				default:
					filter = CanvasItem::TEXTURE_FILTER_NEAREST_WITH_MIPMAPS;
					break;
			}
		} else {
			filter = CanvasItem::TEXTURE_FILTER_NEAREST_WITH_MIPMAPS;
		}
	}

	texture_preview->set_texture_filter(filter);
	texture_preview->set_texture_repeat(CanvasItem::TEXTURE_REPEAT_DISABLED);

	if (cache_map.has(object_texture->get_rid())) {
		autoslice_cache = cache_map[object_texture->get_rid()];
		autoslice_is_dirty = false;
	} else {
		if (is_visible() && snap_mode == SNAP_AUTOSLICE) {
			_update_autoslice();
		} else {
			autoslice_is_dirty = true;
		}
	}

	// Avoiding clamping with mismatched min/max.
	_set_grid_parameters_clamping(false);
	const Size2 tex_size = object_texture->get_size();
	sb_off_x->set_min(-tex_size.x);
	sb_off_x->set_max(tex_size.x);
	sb_off_y->set_min(-tex_size.y);
	sb_off_y->set_max(tex_size.y);
	sb_step_x->set_max(tex_size.x);
	sb_step_y->set_max(tex_size.y);
	sb_sep_x->set_max(tex_size.x);
	sb_sep_y->set_max(tex_size.y);

	_set_grid_parameters_clamping(true);
	sb_off_x->set_value(snap_offset.x);
	sb_off_y->set_value(snap_offset.y);
	sb_step_x->set_value(snap_step.x);
	sb_step_y->set_value(snap_step.y);
	sb_sep_x->set_value(snap_separation.x);
	sb_sep_y->set_value(snap_separation.y);

	_update_rect();
	texture_preview->queue_redraw();
	texture_overlay->queue_redraw();
}

Vector2 TextureRegionEditor::snap_point(Vector2 p_target) const {
	if (snap_mode == SNAP_GRID) {
		p_target.x = Math::snap_scalar_separation(snap_offset.x, snap_step.x, p_target.x, snap_separation.x);
		p_target.y = Math::snap_scalar_separation(snap_offset.y, snap_step.y, p_target.y, snap_separation.y);
	}

	return p_target;
}

void TextureRegionEditor::shortcut_input(const Ref<InputEvent> &p_event) {
	const Ref<InputEventKey> k = p_event;
	if (k.is_valid() && k->is_pressed()) {
		bool handled = false;

		if (ED_IS_SHORTCUT("ui_undo", p_event)) {
			EditorNode::get_singleton()->undo();
			handled = true;
		}

		if (ED_IS_SHORTCUT("ui_redo", p_event)) {
			EditorNode::get_singleton()->redo();
			handled = true;
		}

		if (handled) {
			set_input_as_handled();
		}
	}
}

void TextureRegionEditor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_update_rect"), &TextureRegionEditor::_update_rect);
}

TextureRegionEditor::TextureRegionEditor() {
	set_title(TTR("Region Editor"));
	set_process_shortcut_input(true);
	set_ok_button_text(TTR("Close"));

	// A power-of-two value works better as a default grid size.
	snap_offset = EditorSettings::get_singleton()->get_project_metadata("texture_region_editor", "snap_offset", Vector2());
	snap_step = EditorSettings::get_singleton()->get_project_metadata("texture_region_editor", "snap_step", Vector2(8, 8));
	snap_separation = EditorSettings::get_singleton()->get_project_metadata("texture_region_editor", "snap_separation", Vector2());
	snap_mode = (SnapMode)(int)EditorSettings::get_singleton()->get_project_metadata("texture_region_editor", "snap_mode", SNAP_NONE);

	panner.instantiate();
	panner->set_callbacks(callable_mp(this, &TextureRegionEditor::_pan_callback), callable_mp(this, &TextureRegionEditor::_zoom_callback));

	VBoxContainer *vb = memnew(VBoxContainer);
	add_child(vb);

	HBoxContainer *hb_tools = memnew(HBoxContainer);
	vb->add_child(hb_tools);
	hb_tools->add_child(memnew(Label(TTR("Snap Mode:"))));

	snap_mode_button = memnew(OptionButton);
	hb_tools->add_child(snap_mode_button);
	snap_mode_button->add_item(TTR("None"), 0);
	snap_mode_button->add_item(TTR("Pixel Snap"), 1);
	snap_mode_button->add_item(TTR("Grid Snap"), 2);
	snap_mode_button->add_item(TTR("Auto Slice"), 3);
	snap_mode_button->select(snap_mode);
	snap_mode_button->connect(SceneStringName(item_selected), callable_mp(this, &TextureRegionEditor::_set_snap_mode));

	hb_grid = memnew(HBoxContainer);
	hb_tools->add_child(hb_grid);

	hb_grid->add_child(memnew(VSeparator));
	hb_grid->add_child(memnew(Label(TTR("Offset:"))));

	sb_off_x = memnew(SpinBox);
	sb_off_x->set_step(1);
	sb_off_x->set_suffix("px");
	sb_off_x->connect(SceneStringName(value_changed), callable_mp(this, &TextureRegionEditor::_set_snap_off_x));
	hb_grid->add_child(sb_off_x);

	sb_off_y = memnew(SpinBox);
	sb_off_y->set_step(1);
	sb_off_y->set_suffix("px");
	sb_off_y->connect(SceneStringName(value_changed), callable_mp(this, &TextureRegionEditor::_set_snap_off_y));
	hb_grid->add_child(sb_off_y);

	hb_grid->add_child(memnew(VSeparator));
	hb_grid->add_child(memnew(Label(TTR("Step:"))));

	sb_step_x = memnew(SpinBox);
	sb_step_x->set_min(0);
	sb_step_x->set_step(1);
	sb_step_x->set_suffix("px");
	sb_step_x->connect(SceneStringName(value_changed), callable_mp(this, &TextureRegionEditor::_set_snap_step_x));
	hb_grid->add_child(sb_step_x);

	sb_step_y = memnew(SpinBox);
	sb_step_y->set_min(0);
	sb_step_y->set_step(1);
	sb_step_y->set_suffix("px");
	sb_step_y->connect(SceneStringName(value_changed), callable_mp(this, &TextureRegionEditor::_set_snap_step_y));
	hb_grid->add_child(sb_step_y);

	hb_grid->add_child(memnew(VSeparator));
	hb_grid->add_child(memnew(Label(TTR("Separation:"))));

	sb_sep_x = memnew(SpinBox);
	sb_sep_x->set_min(0);
	sb_sep_x->set_step(1);
	sb_sep_x->set_suffix("px");
	sb_sep_x->connect(SceneStringName(value_changed), callable_mp(this, &TextureRegionEditor::_set_snap_sep_x));
	hb_grid->add_child(sb_sep_x);

	sb_sep_y = memnew(SpinBox);
	sb_sep_y->set_min(0);
	sb_sep_y->set_step(1);
	sb_sep_y->set_suffix("px");
	sb_sep_y->connect(SceneStringName(value_changed), callable_mp(this, &TextureRegionEditor::_set_snap_sep_y));
	hb_grid->add_child(sb_sep_y);

	hb_grid->hide();

	// Restore grid snap parameters.
	_set_grid_parameters_clamping(false);
	sb_off_x->set_value(snap_offset.x);
	sb_off_y->set_value(snap_offset.y);
	sb_step_x->set_value(snap_step.x);
	sb_step_y->set_value(snap_step.y);
	sb_sep_x->set_value(snap_separation.x);
	sb_sep_y->set_value(snap_separation.y);

	// Default the zoom to match the editor scale, but don't dezoom on editor scales below 100% to prevent pixel art from looking bad.
	draw_zoom = MAX(1.0f, EDSCALE);
	max_draw_zoom = 128.0f * MAX(1.0f, EDSCALE);
	min_draw_zoom = 0.01f * MAX(1.0f, EDSCALE);

	texture_preview = memnew(PanelContainer);
	vb->add_child(texture_preview);
	texture_preview->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	texture_preview->set_clip_contents(true);
	texture_preview->connect(SceneStringName(draw), callable_mp(this, &TextureRegionEditor::_texture_preview_draw));

	texture_overlay = memnew(Panel);
	texture_preview->add_child(texture_overlay);
	texture_overlay->set_focus_mode(Control::FOCUS_CLICK);
	texture_overlay->connect(SceneStringName(draw), callable_mp(this, &TextureRegionEditor::_texture_overlay_draw));
	texture_overlay->connect(SceneStringName(gui_input), callable_mp(this, &TextureRegionEditor::_texture_overlay_input));
	texture_overlay->connect(SceneStringName(focus_exited), callable_mp(panner.ptr(), &ViewPanner::release_pan_key));

	HBoxContainer *zoom_hb = memnew(HBoxContainer);
	texture_overlay->add_child(zoom_hb);
	zoom_hb->set_begin(Point2(5, 5));

	zoom_out = memnew(Button);
	zoom_out->set_flat(true);
	zoom_out->set_tooltip_text(TTR("Zoom Out"));
	zoom_out->connect(SceneStringName(pressed), callable_mp(this, &TextureRegionEditor::_zoom_out));
	zoom_hb->add_child(zoom_out);

	zoom_reset = memnew(Button);
	zoom_reset->set_flat(true);
	zoom_reset->set_tooltip_text(TTR("Zoom Reset"));
	zoom_reset->connect(SceneStringName(pressed), callable_mp(this, &TextureRegionEditor::_zoom_reset));
	zoom_hb->add_child(zoom_reset);

	zoom_in = memnew(Button);
	zoom_in->set_flat(true);
	zoom_in->set_tooltip_text(TTR("Zoom In"));
	zoom_in->connect(SceneStringName(pressed), callable_mp(this, &TextureRegionEditor::_zoom_in));
	zoom_hb->add_child(zoom_in);

	vscroll = memnew(VScrollBar);
	vscroll->set_anchors_and_offsets_preset(Control::PRESET_RIGHT_WIDE);
	vscroll->set_step(0.001);
	vscroll->connect(SceneStringName(value_changed), callable_mp(this, &TextureRegionEditor::_scroll_changed));
	texture_overlay->add_child(vscroll);

	hscroll = memnew(HScrollBar);
	hscroll->set_anchors_and_offsets_preset(Control::PRESET_BOTTOM_WIDE);
	hscroll->set_step(0.001);
	hscroll->connect(SceneStringName(value_changed), callable_mp(this, &TextureRegionEditor::_scroll_changed));
	texture_overlay->add_child(hscroll);
}

////////////////////////

bool EditorInspectorPluginTextureRegion::can_handle(Object *p_object) {
	return Object::cast_to<Sprite2D>(p_object) || Object::cast_to<Sprite3D>(p_object) || Object::cast_to<NinePatchRect>(p_object) || Object::cast_to<StyleBoxTexture>(p_object) || Object::cast_to<AtlasTexture>(p_object);
}

void EditorInspectorPluginTextureRegion::_region_edit(Object *p_object) {
	texture_region_editor->edit(p_object);
}

bool EditorInspectorPluginTextureRegion::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	if ((p_type == Variant::RECT2 || p_type == Variant::RECT2I)) {
		if (((Object::cast_to<Sprite2D>(p_object) || Object::cast_to<Sprite3D>(p_object) || Object::cast_to<NinePatchRect>(p_object) || Object::cast_to<StyleBoxTexture>(p_object)) && p_path == "region_rect") || (Object::cast_to<AtlasTexture>(p_object) && p_path == "region")) {
			Button *button = EditorInspector::create_inspector_action_button(TTR("Edit Region"));
			button->set_button_icon(texture_region_editor->get_editor_theme_icon(SNAME("RegionEdit")));
			button->connect(SceneStringName(pressed), callable_mp(this, &EditorInspectorPluginTextureRegion::_region_edit).bind(p_object));
			add_property_editor(p_path, button, true);
		}
	}
	return false; //not exclusive
}

EditorInspectorPluginTextureRegion::EditorInspectorPluginTextureRegion() {
	texture_region_editor = memnew(TextureRegionEditor);
	EditorNode::get_singleton()->get_gui_base()->add_child(texture_region_editor);
}

TextureRegionEditorPlugin::TextureRegionEditorPlugin() {
	Ref<EditorInspectorPluginTextureRegion> inspector_plugin;
	inspector_plugin.instantiate();
	add_inspector_plugin(inspector_plugin);
}
