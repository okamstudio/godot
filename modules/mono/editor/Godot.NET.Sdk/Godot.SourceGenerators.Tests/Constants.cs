using System.IO;
using System.Reflection;
using Microsoft.CodeAnalysis.Testing;

namespace Godot.SourceGenerators.Tests;

public static class Constants
{
    public static Assembly GodotSharpAssembly => typeof(GodotObject).Assembly;

    // Can't find what needs updating to be able to access ReferenceAssemblies.Net.Net80, so we're making our own one.
    public static ReferenceAssemblies Net80 => new ReferenceAssemblies(
        "net8.0",
        new PackageIdentity("Microsoft.NETCore.App.Ref", "8.0.0"),
        Path.Combine("ref", "net8.0")
    );

    public static string ExecutingAssemblyPath { get; }
    public static string SourceFolderPath { get; }
    public static string GeneratedSourceFolderPath { get; }

    static Constants()
    {
        ExecutingAssemblyPath = Path.GetFullPath(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location!)!);

        var testDataPath = Path.Combine(ExecutingAssemblyPath, "TestData");

        SourceFolderPath = Path.Combine(testDataPath, "Sources");
        GeneratedSourceFolderPath = Path.Combine(testDataPath, "GeneratedSources");
    }
}
