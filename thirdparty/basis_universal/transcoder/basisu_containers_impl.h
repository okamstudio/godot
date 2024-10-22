// basisu_containers_impl.h
// Do not include directly

#ifdef _MSC_VER
#pragma warning (disable:4127) // warning C4127: conditional expression is constant
#endif

namespace basisu
{
   bool elemental_vector::increase_capacity(uint32_t min_new_capacity, bool grow_hint, uint32_t element_size, object_mover pMover, bool nofail)
   {
      assert(m_size <= m_capacity);

      if (sizeof(void *) == sizeof(uint64_t))
         assert(min_new_capacity < (0x400000000ULL / element_size));
      else
         assert(min_new_capacity < (0x7FFF0000U / element_size));

      if (m_capacity >= min_new_capacity)
         return true;

      uint64_t new_capacity_u64 = min_new_capacity;
      if ((grow_hint) && (!helpers::is_power_of_2(new_capacity_u64)))
          new_capacity_u64 = helpers::next_pow2(new_capacity_u64);

      size_t new_capacity = (size_t)new_capacity_u64;
      if (new_capacity != new_capacity_u64)
      {
          if (nofail)
              return false;
          fprintf(stderr, "elemental_vector::increase_capacity: vector too large\n");
          abort();
      }
            
      const uint64_t desired_size_u64 = (uint64_t)element_size * new_capacity;
            
      const size_t desired_size = (size_t)desired_size_u64;
      if (desired_size_u64 != desired_size)
      {
          if (nofail)
              return false;
          fprintf(stderr, "elemental_vector::increase_capacity: vector too large\n");
          abort();
      }

      size_t actual_size = 0;
      if (!pMover)
      {
         void* new_p = realloc(m_p, desired_size);
         if (!new_p)
         {
            if (nofail)
               return false;

            char buf[256];
            snprintf(buf, sizeof(buf), "elemental_vector::increase_capacity: realloc() failed allocating %zu bytes", desired_size);
            fprintf(stderr, "%s", buf);
            abort();
         }

#if BASISU_VECTOR_DETERMINISTIC
         actual_size = desired_size;
#elif defined(_MSC_VER)
         actual_size = _msize(new_p);
#elif HAS_MALLOC_USABLE_SIZE
         actual_size = malloc_usable_size(new_p);
#else
         actual_size = desired_size;
#endif
         m_p = new_p;
      }
      else
      {
         void* new_p = malloc(desired_size);
         if (!new_p)
         {
            if (nofail)
               return false;

            char buf[256];
            snprintf(buf, sizeof(buf), "elemental_vector::increase_capacity: malloc() failed allocating %zu bytes", desired_size);
            fprintf(stderr, "%s", buf);
            abort();
         }

#if BASISU_VECTOR_DETERMINISTIC
         actual_size = desired_size;
#elif defined(_MSC_VER)
         actual_size = _msize(new_p);
#elif HAS_MALLOC_USABLE_SIZE
         actual_size = malloc_usable_size(new_p);
#else
         actual_size = desired_size;
#endif

         (*pMover)(new_p, m_p, m_size);

         if (m_p)
            free(m_p);
         
         m_p = new_p;
      }

      if (actual_size > desired_size)
         m_capacity = static_cast<uint32_t>(actual_size / element_size);
      else
         m_capacity = static_cast<uint32_t>(new_capacity);

      return true;
   }

#if BASISU_HASHMAP_TEST

#define HASHMAP_TEST_VERIFY(c) do { if (!(c)) handle_hashmap_test_verify_failure(__LINE__); } while(0)

   static void handle_hashmap_test_verify_failure(int line)
   {
      fprintf(stderr, "HASHMAP_TEST_VERIFY() faild on line %i\n", line);
      abort();
   }

   class counted_obj
   {
   public:
      counted_obj(uint32_t v = 0) :
         m_val(v)
      {
         m_count++;
      }

      counted_obj(const counted_obj& obj) :
         m_val(obj.m_val)
      {
         m_count++;
      }

      ~counted_obj()
      {
         assert(m_count > 0);
         m_count--;
      }

      static uint32_t m_count;

      uint32_t m_val;

      operator size_t() const { return m_val; }

      bool operator== (const counted_obj& rhs) const { return m_val == rhs.m_val; }
      bool operator== (const uint32_t rhs) const { return m_val == rhs; }

   };

   uint32_t counted_obj::m_count;

   static uint32_t urand32()
   {
      uint32_t a = rand();
      uint32_t b = rand() << 15;
      uint32_t c = rand() << (32 - 15);
      return a ^ b ^ c;
   }

   static int irand32(int l, int h)
   {
      assert(l < h);
      if (l >= h)
         return l;

      uint32_t range = static_cast<uint32_t>(h - l);

      uint32_t rnd = urand32();

      uint32_t rnd_range = static_cast<uint32_t>((((uint64_t)range) * ((uint64_t)rnd)) >> 32U);

      int result = l + rnd_range;
      assert((result >= l) && (result < h));
      return result;
   }

   void hash_map_test()
   {
      {
         basisu::hash_map<uint64_t, uint64_t> k;
         basisu::hash_map<uint64_t, uint64_t> l;
         std::swap(k, l);

         k.begin();
         k.end();
         k.clear();
         k.empty();
         k.erase(0);
         k.insert(0, 1);
         k.find(0);
         k.get_equals();
         k.get_hasher();
         k.get_table_size();
         k.reset();
         k.reserve(1);
         k = l;
         k.set_equals(l.get_equals());
         k.set_hasher(l.get_hasher());
         k.get_table_size();
      }

      uint32_t seed = 0;
      for (; ; )
      {
         seed++;

         typedef basisu::hash_map<counted_obj, counted_obj> my_hash_map;
         my_hash_map m;

         const uint32_t n = irand32(0, 100000);

         printf("%u\n", n);

         srand(seed); // r1.seed(seed);

         basisu::vector<int> q;

         uint32_t count = 0;
         for (uint32_t i = 0; i < n; i++)
         {
            uint32_t v = urand32() & 0x7FFFFFFF;
            my_hash_map::insert_result res = m.insert(counted_obj(v), counted_obj(v ^ 0xdeadbeef));
            if (res.second)
            {
               count++;
               q.push_back(v);
            }
         }

         HASHMAP_TEST_VERIFY(m.size() == count);

         srand(seed);

         my_hash_map cm(m);
         m.clear();
         m = cm;
         cm.reset();

         for (uint32_t i = 0; i < n; i++)
         {
            uint32_t v = urand32() & 0x7FFFFFFF;
            my_hash_map::const_iterator it = m.find(counted_obj(v));
            HASHMAP_TEST_VERIFY(it != m.end());
            HASHMAP_TEST_VERIFY(it->first == v);
            HASHMAP_TEST_VERIFY(it->second == (v ^ 0xdeadbeef));
         }

         for (uint32_t t = 0; t < 2; t++)
         {
            const uint32_t nd = irand32(1, q.size() + 1);
            for (uint32_t i = 0; i < nd; i++)
            {
               uint32_t p = irand32(0, q.size());

               int k = q[p];
               if (k >= 0)
               {
                  q[p] = -k - 1;

                  bool s = m.erase(counted_obj(k));
                  HASHMAP_TEST_VERIFY(s);
               }
            }

            typedef basisu::hash_map<uint32_t, empty_type> uint_hash_set;
            uint_hash_set s;

            for (uint32_t i = 0; i < q.size(); i++)
            {
               int v = q[i];

               if (v >= 0)
               {
                  my_hash_map::const_iterator it = m.find(counted_obj(v));
                  HASHMAP_TEST_VERIFY(it != m.end());
                  HASHMAP_TEST_VERIFY(it->first == (uint32_t)v);
                  HASHMAP_TEST_VERIFY(it->second == ((uint32_t)v ^ 0xdeadbeef));

                  s.insert(v);
               }
               else
               {
                  my_hash_map::const_iterator it = m.find(counted_obj(-v - 1));
                  HASHMAP_TEST_VERIFY(it == m.end());
               }
            }

            uint32_t found_count = 0;
            for (my_hash_map::const_iterator it = m.begin(); it != m.end(); ++it)
            {
               HASHMAP_TEST_VERIFY(it->second == ((uint32_t)it->first ^ 0xdeadbeef));

               uint_hash_set::const_iterator fit(s.find((uint32_t)it->first));
               HASHMAP_TEST_VERIFY(fit != s.end());

               HASHMAP_TEST_VERIFY(fit->first == it->first);

               found_count++;
            }

            HASHMAP_TEST_VERIFY(found_count == s.size());
         }

         HASHMAP_TEST_VERIFY(counted_obj::m_count == m.size() * 2);
      }
   }

#endif // BASISU_HASHMAP_TEST

} // namespace basisu
