// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
 
#pragma once

#include "default.h"
#include "device.h"
#include "builder.h"
#include "scene_triangle_mesh.h"
#include "scene_quad_mesh.h"
#include "scene_user_geometry.h"
#include "scene_instance.h"
#include "scene_instance_array.h"
#include "scene_curves.h"
#include "scene_line_segments.h"
#include "scene_subdiv_mesh.h"
#include "scene_grid_mesh.h"
#include "scene_points.h"
#include "../subdiv/tessellation_cache.h"

#include "acceln.h"
#include "geometry.h"

#if defined(EMBREE_SYCL_SUPPORT)
#include "../sycl/rthwif_embree_builder.h"
#endif

namespace embree
{
  struct TaskGroup;

  /*! Base class all scenes are derived from */
  class Scene : public AccelN
  {
    ALIGNED_CLASS_USM_(std::alignment_of<Scene>::value);

  public:
    template<typename Ty, bool mblur = false>
      class Iterator
      {
      public:
      Iterator ()  {}
      
      Iterator (Scene* scene, bool all = false) 
      : scene(scene), all(all) {}
      
      __forceinline Ty* at(const size_t i)
      {
        Geometry* geom = scene->geometries[i].ptr;
        if (geom == nullptr) return nullptr;
        if (!all && !geom->isEnabled()) return nullptr;
        const size_t mask = geom->getTypeMask() & Ty::geom_type; 
        if (!(mask)) return nullptr;
        if ((geom->numTimeSteps != 1) != mblur) return nullptr;
        return (Ty*) geom;
      }

      __forceinline Ty* operator[] (const size_t i) {
        return at(i);
      }

      __forceinline size_t size() const {
        return scene->size();
      }
      
      __forceinline size_t numPrimitives() const {
        return scene->getNumPrimitives(Ty::geom_type,mblur);
      }

      __forceinline size_t maxPrimitivesPerGeometry() 
      {
        size_t ret = 0;
        for (size_t i=0; i<scene->size(); i++) {
          Ty* mesh = at(i);
          if (mesh == nullptr) continue;
          ret = max(ret,mesh->size());
        }
        return ret;
      }

      __forceinline unsigned int maxGeomID() 
      {
        unsigned int ret = 0;
        for (size_t i=0; i<scene->size(); i++) {
          Ty* mesh = at(i);
          if (mesh == nullptr) continue;
          ret = max(ret,(unsigned int)i);
        }
        return ret;
      }

      __forceinline unsigned maxTimeStepsPerGeometry()
      {
        unsigned ret = 0;
        for (size_t i=0; i<scene->size(); i++) {
          Ty* mesh = at(i);
          if (mesh == nullptr) continue;
          ret = max(ret,mesh->numTimeSteps);
        }
        return ret;
      }
      
    private:
      Scene* scene;
      bool all;
      };

      class Iterator2
      {
      public:
      Iterator2 () {}
      
      Iterator2 (Scene* scene, Geometry::GTypeMask typemask, bool mblur) 
      : scene(scene), typemask(typemask), mblur(mblur) {}
      
      __forceinline Geometry* at(const size_t i)
      {
        Geometry* geom = scene->geometries[i].ptr;
        if (geom == nullptr) return nullptr;
        if (!geom->isEnabled()) return nullptr;
        if (!(geom->getTypeMask() & typemask)) return nullptr;
        if ((geom->numTimeSteps != 1) != mblur) return nullptr;
        return geom;
      }

      __forceinline Geometry* operator[] (const size_t i) {
        return at(i);
      }

      __forceinline size_t size() const {
        return scene->size();
      }
      
    private:
      Scene* scene;
      Geometry::GTypeMask typemask;
      bool mblur;
    };

  public:
    
    /*! Scene construction */
    Scene (Device* device);

    /*! Scene destruction */
    ~Scene () noexcept;

  private:
    
    /*! class is non-copyable */
    Scene (const Scene& other) DELETED; // do not implement
    Scene& operator= (const Scene& other) DELETED; // do not implement

  public:
    void createTriangleAccel();
    void createTriangleMBAccel();
    void createQuadAccel();
    void createQuadMBAccel();
    void createHairAccel();
    void createHairMBAccel();
    void createSubdivAccel();
    void createSubdivMBAccel();
    void createUserGeometryAccel();
    void createUserGeometryMBAccel();
    void createInstanceAccel();
    void createInstanceMBAccel();
    void createInstanceExpensiveAccel();
    void createInstanceExpensiveMBAccel();
    void createInstanceArrayAccel();
    void createInstanceArrayMBAccel();
    void createGridAccel();
    void createGridMBAccel();

    /*! prints statistics about the scene */
    void printStatistics();

    /*! clears the scene */
    void clear();

    /*! detaches some geometry */
    void detachGeometry(size_t geomID);

    void setBuildQuality(RTCBuildQuality quality_flags);
    RTCBuildQuality getBuildQuality() const;
    
    void setSceneFlags(RTCSceneFlags scene_flags);
    RTCSceneFlags getSceneFlags() const;

    void build_cpu_accels();
    void build_gpu_accels();
    void commit (bool join);
    void commit_task ();
    void build () {}

    /* return number of geometries */
    __forceinline size_t size() const { return geometries.size(); }
    
    /* bind geometry to the scene */
    unsigned int bind (unsigned geomID, Ref<Geometry> geometry);
    
    /* determines if scene is modified */
    __forceinline bool isModified() const { return modified; }

    /* sets modified flag */
    __forceinline void setModified(bool f = true) { 
      modified = f; 
    }

    __forceinline bool hasMotionBlur() const { return maxTimeSegments > 1; };

    __forceinline uint32_t getMaxTimeSegments() const { return maxTimeSegments; };

    __forceinline bool isGeometryModified(size_t geomID)
    {
      Ref<Geometry>& g = geometries[geomID];
      if (!g) return false;
      return g->getModCounter() > geometryModCounters_[geomID];
    }

  protected:

    void checkIfModifiedAndSet ();

  public:

    /* get mesh by ID */
    __forceinline       Geometry* get(size_t i)       { assert(i < geometries.size()); return geometries[i].ptr; }
    __forceinline const Geometry* get(size_t i) const { assert(i < geometries.size()); return geometries[i].ptr; }

    template<typename Mesh>
      __forceinline       Mesh* get(size_t i)       { 
      assert(i < geometries.size()); 
      assert(geometries[i]->getTypeMask() & Mesh::geom_type);
      return (Mesh*)geometries[i].ptr; 
    }
    template<typename Mesh>
      __forceinline const Mesh* get(size_t i) const { 
      assert(i < geometries.size()); 
      assert(geometries[i]->getTypeMask() & Mesh::geom_type);
      return (Mesh*)geometries[i].ptr; 
    }

    template<typename Mesh>
    __forceinline Mesh* getSafe(size_t i) {
      assert(i < geometries.size());
      if (geometries[i] == null) return nullptr;
      if (!(geometries[i]->getTypeMask() & Mesh::geom_type)) return nullptr;
      else return (Mesh*) geometries[i].ptr;
    }

    __forceinline Ref<Geometry> get_locked(size_t i)  {
      Lock<MutexSys> lock(geometriesMutex);
      assert(i < geometries.size()); 
      return geometries[i]; 
    }

    /* flag decoding */
    __forceinline bool isFastAccel() const { return !isCompactAccel() && !isRobustAccel(); }
    __forceinline bool isCompactAccel() const { return scene_flags & RTC_SCENE_FLAG_COMPACT; }
    __forceinline bool isRobustAccel()  const { return scene_flags & RTC_SCENE_FLAG_ROBUST; }
    __forceinline bool isStaticAccel()  const { return !(scene_flags & RTC_SCENE_FLAG_DYNAMIC); }
    __forceinline bool isDynamicAccel() const { return scene_flags & RTC_SCENE_FLAG_DYNAMIC; }
    
    __forceinline bool hasArgumentFilterFunction() const {
      return scene_flags & RTC_SCENE_FLAG_FILTER_FUNCTION_IN_ARGUMENTS;
    }
    
    __forceinline bool hasGeometryFilterFunction() {
      return world.numFilterFunctions != 0;
    }
      
    __forceinline bool hasFilterFunction() {
      return hasArgumentFilterFunction() || hasGeometryFilterFunction();
    }
    
    void* createQBVH6Accel();

  public:
    Device* device;

  public:
    IDPool<unsigned,0xFFFFFFFE> id_pool;
    Device::vector<Ref<Geometry>> geometries = device; //!< list of all user geometries
    avector<unsigned int> geometryModCounters_;
    Device::vector<float*> vertices = device;
    
  public:
    /* these are to detect if we need to recreate the acceleration structures */
    bool flags_modified;
    unsigned int enabled_geometry_types;
    
    RTCSceneFlags scene_flags;
    RTCBuildQuality quality_flags;
    MutexSys buildMutex;
    MutexSys geometriesMutex;

#if defined(EMBREE_SYCL_SUPPORT)
  public:
    BBox3f hwaccel_bounds = empty;
    AccelBuffer hwaccel;   // the buffer containing the HW acceleration structures corresponding to the scene. One for each time segment, stored in a contiguous chunk of memory.
    size_t hwaccel_stride; // the stride between two HW acceleration structures for different time segments stored in hwaccel.

    __forceinline char* getHWAccel(uint32_t time_segment) const {
      char* ptr = (char*)hwaccel.data() + time_segment * hwaccel_stride;
      return ptr;
    }
#endif
    
  private:
    bool modified;            //!< true if scene got modified
    uint32_t maxTimeSegments; //!< maximal number of motion blur time segments in scene

  public:

    std::unique_ptr<TaskGroup> taskGroup;
    
  public:
    struct BuildProgressMonitorInterface : public BuildProgressMonitor {
      BuildProgressMonitorInterface(Scene* scene) 
      : scene(scene) {}
      void operator() (size_t dn) const { scene->progressMonitor(double(dn)); }
    private:
      Scene* scene;
    };
    BuildProgressMonitorInterface progressInterface;
    RTCProgressMonitorFunction progress_monitor_function;
    void* progress_monitor_ptr;
    std::atomic<size_t> progress_monitor_counter;
    void progressMonitor(double nprims);
    void setProgressMonitorFunction(RTCProgressMonitorFunction func, void* ptr);

  private:
    GeometryCounts world;               //!< counts for geometry

  public:

    __forceinline size_t numPrimitives() const {
      return world.size();
    }

    __forceinline size_t getNumPrimitives(Geometry::GTypeMask mask, bool mblur) const
    {
      size_t count = 0;
      
      if (mask & Geometry::MTY_TRIANGLE_MESH)
        count += mblur ? world.numMBTriangles : world.numTriangles;
      
      if (mask & Geometry::MTY_QUAD_MESH)
        count += mblur ? world.numMBQuads : world.numQuads;
      
      if (mask & Geometry::MTY_CURVE2)
        count += mblur ? world.numMBLineSegments : world.numLineSegments;
      
      if (mask & Geometry::MTY_CURVE4)
        count += mblur ? world.numMBBezierCurves : world.numBezierCurves;
      
      if (mask & Geometry::MTY_POINTS)
        count += mblur ? world.numMBPoints : world.numPoints;
      
      if (mask & Geometry::MTY_SUBDIV_MESH)
        count += mblur ? world.numMBSubdivPatches : world.numSubdivPatches;
      
      if (mask & Geometry::MTY_USER_GEOMETRY)
        count += mblur ? world.numMBUserGeometries : world.numUserGeometries;
      
      if (mask & Geometry::MTY_INSTANCE_CHEAP)
        count += mblur ? world.numMBInstancesCheap : world.numInstancesCheap;
      
      if (mask & Geometry::MTY_INSTANCE_EXPENSIVE)
        count += mblur  ? world.numMBInstancesExpensive : world.numInstancesExpensive;

      if (mask & Geometry::MTY_INSTANCE_ARRAY)
        count += mblur  ? world.numMBInstanceArrays : world.numInstanceArrays;

      if (mask & Geometry::MTY_GRID_MESH)
        count += mblur  ? world.numMBGrids : world.numGrids;
      
      return count;
    }

    __forceinline size_t getNumSubPrimitives(Geometry::GTypeMask mask, bool mblur) const
    {
      size_t count = 0;
      
      if (mask & Geometry::MTY_GRID_MESH)
        count += mblur  ? world.numMBSubGrids : world.numSubGrids;

      Geometry::GTypeMask new_mask = (Geometry::GTypeMask)(mask & ~Geometry::MTY_GRID_MESH);
      count += getNumPrimitives(new_mask, mblur);
      
      return count;
    }
    
    template<typename Mesh, bool mblur>
    __forceinline unsigned getNumTimeSteps()
    {
      if (!mblur)
        return 1;

      Scene::Iterator<Mesh,mblur> iter(this);
      return iter.maxTimeStepsPerGeometry();
    }

    template<typename Mesh, bool mblur>
    __forceinline unsigned int getMaxGeomID()
    {
      Scene::Iterator<Mesh,mblur> iter(this);
      return iter.maxGeomID();
    }
  };
}
