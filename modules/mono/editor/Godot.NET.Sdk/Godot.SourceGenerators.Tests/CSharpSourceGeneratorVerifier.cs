using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp.Testing;
using Microsoft.CodeAnalysis.Testing;
using Microsoft.CodeAnalysis.Testing.Verifiers;
using Microsoft.CodeAnalysis.Text;

namespace Godot.SourceGenerators.Tests;

public static class CSharpSourceGeneratorVerifier<TSourceGenerator>
where TSourceGenerator : ISourceGenerator, new()
{
    public class Test : CSharpSourceGeneratorTest<TSourceGenerator, XUnitVerifier>
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

    public static Task Verify(string source, params string[] generatedSources)
    {
        return Verify(new string[] { source }, generatedSources);
    }

    public static Task VerifyNoCompilerDiagnostics(string source, params string[] generatedSources)
    {
        return VerifyNoCompilerDiagnostics(new string[] { source }, generatedSources);
    }

    public static Task Verify(ICollection<string> sources, params string[] generatedSources)
    {
        return MakeVerifier(sources, generatedSources).RunAsync();
    }

    public static Task VerifyNoCompilerDiagnostics(ICollection<string> sources, params string[] generatedSources)
    {
        var verifier = MakeVerifier(sources, generatedSources);
        verifier.CompilerDiagnostics = CompilerDiagnostics.None;
        return verifier.RunAsync();
    }

    public static Test MakeVerifier(ICollection<string> sources, ICollection<string> generatedSources)
    {
        var verifier = new Test();

        verifier.TestState.AnalyzerConfigFiles.Add(("/.globalconfig", $"""
        is_global = true
        build_property.GodotProjectDir = {Constants.ExecutingAssemblyPath}
        """));

        verifier.TestState.Sources.AddRange(sources.Select(source => (
            source,
            SourceText.From(File.ReadAllText(Path.Combine(Constants.SourceFolderPath, source)))
        )));

        verifier.TestState.GeneratedSources.AddRange(generatedSources.Select(generatedSource => (
                FullGeneratedSourceName(generatedSource),
                SourceText.From(File.ReadAllText(Path.Combine(Constants.GeneratedSourceFolderPath, generatedSource)), Encoding.UTF8)
        )));

        return verifier;
    }

    private static string FullGeneratedSourceName(string name)
    {
        var generatorType = typeof(TSourceGenerator);
        return Path.Combine(generatorType.Namespace!, generatorType.FullName!, name);
    }
}
