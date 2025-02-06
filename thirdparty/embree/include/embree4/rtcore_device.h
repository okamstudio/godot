// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "rtcore_common.h"

RTC_NAMESPACE_BEGIN

/* Opaque device type */
typedef struct RTCDeviceTy* RTCDevice;

/* Creates a new Embree device. */
RTC_API RTCDevice rtcNewDevice(const char* config);

#if defined(EMBREE_SYCL_SUPPORT) && defined(SYCL_LANGUAGE_VERSION)


/* Creates a new Embree SYCL device. */
RTC_API_EXTERN_C RTCDevice rtcNewSYCLDevice(sycl::context context, const char* config);

/* Checks if SYCL device is supported by Embree. */
RTC_API bool rtcIsSYCLDeviceSupported(const sycl::device sycl_device);

/* SYCL selector for Embree supported devices */
RTC_API int rtcSYCLDeviceSelector(const sycl::device sycl_device);

/* Set the SYCL device to be used to allocate data */
RTC_API void rtcSetDeviceSYCLDevice(RTCDevice device, const sycl::device sycl_device);

#endif


/* Retains the Embree device (increments the reference count). */
RTC_API void rtcRetainDevice(RTCDevice device);
  
/* Releases an Embree device (decrements the reference count). */
RTC_API void rtcReleaseDevice(RTCDevice device);

/* Device properties */
enum RTCDeviceProperty
{
  RTC_DEVICE_PROPERTY_VERSION       = 0,
  RTC_DEVICE_PROPERTY_VERSION_MAJOR = 1,
  RTC_DEVICE_PROPERTY_VERSION_MINOR = 2,
  RTC_DEVICE_PROPERTY_VERSION_PATCH = 3,

  RTC_DEVICE_PROPERTY_NATIVE_RAY4_SUPPORTED  = 32,
  RTC_DEVICE_PROPERTY_NATIVE_RAY8_SUPPORTED  = 33,
  RTC_DEVICE_PROPERTY_NATIVE_RAY16_SUPPORTED = 34,

  RTC_DEVICE_PROPERTY_BACKFACE_CULLING_SPHERES_ENABLED = 62,
  RTC_DEVICE_PROPERTY_BACKFACE_CULLING_CURVES_ENABLED = 63,
  RTC_DEVICE_PROPERTY_RAY_MASK_SUPPORTED          = 64,
  RTC_DEVICE_PROPERTY_BACKFACE_CULLING_ENABLED    = 65,
  RTC_DEVICE_PROPERTY_FILTER_FUNCTION_SUPPORTED   = 66,
  RTC_DEVICE_PROPERTY_IGNORE_INVALID_RAYS_ENABLED = 67,
  RTC_DEVICE_PROPERTY_COMPACT_POLYS_ENABLED       = 68,

  RTC_DEVICE_PROPERTY_TRIANGLE_GEOMETRY_SUPPORTED    = 96,
  RTC_DEVICE_PROPERTY_QUAD_GEOMETRY_SUPPORTED        = 97,
  RTC_DEVICE_PROPERTY_SUBDIVISION_GEOMETRY_SUPPORTED = 98,
  RTC_DEVICE_PROPERTY_CURVE_GEOMETRY_SUPPORTED       = 99,
  RTC_DEVICE_PROPERTY_USER_GEOMETRY_SUPPORTED        = 100,
  RTC_DEVICE_PROPERTY_POINT_GEOMETRY_SUPPORTED       = 101,

  RTC_DEVICE_PROPERTY_TASKING_SYSTEM        = 128,
  RTC_DEVICE_PROPERTY_JOIN_COMMIT_SUPPORTED = 129,
  RTC_DEVICE_PROPERTY_PARALLEL_COMMIT_SUPPORTED = 130,

  RTC_DEVICE_PROPERTY_CPU_DEVICE  = 140,
  RTC_DEVICE_PROPERTY_SYCL_DEVICE = 141
};

/* Gets a device property. */
RTC_API ssize_t rtcGetDeviceProperty(RTCDevice device, enum RTCDeviceProperty prop);

/* Sets a device property. */
RTC_API void rtcSetDeviceProperty(RTCDevice device, const enum RTCDeviceProperty prop, ssize_t value);
  
/* Error codes */
enum RTCError
{
  RTC_ERROR_NONE                                  = 0,
  RTC_ERROR_UNKNOWN                               = 1,
  RTC_ERROR_INVALID_ARGUMENT                      = 2,
  RTC_ERROR_INVALID_OPERATION                     = 3,
  RTC_ERROR_OUT_OF_MEMORY                         = 4,
  RTC_ERROR_UNSUPPORTED_CPU                       = 5,
  RTC_ERROR_CANCELLED                             = 6,
  RTC_ERROR_LEVEL_ZERO_RAYTRACING_SUPPORT_MISSING = 7,
};

/* Returns the string representation for the error code. For example, for RTC_ERROR_UNKNOWN the string "RTC_ERROR_UNKNOWN" will be returned. */
RTC_API const char* rtcGetErrorString(enum RTCError error);

/* Returns the error code. */
RTC_API enum RTCError rtcGetDeviceError(RTCDevice device);

/* Returns a message corresponding to the last error code (returned by rtcGetDeviceError) which provides details about the error that happened.
   The same message will be written to console when verbosity is > 0 or when an error callback function is set for the device.
   However, when device creation itself fails this is the only way to get additional information about the error. */
RTC_API const char* rtcGetDeviceLastErrorMessage(RTCDevice device);

/* Error callback function */
typedef void (*RTCErrorFunction)(void* userPtr, enum RTCError code, const char* str);

/* Sets the error callback function. */
RTC_API void rtcSetDeviceErrorFunction(RTCDevice device, RTCErrorFunction error, void* userPtr);

/* Memory monitor callback function */
typedef bool (*RTCMemoryMonitorFunction)(void* ptr, ssize_t bytes, bool post);

/* Sets the memory monitor callback function. */
RTC_API void rtcSetDeviceMemoryMonitorFunction(RTCDevice device, RTCMemoryMonitorFunction memoryMonitor, void* userPtr);

RTC_NAMESPACE_END
