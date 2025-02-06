//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// eglext_angle.h: ANGLE modifications to the eglext.h header file.
//   Currently we don't include this file directly, we patch eglext.h
//   to include it implicitly so it is visible throughout our code.

#ifndef INCLUDE_EGL_EGLEXT_ANGLE_
#define INCLUDE_EGL_EGLEXT_ANGLE_

// clang-format off

#ifndef EGL_ANGLE_robust_resource_initialization
#define EGL_ANGLE_robust_resource_initialization 1
#define EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE 0x3453
#endif /* EGL_ANGLE_robust_resource_initialization */

#ifndef EGL_ANGLE_keyed_mutex
#define EGL_ANGLE_keyed_mutex 1
#define EGL_DXGI_KEYED_MUTEX_ANGLE        0x33A2
#endif /* EGL_ANGLE_keyed_mutex */

#ifndef EGL_ANGLE_d3d_texture_client_buffer
#define EGL_ANGLE_d3d_texture_client_buffer 1
#define EGL_D3D_TEXTURE_ANGLE 0x33A3
#define EGL_TEXTURE_OFFSET_X_ANGLE 0x3490
#define EGL_TEXTURE_OFFSET_Y_ANGLE 0x3491
#define EGL_D3D11_TEXTURE_PLANE_ANGLE 0x3492
#define EGL_D3D11_TEXTURE_ARRAY_SLICE_ANGLE 0x3493
#endif /* EGL_ANGLE_d3d_texture_client_buffer */

#ifndef EGL_ANGLE_software_display
#define EGL_ANGLE_software_display 1
#define EGL_SOFTWARE_DISPLAY_ANGLE ((EGLNativeDisplayType)-1)
#endif /* EGL_ANGLE_software_display */

#ifndef EGL_ANGLE_direct3d_display
#define EGL_ANGLE_direct3d_display 1
#define EGL_D3D11_ELSE_D3D9_DISPLAY_ANGLE ((EGLNativeDisplayType)-2)
#define EGL_D3D11_ONLY_DISPLAY_ANGLE ((EGLNativeDisplayType)-3)
#endif /* EGL_ANGLE_direct3d_display */

#ifndef EGL_ANGLE_direct_composition
#define EGL_ANGLE_direct_composition 1
#define EGL_DIRECT_COMPOSITION_ANGLE 0x33A5
#endif /* EGL_ANGLE_direct_composition */

#ifndef EGL_ANGLE_platform_angle
#define EGL_ANGLE_platform_angle 1
#define EGL_PLATFORM_ANGLE_ANGLE          0x3202
#define EGL_PLATFORM_ANGLE_TYPE_ANGLE     0x3203
#define EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE 0x3204
#define EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE 0x3205
#define EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE 0x3206
#define EGL_PLATFORM_ANGLE_DEBUG_LAYERS_ENABLED_ANGLE 0x3451
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE 0x3209
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE 0x320A
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE 0x345E
#define EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE 0x348F
#endif /* EGL_ANGLE_platform_angle */

#ifndef EGL_ANGLE_platform_angle_d3d
#define EGL_ANGLE_platform_angle_d3d 1
#define EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE 0x3207
#define EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE 0x3208
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_WARP_ANGLE 0x320B
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_REFERENCE_ANGLE 0x320C
#define EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE 0x320F
#endif /* EGL_ANGLE_platform_angle_d3d */

#ifndef EGL_ANGLE_platform_angle_d3d_luid
#define EGL_ANGLE_platform_angle_d3d_luid 1
#define EGL_PLATFORM_ANGLE_D3D_LUID_HIGH_ANGLE 0x34A0
#define EGL_PLATFORM_ANGLE_D3D_LUID_LOW_ANGLE 0x34A1
#endif /* EGL_ANGLE_platform_angle_d3d_luid */

#ifndef EGL_ANGLE_platform_angle_d3d11on12
#define EGL_ANGLE_platform_angle_d3d11on12 1
#define EGL_PLATFORM_ANGLE_D3D11ON12_ANGLE 0x3488
#endif /* EGL_ANGLE_platform_angle_d3d11on12 */

#ifndef EGL_ANGLE_platform_angle_opengl
#define EGL_ANGLE_platform_angle_opengl 1
#define EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE 0x320D
#define EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE 0x320E
#define EGL_PLATFORM_ANGLE_EGL_HANDLE_ANGLE 0x3480
#endif /* EGL_ANGLE_platform_angle_opengl */

#ifndef EGL_ANGLE_platform_angle_null
#define EGL_ANGLE_platform_angle_null 1
#define EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE 0x33AE
#endif /* EGL_ANGLE_platform_angle_null */

#ifndef EGL_ANGLE_platform_angle_webgpu
#define EGL_ANGLE_platform_angle_webgpu 1
#define EGL_PLATFORM_ANGLE_TYPE_WEBGPU_ANGLE 0x34DD
#endif /* EGL_ANGLE_platform_angle_webgpu */

#ifndef EGL_ANGLE_platform_angle_vulkan
#define EGL_ANGLE_platform_angle_vulkan 1
#define EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE 0x3450
#define EGL_PLATFORM_VULKAN_DISPLAY_MODE_SIMPLE_ANGLE 0x34A4
#define EGL_PLATFORM_VULKAN_DISPLAY_MODE_HEADLESS_ANGLE 0x34A5
#endif /* EGL_ANGLE_platform_angle_vulkan */

#ifndef EGL_ANGLE_platform_angle_metal
#define EGL_ANGLE_platform_angle_metal 1
#define EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE 0x3489
#endif /* EGL_ANGLE_platform_angle_metal  */

#ifndef EGL_ANGLE_platform_angle_device_type_swiftshader
#define EGL_ANGLE_platform_angle_device_type_swiftshader
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE 0x3487
#endif /* EGL_ANGLE_platform_angle_device_type_swiftshader */

#ifndef EGL_ANGLE_platform_angle_device_type_egl_angle
#define EGL_ANGLE_platform_angle_device_type_egl_angle
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE 0x348E
#endif /* EGL_ANGLE_platform_angle_device_type_egl_angle */

#ifndef EGL_ANGLE_context_virtualization
#define EGL_ANGLE_context_virtualization 1
#define EGL_CONTEXT_VIRTUALIZATION_GROUP_ANGLE 0x3481
#endif /* EGL_ANGLE_context_virtualization */

#ifndef EGL_ANGLE_platform_angle_device_context_volatile_eagl
#define EGL_ANGLE_platform_angle_device_context_volatile_eagl 1
#define EGL_PLATFORM_ANGLE_DEVICE_CONTEXT_VOLATILE_EAGL_ANGLE 0x34A2
#endif /* EGL_ANGLE_platform_angle_device_context_volatile_eagl */

#ifndef EGL_ANGLE_platform_angle_device_context_volatile_cgl
#define EGL_ANGLE_platform_angle_device_context_volatile_cgl 1
#define EGL_PLATFORM_ANGLE_DEVICE_CONTEXT_VOLATILE_CGL_ANGLE 0x34A3
#endif /* EGL_ANGLE_platform_angle_device_context_volatile_cgl */

#ifndef EGL_ANGLE_platform_angle_device_id
#define EGL_ANGLE_platform_angle_device_id
#define EGL_PLATFORM_ANGLE_DEVICE_ID_HIGH_ANGLE 0x34D6
#define EGL_PLATFORM_ANGLE_DEVICE_ID_LOW_ANGLE 0x34D7
#define EGL_PLATFORM_ANGLE_DISPLAY_KEY_ANGLE 0x34DC
#endif /* EGL_ANGLE_platform_angle_device_id */

#ifndef EGL_ANGLE_x11_visual
#define EGL_ANGLE_x11_visual
#define EGL_X11_VISUAL_ID_ANGLE 0x33A3
#endif /* EGL_ANGLE_x11_visual */

#ifndef EGL_ANGLE_surface_orientation
#define EGL_ANGLE_surface_orientation
#define EGL_OPTIMAL_SURFACE_ORIENTATION_ANGLE 0x33A7
#define EGL_SURFACE_ORIENTATION_ANGLE 0x33A8
#define EGL_SURFACE_ORIENTATION_INVERT_X_ANGLE 0x0001
#define EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE 0x0002
#endif /* EGL_ANGLE_surface_orientation */

#ifndef EGL_ANGLE_experimental_present_path
#define EGL_ANGLE_experimental_present_path
#define EGL_EXPERIMENTAL_PRESENT_PATH_ANGLE 0x33A4
#define EGL_EXPERIMENTAL_PRESENT_PATH_FAST_ANGLE 0x33A9
#define EGL_EXPERIMENTAL_PRESENT_PATH_COPY_ANGLE 0x33AA
#endif /* EGL_ANGLE_experimental_present_path */

#ifndef EGL_ANGLE_stream_producer_d3d_texture
#define EGL_ANGLE_stream_producer_d3d_texture
#define EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE 0x33AB
typedef EGLBoolean(EGLAPIENTRYP PFNEGLCREATESTREAMPRODUCERD3DTEXTUREANGLEPROC)(EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list);
typedef EGLBoolean(EGLAPIENTRYP PFNEGLSTREAMPOSTD3DTEXTUREANGLEPROC)(EGLDisplay dpy, EGLStreamKHR stream, void *texture, const EGLAttrib *attrib_list);
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglCreateStreamProducerD3DTextureANGLE(EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list);
EGLAPI EGLBoolean EGLAPIENTRY eglStreamPostD3DTextureANGLE(EGLDisplay dpy, EGLStreamKHR stream, void *texture, const EGLAttrib *attrib_list);
#endif
#endif /* EGL_ANGLE_stream_producer_d3d_texture */

#ifndef EGL_ANGLE_create_context_webgl_compatibility
#define EGL_ANGLE_create_context_webgl_compatibility 1
#define EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE 0x33AC
#endif /* EGL_ANGLE_create_context_webgl_compatibility */

#ifndef EGL_ANGLE_display_texture_share_group
#define EGL_ANGLE_display_texture_share_group 1
#define EGL_DISPLAY_TEXTURE_SHARE_GROUP_ANGLE 0x33AF
#endif /* EGL_ANGLE_display_texture_share_group */

#ifndef EGL_CHROMIUM_create_context_bind_generates_resource
#define EGL_CHROMIUM_create_context_bind_generates_resource 1
#define EGL_CONTEXT_BIND_GENERATES_RESOURCE_CHROMIUM 0x33AD
#endif /* EGL_CHROMIUM_create_context_bind_generates_resource */

#ifndef EGL_ANGLE_metal_create_context_ownership_identity
#define EGL_ANGLE_metal_create_context_ownership_identity 1
#define EGL_CONTEXT_METAL_OWNERSHIP_IDENTITY_ANGLE 0x34D2
#endif /* EGL_ANGLE_metal_create_context_ownership_identity */

#ifndef EGL_ANGLE_create_context_client_arrays
#define EGL_ANGLE_create_context_client_arrays 1
#define EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE 0x3452
#endif /* EGL_ANGLE_create_context_client_arrays */

#ifndef EGL_ANGLE_device_creation
#define EGL_ANGLE_device_creation 1
typedef EGLDeviceEXT(EGLAPIENTRYP PFNEGLCREATEDEVICEANGLEPROC) (EGLint device_type, void *native_device, const EGLAttrib *attrib_list);
typedef EGLBoolean(EGLAPIENTRYP PFNEGLRELEASEDEVICEANGLEPROC) (EGLDeviceEXT device);
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLDeviceEXT EGLAPIENTRY eglCreateDeviceANGLE(EGLint device_type, void *native_device, const EGLAttrib *attrib_list);
EGLAPI EGLBoolean EGLAPIENTRY eglReleaseDeviceANGLE(EGLDeviceEXT device);
#endif
#endif /* EGL_ANGLE_device_creation */

#ifndef EGL_ANGLE_program_cache_control
#define EGL_ANGLE_program_cache_control 1
#define EGL_PROGRAM_CACHE_SIZE_ANGLE 0x3455
#define EGL_PROGRAM_CACHE_KEY_LENGTH_ANGLE 0x3456
#define EGL_PROGRAM_CACHE_RESIZE_ANGLE 0x3457
#define EGL_PROGRAM_CACHE_TRIM_ANGLE 0x3458
#define EGL_CONTEXT_PROGRAM_BINARY_CACHE_ENABLED_ANGLE 0x3459
typedef EGLint (EGLAPIENTRYP PFNEGLPROGRAMCACHEGETATTRIBANGLEPROC) (EGLDisplay dpy, EGLenum attrib);
typedef void (EGLAPIENTRYP PFNEGLPROGRAMCACHEQUERYANGLEPROC) (EGLDisplay dpy, EGLint index, void *key, EGLint *keysize, void *binary, EGLint *binarysize);
typedef void (EGLAPIENTRYP PFNEGLPROGRAMCACHEPOPULATEANGLEPROC) (EGLDisplay dpy, const void *key, EGLint keysize, const void *binary, EGLint binarysize);
typedef EGLint (EGLAPIENTRYP PFNEGLPROGRAMCACHERESIZEANGLEPROC) (EGLDisplay dpy, EGLint limit, EGLint mode);
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLint EGLAPIENTRY eglProgramCacheGetAttribANGLE(EGLDisplay dpy, EGLenum attrib);
EGLAPI void EGLAPIENTRY eglProgramCacheQueryANGLE(EGLDisplay dpy, EGLint index, void *key, EGLint *keysize, void *binary, EGLint *binarysize);
EGLAPI void EGLAPIENTRY eglProgramCachePopulateANGLE(EGLDisplay dpy, const void *key, EGLint keysize, const void *binary, EGLint binarysize);
EGLAPI EGLint EGLAPIENTRY eglProgramCacheResizeANGLE(EGLDisplay dpy, EGLint limit, EGLint mode);
#endif
#endif /* EGL_ANGLE_program_cache_control */

#ifndef EGL_ANGLE_iosurface_client_buffer
#define EGL_ANGLE_iosurface_client_buffer 1
#define EGL_IOSURFACE_ANGLE 0x3454
#define EGL_IOSURFACE_PLANE_ANGLE 0x345A
#define EGL_TEXTURE_RECTANGLE_ANGLE 0x345B
#define EGL_TEXTURE_TYPE_ANGLE 0x345C
#define EGL_TEXTURE_INTERNAL_FORMAT_ANGLE 0x345D
#define EGL_IOSURFACE_USAGE_HINT_ANGLE 0x348A
#define EGL_IOSURFACE_READ_HINT_ANGLE 0x0001
#define EGL_IOSURFACE_WRITE_HINT_ANGLE 0x0002
#define EGL_BIND_TO_TEXTURE_TARGET_ANGLE 0x348D
#endif /* EGL_ANGLE_iosurface_client_buffer */

#ifndef ANGLE_metal_texture_client_buffer
#define ANGLE_metal_texture_client_buffer 1
#define EGL_METAL_TEXTURE_ANGLE 0x34A7
#define EGL_METAL_TEXTURE_ARRAY_SLICE_ANGLE 0x34DD
#endif /* ANGLE_metal_texture_client_buffer */

#ifndef EGL_ANGLE_create_context_extensions_enabled
#define EGL_ANGLE_create_context_extensions_enabled 1
#define EGL_EXTENSIONS_ENABLED_ANGLE 0x345F
#endif /* EGL_ANGLE_create_context_extensions_enabled */

#ifndef EGL_CHROMIUM_sync_control
#define EGL_CHROMIUM_sync_control 1
typedef EGLBoolean (EGLAPIENTRYP PFNEGLGETSYNCVALUESCHROMIUMPROC) (EGLDisplay dpy,
                                                             EGLSurface surface,
                                                             EGLuint64KHR *ust,
                                                             EGLuint64KHR *msc,
                                                             EGLuint64KHR *sbc);
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglGetSyncValuesCHROMIUM(EGLDisplay dpy,
                                                             EGLSurface surface,
                                                             EGLuint64KHR *ust,
                                                             EGLuint64KHR *msc,
                                                             EGLuint64KHR *sbc);
#endif
#endif /* EGL_CHROMIUM_sync_control */

#ifndef EGL_ANGLE_sync_control_rate
#define EGL_ANGLE_sync_control_rate 1
typedef EGLBoolean (EGLAPIENTRYP PFNEGLGETMSCRATEANGLEPROC) (EGLDisplay dpy,
                                                             EGLSurface surface,
                                                             EGLint *numerator,
                                                             EGLint *denominator);
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglGetMscRateANGLE(EGLDisplay dpy,
                                                             EGLSurface surface,
                                                             EGLint *numerator,
                                                             EGLint *denominator);
#endif
#endif /* EGL_ANGLE_sync_control_rate */

#ifndef EGL_ANGLE_power_preference
#define EGL_ANGLE_power_preference 1
#define EGL_POWER_PREFERENCE_ANGLE 0x3482
#define EGL_LOW_POWER_ANGLE 0x0001
#define EGL_HIGH_POWER_ANGLE 0x0002
typedef void(EGLAPIENTRYP PFNEGLRELEASEHIGHPOWERGPUANGLEPROC) (EGLDisplay dpy, EGLContext ctx);
typedef void(EGLAPIENTRYP PFNEGLREACQUIREHIGHPOWERGPUANGLEPROC) (EGLDisplay dpy, EGLContext ctx);
typedef void(EGLAPIENTRYP PFNEGLHANDLEGPUSWITCHANGLEPROC) (EGLDisplay dpy);
typedef void(EGLAPIENTRYP PFNEGLFORCEGPUSWITCHANGLEPROC) (EGLDisplay dpy, EGLint gpuIDHigh, EGLint gpuIDLow);
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI void EGLAPIENTRY eglReleaseHighPowerGPUANGLE(EGLDisplay dpy, EGLContext ctx);
EGLAPI void EGLAPIENTRY eglReacquireHighPowerGPUANGLE(EGLDisplay dpy, EGLContext ctx);
EGLAPI void EGLAPIENTRY eglHandleGPUSwitchANGLE(EGLDisplay dpy);
EGLAPI void EGLAPIENTRY eglForceGPUSwitchANGLE(EGLDisplay dpy, EGLint gpuIDHigh, EGLint gpuIDLow);
#endif
#endif /* EGL_ANGLE_power_preference */

#ifndef EGL_ANGLE_wait_until_work_scheduled
#define EGL_ANGLE_wait_until_work_scheduled 1
typedef void(EGLAPIENTRYP PFNEGLWAITUNTILWORKSCHEDULEDANGLEPROC) (EGLDisplay dpy);
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI void EGLAPIENTRY eglWaitUntilWorkScheduledANGLE(EGLDisplay dpy);
#endif
#endif /* EGL_ANGLE_wait_until_work_scheduled */

#ifndef EGL_ANGLE_feature_control
#define EGL_ANGLE_feature_control 1
#define EGL_FEATURE_NAME_ANGLE 0x3460
#define EGL_FEATURE_CATEGORY_ANGLE 0x3461
#define EGL_FEATURE_DESCRIPTION_ANGLE 0x3462
#define EGL_FEATURE_BUG_ANGLE 0x3463
#define EGL_FEATURE_STATUS_ANGLE 0x3464
#define EGL_FEATURE_COUNT_ANGLE 0x3465
#define EGL_FEATURE_OVERRIDES_ENABLED_ANGLE 0x3466
#define EGL_FEATURE_OVERRIDES_DISABLED_ANGLE 0x3467
#define EGL_FEATURE_CONDITION_ANGLE 0x3468
#define EGL_FEATURE_ALL_DISABLED_ANGLE 0x3469
typedef const char *(EGLAPIENTRYP PFNEGLQUERYSTRINGIANGLEPROC) (EGLDisplay dpy, EGLint name, EGLint index);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYDISPLAYATTRIBANGLEPROC) (EGLDisplay dpy, EGLint attribute, EGLAttrib *value);
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI const char *EGLAPIENTRY eglQueryStringiANGLE(EGLDisplay dpy, EGLint name, EGLint index);
EGLAPI EGLBoolean EGLAPIENTRY eglQueryDisplayAttribANGLE(EGLDisplay dpy, EGLint attribute, EGLAttrib *value);
#endif
#endif /* EGL_ANGLE_feature_control */

#ifndef EGL_ANGLE_image_d3d11_texture
#define EGL_D3D11_TEXTURE_ANGLE 0x3484
#define EGL_TEXTURE_INTERNAL_FORMAT_ANGLE 0x345D
#endif /* EGL_ANGLE_image_d3d11_texture */

#ifndef EGL_ANGLE_create_context_backwards_compatible
#define EGL_ANGLE_create_context_backwards_compatible 1
#define EGL_CONTEXT_OPENGL_BACKWARDS_COMPATIBLE_ANGLE 0x3483
#endif /* EGL_ANGLE_create_context_backwards_compatible */

#ifndef EGL_ANGLE_device_cgl
#define EGL_ANGLE_device_cgl 1
#define EGL_CGL_CONTEXT_ANGLE 0x3485
#define EGL_CGL_PIXEL_FORMAT_ANGLE 0x3486
#endif

#ifndef EGL_ANGLE_ggp_stream_descriptor
#define EGL_ANGLE_ggp_stream_descriptor 1
#define EGL_GGP_STREAM_DESCRIPTOR_ANGLE 0x348B
#endif /* EGL_ANGLE_ggp_stream_descriptor */

#ifndef EGL_ANGLE_swap_with_frame_token
#define EGL_ANGLE_swap_with_frame_token 1
typedef khronos_uint64_t EGLFrameTokenANGLE;
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSWAPBUFFERSWITHFRAMETOKENANGLEPROC)(EGLDisplay dpy, EGLSurface surface, EGLFrameTokenANGLE frametoken);
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglSwapBuffersWithFrameTokenANGLE(EGLDisplay dpy, EGLSurface surface, EGLFrameTokenANGLE frametoken);
#endif
#endif /* EGL_ANGLE_swap_with_frame_token */

#ifndef EGL_ANGLE_prepare_swap_buffers
#define EGL_ANGLE_prepare_swap_buffers 1
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPREPARESWAPBUFFERSANGLEPROC)(EGLDisplay dpy, EGLSurface surface);
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglPrepareSwapBuffersANGLE(EGLDisplay dpy, EGLSurface surface);
#endif
#endif /* EGL_ANGLE_prepare_swap_buffers */

#ifndef EGL_ANGLE_device_eagl
#define EGL_ANGLE_device_eagl 1
#define EGL_EAGL_CONTEXT_ANGLE 0x348C
#endif

#ifndef EGL_ANGLE_device_metal
#define EGL_ANGLE_device_metal 1
#define EGL_METAL_DEVICE_ANGLE 0x34A6
#endif /* EGL_ANGLE_device_metal */

#ifndef EGL_ANGLE_display_semaphore_share_group
#define EGL_ANGLE_display_semaphore_share_group 1
#define EGL_DISPLAY_SEMAPHORE_SHARE_GROUP_ANGLE 0x348D
#endif /* EGL_ANGLE_display_semaphore_share_group */

#ifndef EGL_ANGLE_external_context_and_surface
#define EGL_ANGLE_external_context_and_surface 1
#define EGL_EXTERNAL_CONTEXT_ANGLE 0x348E
#define EGL_EXTERNAL_SURFACE_ANGLE 0x348F
typedef void (EGLAPIENTRYP PFNEGLACQUIREEXTERNALCONTEXTANGLEPROC) (EGLDisplay dpy, EGLSurface readAndDraw);
typedef void (EGLAPIENTRYP PFNEGLRELEASEEXTERNALCONTEXTANGLEPROC) (EGLDisplay dpy);
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI void EGLAPIENTRY eglAcquireExternalContextANGLE(EGLDisplay dpy, EGLSurface drawAndRead);
EGLAPI void EGLAPIENTRY eglReleaseExternalContextANGLE(EGLDisplay dpy);
#endif
#endif /* EGL_ANGLE_external_context_and_surface */

#ifndef EGL_ANGLE_create_surface_swap_interval
#define EGL_ANGLE_create_surface_swap_interval 1
#define EGL_SWAP_INTERVAL_ANGLE 0x322F
#endif /* EGL_ANGLE_create_surface_swap_interval */

#ifndef EGL_ANGLE_device_vulkan
#define EGL_ANGLE_device_vulkan 1
#define EGL_VULKAN_VERSION_ANGLE 0x34A8
#define EGL_VULKAN_INSTANCE_ANGLE 0x34A9
#define EGL_VULKAN_INSTANCE_EXTENSIONS_ANGLE 0x34AA
#define EGL_VULKAN_PHYSICAL_DEVICE_ANGLE 0x34AB
#define EGL_VULKAN_DEVICE_ANGLE 0x34AC
#define EGL_VULKAN_DEVICE_EXTENSIONS_ANGLE 0x34AD
#define EGL_VULKAN_FEATURES_ANGLE 0x34AE
#define EGL_VULKAN_QUEUE_ANGLE 0x34AF
#define EGL_VULKAN_QUEUE_FAMILIY_INDEX_ANGLE 0x34D0
#define EGL_VULKAN_GET_INSTANCE_PROC_ADDR 0x34D1
#endif /* EGL_ANGLE_device_vulkan */

#ifndef EGL_ANGLE_vulkan_image
#define EGL_ANGLE_vulkan_image
#define EGL_VULKAN_IMAGE_ANGLE 0x34D3
#define EGL_VULKAN_IMAGE_CREATE_INFO_HI_ANGLE 0x34D4
#define EGL_VULKAN_IMAGE_CREATE_INFO_LO_ANGLE 0x34D5
typedef EGLBoolean (EGLAPIENTRYP PFNEGLEXPORTVKIMAGEANGLEPROC)(EGLDisplay dpy, EGLImage image, void* vk_image, void* vk_image_create_info);
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglExportVkImageANGLE(EGLDisplay dpy, EGLImage image, void* vk_image, void* vk_image_create_info);
#endif
#endif /* EGL_ANGLE_vulkan_image */

#ifndef EGL_ANGLE_metal_shared_event_sync
#define EGL_ANGLE_metal_hared_event_sync 1
#define EGL_SYNC_METAL_SHARED_EVENT_ANGLE 0x34D8
#define EGL_SYNC_METAL_SHARED_EVENT_OBJECT_ANGLE 0x34D9
#define EGL_SYNC_METAL_SHARED_EVENT_SIGNAL_VALUE_LO_ANGLE 0x34DA
#define EGL_SYNC_METAL_SHARED_EVENT_SIGNAL_VALUE_HI_ANGLE 0x34DB
#define EGL_SYNC_METAL_SHARED_EVENT_SIGNALED_ANGLE 0x34DC
typedef void* (EGLAPIENTRYP PFNEGLCOPYMETALSHAREDEVENTANGLEPROC)(EGLDisplay dpy, EGLSync sync);
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI void *EGLAPIENTRY eglCopyMetalSharedEventANGLE(EGLDisplay dpy, EGLSync sync);
#endif
#endif /* EGL_ANGLE_metal_shared_event_sync */

#ifndef EGL_ANGLE_global_fence_sync
#define EGL_ANGLE_global_fence_sync 1
#define EGL_SYNC_GLOBAL_FENCE_ANGLE 0x34DE
#endif /* EGL_ANGLE_global_fence_sync */

// clang-format on

#endif  // INCLUDE_EGL_EGLEXT_ANGLE_
