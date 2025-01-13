using Godot;
using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using GodotTools.Build;
using GodotTools.Internals;
using Directory = GodotTools.Utils.Directory;
using File = GodotTools.Utils.File;
using OS = GodotTools.Utils.OS;
using Path = System.IO.Path;
using System.Globalization;

namespace GodotTools.Export
{
    public partial class ExportPlugin : EditorExportPlugin
    {
        public override string _GetName() => "C#";

        private List<string> _tempFolders = new List<string>();

        private static bool ProjectContainsDotNet()
        {
            return File.Exists(GodotSharpDirs.ProjectSlnPath);
        }

        public override string[] _GetExportFeatures(EditorExportPlatform platform, bool debug)
        {
            if (!ProjectContainsDotNet())
                return Array.Empty<string>();

            return new string[] { "dotnet" };
        }

        public override Godot.Collections.Array<Godot.Collections.Dictionary> _GetExportOptions(EditorExportPlatform platform)
        {
            return new Godot.Collections.Array<Godot.Collections.Dictionary>()
            {
                new Godot.Collections.Dictionary()
                {
                    {
                        "option", new Godot.Collections.Dictionary()
                        {
                            { "name", "dotnet/include_scripts_content" },
                            { "type", (int)Variant.Type.Bool }
                        }
                    },
                    { "default_value", false }
                },
                new Godot.Collections.Dictionary()
                {
                    {
                        "option", new Godot.Collections.Dictionary()
                        {
                            { "name", "dotnet/include_debug_symbols" },
                            { "type", (int)Variant.Type.Bool }
                        }
                    },
                    { "default_value", true }
                },
                new Godot.Collections.Dictionary()
                {
                    {
                        "option", new Godot.Collections.Dictionary()
                        {
                            { "name", "dotnet/embed_build_outputs" },
                            { "type", (int)Variant.Type.Bool }
                        }
                    },
                    { "default_value", false }
                }
            };
        }

        private void AddExceptionMessage(EditorExportPlatform platform, Exception exception)
        {
            string? exceptionMessage = exception.Message;
            if (string.IsNullOrEmpty(exceptionMessage))
            {
                exceptionMessage = $"Exception thrown: {exception.GetType().Name}";
            }

            platform.AddMessage(EditorExportPlatform.ExportMessageType.Error, "Export .NET Project", exceptionMessage);

            // We also print exceptions as we receive them to stderr.
            Console.Error.WriteLine(exception);
        }

        // With this method we can override how a file is exported in the PCK
        public override void _ExportFile(string path, string type, string[] features)
        {
            base._ExportFile(path, type, features);

            if (type != Internal.CSharpLanguageType)
                return;

            if (Path.GetExtension(path) != Internal.CSharpLanguageExtension)
                throw new ArgumentException(
                    $"Resource of type {Internal.CSharpLanguageType} has an invalid file extension: {path}",
                    nameof(path));

            if (!ProjectContainsDotNet())
            {
                GetExportPlatform().AddMessage(EditorExportPlatform.ExportMessageType.Error, "Export .NET Project", $"This project contains C# files but no solution file was found at the following path: {GodotSharpDirs.ProjectSlnPath}\n" +
                    "A solution file is required for projects with C# files. Please ensure that the solution file exists in the specified location and try again.");
                throw new InvalidOperationException($"{path} is a C# file but no solution file exists.");
            }

            // TODO: What if the source file is not part of the game's C# project?

            bool includeScriptsContent = (bool)GetOption("dotnet/include_scripts_content");

            if (!includeScriptsContent)
            {
                // We don't want to include the source code on exported games.

                // Sadly, Godot prints errors when adding an empty file (nothing goes wrong, it's just noise).
                // Because of this, we add a file which contains a line break.
                AddFile(path, System.Text.Encoding.UTF8.GetBytes("\n"), remap: false);

                // Tell the Godot exporter that we already took care of the file.
                Skip();
            }
        }

        public override void _ExportBegin(string[] features, bool isDebug, string path, uint flags)
        {
            base._ExportBegin(features, isDebug, path, flags);

            try
            {
                _ExportBeginImpl(features, isDebug, path, flags);
            }
            catch (Exception e)
            {
                AddExceptionMessage(GetExportPlatform(), e);
            }
        }

        private void _ExportBeginImpl(string[] features, bool isDebug, string path, long flags)
        {
            _ = flags; // Unused.

            if (!ProjectContainsDotNet())
                return;

            string osName = GetExportPlatform().GetOsName();

            if (!TryDeterminePlatformFromOSName(osName, out string? platform))
                throw new NotSupportedException("Target platform not supported.");

            if (!new[] { OS.Platforms.Windows, OS.Platforms.LinuxBSD, OS.Platforms.MacOS, OS.Platforms.Android, OS.Platforms.iOS }
                    .Contains(platform))
            {
                throw new NotImplementedException("Target platform not yet implemented.");
            }

            PublishConfig publishConfig = new()
            {
                BuildConfig = isDebug ? "ExportDebug" : "ExportRelease",
                IncludeDebugSymbols = (bool)GetOption("dotnet/include_debug_symbols"),
                RidOS = DetermineRuntimeIdentifierOS(platform),
                Archs = new List<string>(),
                UseTempDir = platform != OS.Platforms.iOS, // xcode project links directly to files in the publish dir, so use one that sticks around.
                BundleOutputs = true,
            };

            if (features.Contains("x86_64"))
            {
                publishConfig.Archs.Add("x86_64");
            }

            if (features.Contains("x86_32"))
            {
                publishConfig.Archs.Add("x86_32");
            }

            if (features.Contains("arm64"))
            {
                publishConfig.Archs.Add("arm64");
            }

            if (features.Contains("arm32"))
            {
                publishConfig.Archs.Add("arm32");
            }

            if (features.Contains("universal"))
            {
                if (platform == OS.Platforms.MacOS)
                {
                    publishConfig.Archs.Add("x86_64");
                    publishConfig.Archs.Add("arm64");
                }
            }

            var targets = new List<PublishConfig> { publishConfig };

            if (platform == OS.Platforms.iOS)
            {
                targets.Add(new PublishConfig
                {
                    BuildConfig = publishConfig.BuildConfig,
                    Archs = new List<string> { "arm64", "x86_64" },
                    BundleOutputs = false,
                    IncludeDebugSymbols = publishConfig.IncludeDebugSymbols,
                    RidOS = OS.DotNetOS.iOSSimulator,
                    UseTempDir = false,
                });
            }

            List<string> outputPaths = new();

            bool embedBuildResults = ((bool)GetOption("dotnet/embed_build_outputs") || platform == OS.Platforms.Android) && platform != OS.Platforms.MacOS;

            foreach (PublishConfig config in targets)
            {
                string ridOS = config.RidOS;
                string buildConfig = config.BuildConfig;
                bool includeDebugSymbols = config.IncludeDebugSymbols;

                foreach (string arch in config.Archs)
                {
                    string ridArch = DetermineRuntimeIdentifierArch(arch);
                    string runtimeIdentifier = $"{ridOS}-{ridArch}";
                    string projectDataDirName = $"data_{GodotSharpDirs.CSharpProjectName}_{platform}_{arch}";
                    if (platform == OS.Platforms.MacOS)
                    {
                        projectDataDirName = Path.Combine("Contents", "Resources", projectDataDirName);
                    }

                    // Create temporary publish output directory.
                    string publishOutputDir;

                    if (config.UseTempDir)
                    {
                        publishOutputDir = Path.Combine(Path.GetTempPath(), "godot-publish-dotnet",
                            $"{System.Environment.ProcessId}-{buildConfig}-{runtimeIdentifier}");
                        _tempFolders.Add(publishOutputDir);
                    }
                    else
                    {
                        publishOutputDir = Path.Combine(GodotSharpDirs.ProjectBaseOutputPath, "godot-publish-dotnet",
                            $"{buildConfig}-{runtimeIdentifier}");
                    }

                    outputPaths.Add(publishOutputDir);

                    if (!Directory.Exists(publishOutputDir))
                        Directory.CreateDirectory(publishOutputDir);

                    // Execute dotnet publish.
                    if (!BuildManager.PublishProjectBlocking(buildConfig, platform,
                            runtimeIdentifier, publishOutputDir, includeDebugSymbols))
                    {
                        throw new InvalidOperationException("Failed to build project.");
                    }

                    string soExt = ridOS switch
                    {
                        OS.DotNetOS.Win or OS.DotNetOS.Win10 => "dll",
                        OS.DotNetOS.OSX or OS.DotNetOS.iOS or OS.DotNetOS.iOSSimulator => "dylib",
                        _ => "so"
                    };

                    string assemblyPath = Path.Combine(publishOutputDir, $"{GodotSharpDirs.ProjectAssemblyName}.dll");
                    string nativeAotPath = Path.Combine(publishOutputDir,
                        $"{GodotSharpDirs.ProjectAssemblyName}.{soExt}");

                    if (!File.Exists(assemblyPath) && !File.Exists(nativeAotPath))
                    {
                        throw new NotSupportedException(
                            $"Publish succeeded but project assembly not found at '{assemblyPath}' or '{nativeAotPath}'.");
                    }

                    // For ios simulator builds, skip packaging the build outputs.
                    if (!config.BundleOutputs)
                        continue;

                    var manifest = new StringBuilder();

                    // Add to the exported project shared object list or packed resources.
                    RecursePublishContents(publishOutputDir,
                        filterDir: dir =>
                        {
                            if (platform == OS.Platforms.iOS)
                            {
                                // Exclude dsym folders.
                                return !dir.EndsWith(".dsym", StringComparison.OrdinalIgnoreCase);
                            }

                            return true;
                        },
                        filterFile: file =>
                        {
                            if (platform == OS.Platforms.iOS)
                            {
                                // Exclude the dylib artifact, since it's included separately as an xcframework.
                                return Path.GetFileName(file) != $"{GodotSharpDirs.ProjectAssemblyName}.dylib";
                            }

                            return true;
                        },
                        recurseDir: dir =>
                        {
                            if (platform == OS.Platforms.iOS)
                            {
                                // Don't recurse into dsym folders.
                                return !dir.EndsWith(".dsym", StringComparison.OrdinalIgnoreCase);
                            }

                            return true;
                        },
                        addEntry: (path, isFile) =>
                        {
                            // We get called back for both directories and files, but we only package files for now.
                            if (isFile)
                            {
                                if (embedBuildResults)
                                {
                                    if (platform == OS.Platforms.Android)
                                    {
                                        string fileName = Path.GetFileName(path);

                                        if (fileName.EndsWith(".jar"))
                                        {
                                            // We exclude jar files from the export since they should
                                            // already be included in the Godot templates, adding them
                                            // again would cause conflicts.
                                            return;
                                        }

                                        if (IsSharedObject(fileName))
                                        {
                                            AddSharedObject(path, tags: new string[] { arch },
                                                Path.Join(projectDataDirName,
                                                    Path.GetRelativePath(publishOutputDir,
                                                        Path.GetDirectoryName(path)!)));

                                            return;
                                        }

                                        static bool IsSharedObject(string fileName)
                                        {
                                            if (fileName.EndsWith(".so") || fileName.EndsWith(".a")
                                             || fileName.EndsWith(".dex"))
                                            {
                                                return true;
                                            }

                                            return false;
                                        }
                                    }

                                    string filePath = SanitizeSlashes(Path.GetRelativePath(publishOutputDir, path));
                                    byte[] fileData = File.ReadAllBytes(path);
                                    string hash = Convert.ToBase64String(SHA512.HashData(fileData));

                                    manifest.Append(CultureInfo.InvariantCulture, $"{filePath}\t{hash}\n");

                                    AddFile($"res://.godot/mono/publish/{arch}/{filePath}", fileData, false);
                                }
                                else
                                {
                                    if (platform == OS.Platforms.iOS && path.EndsWith(".dat", StringComparison.OrdinalIgnoreCase))
                                    {
                                        AddIosBundleFile(path);
                                    }
                                    else
                                    {
                                        AddSharedObject(path, tags: null,
                                            Path.Join(projectDataDirName,
                                                Path.GetRelativePath(publishOutputDir,
                                                    Path.GetDirectoryName(path)!)));
                                    }
                                }
                            }
                        });

                    if (embedBuildResults)
                    {
                        byte[] fileData = Encoding.Default.GetBytes(manifest.ToString());
                        AddFile($"res://.godot/mono/publish/{arch}/.dotnet-publish-manifest", fileData, false);
                    }
                }
            }

            if (platform == OS.Platforms.iOS)
            {
                if (outputPaths.Count > 2)
                {
                    // lipo the simulator binaries together

                    string outputPath = Path.Combine(outputPaths[1], $"{GodotSharpDirs.ProjectAssemblyName}.dylib");
                    string[] files = outputPaths
                        .Skip(1)
                        .Select(path => Path.Combine(path, $"{GodotSharpDirs.ProjectAssemblyName}.dylib"))
                        .ToArray();

                    if (!Internal.LipOCreateFile(outputPath, files))
                    {
                        throw new InvalidOperationException($"Failed to 'lipo' simulator binaries.");
                    }

                    outputPaths.RemoveRange(2, outputPaths.Count - 2);
                }

                string xcFrameworkPath = Path.Combine(GodotSharpDirs.ProjectBaseOutputPath, publishConfig.BuildConfig, $"{GodotSharpDirs.ProjectAssemblyName}_aot.xcframework");
                if (!BuildManager.GenerateXCFrameworkBlocking(outputPaths, xcFrameworkPath))
                {
                    throw new InvalidOperationException("Failed to generate xcframework.");
                }

                AddIosEmbeddedFramework(xcFrameworkPath);
            }
        }

        private static void RecursePublishContents(string path, Func<string, bool> filterDir,
            Func<string, bool> filterFile, Func<string, bool> recurseDir,
            Action<string, bool> addEntry)
        {
            foreach (string file in Directory.GetFiles(path, "*", SearchOption.TopDirectoryOnly))
            {
                if (filterFile(file))
                {
                    addEntry(file, true);
                }
            }

            foreach (string dir in Directory.GetDirectories(path, "*", SearchOption.TopDirectoryOnly))
            {
                if (filterDir(dir))
                {
                    addEntry(dir, false);
                    if (recurseDir(dir))
                    {
                        RecursePublishContents(dir, filterDir, filterFile, recurseDir, addEntry);
                    }
                }
            }
        }

        private string SanitizeSlashes(string path)
        {
            if (Path.DirectorySeparatorChar == '\\')
                return path.Replace('\\', '/');
            return path;
        }

        private string DetermineRuntimeIdentifierOS(string platform)
            => OS.DotNetOSPlatformMap[platform];

        private string DetermineRuntimeIdentifierArch(string arch)
        {
            return arch switch
            {
                "x86" => "x86",
                "x86_32" => "x86",
                "x64" => "x64",
                "x86_64" => "x64",
                "armeabi-v7a" => "arm",
                "arm64-v8a" => "arm64",
                "arm32" => "arm",
                "arm64" => "arm64",
                _ => throw new ArgumentOutOfRangeException(nameof(arch), arch, "Unexpected architecture")
            };
        }

        public override void _ExportEnd()
        {
            base._ExportEnd();

            string aotTempDir = Path.Combine(Path.GetTempPath(), $"godot-aot-{System.Environment.ProcessId}");

            if (Directory.Exists(aotTempDir))
                Directory.Delete(aotTempDir, recursive: true);

            foreach (string folder in _tempFolders)
            {
                Directory.Delete(folder, recursive: true);
            }
            _tempFolders.Clear();
        }

        /// <summary>
        /// Tries to determine the platform from the export preset's platform OS name.
        /// </summary>
        /// <param name="osName">Name of the export operating system.</param>
        /// <param name="platform">Platform name for the recognized supported platform.</param>
        /// <returns>
        /// <see langword="true"/> when the platform OS name is recognized as a supported platform,
        /// <see langword="false"/> otherwise.
        /// </returns>
        private static bool TryDeterminePlatformFromOSName(string osName, [NotNullWhen(true)] out string? platform)
        {
            if (OS.PlatformFeatureMap.TryGetValue(osName, out platform))
            {
                return true;
            }

            platform = null;
            return false;
        }

        private struct PublishConfig
        {
            public bool UseTempDir;
            public bool BundleOutputs;
            public string RidOS;
            public List<string> Archs;
            public string BuildConfig;
            public bool IncludeDebugSymbols;
        }
    }
}
