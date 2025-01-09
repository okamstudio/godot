// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "bvh.h"
#include "bvh_builder.h"

#include "../builders/primrefgen.h"
#include "../builders/primrefgen_presplit.h"
#include "../builders/splitter.h"

#include "../geometry/linei.h"
#include "../geometry/triangle.h"
#include "../geometry/trianglev.h"
#include "../geometry/trianglev_mb.h"
#include "../geometry/trianglei.h"
#include "../geometry/quadv.h"
#include "../geometry/quadi.h"
#include "../geometry/object.h"
#include "../geometry/instance.h"
#include "../geometry/subgrid.h"

#include "../common/state.h"

namespace embree
{
  namespace isa
  {
    template<int N, typename Primitive>
    struct CreateLeafSpatial
    {
      typedef BVHN<N> BVH;
      typedef typename BVH::NodeRef NodeRef;

      __forceinline CreateLeafSpatial (BVH* bvh) : bvh(bvh) {}

      __forceinline NodeRef operator() (const PrimRef* prims, const range<size_t>& set, const FastAllocator::CachedAllocator& alloc) const
      {
        size_t n = set.size();
        size_t items = Primitive::blocks(n);
        size_t start = set.begin();
        Primitive* accel = (Primitive*) alloc.malloc1(items*sizeof(Primitive),BVH::byteAlignment);
        typename BVH::NodeRef node = BVH::encodeLeaf((char*)accel,items);
        for (size_t i=0; i<items; i++) {
          accel[i].fill(prims,start,set.end(),bvh->scene);
        }
        return node;
      }

      BVH* bvh;
    };

    template<int N, typename Mesh, typename Primitive, typename Splitter>
    struct BVHNBuilderFastSpatialSAH : public Builder
    {
      typedef BVHN<N> BVH;
      typedef typename BVH::NodeRef NodeRef;
      BVH* bvh;
      Scene* scene;
      Mesh* mesh;
      mvector<PrimRef> prims0;
      GeneralBVHBuilder::Settings settings;
      const float splitFactor;
      unsigned int geomID_ = std::numeric_limits<unsigned int>::max();
      unsigned int numPreviousPrimitives = 0;

      BVHNBuilderFastSpatialSAH (BVH* bvh, Scene* scene, const size_t sahBlockSize, const float intCost, const size_t minLeafSize, const size_t maxLeafSize, const size_t mode)
        : bvh(bvh), scene(scene), mesh(nullptr), prims0(scene->device,0), settings(sahBlockSize, minLeafSize, min(maxLeafSize,Primitive::max_size()*BVH::maxLeafBlocks), travCost, intCost, DEFAULT_SINGLE_THREAD_THRESHOLD),
          splitFactor(scene->device->max_spatial_split_replications) {}

      BVHNBuilderFastSpatialSAH (BVH* bvh, Mesh* mesh, const unsigned int geomID, const size_t sahBlockSize, const float intCost, const size_t minLeafSize, const size_t maxLeafSize, const size_t mode)
        : bvh(bvh), scene(nullptr), mesh(mesh), prims0(bvh->device,0), settings(sahBlockSize, minLeafSize, min(maxLeafSize,Primitive::max_size()*BVH::maxLeafBlocks), travCost, intCost, DEFAULT_SINGLE_THREAD_THRESHOLD),
          splitFactor(scene->device->max_spatial_split_replications), geomID_(geomID) {}

      // FIXME: shrink bvh->alloc in destructor here and in other builders too

      void build()
      {
        /* we reset the allocator when the mesh size changed */
        if (mesh && mesh->numPrimitives != numPreviousPrimitives) {
          bvh->alloc.clear();
        }

	/* skip build for empty scene */
        const size_t numOriginalPrimitives = mesh ? mesh->size() : scene->getNumPrimitives(Mesh::geom_type,false);
        numPreviousPrimitives = numOriginalPrimitives;
        if (numOriginalPrimitives == 0) {
          prims0.clear();
          bvh->clear();
          return;
        }

        const unsigned int maxGeomID = mesh ? geomID_ : scene->getMaxGeomID<Mesh,false>();
        const bool usePreSplits = scene->device->useSpatialPreSplits || (maxGeomID >= ((unsigned int)1 << (32-RESERVED_NUM_SPATIAL_SPLITS_GEOMID_BITS)));
        double t0 = bvh->preBuild(mesh ? "" : TOSTRING(isa) "::BVH" + toString(N) + (usePreSplits ? "BuilderFastSpatialPresplitSAH" : "BuilderFastSpatialSAH"));

        /* create primref array */
        const size_t numSplitPrimitives = max(numOriginalPrimitives,size_t(splitFactor*numOriginalPrimitives));
        prims0.resize(numSplitPrimitives);

        /* enable os_malloc for two level build */
        if (mesh)
          bvh->alloc.setOSallocation(true);
	
	NodeRef root(0);
	PrimInfo pinfo;
	

        if (likely(usePreSplits))
	  {		     
            /* spatial presplit SAH BVH builder */
	    pinfo = mesh ?
	      createPrimRefArray_presplit<Mesh,Splitter>(mesh,maxGeomID,numOriginalPrimitives,prims0,bvh->scene->progressInterface) :
	      createPrimRefArray_presplit<Mesh,Splitter>(scene,Mesh::geom_type,false,numOriginalPrimitives,prims0,bvh->scene->progressInterface);

	    const size_t node_bytes = pinfo.size()*sizeof(typename BVH::AABBNode)/(4*N);
	    const size_t leaf_bytes = size_t(1.2*Primitive::blocks(pinfo.size())*sizeof(Primitive));
	    bvh->alloc.init_estimate(node_bytes+leaf_bytes);
	    settings.singleThreadThreshold = bvh->alloc.fixSingleThreadThreshold(N,DEFAULT_SINGLE_THREAD_THRESHOLD,pinfo.size(),node_bytes+leaf_bytes);

	    settings.branchingFactor = N;
	    settings.maxDepth = BVH::maxBuildDepthLeaf;

	    /* call BVH builder */
	    root = BVHNBuilderVirtual<N>::build(&bvh->alloc,CreateLeafSpatial<N,Primitive>(bvh),bvh->scene->progressInterface,prims0.data(),pinfo,settings);
	  }
	else
	  {
            /* standard spatial split SAH BVH builder */
	    pinfo = mesh ?
	      createPrimRefArray(mesh,geomID_,numSplitPrimitives,prims0,bvh->scene->progressInterface) :
	      createPrimRefArray(scene,Mesh::geom_type,false,numSplitPrimitives,prims0,bvh->scene->progressInterface);
	
	    Splitter splitter(scene);

	    const size_t node_bytes = pinfo.size()*sizeof(typename BVH::AABBNode)/(4*N);
	    const size_t leaf_bytes = size_t(1.2*Primitive::blocks(pinfo.size())*sizeof(Primitive));
	    bvh->alloc.init_estimate(node_bytes+leaf_bytes);
	    settings.singleThreadThreshold = bvh->alloc.fixSingleThreadThreshold(N,DEFAULT_SINGLE_THREAD_THRESHOLD,pinfo.size(),node_bytes+leaf_bytes);

	    settings.branchingFactor = N;
	    settings.maxDepth = BVH::maxBuildDepthLeaf;

	    /* call BVH builder */
	    root = BVHBuilderBinnedFastSpatialSAH::build<NodeRef>(
								  typename BVH::CreateAlloc(bvh),
								  typename BVH::AABBNode::Create2(),
								  typename BVH::AABBNode::Set2(),
								  CreateLeafSpatial<N,Primitive>(bvh),
								  splitter,
								  bvh->scene->progressInterface,
								  prims0.data(),
								  numSplitPrimitives,
								  pinfo,settings);

	    /* ==================== */
	  }

        bvh->set(root,LBBox3fa(pinfo.geomBounds),pinfo.size());
        bvh->layoutLargeNodes(size_t(pinfo.size()*0.005f));

	/* clear temporary data for static geometry */
	if (scene && scene->isStaticAccel()) {
          prims0.clear();
        }
	bvh->cleanup();
        bvh->postBuild(t0);
      }

      void clear() {
        prims0.clear();
      }
    };

    /************************************************************************************/
    /************************************************************************************/
    /************************************************************************************/
    /************************************************************************************/


#if defined(EMBREE_GEOMETRY_TRIANGLE)

    Builder* BVH4Triangle4SceneBuilderFastSpatialSAH  (void* bvh, Scene* scene, size_t mode) { return new BVHNBuilderFastSpatialSAH<4,TriangleMesh,Triangle4,TriangleSplitterFactory>((BVH4*)bvh,scene,4,1.0f,4,scene->device->max_triangles_per_leaf,mode); }
    Builder* BVH4Triangle4vSceneBuilderFastSpatialSAH (void* bvh, Scene* scene, size_t mode) { return new BVHNBuilderFastSpatialSAH<4,TriangleMesh,Triangle4v,TriangleSplitterFactory>((BVH4*)bvh,scene,4,1.0f,4,scene->device->max_triangles_per_leaf,mode); }
    Builder* BVH4Triangle4iSceneBuilderFastSpatialSAH (void* bvh, Scene* scene, size_t mode) { return new BVHNBuilderFastSpatialSAH<4,TriangleMesh,Triangle4i,TriangleSplitterFactory>((BVH4*)bvh,scene,4,1.0f,4,scene->device->max_triangles_per_leaf,mode); }

#if defined(__AVX__)
    Builder* BVH8Triangle4SceneBuilderFastSpatialSAH  (void* bvh, Scene* scene, size_t mode) { return new BVHNBuilderFastSpatialSAH<8,TriangleMesh,Triangle4,TriangleSplitterFactory>((BVH8*)bvh,scene,4,1.0f,4,inf,mode); }
    Builder* BVH8Triangle4vSceneBuilderFastSpatialSAH  (void* bvh, Scene* scene, size_t mode) { return new BVHNBuilderFastSpatialSAH<8,TriangleMesh,Triangle4v,TriangleSplitterFactory>((BVH8*)bvh,scene,4,1.0f,4,inf,mode); }
#endif
#endif

#if defined(EMBREE_GEOMETRY_QUAD)
    Builder* BVH4Quad4vSceneBuilderFastSpatialSAH  (void* bvh, Scene* scene, size_t mode) { return new BVHNBuilderFastSpatialSAH<4,QuadMesh,Quad4v,QuadSplitterFactory>((BVH4*)bvh,scene,4,1.0f,4,inf,mode); }

#if defined(__AVX__)
    Builder* BVH8Quad4vSceneBuilderFastSpatialSAH  (void* bvh, Scene* scene, size_t mode) { return new BVHNBuilderFastSpatialSAH<8,QuadMesh,Quad4v,QuadSplitterFactory>((BVH8*)bvh,scene,4,1.0f,4,inf,mode); }
#endif

#endif
  }
}
