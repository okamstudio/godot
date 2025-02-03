using Godot;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using GodotTools.Build;
using GodotTools.Core;
using GodotTools.Internals;
using JetBrains.Annotations;
using static GodotTools.Internals.Globals;
using Directory = GodotTools.Utils.Directory;
using File = GodotTools.Utils.File;
using OS = GodotTools.Utils.OS;
using Path = System.IO.Path;

namespace GodotTools.Export
{
    public class ExportPlugin : EditorExportPlugin
    {
        [Flags]
        private enum I18NCodesets
        {
            None = 0,
            CJK = 1,
            MidEast = 2,
            Other = 4,
            Rare = 8,
            West = 16,
            All = CJK | MidEast | Other | Rare | West
        }

        private string _maybeLastExportError;

        private void AddI18NAssemblies(Godot.Collections.Dictionary<string, string> assemblies, string bclDir)
        {
            var codesets = (I18NCodesets)ProjectSettings.GetSetting("mono/export/i18n_codesets");

            if (codesets == I18NCodesets.None)
                return;

            void AddI18NAssembly(string name) => assemblies.Add(name, Path.Combine(bclDir, $"{name}.dll"));

            AddI18NAssembly("I18N");

            if ((codesets & I18NCodesets.CJK) != 0)
                AddI18NAssembly("I18N.CJK");
            if ((codesets & I18NCodesets.MidEast) != 0)
                AddI18NAssembly("I18N.MidEast");
            if ((codesets & I18NCodesets.Other) != 0)
                AddI18NAssembly("I18N.Other");
            if ((codesets & I18NCodesets.Rare) != 0)
                AddI18NAssembly("I18N.Rare");
            if ((codesets & I18NCodesets.West) != 0)
                AddI18NAssembly("I18N.West");
        }

        public void RegisterExportSettings()
        {
            // TODO: These would be better as export preset options, but that doesn't seem to be supported yet

            GlobalDef("mono/export/include_scripts_content", false);
            GlobalDef("mono/export/export_assemblies_inside_pck", true);

            GlobalDef("mono/export/i18n_codesets", I18NCodesets.All);

            ProjectSettings.AddPropertyInfo(new Godot.Collections.Dictionary
            {
                ["type"] = Variant.Type.Int,
                ["name"] = "mono/export/i18n_codesets",
                ["hint"] = PropertyHint.Flags,
                ["hint_string"] = "CJK,MidEast,Other,Rare,West"
            });

            GlobalDef("mono/export/aot/enabled", false);
            GlobalDef("mono/export/aot/full_aot", false);
            GlobalDef("mono/export/aot/use_interpreter", true);

            // --aot or --aot=opt1,opt2 (use 'mono --aot=help AuxAssembly.dll' to list AOT options)
            GlobalDef("mono/export/aot/extra_aot_options", Array.Empty<string>());
            // --optimize/-O=opt1,opt2 (use 'mono --list-opt'' to list optimize options)
            GlobalDef("mono/export/aot/extra_optimizer_options", Array.Empty<string>());

            GlobalDef("mono/export/aot/android_toolchain_path", "");
        }

        private void AddFile(string srcPath, string dstPath, bool remap = false)
        {
            // Add file to the PCK
            AddFile(dstPath.Replace("\\", "/"), File.ReadAllBytes(srcPath), remap);
        }

        // With this method we can override how a file is exported in the PCK
        public override void _ExportFile(string path, string type, string[] features)
        {
            base._ExportFile(path, type, features);

            if (type != Internal.CSharpLanguageType)
                return;

            if (Path.GetExtension(path) != Internal.CSharpLanguageExtension)
                throw new ArgumentException($"Resource of type {Internal.CSharpLanguageType} has an invalid file extension: {path}", nameof(path));

            // TODO What if the source file is not part of the game's C# project

            bool includeScriptsContent = (bool)ProjectSettings.GetSetting("mono/export/include_scripts_content");

            if (!includeScriptsContent)
            {
                // We don't want to include the source code on exported games.

                // Sadly, Godot prints errors when adding an empty file (nothing goes wrong, it's just noise).
                // Because of this, we add a file which contains a line break.
                AddFile(path, System.Text.Encoding.UTF8.GetBytes("\n"), remap: false);

                // Tell the Godot exporter that we already took care of the file
                Skip();
            }
        }

        public override void _ExportBegin(string[] features, bool isDebug, string path, int flags)
        {
            base._ExportBegin(features, isDebug, path, flags);

            try
            {
                _ExportBeginImpl(features, isDebug, path, flags);
            }
            catch (Exception e)
            {
                _maybeLastExportError = e.Message;

                // 'maybeLastExportError' cannot be null or empty if there was an error, so we
                // must consider the possibility of exceptions being thrown without a message.
                if (string.IsNullOrEmpty(_maybeLastExportError))
                    _maybeLastExportError = $"Exception thrown: {e.GetType().Name}";

                GD.PushError($"Failed to export project: {_maybeLastExportError}");
                Console.Error.WriteLine(e);
                // TODO: Do something on error once _ExportBegin supports failing.
            }
        }

        private void _ExportBeginImpl(string[] features, bool isDebug, string path, int flags)
        {
            _ = flags; // Unused

            if (!File.Exists(GodotSharpDirs.ProjectSlnPath))
                return;

            if (!DeterminePlatformFromFeatures(features, out string platform))
                throw new NotSupportedException("Target platform not supported");

            string outputDir = new FileInfo(path).Directory?.FullName ??
                               throw new FileNotFoundException("Base directory not found");

            string buildConfig = isDebug ? "ExportDebug" : "ExportRelease";

            string scriptsMetadataPath = BuildManager.GenerateExportedGameScriptMetadata(isDebug);
            AddFile(scriptsMetadataPath, scriptsMetadataPath);

            if (!BuildManager.BuildProjectBlocking(buildConfig, platform: platform, features: features))
                throw new Exception("Failed to build project");

            // Add dependency assemblies

            var assemblies = new Godot.Collections.Dictionary<string, string>();

            string projectDllName = GodotSharpDirs.ProjectAssemblyName;
            string projectDllSrcDir = Path.Combine(GodotSharpDirs.ResTempAssembliesBaseDir, buildConfig);
            string projectDllSrcPath = Path.Combine(projectDllSrcDir, $"{projectDllName}.dll");

            assemblies[projectDllName] = projectDllSrcPath;

            string bclDir = DeterminePlatformBclDir(platform);

            if (platform == OS.Platforms.Android)
            {
                string godotAndroidExtProfileDir = GetBclProfileDir("godot_android_ext");
                string monoAndroidAssemblyPath = Path.Combine(godotAndroidExtProfileDir, "Mono.Android.dll");

                if (!File.Exists(monoAndroidAssemblyPath))
                    throw new FileNotFoundException("Assembly not found: 'Mono.Android'", monoAndroidAssemblyPath);

                assemblies["Mono.Android"] = monoAndroidAssemblyPath;
            }
            else if (platform == OS.Platforms.HTML5)
            {
                // Ideally these would be added automatically since they're referenced by the wasm BCL assemblies.
                // However, at least in the case of 'WebAssembly.Net.Http' for some reason the BCL assemblies
                // reference a different version even though the assembly is the same, for some weird reason.

                var wasmFrameworkAssemblies = new[] { "WebAssembly.Bindings", "WebAssembly.Net.WebSockets" };

                foreach (string thisWasmFrameworkAssemblyName in wasmFrameworkAssemblies)
                {
                    string thisWasmFrameworkAssemblyPath = Path.Combine(bclDir, thisWasmFrameworkAssemblyName + ".dll");
                    if (!File.Exists(thisWasmFrameworkAssemblyPath))
                        throw new FileNotFoundException($"Assembly not found: '{thisWasmFrameworkAssemblyName}'", thisWasmFrameworkAssemblyPath);
                    assemblies[thisWasmFrameworkAssemblyName] = thisWasmFrameworkAssemblyPath;
                }

                // Assemblies that can have a different name in a newer version. Newer version must come first and it has priority.
                (string newName, string oldName)[] wasmFrameworkAssembliesOneOf = new[]
                {
                    ("System.Net.Http.WebAssemblyHttpHandler", "WebAssembly.Net.Http")
                };

                foreach (var thisWasmFrameworkAssemblyName in wasmFrameworkAssembliesOneOf)
                {
                    string thisWasmFrameworkAssemblyPath = Path.Combine(bclDir, thisWasmFrameworkAssemblyName.newName + ".dll");
                    if (File.Exists(thisWasmFrameworkAssemblyPath))
                    {
                        assemblies[thisWasmFrameworkAssemblyName.newName] = thisWasmFrameworkAssemblyPath;
                    }
                    else
                    {
                        thisWasmFrameworkAssemblyPath = Path.Combine(bclDir, thisWasmFrameworkAssemblyName.oldName + ".dll");
                        if (!File.Exists(thisWasmFrameworkAssemblyPath))
                        {
                            throw new FileNotFoundException("Expected one of the following assemblies but none were found: " +
                                                            $"'{thisWasmFrameworkAssemblyName.newName}' / '{thisWasmFrameworkAssemblyName.oldName}'",
                                thisWasmFrameworkAssemblyPath);
                        }

                        assemblies[thisWasmFrameworkAssemblyName.oldName] = thisWasmFrameworkAssemblyPath;
                    }
                }
            }

            var initialAssemblies = assemblies.Duplicate();
            internal_GetExportedAssemblyDependencies(initialAssemblies, buildConfig, bclDir, assemblies);

            AddI18NAssemblies(assemblies, bclDir);

            string outputDataDir = null;

            if (PlatformHasTemplateDir(platform))
                outputDataDir = ExportDataDirectory(features, platform, isDebug, outputDir);

            string apiConfig = isDebug ? "Debug" : "Release";
            string resAssembliesDir = Path.Combine(GodotSharpDirs.ResAssembliesBaseDir, apiConfig);

            bool assembliesInsidePck = (bool)ProjectSettings.GetSetting("mono/export/export_assemblies_inside_pck") || outputDataDir == null;

            if (!assembliesInsidePck)
            {
                string outputDataGameAssembliesDir = Path.Combine(outputDataDir, "Assemblies");
                if (!Directory.Exists(outputDataGameAssembliesDir))
                    Directory.CreateDirectory(outputDataGameAssembliesDir);
            }

            foreach (var assembly in assemblies)
            {
                void AddToAssembliesDir(string fileSrcPath)
                {
                    if (assembliesInsidePck)
                    {
                        string fileDstPath = Path.Combine(resAssembliesDir, fileSrcPath.GetFile());
                        AddFile(fileSrcPath, fileDstPath);
                    }
                    else
                    {
                        Debug.Assert(outputDataDir != null);
                        string fileDstPath = Path.Combine(outputDataDir, "Assemblies", fileSrcPath.GetFile());
                        File.Copy(fileSrcPath, fileDstPath);
                    }
                }

                string assemblySrcPath = assembly.Value;

                string assemblyPathWithoutExtension = Path.ChangeExtension(assemblySrcPath, null);
                string pdbSrcPath = assemblyPathWithoutExtension + ".pdb";

                AddToAssembliesDir(assemblySrcPath);

                if (File.Exists(pdbSrcPath))
                    AddToAssembliesDir(pdbSrcPath);
            }

            // AOT compilation
            bool aotEnabled = platform == OS.Platforms.iOS || (bool)ProjectSettings.GetSetting("mono/export/aot/enabled");

            if (aotEnabled)
            {
                string aotToolchainPath = null;

                if (platform == OS.Platforms.Android)
                    aotToolchainPath = (string)ProjectSettings.GetSetting("mono/export/aot/android_toolchain_path");

                if (aotToolchainPath == string.Empty)
                    aotToolchainPath = null; // Don't risk it being used as current working dir

                // TODO: LLVM settings are hard-coded and disabled for now
                var aotOpts = new AotOptions
                {
                    EnableLLVM = false,
                    LLVMOnly = false,
                    LLVMPath = "",
                    LLVMOutputPath = "",
                    FullAot = platform == OS.Platforms.iOS || (bool)(ProjectSettings.GetSetting("mono/export/aot/full_aot") ?? false),
                    UseInterpreter = (bool)ProjectSettings.GetSetting("mono/export/aot/use_interpreter"),
                    ExtraAotOptions = (string[])ProjectSettings.GetSetting("mono/export/aot/extra_aot_options") ?? Array.Empty<string>(),
                    ExtraOptimizerOptions = (string[])ProjectSettings.GetSetting("mono/export/aot/extra_optimizer_options") ?? Array.Empty<string>(),
                    ToolchainPath = aotToolchainPath
                };

                AotBuilder.CompileAssemblies(this, aotOpts, features, platform, isDebug, bclDir, outputDir, outputDataDir, assemblies);
            }
        }

        public override void _ExportEnd()
        {
            base._ExportEnd();

            string aotTempDir = Path.Combine(Path.GetTempPath(), $"godot-aot-{Process.GetCurrentProcess().Id}");

            if (Directory.Exists(aotTempDir))
                Directory.Delete(aotTempDir, recursive: true);

            // TODO: Just a workaround until the export plugins can be made to abort with errors
            if (!string.IsNullOrEmpty(_maybeLastExportError)) // Check empty as well, because it's set to empty after hot-reloading
            {
                string lastExportError = _maybeLastExportError;
                _maybeLastExportError = null;

                GodotSharpEditor.Instance.ShowErrorDialog(lastExportError, "Failed to export C# project");
            }
        }

        [NotNull]
        private static string ExportDataDirectory(string[] features, string platform, bool isDebug, string outputDir)
        {
            string target = isDebug ? "release_debug" : "release";

            // NOTE: Bits is ok for now as all platforms with a data directory only have one or two architectures.
            // However, this may change in the future if we add arm linux or windows desktop templates.
            string bits = features.Contains("64") ? "64" : "32";

            string TemplateDirName() => $"data.mono.{platform}.{bits}.{target}";

            string templateDirPath = Path.Combine(Internal.FullTemplatesDir, TemplateDirName());
            bool validTemplatePathFound = true;

            if (!Directory.Exists(templateDirPath))
            {
                validTemplatePathFound = false;

                if (isDebug)
                {
                    target = "debug"; // Support both 'release_debug' and 'debug' for the template data directory name
                    templateDirPath = Path.Combine(Internal.FullTemplatesDir, TemplateDirName());
                    validTemplatePathFound = true;

                    if (!Directory.Exists(templateDirPath))
                        validTemplatePathFound = false;
                }
            }

            if (!validTemplatePathFound)
                throw new FileNotFoundException("Data template directory not found", templateDirPath);

            string outputDataDir = Path.Combine(outputDir, DetermineDataDirNameForProject());

            if (Directory.Exists(outputDataDir))
                Directory.Delete(outputDataDir, recursive: true); // Clean first

            Directory.CreateDirectory(outputDataDir);

            foreach (string dir in Directory.GetDirectories(templateDirPath, "*", SearchOption.AllDirectories))
            {
                Directory.CreateDirectory(Path.Combine(outputDataDir, dir.Substring(templateDirPath.Length + 1)));
            }

            foreach (string file in Directory.GetFiles(templateDirPath, "*", SearchOption.AllDirectories))
            {
                File.Copy(file, Path.Combine(outputDataDir, file.Substring(templateDirPath.Length + 1)));
            }

            return outputDataDir;
        }

        private static bool PlatformHasTemplateDir(string platform)
        {
            // OSX export templates are contained in a zip, so we place our custom template inside it and let Godot do the rest.
            return !new[] { OS.Platforms.OSX, OS.Platforms.Android, OS.Platforms.iOS, OS.Platforms.HTML5 }.Contains(platform);
        }

        private static bool DeterminePlatformFromFeatures(IEnumerable<string> features, out string platform)
        {
            foreach (var feature in features)
            {
                if (OS.PlatformNameMap.TryGetValue(feature, out platform))
                    return true;
            }

            platform = null;
            return false;
        }

        private static string GetBclProfileDir(string profile)
        {
            string templatesDir = Internal.FullTemplatesDir;
            return Path.Combine(templatesDir, "bcl", profile);
        }

        private static string DeterminePlatformBclDir(string platform)
        {
            string templatesDir = Internal.FullTemplatesDir;
            string platformBclDir = Path.Combine(templatesDir, "bcl", platform);

            if (!File.Exists(Path.Combine(platformBclDir, "mscorlib.dll")))
            {
                string profile = DeterminePlatformBclProfile(platform);
                platformBclDir = Path.Combine(templatesDir, "bcl", profile);

                if (!File.Exists(Path.Combine(platformBclDir, "mscorlib.dll")))
                {
                    if (PlatformRequiresCustomBcl(platform))
                        throw new FileNotFoundException($"Missing BCL (Base Class Library) for platform: {platform}");

                    platformBclDir = typeof(object).Assembly.Location.GetBaseDir(); // Use the one we're running on
                }
            }

            return platformBclDir;
        }

        /// <summary>
        /// Determines whether the BCL bundled with the Godot editor can be used for the target platform,
        /// or if it requires a custom BCL that must be distributed with the export templates.
        /// </summary>
        private static bool PlatformRequiresCustomBcl(string platform)
        {
            if (new[] { OS.Platforms.Android, OS.Platforms.iOS, OS.Platforms.HTML5 }.Contains(platform))
                return true;

            // The 'net_4_x' BCL is not compatible between Windows and the other platforms.
            // We use the names 'net_4_x_win' and 'net_4_x' to differentiate between the two.

            bool isWinOrUwp = new[]
            {
                OS.Platforms.Windows,
                OS.Platforms.UWP
            }.Contains(platform);

            return OS.IsWindows ? !isWinOrUwp : isWinOrUwp;
        }

        private static string DeterminePlatformBclProfile(string platform)
        {
            switch (platform)
            {
                case OS.Platforms.Windows:
                case OS.Platforms.UWP:
                    return "net_4_x_win";
                case OS.Platforms.OSX:
                case OS.Platforms.X11:
                case OS.Platforms.Server:
                case OS.Platforms.Haiku:
                    return "net_4_x";
                case OS.Platforms.Android:
                    return "monodroid";
                case OS.Platforms.iOS:
                    return "monotouch";
                case OS.Platforms.HTML5:
                    return "wasm";
                default:
                    throw new NotSupportedException($"Platform not supported: {platform}");
            }
        }

        private static string DetermineDataDirNameForProject()
        {
            string appName = (string)ProjectSettings.GetSetting("application/config/name");
            string appNameSafe = appName.ToSafeDirName();
            return $"data_{appNameSafe}";
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void internal_GetExportedAssemblyDependencies(Godot.Collections.Dictionary<string, string> initialAssemblies,
            string buildConfig, string customBclDir, Godot.Collections.Dictionary<string, string> dependencyAssemblies);
    }
}
