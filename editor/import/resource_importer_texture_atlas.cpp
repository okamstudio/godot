/**************************************************************************/
/*  resource_importer_texture_atlas.cpp                                   */
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

#include "resource_importer_texture_atlas.h"

#include "atlas_import_failed.xpm"
#include "core/config/project_settings.h"
#include "core/io/image_loader.h"
#include "core/io/resource_saver.h"
#include "core/math/geometry_2d.h"
#include "editor/editor_atlas_packer.h"
#include "scene/resources/atlas_texture.h"
#include "scene/resources/bit_map.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/mesh.h"
#include "scene/resources/mesh_texture.h"

String ResourceImporterTextureAtlas::get_importer_name() const {
	return "texture_atlas";
}

String ResourceImporterTextureAtlas::get_visible_name() const {
	return "TextureAtlas";
}

void ResourceImporterTextureAtlas::get_recognized_extensions(List<String> *p_extensions) const {
	ImageLoader::get_recognized_extensions(p_extensions);
}

String ResourceImporterTextureAtlas::get_save_extension() const {
	return "res";
}

String ResourceImporterTextureAtlas::get_resource_type() const {
	return "Texture2D";
}

bool ResourceImporterTextureAtlas::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	if (p_option == "crop_to_region" && int(p_options["import_mode"]) != IMPORT_MODE_REGION) {
		return false;
	} else if (p_option == "trim_alpha_border_from_region" && int(p_options["import_mode"]) != IMPORT_MODE_REGION) {
		return false;
	}

	return true;
}

int ResourceImporterTextureAtlas::get_preset_count() const {
	return 0;
}

String ResourceImporterTextureAtlas::get_preset_name(int p_idx) const {
	return String();
}

void ResourceImporterTextureAtlas::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::STRING, "atlas_file", PROPERTY_HINT_SAVE_FILE, "*.png"), ""));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "import_mode", PROPERTY_HINT_ENUM, "Region,Mesh2D", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "crop_to_region"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "trim_alpha_border_from_region"), true));
}

String ResourceImporterTextureAtlas::get_option_group_file() const {
	return "atlas_file";
}

Error ResourceImporterTextureAtlas::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	/* If this happens, it's because the atlas_file field was not filled, so just import a broken texture */

	//use an xpm because it's size independent, the editor images are vector and size dependent
	//it's a simple hack
	Ref<Image> broken = memnew(Image((const char **)atlas_import_failed_xpm));
	ResourceSaver::save(ImageTexture::create_from_image(broken), p_save_path + ".tex");

	return OK;
}

// FIXME: Rasterization has issues, see https://github.com/godotengine/godot/issues/68350#issuecomment-1305610290
static void _plot_triangle(Vector2i *p_vertices, const Vector2i &p_offset, bool p_transposed, Ref<Image> p_image, const Ref<Image> &p_src_image) {
	int width = p_image->get_width();
	int height = p_image->get_height();
	int src_width = p_src_image->get_width();
	int src_height = p_src_image->get_height();

	int x[3];
	int y[3];

	for (int j = 0; j < 3; j++) {
		x[j] = p_vertices[j].x;
		y[j] = p_vertices[j].y;
	}

	// sort the points vertically
	if (y[1] > y[2]) {
		SWAP(x[1], x[2]);
		SWAP(y[1], y[2]);
	}
	if (y[0] > y[1]) {
		SWAP(x[0], x[1]);
		SWAP(y[0], y[1]);
	}
	if (y[1] > y[2]) {
		SWAP(x[1], x[2]);
		SWAP(y[1], y[2]);
	}

	double dx_far = double(x[2] - x[0]) / (y[2] - y[0] + 1);
	double dx_upper = double(x[1] - x[0]) / (y[1] - y[0] + 1);
	double dx_low = double(x[2] - x[1]) / (y[2] - y[1] + 1);
	double xf = x[0];
	double xt = x[0] + dx_upper; // if y[0] == y[1], special case
	int max_y = MIN(y[2], p_transposed ? (width - p_offset.x - 1) : (height - p_offset.y - 1));
	for (int yi = y[0]; yi < max_y; yi++) {
		if (yi >= 0) {
			for (int xi = (xf > 0 ? int(xf) : 0); xi < (xt <= src_width ? xt : src_width); xi++) {
				int px = xi, py = yi;
				int sx = px, sy = py;
				sx = CLAMP(sx, 0, src_width - 1);
				sy = CLAMP(sy, 0, src_height - 1);
				Color color = p_src_image->get_pixel(sx, sy);
				if (p_transposed) {
					SWAP(px, py);
				}
				px += p_offset.x;
				py += p_offset.y;

				//may have been cropped, so don't blit what is not visible?
				if (px < 0 || px >= width) {
					continue;
				}
				if (py < 0 || py >= height) {
					continue;
				}
				p_image->set_pixel(px, py, color);
			}

			for (int xi = (xf < src_width ? int(xf) : src_width - 1); xi >= (xt > 0 ? xt : 0); xi--) {
				int px = xi, py = yi;
				int sx = px, sy = py;
				sx = CLAMP(sx, 0, src_width - 1);
				sy = CLAMP(sy, 0, src_height - 1);
				Color color = p_src_image->get_pixel(sx, sy);
				if (p_transposed) {
					SWAP(px, py);
				}
				px += p_offset.x;
				py += p_offset.y;

				//may have been cropped, so don't blit what is not visible?
				if (px < 0 || px >= width) {
					continue;
				}
				if (py < 0 || py >= height) {
					continue;
				}
				p_image->set_pixel(px, py, color);
			}
		}
		xf += dx_far;
		if (yi < y[1]) {
			xt += dx_upper;
		} else {
			xt += dx_low;
		}
	}
}

Error ResourceImporterTextureAtlas::import_group_file(const String &p_group_file, const HashMap<String, HashMap<StringName, Variant>> &p_source_file_options, const HashMap<String, String> &p_base_paths) {
	ERR_FAIL_COND_V(p_source_file_options.is_empty(), ERR_BUG); //should never happen

	Vector<EditorAtlasPacker::Chart> charts;
	Vector<PackData> pack_data_files;

	pack_data_files.resize(p_source_file_options.size());

	int idx = 0;
	for (const KeyValue<String, HashMap<StringName, Variant>> &E : p_source_file_options) {
		PackData &pack_data = pack_data_files.write[idx];
		const String &source = E.key;
		const HashMap<StringName, Variant> &options = E.value;

		Ref<Image> image;
		image.instantiate();
		Error err = ImageLoader::load_image(source, image);
		ERR_CONTINUE(err != OK);

		pack_data.image = image;

		int mode = options["import_mode"];

		if (mode == IMPORT_MODE_REGION) {
			pack_data.is_mesh = false;
			pack_data.is_cropped = options["crop_to_region"];

			EditorAtlasPacker::Chart chart;

			Rect2i used_rect = Rect2i(Vector2i(), image->get_size());
			if (options["trim_alpha_border_from_region"]) {
				// Clip a region from the image.
				used_rect = image->get_used_rect();
			}
			pack_data.region = used_rect;

			chart.vertices.push_back(used_rect.position);
			chart.vertices.push_back(used_rect.position + Vector2i(used_rect.size.x, 0));
			chart.vertices.push_back(used_rect.position + Vector2i(used_rect.size.x, used_rect.size.y));
			chart.vertices.push_back(used_rect.position + Vector2i(0, used_rect.size.y));
			EditorAtlasPacker::Chart::Face f;
			f.vertex[0] = 0;
			f.vertex[1] = 1;
			f.vertex[2] = 2;
			chart.faces.push_back(f);
			f.vertex[0] = 0;
			f.vertex[1] = 2;
			f.vertex[2] = 3;
			chart.faces.push_back(f);
			chart.can_transpose = false;
			pack_data.chart_vertices.push_back(chart.vertices);
			pack_data.chart_pieces.push_back(charts.size());
			charts.push_back(chart);

		} else {
			pack_data.is_mesh = true;

			Ref<BitMap> bit_map;
			bit_map.instantiate();
			bit_map->create_from_image_alpha(image);
			Vector<Vector<Vector2>> polygons = bit_map->clip_opaque_to_polygons(Rect2(Vector2(), image->get_size()));

			for (int j = 0; j < polygons.size(); j++) {
				EditorAtlasPacker::Chart chart;
				chart.vertices = polygons[j];
				chart.can_transpose = true;

				Vector<int> poly = Geometry2D::triangulate_polygon(polygons[j]);
				for (int i = 0; i < poly.size(); i += 3) {
					EditorAtlasPacker::Chart::Face f;
					f.vertex[0] = poly[i + 0];
					f.vertex[1] = poly[i + 1];
					f.vertex[2] = poly[i + 2];
					chart.faces.push_back(f);
				}

				pack_data.chart_pieces.push_back(charts.size());
				charts.push_back(chart);

				pack_data.chart_vertices.push_back(polygons[j]);
			}
		}
		idx++;
	}

	const int max_width = (int)GLOBAL_GET("editor/import/atlas_max_width");

	//pack the charts
	int atlas_width, atlas_height;
	EditorAtlasPacker::chart_pack(charts, atlas_width, atlas_height, max_width);

	if (atlas_height > max_width * 2) {
		WARN_PRINT(vformat(TTR("%s: Atlas texture significantly larger on one axis (%d), consider changing the `editor/import/atlas_max_width` Project Setting to allow a wider texture, making the result more even in size."), p_group_file, atlas_height));
	}

	//blit the atlas
	Ref<Image> new_atlas = Image::create_empty(atlas_width, atlas_height, false, Image::FORMAT_RGBA8);

	for (int i = 0; i < pack_data_files.size(); i++) {
		PackData &pack_data = pack_data_files.write[i];

		for (int j = 0; j < pack_data.chart_pieces.size(); j++) {
			const EditorAtlasPacker::Chart &chart = charts[pack_data.chart_pieces[j]];
			for (int k = 0; k < chart.faces.size(); k++) {
				Vector2i positions[3];
				for (int l = 0; l < 3; l++) {
					int vertex_idx = chart.faces[k].vertex[l];
					positions[l] = Vector2i(chart.vertices[vertex_idx]);
				}

				_plot_triangle(positions, Vector2i(chart.final_offset), chart.transposed, new_atlas, pack_data.image);
			}
		}
	}

	//save the atlas

	new_atlas->save_png(p_group_file);

	//update cache if existing, else create
	Ref<Texture2D> cache;
	cache = ResourceCache::get_ref(p_group_file);
	if (cache.is_null()) {
		Ref<ImageTexture> res_cache = ImageTexture::create_from_image(new_atlas);
		res_cache->set_path(p_group_file);
		cache = res_cache;
	}

	//save the images
	idx = 0;
	for (const KeyValue<String, HashMap<StringName, Variant>> &E : p_source_file_options) {
		PackData &pack_data = pack_data_files.write[idx];

		Ref<Texture2D> texture;

		if (!pack_data.is_mesh) {
			Vector2 offset = charts[pack_data.chart_pieces[0]].vertices[0] + charts[pack_data.chart_pieces[0]].final_offset;

			//region
			Ref<AtlasTexture> atlas_texture;
			atlas_texture.instantiate();
			atlas_texture->set_atlas(cache);
			atlas_texture->set_region(Rect2(offset, pack_data.region.size));

			if (!pack_data.is_cropped) {
				atlas_texture->set_margin(Rect2(pack_data.region.position, pack_data.image->get_size() - pack_data.region.size));
			}

			texture = atlas_texture;
		} else {
			Ref<ArrayMesh> mesh;
			mesh.instantiate();

			for (int i = 0; i < pack_data.chart_pieces.size(); i++) {
				const EditorAtlasPacker::Chart &chart = charts[pack_data.chart_pieces[i]];
				Vector<Vector2> vertices;
				Vector<int> indices;
				Vector<Vector2> uvs;
				int vc = chart.vertices.size();
				int fc = chart.faces.size();
				vertices.resize(vc);
				uvs.resize(vc);
				indices.resize(fc * 3);

				{
					Vector2 *vw = vertices.ptrw();
					int *iw = indices.ptrw();
					Vector2 *uvw = uvs.ptrw();

					for (int j = 0; j < vc; j++) {
						vw[j] = chart.vertices[j];
						Vector2 uv = chart.vertices[j];
						if (chart.transposed) {
							SWAP(uv.x, uv.y);
						}
						uv += chart.final_offset;
						uv /= new_atlas->get_size(); //normalize uv to 0-1 range
						uvw[j] = uv;
					}

					for (int j = 0; j < fc; j++) {
						iw[j * 3 + 0] = chart.faces[j].vertex[0];
						iw[j * 3 + 1] = chart.faces[j].vertex[1];
						iw[j * 3 + 2] = chart.faces[j].vertex[2];
					}
				}

				Array arrays;
				arrays.resize(Mesh::ARRAY_MAX);
				arrays[Mesh::ARRAY_VERTEX] = vertices;
				arrays[Mesh::ARRAY_TEX_UV] = uvs;
				arrays[Mesh::ARRAY_INDEX] = indices;

				mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
			}

			Ref<MeshTexture> mesh_texture;
			mesh_texture.instantiate();
			mesh_texture->set_base_texture(cache);
			mesh_texture->set_image_size(pack_data.image->get_size());
			mesh_texture->set_mesh(mesh);

			texture = mesh_texture;
		}

		String save_path = p_base_paths[E.key] + ".res";
		ResourceSaver::save(texture, save_path);
		idx++;
	}

	return OK;
}

ResourceImporterTextureAtlas::ResourceImporterTextureAtlas() {
}
