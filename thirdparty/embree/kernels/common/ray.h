// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "default.h"
#include "instance_stack.h"

// FIXME: if ray gets separated into ray* and hit, uload4 needs to be adjusted

namespace embree
{
  /* Ray structure for K rays */
  template<int K>
  struct RayK
  {
    /* Default construction does nothing */
    __forceinline RayK() {}

    /* Constructs a ray from origin, direction, and ray segment. Near
     * has to be smaller than far */
    __forceinline RayK(const Vec3vf<K>& org, const Vec3vf<K>& dir,
                       const vfloat<K>& tnear = zero, const vfloat<K>& tfar = inf,
                       const vfloat<K>& time = zero, const vint<K>& mask = -1, const vint<K>& id = 0, const vint<K>& flags = 0)
      : org(org), dir(dir), _tnear(tnear), tfar(tfar), _time(time), mask(mask), id(id), flags(flags) {}

    /* Returns the size of the ray */
    static __forceinline size_t size() { return K; }

    /* Calculates if this is a valid ray that does not cause issues during traversal */
    __forceinline vbool<K> valid() const
    {
      const vbool<K> vx = (abs(org.x) <= vfloat<K>(FLT_LARGE)) & (abs(dir.x) <= vfloat<K>(FLT_LARGE));
      const vbool<K> vy = (abs(org.y) <= vfloat<K>(FLT_LARGE)) & (abs(dir.y) <= vfloat<K>(FLT_LARGE));
      const vbool<K> vz = (abs(org.z) <= vfloat<K>(FLT_LARGE)) & (abs(dir.z) <= vfloat<K>(FLT_LARGE));
      const vbool<K> vn = abs(tnear()) <= vfloat<K>(inf);
      const vbool<K> vf = abs(tfar) <= vfloat<K>(inf);
      return vx & vy & vz & vn & vf;
    }

    __forceinline void get(RayK<1>* ray) const;
    __forceinline void get(size_t i, RayK<1>& ray) const;
    __forceinline void set(const RayK<1>* ray);
    __forceinline void set(size_t i, const RayK<1>& ray);

    __forceinline void copy(size_t dest, size_t source);

    __forceinline vint<K> octant() const
    {
      return select(dir.x < 0.0f, vint<K>(1), vint<K>(zero)) |
             select(dir.y < 0.0f, vint<K>(2), vint<K>(zero)) |
             select(dir.z < 0.0f, vint<K>(4), vint<K>(zero));
    }

    /* Ray data */
    Vec3vf<K> org;    // ray origin
    vfloat<K> _tnear; // start of ray segment
    Vec3vf<K> dir;    // ray direction
    vfloat<K> _time;  // time of this ray for motion blur
    vfloat<K> tfar;   // end of ray segment
    vint<K> mask;     // used to mask out objects during traversal
    vint<K> id;      
    vint<K> flags;  

    __forceinline vfloat<K>& tnear() { return _tnear; }
    __forceinline vfloat<K>& time()  { return _time; }
    __forceinline const vfloat<K>& tnear() const { return _tnear; }
    __forceinline const vfloat<K>& time()  const { return _time; }
  };

  /* Ray+hit structure for K rays */
  template<int K>
  struct RayHitK : RayK<K>
  {
    using RayK<K>::org;
    using RayK<K>::_tnear;
    using RayK<K>::dir;
    using RayK<K>::_time;
    using RayK<K>::tfar;
    using RayK<K>::mask;
    using RayK<K>::id;
    using RayK<K>::flags;

    using RayK<K>::tnear;
    using RayK<K>::time;

    /* Default construction does nothing */
    __forceinline RayHitK() {}

    /* Constructs a ray from origin, direction, and ray segment. Near
     * has to be smaller than far */
    __forceinline RayHitK(const Vec3vf<K>& org, const Vec3vf<K>& dir,
                          const vfloat<K>& tnear = zero, const vfloat<K>& tfar = inf,
                          const vfloat<K>& time = zero, const vint<K>& mask = -1, const vint<K>& id = 0, const vint<K>& flags = 0)
      : RayK<K>(org, dir, tnear, tfar, time, mask, id, flags),
        geomID(RTC_INVALID_GEOMETRY_ID) 
    {
      for (unsigned l = 0; l < RTC_MAX_INSTANCE_LEVEL_COUNT; ++l) {
        instID[l] = RTC_INVALID_GEOMETRY_ID;
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
        instPrimID[l] = RTC_INVALID_GEOMETRY_ID;
#endif
      }
    }

    __forceinline RayHitK(const RayK<K>& ray)
      : RayK<K>(ray),
        geomID(RTC_INVALID_GEOMETRY_ID) 
    {
      for (unsigned l = 0; l < RTC_MAX_INSTANCE_LEVEL_COUNT; ++l) {
        instID[l] = RTC_INVALID_GEOMETRY_ID;
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
        instPrimID[l] = RTC_INVALID_GEOMETRY_ID;
#endif
      }
    }

    __forceinline RayHitK<K>& operator =(const RayK<K>& ray)
    {
      org    = ray.org;
      _tnear = ray._tnear;
      dir    = ray.dir;
      _time  = ray._time;
      tfar   = ray.tfar;
      mask   = ray.mask;
      id     = ray.id;
      flags  = ray.flags;

      geomID = RTC_INVALID_GEOMETRY_ID;
      for (unsigned l = 0; l < RTC_MAX_INSTANCE_LEVEL_COUNT; ++l) {
        instID[l] = RTC_INVALID_GEOMETRY_ID;
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
        instPrimID[l] = RTC_INVALID_GEOMETRY_ID;
#endif
      }

      return *this;
    }

    /* Calculates if the hit is valid */
    __forceinline void verifyHit(const vbool<K>& valid0) const
    {
      vbool<K> valid = valid0 & geomID != vuint<K>(RTC_INVALID_GEOMETRY_ID);
      const vbool<K> vt = (abs(tfar) <= vfloat<K>(FLT_LARGE)) | (tfar == vfloat<K>(neg_inf));
      const vbool<K> vu = (abs(u) <= vfloat<K>(FLT_LARGE));
      const vbool<K> vv = (abs(v) <= vfloat<K>(FLT_LARGE));
      const vbool<K> vnx = abs(Ng.x) <= vfloat<K>(FLT_LARGE);
      const vbool<K> vny = abs(Ng.y) <= vfloat<K>(FLT_LARGE);
      const vbool<K> vnz = abs(Ng.z) <= vfloat<K>(FLT_LARGE);
      if (any(valid & !vt)) throw_RTCError(RTC_ERROR_UNKNOWN,"invalid t");
      if (any(valid & !vu)) throw_RTCError(RTC_ERROR_UNKNOWN,"invalid u");
      if (any(valid & !vv)) throw_RTCError(RTC_ERROR_UNKNOWN,"invalid v");
      if (any(valid & !vnx)) throw_RTCError(RTC_ERROR_UNKNOWN,"invalid Ng.x");
      if (any(valid & !vny)) throw_RTCError(RTC_ERROR_UNKNOWN,"invalid Ng.y");
      if (any(valid & !vnz)) throw_RTCError(RTC_ERROR_UNKNOWN,"invalid Ng.z");
    }

    __forceinline void get(RayHitK<1>* ray) const;
    __forceinline void get(size_t i, RayHitK<1>& ray) const;
    __forceinline void set(const RayHitK<1>* ray);
    __forceinline void set(size_t i, const RayHitK<1>& ray);

    __forceinline void copy(size_t dest, size_t source);

    /* Hit data */
    Vec3vf<K> Ng;   // geometry normal
    vfloat<K> u;    // barycentric u coordinate of hit
    vfloat<K> v;    // barycentric v coordinate of hit
    vuint<K> primID; // primitive ID
    vuint<K> geomID; // geometry ID
    vuint<K> instID[RTC_MAX_INSTANCE_LEVEL_COUNT]; // instance ID
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
    vuint<K> instPrimID[RTC_MAX_INSTANCE_LEVEL_COUNT]; // instance prim ID
#endif
};

  /* Specialization for a single ray */
  template<>
  struct RayK<1>
  {
    /* Default construction does nothing */
    __forceinline RayK() {}

    /* Constructs a ray from origin, direction, and ray segment. Near
     *  has to be smaller than far */
    __forceinline RayK(const Vec3fa& org, const Vec3fa& dir, float tnear = zero, float tfar = inf, float time = zero, int mask = -1, int id = 0, int flags = 0)
      : org(org,tnear), dir(dir,time), tfar(tfar), mask(mask), id(id), flags(flags) {}

    /* Calculates if this is a valid ray that does not cause issues during traversal */
    __forceinline bool valid() const {
      return all(le_mask(abs(Vec3fa(org)), Vec3fa(FLT_LARGE)) & le_mask(abs(Vec3fa(dir)), Vec3fa(FLT_LARGE))) && abs(tnear()) <= float(inf) && abs(tfar) <= float(inf);
    }

    /* checks if occlusion ray is done */
    __forceinline bool occluded() const {
      return tfar < 0.0f;
    }

    /* Ray data */
    Vec3ff org;  // 3 floats for ray origin, 1 float for tnear
    //float tnear; // start of ray segment
    Vec3ff dir;  // 3 floats for ray direction, 1 float for time
    // float time; 
    float tfar;  // end of ray segment
    int mask;    // used to mask out objects during traversal
    int id;      // ray ID
    int flags;   // ray flags

    __forceinline float& tnear() { return org.w; };
    __forceinline const float& tnear() const { return org.w; };

    __forceinline float& time() { return dir.w; };
    __forceinline const float& time() const { return dir.w; };

  };

  template<>
  struct RayHitK<1> : RayK<1>
  {
    /* Default construction does nothing */
    __forceinline RayHitK() {}

    /* Constructs a ray from origin, direction, and ray segment. Near
     *  has to be smaller than far */
    __forceinline RayHitK(const Vec3fa& org, const Vec3fa& dir, float tnear = zero, float tfar = inf, float time = zero, int mask = -1, int id = 0, int flags = 0)
      : RayK<1>(org, dir, tnear, tfar, time, mask, id, flags),
        geomID(RTC_INVALID_GEOMETRY_ID) {}

    __forceinline RayHitK(const RayK<1>& ray)
      : RayK<1>(ray),
        geomID(RTC_INVALID_GEOMETRY_ID) {}

    __forceinline RayHitK<1>& operator =(const RayK<1>& ray)
    {
      org    = ray.org;
      dir    = ray.dir;
      tfar   = ray.tfar;
      mask   = ray.mask;
      id     = ray.id;
      flags  = ray.flags;

      geomID = RTC_INVALID_GEOMETRY_ID;

      return *this;
    }

    /* Calculates if the hit is valid */
    __forceinline void verifyHit() const
    {
      if (geomID == RTC_INVALID_GEOMETRY_ID) return;
      const bool vt = (abs(tfar) <= FLT_LARGE) || (tfar == float(neg_inf));
      const bool vu = (abs(u) <= FLT_LARGE);
      const bool vv = (abs(u) <= FLT_LARGE);
      const bool vnx = abs(Ng.x) <= FLT_LARGE;
      const bool vny = abs(Ng.y) <= FLT_LARGE;
      const bool vnz = abs(Ng.z) <= FLT_LARGE;
      if (!vt) throw_RTCError(RTC_ERROR_UNKNOWN, "invalid t");
      if (!vu) throw_RTCError(RTC_ERROR_UNKNOWN, "invalid u");
      if (!vv) throw_RTCError(RTC_ERROR_UNKNOWN, "invalid v");
      if (!vnx) throw_RTCError(RTC_ERROR_UNKNOWN, "invalid Ng.x");
      if (!vny) throw_RTCError(RTC_ERROR_UNKNOWN, "invalid Ng.y");
      if (!vnz) throw_RTCError(RTC_ERROR_UNKNOWN, "invalid Ng.z");
    }

    /* Hit data */
    Vec3f Ng;            // not normalized geometry normal
    float u;             // barycentric u coordinate of hit
    float v;             // barycentric v coordinate of hit
    unsigned int primID; // primitive ID
    unsigned int geomID; // geometry ID
    unsigned int instID[RTC_MAX_INSTANCE_LEVEL_COUNT]; // instance ID
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
    unsigned int instPrimID[RTC_MAX_INSTANCE_LEVEL_COUNT]; // instance primitive ID
#endif
  };

  /* Converts ray packet to single rays */
  template<int K>
  __forceinline void RayK<K>::get(RayK<1>* ray) const
  {
    for (size_t i = 0; i < K; i++) // FIXME: use SIMD transpose
    {
      ray[i].org.x = org.x[i]; ray[i].org.y = org.y[i]; ray[i].org.z = org.z[i]; ray[i].tnear() = tnear()[i];
      ray[i].dir.x = dir.x[i]; ray[i].dir.y = dir.y[i]; ray[i].dir.z = dir.z[i]; ray[i].time()  = time()[i];
      ray[i].tfar  = tfar[i];  ray[i].mask = mask[i]; ray[i].id = id[i]; ray[i].flags = flags[i];
    }
  }

  template<int K>
  __forceinline void RayHitK<K>::get(RayHitK<1>* ray) const
  {
    // FIXME: use SIMD transpose
    for (size_t i = 0; i < K; i++)
      get(i, ray[i]);
  }

  /* Extracts a single ray out of a ray packet*/
  template<int K>
  __forceinline void RayK<K>::get(size_t i, RayK<1>& ray) const
  {
    ray.org.x = org.x[i]; ray.org.y = org.y[i]; ray.org.z = org.z[i]; ray.tnear() = tnear()[i]; 
    ray.dir.x = dir.x[i]; ray.dir.y = dir.y[i]; ray.dir.z = dir.z[i]; ray.time()  = time()[i];  
    ray.tfar  = tfar[i]; ray.mask = mask[i];  ray.id = id[i]; ray.flags = flags[i];
  }

  template<int K>
  __forceinline void RayHitK<K>::get(size_t i, RayHitK<1>& ray) const
  {
    ray.org.x = org.x[i]; ray.org.y = org.y[i]; ray.org.z = org.z[i]; ray.tnear() = tnear()[i];
    ray.dir.x = dir.x[i]; ray.dir.y = dir.y[i]; ray.dir.z = dir.z[i]; ray.tfar  = tfar[i]; ray.time()  = time()[i]; 
    ray.mask = mask[i];  ray.id = id[i]; ray.flags = flags[i];
    ray.Ng.x = Ng.x[i]; ray.Ng.y = Ng.y[i]; ray.Ng.z = Ng.z[i];
    ray.u = u[i]; ray.v = v[i];
    ray.primID = primID[i]; ray.geomID = geomID[i]; 

    instance_id_stack::copy_VU<K>(instID, ray.instID, i);
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
    instance_id_stack::copy_VU<K>(instPrimID, ray.instPrimID, i);
#endif
  }

  /* Converts single rays to ray packet */
  template<int K>
  __forceinline void RayK<K>::set(const RayK<1>* ray)
  {
    // FIXME: use SIMD transpose
    for (size_t i = 0; i < K; i++)
      set(i, ray[i]);
  }

  template<int K>
  __forceinline void RayHitK<K>::set(const RayHitK<1>* ray)
  {
    // FIXME: use SIMD transpose
    for (size_t i = 0; i < K; i++)
      set(i, ray[i]);
  }

  /* inserts a single ray into a ray packet element */
  template<int K>
  __forceinline void RayK<K>::set(size_t i, const RayK<1>& ray)
  {
    org.x[i] = ray.org.x; org.y[i] = ray.org.y; org.z[i] = ray.org.z; tnear()[i] = ray.tnear();
    dir.x[i] = ray.dir.x; dir.y[i] = ray.dir.y; dir.z[i] = ray.dir.z; time()[i] = ray.time();
    tfar[i] = ray.tfar; mask[i] = ray.mask; id[i] = ray.id; flags[i] = ray.flags;
  }

  template<int K>
  __forceinline void RayHitK<K>::set(size_t i, const RayHitK<1>& ray)
  {
    org.x[i] = ray.org.x; org.y[i] = ray.org.y; org.z[i] = ray.org.z; tnear()[i] = ray.tnear();
    dir.x[i] = ray.dir.x; dir.y[i] = ray.dir.y; dir.z[i] = ray.dir.z; time()[i] = ray.time();
    tfar[i] = ray.tfar; mask[i] = ray.mask; id[i] = ray.id; flags[i] = ray.flags;
    Ng.x[i] = ray.Ng.x; Ng.y[i] = ray.Ng.y; Ng.z[i] = ray.Ng.z;
    u[i] = ray.u; v[i] = ray.v;
    primID[i] = ray.primID; geomID[i] = ray.geomID;

    instance_id_stack::copy_UV<K>(ray.instID, instID, i);
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
    instance_id_stack::copy_UV<K>(ray.instPrimID, instPrimID, i);
#endif
  }

  /* copies a ray packet element into another element*/
  template<int K>
  __forceinline void RayK<K>::copy(size_t dest, size_t source)
  {
    org.x[dest] = org.x[source]; org.y[dest] = org.y[source]; org.z[dest] = org.z[source]; tnear()[dest] = tnear()[source];
    dir.x[dest] = dir.x[source]; dir.y[dest] = dir.y[source]; dir.z[dest] = dir.z[source]; time()[dest] = time()[source]; 
    tfar [dest] = tfar[source]; mask[dest] = mask[source]; id[dest] = id[source]; flags[dest] = flags[source]; 
  }

  template<int K>
  __forceinline void RayHitK<K>::copy(size_t dest, size_t source)
  {
    org.x[dest] = org.x[source]; org.y[dest] = org.y[source]; org.z[dest] = org.z[source]; tnear()[dest] = tnear()[source];
    dir.x[dest] = dir.x[source]; dir.y[dest] = dir.y[source]; dir.z[dest] = dir.z[source]; time()[dest] = time()[source]; 
    tfar [dest] = tfar[source]; mask[dest] = mask[source]; id[dest] = id[source]; flags[dest] = flags[source];
    Ng.x[dest] = Ng.x[source]; Ng.y[dest] = Ng.y[source]; Ng.z[dest] = Ng.z[source];
    u[dest] = u[source]; v[dest] = v[source];
    primID[dest] = primID[source]; geomID[dest] = geomID[source];  

    instance_id_stack::copy_VV<K>(instID, instID, source, dest);
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
    instance_id_stack::copy_VV<K>(instPrimID, instPrimID, source, dest);
#endif
  }

  /* Shortcuts */
  typedef RayK<1>  Ray;
  typedef RayK<4>  Ray4;
  typedef RayK<8>  Ray8;
  typedef RayK<16> Ray16;
  typedef RayK<VSIZEX> Rayx;
  struct RayN;

  typedef RayHitK<1>  RayHit;
  typedef RayHitK<4>  RayHit4;
  typedef RayHitK<8>  RayHit8;
  typedef RayHitK<16> RayHit16;
  typedef RayHitK<VSIZEX> RayHitx;
  struct RayHitN;

  template<int K, bool intersect>
  struct RayTypeHelper;

  template<int K>
  struct RayTypeHelper<K, true>
  {
    typedef RayHitK<K> Ty;
  };

  template<int K>
  struct RayTypeHelper<K, false>
  {
    typedef RayK<K> Ty;
  };

  template<bool intersect>
  using RayType = typename RayTypeHelper<1, intersect>::Ty;

  template<int K, bool intersect>
  using RayTypeK = typename RayTypeHelper<K, intersect>::Ty;

  /* Outputs ray to stream */
  template<int K>
  __forceinline embree_ostream operator <<(embree_ostream cout, const RayK<K>& ray)
  {
    return cout << "{ " << embree_endl
                << "  org = " << ray.org << embree_endl
                << "  dir = " << ray.dir << embree_endl
                << "  near = " << ray.tnear() << embree_endl
                << "  far = " << ray.tfar << embree_endl
                << "  time = " << ray.time() << embree_endl
                << "  mask = " << ray.mask << embree_endl
                << "  id = " << ray.id << embree_endl
                << "  flags = " << ray.flags << embree_endl
                << "}";
  }

  template<int K>
  __forceinline embree_ostream operator <<(embree_ostream cout, const RayHitK<K>& ray)
  {
    cout << "{ " << embree_endl
         << "  org = " << ray.org << embree_endl
         << "  dir = " << ray.dir << embree_endl
         << "  near = " << ray.tnear() << embree_endl
         << "  far = " << ray.tfar << embree_endl
         << "  time = " << ray.time() << embree_endl
         << "  mask = " << ray.mask << embree_endl
         << "  id = " << ray.id << embree_endl
         << "  flags = " << ray.flags << embree_endl
         << "  Ng = " << ray.Ng
         << "  u = " << ray.u <<  embree_endl
         << "  v = " << ray.v << embree_endl
         << "  primID = " << ray.primID <<  embree_endl
         << "  geomID = " << ray.geomID << embree_endl
         << "  instID =";
    for (unsigned l = 0; l < RTC_MAX_INSTANCE_LEVEL_COUNT; ++l)
    {
      cout << " " << ray.instID[l];
    }
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
    cout << "  instPrimID =";
    for (unsigned l = 0; l < RTC_MAX_INSTANCE_LEVEL_COUNT; ++l)
    {
      cout << " " << ray.instPrimID[l];
    }
#endif
    cout << embree_endl;
    return cout << "}";
  }

  struct RayStreamSOA
  {
    __forceinline RayStreamSOA(void* rays, size_t N)
      : ptr((char*)rays), N(N) {}

    /* ray data access functions */
    __forceinline float* org_x(size_t offset = 0) { return (float*)&ptr[0*4*N+offset]; }  // x coordinate of ray origin
    __forceinline float* org_y(size_t offset = 0) { return (float*)&ptr[1*4*N+offset]; }  // y coordinate of ray origin
    __forceinline float* org_z(size_t offset = 0) { return (float*)&ptr[2*4*N+offset]; }; // z coordinate of ray origin
    __forceinline float* tnear(size_t offset = 0) { return (float*)&ptr[3*4*N+offset]; }; // start of ray segment

    __forceinline float* dir_x(size_t offset = 0) { return (float*)&ptr[4*4*N+offset]; }; // x coordinate of ray direction
    __forceinline float* dir_y(size_t offset = 0) { return (float*)&ptr[5*4*N+offset]; }; // y coordinate of ray direction
    __forceinline float* dir_z(size_t offset = 0) { return (float*)&ptr[6*4*N+offset]; }; // z coordinate of ray direction
    __forceinline float* time (size_t offset = 0) { return (float*)&ptr[7*4*N+offset]; }; // time of this ray for motion blur

    __forceinline float* tfar (size_t offset = 0) { return (float*)&ptr[8*4*N+offset]; }; // end of ray segment (set to hit distance)
    __forceinline int*   mask (size_t offset = 0) { return (int*)&ptr[9*4*N+offset];   }; // used to mask out objects during traversal (optional)
    __forceinline int*   id   (size_t offset = 0) { return (int*)&ptr[10*4*N+offset];  }; // id
    __forceinline int*   flags(size_t offset = 0) { return (int*)&ptr[11*4*N+offset];  }; // flags

    /* hit data access functions */
    __forceinline float* Ng_x(size_t offset = 0) { return (float*)&ptr[12*4*N+offset]; }; // x coordinate of geometry normal
    __forceinline float* Ng_y(size_t offset = 0) { return (float*)&ptr[13*4*N+offset]; }; // y coordinate of geometry normal
    __forceinline float* Ng_z(size_t offset = 0) { return (float*)&ptr[14*4*N+offset]; }; // z coordinate of geometry normal

    __forceinline float* u(size_t offset = 0) { return (float*)&ptr[15*4*N+offset]; };    // barycentric u coordinate of hit
    __forceinline float* v(size_t offset = 0) { return (float*)&ptr[16*4*N+offset]; };    // barycentric v coordinate of hit

    __forceinline unsigned int* primID(size_t offset = 0) { return (unsigned int*)&ptr[17*4*N+offset]; };   // primitive ID
    __forceinline unsigned int* geomID(size_t offset = 0) { return (unsigned int*)&ptr[18*4*N+offset]; };   // geometry ID
    __forceinline unsigned int* instID(size_t level, size_t offset = 0) { return (unsigned int*)&ptr[19*4*N+level*4*N+offset]; };   // instance ID
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
    __forceinline unsigned int* instPrimID(size_t level, size_t offset = 0) { return (unsigned int*)&ptr[19*4*N+RTC_MAX_INSTANCE_LEVEL_COUNT*4*N+level*4*N+offset]; };   // instance primitive ID
#endif

    __forceinline Ray getRayByOffset(size_t offset)
    {
      Ray ray;
      ray.org.x   = org_x(offset)[0];
      ray.org.y   = org_y(offset)[0];
      ray.org.z   = org_z(offset)[0];
      ray.tnear() = tnear(offset)[0];
      ray.dir.x   = dir_x(offset)[0];
      ray.dir.y   = dir_y(offset)[0];
      ray.dir.z   = dir_z(offset)[0];
      ray.time()  = time(offset)[0];
      ray.tfar    = tfar(offset)[0];
      ray.mask    = mask(offset)[0];
      ray.id      = id(offset)[0];
      ray.flags   = flags(offset)[0];
      return ray;
    }

    template<int K>
    __forceinline RayK<K> getRayByOffset(size_t offset)
    {
      RayK<K> ray;
      ray.org.x  = vfloat<K>::loadu(org_x(offset));
      ray.org.y  = vfloat<K>::loadu(org_y(offset));
      ray.org.z  = vfloat<K>::loadu(org_z(offset));
      ray.tnear  = vfloat<K>::loadu(tnear(offset));
      ray.dir.x  = vfloat<K>::loadu(dir_x(offset));
      ray.dir.y  = vfloat<K>::loadu(dir_y(offset));
      ray.dir.z  = vfloat<K>::loadu(dir_z(offset));
      ray.time   = vfloat<K>::loadu(time(offset));
      ray.tfar   = vfloat<K>::loadu(tfar(offset));
      ray.mask   = vint<K>::loadu(mask(offset));
      ray.id     = vint<K>::loadu(id(offset));
      ray.flags  = vint<K>::loadu(flags(offset));
      return ray;
    }

    template<int K>
    __forceinline RayK<K> getRayByOffset(const vbool<K>& valid, size_t offset)
    {
      RayK<K> ray;
      ray.org.x   = vfloat<K>::loadu(valid, org_x(offset));
      ray.org.y   = vfloat<K>::loadu(valid, org_y(offset));
      ray.org.z   = vfloat<K>::loadu(valid, org_z(offset));
      ray.tnear() = vfloat<K>::loadu(valid, tnear(offset));
      ray.dir.x   = vfloat<K>::loadu(valid, dir_x(offset));
      ray.dir.y   = vfloat<K>::loadu(valid, dir_y(offset));
      ray.dir.z   = vfloat<K>::loadu(valid, dir_z(offset));
      ray.time()  = vfloat<K>::loadu(valid, time(offset));
      ray.tfar  = vfloat<K>::loadu(valid, tfar(offset));

#if !defined(__AVX__)
      /* SSE: some ray members must be loaded with scalar instructions to ensure that we don't cause memory faults,
         because the SSE masked loads always access the entire vector */
      if (unlikely(!all(valid)))
      {
        ray.mask  = zero;
        ray.id    = zero;
        ray.flags = zero;

        for (size_t k = 0; k < K; k++)
        {
          if (likely(valid[k]))
          {
            ray.mask[k]  = mask(offset)[k];
            ray.id[k]    = id(offset)[k];
            ray.flags[k] = flags(offset)[k];
          }
        }
      }
      else
#endif
      {
        ray.mask  = vint<K>::loadu(valid, mask(offset));
        ray.id    = vint<K>::loadu(valid, id(offset));
        ray.flags = vint<K>::loadu(valid, flags(offset));
      }

      return ray;
    }

    template<int K>
    __forceinline void setHitByOffset(const vbool<K>& valid_i, size_t offset, const RayHitK<K>& ray)
    {
      /* 
       * valid_i: stores which of the input rays exist (do not access nonexistent rays!)
       * valid:   stores which of the rays actually hit something.
       */
      vbool<K> valid = valid_i;
      valid &= (ray.geomID != RTC_INVALID_GEOMETRY_ID);

      if (likely(any(valid)))
      {
        vfloat<K>::storeu(valid, tfar(offset), ray.tfar);
        vfloat<K>::storeu(valid, Ng_x(offset), ray.Ng.x);
        vfloat<K>::storeu(valid, Ng_y(offset), ray.Ng.y);
        vfloat<K>::storeu(valid, Ng_z(offset), ray.Ng.z);
        vfloat<K>::storeu(valid, u(offset), ray.u);
        vfloat<K>::storeu(valid, v(offset), ray.v);

#if !defined(__AVX__)
        /* SSE: some ray members must be stored with scalar instructions to ensure that we don't cause memory faults,
           because the SSE masked stores always access the entire vector */
        if (unlikely(!all(valid_i)))
        {
          for (size_t k = 0; k < K; k++)
          {
            if (likely(valid[k]))
            {
              primID(offset)[k] = ray.primID[k];
              geomID(offset)[k] = ray.geomID[k];

              instID(0, offset)[k] = ray.instID[0][k];
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
              instPrimID(0, offset)[k] = ray.instPrimID[0][k];
#endif
#if (RTC_MAX_INSTANCE_LEVEL_COUNT > 1)
              for (unsigned l = 1; l < RTC_MAX_INSTANCE_LEVEL_COUNT && ray.instID[l-1][k] != RTC_INVALID_GEOMETRY_ID; ++l) {
                instID(l, offset)[k] = ray.instID[l][k];
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
                instPrimID(l, offset)[k] = ray.instPrimID[l][k];
#endif
              }
#endif
            }
          }
        }
        else
#endif
        {
          vuint<K>::storeu(valid, primID(offset), ray.primID);
          vuint<K>::storeu(valid, geomID(offset), ray.geomID);

          vuint<K>::storeu(valid, instID(0, offset), ray.instID[0]);
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
          vuint<K>::storeu(valid, instPrimID(0, offset), ray.instPrimID[0]);
#endif
#if (RTC_MAX_INSTANCE_LEVEL_COUNT > 1)
          for (unsigned l = 1; l < RTC_MAX_INSTANCE_LEVEL_COUNT && any(valid & (ray.instID[l-1] != RTC_INVALID_GEOMETRY_ID)); ++l) {
            vuint<K>::storeu(valid, instID(l, offset), ray.instID[l]);
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
            vuint<K>::storeu(valid, instPrimID(l, offset), ray.instPrimID[l]);
#endif
          }
#endif
        }
      }
    }

    template<int K>
    __forceinline void setHitByOffset(const vbool<K>& valid_i, size_t offset, const RayK<K>& ray)
    {
      vbool<K> valid = valid_i;
      valid &= (ray.tfar < 0.0f);

      if (likely(any(valid)))
        vfloat<K>::storeu(valid, tfar(offset), ray.tfar);
    }

    __forceinline size_t getOctantByOffset(size_t offset)
    {
      const float dx = dir_x(offset)[0];
      const float dy = dir_y(offset)[0];
      const float dz = dir_z(offset)[0];
      const size_t octantID = (dx < 0.0f ? 1 : 0) + (dy < 0.0f ? 2 : 0) + (dz < 0.0f ? 4 : 0);
      return octantID;
    }

    __forceinline bool isValidByOffset(size_t offset)
    {
      const float nnear = tnear(offset)[0];
      const float ffar  = tfar(offset)[0];
      return nnear <= ffar;
    }

    template<int K>
    __forceinline RayK<K> getRayByOffset(const vbool<K>& valid, const vint<K>& offset)
    {
      RayK<K> ray;

#if defined(__AVX2__)
      ray.org.x   = vfloat<K>::template gather<1>(valid, org_x(), offset);
      ray.org.y   = vfloat<K>::template gather<1>(valid, org_y(), offset);
      ray.org.z   = vfloat<K>::template gather<1>(valid, org_z(), offset);
      ray.tnear() = vfloat<K>::template gather<1>(valid, tnear(), offset);
      ray.dir.x   = vfloat<K>::template gather<1>(valid, dir_x(), offset);
      ray.dir.y   = vfloat<K>::template gather<1>(valid, dir_y(), offset);
      ray.dir.z   = vfloat<K>::template gather<1>(valid, dir_z(), offset);
      ray.time()  = vfloat<K>::template gather<1>(valid, time(), offset);
      ray.tfar    = vfloat<K>::template gather<1>(valid, tfar(), offset);
      ray.mask    = vint<K>::template gather<1>(valid, mask(), offset);
      ray.id      = vint<K>::template gather<1>(valid, id(), offset);
      ray.flags   = vint<K>::template gather<1>(valid, flags(), offset);
#else
      ray.org     = zero;
      ray.tnear() = zero;
      ray.dir     = zero;
      ray.time()  = zero;
      ray.tfar    = zero;
      ray.mask    = zero;
      ray.id      = zero;
      ray.flags   = zero;

      for (size_t k = 0; k < K; k++)
      {
        if (likely(valid[k]))
        {
          const size_t ofs = offset[k];

          ray.org.x[k]   = *org_x(ofs);
          ray.org.y[k]   = *org_y(ofs);
          ray.org.z[k]   = *org_z(ofs);
          ray.tnear()[k] = *tnear(ofs);
          ray.dir.x[k]   = *dir_x(ofs);
          ray.dir.y[k]   = *dir_y(ofs);
          ray.dir.z[k]   = *dir_z(ofs);
          ray.time()[k]  = *time(ofs);
          ray.tfar[k]    = *tfar(ofs);
          ray.mask[k]    = *mask(ofs);
          ray.id[k]      = *id(ofs);
          ray.flags[k]   = *flags(ofs);
        }
      }
#endif

      return ray;
    }

    template<int K>
    __forceinline void setHitByOffset(const vbool<K>& valid_i, const vint<K>& offset, const RayHitK<K>& ray)
    {
      vbool<K> valid = valid_i;
      valid &= (ray.geomID != RTC_INVALID_GEOMETRY_ID);

      if (likely(any(valid)))
      {
#if defined(__AVX512F__)
        vfloat<K>::template scatter<1>(valid, tfar(), offset, ray.tfar);
        vfloat<K>::template scatter<1>(valid, Ng_x(), offset, ray.Ng.x);
        vfloat<K>::template scatter<1>(valid, Ng_y(), offset, ray.Ng.y);
        vfloat<K>::template scatter<1>(valid, Ng_z(), offset, ray.Ng.z);
        vfloat<K>::template scatter<1>(valid, u(), offset, ray.u);
        vfloat<K>::template scatter<1>(valid, v(), offset, ray.v);
        vuint<K>::template scatter<1>(valid, primID(), offset, ray.primID);
        vuint<K>::template scatter<1>(valid, geomID(), offset, ray.geomID);

        vuint<K>::template scatter<1>(valid, instID(0), offset, ray.instID[0]);
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
        vuint<K>::template scatter<1>(valid, instPrimID(0), offset, ray.instPrimID[0]);
#endif
#if (RTC_MAX_INSTANCE_LEVEL_COUNT > 1)
        for (unsigned l = 1; l < RTC_MAX_INSTANCE_LEVEL_COUNT && any(valid & (ray.instID[l-1] != RTC_INVALID_GEOMETRY_ID)); ++l) {
          vuint<K>::template scatter<1>(valid, instID(l), offset, ray.instID[l]);
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
          vuint<K>::template scatter<1>(valid, instPrimID(l), offset, ray.instPrimID[l]);
#endif
        }
#endif
#else
        size_t valid_bits = movemask(valid);
        while (valid_bits != 0)
        {
          const size_t k = bscf(valid_bits);
          const size_t ofs = offset[k];

          *tfar(ofs) = ray.tfar[k];

          *Ng_x(ofs)   = ray.Ng.x[k];
          *Ng_y(ofs)   = ray.Ng.y[k];
          *Ng_z(ofs)   = ray.Ng.z[k];
          *u(ofs)      = ray.u[k];
          *v(ofs)      = ray.v[k];
          *primID(ofs) = ray.primID[k];
          *geomID(ofs) = ray.geomID[k];

          *instID(0, ofs) = ray.instID[0][k];
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
          *instPrimID(0, ofs) = ray.instPrimID[0][k];
#endif
#if (RTC_MAX_INSTANCE_LEVEL_COUNT > 1)
          for (unsigned l = 1; l < RTC_MAX_INSTANCE_LEVEL_COUNT && ray.instID[l-1][k] != RTC_INVALID_GEOMETRY_ID; ++l) {
            *instID(l, ofs) = ray.instID[l][k];
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
            *instPrimID(l, ofs) = ray.instPrimID[l][k];
#endif
          }
#endif
        }
#endif
      }
    }

    template<int K>
    __forceinline void setHitByOffset(const vbool<K>& valid_i, const vint<K>& offset, const RayK<K>& ray)
    {
      vbool<K> valid = valid_i;
      valid &= (ray.tfar < 0.0f);

      if (likely(any(valid)))
      {
#if defined(__AVX512F__)
        vfloat<K>::template scatter<1>(valid, tfar(), offset, ray.tfar);
#else
        size_t valid_bits = movemask(valid);
        while (valid_bits != 0)
        {
          const size_t k = bscf(valid_bits);
          const size_t ofs = offset[k];

          *tfar(ofs) = ray.tfar[k];
        }
#endif
      }
    }

    char* __restrict__ ptr;
    size_t N;
  };

  template<size_t MAX_K>
  struct StackRayStreamSOA : public RayStreamSOA
  {
    __forceinline StackRayStreamSOA(size_t K)
      : RayStreamSOA(data, K) { assert(K <= MAX_K); }

    char data[MAX_K / 4 * sizeof(RayHit4)];
  };


  struct RayStreamSOP
  {
    template<class T>
    __forceinline void init(T& t)
    {
      org_x  = (float*)&t.org.x;
      org_y  = (float*)&t.org.y;
      org_z  = (float*)&t.org.z;
      tnear  = (float*)&t.tnear;
      dir_x  = (float*)&t.dir.x;
      dir_y  = (float*)&t.dir.y;
      dir_z  = (float*)&t.dir.z;
      time   = (float*)&t.time;
      tfar   = (float*)&t.tfar;
      mask   = (unsigned int*)&t.mask;
      id     = (unsigned int*)&t.id;
      flags  = (unsigned int*)&t.flags;

      Ng_x   = (float*)&t.Ng.x;
      Ng_y   = (float*)&t.Ng.y;
      Ng_z   = (float*)&t.Ng.z;
      u      = (float*)&t.u;
      v      = (float*)&t.v;
      primID = (unsigned int*)&t.primID;
      geomID = (unsigned int*)&t.geomID;

      for (unsigned l = 0; l < RTC_MAX_INSTANCE_LEVEL_COUNT; ++l) {
        instID[l] = (unsigned int*)&t.instID[l];
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
        instPrimID[l] = (unsigned int*)&t.instPrimID[l];
#endif
      }
    }

    __forceinline Ray getRayByOffset(size_t offset)
    {
      Ray ray;
      ray.org.x   = *(float* __restrict__)((char*)org_x + offset);
      ray.org.y   = *(float* __restrict__)((char*)org_y + offset);
      ray.org.z   = *(float* __restrict__)((char*)org_z + offset);
      ray.dir.x   = *(float* __restrict__)((char*)dir_x + offset);
      ray.dir.y   = *(float* __restrict__)((char*)dir_y + offset);
      ray.dir.z   = *(float* __restrict__)((char*)dir_z + offset);
      ray.tfar  = *(float* __restrict__)((char*)tfar + offset);
      ray.tnear() = tnear ? *(float* __restrict__)((char*)tnear + offset) : 0.0f;
      ray.time()  = time ? *(float* __restrict__)((char*)time + offset) : 0.0f;
      ray.mask    = mask ? *(unsigned int* __restrict__)((char*)mask + offset) : -1;
      ray.id      = id ? *(unsigned int* __restrict__)((char*)id + offset) : -1;
      ray.flags   = flags ? *(unsigned int* __restrict__)((char*)flags + offset) : -1;
      return ray;
    }

    template<int K>
    __forceinline RayK<K> getRayByOffset(const vbool<K>& valid, size_t offset)
    {
      RayK<K> ray;
      ray.org.x   = vfloat<K>::loadu(valid, (float* __restrict__)((char*)org_x + offset));
      ray.org.y   = vfloat<K>::loadu(valid, (float* __restrict__)((char*)org_y + offset));
      ray.org.z   = vfloat<K>::loadu(valid, (float* __restrict__)((char*)org_z + offset));
      ray.dir.x   = vfloat<K>::loadu(valid, (float* __restrict__)((char*)dir_x + offset));
      ray.dir.y   = vfloat<K>::loadu(valid, (float* __restrict__)((char*)dir_y + offset));
      ray.dir.z   = vfloat<K>::loadu(valid, (float* __restrict__)((char*)dir_z + offset));
      ray.tfar    = vfloat<K>::loadu(valid, (float* __restrict__)((char*)tfar + offset));
      ray.tnear() = tnear ? vfloat<K>::loadu(valid, (float* __restrict__)((char*)tnear + offset)) : 0.0f;
      ray.time()  = time ? vfloat<K>::loadu(valid, (float* __restrict__)((char*)time + offset)) : 0.0f;
      ray.mask    = mask ? vint<K>::loadu(valid, (const void* __restrict__)((char*)mask + offset)) : -1;
      ray.id      = id ? vint<K>::loadu(valid, (const void* __restrict__)((char*)id + offset)) : -1;
      ray.flags   = flags ? vint<K>::loadu(valid, (const void* __restrict__)((char*)flags + offset)) : -1;
      return ray;
    }

    template<int K>
    __forceinline Vec3vf<K> getDirByOffset(const vbool<K>& valid, size_t offset)
    {
      Vec3vf<K> dir;
      dir.x = vfloat<K>::loadu(valid, (float* __restrict__)((char*)dir_x + offset));
      dir.y = vfloat<K>::loadu(valid, (float* __restrict__)((char*)dir_y + offset));
      dir.z = vfloat<K>::loadu(valid, (float* __restrict__)((char*)dir_z + offset));
      return dir;
    }

    __forceinline void setHitByOffset(size_t offset, const RayHit& ray)
    {
      if (ray.geomID != RTC_INVALID_GEOMETRY_ID)
      {
        *(float* __restrict__)((char*)tfar + offset) = ray.tfar;

        if (likely(Ng_x)) *(float* __restrict__)((char*)Ng_x + offset) = ray.Ng.x;
        if (likely(Ng_y)) *(float* __restrict__)((char*)Ng_y + offset) = ray.Ng.y;
        if (likely(Ng_z)) *(float* __restrict__)((char*)Ng_z + offset) = ray.Ng.z;
        *(float* __restrict__)((char*)u + offset) = ray.u;
        *(float* __restrict__)((char*)v + offset) = ray.v;
        *(unsigned int* __restrict__)((char*)geomID + offset) = ray.geomID;
        *(unsigned int* __restrict__)((char*)primID + offset) = ray.primID;

        if (likely(instID[0])) {
          *(unsigned int* __restrict__)((char*)instID[0] + offset) = ray.instID[0];
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
          *(unsigned int* __restrict__)((char*)instPrimID[0] + offset) = ray.instPrimID[0];
#endif
#if (RTC_MAX_INSTANCE_LEVEL_COUNT > 1)
          for (unsigned l = 1; l < RTC_MAX_INSTANCE_LEVEL_COUNT && ray.instID[l-1] != RTC_INVALID_GEOMETRY_ID; ++l) {
            *(unsigned int* __restrict__)((char*)instID[l] + offset) = ray.instID[l];
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
            *(unsigned int* __restrict__)((char*)instPrimID[l] + offset) = ray.instPrimID[l];
#endif
          }
#endif
        }
      }
    }

    __forceinline void setHitByOffset(size_t offset, const Ray& ray)
    {
      *(float* __restrict__)((char*)tfar + offset) = ray.tfar;
    }

    template<int K>
    __forceinline void setHitByOffset(const vbool<K>& valid_i, size_t offset, const RayHitK<K>& ray)
    {
      vbool<K> valid = valid_i;
      valid &= (ray.geomID != RTC_INVALID_GEOMETRY_ID);

      if (likely(any(valid)))
      {
        vfloat<K>::storeu(valid, (float* __restrict__)((char*)tfar + offset), ray.tfar);

        if (likely(Ng_x)) vfloat<K>::storeu(valid, (float* __restrict__)((char*)Ng_x + offset), ray.Ng.x);
        if (likely(Ng_y)) vfloat<K>::storeu(valid, (float* __restrict__)((char*)Ng_y + offset), ray.Ng.y);
        if (likely(Ng_z)) vfloat<K>::storeu(valid, (float* __restrict__)((char*)Ng_z + offset), ray.Ng.z);
        vfloat<K>::storeu(valid, (float* __restrict__)((char*)u + offset), ray.u);
        vfloat<K>::storeu(valid, (float* __restrict__)((char*)v + offset), ray.v);
        vuint<K>::storeu(valid, (unsigned int* __restrict__)((char*)primID + offset), ray.primID);
        vuint<K>::storeu(valid, (unsigned int* __restrict__)((char*)geomID + offset), ray.geomID);

        if (likely(instID[0])) {
          vuint<K>::storeu(valid, (unsigned int* __restrict__)((char*)instID[0] + offset), ray.instID[0]);
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
          vuint<K>::storeu(valid, (unsigned int* __restrict__)((char*)instPrimID[0] + offset), ray.instPrimID[0]);
#endif
#if (RTC_MAX_INSTANCE_LEVEL_COUNT > 1)
          for (unsigned l = 1; l < RTC_MAX_INSTANCE_LEVEL_COUNT && any(valid & (ray.instID[l-1] != RTC_INVALID_GEOMETRY_ID)); ++l) {
            vuint<K>::storeu(valid, (unsigned int* __restrict__)((char*)instID[l] + offset), ray.instID[l]);
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
            vuint<K>::storeu(valid, (unsigned int* __restrict__)((char*)instPrimID[l] + offset), ray.instPrimID[l]);
#endif
          }
#endif
        }
      }
    }

    template<int K>
    __forceinline void setHitByOffset(const vbool<K>& valid_i, size_t offset, const RayK<K>& ray)
    {
      vbool<K> valid = valid_i;
      valid &= (ray.tfar < 0.0f);

      if (likely(any(valid)))
        vfloat<K>::storeu(valid, (float* __restrict__)((char*)tfar + offset), ray.tfar);
    }

    __forceinline size_t getOctantByOffset(size_t offset)
    {
      const float dx = *(float* __restrict__)((char*)dir_x + offset);
      const float dy = *(float* __restrict__)((char*)dir_y + offset);
      const float dz = *(float* __restrict__)((char*)dir_z + offset);
      const size_t octantID = (dx < 0.0f ? 1 : 0) + (dy < 0.0f ? 2 : 0) + (dz < 0.0f ? 4 : 0);
      return octantID;
    }

    __forceinline bool isValidByOffset(size_t offset)
    {
      const float nnear = tnear ? *(float* __restrict__)((char*)tnear + offset) : 0.0f;
      const float ffar  = *(float* __restrict__)((char*)tfar + offset);
      return nnear <= ffar;
    }

    template<int K>
    __forceinline vbool<K> isValidByOffset(const vbool<K>& valid, size_t offset)
    {
      const vfloat<K> nnear = tnear ? vfloat<K>::loadu(valid, (float* __restrict__)((char*)tnear + offset)) : 0.0f;
      const vfloat<K> ffar  = vfloat<K>::loadu(valid, (float* __restrict__)((char*)tfar + offset));
      return nnear <= ffar;
    }

    template<int K>
    __forceinline RayK<K> getRayByOffset(const vbool<K>& valid, const vint<K>& offset)
    {
      RayK<K> ray;

#if defined(__AVX2__)
      ray.org.x   = vfloat<K>::template gather<1>(valid, org_x, offset);
      ray.org.y   = vfloat<K>::template gather<1>(valid, org_y, offset);
      ray.org.z   = vfloat<K>::template gather<1>(valid, org_z, offset);
      ray.dir.x   = vfloat<K>::template gather<1>(valid, dir_x, offset);
      ray.dir.y   = vfloat<K>::template gather<1>(valid, dir_y, offset);
      ray.dir.z   = vfloat<K>::template gather<1>(valid, dir_z, offset);
      ray.tfar    = vfloat<K>::template gather<1>(valid, tfar, offset);
      ray.tnear() = tnear ? vfloat<K>::template gather<1>(valid, tnear, offset) : vfloat<K>(zero);
      ray.time()  = time ? vfloat<K>::template gather<1>(valid, time, offset) : vfloat<K>(zero);
      ray.mask    = mask ? vint<K>::template gather<1>(valid, (int*)mask, offset) : vint<K>(-1);
      ray.id      = id ? vint<K>::template gather<1>(valid, (int*)id, offset) : vint<K>(-1);
      ray.flags   = flags ? vint<K>::template gather<1>(valid, (int*)flags, offset) : vint<K>(-1);
#else
      ray.org     = zero;
      ray.tnear() = zero;
      ray.dir     = zero;
      ray.tfar    = zero;
      ray.time()  = zero;
      ray.mask    = zero;
      ray.id      = zero;
      ray.flags   = zero;

      for (size_t k = 0; k < K; k++)
      {
        if (likely(valid[k]))
        {
          const size_t ofs = offset[k];

          ray.org.x[k]   = *(float* __restrict__)((char*)org_x + ofs);
          ray.org.y[k]   = *(float* __restrict__)((char*)org_y + ofs);
          ray.org.z[k]   = *(float* __restrict__)((char*)org_z + ofs);
          ray.dir.x[k]   = *(float* __restrict__)((char*)dir_x + ofs);
          ray.dir.y[k]   = *(float* __restrict__)((char*)dir_y + ofs);
          ray.dir.z[k]   = *(float* __restrict__)((char*)dir_z + ofs);
          ray.tfar[k]  = *(float* __restrict__)((char*)tfar + ofs);
          ray.tnear()[k] = tnear ? *(float* __restrict__)((char*)tnear + ofs) : 0.0f;
          ray.time()[k]  = time ? *(float* __restrict__)((char*)time + ofs) : 0.0f;
          ray.mask[k]    = mask ? *(int* __restrict__)((char*)mask + ofs) : -1;
          ray.id[k]      = id ? *(int* __restrict__)((char*)id + ofs) : -1;
          ray.flags[k]   = flags ? *(int* __restrict__)((char*)flags + ofs) : -1;
        }
      }
#endif

      return ray;
    }

    template<int K>
    __forceinline void setHitByOffset(const vbool<K>& valid_i, const vint<K>& offset, const RayHitK<K>& ray)
    {
      vbool<K> valid = valid_i;
      valid &= (ray.geomID != RTC_INVALID_GEOMETRY_ID);

      if (likely(any(valid)))
      {
#if defined(__AVX512F__)
        vfloat<K>::template scatter<1>(valid, tfar, offset, ray.tfar);

        if (likely(Ng_x)) vfloat<K>::template scatter<1>(valid, Ng_x, offset, ray.Ng.x);
        if (likely(Ng_y)) vfloat<K>::template scatter<1>(valid, Ng_y, offset, ray.Ng.y);
        if (likely(Ng_z)) vfloat<K>::template scatter<1>(valid, Ng_z, offset, ray.Ng.z);
        vfloat<K>::template scatter<1>(valid, u, offset, ray.u);
        vfloat<K>::template scatter<1>(valid, v, offset, ray.v);
        vuint<K>::template scatter<1>(valid, (unsigned int*)geomID, offset, ray.geomID);
        vuint<K>::template scatter<1>(valid, (unsigned int*)primID, offset, ray.primID);

        if (likely(instID[0])) {
          vuint<K>::template scatter<1>(valid, (unsigned int*)instID[0], offset, ray.instID[0]);
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
          vuint<K>::template scatter<1>(valid, (unsigned int*)instPrimID[0], offset, ray.instPrimID[0]);
#endif
#if (RTC_MAX_INSTANCE_LEVEL_COUNT > 1)
          for (unsigned l = 1; l < RTC_MAX_INSTANCE_LEVEL_COUNT && any(valid & (ray.instID[l-1] != RTC_INVALID_GEOMETRY_ID)); ++l) {
            vuint<K>::template scatter<1>(valid, (unsigned int*)instID[l], offset, ray.instID[l]);
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
            vuint<K>::template scatter<1>(valid, (unsigned int*)instPrimID[l], offset, ray.instPrimID[l]);
#endif
          }
#endif
        }
#else
        size_t valid_bits = movemask(valid);
        while (valid_bits != 0)
        {
          const size_t k = bscf(valid_bits);
          const size_t ofs = offset[k];

          *(float* __restrict__)((char*)tfar + ofs) = ray.tfar[k];

          if (likely(Ng_x)) *(float* __restrict__)((char*)Ng_x + ofs) = ray.Ng.x[k];
          if (likely(Ng_y)) *(float* __restrict__)((char*)Ng_y + ofs) = ray.Ng.y[k];
          if (likely(Ng_z)) *(float* __restrict__)((char*)Ng_z + ofs) = ray.Ng.z[k];
          *(float* __restrict__)((char*)u + ofs) = ray.u[k];
          *(float* __restrict__)((char*)v + ofs) = ray.v[k];
          *(unsigned int* __restrict__)((char*)primID + ofs) = ray.primID[k];
          *(unsigned int* __restrict__)((char*)geomID + ofs) = ray.geomID[k];

          if (likely(instID[0])) {
            *(unsigned int* __restrict__)((char*)instID[0] + ofs) = ray.instID[0][k];
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
            *(unsigned int* __restrict__)((char*)instPrimID[0] + ofs) = ray.instPrimID[0][k];
#endif
#if (RTC_MAX_INSTANCE_LEVEL_COUNT > 1)
            for (unsigned l = 1; l < RTC_MAX_INSTANCE_LEVEL_COUNT && ray.instID[l-1][k] != RTC_INVALID_GEOMETRY_ID; ++l) {
              *(unsigned int* __restrict__)((char*)instID[l] + ofs) = ray.instID[l][k];
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
              *(unsigned int* __restrict__)((char*)instPrimID[l] + ofs) = ray.instPrimID[l][k];
#endif
            }
#endif
          }
        }
#endif
      }
    }

    template<int K>
    __forceinline void setHitByOffset(const vbool<K>& valid_i, const vint<K>& offset, const RayK<K>& ray)
    {
      vbool<K> valid = valid_i;
      valid &= (ray.tfar < 0.0f);

      if (likely(any(valid)))
      {
#if defined(__AVX512F__)
        vfloat<K>::template scatter<1>(valid, tfar, offset, ray.tfar);
#else
        size_t valid_bits = movemask(valid);
        while (valid_bits != 0)
        {
          const size_t k = bscf(valid_bits);
          const size_t ofs = offset[k];

          *(float* __restrict__)((char*)tfar + ofs) = ray.tfar[k];
        }
#endif
      }
    }

    /* ray data */
    float* __restrict__ org_x; // x coordinate of ray origin
    float* __restrict__ org_y; // y coordinate of ray origin
    float* __restrict__ org_z; // z coordinate of ray origin
    float* __restrict__ tnear; // start of ray segment (optional)

    float* __restrict__ dir_x; // x coordinate of ray direction
    float* __restrict__ dir_y; // y coordinate of ray direction
    float* __restrict__ dir_z; // z coordinate of ray direction
    float* __restrict__ time;         // time of this ray for motion blur (optional)

    float* __restrict__ tfar;  // end of ray segment (set to hit distance)
    unsigned int* __restrict__ mask;  // used to mask out objects during traversal (optional)
    unsigned int* __restrict__ id;    // ray ID
    unsigned int* __restrict__ flags; // ray flags

    /* hit data */
    float* __restrict__ Ng_x; // x coordinate of geometry normal (optional)
    float* __restrict__ Ng_y; // y coordinate of geometry normal (optional)
    float* __restrict__ Ng_z; // z coordinate of geometry normal (optional)

    float* __restrict__ u;    // barycentric u coordinate of hit
    float* __restrict__ v;    // barycentric v coordinate of hit

    unsigned int* __restrict__ primID; // primitive ID
    unsigned int* __restrict__ geomID; // geometry ID
    unsigned int* __restrict__ instID[RTC_MAX_INSTANCE_LEVEL_COUNT]; // instance ID
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
    unsigned int* __restrict__ instPrimID[RTC_MAX_INSTANCE_LEVEL_COUNT]; // instance primitive ID (optional)
#endif
  };


  struct RayStreamAOS
  {
    __forceinline RayStreamAOS(void* rays)
      : ptr((Ray*)rays) {}

    __forceinline Ray& getRayByOffset(size_t offset)
    {
      return *(Ray*)((char*)ptr + offset);
    }

    template<int K>
    __forceinline RayK<K> getRayByOffset(const vint<K>& offset);

    template<int K>
    __forceinline RayK<K> getRayByOffset(const vbool<K>& valid, const vint<K>& offset)
    {
      const vint<K> valid_offset = select(valid, offset, vintx(zero));
      return getRayByOffset<K>(valid_offset);
    }

    template<int K>
    __forceinline void setHitByOffset(const vbool<K>& valid_i, const vint<K>& offset, const RayHitK<K>& ray)
    {
      vbool<K> valid = valid_i;
      valid &= (ray.geomID != RTC_INVALID_GEOMETRY_ID);

      if (likely(any(valid)))
      {
#if defined(__AVX512F__)
        vfloat<K>::template scatter<1>(valid, &ptr->tfar, offset, ray.tfar);
        vfloat<K>::template scatter<1>(valid, &((RayHit*)ptr)->Ng.x, offset, ray.Ng.x);
        vfloat<K>::template scatter<1>(valid, &((RayHit*)ptr)->Ng.y, offset, ray.Ng.y);
        vfloat<K>::template scatter<1>(valid, &((RayHit*)ptr)->Ng.z, offset, ray.Ng.z);
        vfloat<K>::template scatter<1>(valid, &((RayHit*)ptr)->u, offset, ray.u);
        vfloat<K>::template scatter<1>(valid, &((RayHit*)ptr)->v, offset, ray.v);
        vuint<K>::template scatter<1>(valid, (unsigned int*)&((RayHit*)ptr)->primID, offset, ray.primID);
        vuint<K>::template scatter<1>(valid, (unsigned int*)&((RayHit*)ptr)->geomID, offset, ray.geomID);

        vuint<K>::template scatter<1>(valid, (unsigned int*)&((RayHit*)ptr)->instID[0], offset, ray.instID[0]);
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
        vuint<K>::template scatter<1>(valid, (unsigned int*)&((RayHit*)ptr)->instPrimID[0], offset, ray.instPrimID[0]);
#endif
#if (RTC_MAX_INSTANCE_LEVEL_COUNT > 1)
        for (unsigned l = 1; l < RTC_MAX_INSTANCE_LEVEL_COUNT && any(valid & (ray.instID[l-1] != RTC_INVALID_GEOMETRY_ID)); ++l) {
          vuint<K>::template scatter<1>(valid, (unsigned int*)&((RayHit*)ptr)->instID[l], offset, ray.instID[l]);
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
          vuint<K>::template scatter<1>(valid, (unsigned int*)&((RayHit*)ptr)->instPrimID[l], offset, ray.instPrimID[l]);
#endif
        }
#endif
#else
        size_t valid_bits = movemask(valid);
        while (valid_bits != 0)
        {
          const size_t k = bscf(valid_bits);
          RayHit* __restrict__ ray_k = (RayHit*)((char*)ptr + offset[k]);
          ray_k->tfar   = ray.tfar[k];
          ray_k->Ng.x   = ray.Ng.x[k];
          ray_k->Ng.y   = ray.Ng.y[k];
          ray_k->Ng.z   = ray.Ng.z[k];
          ray_k->u      = ray.u[k];
          ray_k->v      = ray.v[k];
          ray_k->primID = ray.primID[k];
          ray_k->geomID = ray.geomID[k];

          instance_id_stack::copy_VU<K>(ray.instID, ray_k->instID, k);
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
          instance_id_stack::copy_VU<K>(ray.instPrimID, ray_k->instPrimID, k);
#endif
        }
#endif
      }
    }

    template<int K>
    __forceinline void setHitByOffset(const vbool<K>& valid_i, const vint<K>& offset, const RayK<K>& ray)
    {
      vbool<K> valid = valid_i;
      valid &= (ray.tfar < 0.0f);

      if (likely(any(valid)))
      {
#if defined(__AVX512F__)
        vfloat<K>::template scatter<1>(valid, &ptr->tfar, offset, ray.tfar);
#else
        size_t valid_bits = movemask(valid);
        while (valid_bits != 0)
        {
          const size_t k = bscf(valid_bits);
          Ray* __restrict__ ray_k = (Ray*)((char*)ptr + offset[k]);
          ray_k->tfar = ray.tfar[k];
        }
#endif
      }
    }

    Ray* __restrict__ ptr;
  };
  
  template<>
  __forceinline Ray4 RayStreamAOS::getRayByOffset<4>(const vint4& offset)
  {
    Ray4 ray;

    /* load and transpose: org.x, org.y, org.z, tnear */
    const vfloat4 a0 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[0]))->org);
    const vfloat4 a1 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[1]))->org);
    const vfloat4 a2 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[2]))->org);
    const vfloat4 a3 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[3]))->org);

    transpose(a0,a1,a2,a3, ray.org.x, ray.org.y, ray.org.z, ray.tnear());

    /* load and transpose: dir.x, dir.y, dir.z, time */
    const vfloat4 b0 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[0]))->dir);
    const vfloat4 b1 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[1]))->dir);
    const vfloat4 b2 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[2]))->dir);
    const vfloat4 b3 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[3]))->dir);

    transpose(b0,b1,b2,b3, ray.dir.x, ray.dir.y, ray.dir.z, ray.time());

    /* load and transpose: tfar, mask, id, flags */
    const vfloat4 c0 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[0]))->tfar);
    const vfloat4 c1 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[1]))->tfar);
    const vfloat4 c2 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[2]))->tfar);
    const vfloat4 c3 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[3]))->tfar);

    vfloat4 maskf, idf, flagsf;
    transpose(c0,c1,c2,c3, ray.tfar, maskf, idf, flagsf);
    ray.mask  = asInt(maskf);
    ray.id    = asInt(idf);
    ray.flags = asInt(flagsf);

    return ray;
  }
  
#if defined(__AVX__)
  template<>
  __forceinline Ray8 RayStreamAOS::getRayByOffset<8>(const vint8& offset)
  {
    Ray8 ray;

    /* load and transpose: org.x, org.y, org.z, tnear, dir.x, dir.y, dir.z, time */
    const vfloat8 ab0 = vfloat8::loadu(&((Ray*)((char*)ptr + offset[0]))->org);
    const vfloat8 ab1 = vfloat8::loadu(&((Ray*)((char*)ptr + offset[1]))->org);
    const vfloat8 ab2 = vfloat8::loadu(&((Ray*)((char*)ptr + offset[2]))->org);
    const vfloat8 ab3 = vfloat8::loadu(&((Ray*)((char*)ptr + offset[3]))->org);
    const vfloat8 ab4 = vfloat8::loadu(&((Ray*)((char*)ptr + offset[4]))->org);
    const vfloat8 ab5 = vfloat8::loadu(&((Ray*)((char*)ptr + offset[5]))->org);
    const vfloat8 ab6 = vfloat8::loadu(&((Ray*)((char*)ptr + offset[6]))->org);
    const vfloat8 ab7 = vfloat8::loadu(&((Ray*)((char*)ptr + offset[7]))->org);

    transpose(ab0,ab1,ab2,ab3,ab4,ab5,ab6,ab7, ray.org.x, ray.org.y, ray.org.z, ray.tnear(), ray.dir.x, ray.dir.y, ray.dir.z, ray.time());

    /* load and transpose: tfar, mask, id, flags */
    const vfloat4 c0 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[0]))->tfar);
    const vfloat4 c1 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[1]))->tfar);
    const vfloat4 c2 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[2]))->tfar);
    const vfloat4 c3 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[3]))->tfar);
    const vfloat4 c4 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[4]))->tfar);
    const vfloat4 c5 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[5]))->tfar);
    const vfloat4 c6 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[6]))->tfar);
    const vfloat4 c7 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[7]))->tfar);

    vfloat8 maskf, idf, flagsf;
    transpose(c0,c1,c2,c3,c4,c5,c6,c7, ray.tfar, maskf, idf, flagsf);
    ray.mask  = asInt(maskf);
    ray.id    = asInt(idf);
    ray.flags = asInt(flagsf);

    return ray;
  }
#endif

#if defined(__AVX512F__)
  template<>
  __forceinline Ray16 RayStreamAOS::getRayByOffset<16>(const vint16& offset)
  {
    Ray16 ray;

    /* load and transpose: org.x, org.y, org.z, tnear, dir.x, dir.y, dir.z, time */
    const vfloat8 ab0  = vfloat8::loadu(&((Ray*)((char*)ptr + offset[ 0]))->org);
    const vfloat8 ab1  = vfloat8::loadu(&((Ray*)((char*)ptr + offset[ 1]))->org);
    const vfloat8 ab2  = vfloat8::loadu(&((Ray*)((char*)ptr + offset[ 2]))->org);
    const vfloat8 ab3  = vfloat8::loadu(&((Ray*)((char*)ptr + offset[ 3]))->org);
    const vfloat8 ab4  = vfloat8::loadu(&((Ray*)((char*)ptr + offset[ 4]))->org);
    const vfloat8 ab5  = vfloat8::loadu(&((Ray*)((char*)ptr + offset[ 5]))->org);
    const vfloat8 ab6  = vfloat8::loadu(&((Ray*)((char*)ptr + offset[ 6]))->org);
    const vfloat8 ab7  = vfloat8::loadu(&((Ray*)((char*)ptr + offset[ 7]))->org);
    const vfloat8 ab8  = vfloat8::loadu(&((Ray*)((char*)ptr + offset[ 8]))->org);
    const vfloat8 ab9  = vfloat8::loadu(&((Ray*)((char*)ptr + offset[ 9]))->org);
    const vfloat8 ab10 = vfloat8::loadu(&((Ray*)((char*)ptr + offset[10]))->org);
    const vfloat8 ab11 = vfloat8::loadu(&((Ray*)((char*)ptr + offset[11]))->org);
    const vfloat8 ab12 = vfloat8::loadu(&((Ray*)((char*)ptr + offset[12]))->org);
    const vfloat8 ab13 = vfloat8::loadu(&((Ray*)((char*)ptr + offset[13]))->org);
    const vfloat8 ab14 = vfloat8::loadu(&((Ray*)((char*)ptr + offset[14]))->org);
    const vfloat8 ab15 = vfloat8::loadu(&((Ray*)((char*)ptr + offset[15]))->org);

    transpose(ab0,ab1,ab2,ab3,ab4,ab5,ab6,ab7,ab8,ab9,ab10,ab11,ab12,ab13,ab14,ab15,
              ray.org.x, ray.org.y, ray.org.z, ray.tnear(), ray.dir.x, ray.dir.y, ray.dir.z, ray.time());

    /* load and transpose: tfar, mask, id, flags */
    const vfloat4 c0  = vfloat4::loadu(&((Ray*)((char*)ptr + offset[ 0]))->tfar);
    const vfloat4 c1  = vfloat4::loadu(&((Ray*)((char*)ptr + offset[ 1]))->tfar);
    const vfloat4 c2  = vfloat4::loadu(&((Ray*)((char*)ptr + offset[ 2]))->tfar);
    const vfloat4 c3  = vfloat4::loadu(&((Ray*)((char*)ptr + offset[ 3]))->tfar);
    const vfloat4 c4  = vfloat4::loadu(&((Ray*)((char*)ptr + offset[ 4]))->tfar);
    const vfloat4 c5  = vfloat4::loadu(&((Ray*)((char*)ptr + offset[ 5]))->tfar);
    const vfloat4 c6  = vfloat4::loadu(&((Ray*)((char*)ptr + offset[ 6]))->tfar);
    const vfloat4 c7  = vfloat4::loadu(&((Ray*)((char*)ptr + offset[ 7]))->tfar);
    const vfloat4 c8  = vfloat4::loadu(&((Ray*)((char*)ptr + offset[ 8]))->tfar);
    const vfloat4 c9  = vfloat4::loadu(&((Ray*)((char*)ptr + offset[ 9]))->tfar);
    const vfloat4 c10 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[10]))->tfar);
    const vfloat4 c11 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[11]))->tfar);
    const vfloat4 c12 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[12]))->tfar);
    const vfloat4 c13 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[13]))->tfar);
    const vfloat4 c14 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[14]))->tfar);
    const vfloat4 c15 = vfloat4::loadu(&((Ray*)((char*)ptr + offset[15]))->tfar);

    vfloat16 maskf, idf, flagsf;
    transpose(c0,c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13,c14,c15,
              ray.tfar, maskf, idf, flagsf);
    ray.mask  = asInt(maskf);
    ray.id    = asInt(idf);
    ray.flags = asInt(flagsf);

    return ray;
  }
#endif


  struct RayStreamAOP
  {
    __forceinline RayStreamAOP(void* rays)
      : ptr((Ray**)rays) {}

    __forceinline Ray& getRayByIndex(size_t index)
    {
      return *ptr[index];
    }

    template<int K>
    __forceinline RayK<K> getRayByIndex(const vint<K>& index);

    template<int K>
    __forceinline RayK<K> getRayByIndex(const vbool<K>& valid, const vint<K>& index)
    {
      const vint<K> valid_index = select(valid, index, vintx(zero));
      return getRayByIndex<K>(valid_index);
    }

    template<int K>
    __forceinline void setHitByIndex(const vbool<K>& valid_i, const vint<K>& index, const RayHitK<K>& ray)
    {
      vbool<K> valid = valid_i;
      valid &= (ray.geomID != RTC_INVALID_GEOMETRY_ID);

      if (likely(any(valid)))
      {
        size_t valid_bits = movemask(valid);
        while (valid_bits != 0)
        {
          const size_t k = bscf(valid_bits);
          RayHit* __restrict__ ray_k = (RayHit*)ptr[index[k]];

          ray_k->tfar = ray.tfar[k];
          ray_k->Ng.x   = ray.Ng.x[k];
          ray_k->Ng.y   = ray.Ng.y[k];
          ray_k->Ng.z   = ray.Ng.z[k];
          ray_k->u      = ray.u[k];
          ray_k->v      = ray.v[k];
          ray_k->primID = ray.primID[k];
          ray_k->geomID = ray.geomID[k];
          instance_id_stack::copy_VU<K>(ray.instID, ray_k->instID, k);
#if defined(RTC_GEOMETRY_INSTANCE_ARRAY)
          instance_id_stack::copy_VU<K>(ray.instPrimID, ray_k->instPrimID, k);
#endif
        }
      }
    }

    template<int K>
    __forceinline void setHitByIndex(const vbool<K>& valid_i, const vint<K>& index, const RayK<K>& ray)
    {
      vbool<K> valid = valid_i;
      valid &= (ray.tfar < 0.0f);

      if (likely(any(valid)))
      {
        size_t valid_bits = movemask(valid);
        while (valid_bits != 0)
        {
          const size_t k = bscf(valid_bits);
          Ray* __restrict__ ray_k = ptr[index[k]];

          ray_k->tfar = ray.tfar[k];
        }
      }
    }

    Ray** __restrict__ ptr;
  };
  
  template<>
  __forceinline Ray4 RayStreamAOP::getRayByIndex<4>(const vint4& index)
  {
    Ray4 ray;

    /* load and transpose: org.x, org.y, org.z, tnear */
    const vfloat4 a0 = vfloat4::loadu(&ptr[index[0]]->org);
    const vfloat4 a1 = vfloat4::loadu(&ptr[index[1]]->org);
    const vfloat4 a2 = vfloat4::loadu(&ptr[index[2]]->org);
    const vfloat4 a3 = vfloat4::loadu(&ptr[index[3]]->org);

    transpose(a0,a1,a2,a3, ray.org.x, ray.org.y, ray.org.z, ray.tnear());

    /* load and transpose: dir.x, dir.y, dir.z, time */
    const vfloat4 b0 = vfloat4::loadu(&ptr[index[0]]->dir);
    const vfloat4 b1 = vfloat4::loadu(&ptr[index[1]]->dir);
    const vfloat4 b2 = vfloat4::loadu(&ptr[index[2]]->dir);
    const vfloat4 b3 = vfloat4::loadu(&ptr[index[3]]->dir);

    transpose(b0,b1,b2,b3, ray.dir.x, ray.dir.y, ray.dir.z, ray.time());

    /* load and transpose: tfar, mask, id, flags */
    const vfloat4 c0 = vfloat4::loadu(&ptr[index[0]]->tfar);
    const vfloat4 c1 = vfloat4::loadu(&ptr[index[1]]->tfar);
    const vfloat4 c2 = vfloat4::loadu(&ptr[index[2]]->tfar);
    const vfloat4 c3 = vfloat4::loadu(&ptr[index[3]]->tfar);

    vfloat4 maskf, idf, flagsf;
    transpose(c0,c1,c2,c3, ray.tfar, maskf, idf, flagsf);
    ray.mask  = asInt(maskf);
    ray.id    = asInt(idf);
    ray.flags = asInt(flagsf);

    return ray;
  }
  
#if defined(__AVX__)
  template<>
  __forceinline Ray8 RayStreamAOP::getRayByIndex<8>(const vint8& index)
  {
    Ray8 ray;

    /* load and transpose: org.x, org.y, org.z, tnear, dir.x, dir.y, dir.z, time */
    const vfloat8 ab0 = vfloat8::loadu(&ptr[index[0]]->org);
    const vfloat8 ab1 = vfloat8::loadu(&ptr[index[1]]->org);
    const vfloat8 ab2 = vfloat8::loadu(&ptr[index[2]]->org);
    const vfloat8 ab3 = vfloat8::loadu(&ptr[index[3]]->org);
    const vfloat8 ab4 = vfloat8::loadu(&ptr[index[4]]->org);
    const vfloat8 ab5 = vfloat8::loadu(&ptr[index[5]]->org);
    const vfloat8 ab6 = vfloat8::loadu(&ptr[index[6]]->org);
    const vfloat8 ab7 = vfloat8::loadu(&ptr[index[7]]->org);

    transpose(ab0,ab1,ab2,ab3,ab4,ab5,ab6,ab7, ray.org.x, ray.org.y, ray.org.z, ray.tnear(), ray.dir.x, ray.dir.y, ray.dir.z, ray.time());

    /* load and transpose: tfar, mask, id, flags */
    const vfloat4 c0 = vfloat4::loadu(&ptr[index[0]]->tfar);
    const vfloat4 c1 = vfloat4::loadu(&ptr[index[1]]->tfar);
    const vfloat4 c2 = vfloat4::loadu(&ptr[index[2]]->tfar);
    const vfloat4 c3 = vfloat4::loadu(&ptr[index[3]]->tfar);
    const vfloat4 c4 = vfloat4::loadu(&ptr[index[4]]->tfar);
    const vfloat4 c5 = vfloat4::loadu(&ptr[index[5]]->tfar);
    const vfloat4 c6 = vfloat4::loadu(&ptr[index[6]]->tfar);
    const vfloat4 c7 = vfloat4::loadu(&ptr[index[7]]->tfar);

    vfloat8 maskf, idf, flagsf;
    transpose(c0,c1,c2,c3,c4,c5,c6,c7, ray.tfar, maskf, idf, flagsf);
    ray.mask  = asInt(maskf);
    ray.id    = asInt(idf);
    ray.flags = asInt(flagsf);

    return ray;
  }
#endif

#if defined(__AVX512F__)
  template<>
  __forceinline Ray16 RayStreamAOP::getRayByIndex<16>(const vint16& index)
  {
    Ray16 ray;

    /* load and transpose: org.x, org.y, org.z, tnear, dir.x, dir.y, dir.z, time */
    const vfloat8 ab0  = vfloat8::loadu(&ptr[index[0]]->org);
    const vfloat8 ab1  = vfloat8::loadu(&ptr[index[1]]->org);
    const vfloat8 ab2  = vfloat8::loadu(&ptr[index[2]]->org);
    const vfloat8 ab3  = vfloat8::loadu(&ptr[index[3]]->org);
    const vfloat8 ab4  = vfloat8::loadu(&ptr[index[4]]->org);
    const vfloat8 ab5  = vfloat8::loadu(&ptr[index[5]]->org);
    const vfloat8 ab6  = vfloat8::loadu(&ptr[index[6]]->org);
    const vfloat8 ab7  = vfloat8::loadu(&ptr[index[7]]->org);
    const vfloat8 ab8  = vfloat8::loadu(&ptr[index[8]]->org);
    const vfloat8 ab9  = vfloat8::loadu(&ptr[index[9]]->org);
    const vfloat8 ab10 = vfloat8::loadu(&ptr[index[10]]->org);
    const vfloat8 ab11 = vfloat8::loadu(&ptr[index[11]]->org);
    const vfloat8 ab12 = vfloat8::loadu(&ptr[index[12]]->org);
    const vfloat8 ab13 = vfloat8::loadu(&ptr[index[13]]->org);
    const vfloat8 ab14 = vfloat8::loadu(&ptr[index[14]]->org);
    const vfloat8 ab15 = vfloat8::loadu(&ptr[index[15]]->org);

    transpose(ab0,ab1,ab2,ab3,ab4,ab5,ab6,ab7,ab8,ab9,ab10,ab11,ab12,ab13,ab14,ab15,
              ray.org.x, ray.org.y, ray.org.z, ray.tnear(), ray.dir.x, ray.dir.y, ray.dir.z, ray.time());

    /* load and transpose: tfar, mask, id, flags */
    const vfloat4 c0  = vfloat4::loadu(&ptr[index[0]]->tfar);
    const vfloat4 c1  = vfloat4::loadu(&ptr[index[1]]->tfar);
    const vfloat4 c2  = vfloat4::loadu(&ptr[index[2]]->tfar);
    const vfloat4 c3  = vfloat4::loadu(&ptr[index[3]]->tfar);
    const vfloat4 c4  = vfloat4::loadu(&ptr[index[4]]->tfar);
    const vfloat4 c5  = vfloat4::loadu(&ptr[index[5]]->tfar);
    const vfloat4 c6  = vfloat4::loadu(&ptr[index[6]]->tfar);
    const vfloat4 c7  = vfloat4::loadu(&ptr[index[7]]->tfar);
    const vfloat4 c8  = vfloat4::loadu(&ptr[index[8]]->tfar);
    const vfloat4 c9  = vfloat4::loadu(&ptr[index[9]]->tfar);
    const vfloat4 c10 = vfloat4::loadu(&ptr[index[10]]->tfar);
    const vfloat4 c11 = vfloat4::loadu(&ptr[index[11]]->tfar);
    const vfloat4 c12 = vfloat4::loadu(&ptr[index[12]]->tfar);
    const vfloat4 c13 = vfloat4::loadu(&ptr[index[13]]->tfar);
    const vfloat4 c14 = vfloat4::loadu(&ptr[index[14]]->tfar);
    const vfloat4 c15 = vfloat4::loadu(&ptr[index[15]]->tfar);

    vfloat16 maskf, idf, flagsf;
    transpose(c0,c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13,c14,c15,
              ray.tfar, maskf, idf, flagsf);

    ray.mask  = asInt(maskf);
    ray.id    = asInt(idf);
    ray.flags = asInt(flagsf);

    return ray;
  }
#endif
}
