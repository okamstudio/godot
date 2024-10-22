// basisu_kernels_imp.h - Do not directly include
// Copyright (C) 2019-2024 Binomial LLC. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

using namespace CPPSPMD;

namespace CPPSPMD_NAME(basisu_kernels_namespace)
{
   struct perceptual_distance_rgb_4_N : spmd_kernel
   {
      void _call(int64_t* pDistance,
         const uint8_t* pSelectors,
         const color_rgba* pBlock_colors,
         const color_rgba* pSrc_pixels, uint32_t n, 
         int64_t early_out_err)
      {
         assert(early_out_err >= 0);

         *pDistance = 0;

         __m128i block_colors[4];
         vint block_colors_r[4], block_colors_g[4], block_colors_b[4];
         for (uint32_t i = 0; i < 4; i++)
         {
            block_colors[i] = load_rgba32(&pBlock_colors[i]);
            store_all(block_colors_r[i], (int)pBlock_colors[i].r);
            store_all(block_colors_g[i], (int)pBlock_colors[i].g);
            store_all(block_colors_b[i], (int)pBlock_colors[i].b);
         }

         uint32_t i;
         for (i = 0; (i + 4) <= n; i += 4)
         {
            __m128i c0 = load_rgba32(&pSrc_pixels[i + 0]), c1 = load_rgba32(&pSrc_pixels[i + 1]), c2 = load_rgba32(&pSrc_pixels[i + 2]), c3 = load_rgba32(&pSrc_pixels[i + 3]);

            vint r, g, b, a;
            transpose4x4(r.m_value, g.m_value, b.m_value, a.m_value, c0, c1, c2, c3);

            int s0 = pSelectors[i], s1 = pSelectors[i + 1], s2 = pSelectors[i + 2], s3 = pSelectors[i + 3];

            vint base_r, base_g, base_b, base_a;
            if ((s0 == s1) && (s0 == s2) && (s0 == s3))
            {
               store_all(base_r, block_colors_r[s0]);
               store_all(base_g, block_colors_g[s0]);
               store_all(base_b, block_colors_b[s0]);
            }
            else
            {
               __m128i k0 = block_colors[s0], k1 = block_colors[s1], k2 = block_colors[s2], k3 = block_colors[s3];
               transpose4x4(base_r.m_value, base_g.m_value, base_b.m_value, base_a.m_value, k0, k1, k2, k3);
            }

            vint dr = base_r - r;
            vint dg = base_g - g;
            vint db = base_b - b;

            vint delta_l = dr * 27 + dg * 92 + db * 9;
            vint delta_cr = dr * 128 - delta_l;
            vint delta_cb = db * 128 - delta_l;

            vint id = ((delta_l * delta_l) >> 7) +
               ((((delta_cr * delta_cr) >> 7) * 26) >> 7) +
               ((((delta_cb * delta_cb) >> 7) * 3) >> 7);

            *pDistance += reduce_add(id);
            if (*pDistance >= early_out_err)
               return;
         }

         for (; i < n; i++)
         {
            int r = pSrc_pixels[i].r, g = pSrc_pixels[i].g, b = pSrc_pixels[i].b;

            int sel = pSelectors[i];
            int base_r = pBlock_colors[sel].r, base_g = pBlock_colors[sel].g, base_b = pBlock_colors[sel].b;

            int dr = base_r - r;
            int dg = base_g - g;
            int db = base_b - b;

            int delta_l = dr * 27 + dg * 92 + db * 9;
            int delta_cr = dr * 128 - delta_l;
            int delta_cb = db * 128 - delta_l;

            int id = ((delta_l * delta_l) >> 7) +
               ((((delta_cr * delta_cr) >> 7) * 26) >> 7) +
               ((((delta_cb * delta_cb) >> 7) * 3) >> 7);

            *pDistance += id;
            if (*pDistance >= early_out_err)
               return;
         }
      }
   };

   struct linear_distance_rgb_4_N : spmd_kernel
   {
      void _call(int64_t* pDistance,
         const uint8_t* pSelectors,
         const color_rgba* pBlock_colors,
         const color_rgba* pSrc_pixels, uint32_t n, 
         int64_t early_out_err)
      {
         assert(early_out_err >= 0);

         *pDistance = 0;

         __m128i block_colors[4];
         vint block_colors_r[4], block_colors_g[4], block_colors_b[4];
         for (uint32_t i = 0; i < 4; i++)
         {
            block_colors[i] = load_rgba32(&pBlock_colors[i]);
            store_all(block_colors_r[i], (int)pBlock_colors[i].r);
            store_all(block_colors_g[i], (int)pBlock_colors[i].g);
            store_all(block_colors_b[i], (int)pBlock_colors[i].b);
         }

         uint32_t i;
         for (i = 0; (i + 4) <= n; i += 4)
         {
            __m128i c0 = load_rgba32(&pSrc_pixels[i + 0]), c1 = load_rgba32(&pSrc_pixels[i + 1]), c2 = load_rgba32(&pSrc_pixels[i + 2]), c3 = load_rgba32(&pSrc_pixels[i + 3]);

            vint r, g, b, a;
            transpose4x4(r.m_value, g.m_value, b.m_value, a.m_value, c0, c1, c2, c3);

            int s0 = pSelectors[i], s1 = pSelectors[i + 1], s2 = pSelectors[i + 2], s3 = pSelectors[i + 3];

            vint base_r, base_g, base_b, base_a;
            if ((s0 == s1) && (s0 == s2) && (s0 == s3))
            {
               store_all(base_r, block_colors_r[s0]);
               store_all(base_g, block_colors_g[s0]);
               store_all(base_b, block_colors_b[s0]);
            }
            else
            {
               __m128i k0 = block_colors[s0], k1 = block_colors[s1], k2 = block_colors[s2], k3 = block_colors[s3];
               transpose4x4(base_r.m_value, base_g.m_value, base_b.m_value, base_a.m_value, k0, k1, k2, k3);
            }

            vint dr = base_r - r;
            vint dg = base_g - g;
            vint db = base_b - b;

            vint id = dr * dr + dg * dg + db * db;

            *pDistance += reduce_add(id);
            if (*pDistance >= early_out_err)
               return;
         }

         for (; i < n; i++)
         {
            int r = pSrc_pixels[i].r, g = pSrc_pixels[i].g, b = pSrc_pixels[i].b;

            int sel = pSelectors[i];
            int base_r = pBlock_colors[sel].r, base_g = pBlock_colors[sel].g, base_b = pBlock_colors[sel].b;

            int dr = base_r - r;
            int dg = base_g - g;
            int db = base_b - b;

            int id = dr * dr + dg * dg + db * db;

            *pDistance += id;
            if (*pDistance >= early_out_err)
               return;
         }
      }
   };

   struct find_selectors_perceptual_rgb_4_N : spmd_kernel
   {
      inline vint compute_dist(
         const vint& base_r, const vint& base_g, const vint& base_b,
         const vint& r, const vint& g, const vint& b)
      {
         vint dr = base_r - r;
         vint dg = base_g - g;
         vint db = base_b - b;

         vint delta_l = dr * 27 + dg * 92 + db * 9;
         vint delta_cr = dr * 128 - delta_l;
         vint delta_cb = db * 128 - delta_l;

         vint id = VINT_SHIFT_RIGHT(delta_l * delta_l, 7) +
            VINT_SHIFT_RIGHT(VINT_SHIFT_RIGHT(delta_cr * delta_cr, 7) * 26, 7) +
            VINT_SHIFT_RIGHT(VINT_SHIFT_RIGHT(delta_cb * delta_cb, 7) * 3, 7);

         return id;
      }

      void _call(int64_t* pDistance,
         uint8_t* pSelectors,
         const color_rgba* pBlock_colors,
         const color_rgba* pSrc_pixels, uint32_t n, 
         int64_t early_out_err)
      {
         assert(early_out_err >= 0);

         *pDistance = 0;

         vint block_colors_r[4], block_colors_g[4], block_colors_b[4];
         for (uint32_t i = 0; i < 4; i++)
         {
            store_all(block_colors_r[i], (int)pBlock_colors[i].r);
            store_all(block_colors_g[i], (int)pBlock_colors[i].g);
            store_all(block_colors_b[i], (int)pBlock_colors[i].b);
         }

         const __m128i shuf = _mm_set_epi8(-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 12, 8, 4, 0);

         uint32_t i;

         for (i = 0; (i + 4) <= n; i += 4)
         {
            __m128i c0 = load_rgba32(&pSrc_pixels[i + 0]), c1 = load_rgba32(&pSrc_pixels[i + 1]), c2 = load_rgba32(&pSrc_pixels[i + 2]), c3 = load_rgba32(&pSrc_pixels[i + 3]);

            vint r, g, b, a;
            transpose4x4(r.m_value, g.m_value, b.m_value, a.m_value, c0, c1, c2, c3);

            vint dist0 = compute_dist(block_colors_r[0], block_colors_g[0], block_colors_b[0], r, g, b);
            vint dist1 = compute_dist(block_colors_r[1], block_colors_g[1], block_colors_b[1], r, g, b);
            vint dist2 = compute_dist(block_colors_r[2], block_colors_g[2], block_colors_b[2], r, g, b);
            vint dist3 = compute_dist(block_colors_r[3], block_colors_g[3], block_colors_b[3], r, g, b);

            vint min_dist = min(min(min(dist0, dist1), dist2), dist3);

            vint sels = spmd_ternaryi(min_dist == dist0, 0, spmd_ternaryi(min_dist == dist1, 1, spmd_ternaryi(min_dist == dist2, 2, 3)));

            __m128i vsels = shuffle_epi8(sels.m_value, shuf);
            storeu_si32((void *)(pSelectors + i), vsels);

            *pDistance += reduce_add(min_dist);
            if (*pDistance >= early_out_err)
               return;
         }

         for (; i < n; i++)
         {
            int r = pSrc_pixels[i].r, g = pSrc_pixels[i].g, b = pSrc_pixels[i].b;

            int best_err = INT_MAX, best_sel = 0;
            for (int sel = 0; sel < 4; sel++)
            {
               int base_r = pBlock_colors[sel].r, base_g = pBlock_colors[sel].g, base_b = pBlock_colors[sel].b;

               int dr = base_r - r;
               int dg = base_g - g;
               int db = base_b - b;

               int delta_l = dr * 27 + dg * 92 + db * 9;
               int delta_cr = dr * 128 - delta_l;
               int delta_cb = db * 128 - delta_l;

               int id = ((delta_l * delta_l) >> 7) +
                  ((((delta_cr * delta_cr) >> 7) * 26) >> 7) +
                  ((((delta_cb * delta_cb) >> 7) * 3) >> 7);
               if (id < best_err)
               {
                  best_err = id;
                  best_sel = sel;
               }
            }

            pSelectors[i] = (uint8_t)best_sel;

            *pDistance += best_err;
            if (*pDistance >= early_out_err)
               return;
         }
      }
   };

   struct find_selectors_linear_rgb_4_N : spmd_kernel
   {
      inline vint compute_dist(
         const vint& base_r, const vint& base_g, const vint& base_b,
         const vint& r, const vint& g, const vint& b)
      {
         vint dr = base_r - r;
         vint dg = base_g - g;
         vint db = base_b - b;

         vint id = dr * dr + dg * dg + db * db;
         return id;
      }

      void _call(int64_t* pDistance,
         uint8_t* pSelectors,
         const color_rgba* pBlock_colors,
         const color_rgba* pSrc_pixels, uint32_t n, 
         int64_t early_out_err)
      {
         assert(early_out_err >= 0);

         *pDistance = 0;

         vint block_colors_r[4], block_colors_g[4], block_colors_b[4];
         for (uint32_t i = 0; i < 4; i++)
         {
            store_all(block_colors_r[i], (int)pBlock_colors[i].r);
            store_all(block_colors_g[i], (int)pBlock_colors[i].g);
            store_all(block_colors_b[i], (int)pBlock_colors[i].b);
         }

         const __m128i shuf = _mm_set_epi8(-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 12, 8, 4, 0);

         uint32_t i;

         for (i = 0; (i + 4) <= n; i += 4)
         {
            __m128i c0 = load_rgba32(&pSrc_pixels[i + 0]), c1 = load_rgba32(&pSrc_pixels[i + 1]), c2 = load_rgba32(&pSrc_pixels[i + 2]), c3 = load_rgba32(&pSrc_pixels[i + 3]);

            vint r, g, b, a;
            transpose4x4(r.m_value, g.m_value, b.m_value, a.m_value, c0, c1, c2, c3);

            vint dist0 = compute_dist(block_colors_r[0], block_colors_g[0], block_colors_b[0], r, g, b);
            vint dist1 = compute_dist(block_colors_r[1], block_colors_g[1], block_colors_b[1], r, g, b);
            vint dist2 = compute_dist(block_colors_r[2], block_colors_g[2], block_colors_b[2], r, g, b);
            vint dist3 = compute_dist(block_colors_r[3], block_colors_g[3], block_colors_b[3], r, g, b);

            vint min_dist = min(min(min(dist0, dist1), dist2), dist3);

            vint sels = spmd_ternaryi(min_dist == dist0, 0, spmd_ternaryi(min_dist == dist1, 1, spmd_ternaryi(min_dist == dist2, 2, 3)));

            __m128i vsels = shuffle_epi8(sels.m_value, shuf);
            storeu_si32((void *)(pSelectors + i), vsels);

            *pDistance += reduce_add(min_dist);
            if (*pDistance >= early_out_err)
               return;
         }

         for (; i < n; i++)
         {
            int r = pSrc_pixels[i].r, g = pSrc_pixels[i].g, b = pSrc_pixels[i].b;

            int best_err = INT_MAX, best_sel = 0;
            for (int sel = 0; sel < 4; sel++)
            {
               int base_r = pBlock_colors[sel].r, base_g = pBlock_colors[sel].g, base_b = pBlock_colors[sel].b;

               int dr = base_r - r;
               int dg = base_g - g;
               int db = base_b - b;

               int id = dr * dr + dg * dg + db * db;
               if (id < best_err)
               {
                  best_err = id;
                  best_sel = sel;
               }
            }

            pSelectors[i] = (uint8_t)best_sel;

            *pDistance += best_err;
            if (*pDistance >= early_out_err)
               return;
         }
      }
   };

   struct find_lowest_error_perceptual_rgb_4_N : spmd_kernel
   {
      inline vint compute_dist(
         const vint& base_r, const vint& base_g, const vint& base_b,
         const vint& r, const vint& g, const vint& b)
      {
         vint dr = base_r - r;
         vint dg = base_g - g;
         vint db = base_b - b;

         vint delta_l = dr * 27 + dg * 92 + db * 9;
         vint delta_cr = dr * 128 - delta_l;
         vint delta_cb = db * 128 - delta_l;

         vint id = VINT_SHIFT_RIGHT(delta_l * delta_l, 7) +
            VINT_SHIFT_RIGHT(VINT_SHIFT_RIGHT(delta_cr * delta_cr, 7) * 26, 7) +
            VINT_SHIFT_RIGHT(VINT_SHIFT_RIGHT(delta_cb * delta_cb, 7) * 3, 7);

         return id;
      }

      void _call(int64_t* pDistance,
         const color_rgba* pBlock_colors,
         const color_rgba* pSrc_pixels, uint32_t n, 
         int64_t early_out_error)
      {
         assert(early_out_error >= 0);

         *pDistance = 0;

         vint block_colors_r[4], block_colors_g[4], block_colors_b[4];
         for (uint32_t i = 0; i < 4; i++)
         {
            store_all(block_colors_r[i], (int)pBlock_colors[i].r);
            store_all(block_colors_g[i], (int)pBlock_colors[i].g);
            store_all(block_colors_b[i], (int)pBlock_colors[i].b);
         }

         uint32_t i;

         for (i = 0; (i + 4) <= n; i += 4)
         {
            __m128i c0 = load_rgba32(&pSrc_pixels[i + 0]), c1 = load_rgba32(&pSrc_pixels[i + 1]), c2 = load_rgba32(&pSrc_pixels[i + 2]), c3 = load_rgba32(&pSrc_pixels[i + 3]);

            vint r, g, b, a;
            transpose4x4(r.m_value, g.m_value, b.m_value, a.m_value, c0, c1, c2, c3);

            vint dist0 = compute_dist(block_colors_r[0], block_colors_g[0], block_colors_b[0], r, g, b);
            vint dist1 = compute_dist(block_colors_r[1], block_colors_g[1], block_colors_b[1], r, g, b);
            vint dist2 = compute_dist(block_colors_r[2], block_colors_g[2], block_colors_b[2], r, g, b);
            vint dist3 = compute_dist(block_colors_r[3], block_colors_g[3], block_colors_b[3], r, g, b);

            vint min_dist = min(min(min(dist0, dist1), dist2), dist3);

            *pDistance += reduce_add(min_dist);
            if (*pDistance > early_out_error)
               return;
         }

         for (; i < n; i++)
         {
            int r = pSrc_pixels[i].r, g = pSrc_pixels[i].g, b = pSrc_pixels[i].b;

            int best_err = INT_MAX;
            for (int sel = 0; sel < 4; sel++)
            {
               int base_r = pBlock_colors[sel].r, base_g = pBlock_colors[sel].g, base_b = pBlock_colors[sel].b;

               int dr = base_r - r;
               int dg = base_g - g;
               int db = base_b - b;

               int delta_l = dr * 27 + dg * 92 + db * 9;
               int delta_cr = dr * 128 - delta_l;
               int delta_cb = db * 128 - delta_l;

               int id = ((delta_l * delta_l) >> 7) +
                  ((((delta_cr * delta_cr) >> 7) * 26) >> 7) +
                  ((((delta_cb * delta_cb) >> 7) * 3) >> 7);
               
               if (id < best_err)
               {
                  best_err = id;
               }
            }

            *pDistance += best_err;
            if (*pDistance > early_out_error)
               return;
         }
      }
   };

   struct find_lowest_error_linear_rgb_4_N : spmd_kernel
   {
      inline vint compute_dist(
         const vint& base_r, const vint& base_g, const vint& base_b,
         const vint& r, const vint& g, const vint& b)
      {
         vint dr = base_r - r;
         vint dg = base_g - g;
         vint db = base_b - b;

         vint id = dr * dr + dg * dg + db * db;

         return id;
      }

      void _call(int64_t* pDistance,
         const color_rgba* pBlock_colors,
         const color_rgba* pSrc_pixels, uint32_t n,
         int64_t early_out_error)
      {
         assert(early_out_error >= 0);

         *pDistance = 0;

         vint block_colors_r[4], block_colors_g[4], block_colors_b[4];
         for (uint32_t i = 0; i < 4; i++)
         {
            store_all(block_colors_r[i], (int)pBlock_colors[i].r);
            store_all(block_colors_g[i], (int)pBlock_colors[i].g);
            store_all(block_colors_b[i], (int)pBlock_colors[i].b);
         }

         uint32_t i;

         for (i = 0; (i + 4) <= n; i += 4)
         {
            __m128i c0 = load_rgba32(&pSrc_pixels[i + 0]), c1 = load_rgba32(&pSrc_pixels[i + 1]), c2 = load_rgba32(&pSrc_pixels[i + 2]), c3 = load_rgba32(&pSrc_pixels[i + 3]);

            vint r, g, b, a;
            transpose4x4(r.m_value, g.m_value, b.m_value, a.m_value, c0, c1, c2, c3);

            vint dist0 = compute_dist(block_colors_r[0], block_colors_g[0], block_colors_b[0], r, g, b);
            vint dist1 = compute_dist(block_colors_r[1], block_colors_g[1], block_colors_b[1], r, g, b);
            vint dist2 = compute_dist(block_colors_r[2], block_colors_g[2], block_colors_b[2], r, g, b);
            vint dist3 = compute_dist(block_colors_r[3], block_colors_g[3], block_colors_b[3], r, g, b);

            vint min_dist = min(min(min(dist0, dist1), dist2), dist3);

            *pDistance += reduce_add(min_dist);
            if (*pDistance > early_out_error)
               return;
         }

         for (; i < n; i++)
         {
            int r = pSrc_pixels[i].r, g = pSrc_pixels[i].g, b = pSrc_pixels[i].b;

            int best_err = INT_MAX;
            for (int sel = 0; sel < 4; sel++)
            {
               int base_r = pBlock_colors[sel].r, base_g = pBlock_colors[sel].g, base_b = pBlock_colors[sel].b;

               int dr = base_r - r;
               int dg = base_g - g;
               int db = base_b - b;

               int id = dr * dr + dg * dg + db * db;

               if (id < best_err)
               {
                  best_err = id;
               }
            }

            *pDistance += best_err;
            if (*pDistance > early_out_error)
               return;
         }
      }
   };

   struct update_covar_matrix_16x16 : spmd_kernel
   {
      void _call(
         uint32_t num_vecs, const void* pWeighted_vecs_void, const void* pOrigin_void, const uint32_t* pVec_indices, void* pMatrix16x16_void)
      {
         const std::pair<vec16F, uint64_t>* pWeighted_vecs = static_cast< const std::pair<vec16F, uint64_t> *>(pWeighted_vecs_void);
         
         const float* pOrigin = static_cast<const float*>(pOrigin_void);
         vfloat org0 = loadu_linear_all(pOrigin), org1 = loadu_linear_all(pOrigin + 4), org2 = loadu_linear_all(pOrigin + 8), org3 = loadu_linear_all(pOrigin + 12);
                  
         vfloat mat[16][4];
         vfloat vzero(zero_vfloat());

         for (uint32_t i = 0; i < 16; i++)
         {
            store_all(mat[i][0], vzero);
            store_all(mat[i][1], vzero);
            store_all(mat[i][2], vzero);
            store_all(mat[i][3], vzero);
         }

         for (uint32_t k = 0; k < num_vecs; k++)
         {
            const uint32_t vec_index = pVec_indices[k];

            const float* pW = pWeighted_vecs[vec_index].first.get_ptr();
            vfloat weight((float)pWeighted_vecs[vec_index].second);

            vfloat vec[4] = { loadu_linear_all(pW) - org0, loadu_linear_all(pW + 4) - org1, loadu_linear_all(pW + 8) - org2, loadu_linear_all(pW + 12) - org3 };
                                                
            vfloat wvec0 = vec[0] * weight, wvec1 = vec[1] * weight, wvec2 = vec[2] * weight, wvec3 = vec[3] * weight;

            for (uint32_t j = 0; j < 16; j++)
            {
               vfloat vx = ((const float*)vec)[j];

               store_all(mat[j][0], mat[j][0] + vx * wvec0);
               store_all(mat[j][1], mat[j][1] + vx * wvec1);
               store_all(mat[j][2], mat[j][2] + vx * wvec2);
               store_all(mat[j][3], mat[j][3] + vx * wvec3);

            } // j

         } // k

         float* pMatrix = static_cast<float*>(pMatrix16x16_void);

         float* pDst = pMatrix;
         for (uint32_t i = 0; i < 16; i++)
         {
            storeu_linear_all(pDst, mat[i][0]);
            storeu_linear_all(pDst + 4, mat[i][1]);
            storeu_linear_all(pDst + 8, mat[i][2]);
            storeu_linear_all(pDst + 12, mat[i][3]);
            pDst += 16;
         }
      }
   };

} // namespace

using namespace CPPSPMD_NAME(basisu_kernels_namespace);

void CPPSPMD_NAME(perceptual_distance_rgb_4_N)(int64_t* pDistance, const uint8_t* pSelectors, const color_rgba* pBlock_colors, const color_rgba* pSrc_pixels, uint32_t n, int64_t early_out_err)
{
   spmd_call< perceptual_distance_rgb_4_N >(pDistance, pSelectors, pBlock_colors, pSrc_pixels, n, early_out_err);
}

void CPPSPMD_NAME(linear_distance_rgb_4_N)(int64_t* pDistance, const uint8_t* pSelectors, const color_rgba* pBlock_colors, const color_rgba* pSrc_pixels, uint32_t n, int64_t early_out_err)
{
   spmd_call< linear_distance_rgb_4_N >(pDistance, pSelectors, pBlock_colors, pSrc_pixels, n, early_out_err);
}

void CPPSPMD_NAME(find_selectors_perceptual_rgb_4_N)(int64_t *pDistance, uint8_t* pSelectors, const color_rgba* pBlock_colors, const color_rgba* pSrc_pixels, uint32_t n, int64_t early_out_err)
{
   spmd_call< find_selectors_perceptual_rgb_4_N >(pDistance, pSelectors, pBlock_colors, pSrc_pixels, n, early_out_err);
}

void CPPSPMD_NAME(find_selectors_linear_rgb_4_N)(int64_t* pDistance, uint8_t* pSelectors, const color_rgba* pBlock_colors, const color_rgba* pSrc_pixels, uint32_t n, int64_t early_out_err)
{
   spmd_call< find_selectors_linear_rgb_4_N >(pDistance, pSelectors, pBlock_colors, pSrc_pixels, n, early_out_err);
}

void CPPSPMD_NAME(find_lowest_error_perceptual_rgb_4_N)(int64_t* pDistance, const color_rgba* pBlock_colors, const color_rgba* pSrc_pixels, uint32_t n, int64_t early_out_error)
{
   spmd_call< find_lowest_error_perceptual_rgb_4_N >(pDistance, pBlock_colors, pSrc_pixels, n, early_out_error);
}

void CPPSPMD_NAME(find_lowest_error_linear_rgb_4_N)(int64_t* pDistance, const color_rgba* pBlock_colors, const color_rgba* pSrc_pixels, uint32_t n, int64_t early_out_error)
{
   spmd_call< find_lowest_error_linear_rgb_4_N >(pDistance, pBlock_colors, pSrc_pixels, n, early_out_error);
}

void CPPSPMD_NAME(update_covar_matrix_16x16)(uint32_t num_vecs, const void* pWeighted_vecs, const void* pOrigin, const uint32_t *pVec_indices, void* pMatrix16x16)
{
   spmd_call < update_covar_matrix_16x16 >(num_vecs, pWeighted_vecs, pOrigin, pVec_indices, pMatrix16x16);
}
