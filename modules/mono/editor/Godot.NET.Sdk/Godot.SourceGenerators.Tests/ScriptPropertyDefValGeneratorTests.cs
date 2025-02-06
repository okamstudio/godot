using System;
using Xunit;

namespace Godot.SourceGenerators.Tests;

public class ScriptPropertyDefValGeneratorTests
{
    [Fact]
    public async void DisableGenerator()
    {
        var verifier = CSharpSourceGeneratorVerifier<ScriptPropertyDefValGenerator>.MakeVerifier(
            new string[] { "ExportedFields.cs", "ExportedProperties.cs" },
            Array.Empty<string>()
        );
        verifier.TestState.AddGlobalConfig(Utils.DisabledGenerators("ScriptPropertyDefVal"));
        await verifier.RunAsync();
    }

    [Fact]
    public async void ExportedFields()
    {
        await CSharpSourceGeneratorVerifier<ScriptPropertyDefValGenerator>.Verify(
            new string[] { "ExportedFields.cs", "MoreExportedFields.cs" },
            new string[] { "ExportedFields_ScriptPropertyDefVal.generated.cs" }
        );
    }

    [Fact]
    public async void ExportedProperties()
    {
        await CSharpSourceGeneratorVerifier<ScriptPropertyDefValGenerator>.Verify(
            "ExportedProperties.cs",
            "ExportedProperties_ScriptPropertyDefVal.generated.cs"
        );
    }

    [Fact]
    public async void ExportedComplexStrings()
    {
        await CSharpSourceGeneratorVerifier<ScriptPropertyDefValGenerator>.Verify(
            "ExportedComplexStrings.cs",
            "ExportedComplexStrings_ScriptPropertyDefVal.generated.cs"
        );
    }
}
