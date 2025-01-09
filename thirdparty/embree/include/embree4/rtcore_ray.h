// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "rtcore_common.h"

RTC_NAMESPACE_BEGIN

/* Ray structure for a single ray */
struct RTC_ALIGN(16) RTCRay
{
  float org_x;        // x coordinate of ray origin
  float org_y;        // y coordinate of ray origin
  float org_z;        // z coordinate of ray origin
  float tnear;        // start of ray segment

  float dir_x;        // x coordinate of ray direction
  float dir_y;        // y coordinate of ray direction
  float dir_z;        // z coordinate of ray direction
  float time;         // time of this ray for motion blur

  float tfar;         // end of ray segment (set to hit distance)
  unsigned int mask;  // ray mask
  unsigned int id;    // ray ID
  unsigned int flags; // ray flags
};

/* Hit structure for a single ray */
struct RTC_ALIGN(16) RTCHit
{
  float Ng_x;          // x coordinate of geometry normal
  float Ng_y;          // y coordinate of geometry normal
  float Ng_z;          // z coordinate of geometry normal

  float u;             // barycentric u coordinate of hit
  float v;             // barycentric v coordinate of hit

  unsigned int primID; // primitive ID
  unsigned int geomID; // geometry ID
  unsigned int instID[RTC_MAX_INSTANCE_LEVEL_COUNT]; // instance ID
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
  unsigned int instPrimID[RTC_MAX_INSTANCE_LEVEL_COUNT]; // instance primitive ID
#endif
};

/* Combined ray/hit structure for a single ray */
struct RTCRayHit
{
  struct RTCRay ray;
  struct RTCHit hit;
};

/* Ray structure for a packet of 4 rays */
struct RTC_ALIGN(16) RTCRay4
{
  float org_x[4];
  float org_y[4];
  float org_z[4];
  float tnear[4];

  float dir_x[4];
  float dir_y[4];
  float dir_z[4];
  float time[4];

  float tfar[4];
  unsigned int mask[4];
  unsigned int id[4];
  unsigned int flags[4];
};

/* Hit structure for a packet of 4 rays */
struct RTC_ALIGN(16) RTCHit4
{
  float Ng_x[4];
  float Ng_y[4];
  float Ng_z[4];

  float u[4];
  float v[4];

  unsigned int primID[4];
  unsigned int geomID[4];
  unsigned int instID[RTC_MAX_INSTANCE_LEVEL_COUNT][4];
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
  unsigned int instPrimID[RTC_MAX_INSTANCE_LEVEL_COUNT][4];
#endif
};

/* Combined ray/hit structure for a packet of 4 rays */
struct RTCRayHit4
{
  struct RTCRay4 ray;
  struct RTCHit4 hit;
};

/* Ray structure for a packet of 8 rays */
struct RTC_ALIGN(32) RTCRay8
{
  float org_x[8];
  float org_y[8];
  float org_z[8];
  float tnear[8];

  float dir_x[8];
  float dir_y[8];
  float dir_z[8];
  float time[8];

  float tfar[8];
  unsigned int mask[8];
  unsigned int id[8];
  unsigned int flags[8];
};

/* Hit structure for a packet of 8 rays */
struct RTC_ALIGN(32) RTCHit8
{
  float Ng_x[8];
  float Ng_y[8];
  float Ng_z[8];

  float u[8];
  float v[8];

  unsigned int primID[8];
  unsigned int geomID[8];
  unsigned int instID[RTC_MAX_INSTANCE_LEVEL_COUNT][8];
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
  unsigned int instPrimID[RTC_MAX_INSTANCE_LEVEL_COUNT][8];
#endif
};

/* Combined ray/hit structure for a packet of 8 rays */
struct RTCRayHit8
{
  struct RTCRay8 ray;
  struct RTCHit8 hit;
};

/* Ray structure for a packet of 16 rays */
struct RTC_ALIGN(64) RTCRay16
{
  float org_x[16];
  float org_y[16];
  float org_z[16];
  float tnear[16];

  float dir_x[16];
  float dir_y[16];
  float dir_z[16];
  float time[16];

  float tfar[16];
  unsigned int mask[16];
  unsigned int id[16];
  unsigned int flags[16];
};

/* Hit structure for a packet of 16 rays */
struct RTC_ALIGN(64) RTCHit16
{
  float Ng_x[16];
  float Ng_y[16];
  float Ng_z[16];

  float u[16];
  float v[16];

  unsigned int primID[16];
  unsigned int geomID[16];
  unsigned int instID[RTC_MAX_INSTANCE_LEVEL_COUNT][16];
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
  unsigned int instPrimID[RTC_MAX_INSTANCE_LEVEL_COUNT][16];
#endif
};

/* Combined ray/hit structure for a packet of 16 rays */
struct RTCRayHit16
{
  struct RTCRay16 ray;
  struct RTCHit16 hit;
};

struct RTCRayN;
struct RTCHitN;
struct RTCRayHitN;

#if defined(__cplusplus)

/* Helper functions to access ray packets of runtime size N */
RTC_FORCEINLINE float& RTCRayN_org_x(RTCRayN* ray, unsigned int N, unsigned int i) { return ((float*)ray)[0*N+i]; }
RTC_FORCEINLINE float& RTCRayN_org_y(RTCRayN* ray, unsigned int N, unsigned int i) { return ((float*)ray)[1*N+i]; }
RTC_FORCEINLINE float& RTCRayN_org_z(RTCRayN* ray, unsigned int N, unsigned int i) { return ((float*)ray)[2*N+i]; }
RTC_FORCEINLINE float& RTCRayN_tnear(RTCRayN* ray, unsigned int N, unsigned int i) { return ((float*)ray)[3*N+i]; }

RTC_FORCEINLINE float& RTCRayN_dir_x(RTCRayN* ray, unsigned int N, unsigned int i) { return ((float*)ray)[4*N+i]; }
RTC_FORCEINLINE float& RTCRayN_dir_y(RTCRayN* ray, unsigned int N, unsigned int i) { return ((float*)ray)[5*N+i]; }
RTC_FORCEINLINE float& RTCRayN_dir_z(RTCRayN* ray, unsigned int N, unsigned int i) { return ((float*)ray)[6*N+i]; }
RTC_FORCEINLINE float& RTCRayN_time (RTCRayN* ray, unsigned int N, unsigned int i) { return ((float*)ray)[7*N+i]; }

RTC_FORCEINLINE float&        RTCRayN_tfar (RTCRayN* ray, unsigned int N, unsigned int i) { return ((float*)ray)[8*N+i]; }
RTC_FORCEINLINE unsigned int& RTCRayN_mask (RTCRayN* ray, unsigned int N, unsigned int i) { return ((unsigned*)ray)[9*N+i]; }
RTC_FORCEINLINE unsigned int& RTCRayN_id   (RTCRayN* ray, unsigned int N, unsigned int i) { return ((unsigned*)ray)[10*N+i]; }
RTC_FORCEINLINE unsigned int& RTCRayN_flags(RTCRayN* ray, unsigned int N, unsigned int i) { return ((unsigned*)ray)[11*N+i]; }

/* Helper functions to access hit packets of runtime size N */
RTC_FORCEINLINE float& RTCHitN_Ng_x(RTCHitN* hit, unsigned int N, unsigned int i) { return ((float*)hit)[0*N+i]; }
RTC_FORCEINLINE float& RTCHitN_Ng_y(RTCHitN* hit, unsigned int N, unsigned int i) { return ((float*)hit)[1*N+i]; }
RTC_FORCEINLINE float& RTCHitN_Ng_z(RTCHitN* hit, unsigned int N, unsigned int i) { return ((float*)hit)[2*N+i]; }

RTC_FORCEINLINE float& RTCHitN_u(RTCHitN* hit, unsigned int N, unsigned int i) { return ((float*)hit)[3*N+i]; }
RTC_FORCEINLINE float& RTCHitN_v(RTCHitN* hit, unsigned int N, unsigned int i) { return ((float*)hit)[4*N+i]; }

RTC_FORCEINLINE unsigned int& RTCHitN_primID    (RTCHitN* hit, unsigned int N, unsigned int i) { return ((unsigned*)hit)[5*N+i]; }
RTC_FORCEINLINE unsigned int& RTCHitN_geomID    (RTCHitN* hit, unsigned int N, unsigned int i) { return ((unsigned*)hit)[6*N+i]; }
RTC_FORCEINLINE unsigned int& RTCHitN_instID    (RTCHitN* hit, unsigned int N, unsigned int i, unsigned int l) { return ((unsigned*)hit)[7*N +                                  N*l + i]; }
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
RTC_FORCEINLINE unsigned int& RTCHitN_instPrimID(RTCHitN* hit, unsigned int N, unsigned int i, unsigned int l) { return ((unsigned*)hit)[7*N + N*RTC_MAX_INSTANCE_LEVEL_COUNT + N*l + i]; }
#endif

/* Helper functions to extract RTCRayN and RTCHitN from RTCRayHitN */
RTC_FORCEINLINE RTCRayN* RTCRayHitN_RayN(RTCRayHitN* rayhit, unsigned int N) { return (RTCRayN*)&((float*)rayhit)[0*N]; }
RTC_FORCEINLINE RTCHitN* RTCRayHitN_HitN(RTCRayHitN* rayhit, unsigned int N) { return (RTCHitN*)&((float*)rayhit)[12*N]; }

/* Helper structure for a ray packet of compile-time size N */
template<unsigned int N>
struct RTC_ALIGN((N && !(N & (N - 1)) ? (N * 4 > 16 ? N * 4 : 16) : 16)) RTCRayNt
{
  float org_x[N];
  float org_y[N];
  float org_z[N];
  float tnear[N];

  float dir_x[N];
  float dir_y[N];
  float dir_z[N];
  float time[N];

  float tfar[N];
  unsigned int mask[N];
  unsigned int id[N];
  unsigned int flags[N];
};

/* Helper structure for a hit packet of compile-time size N */
template<unsigned int N>
struct RTC_ALIGN((N && !(N & (N - 1)) ? (N * 4 > 16 ? N * 4 : 16) : 16)) RTCHitNt
{
  float Ng_x[N];
  float Ng_y[N];
  float Ng_z[N];

  float u[N];
  float v[N];

  unsigned int primID[N];
  unsigned int geomID[N];
  unsigned int instID[RTC_MAX_INSTANCE_LEVEL_COUNT][N];
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
  unsigned int instPrimID[RTC_MAX_INSTANCE_LEVEL_COUNT][N];
#endif
};

/* Helper structure for a combined ray/hit packet of compile-time size N */
template<int N>
struct RTCRayHitNt
{
  RTCRayNt<N> ray;
  RTCHitNt<N> hit;
};

RTC_FORCEINLINE RTCRay rtcGetRayFromRayN(RTCRayN* rayN, unsigned int N, unsigned int i)
{
  RTCRay ray;
  ray.org_x = RTCRayN_org_x(rayN,N,i);
  ray.org_y = RTCRayN_org_y(rayN,N,i);
  ray.org_z = RTCRayN_org_z(rayN,N,i);
  ray.tnear = RTCRayN_tnear(rayN,N,i);
  ray.dir_x = RTCRayN_dir_x(rayN,N,i);
  ray.dir_y = RTCRayN_dir_y(rayN,N,i);
  ray.dir_z = RTCRayN_dir_z(rayN,N,i);
  ray.time  = RTCRayN_time(rayN,N,i);
  ray.tfar  = RTCRayN_tfar(rayN,N,i);
  ray.mask  = RTCRayN_mask(rayN,N,i);
  ray.id    = RTCRayN_id(rayN,N,i);
  ray.flags = RTCRayN_flags(rayN,N,i);
  return ray;
}

RTC_FORCEINLINE RTCHit rtcGetHitFromHitN(RTCHitN* hitN, unsigned int N, unsigned int i)
{
  RTCHit hit;
  hit.Ng_x   = RTCHitN_Ng_x(hitN,N,i);
  hit.Ng_y   = RTCHitN_Ng_y(hitN,N,i);
  hit.Ng_z   = RTCHitN_Ng_z(hitN,N,i);
  hit.u      = RTCHitN_u(hitN,N,i);
  hit.v      = RTCHitN_v(hitN,N,i);
  hit.primID = RTCHitN_primID(hitN,N,i);
  hit.geomID = RTCHitN_geomID(hitN,N,i);
  for (unsigned int l = 0; l < RTC_MAX_INSTANCE_LEVEL_COUNT; l++) {
    hit.instID[l] = RTCHitN_instID(hitN,N,i,l);
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
    hit.instPrimID[l] = RTCHitN_instPrimID(hitN,N,i,l);
#endif
  }
  return hit;
}

RTC_FORCEINLINE void rtcCopyHitToHitN(RTCHitN* hitN, const RTCHit* hit, unsigned int N, unsigned int i)
{
  RTCHitN_Ng_x(hitN,N,i)   = hit->Ng_x;
  RTCHitN_Ng_y(hitN,N,i)   = hit->Ng_y;
  RTCHitN_Ng_z(hitN,N,i)   = hit->Ng_z;
  RTCHitN_u(hitN,N,i)      = hit->u;
  RTCHitN_v(hitN,N,i)      = hit->v;
  RTCHitN_primID(hitN,N,i) = hit->primID;
  RTCHitN_geomID(hitN,N,i) = hit->geomID;
  for (unsigned int l = 0; l < RTC_MAX_INSTANCE_LEVEL_COUNT; l++) {
    RTCHitN_instID(hitN,N,i,l) = hit->instID[l];
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
    RTCHitN_instPrimID(hitN,N,i,l) = hit->instPrimID[l];
#endif
  }
}

RTC_FORCEINLINE RTCRayHit rtcGetRayHitFromRayHitN(RTCRayHitN* rayhitN, unsigned int N, unsigned int i)
{
  RTCRayHit rh;

  RTCRayN* ray = RTCRayHitN_RayN(rayhitN,N);
  rh.ray.org_x = RTCRayN_org_x(ray,N,i);
  rh.ray.org_y = RTCRayN_org_y(ray,N,i);
  rh.ray.org_z = RTCRayN_org_z(ray,N,i);
  rh.ray.tnear = RTCRayN_tnear(ray,N,i);
  rh.ray.dir_x = RTCRayN_dir_x(ray,N,i);
  rh.ray.dir_y = RTCRayN_dir_y(ray,N,i);
  rh.ray.dir_z = RTCRayN_dir_z(ray,N,i);
  rh.ray.time  = RTCRayN_time(ray,N,i);
  rh.ray.tfar  = RTCRayN_tfar(ray,N,i);
  rh.ray.mask  = RTCRayN_mask(ray,N,i);
  rh.ray.id    = RTCRayN_id(ray,N,i);
  rh.ray.flags = RTCRayN_flags(ray,N,i);

  RTCHitN* hit  = RTCRayHitN_HitN(rayhitN,N);
  rh.hit.Ng_x   = RTCHitN_Ng_x(hit,N,i);
  rh.hit.Ng_y   = RTCHitN_Ng_y(hit,N,i);
  rh.hit.Ng_z   = RTCHitN_Ng_z(hit,N,i);
  rh.hit.u      = RTCHitN_u(hit,N,i);
  rh.hit.v      = RTCHitN_v(hit,N,i);
  rh.hit.primID = RTCHitN_primID(hit,N,i);
  rh.hit.geomID = RTCHitN_geomID(hit,N,i);
  for (unsigned int l = 0; l < RTC_MAX_INSTANCE_LEVEL_COUNT; l++) {
    rh.hit.instID[l] = RTCHitN_instID(hit,N,i,l);
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
    rh.hit.instPrimID[l] = RTCHitN_instPrimID(hit,N,i,l);
#endif
  }

  return rh;
}

#endif

RTC_NAMESPACE_END

