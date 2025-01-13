/**************************************************************************/
/*  sort_effects.cpp                                                      */
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

#include "sort_effects.h"
// #include "servers/rendering/renderer_rd/renderer_compositor_rd.h"

using namespace RendererRD;

SortEffects::SortEffects() {
	Vector<String> sort_modes;
	sort_modes.push_back("\n#define MODE_SORT_BLOCK\n");
	sort_modes.push_back("\n#define MODE_SORT_STEP\n");
	sort_modes.push_back("\n#define MODE_SORT_INNER\n");

	shader.initialize(sort_modes);

	shader_version = shader.version_create();

	for (int i = 0; i < SORT_MODE_MAX; i++) {
		pipelines[i] = RD::get_singleton()->compute_pipeline_create(shader.version_get_shader(shader_version, i));
	}
}

SortEffects::~SortEffects() {
	shader.version_free(shader_version);
}

void SortEffects::sort_buffer(RID p_uniform_set, int p_size) {
	PushConstant push_constant;
	push_constant.total_elements = p_size;

	bool done = true;

	int numThreadGroups = ((p_size - 1) >> 9) + 1;

	if (numThreadGroups > 1) {
		done = false;
	}

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();

	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[SORT_MODE_BLOCK]);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, p_uniform_set, 1);
	RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(PushConstant));
	RD::get_singleton()->compute_list_dispatch(compute_list, numThreadGroups, 1, 1);

	int presorted = 512;

	while (!done) {
		RD::get_singleton()->compute_list_add_barrier(compute_list);

		done = true;
		RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[SORT_MODE_STEP]);

		numThreadGroups = 0;

		if (p_size > presorted) {
			if (p_size > presorted * 2) {
				done = false;
			}

			int pow2 = presorted;
			while (pow2 < p_size) {
				pow2 *= 2;
			}
			numThreadGroups = pow2 >> 9;
		}

		unsigned int nMergeSize = presorted * 2;

		for (unsigned int nMergeSubSize = nMergeSize >> 1; nMergeSubSize > 256; nMergeSubSize = nMergeSubSize >> 1) {
			push_constant.job_params[0] = nMergeSubSize;
			if (nMergeSubSize == nMergeSize >> 1) {
				push_constant.job_params[1] = (2 * nMergeSubSize - 1);
				push_constant.job_params[2] = -1;
			} else {
				push_constant.job_params[1] = nMergeSubSize;
				push_constant.job_params[2] = 1;
			}
			push_constant.job_params[3] = 0;

			RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(PushConstant));
			RD::get_singleton()->compute_list_dispatch(compute_list, numThreadGroups, 1, 1);
			RD::get_singleton()->compute_list_add_barrier(compute_list);
		}

		RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[SORT_MODE_INNER]);
		RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(PushConstant));
		RD::get_singleton()->compute_list_dispatch(compute_list, numThreadGroups, 1, 1);

		presorted *= 2;
	}

	RD::get_singleton()->compute_list_end();
}
