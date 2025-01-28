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
#include "editor/plugins/color_channel_selector.h"
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
#include "scene/resources/style_box_flat.h"

constexpr const char *texture_2d_shader = R"(
shader_type canvas_item;
render_mode blend_mix;

uniform vec4 u_channel_factors = vec4(1.0);

vec4 filter_preview_colors(vec4 input_color, vec4 factors) {
	// Filter RGB.
	vec4 output_color = input_color * vec4(factors.rgb, input_color.a);

	// Remove transparency when alpha is not enabled.
	output_color.a = mix(1.0, output_color.a, factors.a);

	// Switch to opaque grayscale when visualizing only one channel.
	float csum = factors.r + factors.g + factors.b + factors.a;
	float single = clamp(2.0 - csum, 0.0, 1.0);
	for (int i = 0; i < 4; i++) {
		float c = input_color[i];
		output_color = mix(output_color, vec4(c, c, c, 1.0), factors[i] * single);
	}

	return output_color;
}

void fragment() {
	COLOR = filter_preview_colors(texture(TEXTURE, UV), u_channel_factors);
}
)";

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
	const Rect2 outline_rect = Rect2(Vector2(), outline_overlay->get_size()).grow(outline_width * 0.5);
	outline_overlay->draw_rect(outline_rect, cached_outline_color, false, outline_width);
}

void TexturePreview::_update_texture_display_ratio() {
	if (texture_display->get_texture().is_valid()) {
		centering_container->set_ratio(texture_display->get_texture()->get_size().aspect());
	}
}

static Image::Format get_texture_2d_format(const Ref<Texture2D> &p_texture) {
	const Ref<ImageTexture> image_texture = p_texture;
	if (image_texture.is_valid()) {
		return image_texture->get_format();
	}

	const Ref<CompressedTexture2D> compressed_texture = p_texture;
	if (compressed_texture.is_valid()) {
		return compressed_texture->get_format();
	}

	// AtlasTexture?

	// Unknown
	return Image::FORMAT_MAX;
}

static int get_texture_mipmaps_count(const Ref<Texture2D> &p_texture) {
	ERR_FAIL_COND_V(p_texture.is_null(), -1);
	// We are having to download the image only to get its mipmaps count. It would be nice if we didn't have to.
	Ref<Image> image = p_texture->get_image();
	if (image.is_valid()) {
		return image->get_mipmap_count();
	}
	return -1;
}

void TexturePreview::_update_metadata_label_text() {
	const Ref<Texture2D> texture = texture_display->get_texture();
	ERR_FAIL_COND(texture.is_null());

	const Image::Format format = get_texture_2d_format(texture.ptr());

	const String format_name = format != Image::FORMAT_MAX ? Image::get_format_name(format) : texture->get_class();

	const Vector2i resolution = texture->get_size();
	const int mipmaps = get_texture_mipmaps_count(texture);

	if (format != Image::FORMAT_MAX) {
		// Avoid signed integer overflow that could occur with huge texture sizes by casting everything to uint64_t.
		uint64_t memory = uint64_t(resolution.x) * uint64_t(resolution.y) * uint64_t(Image::get_format_pixel_size(format));
		// Handle VRAM-compressed formats that are stored with 4 bpp.
		memory >>= Image::get_format_pixel_rshift(format);

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
							format_name,
							mipmaps,
							String::humanize_size(memory)));
		} else {
			// "No Mipmaps" is easier to distinguish than "0 Mipmaps",
			// especially since 0, 6, and 8 look quite close with the default code font.
			metadata_label->set_text(
					vformat(String::utf8("%d×%d %s\n") + TTR("No Mipmaps") + "\n" + TTR("Memory: %s"),
							texture->get_width(),
							texture->get_height(),
							format_name,
							String::humanize_size(memory)));
		}
	} else {
		metadata_label->set_text(
				vformat(String::utf8("%d×%d %s"),
						texture->get_width(),
						texture->get_height(),
						format_name));
	}
}

void TexturePreview::on_selected_channels_changed() {
	material->set_shader_parameter("u_channel_factors", channel_selector->get_selected_channel_factors());
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

	{
		Ref<Shader> shader;
		shader.instantiate();
		shader->set_code(texture_2d_shader);

		material.instantiate();
		material->set_shader(shader);
		material->set_shader_parameter("u_channel_factors", Vector4(1, 1, 1, 1));
	}

	texture_display = memnew(TextureRect);
	texture_display->set_texture_filter(TEXTURE_FILTER_NEAREST_WITH_MIPMAPS);
	texture_display->set_texture(p_texture);
	texture_display->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	texture_display->set_material(material);
	centering_container->add_child(texture_display);

	// Creating a separate control so it is not affected by the filtering shader.
	outline_overlay = memnew(Control);
	centering_container->add_child(outline_overlay);

	outline_overlay->connect(SceneStringName(draw), callable_mp(this, &TexturePreview::_draw_outline));

	if (p_texture.is_valid()) {
		_update_texture_display_ratio();
		p_texture->connect_changed(callable_mp(this, &TexturePreview::_update_texture_display_ratio));
	}

	// Null can be passed by `Camera3DPreview` (which immediately after sets a texture anyways).
	const Image::Format format = p_texture.is_valid() ? get_texture_2d_format(p_texture.ptr()) : Image::FORMAT_MAX;
	const uint32_t components_mask = format != Image::FORMAT_MAX ? Image::get_format_component_mask(format) : 0xf;

	// Add color channel selector at the bottom left if more than 1 channel is available.
	if (p_show_metadata && !is_power_of_2(components_mask)) {
		channel_selector = memnew(ColorChannelSelector);
		channel_selector->connect("selected_channels_changed", callable_mp(this, &TexturePreview::on_selected_channels_changed));
		channel_selector->set_h_size_flags(Control::SIZE_SHRINK_BEGIN);
		channel_selector->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
		channel_selector->set_available_channels_mask(components_mask);
		add_child(channel_selector);
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
