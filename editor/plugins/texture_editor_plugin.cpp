/**************************************************************************/
/*  texture_editor_plugin.cpp                                             */
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

#include "texture_editor_plugin.h"

#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/aspect_ratio_container.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/label.h"
#include "scene/gui/texture_rect.h"
#include "scene/resources/animated_texture.h"
#include "scene/resources/atlas_texture.h"
#include "scene/resources/compressed_texture.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/portable_compressed_texture.h"

TextureRect *TexturePreview::get_texture_display() {
	return texture_display;
}

void TexturePreview::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			if (!is_inside_tree()) {
				// TODO: This is a workaround because `NOTIFICATION_THEME_CHANGED`
				// is getting called for some reason when the `TexturePreview` is
				// getting destroyed, which causes `get_theme_font()` to return `nullptr`.
				// See https://github.com/godotengine/godot/issues/50743.
				break;
			}

			if (metadata_label) {
				Ref<Font> metadata_label_font = get_theme_font(SNAME("expression"), EditorStringName(EditorFonts));
				metadata_label->add_theme_font_override(SceneStringName(font), metadata_label_font);
			}

			bg_rect->set_color(get_theme_color(SNAME("dark_color_2"), EditorStringName(Editor)));
			checkerboard->set_texture(get_editor_theme_icon(SNAME("Checkerboard")));
			cached_outline_color = get_theme_color(SNAME("extra_border_color_1"), EditorStringName(Editor));
		} break;
	}
}

void TexturePreview::_draw_outline() {
	const float outline_width = Math::round(EDSCALE);
	const Rect2 outline_rect = Rect2(Vector2(), texture_display->get_size()).grow(outline_width * 0.5);
	texture_display->draw_rect(outline_rect, cached_outline_color, false, outline_width);
}

void TexturePreview::_update_texture_display_ratio() {
	if (texture_display->get_texture().is_valid()) {
		centering_container->set_ratio(texture_display->get_texture()->get_size().aspect());
	}
}

void TexturePreview::_update_metadata_label_text() {
	const Ref<Texture2D> texture = texture_display->get_texture();

	String format;
	if (Object::cast_to<ImageTexture>(*texture)) {
		format = Image::get_format_name(Object::cast_to<ImageTexture>(*texture)->get_format());
	} else if (Object::cast_to<CompressedTexture2D>(*texture)) {
		format = Image::get_format_name(Object::cast_to<CompressedTexture2D>(*texture)->get_format());
	} else {
		format = texture->get_class();
	}

	const Ref<Image> image = texture->get_image();
	if (image.is_valid()) {
		const int mipmaps = image->get_mipmap_count();
		// Avoid signed integer overflow that could occur with huge texture sizes by casting everything to uint64_t.
		uint64_t memory = uint64_t(image->get_width()) * uint64_t(image->get_height()) * uint64_t(Image::get_format_pixel_size(image->get_format()));
		// Handle VRAM-compressed formats that are stored with 4 bpp.
		memory >>= Image::get_format_pixel_rshift(image->get_format());

		float mipmaps_multiplier = 1.0;
		float mipmap_increase = 0.25;
		for (int i = 0; i < mipmaps; i++) {
			// Each mip adds 25% memory usage of the previous one.
			// With a complete mipmap chain, memory usage increases by ~33%.
			mipmaps_multiplier += mipmap_increase;
			mipmap_increase *= 0.25;
		}
		memory *= mipmaps_multiplier;

		if (mipmaps >= 1) {
			metadata_label->set_text(
					vformat(String::utf8("%d×%d %s\n") + TTR("%s Mipmaps") + "\n" + TTR("Memory: %s"),
							texture->get_width(),
							texture->get_height(),
							format,
							mipmaps,
							String::humanize_size(memory)));
		} else {
			// "No Mipmaps" is easier to distinguish than "0 Mipmaps",
			// especially since 0, 6, and 8 look quite close with the default code font.
			metadata_label->set_text(
					vformat(String::utf8("%d×%d %s\n") + TTR("No Mipmaps") + "\n" + TTR("Memory: %s"),
							texture->get_width(),
							texture->get_height(),
							format,
							String::humanize_size(memory)));
		}
	} else {
		metadata_label->set_text(
				vformat(String::utf8("%d×%d %s"),
						texture->get_width(),
						texture->get_height(),
						format));
	}
}

TexturePreview::TexturePreview(Ref<Texture2D> p_texture, bool p_show_metadata) {
	set_custom_minimum_size(Size2(0.0, 256.0) * EDSCALE);

	bg_rect = memnew(ColorRect);

	add_child(bg_rect);

	margin_container = memnew(MarginContainer);
	const float outline_width = Math::round(EDSCALE);
	margin_container->add_theme_constant_override("margin_right", outline_width);
	margin_container->add_theme_constant_override("margin_top", outline_width);
	margin_container->add_theme_constant_override("margin_left", outline_width);
	margin_container->add_theme_constant_override("margin_bottom", outline_width);
	add_child(margin_container);

	centering_container = memnew(AspectRatioContainer);
	margin_container->add_child(centering_container);

	checkerboard = memnew(TextureRect);
	checkerboard->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	checkerboard->set_stretch_mode(TextureRect::STRETCH_TILE);
	checkerboard->set_texture_repeat(CanvasItem::TEXTURE_REPEAT_ENABLED);
	centering_container->add_child(checkerboard);

	texture_display = memnew(TextureRect);
	texture_display->set_texture_filter(TEXTURE_FILTER_NEAREST_WITH_MIPMAPS);
	texture_display->set_texture(p_texture);
	texture_display->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	centering_container->add_child(texture_display);

	texture_display->connect(SceneStringName(draw), callable_mp(this, &TexturePreview::_draw_outline));

	if (p_texture.is_valid()) {
		_update_texture_display_ratio();
		p_texture->connect_changed(callable_mp(this, &TexturePreview::_update_texture_display_ratio));
	}

	if (p_show_metadata) {
		metadata_label = memnew(Label);

		if (p_texture.is_valid()) {
			_update_metadata_label_text();
			p_texture->connect_changed(callable_mp(this, &TexturePreview::_update_metadata_label_text));
		}

		// It's okay that these colors are static since the grid color is static too.
		metadata_label->add_theme_color_override(SceneStringName(font_color), Color(1, 1, 1));
		metadata_label->add_theme_color_override("font_shadow_color", Color(0, 0, 0));

		metadata_label->add_theme_font_size_override(SceneStringName(font_size), 14 * EDSCALE);
		metadata_label->add_theme_color_override("font_outline_color", Color(0, 0, 0));
		metadata_label->add_theme_constant_override("outline_size", 8 * EDSCALE);

		metadata_label->set_h_size_flags(Control::SIZE_SHRINK_END);
		metadata_label->set_v_size_flags(Control::SIZE_SHRINK_END);

		add_child(metadata_label);
	}
}

bool EditorInspectorPluginTexture::can_handle(Object *p_object) {
	return Object::cast_to<ImageTexture>(p_object) != nullptr || Object::cast_to<AtlasTexture>(p_object) != nullptr || Object::cast_to<CompressedTexture2D>(p_object) != nullptr || Object::cast_to<PortableCompressedTexture2D>(p_object) != nullptr || Object::cast_to<AnimatedTexture>(p_object) != nullptr || Object::cast_to<Image>(p_object) != nullptr;
}

void EditorInspectorPluginTexture::parse_begin(Object *p_object) {
	Ref<Texture> texture(Object::cast_to<Texture>(p_object));
	if (texture.is_null()) {
		Ref<Image> image(Object::cast_to<Image>(p_object));
		texture = ImageTexture::create_from_image(image);

		ERR_FAIL_COND_MSG(texture.is_null(), "Failed to create the texture from an invalid image.");
	}

	add_custom_control(memnew(TexturePreview(texture, true)));
}

TextureEditorPlugin::TextureEditorPlugin() {
	Ref<EditorInspectorPluginTexture> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}
