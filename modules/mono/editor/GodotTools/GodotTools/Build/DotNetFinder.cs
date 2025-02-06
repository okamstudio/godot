using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using OS = GodotTools.Utils.OS;

namespace GodotTools.Build
{
    public static class DotNetFinder
    {
        public static string? FindDotNetExe(string? overrideDotnetExecutable)
        {
            if (string.IsNullOrEmpty(overrideDotnetExecutable) == false)
            {
                if (File.Exists(overrideDotnetExecutable))
                {
                    return overrideDotnetExecutable;
                }
            }

            string? pathDotnet = OS.PathWhich("dotnet");
            if (pathDotnet != null)
            {
                return pathDotnet;
            }
            // In the future, this method may do more than just search in PATH. We could look in
            // known locations or use Godot's linked nethost to search from the hostfxr location.

            if (OS.IsMacOS)
            {
                if (RuntimeInformation.OSArchitecture == Architecture.X64)
                {
                    string dotnet_x64 = "/usr/local/share/dotnet/x64/dotnet"; // Look for x64 version, when running under Rosetta 2.
                    if (File.Exists(dotnet_x64))
                    {
                        return dotnet_x64;
                    }
                }
                string dotnet = "/usr/local/share/dotnet/dotnet"; // Look for native version.
                if (File.Exists(dotnet))
                {
                    return dotnet;
                }
            }

            return null;
        }

        public static bool TryFindDotNetSdk(
            Version expectedVersion,
            string? overrideDotnetExecutable,
            [NotNullWhen(true)] out Version? version,
            [NotNullWhen(true)] out string? path
        )
        {
            version = null;
            path = null;

            string? dotNetExe = FindDotNetExe(overrideDotnetExecutable);

            if (string.IsNullOrEmpty(dotNetExe))
                return false;

            using Process process = new Process();
            process.StartInfo = new ProcessStartInfo(dotNetExe, "--list-sdks")
            {
                UseShellExecute = false,
                RedirectStandardOutput = true
            };

            if (OperatingSystem.IsWindows())
            {
                process.StartInfo.StandardOutputEncoding = Encoding.UTF8;
            }

            process.StartInfo.EnvironmentVariables["DOTNET_CLI_UI_LANGUAGE"] = "en-US";

            var lines = new List<string>();

            process.OutputDataReceived += (_, e) =>
            {
                if (!string.IsNullOrWhiteSpace(e.Data))
                    lines.Add(e.Data);
            };

            try
            {
                process.Start();
            }
            catch
            {
                return false;
            }

            process.BeginOutputReadLine();
            process.WaitForExit();

            Version? latestVersionMatch = null;
            string? matchPath = null;

            foreach (var line in lines)
            {
                string[] sdkLineParts = line.Trim()
                    .Split(' ', 2, StringSplitOptions.TrimEntries);

                if (sdkLineParts.Length < 2)
                    continue;

                if (!Version.TryParse(sdkLineParts[0], out var lineVersion))
                    continue;

                // We're looking for the exact same major version
                if (lineVersion.Major != expectedVersion.Major)
                    continue;

                if (latestVersionMatch != null && lineVersion < latestVersionMatch)
                    continue;

                latestVersionMatch = lineVersion;
                matchPath = sdkLineParts[1].TrimStart('[').TrimEnd(']');
            }

            if (latestVersionMatch == null)
                return false;

            version = latestVersionMatch;
            path = Path.Combine(matchPath!, version.ToString());

            return true;
        }
    }
}
