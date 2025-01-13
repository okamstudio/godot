using System.IO;
using System.Threading.Tasks;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CodeFixes;
using Microsoft.CodeAnalysis.CSharp.Testing;
using Microsoft.CodeAnalysis.Diagnostics;
using Microsoft.CodeAnalysis.Testing.Verifiers;

namespace Godot.SourceGenerators.Tests;

public static class CSharpCodeFixVerifier<TCodeFix, TAnalyzer>
    where TCodeFix : CodeFixProvider, new()
    where TAnalyzer : DiagnosticAnalyzer, new()
{
    public class Test : CSharpCodeFixTest<TAnalyzer, TCodeFix, XUnitVerifier>
    {
        public Test()
        {
            ReferenceAssemblies = Constants.Net80;
            SolutionTransforms.Add((Solution solution, ProjectId projectId) =>
            {
                Project project = solution.GetProject(projectId)!
                    .AddMetadataReference(Constants.GodotSharpAssembly.CreateMetadataReference());
                return project.Solution;
            });
        }
    }

    public static Task Verify(string sources, string fixedSources)
    {
        return MakeVerifier(sources, fixedSources).RunAsync();
    }

    public static Test MakeVerifier(string source, string results)
    {
        var verifier = new Test();

        verifier.TestCode = File.ReadAllText(Path.Combine(Constants.SourceFolderPath, source));
        verifier.FixedCode = File.ReadAllText(Path.Combine(Constants.GeneratedSourceFolderPath, results));

        verifier.TestState.AnalyzerConfigFiles.Add(("/.globalconfig", $"""
        is_global = true
        build_property.GodotProjectDir = {Constants.ExecutingAssemblyPath}
        """));

        return verifier;
    }
}
