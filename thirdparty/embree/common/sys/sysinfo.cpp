// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#if defined(__INTEL_LLVM_COMPILER)
// prevents "'__thiscall' calling convention is not supported for this target" warning from TBB
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wignored-attributes"
#endif

#include "sysinfo.h"
#include "intrinsics.h"
#include "estring.h"
#include "ref.h"
#if defined(__FREEBSD__)
#include <sys/cpuset.h>
#include <pthread_np.h>
typedef cpuset_t cpu_set_t;
#endif

////////////////////////////////////////////////////////////////////////////////
/// All Platforms
////////////////////////////////////////////////////////////////////////////////

namespace embree
{
  NullTy null;
  
  std::string getPlatformName() 
  {
#if defined(__ANDROID__) && !defined(__64BIT__)
    return "Android (32bit)";
#elif defined(__ANDROID__) && defined(__64BIT__)
    return "Android (64bit)";
#elif defined(__LINUX__) && !defined(__64BIT__)
    return "Linux (32bit)";
#elif defined(__LINUX__) && defined(__64BIT__)
    return "Linux (64bit)";
#elif defined(__FREEBSD__) && !defined(__64BIT__)
    return "FreeBSD (32bit)";
#elif defined(__FREEBSD__) && defined(__64BIT__)
    return "FreeBSD (64bit)";
#elif defined(__CYGWIN__) && !defined(__64BIT__)
    return "Cygwin (32bit)";
#elif defined(__CYGWIN__) && defined(__64BIT__)
    return "Cygwin (64bit)";
#elif defined(__WIN32__) && !defined(__64BIT__)
    return "Windows (32bit)";
#elif defined(__WIN32__) && defined(__64BIT__)
    return "Windows (64bit)";
#elif defined(__MACOSX__) && !defined(__64BIT__)
    return "Mac OS X (32bit)";
#elif defined(__MACOSX__) && defined(__64BIT__)
    return "Mac OS X (64bit)";
#elif defined(__UNIX__) && !defined(__64BIT__)
    return "Unix (32bit)";
#elif defined(__UNIX__) && defined(__64BIT__)
    return "Unix (64bit)";
#else
    return "Unknown";
#endif
  }

  std::string getCompilerName()
  {
#if defined(__INTEL_COMPILER)
    int icc_mayor = __INTEL_COMPILER / 100 % 100;
    int icc_minor = __INTEL_COMPILER % 100;
    std::string version = "Intel Compiler ";
    version += toString(icc_mayor);
    version += "." + toString(icc_minor);
#if defined(__INTEL_COMPILER_UPDATE)
    version += "." + toString(__INTEL_COMPILER_UPDATE);
#endif
    return version;
#elif defined(__clang__)
    return "CLANG " __clang_version__;
#elif defined (__GNUC__)
    return "GCC " __VERSION__;
#elif defined(_MSC_VER)
    std::string version = toString(_MSC_FULL_VER);
    version.insert(4,".");
    version.insert(9,".");
    version.insert(2,".");
    return "Visual C++ Compiler " + version;
#else
    return "Unknown Compiler";
#endif
  }

  std::string getCPUVendor()
  {
#if defined(__X86_ASM__)
    int cpuinfo[4]; 
    __cpuid (cpuinfo, 0); 
    int name[4];
    name[0] = cpuinfo[1];
    name[1] = cpuinfo[3];
    name[2] = cpuinfo[2];
    name[3] = 0;
    return (char*)name;
#elif defined(__ARM_NEON)
    return "ARM";
#else
    return "Unknown";
#endif
  }

  CPU getCPUModel() 
  {
#if defined(__X86_ASM__)
    if (getCPUVendor() != "GenuineIntel")
      return CPU::UNKNOWN;
    
    int out[4];
    __cpuid(out, 0);
    if (out[0] < 1) return CPU::UNKNOWN;
    __cpuid(out, 1);

    /* please see CPUID documentation for these formulas */
    uint32_t family_ID          = (out[0] >>  8) & 0x0F;
    uint32_t extended_family_ID = (out[0] >> 20) & 0xFF;
    
    uint32_t model_ID           = (out[0] >>  4) & 0x0F;
    uint32_t extended_model_ID  = (out[0] >> 16) & 0x0F;
    
    uint32_t DisplayFamily = family_ID;
    if (family_ID == 0x0F)
      DisplayFamily += extended_family_ID;
    
    uint32_t DisplayModel = model_ID;
    if (family_ID == 0x06 || family_ID == 0x0F)
      DisplayModel += extended_model_ID << 4;

    uint32_t DisplayFamily_DisplayModel = (DisplayFamily << 8) + (DisplayModel << 0);

    // Data from Intel® 64 and IA-32 Architectures, Volume 4, Chapter 2, Table 2-1 (CPUID Signature Values of DisplayFamily_DisplayModel)
    if (DisplayFamily_DisplayModel == 0x067D) return CPU::CORE_ICE_LAKE;
    if (DisplayFamily_DisplayModel == 0x067E) return CPU::CORE_ICE_LAKE;
    if (DisplayFamily_DisplayModel == 0x068C) return CPU::CORE_TIGER_LAKE;
    if (DisplayFamily_DisplayModel == 0x06A5) return CPU::CORE_COMET_LAKE;
    if (DisplayFamily_DisplayModel == 0x06A6) return CPU::CORE_COMET_LAKE;
    if (DisplayFamily_DisplayModel == 0x0666) return CPU::CORE_CANNON_LAKE;
    if (DisplayFamily_DisplayModel == 0x068E) return CPU::CORE_KABY_LAKE;
    if (DisplayFamily_DisplayModel == 0x069E) return CPU::CORE_KABY_LAKE;
    if (DisplayFamily_DisplayModel == 0x066A) return CPU::XEON_ICE_LAKE;
    if (DisplayFamily_DisplayModel == 0x066C) return CPU::XEON_ICE_LAKE;
    if (DisplayFamily_DisplayModel == 0x0655) return CPU::XEON_SKY_LAKE;
    if (DisplayFamily_DisplayModel == 0x064E) return CPU::CORE_SKY_LAKE;
    if (DisplayFamily_DisplayModel == 0x065E) return CPU::CORE_SKY_LAKE;
    if (DisplayFamily_DisplayModel == 0x0656) return CPU::XEON_BROADWELL;
    if (DisplayFamily_DisplayModel == 0x064F) return CPU::XEON_BROADWELL;
    if (DisplayFamily_DisplayModel == 0x0647) return CPU::CORE_BROADWELL;
    if (DisplayFamily_DisplayModel == 0x063D) return CPU::CORE_BROADWELL;
    if (DisplayFamily_DisplayModel == 0x063F) return CPU::XEON_HASWELL;
    if (DisplayFamily_DisplayModel == 0x063C) return CPU::CORE_HASWELL;
    if (DisplayFamily_DisplayModel == 0x0645) return CPU::CORE_HASWELL;
    if (DisplayFamily_DisplayModel == 0x0646) return CPU::CORE_HASWELL;
    if (DisplayFamily_DisplayModel == 0x063E) return CPU::XEON_IVY_BRIDGE;
    if (DisplayFamily_DisplayModel == 0x063A) return CPU::CORE_IVY_BRIDGE;
    if (DisplayFamily_DisplayModel == 0x062D) return CPU::SANDY_BRIDGE;
    if (DisplayFamily_DisplayModel == 0x062F) return CPU::SANDY_BRIDGE;
    if (DisplayFamily_DisplayModel == 0x062A) return CPU::SANDY_BRIDGE;
    if (DisplayFamily_DisplayModel == 0x062E) return CPU::NEHALEM;
    if (DisplayFamily_DisplayModel == 0x0625) return CPU::NEHALEM;
    if (DisplayFamily_DisplayModel == 0x062C) return CPU::NEHALEM;
    if (DisplayFamily_DisplayModel == 0x061E) return CPU::NEHALEM;
    if (DisplayFamily_DisplayModel == 0x061F) return CPU::NEHALEM;
    if (DisplayFamily_DisplayModel == 0x061A) return CPU::NEHALEM;
    if (DisplayFamily_DisplayModel == 0x061D) return CPU::NEHALEM;
    if (DisplayFamily_DisplayModel == 0x0617) return CPU::CORE2;
    if (DisplayFamily_DisplayModel == 0x060F) return CPU::CORE2;
    if (DisplayFamily_DisplayModel == 0x060E) return CPU::CORE1;

    if (DisplayFamily_DisplayModel == 0x0685) return CPU::XEON_PHI_KNIGHTS_MILL;
    if (DisplayFamily_DisplayModel == 0x0657) return CPU::XEON_PHI_KNIGHTS_LANDING;
    
#elif defined(__ARM_NEON)
    return CPU::ARM;
#endif
    
    return CPU::UNKNOWN;
  }

  std::string stringOfCPUModel(CPU model)
  {
    switch (model) {
    case CPU::XEON_ICE_LAKE           : return "Xeon Ice Lake";
    case CPU::CORE_ICE_LAKE           : return "Core Ice Lake";
    case CPU::CORE_TIGER_LAKE         : return "Core Tiger Lake";
    case CPU::CORE_COMET_LAKE         : return "Core Comet Lake";
    case CPU::CORE_CANNON_LAKE        : return "Core Cannon Lake";
    case CPU::CORE_KABY_LAKE          : return "Core Kaby Lake";
    case CPU::XEON_SKY_LAKE           : return "Xeon Sky Lake";
    case CPU::CORE_SKY_LAKE           : return "Core Sky Lake";
    case CPU::XEON_PHI_KNIGHTS_MILL   : return "Xeon Phi Knights Mill";
    case CPU::XEON_PHI_KNIGHTS_LANDING: return "Xeon Phi Knights Landing";
    case CPU::XEON_BROADWELL          : return "Xeon Broadwell";
    case CPU::CORE_BROADWELL          : return "Core Broadwell";
    case CPU::XEON_HASWELL            : return "Xeon Haswell";
    case CPU::CORE_HASWELL            : return "Core Haswell";
    case CPU::XEON_IVY_BRIDGE         : return "Xeon Ivy Bridge";
    case CPU::CORE_IVY_BRIDGE         : return "Core Ivy Bridge";
    case CPU::SANDY_BRIDGE            : return "Sandy Bridge";
    case CPU::NEHALEM                 : return "Nehalem";
    case CPU::CORE2                   : return "Core2";
    case CPU::CORE1                   : return "Core";
    case CPU::ARM                     : return "ARM";
    case CPU::UNKNOWN                 : return "Unknown CPU";
    }
    return "Unknown CPU (error)";
  }

#if defined(__X86_ASM__)
  /* constants to access destination registers of CPUID instruction */
  static const int EAX = 0;
  static const int EBX = 1;
  static const int ECX = 2;
  static const int EDX = 3;

  /* cpuid[eax=1].ecx */
  static const int CPU_FEATURE_BIT_SSE3   = 1 << 0;
  static const int CPU_FEATURE_BIT_SSSE3  = 1 << 9;
  static const int CPU_FEATURE_BIT_FMA3   = 1 << 12;
  static const int CPU_FEATURE_BIT_SSE4_1 = 1 << 19;
  static const int CPU_FEATURE_BIT_SSE4_2 = 1 << 20;
  //static const int CPU_FEATURE_BIT_MOVBE  = 1 << 22;
  static const int CPU_FEATURE_BIT_POPCNT = 1 << 23;
  //static const int CPU_FEATURE_BIT_XSAVE  = 1 << 26;
  static const int CPU_FEATURE_BIT_OXSAVE = 1 << 27;
  static const int CPU_FEATURE_BIT_AVX    = 1 << 28;
  static const int CPU_FEATURE_BIT_F16C   = 1 << 29;
  static const int CPU_FEATURE_BIT_RDRAND = 1 << 30;

  /* cpuid[eax=1].edx */
  static const int CPU_FEATURE_BIT_SSE  = 1 << 25;
  static const int CPU_FEATURE_BIT_SSE2 = 1 << 26;

  /* cpuid[eax=0x80000001].ecx */
  static const int CPU_FEATURE_BIT_LZCNT = 1 << 5;

  /* cpuid[eax=7,ecx=0].ebx */
  static const int CPU_FEATURE_BIT_BMI1    = 1 << 3;
  static const int CPU_FEATURE_BIT_AVX2    = 1 << 5;
  static const int CPU_FEATURE_BIT_BMI2    = 1 << 8;
  static const int CPU_FEATURE_BIT_AVX512F = 1 << 16;     // AVX512F  (foundation)
  static const int CPU_FEATURE_BIT_AVX512DQ = 1 << 17;    // AVX512DQ (doubleword and quadword instructions)
  static const int CPU_FEATURE_BIT_AVX512PF = 1 << 26;    // AVX512PF (prefetch gather/scatter instructions)
  static const int CPU_FEATURE_BIT_AVX512ER = 1 << 27;    // AVX512ER (exponential and reciprocal instructions)
  static const int CPU_FEATURE_BIT_AVX512CD = 1 << 28;    // AVX512CD (conflict detection instructions)
  static const int CPU_FEATURE_BIT_AVX512BW = 1 << 30;    // AVX512BW (byte and word instructions)
  static const int CPU_FEATURE_BIT_AVX512VL = 1 << 31;    // AVX512VL (vector length extensions)
  static const int CPU_FEATURE_BIT_AVX512IFMA = 1 << 21;  // AVX512IFMA (integer fused multiple-add instructions)
  
  /* cpuid[eax=7,ecx=0].ecx */
  static const int CPU_FEATURE_BIT_AVX512VBMI = 1 << 1;   // AVX512VBMI (vector bit manipulation instructions)
#endif

#if defined(__X86_ASM__)
  __noinline int64_t get_xcr0() 
  {
#if defined (__WIN32__) && !defined (__MINGW32__) && defined(_XCR_XFEATURE_ENABLED_MASK)
    int64_t xcr0 = 0; // int64_t is workaround for compiler bug under VS2013, Win32
    xcr0 = _xgetbv(0);
    return xcr0;
#else
    int xcr0 = 0;
    __asm__ ("xgetbv" : "=a" (xcr0) : "c" (0) : "%edx" );
    return xcr0;
#endif
  }
#endif

  int getCPUFeatures()
  {
#if defined(__X86_ASM__)
    /* cache CPU features access */
    static int cpu_features = 0;
    if (cpu_features) 
      return cpu_features;

    /* get number of CPUID leaves */
    int cpuid_leaf0[4]; 
    __cpuid(cpuid_leaf0, 0x00000000);
    unsigned nIds = cpuid_leaf0[EAX];  

    /* get number of extended CPUID leaves */
    int cpuid_leafe[4]; 
    __cpuid(cpuid_leafe, 0x80000000);
    unsigned nExIds = cpuid_leafe[EAX];

    /* get CPUID leaves for EAX = 1,7, and 0x80000001 */
    int cpuid_leaf_1[4] = { 0,0,0,0 };
    int cpuid_leaf_7[4] = { 0,0,0,0 };
    int cpuid_leaf_e1[4] = { 0,0,0,0 };
    if (nIds >= 1) __cpuid (cpuid_leaf_1,0x00000001);
#if _WIN32
#if _MSC_VER && (_MSC_FULL_VER < 160040219)
#elif defined(_MSC_VER)
    if (nIds >= 7) __cpuidex(cpuid_leaf_7,0x00000007,0);
#endif
#else
    if (nIds >= 7) __cpuid_count(cpuid_leaf_7,0x00000007,0);
#endif
    if (nExIds >= 0x80000001) __cpuid(cpuid_leaf_e1,0x80000001);

    /* detect if OS saves XMM, YMM, and ZMM states */
    bool xmm_enabled = true;
    bool ymm_enabled = false;
    bool zmm_enabled = false;
    if (cpuid_leaf_1[ECX] & CPU_FEATURE_BIT_OXSAVE) {
      int64_t xcr0 = get_xcr0();
      xmm_enabled = ((xcr0 & 0x02) == 0x02);                /* checks if xmm are enabled in XCR0 */
      ymm_enabled = xmm_enabled && ((xcr0 & 0x04) == 0x04); /* checks if ymm state are enabled in XCR0 */
      zmm_enabled = ymm_enabled && ((xcr0 & 0xE0) == 0xE0); /* checks if OPMASK state, upper 256-bit of ZMM0-ZMM15 and ZMM16-ZMM31 state are enabled in XCR0 */
    }
    if (xmm_enabled) cpu_features |= CPU_FEATURE_XMM_ENABLED;
    if (ymm_enabled) cpu_features |= CPU_FEATURE_YMM_ENABLED;
    if (zmm_enabled) cpu_features |= CPU_FEATURE_ZMM_ENABLED;
    
    if (cpuid_leaf_1[EDX] & CPU_FEATURE_BIT_SSE   ) cpu_features |= CPU_FEATURE_SSE;
    if (cpuid_leaf_1[EDX] & CPU_FEATURE_BIT_SSE2  ) cpu_features |= CPU_FEATURE_SSE2;
    if (cpuid_leaf_1[ECX] & CPU_FEATURE_BIT_SSE3  ) cpu_features |= CPU_FEATURE_SSE3;
    if (cpuid_leaf_1[ECX] & CPU_FEATURE_BIT_SSSE3 ) cpu_features |= CPU_FEATURE_SSSE3;
    if (cpuid_leaf_1[ECX] & CPU_FEATURE_BIT_SSE4_1) cpu_features |= CPU_FEATURE_SSE41;
    if (cpuid_leaf_1[ECX] & CPU_FEATURE_BIT_SSE4_2) cpu_features |= CPU_FEATURE_SSE42;
    if (cpuid_leaf_1[ECX] & CPU_FEATURE_BIT_POPCNT) cpu_features |= CPU_FEATURE_POPCNT;
    
    if (cpuid_leaf_1[ECX] & CPU_FEATURE_BIT_AVX   ) cpu_features |= CPU_FEATURE_AVX;
    if (cpuid_leaf_1[ECX] & CPU_FEATURE_BIT_F16C  ) cpu_features |= CPU_FEATURE_F16C;
    if (cpuid_leaf_1[ECX] & CPU_FEATURE_BIT_RDRAND) cpu_features |= CPU_FEATURE_RDRAND;
    if (cpuid_leaf_7[EBX] & CPU_FEATURE_BIT_AVX2  ) cpu_features |= CPU_FEATURE_AVX2;
    if (cpuid_leaf_1[ECX] & CPU_FEATURE_BIT_FMA3  ) cpu_features |= CPU_FEATURE_FMA3;
    if (cpuid_leaf_e1[ECX] & CPU_FEATURE_BIT_LZCNT) cpu_features |= CPU_FEATURE_LZCNT;
    if (cpuid_leaf_7 [EBX] & CPU_FEATURE_BIT_BMI1 ) cpu_features |= CPU_FEATURE_BMI1;
    if (cpuid_leaf_7 [EBX] & CPU_FEATURE_BIT_BMI2 ) cpu_features |= CPU_FEATURE_BMI2;

    if (cpuid_leaf_7[EBX] & CPU_FEATURE_BIT_AVX512F   ) cpu_features |= CPU_FEATURE_AVX512F;
    if (cpuid_leaf_7[EBX] & CPU_FEATURE_BIT_AVX512DQ  ) cpu_features |= CPU_FEATURE_AVX512DQ;
    if (cpuid_leaf_7[EBX] & CPU_FEATURE_BIT_AVX512PF  ) cpu_features |= CPU_FEATURE_AVX512PF;
    if (cpuid_leaf_7[EBX] & CPU_FEATURE_BIT_AVX512ER  ) cpu_features |= CPU_FEATURE_AVX512ER; 
    if (cpuid_leaf_7[EBX] & CPU_FEATURE_BIT_AVX512CD  ) cpu_features |= CPU_FEATURE_AVX512CD;
    if (cpuid_leaf_7[EBX] & CPU_FEATURE_BIT_AVX512BW  ) cpu_features |= CPU_FEATURE_AVX512BW;
    if (cpuid_leaf_7[EBX] & CPU_FEATURE_BIT_AVX512IFMA) cpu_features |= CPU_FEATURE_AVX512IFMA;
    if (cpuid_leaf_7[EBX] & CPU_FEATURE_BIT_AVX512VL  ) cpu_features |= CPU_FEATURE_AVX512VL;
    if (cpuid_leaf_7[ECX] & CPU_FEATURE_BIT_AVX512VBMI) cpu_features |= CPU_FEATURE_AVX512VBMI;

#if defined(__MACOSX__)
    if (   (cpu_features & CPU_FEATURE_AVX512F)
        || (cpu_features & CPU_FEATURE_AVX512DQ)
        || (cpu_features & CPU_FEATURE_AVX512CD)
        || (cpu_features & CPU_FEATURE_AVX512BW)
        || (cpu_features & CPU_FEATURE_AVX512VL) )
      {
        // on macOS AVX512 will be enabled automatically by the kernel when the first AVX512 instruction is called
        // see https://github.com/apple/darwin-xnu/blob/0a798f6738bc1db01281fc08ae024145e84df927/osfmk/i386/fpu.c#L176
        // therefore we ignore the state of XCR0
        cpu_features |= CPU_FEATURE_ZMM_ENABLED;
      }
#endif
    return cpu_features;

#elif defined(__ARM_NEON) || defined(__EMSCRIPTEN__)

    int cpu_features = CPU_FEATURE_NEON|CPU_FEATURE_SSE|CPU_FEATURE_SSE2;
    cpu_features |= CPU_FEATURE_SSE3|CPU_FEATURE_SSSE3|CPU_FEATURE_SSE42;
    cpu_features |= CPU_FEATURE_XMM_ENABLED;
    cpu_features |= CPU_FEATURE_YMM_ENABLED;
    cpu_features |= CPU_FEATURE_SSE41 | CPU_FEATURE_RDRAND | CPU_FEATURE_F16C;
    cpu_features |= CPU_FEATURE_POPCNT;
    cpu_features |= CPU_FEATURE_AVX;
    cpu_features |= CPU_FEATURE_AVX2;
    cpu_features |= CPU_FEATURE_FMA3;
    cpu_features |= CPU_FEATURE_LZCNT;
    cpu_features |= CPU_FEATURE_BMI1;
    cpu_features |= CPU_FEATURE_BMI2;
    cpu_features |= CPU_FEATURE_NEON_2X;
    return cpu_features;

#else
    /* Unknown CPU. */
    return 0;
#endif
  }

  std::string stringOfCPUFeatures(int features)
  {
    std::string str;
    if (features & CPU_FEATURE_XMM_ENABLED) str += "XMM ";
    if (features & CPU_FEATURE_YMM_ENABLED) str += "YMM ";
    if (features & CPU_FEATURE_ZMM_ENABLED) str += "ZMM ";
    if (features & CPU_FEATURE_SSE   ) str += "SSE ";
    if (features & CPU_FEATURE_SSE2  ) str += "SSE2 ";
    if (features & CPU_FEATURE_SSE3  ) str += "SSE3 ";
    if (features & CPU_FEATURE_SSSE3 ) str += "SSSE3 ";
    if (features & CPU_FEATURE_SSE41 ) str += "SSE4.1 ";
    if (features & CPU_FEATURE_SSE42 ) str += "SSE4.2 ";
    if (features & CPU_FEATURE_POPCNT) str += "POPCNT ";
    if (features & CPU_FEATURE_AVX   ) str += "AVX ";
    if (features & CPU_FEATURE_F16C  ) str += "F16C ";
    if (features & CPU_FEATURE_RDRAND) str += "RDRAND ";
    if (features & CPU_FEATURE_AVX2  ) str += "AVX2 ";
    if (features & CPU_FEATURE_FMA3  ) str += "FMA3 ";
    if (features & CPU_FEATURE_LZCNT ) str += "LZCNT ";
    if (features & CPU_FEATURE_BMI1  ) str += "BMI1 ";
    if (features & CPU_FEATURE_BMI2  ) str += "BMI2 ";
    if (features & CPU_FEATURE_AVX512F) str += "AVX512F ";
    if (features & CPU_FEATURE_AVX512DQ) str += "AVX512DQ ";
    if (features & CPU_FEATURE_AVX512PF) str += "AVX512PF ";
    if (features & CPU_FEATURE_AVX512ER) str += "AVX512ER ";
    if (features & CPU_FEATURE_AVX512CD) str += "AVX512CD ";
    if (features & CPU_FEATURE_AVX512BW) str += "AVX512BW ";
    if (features & CPU_FEATURE_AVX512VL) str += "AVX512VL ";
    if (features & CPU_FEATURE_AVX512IFMA) str += "AVX512IFMA ";
    if (features & CPU_FEATURE_AVX512VBMI) str += "AVX512VBMI ";
    if (features & CPU_FEATURE_NEON) str += "NEON ";
    if (features & CPU_FEATURE_NEON_2X) str += "2xNEON ";
    return str;
  }
  
  std::string stringOfISA (int isa)
  {
    if (isa == SSE) return "SSE";
    if (isa == SSE2) return "SSE2";
    if (isa == SSE3) return "SSE3";
    if (isa == SSSE3) return "SSSE3";
    if (isa == SSE41) return "SSE4.1";
    if (isa == SSE42) return "SSE4.2";
    if (isa == AVX) return "AVX";
    if (isa == AVX2) return "AVX2";
    if (isa == AVX512) return "AVX512";

    if (isa == NEON) return "NEON";
    if (isa == NEON_2X) return "2xNEON";
    return "UNKNOWN";
  }

  bool hasISA(int features, int isa) {
    return (features & isa) == isa;
  }
  
  std::string supportedTargetList (int features)
  {
    std::string v;
    if (hasISA(features,SSE)) v += "SSE ";
    if (hasISA(features,SSE2)) v += "SSE2 ";
    if (hasISA(features,SSE3)) v += "SSE3 ";
    if (hasISA(features,SSSE3)) v += "SSSE3 ";
    if (hasISA(features,SSE41)) v += "SSE4.1 ";
    if (hasISA(features,SSE42)) v += "SSE4.2 ";
    if (hasISA(features,AVX)) v += "AVX ";
    if (hasISA(features,AVXI)) v += "AVXI ";
    if (hasISA(features,AVX2)) v += "AVX2 ";
    if (hasISA(features,AVX512)) v += "AVX512 ";

    if (hasISA(features,NEON)) v += "NEON ";
    if (hasISA(features,NEON_2X)) v += "2xNEON ";
    return v;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// Windows Platform
////////////////////////////////////////////////////////////////////////////////

#if defined(__WIN32__)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

namespace embree
{
  std::string getExecutableFileName() {
    char filename[1024];
    if (!GetModuleFileName(nullptr, filename, sizeof(filename)))
      return std::string();
    return std::string(filename);
  }

  unsigned int getNumberOfLogicalThreads() 
  {
    static int nThreads = -1;
    if (nThreads != -1) return nThreads;

    typedef WORD (WINAPI *GetActiveProcessorGroupCountFunc)();
    typedef DWORD (WINAPI *GetActiveProcessorCountFunc)(WORD);
    HMODULE hlib = LoadLibrary("Kernel32");
    GetActiveProcessorGroupCountFunc pGetActiveProcessorGroupCount = (GetActiveProcessorGroupCountFunc)GetProcAddress(hlib, "GetActiveProcessorGroupCount");
    GetActiveProcessorCountFunc      pGetActiveProcessorCount      = (GetActiveProcessorCountFunc)     GetProcAddress(hlib, "GetActiveProcessorCount");

    if (pGetActiveProcessorGroupCount && pGetActiveProcessorCount) 
    {
      int groups = pGetActiveProcessorGroupCount();
      int totalProcessors = 0;
      for (int i = 0; i < groups; i++) 
        totalProcessors += pGetActiveProcessorCount(i);
      nThreads = totalProcessors;
    }
    else
    {
      SYSTEM_INFO sysinfo;
      GetSystemInfo(&sysinfo);
      nThreads = sysinfo.dwNumberOfProcessors;
    }
    assert(nThreads);
    return nThreads;
  }

  int getTerminalWidth() 
  {
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE) return 80;
    CONSOLE_SCREEN_BUFFER_INFO info;
    memset(&info,0,sizeof(info));
    GetConsoleScreenBufferInfo(handle, &info);
    return info.dwSize.X;
  }

  double getSeconds() 
  {
    LARGE_INTEGER freq, val;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&val);
    return (double)val.QuadPart / (double)freq.QuadPart;
  }

  void sleepSeconds(double t) {
    Sleep(DWORD(1000.0*t));
  }

  size_t getVirtualMemoryBytes()
  {
    PROCESS_MEMORY_COUNTERS info;
    GetProcessMemoryInfo( GetCurrentProcess( ), &info, sizeof(info) );
    return (size_t)info.QuotaPeakPagedPoolUsage;
  }

  size_t getResidentMemoryBytes()
  {
    PROCESS_MEMORY_COUNTERS info;
    GetProcessMemoryInfo( GetCurrentProcess( ), &info, sizeof(info) );
    return (size_t)info.WorkingSetSize;
  }
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// Linux Platform
////////////////////////////////////////////////////////////////////////////////

#if defined(__LINUX__)

#include <stdio.h>
#include <unistd.h>

namespace embree
{
  std::string getExecutableFileName() 
  {
    std::string pid = "/proc/" + toString(getpid()) + "/exe";
    char buf[4096];
    memset(buf,0,sizeof(buf));
    if (readlink(pid.c_str(), buf, sizeof(buf)-1) == -1)
      return std::string();
    return std::string(buf);
  }

  size_t getVirtualMemoryBytes()
  {
    size_t virt, resident, shared;
    std::ifstream buffer("/proc/self/statm");
    buffer >> virt >> resident >> shared;
    return virt*sysconf(_SC_PAGE_SIZE);
  }

  size_t getResidentMemoryBytes()
  {
    size_t virt, resident, shared;
    std::ifstream buffer("/proc/self/statm");
    buffer >> virt >> resident >> shared;
    return resident*sysconf(_SC_PAGE_SIZE);
  }
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// FreeBSD Platform
////////////////////////////////////////////////////////////////////////////////

#if defined (__FreeBSD__)

#include <sys/sysctl.h>

namespace embree
{
  std::string getExecutableFileName()
  {
    const int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
    char buf[4096];
    memset(buf,0,sizeof(buf));
    size_t len = sizeof(buf)-1;
    if (sysctl(mib, 4, buf, &len, 0x0, 0) == -1)
      return std::string();
    return std::string(buf);
  }

  size_t getVirtualMemoryBytes() {
    return 0;
  }
   
  size_t getResidentMemoryBytes() {
    return 0;
  }
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// Mac OS X Platform
////////////////////////////////////////////////////////////////////////////////

#if defined(__MACOSX__)

#include <mach-o/dyld.h>

namespace embree
{
  std::string getExecutableFileName()
  {
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0)
      return std::string();
    return std::string(buf);
  }

  size_t getVirtualMemoryBytes() {
    return 0;
  }
   
  size_t getResidentMemoryBytes() {
    return 0;
  }
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// Unix Platform
////////////////////////////////////////////////////////////////////////////////

#if defined(__UNIX__)

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <pthread.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>

// -- GODOT start --
extern "C" {
extern int godot_js_os_hw_concurrency_get();
}
// -- GODOT end --
#endif

namespace embree
{
  unsigned int getNumberOfLogicalThreads() 
  {
    static int nThreads = -1;
    if (nThreads != -1) return nThreads;

#if defined(__MACOSX__) || defined(__ANDROID__)
    nThreads = sysconf(_SC_NPROCESSORS_ONLN); // does not work in Linux LXC container
    assert(nThreads);
#elif defined(__EMSCRIPTEN__)
    // -- GODOT start --
    nThreads = godot_js_os_hw_concurrency_get();
    // -- GODOT end --
#else
    cpu_set_t set;
    if (pthread_getaffinity_np(pthread_self(), sizeof(set), &set) == 0)
      nThreads = CPU_COUNT(&set);
#endif
    
    assert(nThreads);
    return nThreads;
  }

  int getTerminalWidth() 
  {
    struct winsize info;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &info) < 0) return 80;
    return info.ws_col;
  }

  double getSeconds() {
    struct timeval tp; gettimeofday(&tp,nullptr);
    return double(tp.tv_sec) + double(tp.tv_usec)/1E6;
  }

  void sleepSeconds(double t) {
    usleep(1000000.0*t);
  }
}
#endif

#if defined(__INTEL_LLVM_COMPILER)
#pragma clang diagnostic pop
#endif
