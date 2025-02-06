using System.Collections.Generic;
using System.Reflection;
using System.Text;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.Testing;
using Microsoft.CodeAnalysis.Text;

namespace Godot.SourceGenerators.Tests;

public static class Extensions
{
    public static MetadataReference CreateMetadataReference(this Assembly assembly)
    {
        return MetadataReference.CreateFromFile(assembly.Location);
    }

    public static void AddGlobalConfig(this SolutionState state, Dictionary<string, string> config)
    {
        var rawContent = new StringBuilder();
        var index = state.AnalyzerConfigFiles.FindIndex(((string Name, SourceText) file) => file.Name == Constants.GlobalConfigPath);
        if (index >= 0)
        {
            rawContent.AppendLine(state.AnalyzerConfigFiles[index].content.ToString());
            state.AnalyzerConfigFiles.RemoveAt(index);
        }

        foreach (var (key, value) in config)
        {
            rawContent.Append(key);
            rawContent.Append(" = ");
            rawContent.AppendLine(value);
        }

        state.AnalyzerConfigFiles.Add((Constants.GlobalConfigPath, rawContent.ToString()));
    }
}
