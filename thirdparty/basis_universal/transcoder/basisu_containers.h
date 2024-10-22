// basisu_containers.h
#pragma once
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <algorithm>

#if defined(__linux__) && !defined(ANDROID)
// Only for malloc_usable_size() in basisu_containers_impl.h
#include <malloc.h>
#define HAS_MALLOC_USABLE_SIZE 1
#endif

// Set to 1 to always check vector operator[], front(), and back() even in release.
#define BASISU_VECTOR_FORCE_CHECKING 0

// If 1, the vector container will not query the CRT to get the size of resized memory blocks.
#define BASISU_VECTOR_DETERMINISTIC 1

#ifdef _MSC_VER
#define BASISU_FORCE_INLINE __forceinline
#else
#define BASISU_FORCE_INLINE inline
#endif

namespace basisu
{
   enum { cInvalidIndex = -1 };

   namespace helpers
   {
      inline bool is_power_of_2(uint32_t x) { return x && ((x & (x - 1U)) == 0U); }
      inline bool is_power_of_2(uint64_t x) { return x && ((x & (x - 1U)) == 0U); }
      template<class T> const T& minimum(const T& a, const T& b) { return (b < a) ? b : a; }
      template<class T> const T& maximum(const T& a, const T& b) { return (a < b) ? b : a; }

      inline uint32_t floor_log2i(uint32_t v)
      {
         uint32_t l = 0;
         while (v > 1U)
         {
            v >>= 1;
            l++;
         }
         return l;
      }

      inline uint32_t next_pow2(uint32_t val)
      {
         val--;
         val |= val >> 16;
         val |= val >> 8;
         val |= val >> 4;
         val |= val >> 2;
         val |= val >> 1;
         return val + 1;
      }

      inline uint64_t next_pow2(uint64_t val)
      {
         val--;
         val |= val >> 32;
         val |= val >> 16;
         val |= val >> 8;
         val |= val >> 4;
         val |= val >> 2;
         val |= val >> 1;
         return val + 1;
      }
   } // namespace helpers

   template <typename T>
   inline T* construct(T* p)
   {
      return new (static_cast<void*>(p)) T;
   }

   template <typename T, typename U>
   inline T* construct(T* p, const U& init)
   {
      return new (static_cast<void*>(p)) T(init);
   }

   template <typename T>
   inline void construct_array(T* p, size_t n)
   {
      T* q = p + n;
      for (; p != q; ++p)
         new (static_cast<void*>(p)) T;
   }

   template <typename T, typename U>
   inline void construct_array(T* p, size_t n, const U& init)
   {
      T* q = p + n;
      for (; p != q; ++p)
         new (static_cast<void*>(p)) T(init);
   }

   template <typename T>
   inline void destruct(T* p)
   {
      (void)p;
      p->~T();
   }

   template <typename T> inline void destruct_array(T* p, size_t n)
   {
      T* q = p + n;
      for (; p != q; ++p)
         p->~T();
   }

   template<typename T> struct int_traits { enum { cMin = INT32_MIN, cMax = INT32_MAX, cSigned = true }; };

   template<> struct int_traits<int8_t> { enum { cMin = INT8_MIN, cMax = INT8_MAX, cSigned = true }; };
   template<> struct int_traits<int16_t> { enum { cMin = INT16_MIN, cMax = INT16_MAX, cSigned = true }; };
   template<> struct int_traits<int32_t> { enum { cMin = INT32_MIN, cMax = INT32_MAX, cSigned = true }; };

   template<> struct int_traits<uint8_t> { enum { cMin = 0, cMax = UINT8_MAX, cSigned = false }; };
   template<> struct int_traits<uint16_t> { enum { cMin = 0, cMax = UINT16_MAX, cSigned = false }; };
   template<> struct int_traits<uint32_t> { enum { cMin = 0, cMax = UINT32_MAX, cSigned = false }; };

   template<typename T>
   struct scalar_type
   {
      enum { cFlag = false };
      static inline void construct(T* p) { basisu::construct(p); }
      static inline void construct(T* p, const T& init) { basisu::construct(p, init); }
      static inline void construct_array(T* p, size_t n) { basisu::construct_array(p, n); }
      static inline void destruct(T* p) { basisu::destruct(p); }
      static inline void destruct_array(T* p, size_t n) { basisu::destruct_array(p, n); }
   };

   template<typename T> struct scalar_type<T*>
   {
      enum { cFlag = true };
      static inline void construct(T** p) { memset(p, 0, sizeof(T*)); }
      static inline void construct(T** p, T* init) { *p = init; }
      static inline void construct_array(T** p, size_t n) { memset(p, 0, sizeof(T*) * n); }
      static inline void destruct(T** p) { p; }
      static inline void destruct_array(T** p, size_t n) { p, n; }
   };

#define BASISU_DEFINE_BUILT_IN_TYPE(X) \
   template<> struct scalar_type<X> { \
   enum { cFlag = true }; \
   static inline void construct(X* p) { memset(p, 0, sizeof(X)); } \
   static inline void construct(X* p, const X& init) { memcpy(p, &init, sizeof(X)); } \
   static inline void construct_array(X* p, size_t n) { memset(p, 0, sizeof(X) * n); } \
   static inline void destruct(X* p) { p; } \
   static inline void destruct_array(X* p, size_t n) { p, n; } };

   BASISU_DEFINE_BUILT_IN_TYPE(bool)
   BASISU_DEFINE_BUILT_IN_TYPE(char)
   BASISU_DEFINE_BUILT_IN_TYPE(unsigned char)
   BASISU_DEFINE_BUILT_IN_TYPE(short)
   BASISU_DEFINE_BUILT_IN_TYPE(unsigned short)
   BASISU_DEFINE_BUILT_IN_TYPE(int)
   BASISU_DEFINE_BUILT_IN_TYPE(unsigned int)
   BASISU_DEFINE_BUILT_IN_TYPE(long)
   BASISU_DEFINE_BUILT_IN_TYPE(unsigned long)
#ifdef __GNUC__
   BASISU_DEFINE_BUILT_IN_TYPE(long long)
   BASISU_DEFINE_BUILT_IN_TYPE(unsigned long long)
#else
   BASISU_DEFINE_BUILT_IN_TYPE(__int64)
   BASISU_DEFINE_BUILT_IN_TYPE(unsigned __int64)
#endif
   BASISU_DEFINE_BUILT_IN_TYPE(float)
   BASISU_DEFINE_BUILT_IN_TYPE(double)
   BASISU_DEFINE_BUILT_IN_TYPE(long double)

#undef BASISU_DEFINE_BUILT_IN_TYPE

   template<typename T>
   struct bitwise_movable { enum { cFlag = false }; };

#define BASISU_DEFINE_BITWISE_MOVABLE(Q) template<> struct bitwise_movable<Q> { enum { cFlag = true }; };

   template<typename T>
   struct bitwise_copyable { enum { cFlag = false }; };

#define BASISU_DEFINE_BITWISE_COPYABLE(Q) template<> struct bitwise_copyable<Q> { enum { cFlag = true }; };

#define BASISU_IS_POD(T) __is_pod(T)

#define BASISU_IS_SCALAR_TYPE(T) (scalar_type<T>::cFlag)

#if !defined(BASISU_HAVE_STD_TRIVIALLY_COPYABLE) && defined(__GNUC__) && __GNUC__<5
   //#define BASISU_IS_TRIVIALLY_COPYABLE(...) __has_trivial_copy(__VA_ARGS__)
    #define BASISU_IS_TRIVIALLY_COPYABLE(...) __is_trivially_copyable(__VA_ARGS__)
#else
   #define BASISU_IS_TRIVIALLY_COPYABLE(...) std::is_trivially_copyable<__VA_ARGS__>::value
#endif

// TODO: clean this up
#define BASISU_IS_BITWISE_COPYABLE(T) (BASISU_IS_SCALAR_TYPE(T) || BASISU_IS_POD(T) || BASISU_IS_TRIVIALLY_COPYABLE(T) || (bitwise_copyable<T>::cFlag))

#define BASISU_IS_BITWISE_COPYABLE_OR_MOVABLE(T) (BASISU_IS_BITWISE_COPYABLE(T) || (bitwise_movable<T>::cFlag))

#define BASISU_HAS_DESTRUCTOR(T) ((!scalar_type<T>::cFlag) && (!__is_pod(T)))

   typedef char(&yes_t)[1];
   typedef char(&no_t)[2];

   template <class U> yes_t class_test(int U::*);
   template <class U> no_t class_test(...);

   template <class T> struct is_class
   {
      enum { value = (sizeof(class_test<T>(0)) == sizeof(yes_t)) };
   };

   template <typename T> struct is_pointer
   {
      enum { value = false };
   };

   template <typename T> struct is_pointer<T*>
   {
      enum { value = true };
   };

   struct empty_type { };

   BASISU_DEFINE_BITWISE_COPYABLE(empty_type);
   BASISU_DEFINE_BITWISE_MOVABLE(empty_type);

   template<typename T> struct rel_ops
   {
      friend bool operator!=(const T& x, const T& y) { return (!(x == y)); }
      friend bool operator> (const T& x, const T& y) { return (y < x); }
      friend bool operator<=(const T& x, const T& y) { return (!(y < x)); }
      friend bool operator>=(const T& x, const T& y) { return (!(x < y)); }
   };

   struct elemental_vector
   {
      void* m_p;
      uint32_t m_size;
      uint32_t m_capacity;

      typedef void (*object_mover)(void* pDst, void* pSrc, uint32_t num);

      bool increase_capacity(uint32_t min_new_capacity, bool grow_hint, uint32_t element_size, object_mover pRelocate, bool nofail);
   };

   template<typename T>
   class vector : public rel_ops< vector<T> >
   {
   public:
      typedef T* iterator;
      typedef const T* const_iterator;
      typedef T value_type;
      typedef T& reference;
      typedef const T& const_reference;
      typedef T* pointer;
      typedef const T* const_pointer;

      inline vector() :
         m_p(NULL),
         m_size(0),
         m_capacity(0)
      {
      }

      inline vector(uint32_t n, const T& init) :
         m_p(NULL),
         m_size(0),
         m_capacity(0)
      {
         increase_capacity(n, false);
         construct_array(m_p, n, init);
         m_size = n;
      }

      inline vector(const vector& other) :
         m_p(NULL),
         m_size(0),
         m_capacity(0)
      {
         increase_capacity(other.m_size, false);

         m_size = other.m_size;

         if (BASISU_IS_BITWISE_COPYABLE(T))
         {
#ifndef __EMSCRIPTEN__
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"            
#endif                  
#endif
             if ((m_p) && (other.m_p))
                memcpy(m_p, other.m_p, m_size * sizeof(T));
#ifndef __EMSCRIPTEN__
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif                
#endif
         }
         else
         {
            T* pDst = m_p;
            const T* pSrc = other.m_p;
            for (uint32_t i = m_size; i > 0; i--)
               construct(pDst++, *pSrc++);
         }
      }

      inline explicit vector(size_t size) :
         m_p(NULL),
         m_size(0),
         m_capacity(0)
      {
         resize(size);
      }

      inline ~vector()
      {
         if (m_p)
         {
            scalar_type<T>::destruct_array(m_p, m_size);
            free(m_p);
         }
      }

      inline vector& operator= (const vector& other)
      {
         if (this == &other)
            return *this;

         if (m_capacity >= other.m_size)
            resize(0);
         else
         {
            clear();
            increase_capacity(other.m_size, false);
         }

         if (BASISU_IS_BITWISE_COPYABLE(T))
         {
#ifndef __EMSCRIPTEN__
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"            
#endif         
#endif
             if ((m_p) && (other.m_p))
                memcpy(m_p, other.m_p, other.m_size * sizeof(T));
#ifndef __EMSCRIPTEN__          
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif                            
#endif
         }
         else
         {
            T* pDst = m_p;
            const T* pSrc = other.m_p;
            for (uint32_t i = other.m_size; i > 0; i--)
               construct(pDst++, *pSrc++);
         }

         m_size = other.m_size;

         return *this;
      }

      BASISU_FORCE_INLINE const T* begin() const { return m_p; }
      BASISU_FORCE_INLINE T* begin() { return m_p; }

      BASISU_FORCE_INLINE const T* end() const { return m_p + m_size; }
      BASISU_FORCE_INLINE T* end() { return m_p + m_size; }

      BASISU_FORCE_INLINE bool empty() const { return !m_size; }
      BASISU_FORCE_INLINE uint32_t size() const { return m_size; }
      BASISU_FORCE_INLINE uint32_t size_in_bytes() const { return m_size * sizeof(T); }
      BASISU_FORCE_INLINE uint32_t capacity() const { return m_capacity; }

      // operator[] will assert on out of range indices, but in final builds there is (and will never be) any range checking on this method.
      //BASISU_FORCE_INLINE const T& operator[] (uint32_t i) const { assert(i < m_size); return m_p[i]; }
      //BASISU_FORCE_INLINE T& operator[] (uint32_t i) { assert(i < m_size); return m_p[i]; }
            
#if !BASISU_VECTOR_FORCE_CHECKING
      BASISU_FORCE_INLINE const T& operator[] (size_t i) const { assert(i < m_size); return m_p[i]; }
      BASISU_FORCE_INLINE T& operator[] (size_t i) { assert(i < m_size); return m_p[i]; }
#else
      BASISU_FORCE_INLINE const T& operator[] (size_t i) const 
      { 
          if (i >= m_size)
          {
              fprintf(stderr, "operator[] invalid index: %u, max entries %u, type size %u\n", (uint32_t)i, m_size, (uint32_t)sizeof(T));
              abort();
          }
          return m_p[i]; 
      }
      BASISU_FORCE_INLINE T& operator[] (size_t i) 
      { 
          if (i >= m_size)
          {
              fprintf(stderr, "operator[] invalid index: %u, max entries %u, type size %u\n", (uint32_t)i, m_size, (uint32_t)sizeof(T));
              abort();
          }
          return m_p[i]; 
      }
#endif

      // at() always includes range checking, even in final builds, unlike operator [].
      // The first element is returned if the index is out of range.
      BASISU_FORCE_INLINE const T& at(size_t i) const { assert(i < m_size); return (i >= m_size) ? m_p[0] : m_p[i]; }
      BASISU_FORCE_INLINE T& at(size_t i) { assert(i < m_size); return (i >= m_size) ? m_p[0] : m_p[i]; }
            
#if !BASISU_VECTOR_FORCE_CHECKING
      BASISU_FORCE_INLINE const T& front() const { assert(m_size); return m_p[0]; }
      BASISU_FORCE_INLINE T& front() { assert(m_size); return m_p[0]; }

      BASISU_FORCE_INLINE const T& back() const { assert(m_size); return m_p[m_size - 1]; }
      BASISU_FORCE_INLINE T& back() { assert(m_size); return m_p[m_size - 1]; }
#else
      BASISU_FORCE_INLINE const T& front() const 
      { 
          if (!m_size)
          {
              fprintf(stderr, "front: vector is empty, type size %u\n", (uint32_t)sizeof(T));
              abort();
          }
          return m_p[0]; 
      }
      BASISU_FORCE_INLINE T& front() 
      { 
          if (!m_size)
          {
              fprintf(stderr, "front: vector is empty, type size %u\n", (uint32_t)sizeof(T));
              abort();
          }
          return m_p[0]; 
      }

      BASISU_FORCE_INLINE const T& back() const 
      { 
          if(!m_size)
          {
              fprintf(stderr, "back: vector is empty, type size %u\n", (uint32_t)sizeof(T));
              abort();
          }
          return m_p[m_size - 1]; 
      }
      BASISU_FORCE_INLINE T& back() 
      { 
          if (!m_size)
          {
              fprintf(stderr, "back: vector is empty, type size %u\n", (uint32_t)sizeof(T));
              abort();
          }
          return m_p[m_size - 1]; 
      }
#endif

      BASISU_FORCE_INLINE const T* get_ptr() const { return m_p; }
      BASISU_FORCE_INLINE T* get_ptr() { return m_p; }

      BASISU_FORCE_INLINE const T* data() const { return m_p; }
      BASISU_FORCE_INLINE T* data() { return m_p; }

      // clear() sets the container to empty, then frees the allocated block.
      inline void clear()
      {
         if (m_p)
         {
            scalar_type<T>::destruct_array(m_p, m_size);
            free(m_p);
            m_p = NULL;
            m_size = 0;
            m_capacity = 0;
         }
      }

      inline void clear_no_destruction()
      {
         if (m_p)
         {
            free(m_p);
            m_p = NULL;
            m_size = 0;
            m_capacity = 0;
         }
      }

      inline void reserve(size_t new_capacity_size_t)
      {
         if (new_capacity_size_t > UINT32_MAX)
         {
            assert(0);
            return;
         }

         uint32_t new_capacity = (uint32_t)new_capacity_size_t;

         if (new_capacity > m_capacity)
            increase_capacity(new_capacity, false);
         else if (new_capacity < m_capacity)
         {
            // Must work around the lack of a "decrease_capacity()" method.
            // This case is rare enough in practice that it's probably not worth implementing an optimized in-place resize.
            vector tmp;
            tmp.increase_capacity(helpers::maximum(m_size, new_capacity), false);
            tmp = *this;
            swap(tmp);
         }
      }

      inline bool try_reserve(size_t new_capacity_size_t)
      {
         if (new_capacity_size_t > UINT32_MAX)
         {
            assert(0);
            return false;
         }

         uint32_t new_capacity = (uint32_t)new_capacity_size_t;

         if (new_capacity > m_capacity)
         {
            if (!increase_capacity(new_capacity, false, true))
               return false;
         }
         else if (new_capacity < m_capacity)
         {
            // Must work around the lack of a "decrease_capacity()" method.
            // This case is rare enough in practice that it's probably not worth implementing an optimized in-place resize.
            vector tmp;
            if (!tmp.increase_capacity(helpers::maximum(m_size, new_capacity), false, true))
                return false;
            tmp = *this;
            swap(tmp);
         }

         return true;
      }

      // resize(0) sets the container to empty, but does not free the allocated block.
      inline void resize(size_t new_size_size_t, bool grow_hint = false)
      {
         if (new_size_size_t > UINT32_MAX)
         {
            assert(0);
            return;
         }

         uint32_t new_size = (uint32_t)new_size_size_t;

         if (m_size != new_size)
         {
            if (new_size < m_size)
               scalar_type<T>::destruct_array(m_p + new_size, m_size - new_size);
            else
            {
               if (new_size > m_capacity)
                  increase_capacity(new_size, (new_size == (m_size + 1)) || grow_hint);

               scalar_type<T>::construct_array(m_p + m_size, new_size - m_size);
            }

            m_size = new_size;
         }
      }

      inline bool try_resize(size_t new_size_size_t, bool grow_hint = false)
      {
         if (new_size_size_t > UINT32_MAX)
         {
            assert(0);
            return false;
         }

         uint32_t new_size = (uint32_t)new_size_size_t;

         if (m_size != new_size)
         {
            if (new_size < m_size)
               scalar_type<T>::destruct_array(m_p + new_size, m_size - new_size);
            else
            {
               if (new_size > m_capacity)
               {
                  if (!increase_capacity(new_size, (new_size == (m_size + 1)) || grow_hint, true))
                     return false;
               }

               scalar_type<T>::construct_array(m_p + m_size, new_size - m_size);
            }

            m_size = new_size;
         }

         return true;
      }

      // If size >= capacity/2, reset() sets the container's size to 0 but doesn't free the allocated block (because the container may be similarly loaded in the future).
      // Otherwise it blows away the allocated block. See http://www.codercorner.com/blog/?p=494
      inline void reset()
      {
         if (m_size >= (m_capacity >> 1))
            resize(0);
         else
            clear();
      }

      inline T* enlarge(uint32_t i)
      {
         uint32_t cur_size = m_size;
         resize(cur_size + i, true);
         return get_ptr() + cur_size;
      }

      inline T* try_enlarge(uint32_t i)
      {
         uint32_t cur_size = m_size;
         if (!try_resize(cur_size + i, true))
            return NULL;
         return get_ptr() + cur_size;
      }

      BASISU_FORCE_INLINE void push_back(const T& obj)
      {
         assert(!m_p || (&obj < m_p) || (&obj >= (m_p + m_size)));

         if (m_size >= m_capacity)
            increase_capacity(m_size + 1, true);

         scalar_type<T>::construct(m_p + m_size, obj);
         m_size++;
      }

      inline bool try_push_back(const T& obj)
      {
         assert(!m_p || (&obj < m_p) || (&obj >= (m_p + m_size)));

         if (m_size >= m_capacity)
         {
            if (!increase_capacity(m_size + 1, true, true))
               return false;
         }

         scalar_type<T>::construct(m_p + m_size, obj);
         m_size++;

         return true;
      }

      inline void push_back_value(T obj)
      {
         if (m_size >= m_capacity)
            increase_capacity(m_size + 1, true);

         scalar_type<T>::construct(m_p + m_size, obj);
         m_size++;
      }

      inline void pop_back()
      {
         assert(m_size);

         if (m_size)
         {
            m_size--;
            scalar_type<T>::destruct(&m_p[m_size]);
         }
      }

      inline void insert(uint32_t index, const T* p, uint32_t n)
      {
         assert(index <= m_size);
         if (!n)
            return;

         const uint32_t orig_size = m_size;
         resize(m_size + n, true);

         const uint32_t num_to_move = orig_size - index;

         if (BASISU_IS_BITWISE_COPYABLE(T))
         {
            // This overwrites the destination object bits, but bitwise copyable means we don't need to worry about destruction.
            memmove(m_p + index + n, m_p + index, sizeof(T) * num_to_move);
         }
         else
         {
            const T* pSrc = m_p + orig_size - 1;
            T* pDst = const_cast<T*>(pSrc) + n;

            for (uint32_t i = 0; i < num_to_move; i++)
            {
               assert((pDst - m_p) < (int)m_size);
               *pDst-- = *pSrc--;
            }
         }

         T* pDst = m_p + index;

         if (BASISU_IS_BITWISE_COPYABLE(T))
         {
            // This copies in the new bits, overwriting the existing objects, which is OK for copyable types that don't need destruction.
            memcpy(pDst, p, sizeof(T) * n);
         }
         else
         {
            for (uint32_t i = 0; i < n; i++)
            {
               assert((pDst - m_p) < (int)m_size);
               *pDst++ = *p++;
            }
         }
      }

      inline void insert(T* p, const T& obj)
      {
         int64_t ofs = p - begin();
         if ((ofs < 0) || (ofs > UINT32_MAX))
         {
            assert(0);
            return;
         }

         insert((uint32_t)ofs, &obj, 1);
      }

      // push_front() isn't going to be very fast - it's only here for usability.
      inline void push_front(const T& obj)
      {
         insert(0, &obj, 1);
      }

      vector& append(const vector& other)
      {
         if (other.m_size)
            insert(m_size, &other[0], other.m_size);
         return *this;
      }

      vector& append(const T* p, uint32_t n)
      {
         if (n)
            insert(m_size, p, n);
         return *this;
      }
            
      inline void erase(uint32_t start, uint32_t n)
      {
         assert((start + n) <= m_size);
         if ((start + n) > m_size)
            return;

         if (!n)
            return;

         const uint32_t num_to_move = m_size - (start + n);

         T* pDst = m_p + start;

         const T* pSrc = m_p + start + n;

         if (BASISU_IS_BITWISE_COPYABLE_OR_MOVABLE(T))
         {
            // This test is overly cautious.
            if ((!BASISU_IS_BITWISE_COPYABLE(T)) || (BASISU_HAS_DESTRUCTOR(T)))
            {
               // Type has been marked explictly as bitwise movable, which means we can move them around but they may need to be destructed.
               // First destroy the erased objects.
               scalar_type<T>::destruct_array(pDst, n);
            }

            // Copy "down" the objects to preserve, filling in the empty slots.

#ifndef __EMSCRIPTEN__
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"            
#endif
#endif

            memmove(pDst, pSrc, num_to_move * sizeof(T));

#ifndef __EMSCRIPTEN__
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif            
#endif
         }
         else
         {
            // Type is not bitwise copyable or movable. 
            // Move them down one at a time by using the equals operator, and destroying anything that's left over at the end.
            T* pDst_end = pDst + num_to_move;
            while (pDst != pDst_end)
               *pDst++ = *pSrc++;

            scalar_type<T>::destruct_array(pDst_end, n);
         }

         m_size -= n;
      }

      inline void erase(uint32_t index)
      {
         erase(index, 1);
      }

      inline void erase(T* p)
      {
         assert((p >= m_p) && (p < (m_p + m_size)));
         erase(static_cast<uint32_t>(p - m_p));
      }

      inline void erase(T *pFirst, T *pEnd)
      {
         assert(pFirst <= pEnd);
         assert(pFirst >= begin() && pFirst <= end());
         assert(pEnd >= begin() && pEnd <= end());

         int64_t ofs = pFirst - begin();
         if ((ofs < 0) || (ofs > UINT32_MAX))
         {
            assert(0);
            return;
         }

         int64_t n = pEnd - pFirst;
         if ((n < 0) || (n > UINT32_MAX))
         {
            assert(0);
            return;
         }

         erase((uint32_t)ofs, (uint32_t)n);
      }

      void erase_unordered(uint32_t index)
      {
         assert(index < m_size);

         if ((index + 1) < m_size)
            (*this)[index] = back();

         pop_back();
      }

      inline bool operator== (const vector& rhs) const
      {
         if (m_size != rhs.m_size)
            return false;
         else if (m_size)
         {
            if (scalar_type<T>::cFlag)
               return memcmp(m_p, rhs.m_p, sizeof(T) * m_size) == 0;
            else
            {
               const T* pSrc = m_p;
               const T* pDst = rhs.m_p;
               for (uint32_t i = m_size; i; i--)
                  if (!(*pSrc++ == *pDst++))
                     return false;
            }
         }

         return true;
      }

      inline bool operator< (const vector& rhs) const
      {
         const uint32_t min_size = helpers::minimum(m_size, rhs.m_size);

         const T* pSrc = m_p;
         const T* pSrc_end = m_p + min_size;
         const T* pDst = rhs.m_p;

         while ((pSrc < pSrc_end) && (*pSrc == *pDst))
         {
            pSrc++;
            pDst++;
         }

         if (pSrc < pSrc_end)
            return *pSrc < *pDst;

         return m_size < rhs.m_size;
      }

      inline void swap(vector& other)
      {
         std::swap(m_p, other.m_p);
         std::swap(m_size, other.m_size);
         std::swap(m_capacity, other.m_capacity);
      }

      inline void sort()
      {
         std::sort(begin(), end());
      }

      inline void unique()
      {
         if (!empty())
         {
            sort();

            resize(std::unique(begin(), end()) - begin());
         }
      }

      inline void reverse()
      {
         uint32_t j = m_size >> 1;
         for (uint32_t i = 0; i < j; i++)
            std::swap(m_p[i], m_p[m_size - 1 - i]);
      }

      inline int find(const T& key) const
      {
         const T* p = m_p;
         const T* p_end = m_p + m_size;

         uint32_t index = 0;

         while (p != p_end)
         {
            if (key == *p)
               return index;

            p++;
            index++;
         }

         return cInvalidIndex;
      }

      inline int find_sorted(const T& key) const
      {
         if (m_size)
         {
            // Uniform binary search - Knuth Algorithm 6.2.1 U, unrolled twice.
            int i = ((m_size + 1) >> 1) - 1;
            int m = m_size;

            for (; ; )
            {
               assert(i >= 0 && i < (int)m_size);
               const T* pKey_i = m_p + i;
               int cmp = key < *pKey_i;
#if defined(_DEBUG) || defined(DEBUG)
               int cmp2 = *pKey_i < key;
               assert((cmp != cmp2) || (key == *pKey_i));
#endif
               if ((!cmp) && (key == *pKey_i)) return i;
               m >>= 1;
               if (!m) break;
               cmp = -cmp;
               i += (((m + 1) >> 1) ^ cmp) - cmp;
               if (i < 0)
                  break;

               assert(i >= 0 && i < (int)m_size);
               pKey_i = m_p + i;
               cmp = key < *pKey_i;
#if defined(_DEBUG) || defined(DEBUG)
               cmp2 = *pKey_i < key;
               assert((cmp != cmp2) || (key == *pKey_i));
#endif
               if ((!cmp) && (key == *pKey_i)) return i;
               m >>= 1;
               if (!m) break;
               cmp = -cmp;
               i += (((m + 1) >> 1) ^ cmp) - cmp;
               if (i < 0)
                  break;
            }
         }

         return cInvalidIndex;
      }

      template<typename Q>
      inline int find_sorted(const T& key, Q less_than) const
      {
         if (m_size)
         {
            // Uniform binary search - Knuth Algorithm 6.2.1 U, unrolled twice.
            int i = ((m_size + 1) >> 1) - 1;
            int m = m_size;

            for (; ; )
            {
               assert(i >= 0 && i < (int)m_size);
               const T* pKey_i = m_p + i;
               int cmp = less_than(key, *pKey_i);
               if ((!cmp) && (!less_than(*pKey_i, key))) return i;
               m >>= 1;
               if (!m) break;
               cmp = -cmp;
               i += (((m + 1) >> 1) ^ cmp) - cmp;
               if (i < 0)
                  break;

               assert(i >= 0 && i < (int)m_size);
               pKey_i = m_p + i;
               cmp = less_than(key, *pKey_i);
               if ((!cmp) && (!less_than(*pKey_i, key))) return i;
               m >>= 1;
               if (!m) break;
               cmp = -cmp;
               i += (((m + 1) >> 1) ^ cmp) - cmp;
               if (i < 0) 
                  break;
            }
         }

         return cInvalidIndex;
      }

      inline uint32_t count_occurences(const T& key) const
      {
         uint32_t c = 0;

         const T* p = m_p;
         const T* p_end = m_p + m_size;

         while (p != p_end)
         {
            if (key == *p)
               c++;

            p++;
         }

         return c;
      }

      inline void set_all(const T& o)
      {
         if ((sizeof(T) == 1) && (scalar_type<T>::cFlag))
         {
#ifndef __EMSCRIPTEN__
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"            
#endif              
#endif
            memset(m_p, *reinterpret_cast<const uint8_t*>(&o), m_size);

#ifndef __EMSCRIPTEN__            
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif                        
#endif
         }
         else
         {
            T* pDst = m_p;
            T* pDst_end = pDst + m_size;
            while (pDst != pDst_end)
               *pDst++ = o;
         }
      }

      // Caller assumes ownership of the heap block associated with the container. Container is cleared.
      inline void* assume_ownership()
      {
         T* p = m_p;
         m_p = NULL;
         m_size = 0;
         m_capacity = 0;
         return p;
      }

      // Caller is granting ownership of the indicated heap block.
      // Block must have size constructed elements, and have enough room for capacity elements.
      // The block must have been allocated using malloc().
      // Important: This method is used in Basis Universal. If you change how this container allocates memory, you'll need to change any users of this method.
      inline bool grant_ownership(T* p, uint32_t size, uint32_t capacity)
      {
         // To prevent the caller from obviously shooting themselves in the foot.
         if (((p + capacity) > m_p) && (p < (m_p + m_capacity)))
         {
            // Can grant ownership of a block inside the container itself!
            assert(0);
            return false;
         }

         if (size > capacity)
         {
            assert(0);
            return false;
         }

         if (!p)
         {
            if (capacity)
            {
               assert(0);
               return false;
            }
         }
         else if (!capacity)
         {
            assert(0);
            return false;
         }

         clear();
         m_p = p;
         m_size = size;
         m_capacity = capacity;
         return true;
      }

   private:
      T* m_p;
      uint32_t m_size;
      uint32_t m_capacity;

      template<typename Q> struct is_vector { enum { cFlag = false }; };
      template<typename Q> struct is_vector< vector<Q> > { enum { cFlag = true }; };

      static void object_mover(void* pDst_void, void* pSrc_void, uint32_t num)
      {
         T* pSrc = static_cast<T*>(pSrc_void);
         T* const pSrc_end = pSrc + num;
         T* pDst = static_cast<T*>(pDst_void);

         while (pSrc != pSrc_end)
         {
            // placement new
            new (static_cast<void*>(pDst)) T(*pSrc);
            pSrc->~T();
            ++pSrc;
            ++pDst;
         }
      }

      inline bool increase_capacity(uint32_t min_new_capacity, bool grow_hint, bool nofail = false)
      {
         return reinterpret_cast<elemental_vector*>(this)->increase_capacity(
            min_new_capacity, grow_hint, sizeof(T),
            (BASISU_IS_BITWISE_COPYABLE_OR_MOVABLE(T) || (is_vector<T>::cFlag)) ? NULL : object_mover, nofail);
      }
   };

   template<typename T> struct bitwise_movable< vector<T> > { enum { cFlag = true }; };

   // Hash map

   template <typename T>
   struct hasher
   {
      inline size_t operator() (const T& key) const { return static_cast<size_t>(key); }
   };

   template <typename T>
   struct equal_to
   {
      inline bool operator()(const T& a, const T& b) const { return a == b; }
   };

   // Important: The Hasher and Equals objects must be bitwise movable!
   template<typename Key, typename Value = empty_type, typename Hasher = hasher<Key>, typename Equals = equal_to<Key> >
   class hash_map
   {
   public:
      class iterator;
      class const_iterator;
   
   private:
      friend class iterator;
      friend class const_iterator;

      enum state
      {
         cStateInvalid = 0,
         cStateValid = 1
      };

      enum
      {
         cMinHashSize = 4U
      };

   public:
      typedef hash_map<Key, Value, Hasher, Equals> hash_map_type;
      typedef std::pair<Key, Value> value_type;
      typedef Key                   key_type;
      typedef Value                 referent_type;
      typedef Hasher                hasher_type;
      typedef Equals                equals_type;

      hash_map() :
         m_hash_shift(32), m_num_valid(0), m_grow_threshold(0)
      {
      }

      hash_map(const hash_map& other) :
         m_values(other.m_values),
         m_hash_shift(other.m_hash_shift),
         m_hasher(other.m_hasher),
         m_equals(other.m_equals),
         m_num_valid(other.m_num_valid),
         m_grow_threshold(other.m_grow_threshold)
      {
      }

      hash_map& operator= (const hash_map& other)
      {
         if (this == &other)
            return *this;

         clear();

         m_values = other.m_values;
         m_hash_shift = other.m_hash_shift;
         m_num_valid = other.m_num_valid;
         m_grow_threshold = other.m_grow_threshold;
         m_hasher = other.m_hasher;
         m_equals = other.m_equals;

         return *this;
      }

      inline ~hash_map()
      {
         clear();
      }

      const Equals& get_equals() const { return m_equals; }
      Equals& get_equals() { return m_equals; }

      void set_equals(const Equals& equals) { m_equals = equals; }

      const Hasher& get_hasher() const { return m_hasher; }
      Hasher& get_hasher() { return m_hasher; }

      void set_hasher(const Hasher& hasher) { m_hasher = hasher; }

      inline void clear()
      {
         if (!m_values.empty())
         {
            if (BASISU_HAS_DESTRUCTOR(Key) || BASISU_HAS_DESTRUCTOR(Value))
            {
               node* p = &get_node(0);
               node* p_end = p + m_values.size();

               uint32_t num_remaining = m_num_valid;
               while (p != p_end)
               {
                  if (p->state)
                  {
                     destruct_value_type(p);
                     num_remaining--;
                     if (!num_remaining)
                        break;
                  }

                  p++;
               }
            }

            m_values.clear_no_destruction();

            m_hash_shift = 32;
            m_num_valid = 0;
            m_grow_threshold = 0;
         }
      }

      inline void reset()
      {
         if (!m_num_valid)
            return;

         if (BASISU_HAS_DESTRUCTOR(Key) || BASISU_HAS_DESTRUCTOR(Value))
         {
            node* p = &get_node(0);
            node* p_end = p + m_values.size();

            uint32_t num_remaining = m_num_valid;
            while (p != p_end)
            {
               if (p->state)
               {
                  destruct_value_type(p);
                  p->state = cStateInvalid;

                  num_remaining--;
                  if (!num_remaining)
                     break;
               }

               p++;
            }
         }
         else if (sizeof(node) <= 32)
         {
            memset(&m_values[0], 0, m_values.size_in_bytes());
         }
         else
         {
            node* p = &get_node(0);
            node* p_end = p + m_values.size();

            uint32_t num_remaining = m_num_valid;
            while (p != p_end)
            {
               if (p->state)
               {
                  p->state = cStateInvalid;

                  num_remaining--;
                  if (!num_remaining)
                     break;
               }

               p++;
            }
         }

         m_num_valid = 0;
      }

      inline uint32_t size()
      {
         return m_num_valid;
      }

      inline uint32_t get_table_size()
      {
         return m_values.size();
      }

      inline bool empty()
      {
         return !m_num_valid;
      }

      inline void reserve(uint32_t new_capacity)
      {
         uint64_t new_hash_size = helpers::maximum(1U, new_capacity);

         new_hash_size = new_hash_size * 2ULL;

         if (!helpers::is_power_of_2(new_hash_size))
            new_hash_size = helpers::next_pow2(new_hash_size);

         new_hash_size = helpers::maximum<uint64_t>(cMinHashSize, new_hash_size);

         new_hash_size = helpers::minimum<uint64_t>(0x80000000UL, new_hash_size);

         if (new_hash_size > m_values.size())
            rehash((uint32_t)new_hash_size);
      }
            
      class iterator
      {
         friend class hash_map<Key, Value, Hasher, Equals>;
         friend class hash_map<Key, Value, Hasher, Equals>::const_iterator;

      public:
         inline iterator() : m_pTable(NULL), m_index(0) { }
         inline iterator(hash_map_type& table, uint32_t index) : m_pTable(&table), m_index(index) { }
         inline iterator(const iterator& other) : m_pTable(other.m_pTable), m_index(other.m_index) { }

         inline iterator& operator= (const iterator& other)
         {
            m_pTable = other.m_pTable;
            m_index = other.m_index;
            return *this;
         }

         // post-increment
         inline iterator operator++(int)
         {
            iterator result(*this);
            ++*this;
            return result;
         }

         // pre-increment
         inline iterator& operator++()
         {
            probe();
            return *this;
         }

         inline value_type& operator*() const { return *get_cur(); }
         inline value_type* operator->() const { return get_cur(); }

         inline bool operator == (const iterator& b) const { return (m_pTable == b.m_pTable) && (m_index == b.m_index); }
         inline bool operator != (const iterator& b) const { return !(*this == b); }
         inline bool operator == (const const_iterator& b) const { return (m_pTable == b.m_pTable) && (m_index == b.m_index); }
         inline bool operator != (const const_iterator& b) const { return !(*this == b); }

      private:
         hash_map_type* m_pTable;
         uint32_t m_index;

         inline value_type* get_cur() const
         {
            assert(m_pTable && (m_index < m_pTable->m_values.size()));
            assert(m_pTable->get_node_state(m_index) == cStateValid);

            return &m_pTable->get_node(m_index);
         }

         inline void probe()
         {
            assert(m_pTable);
            m_index = m_pTable->find_next(m_index);
         }
      };

      class const_iterator
      {
         friend class hash_map<Key, Value, Hasher, Equals>;
         friend class hash_map<Key, Value, Hasher, Equals>::iterator;

      public:
         inline const_iterator() : m_pTable(NULL), m_index(0) { }
         inline const_iterator(const hash_map_type& table, uint32_t index) : m_pTable(&table), m_index(index) { }
         inline const_iterator(const iterator& other) : m_pTable(other.m_pTable), m_index(other.m_index) { }
         inline const_iterator(const const_iterator& other) : m_pTable(other.m_pTable), m_index(other.m_index) { }

         inline const_iterator& operator= (const const_iterator& other)
         {
            m_pTable = other.m_pTable;
            m_index = other.m_index;
            return *this;
         }

         inline const_iterator& operator= (const iterator& other)
         {
            m_pTable = other.m_pTable;
            m_index = other.m_index;
            return *this;
         }

         // post-increment
         inline const_iterator operator++(int)
         {
            const_iterator result(*this);
            ++*this;
            return result;
         }

         // pre-increment
         inline const_iterator& operator++()
         {
            probe();
            return *this;
         }

         inline const value_type& operator*() const { return *get_cur(); }
         inline const value_type* operator->() const { return get_cur(); }

         inline bool operator == (const const_iterator& b) const { return (m_pTable == b.m_pTable) && (m_index == b.m_index); }
         inline bool operator != (const const_iterator& b) const { return !(*this == b); }
         inline bool operator == (const iterator& b) const { return (m_pTable == b.m_pTable) && (m_index == b.m_index); }
         inline bool operator != (const iterator& b) const { return !(*this == b); }

      private:
         const hash_map_type* m_pTable;
         uint32_t m_index;

         inline const value_type* get_cur() const
         {
            assert(m_pTable && (m_index < m_pTable->m_values.size()));
            assert(m_pTable->get_node_state(m_index) == cStateValid);

            return &m_pTable->get_node(m_index);
         }

         inline void probe()
         {
            assert(m_pTable);
            m_index = m_pTable->find_next(m_index);
         }
      };

      inline const_iterator begin() const
      {
         if (!m_num_valid)
            return end();

         return const_iterator(*this, find_next(UINT32_MAX));
      }

      inline const_iterator end() const
      {
         return const_iterator(*this, m_values.size());
      }

      inline iterator begin()
      {
         if (!m_num_valid)
            return end();

         return iterator(*this, find_next(UINT32_MAX));
      }

      inline iterator end()
      {
         return iterator(*this, m_values.size());
      }

      // insert_result.first will always point to inserted key/value (or the already existing key/value).
      // insert_resutt.second will be true if a new key/value was inserted, or false if the key already existed (in which case first will point to the already existing value).
      typedef std::pair<iterator, bool> insert_result;

      inline insert_result insert(const Key& k, const Value& v = Value())
      {
         insert_result result;
         if (!insert_no_grow(result, k, v))
         {
            grow();

            // This must succeed.
            if (!insert_no_grow(result, k, v))
            {
               fprintf(stderr, "insert() failed");
               abort();
            }
         }

         return result;
      }

      inline insert_result insert(const value_type& v)
      {
         return insert(v.first, v.second);
      }

      inline const_iterator find(const Key& k) const
      {
         return const_iterator(*this, find_index(k));
      }

      inline iterator find(const Key& k)
      {
         return iterator(*this, find_index(k));
      }

      inline bool erase(const Key& k)
      {
         uint32_t i = find_index(k);

         if (i >= m_values.size())
            return false;

         node* pDst = &get_node(i);
         destruct_value_type(pDst);
         pDst->state = cStateInvalid;

         m_num_valid--;

         for (; ; )
         {
            uint32_t r, j = i;

            node* pSrc = pDst;

            do
            {
               if (!i)
               {
                  i = m_values.size() - 1;
                  pSrc = &get_node(i);
               }
               else
               {
                  i--;
                  pSrc--;
               }

               if (!pSrc->state)
                  return true;

               r = hash_key(pSrc->first);

            } while ((i <= r && r < j) || (r < j && j < i) || (j < i && i <= r));

            move_node(pDst, pSrc);

            pDst = pSrc;
         }
      }

      inline void swap(hash_map_type& other)
      {
         m_values.swap(other.m_values);
         std::swap(m_hash_shift, other.m_hash_shift);
         std::swap(m_num_valid, other.m_num_valid);
         std::swap(m_grow_threshold, other.m_grow_threshold);
         std::swap(m_hasher, other.m_hasher);
         std::swap(m_equals, other.m_equals);
      }

   private:
      struct node : public value_type
      {
         uint8_t state;
      };

      static inline void construct_value_type(value_type* pDst, const Key& k, const Value& v)
      {
         if (BASISU_IS_BITWISE_COPYABLE(Key))
            memcpy(&pDst->first, &k, sizeof(Key));
         else
            scalar_type<Key>::construct(&pDst->first, k);

         if (BASISU_IS_BITWISE_COPYABLE(Value))
            memcpy(&pDst->second, &v, sizeof(Value));
         else
            scalar_type<Value>::construct(&pDst->second, v);
      }

      static inline void construct_value_type(value_type* pDst, const value_type* pSrc)
      {
         if ((BASISU_IS_BITWISE_COPYABLE(Key)) && (BASISU_IS_BITWISE_COPYABLE(Value)))
         {
            memcpy(pDst, pSrc, sizeof(value_type));
         }
         else
         {
            if (BASISU_IS_BITWISE_COPYABLE(Key))
               memcpy(&pDst->first, &pSrc->first, sizeof(Key));
            else
               scalar_type<Key>::construct(&pDst->first, pSrc->first);

            if (BASISU_IS_BITWISE_COPYABLE(Value))
               memcpy(&pDst->second, &pSrc->second, sizeof(Value));
            else
               scalar_type<Value>::construct(&pDst->second, pSrc->second);
         }
      }

      static inline void destruct_value_type(value_type* p)
      {
         scalar_type<Key>::destruct(&p->first);
         scalar_type<Value>::destruct(&p->second);
      }

      // Moves *pSrc to *pDst efficiently.
      // pDst should NOT be constructed on entry.
      static inline void move_node(node* pDst, node* pSrc, bool update_src_state = true)
      {
         assert(!pDst->state);

         if (BASISU_IS_BITWISE_COPYABLE_OR_MOVABLE(Key) && BASISU_IS_BITWISE_COPYABLE_OR_MOVABLE(Value))
         {
            memcpy(pDst, pSrc, sizeof(node));
         }
         else
         {
            if (BASISU_IS_BITWISE_COPYABLE_OR_MOVABLE(Key))
               memcpy(&pDst->first, &pSrc->first, sizeof(Key));
            else
            {
               scalar_type<Key>::construct(&pDst->first, pSrc->first);
               scalar_type<Key>::destruct(&pSrc->first);
            }

            if (BASISU_IS_BITWISE_COPYABLE_OR_MOVABLE(Value))
               memcpy(&pDst->second, &pSrc->second, sizeof(Value));
            else
            {
               scalar_type<Value>::construct(&pDst->second, pSrc->second);
               scalar_type<Value>::destruct(&pSrc->second);
            }

            pDst->state = cStateValid;
         }

         if (update_src_state)
            pSrc->state = cStateInvalid;
      }

      struct raw_node
      {
         inline raw_node()
         {
            node* p = reinterpret_cast<node*>(this);
            p->state = cStateInvalid;
         }

         inline ~raw_node()
         {
            node* p = reinterpret_cast<node*>(this);
            if (p->state)
               hash_map_type::destruct_value_type(p);
         }

         inline raw_node(const raw_node& other)
         {
            node* pDst = reinterpret_cast<node*>(this);
            const node* pSrc = reinterpret_cast<const node*>(&other);

            if (pSrc->state)
            {
               hash_map_type::construct_value_type(pDst, pSrc);
               pDst->state = cStateValid;
            }
            else
               pDst->state = cStateInvalid;
         }

         inline raw_node& operator= (const raw_node& rhs)
         {
            if (this == &rhs)
               return *this;

            node* pDst = reinterpret_cast<node*>(this);
            const node* pSrc = reinterpret_cast<const node*>(&rhs);

            if (pSrc->state)
            {
               if (pDst->state)
               {
                  pDst->first = pSrc->first;
                  pDst->second = pSrc->second;
               }
               else
               {
                  hash_map_type::construct_value_type(pDst, pSrc);
                  pDst->state = cStateValid;
               }
            }
            else if (pDst->state)
            {
               hash_map_type::destruct_value_type(pDst);
               pDst->state = cStateInvalid;
            }

            return *this;
         }

         uint8_t m_bits[sizeof(node)];
      };

      typedef basisu::vector<raw_node> node_vector;

      node_vector    m_values;
      uint32_t       m_hash_shift;

      Hasher         m_hasher;
      Equals         m_equals;

      uint32_t       m_num_valid;

      uint32_t       m_grow_threshold;

      inline uint32_t hash_key(const Key& k) const
      {
         assert((1U << (32U - m_hash_shift)) == m_values.size());

         uint32_t hash = static_cast<uint32_t>(m_hasher(k));

         // Fibonacci hashing
         hash = (2654435769U * hash) >> m_hash_shift;

         assert(hash < m_values.size());
         return hash;
      }

      inline const node& get_node(uint32_t index) const
      {
         return *reinterpret_cast<const node*>(&m_values[index]);
      }

      inline node& get_node(uint32_t index)
      {
         return *reinterpret_cast<node*>(&m_values[index]);
      }

      inline state get_node_state(uint32_t index) const
      {
         return static_cast<state>(get_node(index).state);
      }

      inline void set_node_state(uint32_t index, bool valid)
      {
         get_node(index).state = valid;
      }

      inline void grow()
      {
         uint64_t n = m_values.size() * 3ULL; // was * 2
         
         if (!helpers::is_power_of_2(n))
            n = helpers::next_pow2(n);

         if (n > 0x80000000UL)
            n = 0x80000000UL;

         rehash(helpers::maximum<uint32_t>(cMinHashSize, (uint32_t)n));
      }

      inline void rehash(uint32_t new_hash_size)
      {
         assert(new_hash_size >= m_num_valid);
         assert(helpers::is_power_of_2(new_hash_size));

         if ((new_hash_size < m_num_valid) || (new_hash_size == m_values.size()))
            return;

         hash_map new_map;
         new_map.m_values.resize(new_hash_size);
         new_map.m_hash_shift = 32U - helpers::floor_log2i(new_hash_size);
         assert(new_hash_size == (1U << (32U - new_map.m_hash_shift)));
         new_map.m_grow_threshold = UINT_MAX;

         node* pNode = reinterpret_cast<node*>(m_values.begin());
         node* pNode_end = pNode + m_values.size();

         while (pNode != pNode_end)
         {
            if (pNode->state)
            {
               new_map.move_into(pNode);

               if (new_map.m_num_valid == m_num_valid)
                  break;
            }

            pNode++;
         }

         new_map.m_grow_threshold = (new_hash_size + 1U) >> 1U;

         m_values.clear_no_destruction();
         m_hash_shift = 32;

         swap(new_map);
      }

      inline uint32_t find_next(uint32_t index) const
      {
         index++;

         if (index >= m_values.size())
            return index;

         const node* pNode = &get_node(index);

         for (; ; )
         {
            if (pNode->state)
               break;

            if (++index >= m_values.size())
               break;

            pNode++;
         }

         return index;
      }

      inline uint32_t find_index(const Key& k) const
      {
         if (m_num_valid)
         {
            uint32_t index = hash_key(k);
            const node* pNode = &get_node(index);

            if (pNode->state)
            {
               if (m_equals(pNode->first, k))
                  return index;

               const uint32_t orig_index = index;

               for (; ; )
               {
                  if (!index)
                  {
                     index = m_values.size() - 1;
                     pNode = &get_node(index);
                  }
                  else
                  {
                     index--;
                     pNode--;
                  }

                  if (index == orig_index)
                     break;

                  if (!pNode->state)
                     break;

                  if (m_equals(pNode->first, k))
                     return index;
               }
            }
         }

         return m_values.size();
      }

      inline bool insert_no_grow(insert_result& result, const Key& k, const Value& v = Value())
      {
         if (!m_values.size())
            return false;

         uint32_t index = hash_key(k);
         node* pNode = &get_node(index);

         if (pNode->state)
         {
            if (m_equals(pNode->first, k))
            {
               result.first = iterator(*this, index);
               result.second = false;
               return true;
            }

            const uint32_t orig_index = index;

            for (; ; )
            {
               if (!index)
               {
                  index = m_values.size() - 1;
                  pNode = &get_node(index);
               }
               else
               {
                  index--;
                  pNode--;
               }

               if (orig_index == index)
                  return false;

               if (!pNode->state)
                  break;

               if (m_equals(pNode->first, k))
               {
                  result.first = iterator(*this, index);
                  result.second = false;
                  return true;
               }
            }
         }

         if (m_num_valid >= m_grow_threshold)
            return false;

         construct_value_type(pNode, k, v);

         pNode->state = cStateValid;

         m_num_valid++;
         assert(m_num_valid <= m_values.size());

         result.first = iterator(*this, index);
         result.second = true;

         return true;
      }

      inline void move_into(node* pNode)
      {
         uint32_t index = hash_key(pNode->first);
         node* pDst_node = &get_node(index);

         if (pDst_node->state)
         {
            const uint32_t orig_index = index;

            for (; ; )
            {
               if (!index)
               {
                  index = m_values.size() - 1;
                  pDst_node = &get_node(index);
               }
               else
               {
                  index--;
                  pDst_node--;
               }

               if (index == orig_index)
               {
                  assert(false);
                  return;
               }

               if (!pDst_node->state)
                  break;
            }
         }

         move_node(pDst_node, pNode, false);

         m_num_valid++;
      }
   };

   template<typename Key, typename Value, typename Hasher, typename Equals>
   struct bitwise_movable< hash_map<Key, Value, Hasher, Equals> > { enum { cFlag = true }; };
   
#if BASISU_HASHMAP_TEST
   extern void hash_map_test();
#endif
      
} // namespace basisu

namespace std
{
   template<typename T>
   inline void swap(basisu::vector<T>& a, basisu::vector<T>& b)
   {
      a.swap(b);
   }

   template<typename Key, typename Value, typename Hasher, typename Equals>
   inline void swap(basisu::hash_map<Key, Value, Hasher, Equals>& a, basisu::hash_map<Key, Value, Hasher, Equals>& b)
   {
      a.swap(b);
   }

} // namespace std
