using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Loader;
using Godot.Bridge;
using Godot.NativeInterop;

namespace GodotPlugins
{
    public static class Main
    {
        // IMPORTANT:
        // Keeping strong references to the AssemblyLoadContext (our PluginLoadContext) prevents
        // it from being unloaded. To avoid issues, we wrap the reference in this class, and mark
        // all the methods that access it as non-inlineable. This way we prevent local references
        // (either real or introduced by the JIT) to escape the scope of these methods due to
        // inlining, which could keep the AssemblyLoadContext alive while trying to unload.
        private sealed class PluginLoadContextWrapper
        {
            private PluginLoadContext? _pluginLoadContext;
            private readonly WeakReference _weakReference;

            private PluginLoadContextWrapper(PluginLoadContext pluginLoadContext, WeakReference weakReference)
            {
                _pluginLoadContext = pluginLoadContext;
                _weakReference = weakReference;
            }

            public string? AssemblyLoadedPath
            {
                [MethodImpl(MethodImplOptions.NoInlining)]
                get => _pluginLoadContext?.AssemblyLoadedPath;
            }

            public bool IsCollectible
            {
                [MethodImpl(MethodImplOptions.NoInlining)]
                // if _pluginLoadContext is null we already started unloading, so it was collectible
                get => _pluginLoadContext?.IsCollectible ?? true;
            }

            public bool IsAlive
            {
                [MethodImpl(MethodImplOptions.NoInlining)]
                get => _weakReference.IsAlive;
            }

            [MethodImpl(MethodImplOptions.NoInlining)]
            public static (Assembly, PluginLoadContextWrapper) CreateAndLoadFromAssemblyName(
                AssemblyName assemblyName,
                string pluginPath,
                ICollection<string> sharedAssemblies,
                AssemblyLoadContext mainLoadContext,
                bool isCollectible
            )
            {
                var context = new PluginLoadContext(pluginPath, sharedAssemblies, mainLoadContext, isCollectible);
                var reference = new WeakReference(context, trackResurrection: true);
                var wrapper = new PluginLoadContextWrapper(context, reference);
                var assembly = context.LoadFromAssemblyName(assemblyName);
                return (assembly, wrapper);
            }

            [MethodImpl(MethodImplOptions.NoInlining)]
            internal void Unload()
            {
                _pluginLoadContext?.Unload();
                _pluginLoadContext = null;
            }
        }

        private static readonly List<AssemblyName> SharedAssemblies = new();
        private static readonly Assembly CoreApiAssembly = typeof(global::Godot.GodotObject).Assembly;
        private static Assembly? _editorApiAssembly;
        private static PluginLoadContextWrapper? _projectLoadContext;
        private static bool _editorHint = false;

        private static readonly AssemblyLoadContext MainLoadContext =
            AssemblyLoadContext.GetLoadContext(Assembly.GetExecutingAssembly()) ??
            AssemblyLoadContext.Default;

        private static DllImportResolver? _dllImportResolver;

        // Right now we do it this way for simplicity as hot-reload is disabled. It will need to be changed later.
        [UnmanagedCallersOnly]
        // ReSharper disable once UnusedMember.Local
        private static unsafe godot_bool InitializeFromEngine(IntPtr godotDllHandle, godot_bool editorHint,
            PluginsCallbacks* pluginsCallbacks, ManagedCallbacks* managedCallbacks,
            IntPtr unmanagedCallbacks, int unmanagedCallbacksSize)
        {
            try
            {
                _editorHint = editorHint.ToBool();

                _dllImportResolver = new GodotDllImportResolver(godotDllHandle).OnResolveDllImport;

                SharedAssemblies.Add(CoreApiAssembly.GetName());
                NativeLibrary.SetDllImportResolver(CoreApiAssembly, _dllImportResolver);

                AlcReloadCfg.Configure(alcReloadEnabled: _editorHint);
                NativeFuncs.Initialize(unmanagedCallbacks, unmanagedCallbacksSize);

                if (_editorHint)
                {
                    _editorApiAssembly = Assembly.Load("GodotSharpEditor");
                    SharedAssemblies.Add(_editorApiAssembly.GetName());
                    NativeLibrary.SetDllImportResolver(_editorApiAssembly, _dllImportResolver);
                }

                *pluginsCallbacks = new()
                {
                    LoadProjectAssemblyCallback = &LoadProjectAssembly,
                    LoadToolsAssemblyCallback = &LoadToolsAssembly,
                    UnloadProjectPluginCallback = &UnloadProjectPlugin,
                };

                *managedCallbacks = ManagedCallbacks.Create();

                return godot_bool.True;
            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e);
                return godot_bool.False;
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct PluginsCallbacks
        {
            public unsafe delegate* unmanaged<char*, godot_string*, godot_string*, godot_bool*, godot_bool> LoadProjectAssemblyCallback;
            public unsafe delegate* unmanaged<char*, IntPtr, int, IntPtr> LoadToolsAssemblyCallback;
            public unsafe delegate* unmanaged<godot_bool> UnloadProjectPluginCallback;
        }

        [UnmanagedCallersOnly]
        private static unsafe godot_bool LoadProjectAssembly(char* nAssemblyPath, godot_string* outLoadedAssemblyPath, godot_string* outFailedReason, godot_bool* shouldRetry)
        {
            try
            {
                if (_projectLoadContext != null)
                    return godot_bool.True; // Already loaded

                *shouldRetry = godot_bool.False;
                string assemblyPath = new(nAssemblyPath);

                (var projectAssembly, _projectLoadContext) = LoadPlugin(assemblyPath, isCollectible: _editorHint);

                string loadedAssemblyPath = _projectLoadContext.AssemblyLoadedPath ?? assemblyPath;
                *outLoadedAssemblyPath = Marshaling.ConvertStringToNative(loadedAssemblyPath);

                // This generated field is here to ensure that
                // the developer do not drop the TOOLS compiler preprocessor
                // from their csproj, which breaks the C# Editor Functionality.
                var isToolsDefinedField =
                    projectAssembly
                        .GetType("GodotPlugins.Game.Main")?
                        .GetField("ToolsDefined", BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic);

                if (isToolsDefinedField == null)
                {
                    *outFailedReason = Marshaling.ConvertStringToNative("The csproj was missing the `TOOLS` compiler symbol, which is required for C# editor functionality. Please modify the csproj file and rebuild the C# project, check https://github.com/godotengine/godot/issues/98124 for more information.");
                    return godot_bool.False;
                }

                ScriptManagerBridge.LookupScriptsInAssembly(projectAssembly);

                return godot_bool.True;
            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e);
                *outFailedReason = Marshaling.ConvertStringToNative(e.Message);
                return godot_bool.False;
            }
        }

        [UnmanagedCallersOnly]
        private static unsafe IntPtr LoadToolsAssembly(char* nAssemblyPath,
            IntPtr unmanagedCallbacks, int unmanagedCallbacksSize)
        {
            try
            {
                string assemblyPath = new(nAssemblyPath);

                if (_editorApiAssembly == null)
                    throw new InvalidOperationException("The Godot editor API assembly is not loaded.");

                var (assembly, _) = LoadPlugin(assemblyPath, isCollectible: false);

                NativeLibrary.SetDllImportResolver(assembly, _dllImportResolver!);

                var method = assembly.GetType("GodotTools.GodotSharpEditor")?
                    .GetMethod("InternalCreateInstance",
                        BindingFlags.Static | BindingFlags.NonPublic | BindingFlags.Public);

                if (method == null)
                {
                    throw new MissingMethodException("GodotTools.GodotSharpEditor",
                        "InternalCreateInstance");
                }

                return (IntPtr?)method
                           .Invoke(null, new object[] { unmanagedCallbacks, unmanagedCallbacksSize })
                       ?? IntPtr.Zero;
            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e);
                return IntPtr.Zero;
            }
        }

        private static (Assembly, PluginLoadContextWrapper) LoadPlugin(string assemblyPath, bool isCollectible)
        {
            string assemblyName = Path.GetFileNameWithoutExtension(assemblyPath);

            var sharedAssemblies = new List<string>();

            foreach (var sharedAssembly in SharedAssemblies)
            {
                string? sharedAssemblyName = sharedAssembly.Name;
                if (sharedAssemblyName != null)
                    sharedAssemblies.Add(sharedAssemblyName);
            }

            return PluginLoadContextWrapper.CreateAndLoadFromAssemblyName(
                new AssemblyName(assemblyName), assemblyPath, sharedAssemblies, MainLoadContext, isCollectible);
        }

        [UnmanagedCallersOnly]
        private static godot_bool UnloadProjectPlugin()
        {
            try
            {
                return UnloadPlugin(ref _projectLoadContext).ToGodotBool();
            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e);
                return godot_bool.False;
            }
        }

        private static bool UnloadPlugin(ref PluginLoadContextWrapper? pluginLoadContext)
        {
            try
            {
                if (pluginLoadContext == null)
                    return true;

                if (!pluginLoadContext.IsCollectible)
                {
                    Console.Error.WriteLine("Cannot unload a non-collectible assembly load context.");
                    return false;
                }

                Console.WriteLine("Unloading assembly load context...");

                pluginLoadContext.Unload();

                int startTimeMs = Environment.TickCount;
                bool takingTooLong = false;

                while (pluginLoadContext.IsAlive)
                {
                    GC.Collect(GC.MaxGeneration, GCCollectionMode.Forced);
                    GC.WaitForPendingFinalizers();

                    if (!pluginLoadContext.IsAlive)
                        break;

                    int elapsedTimeMs = Environment.TickCount - startTimeMs;

                    if (!takingTooLong && elapsedTimeMs >= 200)
                    {
                        takingTooLong = true;

                        // TODO: How to log from GodotPlugins? (delegate pointer?)
                        Console.Error.WriteLine("Assembly unloading is taking longer than expected...");
                    }
                    else if (elapsedTimeMs >= 1000)
                    {
                        // TODO: How to log from GodotPlugins? (delegate pointer?)
                        Console.Error.WriteLine(
                            "Failed to unload assemblies. Possible causes: Strong GC handles, running threads, etc.");

                        return false;
                    }
                }

                Console.WriteLine("Assembly load context unloaded successfully.");

                pluginLoadContext = null;
                return true;
            }
            catch (Exception e)
            {
                // TODO: How to log exceptions from GodotPlugins? (delegate pointer?)
                Console.Error.WriteLine(e);
                return false;
            }
        }
    }
}
