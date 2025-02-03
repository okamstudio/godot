/**************************************************************************/
/*  texture_progress.cpp                                                  */
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

#include "texture_progress.h"

#include "core/engine.h"
#include "math.h"

void TextureProgress::set_under_texture(const Ref<Texture> &p_texture) {
	under = p_texture;
	update();
	minimum_size_changed();
}

Ref<Texture> TextureProgress::get_under_texture() const {
	return under;
}

void TextureProgress::set_over_texture(const Ref<Texture> &p_texture) {
	over = p_texture;
	update();
	if (under.is_null()) {
		minimum_size_changed();
	}
}

Ref<Texture> TextureProgress::get_over_texture() const {
	return over;
}

void TextureProgress::set_stretch_margin(Margin p_margin, int p_size) {
	ERR_FAIL_INDEX((int)p_margin, 4);
	stretch_margin[p_margin] = p_size;
	update();
	minimum_size_changed();
}

int TextureProgress::get_stretch_margin(Margin p_margin) const {
	ERR_FAIL_INDEX_V((int)p_margin, 4, 0);
	return stretch_margin[p_margin];
}

void TextureProgress::set_nine_patch_stretch(bool p_stretch) {
	nine_patch_stretch = p_stretch;
	update();
	minimum_size_changed();
}

bool TextureProgress::get_nine_patch_stretch() const {
	return nine_patch_stretch;
}

Size2 TextureProgress::get_minimum_size() const {
	if (nine_patch_stretch) {
		return Size2(stretch_margin[MARGIN_LEFT] + stretch_margin[MARGIN_RIGHT], stretch_margin[MARGIN_TOP] + stretch_margin[MARGIN_BOTTOM]);
	} else if (under.is_valid()) {
		return under->get_size();
	} else if (over.is_valid()) {
		return over->get_size();
	} else if (progress.is_valid()) {
		return progress->get_size();
	}

	return Size2(1, 1);
}

void TextureProgress::set_progress_texture(const Ref<Texture> &p_texture) {
	progress = p_texture;
	update();
	minimum_size_changed();
}

Ref<Texture> TextureProgress::get_progress_texture() const {
	return progress;
}

void TextureProgress::set_progress_offset(Point2 p_offset) {
	progress_offset = p_offset;
	update();
}

Point2 TextureProgress::get_progress_offset() const {
	return progress_offset;
}

void TextureProgress::set_tint_under(const Color &p_tint) {
	tint_under = p_tint;
	update();
}

Color TextureProgress::get_tint_under() const {
	return tint_under;
}

void TextureProgress::set_tint_progress(const Color &p_tint) {
	tint_progress = p_tint;
	update();
}

Color TextureProgress::get_tint_progress() const {
	return tint_progress;
}

void TextureProgress::set_tint_over(const Color &p_tint) {
	tint_over = p_tint;
	update();
}

Color TextureProgress::get_tint_over() const {
	return tint_over;
}

Point2 TextureProgress::unit_val_to_uv(float val) {
	if (progress.is_null()) {
		return Point2();
	}

	if (val < 0) {
		val += 1;
	}
	if (val > 1) {
		val -= 1;
	}

	Point2 p = get_relative_center();

	// Minimal version of Liang-Barsky clipping algorithm
	float angle = (val * Math_TAU) - Math_PI * 0.5;
	Point2 dir = Vector2(Math::cos(angle), Math::sin(angle));
	float t1 = 1.0;
	float cp = 0;
	float cq = 0;
	float cr = 0;
	float edgeLeft = 0.0;
	float edgeRight = 1.0;
	float edgeBottom = 0.0;
	float edgeTop = 1.0;

	for (int edge = 0; edge < 4; edge++) {
		if (edge == 0) {
			if (dir.x > 0) {
				continue;
			}
			cq = -(edgeLeft - p.x);
			dir.x *= 2.0 * cq;
			cp = -dir.x;
		} else if (edge == 1) {
			if (dir.x < 0) {
				continue;
			}
			cq = (edgeRight - p.x);
			dir.x *= 2.0 * cq;
			cp = dir.x;
		} else if (edge == 2) {
			if (dir.y > 0) {
				continue;
			}
			cq = -(edgeBottom - p.y);
			dir.y *= 2.0 * cq;
			cp = -dir.y;
		} else if (edge == 3) {
			if (dir.y < 0) {
				continue;
			}
			cq = (edgeTop - p.y);
			dir.y *= 2.0 * cq;
			cp = dir.y;
		}
		cr = cq / cp;
		if (cr >= 0 && cr < t1) {
			t1 = cr;
		}
	}
	return (p + t1 * dir);
}

Point2 TextureProgress::get_relative_center() {
	if (progress.is_null()) {
		return Point2();
	}
	Point2 p = progress->get_size() / 2;
	p += rad_center_off;
	p.x /= progress->get_width();
	p.y /= progress->get_height();
	p.x = CLAMP(p.x, 0, 1);
	p.y = CLAMP(p.y, 0, 1);
	return p;
}

void TextureProgress::draw_nine_patch_stretched(const Ref<Texture> &p_texture, FillMode p_mode, double p_ratio, const Color &p_modulate) {
	Vector2 texture_size = p_texture->get_size();
	Vector2 topleft = Vector2(stretch_margin[MARGIN_LEFT], stretch_margin[MARGIN_TOP]);
	Vector2 bottomright = Vector2(stretch_margin[MARGIN_RIGHT], stretch_margin[MARGIN_BOTTOM]);

	Rect2 src_rect = Rect2(Point2(), texture_size);
	Rect2 dst_rect = Rect2(Point2(), get_size());

	if (p_ratio < 1.0) {
		// Drawing a partially-filled 9-patch is a little tricky -
		// texture is divided by 3 sections toward fill direction,
		// then middle section is stretching while the other two aren't.

		double width_total = 0.0;
		double width_texture = 0.0;
		double first_section_size = 0.0;
		double last_section_size = 0.0;
		switch (p_mode) {
			case FILL_LEFT_TO_RIGHT: {
				width_total = dst_rect.size.x;
				width_texture = texture_size.x;
				first_section_size = topleft.x;
				last_section_size = bottomright.x;
			} break;
			case FILL_RIGHT_TO_LEFT: {
				width_total = dst_rect.size.x;
				width_texture = texture_size.x;
				// In contrast to `FILL_LEFT_TO_RIGHT`, `first_section_size` and `last_section_size` should switch value.
				first_section_size = bottomright.x;
				last_section_size = topleft.x;
			} break;
			case FILL_TOP_TO_BOTTOM: {
				width_total = dst_rect.size.y;
				width_texture = texture_size.y;
				first_section_size = topleft.y;
				last_section_size = bottomright.y;
			} break;
			case FILL_BOTTOM_TO_TOP: {
				width_total = dst_rect.size.y;
				width_texture = texture_size.y;
				// Similar to `FILL_RIGHT_TO_LEFT`.
				first_section_size = bottomright.y;
				last_section_size = topleft.y;
			} break;
			case FILL_BILINEAR_LEFT_AND_RIGHT: {
				width_total = dst_rect.size.x;
				width_texture = texture_size.x;
				first_section_size = topleft.x;
				last_section_size = bottomright.x;
			} break;
			case FILL_BILINEAR_TOP_AND_BOTTOM: {
				width_total = dst_rect.size.y;
				width_texture = texture_size.y;
				first_section_size = topleft.y;
				last_section_size = bottomright.y;
			} break;
			case FILL_CLOCKWISE:
			case FILL_CLOCKWISE_AND_COUNTER_CLOCKWISE:
			case FILL_COUNTER_CLOCKWISE: {
				// Those modes are circular, not relevant for nine patch.
			} break;
			case FILL_MODE_MAX:
				break;
		}

		double width_filled = width_total * p_ratio;
		double middle_section_size = MAX(0.0, width_texture - first_section_size - last_section_size);

		// Maximum middle texture size.
		double max_middle_texture_size = middle_section_size;

		// Maximum real middle texture size.
		double max_middle_real_size = MAX(0.0, width_total - (first_section_size + last_section_size));

		switch (p_mode) {
			case FILL_BILINEAR_LEFT_AND_RIGHT:
			case FILL_BILINEAR_TOP_AND_BOTTOM: {
				last_section_size = MAX(0.0, last_section_size - (width_total - width_filled) * 0.5);
				first_section_size = MAX(0.0, first_section_size - (width_total - width_filled) * 0.5);

				// When `width_filled` increases, `middle_section_size` only increases when either of `first_section_size` and `last_section_size` is zero.
				// Also, it should always be smaller than or equal to `(width_total - (first_section_size + last_section_size))`.
				double real_middle_size = width_filled - first_section_size - last_section_size;
				middle_section_size *= MIN(max_middle_real_size, real_middle_size) / max_middle_real_size;

				width_texture = MIN(width_texture, first_section_size + middle_section_size + last_section_size);
			} break;
			case FILL_MODE_MAX:
				break;
			default: {
				middle_section_size *= MIN(1.0, (MAX(0.0, width_filled - first_section_size) / MAX(1.0, width_total - first_section_size - last_section_size)));
				last_section_size = MAX(0.0, last_section_size - (width_total - width_filled));
				first_section_size = MIN(first_section_size, width_filled);
				width_texture = MIN(width_texture, first_section_size + middle_section_size + last_section_size);
			}
		}

		switch (p_mode) {
			case FILL_LEFT_TO_RIGHT: {
				src_rect.size.x = width_texture;
				dst_rect.size.x = width_filled;
				topleft.x = first_section_size;
				bottomright.x = last_section_size;
			} break;
			case FILL_RIGHT_TO_LEFT: {
				src_rect.position.x += src_rect.size.x - width_texture;
				src_rect.size.x = width_texture;
				dst_rect.position.x += width_total - width_filled;
				dst_rect.size.x = width_filled;
				topleft.x = last_section_size;
				bottomright.x = first_section_size;
			} break;
			case FILL_TOP_TO_BOTTOM: {
				src_rect.size.y = width_texture;
				dst_rect.size.y = width_filled;
				bottomright.y = last_section_size;
				topleft.y = first_section_size;
			} break;
			case FILL_BOTTOM_TO_TOP: {
				src_rect.position.y += src_rect.size.y - width_texture;
				src_rect.size.y = width_texture;
				dst_rect.position.y += width_total - width_filled;
				dst_rect.size.y = width_filled;
				topleft.y = last_section_size;
				bottomright.y = first_section_size;
			} break;
			case FILL_BILINEAR_LEFT_AND_RIGHT: {
				double center_mapped_from_real_width = (width_total * 0.5 - topleft.x) / max_middle_real_size * max_middle_texture_size + topleft.x;
				double drift_from_unscaled_center = 0;
				if (bottomright.y != topleft.y) { // To avoid division by zero.
					drift_from_unscaled_center = (src_rect.size.x * 0.5 - center_mapped_from_real_width) * (last_section_size - first_section_size) / (bottomright.x - topleft.x);
				}

				src_rect.position.x += center_mapped_from_real_width + drift_from_unscaled_center - width_texture * 0.5;
				src_rect.size.x = width_texture;
				dst_rect.position.x += (width_total - width_filled) * 0.5;
				dst_rect.size.x = width_filled;
				topleft.x = first_section_size;
				bottomright.x = last_section_size;
			} break;
			case FILL_BILINEAR_TOP_AND_BOTTOM: {
				double center_mapped_from_real_width = (width_total * 0.5 - topleft.y) / max_middle_real_size * max_middle_texture_size + topleft.y;
				double drift_from_unscaled_center = 0;
				if (bottomright.y != topleft.y) { // To avoid division by zero.
					drift_from_unscaled_center = (src_rect.size.y * 0.5 - center_mapped_from_real_width) * (last_section_size - first_section_size) / (bottomright.y - topleft.y);
				}

				src_rect.position.y += center_mapped_from_real_width + drift_from_unscaled_center - width_texture * 0.5;
				src_rect.size.y = width_texture;
				dst_rect.position.y += (width_total - width_filled) * 0.5;
				dst_rect.size.y = width_filled;
				topleft.y = first_section_size;
				bottomright.y = last_section_size;
			} break;
			case FILL_CLOCKWISE:
			case FILL_CLOCKWISE_AND_COUNTER_CLOCKWISE:
			case FILL_COUNTER_CLOCKWISE: {
				// Those modes are circular, not relevant for nine patch.
			} break;
			case FILL_MODE_MAX:
				break;
		}
	}

	if (p_texture == progress) {
		dst_rect.position += progress_offset;
	}
	p_texture->get_rect_region(dst_rect, src_rect, dst_rect, src_rect);

	RID ci = get_canvas_item();
	VS::get_singleton()->canvas_item_add_nine_patch(ci, dst_rect, src_rect, p_texture->get_rid(), topleft, bottomright, VS::NINE_PATCH_STRETCH, VS::NINE_PATCH_STRETCH, true, p_modulate);
}

void TextureProgress::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			if (nine_patch_stretch && (mode == FILL_LEFT_TO_RIGHT || mode == FILL_RIGHT_TO_LEFT || mode == FILL_TOP_TO_BOTTOM || mode == FILL_BOTTOM_TO_TOP || mode == FILL_BILINEAR_LEFT_AND_RIGHT || mode == FILL_BILINEAR_TOP_AND_BOTTOM)) {
				if (under.is_valid()) {
					draw_nine_patch_stretched(under, mode, 1.0, tint_under);
				}
				if (progress.is_valid()) {
					draw_nine_patch_stretched(progress, mode, get_as_ratio(), tint_progress);
				}
				if (over.is_valid()) {
					draw_nine_patch_stretched(over, mode, 1.0, tint_over);
				}
			} else {
				if (under.is_valid()) {
					switch (mode) {
						case FILL_CLOCKWISE:
						case FILL_COUNTER_CLOCKWISE:
						case FILL_CLOCKWISE_AND_COUNTER_CLOCKWISE: {
							if (nine_patch_stretch) {
								Rect2 region = Rect2(Point2(), get_size());
								draw_texture_rect(under, region, false, tint_under);
							} else {
								draw_texture(under, Point2(), tint_under);
							}
						} break;
						case FILL_MODE_MAX:
							break;
						default:
							draw_texture(under, Point2(), tint_under);
					}
				}
				if (progress.is_valid()) {
					Size2 s = progress->get_size();
					switch (mode) {
						case FILL_LEFT_TO_RIGHT: {
							Rect2 region = Rect2(progress_offset, Size2(s.x * get_as_ratio(), s.y));
							Rect2 source = Rect2(Point2(), Size2(s.x * get_as_ratio(), s.y));
							draw_texture_rect_region(progress, region, source, tint_progress);
						} break;
						case FILL_RIGHT_TO_LEFT: {
							Rect2 region = Rect2(progress_offset + Point2(s.x - s.x * get_as_ratio(), 0), Size2(s.x * get_as_ratio(), s.y));
							Rect2 source = Rect2(Point2(s.x - s.x * get_as_ratio(), 0), Size2(s.x * get_as_ratio(), s.y));
							draw_texture_rect_region(progress, region, source, tint_progress);
						} break;
						case FILL_TOP_TO_BOTTOM: {
							Rect2 region = Rect2(progress_offset + Point2(), Size2(s.x, s.y * get_as_ratio()));
							Rect2 source = Rect2(Point2(), Size2(s.x, s.y * get_as_ratio()));
							draw_texture_rect_region(progress, region, source, tint_progress);
						} break;
						case FILL_BOTTOM_TO_TOP: {
							Rect2 region = Rect2(progress_offset + Point2(0, s.y - s.y * get_as_ratio()), Size2(s.x, s.y * get_as_ratio()));
							Rect2 source = Rect2(Point2(0, s.y - s.y * get_as_ratio()), Size2(s.x, s.y * get_as_ratio()));
							draw_texture_rect_region(progress, region, source, tint_progress);
						} break;
						case FILL_CLOCKWISE:
						case FILL_COUNTER_CLOCKWISE:
						case FILL_CLOCKWISE_AND_COUNTER_CLOCKWISE: {
							if (nine_patch_stretch) {
								s = get_size();
							}

							float val = get_as_ratio() * rad_max_degrees / 360;
							if (val == 1) {
								Rect2 region = Rect2(progress_offset, s);
								Rect2 source = Rect2(Point2(), progress->get_size());
								draw_texture_rect_region(progress, region, source, tint_progress);
							} else if (val != 0) {
								Array pts;
								float direction = mode == FILL_COUNTER_CLOCKWISE ? -1 : 1;
								float start;

								if (mode == FILL_CLOCKWISE_AND_COUNTER_CLOCKWISE) {
									start = rad_init_angle / 360 - val / 2;
								} else {
									start = rad_init_angle / 360;
								}

								float end = start + direction * val;
								float from = MIN(start, end);
								float to = MAX(start, end);
								pts.append(from);
								for (float corner = Math::floor(from * 4 + 0.5) * 0.25 + 0.125; corner < to; corner += 0.25) {
									pts.append(corner);
								}
								pts.append(to);

								Ref<AtlasTexture> atlas_progress = progress;
								bool valid_atlas_progress = atlas_progress.is_valid() && atlas_progress->get_atlas().is_valid();
								Rect2 region_rect;
								Size2 atlas_size;
								if (valid_atlas_progress) {
									region_rect = atlas_progress->get_region();
									atlas_size = atlas_progress->get_atlas()->get_size();
								}

								Vector<Point2> uvs;
								Vector<Point2> points;
								for (int i = 0; i < pts.size(); i++) {
									Point2 uv = unit_val_to_uv(pts[i]);
									if (uvs.find(uv) >= 0) {
										continue;
									}
									points.push_back(progress_offset + Point2(uv.x * s.x, uv.y * s.y));
									if (valid_atlas_progress) {
										uv.x = Math::range_lerp(uv.x, 0, 1, region_rect.position.x / atlas_size.x, (region_rect.position.x + region_rect.size.x) / atlas_size.x);
										uv.y = Math::range_lerp(uv.y, 0, 1, region_rect.position.y / atlas_size.y, (region_rect.position.y + region_rect.size.y) / atlas_size.y);
									}
									uvs.push_back(uv);
								}

								Point2 center_point = get_relative_center();
								points.push_back(progress_offset + s * center_point);
								if (valid_atlas_progress) {
									center_point.x = Math::range_lerp(center_point.x, 0, 1, region_rect.position.x / atlas_size.x, (region_rect.position.x + region_rect.size.x) / atlas_size.x);
									center_point.y = Math::range_lerp(center_point.y, 0, 1, region_rect.position.y / atlas_size.y, (region_rect.position.y + region_rect.size.y) / atlas_size.y);
								}
								uvs.push_back(center_point);
								Vector<Color> colors;
								colors.push_back(tint_progress);
								draw_polygon(points, colors, uvs, progress);
							}

							// Draw a reference cross.
							if (Engine::get_singleton()->is_editor_hint()) {
								Point2 p;

								if (nine_patch_stretch) {
									p = get_size();
								} else {
									p = progress->get_size();
								}

								p *= get_relative_center();
								p += progress_offset;
								p = p.floor();
								draw_line(p - Point2(8, 0), p + Point2(8, 0), Color(0.9, 0.5, 0.5), 2);
								draw_line(p - Point2(0, 8), p + Point2(0, 8), Color(0.9, 0.5, 0.5), 2);
							}
						} break;
						case FILL_BILINEAR_LEFT_AND_RIGHT: {
							Rect2 region = Rect2(progress_offset + Point2(s.x / 2 - s.x * get_as_ratio() / 2, 0), Size2(s.x * get_as_ratio(), s.y));
							Rect2 source = Rect2(Point2(s.x / 2 - s.x * get_as_ratio() / 2, 0), Size2(s.x * get_as_ratio(), s.y));
							draw_texture_rect_region(progress, region, source, tint_progress);
						} break;
						case FILL_BILINEAR_TOP_AND_BOTTOM: {
							Rect2 region = Rect2(progress_offset + Point2(0, s.y / 2 - s.y * get_as_ratio() / 2), Size2(s.x, s.y * get_as_ratio()));
							Rect2 source = Rect2(Point2(0, s.y / 2 - s.y * get_as_ratio() / 2), Size2(s.x, s.y * get_as_ratio()));
							draw_texture_rect_region(progress, region, source, tint_progress);
						} break;
						case FILL_MODE_MAX:
							break;
						default:
							draw_texture_rect_region(progress, Rect2(progress_offset, Size2(s.x * get_as_ratio(), s.y)), Rect2(Point2(), Size2(s.x * get_as_ratio(), s.y)), tint_progress);
					}
				}
				if (over.is_valid()) {
					switch (mode) {
						case FILL_CLOCKWISE:
						case FILL_COUNTER_CLOCKWISE:
						case FILL_CLOCKWISE_AND_COUNTER_CLOCKWISE: {
							if (nine_patch_stretch) {
								Rect2 region = Rect2(Point2(), get_size());
								draw_texture_rect(over, region, false, tint_over);
							} else {
								draw_texture(over, Point2(), tint_over);
							}
						} break;
						case FILL_MODE_MAX:
							break;
						default:
							draw_texture(over, Point2(), tint_over);
					}
				}
			}

		} break;
	}
}

void TextureProgress::set_fill_mode(int p_fill) {
	ERR_FAIL_INDEX(p_fill, FILL_MODE_MAX);
	mode = (FillMode)p_fill;
	update();
}

int TextureProgress::get_fill_mode() {
	return mode;
}

void TextureProgress::set_radial_initial_angle(float p_angle) {
	ERR_FAIL_COND_MSG(!isfinite(p_angle), "Angle is non-finite.");

	if (p_angle < 0.0 || p_angle > 360.0) {
		p_angle = Math::fposmod(p_angle, 360.0f);
	}

	rad_init_angle = p_angle;
	update();
}

float TextureProgress::get_radial_initial_angle() {
	return rad_init_angle;
}

void TextureProgress::set_fill_degrees(float p_angle) {
	rad_max_degrees = CLAMP(p_angle, 0, 360);
	update();
}

float TextureProgress::get_fill_degrees() {
	return rad_max_degrees;
}

void TextureProgress::set_radial_center_offset(const Point2 &p_off) {
	rad_center_off = p_off;
	update();
}

Point2 TextureProgress::get_radial_center_offset() {
	return rad_center_off;
}

void TextureProgress::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_under_texture", "tex"), &TextureProgress::set_under_texture);
	ClassDB::bind_method(D_METHOD("get_under_texture"), &TextureProgress::get_under_texture);

	ClassDB::bind_method(D_METHOD("set_progress_texture", "tex"), &TextureProgress::set_progress_texture);
	ClassDB::bind_method(D_METHOD("get_progress_texture"), &TextureProgress::get_progress_texture);

	ClassDB::bind_method(D_METHOD("set_over_texture", "tex"), &TextureProgress::set_over_texture);
	ClassDB::bind_method(D_METHOD("get_over_texture"), &TextureProgress::get_over_texture);

	ClassDB::bind_method(D_METHOD("set_fill_mode", "mode"), &TextureProgress::set_fill_mode);
	ClassDB::bind_method(D_METHOD("get_fill_mode"), &TextureProgress::get_fill_mode);

	ClassDB::bind_method(D_METHOD("set_tint_under", "tint"), &TextureProgress::set_tint_under);
	ClassDB::bind_method(D_METHOD("get_tint_under"), &TextureProgress::get_tint_under);

	ClassDB::bind_method(D_METHOD("set_tint_progress", "tint"), &TextureProgress::set_tint_progress);
	ClassDB::bind_method(D_METHOD("get_tint_progress"), &TextureProgress::get_tint_progress);

	ClassDB::bind_method(D_METHOD("set_tint_over", "tint"), &TextureProgress::set_tint_over);
	ClassDB::bind_method(D_METHOD("get_tint_over"), &TextureProgress::get_tint_over);

	ClassDB::bind_method(D_METHOD("set_texture_progress_offset", "offset"), &TextureProgress::set_progress_offset);
	ClassDB::bind_method(D_METHOD("get_texture_progress_offset"), &TextureProgress::get_progress_offset);

	ClassDB::bind_method(D_METHOD("set_radial_initial_angle", "mode"), &TextureProgress::set_radial_initial_angle);
	ClassDB::bind_method(D_METHOD("get_radial_initial_angle"), &TextureProgress::get_radial_initial_angle);

	ClassDB::bind_method(D_METHOD("set_radial_center_offset", "mode"), &TextureProgress::set_radial_center_offset);
	ClassDB::bind_method(D_METHOD("get_radial_center_offset"), &TextureProgress::get_radial_center_offset);

	ClassDB::bind_method(D_METHOD("set_fill_degrees", "mode"), &TextureProgress::set_fill_degrees);
	ClassDB::bind_method(D_METHOD("get_fill_degrees"), &TextureProgress::get_fill_degrees);

	ClassDB::bind_method(D_METHOD("set_stretch_margin", "margin", "value"), &TextureProgress::set_stretch_margin);
	ClassDB::bind_method(D_METHOD("get_stretch_margin", "margin"), &TextureProgress::get_stretch_margin);

	ClassDB::bind_method(D_METHOD("set_nine_patch_stretch", "stretch"), &TextureProgress::set_nine_patch_stretch);
	ClassDB::bind_method(D_METHOD("get_nine_patch_stretch"), &TextureProgress::get_nine_patch_stretch);

	ADD_GROUP("Textures", "texture_");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture_under", PROPERTY_HINT_RESOURCE_TYPE, "Texture"), "set_under_texture", "get_under_texture");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture_over", PROPERTY_HINT_RESOURCE_TYPE, "Texture"), "set_over_texture", "get_over_texture");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture_progress", PROPERTY_HINT_RESOURCE_TYPE, "Texture"), "set_progress_texture", "get_progress_texture");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "texture_progress_offset"), "set_texture_progress_offset", "get_texture_progress_offset");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "fill_mode", PROPERTY_HINT_ENUM, "Left to Right,Right to Left,Top to Bottom,Bottom to Top,Clockwise,Counter Clockwise,Bilinear (Left and Right),Bilinear (Top and Bottom),Clockwise and Counter Clockwise"), "set_fill_mode", "get_fill_mode");
	ADD_GROUP("Tint", "tint_");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "tint_under"), "set_tint_under", "get_tint_under");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "tint_over"), "set_tint_over", "get_tint_over");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "tint_progress"), "set_tint_progress", "get_tint_progress");
	ADD_GROUP("Radial Fill", "radial_");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "radial_initial_angle", PROPERTY_HINT_RANGE, "0.0,360.0,0.1,slider"), "set_radial_initial_angle", "get_radial_initial_angle");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "radial_fill_degrees", PROPERTY_HINT_RANGE, "0.0,360.0,0.1,slider"), "set_fill_degrees", "get_fill_degrees");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "radial_center_offset"), "set_radial_center_offset", "get_radial_center_offset");
	ADD_GROUP("Stretch", "stretch_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "nine_patch_stretch"), "set_nine_patch_stretch", "get_nine_patch_stretch");
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "stretch_margin_left", PROPERTY_HINT_RANGE, "0,16384,1"), "set_stretch_margin", "get_stretch_margin", MARGIN_LEFT);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "stretch_margin_top", PROPERTY_HINT_RANGE, "0,16384,1"), "set_stretch_margin", "get_stretch_margin", MARGIN_TOP);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "stretch_margin_right", PROPERTY_HINT_RANGE, "0,16384,1"), "set_stretch_margin", "get_stretch_margin", MARGIN_RIGHT);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "stretch_margin_bottom", PROPERTY_HINT_RANGE, "0,16384,1"), "set_stretch_margin", "get_stretch_margin", MARGIN_BOTTOM);

	BIND_ENUM_CONSTANT(FILL_LEFT_TO_RIGHT);
	BIND_ENUM_CONSTANT(FILL_RIGHT_TO_LEFT);
	BIND_ENUM_CONSTANT(FILL_TOP_TO_BOTTOM);
	BIND_ENUM_CONSTANT(FILL_BOTTOM_TO_TOP);
	BIND_ENUM_CONSTANT(FILL_CLOCKWISE);
	BIND_ENUM_CONSTANT(FILL_COUNTER_CLOCKWISE);
	BIND_ENUM_CONSTANT(FILL_BILINEAR_LEFT_AND_RIGHT);
	BIND_ENUM_CONSTANT(FILL_BILINEAR_TOP_AND_BOTTOM);
	BIND_ENUM_CONSTANT(FILL_CLOCKWISE_AND_COUNTER_CLOCKWISE);
}

TextureProgress::TextureProgress() {
	mode = FILL_LEFT_TO_RIGHT;
	rad_init_angle = 0;
	rad_center_off = Point2();
	rad_max_degrees = 360;
	set_mouse_filter(MOUSE_FILTER_PASS);

	nine_patch_stretch = false;
	stretch_margin[MARGIN_LEFT] = 0;
	stretch_margin[MARGIN_RIGHT] = 0;
	stretch_margin[MARGIN_BOTTOM] = 0;
	stretch_margin[MARGIN_TOP] = 0;

	tint_under = tint_progress = tint_over = Color(1, 1, 1);
}
