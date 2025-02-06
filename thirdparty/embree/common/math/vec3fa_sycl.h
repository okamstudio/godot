// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "../sys/alloc.h"
#include "emath.h"
#include "../simd/sse.h"

namespace embree
{
  ////////////////////////////////////////////////////////////////////////////////
  /// SSE Vec3fa Type
  ////////////////////////////////////////////////////////////////////////////////

  struct __aligned(16) Vec3fa
  {
    //ALIGNED_STRUCT_(16);

    typedef float Scalar;
    enum { N = 3 };
    struct { float x,y,z, do_not_use; };

    ////////////////////////////////////////////////////////////////////////////////
    /// Constructors, Assignment & Cast Operators
    ////////////////////////////////////////////////////////////////////////////////

    __forceinline Vec3fa( ) {}
    //__forceinline Vec3fa( const __m128 a ) : m128(a) {}
    //__forceinline explicit Vec3fa(const vfloat4& a) : x(a[0]), y(a[1]), z(a[2]) {}

    __forceinline Vec3fa            ( const Vec3<float>& other  ) { x = other.x; y = other.y; z = other.z; }
    //__forceinline Vec3fa& operator =( const Vec3<float>& other ) { x = other.x; y = other.y; z = other.z; return *this; }

    __forceinline Vec3fa            ( const Vec3fa& other ) { x = other.x; y = other.y; z = other.z; }
    __forceinline Vec3fa& operator =( const Vec3fa& other ) { x = other.x; y = other.y; z = other.z; return *this; }

    __forceinline explicit Vec3fa( const float a ) : x(a), y(a), z(a) {}
    __forceinline          Vec3fa( const float x, const float y, const float z) : x(x), y(y), z(z) {}

    __forceinline explicit Vec3fa( const Vec3ia& a ) : x((float)a.x), y((float)a.y), z((float)a.z) {}

    //__forceinline operator const __m128&() const { return m128; }
    //__forceinline operator       __m128&()       { return m128; }
    __forceinline operator vfloat4() const { return vfloat4(x,y,z,0.0f); } // FIXME: we should not need this!!

    //friend __forceinline Vec3fa copy_a( const Vec3fa& a, const Vec3fa& b ) { Vec3fa c = a; c.a = b.a; return c; }

    ////////////////////////////////////////////////////////////////////////////////
    /// Loads and Stores
    ////////////////////////////////////////////////////////////////////////////////

    static __forceinline Vec3fa load( const void* const a ) {
      const float* ptr = (const float*)a;
      return Vec3fa(ptr[0],ptr[1],ptr[2]);
    }

    static __forceinline Vec3fa loadu( const void* const a ) {
      const float* ptr = (const float*)a;
      return Vec3fa(ptr[0],ptr[1],ptr[2]);
    }

    static __forceinline void storeu ( void* a, const Vec3fa& v ) {
      float* ptr = (float*)a;
      ptr[0] = v.x; ptr[1] = v.y; ptr[2] = v.z;
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// Constants
    ////////////////////////////////////////////////////////////////////////////////

    __forceinline Vec3fa( ZeroTy   ) : x(0.0f), y(0.0f), z(0.0f) {}
    __forceinline Vec3fa( OneTy    ) : x(1.0f), y(1.0f), z(1.0f) {}
    __forceinline Vec3fa( PosInfTy ) : x(+INFINITY), y(+INFINITY), z(+INFINITY) {}
    __forceinline Vec3fa( NegInfTy ) : x(-INFINITY), y(-INFINITY), z(-INFINITY) {}

    ////////////////////////////////////////////////////////////////////////////////
    /// Array Access
    ////////////////////////////////////////////////////////////////////////////////

    __forceinline const float& operator []( const size_t index ) const { assert(index < 3); return (&x)[index]; }
    __forceinline       float& operator []( const size_t index )       { assert(index < 3); return (&x)[index]; }
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// Unary Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline Vec3fa operator +( const Vec3fa& a ) { return a; }
  __forceinline Vec3fa operator -( const Vec3fa& a ) { return Vec3fa(-a.x,-a.y,-a.z); }
  __forceinline Vec3fa abs  ( const Vec3fa& a ) { return Vec3fa(sycl::fabs(a.x),sycl::fabs(a.y),sycl::fabs(a.z)); }
  __forceinline Vec3fa sign ( const Vec3fa& a ) { return Vec3fa(sycl::sign(a.x),sycl::sign(a.y),sycl::sign(a.z)); }

  //__forceinline Vec3fa rcp  ( const Vec3fa& a ) { return Vec3fa(sycl::recip(a.x),sycl::recip(a.y),sycl::recip(a.z)); }
  __forceinline Vec3fa rcp  ( const Vec3fa& a ) { return Vec3fa(sycl::native::recip(a.x),sycl::native::recip(a.y),sycl::native::recip(a.z)); }
  __forceinline Vec3fa sqrt ( const Vec3fa& a ) { return Vec3fa(sycl::sqrt(a.x),sycl::sqrt(a.y),sycl::sqrt(a.z)); }
  __forceinline Vec3fa sqr  ( const Vec3fa& a ) { return Vec3fa(a.x*a.x,a.y*a.y,a.z*a.z); }

  __forceinline Vec3fa rsqrt( const Vec3fa& a ) { return Vec3fa(sycl::rsqrt(a.x),sycl::rsqrt(a.y),sycl::rsqrt(a.z)); }

  __forceinline Vec3fa zero_fix(const Vec3fa& a) {
    const float x = sycl::fabs(a.x) < min_rcp_input ? min_rcp_input : a.x;
    const float y = sycl::fabs(a.y) < min_rcp_input ? min_rcp_input : a.y;
    const float z = sycl::fabs(a.z) < min_rcp_input ? min_rcp_input : a.z;
    return Vec3fa(x,y,z);
  }
  __forceinline Vec3fa rcp_safe(const Vec3fa& a) {
    return rcp(zero_fix(a));
  }
  __forceinline Vec3fa log ( const Vec3fa& a ) {
    return Vec3fa(sycl::log(a.x),sycl::log(a.y),sycl::log(a.z));
  }

  __forceinline Vec3fa exp ( const Vec3fa& a ) {
    return Vec3fa(sycl::exp(a.x),sycl::exp(a.y),sycl::exp(a.z));
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// Binary Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline Vec3fa operator +( const Vec3fa& a, const Vec3fa& b ) { return Vec3fa(a.x+b.x, a.y+b.y, a.z+b.z); }
  __forceinline Vec3fa operator -( const Vec3fa& a, const Vec3fa& b ) { return Vec3fa(a.x-b.x, a.y-b.y, a.z-b.z); }
  __forceinline Vec3fa operator *( const Vec3fa& a, const Vec3fa& b ) { return Vec3fa(a.x*b.x, a.y*b.y, a.z*b.z); }
  __forceinline Vec3fa operator *( const Vec3fa& a, const float b ) { return a * Vec3fa(b); }
  __forceinline Vec3fa operator *( const float a, const Vec3fa& b ) { return Vec3fa(a) * b; }
  __forceinline Vec3fa operator /( const Vec3fa& a, const Vec3fa& b ) { return Vec3fa(a.x/b.x, a.y/b.y, a.z/b.z); }
  __forceinline Vec3fa operator /( const Vec3fa& a, const float b        ) { return Vec3fa(a.x/b, a.y/b, a.z/b); }
  __forceinline Vec3fa operator /( const        float a, const Vec3fa& b ) { return Vec3fa(a/b.x, a/b.y, a/b.z); }

  __forceinline Vec3fa min( const Vec3fa& a, const Vec3fa& b ) {
    return Vec3fa(sycl::fmin(a.x,b.x), sycl::fmin(a.y,b.y), sycl::fmin(a.z,b.z));
  }
  __forceinline Vec3fa max( const Vec3fa& a, const Vec3fa& b ) {
    return Vec3fa(sycl::fmax(a.x,b.x), sycl::fmax(a.y,b.y), sycl::fmax(a.z,b.z));
  }

/*
#if defined(__SSE4_1__)
    __forceinline Vec3fa mini(const Vec3fa& a, const Vec3fa& b) {
      const vint4 ai = _mm_castps_si128(a);
      const vint4 bi = _mm_castps_si128(b);
      const vint4 ci = _mm_min_epi32(ai,bi);
      return _mm_castsi128_ps(ci);
    }
#endif

#if defined(__SSE4_1__)
    __forceinline Vec3fa maxi(const Vec3fa& a, const Vec3fa& b) {
      const vint4 ai = _mm_castps_si128(a);
      const vint4 bi = _mm_castps_si128(b);
      const vint4 ci = _mm_max_epi32(ai,bi);
      return _mm_castsi128_ps(ci);
    }
#endif
*/
  __forceinline Vec3fa pow ( const Vec3fa& a, const float& b ) {
    return Vec3fa(powf(a.x,b),powf(a.y,b),powf(a.z,b));
  }
  
  ////////////////////////////////////////////////////////////////////////////////
  /// Ternary Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline Vec3fa madd  ( const Vec3fa& a, const Vec3fa& b, const Vec3fa& c) { return Vec3fa(madd(a.x,b.x,c.x), madd(a.y,b.y,c.y), madd(a.z,b.z,c.z)); }
  __forceinline Vec3fa msub  ( const Vec3fa& a, const Vec3fa& b, const Vec3fa& c) { return Vec3fa(msub(a.x,b.x,c.x), msub(a.y,b.y,c.y), msub(a.z,b.z,c.z)); }
  __forceinline Vec3fa nmadd ( const Vec3fa& a, const Vec3fa& b, const Vec3fa& c) { return Vec3fa(nmadd(a.x,b.x,c.x), nmadd(a.y,b.y,c.y), nmadd(a.z,b.z,c.z)); }
  __forceinline Vec3fa nmsub ( const Vec3fa& a, const Vec3fa& b, const Vec3fa& c) { return Vec3fa(nmsub(a.x,b.x,c.x), nmsub(a.y,b.y,c.y), nmsub(a.z,b.z,c.z)); }

  __forceinline Vec3fa madd  ( const float a, const Vec3fa& b, const Vec3fa& c) { return madd(Vec3fa(a),b,c); }
  __forceinline Vec3fa msub  ( const float a, const Vec3fa& b, const Vec3fa& c) { return msub(Vec3fa(a),b,c); }
  __forceinline Vec3fa nmadd ( const float a, const Vec3fa& b, const Vec3fa& c) { return nmadd(Vec3fa(a),b,c); }
  __forceinline Vec3fa nmsub ( const float a, const Vec3fa& b, const Vec3fa& c) { return nmsub(Vec3fa(a),b,c); }

  ////////////////////////////////////////////////////////////////////////////////
  /// Assignment Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline Vec3fa& operator +=( Vec3fa& a, const Vec3fa& b ) { return a = a + b; }
  __forceinline Vec3fa& operator -=( Vec3fa& a, const Vec3fa& b ) { return a = a - b; }
  __forceinline Vec3fa& operator *=( Vec3fa& a, const Vec3fa& b ) { return a = a * b; }
  __forceinline Vec3fa& operator *=( Vec3fa& a, const float   b ) { return a = a * b; }
  __forceinline Vec3fa& operator /=( Vec3fa& a, const Vec3fa& b ) { return a = a / b; }
  __forceinline Vec3fa& operator /=( Vec3fa& a, const float   b ) { return a = a / b; }

  ////////////////////////////////////////////////////////////////////////////////
  /// Reductions
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline float reduce_add(const Vec3fa& v) { return v.x+v.y+v.z; }
  __forceinline float reduce_mul(const Vec3fa& v) { return v.x*v.y*v.z; }
  __forceinline float reduce_min(const Vec3fa& v) { return sycl::fmin(sycl::fmin(v.x,v.y),v.z); }
  __forceinline float reduce_max(const Vec3fa& v) { return sycl::fmax(sycl::fmax(v.x,v.y),v.z); }

  ////////////////////////////////////////////////////////////////////////////////
  /// Comparison Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline bool operator ==( const Vec3fa& a, const Vec3fa& b ) { return a.x == b.x && a.y == b.y && a.z == b.z; }
  __forceinline bool operator !=( const Vec3fa& a, const Vec3fa& b ) { return a.x != b.x || a.y != b.y || a.z != b.z; }

  __forceinline Vec3ba eq_mask( const Vec3fa& a, const Vec3fa& b ) { return Vec3ba(a.x == b.x, a.y == b.y, a.z == b.z); }
  __forceinline Vec3ba neq_mask(const Vec3fa& a, const Vec3fa& b ) { return Vec3ba(a.x != b.x, a.y != b.y, a.z != b.z); }
  __forceinline Vec3ba lt_mask( const Vec3fa& a, const Vec3fa& b ) { return Vec3ba(a.x <  b.x, a.y <  b.y, a.z <  b.z); }
  __forceinline Vec3ba le_mask( const Vec3fa& a, const Vec3fa& b ) { return Vec3ba(a.x <= b.x, a.y <= b.y, a.z <= b.z); }
  __forceinline Vec3ba gt_mask( const Vec3fa& a, const Vec3fa& b ) { return Vec3ba(a.x >  b.x, a.y >  b.y, a.z >  b.z); }
  __forceinline Vec3ba ge_mask( const Vec3fa& a, const Vec3fa& b ) { return Vec3ba(a.x >= b.x, a.y >= b.y, a.z >= b.z); }

  __forceinline bool isvalid ( const Vec3fa& v ) {
    return all(gt_mask(v,Vec3fa(-FLT_LARGE)) & lt_mask(v,Vec3fa(+FLT_LARGE)));
  }

  __forceinline bool is_finite ( const Vec3fa& a ) {
    return all(ge_mask(a,Vec3fa(-FLT_MAX)) & le_mask(a,Vec3fa(+FLT_MAX)));
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// Euclidian Space Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline float dot ( const Vec3fa& a, const Vec3fa& b ) {
    return reduce_add(a*b);
  }

  __forceinline Vec3fa cross ( const Vec3fa& a, const Vec3fa& b ) {
    return Vec3fa(msub(a.y,b.z,a.z*b.y), msub(a.z,b.x,a.x*b.z), msub(a.x,b.y,a.y*b.x));
  }
  
  __forceinline float  sqr_length ( const Vec3fa& a )                { return dot(a,a); }
  __forceinline float  rcp_length ( const Vec3fa& a )                { return rsqrt(dot(a,a)); }
  __forceinline float  rcp_length2( const Vec3fa& a )                { return rcp(dot(a,a)); }
  __forceinline float  length   ( const Vec3fa& a )                  { return sqrt(dot(a,a)); }
  __forceinline Vec3fa normalize( const Vec3fa& a )                  { return a*rsqrt(dot(a,a)); }
  __forceinline float  distance ( const Vec3fa& a, const Vec3fa& b ) { return length(a-b); }
  __forceinline float  halfArea ( const Vec3fa& d )                  { return madd(d.x,(d.y+d.z),d.y*d.z); }
  __forceinline float  area     ( const Vec3fa& d )                  { return 2.0f*halfArea(d); }

  __forceinline Vec3fa normalize_safe( const Vec3fa& a ) {
    const float d = dot(a,a); if (unlikely(d == 0.0f)) return a; else return a*rsqrt(d);
  }

  /*! differentiated normalization */
  __forceinline Vec3fa dnormalize(const Vec3fa& p, const Vec3fa& dp)
  {
    const float pp  = dot(p,p);
    const float pdp = dot(p,dp);
    return (pp*dp-pdp*p)*rcp(pp)*rsqrt(pp);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// Select
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline Vec3fa select( bool s, const Vec3fa& t, const Vec3fa& f ) {
    return Vec3fa(s ? t.x : f.x, s ? t.y : f.y, s ? t.z : f.z);
  }

  __forceinline Vec3fa select( const Vec3ba& s, const Vec3fa& t, const Vec3fa& f ) {
    return Vec3fa(s.x ? t.x : f.x, s.y ? t.y : f.y, s.z ? t.z : f.z);
  }
  
  __forceinline Vec3fa lerp(const Vec3fa& v0, const Vec3fa& v1, const float t) {
    return madd(1.0f-t,v0,t*v1);
  }

  __forceinline int maxDim ( const Vec3fa& a )
  {
    const Vec3fa b = abs(a);
    if (b.x > b.y) {
      if (b.x > b.z) return 0; else return 2;
    } else {
      if (b.y > b.z) return 1; else return 2;
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// Rounding Functions
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline Vec3fa trunc( const Vec3fa& a ) { return Vec3fa(sycl::trunc(a.x),sycl::trunc(a.y),sycl::trunc(a.z)); }
  __forceinline Vec3fa floor( const Vec3fa& a ) { return Vec3fa(sycl::floor(a.x),sycl::floor(a.y),sycl::floor(a.z)); }
  __forceinline Vec3fa ceil ( const Vec3fa& a ) { return Vec3fa(sycl::ceil (a.x),sycl::ceil (a.y),sycl::ceil (a.z)); }

  ////////////////////////////////////////////////////////////////////////////////
  /// Output Operators
  ////////////////////////////////////////////////////////////////////////////////

  inline embree_ostream operator<<(embree_ostream cout, const Vec3fa& a) {
    return cout << "(" << a.x << ", " << a.y << ", " << a.z << ")";
  }

  __forceinline Vec2fa::Vec2fa(const Vec3fa& a)
    : x(a.x), y(a.y) {}

  __forceinline Vec3ia::Vec3ia( const Vec3fa& a )
    : x((int)a.x), y((int)a.y), z((int)a.z) {}

  typedef Vec3fa Vec3fa_t;



  ////////////////////////////////////////////////////////////////////////////////
  /// SSE Vec3fx Type
  ////////////////////////////////////////////////////////////////////////////////

  struct __aligned(16) Vec3fx
  {
    //ALIGNED_STRUCT_(16);

    typedef float Scalar;
    enum { N = 3 };
    struct { float x,y,z; union { int a; unsigned u; float w; }; };

    ////////////////////////////////////////////////////////////////////////////////
    /// Constructors, Assignment & Cast Operators
    ////////////////////////////////////////////////////////////////////////////////

    __forceinline Vec3fx( ) {}
    //__forceinline Vec3fx( const __m128 a ) : m128(a) {}
    __forceinline explicit Vec3fx(const vfloat4& a) : x(a[0]), y(a[1]), z(a[2]), w(a[3]) {}

    __forceinline explicit Vec3fx(const Vec3fa& v) : x(v.x), y(v.y), z(v.z), w(0.0f) {}
    __forceinline operator Vec3fa() const { return Vec3fa(x,y,z); }
    
    __forceinline explicit Vec3fx ( const Vec3<float>& other  ) { x = other.x; y = other.y; z = other.z; }
    //__forceinline Vec3fx& operator =( const Vec3<float>& other ) { x = other.x; y = other.y; z = other.z; return *this; }

    //__forceinline Vec3fx            ( const Vec3fx& other ) { *(sycl::float4*)this = *(const sycl::float4*)&other; }
    //__forceinline Vec3fx& operator =( const Vec3fx& other ) { *(sycl::float4*)this = *(const sycl::float4*)&other; return *this; }

    __forceinline explicit Vec3fx( const float a ) : x(a), y(a), z(a), w(a) {}
    __forceinline          Vec3fx( const float x, const float y, const float z) : x(x), y(y), z(z), w(z) {}

    __forceinline Vec3fx( const Vec3fa& other, const int      a1) : x(other.x), y(other.y), z(other.z), a(a1) {}
    __forceinline Vec3fx( const Vec3fa& other, const unsigned a1) : x(other.x), y(other.y), z(other.z), u(a1) {}
    __forceinline Vec3fx( const Vec3fa& other, const float    w1) : x(other.x), y(other.y), z(other.z), w(w1) {}

    //__forceinline Vec3fx( const float x, const float y, const float z, const int      a) : x(x), y(y), z(z), a(a) {} // not working properly!
    //__forceinline Vec3fx( const float x, const float y, const float z, const unsigned a) : x(x), y(y), z(z), u(a) {} // not working properly!
    __forceinline Vec3fx( const float x, const float y, const float z, const float w) : x(x), y(y), z(z), w(w) {}

    __forceinline explicit Vec3fx( const Vec3ia& a ) : x((float)a.x), y((float)a.y), z((float)a.z), w(0.0f) {}

    //__forceinline operator const __m128&() const { return m128; }
    //__forceinline operator       __m128&()       { return m128; }
    __forceinline operator vfloat4() const { return vfloat4(x,y,z,w); }

    //friend __forceinline Vec3fx copy_a( const Vec3fx& a, const Vec3fx& b ) { Vec3fx c = a; c.a = b.a; return c; }

    ////////////////////////////////////////////////////////////////////////////////
    /// Loads and Stores
    ////////////////////////////////////////////////////////////////////////////////

    static __forceinline Vec3fx load( const void* const a ) {
      const float* ptr = (const float*)a;
      return Vec3fx(ptr[0],ptr[1],ptr[2],ptr[3]);
    }

    static __forceinline Vec3fx loadu( const void* const a ) {
      const float* ptr = (const float*)a;
      return Vec3fx(ptr[0],ptr[1],ptr[2],ptr[3]);
    }

    static __forceinline void storeu ( void* a, const Vec3fx& v ) {
      float* ptr = (float*)a;
      ptr[0] = v.x; ptr[1] = v.y; ptr[2] = v.z; ptr[3] = v.w;
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// Constants
    ////////////////////////////////////////////////////////////////////////////////

    __forceinline Vec3fx( ZeroTy   ) : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
    __forceinline Vec3fx( OneTy    ) : x(1.0f), y(1.0f), z(1.0f), w(1.0f) {}
    __forceinline Vec3fx( PosInfTy ) : x(+INFINITY), y(+INFINITY), z(+INFINITY), w(+INFINITY) {}
    __forceinline Vec3fx( NegInfTy ) : x(-INFINITY), y(-INFINITY), z(-INFINITY), w(-INFINITY) {}

    ////////////////////////////////////////////////////////////////////////////////
    /// Array Access
    ////////////////////////////////////////////////////////////////////////////////

    __forceinline const float& operator []( const size_t index ) const { assert(index < 3); return (&x)[index]; }
    __forceinline       float& operator []( const size_t index )       { assert(index < 3); return (&x)[index]; }
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// Unary Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline Vec3fx operator +( const Vec3fx& a ) { return a; }
  __forceinline Vec3fx operator -( const Vec3fx& a ) { return Vec3fx(-a.x,-a.y,-a.z,-a.w); }
  __forceinline Vec3fx abs  ( const Vec3fx& a ) { return Vec3fx(sycl::fabs(a.x),sycl::fabs(a.y),sycl::fabs(a.z),sycl::fabs(a.w)); }
  __forceinline Vec3fx sign ( const Vec3fx& a ) { return Vec3fx(sycl::sign(a.x),sycl::sign(a.y),sycl::sign(a.z),sycl::sign(a.z)); }

  //__forceinline Vec3fx rcp  ( const Vec3fx& a ) { return Vec3fx(sycl::recip(a.x),sycl::recip(a.y),sycl::recip(a.z)); }
  __forceinline Vec3fx rcp  ( const Vec3fx& a ) { return Vec3fx(sycl::native::recip(a.x),sycl::native::recip(a.y),sycl::native::recip(a.z),sycl::native::recip(a.w)); }
  __forceinline Vec3fx sqrt ( const Vec3fx& a ) { return Vec3fx(sycl::sqrt(a.x),sycl::sqrt(a.y),sycl::sqrt(a.z),sycl::sqrt(a.w)); }
  __forceinline Vec3fx sqr  ( const Vec3fx& a ) { return Vec3fx(a.x*a.x,a.y*a.y,a.z*a.z,a.w*a.w); }

  __forceinline Vec3fx rsqrt( const Vec3fx& a ) { return Vec3fx(sycl::rsqrt(a.x),sycl::rsqrt(a.y),sycl::rsqrt(a.z),sycl::rsqrt(a.w)); }

  __forceinline Vec3fx zero_fix(const Vec3fx& a) {
    const float x = sycl::fabs(a.x) < min_rcp_input ? min_rcp_input : a.x;
    const float y = sycl::fabs(a.y) < min_rcp_input ? min_rcp_input : a.y;
    const float z = sycl::fabs(a.z) < min_rcp_input ? min_rcp_input : a.z;
    return Vec3fx(x,y,z);
  }
  __forceinline Vec3fx rcp_safe(const Vec3fx& a) {
    return rcp(zero_fix(a));
  }
  __forceinline Vec3fx log ( const Vec3fx& a ) {
    return Vec3fx(sycl::log(a.x),sycl::log(a.y),sycl::log(a.z));
  }

  __forceinline Vec3fx exp ( const Vec3fx& a ) {
    return Vec3fx(sycl::exp(a.x),sycl::exp(a.y),sycl::exp(a.z));
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// Binary Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline Vec3fx operator +( const Vec3fx& a, const Vec3fx& b ) { return Vec3fx(a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w); }
  __forceinline Vec3fx operator -( const Vec3fx& a, const Vec3fx& b ) { return Vec3fx(a.x-b.x, a.y-b.y, a.z-b.z, a.w-b.w); }
  __forceinline Vec3fx operator *( const Vec3fx& a, const Vec3fx& b ) { return Vec3fx(a.x*b.x, a.y*b.y, a.z*b.z, a.w*b.w); }
  __forceinline Vec3fx operator *( const Vec3fx& a, const float b ) { return a * Vec3fx(b); }
  __forceinline Vec3fx operator *( const float a, const Vec3fx& b ) { return Vec3fx(a) * b; }
  __forceinline Vec3fx operator /( const Vec3fx& a, const Vec3fx& b ) { return Vec3fx(a.x/b.x, a.y/b.y, a.z/b.z, a.w/b.w); }
  __forceinline Vec3fx operator /( const Vec3fx& a, const float b        ) { return Vec3fx(a.x/b, a.y/b, a.z/b, a.w/b); }
  __forceinline Vec3fx operator /( const        float a, const Vec3fx& b ) { return Vec3fx(a/b.x, a/b.y, a/b.z, a/b.w); }

  __forceinline Vec3fx min( const Vec3fx& a, const Vec3fx& b ) {
    return Vec3fx(sycl::fmin(a.x,b.x), sycl::fmin(a.y,b.y), sycl::fmin(a.z,b.z), sycl::fmin(a.w,b.w));
  }
  __forceinline Vec3fx max( const Vec3fx& a, const Vec3fx& b ) {
    return Vec3fx(sycl::fmax(a.x,b.x), sycl::fmax(a.y,b.y), sycl::fmax(a.z,b.z), sycl::fmax(a.w,b.w));
  }

/*
#if defined(__SSE4_1__)
    __forceinline Vec3fx mini(const Vec3fx& a, const Vec3fx& b) {
      const vint4 ai = _mm_castps_si128(a);
      const vint4 bi = _mm_castps_si128(b);
      const vint4 ci = _mm_min_epi32(ai,bi);
      return _mm_castsi128_ps(ci);
    }
#endif

#if defined(__SSE4_1__)
    __forceinline Vec3fx maxi(const Vec3fx& a, const Vec3fx& b) {
      const vint4 ai = _mm_castps_si128(a);
      const vint4 bi = _mm_castps_si128(b);
      const vint4 ci = _mm_max_epi32(ai,bi);
      return _mm_castsi128_ps(ci);
    }
#endif

    __forceinline Vec3fx pow ( const Vec3fx& a, const float& b ) {
      return Vec3fx(powf(a.x,b),powf(a.y,b),powf(a.z,b));
    }
*/
  
  ////////////////////////////////////////////////////////////////////////////////
  /// Ternary Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline Vec3fx madd  ( const Vec3fx& a, const Vec3fx& b, const Vec3fx& c) { return Vec3fx(madd(a.x,b.x,c.x), madd(a.y,b.y,c.y), madd(a.z,b.z,c.z), madd(a.w,b.w,c.w)); }
  __forceinline Vec3fx msub  ( const Vec3fx& a, const Vec3fx& b, const Vec3fx& c) { return Vec3fx(msub(a.x,b.x,c.x), msub(a.y,b.y,c.y), msub(a.z,b.z,c.z), msub(a.w,b.w,c.w)); }
  __forceinline Vec3fx nmadd ( const Vec3fx& a, const Vec3fx& b, const Vec3fx& c) { return Vec3fx(nmadd(a.x,b.x,c.x), nmadd(a.y,b.y,c.y), nmadd(a.z,b.z,c.z), nmadd(a.w,b.w,c.w)); }
  __forceinline Vec3fx nmsub ( const Vec3fx& a, const Vec3fx& b, const Vec3fx& c) { return Vec3fx(nmsub(a.x,b.x,c.x), nmsub(a.y,b.y,c.y), nmsub(a.z,b.z,c.z), nmsub(a.w,b.w,c.w)); }

  __forceinline Vec3fx madd  ( const float a, const Vec3fx& b, const Vec3fx& c) { return madd(Vec3fx(a),b,c); }
  __forceinline Vec3fx msub  ( const float a, const Vec3fx& b, const Vec3fx& c) { return msub(Vec3fx(a),b,c); }
  __forceinline Vec3fx nmadd ( const float a, const Vec3fx& b, const Vec3fx& c) { return nmadd(Vec3fx(a),b,c); }
  __forceinline Vec3fx nmsub ( const float a, const Vec3fx& b, const Vec3fx& c) { return nmsub(Vec3fx(a),b,c); }

  ////////////////////////////////////////////////////////////////////////////////
  /// Assignment Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline Vec3fx& operator +=( Vec3fx& a, const Vec3fx& b ) { return a = a + b; }
  __forceinline Vec3fx& operator -=( Vec3fx& a, const Vec3fx& b ) { return a = a - b; }
  __forceinline Vec3fx& operator *=( Vec3fx& a, const Vec3fx& b ) { return a = a * b; }
  __forceinline Vec3fx& operator *=( Vec3fx& a, const float   b ) { return a = a * b; }
  __forceinline Vec3fx& operator /=( Vec3fx& a, const Vec3fx& b ) { return a = a / b; }
  __forceinline Vec3fx& operator /=( Vec3fx& a, const float   b ) { return a = a / b; }

  ////////////////////////////////////////////////////////////////////////////////
  /// Reductions
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline float reduce_add(const Vec3fx& v) { return v.x+v.y+v.z; }
  __forceinline float reduce_mul(const Vec3fx& v) { return v.x*v.y*v.z; }
  __forceinline float reduce_min(const Vec3fx& v) { return sycl::fmin(sycl::fmin(v.x,v.y),v.z); }
  __forceinline float reduce_max(const Vec3fx& v) { return sycl::fmax(sycl::fmax(v.x,v.y),v.z); }

  ////////////////////////////////////////////////////////////////////////////////
  /// Comparison Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline bool operator ==( const Vec3fx& a, const Vec3fx& b ) { return a.x == b.x && a.y == b.y && a.z == b.z; }
  __forceinline bool operator !=( const Vec3fx& a, const Vec3fx& b ) { return a.x != b.x || a.y != b.y || a.z != b.z; }

  __forceinline Vec3ba eq_mask( const Vec3fx& a, const Vec3fx& b ) { return Vec3ba(a.x == b.x, a.y == b.y, a.z == b.z); }
  __forceinline Vec3ba neq_mask(const Vec3fx& a, const Vec3fx& b ) { return Vec3ba(a.x != b.x, a.y != b.y, a.z != b.z); }
  __forceinline Vec3ba lt_mask( const Vec3fx& a, const Vec3fx& b ) { return Vec3ba(a.x <  b.x, a.y <  b.y, a.z <  b.z); }
  __forceinline Vec3ba le_mask( const Vec3fx& a, const Vec3fx& b ) { return Vec3ba(a.x <= b.x, a.y <= b.y, a.z <= b.z); }
  __forceinline Vec3ba gt_mask( const Vec3fx& a, const Vec3fx& b ) { return Vec3ba(a.x >  b.x, a.y >  b.y, a.z >  b.z); }
  __forceinline Vec3ba ge_mask( const Vec3fx& a, const Vec3fx& b ) { return Vec3ba(a.x >= b.x, a.y >= b.y, a.z >= b.z); }

  __forceinline bool isvalid ( const Vec3fx& v ) {
    return all(gt_mask(v,Vec3fx(-FLT_LARGE)) & lt_mask(v,Vec3fx(+FLT_LARGE)));
  }

  __forceinline bool is_finite ( const Vec3fx& a ) {
    return all(ge_mask(a,Vec3fx(-FLT_MAX)) & le_mask(a,Vec3fx(+FLT_MAX)));
  }

  __forceinline bool isvalid4 ( const Vec3fx& v ) {
    const bool valid_x = v.x >= -FLT_LARGE & v.x <= +FLT_LARGE;
    const bool valid_y = v.y >= -FLT_LARGE & v.y <= +FLT_LARGE;
    const bool valid_z = v.z >= -FLT_LARGE & v.z <= +FLT_LARGE;
    const bool valid_w = v.w >= -FLT_LARGE & v.w <= +FLT_LARGE;
    return valid_x & valid_y & valid_z & valid_w;
  }

  __forceinline bool is_finite4 ( const Vec3fx& v ) {
    const bool finite_x = v.x >= -FLT_MAX & v.x <= +FLT_MAX;
    const bool finite_y = v.y >= -FLT_MAX & v.y <= +FLT_MAX;
    const bool finite_z = v.z >= -FLT_MAX & v.z <= +FLT_MAX;
    const bool finite_w = v.w >= -FLT_MAX & v.w <= +FLT_MAX;
    return finite_x & finite_y & finite_z & finite_w;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// Euclidian Space Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline float dot ( const Vec3fx& a, const Vec3fx& b ) {
    return reduce_add(a*b);
  }

  __forceinline Vec3fx cross ( const Vec3fx& a, const Vec3fx& b ) {
    return Vec3fx(msub(a.y,b.z,a.z*b.y), msub(a.z,b.x,a.x*b.z), msub(a.x,b.y,a.y*b.x));
  }
  
  __forceinline float  sqr_length ( const Vec3fx& a )                { return dot(a,a); }
  __forceinline float  rcp_length ( const Vec3fx& a )                { return rsqrt(dot(a,a)); }
  __forceinline float  rcp_length2( const Vec3fx& a )                { return rcp(dot(a,a)); }
  __forceinline float  length   ( const Vec3fx& a )                  { return sqrt(dot(a,a)); }
  __forceinline Vec3fx normalize( const Vec3fx& a )                  { return a*rsqrt(dot(a,a)); }
  __forceinline float  distance ( const Vec3fx& a, const Vec3fx& b ) { return length(a-b); }
  __forceinline float  halfArea ( const Vec3fx& d )                  { return madd(d.x,(d.y+d.z),d.y*d.z); }
  __forceinline float  area     ( const Vec3fx& d )                  { return 2.0f*halfArea(d); }

  __forceinline Vec3fx normalize_safe( const Vec3fx& a ) {
    const float d = dot(a,a); if (unlikely(d == 0.0f)) return a; else return a*rsqrt(d);
  }

  /*! differentiated normalization */
  __forceinline Vec3fx dnormalize(const Vec3fx& p, const Vec3fx& dp)
  {
    const float pp  = dot(p,p);
    const float pdp = dot(p,dp);
    return (pp*dp-pdp*p)*rcp(pp)*rsqrt(pp);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// Select
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline Vec3fx select( bool s, const Vec3fx& t, const Vec3fx& f ) {
    return Vec3fx(s ? t.x : f.x, s ? t.y : f.y, s ? t.z : f.z, s ? t.w : f.w);
  }

  __forceinline Vec3fx select( const Vec3ba& s, const Vec3fx& t, const Vec3fx& f ) {
    return Vec3fx(s.x ? t.x : f.x, s.y ? t.y : f.y, s.z ? t.z : f.z);
  }
  
  __forceinline Vec3fx lerp(const Vec3fx& v0, const Vec3fx& v1, const float t) {
    return madd(1.0f-t,v0,t*v1);
  }

  __forceinline int maxDim ( const Vec3fx& a )
  {
    const Vec3fx b = abs(a);
    if (b.x > b.y) {
      if (b.x > b.z) return 0; else return 2;
    } else {
      if (b.y > b.z) return 1; else return 2;
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// Rounding Functions
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline Vec3fx trunc( const Vec3fx& a ) { return Vec3fx(sycl::trunc(a.x),sycl::trunc(a.y),sycl::trunc(a.z),sycl::trunc(a.w)); }
  __forceinline Vec3fx floor( const Vec3fx& a ) { return Vec3fx(sycl::floor(a.x),sycl::floor(a.y),sycl::floor(a.z),sycl::floor(a.w)); }
  __forceinline Vec3fx ceil ( const Vec3fx& a ) { return Vec3fx(sycl::ceil (a.x),sycl::ceil (a.y),sycl::ceil (a.z),sycl::ceil (a.w)); }

  ////////////////////////////////////////////////////////////////////////////////
  /// Output Operators
  ////////////////////////////////////////////////////////////////////////////////

  inline embree_ostream operator<<(embree_ostream cout, const Vec3fx& a) {
    return cout << "(" << a.x << ", " << a.y << ", " << a.z << "," << a.w << ")";
  }

  typedef Vec3fx Vec3ff;

  //__forceinline Vec2fa::Vec2fa(const Vec3fx& a)
  //  : x(a.x), y(a.y) {}

  //__forceinline Vec3ia::Vec3ia( const Vec3fx& a )
  //  : x((int)a.x), y((int)a.y), z((int)a.z) {}
}
