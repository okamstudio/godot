/**************************************************************************/
/*  gd_mono_cache.cpp                                                     */
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

#include "gd_mono_cache.h"

#include "core/error/error_macros.h"

namespace GDMonoCache {

ManagedCallbacks managed_callbacks;
bool godot_api_cache_updated = false;

void update_godot_api_cache(const ManagedCallbacks &p_managed_callbacks) {
	int checked_count = 0;

#define CHECK_CALLBACK_NOT_NULL_IMPL(m_var, m_class, m_method)                             \
	{                                                                                      \
		ERR_FAIL_NULL_MSG(m_var,                                                           \
				"Mono Cache: Managed callback for '" #m_class "_" #m_method "' is null."); \
		checked_count += 1;                                                                \
	}

#define CHECK_CALLBACK_NOT_NULL(m_class, m_method) CHECK_CALLBACK_NOT_NULL_IMPL(p_managed_callbacks.m_class##_##m_method, m_class, m_method)

	CHECK_CALLBACK_NOT_NULL(SignalAwaiter, SignalCallback);
	CHECK_CALLBACK_NOT_NULL(DelegateUtils, InvokeWithVariantArgs);
	CHECK_CALLBACK_NOT_NULL(DelegateUtils, DelegateEquals);
	CHECK_CALLBACK_NOT_NULL(DelegateUtils, DelegateHash);
	CHECK_CALLBACK_NOT_NULL(DelegateUtils, GetArgumentCount);
	CHECK_CALLBACK_NOT_NULL(DelegateUtils, TrySerializeDelegateWithGCHandle);
	CHECK_CALLBACK_NOT_NULL(DelegateUtils, TryDeserializeDelegateWithGCHandle);
	CHECK_CALLBACK_NOT_NULL(ScriptManagerBridge, FrameCallback);
	CHECK_CALLBACK_NOT_NULL(ScriptManagerBridge, CreateManagedForGodotObjectBinding);
	CHECK_CALLBACK_NOT_NULL(ScriptManagerBridge, CreateManagedForGodotObjectScriptInstance);
	CHECK_CALLBACK_NOT_NULL(ScriptManagerBridge, GetScriptNativeName);
	CHECK_CALLBACK_NOT_NULL(ScriptManagerBridge, GetGlobalClassName);
	CHECK_CALLBACK_NOT_NULL(ScriptManagerBridge, SetGodotObjectPtr);
	CHECK_CALLBACK_NOT_NULL(ScriptManagerBridge, RaiseEventSignal);
	CHECK_CALLBACK_NOT_NULL(ScriptManagerBridge, ScriptIsOrInherits);
	CHECK_CALLBACK_NOT_NULL(ScriptManagerBridge, AddScriptBridge);
	CHECK_CALLBACK_NOT_NULL(ScriptManagerBridge, GetOrCreateScriptBridgeForPath);
	CHECK_CALLBACK_NOT_NULL(ScriptManagerBridge, RemoveScriptBridge);
	CHECK_CALLBACK_NOT_NULL(ScriptManagerBridge, TryReloadRegisteredScriptWithClass);
	CHECK_CALLBACK_NOT_NULL(ScriptManagerBridge, UpdateScriptClassInfo);
	CHECK_CALLBACK_NOT_NULL(ScriptManagerBridge, GetDocs);
	CHECK_CALLBACK_NOT_NULL(ScriptManagerBridge, SwapGCHandleForType);
	CHECK_CALLBACK_NOT_NULL(ScriptManagerBridge, GetPropertyInfoList);
	CHECK_CALLBACK_NOT_NULL(ScriptManagerBridge, GetPropertyDefaultValues);
	CHECK_CALLBACK_NOT_NULL(ScriptManagerBridge, CallStatic);
	CHECK_CALLBACK_NOT_NULL(CSharpInstanceBridge, Call);
	CHECK_CALLBACK_NOT_NULL(CSharpInstanceBridge, Set);
	CHECK_CALLBACK_NOT_NULL(CSharpInstanceBridge, Get);
	CHECK_CALLBACK_NOT_NULL(CSharpInstanceBridge, CallDispose);
	CHECK_CALLBACK_NOT_NULL(CSharpInstanceBridge, CallToString);
	CHECK_CALLBACK_NOT_NULL(CSharpInstanceBridge, HasMethodUnknownParams);
	CHECK_CALLBACK_NOT_NULL(CSharpInstanceBridge, SerializeState);
	CHECK_CALLBACK_NOT_NULL(CSharpInstanceBridge, DeserializeState);
	CHECK_CALLBACK_NOT_NULL(GCHandleBridge, FreeGCHandle);
	CHECK_CALLBACK_NOT_NULL(GCHandleBridge, GCHandleIsTargetCollectible);
	CHECK_CALLBACK_NOT_NULL(DebuggingUtils, GetCurrentStackInfo);
	CHECK_CALLBACK_NOT_NULL(DisposablesTracker, OnGodotShuttingDown);
	CHECK_CALLBACK_NOT_NULL(GD, OnCoreApiAssemblyLoaded);

	managed_callbacks = p_managed_callbacks;

	// It's easy to forget to add new callbacks here, so this should help
	if (checked_count * sizeof(void *) != sizeof(ManagedCallbacks)) {
		int missing_count = (sizeof(ManagedCallbacks) / sizeof(void *)) - checked_count;
		WARN_PRINT("The presence of " + itos(missing_count) + " callback(s) was not validated");
	}

	godot_api_cache_updated = true;
}
} // namespace GDMonoCache
