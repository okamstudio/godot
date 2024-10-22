// basisu_kernels_sse.cpp
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
#include "basisu_enc.h"

#if BASISU_SUPPORT_SSE

#define CPPSPMD_SSE2 0

#ifdef _MSC_VER
#include <intrin.h>
#endif

#include "cppspmd_sse.h"

#include "cppspmd_type_aliases.h"

using namespace basisu;

#include "basisu_kernels_declares.h"
#include "basisu_kernels_imp.h"

namespace basisu
{

struct cpu_info
{
	cpu_info() { memset(this, 0, sizeof(*this)); }

	bool m_has_fpu;
	bool m_has_mmx;
	bool m_has_sse;
	bool m_has_sse2;
	bool m_has_sse3;
	bool m_has_ssse3;
	bool m_has_sse41;
	bool m_has_sse42;
	bool m_has_avx;
	bool m_has_avx2;
	bool m_has_pclmulqdq;
};

static void extract_x86_flags(cpu_info &info, uint32_t ecx, uint32_t edx)
{
	info.m_has_fpu = (edx & (1 << 0)) != 0;
	info.m_has_mmx = (edx & (1 << 23)) != 0;
	info.m_has_sse = (edx & (1 << 25)) != 0;
	info.m_has_sse2 = (edx & (1 << 26)) != 0;
	info.m_has_sse3 = (ecx & (1 << 0)) != 0;
	info.m_has_ssse3 = (ecx & (1 << 9)) != 0;
	info.m_has_sse41 = (ecx & (1 << 19)) != 0;
	info.m_has_sse42 = (ecx & (1 << 20)) != 0;
	info.m_has_pclmulqdq = (ecx & (1 << 1)) != 0;
	info.m_has_avx = (ecx & (1 << 28)) != 0;
}

static void extract_x86_extended_flags(cpu_info &info, uint32_t ebx)
{
	info.m_has_avx2 = (ebx & (1 << 5)) != 0;
}

#ifndef _MSC_VER
static void do_cpuid(uint32_t eax, uint32_t ecx, uint32_t* regs)
{
	uint32_t ebx = 0, edx = 0;

#if defined(__PIC__) && defined(__i386__)
	__asm__("movl %%ebx, %%edi;"
		"cpuid;"
		"xchgl %%ebx, %%edi;"
		: "=D"(ebx), "+a"(eax), "+c"(ecx), "=d"(edx));
#else
	__asm__("cpuid;" : "+b"(ebx), "+a"(eax), "+c"(ecx), "=d"(edx));
#endif

	regs[0] = eax; regs[1] = ebx; regs[2] = ecx; regs[3] = edx;
}
#endif

static void get_cpuinfo(cpu_info &info)
{
	int regs[4];

#ifdef _MSC_VER
	__cpuid(regs, 0);
#else
	do_cpuid(0, 0, (uint32_t *)regs);
#endif

	const uint32_t max_eax = regs[0];

	if (max_eax >= 1U)
	{
#ifdef _MSC_VER
		__cpuid(regs, 1);
#else
		do_cpuid(1, 0, (uint32_t*)regs);
#endif
		extract_x86_flags(info, regs[2], regs[3]);
	}

	if (max_eax >= 7U)
	{
#ifdef _MSC_VER
		__cpuidex(regs, 7, 0);
#else
		do_cpuid(7, 0, (uint32_t*)regs);
#endif

		extract_x86_extended_flags(info, regs[1]);
	}
}

void detect_sse41()
{
	cpu_info info;
	get_cpuinfo(info);

	// Check for everything from SSE to SSE 4.1
	g_cpu_supports_sse41 = info.m_has_sse && info.m_has_sse2 && info.m_has_sse3 && info.m_has_ssse3 && info.m_has_sse41;
}

} // namespace basisu
#else // #if BASISU_SUPPORT_SSE
namespace basisu
{

void detect_sse41()
{
}

} // namespace basisu
#endif // #if BASISU_SUPPORT_SSE

