/**************************************************************************/
/*  voxelizer.cpp                                                         */
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

#include "voxelizer.h"

#include "core/config/project_settings.h"

static _FORCE_INLINE_ void get_uv_and_normal(const Vector3 &p_pos, const Vector3 *p_vtx, const Vector2 *p_uv, const Vector3 *p_normal, Vector2 &r_uv, Vector3 &r_normal) {
	if (p_pos.is_equal_approx(p_vtx[0])) {
		r_uv = p_uv[0];
		r_normal = p_normal[0];
		return;
	}
	if (p_pos.is_equal_approx(p_vtx[1])) {
		r_uv = p_uv[1];
		r_normal = p_normal[1];
		return;
	}
	if (p_pos.is_equal_approx(p_vtx[2])) {
		r_uv = p_uv[2];
		r_normal = p_normal[2];
		return;
	}

	Vector3 v0 = p_vtx[1] - p_vtx[0];
	Vector3 v1 = p_vtx[2] - p_vtx[0];
	Vector3 v2 = p_pos - p_vtx[0];

	real_t d00 = v0.dot(v0);
	real_t d01 = v0.dot(v1);
	real_t d11 = v1.dot(v1);
	real_t d20 = v2.dot(v0);
	real_t d21 = v2.dot(v1);
	real_t denom = (d00 * d11 - d01 * d01);
	if (denom == 0) {
		r_uv = p_uv[0];
		r_normal = p_normal[0];
		return;
	}
	real_t v = (d11 * d20 - d01 * d21) / denom;
	real_t w = (d00 * d21 - d01 * d20) / denom;
	real_t u = 1.0f - v - w;

	r_uv = p_uv[0] * u + p_uv[1] * v + p_uv[2] * w;
	r_normal = (p_normal[0] * u + p_normal[1] * v + p_normal[2] * w).normalized();
}

void Voxelizer::_plot_face(int p_idx, int p_level, int p_x, int p_y, int p_z, const Vector3 *p_vtx, const Vector3 *p_normal, const Vector2 *p_uv, const MaterialCache &p_material, const AABB &p_aabb) {
	if (p_level == cell_subdiv) {
		//plot the face by guessing its albedo and emission value

		//find best axis to map to, for scanning values
		int closest_axis = 0;
		real_t closest_dot = 0;

		Plane plane = Plane(p_vtx[0], p_vtx[1], p_vtx[2]);
		Vector3 normal = plane.normal;

		for (int i = 0; i < 3; i++) {
			Vector3 axis;
			axis[i] = 1.0;
			real_t dot = ABS(normal.dot(axis));
			if (i == 0 || dot > closest_dot) {
				closest_axis = i;
				closest_dot = dot;
			}
		}

		Vector3 axis;
		axis[closest_axis] = 1.0;
		Vector3 t1;
		t1[(closest_axis + 1) % 3] = 1.0;
		Vector3 t2;
		t2[(closest_axis + 2) % 3] = 1.0;

		t1 *= p_aabb.size[(closest_axis + 1) % 3] / real_t(color_scan_cell_width);
		t2 *= p_aabb.size[(closest_axis + 2) % 3] / real_t(color_scan_cell_width);

		Color albedo_accum;
		Color emission_accum;
		Vector3 normal_accum;

		float alpha = 0.0;

		//map to a grid average in the best axis for this face
		for (int i = 0; i < color_scan_cell_width; i++) {
			Vector3 ofs_i = real_t(i) * t1;

			for (int j = 0; j < color_scan_cell_width; j++) {
				Vector3 ofs_j = real_t(j) * t2;

				Vector3 from = p_aabb.position + ofs_i + ofs_j;
				Vector3 to = from + t1 + t2 + axis * p_aabb.size[closest_axis];
				Vector3 half = (to - from) * 0.5;

				//is in this cell?
				if (!Geometry3D::triangle_box_overlap(from + half, half, p_vtx)) {
					continue; //face does not span this cell
				}

				//go from -size to +size*2 to avoid skipping collisions
				Vector3 ray_from = from + (t1 + t2) * 0.5 - axis * p_aabb.size[closest_axis];
				Vector3 ray_to = ray_from + axis * p_aabb.size[closest_axis] * 2;

				if (normal.dot(ray_from - ray_to) < 0) {
					SWAP(ray_from, ray_to);
				}

				Vector3 intersection;

				if (!plane.intersects_segment(ray_from, ray_to, &intersection)) {
					if (ABS(plane.distance_to(ray_from)) < ABS(plane.distance_to(ray_to))) {
						intersection = plane.project(ray_from);
					} else {
						intersection = plane.project(ray_to);
					}
				}

				intersection = Face3(p_vtx[0], p_vtx[1], p_vtx[2]).get_closest_point_to(intersection);

				Vector2 uv;
				Vector3 lnormal;
				get_uv_and_normal(intersection, p_vtx, p_uv, p_normal, uv, lnormal);
				if (lnormal == Vector3()) { //just in case normal is not provided
					lnormal = normal;
				}

				int uv_x = CLAMP(int(Math::fposmod(uv.x, (real_t)1.0) * bake_texture_size), 0, bake_texture_size - 1);
				int uv_y = CLAMP(int(Math::fposmod(uv.y, (real_t)1.0) * bake_texture_size), 0, bake_texture_size - 1);

				int ofs = uv_y * bake_texture_size + uv_x;
				albedo_accum.r += p_material.albedo[ofs].r;
				albedo_accum.g += p_material.albedo[ofs].g;
				albedo_accum.b += p_material.albedo[ofs].b;
				albedo_accum.a += p_material.albedo[ofs].a;

				emission_accum.r += p_material.emission[ofs].r;
				emission_accum.g += p_material.emission[ofs].g;
				emission_accum.b += p_material.emission[ofs].b;

				normal_accum += lnormal;

				alpha += 1.0;
			}
		}

		if (alpha == 0) {
			//could not in any way get texture information.. so use closest point to center

			Face3 f(p_vtx[0], p_vtx[1], p_vtx[2]);
			Vector3 inters = f.get_closest_point_to(p_aabb.get_center());

			Vector3 lnormal;
			Vector2 uv;
			get_uv_and_normal(inters, p_vtx, p_uv, p_normal, uv, normal);
			if (lnormal == Vector3()) { //just in case normal is not provided
				lnormal = normal;
			}

			int uv_x = CLAMP(Math::fposmod(uv.x, (real_t)1.0) * bake_texture_size, 0, bake_texture_size - 1);
			int uv_y = CLAMP(Math::fposmod(uv.y, (real_t)1.0) * bake_texture_size, 0, bake_texture_size - 1);

			int ofs = uv_y * bake_texture_size + uv_x;

			alpha = 1.0 / (color_scan_cell_width * color_scan_cell_width);

			albedo_accum.r = p_material.albedo[ofs].r * alpha;
			albedo_accum.g = p_material.albedo[ofs].g * alpha;
			albedo_accum.b = p_material.albedo[ofs].b * alpha;
			albedo_accum.a = p_material.albedo[ofs].a * alpha;

			emission_accum.r = p_material.emission[ofs].r * alpha;
			emission_accum.g = p_material.emission[ofs].g * alpha;
			emission_accum.b = p_material.emission[ofs].b * alpha;

			normal_accum = lnormal * alpha;

		} else {
			float accdiv = 1.0 / (color_scan_cell_width * color_scan_cell_width);
			alpha *= accdiv;

			albedo_accum.r *= accdiv;
			albedo_accum.g *= accdiv;
			albedo_accum.b *= accdiv;
			albedo_accum.a *= accdiv;

			emission_accum.r *= accdiv;
			emission_accum.g *= accdiv;
			emission_accum.b *= accdiv;

			normal_accum *= accdiv;
		}

		//put this temporarily here, corrected in a later step
		bake_cells.write[p_idx].albedo[0] += albedo_accum.r;
		bake_cells.write[p_idx].albedo[1] += albedo_accum.g;
		bake_cells.write[p_idx].albedo[2] += albedo_accum.b;
		bake_cells.write[p_idx].emission[0] += emission_accum.r;
		bake_cells.write[p_idx].emission[1] += emission_accum.g;
		bake_cells.write[p_idx].emission[2] += emission_accum.b;
		bake_cells.write[p_idx].normal[0] += normal_accum.x;
		bake_cells.write[p_idx].normal[1] += normal_accum.y;
		bake_cells.write[p_idx].normal[2] += normal_accum.z;
		bake_cells.write[p_idx].alpha += alpha;

	} else {
		//go down

		int half = (1 << cell_subdiv) >> (p_level + 1);
		for (int i = 0; i < 8; i++) {
			AABB aabb = p_aabb;
			aabb.size *= 0.5;

			int nx = p_x;
			int ny = p_y;
			int nz = p_z;

			if (i & 1) {
				aabb.position.x += aabb.size.x;
				nx += half;
			}
			if (i & 2) {
				aabb.position.y += aabb.size.y;
				ny += half;
			}
			if (i & 4) {
				aabb.position.z += aabb.size.z;
				nz += half;
			}
			//make sure to not plot beyond limits
			if (nx < 0 || nx >= axis_cell_size[0] || ny < 0 || ny >= axis_cell_size[1] || nz < 0 || nz >= axis_cell_size[2]) {
				continue;
			}

			{
				AABB test_aabb = aabb;
				//test_aabb.grow_by(test_aabb.get_longest_axis_size()*0.05); //grow a bit to avoid numerical error in real-time
				Vector3 qsize = test_aabb.size * 0.5; //quarter size, for fast aabb test

				if (!Geometry3D::triangle_box_overlap(test_aabb.position + qsize, qsize, p_vtx)) {
					//if (!Face3(p_vtx[0],p_vtx[1],p_vtx[2]).intersects_aabb2(aabb)) {
					//does not fit in child, go on
					continue;
				}
			}

			if (bake_cells[p_idx].children[i] == CHILD_EMPTY) {
				//sub cell must be created

				uint32_t child_idx = bake_cells.size();
				bake_cells.write[p_idx].children[i] = child_idx;
				bake_cells.resize(bake_cells.size() + 1);
				bake_cells.write[child_idx].level = p_level + 1;
				bake_cells.write[child_idx].x = nx / half;
				bake_cells.write[child_idx].y = ny / half;
				bake_cells.write[child_idx].z = nz / half;
			}

			_plot_face(bake_cells[p_idx].children[i], p_level + 1, nx, ny, nz, p_vtx, p_normal, p_uv, p_material, aabb);
		}
	}
}

Vector<Color> Voxelizer::_get_bake_texture(Ref<Image> p_image, const Color &p_color_mul, const Color &p_color_add) {
	Vector<Color> ret;

	if (p_image.is_null() || p_image->is_empty()) {
		ret.resize(bake_texture_size * bake_texture_size);
		for (int i = 0; i < bake_texture_size * bake_texture_size; i++) {
			ret.write[i] = p_color_add;
		}

		return ret;
	}
	p_image = p_image->duplicate();

	if (p_image->is_compressed()) {
		p_image->decompress();
	}
	p_image->convert(Image::FORMAT_RGBA8);
	p_image->resize(bake_texture_size, bake_texture_size, Image::INTERPOLATE_CUBIC);

	const uint8_t *r = p_image->get_data().ptr();
	ret.resize(bake_texture_size * bake_texture_size);

	for (int i = 0; i < bake_texture_size * bake_texture_size; i++) {
		Color c;
		c.r = (r[i * 4 + 0] / 255.0) * p_color_mul.r + p_color_add.r;
		c.g = (r[i * 4 + 1] / 255.0) * p_color_mul.g + p_color_add.g;
		c.b = (r[i * 4 + 2] / 255.0) * p_color_mul.b + p_color_add.b;

		c.a = r[i * 4 + 3] / 255.0;

		ret.write[i] = c;
	}

	return ret;
}

Voxelizer::MaterialCache Voxelizer::_get_material_cache(Ref<Material> p_material) {
	// This way of obtaining materials is inaccurate and also does not support some compressed formats very well.
	Ref<BaseMaterial3D> mat = p_material;

	Ref<Material> material = mat; //hack for now

	if (material_cache.has(material)) {
		return material_cache[material];
	}

	MaterialCache mc;

	if (mat.is_valid()) {
		Ref<Texture2D> albedo_tex = mat->get_texture(BaseMaterial3D::TEXTURE_ALBEDO);

		Ref<Image> img_albedo;
		if (albedo_tex.is_valid()) {
			img_albedo = albedo_tex->get_image();
			mc.albedo = _get_bake_texture(img_albedo, mat->get_albedo(), Color(0, 0, 0)); // albedo texture, color is multiplicative
		} else {
			mc.albedo = _get_bake_texture(img_albedo, Color(1, 1, 1), mat->get_albedo()); // no albedo texture, color is additive
		}
		if (mat->get_feature(BaseMaterial3D::FEATURE_EMISSION)) {
			Ref<Texture2D> emission_tex = mat->get_texture(BaseMaterial3D::TEXTURE_EMISSION);

			Color emission_col = mat->get_emission();
			float emission_energy = mat->get_emission_energy_multiplier() * exposure_normalization;
			if (GLOBAL_GET("rendering/lights_and_shadows/use_physical_light_units")) {
				emission_energy *= mat->get_emission_intensity();
			}

			Ref<Image> img_emission;

			if (emission_tex.is_valid()) {
				img_emission = emission_tex->get_image();
			}

			if (mat->get_emission_operator() == BaseMaterial3D::EMISSION_OP_ADD) {
				mc.emission = _get_bake_texture(img_emission, Color(1, 1, 1) * emission_energy, emission_col * emission_energy);
			} else {
				mc.emission = _get_bake_texture(img_emission, emission_col * emission_energy, Color(0, 0, 0));
			}
		} else {
			Ref<Image> empty;
			mc.emission = _get_bake_texture(empty, Color(0, 0, 0), Color(0, 0, 0));
		}

	} else {
		Ref<Image> empty;

		mc.albedo = _get_bake_texture(empty, Color(0, 0, 0), Color(1, 1, 1));
		mc.emission = _get_bake_texture(empty, Color(0, 0, 0), Color(0, 0, 0));
	}

	material_cache[p_material] = mc;
	return mc;
}

int Voxelizer::get_bake_steps(Ref<Mesh> &p_mesh) const {
	int bake_total = 0;
	for (int i = 0; i < p_mesh->get_surface_count(); i++) {
		if (p_mesh->surface_get_primitive_type(i) != Mesh::PRIMITIVE_TRIANGLES) {
			continue; // Only triangles.
		}
		Array a = p_mesh->surface_get_arrays(i);
		Vector<Vector3> vertices = a[Mesh::ARRAY_VERTEX];
		Vector<int> index = a[Mesh::ARRAY_INDEX];
		bake_total += (index.size() > 0 ? index.size() : vertices.size()) / 3;
	}
	return bake_total;
}

Voxelizer::BakeResult Voxelizer::plot_mesh(const Transform3D &p_xform, Ref<Mesh> &p_mesh, const Vector<Ref<Material>> &p_materials, const Ref<Material> &p_override_material, BakeStepFunc p_bake_step_func) {
	ERR_FAIL_COND_V_MSG(!p_xform.is_finite(), BAKE_RESULT_INVALID_PARAMETER, "Invalid mesh bake transform.");

	int bake_total = get_bake_steps(p_mesh), bake_current = 0;

	for (int i = 0; i < p_mesh->get_surface_count(); i++) {
		if (p_mesh->surface_get_primitive_type(i) != Mesh::PRIMITIVE_TRIANGLES) {
			continue; //only triangles
		}

		Ref<Material> src_material;

		if (p_override_material.is_valid()) {
			src_material = p_override_material;
		} else if (i < p_materials.size() && p_materials[i].is_valid()) {
			src_material = p_materials[i];
		} else {
			src_material = p_mesh->surface_get_material(i);
		}
		MaterialCache material = _get_material_cache(src_material);

		Array a = p_mesh->surface_get_arrays(i);

		Vector<Vector3> vertices = a[Mesh::ARRAY_VERTEX];
		const Vector3 *vr = vertices.ptr();
		Vector<Vector2> uv = a[Mesh::ARRAY_TEX_UV];
		const Vector2 *uvr = nullptr;
		Vector<Vector3> normals = a[Mesh::ARRAY_NORMAL];
		const Vector3 *nr = nullptr;
		Vector<int> index = a[Mesh::ARRAY_INDEX];

		if (uv.size()) {
			uvr = uv.ptr();
		}

		if (normals.size()) {
			nr = normals.ptr();
		}

		if (index.size()) {
			int facecount = index.size() / 3;
			const int *ir = index.ptr();

			for (int j = 0; j < facecount; j++) {
				Vector3 vtxs[3];
				Vector2 uvs[3];
				Vector3 normal[3];

				bake_current++;
				if (p_bake_step_func != nullptr && (bake_current & 2047) == 1) {
					if (p_bake_step_func(bake_current, bake_total)) {
						return BAKE_RESULT_CANCELLED;
					}
				}

				for (int k = 0; k < 3; k++) {
					vtxs[k] = p_xform.xform(vr[ir[j * 3 + k]]);
				}

				if (uvr) {
					for (int k = 0; k < 3; k++) {
						uvs[k] = uvr[ir[j * 3 + k]];
					}
				}

				if (nr) {
					for (int k = 0; k < 3; k++) {
						normal[k] = nr[ir[j * 3 + k]];
					}
				}

				//test against original bounds
				if (!Geometry3D::triangle_box_overlap(original_bounds.get_center(), original_bounds.size * 0.5, vtxs)) {
					continue;
				}
				//plot
				_plot_face(0, 0, 0, 0, 0, vtxs, normal, uvs, material, po2_bounds);
			}

		} else {
			int facecount = vertices.size() / 3;

			for (int j = 0; j < facecount; j++) {
				Vector3 vtxs[3];
				Vector2 uvs[3];
				Vector3 normal[3];

				bake_current++;
				if (p_bake_step_func != nullptr && (bake_current & 2047) == 1) {
					if (p_bake_step_func(bake_current, bake_total)) {
						return BAKE_RESULT_CANCELLED;
					}
				}

				for (int k = 0; k < 3; k++) {
					vtxs[k] = p_xform.xform(vr[j * 3 + k]);
				}

				if (uvr) {
					for (int k = 0; k < 3; k++) {
						uvs[k] = uvr[j * 3 + k];
					}
				}

				if (nr) {
					for (int k = 0; k < 3; k++) {
						normal[k] = nr[j * 3 + k];
					}
				}

				//test against original bounds
				if (!Geometry3D::triangle_box_overlap(original_bounds.get_center(), original_bounds.size * 0.5, vtxs)) {
					continue;
				}
				//plot face
				_plot_face(0, 0, 0, 0, 0, vtxs, normal, uvs, material, po2_bounds);
			}
		}
	}

	max_original_cells = bake_cells.size();

	return BAKE_RESULT_OK;
}

void Voxelizer::_sort() {
	// cells need to be sorted by level and coordinates
	// it is important that level has more priority (for compute), and that Z has the least,
	// given it may aid older implementations plot using GPU

	Vector<CellSort> sorted_cells;
	uint32_t cell_count = bake_cells.size();
	sorted_cells.resize(cell_count);
	{
		CellSort *sort_cellsp = sorted_cells.ptrw();
		const Cell *bake_cellsp = bake_cells.ptr();

		for (uint32_t i = 0; i < cell_count; i++) {
			sort_cellsp[i].x = bake_cellsp[i].x;
			sort_cellsp[i].y = bake_cellsp[i].y;
			sort_cellsp[i].z = bake_cellsp[i].z;
			sort_cellsp[i].level = bake_cellsp[i].level;
			sort_cellsp[i].index = i;
		}
	}

	sorted_cells.sort();

	//verify just in case, index 0 must be level 0
	ERR_FAIL_COND(sorted_cells[0].level != 0);

	Vector<Cell> new_bake_cells;
	new_bake_cells.resize(cell_count);
	Vector<uint32_t> reverse_map;

	{
		reverse_map.resize(cell_count);
		const CellSort *sort_cellsp = sorted_cells.ptr();
		uint32_t *reverse_mapp = reverse_map.ptrw();

		for (uint32_t i = 0; i < cell_count; i++) {
			reverse_mapp[sort_cellsp[i].index] = i;
		}
	}

	{
		const CellSort *sort_cellsp = sorted_cells.ptr();
		const Cell *bake_cellsp = bake_cells.ptr();
		const uint32_t *reverse_mapp = reverse_map.ptr();
		Cell *new_bake_cellsp = new_bake_cells.ptrw();

		for (uint32_t i = 0; i < cell_count; i++) {
			//copy to new cell
			new_bake_cellsp[i] = bake_cellsp[sort_cellsp[i].index];
			//remap children
			for (uint32_t j = 0; j < 8; j++) {
				if (new_bake_cellsp[i].children[j] != CHILD_EMPTY) {
					new_bake_cellsp[i].children[j] = reverse_mapp[new_bake_cellsp[i].children[j]];
				}
			}
		}
	}

	bake_cells = new_bake_cells;
	sorted = true;
}

void Voxelizer::_fixup_plot(int p_idx, int p_level) {
	if (p_level == cell_subdiv) {
		leaf_voxel_count++;
		float alpha = bake_cells[p_idx].alpha;

		bake_cells.write[p_idx].albedo[0] /= alpha;
		bake_cells.write[p_idx].albedo[1] /= alpha;
		bake_cells.write[p_idx].albedo[2] /= alpha;

		//transfer emission to light
		bake_cells.write[p_idx].emission[0] /= alpha;
		bake_cells.write[p_idx].emission[1] /= alpha;
		bake_cells.write[p_idx].emission[2] /= alpha;

		bake_cells.write[p_idx].normal[0] /= alpha;
		bake_cells.write[p_idx].normal[1] /= alpha;
		bake_cells.write[p_idx].normal[2] /= alpha;

		Vector3 n(bake_cells[p_idx].normal[0], bake_cells[p_idx].normal[1], bake_cells[p_idx].normal[2]);
		if (n.length() < 0.01) {
			//too much fight over normal, zero it
			bake_cells.write[p_idx].normal[0] = 0;
			bake_cells.write[p_idx].normal[1] = 0;
			bake_cells.write[p_idx].normal[2] = 0;
		} else {
			n.normalize();
			bake_cells.write[p_idx].normal[0] = n.x;
			bake_cells.write[p_idx].normal[1] = n.y;
			bake_cells.write[p_idx].normal[2] = n.z;
		}

		bake_cells.write[p_idx].alpha = 1.0;

		/*if (bake_light.size()) {
			for(int i=0;i<6;i++) {
			}
		}*/

	} else {
		//go down

		bake_cells.write[p_idx].emission[0] = 0;
		bake_cells.write[p_idx].emission[1] = 0;
		bake_cells.write[p_idx].emission[2] = 0;
		bake_cells.write[p_idx].normal[0] = 0;
		bake_cells.write[p_idx].normal[1] = 0;
		bake_cells.write[p_idx].normal[2] = 0;
		bake_cells.write[p_idx].albedo[0] = 0;
		bake_cells.write[p_idx].albedo[1] = 0;
		bake_cells.write[p_idx].albedo[2] = 0;

		float alpha_average = 0;

		for (int i = 0; i < 8; i++) {
			uint32_t child = bake_cells[p_idx].children[i];

			if (child == CHILD_EMPTY) {
				continue;
			}

			_fixup_plot(child, p_level + 1);
			alpha_average += bake_cells[child].alpha;
		}

		bake_cells.write[p_idx].alpha = alpha_average / 8.0;
	}
}

void Voxelizer::begin_bake(int p_subdiv, const AABB &p_bounds, float p_exposure_normalization) {
	sorted = false;
	original_bounds = p_bounds;
	cell_subdiv = p_subdiv;
	exposure_normalization = p_exposure_normalization;
	bake_cells.resize(1);
	material_cache.clear();

	//find out the actual real bounds, power of 2, which gets the highest subdivision
	po2_bounds = p_bounds;
	int longest_axis = po2_bounds.get_longest_axis_index();
	axis_cell_size[longest_axis] = 1 << cell_subdiv;
	leaf_voxel_count = 0;

	for (int i = 0; i < 3; i++) {
		if (i == longest_axis) {
			continue;
		}

		axis_cell_size[i] = axis_cell_size[longest_axis];
		real_t axis_size = po2_bounds.size[longest_axis];

		//shrink until fit subdiv
		while (axis_size / 2.0 >= po2_bounds.size[i]) {
			axis_size /= 2.0;
			axis_cell_size[i] >>= 1;
		}

		po2_bounds.size[i] = po2_bounds.size[longest_axis];
	}

	Transform3D to_bounds;
	to_bounds.basis.scale(Vector3(po2_bounds.size[longest_axis], po2_bounds.size[longest_axis], po2_bounds.size[longest_axis]));
	to_bounds.origin = po2_bounds.position;

	Transform3D to_grid;
	to_grid.basis.scale(Vector3(axis_cell_size[longest_axis], axis_cell_size[longest_axis], axis_cell_size[longest_axis]));

	to_cell_space = to_grid * to_bounds.affine_inverse();

	cell_size = po2_bounds.size[longest_axis] / axis_cell_size[longest_axis];
}

void Voxelizer::end_bake() {
	if (!sorted) {
		_sort();
	}
	_fixup_plot(0, 0);
}

//create the data for rendering server

int Voxelizer::get_voxel_gi_octree_depth() const {
	return cell_subdiv;
}

Vector3i Voxelizer::get_voxel_gi_octree_size() const {
	return Vector3i(axis_cell_size[0], axis_cell_size[1], axis_cell_size[2]);
}

int Voxelizer::get_voxel_gi_cell_count() const {
	return bake_cells.size();
}

Vector<uint8_t> Voxelizer::get_voxel_gi_octree_cells() const {
	Vector<uint8_t> data;
	data.resize((8 * 4) * bake_cells.size()); //8 uint32t values
	{
		uint8_t *w = data.ptrw();
		uint32_t *children_cells = (uint32_t *)w;
		const Cell *cells = bake_cells.ptr();

		uint32_t cell_count = bake_cells.size();

		for (uint32_t i = 0; i < cell_count; i++) {
			for (uint32_t j = 0; j < 8; j++) {
				children_cells[i * 8 + j] = cells[i].children[j];
			}
		}
	}

	return data;
}

Vector<uint8_t> Voxelizer::get_voxel_gi_data_cells() const {
	Vector<uint8_t> data;
	data.resize((4 * 4) * bake_cells.size()); //8 uint32t values
	{
		uint8_t *w = data.ptrw();
		uint32_t *dataptr = (uint32_t *)w;
		const Cell *cells = bake_cells.ptr();

		uint32_t cell_count = bake_cells.size();

		for (uint32_t i = 0; i < cell_count; i++) {
			{ //position

				uint32_t x = cells[i].x;
				uint32_t y = cells[i].y;
				uint32_t z = cells[i].z;

				uint32_t position = x;
				position |= y << 11;
				position |= z << 21;

				dataptr[i * 4 + 0] = position;
			}

			{ //albedo + alpha
				uint32_t rgba = uint32_t(CLAMP(cells[i].alpha * 255.0, 0, 255)) << 24; //a
				rgba |= uint32_t(CLAMP(cells[i].albedo[2] * 255.0, 0, 255)) << 16; //b
				rgba |= uint32_t(CLAMP(cells[i].albedo[1] * 255.0, 0, 255)) << 8; //g
				rgba |= uint32_t(CLAMP(cells[i].albedo[0] * 255.0, 0, 255)); //r

				dataptr[i * 4 + 1] = rgba;
			}

			{ //emission, as rgbe9995
				Color emission = Color(cells[i].emission[0], cells[i].emission[1], cells[i].emission[2]);
				dataptr[i * 4 + 2] = emission.to_rgbe9995();
			}

			{ //normal

				Vector3 n(bake_cells[i].normal[0], bake_cells[i].normal[1], bake_cells[i].normal[2]);
				n.normalize();

				uint32_t normal = uint32_t(uint8_t(int8_t(CLAMP(n.x * 127.0, -128, 127))));
				normal |= uint32_t(uint8_t(int8_t(CLAMP(n.y * 127.0, -128, 127)))) << 8;
				normal |= uint32_t(uint8_t(int8_t(CLAMP(n.z * 127.0, -128, 127)))) << 16;

				dataptr[i * 4 + 3] = normal;
			}
		}
	}

	return data;
}

Vector<int> Voxelizer::get_voxel_gi_level_cell_count() const {
	uint32_t cell_count = bake_cells.size();
	const Cell *cells = bake_cells.ptr();
	Vector<int> level_count;
	level_count.resize(cell_subdiv + 1); //remember, always x+1 levels for x subdivisions
	{
		int *w = level_count.ptrw();
		for (int i = 0; i < cell_subdiv + 1; i++) {
			w[i] = 0;
		}

		for (uint32_t i = 0; i < cell_count; i++) {
			w[cells[i].level]++;
		}
	}

	return level_count;
}

// euclidean distance computation based on:
// https://prideout.net/blog/distance_fields/

#define square(m_s) ((m_s) * (m_s))
#define INF 1e20

/* dt of 1d function using squared distance */
static void edt(float *f, int stride, int n) {
	float *d = (float *)alloca(sizeof(float) * n + sizeof(int) * n + sizeof(float) * (n + 1));
	int *v = reinterpret_cast<int *>(&(d[n]));
	float *z = reinterpret_cast<float *>(&v[n]);

	int k = 0;
	v[0] = 0;
	z[0] = -INF;
	z[1] = +INF;
	for (int q = 1; q <= n - 1; q++) {
		float s = ((f[q * stride] + square(q)) - (f[v[k] * stride] + square(v[k]))) / (2 * q - 2 * v[k]);
		while (s <= z[k]) {
			k--;
			s = ((f[q * stride] + square(q)) - (f[v[k] * stride] + square(v[k]))) / (2 * q - 2 * v[k]);
		}
		k++;
		v[k] = q;

		z[k] = s;
		z[k + 1] = +INF;
	}

	k = 0;
	for (int q = 0; q <= n - 1; q++) {
		while (z[k + 1] < q) {
			k++;
		}
		d[q] = square(q - v[k]) + f[v[k] * stride];
	}

	for (int i = 0; i < n; i++) {
		f[i * stride] = d[i];
	}
}

#undef square

Voxelizer::BakeResult Voxelizer::get_sdf_3d_image(Vector<uint8_t> &r_image, BakeStepFunc p_bake_step_function) const {
	Vector3i octree_size = get_voxel_gi_octree_size();

	uint32_t float_count = octree_size.x * octree_size.y * octree_size.z;
	float *work_memory = memnew_arr(float, float_count);
	for (uint32_t i = 0; i < float_count; i++) {
		work_memory[i] = INF;
	}

	uint32_t y_mult = octree_size.x;
	uint32_t z_mult = y_mult * octree_size.y;

	//plot solid cells
	{
		const Cell *cells = bake_cells.ptr();
		uint32_t cell_count = bake_cells.size();

		for (uint32_t i = 0; i < cell_count; i++) {
			if (cells[i].level < (cell_subdiv - 1)) {
				continue; //do not care about this level
			}

			work_memory[cells[i].x + cells[i].y * y_mult + cells[i].z * z_mult] = 0;
		}
	}

	//process in each direction

	int bake_total = octree_size.x * 2 + octree_size.y, bake_current = 0;

	//xy->z

	for (int i = 0; i < octree_size.x; i++, bake_current++) {
		if (p_bake_step_function) {
			if (p_bake_step_function(bake_current, bake_total)) {
				memdelete_arr(work_memory);
				return BAKE_RESULT_CANCELLED;
			}
		}
		for (int j = 0; j < octree_size.y; j++) {
			edt(&work_memory[i + j * y_mult], z_mult, octree_size.z);
		}
	}

	//xz->y

	for (int i = 0; i < octree_size.x; i++, bake_current++) {
		if (p_bake_step_function) {
			if (p_bake_step_function(bake_current, bake_total)) {
				memdelete_arr(work_memory);
				return BAKE_RESULT_CANCELLED;
			}
		}
		for (int j = 0; j < octree_size.z; j++) {
			edt(&work_memory[i + j * z_mult], y_mult, octree_size.y);
		}
	}

	//yz->x
	for (int i = 0; i < octree_size.y; i++, bake_current++) {
		if (p_bake_step_function) {
			if (p_bake_step_function(bake_current, bake_total)) {
				memdelete_arr(work_memory);
				return BAKE_RESULT_CANCELLED;
			}
		}
		for (int j = 0; j < octree_size.z; j++) {
			edt(&work_memory[i * y_mult + j * z_mult], 1, octree_size.x);
		}
	}

	r_image.resize(float_count);
	{
		uint8_t *w = r_image.ptrw();
		for (uint32_t i = 0; i < float_count; i++) {
			uint32_t d = uint32_t(Math::sqrt(work_memory[i]));
			if (d == 0) {
				w[i] = 0;
			} else {
				w[i] = MIN(d, 254u) + 1;
			}
		}
	}

	memdelete_arr(work_memory);

	return BAKE_RESULT_OK;
}

#undef INF

void Voxelizer::_debug_mesh(int p_idx, int p_level, const AABB &p_aabb, Ref<MultiMesh> &p_multimesh, int &idx) {
	if (p_level == cell_subdiv - 1) {
		Vector3 center = p_aabb.get_center();
		Transform3D xform;
		xform.origin = center;
		xform.basis.scale(p_aabb.size * 0.5);
		p_multimesh->set_instance_transform(idx, xform);
		Color col;
		col = Color(bake_cells[p_idx].albedo[0], bake_cells[p_idx].albedo[1], bake_cells[p_idx].albedo[2]);
		//Color col = Color(bake_cells[p_idx].emission[0], bake_cells[p_idx].emission[1], bake_cells[p_idx].emission[2]);
		p_multimesh->set_instance_color(idx, col);

		idx++;

	} else {
		for (int i = 0; i < 8; i++) {
			uint32_t child = bake_cells[p_idx].children[i];

			if (child == CHILD_EMPTY || child >= (uint32_t)max_original_cells) {
				continue;
			}

			AABB aabb = p_aabb;
			aabb.size *= 0.5;

			if (i & 1) {
				aabb.position.x += aabb.size.x;
			}
			if (i & 2) {
				aabb.position.y += aabb.size.y;
			}
			if (i & 4) {
				aabb.position.z += aabb.size.z;
			}

			_debug_mesh(bake_cells[p_idx].children[i], p_level + 1, aabb, p_multimesh, idx);
		}
	}
}

Ref<MultiMesh> Voxelizer::create_debug_multimesh() {
	Ref<MultiMesh> mm;

	mm.instantiate();

	mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	mm->set_use_colors(true);
	mm->set_instance_count(leaf_voxel_count);

	Ref<ArrayMesh> mesh;
	mesh.instantiate();

	{
		Array arr;
		arr.resize(Mesh::ARRAY_MAX);

		Vector<Vector3> vertices;
		Vector<Color> colors;
#define ADD_VTX(m_idx)                      \
	vertices.push_back(face_points[m_idx]); \
	colors.push_back(Color(1, 1, 1, 1));

		for (int i = 0; i < 6; i++) {
			Vector3 face_points[4];

			for (int j = 0; j < 4; j++) {
				real_t v[3];
				v[0] = 1.0;
				v[1] = 1 - 2 * ((j >> 1) & 1);
				v[2] = v[1] * (1 - 2 * (j & 1));

				for (int k = 0; k < 3; k++) {
					if (i < 3) {
						face_points[j][(i + k) % 3] = v[k];
					} else {
						face_points[3 - j][(i + k) % 3] = -v[k];
					}
				}
			}

			//tri 1
			ADD_VTX(0);
			ADD_VTX(1);
			ADD_VTX(2);
			//tri 2
			ADD_VTX(2);
			ADD_VTX(3);
			ADD_VTX(0);
		}

		arr[Mesh::ARRAY_VERTEX] = vertices;
		arr[Mesh::ARRAY_COLOR] = colors;
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arr);
	}

	{
		Ref<StandardMaterial3D> fsm;
		fsm.instantiate();
		fsm->set_flag(StandardMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
		fsm->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		fsm->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
		fsm->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
		fsm->set_albedo(Color(1, 1, 1, 1));

		mesh->surface_set_material(0, fsm);
	}

	mm->set_mesh(mesh);

	int idx = 0;
	_debug_mesh(0, 0, po2_bounds, mm, idx);

	return mm;
}

Transform3D Voxelizer::get_to_cell_space_xform() const {
	return to_cell_space;
}

Voxelizer::Voxelizer() {
}
