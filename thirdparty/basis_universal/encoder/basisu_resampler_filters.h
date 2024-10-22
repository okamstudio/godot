// basisu_resampler_filters.h
// Copyright (C) 2019-2024 Binomial LLC. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#include "../transcoder/basisu.h"

namespace basisu
{
	typedef float (*resample_filter_func)(float t);

	struct resample_filter
	{
		const char *name;
		resample_filter_func func;
		float support;
	};

	extern const resample_filter g_resample_filters[];
	extern const int g_num_resample_filters;

	int find_resample_filter(const char *pName);

} // namespace basisu
