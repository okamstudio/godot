using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using Microsoft.CodeAnalysis.Text;
using Xunit;

namespace Godot.SourceGenerators.Tests;

public class ScriptPathAttributeGeneratorTests
{
    private static (string, SourceText) MakeAssemblyScriptTypesGeneratedSource(ICollection<string> types)
    {
        return (
            Path.Combine("Godot.SourceGenerators", "Godot.SourceGenerators.ScriptPathAttributeGenerator", "AssemblyScriptTypes.generated.cs"),
            SourceText.From($$"""
            [assembly:Godot.AssemblyHasScriptsAttribute(new System.Type[] {{{string.Join(", ", types.Select(type => $"typeof({type})"))}}})]

            """, Encoding.UTF8)
        );
    }

    [Fact]
    private async void DisableGenerator()
    {
        var verifier = CSharpSourceGeneratorVerifier<ScriptPathAttributeGenerator>.MakeVerifier(
            new string[] { "ScriptBoilerplate.cs" },
            Array.Empty<string>()
        );
        verifier.TestState.AddGlobalConfig(Utils.DisabledGenerators("ScriptPathAttribute"));
        await verifier.RunAsync();
    }

    // TODO: Remove the skip when the upstream issue is resolved, https://github.com/dotnet/roslyn/pull/51625
    [Fact(Skip = "This is currently failing due to an upstream issue with ';' in the editorconfig file format.")]
    private async void DisableGeneratorButNotFirst()
    {
        var verifier = CSharpSourceGeneratorVerifier<ScriptPathAttributeGenerator>.MakeVerifier(
            new string[] { "ScriptBoilerplate.cs" },
            Array.Empty<string>()
        );
        verifier.TestState.AddGlobalConfig(Utils.DisabledGenerators("SomePlaceholder", "ScriptPathAttribute"));
        await verifier.RunAsync();
    }

    [Fact]
    public async void ScriptBoilerplate()
    {
        var verifier = CSharpSourceGeneratorVerifier<ScriptPathAttributeGenerator>.MakeVerifier(
            new string[] { "ScriptBoilerplate.cs" },
            new string[] { "ScriptBoilerplate_ScriptPath.generated.cs" }
        );
        verifier.TestState.GeneratedSources.Add(MakeAssemblyScriptTypesGeneratedSource(new string[] { "global::ScriptBoilerplate" }));
        await verifier.RunAsync();
    }

    [Fact]
    public async void FooBar()
    {
        var verifier = CSharpSourceGeneratorVerifier<ScriptPathAttributeGenerator>.MakeVerifier(
            new string[] { "Foo.cs", "Bar.cs" },
            new string[] { "Foo_ScriptPath.generated.cs", "Bar_ScriptPath.generated.cs" }
        );
        verifier.TestState.GeneratedSources.Add(MakeAssemblyScriptTypesGeneratedSource(new string[] { "global::Foo", "global::Bar" }));
        await verifier.RunAsync();
    }

    [Fact]
    public async void Generic()
    {
        var verifier = CSharpSourceGeneratorVerifier<ScriptPathAttributeGenerator>.MakeVerifier(
            new string[] { "Generic.cs" },
            new string[] { "Generic(Of T)_ScriptPath.generated.cs" }
        );
        verifier.TestState.GeneratedSources.Add(MakeAssemblyScriptTypesGeneratedSource(new string[] { "global::Generic<>" }));
        await verifier.RunAsync();
    }

    [Fact]
    public async void GenericMultipleClassesSameName()
    {
        var verifier = CSharpSourceGeneratorVerifier<ScriptPathAttributeGenerator>.MakeVerifier(
            Array.Empty<string>(),
            new string[] { "Generic(Of T)_ScriptPath.generated.cs" }
        );
        verifier.TestState.Sources.Add(("Generic.cs", File.ReadAllText(Path.Combine(Constants.SourceFolderPath, "Generic.GD0003.cs"))));
        verifier.TestState.GeneratedSources.Add(MakeAssemblyScriptTypesGeneratedSource(new string[] { "global::Generic<>", "global::Generic<,>", "global::Generic" }));
        await verifier.RunAsync();
    }

    [Fact]
    public async void NamespaceMultipleClassesSameName()
    {
        var verifier = CSharpSourceGeneratorVerifier<ScriptPathAttributeGenerator>.MakeVerifier(
            Array.Empty<string>(),
            new string[] { "NamespaceA.SameName_ScriptPath.generated.cs" }
        );
        verifier.TestState.Sources.Add(("SameName.cs", File.ReadAllText(Path.Combine(Constants.SourceFolderPath, "SameName.GD0003.cs"))));
        verifier.TestState.GeneratedSources.Add(MakeAssemblyScriptTypesGeneratedSource(new string[] { "global::NamespaceA.SameName", "global::NamespaceB.SameName" }));
        await verifier.RunAsync();
    }
}
