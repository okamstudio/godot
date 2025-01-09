// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "heuristic_binning_array_aligned.h"
#include "heuristic_spatial_array.h"
#include "heuristic_openmerge_array.h"

#define NUM_OBJECT_BINS 32
#define NUM_SPATIAL_BINS 16

namespace embree
{
  namespace isa
  {
    MAYBE_UNUSED static const float travCost = 1.0f;
    MAYBE_UNUSED static const size_t DEFAULT_SINGLE_THREAD_THRESHOLD = 1024;

    struct GeneralBVHBuilder
    {
      static const size_t MAX_BRANCHING_FACTOR = 16;       //!< maximum supported BVH branching factor      
      static const size_t MIN_LARGE_LEAF_LEVELS = 8;       //!< create balanced tree of we are that many levels before the maximum tree depth
      

      /*! settings for SAH builder */
      struct Settings
      {
        /*! default settings */
        Settings ()
        : branchingFactor(2), maxDepth(32), logBlockSize(0), minLeafSize(1), maxLeafSize(7),
          travCost(1.0f), intCost(1.0f), singleThreadThreshold(1024), primrefarrayalloc(inf) {}

        /*! initialize settings from API settings */
        Settings (const RTCBuildArguments& settings)
        : branchingFactor(2), maxDepth(32), logBlockSize(0), minLeafSize(1), maxLeafSize(7),
          travCost(1.0f), intCost(1.0f), singleThreadThreshold(1024), primrefarrayalloc(inf)
        {
          if (RTC_BUILD_ARGUMENTS_HAS(settings,maxBranchingFactor)) branchingFactor = settings.maxBranchingFactor;
          if (RTC_BUILD_ARGUMENTS_HAS(settings,maxDepth          )) maxDepth        = settings.maxDepth;
          if (RTC_BUILD_ARGUMENTS_HAS(settings,sahBlockSize      )) logBlockSize    = bsr(settings.sahBlockSize);
          if (RTC_BUILD_ARGUMENTS_HAS(settings,minLeafSize       )) minLeafSize     = settings.minLeafSize;
          if (RTC_BUILD_ARGUMENTS_HAS(settings,maxLeafSize       )) maxLeafSize     = settings.maxLeafSize;
          if (RTC_BUILD_ARGUMENTS_HAS(settings,traversalCost     )) travCost        = settings.traversalCost;
          if (RTC_BUILD_ARGUMENTS_HAS(settings,intersectionCost  )) intCost         = settings.intersectionCost;

          minLeafSize = min(minLeafSize,maxLeafSize);
        }

        Settings (size_t sahBlockSize, size_t minLeafSize, size_t maxLeafSize, float travCost, float intCost, size_t singleThreadThreshold, size_t primrefarrayalloc = inf)
        : branchingFactor(2), maxDepth(32), logBlockSize(bsr(sahBlockSize)), minLeafSize(min(minLeafSize,maxLeafSize)), maxLeafSize(maxLeafSize),
          travCost(travCost), intCost(intCost), singleThreadThreshold(singleThreadThreshold), primrefarrayalloc(primrefarrayalloc)
        {
        }

      public:
        size_t branchingFactor;  //!< branching factor of BVH to build
        size_t maxDepth;         //!< maximum depth of BVH to build
        size_t logBlockSize;     //!< log2 of blocksize for SAH heuristic
        size_t minLeafSize;      //!< minimum size of a leaf
        size_t maxLeafSize;      //!< maximum size of a leaf
        float travCost;          //!< estimated cost of one traversal step
        float intCost;           //!< estimated cost of one primitive intersection
        size_t singleThreadThreshold; //!< threshold when we switch to single threaded build
        size_t primrefarrayalloc;  //!< builder uses prim ref array to allocate nodes and leaves when a subtree of that size is finished
      };

      /*! recursive state of builder */
      template<typename Set, typename Split>
        struct BuildRecordT
        {
        public:
          __forceinline BuildRecordT () {}

          __forceinline BuildRecordT (size_t depth)
            : depth(depth), alloc_barrier(false), prims(empty) {}

          __forceinline BuildRecordT (size_t depth, const Set& prims)
            : depth(depth), alloc_barrier(false), prims(prims) {}

          __forceinline BBox3fa bounds() const { return prims.geomBounds; }

          __forceinline friend bool operator< (const BuildRecordT& a, const BuildRecordT& b) { return a.prims.size() < b.prims.size(); }
          __forceinline friend bool operator> (const BuildRecordT& a, const BuildRecordT& b) { return a.prims.size() > b.prims.size();  }

          __forceinline size_t size() const { return prims.size(); }

        public:
          size_t depth;       //!< Depth of the root of this subtree.
          bool alloc_barrier; //!< barrier used to reuse primref-array blocks to allocate nodes
          Set prims;          //!< The list of primitives.
        };

      template<typename PrimRef, typename Set>
      struct DefaultCanCreateLeafFunc
      {
        __forceinline bool operator()(const PrimRef*, const Set&) const { return true; }
      };

      template<typename PrimRef, typename Set>
      struct DefaultCanCreateLeafSplitFunc
      {
        __forceinline void operator()(PrimRef*, const Set&, Set&, Set&) const { }
      };

      template<typename BuildRecord,
        typename Heuristic,
        typename Set,
        typename PrimRef,
        typename ReductionTy,
        typename Allocator,
        typename CreateAllocFunc,
        typename CreateNodeFunc,
        typename UpdateNodeFunc,
        typename CreateLeafFunc,
        typename CanCreateLeafFunc,
        typename CanCreateLeafSplitFunc,
        typename ProgressMonitor>

        class BuilderT
        {
          friend struct GeneralBVHBuilder;

          BuilderT (PrimRef* prims,
                    Heuristic& heuristic,
                    const CreateAllocFunc& createAlloc,
                    const CreateNodeFunc& createNode,
                    const UpdateNodeFunc& updateNode,
                    const CreateLeafFunc& createLeaf,
                    const CanCreateLeafFunc& canCreateLeaf,
                    const CanCreateLeafSplitFunc& canCreateLeafSplit,
                    const ProgressMonitor& progressMonitor,
                    const Settings& settings) :
                    cfg(settings),
                    prims(prims),
                    heuristic(heuristic),
                    createAlloc(createAlloc),
                    createNode(createNode),
                    updateNode(updateNode),
                    createLeaf(createLeaf),
                    canCreateLeaf(canCreateLeaf),
                    canCreateLeafSplit(canCreateLeafSplit),
                    progressMonitor(progressMonitor)
          {
            if (cfg.branchingFactor > MAX_BRANCHING_FACTOR)
              throw_RTCError(RTC_ERROR_UNKNOWN,"bvh_builder: branching factor too large");
          }

          const ReductionTy createLargeLeaf(const BuildRecord& current, Allocator alloc)
          {
            /* this should never occur but is a fatal error */
            if (current.depth > cfg.maxDepth)
              throw_RTCError(RTC_ERROR_UNKNOWN,"depth limit reached");

            /* create leaf for few primitives */
            if (current.prims.size() <= cfg.maxLeafSize && canCreateLeaf(prims,current.prims))
              return createLeaf(prims,current.prims,alloc);

            /* fill all children by always splitting the largest one */
            ReductionTy values[MAX_BRANCHING_FACTOR];
            BuildRecord children[MAX_BRANCHING_FACTOR];
            size_t numChildren = 1;
            children[0] = current;
            do {

              /* find best child with largest bounding box area */
              size_t bestChild = -1;
              size_t bestSize = 0;
              for (size_t i=0; i<numChildren; i++)
              {
                /* ignore leaves as they cannot get split */
                if (children[i].prims.size() <= cfg.maxLeafSize && canCreateLeaf(prims,children[i].prims))
                  continue;

                /* remember child with largest size */
                if (children[i].prims.size() > bestSize) {
                  bestSize = children[i].prims.size();
                  bestChild = i;
                }
              }
              if (bestChild == (size_t)-1) break;

              /*! split best child into left and right child */
              BuildRecord left(current.depth+1);
              BuildRecord right(current.depth+1);
              if (!canCreateLeaf(prims,children[bestChild].prims)) {
                canCreateLeafSplit(prims,children[bestChild].prims,left.prims,right.prims);
              } else {
                heuristic.splitFallback(children[bestChild].prims,left.prims,right.prims);
              }

              /* add new children left and right */
              children[bestChild] = children[numChildren-1];
              children[numChildren-1] = left;
              children[numChildren+0] = right;
              numChildren++;

            } while (numChildren < cfg.branchingFactor);

            /* set barrier for primrefarrayalloc */
            if (unlikely(current.size() > cfg.primrefarrayalloc))
              for (size_t i=0; i<numChildren; i++)
                children[i].alloc_barrier = children[i].size() <= cfg.primrefarrayalloc;

            /* create node */
            auto node = createNode(children,numChildren,alloc);

            /* recurse into each child  and perform reduction */
            for (size_t i=0; i<numChildren; i++)
              values[i] = createLargeLeaf(children[i],alloc);

            /* perform reduction */
            return updateNode(current,children,node,values,numChildren);
          }

          const ReductionTy recurse(BuildRecord& current, Allocator alloc, bool toplevel)
          {
            /* get thread local allocator */
            if (!alloc)
              alloc = createAlloc();

            /* call memory monitor function to signal progress */
            if (toplevel && current.size() <= cfg.singleThreadThreshold)
              progressMonitor(current.size());

            /*! find best split */
            auto split = heuristic.find(current.prims,cfg.logBlockSize);

            /*! compute leaf and split cost */
            const float leafSAH  = cfg.intCost*current.prims.leafSAH(cfg.logBlockSize);
            const float splitSAH = cfg.travCost*halfArea(current.prims.geomBounds)+cfg.intCost*split.splitSAH();
            assert((current.prims.size() == 0) || ((leafSAH >= 0) && (splitSAH >= 0)));

            /*! create a leaf node when threshold reached or SAH tells us to stop */
            if (current.prims.size() <= cfg.minLeafSize || current.depth+MIN_LARGE_LEAF_LEVELS >= cfg.maxDepth || (current.prims.size() <= cfg.maxLeafSize && leafSAH <= splitSAH)) {
              heuristic.deterministic_order(current.prims);
              return createLargeLeaf(current,alloc);
            }

            /*! perform initial split */
            Set lprims,rprims;
            heuristic.split(split,current.prims,lprims,rprims);
	    
            /*! initialize child list with initial split */
            ReductionTy values[MAX_BRANCHING_FACTOR];
            BuildRecord children[MAX_BRANCHING_FACTOR];
            children[0] = BuildRecord(current.depth+1,lprims);
            children[1] = BuildRecord(current.depth+1,rprims);
            size_t numChildren = 2;

            /*! split until node is full or SAH tells us to stop */
            while (numChildren < cfg.branchingFactor)
            {
              /*! find best child to split */
              float bestArea = neg_inf;
              ssize_t bestChild = -1;
              for (size_t i=0; i<numChildren; i++)
              {
                /* ignore leaves as they cannot get split */
                if (children[i].prims.size() <= cfg.minLeafSize) continue;

                /* find child with largest surface area */
                if (halfArea(children[i].prims.geomBounds) > bestArea) {
                  bestChild = i;
                  bestArea = halfArea(children[i].prims.geomBounds);
                }
              }
              if (bestChild == -1) break;

              /* perform best found split */
              BuildRecord& brecord = children[bestChild];
              BuildRecord lrecord(current.depth+1);
              BuildRecord rrecord(current.depth+1);
              auto split = heuristic.find(brecord.prims,cfg.logBlockSize);
              heuristic.split(split,brecord.prims,lrecord.prims,rrecord.prims);
              children[bestChild  ] = lrecord;
              children[numChildren] = rrecord;
              numChildren++;
            }

            /* set barrier for primrefarrayalloc */
            if (unlikely(current.size() > cfg.primrefarrayalloc))
              for (size_t i=0; i<numChildren; i++)
                children[i].alloc_barrier = children[i].size() <= cfg.primrefarrayalloc;

            /* sort buildrecords for faster shadow ray traversal */
            std::sort(&children[0],&children[numChildren],std::greater<BuildRecord>());

            /*! create an inner node */
            auto node = createNode(children,numChildren,alloc);

            /* spawn tasks */
            if (current.size() > cfg.singleThreadThreshold)
            {
              /*! parallel_for is faster than spawning sub-tasks */
              parallel_for(size_t(0), numChildren, [&] (const range<size_t>& r) { // FIXME: no range here
                  for (size_t i=r.begin(); i<r.end(); i++) {
                    values[i] = recurse(children[i],nullptr,true);
                    _mm_mfence(); // to allow non-temporal stores during build
                  }
                });

              return updateNode(current,children,node,values,numChildren);
            }
            /* recurse into each child */
            else
            {
              for (size_t i=0; i<numChildren; i++)
                values[i] = recurse(children[i],alloc,false);

              return updateNode(current,children,node,values,numChildren);
            }
          }

        private:
          Settings cfg;
          PrimRef* prims;
          Heuristic& heuristic;
          const CreateAllocFunc& createAlloc;
          const CreateNodeFunc& createNode;
          const UpdateNodeFunc& updateNode;
          const CreateLeafFunc& createLeaf;
          const CanCreateLeafFunc& canCreateLeaf;
          const CanCreateLeafSplitFunc& canCreateLeafSplit;
          const ProgressMonitor& progressMonitor;
        };

      template<
      typename ReductionTy,
        typename Heuristic,
        typename Set,
        typename PrimRef,
        typename CreateAllocFunc,
        typename CreateNodeFunc,
        typename UpdateNodeFunc,
        typename CreateLeafFunc,
        typename ProgressMonitor>

        __noinline static ReductionTy build(Heuristic& heuristic,
                                            PrimRef* prims,
                                            const Set& set,
                                            CreateAllocFunc createAlloc,
                                            CreateNodeFunc createNode, UpdateNodeFunc updateNode,
                                            const CreateLeafFunc& createLeaf,
                                            const ProgressMonitor& progressMonitor,
                                            const Settings& settings)
      {
        typedef BuildRecordT<Set,typename Heuristic::Split> BuildRecord;

        typedef BuilderT<
          BuildRecord,
          Heuristic,
          Set,
          PrimRef,
          ReductionTy,
          decltype(createAlloc()),
          CreateAllocFunc,
          CreateNodeFunc,
          UpdateNodeFunc,
          CreateLeafFunc,
          DefaultCanCreateLeafFunc<PrimRef, Set>,
          DefaultCanCreateLeafSplitFunc<PrimRef, Set>,
          ProgressMonitor> Builder;

        /* instantiate builder */
        Builder builder(prims,
                        heuristic,
                        createAlloc,
                        createNode,
                        updateNode,
                        createLeaf,
                        DefaultCanCreateLeafFunc<PrimRef, Set>(),
                        DefaultCanCreateLeafSplitFunc<PrimRef, Set>(),
                        progressMonitor,
                        settings);

        /* build hierarchy */
        BuildRecord record(1,set);
        const ReductionTy root = builder.recurse(record,nullptr,true);
        _mm_mfence(); // to allow non-temporal stores during build
        return root;
      }

      template<
      typename ReductionTy,
        typename Heuristic,
        typename Set,
        typename PrimRef,
        typename CreateAllocFunc,
        typename CreateNodeFunc,
        typename UpdateNodeFunc,
        typename CreateLeafFunc,
        typename CanCreateLeafFunc,
        typename CanCreateLeafSplitFunc,
        typename ProgressMonitor>

        __noinline static ReductionTy build(Heuristic& heuristic,
                                            PrimRef* prims,
                                            const Set& set,
                                            CreateAllocFunc createAlloc,
                                            CreateNodeFunc createNode, UpdateNodeFunc updateNode,
                                            const CreateLeafFunc& createLeaf,
                                            const CanCreateLeafFunc& canCreateLeaf,
                                            const CanCreateLeafSplitFunc& canCreateLeafSplit,
                                            const ProgressMonitor& progressMonitor,
                                            const Settings& settings)
      {
        typedef BuildRecordT<Set,typename Heuristic::Split> BuildRecord;

        typedef BuilderT<
          BuildRecord,
          Heuristic,
          Set,
          PrimRef,
          ReductionTy,
          decltype(createAlloc()),
          CreateAllocFunc,
          CreateNodeFunc,
          UpdateNodeFunc,
          CreateLeafFunc,
          CanCreateLeafFunc,
          CanCreateLeafSplitFunc,
          ProgressMonitor> Builder;

        /* instantiate builder */
        Builder builder(prims,
                        heuristic,
                        createAlloc,
                        createNode,
                        updateNode,
                        createLeaf,
                        canCreateLeaf,
                        canCreateLeafSplit,
                        progressMonitor,
                        settings);

        /* build hierarchy */
        BuildRecord record(1,set);
        const ReductionTy root = builder.recurse(record,nullptr,true);
        _mm_mfence(); // to allow non-temporal stores during build
        return root;
      }
    };

    /* SAH builder that operates on an array of BuildRecords */
    struct BVHBuilderBinnedSAH
    {
      typedef PrimInfoRange Set;
      typedef HeuristicArrayBinningSAH<PrimRef,NUM_OBJECT_BINS> Heuristic;
      typedef GeneralBVHBuilder::BuildRecordT<Set,typename Heuristic::Split> BuildRecord;
      typedef GeneralBVHBuilder::Settings Settings;

      /*! special builder that propagates reduction over the tree */
      template<
      typename ReductionTy,
        typename CreateAllocFunc,
        typename CreateNodeFunc,
        typename UpdateNodeFunc,
        typename CreateLeafFunc,
        typename ProgressMonitor>

        static ReductionTy build(CreateAllocFunc createAlloc,
                                 CreateNodeFunc createNode, UpdateNodeFunc updateNode,
                                 const CreateLeafFunc& createLeaf,
                                 const ProgressMonitor& progressMonitor,
                                 PrimRef* prims, const PrimInfo& pinfo,
                                 const Settings& settings)
      {
        Heuristic heuristic(prims);
        return GeneralBVHBuilder::build<ReductionTy,Heuristic,Set,PrimRef>(
          heuristic,
          prims,
          PrimInfoRange(0,pinfo.size(),pinfo),
          createAlloc,
          createNode,
          updateNode,
          createLeaf,
          progressMonitor,
          settings);
      }

      /*! special builder that propagates reduction over the tree */
      template<
      typename ReductionTy,
        typename CreateAllocFunc,
        typename CreateNodeFunc,
        typename UpdateNodeFunc,
        typename CreateLeafFunc,
        typename CanCreateLeafFunc,
        typename CanCreateLeafSplitFunc,
        typename ProgressMonitor>

        static ReductionTy build(CreateAllocFunc createAlloc,
                                 CreateNodeFunc createNode, UpdateNodeFunc updateNode,
                                 const CreateLeafFunc& createLeaf,
                                 const CanCreateLeafFunc& canCreateLeaf,
                                 const CanCreateLeafSplitFunc& canCreateLeafSplit,
                                 const ProgressMonitor& progressMonitor,
                                 PrimRef* prims, const PrimInfo& pinfo,
                                 const Settings& settings)
      {
        Heuristic heuristic(prims);
        return GeneralBVHBuilder::build<ReductionTy,Heuristic,Set,PrimRef>(
          heuristic,
          prims,
          PrimInfoRange(0,pinfo.size(),pinfo),
          createAlloc,
          createNode,
          updateNode,
          createLeaf,
          canCreateLeaf,
          canCreateLeafSplit,
          progressMonitor,
          settings);
      }
    };

    /* Spatial SAH builder that operates on an double-buffered array of BuildRecords */
    struct BVHBuilderBinnedFastSpatialSAH
    {
      typedef PrimInfoExtRange Set;
      typedef Split2<BinSplit<NUM_OBJECT_BINS>,SpatialBinSplit<NUM_SPATIAL_BINS> > Split;
      typedef GeneralBVHBuilder::BuildRecordT<Set,Split> BuildRecord;
      typedef GeneralBVHBuilder::Settings Settings;

      static const unsigned int GEOMID_MASK = 0xFFFFFFFF >>     RESERVED_NUM_SPATIAL_SPLITS_GEOMID_BITS;
      static const unsigned int SPLITS_MASK = 0xFFFFFFFF << (32-RESERVED_NUM_SPATIAL_SPLITS_GEOMID_BITS);

      template<typename ReductionTy, typename UserCreateLeaf>
      struct CreateLeafExt
      {
        __forceinline CreateLeafExt (const UserCreateLeaf userCreateLeaf)
          : userCreateLeaf(userCreateLeaf) {}

        // __noinline is workaround for ICC2016 compiler bug
        template<typename Allocator>
        __noinline ReductionTy operator() (PrimRef* prims, const range<size_t>& range, Allocator alloc) const
        {
          for (size_t i=range.begin(); i<range.end(); i++)
            prims[i].lower.u &= GEOMID_MASK;

          return userCreateLeaf(prims,range,alloc);
        }

        const UserCreateLeaf userCreateLeaf;
      };

      /*! special builder that propagates reduction over the tree */
      template<
      typename ReductionTy,
        typename CreateAllocFunc,
        typename CreateNodeFunc,
        typename UpdateNodeFunc,
        typename CreateLeafFunc,
        typename SplitPrimitiveFunc,
        typename ProgressMonitor>

        static ReductionTy build(CreateAllocFunc createAlloc,
                                 CreateNodeFunc createNode,
                                 UpdateNodeFunc updateNode,
                                 const CreateLeafFunc& createLeaf,
                                 SplitPrimitiveFunc splitPrimitive,
                                 ProgressMonitor progressMonitor,
                                 PrimRef* prims,
                                 const size_t extSize,
                                 const PrimInfo& pinfo,
                                 const Settings& settings)
        {
          typedef HeuristicArraySpatialSAH<SplitPrimitiveFunc,PrimRef,NUM_OBJECT_BINS,NUM_SPATIAL_BINS> Heuristic;
          Heuristic heuristic(splitPrimitive,prims,pinfo);

          /* calculate total surface area */ // FIXME: this sum is not deterministic
          const float A = (float) parallel_reduce(size_t(0),pinfo.size(),0.0, [&] (const range<size_t>& r) -> double {

              double A = 0.0f;
              for (size_t i=r.begin(); i<r.end(); i++)
              {
                PrimRef& prim = prims[i];
                A += area(prim.bounds());
              }
              return A;
            },std::plus<double>());


          /* calculate maximum number of spatial splits per primitive */
          const unsigned int maxSplits = ((size_t)1 << RESERVED_NUM_SPATIAL_SPLITS_GEOMID_BITS)-1;
          const float f = 10.0f;

          const float invA = 1.0f / A;
          parallel_for( size_t(0), pinfo.size(), [&](const range<size_t>& r) {

              for (size_t i=r.begin(); i<r.end(); i++)
              {
                PrimRef& prim = prims[i];
                assert((prim.geomID() & SPLITS_MASK) == 0);
                // FIXME: is there a better general heuristic ?
                const float nf = ceilf(f*pinfo.size()*area(prim.bounds()) * invA);
                unsigned int n = 4+min((int)maxSplits-4, max(1, (int)(nf)));
                prim.lower.u |= n << (32-RESERVED_NUM_SPATIAL_SPLITS_GEOMID_BITS);
              }
            });

          return GeneralBVHBuilder::build<ReductionTy,Heuristic,Set,PrimRef>(
            heuristic,
            prims,
            PrimInfoExtRange(0,pinfo.size(),extSize,pinfo),
            createAlloc,
            createNode,
            updateNode,
            CreateLeafExt<ReductionTy,CreateLeafFunc>(createLeaf),
            progressMonitor,
            settings);
        }
    };

    /* Open/Merge SAH builder that operates on an array of BuildRecords */
    struct BVHBuilderBinnedOpenMergeSAH
    {
      static const size_t NUM_OBJECT_BINS_HQ = 32;
      typedef PrimInfoExtRange Set;
      typedef BinSplit<NUM_OBJECT_BINS_HQ> Split;
      typedef GeneralBVHBuilder::BuildRecordT<Set,Split> BuildRecord;
      typedef GeneralBVHBuilder::Settings Settings;
      
      /*! special builder that propagates reduction over the tree */
      template<
        typename ReductionTy, 
        typename BuildRef,
        typename CreateAllocFunc, 
        typename CreateNodeFunc, 
        typename UpdateNodeFunc, 
        typename CreateLeafFunc, 
        typename NodeOpenerFunc, 
        typename ProgressMonitor>
        
        static ReductionTy build(CreateAllocFunc createAlloc, 
                                 CreateNodeFunc createNode, 
                                 UpdateNodeFunc updateNode, 
                                 const CreateLeafFunc& createLeaf, 
                                 NodeOpenerFunc nodeOpenerFunc,
                                 ProgressMonitor progressMonitor,
                                 BuildRef* prims, 
                                 const size_t extSize,
                                 const PrimInfo& pinfo, 
                                 const Settings& settings)
      {
        typedef HeuristicArrayOpenMergeSAH<NodeOpenerFunc,BuildRef,NUM_OBJECT_BINS_HQ> Heuristic;
        Heuristic heuristic(nodeOpenerFunc,prims,settings.branchingFactor);

        return GeneralBVHBuilder::build<ReductionTy,Heuristic,Set,BuildRef>(
          heuristic,
          prims,
          PrimInfoExtRange(0,pinfo.size(),extSize,pinfo),
          createAlloc,
          createNode,
          updateNode,
          createLeaf,
          progressMonitor,
          settings);
      }
    };
  }
}
