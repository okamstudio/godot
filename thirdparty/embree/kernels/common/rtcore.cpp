// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#define RTC_EXPORT_API

#include "default.h"
#include "device.h"
#include "scene.h"
#include "context.h"
#include "../geometry/filter.h"
#include "../../include/embree4/rtcore_ray.h"
using namespace embree;

RTC_NAMESPACE_BEGIN;

#define RTC_ENTER_DEVICE(arg) \
  DeviceEnterLeave enterleave(arg);

  /* mutex to make API thread safe */
  static MutexSys g_mutex;

  RTC_API RTCDevice rtcNewDevice(const char* config)
  {
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcNewDevice);
    Lock<MutexSys> lock(g_mutex);
    Device* device = new Device(config);
    return (RTCDevice) device->refInc();
    RTC_CATCH_END(nullptr);
    return (RTCDevice) nullptr;
  }

#if defined(EMBREE_SYCL_SUPPORT)

  RTC_API RTCDevice rtcNewSYCLDeviceInternal(sycl::context sycl_context, const char* config)
  {
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcNewSYCLDevice);
    Lock<MutexSys> lock(g_mutex);

    DeviceGPU* device = new DeviceGPU(sycl_context,config);
    return (RTCDevice) device->refInc();
    RTC_CATCH_END(nullptr);
    return (RTCDevice) nullptr;
  }

  RTC_API bool rtcIsSYCLDeviceSupported(const sycl::device device)
  {
    try {
      RTC_TRACE(rtcIsSYCLDeviceSupported);
      return rthwifIsSYCLDeviceSupported(device) > 0;
    } catch (...) {
      return false;
    }
    return false;
  }

  RTC_API int rtcSYCLDeviceSelector(const sycl::device device)
  {
    try {
      RTC_TRACE(rtcSYCLDeviceSelector);
      return rthwifIsSYCLDeviceSupported(device);
    } catch (...) {
      return -1;
    }
    return -1;
  }

  RTC_API void rtcSetDeviceSYCLDevice(RTCDevice hdevice, const sycl::device sycl_device)
  {
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetDeviceSYCLDevice);
    RTC_VERIFY_HANDLE(hdevice);

    Lock<MutexSys> lock(g_mutex);
    
    DeviceGPU* device = dynamic_cast<DeviceGPU*>((Device*) hdevice);
    if (device == nullptr)
      throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "passed device must be an Embree SYCL device")
      
    device->setSYCLDevice(sycl_device);
    
    RTC_CATCH_END(nullptr);
  }

#endif

  RTC_API void rtcRetainDevice(RTCDevice hdevice) 
  {
    Device* device = (Device*) hdevice;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcRetainDevice);
    RTC_VERIFY_HANDLE(hdevice);
    Lock<MutexSys> lock(g_mutex);
    device->refInc();
    RTC_CATCH_END(nullptr);
  }
  
  RTC_API void rtcReleaseDevice(RTCDevice hdevice) 
  {
    Device* device = (Device*) hdevice;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcReleaseDevice);
    RTC_VERIFY_HANDLE(hdevice);
    Lock<MutexSys> lock(g_mutex);
    device->refDec();
    RTC_CATCH_END(nullptr);
  }
  
  RTC_API ssize_t rtcGetDeviceProperty(RTCDevice hdevice, RTCDeviceProperty prop)
  {
    Device* device = (Device*) hdevice;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetDeviceProperty);
    RTC_VERIFY_HANDLE(hdevice);
    Lock<MutexSys> lock(g_mutex);
    return device->getProperty(prop);
    RTC_CATCH_END(device);
    return 0;
  }

  RTC_API void rtcSetDeviceProperty(RTCDevice hdevice, const RTCDeviceProperty prop, ssize_t val)
  {
    Device* device = (Device*) hdevice;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetDeviceProperty);
    const bool internal_prop = (size_t)prop >= 1000000 && (size_t)prop < 1000004;
    if (!internal_prop) RTC_VERIFY_HANDLE(hdevice); // allow NULL device for special internal settings
    Lock<MutexSys> lock(g_mutex);
    device->setProperty(prop,val);
    RTC_CATCH_END(device);
  }

  RTC_API RTCError rtcGetDeviceError(RTCDevice hdevice)
  {
    Device* device = (Device*) hdevice;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetDeviceError);
    if (device == nullptr) return Device::getThreadErrorCode();
    else                   return device->getDeviceErrorCode();
    RTC_CATCH_END(device);
    return RTC_ERROR_UNKNOWN;
  }

  RTC_API const char* rtcGetDeviceLastErrorMessage(RTCDevice hdevice)
  {
    Device* device = (Device*) hdevice;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetDeviceLastErrorMessage);
    if (device == nullptr) return Device::getThreadLastErrorMessage();
    else                   return device->getDeviceLastErrorMessage();
    RTC_CATCH_END(device);
    return "";
  }

  RTC_API void rtcSetDeviceErrorFunction(RTCDevice hdevice, RTCErrorFunction error, void* userPtr)
  {
    Device* device = (Device*) hdevice;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetDeviceErrorFunction);
    RTC_VERIFY_HANDLE(hdevice);
    device->setErrorFunction(error, userPtr);
    RTC_CATCH_END(device);
  }

  RTC_API void rtcSetDeviceMemoryMonitorFunction(RTCDevice hdevice, RTCMemoryMonitorFunction memoryMonitor, void* userPtr)
  {
    Device* device = (Device*) hdevice;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetDeviceMemoryMonitorFunction);
    device->setMemoryMonitorFunction(memoryMonitor, userPtr);
    RTC_CATCH_END(device);
  }

  RTC_API RTCBuffer rtcNewBuffer(RTCDevice hdevice, size_t byteSize)
  {
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcNewBuffer);
    RTC_VERIFY_HANDLE(hdevice);
    RTC_ENTER_DEVICE(hdevice);
    Buffer* buffer = new Buffer((Device*)hdevice, byteSize);
    return (RTCBuffer)buffer->refInc();
    RTC_CATCH_END((Device*)hdevice);
    return nullptr;
  }

  RTC_API RTCBuffer rtcNewSharedBuffer(RTCDevice hdevice, void* ptr, size_t byteSize)
  {
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcNewSharedBuffer);
    RTC_VERIFY_HANDLE(hdevice);
    RTC_ENTER_DEVICE(hdevice);
    Buffer* buffer = new Buffer((Device*)hdevice, byteSize, ptr);
    return (RTCBuffer)buffer->refInc();
    RTC_CATCH_END((Device*)hdevice);
    return nullptr;
  }

  RTC_API void* rtcGetBufferData(RTCBuffer hbuffer)
  {
    Buffer* buffer = (Buffer*)hbuffer;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetBufferData);
    RTC_VERIFY_HANDLE(hbuffer);
    RTC_ENTER_DEVICE(hbuffer);
    return buffer->data();
    RTC_CATCH_END2(buffer);
    return nullptr;
  }

  RTC_API void rtcRetainBuffer(RTCBuffer hbuffer)
  {
    Buffer* buffer = (Buffer*)hbuffer;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcRetainBuffer);
    RTC_VERIFY_HANDLE(hbuffer);
    RTC_ENTER_DEVICE(hbuffer);
    buffer->refInc();
    RTC_CATCH_END2(buffer);
  }
  
  RTC_API void rtcReleaseBuffer(RTCBuffer hbuffer)
  {
    Buffer* buffer = (Buffer*)hbuffer;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcReleaseBuffer);
    RTC_VERIFY_HANDLE(hbuffer);
    RTC_ENTER_DEVICE(hbuffer);
    buffer->refDec();
    RTC_CATCH_END2(buffer);
  }

  RTC_API RTCScene rtcNewScene (RTCDevice hdevice) 
  {
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcNewScene);
    RTC_VERIFY_HANDLE(hdevice);
    RTC_ENTER_DEVICE(hdevice);
    Scene* scene = new Scene((Device*)hdevice);
    return (RTCScene) scene->refInc();
    RTC_CATCH_END((Device*)hdevice);
    return nullptr;
  }

  RTC_API RTCDevice rtcGetSceneDevice(RTCScene hscene)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetSceneDevice);
    RTC_VERIFY_HANDLE(hscene);
    return (RTCDevice)scene->device->refInc(); // user will own one additional device reference
    RTC_CATCH_END2(scene);
    return (RTCDevice)nullptr;
  }

  RTC_API void rtcSetSceneProgressMonitorFunction(RTCScene hscene, RTCProgressMonitorFunction progress, void* ptr) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetSceneProgressMonitorFunction);
    RTC_VERIFY_HANDLE(hscene);
    RTC_ENTER_DEVICE(hscene);
    Lock<MutexSys> lock(g_mutex);
    scene->setProgressMonitorFunction(progress,ptr);
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcSetSceneBuildQuality (RTCScene hscene, RTCBuildQuality quality) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetSceneBuildQuality);
    RTC_VERIFY_HANDLE(hscene);
    RTC_ENTER_DEVICE(hscene);
    // -- GODOT start --
    // if (quality != RTC_BUILD_QUALITY_LOW &&
    //     quality != RTC_BUILD_QUALITY_MEDIUM &&
    //     quality != RTC_BUILD_QUALITY_HIGH)
    //   throw std::runtime_error("invalid build quality");
    if (quality != RTC_BUILD_QUALITY_LOW &&
        quality != RTC_BUILD_QUALITY_MEDIUM &&
        quality != RTC_BUILD_QUALITY_HIGH) {
      abort();
    }
    // -- GODOT end --
    scene->setBuildQuality(quality);
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcSetSceneFlags (RTCScene hscene, RTCSceneFlags flags) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetSceneFlags);
    RTC_VERIFY_HANDLE(hscene);
    RTC_ENTER_DEVICE(hscene);
    scene->setSceneFlags(flags);
    RTC_CATCH_END2(scene);
  }

  RTC_API RTCSceneFlags rtcGetSceneFlags(RTCScene hscene)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetSceneFlags);
    RTC_VERIFY_HANDLE(hscene);
    //RTC_ENTER_DEVICE(hscene);
    return scene->getSceneFlags();
    RTC_CATCH_END2(scene);
    return RTC_SCENE_FLAG_NONE;
  }

  RTC_API_EXTERN_C bool prefetchUSMSharedOnGPU(RTCScene scene);

  RTC_API void rtcCommitScene (RTCScene hscene)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcCommitScene);
    RTC_VERIFY_HANDLE(hscene);
    RTC_ENTER_DEVICE(hscene);
    scene->commit(false);

#if defined(EMBREE_SYCL_SUPPORT)
    prefetchUSMSharedOnGPU(hscene);
#endif

    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcJoinCommitScene (RTCScene hscene) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcJoinCommitScene);
    RTC_VERIFY_HANDLE(hscene);
    RTC_ENTER_DEVICE(hscene);
    scene->commit(true);
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcGetSceneBounds(RTCScene hscene, RTCBounds* bounds_o)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetSceneBounds);
    RTC_VERIFY_HANDLE(hscene);
    RTC_ENTER_DEVICE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene not committed");
    BBox3fa bounds = scene->bounds.bounds();
    bounds_o->lower_x = bounds.lower.x;
    bounds_o->lower_y = bounds.lower.y;
    bounds_o->lower_z = bounds.lower.z;
    bounds_o->align0  = 0;
    bounds_o->upper_x = bounds.upper.x;
    bounds_o->upper_y = bounds.upper.y;
    bounds_o->upper_z = bounds.upper.z;
    bounds_o->align1  = 0;
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcGetSceneLinearBounds(RTCScene hscene, RTCLinearBounds* bounds_o)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetSceneBounds);
    RTC_VERIFY_HANDLE(hscene);
    RTC_ENTER_DEVICE(hscene);
    if (bounds_o == nullptr)
      throw_RTCError(RTC_ERROR_INVALID_OPERATION,"invalid destination pointer");
    if (scene->isModified())
      throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene not committed");
    
    bounds_o->bounds0.lower_x = scene->bounds.bounds0.lower.x;
    bounds_o->bounds0.lower_y = scene->bounds.bounds0.lower.y;
    bounds_o->bounds0.lower_z = scene->bounds.bounds0.lower.z;
    bounds_o->bounds0.align0  = 0;
    bounds_o->bounds0.upper_x = scene->bounds.bounds0.upper.x;
    bounds_o->bounds0.upper_y = scene->bounds.bounds0.upper.y;
    bounds_o->bounds0.upper_z = scene->bounds.bounds0.upper.z;
    bounds_o->bounds0.align1  = 0;
    bounds_o->bounds1.lower_x = scene->bounds.bounds1.lower.x;
    bounds_o->bounds1.lower_y = scene->bounds.bounds1.lower.y;
    bounds_o->bounds1.lower_z = scene->bounds.bounds1.lower.z;
    bounds_o->bounds1.align0  = 0;
    bounds_o->bounds1.upper_x = scene->bounds.bounds1.upper.x;
    bounds_o->bounds1.upper_y = scene->bounds.bounds1.upper.y;
    bounds_o->bounds1.upper_z = scene->bounds.bounds1.upper.z;
    bounds_o->bounds1.align1  = 0;
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcCollide (RTCScene hscene0, RTCScene hscene1, RTCCollideFunc callback, void* userPtr)
  {
    Scene* scene0 = (Scene*) hscene0;
    Scene* scene1 = (Scene*) hscene1;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcCollide);
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene0);
    RTC_VERIFY_HANDLE(hscene1);
    if (scene0->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    if (scene1->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    if (scene0->device != scene1->device) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scenes are from different devices");
    auto nUserPrims0 = scene0->getNumPrimitives (Geometry::MTY_USER_GEOMETRY, false);
    auto nUserPrims1 = scene1->getNumPrimitives (Geometry::MTY_USER_GEOMETRY, false);
    if (scene0->numPrimitives() != nUserPrims0 && scene1->numPrimitives() != nUserPrims1) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scenes must only contain user geometries with a single timestep");
#endif
    scene0->intersectors.collide(scene0,scene1,callback,userPtr);
    RTC_CATCH_END(scene0->device);
  }
  
  inline bool pointQuery(Scene* scene, RTCPointQuery* query, RTCPointQueryContext* userContext, RTCPointQueryFunction queryFunc, void* userPtr)
  {
    bool changed = false;
    if (userContext->instStackSize > 0)
    {
      const AffineSpace3fa transform = AffineSpace3fa_load_unaligned((AffineSpace3fa*)userContext->world2inst[userContext->instStackSize-1]);

      float similarityScale = 0.f;
      const bool similtude = similarityTransform(transform, &similarityScale);
      assert((similtude && similarityScale > 0) || (!similtude && similarityScale == 0.f));

      PointQuery query_inst;
      query_inst.p = xfmPoint(transform, Vec3fa(query->x, query->y, query->z)); 
      query_inst.radius = query->radius * similarityScale;
      query_inst.time = query->time;
      
      PointQueryContext context_inst(scene, (PointQuery*)query,
        similtude ? POINT_QUERY_TYPE_SPHERE : POINT_QUERY_TYPE_AABB,
        queryFunc, userContext, similarityScale, userPtr);
      changed = scene->intersectors.pointQuery((PointQuery*)&query_inst, &context_inst);
    }
    else
    {
      PointQueryContext context(scene, (PointQuery*)query, 
        POINT_QUERY_TYPE_SPHERE, queryFunc, userContext, 1.f, userPtr);
      changed = scene->intersectors.pointQuery((PointQuery*)query, &context);
    }
    return changed;
  }

  RTC_API bool rtcPointQuery(RTCScene hscene, RTCPointQuery* query, RTCPointQueryContext* userContext, RTCPointQueryFunction queryFunc, void* userPtr)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcPointQuery);
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    RTC_VERIFY_HANDLE(userContext);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    if (((size_t)query) & 0x0F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "query not aligned to 16 bytes");   
    if (((size_t)userContext) & 0x0F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "context not aligned to 16 bytes");   
#endif

    return pointQuery(scene, query, userContext, queryFunc, userPtr);
    RTC_CATCH_END2_FALSE(scene);
  }
  
  RTC_API bool rtcPointQuery4 (const int* valid, RTCScene hscene, RTCPointQuery4* query, struct RTCPointQueryContext* userContext, RTCPointQueryFunction queryFunc, void** userPtrN)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcPointQuery4);

#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    if (((size_t)valid) & 0x0F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "mask not aligned to 16 bytes");   
    if (((size_t)query) & 0x0F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "query not aligned to 16 bytes");   
#endif
    STAT(size_t cnt=0; for (size_t i=0; i<4; i++) cnt += ((int*)valid)[i] == -1;);
    STAT3(point_query.travs,cnt,cnt,cnt);

    bool changed = false;
    PointQuery4* query4 = (PointQuery4*)query;
    PointQuery query1; 
    for (size_t i=0; i<4; i++) {
      if (!valid[i]) continue;
      query4->get(i,query1);
      changed |= pointQuery(scene, (RTCPointQuery*)&query1, userContext, queryFunc, userPtrN?userPtrN[i]:NULL);
      query4->set(i,query1);
    }
    return changed;
    RTC_CATCH_END2_FALSE(scene);
  }
  
  RTC_API bool rtcPointQuery8 (const int* valid, RTCScene hscene, RTCPointQuery8* query, struct RTCPointQueryContext* userContext, RTCPointQueryFunction queryFunc, void** userPtrN)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcPointQuery8);
    
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    if (((size_t)valid) & 0x0F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "mask not aligned to 16 bytes");   
    if (((size_t)query) & 0x0F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "query not aligned to 16 bytes");   
#endif
    STAT(size_t cnt=0; for (size_t i=0; i<4; i++) cnt += ((int*)valid)[i] == -1;);
    STAT3(point_query.travs,cnt,cnt,cnt);

    bool changed = false;
    PointQuery8* query8 = (PointQuery8*)query;
    PointQuery query1; 
    for (size_t i=0; i<8; i++) {
      if (!valid[i]) continue;
      query8->get(i,query1);
      changed |= pointQuery(scene, (RTCPointQuery*)&query1, userContext, queryFunc, userPtrN?userPtrN[i]:NULL);
      query8->set(i,query1);
    }
    return changed;
    RTC_CATCH_END2_FALSE(scene);
  }

  RTC_API bool rtcPointQuery16 (const int* valid, RTCScene hscene, RTCPointQuery16* query, struct RTCPointQueryContext* userContext, RTCPointQueryFunction queryFunc, void** userPtrN)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcPointQuery16);

#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    if (((size_t)valid) & 0x0F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "mask not aligned to 16 bytes");   
    if (((size_t)query) & 0x0F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "query not aligned to 16 bytes");   
#endif
    STAT(size_t cnt=0; for (size_t i=0; i<4; i++) cnt += ((int*)valid)[i] == -1;);
    STAT3(point_query.travs,cnt,cnt,cnt);

    bool changed = false;
    PointQuery16* query16 = (PointQuery16*)query;
    PointQuery query1; 
    for (size_t i=0; i<16; i++) {
      if (!valid[i]) continue;
      PointQuery query1; query16->get(i,query1);
      changed |= pointQuery(scene, (RTCPointQuery*)&query1, userContext, queryFunc, userPtrN?userPtrN[i]:NULL);
      query16->set(i,query1);
    }
    return changed;
    RTC_CATCH_END2_FALSE(scene);
  }

  RTC_API void rtcIntersect1 (RTCScene hscene, RTCRayHit* rayhit, RTCIntersectArguments* args) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcIntersect1);
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene not committed");
    if (((size_t)rayhit) & 0x0F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "ray not aligned to 16 bytes");   
#endif
    STAT3(normal.travs,1,1,1);

    RTCIntersectArguments defaultArgs;
    if (unlikely(args == nullptr)) {
      rtcInitIntersectArguments(&defaultArgs);
      args = &defaultArgs;
    }
    RTCRayQueryContext* user_context = args->context;
    
    RTCRayQueryContext defaultContext;
    if (unlikely(user_context == nullptr)) {
      rtcInitRayQueryContext(&defaultContext);
      user_context = &defaultContext;
    }
    RayQueryContext context(scene,user_context,args);
    
    scene->intersectors.intersect(*rayhit,&context);
#if defined(DEBUG)
    ((RayHit*)rayhit)->verifyHit();
#endif
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcForwardIntersect1 (const RTCIntersectFunctionNArguments* args, RTCScene hscene, RTCRay* iray_, unsigned int instID)
  {
    rtcForwardIntersect1Ex(args, hscene, iray_, instID, 0);
  }

  RTC_API void rtcForwardIntersect1Ex(const RTCIntersectFunctionNArguments* args, RTCScene hscene, RTCRay* iray_, unsigned int instID, unsigned int instPrimID)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcForwardIntersect1Ex);
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene not committed");
    if (((size_t)iray_) & 0x0F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "ray not aligned to 16 bytes");
#endif

    Ray* iray = (Ray*) iray_;
    RayHit* oray = (RayHit*)args->rayhit;
    RTCRayQueryContext* user_context = args->context;
    const Vec3ff ray_org_tnear = oray->org;
    const Vec3ff ray_dir_time = oray->dir;
    oray->org = iray->org;
    oray->dir = iray->dir;
    STAT3(normal.travs,1,1,1);

    RTCIntersectArguments* iargs = ((IntersectFunctionNArguments*) args)->args;
    RayQueryContext context(scene,user_context,iargs);

    instance_id_stack::push(user_context, instID, instPrimID);
    scene->intersectors.intersect(*(RTCRayHit*)oray,&context);
    instance_id_stack::pop(user_context);

    oray->org = ray_org_tnear;
    oray->dir = ray_dir_time;

    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcIntersect4 (const int* valid, RTCScene hscene, RTCRayHit4* rayhit, RTCIntersectArguments* args) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcIntersect4);

#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene not committed");
    if (((size_t)valid) & 0x0F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "mask not aligned to 16 bytes");   
    if (((size_t)rayhit)   & 0x0F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit not aligned to 16 bytes");   
#endif
    STAT(size_t cnt=0; for (size_t i=0; i<4; i++) cnt += ((int*)valid)[i] == -1;);
    STAT3(normal.travs,cnt,cnt,cnt);

    RTCIntersectArguments defaultArgs;
    if (unlikely(args == nullptr)) {
      rtcInitIntersectArguments(&defaultArgs);
      args = &defaultArgs;
    }
    RTCRayQueryContext* user_context = args->context;
    
    RTCRayQueryContext defaultContext;
    if (unlikely(user_context == nullptr)) {
      rtcInitRayQueryContext(&defaultContext);
      user_context = &defaultContext;
    }
    RayQueryContext context(scene,user_context,args);

    if (likely(scene->intersectors.intersector4))
      scene->intersectors.intersect4(valid,*rayhit,&context);

    else {
      RayHit4* ray4 = (RayHit4*) rayhit;
      for (size_t i=0; i<4; i++) {
        if (!valid[i]) continue;
        RayHit ray1; ray4->get(i,ray1);
        scene->intersectors.intersect((RTCRayHit&)ray1,&context);
        ray4->set(i,ray1);
      }
    }
    
    RTC_CATCH_END2(scene);
  }

  template<int N> void copy(float* dst, float* src);

  template<>
  __forceinline void copy<4>(float* dst, float* src) {
    vfloat4::storeu(&dst[0],vfloat4::loadu(&src[0]));
  }

  template<>
  __forceinline void copy<8>(float* dst, float* src) {
    vfloat4::storeu(&dst[0],vfloat4::loadu(&src[0]));
    vfloat4::storeu(&dst[4],vfloat4::loadu(&src[4]));
  }

  template<>
  __forceinline void copy<16>(float* dst, float* src) {
    vfloat4::storeu(&dst[0],vfloat4::loadu(&src[0]));
    vfloat4::storeu(&dst[4],vfloat4::loadu(&src[4]));
    vfloat4::storeu(&dst[8],vfloat4::loadu(&src[8]));
    vfloat4::storeu(&dst[12],vfloat4::loadu(&src[12]));
  }

  template<typename RTCRay, typename RTCRayHit, int N>
  __forceinline void rtcForwardIntersectN(const int* valid, const RTCIntersectFunctionNArguments* args, RTCScene hscene, RTCRay* iray, unsigned int instID, unsigned int instPrimID)
  {
    Scene* scene = (Scene*) hscene;
    RTCRayHit* oray = (RTCRayHit*)args->rayhit;
    RTCRayQueryContext* user_context = args->context;

    __aligned(16) float ray_org_x[N];
    __aligned(16) float ray_org_y[N];
    __aligned(16) float ray_org_z[N];
    __aligned(16) float ray_dir_x[N];
    __aligned(16) float ray_dir_y[N];
    __aligned(16) float ray_dir_z[N];
    
    copy<N>(ray_org_x,oray->ray.org_x);
    copy<N>(ray_org_y,oray->ray.org_y);
    copy<N>(ray_org_z,oray->ray.org_z);
    copy<N>(ray_dir_x,oray->ray.dir_x);
    copy<N>(ray_dir_y,oray->ray.dir_y);
    copy<N>(ray_dir_z,oray->ray.dir_z);
    
    copy<N>(oray->ray.org_x,iray->org_x);
    copy<N>(oray->ray.org_y,iray->org_y);
    copy<N>(oray->ray.org_z,iray->org_z);
    copy<N>(oray->ray.dir_x,iray->dir_x);
    copy<N>(oray->ray.dir_y,iray->dir_y);
    copy<N>(oray->ray.dir_z,iray->dir_z);
    
    STAT(size_t cnt=0; for (size_t i=0; i<N; i++) cnt += ((int*)valid)[i] == -1;);
    STAT3(normal.travs,cnt,cnt,cnt);

    RTCIntersectArguments* iargs = ((IntersectFunctionNArguments*) args)->args;
    RayQueryContext context(scene,user_context,iargs);

    instance_id_stack::push(user_context, instID, instPrimID);
    scene->intersectors.intersect(valid,*oray,&context);
    instance_id_stack::pop(user_context);

    copy<N>(oray->ray.org_x,ray_org_x);
    copy<N>(oray->ray.org_y,ray_org_y);
    copy<N>(oray->ray.org_z,ray_org_z);
    copy<N>(oray->ray.dir_x,ray_dir_x);
    copy<N>(oray->ray.dir_y,ray_dir_y);
    copy<N>(oray->ray.dir_z,ray_dir_z);
  }

  RTC_API void rtcForwardIntersect4(const int* valid, const RTCIntersectFunctionNArguments* args, RTCScene hscene, RTCRay4* iray, unsigned int instID)
  {
    RTC_TRACE(rtcForwardIntersect4);
    return rtcForwardIntersect4Ex(valid, args, hscene, iray, instID, 0);
  }

  RTC_API void rtcForwardIntersect4Ex(const int* valid, const RTCIntersectFunctionNArguments* args, RTCScene hscene, RTCRay4* iray, unsigned int instID, unsigned int instPrimID)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcForwardIntersect4);
    rtcForwardIntersectN<RTCRay4,RTCRayHit4,4>(valid,args,hscene,iray,instID,instPrimID);
    RTC_CATCH_END2(scene);
  }
  
  RTC_API void rtcIntersect8 (const int* valid, RTCScene hscene, RTCRayHit8* rayhit, RTCIntersectArguments* args) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcIntersect8);

#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene not committed");
    if (((size_t)valid) & 0x1F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "mask not aligned to 32 bytes");   
    if (((size_t)rayhit)   & 0x1F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit not aligned to 32 bytes");   
#endif
    STAT(size_t cnt=0; for (size_t i=0; i<8; i++) cnt += ((int*)valid)[i] == -1;);
    STAT3(normal.travs,cnt,cnt,cnt);

    RTCIntersectArguments defaultArgs;
    if (unlikely(args == nullptr)) {
      rtcInitIntersectArguments(&defaultArgs);
      args = &defaultArgs;
    }
    RTCRayQueryContext* user_context = args->context;
    
    RTCRayQueryContext defaultContext;
    if (unlikely(user_context == nullptr)) {
      rtcInitRayQueryContext(&defaultContext);
      user_context = &defaultContext;
    }
    RayQueryContext context(scene,user_context,args);
    
    if (likely(scene->intersectors.intersector8)) 
      scene->intersectors.intersect8(valid,*rayhit,&context);
    
    else
    {
      RayHit8* ray8 = (RayHit8*) rayhit;
      for (size_t i=0; i<8; i++) {
        if (!valid[i]) continue;
        RayHit ray1; ray8->get(i,ray1);
        scene->intersectors.intersect((RTCRayHit&)ray1,&context);
        ray8->set(i,ray1);
      }
    }
    
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcForwardIntersect8(const int* valid, const RTCIntersectFunctionNArguments* args, RTCScene hscene, RTCRay8* iray, unsigned int instID)
  {
    RTC_TRACE(rtcForwardIntersect8);
    return rtcForwardIntersect8Ex(valid, args, hscene, iray, instID, 0);
  }

  RTC_API void rtcForwardIntersect8Ex(const int* valid, const RTCIntersectFunctionNArguments* args, RTCScene hscene, RTCRay8* iray, unsigned int instID, unsigned int instPrimID)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcForwardIntersect8Ex);
    rtcForwardIntersectN<RTCRay8,RTCRayHit8,8>(valid,args,hscene,iray,instID,instPrimID);
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcIntersect16 (const int* valid, RTCScene hscene, RTCRayHit16* rayhit, RTCIntersectArguments* args) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcIntersect16);

#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene not committed");
    if (((size_t)valid) & 0x3F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "mask not aligned to 64 bytes");   
    if (((size_t)rayhit)   & 0x3F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit not aligned to 64 bytes");   
#endif
    STAT(size_t cnt=0; for (size_t i=0; i<16; i++) cnt += ((int*)valid)[i] == -1;);
    STAT3(normal.travs,cnt,cnt,cnt);

    RTCIntersectArguments defaultArgs;
    if (unlikely(args == nullptr)) {
      rtcInitIntersectArguments(&defaultArgs);
      args = &defaultArgs;
    }
    RTCRayQueryContext* user_context = args->context;
    
    RTCRayQueryContext defaultContext;
    if (unlikely(user_context == nullptr)) {
      rtcInitRayQueryContext(&defaultContext);
      user_context = &defaultContext;
    }
    RayQueryContext context(scene,user_context,args);

    if (likely(scene->intersectors.intersector16))
      scene->intersectors.intersect16(valid,*rayhit,&context);

    else {
      RayHit16* ray16 = (RayHit16*) rayhit;
      for (size_t i=0; i<16; i++) {
        if (!valid[i]) continue;
        RayHit ray1; ray16->get(i,ray1);
        scene->intersectors.intersect((RTCRayHit&)ray1,&context);
        ray16->set(i,ray1);
      }
    }

    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcForwardIntersect16(const int* valid, const RTCIntersectFunctionNArguments* args, RTCScene hscene, RTCRay16* iray, unsigned int instID)
  {
    RTC_TRACE(rtcForwardIntersect16);
    return rtcForwardIntersect16Ex(valid, args, hscene, iray, instID, 0);
  }

  RTC_API void rtcForwardIntersect16Ex(const int* valid, const RTCIntersectFunctionNArguments* args, RTCScene hscene, RTCRay16* iray, unsigned int instID, unsigned int instPrimID)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcForwardIntersect16Ex);
    rtcForwardIntersectN<RTCRay16,RTCRayHit16,16>(valid,args,hscene,iray,instID,instPrimID);
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcOccluded1 (RTCScene hscene, RTCRay* ray, RTCOccludedArguments* args) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcOccluded1);
    STAT3(shadow.travs,1,1,1);
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene not committed");
    if (((size_t)ray) & 0x0F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "ray not aligned to 16 bytes");   
#endif

    RTCOccludedArguments defaultArgs;
    if (unlikely(args == nullptr)) {
      rtcInitOccludedArguments(&defaultArgs);
      args = &defaultArgs;
    }
    RTCRayQueryContext* user_context = args->context;
    
    RTCRayQueryContext defaultContext;
    if (unlikely(user_context == nullptr)) {
      rtcInitRayQueryContext(&defaultContext);
      user_context = &defaultContext;
    }
    RayQueryContext context(scene,user_context,args);
    
    scene->intersectors.occluded(*ray,&context);
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcForwardOccluded1 (const RTCOccludedFunctionNArguments* args, RTCScene hscene, RTCRay* iray_, unsigned int instID)
  {
    RTC_TRACE(rtcForwardOccluded1);
    return rtcForwardOccluded1Ex(args, hscene, iray_, instID, 0);
  }

  RTC_API void rtcForwardOccluded1Ex(const RTCOccludedFunctionNArguments* args, RTCScene hscene, RTCRay* iray_, unsigned int instID, unsigned int instPrimID)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcForwardOccluded1Ex);
    STAT3(shadow.travs,1,1,1);
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene not committed");
    if (((size_t)iray_) & 0x0F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "ray not aligned to 16 bytes");   
#endif
    
    Ray* iray = (Ray*)iray_;
    Ray* oray = (Ray*)args->ray;
    RTCRayQueryContext* user_context = args->context;
    const Vec3ff ray_org_tnear = oray->org;
    const Vec3ff ray_dir_time = oray->dir;
    oray->org = iray->org;
    oray->dir = iray->dir;

    RTCIntersectArguments* iargs = ((OccludedFunctionNArguments*) args)->args;
    RayQueryContext context(scene,user_context,iargs);

    instance_id_stack::push(user_context, instID, instPrimID);
    scene->intersectors.occluded(*(RTCRay*)oray,&context);
    instance_id_stack::pop(user_context);
    
    oray->org = ray_org_tnear;
    oray->dir = ray_dir_time;

    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcOccluded4 (const int* valid, RTCScene hscene, RTCRay4* ray, RTCOccludedArguments* args) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcOccluded4);

#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene not committed");
    if (((size_t)valid) & 0x0F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "mask not aligned to 16 bytes");   
    if (((size_t)ray)   & 0x0F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "ray not aligned to 16 bytes");   
#endif
    STAT(size_t cnt=0; for (size_t i=0; i<4; i++) cnt += ((int*)valid)[i] == -1;);
    STAT3(shadow.travs,cnt,cnt,cnt);

    RTCOccludedArguments defaultArgs;
    if (unlikely(args == nullptr)) {
      rtcInitOccludedArguments(&defaultArgs);
      args = &defaultArgs;
    }
    RTCRayQueryContext* user_context = args->context;
    
    RTCRayQueryContext defaultContext;
    if (unlikely(user_context == nullptr)) {
      rtcInitRayQueryContext(&defaultContext);
      user_context = &defaultContext;
    }
    RayQueryContext context(scene,user_context,args);

    if (likely(scene->intersectors.intersector4))
       scene->intersectors.occluded4(valid,*ray,&context);

    else {
      RayHit4* ray4 = (RayHit4*) ray;
      for (size_t i=0; i<4; i++) {
        if (!valid[i]) continue;
        RayHit ray1; ray4->get(i,ray1);
        scene->intersectors.occluded((RTCRay&)ray1,&context);
        ray4->geomID[i] = ray1.geomID; 
      }
    }
    
    RTC_CATCH_END2(scene);
  }

  template<typename RTCRay, int N>
  __forceinline void rtcForwardOccludedN (const int* valid, const RTCOccludedFunctionNArguments* args, RTCScene hscene, RTCRay* iray, unsigned int instID, unsigned int instPrimID)
  {
    Scene* scene = (Scene*) hscene;
    RTCRay* oray = (RTCRay*)args->ray;
    RTCRayQueryContext* user_context = args->context;

    __aligned(16) float ray_org_x[N];
    __aligned(16) float ray_org_y[N];
    __aligned(16) float ray_org_z[N];
    __aligned(16) float ray_dir_x[N];
    __aligned(16) float ray_dir_y[N];
    __aligned(16) float ray_dir_z[N];
    
    copy<N>(ray_org_x,oray->org_x);
    copy<N>(ray_org_y,oray->org_y);
    copy<N>(ray_org_z,oray->org_z);
    copy<N>(ray_dir_x,oray->dir_x);
    copy<N>(ray_dir_y,oray->dir_y);
    copy<N>(ray_dir_z,oray->dir_z);
    
    copy<N>(oray->org_x,iray->org_x);
    copy<N>(oray->org_y,iray->org_y);
    copy<N>(oray->org_z,iray->org_z);
    copy<N>(oray->dir_x,iray->dir_x);
    copy<N>(oray->dir_y,iray->dir_y);
    copy<N>(oray->dir_z,iray->dir_z);
    
    STAT(size_t cnt=0; for (size_t i=0; i<N; i++) cnt += ((int*)valid)[i] == -1;);
    STAT3(normal.travs,cnt,cnt,cnt);

    RTCIntersectArguments* iargs = ((IntersectFunctionNArguments*) args)->args;
    RayQueryContext context(scene,user_context,iargs);

    instance_id_stack::push(user_context, instID, instPrimID);
    scene->intersectors.occluded(valid,*oray,&context);
    instance_id_stack::pop(user_context);

    copy<N>(oray->org_x,ray_org_x);
    copy<N>(oray->org_y,ray_org_y);
    copy<N>(oray->org_z,ray_org_z);
    copy<N>(oray->dir_x,ray_dir_x);
    copy<N>(oray->dir_y,ray_dir_y);
    copy<N>(oray->dir_z,ray_dir_z);
  }

  RTC_API void rtcForwardOccluded4(const int* valid, const RTCOccludedFunctionNArguments* args, RTCScene hscene, RTCRay4* iray, unsigned int instID)
  {
    RTC_TRACE(rtcForwardOccluded4);
    return rtcForwardOccluded4Ex(valid, args, hscene, iray, instID, 0);
  }

  RTC_API void rtcForwardOccluded4Ex(const int* valid, const RTCOccludedFunctionNArguments* args, RTCScene hscene, RTCRay4* iray, unsigned int instID, unsigned int instPrimID)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcForwardOccluded4);
    rtcForwardOccludedN<RTCRay4,4>(valid,args,hscene,iray,instID,instPrimID);
    RTC_CATCH_END2(scene);
  }
 
  RTC_API void rtcOccluded8 (const int* valid, RTCScene hscene, RTCRay8* ray, RTCOccludedArguments* args) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcOccluded8);

#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene not committed");
    if (((size_t)valid) & 0x1F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "mask not aligned to 32 bytes");   
    if (((size_t)ray)   & 0x1F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "ray not aligned to 32 bytes");   
#endif
    STAT(size_t cnt=0; for (size_t i=0; i<8; i++) cnt += ((int*)valid)[i] == -1;);
    STAT3(shadow.travs,cnt,cnt,cnt);

    RTCOccludedArguments defaultArgs;
    if (unlikely(args == nullptr)) {
      rtcInitOccludedArguments(&defaultArgs);
      args = &defaultArgs;
    }
    RTCRayQueryContext* user_context = args->context;
    
    RTCRayQueryContext defaultContext;
    if (unlikely(user_context == nullptr)) {
      rtcInitRayQueryContext(&defaultContext);
      user_context = &defaultContext;
    }
    RayQueryContext context(scene,user_context,args);

    if (likely(scene->intersectors.intersector8))
      scene->intersectors.occluded8(valid,*ray,&context);

    else {
      RayHit8* ray8 = (RayHit8*) ray;
      for (size_t i=0; i<8; i++) {
        if (!valid[i]) continue;
        RayHit ray1; ray8->get(i,ray1);
        scene->intersectors.occluded((RTCRay&)ray1,&context);
        ray8->set(i,ray1);
      }
    }

    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcForwardOccluded8(const int* valid, const RTCOccludedFunctionNArguments* args, RTCScene hscene, RTCRay8* iray, unsigned int instID)
  {
    RTC_TRACE(rtcForwardOccluded8);
    return rtcForwardOccluded8Ex(valid, args, hscene, iray, instID, 0);
  }

  RTC_API void rtcForwardOccluded8Ex(const int* valid, const RTCOccludedFunctionNArguments* args, RTCScene hscene, RTCRay8* iray, unsigned int instID, unsigned int instPrimID)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcForwardOccluded8Ex);
    rtcForwardOccludedN<RTCRay8,8>(valid, args, hscene, iray, instID, instPrimID);
    RTC_CATCH_END2(scene);
  }
   
  RTC_API void rtcOccluded16 (const int* valid, RTCScene hscene, RTCRay16* ray, RTCOccludedArguments* args) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcOccluded16);

#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene not committed");
    if (((size_t)valid) & 0x3F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "mask not aligned to 64 bytes");   
    if (((size_t)ray)   & 0x3F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "ray not aligned to 64 bytes");   
#endif
    STAT(size_t cnt=0; for (size_t i=0; i<16; i++) cnt += ((int*)valid)[i] == -1;);
    STAT3(shadow.travs,cnt,cnt,cnt);

    RTCOccludedArguments defaultArgs;
    if (unlikely(args == nullptr)) {
      rtcInitOccludedArguments(&defaultArgs);
      args = &defaultArgs;
    }
    RTCRayQueryContext* user_context = args->context;
    
    RTCRayQueryContext defaultContext;
    if (unlikely(user_context == nullptr)) {
      rtcInitRayQueryContext(&defaultContext);
      user_context = &defaultContext;
    }
    RayQueryContext context(scene,user_context,args);

    if (likely(scene->intersectors.intersector16))
      scene->intersectors.occluded16(valid,*ray,&context);

    else {
      RayHit16* ray16 = (RayHit16*) ray;
      for (size_t i=0; i<16; i++) {
        if (!valid[i]) continue;
        RayHit ray1; ray16->get(i,ray1);
        scene->intersectors.occluded((RTCRay&)ray1,&context);
        ray16->set(i,ray1);
      }
    }

    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcForwardOccluded16(const int* valid, const RTCOccludedFunctionNArguments* args, RTCScene hscene, RTCRay16* iray, unsigned int instID)
  {
    RTC_TRACE(rtcForwardOccluded16);
    return rtcForwardOccluded16Ex(valid, args, hscene, iray, instID, 0);
  }

  RTC_API void rtcForwardOccluded16Ex(const int* valid, const RTCOccludedFunctionNArguments* args, RTCScene hscene, RTCRay16* iray, unsigned int instID, unsigned int instPrimID)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcForwardOccluded16Ex);
    rtcForwardOccludedN<RTCRay16,16>(valid, args, hscene, iray, instID, instPrimID);
    RTC_CATCH_END2(scene);
  }
  
  RTC_API void rtcRetainScene (RTCScene hscene) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcRetainScene);
    RTC_VERIFY_HANDLE(hscene);
    RTC_ENTER_DEVICE(hscene);
    scene->refInc();
    RTC_CATCH_END2(scene);
  }
  
  RTC_API void rtcReleaseScene (RTCScene hscene) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcReleaseScene);
    RTC_VERIFY_HANDLE(hscene);
    RTC_ENTER_DEVICE(hscene);
    scene->refDec();
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcSetGeometryInstancedScene(RTCGeometry hgeometry, RTCScene hscene)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    Ref<Scene> scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryInstancedScene);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_VERIFY_HANDLE(hscene);
    RTC_ENTER_DEVICE(hgeometry);
    geometry->setInstancedScene(scene);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryInstancedScenes(RTCGeometry hgeometry, RTCScene* scenes, size_t numScenes)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryInstancedScene);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_VERIFY_HANDLE(scenes);
    RTC_ENTER_DEVICE(hgeometry);
    geometry->setInstancedScenes(scenes, numScenes);
    RTC_CATCH_END2(geometry);
  }

  AffineSpace3fa loadTransform(RTCFormat format, const float* xfm)
  {
    AffineSpace3fa space = one;
    switch (format)
    {
    case RTC_FORMAT_FLOAT3X4_ROW_MAJOR:
      space = AffineSpace3fa(Vec3fa(xfm[ 0], xfm[ 4], xfm[ 8]),
                             Vec3fa(xfm[ 1], xfm[ 5], xfm[ 9]),
                             Vec3fa(xfm[ 2], xfm[ 6], xfm[10]),
                             Vec3fa(xfm[ 3], xfm[ 7], xfm[11]));
      break;

    case RTC_FORMAT_FLOAT3X4_COLUMN_MAJOR:
      space = AffineSpace3fa(Vec3fa(xfm[ 0], xfm[ 1], xfm[ 2]),
                             Vec3fa(xfm[ 3], xfm[ 4], xfm[ 5]),
                             Vec3fa(xfm[ 6], xfm[ 7], xfm[ 8]),
                             Vec3fa(xfm[ 9], xfm[10], xfm[11]));
      break;

    case RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR:
      space = AffineSpace3fa(Vec3fa(xfm[ 0], xfm[ 1], xfm[ 2]),
                             Vec3fa(xfm[ 4], xfm[ 5], xfm[ 6]),
                             Vec3fa(xfm[ 8], xfm[ 9], xfm[10]),
                             Vec3fa(xfm[12], xfm[13], xfm[14]));
      break;

    default: 
      throw_RTCError(RTC_ERROR_INVALID_OPERATION, "invalid matrix format");
      break;
    }
    return space;
  }

RTC_API void rtcSetGeometryTransform(RTCGeometry hgeometry, unsigned int timeStep, RTCFormat format, const void* xfm)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryTransform);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_VERIFY_HANDLE(xfm);
    RTC_ENTER_DEVICE(hgeometry);
    const AffineSpace3fa transform = loadTransform(format, (const float*)xfm);
    geometry->setTransform(transform, timeStep);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryTransformQuaternion(RTCGeometry hgeometry, unsigned int timeStep, const RTCQuaternionDecomposition* qd)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryTransformQuaternion);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_VERIFY_HANDLE(qd);
    RTC_ENTER_DEVICE(hgeometry);
    
    AffineSpace3fx transform;
    transform.l.vx.x = qd->scale_x;
    transform.l.vy.y = qd->scale_y;
    transform.l.vz.z = qd->scale_z;
    transform.l.vy.x = qd->skew_xy;
    transform.l.vz.x = qd->skew_xz;
    transform.l.vz.y = qd->skew_yz;
    transform.l.vx.y = qd->translation_x;
    transform.l.vx.z = qd->translation_y;
    transform.l.vy.z = qd->translation_z;
    transform.p.x    = qd->shift_x;
    transform.p.y    = qd->shift_y;
    transform.p.z    = qd->shift_z;

    // normalize quaternion
    Quaternion3f q(qd->quaternion_r, qd->quaternion_i, qd->quaternion_j, qd->quaternion_k);
    q = normalize(q);
    transform.l.vx.w = q.i;
    transform.l.vy.w = q.j;
    transform.l.vz.w = q.k;
    transform.p.w    = q.r;

    geometry->setQuaternionDecomposition(transform, timeStep);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcGetGeometryTransform(RTCGeometry hgeometry, float time, RTCFormat format, void* xfm)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetGeometryTransform);
    //RTC_ENTER_DEVICE(hgeometry); // no allocation required
    const AffineSpace3fa transform = geometry->getTransform(time);
    storeTransform(transform, format, (float*)xfm);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcGetGeometryTransformEx(RTCGeometry hgeometry, unsigned int instPrimID, float time, RTCFormat format, void* xfm)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetGeometryTransformEx);
    //RTC_ENTER_DEVICE(hgeometry); // no allocation required
    const AffineSpace3fa transform = geometry->getTransform(instPrimID, time);
    storeTransform(transform, format, (float*)xfm);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcGetGeometryTransformFromScene(RTCScene hscene, unsigned int geomID, float time, RTCFormat format, void* xfm)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetGeometryTransformFromScene);
    //RTC_ENTER_DEVICE(hscene); // no allocation required
    const AffineSpace3fa transform = scene->get(geomID)->getTransform(time);
    storeTransform(transform, format, (float*)xfm);
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcInvokeIntersectFilterFromGeometry(const struct RTCIntersectFunctionNArguments* const args_i, const struct RTCFilterFunctionNArguments* filter_args)
  {
    IntersectFunctionNArguments* args = (IntersectFunctionNArguments*) args_i;
    if (args->geometry->intersectionFilterN)
        args->geometry->intersectionFilterN(filter_args);
  }

  RTC_API void rtcInvokeOccludedFilterFromGeometry(const struct RTCOccludedFunctionNArguments* const args_i, const struct RTCFilterFunctionNArguments* filter_args)
  {
    OccludedFunctionNArguments* args = (OccludedFunctionNArguments*) args_i;
    if (args->geometry->occlusionFilterN)
      args->geometry->occlusionFilterN(filter_args);
  }
  
  RTC_API RTCGeometry rtcNewGeometry (RTCDevice hdevice, RTCGeometryType type)
  {
    Device* device = (Device*) hdevice;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcNewGeometry);
    RTC_ENTER_DEVICE(hdevice);
    RTC_VERIFY_HANDLE(hdevice);

    switch (type)
    {
    case RTC_GEOMETRY_TYPE_TRIANGLE:
    {
#if defined(EMBREE_GEOMETRY_TRIANGLE)
      createTriangleMeshTy createTriangleMesh = nullptr;
      SELECT_SYMBOL_DEFAULT_AVX_AVX2_AVX512(device->enabled_cpu_features,createTriangleMesh);
      Geometry* geom = createTriangleMesh(device);
      return (RTCGeometry) geom->refInc();
#else
      throw_RTCError(RTC_ERROR_UNKNOWN,"RTC_GEOMETRY_TYPE_TRIANGLE is not supported");
#endif
    }
    
    case RTC_GEOMETRY_TYPE_QUAD:
    {
#if defined(EMBREE_GEOMETRY_QUAD)
      createQuadMeshTy createQuadMesh = nullptr;
      SELECT_SYMBOL_DEFAULT_AVX_AVX2_AVX512(device->enabled_cpu_features,createQuadMesh);
      Geometry* geom = createQuadMesh(device);
      return (RTCGeometry) geom->refInc();
#else
      throw_RTCError(RTC_ERROR_UNKNOWN,"RTC_GEOMETRY_TYPE_QUAD is not supported");
#endif
    }
    
    case RTC_GEOMETRY_TYPE_SPHERE_POINT:
    case RTC_GEOMETRY_TYPE_DISC_POINT:
    case RTC_GEOMETRY_TYPE_ORIENTED_DISC_POINT:
    {
#if defined(EMBREE_GEOMETRY_POINT)
      createPointsTy createPoints = nullptr;
      SELECT_SYMBOL_DEFAULT_AVX_AVX2_AVX512(device->enabled_builder_cpu_features, createPoints);

      Geometry *geom;
      switch(type) {
        case RTC_GEOMETRY_TYPE_SPHERE_POINT:
          geom = createPoints(device, Geometry::GTY_SPHERE_POINT);
          break;
        case RTC_GEOMETRY_TYPE_DISC_POINT:
          geom = createPoints(device, Geometry::GTY_DISC_POINT);
          break;
        case RTC_GEOMETRY_TYPE_ORIENTED_DISC_POINT:
          geom = createPoints(device, Geometry::GTY_ORIENTED_DISC_POINT);
          break;
        default:
          geom = nullptr;
          break;
      }
      return (RTCGeometry) geom->refInc();
#else
      throw_RTCError(RTC_ERROR_UNKNOWN,"RTC_GEOMETRY_TYPE_POINT is not supported");
#endif
    }

    case RTC_GEOMETRY_TYPE_CONE_LINEAR_CURVE:
    case RTC_GEOMETRY_TYPE_ROUND_LINEAR_CURVE:
    case RTC_GEOMETRY_TYPE_FLAT_LINEAR_CURVE:
      
    case RTC_GEOMETRY_TYPE_ROUND_BEZIER_CURVE:
    case RTC_GEOMETRY_TYPE_FLAT_BEZIER_CURVE:
    case RTC_GEOMETRY_TYPE_NORMAL_ORIENTED_BEZIER_CURVE:
      
    case RTC_GEOMETRY_TYPE_ROUND_BSPLINE_CURVE:
    case RTC_GEOMETRY_TYPE_FLAT_BSPLINE_CURVE:
    case RTC_GEOMETRY_TYPE_NORMAL_ORIENTED_BSPLINE_CURVE:

    case RTC_GEOMETRY_TYPE_ROUND_HERMITE_CURVE:
    case RTC_GEOMETRY_TYPE_FLAT_HERMITE_CURVE:
    case RTC_GEOMETRY_TYPE_NORMAL_ORIENTED_HERMITE_CURVE:

    case RTC_GEOMETRY_TYPE_ROUND_CATMULL_ROM_CURVE:
    case RTC_GEOMETRY_TYPE_FLAT_CATMULL_ROM_CURVE:
    case RTC_GEOMETRY_TYPE_NORMAL_ORIENTED_CATMULL_ROM_CURVE:
    {
#if defined(EMBREE_GEOMETRY_CURVE)
      createLineSegmentsTy createLineSegments = nullptr;
      SELECT_SYMBOL_DEFAULT_AVX_AVX2_AVX512(device->enabled_cpu_features,createLineSegments);
      createCurvesTy createCurves = nullptr;
      SELECT_SYMBOL_DEFAULT_AVX_AVX2_AVX512(device->enabled_cpu_features,createCurves);
      
      Geometry* geom;
      switch (type) {
      case RTC_GEOMETRY_TYPE_CONE_LINEAR_CURVE             : geom = createLineSegments (device,Geometry::GTY_CONE_LINEAR_CURVE); break;
      case RTC_GEOMETRY_TYPE_ROUND_LINEAR_CURVE            : geom = createLineSegments (device,Geometry::GTY_ROUND_LINEAR_CURVE); break;
      case RTC_GEOMETRY_TYPE_FLAT_LINEAR_CURVE             : geom = createLineSegments (device,Geometry::GTY_FLAT_LINEAR_CURVE); break;
      //case RTC_GEOMETRY_TYPE_NORMAL_ORIENTED_LINEAR_CURVE  : geom = createLineSegments (device,Geometry::GTY_ORIENTED_LINEAR_CURVE); break;
        
      case RTC_GEOMETRY_TYPE_ROUND_BEZIER_CURVE            : geom = createCurves(device,Geometry::GTY_ROUND_BEZIER_CURVE); break;
      case RTC_GEOMETRY_TYPE_FLAT_BEZIER_CURVE             : geom = createCurves(device,Geometry::GTY_FLAT_BEZIER_CURVE); break;
      case RTC_GEOMETRY_TYPE_NORMAL_ORIENTED_BEZIER_CURVE  : geom = createCurves(device,Geometry::GTY_ORIENTED_BEZIER_CURVE); break;
        
      case RTC_GEOMETRY_TYPE_ROUND_BSPLINE_CURVE           : geom = createCurves(device,Geometry::GTY_ROUND_BSPLINE_CURVE); break;
      case RTC_GEOMETRY_TYPE_FLAT_BSPLINE_CURVE            : geom = createCurves(device,Geometry::GTY_FLAT_BSPLINE_CURVE); break;
      case RTC_GEOMETRY_TYPE_NORMAL_ORIENTED_BSPLINE_CURVE : geom = createCurves(device,Geometry::GTY_ORIENTED_BSPLINE_CURVE); break;
        
      case RTC_GEOMETRY_TYPE_ROUND_HERMITE_CURVE           : geom = createCurves(device,Geometry::GTY_ROUND_HERMITE_CURVE); break;
      case RTC_GEOMETRY_TYPE_FLAT_HERMITE_CURVE            : geom = createCurves(device,Geometry::GTY_FLAT_HERMITE_CURVE); break;
      case RTC_GEOMETRY_TYPE_NORMAL_ORIENTED_HERMITE_CURVE : geom = createCurves(device,Geometry::GTY_ORIENTED_HERMITE_CURVE); break;

      case RTC_GEOMETRY_TYPE_ROUND_CATMULL_ROM_CURVE           : geom = createCurves(device,Geometry::GTY_ROUND_CATMULL_ROM_CURVE); break;
      case RTC_GEOMETRY_TYPE_FLAT_CATMULL_ROM_CURVE            : geom = createCurves(device,Geometry::GTY_FLAT_CATMULL_ROM_CURVE); break;
      case RTC_GEOMETRY_TYPE_NORMAL_ORIENTED_CATMULL_ROM_CURVE : geom = createCurves(device,Geometry::GTY_ORIENTED_CATMULL_ROM_CURVE); break;
      default:                                    geom = nullptr; break;
      }
      return (RTCGeometry) geom->refInc();
#else
      throw_RTCError(RTC_ERROR_UNKNOWN,"RTC_GEOMETRY_TYPE_CURVE is not supported");
#endif
    }
    
    case RTC_GEOMETRY_TYPE_SUBDIVISION:
    {
#if defined(EMBREE_GEOMETRY_SUBDIVISION)
      createSubdivMeshTy createSubdivMesh = nullptr;
      SELECT_SYMBOL_DEFAULT_AVX(device->enabled_cpu_features,createSubdivMesh);
      //SELECT_SYMBOL_DEFAULT_AVX_AVX2_AVX512(device->enabled_cpu_features,createSubdivMesh); // FIXME: this does not work for some reason?
      Geometry* geom = createSubdivMesh(device);
      return (RTCGeometry) geom->refInc();
#else
      throw_RTCError(RTC_ERROR_UNKNOWN,"RTC_GEOMETRY_TYPE_SUBDIVISION is not supported");
#endif
    }
    
    case RTC_GEOMETRY_TYPE_USER:
    {
#if defined(EMBREE_GEOMETRY_USER)
      createUserGeometryTy createUserGeometry = nullptr;
      SELECT_SYMBOL_DEFAULT_AVX_AVX2_AVX512(device->enabled_cpu_features,createUserGeometry);
      Geometry* geom = createUserGeometry(device);
      return (RTCGeometry) geom->refInc();
#else
      throw_RTCError(RTC_ERROR_UNKNOWN,"RTC_GEOMETRY_TYPE_USER is not supported");
#endif
    }

    case RTC_GEOMETRY_TYPE_INSTANCE:
    {
#if defined(EMBREE_GEOMETRY_INSTANCE)
      createInstanceTy createInstance = nullptr;
      SELECT_SYMBOL_DEFAULT_AVX_AVX2_AVX512(device->enabled_cpu_features,createInstance);
      Geometry* geom = createInstance(device);
      return (RTCGeometry) geom->refInc();
#else
      throw_RTCError(RTC_ERROR_UNKNOWN,"RTC_GEOMETRY_TYPE_INSTANCE is not supported");
#endif
    }

    case RTC_GEOMETRY_TYPE_INSTANCE_ARRAY:
    {
#if defined(EMBREE_GEOMETRY_INSTANCE_ARRAY)
      createInstanceArrayTy createInstanceArray = nullptr;
      SELECT_SYMBOL_DEFAULT_AVX_AVX2_AVX512(device->enabled_cpu_features,createInstanceArray);
      Geometry* geom = createInstanceArray(device);
      return (RTCGeometry) geom->refInc();
#else
      throw_RTCError(RTC_ERROR_UNKNOWN,"RTC_GEOMETRY_TYPE_INSTANCE_ARRAY is not supported");
#endif
    }

    case RTC_GEOMETRY_TYPE_GRID:
    {
#if defined(EMBREE_GEOMETRY_GRID)
      createGridMeshTy createGridMesh = nullptr;
      SELECT_SYMBOL_DEFAULT_AVX_AVX2_AVX512(device->enabled_cpu_features,createGridMesh);
      Geometry* geom = createGridMesh(device);
      return (RTCGeometry) geom->refInc();
#else
      throw_RTCError(RTC_ERROR_UNKNOWN,"RTC_GEOMETRY_TYPE_GRID is not supported");
#endif
    }
    
    default:
      throw_RTCError(RTC_ERROR_UNKNOWN,"invalid geometry type");
    }
    
    RTC_CATCH_END(device);
    return nullptr;
  }

  RTC_API void rtcSetGeometryUserPrimitiveCount(RTCGeometry hgeometry, unsigned int userPrimitiveCount)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryUserPrimitiveCount);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    
    if (unlikely(geometry->getType() != Geometry::GTY_USER_GEOMETRY))
      throw_RTCError(RTC_ERROR_INVALID_OPERATION,"operation only allowed for user geometries"); 

    geometry->setNumPrimitives(userPrimitiveCount);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryTimeStepCount(RTCGeometry hgeometry, unsigned int timeStepCount)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryTimeStepCount);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);

    if (timeStepCount > RTC_MAX_TIME_STEP_COUNT)
      throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"number of time steps is out of range");
    
    geometry->setNumTimeSteps(timeStepCount);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryTimeRange(RTCGeometry hgeometry, float startTime, float endTime)
  {
    Ref<Geometry> geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryTimeRange);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);

    if (startTime > endTime)
      throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"startTime has to be smaller or equal to the endTime");
        
    geometry->setTimeRange(BBox1f(startTime,endTime));
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryVertexAttributeCount(RTCGeometry hgeometry, unsigned int N)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryVertexAttributeCount);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    geometry->setVertexAttributeCount(N);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryTopologyCount(RTCGeometry hgeometry, unsigned int N)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryTopologyCount);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    geometry->setTopologyCount(N);
    RTC_CATCH_END2(geometry);
  }
 
  RTC_API void rtcSetGeometryBuildQuality (RTCGeometry hgeometry, RTCBuildQuality quality) 
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryBuildQuality);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    // -- GODOT start --
    // if (quality != RTC_BUILD_QUALITY_LOW &&
    //     quality != RTC_BUILD_QUALITY_MEDIUM &&
    //     quality != RTC_BUILD_QUALITY_HIGH &&
    //     quality != RTC_BUILD_QUALITY_REFIT)
    //   throw std::runtime_error("invalid build quality");
    if (quality != RTC_BUILD_QUALITY_LOW &&
        quality != RTC_BUILD_QUALITY_MEDIUM &&
        quality != RTC_BUILD_QUALITY_HIGH &&
        quality != RTC_BUILD_QUALITY_REFIT) {
      abort();
    }
    // -- GODOT end --
    geometry->setBuildQuality(quality);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryMaxRadiusScale(RTCGeometry hgeometry, float maxRadiusScale)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryMaxRadiusScale);
    RTC_VERIFY_HANDLE(hgeometry);
#if RTC_MIN_WIDTH
    if (maxRadiusScale < 1.0f) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"maximal radius scale has to be larger or equal to 1");
    geometry->setMaxRadiusScale(maxRadiusScale);
#else
    throw_RTCError(RTC_ERROR_INVALID_OPERATION,"min-width feature is not enabled");
#endif
    RTC_CATCH_END2(geometry);
  }
  
  RTC_API void rtcSetGeometryMask (RTCGeometry hgeometry, unsigned int mask) 
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryMask);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    geometry->setMask(mask);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometrySubdivisionMode (RTCGeometry hgeometry, unsigned topologyID, RTCSubdivisionMode mode) 
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometrySubdivisionMode);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    geometry->setSubdivisionMode(topologyID,mode);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryVertexAttributeTopology(RTCGeometry hgeometry, unsigned int vertexAttributeID, unsigned int topologyID)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryVertexAttributeTopology);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    geometry->setVertexAttributeTopology(vertexAttributeID, topologyID);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryBuffer(RTCGeometry hgeometry, RTCBufferType type, unsigned int slot, RTCFormat format, RTCBuffer hbuffer, size_t byteOffset, size_t byteStride, size_t itemCount)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    Ref<Buffer> buffer = (Buffer*)hbuffer;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryBuffer);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_VERIFY_HANDLE(hbuffer);
    RTC_ENTER_DEVICE(hgeometry);
    
    if (geometry->device != buffer->device)
      throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"inputs are from different devices");
    
    if (itemCount > 0xFFFFFFFFu)
      throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"buffer too large");
    
    geometry->setBuffer(type, slot, format, buffer, byteOffset, byteStride, (unsigned int)itemCount);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetSharedGeometryBuffer(RTCGeometry hgeometry, RTCBufferType type, unsigned int slot, RTCFormat format, const void* ptr, size_t byteOffset, size_t byteStride, size_t itemCount)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetSharedGeometryBuffer);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    
    if (itemCount > 0xFFFFFFFFu)
      throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"buffer too large");

    Ref<Buffer> buffer = new Buffer(geometry->device, itemCount*byteStride, (char*)ptr + byteOffset);
    geometry->setBuffer(type, slot, format, buffer, 0, byteStride, (unsigned int)itemCount);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void* rtcSetNewGeometryBuffer(RTCGeometry hgeometry, RTCBufferType type, unsigned int slot, RTCFormat format, size_t byteStride, size_t itemCount)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetNewGeometryBuffer);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);

    if (itemCount > 0xFFFFFFFFu)
      throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"buffer too large");
    
    /* vertex buffers need to get overallocated slightly as elements are accessed using SSE loads */
    size_t bytes = itemCount*byteStride;
    if (type == RTC_BUFFER_TYPE_VERTEX || type == RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE)
      bytes += (16 - (byteStride%16))%16;
      
    Ref<Buffer> buffer = new Buffer(geometry->device, bytes);
    geometry->setBuffer(type, slot, format, buffer, 0, byteStride, (unsigned int)itemCount);
    return buffer->data();
    RTC_CATCH_END2(geometry);
    return nullptr;
  }

  RTC_API void* rtcGetGeometryBufferData(RTCGeometry hgeometry, RTCBufferType type, unsigned int slot)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetGeometryBufferData);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    return geometry->getBuffer(type, slot);
    RTC_CATCH_END2(geometry);
    return nullptr;
  }
  
  RTC_API void rtcEnableGeometry (RTCGeometry hgeometry) 
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcEnableGeometry);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    geometry->enable();
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcUpdateGeometryBuffer (RTCGeometry hgeometry, RTCBufferType type, unsigned int slot) 
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcUpdateGeometryBuffer);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    geometry->updateBuffer(type, slot);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcDisableGeometry (RTCGeometry hgeometry) 
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcDisableGeometry);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    geometry->disable();
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryTessellationRate (RTCGeometry hgeometry, float tessellationRate)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryTessellationRate);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    geometry->setTessellationRate(tessellationRate);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryUserData (RTCGeometry hgeometry, void* ptr) 
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryUserData);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    geometry->setUserData(ptr);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void* rtcGetGeometryUserData (RTCGeometry hgeometry)
  {
    Geometry* geometry = (Geometry*) hgeometry; // no ref counting here!
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetGeometryUserData);
    RTC_VERIFY_HANDLE(hgeometry);
    //RTC_ENTER_DEVICE(hgeometry); // do not enable for performance reasons !
    return geometry->getUserData();
    RTC_CATCH_END2(geometry);
    return nullptr;
  }

  RTC_API void* rtcGetGeometryUserDataFromScene (RTCScene hscene, unsigned int geomID)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetGeometryUserDataFromScene);
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    RTC_VERIFY_GEOMID(geomID);
#endif
    //RTC_ENTER_DEVICE(hscene); // do not enable for performance reasons
    return scene->get(geomID)->getUserData();
    RTC_CATCH_END2(scene);
    return nullptr;
  }

  RTC_API void rtcSetGeometryBoundsFunction (RTCGeometry hgeometry, RTCBoundsFunction bounds, void* userPtr)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryBoundsFunction);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    geometry->setBoundsFunction(bounds,userPtr);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryDisplacementFunction (RTCGeometry hgeometry, RTCDisplacementFunctionN displacement)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryDisplacementFunction);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    geometry->setDisplacementFunction(displacement);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryIntersectFunction (RTCGeometry hgeometry, RTCIntersectFunctionN intersect) 
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryIntersectFunction);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    geometry->setIntersectFunctionN(intersect);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryPointQueryFunction(RTCGeometry hgeometry, RTCPointQueryFunction pointQuery)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryPointQueryFunction);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    geometry->setPointQueryFunction(pointQuery);
    RTC_CATCH_END2(geometry);
  }

  RTC_API unsigned int rtcGetGeometryFirstHalfEdge(RTCGeometry hgeometry, unsigned int faceID)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetGeometryFirstHalfEdge);
    //RTC_ENTER_DEVICE(hgeometry); // do not enable for performance reasons
    return geometry->getFirstHalfEdge(faceID);
    RTC_CATCH_END2(geometry);
    return -1;
  }

  RTC_API unsigned int rtcGetGeometryFace(RTCGeometry hgeometry, unsigned int edgeID)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetGeometryFace);
    //RTC_ENTER_DEVICE(hgeometry); // do not enable for performance reasons
    return geometry->getFace(edgeID);
    RTC_CATCH_END2(geometry);
    return -1;
  }

  RTC_API unsigned int rtcGetGeometryNextHalfEdge(RTCGeometry hgeometry, unsigned int edgeID)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetGeometryNextHalfEdge);
    //RTC_ENTER_DEVICE(hgeometry); // do not enable for performance reasons
    return geometry->getNextHalfEdge(edgeID);
    RTC_CATCH_END2(geometry);
    return -1;
  }

  RTC_API unsigned int rtcGetGeometryPreviousHalfEdge(RTCGeometry hgeometry, unsigned int edgeID)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetGeometryPreviousHalfEdge);
    //RTC_ENTER_DEVICE(hgeometry); // do not enable for performance reasons
    return geometry->getPreviousHalfEdge(edgeID);
    RTC_CATCH_END2(geometry);
    return -1;
  }

  RTC_API unsigned int rtcGetGeometryOppositeHalfEdge(RTCGeometry hgeometry, unsigned int topologyID, unsigned int edgeID)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetGeometryOppositeHalfEdge);
    //RTC_ENTER_DEVICE(hgeometry); // do not enable for performance reasons
    return geometry->getOppositeHalfEdge(topologyID,edgeID);
    RTC_CATCH_END2(geometry);
    return -1;
  }

  RTC_API void rtcSetGeometryOccludedFunction (RTCGeometry hgeometry, RTCOccludedFunctionN occluded) 
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetOccludedFunctionN);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    geometry->setOccludedFunctionN(occluded);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryIntersectFilterFunction (RTCGeometry hgeometry, RTCFilterFunctionN filter) 
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryIntersectFilterFunction);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    geometry->setIntersectionFilterFunctionN(filter);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryOccludedFilterFunction (RTCGeometry hgeometry, RTCFilterFunctionN filter) 
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryOccludedFilterFunction);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    geometry->setOcclusionFilterFunctionN(filter);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryEnableFilterFunctionFromArguments (RTCGeometry hgeometry, bool enable) 
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryEnableFilterFunctionFromArguments);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    geometry->enableFilterFunctionFromArguments(enable);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcInterpolate(const RTCInterpolateArguments* const args)
  {
    Geometry* geometry = (Geometry*) args->geometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcInterpolate);
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(args->geometry);
#endif
    //RTC_ENTER_DEVICE(hgeometry); // do not enable for performance reasons
    geometry->interpolate(args);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcInterpolateN(const RTCInterpolateNArguments* const args)
  {
    Geometry* geometry = (Geometry*) args->geometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcInterpolateN);
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(args->geometry);
#endif
    // RTC_ENTER_DEVICE(hgeometry); // do not enable for performance reasons
    geometry->interpolateN(args);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcCommitGeometry (RTCGeometry hgeometry)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcCommitGeometry);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    return geometry->commit();
    RTC_CATCH_END2(geometry);
  }

  RTC_API unsigned int rtcAttachGeometry (RTCScene hscene, RTCGeometry hgeometry)
  {
    Scene* scene = (Scene*) hscene;
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcAttachGeometry);
    RTC_VERIFY_HANDLE(hscene);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    if (scene->device != geometry->device)
      throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"inputs are from different devices");
    return scene->bind(RTC_INVALID_GEOMETRY_ID,geometry);
    RTC_CATCH_END2(scene);
    return -1;
  }

  RTC_API void rtcAttachGeometryByID (RTCScene hscene, RTCGeometry hgeometry, unsigned int geomID)
  {
    Scene* scene = (Scene*) hscene;
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcAttachGeometryByID);
    RTC_VERIFY_HANDLE(hscene);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_VERIFY_GEOMID(geomID);
    RTC_ENTER_DEVICE(hscene);
    if (scene->device != geometry->device)
      throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"inputs are from different devices");
    scene->bind(geomID,geometry);
    RTC_CATCH_END2(scene);
  }
  
  RTC_API void rtcDetachGeometry (RTCScene hscene, unsigned int geomID)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcDetachGeometry);
    RTC_VERIFY_HANDLE(hscene);
    RTC_VERIFY_GEOMID(geomID);
    RTC_ENTER_DEVICE(hscene);
    scene->detachGeometry(geomID);
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcRetainGeometry (RTCGeometry hgeometry)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcRetainGeometry);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    geometry->refInc();
    RTC_CATCH_END2(geometry);
  }
  
  RTC_API void rtcReleaseGeometry (RTCGeometry hgeometry)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcReleaseGeometry);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_ENTER_DEVICE(hgeometry);
    geometry->refDec();
    RTC_CATCH_END2(geometry);
  }

  RTC_API RTCGeometry rtcGetGeometry (RTCScene hscene, unsigned int geomID)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetGeometry);
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    RTC_VERIFY_GEOMID(geomID);
#endif
    //RTC_ENTER_DEVICE(hscene); // do not enable for performance reasons
    return (RTCGeometry) scene->get(geomID);
    RTC_CATCH_END2(scene);
    return nullptr;
  }

  RTC_API RTCGeometry rtcGetGeometryThreadSafe (RTCScene hscene, unsigned int geomID)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetGeometryThreadSafe);
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    RTC_VERIFY_GEOMID(geomID);
#endif
    Ref<Geometry> geom = scene->get_locked(geomID);
    return (RTCGeometry) geom.ptr; 
    RTC_CATCH_END2(scene);
    return nullptr;
  }

  RTC_API const char* rtcGetErrorString(RTCError error)
  {
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetErrorString);
    return Device::getErrorString(error);
    RTC_CATCH_END(nullptr);
    return nullptr;
  }


RTC_NAMESPACE_END
