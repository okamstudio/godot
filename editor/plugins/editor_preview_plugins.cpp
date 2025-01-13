/**************************************************************************/
/*  editor_preview_plugins.cpp                                            */
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

#include "editor_preview_plugins.h"

#include "core/config/project_settings.h"
#include "core/io/image.h"
#include "core/io/resource_loader.h"
#include "core/object/script_language.h"
#include "editor/editor_paths.h"
#include "editor/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/resources/atlas_texture.h"
#include "scene/resources/bit_map.h"
#include "scene/resources/font.h"
#include "scene/resources/gradient_texture.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "servers/audio/audio_stream.h"

void post_process_preview(Ref<Image> p_image) {
	if (p_image->get_format() != Image::FORMAT_RGBA8) {
		p_image->convert(Image::FORMAT_RGBA8);
	}

	const int w = p_image->get_width();
	const int h = p_image->get_height();

	const int r = MIN(w, h) / 32;
	const int r2 = r * r;
	Color transparent = Color(0, 0, 0, 0);

	for (int i = 0; i < r; i++) {
		for (int j = 0; j < r; j++) {
			int dx = i - r;
			int dy = j - r;
			if (dx * dx + dy * dy > r2) {
				p_image->set_pixel(i, j, transparent);
				p_image->set_pixel(w - 1 - i, j, transparent);
				p_image->set_pixel(w - 1 - i, h - 1 - j, transparent);
				p_image->set_pixel(i, h - 1 - j, transparent);
			} else {
				break;
			}
		}
	}
}

bool EditorTexturePreviewPlugin::handles(const String &p_type) const {
	return ClassDB::is_parent_class(p_type, "Texture");
}

bool EditorTexturePreviewPlugin::generate_small_preview_automatically() const {
	return true;
}

Ref<Texture2D> EditorTexturePreviewPlugin::generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const {
	Ref<Image> img;

	Ref<AtlasTexture> tex_atlas = p_from;
	Ref<Texture3D> tex_3d = p_from;
	Ref<TextureLayered> tex_lyr = p_from;

	if (tex_atlas.is_valid()) {
		Ref<Texture2D> tex = tex_atlas->get_atlas();
		if (tex.is_null()) {
			return Ref<Texture2D>();
		}

		Ref<Image> atlas = tex->get_image();
		if (atlas.is_null()) {
			return Ref<Texture2D>();
		}

		if (!tex_atlas->get_region().has_area()) {
			return Ref<Texture2D>();
		}

		img = atlas->get_region(tex_atlas->get_region());

	} else if (tex_3d.is_valid()) {
		if (tex_3d->get_depth() == 0) {
			return Ref<Texture2D>();
		}

		Vector<Ref<Image>> data = tex_3d->get_data();
		if (data.size() != tex_3d->get_depth()) {
			return Ref<Texture2D>();
		}

		// Use the middle slice for the thumbnail.
		const int mid_depth = (tex_3d->get_depth() - 1) / 2;
		if (!data.is_empty() && data[mid_depth].is_valid()) {
			img = data[mid_depth]->duplicate();
		}

	} else if (tex_lyr.is_valid()) {
		if (tex_lyr->get_layers() == 0) {
			return Ref<Texture2D>();
		}

		// Use the middle slice for the thumbnail.
		const int mid_layer = (tex_lyr->get_layers() - 1) / 2;

		Ref<Image> data = tex_lyr->get_layer_data(mid_layer);
		if (data.is_valid()) {
			img = data->duplicate();
		}

	} else {
		Ref<Texture2D> tex = p_from;
		if (tex.is_valid()) {
			img = tex->get_image();
			if (img.is_valid()) {
				img = img->duplicate();
			}
		}
	}

	if (img.is_null() || img->is_empty()) {
		return Ref<Texture2D>();
	}

	p_metadata["dimensions"] = img->get_size();

	img->clear_mipmaps();

	if (img->is_compressed()) {
		if (img->decompress() != OK) {
			return Ref<Texture2D>();
		}
	} else if (img->get_format() != Image::FORMAT_RGB8 && img->get_format() != Image::FORMAT_RGBA8) {
		img->convert(Image::FORMAT_RGBA8);
	}

	Vector2 new_size = img->get_size();
	if (new_size.x > p_size.x) {
		new_size = Vector2(p_size.x, new_size.y * p_size.x / new_size.x);
	}
	if (new_size.y > p_size.y) {
		new_size = Vector2(new_size.x * p_size.y / new_size.y, p_size.y);
	}
	Vector2i new_size_i = Vector2i(new_size).maxi(1);
	img->resize(new_size_i.x, new_size_i.y, Image::INTERPOLATE_CUBIC);
	post_process_preview(img);

	return ImageTexture::create_from_image(img);
}

EditorTexturePreviewPlugin::EditorTexturePreviewPlugin() {
}

////////////////////////////////////////////////////////////////////////////

bool EditorImagePreviewPlugin::handles(const String &p_type) const {
	return p_type == "Image";
}

Ref<Texture2D> EditorImagePreviewPlugin::generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const {
	Ref<Image> img = p_from;

	if (img.is_null() || img->is_empty()) {
		return Ref<Image>();
	}

	img = img->duplicate();
	img->clear_mipmaps();

	if (img->is_compressed()) {
		if (img->decompress() != OK) {
			return Ref<Image>();
		}
	} else if (img->get_format() != Image::FORMAT_RGB8 && img->get_format() != Image::FORMAT_RGBA8) {
		img->convert(Image::FORMAT_RGBA8);
	}

	Vector2 new_size = img->get_size();
	if (new_size.x > p_size.x) {
		new_size = Vector2(p_size.x, new_size.y * p_size.x / new_size.x);
	}
	if (new_size.y > p_size.y) {
		new_size = Vector2(new_size.x * p_size.y / new_size.y, p_size.y);
	}
	img->resize(new_size.x, new_size.y, Image::INTERPOLATE_CUBIC);
	post_process_preview(img);

	return ImageTexture::create_from_image(img);
}

EditorImagePreviewPlugin::EditorImagePreviewPlugin() {
}

bool EditorImagePreviewPlugin::generate_small_preview_automatically() const {
	return true;
}

////////////////////////////////////////////////////////////////////////////

bool EditorBitmapPreviewPlugin::handles(const String &p_type) const {
	return ClassDB::is_parent_class(p_type, "BitMap");
}

Ref<Texture2D> EditorBitmapPreviewPlugin::generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const {
	Ref<BitMap> bm = p_from;

	if (bm->get_size() == Size2()) {
		return Ref<Texture2D>();
	}

	Vector<uint8_t> data;

	data.resize(bm->get_size().width * bm->get_size().height);

	{
		uint8_t *w = data.ptrw();

		for (int i = 0; i < bm->get_size().width; i++) {
			for (int j = 0; j < bm->get_size().height; j++) {
				if (bm->get_bit(i, j)) {
					w[j * (int)bm->get_size().width + i] = 255;
				} else {
					w[j * (int)bm->get_size().width + i] = 0;
				}
			}
		}
	}

	Ref<Image> img = Image::create_from_data(bm->get_size().width, bm->get_size().height, false, Image::FORMAT_L8, data);

	if (img->is_compressed()) {
		if (img->decompress() != OK) {
			return Ref<Texture2D>();
		}
	} else if (img->get_format() != Image::FORMAT_RGB8 && img->get_format() != Image::FORMAT_RGBA8) {
		img->convert(Image::FORMAT_RGBA8);
	}

	Vector2 new_size = img->get_size();
	if (new_size.x > p_size.x) {
		new_size = Vector2(p_size.x, new_size.y * p_size.x / new_size.x);
	}
	if (new_size.y > p_size.y) {
		new_size = Vector2(new_size.x * p_size.y / new_size.y, p_size.y);
	}
	img->resize(new_size.x, new_size.y, Image::INTERPOLATE_CUBIC);
	post_process_preview(img);

	return ImageTexture::create_from_image(img);
}

bool EditorBitmapPreviewPlugin::generate_small_preview_automatically() const {
	return true;
}

EditorBitmapPreviewPlugin::EditorBitmapPreviewPlugin() {
}

///////////////////////////////////////////////////////////////////////////

bool EditorPackedScenePreviewPlugin::handles(const String &p_type) const {
	return ClassDB::is_parent_class(p_type, "PackedScene");
}

Ref<Texture2D> EditorPackedScenePreviewPlugin::generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const {
	return generate_from_path(p_from->get_path(), p_size, p_metadata);
}

Ref<Texture2D> EditorPackedScenePreviewPlugin::generate_from_path(const String &p_path, const Size2 &p_size, Dictionary &p_metadata) const {
	String temp_path = EditorPaths::get_singleton()->get_cache_dir();
	String cache_base = ProjectSettings::get_singleton()->globalize_path(p_path).md5_text();
	cache_base = temp_path.path_join("resthumb-" + cache_base);

	//does not have it, try to load a cached thumbnail

	String path = cache_base + ".png";

	if (!FileAccess::exists(path)) {
		return Ref<Texture2D>();
	}

	Ref<Image> img;
	img.instantiate();
	Error err = img->load(path);
	if (err == OK) {
		post_process_preview(img);
		return ImageTexture::create_from_image(img);

	} else {
		return Ref<Texture2D>();
	}
}

EditorPackedScenePreviewPlugin::EditorPackedScenePreviewPlugin() {
}

//////////////////////////////////////////////////////////////////

void EditorMaterialPreviewPlugin::abort() {
	draw_requester.abort();
}

bool EditorMaterialPreviewPlugin::handles(const String &p_type) const {
	return ClassDB::is_parent_class(p_type, "Material"); // Any material.
}

bool EditorMaterialPreviewPlugin::generate_small_preview_automatically() const {
	return true;
}

Ref<Texture2D> EditorMaterialPreviewPlugin::generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const {
	Ref<Material> material = p_from;
	ERR_FAIL_COND_V(material.is_null(), Ref<Texture2D>());

	if (material->get_shader_mode() == Shader::MODE_SPATIAL) {
		RS::get_singleton()->mesh_surface_set_material(sphere, 0, material->get_rid());

		draw_requester.request_and_wait(viewport);

		Ref<Image> img = RS::get_singleton()->texture_2d_get(viewport_texture);
		RS::get_singleton()->mesh_surface_set_material(sphere, 0, RID());

		ERR_FAIL_COND_V(img.is_null(), Ref<ImageTexture>());

		img->convert(Image::FORMAT_RGBA8);
		int thumbnail_size = MAX(p_size.x, p_size.y);
		img->resize(thumbnail_size, thumbnail_size, Image::INTERPOLATE_CUBIC);
		post_process_preview(img);
		return ImageTexture::create_from_image(img);
	}

	return Ref<Texture2D>();
}

EditorMaterialPreviewPlugin::EditorMaterialPreviewPlugin() {
	scenario = RS::get_singleton()->scenario_create();

	viewport = RS::get_singleton()->viewport_create();
	RS::get_singleton()->viewport_set_update_mode(viewport, RS::VIEWPORT_UPDATE_DISABLED);
	RS::get_singleton()->viewport_set_scenario(viewport, scenario);
	RS::get_singleton()->viewport_set_size(viewport, 128, 128);
	RS::get_singleton()->viewport_set_transparent_background(viewport, true);
	RS::get_singleton()->viewport_set_active(viewport, true);
	viewport_texture = RS::get_singleton()->viewport_get_texture(viewport);

	camera = RS::get_singleton()->camera_create();
	RS::get_singleton()->viewport_attach_camera(viewport, camera);
	RS::get_singleton()->camera_set_transform(camera, Transform3D(Basis(), Vector3(0, 0, 3)));
	RS::get_singleton()->camera_set_perspective(camera, 45, 0.1, 10);

	if (GLOBAL_GET("rendering/lights_and_shadows/use_physical_light_units")) {
		camera_attributes = RS::get_singleton()->camera_attributes_create();
		RS::get_singleton()->camera_attributes_set_exposure(camera_attributes, 1.0, 0.000032552); // Matches default CameraAttributesPhysical to work well with default DirectionalLight3Ds.
		RS::get_singleton()->camera_set_camera_attributes(camera, camera_attributes);
	}

	light = RS::get_singleton()->directional_light_create();
	light_instance = RS::get_singleton()->instance_create2(light, scenario);
	RS::get_singleton()->instance_set_transform(light_instance, Transform3D().looking_at(Vector3(-1, -1, -1), Vector3(0, 1, 0)));

	light2 = RS::get_singleton()->directional_light_create();
	RS::get_singleton()->light_set_color(light2, Color(0.7, 0.7, 0.7));
	//RS::get_singleton()->light_set_color(light2, Color(0.7, 0.7, 0.7));

	light_instance2 = RS::get_singleton()->instance_create2(light2, scenario);

	RS::get_singleton()->instance_set_transform(light_instance2, Transform3D().looking_at(Vector3(0, 1, 0), Vector3(0, 0, 1)));

	sphere = RS::get_singleton()->mesh_create();
	sphere_instance = RS::get_singleton()->instance_create2(sphere, scenario);

	int lats = 32;
	int lons = 32;
	const double lat_step = Math_TAU / lats;
	const double lon_step = Math_TAU / lons;
	real_t radius = 1.0;

	Vector<Vector3> vertices;
	Vector<Vector3> normals;
	Vector<Vector2> uvs;
	Vector<real_t> tangents;
	Basis tt = Basis(Vector3(0, 1, 0), Math_PI * 0.5);

	for (int i = 1; i <= lats; i++) {
		double lat0 = lat_step * (i - 1) - Math_TAU / 4;
		double z0 = Math::sin(lat0);
		double zr0 = Math::cos(lat0);

		double lat1 = lat_step * i - Math_TAU / 4;
		double z1 = Math::sin(lat1);
		double zr1 = Math::cos(lat1);

		for (int j = lons; j >= 1; j--) {
			double lng0 = lon_step * (j - 1);
			double x0 = Math::cos(lng0);
			double y0 = Math::sin(lng0);

			double lng1 = lon_step * j;
			double x1 = Math::cos(lng1);
			double y1 = Math::sin(lng1);

			Vector3 v[4] = {
				Vector3(x1 * zr0, z0, y1 * zr0),
				Vector3(x1 * zr1, z1, y1 * zr1),
				Vector3(x0 * zr1, z1, y0 * zr1),
				Vector3(x0 * zr0, z0, y0 * zr0)
			};

#define ADD_POINT(m_idx)                                                                       \
	normals.push_back(v[m_idx]);                                                               \
	vertices.push_back(v[m_idx] * radius);                                                     \
	{                                                                                          \
		Vector2 uv(Math::atan2(v[m_idx].x, v[m_idx].z), Math::atan2(-v[m_idx].y, v[m_idx].z)); \
		uv /= Math_PI;                                                                         \
		uv *= 4.0;                                                                             \
		uv = uv * 0.5 + Vector2(0.5, 0.5);                                                     \
		uvs.push_back(uv);                                                                     \
	}                                                                                          \
	{                                                                                          \
		Vector3 t = tt.xform(v[m_idx]);                                                        \
		tangents.push_back(t.x);                                                               \
		tangents.push_back(t.y);                                                               \
		tangents.push_back(t.z);                                                               \
		tangents.push_back(1.0);                                                               \
	}

			ADD_POINT(0);
			ADD_POINT(1);
			ADD_POINT(2);

			ADD_POINT(2);
			ADD_POINT(3);
			ADD_POINT(0);
		}
	}

	Array arr;
	arr.resize(RS::ARRAY_MAX);
	arr[RS::ARRAY_VERTEX] = vertices;
	arr[RS::ARRAY_NORMAL] = normals;
	arr[RS::ARRAY_TANGENT] = tangents;
	arr[RS::ARRAY_TEX_UV] = uvs;
	RS::get_singleton()->mesh_add_surface_from_arrays(sphere, RS::PRIMITIVE_TRIANGLES, arr);
}

EditorMaterialPreviewPlugin::~EditorMaterialPreviewPlugin() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RS::get_singleton()->free(sphere);
	RS::get_singleton()->free(sphere_instance);
	RS::get_singleton()->free(viewport);
	RS::get_singleton()->free(light);
	RS::get_singleton()->free(light_instance);
	RS::get_singleton()->free(light2);
	RS::get_singleton()->free(light_instance2);
	RS::get_singleton()->free(camera);
	RS::get_singleton()->free(camera_attributes);
	RS::get_singleton()->free(scenario);
}

///////////////////////////////////////////////////////////////////////////

bool EditorScriptPreviewPlugin::handles(const String &p_type) const {
	return ClassDB::is_parent_class(p_type, "Script");
}

Ref<Texture2D> EditorScriptPreviewPlugin::generate_from_path(const String &p_path, const Size2 &p_size, Dictionary &p_metadata) const {
	Error err;
	String code = FileAccess::get_file_as_string(p_path, &err);
	if (err != OK) {
		return Ref<Texture2D>();
	}

	ScriptLanguage *lang = ScriptServer::get_language_for_extension(p_path.get_extension());
	return _generate_from_source_code(lang, code, p_size, p_metadata);
}

Ref<Texture2D> EditorScriptPreviewPlugin::generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const {
	Ref<Script> scr = p_from;
	if (scr.is_null()) {
		return Ref<Texture2D>();
	}

	String code = scr->get_source_code().strip_edges();
	return _generate_from_source_code(scr->get_language(), code, p_size, p_metadata);
}

Ref<Texture2D> EditorScriptPreviewPlugin::_generate_from_source_code(const ScriptLanguage *p_language, const String &p_source_code, const Size2 &p_size, Dictionary &p_metadata) const {
	if (p_source_code.is_empty()) {
		return Ref<Texture2D>();
	}

	List<String> kwors;
	if (p_language) {
		p_language->get_reserved_words(&kwors);
	}

	HashSet<String> control_flow_keywords;
	HashSet<String> keywords;

	for (const String &E : kwors) {
		if (p_language && p_language->is_control_flow_keyword(E)) {
			control_flow_keywords.insert(E);
		} else {
			keywords.insert(E);
		}
	}

	int line = 0;
	int col = 0;
	int thumbnail_size = MAX(p_size.x, p_size.y);
	Ref<Image> img = Image::create_empty(thumbnail_size, thumbnail_size, false, Image::FORMAT_RGBA8);

	Color bg_color = EDITOR_GET("text_editor/theme/highlighting/background_color");
	Color keyword_color = EDITOR_GET("text_editor/theme/highlighting/keyword_color");
	Color control_flow_keyword_color = EDITOR_GET("text_editor/theme/highlighting/control_flow_keyword_color");
	Color text_color = EDITOR_GET("text_editor/theme/highlighting/text_color");
	Color symbol_color = EDITOR_GET("text_editor/theme/highlighting/symbol_color");
	Color comment_color = EDITOR_GET("text_editor/theme/highlighting/comment_color");
	Color doc_comment_color = EDITOR_GET("text_editor/theme/highlighting/doc_comment_color");

	if (bg_color.a == 0) {
		bg_color = Color(0, 0, 0, 0);
	}
	bg_color.a = MAX(bg_color.a, 0.2); // Ensure we have some background, regardless of the text editor setting.

	img->fill(bg_color);

	const int x0 = thumbnail_size / 8;
	const int y0 = thumbnail_size / 8;
	const int available_height = thumbnail_size - 2 * y0;
	col = x0;

	bool prev_is_text = false;
	bool in_control_flow_keyword = false;
	bool in_keyword = false;
	bool in_comment = false;
	bool in_doc_comment = false;
	for (int i = 0; i < p_source_code.length(); i++) {
		char32_t c = p_source_code[i];
		if (c > 32) {
			if (col < thumbnail_size) {
				Color color = text_color;

				if (c == '#') {
					if (i < p_source_code.length() - 1 && p_source_code[i + 1] == '#') {
						in_doc_comment = true;
					} else {
						in_comment = true;
					}
				}

				if (in_comment) {
					color = comment_color;
				} else if (in_doc_comment) {
					color = doc_comment_color;
				} else {
					if (is_symbol(c)) {
						// Make symbol a little visible.
						color = symbol_color;
						in_control_flow_keyword = false;
						in_keyword = false;
					} else if (!prev_is_text && is_ascii_identifier_char(c)) {
						int pos = i;

						while (is_ascii_identifier_char(p_source_code[pos])) {
							pos++;
						}
						String word = p_source_code.substr(i, pos - i);
						if (control_flow_keywords.has(word)) {
							in_control_flow_keyword = true;
						} else if (keywords.has(word)) {
							in_keyword = true;
						}

					} else if (!is_ascii_identifier_char(c)) {
						in_keyword = false;
					}

					if (in_control_flow_keyword) {
						color = control_flow_keyword_color;
					} else if (in_keyword) {
						color = keyword_color;
					}
				}
				Color ul = color;
				ul.a *= 0.5;
				img->set_pixel(col, y0 + line * 2, bg_color.blend(ul));
				img->set_pixel(col, y0 + line * 2 + 1, color);

				prev_is_text = is_ascii_identifier_char(c);
			}
			col++;
		} else {
			prev_is_text = false;
			in_control_flow_keyword = false;
			in_keyword = false;

			if (c == '\n') {
				in_comment = false;
				in_doc_comment = false;

				col = x0;
				line++;
				if (line >= available_height / 2) {
					break;
				}
			} else if (c == '\t') {
				col += 3;
			} else {
				col++;
			}
		}
	}
	post_process_preview(img);
	return ImageTexture::create_from_image(img);
}

EditorScriptPreviewPlugin::EditorScriptPreviewPlugin() {
}

///////////////////////////////////////////////////////////////////

bool EditorAudioStreamPreviewPlugin::handles(const String &p_type) const {
	return ClassDB::is_parent_class(p_type, "AudioStream");
}

Ref<Texture2D> EditorAudioStreamPreviewPlugin::generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const {
	Ref<AudioStream> stream = p_from;
	ERR_FAIL_COND_V(stream.is_null(), Ref<Texture2D>());

	Vector<uint8_t> img;

	int w = p_size.x;
	int h = p_size.y;
	img.resize(w * h * 3);

	uint8_t *imgdata = img.ptrw();
	uint8_t *imgw = imgdata;

	Ref<AudioStreamPlayback> playback = stream->instantiate_playback();
	ERR_FAIL_COND_V(playback.is_null(), Ref<Texture2D>());

	real_t len_s = stream->get_length();
	if (len_s == 0) {
		len_s = 60; //one minute audio if no length specified
	}
	int frame_length = AudioServer::get_singleton()->get_mix_rate() * len_s;

	Vector<AudioFrame> frames;
	frames.resize(frame_length);

	playback->start();
	playback->mix(frames.ptrw(), 1, frames.size());
	playback->stop();

	for (int i = 0; i < w; i++) {
		real_t max = -1000;
		real_t min = 1000;
		int from = uint64_t(i) * frame_length / w;
		int to = (uint64_t(i) + 1) * frame_length / w;
		to = MIN(to, frame_length);
		from = MIN(from, frame_length - 1);
		if (to == from) {
			to = from + 1;
		}

		for (int j = from; j < to; j++) {
			max = MAX(max, frames[j].left);
			max = MAX(max, frames[j].right);

			min = MIN(min, frames[j].left);
			min = MIN(min, frames[j].right);
		}

		int pfrom = CLAMP((min * 0.5 + 0.5) * h / 2, 0, h / 2) + h / 4;
		int pto = CLAMP((max * 0.5 + 0.5) * h / 2, 0, h / 2) + h / 4;

		for (int j = 0; j < h; j++) {
			uint8_t *p = &imgw[(j * w + i) * 3];
			if (j < pfrom || j > pto) {
				p[0] = 100;
				p[1] = 100;
				p[2] = 100;
			} else {
				p[0] = 180;
				p[1] = 180;
				p[2] = 180;
			}
		}
	}

	p_metadata["length"] = stream->get_length();

	//post_process_preview(img);

	Ref<Image> image = Image::create_from_data(w, h, false, Image::FORMAT_RGB8, img);
	return ImageTexture::create_from_image(image);
}

EditorAudioStreamPreviewPlugin::EditorAudioStreamPreviewPlugin() {
}

///////////////////////////////////////////////////////////////////////////

void EditorMeshPreviewPlugin::abort() {
	draw_requester.abort();
}

bool EditorMeshPreviewPlugin::handles(const String &p_type) const {
	return ClassDB::is_parent_class(p_type, "Mesh"); // Any mesh.
}

Ref<Texture2D> EditorMeshPreviewPlugin::generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const {
	Ref<Mesh> mesh = p_from;
	ERR_FAIL_COND_V(mesh.is_null(), Ref<Texture2D>());

	RS::get_singleton()->instance_set_base(mesh_instance, mesh->get_rid());

	AABB aabb = mesh->get_aabb();
	Vector3 ofs = aabb.get_center();
	aabb.position -= ofs;
	Transform3D xform;
	xform.basis = Basis().rotated(Vector3(0, 1, 0), -Math_PI * 0.125);
	xform.basis = Basis().rotated(Vector3(1, 0, 0), Math_PI * 0.125) * xform.basis;
	AABB rot_aabb = xform.xform(aabb);
	real_t m = MAX(rot_aabb.size.x, rot_aabb.size.y) * 0.5;
	if (m == 0) {
		return Ref<Texture2D>();
	}
	m = 1.0 / m;
	m *= 0.5;
	xform.basis.scale(Vector3(m, m, m));
	xform.origin = -xform.basis.xform(ofs); //-ofs*m;
	xform.origin.z -= rot_aabb.size.z * 2;
	RS::get_singleton()->instance_set_transform(mesh_instance, xform);

	draw_requester.request_and_wait(viewport);

	Ref<Image> img = RS::get_singleton()->texture_2d_get(viewport_texture);
	ERR_FAIL_COND_V(img.is_null(), Ref<ImageTexture>());

	RS::get_singleton()->instance_set_base(mesh_instance, RID());

	img->convert(Image::FORMAT_RGBA8);

	Vector2 new_size = img->get_size();
	if (new_size.x > p_size.x) {
		new_size = Vector2(p_size.x, new_size.y * p_size.x / new_size.x);
	}
	if (new_size.y > p_size.y) {
		new_size = Vector2(new_size.x * p_size.y / new_size.y, p_size.y);
	}
	img->resize(new_size.x, new_size.y, Image::INTERPOLATE_CUBIC);
	post_process_preview(img);

	return ImageTexture::create_from_image(img);
}

EditorMeshPreviewPlugin::EditorMeshPreviewPlugin() {
	scenario = RS::get_singleton()->scenario_create();

	viewport = RS::get_singleton()->viewport_create();
	RS::get_singleton()->viewport_set_update_mode(viewport, RS::VIEWPORT_UPDATE_DISABLED);
	RS::get_singleton()->viewport_set_scenario(viewport, scenario);
	RS::get_singleton()->viewport_set_size(viewport, 128, 128);
	RS::get_singleton()->viewport_set_transparent_background(viewport, true);
	RS::get_singleton()->viewport_set_active(viewport, true);
	viewport_texture = RS::get_singleton()->viewport_get_texture(viewport);

	camera = RS::get_singleton()->camera_create();
	RS::get_singleton()->viewport_attach_camera(viewport, camera);
	RS::get_singleton()->camera_set_transform(camera, Transform3D(Basis(), Vector3(0, 0, 3)));
	//RS::get_singleton()->camera_set_perspective(camera,45,0.1,10);
	RS::get_singleton()->camera_set_orthogonal(camera, 1.0, 0.01, 1000.0);

	if (GLOBAL_GET("rendering/lights_and_shadows/use_physical_light_units")) {
		camera_attributes = RS::get_singleton()->camera_attributes_create();
		RS::get_singleton()->camera_attributes_set_exposure(camera_attributes, 1.0, 0.000032552); // Matches default CameraAttributesPhysical to work well with default DirectionalLight3Ds.
		RS::get_singleton()->camera_set_camera_attributes(camera, camera_attributes);
	}

	light = RS::get_singleton()->directional_light_create();
	light_instance = RS::get_singleton()->instance_create2(light, scenario);
	RS::get_singleton()->instance_set_transform(light_instance, Transform3D().looking_at(Vector3(-1, -1, -1), Vector3(0, 1, 0)));

	light2 = RS::get_singleton()->directional_light_create();
	RS::get_singleton()->light_set_color(light2, Color(0.7, 0.7, 0.7));
	//RS::get_singleton()->light_set_color(light2, RS::LIGHT_COLOR_SPECULAR, Color(0.0, 0.0, 0.0));
	light_instance2 = RS::get_singleton()->instance_create2(light2, scenario);

	RS::get_singleton()->instance_set_transform(light_instance2, Transform3D().looking_at(Vector3(0, 1, 0), Vector3(0, 0, 1)));

	//sphere = RS::get_singleton()->mesh_create();
	mesh_instance = RS::get_singleton()->instance_create();
	RS::get_singleton()->instance_set_scenario(mesh_instance, scenario);
}

EditorMeshPreviewPlugin::~EditorMeshPreviewPlugin() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	//RS::get_singleton()->free(sphere);
	RS::get_singleton()->free(mesh_instance);
	RS::get_singleton()->free(viewport);
	RS::get_singleton()->free(light);
	RS::get_singleton()->free(light_instance);
	RS::get_singleton()->free(light2);
	RS::get_singleton()->free(light_instance2);
	RS::get_singleton()->free(camera);
	RS::get_singleton()->free(camera_attributes);
	RS::get_singleton()->free(scenario);
}

///////////////////////////////////////////////////////////////////////////

void EditorFontPreviewPlugin::abort() {
	draw_requester.abort();
}

bool EditorFontPreviewPlugin::handles(const String &p_type) const {
	return ClassDB::is_parent_class(p_type, "Font");
}

Ref<Texture2D> EditorFontPreviewPlugin::generate_from_path(const String &p_path, const Size2 &p_size, Dictionary &p_metadata) const {
	Ref<Font> sampled_font = ResourceLoader::load(p_path);
	ERR_FAIL_COND_V(sampled_font.is_null(), Ref<Texture2D>());

	String sample;
	static const String sample_base = U"12漢字ԱբΑαАбΑαאבابܐܒހށआআਆઆଆஆఆಆആආกิກິༀကႠა한글ሀᎣᐁᚁᚠᜀᜠᝀᝠកᠠᤁᥐAb😀";
	for (int i = 0; i < sample_base.length(); i++) {
		if (sampled_font->has_char(sample_base[i])) {
			sample += sample_base[i];
		}
	}
	if (sample.is_empty()) {
		sample = sampled_font->get_supported_chars().substr(0, 6);
	}
	Vector2 size = sampled_font->get_string_size(sample, HORIZONTAL_ALIGNMENT_LEFT, -1, 50);

	Vector2 pos;

	pos.x = 64 - size.x / 2;
	pos.y = 80;

	const Color c = GLOBAL_GET("rendering/environment/defaults/default_clear_color");
	const float fg = c.get_luminance() < 0.5 ? 1.0 : 0.0;
	sampled_font->draw_string(canvas_item, pos, sample, HORIZONTAL_ALIGNMENT_LEFT, -1.f, 50, Color(fg, fg, fg));

	draw_requester.request_and_wait(viewport);

	RS::get_singleton()->canvas_item_clear(canvas_item);

	Ref<Image> img = RS::get_singleton()->texture_2d_get(viewport_texture);
	ERR_FAIL_COND_V(img.is_null(), Ref<ImageTexture>());

	img->convert(Image::FORMAT_RGBA8);

	Vector2 new_size = img->get_size();
	if (new_size.x > p_size.x) {
		new_size = Vector2(p_size.x, new_size.y * p_size.x / new_size.x);
	}
	if (new_size.y > p_size.y) {
		new_size = Vector2(new_size.x * p_size.y / new_size.y, p_size.y);
	}
	img->resize(new_size.x, new_size.y, Image::INTERPOLATE_CUBIC);
	post_process_preview(img);

	return ImageTexture::create_from_image(img);
}

Ref<Texture2D> EditorFontPreviewPlugin::generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const {
	String path = p_from->get_path();
	if (!FileAccess::exists(path)) {
		return Ref<Texture2D>();
	}
	return generate_from_path(path, p_size, p_metadata);
}

EditorFontPreviewPlugin::EditorFontPreviewPlugin() {
	viewport = RS::get_singleton()->viewport_create();
	RS::get_singleton()->viewport_set_update_mode(viewport, RS::VIEWPORT_UPDATE_DISABLED);
	RS::get_singleton()->viewport_set_size(viewport, 128, 128);
	RS::get_singleton()->viewport_set_active(viewport, true);
	viewport_texture = RS::get_singleton()->viewport_get_texture(viewport);

	canvas = RS::get_singleton()->canvas_create();
	canvas_item = RS::get_singleton()->canvas_item_create();

	RS::get_singleton()->viewport_attach_canvas(viewport, canvas);
	RS::get_singleton()->canvas_item_set_parent(canvas_item, canvas);
}

EditorFontPreviewPlugin::~EditorFontPreviewPlugin() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RS::get_singleton()->free(canvas_item);
	RS::get_singleton()->free(canvas);
	RS::get_singleton()->free(viewport);
}

////////////////////////////////////////////////////////////////////////////

static const real_t GRADIENT_PREVIEW_TEXTURE_SCALE_FACTOR = 4.0;

bool EditorGradientPreviewPlugin::handles(const String &p_type) const {
	return ClassDB::is_parent_class(p_type, "Gradient");
}

bool EditorGradientPreviewPlugin::generate_small_preview_automatically() const {
	return true;
}

Ref<Texture2D> EditorGradientPreviewPlugin::generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const {
	Ref<Gradient> gradient = p_from;
	if (gradient.is_valid()) {
		Ref<GradientTexture1D> ptex;
		ptex.instantiate();
		ptex->set_width(p_size.width * GRADIENT_PREVIEW_TEXTURE_SCALE_FACTOR * EDSCALE);
		ptex->set_gradient(gradient);
		return ImageTexture::create_from_image(ptex->get_image());
	}
	return Ref<Texture2D>();
}

EditorGradientPreviewPlugin::EditorGradientPreviewPlugin() {
}
