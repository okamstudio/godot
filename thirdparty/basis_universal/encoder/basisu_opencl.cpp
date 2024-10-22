// basisu_opencl.cpp
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
#include "basisu_opencl.h"

// If 1, the kernel source code will come from encoders/ocl_kernels.h. Otherwise, it will be read from the "ocl_kernels.cl" file in the current directory (for development).
#define BASISU_USE_OCL_KERNELS_HEADER (1)
#define BASISU_OCL_KERNELS_FILENAME "ocl_kernels.cl"

#if BASISU_SUPPORT_OPENCL

#include "basisu_enc.h"

// We only use OpenCL v1.2 or less.
#define CL_TARGET_OPENCL_VERSION 120

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

#define BASISU_OPENCL_ASSERT_ON_ANY_ERRORS (1)

namespace basisu
{
#if BASISU_USE_OCL_KERNELS_HEADER
#include "basisu_ocl_kernels.h"
#endif

	static void ocl_error_printf(const char* pFmt, ...)
	{
		va_list args;
		va_start(args, pFmt);
		error_vprintf(pFmt, args);
		va_end(args);

#if BASISU_OPENCL_ASSERT_ON_ANY_ERRORS
		assert(0);
#endif
	}

	class ocl
	{
	public:
		ocl() 
		{
			memset(&m_dev_fp_config, 0, sizeof(m_dev_fp_config));
			
			m_ocl_mutex.lock();
			m_ocl_mutex.unlock();
		}

		~ocl()
		{
		}

		bool is_initialized() const { return m_device_id != nullptr; }

		cl_device_id get_device_id() const { return m_device_id; }
		cl_context get_context() const { return m_context; }
		cl_command_queue get_command_queue() { return m_command_queue; }
		cl_program get_program() const { return m_program; }

		bool init(bool force_serialization)
		{
			deinit();

			interval_timer tm;
			tm.start();

			cl_uint num_platforms = 0;
			cl_int ret = clGetPlatformIDs(0, NULL, &num_platforms);
			if (ret != CL_SUCCESS)
			{
				ocl_error_printf("ocl::init: clGetPlatformIDs() failed with %i\n", ret);
				return false;
			}

			if ((!num_platforms) || (num_platforms > INT_MAX))
			{
				ocl_error_printf("ocl::init: clGetPlatformIDs() returned an invalid number of num_platforms\n");
				return false;
			}

			std::vector<cl_platform_id> platforms(num_platforms);

			ret = clGetPlatformIDs(num_platforms, platforms.data(), NULL);
			if (ret != CL_SUCCESS)
			{
				ocl_error_printf("ocl::init: clGetPlatformIDs() failed\n");
				return false;
			}

			cl_uint num_devices = 0;
			ret = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 1, &m_device_id, &num_devices);

			if (ret == CL_DEVICE_NOT_FOUND)
			{
				ocl_error_printf("ocl::init: Couldn't get any GPU device ID's, trying CL_DEVICE_TYPE_CPU\n");

				ret = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_CPU, 1, &m_device_id, &num_devices);
			}

			if (ret != CL_SUCCESS)
			{
				ocl_error_printf("ocl::init: Unable to get any device ID's\n");

				m_device_id = nullptr;
				return false;
			}

			ret = clGetDeviceInfo(m_device_id,
				CL_DEVICE_SINGLE_FP_CONFIG,
				sizeof(m_dev_fp_config),
				&m_dev_fp_config,
				nullptr);
			if (ret != CL_SUCCESS)
			{
				ocl_error_printf("ocl::init: clGetDeviceInfo() failed\n");
				return false;
			}

			char plat_vers[256];
			size_t rv = 0;
			ret = clGetPlatformInfo(platforms[0], CL_PLATFORM_VERSION, sizeof(plat_vers), plat_vers, &rv);
			if (ret == CL_SUCCESS)
				printf("OpenCL platform version: \"%s\"\n", plat_vers);

			// Serialize CL calls with the AMD driver to avoid lockups when multiple command queues per thread are used. This sucks, but what can we do?
			m_use_mutex = (strstr(plat_vers, "AMD") != nullptr) || force_serialization;

			printf("Serializing OpenCL calls across threads: %u\n", (uint32_t)m_use_mutex);

			m_context = clCreateContext(nullptr, 1, &m_device_id, nullptr, nullptr, &ret);
			if (ret != CL_SUCCESS)
			{
				ocl_error_printf("ocl::init: clCreateContext() failed\n");

				m_device_id = nullptr;
				m_context = nullptr;
				return false;
			}

			m_command_queue = clCreateCommandQueue(m_context, m_device_id, 0, &ret);
			if (ret != CL_SUCCESS)
			{
				ocl_error_printf("ocl::init: clCreateCommandQueue() failed\n");

				deinit();
				return false;
			}
						
			printf("OpenCL init time: %3.3f secs\n", tm.get_elapsed_secs());

			return true;
		}
				
		bool deinit()
		{
			if (m_program)
			{
				clReleaseProgram(m_program);
				m_program = nullptr;
			}

			if (m_command_queue)
			{
				clReleaseCommandQueue(m_command_queue);
				m_command_queue = nullptr;
			}

			if (m_context)
			{
				clReleaseContext(m_context);
				m_context = nullptr;
			}

			m_device_id = nullptr;

			return true;
		}

		cl_command_queue create_command_queue()
		{
			cl_serializer serializer(this);

			cl_int ret = 0;
			cl_command_queue p = clCreateCommandQueue(m_context, m_device_id, 0, &ret);
			if (ret != CL_SUCCESS)
				return nullptr;

			return p;
		}

		void destroy_command_queue(cl_command_queue p)
		{
			if (p)
			{
				cl_serializer serializer(this);

				clReleaseCommandQueue(p);
			}
		}

		bool init_program(const char* pSrc, size_t src_size)
		{
			cl_int ret;

			if (m_program != nullptr)
			{
				clReleaseProgram(m_program);
				m_program = nullptr;
			}

			m_program = clCreateProgramWithSource(m_context, 1, (const char**)&pSrc, (const size_t*)&src_size, &ret);
			if (ret != CL_SUCCESS)
			{
				ocl_error_printf("ocl::init_program: clCreateProgramWithSource() failed!\n");
				return false;
			}

			std::string options;
			if (m_dev_fp_config & CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT)
			{
				options += "-cl-fp32-correctly-rounded-divide-sqrt";
			}

			options += " -cl-std=CL1.2";
			//options += " -cl-opt-disable";
			//options += " -cl-mad-enable";
			//options += " -cl-fast-relaxed-math";

			ret = clBuildProgram(m_program, 1, &m_device_id,
				options.size() ? options.c_str() : nullptr,  // options
				nullptr,  // notify
				nullptr); // user_data

			if (ret != CL_SUCCESS)
			{
				const cl_int build_program_result = ret;

				size_t ret_val_size;
				ret = clGetProgramBuildInfo(m_program, m_device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &ret_val_size);
				if (ret != CL_SUCCESS)
				{
					ocl_error_printf("ocl::init_program: clGetProgramBuildInfo() failed!\n");
					return false;
				}

				std::vector<char> build_log(ret_val_size + 1);

				ret = clGetProgramBuildInfo(m_program, m_device_id, CL_PROGRAM_BUILD_LOG, ret_val_size, build_log.data(), NULL);

				ocl_error_printf("\nclBuildProgram() failed with error %i:\n%s", build_program_result, build_log.data());

				return false;
			}

			return true;
		}

		cl_kernel create_kernel(const char* pName)
		{
			if (!m_program)
				return nullptr;

			cl_serializer serializer(this);

			cl_int ret;
			cl_kernel kernel = clCreateKernel(m_program, pName, &ret);
			if (ret != CL_SUCCESS)
			{
				ocl_error_printf("ocl::create_kernel: clCreateKernel() failed!\n");
				return nullptr;
			}

			return kernel;
		}

		bool destroy_kernel(cl_kernel k)
		{
			if (k)
			{
				cl_serializer serializer(this);

				cl_int ret = clReleaseKernel(k);
				if (ret != CL_SUCCESS)
				{
					ocl_error_printf("ocl::destroy_kernel: clReleaseKernel() failed!\n");
					return false;
				}
			}
			return true;
		}

		cl_mem alloc_read_buffer(size_t size)
		{
			cl_serializer serializer(this);

			cl_int ret;
			cl_mem obj = clCreateBuffer(m_context, CL_MEM_READ_ONLY, size, NULL, &ret);
			if (ret != CL_SUCCESS)
			{
				ocl_error_printf("ocl::alloc_read_buffer: clCreateBuffer() failed!\n");
				return nullptr;
			}

			return obj;
		}

		cl_mem alloc_and_init_read_buffer(cl_command_queue command_queue, const void *pInit, size_t size)
		{
			cl_serializer serializer(this);

			cl_int ret;
			cl_mem obj = clCreateBuffer(m_context, CL_MEM_READ_ONLY, size, NULL, &ret);
			if (ret != CL_SUCCESS)
			{
				ocl_error_printf("ocl::alloc_and_init_read_buffer: clCreateBuffer() failed!\n");
				return nullptr;
			}

#if 0
			if (!write_to_buffer(command_queue, obj, pInit, size))
			{
				destroy_buffer(obj);
				return nullptr;
			}
#else
			ret = clEnqueueWriteBuffer(command_queue, obj, CL_TRUE, 0, size, pInit, 0, NULL, NULL);
			if (ret != CL_SUCCESS)
			{
				ocl_error_printf("ocl::alloc_and_init_read_buffer: clEnqueueWriteBuffer() failed!\n");
				return nullptr;
			}
#endif

			return obj;
		}

		cl_mem alloc_write_buffer(size_t size)
		{
			cl_serializer serializer(this);

			cl_int ret;
			cl_mem obj = clCreateBuffer(m_context, CL_MEM_WRITE_ONLY, size, NULL, &ret);
			if (ret != CL_SUCCESS)
			{
				ocl_error_printf("ocl::alloc_write_buffer: clCreateBuffer() failed!\n");
				return nullptr;
			}

			return obj;
		}
				
		bool destroy_buffer(cl_mem buf)
		{
			if (buf)
			{
				cl_serializer serializer(this);

				cl_int ret = clReleaseMemObject(buf);
				if (ret != CL_SUCCESS)
				{
					ocl_error_printf("ocl::destroy_buffer: clReleaseMemObject() failed!\n");
					return false;
				}
			}

			return true;
		}

		bool write_to_buffer(cl_command_queue command_queue, cl_mem clmem, const void* d, const size_t m)
		{
			cl_serializer serializer(this);

			cl_int ret = clEnqueueWriteBuffer(command_queue, clmem, CL_TRUE, 0, m, d, 0, NULL, NULL);
			if (ret != CL_SUCCESS)
			{
				ocl_error_printf("ocl::write_to_buffer: clEnqueueWriteBuffer() failed!\n");
				return false;
			}

			return true;
		}

		bool read_from_buffer(cl_command_queue command_queue, const cl_mem clmem, void* d, size_t m)
		{
			cl_serializer serializer(this);

			cl_int ret = clEnqueueReadBuffer(command_queue, clmem, CL_TRUE, 0, m, d, 0, NULL, NULL);
			if (ret != CL_SUCCESS)
			{
				ocl_error_printf("ocl::read_from_buffer: clEnqueueReadBuffer() failed!\n");
				return false;
			}

			return true;
		}

		cl_mem create_read_image_u8(uint32_t width, uint32_t height, const void* pPixels, uint32_t bytes_per_pixel, bool normalized)
		{
			cl_image_format fmt = get_image_format(bytes_per_pixel, normalized);

			cl_image_desc desc;
			memset(&desc, 0, sizeof(desc));
			desc.image_type = CL_MEM_OBJECT_IMAGE2D;
			desc.image_width = width;
			desc.image_height = height;
			desc.image_row_pitch = width * bytes_per_pixel;

			cl_serializer serializer(this);

			cl_int ret;
			cl_mem img = clCreateImage(m_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, &fmt, &desc, (void*)pPixels, &ret);
			if (ret != CL_SUCCESS)
			{
				ocl_error_printf("ocl::create_read_image_u8: clCreateImage() failed!\n");
				return nullptr;
			}

			return img;
		}

		cl_mem create_write_image_u8(uint32_t width, uint32_t height, uint32_t bytes_per_pixel, bool normalized)
		{
			cl_image_format fmt = get_image_format(bytes_per_pixel, normalized);

			cl_image_desc desc;
			memset(&desc, 0, sizeof(desc));
			desc.image_type = CL_MEM_OBJECT_IMAGE2D;
			desc.image_width = width;
			desc.image_height = height;

			cl_serializer serializer(this);

			cl_int ret;
			cl_mem img = clCreateImage(m_context, CL_MEM_WRITE_ONLY, &fmt, &desc, nullptr, &ret);
			if (ret != CL_SUCCESS)
			{
				ocl_error_printf("ocl::create_write_image_u8: clCreateImage() failed!\n");
				return nullptr;
			}

			return img;
		}

		bool read_from_image(cl_command_queue command_queue, cl_mem img, void* pPixels, uint32_t ofs_x, uint32_t ofs_y, uint32_t width, uint32_t height)
		{
			cl_serializer serializer(this);

			size_t origin[3] = { ofs_x, ofs_y, 0 }, region[3] = { width, height, 1 };

			cl_int err = clEnqueueReadImage(command_queue, img, CL_TRUE, origin, region, 0, 0, pPixels, 0, NULL, NULL);
			if (err != CL_SUCCESS)
			{
				ocl_error_printf("ocl::read_from_image: clEnqueueReadImage() failed!\n");
				return false;
			}

			return true;
		}

		bool run_1D(cl_command_queue command_queue, const cl_kernel kernel, size_t num_items)
		{
			cl_serializer serializer(this);

			cl_int ret = clEnqueueNDRangeKernel(command_queue, kernel,
				1,  // work_dim
				nullptr, // global_work_offset
				&num_items, // global_work_size
				nullptr, // local_work_size
				0, // num_events_in_wait_list
				nullptr, // event_wait_list
				nullptr // event
			);

			if (ret != CL_SUCCESS)
			{
				ocl_error_printf("ocl::run_1D: clEnqueueNDRangeKernel() failed!\n");
				return false;
			}

			return true;
		}

		bool run_2D(cl_command_queue command_queue, const cl_kernel kernel, size_t width, size_t height)
		{
			cl_serializer serializer(this);

			size_t num_global_items[2] = { width, height };
			//size_t num_local_items[2] = { 1, 1 };

			cl_int ret = clEnqueueNDRangeKernel(command_queue, kernel,
				2,  // work_dim
				nullptr, // global_work_offset
				num_global_items, // global_work_size
				nullptr, // local_work_size
				0, // num_events_in_wait_list
				nullptr, // event_wait_list
				nullptr // event
			);

			if (ret != CL_SUCCESS)
			{
				ocl_error_printf("ocl::run_2D: clEnqueueNDRangeKernel() failed!\n");
				return false;
			}

			return true;
		}

		bool run_2D(cl_command_queue command_queue, const cl_kernel kernel, size_t ofs_x, size_t ofs_y, size_t width, size_t height)
		{
			cl_serializer serializer(this);

			size_t global_ofs[2] = { ofs_x, ofs_y };
			size_t num_global_items[2] = { width, height };
			//size_t num_local_items[2] = { 1, 1 };

			cl_int ret = clEnqueueNDRangeKernel(command_queue, kernel,
				2,  // work_dim
				global_ofs, // global_work_offset
				num_global_items, // global_work_size
				nullptr, // local_work_size
				0, // num_events_in_wait_list
				nullptr, // event_wait_list
				nullptr // event
			);

			if (ret != CL_SUCCESS)
			{
				ocl_error_printf("ocl::run_2D: clEnqueueNDRangeKernel() failed!\n");
				return false;
			}

			return true;
		}

		void flush(cl_command_queue command_queue)
		{
			cl_serializer serializer(this);

			clFlush(command_queue);
			clFinish(command_queue);
		}

		template<typename T>
		bool set_kernel_arg(cl_kernel kernel, uint32_t index, const T& obj)
		{
			cl_serializer serializer(this);

			cl_int ret = clSetKernelArg(kernel, index, sizeof(T), (void*)&obj);
			if (ret != CL_SUCCESS)
			{
				ocl_error_printf("ocl::set_kernel_arg: clSetKernelArg() failed!\n");
				return false;
			}
			return true;
		}

		template<typename T>
		bool set_kernel_args(cl_kernel kernel, const T& obj1)
		{
			cl_serializer serializer(this);

			cl_int ret = clSetKernelArg(kernel, 0, sizeof(T), (void*)&obj1);
			if (ret != CL_SUCCESS)
			{
				ocl_error_printf("ocl::set_kernel_arg: clSetKernelArg() failed!\n");
				return false;
			}
			return true;
		}

#define BASISU_CHECK_ERR if (ret != CL_SUCCESS)	{ ocl_error_printf("ocl::set_kernel_args: clSetKernelArg() failed!\n"); return false; }

		template<typename T, typename U>
		bool set_kernel_args(cl_kernel kernel, const T& obj1, const U& obj2)
		{
			cl_serializer serializer(this);
			cl_int ret = clSetKernelArg(kernel, 0, sizeof(T), (void*)&obj1); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 1, sizeof(U), (void*)&obj2); BASISU_CHECK_ERR
			return true;
		}

		template<typename T, typename U, typename V>
		bool set_kernel_args(cl_kernel kernel, const T& obj1, const U& obj2, const V& obj3)
		{
			cl_serializer serializer(this);
			cl_int ret = clSetKernelArg(kernel, 0, sizeof(T), (void*)&obj1); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 1, sizeof(U), (void*)&obj2); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 2, sizeof(V), (void*)&obj3); BASISU_CHECK_ERR
			return true;
		}

		template<typename T, typename U, typename V, typename W>
		bool set_kernel_args(cl_kernel kernel, const T& obj1, const U& obj2, const V& obj3, const W& obj4)
		{
			cl_serializer serializer(this);
			cl_int ret = clSetKernelArg(kernel, 0, sizeof(T), (void*)&obj1); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 1, sizeof(U), (void*)&obj2); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 2, sizeof(V), (void*)&obj3); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 3, sizeof(W), (void*)&obj4); BASISU_CHECK_ERR
			return true;
		}

		template<typename T, typename U, typename V, typename W, typename X>
		bool set_kernel_args(cl_kernel kernel, const T& obj1, const U& obj2, const V& obj3, const W& obj4, const X& obj5)
		{
			cl_serializer serializer(this);
			cl_int ret = clSetKernelArg(kernel, 0, sizeof(T), (void*)&obj1); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 1, sizeof(U), (void*)&obj2); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 2, sizeof(V), (void*)&obj3); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 3, sizeof(W), (void*)&obj4); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 4, sizeof(X), (void*)&obj5); BASISU_CHECK_ERR
			return true;
		}

		template<typename T, typename U, typename V, typename W, typename X, typename Y>
		bool set_kernel_args(cl_kernel kernel, const T& obj1, const U& obj2, const V& obj3, const W& obj4, const X& obj5, const Y& obj6)
		{
			cl_serializer serializer(this);
			cl_int ret = clSetKernelArg(kernel, 0, sizeof(T), (void*)&obj1); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 1, sizeof(U), (void*)&obj2); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 2, sizeof(V), (void*)&obj3); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 3, sizeof(W), (void*)&obj4); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 4, sizeof(X), (void*)&obj5); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 5, sizeof(Y), (void*)&obj6); BASISU_CHECK_ERR
			return true;
		}

		template<typename T, typename U, typename V, typename W, typename X, typename Y, typename Z>
		bool set_kernel_args(cl_kernel kernel, const T& obj1, const U& obj2, const V& obj3, const W& obj4, const X& obj5, const Y& obj6, const Z& obj7)
		{
			cl_serializer serializer(this);
			cl_int ret = clSetKernelArg(kernel, 0, sizeof(T), (void*)&obj1); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 1, sizeof(U), (void*)&obj2); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 2, sizeof(V), (void*)&obj3); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 3, sizeof(W), (void*)&obj4); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 4, sizeof(X), (void*)&obj5); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 5, sizeof(Y), (void*)&obj6); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 6, sizeof(Z), (void*)&obj7); BASISU_CHECK_ERR
			return true;
		}

		template<typename T, typename U, typename V, typename W, typename X, typename Y, typename Z, typename A>
		bool set_kernel_args(cl_kernel kernel, const T& obj1, const U& obj2, const V& obj3, const W& obj4, const X& obj5, const Y& obj6, const Z& obj7, const A& obj8)
		{
			cl_serializer serializer(this);
			cl_int ret = clSetKernelArg(kernel, 0, sizeof(T), (void*)&obj1); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 1, sizeof(U), (void*)&obj2); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 2, sizeof(V), (void*)&obj3); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 3, sizeof(W), (void*)&obj4); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 4, sizeof(X), (void*)&obj5); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 5, sizeof(Y), (void*)&obj6); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 6, sizeof(Z), (void*)&obj7); BASISU_CHECK_ERR
			ret = clSetKernelArg(kernel, 7, sizeof(A), (void*)&obj8); BASISU_CHECK_ERR
			return true;
		}
#undef BASISU_CHECK_ERR

	private:
		cl_device_id m_device_id = nullptr;
		cl_context m_context = nullptr;
		cl_command_queue m_command_queue = nullptr;
		cl_program m_program = nullptr;
		cl_device_fp_config m_dev_fp_config;
		
		bool m_use_mutex = false;
		std::mutex m_ocl_mutex;

		// This helper object is used to optionally serialize all calls to the CL driver after initialization.
		// Currently this is only used to work around race conditions in the Windows AMD driver.
		struct cl_serializer
		{
			inline cl_serializer(const cl_serializer&);
			cl_serializer& operator= (const cl_serializer&);

			inline cl_serializer(ocl *p) : m_p(p)
			{
				if (m_p->m_use_mutex)
					m_p->m_ocl_mutex.lock();
			}

			inline ~cl_serializer()
			{
				if (m_p->m_use_mutex)
					m_p->m_ocl_mutex.unlock();
			}

		private:
			ocl* m_p;
		};
		
		cl_image_format get_image_format(uint32_t bytes_per_pixel, bool normalized)
		{
			cl_image_format fmt;
			switch (bytes_per_pixel)
			{
			case 1: fmt.image_channel_order = CL_LUMINANCE; break;
			case 2: fmt.image_channel_order = CL_RG; break;
			case 3: fmt.image_channel_order = CL_RGB; break;
			case 4: fmt.image_channel_order = CL_RGBA; break;
			default: assert(0); fmt.image_channel_order = CL_LUMINANCE; break;
			}

			fmt.image_channel_data_type = normalized ? CL_UNORM_INT8 : CL_UNSIGNED_INT8;
			return fmt;
		}
	};
		
	// Library blobal state
	ocl g_ocl;
			
	bool opencl_init(bool force_serialization)
	{
		if (g_ocl.is_initialized())
		{
			assert(0);
			return false;
		}

		if (!g_ocl.init(force_serialization))
		{
			ocl_error_printf("opencl_init: Failed initializing OpenCL\n");
			return false;
		}

		const char* pKernel_src = nullptr;
		size_t kernel_src_size = 0;
		uint8_vec kernel_src;

#if BASISU_USE_OCL_KERNELS_HEADER
		pKernel_src = reinterpret_cast<const char*>(ocl_kernels_cl);
		kernel_src_size = ocl_kernels_cl_len;
#else
		if (!read_file_to_vec(BASISU_OCL_KERNELS_FILENAME, kernel_src))
		{
			ocl_error_printf("opencl_init: Cannot read OpenCL kernel source file \"%s\"\n", BASISU_OCL_KERNELS_FILENAME);
			g_ocl.deinit();
			return false;
		}
			
		pKernel_src = (char*)kernel_src.data();
		kernel_src_size = kernel_src.size();
#endif
		
		if (!kernel_src_size)
		{
			ocl_error_printf("opencl_init: Invalid OpenCL kernel source file \"%s\"\n", BASISU_OCL_KERNELS_FILENAME);
			g_ocl.deinit();
			return false;
		}

		if (!g_ocl.init_program(pKernel_src, kernel_src_size))
		{
			ocl_error_printf("opencl_init: Failed compiling OpenCL program\n");
			g_ocl.deinit();
			return false;
		}
								
		printf("OpenCL support initialized successfully\n");

		return true;
	}

	void opencl_deinit()
	{
		g_ocl.deinit();
	}

	bool opencl_is_available()
	{
		return g_ocl.is_initialized();
	}

	struct opencl_context
	{
		uint32_t m_ocl_total_pixel_blocks;
		cl_mem m_ocl_pixel_blocks;

		cl_command_queue m_command_queue;

		cl_kernel m_ocl_encode_etc1s_blocks_kernel;
		cl_kernel m_ocl_refine_endpoint_clusterization_kernel;
		cl_kernel m_ocl_encode_etc1s_from_pixel_cluster_kernel;
		cl_kernel m_ocl_find_optimal_selector_clusters_for_each_block_kernel;
		cl_kernel m_ocl_determine_selectors_kernel;
	};

	opencl_context_ptr opencl_create_context()
	{
		if (!opencl_is_available())
		{
			ocl_error_printf("opencl_create_context: OpenCL not initialized\n");
			assert(0);
			return nullptr;
		}

		interval_timer tm;
		tm.start();

		opencl_context* pContext = static_cast<opencl_context * >(calloc(sizeof(opencl_context), 1));
		if (!pContext)
			return nullptr;
				
		// To avoid driver bugs in some drivers - serialize this. Likely not necessary, we don't know.
		// https://community.intel.com/t5/OpenCL-for-CPU/Bug-report-clCreateKernelsInProgram-is-not-thread-safe/td-p/1159771
		
		pContext->m_command_queue = g_ocl.create_command_queue();
		if (!pContext->m_command_queue)
		{
			ocl_error_printf("opencl_create_context: Failed creating OpenCL command queue!\n");
			opencl_destroy_context(pContext);
			return nullptr;
		}

		pContext->m_ocl_encode_etc1s_blocks_kernel = g_ocl.create_kernel("encode_etc1s_blocks");
		if (!pContext->m_ocl_encode_etc1s_blocks_kernel)
		{
			ocl_error_printf("opencl_create_context: Failed creating OpenCL kernel encode_etc1s_block\n");
			opencl_destroy_context(pContext);
			return nullptr;
		}

		pContext->m_ocl_refine_endpoint_clusterization_kernel = g_ocl.create_kernel("refine_endpoint_clusterization");
		if (!pContext->m_ocl_refine_endpoint_clusterization_kernel)
		{
			ocl_error_printf("opencl_create_context: Failed creating OpenCL kernel refine_endpoint_clusterization\n");
			opencl_destroy_context(pContext);
			return nullptr;
		}

		pContext->m_ocl_encode_etc1s_from_pixel_cluster_kernel = g_ocl.create_kernel("encode_etc1s_from_pixel_cluster");
		if (!pContext->m_ocl_encode_etc1s_from_pixel_cluster_kernel)
		{
			ocl_error_printf("opencl_create_context: Failed creating OpenCL kernel encode_etc1s_from_pixel_cluster\n");
			opencl_destroy_context(pContext);
			return nullptr;
		}

		pContext->m_ocl_find_optimal_selector_clusters_for_each_block_kernel = g_ocl.create_kernel("find_optimal_selector_clusters_for_each_block");
		if (!pContext->m_ocl_find_optimal_selector_clusters_for_each_block_kernel)
		{
			ocl_error_printf("opencl_create_context: Failed creating OpenCL kernel find_optimal_selector_clusters_for_each_block\n");
			opencl_destroy_context(pContext);
			return nullptr;
		}

		pContext->m_ocl_determine_selectors_kernel = g_ocl.create_kernel("determine_selectors");
		if (!pContext->m_ocl_determine_selectors_kernel)
		{
			ocl_error_printf("opencl_create_context: Failed creating OpenCL kernel determine_selectors\n");
			opencl_destroy_context(pContext);
			return nullptr;
		}

		debug_printf("opencl_create_context: Elapsed time: %f secs\n", tm.get_elapsed_secs());

		return pContext;
	}

	void opencl_destroy_context(opencl_context_ptr pContext)
	{
		if (!pContext)
			return;

		interval_timer tm;
		tm.start();

		g_ocl.destroy_buffer(pContext->m_ocl_pixel_blocks);

		g_ocl.destroy_kernel(pContext->m_ocl_determine_selectors_kernel);
		g_ocl.destroy_kernel(pContext->m_ocl_find_optimal_selector_clusters_for_each_block_kernel);
		g_ocl.destroy_kernel(pContext->m_ocl_encode_etc1s_from_pixel_cluster_kernel);
		g_ocl.destroy_kernel(pContext->m_ocl_encode_etc1s_blocks_kernel);
		g_ocl.destroy_kernel(pContext->m_ocl_refine_endpoint_clusterization_kernel);

		g_ocl.destroy_command_queue(pContext->m_command_queue);
			
		memset(pContext, 0, sizeof(opencl_context));

		free(pContext);

		debug_printf("opencl_destroy_context: Elapsed time: %f secs\n", tm.get_elapsed_secs());
	}

#pragma pack(push, 1)
	struct cl_encode_etc1s_param_struct
	{
		int m_total_blocks;
		int m_perceptual;
		int m_total_perms;
	};
#pragma pack(pop)

	bool opencl_set_pixel_blocks(opencl_context_ptr pContext, uint32_t total_blocks, const cl_pixel_block* pPixel_blocks)
	{
		if (!opencl_is_available())
			return false;

		if (pContext->m_ocl_pixel_blocks)
		{
			g_ocl.destroy_buffer(pContext->m_ocl_pixel_blocks);
			pContext->m_ocl_pixel_blocks = nullptr;
		}

		pContext->m_ocl_pixel_blocks = g_ocl.alloc_and_init_read_buffer(pContext->m_command_queue, pPixel_blocks, sizeof(cl_pixel_block) * total_blocks);
		if (!pContext->m_ocl_pixel_blocks)
			return false;

		pContext->m_ocl_total_pixel_blocks = total_blocks;

		return true;
	}

	bool opencl_encode_etc1s_blocks(opencl_context_ptr pContext, etc_block* pOutput_blocks, bool perceptual, uint32_t total_perms)
	{
		if (!opencl_is_available())
			return false;

		interval_timer tm;
		tm.start();

		assert(pContext->m_ocl_pixel_blocks);
		if (!pContext->m_ocl_pixel_blocks)
			return false;
				
		cl_encode_etc1s_param_struct ps;
		ps.m_total_blocks = pContext->m_ocl_total_pixel_blocks;
		ps.m_perceptual = perceptual;
		ps.m_total_perms = total_perms;

		bool status = false;

		cl_mem vars = g_ocl.alloc_and_init_read_buffer(pContext->m_command_queue , &ps, sizeof(ps));
		cl_mem block_buf = g_ocl.alloc_write_buffer(sizeof(etc_block) * pContext->m_ocl_total_pixel_blocks);
		
		if (!vars || !block_buf)
			goto exit;

		if (!g_ocl.set_kernel_args(pContext->m_ocl_encode_etc1s_blocks_kernel, vars, pContext->m_ocl_pixel_blocks, block_buf))
			goto exit;

		if (!g_ocl.run_2D(pContext->m_command_queue, pContext->m_ocl_encode_etc1s_blocks_kernel, pContext->m_ocl_total_pixel_blocks, 1))
			goto exit;

		if (!g_ocl.read_from_buffer(pContext->m_command_queue, block_buf, pOutput_blocks, pContext->m_ocl_total_pixel_blocks * sizeof(etc_block)))
			goto exit;

		status = true;

		debug_printf("opencl_encode_etc1s_blocks: Elapsed time: %3.3f secs\n", tm.get_elapsed_secs());

exit:
		g_ocl.destroy_buffer(block_buf);
		g_ocl.destroy_buffer(vars);

		return status;
	}

	bool opencl_encode_etc1s_pixel_clusters(
		opencl_context_ptr pContext,
		etc_block* pOutput_blocks,
		uint32_t total_clusters,
		const cl_pixel_cluster* pClusters,
		uint64_t total_pixels,
		const color_rgba* pPixels, const uint32_t* pPixel_weights,
		bool perceptual, uint32_t total_perms)
	{
		if (!opencl_is_available())
			return false;

		interval_timer tm;
		tm.start();
				
		cl_encode_etc1s_param_struct ps;
		ps.m_total_blocks = total_clusters;
		ps.m_perceptual = perceptual;
		ps.m_total_perms = total_perms;

		bool status = false;

		if (sizeof(size_t) == sizeof(uint32_t))
		{
			if ( ((sizeof(cl_pixel_cluster) * total_clusters) > UINT32_MAX) ||
				 ((sizeof(color_rgba) * total_pixels) > UINT32_MAX) ||
				 ((sizeof(uint32_t) * total_pixels) > UINT32_MAX) )
			{
				return false;
			}
		}
				
		cl_mem vars = g_ocl.alloc_and_init_read_buffer(pContext->m_command_queue , &ps, sizeof(ps));
		cl_mem input_clusters = g_ocl.alloc_and_init_read_buffer(pContext->m_command_queue, pClusters, (size_t)(sizeof(cl_pixel_cluster) * total_clusters));
		cl_mem input_pixels = g_ocl.alloc_and_init_read_buffer(pContext->m_command_queue, pPixels, (size_t)(sizeof(color_rgba) * total_pixels));
		cl_mem weights_buf = g_ocl.alloc_and_init_read_buffer(pContext->m_command_queue, pPixel_weights, (size_t)(sizeof(uint32_t) * total_pixels));
		cl_mem block_buf = g_ocl.alloc_write_buffer(sizeof(etc_block) * total_clusters);

		if (!vars || !input_clusters || !input_pixels || !weights_buf || !block_buf)
			goto exit;

		if (!g_ocl.set_kernel_args(pContext->m_ocl_encode_etc1s_from_pixel_cluster_kernel, vars, input_clusters, input_pixels, weights_buf, block_buf))
			goto exit;

		if (!g_ocl.run_2D(pContext->m_command_queue, pContext->m_ocl_encode_etc1s_from_pixel_cluster_kernel, total_clusters, 1))
			goto exit;

		if (!g_ocl.read_from_buffer(pContext->m_command_queue, block_buf, pOutput_blocks, sizeof(etc_block) * total_clusters))
			goto exit;

		status = true;

		debug_printf("opencl_encode_etc1s_pixel_clusters: Elapsed time: %3.3f secs\n", tm.get_elapsed_secs());

	exit:
		g_ocl.destroy_buffer(block_buf);
		g_ocl.destroy_buffer(weights_buf);
		g_ocl.destroy_buffer(input_pixels);
		g_ocl.destroy_buffer(input_clusters);
		g_ocl.destroy_buffer(vars);

		return status;
	}

#pragma pack(push, 1)
	struct cl_rec_param_struct
	{
		int m_total_blocks;
		int m_perceptual;
	};
#pragma pack(pop)

	bool opencl_refine_endpoint_clusterization(
		opencl_context_ptr pContext,
		const cl_block_info_struct* pPixel_block_info,
		uint32_t total_clusters,
		const cl_endpoint_cluster_struct* pCluster_info,
		const uint32_t* pSorted_block_indices,
		uint32_t* pOutput_cluster_indices,
		bool perceptual)
	{
		if (!opencl_is_available())
			return false;

		interval_timer tm;
		tm.start();

		assert(pContext->m_ocl_pixel_blocks);
		if (!pContext->m_ocl_pixel_blocks)
			return false;
				
		cl_rec_param_struct ps;
		ps.m_total_blocks = pContext->m_ocl_total_pixel_blocks;
		ps.m_perceptual = perceptual;

		bool status = false;

		cl_mem pixel_block_info = g_ocl.alloc_and_init_read_buffer(pContext->m_command_queue, pPixel_block_info, sizeof(cl_block_info_struct) * pContext->m_ocl_total_pixel_blocks);
		cl_mem cluster_info = g_ocl.alloc_and_init_read_buffer(pContext->m_command_queue, pCluster_info, sizeof(cl_endpoint_cluster_struct) * total_clusters);
		cl_mem sorted_block_indices = g_ocl.alloc_and_init_read_buffer(pContext->m_command_queue, pSorted_block_indices, sizeof(uint32_t) * pContext->m_ocl_total_pixel_blocks);
		cl_mem output_buf = g_ocl.alloc_write_buffer(sizeof(uint32_t) * pContext->m_ocl_total_pixel_blocks);
		
		if (!pixel_block_info || !cluster_info || !sorted_block_indices || !output_buf)
			goto exit;

		if (!g_ocl.set_kernel_args(pContext->m_ocl_refine_endpoint_clusterization_kernel, ps, pContext->m_ocl_pixel_blocks, pixel_block_info, cluster_info, sorted_block_indices, output_buf))
			goto exit;

		if (!g_ocl.run_2D(pContext->m_command_queue, pContext->m_ocl_refine_endpoint_clusterization_kernel, pContext->m_ocl_total_pixel_blocks, 1))
			goto exit;

		if (!g_ocl.read_from_buffer(pContext->m_command_queue, output_buf, pOutput_cluster_indices, pContext->m_ocl_total_pixel_blocks * sizeof(uint32_t)))
			goto exit;

		debug_printf("opencl_refine_endpoint_clusterization: Elapsed time: %3.3f secs\n", tm.get_elapsed_secs());
		
		status = true;

exit:
		g_ocl.destroy_buffer(pixel_block_info);
		g_ocl.destroy_buffer(cluster_info);
		g_ocl.destroy_buffer(sorted_block_indices);
		g_ocl.destroy_buffer(output_buf);

		return status;
	}

	bool opencl_find_optimal_selector_clusters_for_each_block(
		opencl_context_ptr pContext,
		const fosc_block_struct* pInput_block_info,	// one per block
		uint32_t total_input_selectors,
		const fosc_selector_struct* pInput_selectors,
		const uint32_t* pSelector_cluster_indices,
		uint32_t* pOutput_selector_cluster_indices, // one per block
		bool perceptual)
	{
		if (!opencl_is_available())
			return false;

		interval_timer tm;
		tm.start();

		assert(pContext->m_ocl_pixel_blocks);
		if (!pContext->m_ocl_pixel_blocks)
			return false;

		fosc_param_struct ps;
		ps.m_total_blocks = pContext->m_ocl_total_pixel_blocks;
		ps.m_perceptual = perceptual;
		
		bool status = false;

		cl_mem input_block_info = g_ocl.alloc_and_init_read_buffer(pContext->m_command_queue, pInput_block_info, sizeof(fosc_block_struct) * pContext->m_ocl_total_pixel_blocks);
		cl_mem input_selectors = g_ocl.alloc_and_init_read_buffer(pContext->m_command_queue, pInput_selectors, sizeof(fosc_selector_struct) * total_input_selectors);
		cl_mem selector_cluster_indices = g_ocl.alloc_and_init_read_buffer(pContext->m_command_queue, pSelector_cluster_indices, sizeof(uint32_t) * total_input_selectors);
		cl_mem output_selector_cluster_indices = g_ocl.alloc_write_buffer(sizeof(uint32_t) * pContext->m_ocl_total_pixel_blocks);

		if (!input_block_info || !input_selectors || !selector_cluster_indices || !output_selector_cluster_indices)
			goto exit;

		if (!g_ocl.set_kernel_args(pContext->m_ocl_find_optimal_selector_clusters_for_each_block_kernel, ps, pContext->m_ocl_pixel_blocks, input_block_info, input_selectors, selector_cluster_indices, output_selector_cluster_indices))
			goto exit;

		if (!g_ocl.run_2D(pContext->m_command_queue, pContext->m_ocl_find_optimal_selector_clusters_for_each_block_kernel, pContext->m_ocl_total_pixel_blocks, 1))
			goto exit;

		if (!g_ocl.read_from_buffer(pContext->m_command_queue, output_selector_cluster_indices, pOutput_selector_cluster_indices, pContext->m_ocl_total_pixel_blocks * sizeof(uint32_t)))
			goto exit;

		debug_printf("opencl_find_optimal_selector_clusters_for_each_block: Elapsed time: %3.3f secs\n", tm.get_elapsed_secs());

		status = true;

	exit:
		g_ocl.destroy_buffer(input_block_info);
		g_ocl.destroy_buffer(input_selectors);
		g_ocl.destroy_buffer(selector_cluster_indices);
		g_ocl.destroy_buffer(output_selector_cluster_indices);

		return status;
	}

	bool opencl_determine_selectors(
		opencl_context_ptr pContext,
		const color_rgba* pInput_etc_color5_and_inten,
		etc_block* pOutput_blocks,
		bool perceptual)
	{
		if (!opencl_is_available())
			return false;

		interval_timer tm;
		tm.start();

		assert(pContext->m_ocl_pixel_blocks);
		if (!pContext->m_ocl_pixel_blocks)
			return false;

		ds_param_struct ps;
		ps.m_total_blocks = pContext->m_ocl_total_pixel_blocks;
		ps.m_perceptual = perceptual;

		bool status = false;

		cl_mem input_etc_color5_intens = g_ocl.alloc_and_init_read_buffer(pContext->m_command_queue, pInput_etc_color5_and_inten, sizeof(color_rgba) * pContext->m_ocl_total_pixel_blocks);
		cl_mem output_blocks = g_ocl.alloc_write_buffer(sizeof(etc_block) * pContext->m_ocl_total_pixel_blocks);

		if (!input_etc_color5_intens || !output_blocks)
			goto exit;

		if (!g_ocl.set_kernel_args(pContext->m_ocl_determine_selectors_kernel, ps, pContext->m_ocl_pixel_blocks, input_etc_color5_intens, output_blocks))
			goto exit;

		if (!g_ocl.run_2D(pContext->m_command_queue, pContext->m_ocl_determine_selectors_kernel, pContext->m_ocl_total_pixel_blocks, 1))
			goto exit;

		if (!g_ocl.read_from_buffer(pContext->m_command_queue, output_blocks, pOutput_blocks, pContext->m_ocl_total_pixel_blocks * sizeof(etc_block)))
			goto exit;

		debug_printf("opencl_determine_selectors: Elapsed time: %3.3f secs\n", tm.get_elapsed_secs());
					
		status = true;
	
	exit:
		g_ocl.destroy_buffer(input_etc_color5_intens);
		g_ocl.destroy_buffer(output_blocks);

		return status;
	}

#else	
namespace basisu
{
	// No OpenCL support - all dummy functions that return false;
	bool opencl_init(bool force_serialization)
	{
		BASISU_NOTE_UNUSED(force_serialization);

		return false;
	}

	void opencl_deinit()
	{
	}

	bool opencl_is_available()
	{
		return false;
	}

	opencl_context_ptr opencl_create_context()
	{
		return nullptr;
	}

	void opencl_destroy_context(opencl_context_ptr context)
	{
		BASISU_NOTE_UNUSED(context);
	}

	bool opencl_set_pixel_blocks(opencl_context_ptr pContext, uint32_t total_blocks, const cl_pixel_block* pPixel_blocks)
	{
		BASISU_NOTE_UNUSED(pContext);
		BASISU_NOTE_UNUSED(total_blocks);
		BASISU_NOTE_UNUSED(pPixel_blocks);

		return false;
	}

	bool opencl_encode_etc1s_blocks(opencl_context_ptr pContext, etc_block* pOutput_blocks, bool perceptual, uint32_t total_perms)
	{
		BASISU_NOTE_UNUSED(pContext);
		BASISU_NOTE_UNUSED(pOutput_blocks);
		BASISU_NOTE_UNUSED(perceptual);
		BASISU_NOTE_UNUSED(total_perms);

		return false;
	}

	bool opencl_encode_etc1s_pixel_clusters(
		opencl_context_ptr pContext,
		etc_block* pOutput_blocks,
		uint32_t total_clusters,
		const cl_pixel_cluster* pClusters,
		uint64_t total_pixels,
		const color_rgba* pPixels, const uint32_t *pPixel_weights,
		bool perceptual, uint32_t total_perms)
	{
		BASISU_NOTE_UNUSED(pContext);
		BASISU_NOTE_UNUSED(pOutput_blocks);
		BASISU_NOTE_UNUSED(total_clusters);
		BASISU_NOTE_UNUSED(pClusters);
		BASISU_NOTE_UNUSED(total_pixels);
		BASISU_NOTE_UNUSED(pPixels);
		BASISU_NOTE_UNUSED(pPixel_weights);
		BASISU_NOTE_UNUSED(perceptual);
		BASISU_NOTE_UNUSED(total_perms);
		
		return false;
	}

	bool opencl_refine_endpoint_clusterization(
		opencl_context_ptr pContext,
		const cl_block_info_struct* pPixel_block_info,
		uint32_t total_clusters,
		const cl_endpoint_cluster_struct* pCluster_info,
		const uint32_t* pSorted_block_indices,
		uint32_t* pOutput_cluster_indices,
		bool perceptual)
	{
		BASISU_NOTE_UNUSED(pContext);
		BASISU_NOTE_UNUSED(pPixel_block_info);
		BASISU_NOTE_UNUSED(total_clusters);
		BASISU_NOTE_UNUSED(pCluster_info);
		BASISU_NOTE_UNUSED(pSorted_block_indices);
		BASISU_NOTE_UNUSED(pOutput_cluster_indices);
		BASISU_NOTE_UNUSED(perceptual);

		return false;
	}

	bool opencl_find_optimal_selector_clusters_for_each_block(
		opencl_context_ptr pContext,
		const fosc_block_struct* pInput_block_info,	// one per block
		uint32_t total_input_selectors,
		const fosc_selector_struct* pInput_selectors,
		const uint32_t* pSelector_cluster_indices,
		uint32_t* pOutput_selector_cluster_indices, // one per block
		bool perceptual)
	{
		BASISU_NOTE_UNUSED(pContext);
		BASISU_NOTE_UNUSED(pInput_block_info);
		BASISU_NOTE_UNUSED(total_input_selectors);
		BASISU_NOTE_UNUSED(pInput_selectors);
		BASISU_NOTE_UNUSED(pSelector_cluster_indices);
		BASISU_NOTE_UNUSED(pOutput_selector_cluster_indices);
		BASISU_NOTE_UNUSED(perceptual);

		return false;
	}

	bool opencl_determine_selectors(
		opencl_context_ptr pContext,
		const color_rgba* pInput_etc_color5_and_inten,
		etc_block* pOutput_blocks,
		bool perceptual)
	{
		BASISU_NOTE_UNUSED(pContext);
		BASISU_NOTE_UNUSED(pInput_etc_color5_and_inten);
		BASISU_NOTE_UNUSED(pOutput_blocks);
		BASISU_NOTE_UNUSED(perceptual);

		return false;
	}

#endif // BASISU_SUPPORT_OPENCL

} // namespace basisu
