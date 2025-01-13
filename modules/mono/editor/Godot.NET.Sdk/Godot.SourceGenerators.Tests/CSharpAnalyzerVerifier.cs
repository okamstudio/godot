using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Testing;
using Microsoft.CodeAnalysis.Diagnostics;
using Microsoft.CodeAnalysis.Testing;
using Microsoft.CodeAnalysis.Testing.Verifiers;
using Microsoft.CodeAnalysis.Text;

namespace Godot.SourceGenerators.Tests;

public static class CSharpAnalyzerVerifier<TAnalyzer>
    where TAnalyzer : DiagnosticAnalyzer, new()
{
    public const LanguageVersion LangVersion = LanguageVersion.CSharp11;

    public class Test : CSharpAnalyzerTest<TAnalyzer, XUnitVerifier>
    {
        public Test()
        {
            ReferenceAssemblies = Constants.Net80;

            SolutionTransforms.Add((Solution solution, ProjectId projectId) =>
            {
                Project project =
                    solution.GetProject(projectId)!.AddMetadataReference(Constants.GodotSharpAssembly
                        .CreateMetadataReference()).WithParseOptions(new CSharpParseOptions(LangVersion));

                return project.Solution;
            });
        }
    }

    public static Task Verify(string sources, params DiagnosticResult[] expected)
    {
        return MakeVerifier(new string[] { sources }, expected).RunAsync();
    }

    public static Test MakeVerifier(ICollection<string> sources, params DiagnosticResult[] expected)
    {
        var verifier = new Test();

        verifier.TestState.AnalyzerConfigFiles.Add(("/.globalconfig", $"""
        is_global = true
        build_property.GodotProjectDir = {Constants.ExecutingAssemblyPath}
        """));

        verifier.TestState.Sources.AddRange(sources.Select(source =>
        {
            return (source, SourceText.From(File.ReadAllText(Path.Combine(Constants.SourceFolderPath, source))));
        }));

        verifier.ExpectedDiagnostics.AddRange(expected);
        return verifier;
    }
}
