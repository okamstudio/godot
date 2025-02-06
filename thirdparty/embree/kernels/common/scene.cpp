// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "scene.h"

#include "../../common/tasking/taskscheduler.h"

#include "../bvh/bvh4_factory.h"
#include "../bvh/bvh8_factory.h"

#include "../../common/algorithms/parallel_reduce.h"

#if defined(EMBREE_SYCL_SUPPORT)
#  include "../sycl/rthwif_embree_builder.h"
#endif


namespace embree
{

  struct TaskGroup {
    /*! global lock step task scheduler */
#if defined(TASKING_INTERNAL)
    MutexSys schedulerMutex;
    Ref<TaskScheduler> scheduler;
#elif defined(TASKING_TBB) && TASKING_TBB_USE_TASK_ISOLATION
    tbb::isolated_task_group group;
#elif defined(TASKING_TBB)
    tbb::task_group group;
#elif defined(TASKING_PPL)
    concurrency::task_group group;
#endif
  };

  /* error raising rtcIntersect and rtcOccluded functions */
  void missing_rtcCommit()      { throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene not committed"); }
  void invalid_rtcIntersect1()  { throw_RTCError(RTC_ERROR_INVALID_OPERATION,"rtcIntersect and rtcOccluded not enabled"); }
  void invalid_rtcIntersect4()  { throw_RTCError(RTC_ERROR_INVALID_OPERATION,"rtcIntersect4 and rtcOccluded4 not enabled"); }
  void invalid_rtcIntersect8()  { throw_RTCError(RTC_ERROR_INVALID_OPERATION,"rtcIntersect8 and rtcOccluded8 not enabled"); }
  void invalid_rtcIntersect16() { throw_RTCError(RTC_ERROR_INVALID_OPERATION,"rtcIntersect16 and rtcOccluded16 not enabled"); }
  void invalid_rtcIntersectN()  { throw_RTCError(RTC_ERROR_INVALID_OPERATION,"rtcIntersectN and rtcOccludedN not enabled"); }

  Scene::Scene (Device* device)
    : device(device),
      flags_modified(true), enabled_geometry_types(0),
      scene_flags(RTC_SCENE_FLAG_NONE),
      quality_flags(RTC_BUILD_QUALITY_MEDIUM),
      modified(true),
      maxTimeSegments(0),
      taskGroup(new TaskGroup()),
      progressInterface(this), progress_monitor_function(nullptr), progress_monitor_ptr(nullptr), progress_monitor_counter(0)
  {
    device->refInc();

    intersectors = Accel::Intersectors(missing_rtcCommit);

    /* use proper device and context for SYCL allocations */
#if defined(EMBREE_SYCL_SUPPORT)
    if (DeviceGPU* gpu_device = dynamic_cast<DeviceGPU*>(device))
      hwaccel = AccelBuffer(AccelAllocator<char>(device,gpu_device->getGPUDevice(),gpu_device->getGPUContext()),0);
#endif
       
    /* one can overwrite flags through device for debugging */
    if (device->quality_flags != -1)
      quality_flags = (RTCBuildQuality) device->quality_flags;
    if (device->scene_flags != -1)
      scene_flags = (RTCSceneFlags) device->scene_flags;
  }

  Scene::~Scene() noexcept
  {
    device->refDec();
  }
  
  void Scene::printStatistics()
  {
    /* calculate maximum number of time segments */
    unsigned max_time_steps = 0;
    for (size_t i=0; i<size(); i++) {
      if (!get(i)) continue;
      max_time_steps = max(max_time_steps,get(i)->numTimeSteps);
    }

    /* initialize vectors*/
    std::vector<size_t> statistics[Geometry::GTY_END];
    for (size_t i=0; i<Geometry::GTY_END; i++)
      statistics[i].resize(max_time_steps);

    /* gather statistics */
    for (size_t i=0; i<size(); i++) 
    {
      if (!get(i)) continue;
      int ty = get(i)->getType(); 
      assert(ty<Geometry::GTY_END);
      int timesegments = get(i)->numTimeSegments(); 
      assert((unsigned int)timesegments < max_time_steps);
      statistics[ty][timesegments] += get(i)->size();
    }

    /* print statistics */
    std::cout << std::setw(23) << "segments" << ": ";
    for (size_t t=0; t<max_time_steps; t++)
      std::cout << std::setw(10) << t;
    std::cout << std::endl;

    std::cout << "-------------------------";
    for (size_t t=0; t<max_time_steps; t++)
      std::cout << "----------";
    std::cout << std::endl;
    
    for (size_t p=0; p<Geometry::GTY_END; p++)
    {
      if (std::string(Geometry::gtype_names[p]) == "") continue;
      std::cout << std::setw(23) << Geometry::gtype_names[p] << ": ";
      for (size_t t=0; t<max_time_steps; t++)
        std::cout << std::setw(10) << statistics[p][t];
      std::cout << std::endl;
    }
  }

  void Scene::createTriangleAccel()
  {
#if defined(EMBREE_GEOMETRY_TRIANGLE)

    if (device->tri_accel == "default") 
    {
      if (quality_flags != RTC_BUILD_QUALITY_LOW)
      {	
        int mode =  2*(int)isCompactAccel() + 1*(int)isRobustAccel(); 
        switch (mode) {
        case /*0b00*/ 0: 
#if defined (EMBREE_TARGET_SIMD8)
          if (device->canUseAVX())
	  {
            if (quality_flags == RTC_BUILD_QUALITY_HIGH) 
              accels_add(device->bvh8_factory->BVH8Triangle4(this,BVHFactory::BuildVariant::HIGH_QUALITY,BVHFactory::IntersectVariant::FAST));
            else
              accels_add(device->bvh8_factory->BVH8Triangle4(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::FAST));
          }
          else 
#endif
          { 
            if (quality_flags == RTC_BUILD_QUALITY_HIGH) 
              accels_add(device->bvh4_factory->BVH4Triangle4(this,BVHFactory::BuildVariant::HIGH_QUALITY,BVHFactory::IntersectVariant::FAST));
            else 
              accels_add(device->bvh4_factory->BVH4Triangle4(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::FAST));
          }
          break;

        case /*0b01*/ 1: 
#if defined (EMBREE_TARGET_SIMD8)
          if (device->canUseAVX()) 
            accels_add(device->bvh8_factory->BVH8Triangle4v(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::ROBUST));
          else
#endif
            accels_add(device->bvh4_factory->BVH4Triangle4v(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::ROBUST));

          break;
        case /*0b10*/ 2: accels_add(device->bvh4_factory->BVH4Triangle4i(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::FAST  )); break;
        case /*0b11*/ 3: accels_add(device->bvh4_factory->BVH4Triangle4i(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::ROBUST)); break;
        }
      }
      else /* dynamic */
      {
#if defined (EMBREE_TARGET_SIMD8)
          if (device->canUseAVX())
	  {
            int mode =  2*(int)isCompactAccel() + 1*(int)isRobustAccel();
            switch (mode) {
            case /*0b00*/ 0: accels_add(device->bvh8_factory->BVH8Triangle4 (this,BVHFactory::BuildVariant::DYNAMIC,BVHFactory::IntersectVariant::FAST  )); break;
            case /*0b01*/ 1: accels_add(device->bvh8_factory->BVH8Triangle4v(this,BVHFactory::BuildVariant::DYNAMIC,BVHFactory::IntersectVariant::ROBUST)); break;
            case /*0b10*/ 2: accels_add(device->bvh4_factory->BVH4Triangle4i(this,BVHFactory::BuildVariant::DYNAMIC,BVHFactory::IntersectVariant::FAST  )); break;
            case /*0b11*/ 3: accels_add(device->bvh4_factory->BVH4Triangle4i(this,BVHFactory::BuildVariant::DYNAMIC,BVHFactory::IntersectVariant::ROBUST)); break;
            }
          }
          else
#endif
          {
            int mode =  2*(int)isCompactAccel() + 1*(int)isRobustAccel();
            switch (mode) {
            case /*0b00*/ 0: accels_add(device->bvh4_factory->BVH4Triangle4 (this,BVHFactory::BuildVariant::DYNAMIC,BVHFactory::IntersectVariant::FAST  )); break;
            case /*0b01*/ 1: accels_add(device->bvh4_factory->BVH4Triangle4v(this,BVHFactory::BuildVariant::DYNAMIC,BVHFactory::IntersectVariant::ROBUST)); break;
            case /*0b10*/ 2: accels_add(device->bvh4_factory->BVH4Triangle4i(this,BVHFactory::BuildVariant::DYNAMIC,BVHFactory::IntersectVariant::FAST  )); break;
            case /*0b11*/ 3: accels_add(device->bvh4_factory->BVH4Triangle4i(this,BVHFactory::BuildVariant::DYNAMIC,BVHFactory::IntersectVariant::ROBUST)); break;
            }
          }
      }
    }
    else if (device->tri_accel == "bvh4.triangle4")       accels_add(device->bvh4_factory->BVH4Triangle4 (this));
    else if (device->tri_accel == "bvh4.triangle4v")      accels_add(device->bvh4_factory->BVH4Triangle4v(this));
    else if (device->tri_accel == "bvh4.triangle4i")      accels_add(device->bvh4_factory->BVH4Triangle4i(this));
    else if (device->tri_accel == "qbvh4.triangle4i")     accels_add(device->bvh4_factory->BVH4QuantizedTriangle4i(this));

#if defined (EMBREE_TARGET_SIMD8)
    else if (device->tri_accel == "bvh8.triangle4")       accels_add(device->bvh8_factory->BVH8Triangle4 (this));
    else if (device->tri_accel == "bvh8.triangle4v")      accels_add(device->bvh8_factory->BVH8Triangle4v(this));
    else if (device->tri_accel == "bvh8.triangle4i")      accels_add(device->bvh8_factory->BVH8Triangle4i(this));
    else if (device->tri_accel == "qbvh8.triangle4i")     accels_add(device->bvh8_factory->BVH8QuantizedTriangle4i(this));
    else if (device->tri_accel == "qbvh8.triangle4")      accels_add(device->bvh8_factory->BVH8QuantizedTriangle4(this));
#endif
    else throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"unknown triangle acceleration structure "+device->tri_accel);
#endif

  }

  void Scene::createTriangleMBAccel()
  {
#if defined(EMBREE_GEOMETRY_TRIANGLE)

    if (device->tri_accel_mb == "default")
    {
      int mode =  2*(int)isCompactAccel() + 1*(int)isRobustAccel(); 
      
#if defined (EMBREE_TARGET_SIMD8)
      if (device->canUseAVX2()) // BVH8 reduces performance on AVX only-machines
      {
        switch (mode) {
        case /*0b00*/ 0: accels_add(device->bvh8_factory->BVH8Triangle4iMB(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::FAST  )); break;
        case /*0b01*/ 1: accels_add(device->bvh8_factory->BVH8Triangle4iMB(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::ROBUST)); break;
        case /*0b10*/ 2: accels_add(device->bvh4_factory->BVH4Triangle4iMB(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::FAST  )); break;
        case /*0b11*/ 3: accels_add(device->bvh4_factory->BVH4Triangle4iMB(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::ROBUST)); break;
        }
      }
      else
#endif
      {
        switch (mode) {
        case /*0b00*/ 0: accels_add(device->bvh4_factory->BVH4Triangle4iMB(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::FAST  )); break;
        case /*0b01*/ 1: accels_add(device->bvh4_factory->BVH4Triangle4iMB(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::ROBUST)); break;
        case /*0b10*/ 2: accels_add(device->bvh4_factory->BVH4Triangle4iMB(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::FAST  )); break;
        case /*0b11*/ 3: accels_add(device->bvh4_factory->BVH4Triangle4iMB(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::ROBUST)); break;
        }
      }
    }
    else if (device->tri_accel_mb == "bvh4.triangle4imb") accels_add(device->bvh4_factory->BVH4Triangle4iMB(this));
    else if (device->tri_accel_mb == "bvh4.triangle4vmb") accels_add(device->bvh4_factory->BVH4Triangle4vMB(this));
#if defined (EMBREE_TARGET_SIMD8)
    else if (device->tri_accel_mb == "bvh8.triangle4imb") accels_add(device->bvh8_factory->BVH8Triangle4iMB(this));
    else if (device->tri_accel_mb == "bvh8.triangle4vmb") accels_add(device->bvh8_factory->BVH8Triangle4vMB(this));
#endif
    else throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"unknown motion blur triangle acceleration structure "+device->tri_accel_mb);
#endif
  }

  void Scene::createQuadAccel()
  {
#if defined(EMBREE_GEOMETRY_QUAD)
    
    if (device->quad_accel == "default") 
    {
      if (quality_flags != RTC_BUILD_QUALITY_LOW)
      {
        /* static */
        int mode =  2*(int)isCompactAccel() + 1*(int)isRobustAccel(); 
        switch (mode) {
        case /*0b00*/ 0:
#if defined (EMBREE_TARGET_SIMD8)
          if (device->canUseAVX())
          {
            if (quality_flags == RTC_BUILD_QUALITY_HIGH) 
              accels_add(device->bvh8_factory->BVH8Quad4v(this,BVHFactory::BuildVariant::HIGH_QUALITY,BVHFactory::IntersectVariant::FAST));
            else
              accels_add(device->bvh8_factory->BVH8Quad4v(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::FAST));
          }
          else
#endif
          {
            if (quality_flags == RTC_BUILD_QUALITY_HIGH) 
              accels_add(device->bvh4_factory->BVH4Quad4v(this,BVHFactory::BuildVariant::HIGH_QUALITY,BVHFactory::IntersectVariant::FAST));
            else
              accels_add(device->bvh4_factory->BVH4Quad4v(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::FAST));
          }
          break;

        case /*0b01*/ 1:
#if defined (EMBREE_TARGET_SIMD8)
          if (device->canUseAVX())
            accels_add(device->bvh8_factory->BVH8Quad4v(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::ROBUST));
          else
#endif
            accels_add(device->bvh4_factory->BVH4Quad4v(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::ROBUST));
          break;

        case /*0b10*/ 2: accels_add(device->bvh4_factory->BVH4Quad4i(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::FAST)); break;
        case /*0b11*/ 3: accels_add(device->bvh4_factory->BVH4Quad4i(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::ROBUST)); break;
        }
      }
      else /* dynamic */
      {
#if defined (EMBREE_TARGET_SIMD8)
          if (device->canUseAVX())
	  {
            int mode =  2*(int)isCompactAccel() + 1*(int)isRobustAccel();
            switch (mode) {
            case /*0b00*/ 0: accels_add(device->bvh8_factory->BVH8Quad4v(this,BVHFactory::BuildVariant::DYNAMIC,BVHFactory::IntersectVariant::FAST)); break;
            case /*0b01*/ 1: accels_add(device->bvh8_factory->BVH8Quad4v(this,BVHFactory::BuildVariant::DYNAMIC,BVHFactory::IntersectVariant::ROBUST)); break;
            case /*0b10*/ 2: accels_add(device->bvh4_factory->BVH4Quad4v(this,BVHFactory::BuildVariant::DYNAMIC,BVHFactory::IntersectVariant::FAST)); break;
            case /*0b11*/ 3: accels_add(device->bvh4_factory->BVH4Quad4v(this,BVHFactory::BuildVariant::DYNAMIC,BVHFactory::IntersectVariant::ROBUST)); break;
            }
          }
          else
#endif
          {
            int mode =  2*(int)isCompactAccel() + 1*(int)isRobustAccel();
            switch (mode) {
            case /*0b00*/ 0: accels_add(device->bvh4_factory->BVH4Quad4v(this,BVHFactory::BuildVariant::DYNAMIC,BVHFactory::IntersectVariant::FAST)); break;
            case /*0b01*/ 1: accels_add(device->bvh4_factory->BVH4Quad4v(this,BVHFactory::BuildVariant::DYNAMIC,BVHFactory::IntersectVariant::ROBUST)); break;
            case /*0b10*/ 2: accels_add(device->bvh4_factory->BVH4Quad4v(this,BVHFactory::BuildVariant::DYNAMIC,BVHFactory::IntersectVariant::FAST)); break;
            case /*0b11*/ 3: accels_add(device->bvh4_factory->BVH4Quad4v(this,BVHFactory::BuildVariant::DYNAMIC,BVHFactory::IntersectVariant::ROBUST)); break;
            }
          }
      }
    }
    else if (device->quad_accel == "bvh4.quad4v")       accels_add(device->bvh4_factory->BVH4Quad4v(this));
    else if (device->quad_accel == "bvh4.quad4i")       accels_add(device->bvh4_factory->BVH4Quad4i(this));
    else if (device->quad_accel == "qbvh4.quad4i")      accels_add(device->bvh4_factory->BVH4QuantizedQuad4i(this));

#if defined (EMBREE_TARGET_SIMD8)
    else if (device->quad_accel == "bvh8.quad4v")       accels_add(device->bvh8_factory->BVH8Quad4v(this));
    else if (device->quad_accel == "bvh8.quad4i")       accels_add(device->bvh8_factory->BVH8Quad4i(this));
    else if (device->quad_accel == "qbvh8.quad4i")      accels_add(device->bvh8_factory->BVH8QuantizedQuad4i(this));
#endif
    else throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"unknown quad acceleration structure "+device->quad_accel);
#endif
  }

  void Scene::createQuadMBAccel()
  {
#if defined(EMBREE_GEOMETRY_QUAD)

    if (device->quad_accel_mb == "default") 
    {
      int mode =  2*(int)isCompactAccel() + 1*(int)isRobustAccel(); 
      switch (mode) {
      case /*0b00*/ 0:
#if defined (EMBREE_TARGET_SIMD8)
        if (device->canUseAVX())
          accels_add(device->bvh8_factory->BVH8Quad4iMB(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::FAST));
        else
#endif
          accels_add(device->bvh4_factory->BVH4Quad4iMB(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::FAST));
        break;

      case /*0b01*/ 1:
#if defined (EMBREE_TARGET_SIMD8)
        if (device->canUseAVX())
          accels_add(device->bvh8_factory->BVH8Quad4iMB(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::ROBUST));
        else
#endif
          accels_add(device->bvh4_factory->BVH4Quad4iMB(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::ROBUST));
        break;

      case /*0b10*/ 2: accels_add(device->bvh4_factory->BVH4Quad4iMB(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::FAST  )); break;
      case /*0b11*/ 3: accels_add(device->bvh4_factory->BVH4Quad4iMB(this,BVHFactory::BuildVariant::STATIC,BVHFactory::IntersectVariant::ROBUST)); break;
      }
    }
    else if (device->quad_accel_mb == "bvh4.quad4imb") accels_add(device->bvh4_factory->BVH4Quad4iMB(this));
#if defined (EMBREE_TARGET_SIMD8)
    else if (device->quad_accel_mb == "bvh8.quad4imb") accels_add(device->bvh8_factory->BVH8Quad4iMB(this));
#endif
    else throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"unknown quad motion blur acceleration structure "+device->quad_accel_mb);
#endif
  }

  void Scene::createHairAccel()
  {
#if defined(EMBREE_GEOMETRY_CURVE) || defined(EMBREE_GEOMETRY_POINT)

    if (device->hair_accel == "default")
    {
      int mode = 2*(int)isCompactAccel() + 1*(int)isRobustAccel();
#if defined (EMBREE_TARGET_SIMD8)
      if (device->canUseAVX2()) // only enable on HSW machines, for SNB this codepath is slower
      {
        switch (mode) {
        case /*0b00*/ 0: accels_add(device->bvh8_factory->BVH8OBBVirtualCurve8v(this,BVHFactory::IntersectVariant::FAST)); break;
        case /*0b01*/ 1: accels_add(device->bvh8_factory->BVH8OBBVirtualCurve8v(this,BVHFactory::IntersectVariant::ROBUST)); break;
        case /*0b10*/ 2: accels_add(device->bvh4_factory->BVH4OBBVirtualCurve8i(this,BVHFactory::IntersectVariant::FAST)); break;
        case /*0b11*/ 3: accels_add(device->bvh4_factory->BVH4OBBVirtualCurve8i(this,BVHFactory::IntersectVariant::ROBUST)); break;
        }
      }
      else
#endif
      {
        switch (mode) {
        case /*0b00*/ 0: accels_add(device->bvh4_factory->BVH4OBBVirtualCurve4v(this,BVHFactory::IntersectVariant::FAST)); break;
        case /*0b01*/ 1: accels_add(device->bvh4_factory->BVH4OBBVirtualCurve4v(this,BVHFactory::IntersectVariant::ROBUST)); break;
        case /*0b10*/ 2: accels_add(device->bvh4_factory->BVH4OBBVirtualCurve4i(this,BVHFactory::IntersectVariant::FAST)); break;
        case /*0b11*/ 3: accels_add(device->bvh4_factory->BVH4OBBVirtualCurve4i(this,BVHFactory::IntersectVariant::ROBUST)); break;
        }
      }
    }
    else if (device->hair_accel == "bvh4obb.virtualcurve4v" ) accels_add(device->bvh4_factory->BVH4OBBVirtualCurve4v(this,BVHFactory::IntersectVariant::FAST));
    else if (device->hair_accel == "bvh4obb.virtualcurve4i" ) accels_add(device->bvh4_factory->BVH4OBBVirtualCurve4i(this,BVHFactory::IntersectVariant::FAST));
#if defined (EMBREE_TARGET_SIMD8)
    else if (device->hair_accel == "bvh8obb.virtualcurve8v" ) accels_add(device->bvh8_factory->BVH8OBBVirtualCurve8v(this,BVHFactory::IntersectVariant::FAST));
    else if (device->hair_accel == "bvh4obb.virtualcurve8i" ) accels_add(device->bvh4_factory->BVH4OBBVirtualCurve8i(this,BVHFactory::IntersectVariant::FAST));
#endif
    else throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"unknown hair acceleration structure "+device->hair_accel);
#endif
  }

  void Scene::createHairMBAccel()
  {
#if defined(EMBREE_GEOMETRY_CURVE) || defined(EMBREE_GEOMETRY_POINT)

    if (device->hair_accel_mb == "default")
    {
#if defined (EMBREE_TARGET_SIMD8)
      if (device->canUseAVX2()) // only enable on HSW machines, on SNB this codepath is slower
      {
        if (isRobustAccel()) accels_add(device->bvh8_factory->BVH8OBBVirtualCurve8iMB(this,BVHFactory::IntersectVariant::ROBUST));
        else                 accels_add(device->bvh8_factory->BVH8OBBVirtualCurve8iMB(this,BVHFactory::IntersectVariant::FAST));
      }
      else
#endif
      {
        if (isRobustAccel()) accels_add(device->bvh4_factory->BVH4OBBVirtualCurve4iMB(this,BVHFactory::IntersectVariant::ROBUST));
        else                 accels_add(device->bvh4_factory->BVH4OBBVirtualCurve4iMB(this,BVHFactory::IntersectVariant::FAST));
      }
    }
    else if (device->hair_accel_mb == "bvh4.virtualcurve4imb") accels_add(device->bvh4_factory->BVH4OBBVirtualCurve4iMB(this,BVHFactory::IntersectVariant::FAST));

#if defined (EMBREE_TARGET_SIMD8)
    else if (device->hair_accel_mb == "bvh4.virtualcurve8imb") accels_add(device->bvh4_factory->BVH4OBBVirtualCurve8iMB(this,BVHFactory::IntersectVariant::FAST));
    else if (device->hair_accel_mb == "bvh8.virtualcurve8imb") accels_add(device->bvh8_factory->BVH8OBBVirtualCurve8iMB(this,BVHFactory::IntersectVariant::FAST));
#endif
    else throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"unknown motion blur hair acceleration structure "+device->hair_accel_mb);
#endif
  }

  void Scene::createSubdivAccel()
  {
#if defined(EMBREE_GEOMETRY_SUBDIVISION)
    if (device->subdiv_accel == "default") {
      accels_add(device->bvh4_factory->BVH4SubdivPatch1(this));
    }
    else if (device->subdiv_accel == "bvh4.grid.eager" ) accels_add(device->bvh4_factory->BVH4SubdivPatch1(this));
    else if (device->subdiv_accel == "bvh4.subdivpatch1eager" ) accels_add(device->bvh4_factory->BVH4SubdivPatch1(this));
    else throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"unknown subdiv accel "+device->subdiv_accel);
#endif
  }

  void Scene::createSubdivMBAccel()
  {
#if defined(EMBREE_GEOMETRY_SUBDIVISION)
    if (device->subdiv_accel_mb == "default") {
      accels_add(device->bvh4_factory->BVH4SubdivPatch1MB(this));
    }
    else throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"unknown subdiv mblur accel "+device->subdiv_accel_mb);
#endif
  }

  void Scene::createUserGeometryAccel()
  {
#if defined(EMBREE_GEOMETRY_USER)

    if (device->object_accel == "default")
    {
#if defined (EMBREE_TARGET_SIMD8)
      if (device->canUseAVX() && !isCompactAccel())
      {
        if (quality_flags != RTC_BUILD_QUALITY_LOW) {
          accels_add(device->bvh8_factory->BVH8UserGeometry(this,BVHFactory::BuildVariant::STATIC));
        } else {
          accels_add(device->bvh8_factory->BVH8UserGeometry(this,BVHFactory::BuildVariant::DYNAMIC));
        }
      }
      else
#endif
      {
        if (quality_flags != RTC_BUILD_QUALITY_LOW) {
          accels_add(device->bvh4_factory->BVH4UserGeometry(this,BVHFactory::BuildVariant::STATIC));
        } else {
          accels_add(device->bvh4_factory->BVH4UserGeometry(this,BVHFactory::BuildVariant::DYNAMIC));
        }
      }
    }
    else if (device->object_accel == "bvh4.object") accels_add(device->bvh4_factory->BVH4UserGeometry(this));
#if defined (EMBREE_TARGET_SIMD8)
    else if (device->object_accel == "bvh8.object") accels_add(device->bvh8_factory->BVH8UserGeometry(this));
#endif
    else throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"unknown user geometry accel "+device->object_accel);
#endif
  }

  void Scene::createUserGeometryMBAccel()
  {
#if defined(EMBREE_GEOMETRY_USER)

    if (device->object_accel_mb == "default"    ) {
#if defined (EMBREE_TARGET_SIMD8)
      if (device->canUseAVX() && !isCompactAccel())
        accels_add(device->bvh8_factory->BVH8UserGeometryMB(this));
      else
#endif
        accels_add(device->bvh4_factory->BVH4UserGeometryMB(this));
    }
    else if (device->object_accel_mb == "bvh4.object") accels_add(device->bvh4_factory->BVH4UserGeometryMB(this));
#if defined (EMBREE_TARGET_SIMD8)
    else if (device->object_accel_mb == "bvh8.object") accels_add(device->bvh8_factory->BVH8UserGeometryMB(this));
#endif
    else throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"unknown user geometry mblur accel "+device->object_accel_mb);
#endif
  }

  void Scene::createInstanceAccel()
  {
#if defined(EMBREE_GEOMETRY_INSTANCE)

    // if (device->object_accel == "default") 
    {
#if defined (EMBREE_TARGET_SIMD8)
      if (device->canUseAVX() && !isCompactAccel()) {
        if (quality_flags != RTC_BUILD_QUALITY_LOW) {
          accels_add(device->bvh8_factory->BVH8Instance(this, false, BVHFactory::BuildVariant::STATIC));
        } else {
          accels_add(device->bvh8_factory->BVH8Instance(this, false, BVHFactory::BuildVariant::DYNAMIC));
        }
      } 
      else
#endif
      {
        if (quality_flags != RTC_BUILD_QUALITY_LOW) {
          accels_add(device->bvh4_factory->BVH4Instance(this, false, BVHFactory::BuildVariant::STATIC));
        } else {
          accels_add(device->bvh4_factory->BVH4Instance(this, false, BVHFactory::BuildVariant::DYNAMIC));
        }
      }
    }
    // else throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"unknown instance accel "+device->instance_accel);
#endif
  }

  void Scene::createInstanceMBAccel()
  {
#if defined(EMBREE_GEOMETRY_INSTANCE)

    //if (device->instance_accel_mb == "default")
    {
#if defined (EMBREE_TARGET_SIMD8)
      if (device->canUseAVX() && !isCompactAccel())
        accels_add(device->bvh8_factory->BVH8InstanceMB(this, false));
      else
#endif
        accels_add(device->bvh4_factory->BVH4InstanceMB(this, false));
    }
    //else throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"unknown instance mblur accel "+device->instance_accel_mb);
#endif
  }

  void Scene::createInstanceExpensiveAccel()
  {
#if defined(EMBREE_GEOMETRY_INSTANCE)
    // if (device->object_accel == "default") 
    {
#if defined (EMBREE_TARGET_SIMD8)
      if (device->canUseAVX() && !isCompactAccel()) {
        if (quality_flags != RTC_BUILD_QUALITY_LOW) {
          accels_add(device->bvh8_factory->BVH8Instance(this, true, BVHFactory::BuildVariant::STATIC));
        } else {
          accels_add(device->bvh8_factory->BVH8Instance(this, true, BVHFactory::BuildVariant::DYNAMIC));
        }
      } 
      else
#endif
      {
        if (quality_flags != RTC_BUILD_QUALITY_LOW) {
          accels_add(device->bvh4_factory->BVH4Instance(this, true, BVHFactory::BuildVariant::STATIC));
        } else {
          accels_add(device->bvh4_factory->BVH4Instance(this, true, BVHFactory::BuildVariant::DYNAMIC));
        }
      }
    }
    // else throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"unknown instance accel "+device->instance_accel);
#endif
  }

  void Scene::createInstanceExpensiveMBAccel()
  {
#if defined(EMBREE_GEOMETRY_INSTANCE)
    //if (device->instance_accel_mb == "default")
    {
#if defined (EMBREE_TARGET_SIMD8)
      if (device->canUseAVX() && !isCompactAccel())
        accels_add(device->bvh8_factory->BVH8InstanceMB(this, true));
      else
#endif
        accels_add(device->bvh4_factory->BVH4InstanceMB(this, true));
    }
    //else throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"unknown instance mblur accel "+device->instance_accel_mb);
#endif
  }

  void Scene::createInstanceArrayAccel()
  {
#if defined(EMBREE_GEOMETRY_INSTANCE_ARRAY)

    // if (device->object_accel == "default") 
    {
#if defined (EMBREE_TARGET_SIMD8)
      if (device->canUseAVX() && !isCompactAccel()) {
        if (quality_flags != RTC_BUILD_QUALITY_LOW) {
          accels_add(device->bvh8_factory->BVH8InstanceArray(this, BVHFactory::BuildVariant::STATIC));
        } else {
          accels_add(device->bvh8_factory->BVH8InstanceArray(this, BVHFactory::BuildVariant::DYNAMIC));
        }
      } 
      else
#endif
      {
        if (quality_flags != RTC_BUILD_QUALITY_LOW) {
          accels_add(device->bvh4_factory->BVH4InstanceArray(this, BVHFactory::BuildVariant::STATIC));
        } else {
          accels_add(device->bvh4_factory->BVH4InstanceArray(this, BVHFactory::BuildVariant::DYNAMIC));
        }
      }
    }
    // else throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"unknown instance accel "+device->instance_accel);
#endif
  }

  void Scene::createInstanceArrayMBAccel()
  {
#if defined(EMBREE_GEOMETRY_INSTANCE_ARRAY)

    //if (device->instance_accel_mb == "default")
    {
#if defined (EMBREE_TARGET_SIMD8)
      if (device->canUseAVX() && !isCompactAccel())
        accels_add(device->bvh8_factory->BVH8InstanceArrayMB(this));
      else
#endif
        accels_add(device->bvh4_factory->BVH4InstanceArrayMB(this));
    }
    //else throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"unknown instance mblur accel "+device->instance_accel_mb);
#endif
  }


  void Scene::createGridAccel()
  {
#if defined(EMBREE_GEOMETRY_GRID)
    
    BVHFactory::IntersectVariant ivariant = isRobustAccel() ? BVHFactory::IntersectVariant::ROBUST : BVHFactory::IntersectVariant::FAST;

    if (device->grid_accel == "default") 
    {
#if defined (EMBREE_TARGET_SIMD8)
      if (device->canUseAVX() && !isCompactAccel())
      {
        accels_add(device->bvh8_factory->BVH8Grid(this,BVHFactory::BuildVariant::STATIC,ivariant));
      }
      else
#endif
      {
        accels_add(device->bvh4_factory->BVH4Grid(this,BVHFactory::BuildVariant::STATIC,ivariant));
      }
    }
    else if (device->grid_accel == "bvh4.grid") accels_add(device->bvh4_factory->BVH4Grid(this,BVHFactory::BuildVariant::STATIC,ivariant));
#if defined (EMBREE_TARGET_SIMD8)
    else if (device->grid_accel == "bvh8.grid") accels_add(device->bvh8_factory->BVH8Grid(this,BVHFactory::BuildVariant::STATIC,ivariant));
#endif
    else throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"unknown grid accel "+device->grid_accel);
#endif

  }

  void Scene::createGridMBAccel()
  {
#if defined(EMBREE_GEOMETRY_GRID)

    if (device->grid_accel_mb == "default") 
    {
      accels_add(device->bvh4_factory->BVH4GridMB(this,BVHFactory::BuildVariant::STATIC));
    }
    else if (device->grid_accel_mb == "bvh4mb.grid") accels_add(device->bvh4_factory->BVH4GridMB(this));
    else throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"unknown grid mb accel "+device->grid_accel);
#endif

  }

  void Scene::clear() {
  }

  unsigned Scene::bind(unsigned geomID, Ref<Geometry> geometry) 
  {
    Lock<MutexSys> lock(geometriesMutex);
    if (geomID == RTC_INVALID_GEOMETRY_ID) {
      geomID = id_pool.allocate();
      if (geomID == RTC_INVALID_GEOMETRY_ID)
        throw_RTCError(RTC_ERROR_INVALID_OPERATION,"too many geometries inside scene");
    }
    else
    {
      if (!id_pool.add(geomID))
        throw_RTCError(RTC_ERROR_INVALID_OPERATION,"invalid geometry ID provided");
    }
    if (geomID >= geometries.size()) {
      geometries.resize(geomID+1);
      vertices.resize(geomID+1);
      geometryModCounters_.resize(geomID+1);
    }
    geometries[geomID] = geometry;
    geometryModCounters_[geomID] = 0;
    if (geometry->isEnabled()) {
      setModified ();
    }
    return geomID;
  }

  void Scene::detachGeometry(size_t geomID)
  {
    Lock<MutexSys> lock(geometriesMutex);
    
    if (geomID >= geometries.size())
      throw_RTCError(RTC_ERROR_INVALID_OPERATION,"invalid geometry ID");

    Ref<Geometry>& geometry = geometries[geomID];
    if (geometry == null)
      throw_RTCError(RTC_ERROR_INVALID_OPERATION,"invalid geometry");
    
    setModified ();
    accels_deleteGeometry(unsigned(geomID));
    id_pool.deallocate((unsigned)geomID);
    geometries[geomID] = null;
    vertices[geomID] = nullptr;
    geometryModCounters_[geomID] = 0;
  }

  void Scene::build_cpu_accels()
  {
    /* select acceleration structures to build */
    unsigned int new_enabled_geometry_types = world.enabledGeometryTypesMask();

    if (flags_modified || new_enabled_geometry_types != enabled_geometry_types)
    {
      accels_init();

      /* we need to make all geometries modified, otherwise two level builder will 
        not rebuild currently not modified geometries */
      parallel_for(geometryModCounters_.size(), [&] ( const size_t i ) {
          geometryModCounters_[i] = 0;
        });

      if (getNumPrimitives(TriangleMesh::geom_type,false)) createTriangleAccel();
      if (getNumPrimitives(TriangleMesh::geom_type,true)) createTriangleMBAccel();
      if (getNumPrimitives(QuadMesh::geom_type,false)) createQuadAccel();
      if (getNumPrimitives(QuadMesh::geom_type,true)) createQuadMBAccel();
      if (getNumPrimitives(GridMesh::geom_type,false)) createGridAccel();
      if (getNumPrimitives(GridMesh::geom_type,true)) createGridMBAccel();
      if (getNumPrimitives(SubdivMesh::geom_type,false)) createSubdivAccel();
      if (getNumPrimitives(SubdivMesh::geom_type,true)) createSubdivMBAccel();
      if (getNumPrimitives(Geometry::MTY_CURVES,false)) createHairAccel();
      if (getNumPrimitives(Geometry::MTY_CURVES,true)) createHairMBAccel();
      if (getNumPrimitives(UserGeometry::geom_type,false)) createUserGeometryAccel();
      if (getNumPrimitives(UserGeometry::geom_type,true)) createUserGeometryMBAccel();
      if (getNumPrimitives(Geometry::MTY_INSTANCE_CHEAP,false)) createInstanceAccel();
      if (getNumPrimitives(Geometry::MTY_INSTANCE_CHEAP,true)) createInstanceMBAccel();
      if (getNumPrimitives(Geometry::MTY_INSTANCE_EXPENSIVE,false)) createInstanceExpensiveAccel();
      if (getNumPrimitives(Geometry::MTY_INSTANCE_EXPENSIVE,true)) createInstanceExpensiveMBAccel();
      if (getNumPrimitives(Geometry::MTY_INSTANCE_ARRAY,false)) createInstanceArrayAccel();
      if (getNumPrimitives(Geometry::MTY_INSTANCE_ARRAY,true)) createInstanceArrayMBAccel();

      flags_modified = false;
      enabled_geometry_types = new_enabled_geometry_types;
    }
    
    /* select fast code path if no filter function is present */
    accels_select(hasFilterFunction());
  
    /* build all hierarchies of this scene */
    accels_build();

    /* make static geometry immutable */
    if (!isDynamicAccel()) {
      accels_immutable();
      flags_modified = true; // in non-dynamic mode we have to re-create accels
    }

    if (device->verbosity(2)) {
      std::cout << "created scene intersector" << std::endl;
      accels_print(2);
      std::cout << "selected scene intersector" << std::endl;
      intersectors.print(2);
    }
  }

  void Scene::build_gpu_accels()
  {
#if defined(EMBREE_SYCL_SUPPORT)
    auto [aabb, stride] = rthwifBuild(this,hwaccel);
    hwaccel_stride = stride;
    bounds = LBBox<embree::Vec3fa>(aabb);
    hwaccel_bounds = aabb;
#endif
  }

  void Scene::commit_task ()
  {
    checkIfModifiedAndSet();
    if (!isModified()) return;
    

    /* print scene statistics */
    if (device->verbosity(2))
      printStatistics();

    progress_monitor_counter = 0;
    
    /* gather scene stats and call preCommit function of each geometry */
    this->world = parallel_reduce (size_t(0), geometries.size(), GeometryCounts (), 
      [this](const range<size_t>& r)->GeometryCounts
      {
        GeometryCounts c;
        for (auto i=r.begin(); i<r.end(); ++i) 
        {
          if (geometries[i] && geometries[i]->isEnabled()) 
          {
            geometries[i]->preCommit();
            geometries[i]->addElementsToCount (c);
            c.numFilterFunctions += (int) geometries[i]->hasArgumentFilterFunctions();
            c.numFilterFunctions += (int) geometries[i]->hasGeometryFilterFunctions();
          }
        }
        return c;
      },
      std::plus<GeometryCounts>()
    );

    /* calculate maximal number of motion blur time segments in scene */
    maxTimeSegments = 1;
    for (size_t geomID=0; geomID<size(); geomID++)
    {
      Geometry* geom = get(geomID);
      if (geom == nullptr) continue;
      maxTimeSegments = std::max(maxTimeSegments, geom->numTimeSegments());
    }

#if defined(EMBREE_SYCL_SUPPORT)
    DeviceGPU* gpu_device = dynamic_cast<DeviceGPU*>(device);
    if (gpu_device)
      build_gpu_accels();
    else
#endif
      build_cpu_accels();

    /* call postCommit function of each geometry */
    parallel_for(geometries.size(), [&] ( const size_t i ) {
        if (geometries[i] && geometries[i]->isEnabled()) {
          geometries[i]->postCommit();
          vertices[i] = geometries[i]->getCompactVertexArray();
          geometryModCounters_[i] = geometries[i]->getModCounter();
        }
      });

    setModified(false);
  }

  void Scene::setBuildQuality(RTCBuildQuality quality_flags_i)
  {
    if (quality_flags == quality_flags_i) return;
    quality_flags = quality_flags_i;
    flags_modified = true;
  }

  RTCBuildQuality Scene::getBuildQuality() const {
    return quality_flags;
  }

  void Scene::setSceneFlags(RTCSceneFlags scene_flags_i)
  {
    if (scene_flags == scene_flags_i) return;
    scene_flags = scene_flags_i;
    flags_modified = true;
  }

  RTCSceneFlags Scene::getSceneFlags() const {
    return scene_flags;
  }
                   
#if defined(TASKING_INTERNAL)

  void Scene::commit (bool join) 
  {
    Lock<MutexSys> buildLock(buildMutex,false);

    /* allocates own taskscheduler for each build */
    Ref<TaskScheduler> scheduler = nullptr;
    { 
      Lock<MutexSys> lock(taskGroup->schedulerMutex);
      scheduler = taskGroup->scheduler;
      if (scheduler == null) {
        buildLock.lock();
        taskGroup->scheduler = scheduler = new TaskScheduler;
      }
    }

    /* worker threads join build */
    if (!buildLock.isLocked())
    {
      if (!join) 
        throw_RTCError(RTC_ERROR_INVALID_OPERATION,"use rtcJoinCommitScene to join a build operation");
      
      scheduler->join();
      return;
    }

    /* initiate build */
    // -- GODOT start --
    // try {
      TaskScheduler::TaskGroupContext context;
      scheduler->spawn_root([&]() { commit_task(); Lock<MutexSys> lock(taskGroup->schedulerMutex); taskGroup->scheduler = nullptr; }, &context, 1, !join);
    // }
    // catch (...) {
    //   accels_clear();
    //   Lock<MutexSys> lock(taskGroup->schedulerMutex);
    //   taskGroup->scheduler = nullptr;
    //   throw;
    // }
    // -- GODOT end --
  }

#endif

#if defined(TASKING_TBB)

  void Scene::commit (bool join) 
  {    
#if defined(TASKING_TBB) && (TBB_INTERFACE_VERSION_MAJOR < 8)
    if (join)
      throw_RTCError(RTC_ERROR_INVALID_OPERATION,"rtcJoinCommitScene not supported with this TBB version");
#endif

    /* try to obtain build lock */
    Lock<MutexSys> lock(buildMutex,buildMutex.try_lock());

    /* join hierarchy build */
    if (!lock.isLocked())
    {
#if !TASKING_TBB_USE_TASK_ISOLATION
      if (!join) 
        throw_RTCError(RTC_ERROR_INVALID_OPERATION,"invoking rtcCommitScene from multiple threads is not supported with this TBB version");
#endif
      
      do {
        device->execute(join, [&](){ taskGroup->group.wait(); });

        pause_cpu();
        yield();
      } while (!buildMutex.try_lock());
      
      buildMutex.unlock();
      return;
    }   

    /* for best performance set FTZ and DAZ flags in the MXCSR control and status register */
    const unsigned int mxcsr = _mm_getcsr();
    _mm_setcsr(mxcsr | /* FTZ */ (1<<15) | /* DAZ */ (1<<6));
    
    try {
#if TBB_INTERFACE_VERSION_MAJOR < 8    
      tbb::task_group_context ctx( tbb::task_group_context::isolated, tbb::task_group_context::default_traits);
#else
      tbb::task_group_context ctx( tbb::task_group_context::isolated, tbb::task_group_context::default_traits | tbb::task_group_context::fp_settings );
#endif
      //ctx.set_priority(tbb::priority_high);
      device->execute(join, [&]()
      {
        taskGroup->group.run([&]{
            tbb::parallel_for (size_t(0), size_t(1), size_t(1), [&] (size_t) { commit_task(); }, ctx);
          });
        taskGroup->group.wait();
      });

      /* reset MXCSR register again */
      _mm_setcsr(mxcsr);
    } 
    catch (...)
    {
      /* reset MXCSR register again */
      _mm_setcsr(mxcsr);
      
      accels_clear();
      throw;
    }
  }
#endif

#if defined(TASKING_PPL)

  void Scene::commit (bool join) 
  {
#if defined(TASKING_PPL)
    if (join)
      throw_RTCError(RTC_ERROR_INVALID_OPERATION,"rtcJoinCommitScene not supported with PPL");
#endif

    /* try to obtain build lock */
    Lock<MutexSys> lock(buildMutex);

    checkIfModifiedAndSet ();
    if (!isModified()) {
      return;
    }

    /* for best performance set FTZ and DAZ flags in the MXCSR control and status register */
    const unsigned int mxcsr = _mm_getcsr();
    _mm_setcsr(mxcsr | /* FTZ */ (1<<15) | /* DAZ */ (1<<6));
    
    try {

      taskGroup->group.run([&]{
          concurrency::parallel_for(size_t(0), size_t(1), size_t(1), [&](size_t) { commit_task(); });
        });
      taskGroup->group.wait();

       /* reset MXCSR register again */
      _mm_setcsr(mxcsr);
    } 
    catch (...)
    {
      /* reset MXCSR register again */
      _mm_setcsr(mxcsr);
      
      accels_clear();
      throw;
    }
  }
#endif

  void Scene::setProgressMonitorFunction(RTCProgressMonitorFunction func, void* ptr) 
  {
    progress_monitor_function = func;
    progress_monitor_ptr      = ptr;
  }

  void Scene::progressMonitor(double dn)
  {
    if (progress_monitor_function) {
      size_t n = size_t(dn) + progress_monitor_counter.fetch_add(size_t(dn));
      if (!progress_monitor_function(progress_monitor_ptr, n / (double(numPrimitives())))) {
        throw_RTCError(RTC_ERROR_CANCELLED,"progress monitor forced termination");
      }
    }
  }
}
