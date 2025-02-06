#pragma warning disable IDE1006 // Naming rule violation
// ReSharper disable InconsistentNaming

using System;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;
using Godot;
using Godot.NativeInterop;
using Godot.SourceGenerators.Internal;
using GodotTools.IdeMessaging.Requests;

namespace GodotTools.Internals
{
    [GenerateUnmanagedCallbacks(typeof(InternalUnmanagedCallbacks))]
    internal static partial class Internal
    {
        public const string CSharpLanguageType = "CSharpScript";
        public const string CSharpLanguageExtension = ".cs";

        public static string FullExportTemplatesDir
        {
            get
            {
                godot_icall_Internal_FullExportTemplatesDir(out godot_string dest);
                using (dest)
                    return Marshaling.ConvertStringToManaged(dest);
            }
        }

        public static string SimplifyGodotPath(this string path) => Godot.StringExtensions.SimplifyPath(path);

        public static bool IsMacOSAppBundleInstalled(string bundleId)
        {
            using godot_string bundleIdIn = Marshaling.ConvertStringToNative(bundleId);
            return godot_icall_Internal_IsMacOSAppBundleInstalled(bundleIdIn);
        }

        public static bool LipOCreateFile(string outputPath, string[] files)
        {
            using godot_string outputPathIn = Marshaling.ConvertStringToNative(outputPath);
            using godot_packed_string_array filesIn = Marshaling.ConvertSystemArrayToNativePackedStringArray(files);
            return godot_icall_Internal_LipOCreateFile(outputPathIn, filesIn);
        }

        public static bool GodotIs32Bits() => godot_icall_Internal_GodotIs32Bits();

        public static bool GodotIsRealTDouble() => godot_icall_Internal_GodotIsRealTDouble();

        public static void GodotMainIteration() => godot_icall_Internal_GodotMainIteration();

        public static bool IsAssembliesReloadingNeeded() => godot_icall_Internal_IsAssembliesReloadingNeeded();

        public static void ReloadAssemblies(bool softReload) => godot_icall_Internal_ReloadAssemblies(softReload);

        public static void EditorDebuggerNodeReloadScripts() => godot_icall_Internal_EditorDebuggerNodeReloadScripts();

        public static bool ScriptEditorEdit(Resource resource, int line, int col, bool grabFocus = true) =>
            godot_icall_Internal_ScriptEditorEdit(resource.NativeInstance, line, col, grabFocus);

        public static void EditorNodeConnectAddMethod() => godot_icall_Internal_EditorNodeConnectAddMethod();

        public static void EditorNodeShowScriptScreen() => godot_icall_Internal_EditorNodeShowScriptScreen();

        public static void EditorRunPlay() => godot_icall_Internal_EditorRunPlay();

        public static void EditorRunStop() => godot_icall_Internal_EditorRunStop();

        public static void EditorPlugin_AddControlToEditorRunBar(Control control) =>
            godot_icall_Internal_EditorPlugin_AddControlToEditorRunBar(control.NativeInstance);

        public static void ScriptEditorDebugger_ReloadScripts() =>
            godot_icall_Internal_ScriptEditorDebugger_ReloadScripts();

        public static string[] CodeCompletionRequest(CodeCompletionRequest.CompletionKind kind,
            string scriptFile)
        {
            using godot_string scriptFileIn = Marshaling.ConvertStringToNative(scriptFile);
            godot_icall_Internal_CodeCompletionRequest((int)kind, scriptFileIn, out godot_packed_string_array res);
            using (res)
                return Marshaling.ConvertNativePackedStringArrayToSystemArray(res);
        }

        #region Internal

        private static bool initialized = false;

        // ReSharper disable once ParameterOnlyUsedForPreconditionCheck.Global
        internal static unsafe void Initialize(IntPtr unmanagedCallbacks, int unmanagedCallbacksSize)
        {
            if (initialized)
                throw new InvalidOperationException("Already initialized.");
            initialized = true;

            if (unmanagedCallbacksSize != sizeof(InternalUnmanagedCallbacks))
                throw new ArgumentException("Unmanaged callbacks size mismatch.", nameof(unmanagedCallbacksSize));

            _unmanagedCallbacks = Unsafe.AsRef<InternalUnmanagedCallbacks>((void*)unmanagedCallbacks);
        }

        private partial struct InternalUnmanagedCallbacks
        {
        }

        /*
         * IMPORTANT:
         * The order of the methods defined in NativeFuncs must match the order
         * in the array defined at the bottom of 'editor/editor_internal_calls.cpp'.
         */

        public static partial void godot_icall_GodotSharpDirs_ResMetadataDir(out godot_string r_dest);

        public static partial void godot_icall_GodotSharpDirs_MonoUserDir(out godot_string r_dest);

        public static partial void godot_icall_GodotSharpDirs_BuildLogsDirs(out godot_string r_dest);

        public static partial void godot_icall_GodotSharpDirs_DataEditorToolsDir(out godot_string r_dest);

        public static partial void godot_icall_GodotSharpDirs_CSharpProjectName(out godot_string r_dest);

        public static partial void godot_icall_EditorProgress_Create(in godot_string task, in godot_string label,
            int amount, bool canCancel);

        public static partial void godot_icall_EditorProgress_Dispose(in godot_string task);

        public static partial bool godot_icall_EditorProgress_Step(in godot_string task, in godot_string state,
            int step,
            bool forceRefresh);

        private static partial void godot_icall_Internal_FullExportTemplatesDir(out godot_string dest);

        private static partial bool godot_icall_Internal_IsMacOSAppBundleInstalled(in godot_string bundleId);

        private static partial bool godot_icall_Internal_LipOCreateFile(in godot_string outputPath, in godot_packed_string_array files);

        private static partial bool godot_icall_Internal_GodotIs32Bits();

        private static partial bool godot_icall_Internal_GodotIsRealTDouble();

        private static partial void godot_icall_Internal_GodotMainIteration();

        private static partial bool godot_icall_Internal_IsAssembliesReloadingNeeded();

        private static partial void godot_icall_Internal_ReloadAssemblies(bool softReload);

        private static partial void godot_icall_Internal_EditorDebuggerNodeReloadScripts();

        private static partial bool godot_icall_Internal_ScriptEditorEdit(IntPtr resource, int line, int col,
            bool grabFocus);

        private static partial void godot_icall_Internal_EditorNodeConnectAddMethod();

        private static partial void godot_icall_Internal_EditorNodeShowScriptScreen();

        private static partial void godot_icall_Internal_EditorRunPlay();

        private static partial void godot_icall_Internal_EditorRunStop();

        private static partial void godot_icall_Internal_EditorPlugin_AddControlToEditorRunBar(IntPtr p_control);

        private static partial void godot_icall_Internal_ScriptEditorDebugger_ReloadScripts();

        private static partial void godot_icall_Internal_CodeCompletionRequest(int kind, in godot_string scriptFile,
            out godot_packed_string_array res);

        public static partial float godot_icall_Globals_EditorScale();

        public static partial void godot_icall_Globals_GlobalDef(in godot_string setting, in godot_variant defaultValue,
            bool restartIfChanged, out godot_variant result);

        public static partial void godot_icall_Globals_EditorDef(in godot_string setting, in godot_variant defaultValue,
            bool restartIfChanged, out godot_variant result);

        public static partial void
            godot_icall_Globals_EditorDefShortcut(in godot_string setting, in godot_string name, Key keycode, godot_bool physical, out godot_variant result);

        public static partial void
            godot_icall_Globals_EditorGetShortcut(in godot_string setting, out godot_variant result);

        public static partial void
            godot_icall_Globals_EditorShortcutOverride(in godot_string setting, in godot_string feature, Key keycode, godot_bool physical);

        public static partial void godot_icall_Globals_TTR(in godot_string text, out godot_string dest);

        public static partial void godot_icall_Utils_OS_GetPlatformName(out godot_string dest);

        public static partial bool godot_icall_Utils_OS_UnixFileHasExecutableAccess(in godot_string filePath);

        #endregion
    }
}
