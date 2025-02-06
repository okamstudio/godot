using System;
using System.Runtime.InteropServices;
using Godot.NativeInterop;

namespace Godot.Bridge
{
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct ManagedCallbacks
    {
        // @formatter:off
        public delegate* unmanaged<IntPtr, godot_variant**, int, godot_bool*, void> SignalAwaiter_SignalCallback;
        public delegate* unmanaged<IntPtr, void*, godot_variant**, int, godot_variant*, void> DelegateUtils_InvokeWithVariantArgs;
        public delegate* unmanaged<IntPtr, IntPtr, godot_bool> DelegateUtils_DelegateEquals;
        public delegate* unmanaged<IntPtr, int> DelegateUtils_DelegateHash;
        public delegate* unmanaged<IntPtr, godot_bool*, int> DelegateUtils_GetArgumentCount;
        public delegate* unmanaged<IntPtr, godot_array*, godot_bool> DelegateUtils_TrySerializeDelegateWithGCHandle;
        public delegate* unmanaged<godot_array*, IntPtr*, godot_bool> DelegateUtils_TryDeserializeDelegateWithGCHandle;
        public delegate* unmanaged<void> ScriptManagerBridge_FrameCallback;
        public delegate* unmanaged<godot_string_name*, IntPtr, IntPtr> ScriptManagerBridge_CreateManagedForGodotObjectBinding;
        public delegate* unmanaged<IntPtr, IntPtr, godot_variant**, int, godot_bool> ScriptManagerBridge_CreateManagedForGodotObjectScriptInstance;
        public delegate* unmanaged<IntPtr, godot_string_name*, void> ScriptManagerBridge_GetScriptNativeName;
        public delegate* unmanaged<godot_string*, godot_string*, godot_string*, godot_bool*, godot_bool*, godot_string*, void> ScriptManagerBridge_GetGlobalClassName;
        public delegate* unmanaged<IntPtr, IntPtr, void> ScriptManagerBridge_SetGodotObjectPtr;
        public delegate* unmanaged<IntPtr, godot_string_name*, godot_variant**, int, godot_bool*, void> ScriptManagerBridge_RaiseEventSignal;
        public delegate* unmanaged<IntPtr, IntPtr, godot_bool> ScriptManagerBridge_ScriptIsOrInherits;
        public delegate* unmanaged<IntPtr, godot_string*, godot_bool> ScriptManagerBridge_AddScriptBridge;
        public delegate* unmanaged<godot_string*, godot_ref*, void> ScriptManagerBridge_GetOrCreateScriptBridgeForPath;
        public delegate* unmanaged<IntPtr, void> ScriptManagerBridge_RemoveScriptBridge;
        public delegate* unmanaged<IntPtr, godot_bool> ScriptManagerBridge_TryReloadRegisteredScriptWithClass;
        public delegate* unmanaged<IntPtr, godot_csharp_type_info*, godot_array*, godot_dictionary*, godot_dictionary*, godot_ref*, void> ScriptManagerBridge_UpdateScriptClassInfo;
        public delegate* unmanaged<IntPtr, godot_dictionary*, void> ScriptManagerBridge_GetDocs;
        public delegate* unmanaged<IntPtr, IntPtr*, godot_bool, godot_bool> ScriptManagerBridge_SwapGCHandleForType;
        public delegate* unmanaged<IntPtr, delegate* unmanaged<IntPtr, godot_string*, void*, int, void>, void> ScriptManagerBridge_GetPropertyInfoList;
        public delegate* unmanaged<IntPtr, delegate* unmanaged<IntPtr, void*, int, void>, void> ScriptManagerBridge_GetPropertyDefaultValues;
        public delegate* unmanaged<IntPtr, godot_string_name*, godot_variant**, int, godot_variant_call_error*, godot_variant*, godot_bool> ScriptManagerBridge_CallStatic;
        public delegate* unmanaged<IntPtr, godot_string_name*, godot_variant**, int, godot_variant_call_error*, godot_variant*, godot_bool> CSharpInstanceBridge_Call;
        public delegate* unmanaged<IntPtr, godot_string_name*, godot_variant*, godot_bool> CSharpInstanceBridge_Set;
        public delegate* unmanaged<IntPtr, godot_string_name*, godot_variant*, godot_bool> CSharpInstanceBridge_Get;
        public delegate* unmanaged<IntPtr, godot_bool, void> CSharpInstanceBridge_CallDispose;
        public delegate* unmanaged<IntPtr, godot_string*, godot_bool*, void> CSharpInstanceBridge_CallToString;
        public delegate* unmanaged<IntPtr, godot_string_name*, godot_bool> CSharpInstanceBridge_HasMethodUnknownParams;
        public delegate* unmanaged<IntPtr, godot_dictionary*, godot_dictionary*, void> CSharpInstanceBridge_SerializeState;
        public delegate* unmanaged<IntPtr, godot_dictionary*, godot_dictionary*, void> CSharpInstanceBridge_DeserializeState;
        public delegate* unmanaged<IntPtr, void> GCHandleBridge_FreeGCHandle;
        public delegate* unmanaged<IntPtr, godot_bool> GCHandleBridge_GCHandleIsTargetCollectible;
        public delegate* unmanaged<void*, void> DebuggingUtils_GetCurrentStackInfo;
        public delegate* unmanaged<void> DisposablesTracker_OnGodotShuttingDown;
        public delegate* unmanaged<godot_bool, void> GD_OnCoreApiAssemblyLoaded;
        // @formatter:on

        public static ManagedCallbacks Create()
        {
            return new()
            {
                // @formatter:off
                SignalAwaiter_SignalCallback = &SignalAwaiter.SignalCallback,
                DelegateUtils_InvokeWithVariantArgs = &DelegateUtils.InvokeWithVariantArgs,
                DelegateUtils_DelegateEquals = &DelegateUtils.DelegateEquals,
                DelegateUtils_DelegateHash = &DelegateUtils.DelegateHash,
                DelegateUtils_GetArgumentCount = &DelegateUtils.GetArgumentCount,
                DelegateUtils_TrySerializeDelegateWithGCHandle = &DelegateUtils.TrySerializeDelegateWithGCHandle,
                DelegateUtils_TryDeserializeDelegateWithGCHandle = &DelegateUtils.TryDeserializeDelegateWithGCHandle,
                ScriptManagerBridge_FrameCallback = &ScriptManagerBridge.FrameCallback,
                ScriptManagerBridge_CreateManagedForGodotObjectBinding = &ScriptManagerBridge.CreateManagedForGodotObjectBinding,
                ScriptManagerBridge_CreateManagedForGodotObjectScriptInstance = &ScriptManagerBridge.CreateManagedForGodotObjectScriptInstance,
                ScriptManagerBridge_GetScriptNativeName = &ScriptManagerBridge.GetScriptNativeName,
                ScriptManagerBridge_GetGlobalClassName = &ScriptManagerBridge.GetGlobalClassName,
                ScriptManagerBridge_SetGodotObjectPtr = &ScriptManagerBridge.SetGodotObjectPtr,
                ScriptManagerBridge_RaiseEventSignal = &ScriptManagerBridge.RaiseEventSignal,
                ScriptManagerBridge_ScriptIsOrInherits = &ScriptManagerBridge.ScriptIsOrInherits,
                ScriptManagerBridge_AddScriptBridge = &ScriptManagerBridge.AddScriptBridge,
                ScriptManagerBridge_GetOrCreateScriptBridgeForPath = &ScriptManagerBridge.GetOrCreateScriptBridgeForPath,
                ScriptManagerBridge_RemoveScriptBridge = &ScriptManagerBridge.RemoveScriptBridge,
                ScriptManagerBridge_TryReloadRegisteredScriptWithClass = &ScriptManagerBridge.TryReloadRegisteredScriptWithClass,
                ScriptManagerBridge_UpdateScriptClassInfo = &ScriptManagerBridge.UpdateScriptClassInfo,
                ScriptManagerBridge_GetDocs = &ScriptManagerBridge.GetDocs,
                ScriptManagerBridge_SwapGCHandleForType = &ScriptManagerBridge.SwapGCHandleForType,
                ScriptManagerBridge_GetPropertyInfoList = &ScriptManagerBridge.GetPropertyInfoList,
                ScriptManagerBridge_GetPropertyDefaultValues = &ScriptManagerBridge.GetPropertyDefaultValues,
                ScriptManagerBridge_CallStatic = &ScriptManagerBridge.CallStatic,
                CSharpInstanceBridge_Call = &CSharpInstanceBridge.Call,
                CSharpInstanceBridge_Set = &CSharpInstanceBridge.Set,
                CSharpInstanceBridge_Get = &CSharpInstanceBridge.Get,
                CSharpInstanceBridge_CallDispose = &CSharpInstanceBridge.CallDispose,
                CSharpInstanceBridge_CallToString = &CSharpInstanceBridge.CallToString,
                CSharpInstanceBridge_HasMethodUnknownParams = &CSharpInstanceBridge.HasMethodUnknownParams,
                CSharpInstanceBridge_SerializeState = &CSharpInstanceBridge.SerializeState,
                CSharpInstanceBridge_DeserializeState = &CSharpInstanceBridge.DeserializeState,
                GCHandleBridge_FreeGCHandle = &GCHandleBridge.FreeGCHandle,
                GCHandleBridge_GCHandleIsTargetCollectible = &GCHandleBridge.GCHandleIsTargetCollectible,
                DebuggingUtils_GetCurrentStackInfo = &DebuggingUtils.GetCurrentStackInfo,
                DisposablesTracker_OnGodotShuttingDown = &DisposablesTracker.OnGodotShuttingDown,
                GD_OnCoreApiAssemblyLoaded = &GD.OnCoreApiAssemblyLoaded,
                // @formatter:on
            };
        }

        public static void Create(IntPtr outManagedCallbacks)
            => *(ManagedCallbacks*)outManagedCallbacks = Create();
    }
}
