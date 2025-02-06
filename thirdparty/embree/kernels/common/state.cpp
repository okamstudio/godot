// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "state.h"
#include "../../common/lexers/streamfilters.h"

namespace embree
{
  MutexSys g_printMutex;

  State::ErrorHandler State::g_errorHandler;

  State::ErrorHandler::ErrorHandler()
    : thread_error(createTls()) {}

  State::ErrorHandler::~ErrorHandler()
  {
    Lock<MutexSys> lock(errors_mutex);
    for (size_t i=0; i<thread_errors.size(); i++) {
      delete thread_errors[i];
    }
    destroyTls(thread_error);
    thread_errors.clear();
  }

  RTCErrorMessage* State::ErrorHandler::error()
  {
    RTCErrorMessage* stored_error = (RTCErrorMessage*) getTls(thread_error);
    if (stored_error) {
      return stored_error;
    }

    Lock<MutexSys> lock(errors_mutex);
    stored_error = new RTCErrorMessage(RTC_ERROR_NONE, "");
    thread_errors.push_back(stored_error);
    setTls(thread_error,stored_error);
    return stored_error;
  }

  State::State () 
    : enabled_cpu_features(getCPUFeatures()),
      enabled_builder_cpu_features(enabled_cpu_features),
      frequency_level(FREQUENCY_SIMD256)
  {
    tri_accel = "default";
    tri_builder = "default";
    tri_traverser = "default";
    
    tri_accel_mb = "default";
    tri_builder_mb = "default";
    tri_traverser_mb = "default";

    quad_accel = "default";
    quad_builder = "default";
    quad_traverser = "default";

    quad_accel_mb = "default";
    quad_builder_mb = "default";
    quad_traverser_mb = "default";

    line_accel = "default";
    line_builder = "default";
    line_traverser = "default";

    line_accel_mb = "default";
    line_builder_mb = "default";
    line_traverser_mb = "default";
    
    hair_accel = "default";
    hair_builder = "default";
    hair_traverser = "default";

    hair_accel_mb = "default";
    hair_builder_mb = "default";
    hair_traverser_mb = "default";

    object_accel = "default";
    object_builder = "default";
    object_accel_min_leaf_size = 1;
    object_accel_max_leaf_size = 1;

    object_accel_mb = "default";
    object_builder_mb = "default";
    object_accel_mb_min_leaf_size = 1;
    object_accel_mb_max_leaf_size = 1;

    max_spatial_split_replications = 1.2f;
    useSpatialPreSplits = false;

    max_triangles_per_leaf = inf;

    tessellation_cache_size = 128*1024*1024;

    subdiv_accel = "default";
    subdiv_accel_mb = "default";

    grid_accel = "default";
    grid_builder = "default";
    grid_accel_mb = "default";
    grid_builder_mb = "default";

    instancing_open_min = 0;
    instancing_block_size = 0;
    instancing_open_factor = 8.0f; 
    instancing_open_max_depth = 32;
    instancing_open_max = 50000000;

    float_exceptions = false;
    quality_flags = -1;
    scene_flags = -1;
    verbose = 0;
    benchmark = 0;

    numThreads = 0;
    numUserThreads = 0;

#if TASKING_INTERNAL
    set_affinity = true;
#else
    set_affinity = false;
#endif

    start_threads = false;
    enable_selockmemoryprivilege = false;
#if defined(__LINUX__)
    hugepages = true;
#else
    hugepages = false;
#endif
    hugepages_success = true;

    alloc_main_block_size = 0;
    alloc_num_main_slots = 0;
    alloc_thread_block_size = 0;
    alloc_single_thread_alloc = -1;

    error_function = nullptr;
    error_function_userptr = nullptr;

    memory_monitor_function = nullptr;
    memory_monitor_userptr = nullptr;
  }

  State::~State() {
  }

  bool State::hasISA(const int isa) {
    return (enabled_cpu_features & isa) == isa;
  }

  bool State::checkISASupport() {
#if defined(__ARM_NEON)
    /*
     * NEON CPU type is a mixture of NEON and SSE2
     */

    bool hasSSE2 = (getCPUFeatures() & enabled_cpu_features) & CPU_FEATURE_SSE2;

    /* this will be true when explicitly initialize Device with `isa=neon` config */
    bool hasNEON = (getCPUFeatures() & enabled_cpu_features) & CPU_FEATURE_NEON;

    return hasSSE2 || hasNEON;
#else
    return (getCPUFeatures() & enabled_cpu_features) == enabled_cpu_features;
#endif
  }
  
  void State::verify()
  {
    /* verify that calculations stay in range */
    assert(rcp(min_rcp_input)*FLT_LARGE+FLT_LARGE < 0.01f*FLT_MAX);

    /* here we verify that CPP files compiled for a specific ISA only
     * call that same or lower ISA version of non-inlined class member
     * functions */
#if defined(DEBUG)
#if defined(EMBREE_TARGET_SSE2)
#if !defined(__ARM_NEON)
    assert(sse2::getISA() <= SSE2);
#endif
#endif
#if defined(EMBREE_TARGET_SSE42)
    assert(sse42::getISA() <= SSE42);
#endif
#if defined(EMBREE_TARGET_AVX)
    assert(avx::getISA() <= AVX);
#endif
#if defined(EMBREE_TARGET_AVX2)
    assert(avx2::getISA() <= AVX2);
#endif
#if defined (EMBREE_TARGET_AVX512)
    assert(avx512::getISA() <= AVX512);
#endif
#endif
  }

  const char* symbols[3] = { "=", ",", "|" };

  bool State::parseFile(const FileName& fileName)
  { 
    Ref<Stream<int> > file;
    // -- GODOT start --
    // try {
      file = new FileStream(fileName);
    // }
    // catch (std::runtime_error& e) {
    //   (void) e;
    //   return false;
    // }
    // -- GODOT end --
    
    std::vector<std::string> syms;
    for (size_t i=0; i<sizeof(symbols)/sizeof(void*); i++) 
      syms.push_back(symbols[i]);
    
    Ref<TokenStream> cin = new TokenStream(new LineCommentFilter(file,"#"),
                                           TokenStream::alpha+TokenStream::ALPHA+TokenStream::numbers+"_.",
                                           TokenStream::separators,syms);
    parse(cin);
    return true;
  }

  void State::parseString(const char* cfg)
  {
    if (cfg == nullptr) return;

    std::vector<std::string> syms;
    for (size_t i=0; i<sizeof(symbols)/sizeof(void*); i++) 
      syms.push_back(symbols[i]);
    
    Ref<TokenStream> cin = new TokenStream(new StrStream(cfg),
                                           TokenStream::alpha+TokenStream::ALPHA+TokenStream::numbers+"_.",
                                           TokenStream::separators,syms);
    parse(cin);
  }
  
  int string_to_cpufeatures(const std::string& isa)
  {
    if      (isa == "sse" ) return SSE;
    else if (isa == "sse2") return SSE2;
    else if (isa == "sse3") return SSE3;
    else if (isa == "ssse3") return SSSE3;
    else if (isa == "sse41") return SSE41;
    else if (isa == "sse4.1") return SSE41;
    else if (isa == "sse42") return SSE42;
    else if (isa == "sse4.2") return SSE42;
    else if (isa == "avx") return AVX;
    else if (isa == "avxi") return AVXI;
    else if (isa == "avx2") return AVX2;
    else if (isa == "avx512") return AVX512;
    else return SSE2;
  }

  void State::parse(Ref<TokenStream> cin)
  {
    /* parse until end of stream */
    while (cin->peek() != Token::Eof())
    {
      const Token tok = cin->get();

      if (tok == Token::Id("threads") && cin->trySymbol("=")) 
        numThreads = cin->get().Int();

      else if (tok == Token::Id("user_threads")&& cin->trySymbol("=")) 
        numUserThreads = cin->get().Int();

      else if (tok == Token::Id("set_affinity")&& cin->trySymbol("=")) 
        set_affinity = cin->get().Int();

      else if (tok == Token::Id("affinity")&& cin->trySymbol("=")) 
        set_affinity = cin->get().Int();
      
      else if (tok == Token::Id("start_threads")&& cin->trySymbol("=")) 
        start_threads = cin->get().Int();
      
      else if (tok == Token::Id("isa") && cin->trySymbol("=")) {
        std::string isa_str = toLowerCase(cin->get().Identifier());
        enabled_cpu_features = string_to_cpufeatures(isa_str);
        enabled_builder_cpu_features = enabled_cpu_features;
      }

      else if (tok == Token::Id("max_isa") && cin->trySymbol("=")) {
        std::string isa_str = toLowerCase(cin->get().Identifier());
        enabled_cpu_features &= string_to_cpufeatures(isa_str);
        enabled_builder_cpu_features &= enabled_cpu_features;
      }

      else if (tok == Token::Id("max_builder_isa") && cin->trySymbol("=")) {
        std::string isa_str = toLowerCase(cin->get().Identifier());
        enabled_builder_cpu_features &= string_to_cpufeatures(isa_str);
      }

      else if (tok == Token::Id("frequency_level") && cin->trySymbol("=")) {
        std::string freq = cin->get().Identifier();
        if      (freq == "simd128") frequency_level = FREQUENCY_SIMD128;
        else if (freq == "simd256") frequency_level = FREQUENCY_SIMD256;
        else if (freq == "simd512") frequency_level = FREQUENCY_SIMD512;
      }

      else if (tok == Token::Id("enable_selockmemoryprivilege") && cin->trySymbol("=")) {
        enable_selockmemoryprivilege = cin->get().Int();
      }
      else if (tok == Token::Id("hugepages") && cin->trySymbol("=")) {
        hugepages = cin->get().Int();
      }

      else if (tok == Token::Id("float_exceptions") && cin->trySymbol("=")) 
        float_exceptions = cin->get().Int();

      else if ((tok == Token::Id("tri_accel") || tok == Token::Id("accel")) && cin->trySymbol("="))
        tri_accel = cin->get().Identifier();
      else if ((tok == Token::Id("tri_builder") || tok == Token::Id("builder")) && cin->trySymbol("="))
        tri_builder = cin->get().Identifier();
      else if ((tok == Token::Id("tri_traverser") || tok == Token::Id("traverser")) && cin->trySymbol("="))
        tri_traverser = cin->get().Identifier();
     
      else if ((tok == Token::Id("tri_accel_mb") || tok == Token::Id("accel_mb")) && cin->trySymbol("="))
        tri_accel_mb = cin->get().Identifier();
      else if ((tok == Token::Id("tri_builder_mb") || tok == Token::Id("builder_mb")) && cin->trySymbol("="))
        tri_builder_mb = cin->get().Identifier();
      else if ((tok == Token::Id("tri_traverser_mb") || tok == Token::Id("traverser_mb")) && cin->trySymbol("="))
        tri_traverser_mb = cin->get().Identifier();

      else if ((tok == Token::Id("quad_accel")) && cin->trySymbol("="))
        quad_accel = cin->get().Identifier();
      else if ((tok == Token::Id("quad_builder")) && cin->trySymbol("="))
        quad_builder = cin->get().Identifier();
      else if ((tok == Token::Id("quad_traverser")) && cin->trySymbol("="))
        quad_traverser = cin->get().Identifier();

      else if ((tok == Token::Id("quad_accel_mb")) && cin->trySymbol("="))
        quad_accel_mb = cin->get().Identifier();
      else if ((tok == Token::Id("quad_builder_mb")) && cin->trySymbol("="))
        quad_builder_mb = cin->get().Identifier();
      else if ((tok == Token::Id("quad_traverser_mb")) && cin->trySymbol("="))
        quad_traverser_mb = cin->get().Identifier();

      else if ((tok == Token::Id("line_accel")) && cin->trySymbol("="))
        line_accel = cin->get().Identifier();
      else if ((tok == Token::Id("line_builder")) && cin->trySymbol("="))
        line_builder = cin->get().Identifier();
      else if ((tok == Token::Id("line_traverser")) && cin->trySymbol("="))
        line_traverser = cin->get().Identifier();

      else if ((tok == Token::Id("line_accel_mb")) && cin->trySymbol("="))
        line_accel_mb = cin->get().Identifier();
      else if ((tok == Token::Id("line_builder_mb")) && cin->trySymbol("="))
        line_builder_mb = cin->get().Identifier();
      else if ((tok == Token::Id("line_traverser_mb")) && cin->trySymbol("="))
        line_traverser_mb = cin->get().Identifier();
      
      else if (tok == Token::Id("hair_accel") && cin->trySymbol("="))
        hair_accel = cin->get().Identifier();
      else if (tok == Token::Id("hair_builder") && cin->trySymbol("="))
        hair_builder = cin->get().Identifier();
      else if (tok == Token::Id("hair_traverser") && cin->trySymbol("="))
        hair_traverser = cin->get().Identifier();

      else if (tok == Token::Id("hair_accel_mb") && cin->trySymbol("="))
        hair_accel_mb = cin->get().Identifier();
      else if (tok == Token::Id("hair_builder_mb") && cin->trySymbol("="))
        hair_builder_mb = cin->get().Identifier();
      else if (tok == Token::Id("hair_traverser_mb") && cin->trySymbol("="))
        hair_traverser_mb = cin->get().Identifier();

      else if (tok == Token::Id("object_accel") && cin->trySymbol("="))
        object_accel = cin->get().Identifier();
      else if (tok == Token::Id("object_builder") && cin->trySymbol("="))
        object_builder = cin->get().Identifier();
      else if (tok == Token::Id("object_accel_min_leaf_size") && cin->trySymbol("="))
        object_accel_min_leaf_size = cin->get().Int();
      else if (tok == Token::Id("object_accel_max_leaf_size") && cin->trySymbol("="))
        object_accel_max_leaf_size = cin->get().Int();

      else if (tok == Token::Id("object_accel_mb") && cin->trySymbol("="))
        object_accel_mb = cin->get().Identifier();
      else if (tok == Token::Id("object_builder_mb") && cin->trySymbol("="))
        object_builder_mb = cin->get().Identifier();
      else if (tok == Token::Id("object_accel_mb_min_leaf_size") && cin->trySymbol("="))
        object_accel_mb_min_leaf_size = cin->get().Int();
      else if (tok == Token::Id("object_accel_mb_max_leaf_size") && cin->trySymbol("="))
        object_accel_mb_max_leaf_size = cin->get().Int();

      else if (tok == Token::Id("instancing_open_min") && cin->trySymbol("="))
        instancing_open_min = cin->get().Int();
      else if (tok == Token::Id("instancing_block_size") && cin->trySymbol("=")) {
        instancing_block_size = cin->get().Int();
        instancing_open_factor = 0.0f;
      }
      else if (tok == Token::Id("instancing_open_max_depth") && cin->trySymbol("="))
        instancing_open_max_depth = cin->get().Int();
      else if (tok == Token::Id("instancing_open_factor") && cin->trySymbol("=")) {
        instancing_block_size = 0;
        instancing_open_factor = cin->get().Float();
      }
      else if (tok == Token::Id("instancing_open_max") && cin->trySymbol("="))
        instancing_open_max = cin->get().Int();

      else if (tok == Token::Id("subdiv_accel") && cin->trySymbol("="))
        subdiv_accel = cin->get().Identifier();
      else if (tok == Token::Id("subdiv_accel_mb") && cin->trySymbol("="))
        subdiv_accel_mb = cin->get().Identifier();

      else if (tok == Token::Id("grid_accel") && cin->trySymbol("="))
        grid_accel = cin->get().Identifier();
      else if (tok == Token::Id("grid_accel_mb") && cin->trySymbol("="))
        grid_accel_mb = cin->get().Identifier();

      else if (tok == Token::Id("verbose") && cin->trySymbol("="))
        verbose = cin->get().Int();
      else if (tok == Token::Id("benchmark") && cin->trySymbol("="))
        benchmark = cin->get().Int();
      
      else if (tok == Token::Id("quality")) {
        if (cin->trySymbol("=")) {
          Token flag = cin->get();
          if      (flag == Token::Id("low"))    quality_flags = RTC_BUILD_QUALITY_LOW;
          else if (flag == Token::Id("medium")) quality_flags = RTC_BUILD_QUALITY_MEDIUM;
          else if (flag == Token::Id("high"))   quality_flags = RTC_BUILD_QUALITY_HIGH;
        }
      }

      else if (tok == Token::Id("scene_flags")) {
        scene_flags = 0;
        if (cin->trySymbol("=")) {
          do {
            Token flag = cin->get();
            if (flag == Token::Id("dynamic") ) scene_flags |= RTC_SCENE_FLAG_DYNAMIC;
            else if (flag == Token::Id("compact")) scene_flags |= RTC_SCENE_FLAG_COMPACT;
            else if (flag == Token::Id("robust")) scene_flags |= RTC_SCENE_FLAG_ROBUST;
          } while (cin->trySymbol("|"));
        }
      }

      else if (tok == Token::Id("max_spatial_split_replications") && cin->trySymbol("="))
        max_spatial_split_replications = cin->get().Float();

      else if (tok == Token::Id("max_triangles_per_leaf") && cin->trySymbol("="))
        max_triangles_per_leaf = cin->get().Float();

      else if (tok == Token::Id("presplits") && cin->trySymbol("="))
        useSpatialPreSplits = cin->get().Int() != 0 ? true : false;

      else if (tok == Token::Id("tessellation_cache_size") && cin->trySymbol("="))
        tessellation_cache_size = size_t(cin->get().Float()*1024.0f*1024.0f);
      else if (tok == Token::Id("cache_size") && cin->trySymbol("="))
        tessellation_cache_size = size_t(cin->get().Float()*1024.0f*1024.0f);

      else if (tok == Token::Id("alloc_main_block_size") && cin->trySymbol("="))
        alloc_main_block_size = cin->get().Int();
       else if (tok == Token::Id("alloc_num_main_slots") && cin->trySymbol("="))
        alloc_num_main_slots = cin->get().Int();
       else if (tok == Token::Id("alloc_thread_block_size") && cin->trySymbol("="))
         alloc_thread_block_size = cin->get().Int();
       else if (tok == Token::Id("alloc_single_thread_alloc") && cin->trySymbol("="))
         alloc_single_thread_alloc = cin->get().Int();

      cin->trySymbol(","); // optional , separator
    }
  }

  bool State::verbosity(size_t N) {
    return N <= verbose;
  }

  void State::print()
  {
    std::cout << "general:" << std::endl;
    std::cout << "  build threads      = " << numThreads   << std::endl;
    std::cout << "  build user threads = " << numUserThreads   << std::endl;
    std::cout << "  start_threads      = " << start_threads << std::endl;
    std::cout << "  affinity           = " << set_affinity << std::endl;
    std::cout << "  frequency_level    = ";
    switch (frequency_level) {
    case FREQUENCY_SIMD128: std::cout << "simd128" << std::endl; break;
    case FREQUENCY_SIMD256: std::cout << "simd256" << std::endl; break;
    case FREQUENCY_SIMD512: std::cout << "simd512" << std::endl; break;
    default: std::cout << "error" << std::endl; break;
    }
    
    std::cout << "  hugepages          = ";
    if (!hugepages) std::cout << "disabled" << std::endl;
    else if (hugepages_success) std::cout << "enabled" << std::endl;
    else std::cout << "failed" << std::endl;

    std::cout << "  verbosity          = " << verbose << std::endl;
    std::cout << "  cache_size         = " << float(tessellation_cache_size)*1E-6 << " MB" << std::endl;
    std::cout << "  max_spatial_split_replications = " << max_spatial_split_replications << std::endl;
    
    std::cout << "triangles:" << std::endl;
    std::cout << "  accel              = " << tri_accel << std::endl;
    std::cout << "  builder            = " << tri_builder << std::endl;
    std::cout << "  traverser          = " << tri_traverser << std::endl;
        
    std::cout << "motion blur triangles:" << std::endl;
    std::cout << "  accel              = " << tri_accel_mb << std::endl;
    std::cout << "  builder            = " << tri_builder_mb << std::endl;
    std::cout << "  traverser          = " << tri_traverser_mb << std::endl;

    std::cout << "quads:" << std::endl;
    std::cout << "  accel              = " << quad_accel << std::endl;
    std::cout << "  builder            = " << quad_builder << std::endl;
    std::cout << "  traverser          = " << quad_traverser << std::endl;

    std::cout << "motion blur quads:" << std::endl;
    std::cout << "  accel              = " << quad_accel_mb << std::endl;
    std::cout << "  builder            = " << quad_builder_mb << std::endl;
    std::cout << "  traverser          = " << quad_traverser_mb << std::endl;

    std::cout << "line segments:" << std::endl;
    std::cout << "  accel              = " << line_accel << std::endl;
    std::cout << "  builder            = " << line_builder << std::endl;
    std::cout << "  traverser          = " << line_traverser << std::endl;

    std::cout << "motion blur line segments:" << std::endl;
    std::cout << "  accel              = " << line_accel_mb << std::endl;
    std::cout << "  builder            = " << line_builder_mb << std::endl;
    std::cout << "  traverser          = " << line_traverser_mb << std::endl;
    
    std::cout << "hair:" << std::endl;
    std::cout << "  accel              = " << hair_accel << std::endl;
    std::cout << "  builder            = " << hair_builder << std::endl;
    std::cout << "  traverser          = " << hair_traverser << std::endl;

    std::cout << "motion blur hair:" << std::endl;
    std::cout << "  accel              = " << hair_accel_mb << std::endl;
    std::cout << "  builder            = " << hair_builder_mb << std::endl;
    std::cout << "  traverser          = " << hair_traverser_mb << std::endl;
    
    std::cout << "subdivision surfaces:" << std::endl;
    std::cout << "  accel              = " << subdiv_accel << std::endl;

    std::cout << "grids:" << std::endl;
    std::cout << "  accel              = " << grid_accel << std::endl;
    std::cout << "  builder            = " << grid_builder << std::endl;

    std::cout << "motion blur grids:" << std::endl;
    std::cout << "  accel              = " << grid_accel_mb << std::endl;
    std::cout << "  builder            = " << grid_builder_mb << std::endl;

    std::cout << "object_accel:" << std::endl;
    std::cout << "  min_leaf_size      = " << object_accel_min_leaf_size << std::endl;
    std::cout << "  max_leaf_size      = " << object_accel_max_leaf_size << std::endl;

    std::cout << "object_accel_mb:" << std::endl;
    std::cout << "  min_leaf_size      = " << object_accel_mb_min_leaf_size << std::endl;
    std::cout << "  max_leaf_size      = " << object_accel_mb_max_leaf_size << std::endl;
  }
}
