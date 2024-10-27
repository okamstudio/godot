/**************************************************************************/
/*  shader_types.cpp                                                      */
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

#include "shader_types.h"
#include "core/math/math_defs.h"

const HashMap<StringName, ShaderLanguage::FunctionInfo> &ShaderTypes::get_functions(RS::ShaderMode p_mode) const {
	return shader_modes[p_mode].functions;
}

const Vector<ShaderLanguage::ModeInfo> &ShaderTypes::get_modes(RS::ShaderMode p_mode) const {
	return shader_modes[p_mode].modes;
}

const HashSet<String> &ShaderTypes::get_types() const {
	return shader_types;
}

const List<String> &ShaderTypes::get_types_list() const {
	return shader_types_list;
}

ShaderTypes *ShaderTypes::singleton = nullptr;

static ShaderLanguage::BuiltInInfo constt(ShaderLanguage::DataType p_type) {
	return ShaderLanguage::BuiltInInfo(p_type, true);
}

ShaderTypes::ShaderTypes() {
	singleton = this;

	/*************** SPATIAL ***********************/

	shader_modes[RS::SHADER_SPATIAL].functions["global"].built_ins["TIME"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_SPATIAL].functions["global"].built_ins["EXPOSURE"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_SPATIAL].functions["constants"].built_ins["PI"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_SPATIAL].functions["constants"].built_ins["TAU"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_SPATIAL].functions["constants"].built_ins["E"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_SPATIAL].functions["constants"].built_ins["OUTPUT_IS_SRGB"] = constt(ShaderLanguage::TYPE_BOOL);
	shader_modes[RS::SHADER_SPATIAL].functions["constants"].built_ins["CLIP_SPACE_FAR"] = constt(ShaderLanguage::TYPE_FLOAT);

	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["VERTEX"] = ShaderLanguage::TYPE_VEC3;
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["NORMAL"] = ShaderLanguage::TYPE_VEC3;
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["TANGENT"] = ShaderLanguage::TYPE_VEC3;
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["BINORMAL"] = ShaderLanguage::TYPE_VEC3;
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["POSITION"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["UV"] = ShaderLanguage::TYPE_VEC2;
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["UV2"] = ShaderLanguage::TYPE_VEC2;
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["COLOR"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["POINT_SIZE"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["INSTANCE_ID"] = constt(ShaderLanguage::TYPE_INT);
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["INSTANCE_CUSTOM"] = constt(ShaderLanguage::TYPE_VEC4);
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["VERTEX_ID"] = constt(ShaderLanguage::TYPE_INT);
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["ROUGHNESS"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["BONE_INDICES"] = constt(ShaderLanguage::TYPE_UVEC4);
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["BONE_WEIGHTS"] = constt(ShaderLanguage::TYPE_VEC4);
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["CUSTOM0"] = constt(ShaderLanguage::TYPE_VEC4);
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["CUSTOM1"] = constt(ShaderLanguage::TYPE_VEC4);
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["CUSTOM2"] = constt(ShaderLanguage::TYPE_VEC4);
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["CUSTOM3"] = constt(ShaderLanguage::TYPE_VEC4);
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].can_discard = false;
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].main_function = true;

	//builtins
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["MODEL_MATRIX"] = constt(ShaderLanguage::TYPE_MAT4);
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["MODEL_NORMAL_MATRIX"] = constt(ShaderLanguage::TYPE_MAT3);
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["VIEW_MATRIX"] = constt(ShaderLanguage::TYPE_MAT4);
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["INV_VIEW_MATRIX"] = constt(ShaderLanguage::TYPE_MAT4);
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["PROJECTION_MATRIX"] = ShaderLanguage::TYPE_MAT4;
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["INV_PROJECTION_MATRIX"] = constt(ShaderLanguage::TYPE_MAT4);
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["MODELVIEW_MATRIX"] = ShaderLanguage::TYPE_MAT4;
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["MODELVIEW_NORMAL_MATRIX"] = ShaderLanguage::TYPE_MAT3;
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["VIEWPORT_SIZE"] = constt(ShaderLanguage::TYPE_VEC2);
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["MAIN_CAM_INV_VIEW_MATRIX"] = constt(ShaderLanguage::TYPE_MAT4);

	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["NODE_POSITION_WORLD"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["CAMERA_POSITION_WORLD"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["CAMERA_DIRECTION_WORLD"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["CAMERA_VISIBLE_LAYERS"] = constt(ShaderLanguage::TYPE_UINT);
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["NODE_POSITION_VIEW"] = constt(ShaderLanguage::TYPE_VEC3);

	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["VIEW_INDEX"] = constt(ShaderLanguage::TYPE_INT);
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["VIEW_MONO_LEFT"] = constt(ShaderLanguage::TYPE_INT);
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["VIEW_RIGHT"] = constt(ShaderLanguage::TYPE_INT);
	shader_modes[RS::SHADER_SPATIAL].functions["vertex"].built_ins["EYE_OFFSET"] = constt(ShaderLanguage::TYPE_VEC3);

	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["VERTEX"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["LIGHT_VERTEX"] = ShaderLanguage::TYPE_VEC3;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["FRAGCOORD"] = constt(ShaderLanguage::TYPE_VEC4);
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["FRONT_FACING"] = constt(ShaderLanguage::TYPE_BOOL);
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["NORMAL"] = ShaderLanguage::TYPE_VEC3;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["TANGENT"] = ShaderLanguage::TYPE_VEC3;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["BINORMAL"] = ShaderLanguage::TYPE_VEC3;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["VIEW"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["NORMAL_MAP"] = ShaderLanguage::TYPE_VEC3;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["NORMAL_MAP_DEPTH"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["UV"] = constt(ShaderLanguage::TYPE_VEC2);
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["UV2"] = constt(ShaderLanguage::TYPE_VEC2);
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["COLOR"] = constt(ShaderLanguage::TYPE_VEC4);
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["ALBEDO"] = ShaderLanguage::TYPE_VEC3;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["PREMUL_ALPHA_FACTOR"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["ALPHA"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["METALLIC"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["SPECULAR"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["ROUGHNESS"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["RIM"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["RIM_TINT"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["CLEARCOAT"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["CLEARCOAT_ROUGHNESS"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["ANISOTROPY"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["ANISOTROPY_FLOW"] = ShaderLanguage::TYPE_VEC2;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["SSS_STRENGTH"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["SSS_TRANSMITTANCE_COLOR"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["SSS_TRANSMITTANCE_DEPTH"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["SSS_TRANSMITTANCE_BOOST"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["BACKLIGHT"] = ShaderLanguage::TYPE_VEC3;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["AO"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["AO_LIGHT_AFFECT"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["EMISSION"] = ShaderLanguage::TYPE_VEC3;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["DEPTH"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["SCREEN_UV"] = constt(ShaderLanguage::TYPE_VEC2);
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["POINT_COORD"] = constt(ShaderLanguage::TYPE_VEC2);

	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["NODE_POSITION_WORLD"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["CAMERA_POSITION_WORLD"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["CAMERA_DIRECTION_WORLD"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["CAMERA_VISIBLE_LAYERS"] = constt(ShaderLanguage::TYPE_UINT);
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["NODE_POSITION_VIEW"] = constt(ShaderLanguage::TYPE_VEC3);

	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["VIEW_INDEX"] = constt(ShaderLanguage::TYPE_INT);
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["VIEW_MONO_LEFT"] = constt(ShaderLanguage::TYPE_INT);
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["VIEW_RIGHT"] = constt(ShaderLanguage::TYPE_INT);
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["EYE_OFFSET"] = constt(ShaderLanguage::TYPE_VEC3);

	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["MODEL_MATRIX"] = constt(ShaderLanguage::TYPE_MAT4);
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["MODEL_NORMAL_MATRIX"] = constt(ShaderLanguage::TYPE_MAT3);
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["VIEW_MATRIX"] = constt(ShaderLanguage::TYPE_MAT4);
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["INV_VIEW_MATRIX"] = constt(ShaderLanguage::TYPE_MAT4);
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["PROJECTION_MATRIX"] = constt(ShaderLanguage::TYPE_MAT4);
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["INV_PROJECTION_MATRIX"] = constt(ShaderLanguage::TYPE_MAT4);
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["VIEWPORT_SIZE"] = constt(ShaderLanguage::TYPE_VEC2);
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["FOG"] = ShaderLanguage::TYPE_VEC4; // TODO consider adding to light shader
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["RADIANCE"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["IRRADIANCE"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].can_discard = true;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].main_function = true;

	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["ALPHA_SCISSOR_THRESHOLD"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["ALPHA_HASH_SCALE"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["ALPHA_ANTIALIASING_EDGE"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_SPATIAL].functions["fragment"].built_ins["ALPHA_TEXTURE_COORDINATE"] = ShaderLanguage::TYPE_VEC2;

	shader_modes[RS::SHADER_SPATIAL].functions["light"].built_ins["MODEL_MATRIX"] = constt(ShaderLanguage::TYPE_MAT4);
	shader_modes[RS::SHADER_SPATIAL].functions["light"].built_ins["VIEW_MATRIX"] = constt(ShaderLanguage::TYPE_MAT4);
	shader_modes[RS::SHADER_SPATIAL].functions["light"].built_ins["INV_VIEW_MATRIX"] = constt(ShaderLanguage::TYPE_MAT4);
	shader_modes[RS::SHADER_SPATIAL].functions["light"].built_ins["PROJECTION_MATRIX"] = constt(ShaderLanguage::TYPE_MAT4);
	shader_modes[RS::SHADER_SPATIAL].functions["light"].built_ins["INV_PROJECTION_MATRIX"] = constt(ShaderLanguage::TYPE_MAT4);
	shader_modes[RS::SHADER_SPATIAL].functions["light"].built_ins["VIEWPORT_SIZE"] = constt(ShaderLanguage::TYPE_VEC2);

	shader_modes[RS::SHADER_SPATIAL].functions["light"].built_ins["FRAGCOORD"] = constt(ShaderLanguage::TYPE_VEC4);
	shader_modes[RS::SHADER_SPATIAL].functions["light"].built_ins["NORMAL"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SPATIAL].functions["light"].built_ins["UV"] = constt(ShaderLanguage::TYPE_VEC2);
	shader_modes[RS::SHADER_SPATIAL].functions["light"].built_ins["UV2"] = constt(ShaderLanguage::TYPE_VEC2);
	shader_modes[RS::SHADER_SPATIAL].functions["light"].built_ins["VIEW"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SPATIAL].functions["light"].built_ins["SPECULAR_AMOUNT"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_SPATIAL].functions["light"].built_ins["LIGHT"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SPATIAL].functions["light"].built_ins["LIGHT_COLOR"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SPATIAL].functions["light"].built_ins["LIGHT_IS_DIRECTIONAL"] = constt(ShaderLanguage::TYPE_BOOL);
	shader_modes[RS::SHADER_SPATIAL].functions["light"].built_ins["ATTENUATION"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_SPATIAL].functions["light"].built_ins["ALBEDO"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SPATIAL].functions["light"].built_ins["BACKLIGHT"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SPATIAL].functions["light"].built_ins["METALLIC"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_SPATIAL].functions["light"].built_ins["ROUGHNESS"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_SPATIAL].functions["light"].built_ins["DIFFUSE_LIGHT"] = ShaderLanguage::TYPE_VEC3;
	shader_modes[RS::SHADER_SPATIAL].functions["light"].built_ins["SPECULAR_LIGHT"] = ShaderLanguage::TYPE_VEC3;
	shader_modes[RS::SHADER_SPATIAL].functions["light"].built_ins["ALPHA"] = ShaderLanguage::TYPE_FLOAT;

	shader_modes[RS::SHADER_SPATIAL].functions["light"].can_discard = true;
	shader_modes[RS::SHADER_SPATIAL].functions["light"].main_function = true;

	// spatial render modes
	{
		shader_modes[RS::SHADER_SPATIAL].modes.push_back({ PNAME("blend"), "mix", "add", "sub", "mul", "premul_alpha" });
		shader_modes[RS::SHADER_SPATIAL].modes.push_back({ PNAME("depth_draw"), "opaque", "always", "never" });
		shader_modes[RS::SHADER_SPATIAL].modes.push_back({ PNAME("depth_prepass_alpha") });
		shader_modes[RS::SHADER_SPATIAL].modes.push_back({ PNAME("depth_test_disabled") });
		shader_modes[RS::SHADER_SPATIAL].modes.push_back({ PNAME("sss_mode_skin") });
		shader_modes[RS::SHADER_SPATIAL].modes.push_back({ PNAME("cull"), "back", "front", "disabled" });
		shader_modes[RS::SHADER_SPATIAL].modes.push_back({ PNAME("unshaded") });
		shader_modes[RS::SHADER_SPATIAL].modes.push_back({ PNAME("wireframe") });
		shader_modes[RS::SHADER_SPATIAL].modes.push_back({ PNAME("diffuse"), "lambert", "lambert_wrap", "burley", "toon" });
		shader_modes[RS::SHADER_SPATIAL].modes.push_back({ PNAME("specular"), "schlick_ggx", "toon", "disabled" });
		shader_modes[RS::SHADER_SPATIAL].modes.push_back({ PNAME("skip_vertex_transform") });
		shader_modes[RS::SHADER_SPATIAL].modes.push_back({ PNAME("world_vertex_coords") });
		shader_modes[RS::SHADER_SPATIAL].modes.push_back({ PNAME("ensure_correct_normals") });
		shader_modes[RS::SHADER_SPATIAL].modes.push_back({ PNAME("shadows_disabled") });
		shader_modes[RS::SHADER_SPATIAL].modes.push_back({ PNAME("ambient_light_disabled") });
		shader_modes[RS::SHADER_SPATIAL].modes.push_back({ PNAME("shadow_to_opacity") });
		shader_modes[RS::SHADER_SPATIAL].modes.push_back({ PNAME("vertex_lighting") });
		shader_modes[RS::SHADER_SPATIAL].modes.push_back({ PNAME("particle_trails") });
		shader_modes[RS::SHADER_SPATIAL].modes.push_back({ PNAME("alpha_to_coverage") });
		shader_modes[RS::SHADER_SPATIAL].modes.push_back({ PNAME("alpha_to_coverage_and_one") });
		shader_modes[RS::SHADER_SPATIAL].modes.push_back({ PNAME("debug_shadow_splits") });
		shader_modes[RS::SHADER_SPATIAL].modes.push_back({ PNAME("fog_disabled") });
	}

	/************ CANVAS ITEM **************************/

	shader_modes[RS::SHADER_CANVAS_ITEM].functions["global"].built_ins["TIME"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["constants"].built_ins["PI"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["constants"].built_ins["TAU"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["constants"].built_ins["E"] = constt(ShaderLanguage::TYPE_FLOAT);

	shader_modes[RS::SHADER_CANVAS_ITEM].functions["vertex"].built_ins["VERTEX"] = ShaderLanguage::TYPE_VEC2;
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["vertex"].built_ins["UV"] = ShaderLanguage::TYPE_VEC2;
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["vertex"].built_ins["COLOR"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["vertex"].built_ins["POINT_SIZE"] = ShaderLanguage::TYPE_FLOAT;

	shader_modes[RS::SHADER_CANVAS_ITEM].functions["vertex"].built_ins["MODEL_MATRIX"] = constt(ShaderLanguage::TYPE_MAT4);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["vertex"].built_ins["CANVAS_MATRIX"] = constt(ShaderLanguage::TYPE_MAT4);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["vertex"].built_ins["SCREEN_MATRIX"] = constt(ShaderLanguage::TYPE_MAT4);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["vertex"].built_ins["INSTANCE_CUSTOM"] = constt(ShaderLanguage::TYPE_VEC4);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["vertex"].built_ins["INSTANCE_ID"] = constt(ShaderLanguage::TYPE_INT);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["vertex"].built_ins["VERTEX_ID"] = constt(ShaderLanguage::TYPE_INT);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["vertex"].built_ins["AT_LIGHT_PASS"] = constt(ShaderLanguage::TYPE_BOOL);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["vertex"].built_ins["TEXTURE_PIXEL_SIZE"] = constt(ShaderLanguage::TYPE_VEC2);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["vertex"].built_ins["CUSTOM0"] = constt(ShaderLanguage::TYPE_VEC4);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["vertex"].built_ins["CUSTOM1"] = constt(ShaderLanguage::TYPE_VEC4);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["vertex"].can_discard = false;
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["vertex"].main_function = true;

	shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].built_ins["VERTEX"] = ShaderLanguage::TYPE_VEC2;
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].built_ins["SHADOW_VERTEX"] = ShaderLanguage::TYPE_VEC2;
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].built_ins["LIGHT_VERTEX"] = ShaderLanguage::TYPE_VEC3;
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].built_ins["FRAGCOORD"] = constt(ShaderLanguage::TYPE_VEC4);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].built_ins["NORMAL"] = ShaderLanguage::TYPE_VEC3;
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].built_ins["NORMAL_MAP"] = ShaderLanguage::TYPE_VEC3;
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].built_ins["NORMAL_MAP_DEPTH"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].built_ins["UV"] = constt(ShaderLanguage::TYPE_VEC2);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].built_ins["COLOR"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].built_ins["TEXTURE"] = constt(ShaderLanguage::TYPE_SAMPLER2D);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].built_ins["TEXTURE_PIXEL_SIZE"] = constt(ShaderLanguage::TYPE_VEC2);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].built_ins["NORMAL_TEXTURE"] = constt(ShaderLanguage::TYPE_SAMPLER2D);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].built_ins["SPECULAR_SHININESS_TEXTURE"] = constt(ShaderLanguage::TYPE_SAMPLER2D);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].built_ins["SPECULAR_SHININESS"] = constt(ShaderLanguage::TYPE_VEC4);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].built_ins["SCREEN_UV"] = constt(ShaderLanguage::TYPE_VEC2);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].built_ins["SCREEN_PIXEL_SIZE"] = constt(ShaderLanguage::TYPE_VEC2);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].built_ins["POINT_COORD"] = constt(ShaderLanguage::TYPE_VEC2);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].built_ins["AT_LIGHT_PASS"] = constt(ShaderLanguage::TYPE_BOOL);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].can_discard = true;
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].main_function = true;

	{
		ShaderLanguage::StageFunctionInfo func;
		func.skip_function = "vertex";
		func.arguments.push_back(ShaderLanguage::StageFunctionInfo::Argument("sdf_pos", ShaderLanguage::TYPE_VEC2));
		func.return_type = ShaderLanguage::TYPE_FLOAT; //whether it could emit
		shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].stage_functions["texture_sdf"] = func;
		shader_modes[RS::SHADER_CANVAS_ITEM].functions["light"].stage_functions["texture_sdf"] = func;
		func.return_type = ShaderLanguage::TYPE_VEC2; //whether it could emit
		shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].stage_functions["sdf_to_screen_uv"] = func;
		shader_modes[RS::SHADER_CANVAS_ITEM].functions["light"].stage_functions["sdf_to_screen_uv"] = func;
		shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].stage_functions["texture_sdf_normal"] = func;
		shader_modes[RS::SHADER_CANVAS_ITEM].functions["light"].stage_functions["texture_sdf_normal"] = func;
	}

	{
		ShaderLanguage::StageFunctionInfo func;
		func.skip_function = "vertex";
		func.arguments.push_back(ShaderLanguage::StageFunctionInfo::Argument("uv", ShaderLanguage::TYPE_VEC2));
		func.return_type = ShaderLanguage::TYPE_VEC2; //whether it could emit
		shader_modes[RS::SHADER_CANVAS_ITEM].functions["fragment"].stage_functions["screen_uv_to_sdf"] = func;
		shader_modes[RS::SHADER_CANVAS_ITEM].functions["light"].stage_functions["screen_uv_to_sdf"] = func;
	}

	shader_modes[RS::SHADER_CANVAS_ITEM].functions["light"].built_ins["FRAGCOORD"] = constt(ShaderLanguage::TYPE_VEC4);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["light"].built_ins["NORMAL"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["light"].built_ins["COLOR"] = constt(ShaderLanguage::TYPE_VEC4);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["light"].built_ins["UV"] = constt(ShaderLanguage::TYPE_VEC2);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["light"].built_ins["SPECULAR_SHININESS"] = constt(ShaderLanguage::TYPE_VEC4);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["light"].built_ins["LIGHT_COLOR"] = constt(ShaderLanguage::TYPE_VEC4);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["light"].built_ins["LIGHT_POSITION"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["light"].built_ins["LIGHT_DIRECTION"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["light"].built_ins["LIGHT_ENERGY"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["light"].built_ins["LIGHT_IS_DIRECTIONAL"] = constt(ShaderLanguage::TYPE_BOOL);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["light"].built_ins["LIGHT_VERTEX"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["light"].built_ins["LIGHT"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["light"].built_ins["SHADOW_MODULATE"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["light"].built_ins["SCREEN_UV"] = constt(ShaderLanguage::TYPE_VEC2);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["light"].built_ins["TEXTURE"] = constt(ShaderLanguage::TYPE_SAMPLER2D);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["light"].built_ins["TEXTURE_PIXEL_SIZE"] = constt(ShaderLanguage::TYPE_VEC2);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["light"].built_ins["POINT_COORD"] = constt(ShaderLanguage::TYPE_VEC2);
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["light"].can_discard = true;
	shader_modes[RS::SHADER_CANVAS_ITEM].functions["light"].main_function = true;

	// canvasitem render modes
	{
		shader_modes[RS::SHADER_CANVAS_ITEM].modes.push_back({ PNAME("skip_vertex_transform") });
		shader_modes[RS::SHADER_CANVAS_ITEM].modes.push_back({ PNAME("blend"), "mix", "add", "sub", "mul", "premul_alpha", "disabled" });
		shader_modes[RS::SHADER_CANVAS_ITEM].modes.push_back({ PNAME("unshaded") });
		shader_modes[RS::SHADER_CANVAS_ITEM].modes.push_back({ PNAME("light_only") });
		shader_modes[RS::SHADER_CANVAS_ITEM].modes.push_back({ PNAME("world_vertex_coords") });
	}

	/************ PARTICLES **************************/

	shader_modes[RS::SHADER_PARTICLES].functions["global"].built_ins["TIME"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_PARTICLES].functions["constants"].built_ins["PI"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_PARTICLES].functions["constants"].built_ins["TAU"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_PARTICLES].functions["constants"].built_ins["E"] = constt(ShaderLanguage::TYPE_FLOAT);

	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["COLOR"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["VELOCITY"] = ShaderLanguage::TYPE_VEC3;
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["MASS"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["ACTIVE"] = ShaderLanguage::TYPE_BOOL;
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["CUSTOM"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["USERDATA1"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["USERDATA2"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["USERDATA3"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["USERDATA4"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["USERDATA5"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["USERDATA6"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["TRANSFORM"] = ShaderLanguage::TYPE_MAT4;
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["LIFETIME"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["DELTA"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["NUMBER"] = constt(ShaderLanguage::TYPE_UINT);
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["INDEX"] = constt(ShaderLanguage::TYPE_UINT);
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["EMISSION_TRANSFORM"] = constt(ShaderLanguage::TYPE_MAT4);
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["EMITTER_VELOCITY"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["INTERPOLATE_TO_END"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["RANDOM_SEED"] = constt(ShaderLanguage::TYPE_UINT);
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["FLAG_EMIT_POSITION"] = constt(ShaderLanguage::TYPE_UINT);
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["FLAG_EMIT_ROT_SCALE"] = constt(ShaderLanguage::TYPE_UINT);
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["FLAG_EMIT_VELOCITY"] = constt(ShaderLanguage::TYPE_UINT);
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["FLAG_EMIT_COLOR"] = constt(ShaderLanguage::TYPE_UINT);
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["FLAG_EMIT_CUSTOM"] = constt(ShaderLanguage::TYPE_UINT);
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["RESTART_POSITION"] = constt(ShaderLanguage::TYPE_BOOL);
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["RESTART_ROT_SCALE"] = constt(ShaderLanguage::TYPE_BOOL);
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["RESTART_VELOCITY"] = constt(ShaderLanguage::TYPE_BOOL);
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["RESTART_COLOR"] = constt(ShaderLanguage::TYPE_BOOL);
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["RESTART_CUSTOM"] = constt(ShaderLanguage::TYPE_BOOL);
	shader_modes[RS::SHADER_PARTICLES].functions["start"].built_ins["AMOUNT_RATIO"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_PARTICLES].functions["start"].main_function = true;

	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["COLOR"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["VELOCITY"] = ShaderLanguage::TYPE_VEC3;
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["MASS"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["ACTIVE"] = ShaderLanguage::TYPE_BOOL;
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["RESTART"] = constt(ShaderLanguage::TYPE_BOOL);
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["CUSTOM"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["USERDATA1"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["USERDATA2"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["USERDATA3"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["USERDATA4"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["USERDATA5"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["USERDATA6"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["TRANSFORM"] = ShaderLanguage::TYPE_MAT4;
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["LIFETIME"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["DELTA"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["NUMBER"] = constt(ShaderLanguage::TYPE_UINT);
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["INDEX"] = constt(ShaderLanguage::TYPE_UINT);
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["EMISSION_TRANSFORM"] = constt(ShaderLanguage::TYPE_MAT4);
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["EMITTER_VELOCITY"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["INTERPOLATE_TO_END"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["RANDOM_SEED"] = constt(ShaderLanguage::TYPE_UINT);
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["FLAG_EMIT_POSITION"] = constt(ShaderLanguage::TYPE_UINT);
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["FLAG_EMIT_ROT_SCALE"] = constt(ShaderLanguage::TYPE_UINT);
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["FLAG_EMIT_VELOCITY"] = constt(ShaderLanguage::TYPE_UINT);
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["FLAG_EMIT_COLOR"] = constt(ShaderLanguage::TYPE_UINT);
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["FLAG_EMIT_CUSTOM"] = constt(ShaderLanguage::TYPE_UINT);
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["COLLIDED"] = constt(ShaderLanguage::TYPE_BOOL);
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["COLLISION_NORMAL"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["COLLISION_DEPTH"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["ATTRACTOR_FORCE"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_PARTICLES].functions["process"].built_ins["AMOUNT_RATIO"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_PARTICLES].functions["process"].main_function = true;

	{
		ShaderLanguage::StageFunctionInfo emit_vertex_func;
		emit_vertex_func.arguments.push_back(ShaderLanguage::StageFunctionInfo::Argument("xform", ShaderLanguage::TYPE_MAT4));
		emit_vertex_func.arguments.push_back(ShaderLanguage::StageFunctionInfo::Argument("velocity", ShaderLanguage::TYPE_VEC3));
		emit_vertex_func.arguments.push_back(ShaderLanguage::StageFunctionInfo::Argument("color", ShaderLanguage::TYPE_VEC4));
		emit_vertex_func.arguments.push_back(ShaderLanguage::StageFunctionInfo::Argument("custom", ShaderLanguage::TYPE_VEC4));
		emit_vertex_func.arguments.push_back(ShaderLanguage::StageFunctionInfo::Argument("flags", ShaderLanguage::TYPE_UINT));
		emit_vertex_func.return_type = ShaderLanguage::TYPE_BOOL; //whether it could emit
		shader_modes[RS::SHADER_PARTICLES].functions["start"].stage_functions["emit_subparticle"] = emit_vertex_func;
		shader_modes[RS::SHADER_PARTICLES].functions["process"].stage_functions["emit_subparticle"] = emit_vertex_func;
	}

	// particles render modes
	{
		shader_modes[RS::SHADER_PARTICLES].modes.push_back({ PNAME("collision_use_scale") });
		shader_modes[RS::SHADER_PARTICLES].modes.push_back({ PNAME("disable_force") });
		shader_modes[RS::SHADER_PARTICLES].modes.push_back({ PNAME("disable_velocity") });
		shader_modes[RS::SHADER_PARTICLES].modes.push_back({ PNAME("keep_data") });
	}

	/************ SKY **************************/

	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["TIME"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_SKY].functions["constants"].built_ins["PI"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_SKY].functions["constants"].built_ins["TAU"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_SKY].functions["constants"].built_ins["E"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["POSITION"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["RADIANCE"] = constt(ShaderLanguage::TYPE_SAMPLERCUBE);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["AT_HALF_RES_PASS"] = constt(ShaderLanguage::TYPE_BOOL);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["AT_QUARTER_RES_PASS"] = constt(ShaderLanguage::TYPE_BOOL);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["AT_CUBEMAP_PASS"] = constt(ShaderLanguage::TYPE_BOOL);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["LIGHT0_ENABLED"] = constt(ShaderLanguage::TYPE_BOOL);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["LIGHT0_DIRECTION"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["LIGHT0_ENERGY"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["LIGHT0_COLOR"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["LIGHT0_SIZE"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["LIGHT1_ENABLED"] = constt(ShaderLanguage::TYPE_BOOL);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["LIGHT1_DIRECTION"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["LIGHT1_ENERGY"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["LIGHT1_COLOR"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["LIGHT1_SIZE"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["LIGHT2_ENABLED"] = constt(ShaderLanguage::TYPE_BOOL);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["LIGHT2_DIRECTION"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["LIGHT2_ENERGY"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["LIGHT2_COLOR"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["LIGHT2_SIZE"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["LIGHT3_ENABLED"] = constt(ShaderLanguage::TYPE_BOOL);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["LIGHT3_DIRECTION"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["LIGHT3_ENERGY"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["LIGHT3_COLOR"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SKY].functions["global"].built_ins["LIGHT3_SIZE"] = constt(ShaderLanguage::TYPE_FLOAT);

	shader_modes[RS::SHADER_SKY].functions["sky"].built_ins["COLOR"] = ShaderLanguage::TYPE_VEC3;
	shader_modes[RS::SHADER_SKY].functions["sky"].built_ins["ALPHA"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_SKY].functions["sky"].built_ins["EYEDIR"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_SKY].functions["sky"].built_ins["SCREEN_UV"] = constt(ShaderLanguage::TYPE_VEC2);
	shader_modes[RS::SHADER_SKY].functions["sky"].built_ins["FRAGCOORD"] = constt(ShaderLanguage::TYPE_VEC4);
	shader_modes[RS::SHADER_SKY].functions["sky"].built_ins["SKY_COORDS"] = constt(ShaderLanguage::TYPE_VEC2);
	shader_modes[RS::SHADER_SKY].functions["sky"].built_ins["HALF_RES_COLOR"] = constt(ShaderLanguage::TYPE_VEC4);
	shader_modes[RS::SHADER_SKY].functions["sky"].built_ins["QUARTER_RES_COLOR"] = constt(ShaderLanguage::TYPE_VEC4);
	shader_modes[RS::SHADER_SKY].functions["sky"].built_ins["FOG"] = ShaderLanguage::TYPE_VEC4;
	shader_modes[RS::SHADER_SKY].functions["sky"].main_function = true;

	// sky render modes
	{
		shader_modes[RS::SHADER_SKY].modes.push_back({ PNAME("use_half_res_pass") });
		shader_modes[RS::SHADER_SKY].modes.push_back({ PNAME("use_quarter_res_pass") });
		shader_modes[RS::SHADER_SKY].modes.push_back({ PNAME("disable_fog") });
		shader_modes[RS::SHADER_SKY].modes.push_back({ PNAME("use_debanding") });
	}

	/************ FOG **************************/

	shader_modes[RS::SHADER_FOG].functions["global"].built_ins["TIME"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_FOG].functions["constants"].built_ins["PI"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_FOG].functions["constants"].built_ins["TAU"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_FOG].functions["constants"].built_ins["E"] = constt(ShaderLanguage::TYPE_FLOAT);

	shader_modes[RS::SHADER_FOG].functions["fog"].built_ins["WORLD_POSITION"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_FOG].functions["fog"].built_ins["OBJECT_POSITION"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_FOG].functions["fog"].built_ins["UVW"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_FOG].functions["fog"].built_ins["SIZE"] = constt(ShaderLanguage::TYPE_VEC3);
	shader_modes[RS::SHADER_FOG].functions["fog"].built_ins["SDF"] = constt(ShaderLanguage::TYPE_FLOAT);
	shader_modes[RS::SHADER_FOG].functions["fog"].built_ins["ALBEDO"] = ShaderLanguage::TYPE_VEC3;
	shader_modes[RS::SHADER_FOG].functions["fog"].built_ins["DENSITY"] = ShaderLanguage::TYPE_FLOAT;
	shader_modes[RS::SHADER_FOG].functions["fog"].built_ins["EMISSION"] = ShaderLanguage::TYPE_VEC3;
	shader_modes[RS::SHADER_FOG].functions["fog"].main_function = true;

	shader_types_list.push_back("spatial");
	shader_types_list.push_back("canvas_item");
	shader_types_list.push_back("particles");
	shader_types_list.push_back("sky");
	shader_types_list.push_back("fog");

	for (const String &type : shader_types_list) {
		shader_types.insert(type);
	}
}
