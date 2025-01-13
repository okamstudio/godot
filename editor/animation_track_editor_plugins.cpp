/**************************************************************************/
/*  animation_track_editor_plugins.cpp                                    */
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

#include "animation_track_editor_plugins.h"

#include "editor/audio_stream_preview.h"
#include "editor/editor_resource_preview.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/themes/editor_scale.h"
#include "scene/2d/animated_sprite_2d.h"
#include "scene/2d/sprite_2d.h"
#include "scene/3d/sprite_3d.h"
#include "scene/animation/animation_player.h"
#include "servers/audio/audio_stream.h"

/// BOOL ///
int AnimationTrackEditBool::get_key_height() const {
	Ref<Texture2D> checked = get_theme_icon(SNAME("checked"), SNAME("CheckBox"));
	return checked->get_height();
}

Rect2 AnimationTrackEditBool::get_key_rect(int p_index, float p_pixels_sec) {
	Ref<Texture2D> checked = get_theme_icon(SNAME("checked"), SNAME("CheckBox"));
	return Rect2(-checked->get_width() / 2, 0, checked->get_width(), get_size().height);
}

bool AnimationTrackEditBool::is_key_selectable_by_distance() const {
	return false;
}

void AnimationTrackEditBool::draw_key(int p_index, float p_pixels_sec, int p_x, bool p_selected, int p_clip_left, int p_clip_right) {
	bool checked = get_animation()->track_get_key_value(get_track(), p_index);
	Ref<Texture2D> icon = get_theme_icon(checked ? "checked" : "unchecked", "CheckBox");

	Vector2 ofs(p_x - icon->get_width() / 2, int(get_size().height - icon->get_height()) / 2);

	if (ofs.x + icon->get_width() / 2 < p_clip_left) {
		return;
	}

	if (ofs.x + icon->get_width() / 2 > p_clip_right) {
		return;
	}

	draw_texture(icon, ofs);

	if (p_selected) {
		Color color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
		draw_rect_clipped(Rect2(ofs, icon->get_size()), color, false);
	}
}

/// COLOR ///

int AnimationTrackEditColor::get_key_height() const {
	Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
	int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
	return font->get_height(font_size) * 0.8;
}

Rect2 AnimationTrackEditColor::get_key_rect(int p_index, float p_pixels_sec) {
	Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
	int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
	int fh = font->get_height(font_size) * 0.8;
	return Rect2(-fh / 2, 0, fh, get_size().height);
}

bool AnimationTrackEditColor::is_key_selectable_by_distance() const {
	return false;
}

void AnimationTrackEditColor::draw_key_link(int p_index, float p_pixels_sec, int p_x, int p_next_x, int p_clip_left, int p_clip_right) {
	Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
	int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
	int fh = (font->get_height(font_size) * 0.8);

	fh /= 3;

	int x_from = p_x + fh / 2 - 1;
	int x_to = p_next_x - fh / 2 + 1;
	x_from = MAX(x_from, p_clip_left);
	x_to = MIN(x_to, p_clip_right);

	int y_from = (get_size().height - fh) / 2;

	if (x_from > p_clip_right || x_to < p_clip_left) {
		return;
	}

	Vector<Color> color_samples;
	color_samples.append(get_animation()->track_get_key_value(get_track(), p_index));

	if (get_animation()->track_get_type(get_track()) == Animation::TYPE_VALUE) {
		if (get_animation()->track_get_interpolation_type(get_track()) != Animation::INTERPOLATION_NEAREST &&
				(get_animation()->value_track_get_update_mode(get_track()) == Animation::UPDATE_CONTINUOUS ||
						get_animation()->value_track_get_update_mode(get_track()) == Animation::UPDATE_CAPTURE) &&
				!Math::is_zero_approx(get_animation()->track_get_key_transition(get_track(), p_index))) {
			float start_time = get_animation()->track_get_key_time(get_track(), p_index);
			float end_time = get_animation()->track_get_key_time(get_track(), p_index + 1);

			Color color_next = get_animation()->value_track_interpolate(get_track(), end_time);

			if (!color_samples[0].is_equal_approx(color_next)) {
				color_samples.resize(1 + (x_to - x_from) / 64); // Make a color sample every 64 px.
				for (int i = 1; i < color_samples.size(); i++) {
					float j = i;
					color_samples.write[i] = get_animation()->value_track_interpolate(
							get_track(),
							Math::lerp(start_time, end_time, j / color_samples.size()));
				}
			}
			color_samples.append(color_next);
		} else {
			color_samples.append(color_samples[0]);
		}
	} else {
		color_samples.append(get_animation()->track_get_key_value(get_track(), p_index + 1));
	}

	for (int i = 0; i < color_samples.size() - 1; i++) {
		Vector<Vector2> points = {
			Vector2(Math::lerp(x_from, x_to, float(i) / (color_samples.size() - 1)), y_from),
			Vector2(Math::lerp(x_from, x_to, float(i + 1) / (color_samples.size() - 1)), y_from),
			Vector2(Math::lerp(x_from, x_to, float(i + 1) / (color_samples.size() - 1)), y_from + fh),
			Vector2(Math::lerp(x_from, x_to, float(i) / (color_samples.size() - 1)), y_from + fh)
		};

		Vector<Color> colors = {
			color_samples[i],
			color_samples[i + 1],
			color_samples[i + 1],
			color_samples[i]
		};

		draw_primitive(points, colors, Vector<Vector2>());
	}
}

void AnimationTrackEditColor::draw_key(int p_index, float p_pixels_sec, int p_x, bool p_selected, int p_clip_left, int p_clip_right) {
	Color color = get_animation()->track_get_key_value(get_track(), p_index);

	Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
	int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
	int fh = font->get_height(font_size) * 0.8;

	Rect2 rect(Vector2(p_x - fh / 2, int(get_size().height - fh) / 2), Size2(fh, fh));

	draw_rect_clipped(Rect2(rect.position, rect.size / 2), Color(0.4, 0.4, 0.4));
	draw_rect_clipped(Rect2(rect.position + rect.size / 2, rect.size / 2), Color(0.4, 0.4, 0.4));
	draw_rect_clipped(Rect2(rect.position + Vector2(rect.size.x / 2, 0), rect.size / 2), Color(0.6, 0.6, 0.6));
	draw_rect_clipped(Rect2(rect.position + Vector2(0, rect.size.y / 2), rect.size / 2), Color(0.6, 0.6, 0.6));
	draw_rect_clipped(rect, color);

	if (p_selected) {
		Color accent = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
		draw_rect_clipped(rect, accent, false);
	}
}

/// AUDIO ///

void AnimationTrackEditAudio::_preview_changed(ObjectID p_which) {
	Object *object = ObjectDB::get_instance(id);

	if (!object) {
		return;
	}

	Ref<AudioStream> stream = object->call("get_stream");

	if (stream.is_valid() && stream->get_instance_id() == p_which) {
		queue_redraw();
	}
}

int AnimationTrackEditAudio::get_key_height() const {
	if (!ObjectDB::get_instance(id)) {
		return AnimationTrackEdit::get_key_height();
	}

	Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
	int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
	return int(font->get_height(font_size) * 1.5);
}

Rect2 AnimationTrackEditAudio::get_key_rect(int p_index, float p_pixels_sec) {
	Object *object = ObjectDB::get_instance(id);

	if (!object) {
		return AnimationTrackEdit::get_key_rect(p_index, p_pixels_sec);
	}

	Ref<AudioStream> stream = object->call("get_stream");

	if (stream.is_null()) {
		return AnimationTrackEdit::get_key_rect(p_index, p_pixels_sec);
	}

	bool play = get_animation()->track_get_key_value(get_track(), p_index);
	if (play) {
		float len = stream->get_length();

		if (len == 0) {
			Ref<AudioStreamPreview> preview = AudioStreamPreviewGenerator::get_singleton()->generate_preview(stream);
			len = preview->get_length();
		}

		if (get_animation()->track_get_key_count(get_track()) > p_index + 1) {
			len = MIN(len, get_animation()->track_get_key_time(get_track(), p_index + 1) - get_animation()->track_get_key_time(get_track(), p_index));
		}

		return Rect2(0, 0, len * p_pixels_sec, get_size().height);
	} else {
		Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
		int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
		int fh = font->get_height(font_size) * 0.8;
		return Rect2(0, 0, fh, get_size().height);
	}
}

bool AnimationTrackEditAudio::is_key_selectable_by_distance() const {
	return false;
}

void AnimationTrackEditAudio::draw_key(int p_index, float p_pixels_sec, int p_x, bool p_selected, int p_clip_left, int p_clip_right) {
	Object *object = ObjectDB::get_instance(id);

	if (!object) {
		AnimationTrackEdit::draw_key(p_index, p_pixels_sec, p_x, p_selected, p_clip_left, p_clip_right);
		return;
	}

	Ref<AudioStream> stream = object->call("get_stream");

	if (stream.is_null()) {
		AnimationTrackEdit::draw_key(p_index, p_pixels_sec, p_x, p_selected, p_clip_left, p_clip_right);
		return;
	}

	bool play = get_animation()->track_get_key_value(get_track(), p_index);
	if (play) {
		float len = stream->get_length();

		Ref<AudioStreamPreview> preview = AudioStreamPreviewGenerator::get_singleton()->generate_preview(stream);

		float preview_len = preview->get_length();

		if (len == 0) {
			len = preview_len;
		}

		int pixel_len = len * p_pixels_sec;

		int pixel_begin = p_x;
		int pixel_end = p_x + pixel_len;

		if (pixel_end < p_clip_left) {
			return;
		}

		if (pixel_begin > p_clip_right) {
			return;
		}

		int from_x = MAX(pixel_begin, p_clip_left);
		int to_x = MIN(pixel_end, p_clip_right);

		if (get_animation()->track_get_key_count(get_track()) > p_index + 1) {
			float limit = MIN(len, get_animation()->track_get_key_time(get_track(), p_index + 1) - get_animation()->track_get_key_time(get_track(), p_index));
			int limit_x = pixel_begin + limit * p_pixels_sec;
			to_x = MIN(limit_x, to_x);
		}

		if (to_x <= from_x) {
			return;
		}

		Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
		int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
		float fh = int(font->get_height(font_size) * 1.5);
		Rect2 rect = Rect2(from_x, (get_size().height - fh) / 2, to_x - from_x, fh);
		draw_rect(rect, Color(0.25, 0.25, 0.25));

		Vector<Vector2> points;
		points.resize((to_x - from_x) * 2);
		preview_len = preview->get_length();

		for (int i = from_x; i < to_x; i++) {
			float ofs = (i - pixel_begin) * preview_len / pixel_len;
			float ofs_n = ((i + 1) - pixel_begin) * preview_len / pixel_len;
			float max = preview->get_max(ofs, ofs_n) * 0.5 + 0.5;
			float min = preview->get_min(ofs, ofs_n) * 0.5 + 0.5;

			int idx = i - from_x;
			points.write[idx * 2 + 0] = Vector2(i, rect.position.y + min * rect.size.y);
			points.write[idx * 2 + 1] = Vector2(i, rect.position.y + max * rect.size.y);
		}

		Vector<Color> colors = { Color(0.75, 0.75, 0.75) };

		RS::get_singleton()->canvas_item_add_multiline(get_canvas_item(), points, colors);

		if (p_selected) {
			Color accent = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
			draw_rect(rect, accent, false);
		}
	} else {
		Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
		int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
		int fh = font->get_height(font_size) * 0.8;
		Rect2 rect(Vector2(p_x, int(get_size().height - fh) / 2), Size2(fh, fh));

		Color color = get_theme_color(SceneStringName(font_color), SNAME("Label"));
		draw_rect_clipped(rect, color);

		if (p_selected) {
			Color accent = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
			draw_rect_clipped(rect, accent, false);
		}
	}
}

void AnimationTrackEditAudio::set_node(Object *p_object) {
	id = p_object->get_instance_id();
}

AnimationTrackEditAudio::AnimationTrackEditAudio() {
	AudioStreamPreviewGenerator::get_singleton()->connect("preview_updated", callable_mp(this, &AnimationTrackEditAudio::_preview_changed));
}

/// SPRITE FRAME / FRAME_COORDS ///

int AnimationTrackEditSpriteFrame::get_key_height() const {
	if (!ObjectDB::get_instance(id)) {
		return AnimationTrackEdit::get_key_height();
	}

	Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
	int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
	return int(font->get_height(font_size) * 2);
}

Rect2 AnimationTrackEditSpriteFrame::get_key_rect(int p_index, float p_pixels_sec) {
	Object *object = ObjectDB::get_instance(id);

	if (!object) {
		return AnimationTrackEdit::get_key_rect(p_index, p_pixels_sec);
	}

	Size2 size;

	if (Object::cast_to<Sprite2D>(object) || Object::cast_to<Sprite3D>(object)) {
		Ref<Texture2D> texture = object->call("get_texture");
		if (texture.is_null()) {
			return AnimationTrackEdit::get_key_rect(p_index, p_pixels_sec);
		}

		size = texture->get_size();

		if (bool(object->call("is_region_enabled"))) {
			size = Rect2(object->call("get_region_rect")).size;
		}

		int hframes = object->call("get_hframes");
		int vframes = object->call("get_vframes");

		if (hframes > 1) {
			size.x /= hframes;
		}
		if (vframes > 1) {
			size.y /= vframes;
		}
	} else if (Object::cast_to<AnimatedSprite2D>(object) || Object::cast_to<AnimatedSprite3D>(object)) {
		Ref<SpriteFrames> sf = object->call("get_sprite_frames");
		if (sf.is_null()) {
			return AnimationTrackEdit::get_key_rect(p_index, p_pixels_sec);
		}

		List<StringName> animations;
		sf->get_animation_list(&animations);

		int frame = get_animation()->track_get_key_value(get_track(), p_index);
		String animation_name;
		if (animations.size() == 1) {
			animation_name = animations.front()->get();
		} else {
			// Go through other track to find if animation is set
			String animation_path = get_animation()->track_get_path(get_track());
			animation_path = animation_path.replace(":frame", ":animation");
			int animation_track = get_animation()->find_track(animation_path, get_animation()->track_get_type(get_track()));
			float track_time = get_animation()->track_get_key_time(get_track(), p_index);
			int animation_index = get_animation()->track_find_key(animation_track, track_time);
			animation_name = get_animation()->track_get_key_value(animation_track, animation_index);
		}

		Ref<Texture2D> texture = sf->get_frame_texture(animation_name, frame);
		if (texture.is_null()) {
			return AnimationTrackEdit::get_key_rect(p_index, p_pixels_sec);
		}

		size = texture->get_size();
	}

	size = size.floor();

	Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
	int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
	int height = int(font->get_height(font_size) * 2);
	int width = height * size.width / size.height;

	return Rect2(0, 0, width, get_size().height);
}

bool AnimationTrackEditSpriteFrame::is_key_selectable_by_distance() const {
	return false;
}

void AnimationTrackEditSpriteFrame::draw_key(int p_index, float p_pixels_sec, int p_x, bool p_selected, int p_clip_left, int p_clip_right) {
	Object *object = ObjectDB::get_instance(id);

	if (!object) {
		AnimationTrackEdit::draw_key(p_index, p_pixels_sec, p_x, p_selected, p_clip_left, p_clip_right);
		return;
	}

	Ref<Texture2D> texture;
	Rect2 region;

	if (Object::cast_to<Sprite2D>(object) || Object::cast_to<Sprite3D>(object)) {
		texture = object->call("get_texture");
		if (texture.is_null()) {
			AnimationTrackEdit::draw_key(p_index, p_pixels_sec, p_x, p_selected, p_clip_left, p_clip_right);
			return;
		}

		int hframes = object->call("get_hframes");
		int vframes = object->call("get_vframes");

		Vector2 coords;
		if (is_coords) {
			coords = get_animation()->track_get_key_value(get_track(), p_index);
		} else {
			int frame = get_animation()->track_get_key_value(get_track(), p_index);
			coords.x = frame % hframes;
			coords.y = frame / hframes;
		}

		region.size = texture->get_size();

		if (bool(object->call("is_region_enabled"))) {
			region = Rect2(object->call("get_region_rect"));
		}

		if (hframes > 1) {
			region.size.x /= hframes;
		}
		if (vframes > 1) {
			region.size.y /= vframes;
		}

		region.position.x += region.size.x * coords.x;
		region.position.y += region.size.y * coords.y;

	} else if (Object::cast_to<AnimatedSprite2D>(object) || Object::cast_to<AnimatedSprite3D>(object)) {
		Ref<SpriteFrames> sf = object->call("get_sprite_frames");
		if (sf.is_null()) {
			AnimationTrackEdit::draw_key(p_index, p_pixels_sec, p_x, p_selected, p_clip_left, p_clip_right);
			return;
		}

		List<StringName> animations;
		sf->get_animation_list(&animations);

		int frame = get_animation()->track_get_key_value(get_track(), p_index);
		String animation_name;
		if (animations.size() == 1) {
			animation_name = animations.front()->get();
		} else {
			// Go through other track to find if animation is set
			String animation_path = get_animation()->track_get_path(get_track());
			animation_path = animation_path.replace(":frame", ":animation");
			int animation_track = get_animation()->find_track(animation_path, get_animation()->track_get_type(get_track()));
			float track_time = get_animation()->track_get_key_time(get_track(), p_index);
			int animation_index = get_animation()->track_find_key(animation_track, track_time);
			animation_name = get_animation()->track_get_key_value(animation_track, animation_index);
		}

		texture = sf->get_frame_texture(animation_name, frame);
		if (texture.is_null()) {
			AnimationTrackEdit::draw_key(p_index, p_pixels_sec, p_x, p_selected, p_clip_left, p_clip_right);
			return;
		}

		region.size = texture->get_size();
	}

	Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
	int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
	int height = int(font->get_height(font_size) * 2);

	int width = height * region.size.width / region.size.height;

	Rect2 rect(p_x, int(get_size().height - height) / 2, width, height);

	if (rect.position.x + rect.size.x < p_clip_left) {
		return;
	}

	if (rect.position.x > p_clip_right) {
		return;
	}

	Color accent = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
	Color bg = accent;
	bg.a = 0.15;

	draw_rect_clipped(rect, bg);

	draw_texture_region_clipped(texture, rect, region);

	if (p_selected) {
		draw_rect_clipped(rect, accent, false);
	}
}

void AnimationTrackEditSpriteFrame::set_node(Object *p_object) {
	id = p_object->get_instance_id();
}

void AnimationTrackEditSpriteFrame::set_as_coords() {
	is_coords = true;
}

/// SUB ANIMATION ///

int AnimationTrackEditSubAnim::get_key_height() const {
	if (!ObjectDB::get_instance(id)) {
		return AnimationTrackEdit::get_key_height();
	}

	Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
	int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
	return int(font->get_height(font_size) * 1.5);
}

Rect2 AnimationTrackEditSubAnim::get_key_rect(int p_index, float p_pixels_sec) {
	Object *object = ObjectDB::get_instance(id);

	if (!object) {
		return AnimationTrackEdit::get_key_rect(p_index, p_pixels_sec);
	}

	AnimationPlayer *ap = Object::cast_to<AnimationPlayer>(object);

	if (!ap) {
		return AnimationTrackEdit::get_key_rect(p_index, p_pixels_sec);
	}

	String anim = get_animation()->track_get_key_value(get_track(), p_index);

	if (anim != "[stop]" && ap->has_animation(anim)) {
		float len = ap->get_animation(anim)->get_length();

		if (get_animation()->track_get_key_count(get_track()) > p_index + 1) {
			len = MIN(len, get_animation()->track_get_key_time(get_track(), p_index + 1) - get_animation()->track_get_key_time(get_track(), p_index));
		}

		return Rect2(0, 0, len * p_pixels_sec, get_size().height);
	} else {
		Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
		int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
		int fh = font->get_height(font_size) * 0.8;
		return Rect2(0, 0, fh, get_size().height);
	}
}

bool AnimationTrackEditSubAnim::is_key_selectable_by_distance() const {
	return false;
}

void AnimationTrackEditSubAnim::draw_key(int p_index, float p_pixels_sec, int p_x, bool p_selected, int p_clip_left, int p_clip_right) {
	Object *object = ObjectDB::get_instance(id);

	if (!object) {
		AnimationTrackEdit::draw_key(p_index, p_pixels_sec, p_x, p_selected, p_clip_left, p_clip_right);
		return;
	}

	AnimationPlayer *ap = Object::cast_to<AnimationPlayer>(object);

	if (!ap) {
		AnimationTrackEdit::draw_key(p_index, p_pixels_sec, p_x, p_selected, p_clip_left, p_clip_right);
		return;
	}

	String anim = get_animation()->track_get_key_value(get_track(), p_index);

	if (anim != "[stop]" && ap->has_animation(anim)) {
		float len = ap->get_animation(anim)->get_length();

		if (get_animation()->track_get_key_count(get_track()) > p_index + 1) {
			len = MIN(len, get_animation()->track_get_key_time(get_track(), p_index + 1) - get_animation()->track_get_key_time(get_track(), p_index));
		}

		int pixel_len = len * p_pixels_sec;

		int pixel_begin = p_x;
		int pixel_end = p_x + pixel_len;

		if (pixel_end < p_clip_left) {
			return;
		}

		if (pixel_begin > p_clip_right) {
			return;
		}

		int from_x = MAX(pixel_begin, p_clip_left);
		int to_x = MIN(pixel_end, p_clip_right);

		if (to_x <= from_x) {
			return;
		}

		Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
		int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
		int fh = font->get_height(font_size) * 1.5;

		Rect2 rect(from_x, int(get_size().height - fh) / 2, to_x - from_x, fh);

		Color color = get_theme_color(SceneStringName(font_color), SNAME("Label"));
		Color bg = color;
		bg.r = 1 - color.r;
		bg.g = 1 - color.g;
		bg.b = 1 - color.b;
		draw_rect(rect, bg);

		Vector<Vector2> points;
		Vector<Color> colors = { color };
		{
			Ref<Animation> ap_anim = ap->get_animation(anim);

			for (int i = 0; i < ap_anim->get_track_count(); i++) {
				float h = (rect.size.height - 2) / ap_anim->get_track_count();

				int y = 2 + h * i + h / 2;

				for (int j = 0; j < ap_anim->track_get_key_count(i); j++) {
					float ofs = ap_anim->track_get_key_time(i, j);
					int x = p_x + ofs * p_pixels_sec + 2;

					if (x < from_x || x >= (to_x - 4)) {
						continue;
					}

					points.push_back(Point2(x, y));
					points.push_back(Point2(x + 1, y));
				}
			}
		}

		if (points.size() > 2) {
			RS::get_singleton()->canvas_item_add_multiline(get_canvas_item(), points, colors);
		}

		int limit = to_x - from_x - 4;
		if (limit > 0) {
			draw_string(font, Point2(from_x + 2, int(get_size().height - font->get_height(font_size)) / 2 + font->get_ascent(font_size)), anim, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, color);
		}

		if (p_selected) {
			Color accent = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
			draw_rect(rect, accent, false);
		}
	} else {
		Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
		int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
		int fh = font->get_height(font_size) * 0.8;
		Rect2 rect(Vector2(p_x, int(get_size().height - fh) / 2), Size2(fh, fh));

		Color color = get_theme_color(SceneStringName(font_color), SNAME("Label"));
		draw_rect_clipped(rect, color);

		if (p_selected) {
			Color accent = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
			draw_rect_clipped(rect, accent, false);
		}
	}
}

void AnimationTrackEditSubAnim::set_node(Object *p_object) {
	id = p_object->get_instance_id();
}

//// VOLUME DB ////

int AnimationTrackEditVolumeDB::get_key_height() const {
	Ref<Texture2D> volume_texture = get_editor_theme_icon(SNAME("ColorTrackVu"));
	return volume_texture->get_height() * 1.2;
}

void AnimationTrackEditVolumeDB::draw_bg(int p_clip_left, int p_clip_right) {
	Ref<Texture2D> volume_texture = get_editor_theme_icon(SNAME("ColorTrackVu"));
	int tex_h = volume_texture->get_height();

	int y_from = (get_size().height - tex_h) / 2;
	int y_size = tex_h;

	Color color(1, 1, 1, 0.3);
	draw_texture_rect(volume_texture, Rect2(p_clip_left, y_from, p_clip_right - p_clip_left, y_from + y_size), false, color);
}

void AnimationTrackEditVolumeDB::draw_fg(int p_clip_left, int p_clip_right) {
	Ref<Texture2D> volume_texture = get_editor_theme_icon(SNAME("ColorTrackVu"));
	int tex_h = volume_texture->get_height();
	int y_from = (get_size().height - tex_h) / 2;
	int db0 = y_from + (24 / 80.0) * tex_h;

	draw_line(Vector2(p_clip_left, db0), Vector2(p_clip_right, db0), Color(1, 1, 1, 0.3));
}

void AnimationTrackEditVolumeDB::draw_key_link(int p_index, float p_pixels_sec, int p_x, int p_next_x, int p_clip_left, int p_clip_right) {
	if (p_x > p_clip_right || p_next_x < p_clip_left) {
		return;
	}

	float db = get_animation()->track_get_key_value(get_track(), p_index);
	float db_n = get_animation()->track_get_key_value(get_track(), p_index + 1);

	db = CLAMP(db, -60, 24);
	db_n = CLAMP(db_n, -60, 24);

	float h = 1.0 - ((db + 60) / 84.0);
	float h_n = 1.0 - ((db_n + 60) / 84.0);

	int from_x = p_x;
	int to_x = p_next_x;

	if (from_x < p_clip_left) {
		h = Math::lerp(h, h_n, float(p_clip_left - from_x) / float(to_x - from_x));
		from_x = p_clip_left;
	}

	if (to_x > p_clip_right) {
		h_n = Math::lerp(h, h_n, float(p_clip_right - from_x) / float(to_x - from_x));
		to_x = p_clip_right;
	}

	Ref<Texture2D> volume_texture = get_editor_theme_icon(SNAME("ColorTrackVu"));
	int tex_h = volume_texture->get_height();

	int y_from = (get_size().height - tex_h) / 2;

	Color color = get_theme_color(SceneStringName(font_color), SNAME("Label"));
	color.a *= 0.7;

	draw_line(Point2(from_x, y_from + h * tex_h), Point2(to_x, y_from + h_n * tex_h), color, 2);
}

////////////////////////

/// AUDIO ///

void AnimationTrackEditTypeAudio::_preview_changed(ObjectID p_which) {
	for (int i = 0; i < get_animation()->track_get_key_count(get_track()); i++) {
		Ref<AudioStream> stream = get_animation()->audio_track_get_key_stream(get_track(), i);
		if (stream.is_valid() && stream->get_instance_id() == p_which) {
			queue_redraw();
			return;
		}
	}
}

int AnimationTrackEditTypeAudio::get_key_height() const {
	Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
	int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
	return int(font->get_height(font_size) * 1.5);
}

Rect2 AnimationTrackEditTypeAudio::get_key_rect(int p_index, float p_pixels_sec) {
	Ref<AudioStream> stream = get_animation()->audio_track_get_key_stream(get_track(), p_index);

	if (stream.is_null()) {
		return AnimationTrackEdit::get_key_rect(p_index, p_pixels_sec);
	}

	float start_ofs = get_animation()->audio_track_get_key_start_offset(get_track(), p_index);
	float end_ofs = get_animation()->audio_track_get_key_end_offset(get_track(), p_index);

	float len = stream->get_length();

	if (len == 0) {
		Ref<AudioStreamPreview> preview = AudioStreamPreviewGenerator::get_singleton()->generate_preview(stream);
		len = preview->get_length();
	}

	len -= end_ofs;
	len -= start_ofs;
	if (len <= 0.0001) {
		len = 0.0001;
	}

	if (get_animation()->track_get_key_count(get_track()) > p_index + 1) {
		len = MIN(len, get_animation()->track_get_key_time(get_track(), p_index + 1) - get_animation()->track_get_key_time(get_track(), p_index));
	}

	return Rect2(0, 0, len * p_pixels_sec, get_size().height);
}

bool AnimationTrackEditTypeAudio::is_key_selectable_by_distance() const {
	return false;
}

void AnimationTrackEditTypeAudio::draw_key(int p_index, float p_pixels_sec, int p_x, bool p_selected, int p_clip_left, int p_clip_right) {
	Ref<AudioStream> stream = get_animation()->audio_track_get_key_stream(get_track(), p_index);
	if (stream.is_null()) {
		AnimationTrackEdit::draw_key(p_index, p_pixels_sec, p_x, p_selected, p_clip_left, p_clip_right); // Draw diamond.
		return;
	}

	float len = stream->get_length();
	if (len == 0) {
		AnimationTrackEdit::draw_key(p_index, p_pixels_sec, p_x, p_selected, p_clip_left, p_clip_right); // Draw diamond.
		return;
	}

	float start_ofs = get_animation()->audio_track_get_key_start_offset(get_track(), p_index);
	float end_ofs = get_animation()->audio_track_get_key_end_offset(get_track(), p_index);

	int px_offset = 0;
	if (len_resizing && p_index == len_resizing_index) {
		float ofs_local = len_resizing_rel / get_timeline()->get_zoom_scale();
		if (len_resizing_start) {
			start_ofs += ofs_local;
			px_offset = ofs_local * p_pixels_sec;
		} else {
			end_ofs -= ofs_local;
		}
	}

	Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
	int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
	float fh = int(font->get_height(font_size) * 1.5);

	Ref<AudioStreamPreview> preview = AudioStreamPreviewGenerator::get_singleton()->generate_preview(stream);

	int pixel_total_len = len * p_pixels_sec;

	len -= end_ofs;
	len -= start_ofs;

	if (len <= 0.0001) {
		len = 0.0001;
	}

	int pixel_len = len * p_pixels_sec;

	int pixel_begin = px_offset + p_x;
	int pixel_end = px_offset + p_x + pixel_len;

	if (pixel_end < p_clip_left) {
		return;
	}

	if (pixel_begin > p_clip_right) {
		return;
	}

	int from_x = MAX(pixel_begin, p_clip_left);
	int to_x = MIN(pixel_end, p_clip_right);

	if (get_animation()->track_get_key_count(get_track()) > p_index + 1) {
		float limit = MIN(len, get_animation()->track_get_key_time(get_track(), p_index + 1) - get_animation()->track_get_key_time(get_track(), p_index));
		int limit_x = pixel_begin + limit * p_pixels_sec;
		to_x = MIN(limit_x, to_x);
	}

	if (to_x <= from_x) {
		to_x = from_x + 1;
	}

	int h = get_size().height;
	Rect2 rect = Rect2(from_x, (h - fh) / 2, to_x - from_x, fh);
	draw_rect(rect, Color(0.25, 0.25, 0.25));

	Vector<Vector2> points;
	points.resize((to_x - from_x) * 2);
	float preview_len = preview->get_length();

	for (int i = from_x; i < to_x; i++) {
		float ofs = (i - pixel_begin) * preview_len / pixel_total_len;
		float ofs_n = ((i + 1) - pixel_begin) * preview_len / pixel_total_len;
		ofs += start_ofs;
		ofs_n += start_ofs;

		float max = preview->get_max(ofs, ofs_n) * 0.5 + 0.5;
		float min = preview->get_min(ofs, ofs_n) * 0.5 + 0.5;

		int idx = i - from_x;
		points.write[idx * 2 + 0] = Vector2(i, rect.position.y + min * rect.size.y);
		points.write[idx * 2 + 1] = Vector2(i, rect.position.y + max * rect.size.y);
	}

	Vector<Color> colors = { Color(0.75, 0.75, 0.75) };

	RS::get_singleton()->canvas_item_add_multiline(get_canvas_item(), points, colors);

	Color cut_color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
	cut_color.a = 0.7;
	if (start_ofs > 0 && pixel_begin > p_clip_left) {
		draw_rect(Rect2(pixel_begin, rect.position.y, 1, rect.size.y), cut_color);
	}
	if (end_ofs > 0 && pixel_end < p_clip_right) {
		draw_rect(Rect2(pixel_end, rect.position.y, 1, rect.size.y), cut_color);
	}

	if (p_selected) {
		Color accent = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
		draw_rect(rect, accent, false);
	}
}

AnimationTrackEditTypeAudio::AnimationTrackEditTypeAudio() {
	AudioStreamPreviewGenerator::get_singleton()->connect("preview_updated", callable_mp(this, &AnimationTrackEditTypeAudio::_preview_changed));
}

bool AnimationTrackEditTypeAudio::can_drop_data(const Point2 &p_point, const Variant &p_data) const {
	if (p_point.x > get_timeline()->get_name_limit() && p_point.x < get_size().width - get_timeline()->get_buttons_width()) {
		Dictionary drag_data = p_data;
		if (drag_data.has("type") && String(drag_data["type"]) == "resource") {
			Ref<AudioStream> res = drag_data["resource"];
			if (res.is_valid()) {
				return true;
			}
		}

		if (drag_data.has("type") && String(drag_data["type"]) == "files") {
			Vector<String> files = drag_data["files"];

			if (files.size() == 1) {
				Ref<AudioStream> res = ResourceLoader::load(files[0]);
				if (res.is_valid()) {
					return true;
				}
			}
		}
	}

	return AnimationTrackEdit::can_drop_data(p_point, p_data);
}

void AnimationTrackEditTypeAudio::drop_data(const Point2 &p_point, const Variant &p_data) {
	if (p_point.x > get_timeline()->get_name_limit() && p_point.x < get_size().width - get_timeline()->get_buttons_width()) {
		Ref<AudioStream> stream;
		Dictionary drag_data = p_data;
		if (drag_data.has("type") && String(drag_data["type"]) == "resource") {
			stream = drag_data["resource"];
		} else if (drag_data.has("type") && String(drag_data["type"]) == "files") {
			Vector<String> files = drag_data["files"];

			if (files.size() == 1) {
				stream = ResourceLoader::load(files[0]);
			}
		}

		if (stream.is_valid()) {
			int x = p_point.x - get_timeline()->get_name_limit();
			float ofs = x / get_timeline()->get_zoom_scale();
			ofs += get_timeline()->get_value();

			ofs = get_editor()->snap_time(ofs);

			while (get_animation()->track_find_key(get_track(), ofs, Animation::FIND_MODE_APPROX) != -1) { //make sure insertion point is valid
				ofs += 0.0001;
			}

			EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
			undo_redo->create_action(TTR("Add Audio Track Clip"));
			undo_redo->add_do_method(get_animation().ptr(), "audio_track_insert_key", get_track(), ofs, stream);
			undo_redo->add_undo_method(get_animation().ptr(), "track_remove_key_at_time", get_track(), ofs);
			undo_redo->commit_action();

			queue_redraw();
			return;
		}
	}

	AnimationTrackEdit::drop_data(p_point, p_data);
}

void AnimationTrackEditTypeAudio::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventMouseMotion> mm = p_event;
	if (!len_resizing && mm.is_valid()) {
		bool use_hsize_cursor = false;
		for (int i = 0; i < get_animation()->track_get_key_count(get_track()); i++) {
			Ref<AudioStream> stream = get_animation()->audio_track_get_key_stream(get_track(), i);

			if (stream.is_null()) {
				continue;
			}

			float len = stream->get_length();
			if (len == 0) {
				continue;
			}

			float start_ofs = get_animation()->audio_track_get_key_start_offset(get_track(), i);
			float end_ofs = get_animation()->audio_track_get_key_end_offset(get_track(), i);
			len -= end_ofs;
			len -= start_ofs;

			if (get_animation()->track_get_key_count(get_track()) > i + 1) {
				len = MIN(len, get_animation()->track_get_key_time(get_track(), i + 1) - get_animation()->track_get_key_time(get_track(), i));
			}

			float ofs = get_animation()->track_get_key_time(get_track(), i);

			ofs -= get_timeline()->get_value();
			ofs *= get_timeline()->get_zoom_scale();
			ofs += get_timeline()->get_name_limit();

			int end = ofs + len * get_timeline()->get_zoom_scale();

			if (end >= get_timeline()->get_name_limit() && end <= get_size().width - get_timeline()->get_buttons_width() && ABS(mm->get_position().x - end) < 5 * EDSCALE) {
				len_resizing_start = false;
				use_hsize_cursor = true;
				len_resizing_index = i;
			}

			if (ofs >= get_timeline()->get_name_limit() && ofs <= get_size().width - get_timeline()->get_buttons_width() && ABS(mm->get_position().x - ofs) < 5 * EDSCALE) {
				len_resizing_start = true;
				use_hsize_cursor = true;
				len_resizing_index = i;
			}
		}
		over_drag_position = use_hsize_cursor;
	}

	if (len_resizing && mm.is_valid()) {
		// Rezising index is some.
		len_resizing_rel += mm->get_relative().x;
		float ofs_local = len_resizing_rel / get_timeline()->get_zoom_scale();
		float prev_ofs_start = get_animation()->audio_track_get_key_start_offset(get_track(), len_resizing_index);
		float prev_ofs_end = get_animation()->audio_track_get_key_end_offset(get_track(), len_resizing_index);
		Ref<AudioStream> stream = get_animation()->audio_track_get_key_stream(get_track(), len_resizing_index);
		float len = stream->get_length();
		if (len == 0) {
			Ref<AudioStreamPreview> preview = AudioStreamPreviewGenerator::get_singleton()->generate_preview(stream);
			float preview_len = preview->get_length();
			len = preview_len;
		}

		if (len_resizing_start) {
			len_resizing_rel = CLAMP(ofs_local, -prev_ofs_start, len - prev_ofs_end - prev_ofs_start) * get_timeline()->get_zoom_scale();
		} else {
			len_resizing_rel = CLAMP(ofs_local, -(len - prev_ofs_end - prev_ofs_start), prev_ofs_end) * get_timeline()->get_zoom_scale();
		}

		queue_redraw();
		accept_event();
		return;
	}

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT && over_drag_position) {
		len_resizing = true;
		// In case if resizing index is not set yet reset the flag.
		if (len_resizing_index < 0) {
			len_resizing = false;
			return;
		}
		len_resizing_from_px = mb->get_position().x;
		len_resizing_rel = 0;
		queue_redraw();
		accept_event();
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (len_resizing && mb.is_valid() && !mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		if (len_resizing_rel == 0 || len_resizing_index < 0) {
			len_resizing = false;
			return;
		}

		if (len_resizing_start) {
			float ofs_local = len_resizing_rel / get_timeline()->get_zoom_scale();
			float prev_ofs = get_animation()->audio_track_get_key_start_offset(get_track(), len_resizing_index);
			float prev_time = get_animation()->track_get_key_time(get_track(), len_resizing_index);
			float new_ofs = prev_ofs + ofs_local;
			float new_time = prev_time + ofs_local;
			if (prev_time != new_time) {
				undo_redo->create_action(TTR("Change Audio Track Clip Start Offset"));

				undo_redo->add_do_method(get_animation().ptr(), "track_set_key_time", get_track(), len_resizing_index, new_time);
				undo_redo->add_undo_method(get_animation().ptr(), "track_set_key_time", get_track(), len_resizing_index, prev_time);

				undo_redo->add_do_method(get_animation().ptr(), "audio_track_set_key_start_offset", get_track(), len_resizing_index, new_ofs);
				undo_redo->add_undo_method(get_animation().ptr(), "audio_track_set_key_start_offset", get_track(), len_resizing_index, prev_ofs);

				undo_redo->commit_action();
			}
		} else {
			float ofs_local = -len_resizing_rel / get_timeline()->get_zoom_scale();
			float prev_ofs = get_animation()->audio_track_get_key_end_offset(get_track(), len_resizing_index);
			float new_ofs = prev_ofs + ofs_local;
			if (prev_ofs != new_ofs) {
				undo_redo->create_action(TTR("Change Audio Track Clip End Offset"));
				undo_redo->add_do_method(get_animation().ptr(), "audio_track_set_key_end_offset", get_track(), len_resizing_index, new_ofs);
				undo_redo->add_undo_method(get_animation().ptr(), "audio_track_set_key_end_offset", get_track(), len_resizing_index, prev_ofs);
				undo_redo->commit_action();
			}
		}

		len_resizing_index = -1;
		len_resizing = false;
		queue_redraw();
		accept_event();
		return;
	}

	AnimationTrackEdit::gui_input(p_event);
}

Control::CursorShape AnimationTrackEditTypeAudio::get_cursor_shape(const Point2 &p_pos) const {
	if (over_drag_position || len_resizing) {
		return Control::CURSOR_HSIZE;
	} else {
		return get_default_cursor_shape();
	}
}

////////////////////
/// SUB ANIMATION ///

int AnimationTrackEditTypeAnimation::get_key_height() const {
	if (!ObjectDB::get_instance(id)) {
		return AnimationTrackEdit::get_key_height();
	}

	Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
	int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
	return int(font->get_height(font_size) * 1.5);
}

Rect2 AnimationTrackEditTypeAnimation::get_key_rect(int p_index, float p_pixels_sec) {
	Object *object = ObjectDB::get_instance(id);

	if (!object) {
		return AnimationTrackEdit::get_key_rect(p_index, p_pixels_sec);
	}

	AnimationPlayer *ap = Object::cast_to<AnimationPlayer>(object);

	if (!ap) {
		return AnimationTrackEdit::get_key_rect(p_index, p_pixels_sec);
	}

	String anim = get_animation()->animation_track_get_key_animation(get_track(), p_index);

	if (anim != "[stop]" && ap->has_animation(anim)) {
		float len = ap->get_animation(anim)->get_length();

		if (get_animation()->track_get_key_count(get_track()) > p_index + 1) {
			len = MIN(len, get_animation()->track_get_key_time(get_track(), p_index + 1) - get_animation()->track_get_key_time(get_track(), p_index));
		}

		return Rect2(0, 0, len * p_pixels_sec, get_size().height);
	} else {
		Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
		int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
		int fh = font->get_height(font_size) * 0.8;
		return Rect2(0, 0, fh, get_size().height);
	}
}

bool AnimationTrackEditTypeAnimation::is_key_selectable_by_distance() const {
	return false;
}

void AnimationTrackEditTypeAnimation::draw_key(int p_index, float p_pixels_sec, int p_x, bool p_selected, int p_clip_left, int p_clip_right) {
	Object *object = ObjectDB::get_instance(id);

	if (!object) {
		AnimationTrackEdit::draw_key(p_index, p_pixels_sec, p_x, p_selected, p_clip_left, p_clip_right);
		return;
	}

	AnimationPlayer *ap = Object::cast_to<AnimationPlayer>(object);

	if (!ap) {
		AnimationTrackEdit::draw_key(p_index, p_pixels_sec, p_x, p_selected, p_clip_left, p_clip_right);
		return;
	}

	String anim = get_animation()->animation_track_get_key_animation(get_track(), p_index);

	if (anim != "[stop]" && ap->has_animation(anim)) {
		float len = ap->get_animation(anim)->get_length();

		if (get_animation()->track_get_key_count(get_track()) > p_index + 1) {
			len = MIN(len, get_animation()->track_get_key_time(get_track(), p_index + 1) - get_animation()->track_get_key_time(get_track(), p_index));
		}

		int pixel_len = len * p_pixels_sec;

		int pixel_begin = p_x;
		int pixel_end = p_x + pixel_len;

		if (pixel_end < p_clip_left) {
			return;
		}

		if (pixel_begin > p_clip_right) {
			return;
		}

		int from_x = MAX(pixel_begin, p_clip_left);
		int to_x = MIN(pixel_end, p_clip_right);

		if (to_x <= from_x) {
			return;
		}

		Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
		int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
		int fh = font->get_height(font_size) * 1.5;

		Rect2 rect(from_x, int(get_size().height - fh) / 2, to_x - from_x, fh);

		Color color = get_theme_color(SceneStringName(font_color), SNAME("Label"));
		Color bg = color;
		bg.r = 1 - color.r;
		bg.g = 1 - color.g;
		bg.b = 1 - color.b;
		draw_rect(rect, bg);

		Vector<Vector2> points;
		Vector<Color> colors = { color };
		{
			Ref<Animation> ap_anim = ap->get_animation(anim);

			for (int i = 0; i < ap_anim->get_track_count(); i++) {
				float h = (rect.size.height - 2) / ap_anim->get_track_count();

				int y = 2 + h * i + h / 2;

				for (int j = 0; j < ap_anim->track_get_key_count(i); j++) {
					float ofs = ap_anim->track_get_key_time(i, j);
					int x = p_x + ofs * p_pixels_sec + 2;

					if (x < from_x || x >= (to_x - 4)) {
						continue;
					}

					points.push_back(Point2(x, y));
					points.push_back(Point2(x + 1, y));
				}
			}
		}

		if (points.size() > 2) {
			RS::get_singleton()->canvas_item_add_multiline(get_canvas_item(), points, colors);
		}

		int limit = to_x - from_x - 4;
		if (limit > 0) {
			draw_string(font, Point2(from_x + 2, int(get_size().height - font->get_height(font_size)) / 2 + font->get_ascent(font_size)), anim, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, color);
		}

		if (p_selected) {
			Color accent = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
			draw_rect(rect, accent, false);
		}
	} else {
		Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
		int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
		int fh = font->get_height(font_size) * 0.8;
		Rect2 rect(Vector2(p_x, int(get_size().height - fh) / 2), Size2(fh, fh));

		Color color = get_theme_color(SceneStringName(font_color), SNAME("Label"));
		draw_rect_clipped(rect, color);

		if (p_selected) {
			Color accent = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
			draw_rect_clipped(rect, accent, false);
		}
	}
}

void AnimationTrackEditTypeAnimation::set_node(Object *p_object) {
	id = p_object->get_instance_id();
}

AnimationTrackEditTypeAnimation::AnimationTrackEditTypeAnimation() {
}

/////////
AnimationTrackEdit *AnimationTrackEditDefaultPlugin::create_value_track_edit(Object *p_object, Variant::Type p_type, const String &p_property, PropertyHint p_hint, const String &p_hint_string, int p_usage) {
	if (p_property == "playing" && (p_object->is_class("AudioStreamPlayer") || p_object->is_class("AudioStreamPlayer2D") || p_object->is_class("AudioStreamPlayer3D"))) {
		AnimationTrackEditAudio *audio = memnew(AnimationTrackEditAudio);
		audio->set_node(p_object);
		return audio;
	}

	if (p_property == "frame" && (p_object->is_class("Sprite2D") || p_object->is_class("Sprite3D") || p_object->is_class("AnimatedSprite2D") || p_object->is_class("AnimatedSprite3D"))) {
		AnimationTrackEditSpriteFrame *sprite = memnew(AnimationTrackEditSpriteFrame);
		sprite->set_node(p_object);
		return sprite;
	}

	if (p_property == "frame_coords" && (p_object->is_class("Sprite2D") || p_object->is_class("Sprite3D"))) {
		AnimationTrackEditSpriteFrame *sprite = memnew(AnimationTrackEditSpriteFrame);
		sprite->set_as_coords();
		sprite->set_node(p_object);
		return sprite;
	}

	if (p_property == "current_animation" && (p_object->is_class("AnimationPlayer"))) {
		AnimationTrackEditSubAnim *player = memnew(AnimationTrackEditSubAnim);
		player->set_node(p_object);
		return player;
	}

	if (p_property == "volume_db") {
		AnimationTrackEditVolumeDB *vu = memnew(AnimationTrackEditVolumeDB);
		return vu;
	}

	if (p_type == Variant::BOOL) {
		return memnew(AnimationTrackEditBool);
	}
	if (p_type == Variant::COLOR) {
		return memnew(AnimationTrackEditColor);
	}

	return nullptr;
}

AnimationTrackEdit *AnimationTrackEditDefaultPlugin::create_audio_track_edit() {
	return memnew(AnimationTrackEditTypeAudio);
}

AnimationTrackEdit *AnimationTrackEditDefaultPlugin::create_animation_track_edit(Object *p_object) {
	AnimationTrackEditTypeAnimation *an = memnew(AnimationTrackEditTypeAnimation);
	an->set_node(p_object);
	return an;
}
