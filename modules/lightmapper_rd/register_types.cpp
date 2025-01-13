/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "lightmapper_rd.h"

#include "core/config/project_settings.h"
#include "scene/3d/lightmapper.h"

#ifndef _3D_DISABLED
static Lightmapper *create_lightmapper_rd() {
	return memnew(LightmapperRD);
}
#endif

void initialize_lightmapper_rd_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/lightmapping/bake_quality/low_quality_ray_count", PROPERTY_HINT_RANGE, "1,4096,1,or_greater"), 32);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/lightmapping/bake_quality/medium_quality_ray_count", PROPERTY_HINT_RANGE, "1,4096,1,or_greater"), 128);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/lightmapping/bake_quality/high_quality_ray_count", PROPERTY_HINT_RANGE, "1,4096,1,or_greater"), 512);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/lightmapping/bake_quality/ultra_quality_ray_count", PROPERTY_HINT_RANGE, "1,4096,1,or_greater"), 2048);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/lightmapping/bake_performance/max_rays_per_pass", PROPERTY_HINT_RANGE, "1,256,1,or_greater"), 32);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/lightmapping/bake_performance/region_size", PROPERTY_HINT_RANGE, "1,4096,1,or_greater"), 512);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/lightmapping/bake_performance/max_transparency_rays", PROPERTY_HINT_RANGE, "1,256,1,or_greater"), 8);

	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/lightmapping/bake_quality/low_quality_probe_ray_count", PROPERTY_HINT_RANGE, "1,4096,1,or_greater"), 64);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/lightmapping/bake_quality/medium_quality_probe_ray_count", PROPERTY_HINT_RANGE, "1,4096,1,or_greater"), 256);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/lightmapping/bake_quality/high_quality_probe_ray_count", PROPERTY_HINT_RANGE, "1,4096,1,or_greater"), 512);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/lightmapping/bake_quality/ultra_quality_probe_ray_count", PROPERTY_HINT_RANGE, "1,4096,1,or_greater"), 2048);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/lightmapping/bake_performance/max_rays_per_probe_pass", PROPERTY_HINT_RANGE, "1,256,1,or_greater"), 64);

	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/lightmapping/denoising/denoiser", PROPERTY_HINT_ENUM, "JNLM,OIDN"), 0);
#ifndef _3D_DISABLED
	GDREGISTER_CLASS(LightmapperRD);
	Lightmapper::create_gpu = create_lightmapper_rd;
#endif
}

void uninitialize_lightmapper_rd_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
