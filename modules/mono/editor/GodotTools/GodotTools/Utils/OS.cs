using Godot.NativeInterop;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Runtime.Versioning;
using System.Text;
using GodotTools.Internals;

namespace GodotTools.Utils
{
    [SuppressMessage("ReSharper", "InconsistentNaming")]
    public static class OS
    {
        /// <summary>
        /// Display names for the OS platforms.
        /// </summary>
        private static class Names
        {
            public const string Windows = "Windows";
            public const string MacOS = "macOS";
            public const string Linux = "Linux";
            public const string FreeBSD = "FreeBSD";
            public const string NetBSD = "NetBSD";
            public const string BSD = "BSD";
            public const string Android = "Android";
            public const string iOS = "iOS";
            public const string Web = "Web";
        }

        /// <summary>
        /// Godot platform identifiers.
        /// </summary>
        public static class Platforms
        {
            public const string Windows = "windows";
            public const string MacOS = "macos";
            public const string LinuxBSD = "linuxbsd";
            public const string Android = "android";
            public const string iOS = "ios";
            public const string Web = "web";
        }

        /// <summary>
        /// OS name part of the .NET runtime identifier (RID).
        /// See https://docs.microsoft.com/en-us/dotnet/core/rid-catalog.
        /// </summary>
        public static class DotNetOS
        {
            public const string Win = "win";
            public const string OSX = "osx";
            public const string Linux = "linux";
            public const string Win10 = "win10";
            public const string Android = "android";
            public const string LinuxBionic = "linux-bionic";
            public const string iOS = "ios";
            public const string iOSSimulator = "iossimulator";
            public const string Browser = "browser";
        }

        public static readonly Dictionary<string, string> PlatformFeatureMap = new Dictionary<string, string>(
            // Export `features` may be in lower case
            StringComparer.InvariantCultureIgnoreCase
        )
        {
            ["Windows"] = Platforms.Windows,
            ["macOS"] = Platforms.MacOS,
            ["Linux"] = Platforms.LinuxBSD,
            ["Android"] = Platforms.Android,
            ["iOS"] = Platforms.iOS,
            ["Web"] = Platforms.Web
        };

        public static readonly Dictionary<string, string> PlatformNameMap = new Dictionary<string, string>
        {
            [Names.Windows] = Platforms.Windows,
            [Names.MacOS] = Platforms.MacOS,
            [Names.Linux] = Platforms.LinuxBSD,
            [Names.FreeBSD] = Platforms.LinuxBSD,
            [Names.NetBSD] = Platforms.LinuxBSD,
            [Names.BSD] = Platforms.LinuxBSD,
            [Names.Android] = Platforms.Android,
            [Names.iOS] = Platforms.iOS,
            [Names.Web] = Platforms.Web
        };

        public static readonly Dictionary<string, string> DotNetOSPlatformMap = new Dictionary<string, string>
        {
            [Platforms.Windows] = DotNetOS.Win,
            [Platforms.MacOS] = DotNetOS.OSX,
            // TODO:
            // Does .NET 6 support BSD variants? If it does, it may need the name `unix`
            // instead of `linux` in the runtime identifier. This would be a problem as
            // Godot has a single export profile for both, named LinuxBSD.
            [Platforms.LinuxBSD] = DotNetOS.Linux,
            [Platforms.Android] = DotNetOS.Android,
            [Platforms.iOS] = DotNetOS.iOS,
            [Platforms.Web] = DotNetOS.Browser
        };
        private static bool IsOS(string name)
        {
            Internal.godot_icall_Utils_OS_GetPlatformName(out godot_string dest);
            using (dest)
            {
                string platformName = Marshaling.ConvertStringToManaged(dest);
                return name.Equals(platformName, StringComparison.OrdinalIgnoreCase);
            }
        }

        private static bool IsAnyOS(IEnumerable<string> names)
        {
            Internal.godot_icall_Utils_OS_GetPlatformName(out godot_string dest);
            using (dest)
            {
                string platformName = Marshaling.ConvertStringToManaged(dest);
                return names.Any(p => p.Equals(platformName, StringComparison.OrdinalIgnoreCase));
            }
        }

        private static readonly IEnumerable<string> LinuxBSDPlatforms =
            new[] { Names.Linux, Names.FreeBSD, Names.NetBSD, Names.BSD };

        private static readonly IEnumerable<string> UnixLikePlatforms =
            new[] { Names.MacOS, Names.Android, Names.iOS }
                .Concat(LinuxBSDPlatforms).ToArray();

        private static readonly Lazy<bool> _isWindows = new(() => IsOS(Names.Windows));
        private static readonly Lazy<bool> _isMacOS = new(() => IsOS(Names.MacOS));
        private static readonly Lazy<bool> _isLinuxBSD = new(() => IsAnyOS(LinuxBSDPlatforms));
        private static readonly Lazy<bool> _isAndroid = new(() => IsOS(Names.Android));
        private static readonly Lazy<bool> _isiOS = new(() => IsOS(Names.iOS));
        private static readonly Lazy<bool> _isWeb = new(() => IsOS(Names.Web));
        private static readonly Lazy<bool> _isUnixLike = new(() => IsAnyOS(UnixLikePlatforms));

        [SupportedOSPlatformGuard("windows")] public static bool IsWindows => _isWindows.Value;

        [SupportedOSPlatformGuard("osx")] public static bool IsMacOS => _isMacOS.Value;

        [SupportedOSPlatformGuard("linux")] public static bool IsLinuxBSD => _isLinuxBSD.Value;

        [SupportedOSPlatformGuard("android")] public static bool IsAndroid => _isAndroid.Value;

        [SupportedOSPlatformGuard("ios")] public static bool IsiOS => _isiOS.Value;

        [SupportedOSPlatformGuard("browser")] public static bool IsWeb => _isWeb.Value;
        public static bool IsUnixLike => _isUnixLike.Value;

        public static char PathSep => IsWindows ? ';' : ':';

        public static string? PathWhich(string name)
        {
            if (IsWindows)
                return PathWhichWindows(name);

            return PathWhichUnix(name);
        }

        private static string? PathWhichWindows(string name)
        {
            string[] windowsExts =
                Environment.GetEnvironmentVariable("PATHEXT")?.Split(PathSep) ?? Array.Empty<string>();
            string[]? pathDirs = Environment.GetEnvironmentVariable("PATH")?.Split(PathSep);
            char[] invalidPathChars = Path.GetInvalidPathChars();

            var searchDirs = new List<string>();

            if (pathDirs != null)
            {
                foreach (var pathDir in pathDirs)
                {
                    if (pathDir.IndexOfAny(invalidPathChars) != -1)
                        continue;

                    searchDirs.Add(pathDir);
                }
            }

            string nameExt = Path.GetExtension(name);
            bool hasPathExt = !string.IsNullOrEmpty(nameExt) &&
                              windowsExts.Contains(nameExt, StringComparer.OrdinalIgnoreCase);

            searchDirs.Add(System.IO.Directory.GetCurrentDirectory()); // last in the list

            if (hasPathExt)
                return searchDirs.Select(dir => Path.Combine(dir, name)).FirstOrDefault(File.Exists);

            return (from dir in searchDirs
                    select Path.Combine(dir, name)
                into path
                    from ext in windowsExts
                    select path + ext).FirstOrDefault(File.Exists);
        }

        private static string? PathWhichUnix(string name)
        {
            string[]? pathDirs = Environment.GetEnvironmentVariable("PATH")?.Split(PathSep);
            char[] invalidPathChars = Path.GetInvalidPathChars();

            var searchDirs = new List<string>();

            if (pathDirs != null)
            {
                foreach (var pathDir in pathDirs)
                {
                    if (pathDir.IndexOfAny(invalidPathChars) != -1)
                        continue;

                    searchDirs.Add(pathDir);
                }
            }

            searchDirs.Add(System.IO.Directory.GetCurrentDirectory()); // last in the list

            return searchDirs.Select(dir => Path.Combine(dir, name))
                .FirstOrDefault(path =>
                {
                    using godot_string pathIn = Marshaling.ConvertStringToNative(path);
                    return File.Exists(path) && Internal.godot_icall_Utils_OS_UnixFileHasExecutableAccess(pathIn);
                });
        }

        public static void RunProcess(string command, IEnumerable<string> arguments)
        {
            var startInfo = new ProcessStartInfo(command)
            {
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false,
                CreateNoWindow = true
            };

            foreach (string arg in arguments)
                startInfo.ArgumentList.Add(arg);

            using Process? process = Process.Start(startInfo);

            if (process == null)
                throw new InvalidOperationException("No process was started.");

            process.BeginOutputReadLine();
            process.BeginErrorReadLine();

            if (IsWindows && process.Id > 0)
                User32Dll.AllowSetForegroundWindow(process.Id); // Allows application to focus itself
        }

        public static int ExecuteCommand(string command, IEnumerable<string> arguments)
        {
            var startInfo = new ProcessStartInfo(command)
            {
                // Print the output
                RedirectStandardOutput = false,
                RedirectStandardError = false,
                UseShellExecute = false
            };

            foreach (string arg in arguments)
                startInfo.ArgumentList.Add(arg);

            Console.WriteLine(startInfo.GetCommandLineDisplay(new StringBuilder("Executing: ")).ToString());

            using var process = new Process { StartInfo = startInfo };
            process.Start();
            process.WaitForExit();

            return process.ExitCode;
        }

        private static void AppendProcessFileNameForDisplay(this StringBuilder builder, string fileName)
        {
            if (builder.Length > 0)
                builder.Append(' ');

            if (fileName.Contains(' ', StringComparison.Ordinal))
            {
                builder.Append('"');
                builder.Append(fileName);
                builder.Append('"');
            }
            else
            {
                builder.Append(fileName);
            }
        }

        private static void AppendProcessArgumentsForDisplay(this StringBuilder builder,
            Collection<string> argumentList)
        {
            // This is intended just for reading. It doesn't need to be a valid command line.
            // E.g.: We don't handle escaping of quotes.

            foreach (string argument in argumentList)
            {
                if (builder.Length > 0)
                    builder.Append(' ');

                if (argument.Contains(' ', StringComparison.Ordinal))
                {
                    builder.Append('"');
                    builder.Append(argument);
                    builder.Append('"');
                }
                else
                {
                    builder.Append(argument);
                }
            }
        }

        public static StringBuilder GetCommandLineDisplay(
            this ProcessStartInfo startInfo,
            StringBuilder? optionalBuilder = null
        )
        {
            var builder = optionalBuilder ?? new StringBuilder();

            builder.AppendProcessFileNameForDisplay(startInfo.FileName);

            if (startInfo.ArgumentList.Count == 0)
            {
                builder.Append(' ');
                builder.Append(startInfo.Arguments);
            }
            else
            {
                builder.AppendProcessArgumentsForDisplay(startInfo.ArgumentList);
            }

            return builder;
        }
    }
}
