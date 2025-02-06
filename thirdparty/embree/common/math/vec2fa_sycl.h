// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "../sys/alloc.h"
#include "emath.h"
#include "../simd/sse.h"

namespace embree
{
  struct Vec3fa;
  
  ////////////////////////////////////////////////////////////////////////////////
  /// SSE Vec2fa Type
  ////////////////////////////////////////////////////////////////////////////////

  struct __aligned(16) Vec2fa
  {
    //ALIGNED_STRUCT_(16);

    typedef float Scalar;
    enum { N = 2 };
    struct { float x,y; };
    
    ////////////////////////////////////////////////////////////////////////////////
    /// Constructors, Assignment & Cast Operators
    ////////////////////////////////////////////////////////////////////////////////

    __forceinline Vec2fa( ) {}
    //__forceinline Vec2fa( const __m128 a ) : m128(a) {}
    explicit Vec2fa(const Vec3fa& a);
    
    __forceinline explicit Vec2fa( const vfloat<4>& a ) {
      x = a[0];
      y = a[1];
    }

    __forceinline Vec2fa            ( const Vec2<float>& other  ) { x = other.x; y = other.y; }
    __forceinline Vec2fa& operator =( const Vec2<float>& other ) { x = other.x; y = other.y; return *this; }

    __forceinline Vec2fa            ( const Vec2fa& other ) { x = other.x; y = other.y; }
    __forceinline Vec2fa& operator =( const Vec2fa& other ) { x = other.x; y = other.y; return *this; }

    __forceinline explicit Vec2fa( const float a ) : x(a), y(a) {}
    __forceinline          Vec2fa( const float x, const float y) : x(x), y(y) {}

    //__forceinline explicit Vec2fa( const __m128i a ) : m128(_mm_cvtepi32_ps(a)) {}

    //__forceinline operator const __m128&() const { return m128; }
    //__forceinline operator       __m128&()       { return m128; }

    ////////////////////////////////////////////////////////////////////////////////
    /// Loads and Stores
    ////////////////////////////////////////////////////////////////////////////////

    static __forceinline Vec2fa load( const void* const a ) {
      const float* ptr = (const float*)a;
      return Vec2fa(ptr[0],ptr[1]);
    }

    static __forceinline Vec2fa loadu( const void* const a ) {
      const float* ptr = (const float*)a;
      return Vec2fa(ptr[0],ptr[1]);
    }

    static __forceinline void storeu ( void* a, const Vec2fa& v ) {
      float* ptr = (float*)a;
      ptr[0] = v.x; ptr[1] = v.y;
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// Constants
    ////////////////////////////////////////////////////////////////////////////////

    __forceinline Vec2fa( ZeroTy   ) : x(0.0f), y(0.0f) {}
    __forceinline Vec2fa( OneTy    ) : x(1.0f), y(1.0f) {}
    __forceinline Vec2fa( PosInfTy ) : x(+INFINITY), y(+INFINITY) {}
    __forceinline Vec2fa( NegInfTy ) : x(-INFINITY), y(-INFINITY) {}

    ////////////////////////////////////////////////////////////////////////////////
    /// Array Access
    ////////////////////////////////////////////////////////////////////////////////

    //__forceinline const float& operator []( const size_t index ) const { assert(index < 2); return (&x)[index]; }
    //__forceinline       float& operator []( const size_t index )       { assert(index < 2); return (&x)[index]; }
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// Unary Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline Vec2fa operator +( const Vec2fa& a ) { return a; }
  __forceinline Vec2fa operator -( const Vec2fa& a ) { return Vec2fa(-a.x,-a.y); }
  __forceinline Vec2fa abs  ( const Vec2fa& a ) { return Vec2fa(sycl::fabs(a.x),sycl::fabs(a.y)); }
  __forceinline Vec2fa sign ( const Vec2fa& a ) { return Vec2fa(sycl::sign(a.x),sycl::sign(a.y)); }

   //__forceinline Vec2fa rcp  ( const Vec2fa& a ) { return Vec2fa(sycl::native::recip(a.x),sycl::native::recip(a.y)); }
  __forceinline Vec2fa rcp  ( const Vec2fa& a ) { return Vec2fa(sycl::native::recip(a.x),sycl::native::recip(a.y)); }
  __forceinline Vec2fa sqrt ( const Vec2fa& a ) { return Vec2fa(sycl::sqrt(a.x),sycl::sqrt(a.y)); }
  __forceinline Vec2fa sqr  ( const Vec2fa& a ) { return Vec2fa(a.x*a.x,a.y*a.y); }
  
  __forceinline Vec2fa rsqrt( const Vec2fa& a ) { return Vec2fa(sycl::rsqrt(a.x),sycl::rsqrt(a.y)); }

  __forceinline Vec2fa zero_fix(const Vec2fa& a) {
    const float x = sycl::fabs(a.x) < min_rcp_input ? min_rcp_input : a.x;
    const float y = sycl::fabs(a.y) < min_rcp_input ? min_rcp_input : a.y;
    return Vec2fa(x,y);
  }
  __forceinline Vec2fa rcp_safe(const Vec2fa& a) {
    return rcp(zero_fix(a));
  }
  __forceinline Vec2fa log ( const Vec2fa& a ) {
    return Vec2fa(sycl::log(a.x),sycl::log(a.y));
  }

  __forceinline Vec2fa exp ( const Vec2fa& a ) {
    return Vec2fa(sycl::exp(a.x),sycl::exp(a.y));
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// Binary Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline Vec2fa operator +( const Vec2fa& a, const Vec2fa& b ) { return Vec2fa(a.x+b.x, a.y+b.y); }
  __forceinline Vec2fa operator -( const Vec2fa& a, const Vec2fa& b ) { return Vec2fa(a.x-b.x, a.y-b.y); }
  __forceinline Vec2fa operator *( const Vec2fa& a, const Vec2fa& b ) { return Vec2fa(a.x*b.x, a.y*b.y); }
  __forceinline Vec2fa operator *( const Vec2fa& a, const float b ) { return a * Vec2fa(b); }
  __forceinline Vec2fa operator *( const float a, const Vec2fa& b ) { return Vec2fa(a) * b; }
  __forceinline Vec2fa operator /( const Vec2fa& a, const Vec2fa& b ) { return Vec2fa(a.x/b.x, a.y/b.y); }
  __forceinline Vec2fa operator /( const Vec2fa& a, const float b        ) { return Vec2fa(a.x/b, a.y/b); }
  __forceinline Vec2fa operator /( const        float a, const Vec2fa& b ) { return Vec2fa(a/b.x, a/b.y); }

  __forceinline Vec2fa min( const Vec2fa& a, const Vec2fa& b ) {
    return Vec2fa(sycl::fmin(a.x,b.x), sycl::fmin(a.y,b.y));
  }
  __forceinline Vec2fa max( const Vec2fa& a, const Vec2fa& b ) {
    return Vec2fa(sycl::fmax(a.x,b.x), sycl::fmax(a.y,b.y));
  }

/*
#if defined(__SSE4_1__)
    __forceinline Vec2fa mini(const Vec2fa& a, const Vec2fa& b) {
      const vint4 ai = _mm_castps_si128(a);
      const vint4 bi = _mm_castps_si128(b);
      const vint4 ci = _mm_min_epi32(ai,bi);
      return _mm_castsi128_ps(ci);
    }
#endif

#if defined(__SSE4_1__)
    __forceinline Vec2fa maxi(const Vec2fa& a, const Vec2fa& b) {
      const vint4 ai = _mm_castps_si128(a);
      const vint4 bi = _mm_castps_si128(b);
      const vint4 ci = _mm_max_epi32(ai,bi);
      return _mm_castsi128_ps(ci);
    }
#endif

    __forceinline Vec2fa pow ( const Vec2fa& a, const float& b ) {
      return Vec2fa(powf(a.x,b),powf(a.y,b));
    }
*/
  
  ////////////////////////////////////////////////////////////////////////////////
  /// Ternary Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline Vec2fa madd  ( const Vec2fa& a, const Vec2fa& b, const Vec2fa& c) { return Vec2fa(madd(a.x,b.x,c.x), madd(a.y,b.y,c.y)); }
  __forceinline Vec2fa msub  ( const Vec2fa& a, const Vec2fa& b, const Vec2fa& c) { return Vec2fa(msub(a.x,b.x,c.x), msub(a.y,b.y,c.y)); }
  __forceinline Vec2fa nmadd ( const Vec2fa& a, const Vec2fa& b, const Vec2fa& c) { return Vec2fa(nmadd(a.x,b.x,c.x), nmadd(a.y,b.y,c.y)); }
  __forceinline Vec2fa nmsub ( const Vec2fa& a, const Vec2fa& b, const Vec2fa& c) { return Vec2fa(nmsub(a.x,b.x,c.x), nmsub(a.y,b.y,c.y)); }

  __forceinline Vec2fa madd  ( const float a, const Vec2fa& b, const Vec2fa& c) { return madd(Vec2fa(a),b,c); }
  __forceinline Vec2fa msub  ( const float a, const Vec2fa& b, const Vec2fa& c) { return msub(Vec2fa(a),b,c); }
  __forceinline Vec2fa nmadd ( const float a, const Vec2fa& b, const Vec2fa& c) { return nmadd(Vec2fa(a),b,c); }
  __forceinline Vec2fa nmsub ( const float a, const Vec2fa& b, const Vec2fa& c) { return nmsub(Vec2fa(a),b,c); }

  ////////////////////////////////////////////////////////////////////////////////
  /// Assignment Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline Vec2fa& operator +=( Vec2fa& a, const Vec2fa& b ) { return a = a + b; }
  __forceinline Vec2fa& operator -=( Vec2fa& a, const Vec2fa& b ) { return a = a - b; }
  __forceinline Vec2fa& operator *=( Vec2fa& a, const Vec2fa& b ) { return a = a * b; }
  __forceinline Vec2fa& operator *=( Vec2fa& a, const float   b ) { return a = a * b; }
  __forceinline Vec2fa& operator /=( Vec2fa& a, const Vec2fa& b ) { return a = a / b; }
  __forceinline Vec2fa& operator /=( Vec2fa& a, const float   b ) { return a = a / b; }

  ////////////////////////////////////////////////////////////////////////////////
  /// Reductions
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline float reduce_add(const Vec2fa& v) { return v.x+v.y; }
  __forceinline float reduce_mul(const Vec2fa& v) { return v.x*v.y; }
  __forceinline float reduce_min(const Vec2fa& v) { return sycl::fmin(v.x,v.y); }
  __forceinline float reduce_max(const Vec2fa& v) { return sycl::fmax(v.x,v.y); }

  ////////////////////////////////////////////////////////////////////////////////
  /// Comparison Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline bool operator ==( const Vec2fa& a, const Vec2fa& b ) { return a.x == b.x && a.y == b.y; }
  __forceinline bool operator !=( const Vec2fa& a, const Vec2fa& b ) { return a.x != b.x || a.y != b.y; }

  ////////////////////////////////////////////////////////////////////////////////
  /// Euclidian Space Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline float dot ( const Vec2fa& a, const Vec2fa& b ) {
    return reduce_add(a*b);
  }

  __forceinline Vec2fa cross ( const Vec2fa& a ) {
    return Vec2fa(-a.y,a.x);
  }

  __forceinline float  sqr_length ( const Vec2fa& a )                { return dot(a,a); }
  __forceinline float  rcp_length ( const Vec2fa& a )                { return rsqrt(dot(a,a)); }
  __forceinline float  rcp_length2( const Vec2fa& a )                { return rcp(dot(a,a)); }
  __forceinline float  length   ( const Vec2fa& a )                  { return sqrt(dot(a,a)); }
  __forceinline Vec2fa normalize( const Vec2fa& a )                  { return a*rsqrt(dot(a,a)); }
  __forceinline float  distance ( const Vec2fa& a, const Vec2fa& b ) { return length(a-b); }

  ////////////////////////////////////////////////////////////////////////////////
  /// Select
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline Vec2fa select( bool s, const Vec2fa& t, const Vec2fa& f ) {
    return Vec2fa(s ? t.x : f.x, s ? t.y : f.y);
  }

  __forceinline Vec2fa lerp(const Vec2fa& v0, const Vec2fa& v1, const float t) {
    return madd(1.0f-t,v0,t*v1);
  }

  __forceinline int maxDim ( const Vec2fa& a )
  {
    const Vec2fa b = abs(a);
    if (b.x > b.y) return 0;
    else return 1;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// Rounding Functions
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline Vec2fa trunc( const Vec2fa& a ) { return Vec2fa(sycl::trunc(a.x),sycl::trunc(a.y)); }
  __forceinline Vec2fa floor( const Vec2fa& a ) { return Vec2fa(sycl::floor(a.x),sycl::floor(a.y)); }
  __forceinline Vec2fa ceil ( const Vec2fa& a ) { return Vec2fa(sycl::ceil (a.x),sycl::ceil (a.y)); }

  ////////////////////////////////////////////////////////////////////////////////
  /// Output Operators
  ////////////////////////////////////////////////////////////////////////////////

  inline embree_ostream operator<<(embree_ostream cout, const Vec2fa& a) {
    return cout << "(" << a.x << ", " << a.y << ")";
  }

  /*template<>
  __forceinline vfloat_impl<4>::vfloat_impl(const Vec2fa& a)
  {
    v = 0;
    const unsigned int lid = get_sub_group_local_id();
    if (lid == 0) v = a.x;
    if (lid == 1) v = a.y;
  }*/

  typedef Vec2fa Vec2fa_t;
}
