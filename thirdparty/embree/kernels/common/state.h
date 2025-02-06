// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "default.h"

namespace embree
{
  /* mutex to make printing to cout thread safe */
  extern MutexSys g_printMutex;
  struct RTCErrorMessage
  {
    RTCErrorMessage(RTCError error, std::string const& msg)
      : error(error), msg(msg) {}

    RTCError error;
    std::string msg;
  };

  struct State : public RefCount
  {
  public:
    /*! state construction */
    State ();

    /*! state destruction */
    ~State();

    /*! verifies that state is correct */
    void verify();

    /*! parses state from a configuration file */
    bool parseFile(const FileName& fileName);

    /*! parses the state from a string */
    void parseString(const char* cfg);

    /*! parses the state from a stream */
    void parse(Ref<TokenStream> cin);

    /*! prints the state */
    void print();

    /*! checks if verbosity level is at least N */
    bool verbosity(size_t N);

    /*! checks if some particular ISA is enabled */
    bool hasISA(const int isa);

    /*! check whether selected ISA is supported by the HW */    
    bool checkISASupport();
    
  public:
    std::string tri_accel;                 //!< acceleration structure to use for triangles
    std::string tri_builder;               //!< builder to use for triangles
    std::string tri_traverser;             //!< traverser to use for triangles
    
  public:
    std::string tri_accel_mb;              //!< acceleration structure to use for motion blur triangles
    std::string tri_builder_mb;            //!< builder to use for motion blur triangles
    std::string tri_traverser_mb;          //!< traverser to use for triangles

  public:
    std::string quad_accel;                 //!< acceleration structure to use for quads
    std::string quad_builder;               //!< builder to use for quads
    std::string quad_traverser;             //!< traverser to use for quads

  public:
    std::string quad_accel_mb;             //!< acceleration structure to use for motion blur quads
    std::string quad_builder_mb;           //!< builder to use for motion blur quads
    std::string quad_traverser_mb;         //!< traverser to use for motion blur quads

  public:
    std::string line_accel;                 //!< acceleration structure to use for line segments
    std::string line_builder;               //!< builder to use for line segments
    std::string line_traverser;             //!< traverser to use for line segments

  public:
    std::string line_accel_mb;             //!< acceleration structure to use for motion blur line segments
    std::string line_builder_mb;           //!< builder to use for motion blur line segments
    std::string line_traverser_mb;         //!< traverser to use for motion blur line segments

  public:
    std::string hair_accel;                //!< hair acceleration structure to use
    std::string hair_builder;              //!< builder to use for hair
    std::string hair_traverser;            //!< traverser to use for hair

  public:
    std::string hair_accel_mb;             //!< acceleration structure to use for motion blur hair
    std::string hair_builder_mb;           //!< builder to use for motion blur hair
    std::string hair_traverser_mb;         //!< traverser to use for motion blur hair

  public:
    std::string object_accel;               //!< acceleration structure for user geometries
    std::string object_builder;             //!< builder for user geometries
    int object_accel_min_leaf_size;         //!< minimum leaf size for object acceleration structure
    int object_accel_max_leaf_size;         //!< maximum leaf size for object acceleration structure

  public:
    std::string object_accel_mb;            //!< acceleration structure for user geometries
    std::string object_builder_mb;          //!< builder for user geometries
    int object_accel_mb_min_leaf_size;      //!< minimum leaf size for mblur object acceleration structure
    int object_accel_mb_max_leaf_size;      //!< maximum leaf size for mblur object acceleration structure

  public:
    std::string subdiv_accel;              //!< acceleration structure to use for subdivision surfaces
    std::string subdiv_accel_mb;           //!< acceleration structure to use for subdivision surfaces

  public:
    std::string grid_accel;              //!< acceleration structure to use for grids
    std::string grid_builder;            //!< builder for grids
    std::string grid_accel_mb;           //!< acceleration structure to use for motion blur grids
    std::string grid_builder_mb;         //!< builder for motion blur grids

  public:
    float max_spatial_split_replications;  //!< maximally replications*N many primitives in accel for spatial splits
    bool useSpatialPreSplits;              //!< use spatial pre-splits instead of the full spatial split builder
    size_t tessellation_cache_size;        //!< size of the shared tessellation cache 
    size_t max_triangles_per_leaf;

  public:
    size_t instancing_open_min;            //!< instancing opens tree to minimally that number of subtrees
    size_t instancing_block_size;          //!< instancing opens tree up to average block size of primitives
    float  instancing_open_factor;         //!< instancing opens tree up to x times the number of instances
    size_t instancing_open_max_depth;      //!< maximum open depth for geometries
    size_t instancing_open_max;            //!< instancing opens tree to maximally that number of subtrees

  public:
    bool float_exceptions;                 //!< enable floating point exceptions
    int quality_flags;
    int scene_flags;
    size_t verbose;                        //!< verbosity of output
    size_t benchmark;                      //!< true
    
  public:
    size_t numThreads;                     //!< number of threads to use in builders
    size_t numUserThreads;                 //!< number of user provided threads to use in builders
    bool set_affinity;                     //!< sets affinity for worker threads
    bool start_threads;                    //!< true when threads should be started at device creation time
    int enabled_cpu_features;              //!< CPU ISA features to use
    int enabled_builder_cpu_features;      //!< CPU ISA features to use for builders only
    enum FREQUENCY_LEVEL {
      FREQUENCY_SIMD128,
      FREQUENCY_SIMD256,
      FREQUENCY_SIMD512
    } frequency_level;                     //!< frequency level the app wants to run on (default is SIMD256)
    bool enable_selockmemoryprivilege;     //!< configures the SeLockMemoryPrivilege under Windows to enable huge pages
    bool hugepages;                        //!< true if huge pages should get used
    bool hugepages_success;                //!< status for enabling huge pages

  public:
    size_t alloc_main_block_size;          //!< main allocation block size (shared between threads)
    int alloc_num_main_slots;              //!< number of such shared blocks to be used to allocate
    size_t alloc_thread_block_size;        //!< size of thread local allocator block size
    int alloc_single_thread_alloc;         //!< in single mode nodes and leaves use same thread local allocator

  public:

    /*! checks if we can use AVX */
    bool canUseAVX() {
      return hasISA(AVX) && frequency_level != FREQUENCY_SIMD128;
    }

    /*! checks if we can use AVX2 */
    bool canUseAVX2() {
      return hasISA(AVX2) && frequency_level != FREQUENCY_SIMD128;
    }
    
    struct ErrorHandler
    {
    public:
      ErrorHandler();
      ~ErrorHandler();
      RTCErrorMessage* error();

    public:
      tls_t thread_error;
      std::vector<RTCErrorMessage*> thread_errors;
      MutexSys errors_mutex;
    };
    ErrorHandler errorHandler;
    static ErrorHandler g_errorHandler;

  public:
    void setErrorFunction(RTCErrorFunction fptr, void* uptr) 
    {
      error_function = fptr;
      error_function_userptr = uptr;
    }

    RTCErrorFunction error_function;
    void* error_function_userptr;

  public:
    void setMemoryMonitorFunction(RTCMemoryMonitorFunction fptr, void* uptr) 
    {
      memory_monitor_function = fptr;
      memory_monitor_userptr = uptr;
    }

    RTCMemoryMonitorFunction memory_monitor_function;
    void* memory_monitor_userptr;
  };
}
