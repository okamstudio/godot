/**************************************************************************/
/*  canvas_item_material.cpp                                              */
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

#include "canvas_item_material.h"

#include "core/version.h"

Mutex CanvasItemMaterial::material_mutex;
SelfList<CanvasItemMaterial>::List CanvasItemMaterial::dirty_materials;
HashMap<CanvasItemMaterial::MaterialKey, CanvasItemMaterial::ShaderData, CanvasItemMaterial::MaterialKey> CanvasItemMaterial::shader_map;
CanvasItemMaterial::ShaderNames *CanvasItemMaterial::shader_names = nullptr;

void CanvasItemMaterial::init_shaders() {
	shader_names = memnew(ShaderNames);

	shader_names->particles_anim_h_frames = "particles_anim_h_frames";
	shader_names->particles_anim_v_frames = "particles_anim_v_frames";
	shader_names->particles_anim_loop = "particles_anim_loop";
}

void CanvasItemMaterial::finish_shaders() {
	dirty_materials.clear();

	memdelete(shader_names);
	shader_names = nullptr;
}

void CanvasItemMaterial::_update_shader() {
	MaterialKey mk = _compute_key();
	if (mk.key == current_key.key) {
		return; //no update required in the end
	}

	if (shader_map.has(current_key)) {
		shader_map[current_key].users--;
		if (shader_map[current_key].users == 0) {
			//deallocate shader, as it's no longer in use
			RS::get_singleton()->free(shader_map[current_key].shader);
			shader_map.erase(current_key);
		}
	}

	current_key = mk;

	if (shader_map.has(mk)) {
		RS::get_singleton()->material_set_shader(_get_material(), shader_map[mk].shader);
		shader_map[mk].users++;
		return;
	}

	//must create a shader!

	// Add a comment to describe the shader origin (useful when converting to ShaderMaterial).
	String code = "// NOTE: Shader automatically converted from " VERSION_NAME " " VERSION_FULL_CONFIG "'s CanvasItemMaterial.\n\n";

	code += "shader_type canvas_item;\nrender_mode ";
	switch (blend_mode) {
		case BLEND_MODE_MIX:
			code += "blend_mix";
			break;
		case BLEND_MODE_ADD:
			code += "blend_add";
			break;
		case BLEND_MODE_SUB:
			code += "blend_sub";
			break;
		case BLEND_MODE_MUL:
			code += "blend_mul";
			break;
		case BLEND_MODE_PREMULT_ALPHA:
			code += "blend_premul_alpha";
			break;
		case BLEND_MODE_DISABLED:
			code += "blend_disabled";
			break;
	}

	switch (light_mode) {
		case LIGHT_MODE_NORMAL:
			break;
		case LIGHT_MODE_UNSHADED:
			code += ",unshaded";
			break;
		case LIGHT_MODE_LIGHT_ONLY:
			code += ",light_only";
			break;
	}

	code += ";\n";

	if (particles_animation) {
		code += "uniform int particles_anim_h_frames;\n";
		code += "uniform int particles_anim_v_frames;\n";
		code += "uniform bool particles_anim_loop;\n\n";

		code += "void vertex() {\n";
		code += "	float h_frames = float(particles_anim_h_frames);\n";
		code += "	float v_frames = float(particles_anim_v_frames);\n";
		code += "	VERTEX.xy /= vec2(h_frames, v_frames);\n";
		code += "	float particle_total_frames = float(particles_anim_h_frames * particles_anim_v_frames);\n";
		code += "	float particle_frame = floor(INSTANCE_CUSTOM.z * float(particle_total_frames));\n";
		code += "	if (!particles_anim_loop) {\n";
		code += "		particle_frame = clamp(particle_frame, 0.0, particle_total_frames - 1.0);\n";
		code += "	} else {\n";
		code += "		particle_frame = mod(particle_frame, particle_total_frames);\n";
		code += "	}";
		code += "	UV /= vec2(h_frames, v_frames);\n";
		code += "	UV += vec2(mod(particle_frame, h_frames) / h_frames, floor((particle_frame + 0.5) / h_frames) / v_frames);\n";
		code += "}\n";
	}

	ShaderData shader_data;
	shader_data.shader = RS::get_singleton()->shader_create();
	shader_data.users = 1;

	RS::get_singleton()->shader_set_code(shader_data.shader, code);

	shader_map[mk] = shader_data;

	RS::get_singleton()->material_set_shader(_get_material(), shader_data.shader);
}

void CanvasItemMaterial::flush_changes() {
	MutexLock lock(material_mutex);

	while (dirty_materials.first()) {
		dirty_materials.first()->self()->_update_shader();
		dirty_materials.first()->remove_from_list();
	}
}

void CanvasItemMaterial::_queue_shader_change() {
	MutexLock lock(material_mutex);

	if (_is_initialized() && !element.in_list()) {
		dirty_materials.add(&element);
	}
}

void CanvasItemMaterial::set_blend_mode(BlendMode p_blend_mode) {
	blend_mode = p_blend_mode;
	_queue_shader_change();
}

CanvasItemMaterial::BlendMode CanvasItemMaterial::get_blend_mode() const {
	return blend_mode;
}

void CanvasItemMaterial::set_light_mode(LightMode p_light_mode) {
	light_mode = p_light_mode;
	_queue_shader_change();
}

CanvasItemMaterial::LightMode CanvasItemMaterial::get_light_mode() const {
	return light_mode;
}

void CanvasItemMaterial::set_particles_animation(bool p_particles_anim) {
	particles_animation = p_particles_anim;
	_queue_shader_change();
	notify_property_list_changed();
}

bool CanvasItemMaterial::get_particles_animation() const {
	return particles_animation;
}

void CanvasItemMaterial::set_particles_anim_h_frames(int p_frames) {
	particles_anim_h_frames = p_frames;
	RS::get_singleton()->material_set_param(_get_material(), shader_names->particles_anim_h_frames, p_frames);
}

int CanvasItemMaterial::get_particles_anim_h_frames() const {
	return particles_anim_h_frames;
}

void CanvasItemMaterial::set_particles_anim_v_frames(int p_frames) {
	particles_anim_v_frames = p_frames;
	RS::get_singleton()->material_set_param(_get_material(), shader_names->particles_anim_v_frames, p_frames);
}

int CanvasItemMaterial::get_particles_anim_v_frames() const {
	return particles_anim_v_frames;
}

void CanvasItemMaterial::set_particles_anim_loop(bool p_loop) {
	particles_anim_loop = p_loop;
	RS::get_singleton()->material_set_param(_get_material(), shader_names->particles_anim_loop, particles_anim_loop);
}

bool CanvasItemMaterial::get_particles_anim_loop() const {
	return particles_anim_loop;
}

void CanvasItemMaterial::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name.begins_with("particles_anim_") && !particles_animation) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
}

RID CanvasItemMaterial::get_shader_rid() const {
	ERR_FAIL_COND_V(!shader_map.has(current_key), RID());
	return shader_map[current_key].shader;
}

Shader::Mode CanvasItemMaterial::get_shader_mode() const {
	return Shader::MODE_CANVAS_ITEM;
}

void CanvasItemMaterial::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_blend_mode", "blend_mode"), &CanvasItemMaterial::set_blend_mode);
	ClassDB::bind_method(D_METHOD("get_blend_mode"), &CanvasItemMaterial::get_blend_mode);

	ClassDB::bind_method(D_METHOD("set_light_mode", "light_mode"), &CanvasItemMaterial::set_light_mode);
	ClassDB::bind_method(D_METHOD("get_light_mode"), &CanvasItemMaterial::get_light_mode);

	ClassDB::bind_method(D_METHOD("set_particles_animation", "particles_anim"), &CanvasItemMaterial::set_particles_animation);
	ClassDB::bind_method(D_METHOD("get_particles_animation"), &CanvasItemMaterial::get_particles_animation);

	ClassDB::bind_method(D_METHOD("set_particles_anim_h_frames", "frames"), &CanvasItemMaterial::set_particles_anim_h_frames);
	ClassDB::bind_method(D_METHOD("get_particles_anim_h_frames"), &CanvasItemMaterial::get_particles_anim_h_frames);

	ClassDB::bind_method(D_METHOD("set_particles_anim_v_frames", "frames"), &CanvasItemMaterial::set_particles_anim_v_frames);
	ClassDB::bind_method(D_METHOD("get_particles_anim_v_frames"), &CanvasItemMaterial::get_particles_anim_v_frames);

	ClassDB::bind_method(D_METHOD("set_particles_anim_loop", "loop"), &CanvasItemMaterial::set_particles_anim_loop);
	ClassDB::bind_method(D_METHOD("get_particles_anim_loop"), &CanvasItemMaterial::get_particles_anim_loop);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "blend_mode", PROPERTY_HINT_ENUM, "Mix,Add,Subtract,Multiply,Premultiplied Alpha"), "set_blend_mode", "get_blend_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "light_mode", PROPERTY_HINT_ENUM, "Normal,Unshaded,Light Only"), "set_light_mode", "get_light_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "particles_animation"), "set_particles_animation", "get_particles_animation");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "particles_anim_h_frames", PROPERTY_HINT_RANGE, "1,128,1"), "set_particles_anim_h_frames", "get_particles_anim_h_frames");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "particles_anim_v_frames", PROPERTY_HINT_RANGE, "1,128,1"), "set_particles_anim_v_frames", "get_particles_anim_v_frames");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "particles_anim_loop"), "set_particles_anim_loop", "get_particles_anim_loop");

	BIND_ENUM_CONSTANT(BLEND_MODE_MIX);
	BIND_ENUM_CONSTANT(BLEND_MODE_ADD);
	BIND_ENUM_CONSTANT(BLEND_MODE_SUB);
	BIND_ENUM_CONSTANT(BLEND_MODE_MUL);
	BIND_ENUM_CONSTANT(BLEND_MODE_PREMULT_ALPHA);

	BIND_ENUM_CONSTANT(LIGHT_MODE_NORMAL);
	BIND_ENUM_CONSTANT(LIGHT_MODE_UNSHADED);
	BIND_ENUM_CONSTANT(LIGHT_MODE_LIGHT_ONLY);
}

CanvasItemMaterial::CanvasItemMaterial() :
		element(this) {
	_set_material(RS::get_singleton()->material_create());

	set_particles_anim_h_frames(1);
	set_particles_anim_v_frames(1);
	set_particles_anim_loop(false);

	current_key.invalid_key = 1;

	_mark_initialized(callable_mp(this, &CanvasItemMaterial::_queue_shader_change), callable_mp(this, &CanvasItemMaterial::_update_shader));
}

CanvasItemMaterial::~CanvasItemMaterial() {
	MutexLock lock(material_mutex);

	ERR_FAIL_NULL(RenderingServer::get_singleton());

	if (shader_map.has(current_key)) {
		shader_map[current_key].users--;
		if (shader_map[current_key].users == 0) {
			//deallocate shader, as it's no longer in use
			RS::get_singleton()->free(shader_map[current_key].shader);
			shader_map.erase(current_key);
		}

		RS::get_singleton()->material_set_shader(_get_material(), RID());
	}
}
