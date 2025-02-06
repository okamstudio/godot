// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "default.h"
#include "state.h"
#include "accel.h"

namespace embree
{
  class BVH4Factory;
  class BVH8Factory;
  struct TaskArena;

  class Device : public State, public MemoryMonitorInterface
  {
    ALIGNED_CLASS_(16);
    
  public:
    
    /*! allocator that performs unified shared memory allocations */
    template<typename T, size_t alignment>
    struct allocator
    {
      typedef T value_type;
      typedef T* pointer;
      typedef const T* const_pointer;
      typedef T& reference;
      typedef const T& const_reference;
      typedef std::size_t size_type;
      typedef std::ptrdiff_t difference_type;
      
      allocator() {}
      
      allocator(Device* device)
        : device(device) {}
      
      __forceinline pointer allocate( size_type n ) {
        assert(device);
        return (pointer) device->malloc(n*sizeof(T),alignment);
      }
      
      __forceinline void deallocate( pointer p, size_type n ) {
        if (device) device->free(p);
      }
      
      __forceinline void construct( pointer p, const_reference val ) {
        new (p) T(val);
      }
      
      __forceinline void destroy( pointer p ) {
        p->~T();
      }
      
      Device* device = nullptr;
    };

    /*! vector class that performs aligned allocations from Device object */
    template<typename T>
    using vector = vector_t<T,allocator<T,std::alignment_of<T>::value>>;

    template<typename T, size_t alignment>
    using avector = vector_t<T,allocator<T,alignment>>;

  public:

    /*! Device construction */
    Device (const char* cfg);

    /*! Device destruction */
    virtual ~Device ();

    /*! prints info about the device */
    void print();

    /*! sets the error code */
    void setDeviceErrorCode(RTCError error, std::string const& msg = "");

    /*! returns and clears the error code */
    RTCError getDeviceErrorCode();

    /*! Returns the string representation for the error code. For example, for RTC_ERROR_UNKNOWN the string "RTC_ERROR_UNKNOWN" will be returned. */
    static char* getDeviceErrorString();

    /*! returns the last error message */
    const char* getDeviceLastErrorMessage();

    /*! sets the error code */
    static void setThreadErrorCode(RTCError error, std::string const& msg = "");

    /*! returns and clears the error code */
    static RTCError getThreadErrorCode();


    /*! returns the last error message */
    static const char* getThreadLastErrorMessage();

    /*! processes error codes, do not call directly */
    static void process_error(Device* device, RTCError error, const char* str);

    /*! invokes the memory monitor callback */
    void memoryMonitor(ssize_t bytes, bool post);

    /*! sets the size of the software cache. */
    void setCacheSize(size_t bytes);

    /*! sets a property */
    void setProperty(const RTCDeviceProperty prop, ssize_t val);

    /*! gets a property */
    ssize_t getProperty(const RTCDeviceProperty prop);

    /*! enter device by setting up some global state */
    virtual void enter() {}

    /*! leave device by setting up some global state */
    virtual void leave() {}

    /*! buffer allocation */
    virtual void* malloc(size_t size, size_t align);

    /*! buffer deallocation */
    virtual void free(void* ptr);

  private:

    /*! initializes the tasking system */
    void initTaskingSystem(size_t numThreads);

    /*! shuts down the tasking system */
    void exitTaskingSystem();

    std::unique_ptr<TaskArena> arena;

  public:

    // use tasking system arena to execute func
    void execute(bool join, const std::function<void()>& func);

    /*! some variables that can be set via rtcSetParameter1i for debugging purposes */
  public:
    static ssize_t debug_int0;
    static ssize_t debug_int1;
    static ssize_t debug_int2;
    static ssize_t debug_int3;

  public:
    std::unique_ptr<BVH4Factory> bvh4_factory;
#if defined(EMBREE_TARGET_SIMD8)
    std::unique_ptr<BVH8Factory> bvh8_factory;
#endif

  private:
    static const std::vector<std::string> error_strings;

  public:
    static const char* getErrorString(RTCError error);

  };

#if defined(EMBREE_SYCL_SUPPORT)
     
  class DeviceGPU : public Device
  {
  public:

    DeviceGPU(sycl::context sycl_context, const char* cfg);
    ~DeviceGPU();

    virtual void enter() override;
    virtual void leave() override;
    virtual void* malloc(size_t size, size_t align) override;
    virtual void free(void* ptr) override;

    /* set SYCL device */
    void setSYCLDevice(const sycl::device sycl_device);

  private:
    sycl::context gpu_context;
    sycl::device  gpu_device;
        
    unsigned int gpu_maxWorkGroupSize;
    unsigned int gpu_maxComputeUnits;

  public:
    void* dispatchGlobalsPtr = nullptr;

  public:
    inline sycl::device  &getGPUDevice()  { return gpu_device; }        
    inline sycl::context &getGPUContext() { return gpu_context; }    

    inline unsigned int getGPUMaxWorkGroupSize() { return gpu_maxWorkGroupSize; }

    void init_rthw_level_zero();
    void init_rthw_opencl();
  };

#endif

  struct DeviceEnterLeave
  {
    DeviceEnterLeave (RTCDevice hdevice);
    DeviceEnterLeave (RTCScene hscene);
    DeviceEnterLeave (RTCGeometry hgeometry);
    DeviceEnterLeave (RTCBuffer hbuffer);
    ~DeviceEnterLeave();
  private:
    Device* device;
  };
}
