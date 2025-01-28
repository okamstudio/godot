/**************************************************************************/
/*  voxel_gi.cpp                                                          */
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

#include "voxel_gi.h"

#include "core/config/project_settings.h"
#include "mesh_instance_3d.h"
#include "multimesh_instance_3d.h"
#include "scene/resources/camera_attributes.h"
#include "voxelizer.h"

void VoxelGIData::_set_data(const Dictionary &p_data) {
	ERR_FAIL_COND(!p_data.has("bounds"));
	ERR_FAIL_COND(!p_data.has("octree_size"));
	ERR_FAIL_COND(!p_data.has("octree_cells"));
	ERR_FAIL_COND(!p_data.has("octree_data"));
	ERR_FAIL_COND(!p_data.has("octree_df") && !p_data.has("octree_df_png"));
	ERR_FAIL_COND(!p_data.has("level_counts"));
	ERR_FAIL_COND(!p_data.has("to_cell_xform"));

	AABB bounds_new = p_data["bounds"];
	Vector3 octree_size_new = p_data["octree_size"];
	Vector<uint8_t> octree_cells = p_data["octree_cells"];
	Vector<uint8_t> octree_data = p_data["octree_data"];

	Vector<uint8_t> octree_df;
	if (p_data.has("octree_df")) {
		octree_df = p_data["octree_df"];
	} else if (p_data.has("octree_df_png")) {
		Vector<uint8_t> octree_df_png = p_data["octree_df_png"];
		Ref<Image> img;
		img.instantiate();
		Error err = img->load_png_from_buffer(octree_df_png);
		ERR_FAIL_COND(err != OK);
		ERR_FAIL_COND(img->get_format() != Image::FORMAT_L8);
		octree_df = img->get_data();
	}
	Vector<int> octree_levels = p_data["level_counts"];
	Transform3D to_cell_xform_new = p_data["to_cell_xform"];

	allocate(to_cell_xform_new, bounds_new, octree_size_new, octree_cells, octree_data, octree_df, octree_levels);
}

Dictionary VoxelGIData::_get_data() const {
	Dictionary d;
	d["bounds"] = get_bounds();
	Vector3i otsize = get_octree_size();
	d["octree_size"] = Vector3(otsize);
	d["octree_cells"] = get_octree_cells();
	d["octree_data"] = get_data_cells();
	if (otsize != Vector3i()) {
		Ref<Image> img = Image::create_from_data(otsize.x * otsize.y, otsize.z, false, Image::FORMAT_L8, get_distance_field());
		Vector<uint8_t> df_png = img->save_png_to_buffer();
		ERR_FAIL_COND_V(df_png.is_empty(), Dictionary());
		d["octree_df_png"] = df_png;
	} else {
		d["octree_df"] = Vector<uint8_t>();
	}

	d["level_counts"] = get_level_counts();
	d["to_cell_xform"] = get_to_cell_xform();
	return d;
}

void VoxelGIData::allocate(const Transform3D &p_to_cell_xform, const AABB &p_aabb, const Vector3 &p_octree_size, const Vector<uint8_t> &p_octree_cells, const Vector<uint8_t> &p_data_cells, const Vector<uint8_t> &p_distance_field, const Vector<int> &p_level_counts) {
	RS::get_singleton()->voxel_gi_allocate_data(probe, p_to_cell_xform, p_aabb, p_octree_size, p_octree_cells, p_data_cells, p_distance_field, p_level_counts);
	bounds = p_aabb;
	to_cell_xform = p_to_cell_xform;
	octree_size = p_octree_size;
}

AABB VoxelGIData::get_bounds() const {
	return bounds;
}

Vector3 VoxelGIData::get_octree_size() const {
	return octree_size;
}

Vector<uint8_t> VoxelGIData::get_octree_cells() const {
	return RS::get_singleton()->voxel_gi_get_octree_cells(probe);
}

Vector<uint8_t> VoxelGIData::get_data_cells() const {
	return RS::get_singleton()->voxel_gi_get_data_cells(probe);
}

Vector<uint8_t> VoxelGIData::get_distance_field() const {
	return RS::get_singleton()->voxel_gi_get_distance_field(probe);
}

Vector<int> VoxelGIData::get_level_counts() const {
	return RS::get_singleton()->voxel_gi_get_level_counts(probe);
}

Transform3D VoxelGIData::get_to_cell_xform() const {
	return to_cell_xform;
}

void VoxelGIData::set_dynamic_range(float p_range) {
	RS::get_singleton()->voxel_gi_set_dynamic_range(probe, p_range);
	dynamic_range = p_range;
}

float VoxelGIData::get_dynamic_range() const {
	return dynamic_range;
}

void VoxelGIData::set_propagation(float p_propagation) {
	RS::get_singleton()->voxel_gi_set_propagation(probe, p_propagation);
	propagation = p_propagation;
}

float VoxelGIData::get_propagation() const {
	return propagation;
}

void VoxelGIData::set_energy(float p_energy) {
	RS::get_singleton()->voxel_gi_set_energy(probe, p_energy);
	energy = p_energy;
}

float VoxelGIData::get_energy() const {
	return energy;
}

void VoxelGIData::set_bias(float p_bias) {
	RS::get_singleton()->voxel_gi_set_bias(probe, p_bias);
	bias = p_bias;
}

float VoxelGIData::get_bias() const {
	return bias;
}

void VoxelGIData::set_normal_bias(float p_normal_bias) {
	RS::get_singleton()->voxel_gi_set_normal_bias(probe, p_normal_bias);
	normal_bias = p_normal_bias;
}

float VoxelGIData::get_normal_bias() const {
	return normal_bias;
}

void VoxelGIData::set_interior(bool p_enable) {
	RS::get_singleton()->voxel_gi_set_interior(probe, p_enable);
	interior = p_enable;
}

bool VoxelGIData::is_interior() const {
	return interior;
}

void VoxelGIData::set_use_two_bounces(bool p_enable) {
	RS::get_singleton()->voxel_gi_set_use_two_bounces(probe, p_enable);
	use_two_bounces = p_enable;
}

bool VoxelGIData::is_using_two_bounces() const {
	return use_two_bounces;
}

RID VoxelGIData::get_rid() const {
	return probe;
}

void VoxelGIData::_bind_methods() {
	ClassDB::bind_method(D_METHOD("allocate", "to_cell_xform", "aabb", "octree_size", "octree_cells", "data_cells", "distance_field", "level_counts"), &VoxelGIData::allocate);

	ClassDB::bind_method(D_METHOD("get_bounds"), &VoxelGIData::get_bounds);
	ClassDB::bind_method(D_METHOD("get_octree_size"), &VoxelGIData::get_octree_size);
	ClassDB::bind_method(D_METHOD("get_to_cell_xform"), &VoxelGIData::get_to_cell_xform);
	ClassDB::bind_method(D_METHOD("get_octree_cells"), &VoxelGIData::get_octree_cells);
	ClassDB::bind_method(D_METHOD("get_data_cells"), &VoxelGIData::get_data_cells);
	ClassDB::bind_method(D_METHOD("get_level_counts"), &VoxelGIData::get_level_counts);

	ClassDB::bind_method(D_METHOD("set_dynamic_range", "dynamic_range"), &VoxelGIData::set_dynamic_range);
	ClassDB::bind_method(D_METHOD("get_dynamic_range"), &VoxelGIData::get_dynamic_range);

	ClassDB::bind_method(D_METHOD("set_energy", "energy"), &VoxelGIData::set_energy);
	ClassDB::bind_method(D_METHOD("get_energy"), &VoxelGIData::get_energy);

	ClassDB::bind_method(D_METHOD("set_bias", "bias"), &VoxelGIData::set_bias);
	ClassDB::bind_method(D_METHOD("get_bias"), &VoxelGIData::get_bias);

	ClassDB::bind_method(D_METHOD("set_normal_bias", "bias"), &VoxelGIData::set_normal_bias);
	ClassDB::bind_method(D_METHOD("get_normal_bias"), &VoxelGIData::get_normal_bias);

	ClassDB::bind_method(D_METHOD("set_propagation", "propagation"), &VoxelGIData::set_propagation);
	ClassDB::bind_method(D_METHOD("get_propagation"), &VoxelGIData::get_propagation);

	ClassDB::bind_method(D_METHOD("set_interior", "interior"), &VoxelGIData::set_interior);
	ClassDB::bind_method(D_METHOD("is_interior"), &VoxelGIData::is_interior);

	ClassDB::bind_method(D_METHOD("set_use_two_bounces", "enable"), &VoxelGIData::set_use_two_bounces);
	ClassDB::bind_method(D_METHOD("is_using_two_bounces"), &VoxelGIData::is_using_two_bounces);

	ClassDB::bind_method(D_METHOD("_set_data", "data"), &VoxelGIData::_set_data);
	ClassDB::bind_method(D_METHOD("_get_data"), &VoxelGIData::_get_data);

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "_data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_data", "_get_data");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "dynamic_range", PROPERTY_HINT_RANGE, "1,8,0.01"), "set_dynamic_range", "get_dynamic_range");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "energy", PROPERTY_HINT_RANGE, "0,64,0.01"), "set_energy", "get_energy");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bias", PROPERTY_HINT_RANGE, "0,8,0.01"), "set_bias", "get_bias");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "normal_bias", PROPERTY_HINT_RANGE, "0,8,0.01"), "set_normal_bias", "get_normal_bias");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "propagation", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_propagation", "get_propagation");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_two_bounces"), "set_use_two_bounces", "is_using_two_bounces");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "interior"), "set_interior", "is_interior");
}

#ifndef DISABLE_DEPRECATED
bool VoxelGI::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "extents") { // Compatibility with Godot 3.x.
		set_size((Vector3)p_value * 2);
		return true;
	}
	return false;
}

bool VoxelGI::_get(const StringName &p_name, Variant &r_property) const {
	if (p_name == "extents") { // Compatibility with Godot 3.x.
		r_property = size / 2;
		return true;
	}
	return false;
}
#endif // DISABLE_DEPRECATED

VoxelGIData::VoxelGIData() {
	probe = RS::get_singleton()->voxel_gi_create();
}

VoxelGIData::~VoxelGIData() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RS::get_singleton()->free(probe);
}

//////////////////////
//////////////////////

void VoxelGI::set_probe_data(const Ref<VoxelGIData> &p_data) {
	if (p_data.is_valid()) {
		RS::get_singleton()->instance_set_base(get_instance(), p_data->get_rid());
		RS::get_singleton()->voxel_gi_set_baked_exposure_normalization(p_data->get_rid(), _get_camera_exposure_normalization());
	} else {
		RS::get_singleton()->instance_set_base(get_instance(), RID());
	}

	probe_data = p_data;
}

Ref<VoxelGIData> VoxelGI::get_probe_data() const {
	return probe_data;
}

void VoxelGI::set_subdiv(Subdiv p_subdiv) {
	ERR_FAIL_INDEX(p_subdiv, SUBDIV_MAX);
	subdiv = p_subdiv;
	update_gizmos();
}

VoxelGI::Subdiv VoxelGI::get_subdiv() const {
	return subdiv;
}

void VoxelGI::set_size(const Vector3 &p_size) {
	// Prevent very small size dimensions as these breaks baking if other size dimensions are set very high.
	size = p_size.maxf(1.0);
	update_gizmos();
}

Vector3 VoxelGI::get_size() const {
	return size;
}

void VoxelGI::set_camera_attributes(const Ref<CameraAttributes> &p_camera_attributes) {
	camera_attributes = p_camera_attributes;

	if (probe_data.is_valid()) {
		RS::get_singleton()->voxel_gi_set_baked_exposure_normalization(probe_data->get_rid(), _get_camera_exposure_normalization());
	}
}

Ref<CameraAttributes> VoxelGI::get_camera_attributes() const {
	return camera_attributes;
}

static bool is_node_voxel_bakeable(Node3D *p_node) {
	if (!p_node->is_visible_in_tree()) {
		return false;
	}

	GeometryInstance3D *geometry = Object::cast_to<GeometryInstance3D>(p_node);
	if (geometry != nullptr && geometry->get_gi_mode() != GeometryInstance3D::GI_MODE_STATIC) {
		return false;
	}
	return true;
}

void VoxelGI::_find_meshes(Node *p_at_node, List<PlotMesh> &plot_meshes) {
	MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(p_at_node);
	if (mi && is_node_voxel_bakeable(mi)) {
		Ref<Mesh> mesh = mi->get_mesh();
		if (mesh.is_valid()) {
			AABB aabb = mesh->get_aabb();

			Transform3D xf = get_global_transform().affine_inverse() * mi->get_global_transform();

			if (AABB(-size / 2, size).intersects(xf.xform(aabb))) {
				PlotMesh pm;
				pm.local_xform = xf;
				pm.mesh = mesh;
				for (int i = 0; i < mesh->get_surface_count(); i++) {
					pm.instance_materials.push_back(mi->get_surface_override_material(i));
				}
				pm.override_material = mi->get_material_override();
				plot_meshes.push_back(pm);
			}
		}
	}

	Node3D *s = Object::cast_to<Node3D>(p_at_node);
	if (s) {
		if (is_node_voxel_bakeable(s)) {
			Array meshes;
			MultiMeshInstance3D *multi_mesh = Object::cast_to<MultiMeshInstance3D>(p_at_node);
			if (multi_mesh) {
				meshes = multi_mesh->get_meshes();
			} else {
				meshes = p_at_node->call("get_meshes");
			}

			for (int i = 0; i < meshes.size(); i += 2) {
				Transform3D mxf = meshes[i];
				Ref<Mesh> mesh = meshes[i + 1];
				if (mesh.is_null()) {
					continue;
				}

				AABB aabb = mesh->get_aabb();

				Transform3D xf = get_global_transform().affine_inverse() * (s->get_global_transform() * mxf);

				if (AABB(-size / 2, size).intersects(xf.xform(aabb))) {
					PlotMesh pm;
					pm.local_xform = xf;
					pm.mesh = mesh;
					plot_meshes.push_back(pm);
				}
			}
		}
	}

	for (int i = 0; i < p_at_node->get_child_count(); i++) {
		Node *child = p_at_node->get_child(i);
		_find_meshes(child, plot_meshes);
	}
}

VoxelGI::BakeBeginFunc VoxelGI::bake_begin_function = nullptr;
VoxelGI::BakeStepFunc VoxelGI::bake_step_function = nullptr;
VoxelGI::BakeEndFunc VoxelGI::bake_end_function = nullptr;

static int voxelizer_plot_bake_base = 0;
static int voxelizer_plot_bake_total = 0;

static bool voxelizer_plot_bake_step_function(int current, int) {
	return VoxelGI::bake_step_function((voxelizer_plot_bake_base + current) * 500 / voxelizer_plot_bake_total, RTR("Plotting Meshes"));
}

static bool voxelizer_sdf_bake_step_function(int current, int total) {
	return VoxelGI::bake_step_function(500 + current * 500 / total, RTR("Generating Distance Field"));
}

Vector3i VoxelGI::get_estimated_cell_size() const {
	static const int subdiv_value[SUBDIV_MAX] = { 6, 7, 8, 9 };
	int cell_subdiv = subdiv_value[subdiv];
	int axis_cell_size[3];
	AABB bounds = AABB(-size / 2, size);
	int longest_axis = bounds.get_longest_axis_index();
	axis_cell_size[longest_axis] = 1 << cell_subdiv;

	for (int i = 0; i < 3; i++) {
		if (i == longest_axis) {
			continue;
		}

		axis_cell_size[i] = axis_cell_size[longest_axis];
		float axis_size = bounds.size[longest_axis];

		//shrink until fit subdiv
		while (axis_size / 2.0 >= bounds.size[i]) {
			axis_size /= 2.0;
			axis_cell_size[i] >>= 1;
		}
	}

	return Vector3i(axis_cell_size[0], axis_cell_size[1], axis_cell_size[2]);
}

void VoxelGI::bake(Node *p_from_node, bool p_create_visual_debug) {
	static const int subdiv_value[SUBDIV_MAX] = { 6, 7, 8, 9 };

	p_from_node = p_from_node ? p_from_node : get_parent();
	ERR_FAIL_NULL(p_from_node);

	float exposure_normalization = _get_camera_exposure_normalization();

	Voxelizer baker;

	baker.begin_bake(subdiv_value[subdiv], AABB(-size / 2, size), exposure_normalization);

	List<PlotMesh> mesh_list;

	_find_meshes(p_from_node, mesh_list);

	if (bake_begin_function) {
		bake_begin_function();
	}

	Voxelizer::BakeStepFunc voxelizer_step_func = bake_step_function != nullptr ? voxelizer_plot_bake_step_function : nullptr;

	voxelizer_plot_bake_total = voxelizer_plot_bake_base = 0;
	for (PlotMesh &E : mesh_list) {
		voxelizer_plot_bake_total += baker.get_bake_steps(E.mesh);
	}
	for (PlotMesh &E : mesh_list) {
		if (baker.plot_mesh(E.local_xform, E.mesh, E.instance_materials, E.override_material, voxelizer_step_func) != Voxelizer::BAKE_RESULT_OK) {
			baker.end_bake();
			if (bake_end_function) {
				bake_end_function();
			}
			return;
		}
		voxelizer_plot_bake_base += baker.get_bake_steps(E.mesh);
	}
	if (bake_step_function) {
		bake_step_function(500, RTR("Finishing Plot"));
	}

	baker.end_bake();

	//create the data for rendering server

	if (p_create_visual_debug) {
		MultiMeshInstance3D *mmi = memnew(MultiMeshInstance3D);
		mmi->set_multimesh(baker.create_debug_multimesh());
		add_child(mmi, true);
#ifdef TOOLS_ENABLED
		if (is_inside_tree() && get_tree()->get_edited_scene_root() == this) {
			mmi->set_owner(this);
		} else {
			mmi->set_owner(get_owner());
		}
#else
		mmi->set_owner(get_owner());
#endif

	} else {
		Ref<VoxelGIData> probe_data_new = get_probe_data();

		if (probe_data_new.is_null()) {
			probe_data_new.instantiate();
		}

		if (bake_step_function) {
			bake_step_function(500, RTR("Generating Distance Field"));
		}

		voxelizer_step_func = bake_step_function != nullptr ? voxelizer_sdf_bake_step_function : nullptr;

		Vector<uint8_t> df;
		if (baker.get_sdf_3d_image(df, voxelizer_step_func) == Voxelizer::BAKE_RESULT_OK) {
			RS::get_singleton()->voxel_gi_set_baked_exposure_normalization(probe_data_new->get_rid(), exposure_normalization);

			probe_data_new->allocate(baker.get_to_cell_space_xform(), AABB(-size / 2, size), baker.get_voxel_gi_octree_size(), baker.get_voxel_gi_octree_cells(), baker.get_voxel_gi_data_cells(), df, baker.get_voxel_gi_level_cell_count());

			set_probe_data(probe_data_new);
#ifdef TOOLS_ENABLED
			probe_data_new->set_edited(true); //so it gets saved
#endif
		}
	}

	if (bake_end_function) {
		bake_end_function();
	}

	notify_property_list_changed(); //bake property may have changed
}

void VoxelGI::_debug_bake() {
	bake(nullptr, true);
}

float VoxelGI::_get_camera_exposure_normalization() {
	float exposure_normalization = 1.0;
	if (camera_attributes.is_valid()) {
		exposure_normalization = camera_attributes->get_exposure_multiplier();
		if (GLOBAL_GET("rendering/lights_and_shadows/use_physical_light_units")) {
			exposure_normalization = camera_attributes->calculate_exposure_normalization();
		}
	}
	return exposure_normalization;
}

AABB VoxelGI::get_aabb() const {
	return AABB(-size / 2, size);
}

PackedStringArray VoxelGI::get_configuration_warnings() const {
	PackedStringArray warnings = VisualInstance3D::get_configuration_warnings();

	if (OS::get_singleton()->get_current_rendering_method() == "gl_compatibility") {
		warnings.push_back(RTR("VoxelGI nodes are not supported when using the Compatibility renderer yet. Support will be added in a future release."));
	} else if (probe_data.is_null()) {
		warnings.push_back(RTR("No VoxelGI data set, so this node is disabled. Bake static objects to enable GI."));
	}
	return warnings;
}

void VoxelGI::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_probe_data", "data"), &VoxelGI::set_probe_data);
	ClassDB::bind_method(D_METHOD("get_probe_data"), &VoxelGI::get_probe_data);

	ClassDB::bind_method(D_METHOD("set_subdiv", "subdiv"), &VoxelGI::set_subdiv);
	ClassDB::bind_method(D_METHOD("get_subdiv"), &VoxelGI::get_subdiv);

	ClassDB::bind_method(D_METHOD("set_size", "size"), &VoxelGI::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &VoxelGI::get_size);

	ClassDB::bind_method(D_METHOD("set_camera_attributes", "camera_attributes"), &VoxelGI::set_camera_attributes);
	ClassDB::bind_method(D_METHOD("get_camera_attributes"), &VoxelGI::get_camera_attributes);

	ClassDB::bind_method(D_METHOD("bake", "from_node", "create_visual_debug"), &VoxelGI::bake, DEFVAL(Variant()), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("debug_bake"), &VoxelGI::_debug_bake);
	ClassDB::set_method_flags(get_class_static(), _scs_create("debug_bake"), METHOD_FLAGS_DEFAULT | METHOD_FLAG_EDITOR);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdiv", PROPERTY_HINT_ENUM, "64,128,256,512"), "set_subdiv", "get_subdiv");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "size", PROPERTY_HINT_NONE, "suffix:m"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "camera_attributes", PROPERTY_HINT_RESOURCE_TYPE, "CameraAttributesPractical,CameraAttributesPhysical"), "set_camera_attributes", "get_camera_attributes");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "data", PROPERTY_HINT_RESOURCE_TYPE, "VoxelGIData", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_ALWAYS_DUPLICATE), "set_probe_data", "get_probe_data");

	BIND_ENUM_CONSTANT(SUBDIV_64);
	BIND_ENUM_CONSTANT(SUBDIV_128);
	BIND_ENUM_CONSTANT(SUBDIV_256);
	BIND_ENUM_CONSTANT(SUBDIV_512);
	BIND_ENUM_CONSTANT(SUBDIV_MAX);
}

VoxelGI::VoxelGI() {
	voxel_gi = RS::get_singleton()->voxel_gi_create();
	set_disable_scale(true);
}

VoxelGI::~VoxelGI() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RS::get_singleton()->free(voxel_gi);
}
