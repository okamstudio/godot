/**************************************************************************/
/*  util.h                                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#define UNPACK(...) __VA_ARGS__

#define INIT_XR_FUNC_V(openxr_api, name)                                                                              \
	if constexpr (true) {                                                                                             \
		XrResult get_instance_proc_addr_result;                                                                       \
		get_instance_proc_addr_result = openxr_api->get_instance_proc_addr(#name, (PFN_xrVoidFunction *)&name##_ptr); \
		ERR_FAIL_COND_V(XR_FAILED(get_instance_proc_addr_result), false);                                             \
	} else                                                                                                            \
		((void)0)

#define EXT_INIT_XR_FUNC_V(name) INIT_XR_FUNC_V(OpenXRAPI::get_singleton(), name)
#define OPENXR_API_INIT_XR_FUNC_V(name) INIT_XR_FUNC_V(this, name)

#define INIT_XR_FUNC(openxr_api, name)                                                                                \
	if constexpr (true) {                                                                                             \
		XrResult get_instance_proc_addr_result;                                                                       \
		get_instance_proc_addr_result = openxr_api->get_instance_proc_addr(#name, (PFN_xrVoidFunction *)&name##_ptr); \
		ERR_FAIL_COND(XR_FAILED(get_instance_proc_addr_result));                                                      \
	} else                                                                                                            \
		((void)0)

#define EXT_INIT_XR_FUNC(name) INIT_XR_FUNC(OpenXRAPI::get_singleton(), name)
#define OPENXR_API_INIT_XR_FUNC(name) INIT_XR_FUNC(this, name)

#define TRY_INIT_XR_FUNC(openxr_api, name) \
	openxr_api->try_get_instance_proc_addr(#name, (PFN_xrVoidFunction *)&name##_ptr)

#define EXT_TRY_INIT_XR_FUNC(name) TRY_INIT_XR_FUNC(OpenXRAPI::get_singleton(), name)
#define OPENXR_TRY_API_INIT_XR_FUNC(name) TRY_INIT_XR_FUNC(this, name)
#define GDEXTENSION_INIT_XR_FUNC(name)                                                              \
	if constexpr (true) {                                                                           \
		name##_ptr = reinterpret_cast<PFN_##name>(get_openxr_api()->get_instance_proc_addr(#name)); \
		ERR_FAIL_NULL(name##_ptr);                                                                  \
	} else                                                                                          \
		((void)0)

#define GDEXTENSION_INIT_XR_FUNC_V(name)                                                            \
	if constexpr (true) {                                                                           \
		name##_ptr = reinterpret_cast<PFN_##name>(get_openxr_api()->get_instance_proc_addr(#name)); \
		ERR_FAIL_NULL_V(name##_ptr, false);                                                         \
	} else                                                                                          \
		((void)0)

#define EXT_PROTO_XRRESULT_FUNC1(func_name, arg1_type, arg1)                    \
	PFN_##func_name func_name##_ptr = nullptr;                                  \
	XRAPI_ATTR XrResult XRAPI_CALL func_name(UNPACK arg1_type p_##arg1) const { \
		if (!func_name##_ptr) {                                                 \
			return XR_ERROR_HANDLE_INVALID;                                     \
		}                                                                       \
		return (*func_name##_ptr)(p_##arg1);                                    \
	}

#define EXT_PROTO_XRRESULT_FUNC2(func_name, arg1_type, arg1, arg2_type, arg2)                              \
	PFN_##func_name func_name##_ptr = nullptr;                                                             \
	XRAPI_ATTR XrResult XRAPI_CALL func_name(UNPACK arg1_type p_##arg1, UNPACK arg2_type p_##arg2) const { \
		if (!func_name##_ptr) {                                                                            \
			return XR_ERROR_HANDLE_INVALID;                                                                \
		}                                                                                                  \
		return (*func_name##_ptr)(p_##arg1, p_##arg2);                                                     \
	}

#define EXT_PROTO_XRRESULT_FUNC3(func_name, arg1_type, arg1, arg2_type, arg2, arg3_type, arg3)                                        \
	PFN_##func_name func_name##_ptr = nullptr;                                                                                        \
	XRAPI_ATTR XrResult XRAPI_CALL func_name(UNPACK arg1_type p_##arg1, UNPACK arg2_type p_##arg2, UNPACK arg3_type p_##arg3) const { \
		if (!func_name##_ptr) {                                                                                                       \
			return XR_ERROR_HANDLE_INVALID;                                                                                           \
		}                                                                                                                             \
		return (*func_name##_ptr)(p_##arg1, p_##arg2, p_##arg3);                                                                      \
	}

#define EXT_PROTO_XRRESULT_FUNC4(func_name, arg1_type, arg1, arg2_type, arg2, arg3_type, arg3, arg4_type, arg4)                                                  \
	PFN_##func_name func_name##_ptr = nullptr;                                                                                                                   \
	XRAPI_ATTR XrResult XRAPI_CALL func_name(UNPACK arg1_type p_##arg1, UNPACK arg2_type p_##arg2, UNPACK arg3_type p_##arg3, UNPACK arg4_type p_##arg4) const { \
		if (!func_name##_ptr) {                                                                                                                                  \
			return XR_ERROR_HANDLE_INVALID;                                                                                                                      \
		}                                                                                                                                                        \
		return (*func_name##_ptr)(p_##arg1, p_##arg2, p_##arg3, p_##arg4);                                                                                       \
	}

#define EXT_PROTO_XRRESULT_FUNC5(func_name, arg1_type, arg1, arg2_type, arg2, arg3_type, arg3, arg4_type, arg4, arg5_type, arg5)                                                            \
	PFN_##func_name func_name##_ptr = nullptr;                                                                                                                                              \
	XRAPI_ATTR XrResult XRAPI_CALL func_name(UNPACK arg1_type p_##arg1, UNPACK arg2_type p_##arg2, UNPACK arg3_type p_##arg3, UNPACK arg4_type p_##arg4, UNPACK arg5_type p_##arg5) const { \
		if (!func_name##_ptr) {                                                                                                                                                             \
			return XR_ERROR_HANDLE_INVALID;                                                                                                                                                 \
		}                                                                                                                                                                                   \
		return (*func_name##_ptr)(p_##arg1, p_##arg2, p_##arg3, p_##arg4, p_##arg5);                                                                                                        \
	}

#define EXT_PROTO_XRRESULT_FUNC6(func_name, arg1_type, arg1, arg2_type, arg2, arg3_type, arg3, arg4_type, arg4, arg5_type, arg5, arg6_type, arg6)                                                                      \
	PFN_##func_name func_name##_ptr = nullptr;                                                                                                                                                                         \
	XRAPI_ATTR XrResult XRAPI_CALL func_name(UNPACK arg1_type p_##arg1, UNPACK arg2_type p_##arg2, UNPACK arg3_type p_##arg3, UNPACK arg4_type p_##arg4, UNPACK arg5_type p_##arg5, UNPACK arg6_type p_##arg6) const { \
		if (!func_name##_ptr) {                                                                                                                                                                                        \
			return XR_ERROR_HANDLE_INVALID;                                                                                                                                                                            \
		}                                                                                                                                                                                                              \
		return (*func_name##_ptr)(p_##arg1, p_##arg2, p_##arg3, p_##arg4, p_##arg5, p_##arg6);                                                                                                                         \
	}
