// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "heuristic_binning.h"
#include "heuristic_spatial.h"

namespace embree
{
  namespace isa
  { 
#if 0
#define SPATIAL_ASPLIT_OVERLAP_THRESHOLD 0.2f
#define SPATIAL_ASPLIT_SAH_THRESHOLD 0.95f
#define SPATIAL_ASPLIT_AREA_THRESHOLD 0.0f
#else
#define SPATIAL_ASPLIT_OVERLAP_THRESHOLD 0.1f
#define SPATIAL_ASPLIT_SAH_THRESHOLD 0.99f
#define SPATIAL_ASPLIT_AREA_THRESHOLD 0.000005f
#endif

    struct PrimInfoExtRange : public CentGeomBBox3fa, public extended_range<size_t>
    {
      __forceinline PrimInfoExtRange() {
      }

      __forceinline PrimInfoExtRange(EmptyTy)
        : CentGeomBBox3fa(EmptyTy()), extended_range<size_t>(0,0,0) {}

      __forceinline PrimInfoExtRange(size_t begin, size_t end, size_t ext_end, const CentGeomBBox3fa& centGeomBounds) 
        : CentGeomBBox3fa(centGeomBounds), extended_range<size_t>(begin,end,ext_end) {}
      
      __forceinline float leafSAH() const { 
	return expectedApproxHalfArea(geomBounds)*float(size()); 
      }
      
      __forceinline float leafSAH(size_t block_shift) const { 
	return expectedApproxHalfArea(geomBounds)*float((size()+(size_t(1)<<block_shift)-1) >> block_shift);
      }
    };

    template<typename ObjectSplit, typename SpatialSplit>
      struct Split2
      {
        __forceinline Split2 () {}
        
        __forceinline Split2 (const Split2& other) 
        {
          spatial = other.spatial;
          sah = other.sah;
          if (spatial) spatialSplit() = other.spatialSplit();
          else         objectSplit()  = other.objectSplit();
        }
        
        __forceinline Split2& operator= (const Split2& other) 
        {
          spatial = other.spatial;
          sah = other.sah;
          if (spatial) spatialSplit() = other.spatialSplit();
          else         objectSplit()  = other.objectSplit();
          return *this;
        }
          
          __forceinline     ObjectSplit&  objectSplit()        { return *(      ObjectSplit*)data; }
        __forceinline const ObjectSplit&  objectSplit() const  { return *(const ObjectSplit*)data; }
        
        __forceinline       SpatialSplit& spatialSplit()       { return *(      SpatialSplit*)data; }
        __forceinline const SpatialSplit& spatialSplit() const { return *(const SpatialSplit*)data; }
        
        __forceinline Split2 (const ObjectSplit& objectSplit, float sah)
          : spatial(false), sah(sah) 
        {
          new (data) ObjectSplit(objectSplit);
        }
        
        __forceinline Split2 (const SpatialSplit& spatialSplit, float sah)
          : spatial(true), sah(sah) 
        {
          new (data) SpatialSplit(spatialSplit);
        }
        
        __forceinline float splitSAH() const { 
          return sah; 
        }
        
        __forceinline bool valid() const {
          return sah < float(inf);
        }
        
      public:
        __aligned(64) char data[sizeof(ObjectSplit) > sizeof(SpatialSplit) ? sizeof(ObjectSplit) : sizeof(SpatialSplit)];
        bool spatial;
        float sah;
      };
    
    /*! Performs standard object binning */
    template<typename PrimitiveSplitterFactory, typename PrimRef, size_t OBJECT_BINS, size_t SPATIAL_BINS>
      struct HeuristicArraySpatialSAH
      {
        typedef BinSplit<OBJECT_BINS> ObjectSplit;
        typedef BinInfoT<OBJECT_BINS,PrimRef,BBox3fa> ObjectBinner;

        typedef SpatialBinSplit<SPATIAL_BINS> SpatialSplit;
        typedef SpatialBinInfo<SPATIAL_BINS,PrimRef> SpatialBinner;

        //typedef extended_range<size_t> Set;
        typedef Split2<ObjectSplit,SpatialSplit> Split;
        
        static const size_t PARALLEL_THRESHOLD = 3*1024;
        static const size_t PARALLEL_FIND_BLOCK_SIZE = 1024;
        static const size_t PARALLEL_PARTITION_BLOCK_SIZE = 128;

        static const size_t MOVE_STEP_SIZE = 64;
        static const size_t CREATE_SPLITS_STEP_SIZE = 64;

        __forceinline HeuristicArraySpatialSAH ()
          : prims0(nullptr) {}
        
        /*! remember prim array */
        __forceinline HeuristicArraySpatialSAH (const PrimitiveSplitterFactory& splitterFactory, PrimRef* prims0, const CentGeomBBox3fa& root_info)
          : prims0(prims0), splitterFactory(splitterFactory), root_info(root_info) {}


        /*! compute extended ranges */
        __noinline void setExtentedRanges(const PrimInfoExtRange& set, PrimInfoExtRange& lset, PrimInfoExtRange& rset, const size_t lweight, const size_t rweight)
        {
          assert(set.ext_range_size() > 0);
          const float left_factor           = (float)lweight / (lweight + rweight);
          const size_t ext_range_size       = set.ext_range_size();
          const size_t left_ext_range_size  = min((size_t)(floorf(left_factor * ext_range_size)),ext_range_size);
          const size_t right_ext_range_size = ext_range_size - left_ext_range_size;
          lset.set_ext_range(lset.end() + left_ext_range_size);
          rset.set_ext_range(rset.end() + right_ext_range_size);
        }

        /*! move ranges */
        __noinline void moveExtentedRange(const PrimInfoExtRange& set, const PrimInfoExtRange& lset, PrimInfoExtRange& rset)
        {
          const size_t left_ext_range_size = lset.ext_range_size();
          const size_t right_size = rset.size();

          /* has the left child an extended range? */
          if (left_ext_range_size > 0)
          {
            /* left extended range smaller than right range ? */
            if (left_ext_range_size < right_size)
            {
              /* only move a small part of the beginning of the right range to the end */
              parallel_for( rset.begin(), rset.begin()+left_ext_range_size, MOVE_STEP_SIZE, [&](const range<size_t>& r) {                  
                  for (size_t i=r.begin(); i<r.end(); i++)
                    prims0[i+right_size] = prims0[i];
                });
            }
            else
            {
              /* no overlap, move entire right range to new location, can be made fully parallel */
              parallel_for( rset.begin(), rset.end(), MOVE_STEP_SIZE,  [&](const range<size_t>& r) {
                  for (size_t i=r.begin(); i<r.end(); i++)
                    prims0[i+left_ext_range_size] = prims0[i];
                });
            }
            /* update right range */
            assert(rset.ext_end() + left_ext_range_size == set.ext_end());
            rset.move_right(left_ext_range_size);
          }
        }

        /*! finds the best split */
        const Split find(const PrimInfoExtRange& set, const size_t logBlockSize)
        {
          SplitInfo oinfo;
          const ObjectSplit object_split = object_find(set,logBlockSize,oinfo);
          const float object_split_sah = object_split.splitSAH();

          if (unlikely(set.has_ext_range()))
          {
            const BBox3fa overlap = intersect(oinfo.leftBounds, oinfo.rightBounds);
            
            /* do only spatial splits if the child bounds overlap */
            if (safeArea(overlap) >= SPATIAL_ASPLIT_AREA_THRESHOLD*safeArea(root_info.geomBounds) &&
                safeArea(overlap) >= SPATIAL_ASPLIT_OVERLAP_THRESHOLD*safeArea(set.geomBounds))
            {              
              const SpatialSplit spatial_split = spatial_find(set, logBlockSize);
              const float spatial_split_sah = spatial_split.splitSAH();

              /* valid spatial split, better SAH and number of splits do not exceed extended range */
              if (spatial_split_sah < SPATIAL_ASPLIT_SAH_THRESHOLD*object_split_sah &&
                  spatial_split.left + spatial_split.right - set.size() <= set.ext_range_size())
              {          
                return Split(spatial_split,spatial_split_sah);
              }
            }
          }

          return Split(object_split,object_split_sah);
        }

        /*! finds the best object split */
        __forceinline const ObjectSplit object_find(const PrimInfoExtRange& set, const size_t logBlockSize, SplitInfo &info)
        {
          if (set.size() < PARALLEL_THRESHOLD) return sequential_object_find(set,logBlockSize,info);
          else                                 return parallel_object_find  (set,logBlockSize,info);
        }

        /*! finds the best object split */
        __noinline const ObjectSplit sequential_object_find(const PrimInfoExtRange& set, const size_t logBlockSize, SplitInfo &info)
        {
          ObjectBinner binner(empty); 
          const BinMapping<OBJECT_BINS> mapping(set);
          binner.bin(prims0,set.begin(),set.end(),mapping);
          ObjectSplit s = binner.best(mapping,logBlockSize);
          binner.getSplitInfo(mapping, s, info);
          return s;
        }

        /*! finds the best split */
        __noinline const ObjectSplit parallel_object_find(const PrimInfoExtRange& set, const size_t logBlockSize, SplitInfo &info)
        {
          ObjectBinner binner(empty);
          const BinMapping<OBJECT_BINS> mapping(set);
          const BinMapping<OBJECT_BINS>& _mapping = mapping; // CLANG 3.4 parser bug workaround
          binner = parallel_reduce(set.begin(),set.end(),PARALLEL_FIND_BLOCK_SIZE,binner,
                                   [&] (const range<size_t>& r) -> ObjectBinner { ObjectBinner binner(empty); binner.bin(prims0+r.begin(),r.size(),_mapping); return binner; },
                                   [&] (const ObjectBinner& b0, const ObjectBinner& b1) -> ObjectBinner { ObjectBinner r = b0; r.merge(b1,_mapping.size()); return r; });
          ObjectSplit s = binner.best(mapping,logBlockSize);
          binner.getSplitInfo(mapping, s, info);
          return s;
        }

        /*! finds the best spatial split */
        __forceinline const SpatialSplit spatial_find(const PrimInfoExtRange& set, const size_t logBlockSize)
        {
          if (set.size() < PARALLEL_THRESHOLD) return sequential_spatial_find(set, logBlockSize);
          else                                 return parallel_spatial_find  (set, logBlockSize);
        }

        /*! finds the best spatial split */
        __noinline const SpatialSplit sequential_spatial_find(const PrimInfoExtRange& set, const size_t logBlockSize)
        {
          SpatialBinner binner(empty); 
          const SpatialBinMapping<SPATIAL_BINS> mapping(set);
          binner.bin2(splitterFactory,prims0,set.begin(),set.end(),mapping);
          /* todo: best spatial split not exceeding the extended range does not provide any benefit ?*/
          return binner.best(mapping,logBlockSize); //,set.ext_size());
        }

        __noinline const SpatialSplit parallel_spatial_find(const PrimInfoExtRange& set, const size_t logBlockSize)
        {
          SpatialBinner binner(empty);
          const SpatialBinMapping<SPATIAL_BINS> mapping(set);
          const SpatialBinMapping<SPATIAL_BINS>& _mapping = mapping; // CLANG 3.4 parser bug workaround
          binner = parallel_reduce(set.begin(),set.end(),PARALLEL_FIND_BLOCK_SIZE,binner,
                                   [&] (const range<size_t>& r) -> SpatialBinner { 
                                     SpatialBinner binner(empty); 
                                     binner.bin2(splitterFactory,prims0,r.begin(),r.end(),_mapping);
                                     return binner; },
                                   [&] (const SpatialBinner& b0, const SpatialBinner& b1) -> SpatialBinner { return SpatialBinner::reduce(b0,b1); });
          /* todo: best spatial split not exceeding the extended range does not provide any benefit ?*/
          return binner.best(mapping,logBlockSize); //,set.ext_size());
        }


        /*! subdivides primitives based on a spatial split */
        __noinline void create_spatial_splits(PrimInfoExtRange& set, const SpatialSplit& split, const SpatialBinMapping<SPATIAL_BINS> &mapping)
        {
          assert(set.has_ext_range());
          const size_t max_ext_range_size = set.ext_range_size();
          const size_t ext_range_start = set.end();

          /* atomic counter for number of primref splits */
          std::atomic<size_t> ext_elements;
          ext_elements.store(0);
          
          const float fpos = split.mapping.pos(split.pos,split.dim);
        
          const unsigned int mask = 0xFFFFFFFF >> RESERVED_NUM_SPATIAL_SPLITS_GEOMID_BITS;

          parallel_for( set.begin(), set.end(), CREATE_SPLITS_STEP_SIZE, [&](const range<size_t>& r) {
              for (size_t i=r.begin();i<r.end();i++)
              {
                const unsigned int splits = prims0[i].geomID() >> (32-RESERVED_NUM_SPATIAL_SPLITS_GEOMID_BITS);

                if (likely(splits <= 1)) continue; /* todo: does this ever happen ? */

                const int bin0 = split.mapping.bin(prims0[i].lower)[split.dim];
                const int bin1 = split.mapping.bin(prims0[i].upper)[split.dim];
                if (unlikely(bin0 < split.pos && bin1 >= split.pos))
                {
                  assert(splits > 1);

                  PrimRef left,right;
                  const auto splitter = splitterFactory(prims0[i]);
                  splitter(prims0[i],split.dim,fpos,left,right);
                
                  // no empty splits
                  if (unlikely(left.bounds().empty() || right.bounds().empty())) continue;
                
                  left.lower.u  = (left.lower.u  & mask) | ((splits-1) << (32-RESERVED_NUM_SPATIAL_SPLITS_GEOMID_BITS));
                  right.lower.u = (right.lower.u & mask) | ((splits-1) << (32-RESERVED_NUM_SPATIAL_SPLITS_GEOMID_BITS));

                  const size_t ID = ext_elements.fetch_add(1);

                  /* break if the number of subdivided elements are greater than the maximum allowed size */
                  if (unlikely(ID >= max_ext_range_size)) 
                    break;

                  /* only write within the correct bounds */
                  assert(ID < max_ext_range_size);
                  prims0[i] = left;
                  prims0[ext_range_start+ID] = right;     
                }
              }
            });

          const size_t numExtElements = min(max_ext_range_size,ext_elements.load());          
          assert(set.end()+numExtElements<=set.ext_end());
          set._end += numExtElements;
        }
        
        /*! array partitioning */
        void split(const Split& split, const PrimInfoExtRange& set_i, PrimInfoExtRange& lset, PrimInfoExtRange& rset) 
        {
          PrimInfoExtRange set = set_i;
          
          /* valid split */
          if (unlikely(!split.valid())) {
            deterministic_order(set);
            return splitFallback(set,lset,rset);
          }

          std::pair<size_t,size_t> ext_weights(0,0);

          if (unlikely(split.spatial))
          {
            create_spatial_splits(set,split.spatialSplit(), split.spatialSplit().mapping); 

            /* spatial split */
            if (likely(set.size() < PARALLEL_THRESHOLD)) 
              ext_weights = sequential_spatial_split(split.spatialSplit(),set,lset,rset);
            else
              ext_weights = parallel_spatial_split(split.spatialSplit(),set,lset,rset);
          }
          else
          {
            /* object split */
            if (likely(set.size() < PARALLEL_THRESHOLD)) 
              ext_weights = sequential_object_split(split.objectSplit(),set,lset,rset);
            else
              ext_weights = parallel_object_split(split.objectSplit(),set,lset,rset);
          }

          /* if we have an extended range, set extended child ranges and move right split range */
          if (unlikely(set.has_ext_range())) 
          {
            setExtentedRanges(set,lset,rset,ext_weights.first,ext_weights.second);
            moveExtentedRange(set,lset,rset);
          }
        }

        /*! array partitioning */
        std::pair<size_t,size_t> sequential_object_split(const ObjectSplit& split, const PrimInfoExtRange& set, PrimInfoExtRange& lset, PrimInfoExtRange& rset) 
        {
          const size_t begin = set.begin();
          const size_t end   = set.end();
          PrimInfo local_left(empty);
          PrimInfo local_right(empty);
          const unsigned int splitPos = split.pos;
          const unsigned int splitDim = split.dim;
          const unsigned int splitDimMask = (unsigned int)1 << splitDim; 

          const typename ObjectBinner::vint vSplitPos(splitPos);
          const typename ObjectBinner::vbool vSplitMask(splitDimMask);
          size_t center = serial_partitioning(prims0,
                                              begin,end,local_left,local_right,
                                              [&] (const PrimRef& ref) { 
                                                return split.mapping.bin_unsafe(ref,vSplitPos,vSplitMask);
                                              },
                                              [] (PrimInfo& pinfo,const PrimRef& ref) { pinfo.add_center2(ref,ref.lower.u >> (32-RESERVED_NUM_SPATIAL_SPLITS_GEOMID_BITS)); });          
          const size_t left_weight  = local_left.end;
          const size_t right_weight = local_right.end;

          new (&lset) PrimInfoExtRange(begin,center,center,local_left);
          new (&rset) PrimInfoExtRange(center,end,end,local_right);

          assert(!lset.geomBounds.empty() && area(lset.geomBounds) >= 0.0f);
          assert(!rset.geomBounds.empty() && area(rset.geomBounds) >= 0.0f);
          return std::pair<size_t,size_t>(left_weight,right_weight);
        }


        /*! array partitioning */
        __noinline std::pair<size_t,size_t> sequential_spatial_split(const SpatialSplit& split, const PrimInfoExtRange& set, PrimInfoExtRange& lset, PrimInfoExtRange& rset) 
        {
          const size_t begin = set.begin();
          const size_t end   = set.end();
          PrimInfo local_left(empty);
          PrimInfo local_right(empty);
          const unsigned int splitPos = split.pos;
          const unsigned int splitDim = split.dim;
          const unsigned int splitDimMask = (unsigned int)1 << splitDim; 

          /* init spatial mapping */
          const SpatialBinMapping<SPATIAL_BINS> &mapping = split.mapping;
          const vint4 vSplitPos(splitPos);
          const vbool4 vSplitMask( (int)splitDimMask );

          size_t center = serial_partitioning(prims0,
                                              begin,end,local_left,local_right,
                                              [&] (const PrimRef& ref) {
                                                const Vec3fa c = ref.bounds().center();
                                                return any(((vint4)mapping.bin(c) < vSplitPos) & vSplitMask);
                                              },
                                              [] (PrimInfo& pinfo,const PrimRef& ref) { pinfo.add_center2(ref,ref.lower.u >> (32-RESERVED_NUM_SPATIAL_SPLITS_GEOMID_BITS)); });

          const size_t left_weight  = local_left.end;
          const size_t right_weight = local_right.end;
          
          new (&lset) PrimInfoExtRange(begin,center,center,local_left);
          new (&rset) PrimInfoExtRange(center,end,end,local_right);
          assert(!lset.geomBounds.empty() && area(lset.geomBounds) >= 0.0f);
          assert(!rset.geomBounds.empty() && area(rset.geomBounds) >= 0.0f);
          return std::pair<size_t,size_t>(left_weight,right_weight);
        }


        
        /*! array partitioning */
        __noinline std::pair<size_t,size_t> parallel_object_split(const ObjectSplit& split, const PrimInfoExtRange& set, PrimInfoExtRange& lset, PrimInfoExtRange& rset)
        {
          const size_t begin = set.begin();
          const size_t end   = set.end();
          PrimInfo left(empty);
          PrimInfo right(empty);
          const unsigned int splitPos = split.pos;
          const unsigned int splitDim = split.dim;
          const unsigned int splitDimMask = (unsigned int)1 << splitDim;

          const typename ObjectBinner::vint vSplitPos(splitPos);
          const typename ObjectBinner::vbool vSplitMask(splitDimMask);
          auto isLeft = [&] (const PrimRef &ref) { return split.mapping.bin_unsafe(ref,vSplitPos,vSplitMask); };

          const size_t center = parallel_partitioning(
            prims0,begin,end,EmptyTy(),left,right,isLeft,
            [] (PrimInfo &pinfo,const PrimRef &ref) { pinfo.add_center2(ref,ref.lower.u >> (32-RESERVED_NUM_SPATIAL_SPLITS_GEOMID_BITS)); },
            [] (PrimInfo &pinfo0,const PrimInfo &pinfo1) { pinfo0.merge(pinfo1); },
            PARALLEL_PARTITION_BLOCK_SIZE);

          const size_t left_weight  = left.end;
          const size_t right_weight = right.end;
          
          left.begin  = begin;  left.end  = center; 
          right.begin = center; right.end = end;
          
          new (&lset) PrimInfoExtRange(begin,center,center,left);
          new (&rset) PrimInfoExtRange(center,end,end,right);

          assert(area(left.geomBounds) >= 0.0f);
          assert(area(right.geomBounds) >= 0.0f);
          return std::pair<size_t,size_t>(left_weight,right_weight);
        }

        /*! array partitioning */
        __noinline std::pair<size_t,size_t> parallel_spatial_split(const SpatialSplit& split, const PrimInfoExtRange& set, PrimInfoExtRange& lset, PrimInfoExtRange& rset)
        {
          const size_t begin = set.begin();
          const size_t end   = set.end();
          PrimInfo left(empty);
          PrimInfo right(empty);
          const unsigned int splitPos = split.pos;
          const unsigned int splitDim = split.dim;
          const unsigned int splitDimMask = (unsigned int)1 << splitDim;

          /* init spatial mapping */
          const SpatialBinMapping<SPATIAL_BINS>& mapping = split.mapping;
          const vint4 vSplitPos(splitPos);
          const vbool4 vSplitMask( (int)splitDimMask );

          auto isLeft = [&] (const PrimRef &ref) { 
            const Vec3fa c = ref.bounds().center();
            return any(((vint4)mapping.bin(c) < vSplitPos) & vSplitMask); };

          const size_t center = parallel_partitioning(
            prims0,begin,end,EmptyTy(),left,right,isLeft,
            [] (PrimInfo &pinfo,const PrimRef &ref) { pinfo.add_center2(ref,ref.lower.u >> (32-RESERVED_NUM_SPATIAL_SPLITS_GEOMID_BITS)); },
            [] (PrimInfo &pinfo0,const PrimInfo &pinfo1) { pinfo0.merge(pinfo1); },
            PARALLEL_PARTITION_BLOCK_SIZE);

          const size_t left_weight  = left.end;
          const size_t right_weight = right.end;
          
          left.begin  = begin;  left.end  = center; 
          right.begin = center; right.end = end;
          
          new (&lset) PrimInfoExtRange(begin,center,center,left);
          new (&rset) PrimInfoExtRange(center,end,end,right);

          assert(area(left.geomBounds) >= 0.0f);
          assert(area(right.geomBounds) >= 0.0f);
          return std::pair<size_t,size_t>(left_weight,right_weight);
        }

        void deterministic_order(const PrimInfoExtRange& set) 
        {
          /* required as parallel partition destroys original primitive order */
          std::sort(&prims0[set.begin()],&prims0[set.end()]);
        }

        void splitFallback(const PrimInfoExtRange& set, 
                           PrimInfoExtRange& lset, 
                           PrimInfoExtRange& rset)
        {
          const size_t begin = set.begin();
          const size_t end   = set.end();
          const size_t center = (begin + end)/2;

          PrimInfo left(empty);
          for (size_t i=begin; i<center; i++) {
            left.add_center2(prims0[i],prims0[i].lower.u >> (32-RESERVED_NUM_SPATIAL_SPLITS_GEOMID_BITS));
          }
          const size_t lweight = left.end;
          
          PrimInfo right(empty);
          for (size_t i=center; i<end; i++) {
            right.add_center2(prims0[i],prims0[i].lower.u >> (32-RESERVED_NUM_SPATIAL_SPLITS_GEOMID_BITS));	
          }
          const size_t rweight = right.end;

          new (&lset) PrimInfoExtRange(begin,center,center,left);
          new (&rset) PrimInfoExtRange(center,end,end,right);

          /* if we have an extended range */
          if (set.has_ext_range()) {
            setExtentedRanges(set,lset,rset,lweight,rweight);
            moveExtentedRange(set,lset,rset);
          }
        }
        
      private:
        PrimRef* const prims0;
        const PrimitiveSplitterFactory& splitterFactory;
        const CentGeomBBox3fa& root_info;
      };
  }
}
