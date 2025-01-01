/**************************************************************************/
/*  sky.cpp                                                               */
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

#include "sky.h"

void Sky::set_radiance_size(RadianceSize p_size) {
	ERR_FAIL_INDEX(p_size, RADIANCE_SIZE_MAX);

	radiance_size = p_size;
	static const int size[RADIANCE_SIZE_MAX] = {
		32, 64, 128, 256, 512, 1024, 2048
	};
	RS::get_singleton()->sky_set_radiance_size(sky, size[radiance_size]);
}

Sky::RadianceSize Sky::get_radiance_size() const {
	return radiance_size;
}

void Sky::set_process_mode(ProcessMode p_mode) {
	mode = p_mode;
	RS::get_singleton()->sky_set_mode(sky, RS::SkyMode(mode));
}

Sky::ProcessMode Sky::get_process_mode() const {
	return mode;
}

void Sky::set_material(const Ref<Material> &p_material) {
	sky_material = p_material;
	RID material_rid;
	if (sky_material.is_valid()) {
		material_rid = sky_material->get_rid();
	}
	RS::get_singleton()->sky_set_material(sky, material_rid);
}

Ref<Material> Sky::get_material() const {
	return sky_material;
}

RID Sky::get_rid() const {
	return sky;
}

void Sky::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radiance_size", "size"), &Sky::set_radiance_size);
	ClassDB::bind_method(D_METHOD("get_radiance_size"), &Sky::get_radiance_size);

	ClassDB::bind_method(D_METHOD("set_process_mode", "mode"), &Sky::set_process_mode);
	ClassDB::bind_method(D_METHOD("get_process_mode"), &Sky::get_process_mode);

	ClassDB::bind_method(D_METHOD("set_material", "material"), &Sky::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &Sky::get_material);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "sky_material", PROPERTY_HINT_RESOURCE_TYPE, "PanoramaSkyMaterial,ProceduralSkyMaterial,PhysicalSkyMaterial,ShaderMaterial"), "set_material", "get_material");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "process_mode", PROPERTY_HINT_ENUM, "Automatic,High-Quality,High-Quality Incremental,Real-Time"), "set_process_mode", "get_process_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "radiance_size", PROPERTY_HINT_ENUM, "32,64,128,256,512,1024,2048"), "set_radiance_size", "get_radiance_size");

	BIND_ENUM_CONSTANT(RADIANCE_SIZE_32);
	BIND_ENUM_CONSTANT(RADIANCE_SIZE_64);
	BIND_ENUM_CONSTANT(RADIANCE_SIZE_128);
	BIND_ENUM_CONSTANT(RADIANCE_SIZE_256);
	BIND_ENUM_CONSTANT(RADIANCE_SIZE_512);
	BIND_ENUM_CONSTANT(RADIANCE_SIZE_1024);
	BIND_ENUM_CONSTANT(RADIANCE_SIZE_2048);
	BIND_ENUM_CONSTANT(RADIANCE_SIZE_MAX);

	BIND_ENUM_CONSTANT(PROCESS_MODE_AUTOMATIC);
	BIND_ENUM_CONSTANT(PROCESS_MODE_QUALITY);
	BIND_ENUM_CONSTANT(PROCESS_MODE_INCREMENTAL);
	BIND_ENUM_CONSTANT(PROCESS_MODE_REALTIME);
}

Sky::Sky() {
	sky = RS::get_singleton()->sky_create();
}

Sky::~Sky() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RS::get_singleton()->free(sky);
}
